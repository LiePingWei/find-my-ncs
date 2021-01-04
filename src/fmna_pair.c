#include "fmna_gatt_fmns.h"
#include "events/fmna_pair_event.h"

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMN_ADK_LOG_LEVEL);

#define C1_BLEN                32
#define C2_BLEN                89
#define C3_BLEN                60

#define E1_BLEN                113
#define E2_BLEN                1326
#define E3_BLEN                1040
#define E4_BLEN                1286

#define S2_BLEN                100

#define SESSION_NONCE_BLEN     32
#define SEEDS_BLEN             32
#define ICLOUD_IDENTIFIER_BLEN 60

struct __packed fmna_initiate_pairing {
	uint8_t session_nonce[SESSION_NONCE_BLEN];
	uint8_t e1[E1_BLEN];
};

struct __packed fmna_send_pairing_data {
	uint8_t c1[C1_BLEN];
	uint8_t e2[E2_BLEN];
};

struct __packed fmna_finalize_pairing {
	uint8_t c2[C2_BLEN];
	uint8_t e3[E3_BLEN];
	uint8_t seeds[SEEDS_BLEN];
	uint8_t icloud_id[ICLOUD_IDENTIFIER_BLEN];
	uint8_t s2[S2_BLEN];
};

struct __packed fmna_send_pairing_status {
	uint8_t  c3[C3_BLEN];
	uint32_t status;
	uint8_t  e4[E4_BLEN];
};

static void initiate_pairing_cmd_handle(struct bt_conn *conn,
					struct fmna_pair_buf *buf)
{
	int err;
	struct net_buf_simple buf_desc;

	LOG_INF("FMNA: RX: Initiate pairing command");

	/* Initialize buffer descriptor */
	net_buf_simple_init_with_data(&buf_desc, buf->data, sizeof(buf->data));
	net_buf_simple_reset(&buf_desc);

	/* Use command buffer memory to store response data. */
	net_buf_simple_add(&buf_desc, sizeof(struct fmna_send_pairing_data));
	memset(buf_desc.data, 0xFF, buf_desc.len);

	err = fmns_pairing_data_indicate(conn, &buf_desc);
	if (err) {
		LOG_ERR("fmns_pairing_data_indicate returned error: %d", err);
	}
}

static void finalize_pairing_cmd_handle(struct bt_conn *conn,
					struct fmna_pair_buf *buf)
{
	int err;
	struct net_buf_simple buf_desc;

	LOG_INF("FMNA: RX: Finalize pairing command");

	/* Initialize buffer descriptor */
	net_buf_simple_init_with_data(&buf_desc, buf->data, sizeof(buf->data));
	net_buf_simple_reset(&buf_desc);

	/* Use command buffer memory to store response data. */
	net_buf_simple_add(&buf_desc, sizeof(struct fmna_send_pairing_status));
	memset(buf_desc.data, 0xFF, buf_desc.len);

	err = fmns_pairing_status_indicate(conn, &buf_desc);
	if (err) {
		LOG_ERR("fmns_pairing_status_indicate returned error: %d",
			err);
	}
}

static void pairing_complete_cmd_handle(struct bt_conn *conn,
					struct fmna_pair_buf *buf)
{
	LOG_INF("FMNA: RX: Pairing complete command");
}

static void pairing_cmd_handle(struct fmna_pair_event *pair_event)
{
	switch (pair_event->op) {
	case FMNA_INITIATE_PAIRING:
		initiate_pairing_cmd_handle(pair_event->conn,
					    &pair_event->buf);
		break;
	case FMNA_FINALIZE_PAIRING:
		finalize_pairing_cmd_handle(pair_event->conn,
					    &pair_event->buf);
		break;
	case FMNA_PAIRING_COMPLETE:
		pairing_complete_cmd_handle(pair_event->conn,
					    &pair_event->buf);
		break;
	default:
		LOG_ERR("FMNA: unexpected pairing command opcode: 0x%02X",
			pair_event->op);
	}
}

static bool event_handler(const struct event_header *eh)
{
	if (is_fmna_pair_event(eh)) {
		struct fmna_pair_event *event = cast_fmna_pair_event(eh);

		pairing_cmd_handle(event);

		return false;
	}

	return false;
}

EVENT_LISTENER(fmna, event_handler);
EVENT_SUBSCRIBE(fmna, fmna_pair_event);
