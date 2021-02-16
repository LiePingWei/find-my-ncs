#include "crypto/fm_crypto.h"
#include "events/fmna_event.h"
#include "fmna_keys.h"

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

static uint8_t ble_ltk[16];

static uint32_t primary_pk_rotation_cnt = 0;

static void key_rotation_work_handle(struct k_work *item);
static void key_rotation_timeout_handle(struct k_timer *timer_id);

K_WORK_DEFINE(key_rotation_work, key_rotation_work_handle);
K_TIMER_DEFINE(key_rotation_timer, key_rotation_timeout_handle, NULL);

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
	err = fm_crypto_derive_ltk(curr_primary_sk, ble_ltk);
	if (err) {
		LOG_ERR("symmetric_key_roll returned error: %d for primary SK", err);
		return err;
	}

	LOG_DBG("Rolling Primary Public Key to: P[%d]", primary_pk_rotation_cnt);
	LOG_HEXDUMP_DBG(curr_primary_pk, sizeof(curr_primary_pk), "Primary Public Key");

	/* TODO: Set active LTK in the BLE stack. */

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
