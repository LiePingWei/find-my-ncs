#include "fmna_gatt_pkt_manager.h"

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMN_ADK_LOG_LEVEL);

enum {
	FRAGMENTED_FLAG_START_OR_CONTINUE = 0x0,
	FRAGMENTED_FLAG_FINAL,
};

bool fmna_gatt_pkt_manager_chunk_collect(struct net_buf_simple *pkt,
					 const uint8_t *chunk,
					 uint16_t chunk_len)
{
	bool pkt_complete = false;

	switch (chunk[0]) {
	case FRAGMENTED_FLAG_START_OR_CONTINUE:
		break;
	case FRAGMENTED_FLAG_FINAL:
		pkt_complete = true;
		break;
	default:
		LOG_ERR("FMN Packet header: unexpected value: 0x%02X", chunk[0]);
		return false;
	}
	chunk += FMNA_GATT_PKT_HEADER_LEN;
	chunk_len--;

	net_buf_simple_add_mem(pkt, chunk, chunk_len);

	return pkt_complete;
}

void *fmna_gatt_pkt_manager_chunk_prepare(struct net_buf_simple *pkt,
					  uint16_t *chunk_len)
{
	uint8_t *chunk;

	if (pkt->len == 0 || *chunk_len == 0) {
		return NULL;
	}

	if (*chunk_len > pkt->len) {
		/* The last chunk */
		net_buf_simple_push_u8(pkt, FRAGMENTED_FLAG_FINAL);
		*chunk_len = pkt->len;
	} else {
		net_buf_simple_push_u8(pkt, FRAGMENTED_FLAG_START_OR_CONTINUE);
	}

	chunk = net_buf_simple_pull_mem(pkt, *chunk_len);

	return chunk;
}
