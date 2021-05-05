/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef CRYPTO_HELPER_H_
#define CRYPTO_HELPER_H_

#include <stdint.h>
#include <stddef.h>

#include "fm_crypto_platform.h"

/**
 * @brief Function to generate random number
 * 
 * @param[in,out]   out         Pointer to buffer for rng output
 * @param[in]       num_bytes   Length of RNG to generate
 * 
 * @returns 0 on success, otherwise negative value.
 */
int generate_random(uint8_t *out, size_t num_bytes);

/**
 * @brief Function to create a private/public keypair given curve info
 *
 * @param[in,out]   out_key     Generated key pair.
 * @param[in,out]   out_pub     Copy of generated public key.
 * @param[in]       dp          ECC Curve info.
 *
 * @returns 0 on success, otherwise negative value.
 */
int ecc_gen_keypair(ecc_key * const out_key, ecc_set_type dp);

/**
 * @brief Function to derive key using ANSI X9.63
 *
 * @param[in,out]   output              Pointer to array for output of KDF.
 * @param[in]       output_len          Length of output to generate in KDF.
 * @param[in]       key                 Pointer to array with key input (known as Z).
 * @param[in]       key_len             Key input length (length of Z).
 * @param[in]       shared_info         Pointer to SharedInfo array.
 * @param[in]       shared_info_len     Length of SharedInfo.
 *
 * @returns 0 on success, otherwise negative value.
 */
int ansi_x963_kdf(uint8_t * const output,
		  size_t output_len,
		  uint8_t const *key,
		  size_t key_len,
		  uint8_t const *shared_info,
		  size_t shared_info_len);

#endif /* CRYPTO_HELPER_H_ */
