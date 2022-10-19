/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "crypto/fm_crypto.h"
#include "events/fmna_event.h"
#include "events/fmna_config_event.h"
#include "fmna_conn.h"
#include "fmna_gatt_fmns.h"
#include "fmna_keys.h"
#include "fmna_storage.h"
#include "fmna_state.h"

/* BLE internal header, use with caution. */
#include <bluetooth/host/keys.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

#define PRIMARY_KEYS_PER_SECONDARY_KEY       96
#define SECONDARY_KEY_EVAL_INDEX_LOWER_BOUND 4
#define SECONDARY_KEY_INDEX_FROM_PRIMARY(_index) \
	(((_index) / PRIMARY_KEYS_PER_SECONDARY_KEY) + 1)

/*
 * Index period after which keys are updated in the storage.
 * Increasing this value would cause the storage to be updated
 * less frequently at the cost of application bootup time.
 * 1 index unit corresponds to roughly 15 minutes.
 */
#define STORAGE_UPDATE_PERIOD 16

#define KEY_ROTATION_TIMER_PERIOD K_MINUTES(15)

static k_timeout_t key_rotation_timer_period;

static uint8_t master_pk[FMNA_MASTER_PUBLIC_KEY_LEN];
static uint8_t curr_primary_sk[FMNA_SYMMETRIC_KEY_LEN];
static uint8_t curr_secondary_sk[FMNA_SYMMETRIC_KEY_LEN];

static uint8_t curr_primary_pk[FMNA_PUBLIC_KEY_LEN];
static uint8_t curr_secondary_pk[FMNA_PUBLIC_KEY_LEN];

static uint8_t latched_primary_pk[FMNA_PUBLIC_KEY_LEN];
static bool is_primary_pk_latched = false;

static uint32_t primary_pk_rotation_cnt = 0;
static uint32_t secondary_pk_rotation_delta = 0;
static uint32_t secondary_pk_rotation_cnt = 0;

static bool use_secondary_pk = false;

/* Declaration of variables that are relevant to the BLE stack. */
static uint8_t bt_id;
static uint8_t bt_ltk[16];
static struct bt_keys *bt_keys = NULL;


static void key_rotation_work_handle(struct k_work *item);
static void key_rotation_timeout_handle(struct k_timer *timer_id);

static K_WORK_DEFINE(key_rotation_work, key_rotation_work_handle);
static K_TIMER_DEFINE(key_rotation_timer, key_rotation_timeout_handle, NULL);

static void bt_ltk_set(const bt_addr_le_t *bt_owner_addr)
{
	struct bt_ltk new_ltk;

	bt_keys = bt_keys_get_addr(bt_id, bt_owner_addr);
	if (!bt_keys) {
		LOG_ERR("bt_ltk_set: Owner key set cannot be found");
		return;
	}

	/* Set a proper key properties for the newly created keyset. */
	bt_keys->keys = BT_KEYS_LTK_P256;
	bt_keys->enc_size = sizeof(bt_keys->ltk.val);

	/* Configure the new LTK. EDIV and Rand values are set to 0. */
	memset(&new_ltk, 0, sizeof(new_ltk));
	memcpy(new_ltk.val, bt_ltk, sizeof(new_ltk.val));

	/*
	 * Update the LTK value in the BLE stack.
	 * TODO: Check if it makes sense to guard against data corruption with mutexes, etc.
	 */
	memcpy(&bt_keys->ltk, &new_ltk, sizeof(bt_keys->ltk));

	LOG_HEXDUMP_DBG(new_ltk.val, sizeof(new_ltk.val), "Setting BLE LTK");
}

static int symmetric_key_roll(uint8_t sk[FMNA_SYMMETRIC_KEY_LEN])
{
	int err;
	uint8_t new_sk[FMNA_SYMMETRIC_KEY_LEN];

	err = fm_crypto_roll_sk(sk, new_sk);
	if (err) {
		LOG_ERR("fm_crypto_roll_sk returned error: %d", err);
		return err;
	}

	memcpy(sk, new_sk, FMNA_SYMMETRIC_KEY_LEN);

	return 0;
}

static int primary_key_roll(void)
{
	int err;

	/* SK(i) -> SK(i+1) */
	err = symmetric_key_roll(curr_primary_sk);
	if (err) {
		LOG_ERR("symmetric_key_roll returned error: %d for primary SK", err);
		return err;
	}

	/* SK(i+1) -> Primary_Key(i+1) */
	err = fm_crypto_derive_primary_or_secondary_x(curr_primary_sk, master_pk, curr_primary_pk);
	if (err) {
		LOG_ERR("symmetric_key_roll returned error: %d for primary SK", err);
		return err;
	}

	primary_pk_rotation_cnt++;

	/* SK(i+1) -> LTK(i+1) */
	err = fm_crypto_derive_ltk(curr_primary_sk, bt_ltk);
	if (err) {
		LOG_ERR("symmetric_key_roll returned error: %d for primary SK", err);
		return err;
	}

	LOG_DBG("Rolling Primary Public Key to: P[%d]", primary_pk_rotation_cnt);
	LOG_HEXDUMP_DBG(curr_primary_pk, sizeof(curr_primary_pk), "Primary Public Key");

	return 0;
}

static bool secondary_key_is_outdated(uint32_t primary_key_index)
{
	int result;
	uint32_t expected_secondary_key_index;

	expected_secondary_key_index =
		SECONDARY_KEY_INDEX_FROM_PRIMARY(primary_key_index);
	result = expected_secondary_key_index - secondary_pk_rotation_cnt;

	__ASSERT(((result == 0) || (result == 1)),
		 "Secondary Key is not synced properly with Primary Key. "
		 "Index diff: %d", result);

	return (result != 0);
}

static int secondary_key_roll(void)
{
	int err;

	/* SK(i) -> SK(i+1) */
	err = symmetric_key_roll(curr_secondary_sk);
	if (err) {
		LOG_ERR("symmetric_key_roll returned error: %d for secondary SK", err);
		return err;
	}

	/* SK(i+1) -> Secondary_Key(i+1) */
	err = fm_crypto_derive_primary_or_secondary_x(curr_secondary_sk, master_pk,
						      curr_secondary_pk);
	if (err) {
		LOG_ERR("symmetric_key_roll returned error: %d for secondary SK", err);
		return err;
	}

	secondary_pk_rotation_cnt++;

	LOG_DBG("Rolling Secondary Public Key: PW[%d]", secondary_pk_rotation_cnt);
	LOG_HEXDUMP_DBG(curr_secondary_pk, sizeof(curr_secondary_pk), "Secondary Public Key");

	return 0;
}

static int rotating_key_storage_update(void)
{
	int err;
	uint16_t current_keys_index_diff = 0;

	err = fmna_storage_pairing_item_store(FMNA_STORAGE_PRIMARY_SK_ID,
					      curr_primary_sk,
					      sizeof(curr_primary_sk));
	if (err) {
		LOG_ERR("fmna_keys: cannot store Primary SK");
		return err;
	}

	err = fmna_storage_pairing_item_store(FMNA_STORAGE_SECONDARY_SK_ID,
					      curr_secondary_sk,
					      sizeof(curr_secondary_sk));
	if (err) {
		LOG_ERR("fmna_keys: cannot store Secondary SK");
		return err;
	}

	err = fmna_storage_pairing_item_store(FMNA_STORAGE_PRIMARY_KEY_INDEX_ID,
					      (uint8_t *) &primary_pk_rotation_cnt,
					      sizeof(primary_pk_rotation_cnt));
	if (err) {
		LOG_ERR("fmna_keys: cannot store the Primary Key index");
		return err;
	}

	err = fmna_storage_pairing_item_store(FMNA_STORAGE_CURRENT_KEYS_INDEX_DIFF_ID,
					      (uint8_t *) &current_keys_index_diff,
					      sizeof(current_keys_index_diff));
	if (err) {
		LOG_ERR("fmna_keys: cannot store the diff between current and storage key");
		return err;
	}

	LOG_DBG("Updating FMN keys storage at Primary Key index i=%d",
		primary_pk_rotation_cnt);

	return err;
}

static int key_storage_init(void)
{
	int err;

	err = fmna_storage_pairing_item_store(FMNA_STORAGE_MASTER_PUBLIC_KEY_ID,
					      master_pk,
					      sizeof(master_pk));
	if (err) {
		LOG_ERR("fmna_keys: cannot store Master Public Key");
		return err;
	}

	err = rotating_key_storage_update();
	if (err) {
		LOG_ERR("rotating_key_storage_update returned error: %d", err);
		return err;
	}

	return err;
}

static void primary_key_rotation_indicate(void)
{
	int err;
	struct bt_conn *owners[CONFIG_BT_MAX_CONN];
	uint8_t owners_num = ARRAY_SIZE(owners);

	/* Find connected owners payload. */
	err = fmna_conn_owner_find(owners, &owners_num);
	if (err) {
		LOG_ERR("fmna_conn_owner_find returned error: %d", err);
	}

	/* Encode the indication payload. */
	NET_BUF_SIMPLE_DEFINE(resp_buf, sizeof(primary_pk_rotation_cnt));
	net_buf_simple_add_le32(&resp_buf, primary_pk_rotation_cnt);

	/* Dispatch the indication to the connected owners. */
	for (uint8_t i = 0; i < owners_num; i++) {
		struct bt_conn *conn = owners[i];

		LOG_INF("FMN Keys: sending Primary Key roll indication: %p",
			(void *) conn);

		err = fmna_gatt_config_cp_indicate(
			conn, FMNA_GATT_CONFIG_KEYROLL_IND, &resp_buf);
		if (err) {
			LOG_ERR("fmna_gatt_config_cp_indicate returned error:%d", err);
		}
	}
}

static void key_rotation_work_handle(struct k_work *item)
{
	int err;
	bool separated_key_changed = true;
	uint16_t storage_key_index_diff;

	LOG_INF("Rotating FMNA keys");

	err = primary_key_roll();
	if (err) {
		LOG_ERR("primary_key_roll returned error: %d", err);
		return;
	}

	/* Check if the secondary key update is necessary. */
	if (secondary_key_is_outdated(primary_pk_rotation_cnt)) {
		err = secondary_key_roll();
		if (err) {
			LOG_ERR("secondary_key_roll returned error: %d", err);
			return;
		}
	}

	/* Check if this is the end of the current separated key period. */
	if ((primary_pk_rotation_cnt % PRIMARY_KEYS_PER_SECONDARY_KEY) ==
	    secondary_pk_rotation_delta) {
		/* Reset the latched primary key. */
		is_primary_pk_latched = false;

		/* Use the secondary key as a separated key. */
		use_secondary_pk = true;
	} else {
		/* The secondary Public Key update is omitted. */
		if (!is_primary_pk_latched) {
			if (use_secondary_pk) {
				separated_key_changed = false;
			}
		}
	}

	/* Update storage information after each rotation. */
	storage_key_index_diff = (primary_pk_rotation_cnt % STORAGE_UPDATE_PERIOD);
	if (storage_key_index_diff) {
		err = fmna_storage_pairing_item_store(
			FMNA_STORAGE_CURRENT_KEYS_INDEX_DIFF_ID,
			(uint8_t *) &storage_key_index_diff,
			sizeof(storage_key_index_diff));
		if (err) {
			LOG_ERR("fmna_keys: cannot store the diff between "
				"current and storage key");
			return;
		}
	} else {
		err = rotating_key_storage_update();
		if (err) {
			LOG_ERR("rotating_key_storage_update returned error: %d", err);
			return;
		}
	}

	/* Emit event notifying that the Public Keys have changed. */
	FMNA_EVENT_CREATE(event, FMNA_EVENT_PUBLIC_KEYS_CHANGED, NULL);
	event->public_keys_changed.separated_key_changed = separated_key_changed;
	APP_EVENT_SUBMIT(event);

	/* Indicate to all connected owners that the Primary Key roll has occurred. */
	primary_key_rotation_indicate();
}

static void key_rotation_timeout_handle(struct k_timer *timer_id)
{
	k_work_submit(&key_rotation_work);
}

int fmna_keys_primary_key_get(uint8_t primary_key[FMNA_PUBLIC_KEY_LEN])
{
	/* TODO: Use synchronisation primitive to prevent memory corruption
	*        during the copy operation.
	*/
	memcpy(primary_key, curr_primary_pk, FMNA_PUBLIC_KEY_LEN);

	return 0;
}

int fmna_keys_separated_key_get(uint8_t separated_key[FMNA_PUBLIC_KEY_LEN])
{
	/* TODO: Use synchronisation primitive to prevent memory corruption
	 *	 during the copy operation.
	 */
	if (is_primary_pk_latched) {
		memcpy(separated_key, latched_primary_pk, FMNA_PUBLIC_KEY_LEN);

		return 0;
	}

	if (use_secondary_pk) {
		memcpy(separated_key, curr_secondary_pk, FMNA_PUBLIC_KEY_LEN);
	} else {
		memcpy(separated_key, curr_primary_pk, FMNA_PUBLIC_KEY_LEN);
	}

	return 0;
}

static void fmna_keys_state_cleanup(void)
{
	primary_pk_rotation_cnt = 0;
	secondary_pk_rotation_delta = 0;
	secondary_pk_rotation_cnt = 0;

	is_primary_pk_latched = false;
	use_secondary_pk = false;
}

int fmna_keys_service_stop(void)
{
	/* Stop the key rotation timeout. */
	k_timer_stop(&key_rotation_timer);

	fmna_keys_state_cleanup();

	LOG_INF("FMNA Keys rotation service stopped");

	return 0;
}

static void keys_service_timer_start(void)
{
	/* Start key rotation timeout. */
	k_timer_start(&key_rotation_timer, key_rotation_timer_period, key_rotation_timer_period);

	LOG_INF("FMNA Keys rotation service started");
}

int fmna_keys_service_start(const struct fmna_keys_init *init_keys)
{
	int err;
	uint16_t storage_key_index_diff;

	memcpy(master_pk, init_keys->master_pk, sizeof(master_pk));
	memcpy(curr_primary_sk, init_keys->primary_sk,
	       sizeof(curr_primary_sk));
	memcpy(curr_secondary_sk, init_keys->secondary_sk,
	       sizeof(curr_secondary_sk));

	/* Primary SK N -> Primary SK 0 */
	err = symmetric_key_roll(curr_primary_sk);
	if (err) {
		LOG_ERR("symmetric_key_roll returned error: %d for primary SK", err);
		return err;
	}

	/* Secondary SK N -> Secondary SK 0 */
	err = symmetric_key_roll(curr_secondary_sk);
	if (err) {
		LOG_ERR("symmetric_key_roll returned error: %d for secondary SK", err);
		return err;
	}

	/* Update storage with the FMN keys at index 0. */
	err = key_storage_init();
	if (err) {
		LOG_ERR("key_storage_init returned error: %d", err);
		return err;
	}

	/* Roll to the Primary Public Key 1 */
	err = primary_key_roll();
	if (err) {
		LOG_ERR("primary_key_roll returned error: %d", err);
		return err;
	}

	/* Roll to the Secondary Public Key 1 */
	err = secondary_key_roll();
	if (err) {
		LOG_ERR("secondary_key_roll returned error: %d", err);
		return err;
	}

	/* Update the difference value. */
	storage_key_index_diff = primary_pk_rotation_cnt;
	err = fmna_storage_pairing_item_store(FMNA_STORAGE_CURRENT_KEYS_INDEX_DIFF_ID,
					(uint8_t *) &storage_key_index_diff,
					sizeof(storage_key_index_diff));
	if (err) {
		LOG_ERR("fmna_keys: cannot store the diff between current and storage key");
		return err;
	}

	/* Update the Secondary SK value in storage. */
	err = fmna_storage_pairing_item_store(FMNA_STORAGE_SECONDARY_SK_ID,
					      curr_secondary_sk,
					      sizeof(curr_secondary_sk));
	if (err) {
		LOG_ERR("fmna_keys: cannot store Secondary SK");
		return err;
	}

	/* Start key rotation service timer. */
	keys_service_timer_start();

	return 0;
}

static void fmna_peer_connected(struct bt_conn *conn)
{
	if (fmna_state_is_paired()) {
		bt_ltk_set(bt_conn_get_dst(conn));
	}
}

static void fmna_peer_security_changed(struct bt_conn *conn, bt_security_t level,
				       enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (fmna_state_is_paired()) {
		if (bt_keys && (bt_addr_le_cmp(&bt_keys->addr, bt_conn_get_dst(conn)) == 0)) {
			/* Reset BLE LTK key. */
			bt_keys_clear(bt_keys);
			bt_keys = NULL;

			if (!err) {
				fmna_conn_multi_status_bit_set(
					conn, FMNA_CONN_MULTI_STATUS_BIT_OWNER_CONNECTED);

				/* Stop using the secondary key after exit from
				 * the Separated state.
				 */
				use_secondary_pk = false;

				FMNA_EVENT_CREATE(event, FMNA_EVENT_OWNER_CONNECTED, conn);
				APP_EVENT_SUBMIT(event);
			}
		} else {
			LOG_WRN("fmna_keys: cannot clear FMN LTK from BLE stack key pool for %s",
				addr);
		}
	}
}

static void fmna_peer_disconnected(struct bt_conn *conn)
{
	if (fmna_state_is_paired()) {
		if (bt_keys && (bt_addr_le_cmp(&bt_keys->addr, bt_conn_get_dst(conn)) == 0)) {
			/* Reset BLE LTK key. */
			bt_keys_clear(bt_keys);
			bt_keys = NULL;
		}
	}
}

static int paired_state_restore(void)
{
	int err;
	int64_t start_time;
	int64_t duration;
	char hexdump_header[50];
	uint16_t current_keys_index_diff = 0;

	/* Load storage information relevant to the keys module. */
	err = fmna_storage_pairing_item_load(FMNA_STORAGE_MASTER_PUBLIC_KEY_ID,
					     master_pk,
					     sizeof(master_pk));
	if (err) {
		LOG_ERR("fmna_keys: cannot load Master Public Key");
		return err;
	}

	err = fmna_storage_pairing_item_load(FMNA_STORAGE_PRIMARY_SK_ID,
					     curr_primary_sk,
					     sizeof(curr_primary_sk));
	if (err) {
		LOG_ERR("fmna_keys: cannot load Primary SK");
		return err;
	}

	err = fmna_storage_pairing_item_load(FMNA_STORAGE_SECONDARY_SK_ID,
					     curr_secondary_sk,
					     sizeof(curr_secondary_sk));
	if (err) {
		LOG_ERR("fmna_keys: cannot load Secondary SK");
		return err;
	}

	err = fmna_storage_pairing_item_load(FMNA_STORAGE_PRIMARY_KEY_INDEX_ID,
					     (uint8_t *) &primary_pk_rotation_cnt,
					     sizeof(primary_pk_rotation_cnt));
	if (err) {
		LOG_ERR("fmna_keys: cannot load the Primary Key index");
		return err;
	}

	err = fmna_storage_pairing_item_load(FMNA_STORAGE_CURRENT_KEYS_INDEX_DIFF_ID,
					     (uint8_t *) &current_keys_index_diff,
					     sizeof(current_keys_index_diff));
	if (err) {
		LOG_ERR("fmna_keys: cannot load the diff between current and storage key");
		return err;
	}

	/* Roll keys to the current index. */
	LOG_DBG("Restoring FMN keys state. Rolling index: %d -> %d",
		primary_pk_rotation_cnt,
		primary_pk_rotation_cnt + current_keys_index_diff);

	start_time = k_uptime_get();

	/* Roll symmetric keys. */
	for (uint32_t rolls = 0; rolls < current_keys_index_diff; rolls++) {
		/* Primary SK i -> Primary SK i + 1 */
		err = symmetric_key_roll(curr_primary_sk);
		if (err) {
			LOG_ERR("symmetric_key_roll returned error: %d for primary SK",
				err);
			return err;
		}

		primary_pk_rotation_cnt++;
		if ((primary_pk_rotation_cnt % PRIMARY_KEYS_PER_SECONDARY_KEY) != 0) {
			continue;
		}

		/* Secondary SK j -> Secondary SK j + 1 */
		err = symmetric_key_roll(curr_secondary_sk);
		if (err) {
			LOG_ERR("symmetric_key_roll returned error: %d for secondary SK", err);
			return err;
		}
	}
	secondary_pk_rotation_cnt = SECONDARY_KEY_INDEX_FROM_PRIMARY(primary_pk_rotation_cnt);

	/* Derive public keys and LTK. */
	/* SK(i+1) -> Primary_Key(i+1) */
	err = fm_crypto_derive_primary_or_secondary_x(curr_primary_sk, master_pk, curr_primary_pk);
	if (err) {
		LOG_ERR("symmetric_key_roll returned error: %d for primary SK", err);
		return err;
	}

	/* SK(i+1) -> LTK(i+1) */
	err = fm_crypto_derive_ltk(curr_primary_sk, bt_ltk);
	if (err) {
		LOG_ERR("symmetric_key_roll returned error: %d for primary SK", err);
		return err;
	}

	/* SK(i+1) -> Secondary_Key(i+1) */
	err = fm_crypto_derive_primary_or_secondary_x(curr_secondary_sk, master_pk,
						      curr_secondary_pk);
	if (err) {
		LOG_ERR("symmetric_key_roll returned error: %d for secondary SK", err);
		return err;
	}

	/* Log the results and statistics */
	duration = k_uptime_delta(&start_time);

	LOG_DBG("Restored FMN keys state in: %lld.%lld [s]",
		(duration / 1000), (duration % 1000));

	snprintk(hexdump_header,
		 sizeof(hexdump_header), 
		 "Restored Primary Public Key to: P[%d]:",
		 primary_pk_rotation_cnt);
	LOG_HEXDUMP_DBG(curr_primary_pk, sizeof(curr_primary_pk), hexdump_header);

	snprintk(hexdump_header,
		 sizeof(hexdump_header), 
		 "Restored Secondary Public Key: PW[%d]",
		 secondary_pk_rotation_cnt);
	LOG_HEXDUMP_DBG(curr_secondary_pk, sizeof(curr_secondary_pk), hexdump_header);

	/* Use the secondary key as a separated key. */
	use_secondary_pk = true;

	/* Start key rotation service timer. */
	keys_service_timer_start();

	return 0;
}

int fmna_keys_init(uint8_t id, bool is_paired)
{
	bt_id = id;
	key_rotation_timer_period = KEY_ROTATION_TIMER_PERIOD;

	if (is_paired) {
		int err = paired_state_restore();
		if (err) {
			LOG_ERR("paired_state_restore returned error: %d", err);
			return err;
		}
	}

	return 0;
}

static void inline primary_pk_latch(void)
{
	memcpy(latched_primary_pk, curr_primary_pk, sizeof(latched_primary_pk));
	is_primary_pk_latched = true;

	LOG_DBG("Current Primary Key: P[%d] is latched", primary_pk_rotation_cnt);
}

static void separated_key_latch_request_handle(struct bt_conn *conn)
{
	int err;
	NET_BUF_SIMPLE_DEFINE(resp_buf, sizeof(primary_pk_rotation_cnt));

	LOG_INF("FMN Config CP: responding to separated key latch request");

	/* No need for synchronization since event processing context uses
	 * system workqueue.
	 */
	primary_pk_latch();

	net_buf_simple_add_le32(&resp_buf, primary_pk_rotation_cnt);
	err = fmna_gatt_config_cp_indicate(conn,
					   FMNA_GATT_CONFIG_SEPARATED_KEY_LATCHED_IND,
					   &resp_buf);
	if (err) {
		LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
	}
}

static void secondary_key_eval_index_reconfigure(uint32_t secondary_key_eval_index)
{
	if (secondary_key_eval_index <= primary_pk_rotation_cnt) {
		/* Latch the current primary key til the next secondary key rotation. */
		primary_pk_latch();

		secondary_key_eval_index += PRIMARY_KEYS_PER_SECONDARY_KEY;
	}

	secondary_pk_rotation_delta =
		(secondary_key_eval_index % PRIMARY_KEYS_PER_SECONDARY_KEY);

	LOG_DBG("Next secondary key rotation index reconfigured to: %d",
		secondary_key_eval_index);
}

static void primary_key_roll_reconfigure(uint32_t primary_key_roll)
{
	const k_timeout_t one_time_duration = K_MSEC(primary_key_roll);

	LOG_DBG("Next rotation timer timeout reconfigured to: %d [ms]", primary_key_roll);

	k_timer_start(&key_rotation_timer, one_time_duration, key_rotation_timer_period);
}

static void separated_state_configure_request_handle(struct bt_conn *conn,
						     uint32_t secondary_key_eval_index,
						     uint32_t primary_key_roll)
{
	int err;
	uint16_t resp_opcode;
	uint16_t resp_status = FMNA_GATT_RESPONSE_STATUS_SUCCESS;

	LOG_INF("FMN Config CP: responding to separated state configure request");

	const uint32_t sk_eval_index_lower_bound =
		(primary_pk_rotation_cnt > SECONDARY_KEY_EVAL_INDEX_LOWER_BOUND) ?
		(primary_pk_rotation_cnt - SECONDARY_KEY_EVAL_INDEX_LOWER_BOUND) : 0;
	const uint32_t sk_eval_index_upper_bound =
		(primary_pk_rotation_cnt + PRIMARY_KEYS_PER_SECONDARY_KEY);
	if ((secondary_key_eval_index < sk_eval_index_lower_bound) ||
	    (secondary_key_eval_index > sk_eval_index_upper_bound)) {
		LOG_WRN("Invalid secondary key evaluation index: %d", secondary_key_eval_index);
		resp_status = FMNA_GATT_RESPONSE_STATUS_INVALID_PARAM;
	}

	if (K_MSEC(primary_key_roll).ticks > key_rotation_timer_period.ticks) {
		LOG_WRN("Invalid primary key roll period: %d", primary_key_roll);
		resp_status = FMNA_GATT_RESPONSE_STATUS_INVALID_PARAM;
	}

	if (resp_status == FMNA_GATT_RESPONSE_STATUS_SUCCESS) {
		secondary_key_eval_index_reconfigure(secondary_key_eval_index);
		primary_key_roll_reconfigure(primary_key_roll);
	}

	resp_opcode = fmna_config_event_to_gatt_cmd_opcode(
		FMNA_CONFIG_EVENT_CONFIGURE_SEPARATED_STATE);
	FMNA_GATT_COMMAND_RESPONSE_BUILD(resp_buf, resp_opcode, resp_status);
	err = fmna_gatt_config_cp_indicate(conn, FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND, &resp_buf);
	if (err) {
		LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
	}
}

static void current_primary_key_request_handle(struct bt_conn *conn)
{
	int err;
	struct net_buf_simple pk_rsp_buf;
	uint8_t primary_pk[FMNA_PUBLIC_KEY_LEN];

	LOG_INF("FMN Owner CP: responding to current Primary Key request");

	if (fmna_state_is_paired()) {
		memcpy(primary_pk, curr_primary_pk, sizeof(primary_pk));
	} else {
		memset(primary_pk, 0, sizeof(primary_pk));
	}

	net_buf_simple_init_with_data(&pk_rsp_buf, primary_pk, sizeof(primary_pk));

	err = fmna_gatt_owner_cp_indicate(
		conn, FMNA_GATT_OWNER_PRIMARY_KEY_IND, &pk_rsp_buf);
	if (err) {
		LOG_ERR("fmna_keys: fmna_gatt_owner_cp_indicate returned error: %d", err);
		return;
	}
}

#if CONFIG_FMNA_QUALIFICATION
static void set_key_rotation_request_handle(struct bt_conn *conn, uint32_t key_rotation_timeout)
{
	int err;
	uint16_t resp_opcode;
	uint16_t resp_status = FMNA_GATT_RESPONSE_STATUS_SUCCESS;

	LOG_INF("FMN Debug CP: responding to set key rotation timeout request: %d [ms]",
		key_rotation_timeout);

	key_rotation_timer_period = K_MSEC(key_rotation_timeout);

	/* Restart key rotation timeout. */
	k_timer_start(&key_rotation_timer, key_rotation_timer_period, key_rotation_timer_period);

	resp_opcode = fmna_debug_event_to_gatt_cmd_opcode(
		FMNA_DEBUG_EVENT_SET_KEY_ROTATION_TIMEOUT);
	FMNA_GATT_COMMAND_RESPONSE_BUILD(resp_buf, resp_opcode, resp_status);
	err = fmna_gatt_debug_cp_indicate(conn, FMNA_GATT_DEBUG_COMMAND_RESPONSE_IND, &resp_buf);
	if (err) {
		LOG_ERR("fmna_gatt_debug_cp_indicate returned error: %d", err);
	}
}
#endif

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_fmna_event(aeh)) {
		struct fmna_event *event = cast_fmna_event(aeh);

		switch (event->id) {
		case FMNA_EVENT_PEER_CONNECTED:
			fmna_peer_connected(event->conn);
			break;
		case FMNA_EVENT_PEER_DISCONNECTED:
			fmna_peer_disconnected(event->conn);
			break;
		case FMNA_EVENT_PEER_SECURITY_CHANGED:
			fmna_peer_security_changed(event->conn,
						   event->peer_security_changed.level,
						   event->peer_security_changed.err);
			break;
		default:
			break;
		}

		return false;
	}

	if (is_fmna_config_event(aeh)) {
		struct fmna_config_event *event = cast_fmna_config_event(aeh);

		switch (event->id) {
		case FMNA_CONFIG_EVENT_LATCH_SEPARATED_KEY:
			separated_key_latch_request_handle(event->conn);
			break;
		case FMNA_CONFIG_EVENT_CONFIGURE_SEPARATED_STATE:
			separated_state_configure_request_handle(
				event->conn,
				event->separated_state.seconday_key_evaluation_index,
				event->separated_state.next_primary_key_roll);
			break;
		default:
			break;
		}

		return false;
	}

	if (is_fmna_owner_event(aeh)) {
		struct fmna_owner_event *event = cast_fmna_owner_event(aeh);

		switch (event->id) {
		case FMNA_OWNER_EVENT_GET_CURRENT_PRIMARY_KEY:
			current_primary_key_request_handle(event->conn);
			break;
		default:
			break;
		}

		return false;
	}

#if CONFIG_FMNA_QUALIFICATION
	if (is_fmna_debug_event(aeh)) {
		struct fmna_debug_event *event = cast_fmna_debug_event(aeh);

		switch (event->id) {
		case FMNA_DEBUG_EVENT_SET_KEY_ROTATION_TIMEOUT:
			set_key_rotation_request_handle(event->conn,
							event->key_rotation_timeout);
			break;
		default:
			break;
		}

		return false;
	}
#endif

	return false;
}

APP_EVENT_LISTENER(fmna_keys, app_event_handler);
APP_EVENT_SUBSCRIBE_EARLY(fmna_keys, fmna_event);
APP_EVENT_SUBSCRIBE(fmna_keys, fmna_config_event);
APP_EVENT_SUBSCRIBE(fmna_keys, fmna_owner_event);

#if CONFIG_FMNA_QUALIFICATION
APP_EVENT_SUBSCRIBE(fmna_keys, fmna_debug_event);
#endif
