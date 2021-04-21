#ifndef FMNA_GATT_FMNS_H_
#define FMNA_GATT_FMNS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>
#include <bluetooth/conn.h>
#include <net/buf.h>

#include "events/fmna_non_owner_event.h"
#include "events/fmna_owner_event.h"
#include "events/fmna_config_event.h"
#include "events/fmna_debug_event.h"

#define FMNA_GATT_COMMAND_OPCODE_LEN 2
#define FMNA_GATT_COMMAND_STATUS_LEN 2
#define FMNA_GATT_COMMAND_RESPONSE_BUILD(_name, _opcode, _status)	    \
	BUILD_ASSERT(sizeof(_opcode) == FMNA_GATT_COMMAND_OPCODE_LEN,	    \
		"FMNA GATT Command response opcode has 2 bytes");	    \
	BUILD_ASSERT(sizeof(_status) <= UINT16_MAX,			    \
		"FMNA GATT Command status opcode has 2 bytes");		    \
	NET_BUF_SIMPLE_DEFINE(_name, (sizeof(_opcode) + sizeof(_status)));  \
	net_buf_simple_add_le16(&_name, _opcode);			    \
	net_buf_simple_add_le16(&_name, _status);

enum fmna_gatt_pairing_ind {
	FMNA_GATT_PAIRING_DATA_IND,
	FMNA_GATT_PAIRING_STATUS_IND
};

enum fmna_gatt_config_ind {
	FMNA_GATT_CONFIG_KEYROLL_IND,
	FMNA_GATT_CONFIG_MULTI_STATUS_IND,
	FMNA_GATT_CONFIG_SOUND_COMPLETED_IND,
	FMNA_GATT_CONFIG_SEPARATED_KEY_LATCHED_IND,
	FMNA_GATT_CONFIG_COMMAND_RESPONSE_IND,
};

enum fmna_gatt_non_owner_ind {
	FMNA_GATT_NON_OWNER_SOUND_COMPLETED_IND,
	FMNA_GATT_NON_OWNER_COMMAND_RESPONSE_IND,
};

enum fmna_gatt_owner_ind {
	FMNA_GATT_OWNER_PRIMARY_KEY_IND,
	FMNA_GATT_OWNER_ICLOUD_ID_IND,
	FMNA_GATT_OWNER_SERIAL_NUMBER_IND,
	FMNA_GATT_OWNER_COMMAND_RESPONSE_IND
};

enum fmna_gatt_debug_ind {
	FMNA_GATT_DEBUG_LOG_RESPONSE_IND,
	FMNA_GATT_DEBUG_COMMAND_RESPONSE_IND
};

enum fmna_gatt_response_status {
	FMNA_GATT_RESPONSE_STATUS_SUCCESS               = 0x0000,
	FMNA_GATT_RESPONSE_STATUS_INVALID_STATE         = 0x0001,
	FMNA_GATT_RESPONSE_STATUS_INVALID_CONFIGURATION = 0x0002,
	FMNA_GATT_RESPONSE_STATUS_INVALID_LENGTH        = 0x0003,
	FMNA_GATT_RESPONSE_STATUS_INVALID_PARAM         = 0x0004,
	FMNA_GATT_RESPONSE_STATUS_NO_COMMAND_RESPONSE   = 0xFFFE,
	FMNA_GATT_RESPONSE_STATUS_INVALID_COMMAND       = 0xFFFF,
};

int fmna_gatt_pairing_cp_indicate(struct bt_conn *conn,
				  enum fmna_gatt_pairing_ind ind_type,
				  struct net_buf_simple *buf);

int fmna_gatt_config_cp_indicate(struct bt_conn *conn,
				 enum fmna_gatt_config_ind ind_type,
				 struct net_buf_simple *buf);

int fmna_gatt_non_owner_cp_indicate(struct bt_conn *conn,
				    enum fmna_gatt_non_owner_ind ind_type,
				    struct net_buf_simple *buf);

int fmna_gatt_owner_cp_indicate(struct bt_conn *conn,
				enum fmna_gatt_owner_ind ind_type,
				struct net_buf_simple *buf);

int fmna_gatt_debug_cp_indicate(struct bt_conn *conn,
				enum fmna_gatt_debug_ind ind_type,
				struct net_buf_simple *buf);

uint16_t fmna_config_event_to_gatt_cmd_opcode(enum fmna_config_event_id config_event);

uint16_t fmna_non_owner_event_to_gatt_cmd_opcode(enum fmna_non_owner_event_id non_owner_event);

uint16_t fmna_owner_event_to_gatt_cmd_opcode(enum fmna_owner_event_id owner_event);

uint16_t fmna_debug_event_to_gatt_cmd_opcode(enum fmna_debug_event_id debug_event);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_GATT_FMNS_H_ */
