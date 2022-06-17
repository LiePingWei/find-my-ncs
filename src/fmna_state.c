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
#include <sys/reboot.h>

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

/* Timeout values in seconds */
#define NEARBY_SEPARATED_TIMEOUT_DEFAULT 30
#define NEARBY_SEPARATED_TIMEOUT_MAX     3600
#define PERSISTENT_CONN_ADV_TIMEOUT      3
#define UNPAIRED_ADV_TIMEOUT             600

static enum fmna_state state = FMNA_STATE_DISABLED;
static bool is_adv_paused = false;
static bool is_maintained = false;
static bool unpair_pending = false;
static bool persistent_conn_adv = false;

static bool location_available;
static fmna_state_location_availability_changed_t location_availability_changed_cb;
static fmna_state_paired_state_changed_t paired_state_changed_cb;

static bool pairing_mode = false;
static fmna_state_pairing_mode_timeout_cb_t pairing_mode_timeout_cb;

static uint16_t nearby_separated_timeout = NEARBY_SEPARATED_TIMEOUT_DEFAULT;

static void nearby_separated_work_handle(struct k_work *item);
static void nearby_separated_timeout_handle(struct k_timer *timer_id);
static void persistent_conn_work_handle(struct k_work *item);
static void pairing_mode_timeout_work_handle(struct k_work *item);

static K_WORK_DEFINE(nearby_separated_work, nearby_separated_work_handle);
static K_TIMER_DEFINE(nearby_separated_timer, nearby_separated_timeout_handle, NULL);
static K_WORK_DELAYABLE_DEFINE(persistent_conn_work, persistent_conn_work_handle);
static K_WORK_DELAYABLE_DEFINE(pairing_mode_timeout_work, pairing_mode_timeout_work_handle);

#if CONFIG_FMNA_QUALIFICATION
static void reset_work_handle(struct k_work *item);
static K_WORK_DELAYABLE_DEFINE(reset_work, reset_work_handle);
#endif

static int unpaired_adv_start(bool change_address)
{
	int err;

	if (!pairing_mode) {
		LOG_DBG("Pairing mode is not enabled");

		return 0;
	}

	err = fmna_adv_start_unpaired(change_address);
	if (err) {
		LOG_ERR("fmna_adv_start_unpaired returned error: %d", err);
		return err;
	}

	return err;
}

static int nearby_adv_start(void)
{
	int err;
	struct fmna_adv_nearby_config config;

	if (is_adv_paused) {
		LOG_DBG("Nearby advertising is still paused");

		return 0;
	}

	fmna_keys_primary_key_get(config.primary_key);

	config.fast_mode = persistent_conn_adv;
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

	if (is_adv_paused) {
		LOG_DBG("Separated advertising is still paused");

		return 0;
	}

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

	config.fast_mode = persistent_conn_adv;
	config.is_maintained = is_maintained;

	err = fmna_adv_start_separated(&config);
	if (err) {
		LOG_ERR("fmna_adv_start_separated returned error: %d", err);
		return err;
	}

	LOG_DBG("Separated advertising started");

	return err;
}

static int advertise_restart_on_no_state_change(void)
{
	int err;

	if (!fmna_conn_limit_check()) {
		LOG_WRN("Trying to restart advertising on maximum connection limit");

		err = fmna_adv_stop();
		if (err) {
			LOG_ERR("fmna_adv_stop returned error: %d", err);
			return err;
		}

		return 0;
	}

	switch (state) {
	case FMNA_STATE_UNPAIRED:
		err = unpaired_adv_start(false);
		if (err) {
			LOG_ERR("unpaired_adv_start returned error: %d", err);
			return err;
		}
		break;
	case FMNA_STATE_CONNECTED:
	case FMNA_STATE_NEARBY:
		err = nearby_adv_start();
		if (err) {
			LOG_ERR("nearby_adv_start returned error: %d", err);
			return err;
		}
		break;
	case FMNA_STATE_SEPARATED:
		err = separated_adv_start();
		if (err) {
			LOG_ERR("separated_adv_start returned error: %d", err);
			return err;
		}
		break;
	case FMNA_STATE_DISABLED:
		__ASSERT(0, "FMN state must be enabled at this point");
		break;
	}

	return 0;
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
	case FMNA_STATE_DISABLED:
		return "Disabled";
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
		advertise_restart_on_no_state_change();

		return 0;
	}

	state = new_state;

	/* Handle Unpaired state transition. */
	if (new_state == FMNA_STATE_UNPAIRED) {
		if ((prev_state != FMNA_STATE_CONNECTED) && (prev_state != FMNA_STATE_DISABLED)) {
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
			persistent_conn_adv = false;
		}

		/* Restart the pairing mode when the accessory transitions into unpaired state. */
		pairing_mode = true;
		k_work_reschedule(&pairing_mode_timeout_work, K_SECONDS(UNPAIRED_ADV_TIMEOUT));

		err = unpaired_adv_start(true);
		if (err) {
			LOG_ERR("unpaired_adv_start returned error: %d", err);
			return err;
		}
	} else if (new_state == FMNA_STATE_CONNECTED) {
		/* Handle Connected state transition. */

		if (prev_state == FMNA_STATE_NEARBY) {
			k_timer_stop(&nearby_separated_timer);
		}

		if (prev_state == FMNA_STATE_UNPAIRED) {
			k_work_cancel_delayable(&pairing_mode_timeout_work);
		}

		is_maintained = true;

		if ((prev_state != FMNA_STATE_UNPAIRED) && fmna_conn_limit_check()) {
			err = nearby_adv_start();
			if (err) {
				LOG_ERR("nearby_adv_start returned error: %d", err);
				return err;
			}
		}
	} else if (new_state == FMNA_STATE_NEARBY) {
		/* Handle Nearby state transition. */

		if (prev_state == FMNA_STATE_CONNECTED) {
			fmna_conn_multi_status_bit_clear(
				conn, FMNA_CONN_MULTI_STATUS_BIT_OWNER_CONNECTED);

			if (fmna_conn_multi_status_bit_check(conn,
				FMNA_CONN_MULTI_STATUS_BIT_PERSISTENT_CONNECTION)) {
				k_work_reschedule(&persistent_conn_work,
						  K_SECONDS(PERSISTENT_CONN_ADV_TIMEOUT));

				persistent_conn_adv = true;

				LOG_DBG("Starting persistent connection advertising");
			}
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
		} else {
			err = state_set(NULL, FMNA_STATE_SEPARATED);
			if (err) {
				LOG_ERR("state_set returned error: %d", err);
				return err;
			}
		}
	} else if (new_state == FMNA_STATE_SEPARATED) {
		/* Handle Separated state transition. */

		if ((prev_state != FMNA_STATE_NEARBY) && (prev_state != FMNA_STATE_DISABLED)) {
			LOG_ERR("FMN State: Forbidden transition");
			return -EINVAL;
		}

		/* Use the primary public key from the Nearby advertising payload. */
		err = separated_adv_start();
		if (err) {
			LOG_ERR("separated_adv_start returned error: %d", err);
			return err;
		}
	} else if (new_state == FMNA_STATE_DISABLED) {
		/* Stop all kernel objects */
		k_timer_stop(&nearby_separated_timer);

		/* No need to do anything if the cancellation operation fails
		 * and the workqueue item is already being executed.
		 */
		k_work_cancel(&nearby_separated_work);
		k_work_cancel_delayable(&persistent_conn_work);
		k_work_cancel_delayable(&pairing_mode_timeout_work);

		/* Stop the key service if necessary. */
		if (prev_state != FMNA_STATE_UNPAIRED) {
			err = fmna_keys_service_stop();
			if (err) {
				LOG_ERR("fmna_keys_service_stop returned error: %d", err);
				return err;
			}
		}

		/* Reset the FMN state. */
		is_adv_paused = false;
		is_maintained = false;
		unpair_pending = false;
		persistent_conn_adv = false;
		pairing_mode = false;
	}

	if (prev_state == FMNA_STATE_DISABLED) {
		LOG_DBG("Initializing FMN State to: %s", state_str);
	} else if (new_state == FMNA_STATE_DISABLED) {
		LOG_DBG("Uninitializing FMN State");
	} else {
		LOG_DBG("Changing FMN State to: %s", state_str);
	}

	if (location_availability_changed_cb) {
		bool is_location_available =
			(new_state == FMNA_STATE_NEARBY) ||
			(new_state == FMNA_STATE_SEPARATED);

		if (location_available != is_location_available) {
			location_available = is_location_available;
			location_availability_changed_cb(is_location_available);
		}
	}

	if (paired_state_changed_cb) {
		bool has_pairing_state_changed = false;

		has_pairing_state_changed |= (prev_state == FMNA_STATE_DISABLED);
		has_pairing_state_changed |= (prev_state == FMNA_STATE_UNPAIRED);
		has_pairing_state_changed |= (new_state == FMNA_STATE_UNPAIRED);
		has_pairing_state_changed &= (new_state != FMNA_STATE_DISABLED);

		if (has_pairing_state_changed) {
			paired_state_changed_cb(fmna_state_is_paired());
		}
	}

	/* Emit event notifying that the device is in the new state. */
	FMNA_EVENT_CREATE(event, FMNA_EVENT_STATE_CHANGED, NULL);
	APP_EVENT_SUBMIT(event);

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

static void persistent_conn_work_handle(struct k_work *item)
{
	if (!persistent_conn_adv) {
		return;
	}

	LOG_DBG("Stopping persistent connection advertising");

	persistent_conn_adv = false;

	advertise_restart_on_no_state_change();
}

static void pairing_mode_timeout_work_handle(struct k_work *item)
{
	LOG_DBG("Pairing mode timeout");

	fmna_adv_stop();

	pairing_mode = false;

	if (pairing_mode_timeout_cb) {
		pairing_mode_timeout_cb();
	}
}

static bool all_owners_disconnected(struct bt_conn *conn)
{
	int err;
	struct bt_conn *owners[CONFIG_BT_MAX_CONN];
	uint8_t owners_num = ARRAY_SIZE(owners);

	if (state == FMNA_STATE_CONNECTED) {
		err = fmna_conn_owner_find(owners, &owners_num);
		if (err) {
			LOG_ERR("fmna_conn_owner_find returned error: %d", err);
		}

		for (size_t i = 0; i < owners_num; i++) {
			if (owners[i] != conn) {
				return false;
			}
		}

		return true;
	}

	return false;
}

static void fmna_peer_connected(struct bt_conn *conn)
{
	advertise_restart_on_no_state_change();
}

static void fmna_peer_disconnected(struct bt_conn *conn)
{
	if (all_owners_disconnected(conn)) {
		LOG_DBG("Disconnected from the last connected Owner");

		state_set(conn, (unpair_pending ? FMNA_STATE_UNPAIRED : FMNA_STATE_NEARBY));

		return;
	}

	advertise_restart_on_no_state_change();
}

int fmna_state_pause(void)
{
	int err;

	if (state == FMNA_STATE_DISABLED) {
		return -EINVAL;
	}

	is_adv_paused = true;

	err = fmna_adv_stop();
	if (err) {
		LOG_ERR("fmna_adv_stop returned error: %d", err);
		return err;
	}

	LOG_DBG("Pausing FMN advertising");

	return 0;
}

int fmna_state_resume(void)
{
	if (state == FMNA_STATE_DISABLED) {
		return -EINVAL;
	}

	is_adv_paused = false;

	return advertise_restart_on_no_state_change();
}

int fmna_resume(void)
{
	int err;

	if (state != FMNA_STATE_UNPAIRED) {
		return -EINVAL;
	}

	pairing_mode = true;
	k_work_reschedule(&pairing_mode_timeout_work, K_SECONDS(UNPAIRED_ADV_TIMEOUT));

	err = unpaired_adv_start(true);
	if (err) {
		LOG_ERR("unpaired_adv_start returned error: %d", err);
		return err;
	}

	return err;
}

enum fmna_state fmna_state_get(void)
{
	return state;
}

bool fmna_state_is_paired(void)
{
	enum fmna_state current_state = fmna_state_get();

	return ((current_state != FMNA_STATE_UNPAIRED) &&
		(current_state != FMNA_STATE_DISABLED));
}

bool fmna_state_is_enabled(void)
{
	return (fmna_state_get() != FMNA_STATE_DISABLED);
}

int fmna_state_init(uint8_t bt_id, bool is_paired)
{
	int err;
	enum fmna_state state;

	err = fmna_adv_init(bt_id);
	if (err) {
		LOG_ERR("fmna_adv_init returned error: %d", err);
		return err;
	}

	/* Set the location available variable so that the first callback
	 * is triggered during the initialization.
	 */
	location_available = !is_paired;

	/* Initialize state. */
	state = is_paired ? FMNA_STATE_SEPARATED : FMNA_STATE_UNPAIRED;
	err   = state_set(NULL, state);
	if (err) {
		LOG_ERR("state_set returned error: %d", err);
		return err;
	}

	return 0;
}

int fmna_state_uninit(void)
{
	int err;

	err = state_set(NULL, FMNA_STATE_DISABLED);
	if (err) {
		LOG_ERR("state_set returned error: %d", err);
		return err;
	}

	err = fmna_adv_uninit();
	if (err) {
		LOG_ERR("fmna_adv_uninit returned error: %d", err);
		return err;
	}

	return 0;
}

int fmna_state_location_availability_cb_register(
	fmna_state_location_availability_changed_t cb)
{
	location_availability_changed_cb = cb;

	return 0;
}

int fmna_state_paired_state_changed_cb_register(
	fmna_state_paired_state_changed_t cb)
{
	paired_state_changed_cb = cb;

	return 0;
}

int fmna_state_pairing_mode_timeout_cb_register(fmna_state_pairing_mode_timeout_cb_t cb)
{
	if (!cb) {
		return -EINVAL;
	}

	pairing_mode_timeout_cb = cb;

	return 0;
}

static void fmna_public_keys_changed(struct fmna_public_keys_changed *keys_changed)
{
	/* Set the maintained status. */
	is_maintained = (state == FMNA_STATE_CONNECTED);

	/* Restart the advertising with a new key payload. */
	if (state == FMNA_STATE_UNPAIRED) {
		return;
	}

	if ((state == FMNA_STATE_SEPARATED) &&
	    !keys_changed->separated_key_changed) {
		return;
	}

	advertise_restart_on_no_state_change();
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

	k_work_reschedule(&reset_work, K_MSEC(100));
}
#endif

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_fmna_event(aeh)) {
		struct fmna_event *event = cast_fmna_event(aeh);

		switch (event->id) {
		case FMNA_EVENT_MAX_CONN_CHANGED:
			advertise_restart_on_no_state_change();
			break;
		case FMNA_EVENT_PAIRING_COMPLETED:
		case FMNA_EVENT_OWNER_CONNECTED:
			state_set(event->conn, FMNA_STATE_CONNECTED);
			break;
		case FMNA_EVENT_PEER_CONNECTED:
			fmna_peer_connected(event->conn);
			break;
		case FMNA_EVENT_PEER_DISCONNECTED:
			fmna_peer_disconnected(event->conn);
			break;
		case FMNA_EVENT_PUBLIC_KEYS_CHANGED:
			fmna_public_keys_changed(&event->public_keys_changed);
			break;
		default:
			break;
		}

		return false;
	}

	if (is_fmna_config_event(aeh)) {
		struct fmna_config_event *event = cast_fmna_config_event(aeh);

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

	if (is_fmna_owner_event(aeh)) {
		struct fmna_owner_event *event = cast_fmna_owner_event(aeh);

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
	if (is_fmna_debug_event(aeh)) {
		struct fmna_debug_event *event = cast_fmna_debug_event(aeh);

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

APP_EVENT_LISTENER(fmna_state, app_event_handler);
APP_EVENT_SUBSCRIBE(fmna_state, fmna_event);
APP_EVENT_SUBSCRIBE(fmna_state, fmna_config_event);
APP_EVENT_SUBSCRIBE(fmna_state, fmna_owner_event);

#if CONFIG_FMNA_QUALIFICATION
APP_EVENT_SUBSCRIBE(fmna_state, fmna_debug_event);
#endif
