#include "events/fmna_pair_event.h"

#include <logging/log.h>

LOG_MODULE_REGISTER(fmna, 3);

int fmna_init(void)
{
	return 0;
}

static void pairing_cmd_handler(struct fmna_pair_event *pair_event)
{
	LOG_INF("FMNA pair event: 0x%02X", pair_event->op);
	LOG_HEXDUMP_INF(pair_event->buf.data, pair_event->buf.len,
			"Pairing packet:");
	LOG_INF("Total packet length: %d", pair_event->buf.len);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_fmna_pair_event(eh)) {
		struct fmna_pair_event *event = cast_fmna_pair_event(eh);

		pairing_cmd_handler(event);

		return false;
	}

	return false;
}

EVENT_LISTENER(fmna, event_handler);
EVENT_SUBSCRIBE(fmna, fmna_pair_event);
