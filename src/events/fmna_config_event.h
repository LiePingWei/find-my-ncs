#ifndef FMNA_CONFIG_EVENT_H_
#define FMNA_CONFIG_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "event_manager.h"

enum fmna_config_event_id {
	FMNA_CONFIG_EVENT_START_SOUND,
	FMNA_CONFIG_EVENT_STOP_SOUND,
	FMNA_CONFIG_EVENT_SET_PERSISTANT_CONN_STATUS,
	FMNA_CONFIG_EVENT_SET_NEARBY_TIMEOUT,
	FMNA_CONFIG_EVENT_UNPAIR,
	FMNA_CONFIG_EVENT_CONFIGURE_SEPARATED_STATE,
	FMNA_CONFIG_EVENT_LATCH_SEPARATED_KEY,
	FMNA_CONFIG_EVENT_SET_MAX_CONNECTIONS,
	FMNA_CONFIG_EVENT_SET_UTC,
	FMNA_CONFIG_EVENT_GET_MULTI_STATUS,
};

struct fmna_separated_state {
	uint32_t next_primary_key_roll;
	uint32_t seconday_key_evaluation_index;
};

struct fmna_utc {
	uint64_t current_time;
};

struct fmna_config_event {
	struct event_header header;

	enum fmna_config_event_id id;
	struct bt_conn *conn;

	union {
		uint8_t persistant_conn_status;
		uint16_t nearby_timeout;
		struct fmna_separated_state separated_state;
		uint8_t max_connections;
		struct fmna_utc utc;
	};
};

EVENT_TYPE_DECLARE(fmna_config_event);

#ifdef __cplusplus
}
#endif

#endif /* FMNA_CONFIG_EVENT_H_ */