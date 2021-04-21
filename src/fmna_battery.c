#include <kernel.h>
#include <init.h>
#include <string.h>
#include <sys/byteorder.h>

#include "fmna_battery.h"

#define BATTERY_LEVEL_UNDEFINED (UINT8_MAX)
#define BATTERY_LEVEL_MAX       100

BUILD_ASSERT((CONFIG_FMNA_BATTERY_STATE_MEDIUM_THR < BATTERY_LEVEL_MAX) &&
	(CONFIG_FMNA_BATTERY_STATE_MEDIUM_THR > CONFIG_FMNA_BATTERY_STATE_LOW_THR) &&
	(CONFIG_FMNA_BATTERY_STATE_LOW_THR > CONFIG_FMNA_BATTERY_STATE_CRITICAL_THR),
	"The battery level thresholds are incorrect");

static uint8_t battery_level;
static fmna_battery_level_request_cb_t battery_level_request_cb;

enum fmna_battery_state fmna_battery_state_get(void)
{
	battery_level_request_cb();

	if (battery_level > CONFIG_FMNA_BATTERY_STATE_MEDIUM_THR) {
		return FMNA_BATTERY_STATE_FULL;
	} else if (battery_level > CONFIG_FMNA_BATTERY_STATE_LOW_THR) {
		return FMNA_BATTERY_STATE_MEDIUM;
	} else if (battery_level > CONFIG_FMNA_BATTERY_STATE_CRITICAL_THR) {
		return FMNA_BATTERY_STATE_LOW;
	} else {
		return FMNA_BATTERY_STATE_CRITICALLY_LOW;
	}
}

int fmna_battery_level_set(uint8_t percentage_level)
{
	if (percentage_level > BATTERY_LEVEL_MAX) {
		return -EINVAL;
	}

	battery_level = percentage_level;

	return 0;
}

int fmna_battery_init(fmna_battery_level_request_cb_t cb)
{
	if (!cb) {
		return -EINVAL;
	}

	battery_level_request_cb = cb;
	battery_level = BATTERY_LEVEL_UNDEFINED;

	battery_level_request_cb();

	if (battery_level == BATTERY_LEVEL_UNDEFINED) {
		return -ENODATA;
	}

	return 0;
}
