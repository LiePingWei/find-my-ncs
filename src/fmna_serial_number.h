/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_SERIAL_NUMBER_H_
#define FMNA_SERIAL_NUMBER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

#define FMNA_SERIAL_NUMBER_BLEN 16
#define FMNA_SERIAL_NUMBER_ENC_BLEN 141

enum fmna_serial_number_enc_query_type {
	FMNA_SERIAL_NUMBER_ENC_QUERY_TYPE_TAP,
	FMNA_SERIAL_NUMBER_ENC_QUERY_TYPE_BT,
};

int fmna_serial_number_get(uint8_t serial_number[FMNA_SERIAL_NUMBER_BLEN]);

int fmna_serial_number_enc_get(
	enum fmna_serial_number_enc_query_type query_type,
	uint8_t serial_number_enc[FMNA_SERIAL_NUMBER_ENC_BLEN]);


#ifdef __cplusplus
}
#endif


#endif /* FMNA_SERIAL_NUMBER_H_ */
