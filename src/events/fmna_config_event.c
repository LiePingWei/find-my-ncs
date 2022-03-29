#include <stdio.h>

#include "fmna_config_event.h"

static void log_fmna_config_event(const struct app_event_header *aeh)
{
	struct fmna_config_event *event = cast_fmna_config_event(aeh);

	APP_EVENT_MANAGER_LOG(aeh, "Event ID: 0x%02X", event->id);
}

APP_EVENT_TYPE_DEFINE(fmna_config_event,
		      log_fmna_config_event,
		      NULL,
		      APP_EVENT_FLAGS_CREATE(APP_EVENT_TYPE_FLAGS_INIT_LOG_ENABLE));
