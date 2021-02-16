#include <stdio.h>

#include "fmna_event.h"

static int log_fmna_event(const struct event_header *eh, char *buf,
			  size_t buf_len)
{
	struct fmna_event *event = cast_fmna_event(eh);

	return snprintf(buf, buf_len, "Opcode: 0x%02X", event->id);
}

EVENT_TYPE_DEFINE(fmna_event, true, log_fmna_event, NULL);
