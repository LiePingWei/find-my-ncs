#ifndef FMNA_NON_OWNER_EVENT_H_
#define FMNA_NON_OWNER_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "event_manager.h"

enum fmna_non_owner_event_id {
	FMNA_NON_OWNER_EVENT_START_SOUND,
	FMNA_NON_OWNER_EVENT_STOP_SOUND,
};

struct fmna_non_owner_event {
	struct event_header header;

	enum fmna_non_owner_event_id id;
	struct bt_conn *conn;
};

EVENT_TYPE_DECLARE(fmna_non_owner_event);

#ifdef __cplusplus
}
#endif

#endif /* FMNA_NON_OWNER_EVENT_H_ */
