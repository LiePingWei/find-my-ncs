/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "events/fmna_event.h"
#include "events/fmna_config_event.h"
#include "fmna_adv.h"
#include "fmna_conn.h"
#include "fmna_gatt_fmns.h"
#include "fmna_keys.h"
#include "fmna_state.h"

#include <bluetooth/conn.h>
#include <power/reboot.h>

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

/* Timeout values in seconds */
#define NEARBY_SEPARATED_TIMEOUT_DEFAULT 30
#define NEARBY_SEPARATED_TIMEOUT_MAX     3600

struct disconnected_work {
	struct k_work work;
	struct bt_conn *conn;
	uint8_t reason;
} disconnected_work;

static enum fmna_state state = FMNA_STATE_UNDEFINED;
static struct bt_conn *fmn_paired_conn;
static bool is_bonded = false;
static bool is_maintained = false;
static bool unpair_pending = false;

static uint16_t nearby_separated_timeout = NEARBY_SEPARATED_TIMEOUT_DEFAULT;

static void nearby_separated_work_handle(struct k_work *item);
static void nearby_separated_timeout_handle(struct k_timer *timer_id);

static K_WORK_DEFINE(nearby_separated_work, nearby_separated_work_handle);
static K_TIMER_DEFINE(nearby_separated_timer, nearby_separated_timeout_handle, NULL);

#if CONFIG_FMNA_QUALIFICATION
static void reset_work_handle(struct k_work *item);
static struct k_delayed_work reset_work;
#endif

static int nearby_adv_start(void)
{
	int err;
	struct fmna_adv_nearby_config config;

	fmna_keys_primary_key_get(config.primary_key);

	config.is_maintained = is_maintained;

	err = fmna_adv_start_nearby(&config);
	if (err) {
		LOG_ERR("fmna_adv_start_nearby returned error: %d", err);
		return err;
	}

	LOG_DBG("Nearby advertising started");

	return err;
}

static int separated_adv_start(void)
{
	int err;
	struct fmna_adv_separated_config config;

	err = fmna_keys_primary_key_get(config.primary_key);
	if (err) {
		LOG_ERR("fmna_keys_primary_key_get returned error: %d", err);
		return err;
	}

	err = fmna_keys_separated_key_get(config.separated_key);
	if (err) {
		LOG_ERR("fmna_keys_separated_key_get returned error: %d", err);
		return err;
	}

	config.is_maintained = is_maintained;

	err = fmna_adv_start_separated(&config);
	if (err) {
		LOG_ERR("fmna_adv_start_separated returned error: %d", err);
		return err;
	}

	LOG_DBG("Separated advertising started");

	return err;
}

static const char *state_name_get(enum fmna_state state)
{
	switch (state) {
	case FMNA_STATE_UNPAIRED:
		return "Unpaired";
	case FMNA_STATE_CONNECTED:
		return "Connected";
	case FMNA_STATE_NEARBY:
		return "Nearby";
	case FMNA_STATE_SEPARATED:
		return "Separated";
	case FMNA_STATE_UNDEFINED:
		return "Undefined";
	default:
		return "Unknown";
	}
}

static int state_set(struct bt_conn *conn, enum fmna_state new_state)
{
	int err = 0;
	const char *state_str = state_name_get(new_state);
	enum fmna_state prev_state = state;

	if (prev_state == new_state) {
		LOG_DBG("FMN state: Unchanged");
		return 0;
	}

	state = new_state;

	/* Handle Unpaired state transition. */
	if (new_state == FMNA_STATE_UNPAIRED) {
		if ((prev_state != FMNA_STATE_CONNECTED) && (prev_state != FMNA_STATE_UNDEFINED)) {
			LOG_ERR("FMN State: Forbidden transition");
			return -EINVAL;
		}

		if (prev_state == FMNA_STATE_CONNECTED) {
			err = fmna_keys_service_stop();
			if (err) {
				LOG_ERR("fmna_keys_service_stop returned error: %d", err);
				return err;
			}

			err = fmna_storage_pairing_data_delete();
			if (err) {
				LOG_ERR("fmna_storage_pairing_data_delete returned error: %d",
					err);
				return err;
			}

			unpair_pending = false;
		}

		err = fmna_adv_start_unpaired(true);
		if (err) {
			LOG_ERR("fmna_adv_start_unpaired returned error: %d", err);
			return err;
		}

		fmn_paired_conn = NULL;
	} else if (new_state == FMNA_STATE_CONNECTED) {
		/* Handle Connected state transition. */

		if (prev_state == FMNA_STATE_NEARBY) {
			k_timer_stop(&nearby_separated_timer);
		}

		fmn_paired_conn = conn;
		is_maintained = true;
	} else if (new_state == FMNA_STATE_NEARBY) {
		/* Handle Nearby state transition. */

		if (prev_state == FMNA_STATE_CONNECTED) {
			fmna_conn_multi_status_bit_clear(
				fmn_paired_conn, FMNA_CONN_MULTI_STATUS_BIT_OWNER_CONNECTED);

			fmn_paired_conn = NULL;
		} else {
			LOG_ERR("FMN State: Forbidden transition");
			return -EINVAL;
		}

		if (nearby_separated_timeout != 0) {
			k_timer_start(&nearby_separated_timer,
				K_SECONDS(nearby_separated_timeout),
				K_NO_WAIT);

			err = nearby_adv_start();
			if (err) {
				LOG_ERR("nearby_adv_start returned error: %d", err);
				return err;
			}

			LOG_DBG("FMN State: Nearby");
		} else {
			err = state_set(NULL, FMNA_STATE_SEPARATED);
			if (err) {
				LOG_ERR("state_set returned error: %d", err);
				return err;
			}
		}
	} else if (new_state == FMNA_STATE_SEPARATED) {
		/* Handle Separated state transition. */

		if ((prev_state != FMNA_STATE_NEARBY) && (prev_state != FMNA_STATE_UNDEFINED)) {
			LOG_ERR("FMN State: Forbidden transition");
			return -EINVAL;
		}

		/* Use the primary public key from the Nearby advertising payload. */
		err = separated_adv_start();
		if (err) {
			LOG_ERR("separated_adv_start returned error: %d", err);
			return err;
		}

		/* Emit event notifying that the device is in the Separated state. */
		FMNA_EVENT_CREATE(event, FMNA_EVENT_SEPARATED, NULL);
		EVENT_SUBMIT(event);
	}

	if (prev_state == FMNA_STATE_UNDEFINED) {
		LOG_DBG("Initializing FMN State to: %s", state_str);
	} else {
		LOG_DBG("Changing FMN State to: %s", state_str);
	}

	return 0;
}

static void nearby_separated_work_handle(struct k_work *item)
{
	state_set(NULL, FMNA_STATE_SEPARATED);
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

	if ((state == FMNA_STATE_CONNECTED) && (fmn_paired_conn == conn)) {
		LOG_DBG("Disconnected from the Owner (reason %u)", reason);

		state_set(conn, (unpair_pending ? FMNA_STATE_UNPAIRED : FMNA_STATE_NEARBY));

		return;
	}

	LOG_DBG("Disconnected (reason %u)", reason);

	if (state == FMNA_STATE_UNPAIRED) {
		err = fmna_adv_start_unpaired(false);
		if (err) {
			LOG_ERR("fmna_adv_start_unpaired returned error: %d", err);
			return;
		}
	} else if (state == FMNA_STATE_NEARBY) {
		err = nearby_adv_start();
		if (err) {
			LOG_ERR("nearby_adv_start returned error: %d", err);
			return;
		}
	} else if (state == FMNA_STATE_SEPARATED) {
		err = separated_adv_start();
		if (err) {
			LOG_ERR("separated_adv_start returned error: %d", err);
			return;
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

int fmna_resume(void)
{
	int err;

	if (state == FMNA_STATE_UNDEFINED) {
		return -EINVAL;
	}

	if (state == FMNA_STATE_UNPAIRED) {
		err = fmna_adv_start_unpaired(false);
		if (err) {
			LOG_ERR("fmna_adv_start_unpaired returned error: %d", err);
			return err;
		}

		return 0;
	} else {
		return -EALREADY;
	}
}

enum fmna_state fmna_state_get(void)
{
	return state;
}

bool fmna_state_is_paired(void)
{
	return (fmna_state_get() != FMNA_STATE_UNPAIRED);
}

int fmna_state_init(uint8_t bt_id)
{
	int err;
	enum fmna_state state;
	static struct bt_conn_cb conn_callbacks = {
		.disconnected     = disconnected,
	};

	k_work_init(&disconnected_work.work, disconnected_work_handle);
#if CONFIG_FMNA_QUALIFICATION
	k_delayed_work_init(&reset_work, reset_work_handle);
#endif

	bt_conn_cb_register(&conn_callbacks);

	err = fmna_adv_init(bt_id);
	if (err) {
		LOG_ERR("fmna_adv_init returned error: %d", err);
		return err;
	}

	/* Initialize state. */
	state = is_bonded ? FMNA_STATE_SEPARATED : FMNA_STATE_UNPAIRED;
	err   = state_set(NULL, state);
	if (err) {
		LOG_ERR("state_set returned error: %d", err);
		return err;
	}

	return 0;
}

static void fmna_public_keys_changed(struct fmna_public_keys_changed *keys_changed)
{
	/* Set the maintained status. */
	is_maintained = (state == FMNA_STATE_CONNECTED);

	/* Restart the advertising with a new key payload. */
	if (state == FMNA_STATE_NEARBY) {
		nearby_adv_start();
	} else if (state == FMNA_STATE_SEPARATED) {
		if (keys_changed->separated_key_changed) {
			separated_adv_start();
		}
	}
}

static void nearby_timeout_set_request_handle(struct bt_conn *conn, uint16_t nearby_timeout)
{
	int err;
	uint16_t resp_opcode;
	uint16_t resp_status = FMNA_GATT_RESPONSE_STATUS_SUCCESS;

	LOG_INF("FMN Config CP: responding to nearby timeout set request");

	if (nearby_timeout > NEARBY_SEPARATED_TIMEOUT_MAX) {
		LOG_WRN("Invalid nearby timeout value: %d [s]", nearby_timeout);
		resp_status = FMNA_GATT_RESPONSE_STATUS_INVALID_PARAM;
	}

	if (resp_status == FMNA_GATT_RESPONSE_STATUS_SUCCESS) {
		nearby_separated_timeout = nearby_timeout;

		LOG_INF("Nearby Separated timeout reconfigured to: %d [s]",
			nearby_separated_timeout);
	}

	resp_opcode = fmna_config_event_to_gatt_cmd_opcode(FMNA_CONFIG_EVENT_SET_NEARBY_TIMEOUT);
	FMNA_GATT_COMMAND_RESPONSE_BUILD(cmd_buf, resp_opcode, resp_status);
	err = fmna_gatt_config_cp_indicate(conn, FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND, &cmd_buf);
	if (err) {
		LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
	}
}

static void unpair_request_handle(struct bt_conn *conn)
{
	int err;
	uint16_t resp_opcode;
	uint16_t resp_status = FMNA_GATT_RESPONSE_STATUS_SUCCESS;

	if (fmna_conn_connection_num_get() > 1) {
		resp_status = FMNA_GATT_RESPONSE_STATUS_INVALID_STATE;
	}

	if (resp_status == FMNA_GATT_RESPONSE_STATUS_SUCCESS) {
		unpair_pending = true;

		LOG_INF("Accepting the unpairing request");
	} else {
		LOG_WRN("Rejecting the unpairing request");
	}

	resp_opcode = fmna_config_event_to_gatt_cmd_opcode(FMNA_CONFIG_EVENT_UNPAIR);
	FMNA_GATT_COMMAND_RESPONSE_BUILD(resp_buf, resp_opcode, resp_status);
	err = fmna_gatt_config_cp_indicate(
		conn, FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND, &resp_buf);
	if (err) {
		LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
	}
}

static void utc_request_handle(struct bt_conn *conn, uint64_t utc)
{
	int err;
	uint16_t opcode = fmna_config_event_to_gatt_cmd_opcode(FMNA_CONFIG_EVENT_SET_UTC);

	/* TODO: Set UTC. */
	LOG_INF("FMN Config CP: responding to UTC settings request");

	FMNA_GATT_COMMAND_RESPONSE_BUILD(cmd_buf, opcode, FMNA_GATT_RESPONSE_STATUS_SUCCESS);
	err = fmna_gatt_config_cp_indicate(conn, FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND, &cmd_buf);
	if (err) {
		LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
	}
}

static void icloud_identifier_request_handle(struct bt_conn *conn)
{
	int err;
	struct net_buf_simple icloud_rsp_buf;
	uint8_t icloud_id[FMNA_ICLOUD_ID_LEN];

	LOG_INF("FMN Owner CP: responding to iCloud identifier request");

	err = fmna_storage_pairing_item_load(
		FMNA_STORAGE_ICLOUD_ID_ID, icloud_id, sizeof(icloud_id));
	if (err) {
		LOG_ERR("fmna_state: cannot load iCloud identifier");
		memset(icloud_id, 0, sizeof(icloud_id));
	}

	net_buf_simple_init_with_data(&icloud_rsp_buf, icloud_id, sizeof(icloud_id));

	err = fmna_gatt_owner_cp_indicate(
		conn, FMNA_GATT_OWNER_ICLOUD_ID_IND, &icloud_rsp_buf);
	if (err) {
		LOG_ERR("fmna_gatt_owner_cp_indicate returned error: %d", err);
		return;
	}
}

#if CONFIG_FMNA_QUALIFICATION
static void reset_work_handle(struct k_work *item)
{
	LOG_INF("Executing the debug reset command");

	sys_reboot(SYS_REBOOT_COLD);
}

static void reset_request_handle(struct bt_conn *conn)
{
	int err;
	uint16_t resp_opcode;
	uint16_t resp_status = FMNA_GATT_RESPONSE_STATUS_SUCCESS;

	LOG_INF("FMN Debug CP: responding to reset request");

	resp_opcode = fmna_debug_event_to_gatt_cmd_opcode(FMNA_DEBUG_EVENT_RESET);
	FMNA_GATT_COMMAND_RESPONSE_BUILD(resp_buf, resp_opcode, resp_status);
	err = fmna_gatt_debug_cp_indicate(conn, FMNA_GATT_DEBUG_COMMAND_RESPONSE_IND, &resp_buf);
	if (err) {
		LOG_ERR("fmna_gatt_debug_cp_indicate returned error: %d", err);
	}

	err = k_delayed_work_submit(&reset_work, K_MSEC(100));
	if (err) {
		LOG_ERR("fmna_state: k_delayed_work_submit returned error: %d", err);
	}
}
#endif

static bool event_handler(const struct event_header *eh)
{
	if (is_fmna_event(eh)) {
		struct fmna_event *event = cast_fmna_event(eh);

		switch (event->id) {
		case FMNA_EVENT_BONDED:
			is_bonded = true;
			break;
		case FMNA_EVENT_PAIRING_COMPLETED:
		case FMNA_EVENT_OWNER_CONNECTED:
			state_set(event->conn, FMNA_STATE_CONNECTED);
			break;
		case FMNA_EVENT_PUBLIC_KEYS_CHANGED:
			fmna_public_keys_changed(&event->public_keys_changed);
			break;
		default:
			break;
		}

		return false;
	}

	if (is_fmna_config_event(eh)) {
		struct fmna_config_event *event = cast_fmna_config_event(eh);

		switch (event->id) {
		case FMNA_CONFIG_EVENT_SET_NEARBY_TIMEOUT:
			nearby_timeout_set_request_handle(event->conn, event->nearby_timeout);
			break;
		case FMNA_CONFIG_EVENT_UNPAIR:
			unpair_request_handle(event->conn);
			break;
		case FMNA_CONFIG_EVENT_SET_UTC:
			utc_request_handle(event->conn, event->utc.current_time);
			break;
		default:
			break;
		}

		return false;
	}

	if (is_fmna_owner_event(eh)) {
		struct fmna_owner_event *event = cast_fmna_owner_event(eh);

		switch (event->id) {
		case FMNA_OWNER_EVENT_GET_ICLOUD_IDENTIFIER:
			icloud_identifier_request_handle(event->conn);
			break;
		default:
			break;
		}

		return false;
	}

#if CONFIG_FMNA_QUALIFICATION
	if (is_fmna_debug_event(eh)) {
		struct fmna_debug_event *event = cast_fmna_debug_event(eh);

		switch (event->id) {
		case FMNA_DEBUG_EVENT_RESET:
			reset_request_handle(event->conn);
			break;
		default:
			break;
		}

		return false;
	}
#endif

	return false;
}

EVENT_LISTENER(fmna_state, event_handler);
EVENT_SUBSCRIBE(fmna_state, fmna_event);
EVENT_SUBSCRIBE(fmna_state, fmna_config_event);
EVENT_SUBSCRIBE(fmna_state, fmna_owner_event);

#if CONFIG_FMNA_QUALIFICATION
EVENT_SUBSCRIBE(fmna_state, fmna_debug_event);
#endif
