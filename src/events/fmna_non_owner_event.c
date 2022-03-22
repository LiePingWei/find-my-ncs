#include <stdio.h>

#include "fmna_non_owner_event.h"

static void log_fmna_non_owner_event(const struct event_header *eh)
{
	struct fmna_non_owner_event *event = cast_fmna_non_owner_event(eh);

	EVENT_MANAGER_LOG(eh, "Event ID: 0x%02X", event->id);
}

EVENT_TYPE_DEFINE(fmna_non_owner_event, true, log_fmna_non_owner_event, NULL);
