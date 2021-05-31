/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "events/fmna_config_event.h"
#include "fmna_conn.h"
#include "fmna_gatt_fmns.h"

#include <sys/util.h>

#include <logging/log.h>
LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

struct conn_owner_finder {
	struct bt_conn ** const owners;
	uint8_t * const owner_cnt;
	const uint8_t owner_array_size;
};

struct conn_status_finder {
	const struct {
		enum fmna_conn_multi_status_bit status_bit;
	} in;
	struct {
		bool is_found;
	} out;
};

struct fmna_conn {
	uint32_t multi_status;
	bool is_valid;
};

static struct fmna_conn conns[CONFIG_BT_MAX_CONN];
static uint8_t max_connections = 1;

static void connected(struct bt_conn *conn, uint8_t err)
{
	struct fmna_conn *fmna_conn = &conns[bt_conn_index(conn)];

	fmna_conn->is_valid = true;
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	struct fmna_conn *fmna_conn = &conns[bt_conn_index(conn)];

	memset(fmna_conn, 0, sizeof(*fmna_conn));
}

static void conn_owner_iterator(struct bt_conn *conn, void *user_data)
{
	struct conn_owner_finder *conn_owner_finder = user_data;

	if (fmna_conn_multi_status_bit_check(
		conn, FMNA_CONN_MULTI_STATUS_BIT_OWNER_CONNECTED)) {
		if (*conn_owner_finder->owner_cnt < conn_owner_finder->owner_array_size) {
			conn_owner_finder->owners[*conn_owner_finder->owner_cnt] = conn;
		}

		*conn_owner_finder->owner_cnt += 1;
	}
}

static void conn_status_iterator(struct bt_conn *conn, void *user_data)
{
	struct conn_status_finder *conn_status_finder =
		(struct conn_status_finder *) user_data;

	if (fmna_conn_multi_status_bit_check(conn, conn_status_finder->in.status_bit)) {
		conn_status_finder->out.is_found = true;
	}
}

uint8_t fmna_conn_connection_num_get(void)
{
	uint8_t cnt = 0;

	for (size_t i = 0; i < ARRAY_SIZE(conns); i++) {
		if (conns[i].is_valid) {
			cnt++;
		}
	}

	return cnt;
}

bool fmna_conn_limit_check(void)
{
	uint8_t cnt = fmna_conn_connection_num_get();

	return (cnt < max_connections);
}

int fmna_conn_owner_find(struct bt_conn *owner_conns[], uint8_t *owner_conn_cnt)
{
	struct conn_owner_finder conn_owner_finder = {
		.owners = owner_conns,
		.owner_cnt = owner_conn_cnt,
		.owner_array_size = *owner_conn_cnt,
	};

	/* Reset the owner connection counter. */
	*conn_owner_finder.owner_cnt = 0;

	bt_conn_foreach(BT_CONN_TYPE_ALL, conn_owner_iterator, &conn_owner_finder);

	if (conn_owner_finder.owner_array_size < *conn_owner_finder.owner_cnt) {
		return -ENOMEM;
	}

	return 0;
}

bool fmna_conn_multi_status_bit_check(struct bt_conn *conn,
				      enum fmna_conn_multi_status_bit status_bit)
{
	struct fmna_conn *fmna_conn = &conns[bt_conn_index(conn)];

	__ASSERT(status_bit < (sizeof(fmna_conn->multi_status) * __CHAR_BIT__),
		 "FMNA Status bit is invalid: %d", status_bit);

	if (!fmna_conn->is_valid) {
		return false;
	}

	return (fmna_conn->multi_status & BIT(status_bit));
}

void fmna_conn_multi_status_bit_set(struct bt_conn *conn,
				    enum fmna_conn_multi_status_bit status_bit)
{
	struct fmna_conn *fmna_conn = &conns[bt_conn_index(conn)];

	if (!fmna_conn->is_valid) {
		return;
	}

	__ASSERT(status_bit < (sizeof(fmna_conn->multi_status) * __CHAR_BIT__),
		 "FMNA Status bit is invalid: %d", status_bit);

	WRITE_BIT(fmna_conn->multi_status, status_bit, 1);
}

void fmna_conn_multi_status_bit_clear(struct bt_conn *conn,
				      enum fmna_conn_multi_status_bit status_bit)
{
	struct fmna_conn *fmna_conn = &conns[bt_conn_index(conn)];

	if (!fmna_conn->is_valid) {
		return;
	}

	__ASSERT(status_bit < (sizeof(fmna_conn->multi_status) * __CHAR_BIT__),
		 "FMNA Status bit is invalid: %d", status_bit);

	WRITE_BIT(fmna_conn->multi_status, status_bit, 0);
}

int fmna_conn_init(void)
{
	static struct bt_conn_cb conn_callbacks = {
		.connected = connected,
		.disconnected = disconnected,
	};

	bt_conn_cb_register(&conn_callbacks);

	memset(conns, 0, sizeof(conns));

	return 0;
}

static void persistent_conn_request_handle(struct bt_conn *conn, uint8_t persistent_conn_status)
{
	int err;
	uint16_t resp_opcode;
	struct conn_status_finder conn_status_finder = {
		.in.status_bit = FMNA_CONN_MULTI_STATUS_BIT_PERSISTENT_CONNECTION,
		.out.is_found = false,
	};

	LOG_INF("FMN Config CP: responding to pesistant connection request: %d",
		persistent_conn_status);

	bt_conn_foreach(BT_CONN_TYPE_ALL, conn_status_iterator, &conn_status_finder);
	if ((persistent_conn_status & BIT(0)) && !conn_status_finder.out.is_found) {
		fmna_conn_multi_status_bit_set(
			conn, FMNA_CONN_MULTI_STATUS_BIT_PERSISTENT_CONNECTION);
	} else {
		fmna_conn_multi_status_bit_clear(
			conn, FMNA_CONN_MULTI_STATUS_BIT_PERSISTENT_CONNECTION);
	}

	resp_opcode = fmna_config_event_to_gatt_cmd_opcode(
		FMNA_CONFIG_EVENT_SET_PERSISTENT_CONN_STATUS);
	FMNA_GATT_COMMAND_RESPONSE_BUILD(resp_buf, resp_opcode, FMNA_GATT_RESPONSE_STATUS_SUCCESS);
	err = fmna_gatt_config_cp_indicate(conn, FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND, &resp_buf);
	if (err) {
		LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
	}
}

static void max_connections_request_handle(struct bt_conn *conn, uint8_t max_conns)
{
	int err;
	uint16_t opcode;

	LOG_INF("FMN Config CP: responding to max connections settings request: %d",
		max_conns);

	if (max_conns == 0) {
		LOG_INF("Cannot set max connections to 0");
		return;
	}

	if (max_conns > CONFIG_BT_MAX_CONN) {
		LOG_WRN("Cannot support max connections value due to the limit: %d",
			CONFIG_BT_MAX_CONN);
		max_conns = CONFIG_BT_MAX_CONN;
	}

	max_connections = max_conns;

	opcode = fmna_config_event_to_gatt_cmd_opcode(
		FMNA_CONFIG_EVENT_SET_MAX_CONNECTIONS);
	FMNA_GATT_COMMAND_RESPONSE_BUILD(cmd_buf, opcode, FMNA_GATT_RESPONSE_STATUS_SUCCESS);
	err = fmna_gatt_config_cp_indicate(conn, FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND, &cmd_buf);
	if (err) {
		LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
	}
}

static void multi_status_request_handle(struct bt_conn *conn)
{
	int err;
	struct fmna_conn *fmna_conn = &conns[bt_conn_index(conn)];
	NET_BUF_SIMPLE_DEFINE(status_buf, sizeof(fmna_conn->multi_status));

	LOG_INF("FMN Config CP: responding to connection multi status: 0x%02X",
		fmna_conn->multi_status);

	net_buf_simple_add_le32(&status_buf, fmna_conn->multi_status);

	err = fmna_gatt_config_cp_indicate(conn, FMNA_GATT_CONFIG_MULTI_STATUS_IND, &status_buf);
	if (err) {
		LOG_ERR("fmna_gatt_config_cp_indicate returned error: %d", err);
	}
}

static bool event_handler(const struct event_header *eh)
{
	if (is_fmna_config_event(eh)) {
		struct fmna_config_event *event = cast_fmna_config_event(eh);

		switch (event->id) {
		case FMNA_CONFIG_EVENT_SET_PERSISTENT_CONN_STATUS:
			persistent_conn_request_handle(event->conn,
						       event->persistent_conn_status);
			break;
		case FMNA_CONFIG_EVENT_SET_MAX_CONNECTIONS:
			max_connections_request_handle(event->conn,
						       event->max_connections);
			break;
		case FMNA_CONFIG_EVENT_GET_MULTI_STATUS:
			multi_status_request_handle(event->conn);
			break;
		default:
			break;
		}

		return false;
	}

	return false;
}

EVENT_LISTENER(fmna_conn, event_handler);
EVENT_SUBSCRIBE(fmna_conn, fmna_config_event);
