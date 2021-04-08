#include <stdio.h>

#include "fmna_config_event.h"

static int log_fmna_config_event(const struct event_header *eh, char *buf,
			       size_t buf_len)
{
	struct fmna_config_event *event = cast_fmna_config_event(eh);

	return snprintf(buf, buf_len, "Event ID: 0x%02X", event->id);
}

EVENT_TYPE_DEFINE(fmna_config_event, true, log_fmna_config_event, NULL);
