/** @file
 *  @brief Find My Network Service
 */

#include <zephyr/types.h>
#include <zephyr.h>

#include "fmna_gatt_fmns.h"
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

#define BT_ATT_HEADER_LEN 3

enum pairing_cp_opcode {
	PAIRING_CP_OPCODE_BASE                 = 0x0100,
	PAIRING_CP_OPCODE_INITIATE_PAIRING     = 0x0100,
	PAIRING_CP_OPCODE_SEND_PAIRING_DATA    = 0x0101,
	PAIRING_CP_OPCODE_FINALIZE_PAIRING     = 0x0102,
	PAIRING_CP_OPCODE_SEND_PAIRING_STATUS  = 0x0103,
	PAIRING_CP_OPCODE_PAIRING_COMPLETE     = 0x0104,
};

NET_BUF_SIMPLE_DEFINE_STATIC(pairing_cp_ind_buf, FMNA_GATT_PKT_MAX_LEN);
K_SEM_DEFINE(pairing_cp_tx_sem, 1, 1);

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

static uint16_t pairing_ind_len_get(struct bt_conn *conn)
{
	uint16_t ind_data_len;

	ind_data_len = bt_gatt_get_mtu(conn);
	if (ind_data_len <= BT_ATT_HEADER_LEN) {
		LOG_ERR("FMNS: MTU value too low: %d", ind_data_len);
		LOG_ERR("FMNS: 0 MTU might indicate that the link is "
			"disconnecting");

		return 0;
	}
	ind_data_len -= BT_ATT_HEADER_LEN;

	return ind_data_len;
}

static void pairing_cp_ind_cb(struct bt_conn *conn,
			      struct bt_gatt_indicate_params *params,
			      uint8_t err)
{
	uint8_t *ind_data;
	uint16_t ind_data_len;

	LOG_INF("Received FMN Pairing CP indication ACK with status: 0x%04X",
		err);

	ind_data_len = pairing_ind_len_get(conn);
	ind_data = fmna_gatt_pkt_manager_chunk_prepare(&pairing_cp_ind_buf,
						       &ind_data_len);
	if (!ind_data) {
		/* Release the semaphore when there is not more data
		 * to be sent for the whole packet transmission.
		 */
		k_sem_give(&pairing_cp_tx_sem);

		return;
	}

	params->data = ind_data;
	params->len = ind_data_len;

	err = bt_gatt_indicate(conn, params);
	if (err) {
		LOG_ERR("bt_gatt_indicate returned error: %d", err);

		k_sem_give(&pairing_cp_tx_sem);
	}
}

static int fmns_pairing_cp_indicate(struct bt_conn *conn,
				    enum pairing_cp_opcode opcode,
				    struct net_buf_simple *buf)
{
	int err;
	uint8_t *ind_data;
	uint16_t ind_data_len;

	static struct bt_gatt_indicate_params indicate_params;

	err = k_sem_take(&pairing_cp_tx_sem, K_MSEC(50));
	if (err) {
		LOG_ERR("FMN Pairing CP indication sending in progress");

		return err;
	}

	/* Initialize buffer for sending. */
	net_buf_simple_reset(&pairing_cp_ind_buf);
	net_buf_simple_reserve(&pairing_cp_ind_buf, FMNA_GATT_PKT_HEADER_LEN);
	net_buf_simple_add_le16(&pairing_cp_ind_buf, opcode);
	net_buf_simple_add_mem(&pairing_cp_ind_buf, buf->data, buf->len);

	ind_data_len = pairing_ind_len_get(conn);
	ind_data = fmna_gatt_pkt_manager_chunk_prepare(&pairing_cp_ind_buf,
						       &ind_data_len);
	if (!ind_data) {
		k_sem_give(&pairing_cp_tx_sem);

		return -EINVAL;
	}

	memset(&indicate_params, 0, sizeof(indicate_params));
	indicate_params.attr = &fmns_svc.attrs[2];
	indicate_params.func = pairing_cp_ind_cb;
	indicate_params.data = ind_data;
	indicate_params.len = ind_data_len;

	err = bt_gatt_indicate(conn, &indicate_params);
	if (err) {
		LOG_ERR("bt_gatt_indicate returned error: %d", err);

		k_sem_give(&pairing_cp_tx_sem);
	}

	return err;
}

int fmns_pairing_data_indicate(struct bt_conn *conn,
			       struct net_buf_simple *buf)
{
	return fmns_pairing_cp_indicate(conn,
					PAIRING_CP_OPCODE_SEND_PAIRING_DATA,
					buf);
}

int fmns_pairing_status_indicate(struct bt_conn *conn,
				 struct net_buf_simple *buf)
{
	return fmns_pairing_cp_indicate(conn,
					PAIRING_CP_OPCODE_SEND_PAIRING_STATUS,
					buf);
}
