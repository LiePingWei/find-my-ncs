/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_UARP_SERVICE_H_
#define FMNA_UARP_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

int fmna_uarp_service_hidden_mode_set(bool hidden_mode);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_UARP_SERVICE_H_ */
