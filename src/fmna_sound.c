/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "events/fmna_event.h"
#include "events/fmna_config_event.h"
#include "events/fmna_non_owner_event.h"
#include "fmna_conn.h"
#include "fmna_gatt_fmns.h"

#include <fmna.h>

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

/* Timeout values in seconds */
#define SOUND_TIMEOUT K_SECONDS(10)

static const struct fmna_sound_cb *user_cb = NULL;

static bool play_sound_in_progress = false;
static struct bt_conn *sound_initiator = NULL;

static void sound_timeout_work_handle(struct k_work *item);

static K_WORK_DELAYABLE_DEFINE(sound_timeout_work, sound_timeout_work_handle);

static bool sound_stop_no_callback(struct bt_conn *conn)
{
	if (!play_sound_in_progress) {
		return false;
	}

	if (sound_initiator != conn) {
		return false;
	}

	play_sound_in_progress = false;
	k_work_cancel_delayable(&sound_timeout_work);

	if (conn) {
		fmna_conn_multi_status_bit_clear(
			conn, FMNA_CONN_MULTI_STATUS_BIT_PLAYING_SOUND);
	}

	FMNA_EVENT_CREATE(event, FMNA_EVENT_SOUND_COMPLETED, NULL);
	EVENT_SUBMIT(event);

	return true;
}

static bool sound_stop(struct bt_conn *conn)
{
	bool ret;

	__ASSERT(user_cb,
		"Sound callback structure is not registered. "
		"See fmna_sound_cb_register for details.");
	__ASSERT(user_cb->sound_stop,
		"The sound_start callback is not populated. "
		"See fmna_sound_cb_register for details.");

	ret = sound_stop_no_callback(conn);
	if (!ret) {
		return ret;
	}

	if (user_cb && user_cb->sound_stop) {
		user_cb->sound_stop();
	} else {
		LOG_ERR("The sound_stop callback is not populated");
	}

	return true;
}

bool sound_start(struct bt_conn *conn)
{
	enum fmna_sound_trigger sound_trigger;

	__ASSERT(user_cb,
		"Sound callback structure is not registered. "
		"See fmna_sound_cb_register for details.");
	__ASSERT(user_cb->sound_start,
		"The sound_start callback is not populated. "
		"See fmna_sound_cb_register for details.");

	if (play_sound_in_progress) {
		return false;
	}

	if (conn) {
		fmna_conn_multi_status_bit_set(
			conn, FMNA_CONN_MULTI_STATUS_BIT_PLAYING_SOUND);
		bt_conn_ref(conn);

		if (fmna_conn_multi_status_bit_check(
			conn, FMNA_CONN_MULTI_STATUS_BIT_OWNER_CONNECTED)) {
			sound_trigger = FMNA_SOUND_TRIGGER_OWNER;
		} else {
			sound_trigger = FMNA_SOUND_TRIGGER_NON_OWNER;
		}
	} else {
		sound_trigger = FMNA_SOUND_TRIGGER_UT_DETECTION;
	}

	play_sound_in_progress = true;
	sound_initiator        = conn;

	k_work_reschedule(&sound_timeout_work, SOUND_TIMEOUT);
	if (user_cb && user_cb->sound_start) {
		user_cb->sound_start(sound_trigger);
	} else {
		LOG_ERR("The sound_start callback is not populated");
	}

	return true;
}

static void sound_completed_indication_send(struct bt_conn *conn)
{
	int err;
	NET_BUF_SIMPLE_DEFINE(resp_buf, 0);

	if (!conn) {
		return;
	}

	if (fmna_conn_multi_status_bit_check(conn, FMNA_CONN_MULTI_STATUS_BIT_OWNER_CONNECTED)) {
		err = fmna_gatt_config_cp_indicate(
			conn, FMNA_GATT_CONFIG_SOUND_COMPLETED_IND, &resp_buf);
		if (err) {
			LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
		}
	} else {
		err = fmna_gatt_non_owner_cp_indicate(
			conn, FMNA_GATT_NON_OWNER_SOUND_COMPLETED_IND, &resp_buf);
		if (err) {
			LOG_ERR("fmna_gatt_non_owner_cp_indicate returned error: %d", err);
		}
	}

	bt_conn_unref(conn);
}

static void sound_timeout_work_handle(struct k_work *item)
{
	bool ret;

	ret = sound_stop(sound_initiator);
	if (ret && sound_initiator) {
		sound_completed_indication_send(sound_initiator);
	}
}

int fmna_sound_cb_register(const struct fmna_sound_cb *cb)
{
	if (!IS_ENABLED(CONFIG_FMNA_CAPABILITY_PLAY_SOUND_ENABLED)) {
		return -ENOTSUP;
	}

	if (user_cb) {
		return -EALREADY;
	}

	if (!cb) {
		return -EINVAL;
	}

	if (!cb->sound_start || !cb->sound_stop) {
		return -EINVAL;
	}

	user_cb = cb;
	return 0;
}

int fmna_sound_completed_indicate(void)
{
	int err;
	struct bt_conn *conn = sound_initiator;

	if (!IS_ENABLED(CONFIG_FMNA_CAPABILITY_PLAY_SOUND_ENABLED)) {
		return -ENOTSUP;
	}

	err = sound_stop_no_callback(sound_initiator) ? 0 : (-EINVAL);
	if (err) {
		return err;
	}

	sound_completed_indication_send(conn);

	return 0;
}

bool fmna_sound_start(void)
{
	return sound_start(NULL);
}

static void sound_command_response_send(struct bt_conn *conn,
					uint16_t opcode,
					enum fmna_gatt_response_status resp_status,
					bool is_owner)
{
	int err;

	if (resp_status == FMNA_GATT_RESPONSE_STATUS_INVALID_COMMAND) {
		LOG_INF("Play sound feature unsupported: rejecting related commands");
	}

	FMNA_GATT_COMMAND_RESPONSE_BUILD(cmd_buf, opcode, resp_status);

	if (is_owner) {
		err = fmna_gatt_config_cp_indicate(
			conn, FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND, &cmd_buf);
		if (err) {
			LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
		}
	} else {
		err = fmna_gatt_non_owner_cp_indicate(
			conn, FMNA_GATT_NON_OWNER_COMMAND_RESPONSE_IND, &cmd_buf);
		if (err) {
			LOG_ERR("fmna_gatt_non_owner_cp_indicate returned error: %d", err);
		}
	}
}

static void owner_start_sound_handle(struct bt_conn *conn)
{
	enum fmna_gatt_response_status resp_status;
	uint16_t opcode = fmna_config_event_to_gatt_cmd_opcode(
		FMNA_CONFIG_EVENT_START_SOUND);

	LOG_INF("FMN Config CP: responding to sound start request");

	if (IS_ENABLED(CONFIG_FMNA_CAPABILITY_PLAY_SOUND_ENABLED)) {
		resp_status = sound_start(conn) ?
			FMNA_GATT_RESPONSE_STATUS_SUCCESS :
			FMNA_GATT_RESPONSE_STATUS_INVALID_STATE;
	} else {
		resp_status = FMNA_GATT_RESPONSE_STATUS_INVALID_COMMAND;
	}

	sound_command_response_send(conn, opcode, resp_status, true);
}

static void owner_stop_sound_handle(struct bt_conn *conn)
{
	enum fmna_gatt_response_status resp_status;
	uint16_t opcode = fmna_config_event_to_gatt_cmd_opcode(
		FMNA_CONFIG_EVENT_STOP_SOUND);

	LOG_INF("FMN Config CP: responding to sound stop request");

	if (IS_ENABLED(CONFIG_FMNA_CAPABILITY_PLAY_SOUND_ENABLED)) {
		resp_status = sound_stop(conn) ?
			FMNA_GATT_RESPONSE_STATUS_SUCCESS :
			FMNA_GATT_RESPONSE_STATUS_INVALID_STATE;
	} else {
		resp_status = FMNA_GATT_RESPONSE_STATUS_INVALID_COMMAND;
	}

	if (resp_status != FMNA_GATT_RESPONSE_STATUS_SUCCESS) {
		sound_command_response_send(conn, opcode, resp_status, true);
	} else {
		sound_completed_indication_send(conn);
	}
}

static void non_owner_start_sound_handle(struct bt_conn *conn)
{
	enum fmna_gatt_response_status resp_status;
	uint16_t opcode = fmna_non_owner_event_to_gatt_cmd_opcode(
		FMNA_NON_OWNER_EVENT_START_SOUND);

	LOG_INF("FMN Non-owner CP: responding to sound start request");

	if (IS_ENABLED(CONFIG_FMNA_CAPABILITY_PLAY_SOUND_ENABLED)) {
		resp_status = sound_start(conn) ?
			FMNA_GATT_RESPONSE_STATUS_SUCCESS :
			FMNA_GATT_RESPONSE_STATUS_INVALID_STATE;
	} else {
		resp_status = FMNA_GATT_RESPONSE_STATUS_INVALID_COMMAND;
	}

	sound_command_response_send(conn, opcode, resp_status, false);
}

static void non_owner_stop_sound_handle(struct bt_conn *conn)
{
	enum fmna_gatt_response_status resp_status;
	uint16_t opcode = fmna_config_event_to_gatt_cmd_opcode(
		FMNA_NON_OWNER_EVENT_STOP_SOUND);

	LOG_INF("FMN Non-owner: responding to sound stop request");

	if (IS_ENABLED(CONFIG_FMNA_CAPABILITY_PLAY_SOUND_ENABLED)) {
		resp_status = sound_stop(conn) ?
			FMNA_GATT_RESPONSE_STATUS_SUCCESS :
			FMNA_GATT_RESPONSE_STATUS_INVALID_STATE;
	} else {
		resp_status = FMNA_GATT_RESPONSE_STATUS_INVALID_COMMAND;
	}

	if (resp_status != FMNA_GATT_RESPONSE_STATUS_SUCCESS) {
		sound_command_response_send(conn, opcode, resp_status, true);
	} else {
		sound_completed_indication_send(conn);
	}
}

static bool event_handler(const struct event_header *eh)
{
	if (is_fmna_config_event(eh)) {
		struct fmna_config_event *event = cast_fmna_config_event(eh);

		switch (event->id) {
		case FMNA_CONFIG_EVENT_START_SOUND:
			owner_start_sound_handle(event->conn);
			break;
		case FMNA_CONFIG_EVENT_STOP_SOUND:
			owner_stop_sound_handle(event->conn);
			break;
		default:
			break;
		}

		return false;
	}

	if (is_fmna_non_owner_event(eh)) {
		struct fmna_non_owner_event *event = cast_fmna_non_owner_event(eh);

		switch (event->id) {
		case FMNA_NON_OWNER_EVENT_START_SOUND:
			non_owner_start_sound_handle(event->conn);
			break;
		case FMNA_NON_OWNER_EVENT_STOP_SOUND:
			non_owner_stop_sound_handle(event->conn);
			break;
		default:
			break;
		}

		return false;
	}

	return false;
}

EVENT_LISTENER(fmna_sound, event_handler);
EVENT_SUBSCRIBE(fmna_sound, fmna_config_event);
EVENT_SUBSCRIBE(fmna_sound, fmna_non_owner_event);
