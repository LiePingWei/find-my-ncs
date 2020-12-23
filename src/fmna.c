#include "fmna_pair.h"
#include "fmna_gatt_fmns.h"
#include "events/fmna_pair_event.h"

#include <logging/log.h>

LOG_MODULE_REGISTER(fmna, CONFIG_FMN_ADK_LOG_LEVEL);

int fmna_init(void)
{
	return 0;
}

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
