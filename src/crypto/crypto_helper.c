/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "crypto_helper.h"

#include <random/rand32.h>

#include "ocrypto_sha256.h"
#include "ocrypto_constant_time.h"
#include "ocrypto_ecdh_p224.h"
#include "ocrypto_ecdh_p256.h"
#include "ocrypto_ecdsa_p256.h"
#include "ocrypto_sc_p224.h"
#include "ocrypto_sc_p256.h"
#include "ocrypto_curve_p224.h"
#include "ocrypto_curve_p256.h"

#define LOG_MODULE_NAME fmna_crypto_helper
#include "crypto_log.h"

int generate_random(uint8_t *out, size_t num_bytes)
{
	int err;

	if (out == NULL) {
		return FMN_ERROR_CRYPTO_INVALID_INPUT;
	}

	/* Use Zephyr Random subsystem for random number generation */
	err = sys_csrand_get(out, num_bytes);
	if(err) {
		return FMN_ERROR_CRYPTO_RNG_ERROR;
	}

	return FMN_ERROR_CRYPTO_OK;
}

int ecc_gen_keypair(ecc_key * const out_key, ecc_set_type dp)
{
	int gen_ret;
	int gen_length;

	if (out_key == NULL) {
		return FMN_ERROR_CRYPTO_INVALID_INPUT;
	}

	ocrypto_sc_p224 *priv_p224 = &out_key->private_key.scalar_p224;
	ocrypto_cp_p224 *pub_p224 = &out_key->public_key.point_p224;
	ocrypto_sc_p256 *priv_p256 = &out_key->private_key.scalar_p256;
	ocrypto_cp_p256 *pub_p256 = &out_key->public_key.point_p256;

	/* Using the same buffer for both p256 and p224 generation. */
	uint8_t *priv_buf = out_key->private_key.buffer;

	switch(dp) {
	case ECC_TYPE_P224:
		gen_length = 28;
		break;
	case ECC_TYPE_P256:
		gen_length = 32;
		break;
	default:
		return FMN_ERROR_CRYPTO_INVALID_SIZE;
	}

	do {
		/* buffer is as large at the biggest type */
		gen_ret = generate_random(priv_buf, gen_length);
		if (gen_ret != 0) {
			ocrypto_constant_time_fill_zero(priv_buf, gen_length);
			return -2;
		}

		switch(dp) {
		case ECC_TYPE_P224:
			gen_ret = ocrypto_sc_p224_from28bytes(
				priv_p224,
				priv_buf);
			gen_ret |= ocrypto_curve_p224_scalarmult_base(
				pub_p224,
				priv_p224);
			break;

		case ECC_TYPE_P256:
			gen_ret = ocrypto_sc_p256_from32bytes(
				priv_p256,
				priv_buf);
			gen_ret |= ocrypto_curve_p256_scalarmult_base(
				pub_p256,
				priv_p256);
			break;

		default:
			return -1;
		}
	} while (gen_ret != 0);

	return 0;
}


int ansi_x963_kdf(uint8_t * const output,
		  size_t output_len,
		  uint8_t const *key,
		  size_t key_len,
		  uint8_t const *shared_info,
		  size_t shared_info_len)
{
	/* Hash length is 32 bytes for each block */
	const size_t digest_len = 32;

	size_t counter = 1;
	uint8_t counter_buf[4];

	ocrypto_sha256_ctx hash_ctx;
	uint8_t digest[32];

	size_t remainder = output_len;
	size_t pos = 0;

	LOG_DBG("ansi_x963_kdf");

	do {
		LOG_DBG("loop %d", counter);

		/* Initialize/reset the hash context */
		ocrypto_sha256_init(&hash_ctx);

		/* Begin by adding the input */
		ocrypto_sha256_update(&hash_ctx, key, key_len);

		LOG_HEXDUMP_DBG(key, key_len, "key");

		/* Add the counter value with big-endian representation */
		counter_buf[3] = (uint8_t)(counter          & 0xff);
		counter_buf[2] = (uint8_t)((counter >> 8)   & 0xff);
		counter_buf[1] = (uint8_t)((counter >> 16)  & 0xff);
		counter_buf[0] = (uint8_t)((counter >> 24)  & 0xff);

		ocrypto_sha256_update(&hash_ctx, counter_buf, 4);

		LOG_DBG("counter %d", counter);
		LOG_HEXDUMP_DBG(counter_buf, 4, "");

		/* Add shared info (if present, and has length) */
		if (shared_info != NULL && shared_info_len > 0) {
			ocrypto_sha256_update(&hash_ctx, shared_info, shared_info_len);

			LOG_HEXDUMP_DBG(shared_info, shared_info_len, "shared_info");
		}

		ocrypto_sha256_final(&hash_ctx, digest);

		/* Copy a full "frame" or remainder */
		if (remainder >= digest_len) {
			LOG_HEXDUMP_DBG(digest, digest_len, "digest");

			ocrypto_constant_time_copy(output + pos, digest, digest_len);
			remainder -= 32;
			pos += 32;
		} else {
			LOG_HEXDUMP_DBG(digest, remainder, "digest");

			ocrypto_constant_time_copy(output + pos, digest, remainder);
			remainder = 0;
		}

		LOG_DBG("remaining %d", remainder);

		/* Update counter for next iteration */
		counter++;

	} while (remainder > 0);

	return 0;
}
