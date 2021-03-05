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

enum fmna_gatt_owner_ind {
	FMNA_GATT_OWNER_PRIMARY_KEY_IND,
	FMNA_GATT_OWNER_ICLOUD_ID_IND,
	FMNA_GATT_OWNER_SERIAL_NUMBER_IND,
	FMNA_GATT_OWNER_COMMAND_RESPONSE_IND
};

int fmna_gatt_pairing_cp_indicate(struct bt_conn *conn,
				  enum fmna_gatt_pairing_ind ind_type,
				  struct net_buf_simple *buf);

int fmna_gatt_owner_cp_indicate(struct bt_conn *conn,
				enum fmna_gatt_owner_ind ind_type,
				struct net_buf_simple *buf);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_GATT_FMNS_H_ */