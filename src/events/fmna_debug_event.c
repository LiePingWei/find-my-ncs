#include <stdio.h>

#include "fmna_debug_event.h"

static void log_fmna_debug_event(const struct event_header *eh)
{
	struct fmna_debug_event *event = cast_fmna_debug_event(eh);

	EVENT_MANAGER_LOG(eh, "Event ID: 0x%02X", event->id);
}

EVENT_TYPE_DEFINE(fmna_debug_event,
		  log_fmna_debug_event,
		  NULL,
		  EVENT_FLAGS_CREATE(EVENT_TYPE_FLAGS_INIT_LOG_ENABLE));
