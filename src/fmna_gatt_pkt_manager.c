/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "fmna_gatt_pkt_manager.h"

#include <bluetooth/gatt.h>

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

#define BT_ATT_HEADER_LEN 3

enum {
	FRAGMENTED_FLAG_START_OR_CONTINUE = 0x0,
	FRAGMENTED_FLAG_FINAL,
};

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

int fmna_gatt_pkt_manager_chunk_collect(struct net_buf_simple *pkt,
					const uint8_t *chunk,
					uint16_t chunk_len,
					bool *pkt_complete)
{
	*pkt_complete = false;

	if (!chunk_len) {
		LOG_ERR("FMN Packet: 0 length");
		return -EINVAL;
	}

	switch (chunk[0]) {
	case FRAGMENTED_FLAG_START_OR_CONTINUE:
		break;
	case FRAGMENTED_FLAG_FINAL:
		*pkt_complete = true;
		break;
	default:
		LOG_ERR("FMN Packet header: unexpected value: 0x%02X", chunk[0]);
		return -EINVAL;
	}
	chunk += FMNA_GATT_PKT_HEADER_LEN;
	chunk_len--;

	if (net_buf_simple_tailroom(pkt) < chunk_len) {
		LOG_ERR("FMN Packet too big, %d bytes overflow", chunk_len - net_buf_simple_tailroom(pkt));
		return -ENOMEM;
	}

	net_buf_simple_add_mem(pkt, chunk, chunk_len);

	return 0;
}

void *fmna_gatt_pkt_manager_chunk_prepare(struct bt_conn *conn, struct net_buf_simple *pkt,
					  uint16_t *chunk_len)
{
	uint8_t *chunk;
	uint16_t max_len;

	max_len = pairing_ind_len_get(conn);

	if (pkt->len == 0 || max_len == 0) {
		return NULL;
	}

	if (max_len > pkt->len) {
		/* The last chunk */
		net_buf_simple_push_u8(pkt, FRAGMENTED_FLAG_FINAL);
		*chunk_len = pkt->len;
	} else {
		net_buf_simple_push_u8(pkt, FRAGMENTED_FLAG_START_OR_CONTINUE);
		*chunk_len = max_len;
	}

	chunk = net_buf_simple_pull_mem(pkt, *chunk_len);

	return chunk;
}
