#include "crypto/fm_crypto.h"
#include "events/fmna_event.h"
#include "fmna_conn.h"
#include "fmna_keys.h"

/* BLE internal header, use with caution. */
#include <bluetooth/host/keys.h>

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMN_ADK_LOG_LEVEL);

#define PRIMARY_TO_SECONDARY_KEY_PERIOD_INDEX(_primary_pk_index) \
	(((_primary_pk_index) / 96) + 1)

#define KEY_ROTATION_TIMER_PERIOD K_MINUTES(15)

static uint8_t master_pk[FMNA_MASTER_PUBLIC_KEY_LEN];
static uint8_t curr_primary_sk[FMNA_SYMMETRIC_KEY_LEN];
static uint8_t curr_secondary_sk[FMNA_SYMMETRIC_KEY_LEN];

static uint8_t curr_primary_pk[FMNA_PUBLIC_KEY_LEN];
static uint8_t curr_secondary_pk[FMNA_PUBLIC_KEY_LEN];

static uint32_t primary_pk_rotation_cnt = 0;

static bool is_paired = false;

/* Declaration of variables that are relevant to the BLE stack. */
static uint8_t bt_id;
static uint8_t bt_ltk[16];


static void key_rotation_work_handle(struct k_work *item);
static void key_rotation_timeout_handle(struct k_timer *timer_id);

K_WORK_DEFINE(key_rotation_work, key_rotation_work_handle);
K_TIMER_DEFINE(key_rotation_timer, key_rotation_timeout_handle, NULL);

static void bt_ltk_set(const bt_addr_le_t *bt_owner_addr)
{
	struct bt_keys *curr_keys;
	struct bt_ltk new_ltk;

	curr_keys = bt_keys_get_addr(bt_id, bt_owner_addr);
	if (!curr_keys) {
		LOG_ERR("bt_ltk_set: Owner key set cannot be found");
		return;
	}

	/* Set a proper key properties for the newly created keyset. */
	curr_keys->keys = BT_KEYS_LTK_P256;
	curr_keys->enc_size = sizeof(curr_keys->ltk.val);

	/* Configure the new LTK. EDIV and Rand values are set to 0. */
	memset(&new_ltk, 0, sizeof(new_ltk));
	memcpy(new_ltk.val, bt_ltk, sizeof(new_ltk.val));

	/*
	 * Update the LTK value in the BLE stack.
	 * TODO: Check if it makes sense to guard against data corruption with mutexes, etc.
	 */
	memcpy(&curr_keys->ltk, &new_ltk, sizeof(curr_keys->ltk));

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

	LOG_DBG("Rolling Secondary Public Key to: PW[%d]",
		PRIMARY_TO_SECONDARY_KEY_PERIOD_INDEX(primary_pk_rotation_cnt));
	LOG_HEXDUMP_DBG(curr_secondary_pk, sizeof(curr_secondary_pk), "Secondary Public Key");

	return 0;
}

static void key_rotation_work_handle(struct k_work *item)
{
	int err;
	bool secondary_key_changed;
	uint32_t new_secondary_pk_rotation_cnt;
	uint32_t last_secondary_pk_rotation_cnt =
		PRIMARY_TO_SECONDARY_KEY_PERIOD_INDEX(primary_pk_rotation_cnt);

	LOG_INF("Rotating FMNA keys");

	err = primary_key_roll();
	if (err) {
		LOG_ERR("primary_key_roll returned error: %d", err);
		return;
	}

	secondary_key_changed = false;
	new_secondary_pk_rotation_cnt =
		PRIMARY_TO_SECONDARY_KEY_PERIOD_INDEX(primary_pk_rotation_cnt);
	if (last_secondary_pk_rotation_cnt < new_secondary_pk_rotation_cnt) {
		err = secondary_key_roll();
		if (err) {
			LOG_ERR("secondary_key_roll returned error: %d", err);
			return;
		}

		secondary_key_changed = true;
	} else if (last_secondary_pk_rotation_cnt > new_secondary_pk_rotation_cnt) {
		LOG_ERR("key_rotation_handler: secondary Public Key index is too great");
	} else {
		/* The secondary Public Key update is omitted. */
	}

	/* Emit event notifying that the Public Keys have changed. */
	FMNA_EVENT_CREATE(event, FMNA_PUBLIC_KEYS_CHANGED, NULL);
	event->public_keys_changed.secondary_key_changed = secondary_key_changed;
	EVENT_SUBMIT(event);
}

static void key_rotation_timeout_handle(struct k_timer *timer_id)
{
	k_work_submit(&key_rotation_work);
}

int fmna_keys_primary_key_get(uint8_t primary_pk[FMNA_PUBLIC_KEY_LEN])
{
	/* TODO: Use synchronisation primitive to prevent memory corruption
	 *	 during the copy operation.
	*/
	memcpy(primary_pk, curr_primary_pk, FMNA_PUBLIC_KEY_LEN);

	return 0;
}

int fmna_keys_secondary_key_get(uint8_t secondary_pk[FMNA_PUBLIC_KEY_LEN])
{
	/* TODO: Use synchronisation primitive to prevent memory corruption
	 *	 during the copy operation.
	*/
	memcpy(secondary_pk, curr_secondary_pk, FMNA_PUBLIC_KEY_LEN);

	return 0;
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
		struct bt_keys *curr_keys = bt_keys_get_addr(bt_id, bt_conn_get_dst(conn));
		if (curr_keys) {
			bt_keys_clear(curr_keys);
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

int fmna_keys_init(uint8_t id)
{
	static struct bt_conn_cb conn_callbacks = {
		.connected = connected,
		.security_changed = security_changed,
	};

	bt_conn_cb_register(&conn_callbacks);

	bt_id = id;

	return 0;
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

	return false;
}

EVENT_LISTENER(fmna_keys, event_handler);
EVENT_SUBSCRIBE(fmna_keys, fmna_event);
