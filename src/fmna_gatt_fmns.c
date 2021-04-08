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

LOG_MODULE_DECLARE(fmna, CONFIG_FMN_ADK_LOG_LEVEL);

#define BT_UUID_FMNS_VAL 0xFD44
#define BT_UUID_FMNS     BT_UUID_DECLARE_16(BT_UUID_FMNS_VAL)

#define BT_UUID_FMNS_CHRC_BASE(_chrc_id) \
	BT_UUID_128_ENCODE((0x4F860000 + _chrc_id), 0x943B, 0x49EF, 0xBED4, 0x2F730304427A)

#define BT_UUID_FMNS_PAIRING   BT_UUID_DECLARE_128(BT_UUID_FMNS_CHRC_BASE(0x0001))
#define BT_UUID_FMNS_CONFIG    BT_UUID_DECLARE_128(BT_UUID_FMNS_CHRC_BASE(0x0002))
#define BT_UUID_FMNS_NON_OWNER BT_UUID_DECLARE_128(BT_UUID_FMNS_CHRC_BASE(0x0003))
#define BT_UUID_FMNS_OWNER     BT_UUID_DECLARE_128(BT_UUID_FMNS_CHRC_BASE(0x0004))
#define BT_UUID_FMNS_DEBUG_CP  BT_UUID_DECLARE_128(BT_UUID_FMNS_CHRC_BASE(0x0005))

#define BT_ATT_HEADER_LEN 3

#define FMNS_CONFIG_MAX_RX_LEN 10
#define FMNS_NON_OWNER_MAX_RX_LEN 2
#define FMNS_OWNER_MAX_RX_LEN 2

#define FMNS_PAIRING_CHAR_INDEX   2
#define FMNS_CONFIG_CHAR_INDEX    5
#define FMNS_NON_OWNER_CHAR_INDEX 8
#define FMNS_OWNER_CHAR_INDEX     11
#define FMNS_DEBUG_CHAR_INDEX     14

enum pairing_cp_opcode {
	PAIRING_CP_OPCODE_BASE                 = 0x0100,
	PAIRING_CP_OPCODE_INITIATE_PAIRING     = 0x0100,
	PAIRING_CP_OPCODE_SEND_PAIRING_DATA    = 0x0101,
	PAIRING_CP_OPCODE_FINALIZE_PAIRING     = 0x0102,
	PAIRING_CP_OPCODE_SEND_PAIRING_STATUS  = 0x0103,
	PAIRING_CP_OPCODE_PAIRING_COMPLETE     = 0x0104,
};

enum config_cp_opcode {
	CONFIG_CP_OPCODE_START_SOUND                  = 0x0200,
	CONFIG_CP_OPCODE_STOP_SOUND                   = 0x0201,
	CONFIG_CP_OPCODE_PERSISTANT_CONNECTION_STATUS = 0x0202,
	CONFIG_CP_OPCODE_SET_NEARBY_TIMEOUT           = 0x0203,
	CONFIG_CP_OPCODE_UNPAIR                       = 0x0204,
	CONFIG_CP_OPCODE_CONFIGURE_SEPARATED_STATE    = 0x0205,
	CONFIG_CP_OPCODE_LATCH_SEPARATED_KEY          = 0x0206,
	CONFIG_CP_OPCODE_SET_MAX_CONNECTIONS          = 0x0207,
	CONFIG_CP_OPCODE_SET_UTC                      = 0x0208,
	CONFIG_CP_OPCODE_GET_MULTI_STATUS             = 0x0209,
	CONFIG_CP_OPCODE_KEYROLL_INDICATION           = 0x020A,
	CONFIG_CP_OPCODE_COMMAND_RESPONSE             = 0x020B,
	CONFIG_CP_OPCODE_GET_MULTI_STATUS_RESPONSE    = 0x020C,
	CONFIG_CP_OPCODE_SOUND_COMPLETED              = 0x020D,
	CONFIG_CP_OPCODE_LATCH_SEPARATED_KEY_RESPONSE = 0x020E,
};

enum non_owner_cp_opcode {
	NON_OWNER_CP_OPCODE_START_SOUND      = 0x0300,
	NON_OWNER_CP_OPCODE_STOP_SOUND       = 0x0301,
	NON_OWNER_CP_OPCODE_COMMAND_RESPONSE = 0x0302,
	NON_OWNER_CP_OPCODE_SOUND_COMPLETED  = 0x0303,
};

enum owner_cp_opcode {
	OWNER_CP_OPCODE_GET_CURRENT_PRIMARY_KEY          = 0x0400,
	OWNER_CP_OPCODE_GET_ICLOUD_IDENTIFIER            = 0x0401,
	OWNER_CP_OPCODE_GET_CURRENT_PRIMARY_KEY_RESPONSE = 0x0402,
	OWNER_CP_OPCODE_GET_ICLOUD_IDENTIFIER_RESPONSE   = 0x0403,
	OWNER_CP_OPCODE_GET_SERIAL_NUMBER                = 0x0404,
	OWNER_CP_OPCODE_GET_SERIAL_NUMBER_RESPONSE       = 0x0405,
	OWNER_CP_OPCODE_COMMAND_RESPONSE                 = 0x0406,
};

NET_BUF_SIMPLE_DEFINE_STATIC(cp_ind_buf, FMNA_GATT_PKT_MAX_LEN);
K_SEM_DEFINE(cp_tx_sem, 1, 1);

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

static void non_owner_cp_ccc_cfg_changed(const struct bt_gatt_attr *attr,
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

#if CONFIG_FMN_DEBUG
static void debug_cp_ccc_cfg_changed(const struct bt_gatt_attr *attr,
				     uint16_t value)
{
	LOG_INF("FMN Debug CP CCCD write, handle: %u, value: 0x%04X",
		attr->handle, value);
}
#endif

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
		enum fmna_pair_event_id id;

		LOG_HEXDUMP_INF(pairing_buf.data, pairing_buf.len, "Pairing packet:");
		LOG_INF("Total packet length: %d", pairing_buf.len);

		opcode = net_buf_simple_pull_le16(&pairing_buf);
		switch (opcode) {
		case PAIRING_CP_OPCODE_INITIATE_PAIRING:
			id = FMNA_PAIR_EVENT_INITIATE_PAIRING;
			break;
		case PAIRING_CP_OPCODE_FINALIZE_PAIRING:
			id = FMNA_PAIR_EVENT_FINALIZE_PAIRING;
			break;
		case PAIRING_CP_OPCODE_PAIRING_COMPLETE:
			id = FMNA_PAIR_EVENT_PAIRING_COMPLETE;
			break;
		default:
			LOG_ERR("FMN Pairing CP, unexpected opcode: 0x%02X",
				opcode);
			net_buf_simple_reset(&pairing_buf);
			return len;
		}

		struct fmna_pair_event *event = new_fmna_pair_event();

		event->id = id;
		event->conn = conn;
		event->buf.len = pairing_buf.len;
		memcpy(event->buf.data, pairing_buf.data, pairing_buf.len);

		EVENT_SUBMIT(event);

		net_buf_simple_reset(&pairing_buf);
	}

	return len;
}

static bool config_cp_length_verify(uint16_t opcode, uint32_t len)
{
	uint16_t expected_pkt_len = 0;
	const struct fmna_config_event * const event = NULL;

	switch (opcode) {
	case CONFIG_CP_OPCODE_START_SOUND:
	case CONFIG_CP_OPCODE_STOP_SOUND:
	case CONFIG_CP_OPCODE_UNPAIR:
	case CONFIG_CP_OPCODE_LATCH_SEPARATED_KEY:
	case CONFIG_CP_OPCODE_GET_MULTI_STATUS:
		break;
	case CONFIG_CP_OPCODE_PERSISTANT_CONNECTION_STATUS:
		expected_pkt_len += sizeof(event->persistant_conn_status);
		break;
	case CONFIG_CP_OPCODE_SET_NEARBY_TIMEOUT:
		expected_pkt_len += sizeof(event->nearby_timeout);
		break;
	case CONFIG_CP_OPCODE_CONFIGURE_SEPARATED_STATE:
		expected_pkt_len += sizeof(event->separated_state);
		break;
	case CONFIG_CP_OPCODE_SET_MAX_CONNECTIONS:
		expected_pkt_len += sizeof(event->max_connections);
		break;
	case CONFIG_CP_OPCODE_SET_UTC:
		expected_pkt_len += sizeof(event->utc);
		break;
	default:
		return true;
	}

	if (len != expected_pkt_len) {
		LOG_ERR("FMN Configuration CP: wrong packet length: %d != %d for "
			"0x%04X opcode", len, expected_pkt_len, opcode);
		return false;
	}

	return true;
}

static ssize_t config_cp_write(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr,
			       const void *buf, uint16_t len,
			       uint16_t offset, uint8_t flags)
{
	bool pkt_complete;

	LOG_INF("FMN Configuration CP write, handle: %u, conn: %p",
		attr->handle, conn);

	NET_BUF_SIMPLE_DEFINE(config_buf, FMNS_CONFIG_MAX_RX_LEN);

	pkt_complete = fmna_gatt_pkt_manager_chunk_collect(&config_buf, buf, len);
	if (pkt_complete) {
		struct fmna_config_event event;
		uint16_t opcode;

		LOG_HEXDUMP_INF(config_buf.data, config_buf.len, "Config packet:");
		LOG_INF("Total packet length: %d", config_buf.len);

		if ((config_buf.len < sizeof(opcode)) ||
		    (config_buf.len > FMNS_CONFIG_MAX_RX_LEN)) {
			LOG_ERR("FMN Configuration CP: packet length too large");
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}

		opcode = net_buf_simple_pull_le16(&config_buf);
		if (!config_cp_length_verify(opcode, config_buf.len)) {
			LOG_ERR("FMN Configuration CP: returning GATT error");
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}

		switch (opcode) {
		case CONFIG_CP_OPCODE_START_SOUND:
			event.id = FMNA_CONFIG_EVENT_START_SOUND;
			break;
		case CONFIG_CP_OPCODE_STOP_SOUND:
			event.id = FMNA_CONFIG_EVENT_STOP_SOUND;
			break;
		case CONFIG_CP_OPCODE_PERSISTANT_CONNECTION_STATUS:
			event.id = FMNA_CONFIG_EVENT_SET_PERSISTANT_CONN_STATUS;
			event.persistant_conn_status = net_buf_simple_pull_u8(&config_buf);
			break;
		case CONFIG_CP_OPCODE_SET_NEARBY_TIMEOUT:
			event.id = FMNA_CONFIG_EVENT_SET_NEARBY_TIMEOUT;
			event.nearby_timeout = net_buf_simple_pull_le16(&config_buf);
			break;
		case CONFIG_CP_OPCODE_UNPAIR:
			event.id = FMNA_CONFIG_EVENT_UNPAIR;
			break;
		case CONFIG_CP_OPCODE_CONFIGURE_SEPARATED_STATE:
			event.id = FMNA_CONFIG_EVENT_CONFIGURE_SEPARATED_STATE;
			event.separated_state.next_primary_key_roll =
				net_buf_simple_pull_le32(&config_buf);
			event.separated_state.seconday_key_evaluation_index =
				net_buf_simple_pull_le32(&config_buf);
			break;
		case CONFIG_CP_OPCODE_LATCH_SEPARATED_KEY:
			event.id = FMNA_CONFIG_EVENT_LATCH_SEPARATED_KEY;
			break;
		case CONFIG_CP_OPCODE_SET_MAX_CONNECTIONS:
			event.id = FMNA_CONFIG_EVENT_SET_MAX_CONNECTIONS;
			event.max_connections = net_buf_simple_pull_u8(&config_buf);
			break;
		case CONFIG_CP_OPCODE_SET_UTC:
			event.id = FMNA_CONFIG_EVENT_SET_UTC;
			event.utc.current_time = net_buf_simple_pull_le64(&config_buf);
			break;
		case CONFIG_CP_OPCODE_GET_MULTI_STATUS:
			event.id = FMNA_CONFIG_EVENT_GET_MULTI_STATUS;
			break;
		default:
			LOG_ERR("FMN Configuration CP, unexpected opcode: 0x%02X", opcode);
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
		event.conn = conn;

		struct fmna_config_event *event_heap = new_fmna_config_event();

		memcpy(&event.header, &event_heap->header, sizeof(event.header));
		memcpy(event_heap, &event, sizeof(*event_heap));

		EVENT_SUBMIT(event_heap);

		return len;
	} else {
		LOG_ERR("FMN Configuration CP: no support for chunked packets");
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
	}
}

static ssize_t non_owner_cp_write(struct bt_conn *conn,
				  const struct bt_gatt_attr *attr,
				  const void *buf, uint16_t len,
				  uint16_t offset, uint8_t flags)
{
	bool pkt_complete;

	NET_BUF_SIMPLE_DEFINE(non_owner_buf, FMNS_NON_OWNER_MAX_RX_LEN);

	LOG_INF("FMN Non-owner CP write, handle: %u, conn: %p", attr->handle, conn);

	pkt_complete = fmna_gatt_pkt_manager_chunk_collect(&non_owner_buf, buf, len);
	if (pkt_complete) {
		uint16_t opcode;
		enum fmna_non_owner_event_id id;

		LOG_HEXDUMP_INF(non_owner_buf.data, non_owner_buf.len,
				"Non-owner packet:");
		LOG_INF("Total packet length: %d", non_owner_buf.len);

		opcode = net_buf_simple_pull_le16(&non_owner_buf);
		switch (opcode) {
		case NON_OWNER_CP_OPCODE_START_SOUND:
			id = FMNA_NON_OWNER_EVENT_START_SOUND;
			break;
		case NON_OWNER_CP_OPCODE_STOP_SOUND:
			id = FMNA_NON_OWNER_EVENT_STOP_SOUND;
			break;
		default:
			LOG_ERR("FMN Non-owner CP, unexpected opcode: 0x%02X", opcode);
			return len;
		}

		struct fmna_non_owner_event *event = new_fmna_non_owner_event();

		event->id = id;
		event->conn = conn;

		EVENT_SUBMIT(event);
	}

	return len;
}

static ssize_t owner_cp_write(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr,
			      const void *buf, uint16_t len,
			      uint16_t offset, uint8_t flags)
{
	bool pkt_complete;

	NET_BUF_SIMPLE_DEFINE(owner_buf, FMNS_OWNER_MAX_RX_LEN);

	LOG_INF("FMN Owner CP write, handle: %u, conn: %p", attr->handle, conn);

	pkt_complete = fmna_gatt_pkt_manager_chunk_collect(&owner_buf, buf, len);
	if (pkt_complete) {
		uint16_t opcode;
		enum fmna_owner_event_id id;

		LOG_HEXDUMP_INF(owner_buf.data, owner_buf.len, "Owner packet:");
		LOG_INF("Total packet length: %d", owner_buf.len);

		opcode = net_buf_simple_pull_le16(&owner_buf);
		switch (opcode) {
		case OWNER_CP_OPCODE_GET_CURRENT_PRIMARY_KEY:
			id = FMNA_OWNER_EVENT_GET_CURRENT_PRIMARY_KEY;
			break;
		case OWNER_CP_OPCODE_GET_ICLOUD_IDENTIFIER:
			id = FMNA_OWNER_EVENT_GET_ICLOUD_IDENTIFIER;
			break;
		case OWNER_CP_OPCODE_GET_SERIAL_NUMBER:
			id = FMNA_OWNER_EVENT_GET_SERIAL_NUMBER;
			break;
		default:
			LOG_ERR("FMN Owner CP, unexpected opcode: 0x%02X", opcode);
			return len;
		}

		struct fmna_owner_event *event = new_fmna_owner_event();

		event->id = id;
		event->conn = conn;

		EVENT_SUBMIT(event);
	}

	return len;
}

#if CONFIG_FMN_DEBUG
static ssize_t debug_cp_write(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr,
			      const void *buf, uint16_t len,
			      uint16_t offset, uint8_t flags)
{
	LOG_INF("FMN Debug CP write, handle: %u, conn: %p", attr->handle, conn);

	return len;
}
#endif

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
	BT_GATT_CHARACTERISTIC(BT_UUID_FMNS_NON_OWNER,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       NULL, non_owner_cp_write, NULL),
	BT_GATT_CCC(non_owner_cp_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_FMNS_OWNER,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       NULL, owner_cp_write, NULL),
	BT_GATT_CCC(owner_cp_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
#if CONFIG_FMN_DEBUG
	BT_GATT_CHARACTERISTIC(BT_UUID_FMNS_DEBUG_CP,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       NULL, debug_cp_write, NULL),
	BT_GATT_CCC(debug_cp_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
#endif
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

static void cp_ind_cb(struct bt_conn *conn, struct bt_gatt_indicate_params *params, uint8_t err)
{
	uint8_t *ind_data;
	uint16_t ind_data_len;

	LOG_INF("Received FMN CP indication ACK with status: 0x%04X", err);

	ind_data_len = pairing_ind_len_get(conn);
	ind_data = fmna_gatt_pkt_manager_chunk_prepare(&cp_ind_buf, &ind_data_len);
	if (!ind_data) {
		/* Release the semaphore when there is not more data
		 * to be sent for the whole packet transmission.
		 */
		k_sem_give(&cp_tx_sem);

		return;
	}

	params->data = ind_data;
	params->len = ind_data_len;

	err = bt_gatt_indicate(conn, params);
	if (err) {
		LOG_ERR("bt_gatt_indicate returned error: %d", err);

		k_sem_give(&cp_tx_sem);
	}
}

static int cp_indicate(struct bt_conn *conn,
		       const struct bt_gatt_attr *attr,
		       uint16_t opcode,
		       struct net_buf_simple *buf)
{
	int err;
	uint8_t *ind_data;
	uint16_t ind_data_len;

	static struct bt_gatt_indicate_params indicate_params;

	err = k_sem_take(&cp_tx_sem, K_MSEC(50));
	if (err) {
		LOG_ERR("FMN CP indication sending in progress");

		return err;
	}

	/* Initialize buffer for sending. */
	net_buf_simple_reset(&cp_ind_buf);
	net_buf_simple_reserve(&cp_ind_buf, FMNA_GATT_PKT_HEADER_LEN);
	net_buf_simple_add_le16(&cp_ind_buf, opcode);
	net_buf_simple_add_mem(&cp_ind_buf, buf->data, buf->len);

	ind_data_len = pairing_ind_len_get(conn);
	ind_data = fmna_gatt_pkt_manager_chunk_prepare(&cp_ind_buf,
						       &ind_data_len);
	if (!ind_data) {
		k_sem_give(&cp_tx_sem);

		return -EINVAL;
	}

	memset(&indicate_params, 0, sizeof(indicate_params));
	indicate_params.attr = attr;
	indicate_params.func = cp_ind_cb;
	indicate_params.data = ind_data;
	indicate_params.len = ind_data_len;

	err = bt_gatt_indicate(conn, &indicate_params);
	if (err) {
		LOG_ERR("bt_gatt_indicate returned error: %d", err);

		k_sem_give(&cp_tx_sem);
	}

	return err;
}

int fmna_gatt_pairing_cp_indicate(struct bt_conn *conn,
				  enum fmna_gatt_pairing_ind ind_type,
				  struct net_buf_simple *buf)
{
	uint16_t pairing_opcode;

	switch (ind_type) {
	case FMNA_GATT_PAIRING_DATA_IND:
		pairing_opcode = PAIRING_CP_OPCODE_SEND_PAIRING_DATA;
		break;
	case FMNA_GATT_PAIRING_STATUS_IND:
		pairing_opcode = PAIRING_CP_OPCODE_SEND_PAIRING_STATUS;
		break;
	default:
		LOG_ERR("Pairing CP: invalid indication type: %d", ind_type);
		return -EINVAL;
	}

	return cp_indicate(conn, &fmns_svc.attrs[FMNS_PAIRING_CHAR_INDEX], pairing_opcode, buf);
}

int fmna_gatt_config_cp_indicate(struct bt_conn *conn,
				 enum fmna_gatt_config_ind ind_type,
				 struct net_buf_simple *buf)
{
	uint16_t config_opcode;

	switch (ind_type) {
	case FMNA_GATT_CONFIG_KEYROLL_IND:
		config_opcode = CONFIG_CP_OPCODE_KEYROLL_INDICATION;
		break;
	case FMNA_GATT_CONFIG_MULTI_STATUS_IND:
		config_opcode = CONFIG_CP_OPCODE_GET_MULTI_STATUS_RESPONSE;
		break;
	case FMNA_GATT_CONFIG_SOUND_COMPLETED_IND:
		config_opcode = CONFIG_CP_OPCODE_SOUND_COMPLETED;
		break;
	case FMNA_GATT_CONFIG_SEPARATED_KEY_LATCHED_IND:
		config_opcode = CONFIG_CP_OPCODE_LATCH_SEPARATED_KEY_RESPONSE;
		break;
	case FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND:
		config_opcode = CONFIG_CP_OPCODE_COMMAND_RESPONSE;
		break;
	default:
		LOG_ERR("Config CP: invalid indication type: %d", ind_type);
		return -EINVAL;
	}

	return cp_indicate(conn, &fmns_svc.attrs[FMNS_CONFIG_CHAR_INDEX], config_opcode, buf);
}

int fmna_gatt_non_owner_cp_indicate(struct bt_conn *conn,
				    enum fmna_gatt_non_owner_ind ind_type,
				    struct net_buf_simple *buf)
{
	uint16_t non_owner_opcode;

	switch (ind_type) {
	case FMNA_GATT_NON_OWNER_SOUND_COMPLETED_IND:
		non_owner_opcode = NON_OWNER_CP_OPCODE_SOUND_COMPLETED;
		break;
	case FMNA_GATT_NON_OWNER_COMMAND_RESPONSE_IND:
		non_owner_opcode = NON_OWNER_CP_OPCODE_COMMAND_RESPONSE;
		break;
	default:
		LOG_ERR("Non-owner CP: invalid indication type: %d", ind_type);
		return -EINVAL;
	}

	return cp_indicate(conn, &fmns_svc.attrs[FMNS_NON_OWNER_CHAR_INDEX], non_owner_opcode, buf);
}

int fmna_gatt_owner_cp_indicate(struct bt_conn *conn,
				enum fmna_gatt_owner_ind ind_type,
				struct net_buf_simple *buf)
{
	uint16_t owner_opcode;

	switch (ind_type) {
	case FMNA_GATT_OWNER_PRIMARY_KEY_IND:
		owner_opcode = OWNER_CP_OPCODE_GET_CURRENT_PRIMARY_KEY_RESPONSE;
		break;
	case FMNA_GATT_OWNER_ICLOUD_ID_IND:
		owner_opcode = OWNER_CP_OPCODE_GET_ICLOUD_IDENTIFIER_RESPONSE;
		break;
	case FMNA_GATT_OWNER_SERIAL_NUMBER_IND:
		owner_opcode = OWNER_CP_OPCODE_GET_SERIAL_NUMBER_RESPONSE;
		break;
	case FMNA_GATT_OWNER_COMMAND_RESPONSE_IND:
		owner_opcode = OWNER_CP_OPCODE_COMMAND_RESPONSE;
		break;
	default:
		LOG_ERR("Owner CP: invalid indication type: %d", ind_type);
		return -EINVAL;
	}

	return cp_indicate(conn, &fmns_svc.attrs[FMNS_OWNER_CHAR_INDEX], owner_opcode, buf);
}

uint16_t fmna_config_event_to_gatt_cmd_opcode(enum fmna_config_event_id config_event)
{
	switch (config_event) {
	case FMNA_CONFIG_EVENT_START_SOUND:
		return CONFIG_CP_OPCODE_START_SOUND;
	case FMNA_CONFIG_EVENT_STOP_SOUND:
		return CONFIG_CP_OPCODE_STOP_SOUND;
	case FMNA_CONFIG_EVENT_SET_PERSISTANT_CONN_STATUS:
		return CONFIG_CP_OPCODE_PERSISTANT_CONNECTION_STATUS;
	case FMNA_CONFIG_EVENT_SET_NEARBY_TIMEOUT:
		return CONFIG_CP_OPCODE_SET_NEARBY_TIMEOUT;
	case FMNA_CONFIG_EVENT_UNPAIR:
		return CONFIG_CP_OPCODE_UNPAIR;
	case FMNA_CONFIG_EVENT_CONFIGURE_SEPARATED_STATE:
		return CONFIG_CP_OPCODE_CONFIGURE_SEPARATED_STATE;
	case FMNA_CONFIG_EVENT_SET_MAX_CONNECTIONS:
		return CONFIG_CP_OPCODE_SET_MAX_CONNECTIONS;
	case FMNA_CONFIG_EVENT_SET_UTC:
		return CONFIG_CP_OPCODE_SET_UTC;
	case FMNA_CONFIG_EVENT_GET_MULTI_STATUS:
		return CONFIG_CP_OPCODE_GET_MULTI_STATUS;
	default:
		__ASSERT(0, "Config event type outside the mapping scope: %d",
			config_event);
		return 0;
	}
}

uint16_t fmna_non_owner_event_to_gatt_cmd_opcode(enum fmna_non_owner_event_id non_owner_event)
{
	switch (non_owner_event) {
	case FMNA_NON_OWNER_EVENT_START_SOUND:
		return NON_OWNER_CP_OPCODE_START_SOUND;
	case FMNA_NON_OWNER_EVENT_STOP_SOUND:
		return NON_OWNER_CP_OPCODE_STOP_SOUND;
	default:
		__ASSERT(0, "Owner event type outside the mapping scope: %d",
			non_owner_event);
		return 0;
	}
}

uint16_t fmna_owner_event_to_gatt_cmd_opcode(enum fmna_owner_event_id owner_event)
{
	switch (owner_event) {
	case FMNA_OWNER_EVENT_GET_CURRENT_PRIMARY_KEY:
		return OWNER_CP_OPCODE_GET_CURRENT_PRIMARY_KEY;
	case FMNA_OWNER_EVENT_GET_ICLOUD_IDENTIFIER:
		return OWNER_CP_OPCODE_GET_ICLOUD_IDENTIFIER;
	case FMNA_OWNER_EVENT_GET_SERIAL_NUMBER:
		return OWNER_CP_OPCODE_GET_SERIAL_NUMBER;
	default:
		__ASSERT(0, "Owner event type outside the mapping scope: %d",
			owner_event);
		return 0;
	}
}
