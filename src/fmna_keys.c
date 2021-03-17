#include "crypto/fm_crypto.h"
#include "events/fmna_event.h"
#include "events/fmna_config_event.h"
#include "fmna_conn.h"
#include "fmna_gatt_fmns.h"
#include "fmna_keys.h"

/* BLE internal header, use with caution. */
#include <bluetooth/host/keys.h>

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMN_ADK_LOG_LEVEL);

#define PRIMARY_KEYS_PER_SECONDARY_KEY       96
#define SECONDARY_KEY_EVAL_INDEX_LOWER_BOUND 4

#define KEY_ROTATION_TIMER_PERIOD K_MINUTES(15)

static uint8_t master_pk[FMNA_MASTER_PUBLIC_KEY_LEN];
static uint8_t curr_primary_sk[FMNA_SYMMETRIC_KEY_LEN];
static uint8_t curr_secondary_sk[FMNA_SYMMETRIC_KEY_LEN];

static uint8_t curr_primary_pk[FMNA_PUBLIC_KEY_LEN];
static uint8_t curr_secondary_pk[FMNA_PUBLIC_KEY_LEN];

static uint8_t latched_primary_pk[FMNA_PUBLIC_KEY_LEN];
bool is_primary_pk_latched = false;

static uint32_t primary_pk_rotation_cnt = 0;
static uint32_t secondary_pk_rotation_delta = 0;

bool use_secondary_pk = false;

static bool is_paired = false;

/* Declaration of variables that are relevant to the BLE stack. */
static uint8_t bt_id;
static uint8_t bt_ltk[16];
struct bt_keys *bt_keys = NULL;


static void key_rotation_work_handle(struct k_work *item);
static void key_rotation_timeout_handle(struct k_timer *timer_id);

K_WORK_DEFINE(key_rotation_work, key_rotation_work_handle);
K_TIMER_DEFINE(key_rotation_timer, key_rotation_timeout_handle, NULL);

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

	LOG_DBG("Rolling Secondary Public Key: PW");
	LOG_HEXDUMP_DBG(curr_secondary_pk, sizeof(curr_secondary_pk), "Secondary Public Key");

	return 0;
}

static void key_rotation_work_handle(struct k_work *item)
{
	int err;
	bool separated_key_changed = true;

	LOG_INF("Rotating FMNA keys");

	err = primary_key_roll();
	if (err) {
		LOG_ERR("primary_key_roll returned error: %d", err);
		return;
	}

	if ((primary_pk_rotation_cnt % PRIMARY_KEYS_PER_SECONDARY_KEY) ==
	    secondary_pk_rotation_delta) {
		/* Reset the latched primary key. */
		is_primary_pk_latched = false;

		err = secondary_key_roll();
		if (err) {
			LOG_ERR("secondary_key_roll returned error: %d", err);
			return;
		}

		use_secondary_pk = true;
	} else {
		/* The secondary Public Key update is omitted. */
		if (!is_primary_pk_latched) {
			if (use_secondary_pk) {
				separated_key_changed = false;
			}
		}
	}

	/* Emit event notifying that the Public Keys have changed. */
	FMNA_EVENT_CREATE(event, FMNA_PUBLIC_KEYS_CHANGED, NULL);
	event->public_keys_changed.separated_key_changed = separated_key_changed;
	EVENT_SUBMIT(event);
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

void fmna_keys_nearby_state_notify(void)
{
	use_secondary_pk = false;
}

int fmna_keys_reset(const struct fmna_keys_init *init_keys)
{
	int err;

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

	/* Start key rotation timeout. */
	k_timer_start(&key_rotation_timer, KEY_ROTATION_TIMER_PERIOD, KEY_ROTATION_TIMER_PERIOD);

	return 0;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
		LOG_ERR("fmna_keys: connection failed (err %u)", err);
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("fmna_keys: connected %s", addr);

	if (is_paired) {
		/* TODO: Filter out non-FMN peers. */
		bt_ltk_set(bt_conn_get_dst(conn));
	}
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (is_paired) {
		/* TODO: Filter out non-FMN peers. */
		if (bt_keys && (bt_addr_le_cmp(&bt_keys->addr, bt_conn_get_dst(conn)) == 0)) {

			/* Reset BLE LTK key. */
			bt_keys_clear(bt_keys);
			bt_keys = NULL;

			if (err) {
				LOG_ERR("fmna_keys: security failed: %s level %u err %d\n",
					addr, level, err);
			} else {
				LOG_INF("fmna_keys: security changed: %s level %u",
					addr, level);

				fmna_conn_multi_status_bit_set(
					conn, FMNA_CONN_MULTI_STATUS_BIT_OWNER_CONNECTED);

				FMNA_EVENT_CREATE(event, FMNA_OWNER_CONNECTED, conn);
				EVENT_SUBMIT(event);
			}
		} else {
			LOG_WRN("fmna_keys: cannot clear FMN LTK from BLE stack key pool for %s",
				addr);
		}
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (is_paired) {
		if (bt_keys && (bt_addr_le_cmp(&bt_keys->addr, bt_conn_get_dst(conn)) == 0)) {
			/* Reset BLE LTK key. */
			bt_keys_clear(bt_keys);
			bt_keys = NULL;
		}
	}
}

int fmna_keys_init(uint8_t id)
{
	static struct bt_conn_cb conn_callbacks = {
		.connected = connected,
		.disconnected = disconnected,
		.security_changed = security_changed,
	};

	bt_conn_cb_register(&conn_callbacks);

	bt_id = id;

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

	k_timer_start(&key_rotation_timer, one_time_duration, KEY_ROTATION_TIMER_PERIOD);
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

	if (K_MSEC(primary_key_roll).ticks > KEY_ROTATION_TIMER_PERIOD.ticks) {
		LOG_WRN("Invalid primary key roll period: %d", primary_key_roll);
		resp_status = FMNA_GATT_RESPONSE_STATUS_INVALID_PARAM;
	}

	if (resp_status == FMNA_GATT_RESPONSE_STATUS_SUCCESS) {
		secondary_key_eval_index_reconfigure(secondary_key_eval_index);
		primary_key_roll_reconfigure(primary_key_roll);
	}

	resp_opcode = fmna_config_event_to_gatt_cmd_opcode(FMNA_CONFIGURE_SEPARATED_STATE);
	FMNA_GATT_COMMAND_RESPONSE_BUILD(resp_buf, resp_opcode, resp_status);
	err = fmna_gatt_config_cp_indicate(conn, FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND, &resp_buf);
	if (err) {
		LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
	}
}

static bool event_handler(const struct event_header *eh)
{
	if (is_fmna_event(eh)) {
		struct fmna_event *event = cast_fmna_event(eh);

		switch (event->id) {
		case FMNA_PAIRING_COMPLETED:
			is_paired = true;
			break;
		default:
			break;
		}

		return false;
	}

	if (is_fmna_config_event(eh)) {
		struct fmna_config_event *event = cast_fmna_config_event(eh);

		switch (event->op) {
		case FMNA_LATCH_SEPARATED_KEY:
			separated_key_latch_request_handle(event->conn);
			break;
		case FMNA_CONFIGURE_SEPARATED_STATE:
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

	return false;
}

EVENT_LISTENER(fmna_keys, event_handler);
EVENT_SUBSCRIBE(fmna_keys, fmna_event);
EVENT_SUBSCRIBE(fmna_keys, fmna_config_event);