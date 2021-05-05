/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_BATTERY_H_
#define FMNA_BATTERY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

enum fmna_battery_state {
	FMNA_BATTERY_STATE_FULL,
	FMNA_BATTERY_STATE_MEDIUM,
	FMNA_BATTERY_STATE_LOW,
	FMNA_BATTERY_STATE_CRITICALLY_LOW,
};

typedef void (*fmna_battery_level_request_cb_t)(void);

enum fmna_battery_state fmna_battery_state_get(void);

int fmna_battery_init(fmna_battery_level_request_cb_t cb);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_BATTERY_H_ */
