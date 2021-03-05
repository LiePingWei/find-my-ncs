#ifndef FMNA_GATT_FMNS_H_
#define FMNA_GATT_FMNS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>
#include <bluetooth/conn.h>
#include <net/buf.h>

enum fmna_gatt_pairing_ind {
	FMNA_GATT_PAIRING_DATA_IND,
	FMNA_GATT_PAIRING_STATUS_IND
};

int fmna_gatt_pairing_cp_indicate(struct bt_conn *conn,
				  enum fmna_gatt_pairing_ind ind_type,
				  struct net_buf_simple *buf);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_GATT_FMNS_H_ */