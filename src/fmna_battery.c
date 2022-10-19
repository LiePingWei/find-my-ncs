/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <fmna.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>

#include "events/fmna_event.h"

#include "fmna_battery.h"

#define BATTERY_LEVEL_MAX       100

BUILD_ASSERT((CONFIG_FMNA_BATTERY_STATE_MEDIUM_THR < BATTERY_LEVEL_MAX) &&
	(CONFIG_FMNA_BATTERY_STATE_MEDIUM_THR > CONFIG_FMNA_BATTERY_STATE_LOW_THR) &&
	(CONFIG_FMNA_BATTERY_STATE_LOW_THR > CONFIG_FMNA_BATTERY_STATE_CRITICAL_THR),
	"The battery level thresholds are incorrect");

static uint8_t battery_level;
static fmna_battery_level_request_cb_t battery_level_request_cb;

enum fmna_battery_state fmna_battery_state_get_no_cb(void)
{
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

enum fmna_battery_state fmna_battery_state_get(void)
{
	if (battery_level_request_cb) {
		battery_level_request_cb();
	}

	return fmna_battery_state_get_no_cb();
}

int fmna_battery_level_set(uint8_t percentage_level)
{
	if (percentage_level > BATTERY_LEVEL_MAX) {
		return -EINVAL;
	}

	battery_level = percentage_level;

	FMNA_EVENT_CREATE(event, FMNA_EVENT_BATTERY_LEVEL_CHANGED, NULL);
	APP_EVENT_SUBMIT(event);

	return 0;
}

int fmna_battery_init(uint8_t init_battery_level, fmna_battery_level_request_cb_t cb)
{
	int err;

	battery_level_request_cb = cb;

	err = fmna_battery_level_set(init_battery_level);
	if (err) {
		return err;
	}

	return 0;
}
