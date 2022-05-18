/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_STATE_H_
#define FMNA_STATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

enum fmna_state {
	FMNA_STATE_UNPAIRED,
	FMNA_STATE_CONNECTED,
	FMNA_STATE_NEARBY,
	FMNA_STATE_SEPARATED,

	FMNA_STATE_UNDEFINED,
};

typedef void (*fmna_state_location_availability_changed_t)(bool available);

typedef void (*fmna_state_paired_state_changed_t)(bool paired);

enum fmna_state fmna_state_get(void);

bool fmna_state_is_paired(void);

int fmna_state_pause(void);

int fmna_state_resume(void);

int fmna_state_init(uint8_t bt_id);

int fmna_state_location_availability_cb_register(
	fmna_state_location_availability_changed_t cb);

int fmna_state_paired_state_changed_cb_register(
	fmna_state_paired_state_changed_t cb);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_STATE_H_ */
