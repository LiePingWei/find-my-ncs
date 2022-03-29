#ifndef FMNA_DEBUG_EVENT_H_
#define FMNA_DEBUG_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "app_event_manager.h"

enum fmna_debug_event_id {
	FMNA_DEBUG_EVENT_SET_KEY_ROTATION_TIMEOUT,
	FMNA_DEBUG_EVENT_RETRIEVE_LOGS,
	FMNA_DEBUG_EVENT_RESET,
	FMNA_DEBUG_EVENT_CONFIGURE_UT_TIMERS,
};

struct fmna_debug_event {
	struct app_event_header header;

	enum fmna_debug_event_id id;
	struct bt_conn *conn;

	union {
		uint32_t key_rotation_timeout;
		struct {
			uint32_t separated_ut_timeout;
			uint32_t separated_ut_backoff;
		} configure_ut_timers;
	};
};

APP_EVENT_TYPE_DECLARE(fmna_debug_event);

#ifdef __cplusplus
}
#endif

#endif /* FMNA_DEBUG_EVENT_H_ */
