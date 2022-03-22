#include <stdio.h>

#include "fmna_event.h"

static void log_fmna_event(const struct event_header *eh)
{
	struct fmna_event *event = cast_fmna_event(eh);

	EVENT_MANAGER_LOG(eh, "Opcode: 0x%02X", event->id);
}

EVENT_TYPE_DEFINE(fmna_event, true, log_fmna_event, NULL);
