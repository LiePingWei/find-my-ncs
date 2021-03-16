#include "events/fmna_event.h"
#include "events/fmna_config_event.h"
#include "fmna_adv.h"
#include "fmna_gatt_fmns.h"
#include "fmna_keys.h"

#include <bluetooth/conn.h>

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMN_ADK_LOG_LEVEL);

#define NEARBY_SEPARATED_TIMER_PERIOD K_SECONDS(30)

enum fmna_state {
	UNPAIRED,
	CONNECTED,
	NEARBY,
	SEPARATED,
};

struct disconnected_work {
	struct k_work work;
	struct bt_conn *conn;
	uint8_t reason;
} disconnected_work;

static enum fmna_state state;
static struct bt_conn *fmn_paired_conn;

static void nearby_separated_work_handle(struct k_work *item);
static void nearby_separated_timeout_handle(struct k_timer *timer_id);

static K_WORK_DEFINE(nearby_separated_work, nearby_separated_work_handle);
static K_TIMER_DEFINE(nearby_separated_timer, nearby_separated_timeout_handle, NULL);

static void nearby_adv_start(void)
{
	int err;
	uint8_t primary_pk[FMNA_PUBLIC_KEY_LEN];

	fmna_keys_primary_key_get(primary_pk);

	err = fmna_adv_start_nearby(primary_pk);
	if (err) {
		LOG_ERR("fmna_adv_start_nearby returned error: %d", err);
		return;
	}

	LOG_DBG("Nearby advertising started");
}

static void separated_adv_start(void)
{
	int err;
	uint8_t primary_pk[FMNA_PUBLIC_KEY_LEN];
	uint8_t separated_pk[FMNA_PUBLIC_KEY_LEN];

	err = fmna_keys_primary_key_get(primary_pk);
	if (err) {
		LOG_ERR("fmna_keys_primary_key_get returned error: %d", err);
		return;
	}

	err = fmna_keys_separated_key_get(separated_pk);
	if (err) {
		LOG_ERR("fmna_keys_separated_key_get returned error: %d", err);
		return;
	}

	err = fmna_adv_start_separated(separated_pk, primary_pk[FMNA_ADV_SEPARATED_HINT_INDEX]);
	if (err) {
		LOG_ERR("fmna_adv_start_separated returned error: %d", err);
		return;
	}

	LOG_DBG("Separated advertising started");
}

static void state_set(struct bt_conn *conn, enum fmna_state new_state)
{
	enum fmna_state prev_state = state;

	if (prev_state == new_state) {
		LOG_DBG("FMN state: Unchanged");
		return;
	}

	state = new_state;

	/* Handle Connected state transition. */
	if (new_state == CONNECTED) {
		if (prev_state == NEARBY) {
			k_timer_stop(&nearby_separated_timer);
		}

		fmn_paired_conn = conn;

		LOG_DBG("FMN State: Connected");
		return;
	}

	/* Handle Nearby state transition. */
	if (new_state == NEARBY) {
		if (prev_state == CONNECTED) {
			fmn_paired_conn = NULL;
		} else {
			LOG_ERR("FMN State: Forbidden transition");
			return;
		}

		fmna_keys_nearby_state_notify();

		k_timer_start(&nearby_separated_timer, NEARBY_SEPARATED_TIMER_PERIOD, K_NO_WAIT);

		nearby_adv_start();

		LOG_DBG("FMN State: Nearby");
		return;
	}

	/* Handle Separated state transition. */
	if (new_state == SEPARATED) {
		if (prev_state != NEARBY) {
			LOG_ERR("FMN State: Forbidden transition");
			return;
		}

		/* Use the primary public key from the Nearby advertising payload. */
		separated_adv_start();

		LOG_DBG("FMN State: Separated");
		return;
	}
}

static void nearby_separated_work_handle(struct k_work *item)
{
	state_set(NULL, SEPARATED);
}

static void nearby_separated_timeout_handle(struct k_timer *timer_id)
{
	k_work_submit(&nearby_separated_work);
}

static void disconnected_work_handle(struct k_work *item)
{
	int err;
	struct disconnected_work *disconnect =
		CONTAINER_OF(item, struct disconnected_work, work);
	struct bt_conn *conn = disconnect->conn;
	uint8_t reason = disconnect->reason;

	if ((state == CONNECTED) && (fmn_paired_conn == conn)) {
		LOG_DBG("Disconnected from the Owner (reason %u)", reason);

		state_set(conn, NEARBY);
	} else {
		LOG_DBG("Disconnected (reason %u)", reason);

		if (state == UNPAIRED) {
			err = fmna_adv_start_unpaired();
			if (err) {
				LOG_ERR("fmna_adv_start_unpaired returned error: %d", err);
				return;
			}
		}
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	/*
	 * Process the disconnect logic in the workqueue so that
	 * the BLE stack is finished with the connection bookkeeping
	 * logic and advertising is possible.
	 */
	disconnected_work.conn = conn;
	disconnected_work.reason = reason;
	k_work_submit(&disconnected_work.work);
}

int fmna_state_init(uint8_t bt_id)
{
	int err;
	static struct bt_conn_cb conn_callbacks = {
		.disconnected     = disconnected,
	};

	k_work_init(&disconnected_work.work, disconnected_work_handle);

	bt_conn_cb_register(&conn_callbacks);

	err = fmna_adv_init(bt_id);
	if (err) {
		LOG_ERR("fmna_adv_init returned error: %d", err);
		return err;
	}

	/* Initialize state. */
	state = UNPAIRED;
	fmn_paired_conn = NULL;

	err = fmna_adv_start_unpaired();
	if (err) {
		LOG_ERR("fmna_adv_start_unpaired returned error: %d", err);
		return err;
	}

	return 0;
}

static void fmna_public_keys_changed(struct fmna_public_keys_changed *keys_changed)
{
	if (state == NEARBY) {
		nearby_adv_start();
	} else if (state == SEPARATED) {
		if (keys_changed->separated_key_changed) {
			separated_adv_start();
		}
	}
}

static void utc_request_handle(struct bt_conn *conn, uint64_t utc)
{
	int err;
	uint16_t opcode = fmna_config_event_to_gatt_cmd_opcode(FMNA_SET_UTC);

	/* TODO: Set UTC. */
	LOG_INF("FMN Config CP: responding to UTC settings request");

	FMNA_GATT_COMMAND_RESPONSE_BUILD(cmd_buf, opcode, FMNA_GATT_RESPONSE_STATUS_SUCCESS);
	err = fmna_gatt_config_cp_indicate(conn, FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND, &cmd_buf);
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
		case FMNA_OWNER_CONNECTED:
			state_set(event->conn, CONNECTED);
			break;
		case FMNA_PUBLIC_KEYS_CHANGED:
			fmna_public_keys_changed(&event->public_keys_changed);
			break;
		default:
			break;
		}

		return false;
	}

	if (is_fmna_config_event(eh)) {
		struct fmna_config_event *event = cast_fmna_config_event(eh);

		switch (event->op) {
		case FMNA_SET_UTC:
			utc_request_handle(event->conn, event->utc.current_time);
			break;
		default:
			break;
		}

		return false;
	}

	return false;
}

EVENT_LISTENER(fmna_state, event_handler);
EVENT_SUBSCRIBE(fmna_state, fmna_event);
EVENT_SUBSCRIBE(fmna_state, fmna_config_event);
