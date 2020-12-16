#include "fmna_gatt_pkt_manager.h"

#include <logging/log.h>

LOG_MODULE_REGISTER(fmna_gatt_pkt_manager);

#define FRAGMENTATION_HEADER_LENGTH 1

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
	chunk += FRAGMENTATION_HEADER_LENGTH;
	chunk_len--;

	net_buf_simple_add_mem(pkt, chunk, chunk_len);

	return pkt_complete;
}
