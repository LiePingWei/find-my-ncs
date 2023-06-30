/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/ztest.h>

#include <ocrypto_aes_gcm.h>
#include <ocrypto_ecdh_p256.h>

#include "fm_crypto.h"
#include "crypto_helper.h"

#define LOG_MODULE_NAME fmna_crypto_test_ecies
#include "crypto_log.h"

/*
 * <https://tools.ietf.org/html/rfc6979#appendix-A.2.5>
 */
static const byte d[32] = {
	0xc9, 0xaf, 0xa9, 0xd8, 0x45, 0xba, 0x75, 0x16,
	0x6b, 0x5c, 0x21, 0x57, 0x67, 0xb1, 0xd6, 0x93,
	0x4e, 0x50, 0xc3, 0xdb, 0x36, 0xe8, 0x9b, 0x12,
	0x7b, 0x8a, 0x62, 0x2b, 0x12, 0x0f, 0x67, 0x21
};

static const byte Q[65] = {
	0x04,
	0x60, 0xfe, 0xd4, 0xba, 0x25, 0x5a, 0x9d, 0x31,
	0xc9, 0x61, 0xeb, 0x74, 0xc6, 0x35, 0x6d, 0x68,
	0xc0, 0x49, 0xb8, 0x92, 0x3b, 0x61, 0xfa, 0x6c,
	0xe6, 0x69, 0x62, 0x2e, 0x60, 0xf2, 0x9f, 0xb6,
	0x79, 0x03, 0xfe, 0x10, 0x08, 0xb8, 0xbc, 0x99,
	0xa4, 0x1a, 0xe9, 0xe9, 0x56, 0x28, 0xbc, 0x64,
	0xf2, 0xf1, 0xb2, 0x0c, 0x2d, 0x7e, 0x9f, 0x51,
	0x77, 0xa3, 0xc2, 0x94, 0xd4, 0x46, 0x22, 0x99
};

/* Same as above, but one byte off. */
static const byte Q_invalid[65] = {
	0x04,
	0x60, 0xfe, 0xd4, 0xba, 0x25, 0x5a, 0x9d, 0x31,
	0xc9, 0x61, 0xeb, 0x74, 0xc6, 0x35, 0x6d, 0x68,
	0xc0, 0x49, 0xb8, 0x92, 0x3b, 0x61, 0xfa, 0x6c,
	0xe6, 0x69, 0x62, 0x2e, 0x60, 0xf2, 0x9f, 0xb6,
	0x79, 0x03, 0xfe, 0x10, 0x08, 0xb8, 0xbc, 0x99,
	0xa4, 0x1a, 0xe9, 0xe9, 0x56, 0x28, 0xbc, 0x64,
	0xf2, 0xf1, 0xb2, 0x0c, 0x2d, 0x7e, 0x9f, 0x51,
	0x77, 0xa3, 0xc2, 0x94, 0xd4, 0x46, 0x22, 0x98
};

static const byte msg[] = "sample";

#define CHECK_RV_RET(_rv_, _val_) if (_rv_) return _val_;
#define CHECK_RV(_rv_) CHECK_RV_RET(_rv_, _rv_);
#define CHECK_RV_GOTO(_rv_, _label_) if (_rv_) goto _label_;

/*! @function _fm_crypto_aes128gcm_decrypt
 @abstract Decrypts a ciphertext using AES-128-GCM.

 @param key       128-bit AES key.
 @param iv        128-bit IV.
 @param ct_nbytes Byte length of ciphertext.
 @param ct        Ciphertext.
 @param tag       128-bit authentication tag.
 @param out       Output buffer for the plaintext.

 @return 0 on success, a negative value on error.
 */
static int _fm_crypto_aes128gcm_decrypt(const byte key[16],
					const byte iv[16],
					word32 ct_nbytes,
					const byte *ct,
					const byte *tag,
					byte *out)
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;
	ocrypto_aes_gcm_ctx ctx = {0};

	LOG_DBG("_fm_crypto_aes128gcm_decrypt");
	/*
	* OpenSSL: EVP_DecryptInit_ex()
	* nrf_oberon: Not needed
	*/

	/*
	* OpenSSL: EVP_Decrypt*() + EVP_aes_128_gcm()
	*/
	LOG_HEXDUMP_DBG(tag, 16, "tag");
	LOG_HEXDUMP_DBG(ct, ct_nbytes, "ct");
	LOG_HEXDUMP_DBG(key, 16, "key");
	LOG_HEXDUMP_DBG(iv, 16, "iv");

	ocrypto_aes_gcm_init(&ctx, key, 16, iv);
	ocrypto_aes_gcm_init_iv(&ctx, iv, 16);

	ocrypto_aes_gcm_update_dec(&ctx, out, ct, ct_nbytes);
	ret = ocrypto_aes_gcm_final_dec(&ctx, tag, 16);

	LOG_HEXDUMP_DBG(out, ct_nbytes, "out");

	LOG_DBG("_fm_crypto_aes128gcm_decrypt result: 0x%X", ret);

	return ret;
}

int _fm_server_decrypt(word32 msg_nbytes,
		       const byte *msg,
		       word32 *out_nbytes,
		       byte *out)
{
	int rv;

	if (65 + 16 > msg_nbytes) {
		return -1;
	}

	if (*out_nbytes < msg_nbytes - 65 - 16) {
		return -1;
	}

	zassert_equal(msg[0], 0x04, "");

	/* Generate shared secret. */
	byte x[32];

	rv = ocrypto_ecdh_p256_common_secret(
		x,
		d,
		&msg[1]);
	zassert_equal(rv, 0, "");

	LOG_HEXDUMP_DBG(x, 32, "common_secret");

	/* Compute sharedinfo. */
	byte info[2 * 65];
	memcpy(info, msg, 65);
	memcpy(info + 65, Q, 65);

	/* Derive key and IV. */
	byte key_iv[32];

	rv = ansi_x963_kdf(
		(uint8_t*) &key_iv, 32,  /* derived key */
		x, 32,                   /* Z */
		info, sizeof(info));     /* sharedinfo */
		
	zassert_equal(rv, 0, "");

	LOG_HEXDUMP_DBG(key_iv, 16, "key");
	LOG_HEXDUMP_DBG(&key_iv[16], 16, "iv");

	/* Decrypt. */

	rv = _fm_crypto_aes128gcm_decrypt(
		key_iv,
		&key_iv[16],
		msg_nbytes - 65 - 16,
		msg + 65,
		msg + msg_nbytes - 16,
		out);
	zassert_equal(rv, 0, "");

	*out_nbytes = msg_nbytes - 65 - 16;

	return 0;
}

ZTEST(suite_fmn_crypto, test_ecies)
{
	byte ct[65 + sizeof(msg) - 1 + 16];
	word32 ct_len = sizeof(ct);

	/* Ensure that points not on the curve are rejected. */
	zassert_not_equal(fm_crypto_encrypt_to_server(Q_invalid, sizeof(msg) - 1, msg, &ct_len, ct), 0, "");

	zassert_equal(fm_crypto_encrypt_to_server(Q, sizeof(msg) - 1, msg, &ct_len, ct), 0, "");
	zassert_equal(ct_len, sizeof(ct), "");

	byte pt[sizeof(msg) - 1];
	word32 pt_len = sizeof(pt);
	zassert_equal(_fm_server_decrypt(sizeof(ct), ct, &pt_len, pt), 0, "");
	zassert_equal(pt_len, sizeof(msg) - 1, "");
	zassert_equal(memcmp(pt, msg, sizeof(msg) - 1), 0, "");
}
