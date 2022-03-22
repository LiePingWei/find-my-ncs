#include <stdio.h>

#include "fmna_debug_event.h"

static void log_fmna_debug_event(const struct event_header *eh)
{
	struct fmna_debug_event *event = cast_fmna_debug_event(eh);

	EVENT_MANAGER_LOG(eh, "Event ID: 0x%02X", event->id);
}

EVENT_TYPE_DEFINE(fmna_debug_event, true, log_fmna_debug_event, NULL);
