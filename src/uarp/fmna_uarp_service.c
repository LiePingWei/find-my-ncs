/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr.h>
#include <zephyr/types.h>
#include <init.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <net/buf.h>

#include <logging/log.h>
#define LOG_MODULE_NAME fmna_uarp

#include "events/fmna_event.h"

#include "CoreUARPProtocolDefines.h"
#include "fmna_gatt_pkt_manager.h"

#include "fmna_conn.h"
#include "fmna_uarp.h"

LOG_MODULE_DECLARE(LOG_MODULE_NAME, CONFIG_FMNA_UARP_LOG_LEVEL);

#define BT_UUID_FMN_UARP      BT_UUID_DECLARE_16(0xFD43)
#define BT_UUID_FMN_UARP_DCP  BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x94110001, 0x6D9B, 0x4225, 0xA4F1, 0x6A4A7F01B0DE))

#define UARP_SVC_DATA_CP_CHAR_INDEX 2
#define UARP_SVC_DATA_CP_MIN_WRITE_LENGTH 2
#define MAX_RX_MESSAGE_SIZE (sizeof(union UARPMessages) + CONFIG_FMNA_UARP_RX_MSG_PAYLOAD_SIZE)

enum rx_event_id
{
	RX_EVENT_DISCONNECT,
	RX_EVENT_INDICATION_ACK,
	RX_EVENT_WRITE,
};

struct rx_event {
	void *fifo_reserved;
	struct bt_conn *conn;
	enum rx_event_id id;
	union
	{
		struct
		{
			uint8_t err;
		} indication_ack_data;
		struct
		{
			uint16_t len;
			uint8_t buf[];
		} write_data;
	};
};

static struct bt_conn *active_conn = NULL;
static struct net_buf_simple *sending_buf = NULL;
static K_FIFO_DEFINE(rx_buf_fifo);

static bool submit_event_indication_ack(struct bt_conn *conn, uint8_t err);
static bool submit_event_write(struct bt_conn *conn, const uint8_t *buf, uint16_t len);

static ssize_t data_cp_write(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     const void *buf, uint16_t len,
			     uint16_t offset, uint8_t flags);

/* FMN Firmware Update Service Declaration */
BT_GATT_SERVICE_DEFINE(fmn_uarp_svc,
BT_GATT_PRIMARY_SERVICE(BT_UUID_FMN_UARP),
	BT_GATT_CHARACTERISTIC(BT_UUID_FMN_UARP_DCP,
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_WRITE_ENCRYPT,
			       NULL, data_cp_write, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
);

static ssize_t data_cp_write(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     const void *buf, uint16_t len,
			     uint16_t offset, uint8_t flags)
{
	LOG_INF("UARP data control point write, handle: %u, conn: %p, len: %d",
		attr->handle, (void *) conn, len);

	if (!IS_ENABLED(CONFIG_FMNA_UARP_TEST)) {
		if (!fmna_conn_multi_status_bit_check(conn,
						      FMNA_CONN_MULTI_STATUS_BIT_OWNER_CONNECTED)) {
			return BT_GATT_ERR(BT_ATT_ERR_WRITE_NOT_PERMITTED);
		}
	}

	if (len < UARP_SVC_DATA_CP_MIN_WRITE_LENGTH) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	if (submit_event_write(conn, buf, len)) {
		return len;
	} else {
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
	}
}

static void indication_ack_cb(struct bt_conn *conn,
				   struct bt_gatt_indicate_params *params,
				   uint8_t err)
{
	LOG_INF("Received UARP CP indication ACK with status: 0x%04X", err);
	submit_event_indication_ack(conn, err);
}

static uint32_t uarp_send_message(struct net_buf_simple *buf)
{
	uint8_t err = 0;

	if (sending_buf) {
		return kUARPStatusProcessingIncomplete;
	}

	sending_buf = buf;
	submit_event_indication_ack(active_conn, err);
	return kUARPStatusSuccess;
}

static bool uarp_init(void)
{
	static bool initialized = false;

	if (!initialized) {
		if (fmna_uarp_init(uarp_send_message)) {
			initialized = true;
		} else {
			LOG_ERR("fmna_uarp_init: Initialization failed");
		}
	}

	return initialized;
}

static void handle_disconnect(struct bt_conn *conn)
{
	if (conn != active_conn) {
		return;
	}

	sending_buf = NULL;
	fmna_uarp_controller_remove();
	active_conn = NULL;
}

static void handle_indication_ack(struct bt_conn *conn, uint8_t err)
{
	int result;
	static struct bt_gatt_indicate_params indicate_params;
	uint8_t *chunk;
	uint16_t chunk_len;

	if (conn != active_conn || !sending_buf) {
		return;
	}

	chunk = fmna_gatt_pkt_manager_chunk_prepare(conn, sending_buf, &chunk_len);

	if (!chunk || err) {
		sending_buf = NULL;
		fmna_uarp_send_message_complete();
		return;
	}

	memset(&indicate_params, 0, sizeof(indicate_params));
	indicate_params.attr = &fmn_uarp_svc.attrs[UARP_SVC_DATA_CP_CHAR_INDEX];
	indicate_params.func = indication_ack_cb;
	indicate_params.data = chunk;
	indicate_params.len = chunk_len;

	result = bt_gatt_indicate(active_conn, &indicate_params);
	if (result) {
		LOG_ERR("bt_gatt_indicate returned error: %d", err);
		sending_buf = NULL;
		fmna_uarp_send_message_complete();
	}
}

static void handle_write(struct bt_conn *conn, uint8_t *buf, uint16_t len)
{
	int err;
	bool pkt_complete;
	bool ok;

	NET_BUF_SIMPLE_DEFINE_STATIC(rx_buf, MAX_RX_MESSAGE_SIZE);

	if (conn != active_conn) {
		if (!active_conn) {
			ok = uarp_init();
			if (!ok) {
				return;
			}
		
			LOG_INF("Active UARP connection is 0x%08X", (int)conn);

			active_conn = conn;
			fmna_uarp_controller_add();
			net_buf_simple_reset(&rx_buf);
		} else {
			LOG_ERR("UARP is already active on connection 0x%08X", (int)conn);
			return;
		}
	}
	err = fmna_gatt_pkt_manager_chunk_collect(&rx_buf, buf, len, &pkt_complete);
	if (err) {
		LOG_ERR("fmna_gatt_pkt_manager_chunk_collect: returned error: %d", err);
		LOG_ERR("UARP incoming message invalid");

		net_buf_simple_reset(&rx_buf);

		return;
	}

	if (pkt_complete) {
		fmna_uarp_recv_message(&rx_buf);
		net_buf_simple_reset(&rx_buf);
	}
}

static void handle_rx_event(struct rx_event *event)
{
	if (event->id == RX_EVENT_DISCONNECT) {
		handle_disconnect(event->conn);
	} else if (event->id == RX_EVENT_INDICATION_ACK) {
		handle_indication_ack(event->conn, event->indication_ack_data.err);
	} else {
		handle_write(event->conn, event->write_data.buf, event->write_data.len);
	}
	k_free(event);
}

#ifdef CONFIG_FMNA_UARP_DEDICATED_THREAD
static void rx_thread_entry_point(void *arg0, void *arg1, void *arg2)
{
	struct rx_event *event;

	if (IS_ENABLED(CONFIG_FMNA_UARP_IMAGE_CONFIRMATION_ON_STARTUP)) {
		fmna_uarp_img_confirm();
	}

	while (true) {
		event = k_fifo_get(&rx_buf_fifo, K_FOREVER);
		handle_rx_event(event);
	}
}

K_THREAD_DEFINE(fmna_uarp_thread, CONFIG_FMNA_UARP_THREAD_STACK_SIZE,
		rx_thread_entry_point, NULL, NULL, NULL,
		CONFIG_FMNA_UARP_THREAD_PRIORITY < CONFIG_NUM_PREEMPT_PRIORITIES ?
			CONFIG_FMNA_UARP_THREAD_PRIORITY : CONFIG_NUM_PREEMPT_PRIORITIES - 1,
		0, 0);
#else
static void rx_handler(struct k_work *work)
{
	struct rx_event *event;

	while (true) {
		event = k_fifo_get(&rx_buf_fifo, K_NO_WAIT);
		if (event == NULL) {
			return;
		}
		handle_rx_event(event);
	}
}

static K_WORK_DEFINE(rx_work, rx_handler);

#ifdef CONFIG_FMNA_UARP_IMAGE_CONFIRMATION_ON_STARTUP
static int img_confirm_sys_init(const struct device *dev)
{
	fmna_uarp_img_confirm();
	return 0;
}

SYS_INIT(img_confirm_sys_init, APPLICATION, 99);
#endif /* CONFIG_FMNA_UARP_IMAGE_CONFIRMATION_ON_STARTUP */
#endif /* CONFIG_FMNA_UARP_DEDICATED_THREAD */


static bool submit_event_disconnect(struct bt_conn *conn)
{
	struct rx_event *event;

	event = k_malloc(sizeof(struct rx_event));
	if (event == NULL) {
		return false;
	}

	event->id = RX_EVENT_DISCONNECT;
	event->conn = conn;

	k_fifo_put(&rx_buf_fifo, event);

#ifndef CONFIG_FMNA_UARP_DEDICATED_THREAD
	k_work_submit(&rx_work);
#endif
	return true;
}

static bool submit_event_indication_ack(struct bt_conn *conn, uint8_t err)
{
	struct rx_event *event;

	event = k_malloc(sizeof(struct rx_event));
	if (event == NULL) {
		return false;
	}

	event->id = RX_EVENT_INDICATION_ACK;
	event->conn = conn;
	event->indication_ack_data.err = err;

	k_fifo_put(&rx_buf_fifo, event);

#ifndef CONFIG_FMNA_UARP_DEDICATED_THREAD
	k_work_submit(&rx_work);
#endif
	return true;
}

static bool submit_event_write(struct bt_conn *conn, const uint8_t *buf, uint16_t len)
{
	struct rx_event *event;

	event = k_malloc(offsetof(struct rx_event, write_data.buf) + len);
	if (!event) {
		return false;
	}

	event->id = RX_EVENT_WRITE;
	event->conn = conn;
	event->write_data.len = len;
	memcpy(event->write_data.buf, buf, len);

	k_fifo_put(&rx_buf_fifo, event);

#ifndef CONFIG_FMNA_UARP_DEDICATED_THREAD
	k_work_submit(&rx_work);
#endif
	return true;
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_fmna_event(aeh)) {
		struct fmna_event *event = cast_fmna_event(aeh);

		switch (event->id) {
		case FMNA_EVENT_PEER_DISCONNECTED:
			submit_event_disconnect(event->conn);
			break;
		default:
			break;
		}

		return false;
	}

	return false;
}

APP_EVENT_LISTENER(fmna_uarp_service, app_event_handler);
APP_EVENT_SUBSCRIBE(fmna_uarp_service, fmna_event);
