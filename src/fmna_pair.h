/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_PAIR_H_
#define FMNA_PAIR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

typedef void (*fmna_pair_failed_t)(void);

int fmna_pair_failed_cb_register(fmna_pair_failed_t cb);

int fmna_pair_init(uint8_t bt_id);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_PAIR_H_ */
