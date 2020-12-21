#ifndef FMNA_GATT_PKT_MANGER_H_
#define FMNA_GATT_PKT_MANGER_H_

#include <zephyr.h>
#include <bluetooth/conn.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FMNA_GATT_PKT_HEADER_LEN 1

#define FMNA_GATT_PKT_MAX_LEN 1394

bool fmna_gatt_pkt_manager_chunk_collect(struct net_buf_simple *pkt,
					 const uint8_t *chunk,
					 uint16_t chunk_len);

/* API expects buffer with 1 byte of headroom. */
void *fmna_gatt_pkt_manager_chunk_prepare(struct net_buf_simple *pkt,
					  uint16_t *chunk_len);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_GATT_PKT_MANGER_H_ */