/** @file
 *  @brief Find My Network Service
 */

#include <zephyr/types.h>
#include <zephyr.h>

#include "fmna_gatt_pkt_manager.h"

#include "events/fmna_pair_event.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <net/buf.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(fmna_gatt_fmns, 3);

#define BT_UUID_FMNS_VAL 0xFD44
#define BT_UUID_FMNS     BT_UUID_DECLARE_16(BT_UUID_FMNS_VAL)

#define BT_UUID_FMNS_CHRC_BASE(_chrc_id) \
	BT_UUID_128_ENCODE((0x4F860000 + _chrc_id), 0x943B, 0x49EF, 0xBED4, 0x2F730304427A)

#define BT_UUID_FMNS_PAIRING  BT_UUID_DECLARE_128(BT_UUID_FMNS_CHRC_BASE(0x0001))
#define BT_UUID_FMNS_CONFIG   BT_UUID_DECLARE_128(BT_UUID_FMNS_CHRC_BASE(0x0002))
#define BT_UUID_FMNS_NONOWNER BT_UUID_DECLARE_128(BT_UUID_FMNS_CHRC_BASE(0x0003))
#define BT_UUID_FMNS_OWNER    BT_UUID_DECLARE_128(BT_UUID_FMNS_CHRC_BASE(0x0004))
#define BT_UUID_FMNS_DEBUG_CP BT_UUID_DECLARE_128(BT_UUID_FMNS_CHRC_BASE(0x0005))

enum pairing_cp_opcode {
	PAIRING_CP_OPCODE_BASE                 = 0x0100,
	PAIRING_CP_OPCODE_INITIATE_PAIRING     = 0x0100,
	PAIRING_CP_OPCODE_SEND_PAIRING_DATA    = 0x0101,
	PAIRING_CP_OPCODE_FINALIZE_PAIRING     = 0x0102,
	PAIRING_CP_OPCODE_SEND_PAIRING_STATUS  = 0x0103,
	PAIRING_CP_OPCODE_PAIRING_COMPLETE     = 0x0104,
};

static void pairing_cp_ccc_cfg_changed(const struct bt_gatt_attr *attr,
				       uint16_t value)
{
	LOG_INF("FMN Pairing CP CCCD write, handle: %u, value: 0x%04X",
		attr->handle, value);
}

static void config_cp_ccc_cfg_changed(const struct bt_gatt_attr *attr,
				      uint16_t value)
{
	LOG_INF("FMN Configuration CP CCCD write, handle: %u, value: 0x%04X",
		attr->handle, value);
}

static void nonowner_cp_ccc_cfg_changed(const struct bt_gatt_attr *attr,
					uint16_t value)
{
	LOG_INF("FMN Non Owner CP CCCD write, handle: %u, value: 0x%04X",
		attr->handle, value);
}

static void owner_cp_ccc_cfg_changed(const struct bt_gatt_attr *attr,
				     uint16_t value)
{
	LOG_INF("FMN Owner CP CCCD write, handle: %u, value: 0x%04X",
		attr->handle, value);
}

static void debug_cp_ccc_cfg_changed(const struct bt_gatt_attr *attr,
				     uint16_t value)
{
	LOG_INF("FMN Debug CP CCCD write, handle: %u, value: 0x%04X",
		attr->handle, value);
}

static ssize_t pairing_cp_write(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len,
				uint16_t offset, uint8_t flags)
{
	bool pkt_complete;

	NET_BUF_SIMPLE_DEFINE_STATIC(pairing_buf, FMNA_GATT_PKT_MAX_LEN);

	LOG_INF("FMN Pairing CP write, handle: %u, conn: %p, len: %d",
		attr->handle, conn, len);

	pkt_complete = fmna_gatt_pkt_manager_chunk_collect(&pairing_buf, buf, len);
	if (pkt_complete) {
		uint16_t opcode;
		enum fmna_pair_operation op;

		LOG_HEXDUMP_INF(pairing_buf.data, pairing_buf.len, "Pairing packet:");
		LOG_INF("Total packet length: %d", pairing_buf.len);

		opcode = net_buf_simple_pull_le16(&pairing_buf);
		switch (opcode) {
		case PAIRING_CP_OPCODE_INITIATE_PAIRING:
			op = FMNA_INITIATE_PAIRING;
			break;
		case PAIRING_CP_OPCODE_FINALIZE_PAIRING:
			op = FMNA_FINALIZE_PAIRING;
			break;
		case PAIRING_CP_OPCODE_PAIRING_COMPLETE:
			op = FMNA_PAIRING_COMPLETE;
			break;
		default:
			LOG_ERR("FMN Pairing CP, unexpected opcode: 0x%02X",
				opcode);
			net_buf_simple_reset(&pairing_buf);
			return len;
		}

		struct fmna_pair_event *event = new_fmna_pair_event();

		event->op = op;
		event->conn = conn;
		event->buf.len = pairing_buf.len;
		memcpy(event->buf.data, pairing_buf.data, pairing_buf.len);

		EVENT_SUBMIT(event);

		net_buf_simple_reset(&pairing_buf);
	}

	return len;
}

static ssize_t config_cp_write(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr,
			       const void *buf, uint16_t len,
			       uint16_t offset, uint8_t flags)
{
	LOG_INF("FMN Configuration CP write, handle: %u, conn: %p",
		attr->handle, conn);

	return len;
}

static ssize_t nonowner_cp_write(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr,
				 const void *buf, uint16_t len,
				 uint16_t offset, uint8_t flags)
{
	LOG_INF("FMN Non Owner CP write, handle: %u, conn: %p", attr->handle, conn);

	return len;
}

static ssize_t owner_cp_write(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr,
			      const void *buf, uint16_t len,
			      uint16_t offset, uint8_t flags)
{
	LOG_INF("FMN Owner CP write, handle: %u, conn: %p", attr->handle, conn);

	return len;
}

static ssize_t debug_cp_write(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr,
			      const void *buf, uint16_t len,
			      uint16_t offset, uint8_t flags)
{
	LOG_INF("FMN Debug CP write, handle: %u, conn: %p", attr->handle, conn);

	return len;
}

/* Find My Network Service Declaration */
BT_GATT_SERVICE_DEFINE(fmns_svc,
BT_GATT_PRIMARY_SERVICE(BT_UUID_FMNS),
	BT_GATT_CHARACTERISTIC(BT_UUID_FMNS_PAIRING,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       NULL, pairing_cp_write, NULL),
	BT_GATT_CCC(pairing_cp_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_FMNS_CONFIG,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       NULL, config_cp_write, NULL),
	BT_GATT_CCC(config_cp_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_FMNS_NONOWNER,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       NULL, nonowner_cp_write, NULL),
	BT_GATT_CCC(nonowner_cp_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_FMNS_OWNER,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       NULL, owner_cp_write, NULL),
	BT_GATT_CCC(owner_cp_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_FMNS_DEBUG_CP,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       NULL, debug_cp_write, NULL),
	BT_GATT_CCC(debug_cp_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);