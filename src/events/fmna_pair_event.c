#include <stdio.h>

#include "fmna_pair_event.h"

static int log_fmna_pair_event(const struct event_header *eh, char *buf,
			       size_t buf_len)
{
	struct fmna_pair_event *event = cast_fmna_pair_event(eh);

	return snprintf(buf, buf_len, "Opcode: 0x%02X", event->op);
}

EVENT_TYPE_DEFINE(fmna_pair_event, true, log_fmna_pair_event, NULL);
