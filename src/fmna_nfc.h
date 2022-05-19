/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_NFC_H_
#define FMNA_NFC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

int fmna_nfc_init(uint8_t id);

int fmna_nfc_uninit(void);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_NFC_H_ */
