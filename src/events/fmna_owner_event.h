#ifndef FMNA_OWNER_EVENT_H_
#define FMNA_OWNER_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "event_manager.h"

enum fmna_owner_event_id {
	FMNA_OWNER_EVENT_GET_CURRENT_PRIMARY_KEY,
	FMNA_OWNER_EVENT_GET_ICLOUD_IDENTIFIER,
	FMNA_OWNER_EVENT_GET_SERIAL_NUMBER,
};

struct fmna_owner_event {
	struct event_header header;

	enum fmna_owner_event_id id;
	struct bt_conn *conn;
};

EVENT_TYPE_DECLARE(fmna_owner_event);

#ifdef __cplusplus
}
#endif

#endif /* FMNA_OWNER_EVENT_H_ */
