/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_GATT_AIS_H_
#define FMNA_GATT_AIS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

int fmna_gatt_ais_hidden_mode_set(bool hidden_mode);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_GATT_AIS_H_ */
