#ifndef FMNA_GATT_FMNS_H_
#define FMNA_GATT_FMNS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>
#include <bluetooth/conn.h>
#include <net/buf.h>

int fmns_pairing_data_indicate(struct bt_conn *conn,
			       struct net_buf_simple *buf);

int fmns_pairing_status_indicate(struct bt_conn *conn,
				 struct net_buf_simple *buf);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_GATT_FMNS_H_ */