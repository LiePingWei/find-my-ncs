/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <assert.h>
#include <string.h>

#include "fm_crypto.h"
#include "crypto_helper.h"

#include <ocrypto_constant_time.h>
#include <ocrypto_hmac_sha256.h>
#include <ocrypto_sha256.h>
#include <ocrypto_ecdsa_p256.h>
#include <ocrypto_ecdh_p224.h>
#include <ocrypto_ecdh_p256.h>
#include <ocrypto_sc_p256.h>

#include <mbedtls/gcm.h>

const byte KDF_LABEL_UPDATE[] = "update";
const byte KDF_LABEL_DIVERSIFY[] = "diversify";
const byte KDF_LABEL_INTERMEDIATE[] = "intermediate";
const byte KDF_LABEL_CONNECT[] = "connect";
const byte KDF_LABEL_SERVERSS[] = "ServerSharedSecret";
const byte KDF_LABEL_PAIRINGSESS[] = "PairingSession";
const byte KDF_LABEL_SNPROTECTION[] = "SerialNumberProtection";

#define LOG_MODULE_NAME fmna_crypto_oberon
#include "crypto_log.h"

#define STR_ARRAY_SIZE(array) \
    (sizeof(array) / sizeof((array)[0])) - 1

#define CHECK_RV_RET(_rv_, _val_) if (_rv_) return _val_;
#define CHECK_RV(_rv_) CHECK_RV_RET(_rv_, _rv_);
#define CHECK_RV_GOTO(_rv_, _label_) if (_rv_) goto _label_;

#define ASN1_VALUE_MAX_LEN 0x7F
#define ASN1_TAG_INTEGER   0x02
#define ASN1_TAG_SEQUENCE  0x30

int fm_crypto_sha256(word32 msg_nbytes, const byte *msg, byte out[32])
{
	if(msg == NULL && msg_nbytes > 0) {
		return -1;
	}

	ocrypto_sha256(out, msg, msg_nbytes);
	return 0;
}

int fm_crypto_ckg_init(fm_crypto_ckg_context_t ctx)
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;

	/* Clear out the context. */
	ocrypto_constant_time_fill_zero(ctx, sizeof(*ctx));

	/**
	 * 1. The accessory generates a P-224 scalar s (see Random scalar generation) and a 32-byte random
	 * value r. It sends the value C1 = SHA-256(s || r), where len(C1) = 32 bytes, to the
	 * owner device.
	 */

	/*
	* OpenSSL: EC_GROUP_new_by_curve_name(NID_secp224r1) + EC_POINT_new()
	* nrf_oberon: We don't need to allocate
	*/

	/*
	* OpenSSL: RAND_bytes() (r1, 32 bytes)
	*/
	ret = generate_random(ctx->r1, 32);
	CHECK_RV_GOTO(ret, error);

	LOG_HEXDUMP_DBG(ctx->r1, 32, "fm_crypto_ckg_init: ctx->r1");

	/*
	* OpenSSL: EC_GROUP_new_by_curve_name(NID_secp224r1) + EC_POINT_new()
	* nrf_oberon: Does not require allocation
	*/
	/*
	* OpenSSL: EC_KEY_generate_key()
	*/
	ret = ecc_gen_keypair(&ctx->key, ECC_TYPE_P224);
	CHECK_RV_GOTO(ret, error);

	return ret;

error:
	/* Clear out context on any failure */
	fm_crypto_ckg_free(ctx);
	return ret;
}

void fm_crypto_ckg_free(fm_crypto_ckg_context_t ctx)
{
	/* Clear out the whole context structure */
	ocrypto_constant_time_fill_zero(&ctx, sizeof(*ctx));
}

int fm_crypto_ckg_gen_c1(fm_crypto_ckg_context_t ctx, byte out[32])
{
    ocrypto_sha256_ctx hash_ctx = {0};

    /**
     * 1. The accessory generates a P-224 scalar s (see Random scalar generation) and a 32-byte random
     * value r. It sends the value C1 = SHA-256(s || r), where len(C1) = 32 bytes, to the
     * owner device.
     */

    /* C1 = SHA-256(s || r) */
    ocrypto_sha256_init(&hash_ctx);

    /*
     * OpenSSL: BN_bn2bin() + EC_KEY_get0_private_key()
     * nrf_oberon: Private key already in raw form
     */
    /*
     * OpenSSL: SHA256()
     * nrf_oberon: Calculate digest incrementally (s || r)
     */
    ocrypto_sha256_update(&hash_ctx, ctx->key.private_key.buffer, 28);

    /* r equal to r1 */
    ocrypto_sha256_update(&hash_ctx, ctx->r1, 32);

    /* Write digest into out */
    ocrypto_sha256_final(&hash_ctx, out);

    return FMN_ERROR_CRYPTO_OK;
}

/*! @function _fm_crypto_points_add
 @abstract Adds two given points on an elliptic curve.

 @param r  Resulting EC point r = s + t.
 @param s  EC point s.
 @param t  EC point t.
 @param dp Curve parameters.

 @return 0 on success, a negative value on error.
 */
static int _fm_crypto_points_add(ecc_point *r,
                                 ecc_point *s,
                                 ecc_point *t,
                                 ecc_set_type dp)
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;

	/*
	* Compute s + t.
	*
	* OpenSSL: EC_POINT_add()
	*/
	switch (dp) {
	case ECC_TYPE_P224:
		ret = ocrypto_curve_p224_add(&r->point_p224, &s->point_p224,
					     &t->point_p224);
		CHECK_RV_GOTO(ret, error);

		/* Set the uncompressed tag */
		r->buffer[0] = 0x04;
		ocrypto_curve_p224_to28bytes(r->buffer + 1, &r->point_p224);
		break;

	default:
		goto error;
	}

	return FMN_ERROR_CRYPTO_OK;

error:
	/* p224 and p256 is a union. Just clear the largest */
	ocrypto_constant_time_fill_zero(r, sizeof(*r));
	return ret;
}

int fm_crypto_ckg_gen_c3(fm_crypto_ckg_context_t ctx,
			 const byte c2[89],
			 byte out[60])
{
	/**
	 * 2. The owner device generates a P-224 scalar s’ and a 32-byte random
	 * value r’. It computes S’ = s’ ⋅ G and sends C2 = {S’, r’}, where
	 * len(C2) = 89 bytes, to the accessory.
	 *
	 * C2 = {S', r'}
	 * 89 = {57, 32}
	 */

	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;
	ecc_point S_marked = {0};

	/*
	* OpenSSL: EC_KEY_new_by_curve_name(NID_secp224r1)
	*/
	/* No init needed in nrf_oberon scalar */

	/**
	 * The accessory checks S’ and aborts if it is not a valid point on the curve. (See Elliptic curve point
	 * validation.) It computes the final public key P = S’ + s ⋅ G and sends C3 = {s, r}, where
	 * len(C3) = 60 bytes, to the owner device.
	 * */

	/* Verify C2[0] == 0x04, uncompressed point */
	CHECK_RV_GOTO((c2[0] != 0x04), error);

	/*
	* OpenSSL: EC_POINT_set_affine_coordinates_GFp()
	*/
	/*
	* OpenSSL: EC_POINT_is_on_curve()
	* nrf_oberon: Validation done in ocrypto_curve_p224_from56bytes
	*/
	/* Import point and check that it is valid */
	ret = ocrypto_curve_p224_from56bytes(&S_marked.point_p224, c2 + 1);
	CHECK_RV_GOTO(ret, error);

	/*
	* Compute P = S' + s * G.
	*
	* OpenSSL: EC_POINT_add()
	*/
	ret = _fm_crypto_points_add(&ctx->p,
				&S_marked,
				&ctx->key.public_key,
				ECC_TYPE_P224);
	CHECK_RV_GOTO(ret, error);

	/*
	* C3 := s || r
	*
	* OpenSSL: BN_bn2bin() + EC_KEY_get0_private_key()
	* nrf_oberon: s is already in raw form
	*/

	/* Send C3 = {s, r}, where len(C3) = 60 bytes, to the owner device. */
	ocrypto_sc_p224_to28bytes(out, &ctx->key.private_key.scalar_p224);
	ocrypto_constant_time_copy(out + 28, ctx->r1, 32);

	/* Copy r' from C2 into ctx->r2 */
	ocrypto_constant_time_copy(ctx->r2, c2 + 57, sizeof(ctx->r2));

	return ret;

error:
	ocrypto_constant_time_fill_zero(&S_marked, sizeof(ecc_point));
	return ret;
}

int fm_crypto_ckg_finish(fm_crypto_ckg_context_t ctx,
			 byte p[57],
			 byte skn[32],
			 byte sks[32])
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;
	struct {
		uint32_t skn[8];
		uint32_t sks[8];
	} sk_pair = {0};

	uint8_t pub_buf[56] = {0};

	uint8_t shared_info_buf[64] = {0};

	/*
	* OpenSSL: BN_bn2bin() + EC_POINT_get_affine_coordinates_GFp()
	* nrf_oberon: nothing needed
	*/

	/* The public key is equal, ready to use */
	ocrypto_constant_time_copy(shared_info_buf, ctx->r1, 32);
	ocrypto_constant_time_copy(shared_info_buf + 32, ctx->r2, 32);

	/* Copy out P (x and y) */
	ocrypto_curve_p224_to56bytes(pub_buf, &ctx->p.point_p224);

	/*
	* OpenSSL: Custom X9.63 KDF implementation using SHA256()
	*/
	/** 5. Both the owner device and the accessory compute the final symmetric
	 *    keys SKN and SKS as the 64-byte output of
	 *    ANSI-X9.63-KDF(x(P), r || r’),
	 *    where SKN is the first 32 bytes and SKS is the last 32 bytes
	 */
	ret = ansi_x963_kdf(
		(uint8_t*) &sk_pair, 64,    /* SKN || SKS (derived keys) */
		pub_buf, 28,                /* x(P) (secret/Z) */
		shared_info_buf, 64);       /* r || r' (sharedinfo) */
	CHECK_RV_GOTO(ret, error);

	ocrypto_constant_time_copy(skn, sk_pair.skn, 32);
	ocrypto_constant_time_copy(sks, sk_pair.sks, 32);

	/*
	* Write uncompressed point in ANSI X9.62 format.
	*
	* OpenSSL: BN_bn2bin() + EC_POINT_get_affine_coordinates_GFp()
	* nrf_oberon: p already in raw format in pub_buf
	*/
	/* Set uncompressed point tag */
	p[0] = 0x04;
	/* Copy the public key into p (big endian) */
	ocrypto_constant_time_copy(p + 1, pub_buf, 56);

	return 0;

error:
	ocrypto_constant_time_fill_zero(p, 57);
	ocrypto_constant_time_fill_zero(&sk_pair, sizeof(sk_pair));
	return ret;
}

int fm_crypto_roll_sk(const byte sk[32], byte out[32])
{
	/*
	* OpenSSL: Custom X9.63 KDF implementation using SHA256()
	*/

	/* For a given 15-minute period i:
	 * 1.    Derive SKN i = ANSI-X9.63-KDF(SKN i-1 , “update”),
	 *       where SKN 0 is the SKN as agreed upon at pairing time.
	 */
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;
	ret = ansi_x963_kdf(
		out, 32, /* (SKN i/derived key) */
		sk, 32, /* (Secret/z) */
		KDF_LABEL_UPDATE, /* (shared info) */
		STR_ARRAY_SIZE(KDF_LABEL_UPDATE));
	CHECK_RV_GOTO(ret, error);

	return 0;

error:
	ocrypto_constant_time_fill_zero(out, 32);
	return ret;
}

int fm_crypto_derive_ltk(const byte skn[32], byte out[16])
{
	int ret;
	uint8_t ik[32];

	/* 2. Derive the Intermediate key IK
	 *    i = ANSI-X9.63-KDF(SKN i , “intermediate”), where
	 *    len(IK i ) = 32 bytes.
	 */
	ret = ansi_x963_kdf(
		ik, 32, /* (Generated intermediate derived key) */
		skn, 32, /* (Secret) */
		KDF_LABEL_INTERMEDIATE, /* (SharedInfo) */
		STR_ARRAY_SIZE(KDF_LABEL_INTERMEDIATE));
	CHECK_RV_GOTO(ret, error);

	/* 3. Derive the Link Encryption key LTK
	 *    i = ANSI-X9.63-KDF(IK i , “connect”), where
	 *    len(LTK i ) = 16 bytes.
	 */
	ret = ansi_x963_kdf(
		out, 16, /* (generated derived key) */
		ik, 32, /* (Secret) */
		KDF_LABEL_CONNECT, /* (sharedinfo) */
		STR_ARRAY_SIZE(KDF_LABEL_CONNECT));
	CHECK_RV_GOTO(ret, error);

	return 0;

error:
	ocrypto_constant_time_fill_zero(out, 32);
	return ret;
}

/*! @function _fm_crypto_scmult
 @abstract Scalar multiplication on an elliptic curve.

 @param r  Resulting EC point r = s * B.
 @param s  Scalar s.
 @param B  Base point B.
 @param dp Curve parameters.

 @return 0 on success, a negative value on error.
 */
static int _fm_crypto_scmult(ecc_point *r,
			     mp_int *s,
			     ecc_point *B,
			     ecc_set_type dp)
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;

	switch (dp) {
	case ECC_TYPE_P224:
		ret = ocrypto_curve_p224_scalarmult(
			&r->point_p224,
			&B->point_p224,
			&s->private_key.scalar_p224);
		break;

	case ECC_TYPE_P224_BASE:
		ret = ocrypto_curve_p224_scalarmult_base(
			&r->point_p224,
			&s->private_key.scalar_p224);
		break;

	case ECC_TYPE_NONE:
	default:
		ret = -1;
	}

	return ret;
}

/*! @function _fm_crypto_scmult_reduce
 @abstract Takes a 36-byte value uv, reduces it to a valid scalar s,
           and computes r = s * B.

 @param r  Resulting EC point r = (uv (mod q-1) + 1) * B.
 @param uv 36-byte pre-scalar value.
 @param B  Base point B.
 @param dp Curve parameters.

 @return 0 on success, a negative value on error.
 */
static int _fm_crypto_scmult_reduce(ecc_point *r,
				    const byte uv[36],
				    ecc_point *B,
				    ecc_set_type dp)
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;
	ecc_key ecc_key = {0};

	/* Reduce
	 * s = u (mod q-1) + 1\
	 */
	ocrypto_sc_p224_from36bytes(&ecc_key.private_key.scalar_p224, uv);

	/* Scalar multiply */
	ret = _fm_crypto_scmult(r, (mp_int *)&ecc_key, B, dp);
	CHECK_RV_GOTO(ret, error);

	return 0;

error:
	ocrypto_constant_time_fill_zero(r, sizeof(ecc_point));
	return ret;
}

/*! @function _fm_crypto_scmult_twin_reduce
 @abstract Takes two 36-byte values u and v, reduces them to valid scalars s
           and t, a and computes r = s * P + t * G.

 @param r  Resulting EC point r = (u (mod q-1) + 1) * P + (v (mod q-1) + 1) * G.
 @param u  36-byte pre-scalar value.
 @param v  36-byte pre-scalar value.
 @param P  EC point P.

 @return 0 on success, a negative value on error.
 */
static int _fm_crypto_scmult_twin_reduce(ecc_point *r,
					 const byte u[36],
					 const byte v[36],
					 ecc_point *P)
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;
	ecc_point r1 = {0};
	ecc_point r2 = {0};

	/* r1 = (u (mod q-1) + 1) * P */
	ret = _fm_crypto_scmult_reduce(&r1, u, P, ECC_TYPE_P224);
	CHECK_RV_GOTO(ret, error);

	/* r2 = (v (mod q-1) + 1) * G */
	ret = _fm_crypto_scmult_reduce(&r2, v, NULL, ECC_TYPE_P224_BASE);
	CHECK_RV_GOTO(ret, error);

	/* r = r1 + r2 */
	ret = _fm_crypto_points_add(r, &r1, &r2, ECC_TYPE_P224);
	CHECK_RV_GOTO(ret, error);

	return 0;

error:
	return ret;
}

int fm_crypto_derive_primary_or_secondary_x(const byte sk[32],
					    const byte p[57],
					    byte out[28])
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;
	ecc_point p_res = {0};
	ecc_point p_int = {0};
	struct {
		uint32_t u[9];
		uint32_t v[9];
	} at = {0};

	/**
	 * Copy from spec
	 *
	 * The accessory must derive primary and secondary keys from
	 * the public key P generated at pairing time. P itself must
	 * never be sent out and must be stored in a secure location.
	 */

	/* Check that uncompressed tag is set */
	CHECK_RV_GOTO((p[0] != 0x04), error);

	/*
	* OpenSSL: EC_KEY_new_by_curve_name(NID_secp224r1)
	* nrf_oberon: Allocation not needed
	*/
	/*
	* OpenSSL: EC_GROUP_new_by_curve_name(NID_secp224r1) + EC_POINT_new()
	*/
	/*
	* OpenSSL: EC_POINT_set_affine_coordinates_GFp()
	*/
	/*
	* OpenSSL: EC_POINT_is_on_curve()
	* nrf_oberon: Validation done in ocrypto_curve_p224_from56bytes
	*/
	/* Import public key and check that it is valid */
	ret = ocrypto_curve_p224_from56bytes(&p_int.point_p224, p + 1);
	CHECK_RV_GOTO(ret, error);

	/*
	* OpenSSL: Custom X9.63 KDF implementation using SHA256()
	*/
	/**
	 * Copy from spec
	 *
	 * 2. Derive AT i = (u i , v i) = ANSI-X9.63-KDF(SKN i , “diversify”)
	 *    where len(AT i) = 72 bytes and
	 *    len(u i) = len(v i) = 36 bytes.
	 */
	ret = ansi_x963_kdf(
		(uint8_t*) &at, sizeof(at), /* Generated derived key {u,v} */
		sk, 32, /* Secret */
		KDF_LABEL_DIVERSIFY, /* SharedInfo */
		STR_ARRAY_SIZE(KDF_LABEL_DIVERSIFY));
	CHECK_RV_GOTO(ret, error);

	/**
	 * 3. Reduce the 36-byte values u i , v i into valid P-224 scalars
	 *    by computing the following:
	 *    a. u i = u i (mod q-1) + 1
	 *    b. v i = v i (mod q-1) + 1
	 * 4. Compute P i = u i ⋅ P + v i ⋅ G.
	 */
	ret = _fm_crypto_scmult_twin_reduce(&p_res,
					(uint8_t*) at.u,
					(uint8_t*) at.v,
					&p_int);
	CHECK_RV_GOTO(ret, error);

	/*
	* OpenSSL: BN_bn2bin() + EC_POINT_get_affine_coordinates_GFp()
	* nrf_oberon: p_res already in raw format, just copy the x
	*/
	/* Copy x(P i) out */
	ocrypto_curve_p224_to28bytes(out, &p_res.point_p224);

	return 0;

error:
	ocrypto_constant_time_fill_zero(&at, sizeof(at));
	ocrypto_constant_time_fill_zero(&p_int, sizeof(p_int));
	ocrypto_constant_time_fill_zero(out, 32);
	return ret;
}

int fm_crypto_derive_server_shared_secret(const byte seeds[32],
					  const byte seedk1[32],
					  byte out[32])
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;
	uint8_t ikm[64];

	ocrypto_constant_time_copy(ikm, seeds, 32);
	ocrypto_constant_time_copy(ikm + 32, seedk1, 32);

	/* Upon successful pairing, the accessory must generate and retain
	 * ServerSharedSecret, where ServerSharedSecret is a 32-byte shared secret
	 * ServerSharedSecret = ANSI-X9.63-KDF(SeedS || SeedK1, “ServerSharedSecret”)
	 */

	/*
	* OpenSSL: Custom X9.63 KDF implementation using SHA256()
	*/
	ret = ansi_x963_kdf(
		out, 32, /* Generated ServerSharedSecret */
		ikm, 64, /* Key input (SeedS || SeedK1) */
		KDF_LABEL_SERVERSS, /* SharedInfo */
		STR_ARRAY_SIZE(KDF_LABEL_SERVERSS));
	CHECK_RV_GOTO(ret, error);

	return 0;

error:
	ocrypto_constant_time_fill_zero(out, 32);
	return ret;
}

/*! @function _fm_crypto_aes128gcm_encrypt
 @abstract Encrypts a message using AES-128-GCM.

 @param key        128-bit AES key.
 @param iv         128-bit IV.
 @param msg_nbytes Byte length of message.
 @param msg        Message.
 @param out        Output buffer for the ciphertext.
 @param tag        Output buffer for the 128-bit authentication tag.

 @return 0 on success, a negative value on error.
 */
static int _fm_crypto_aes128gcm_encrypt(const byte key[16],
					const byte iv[16],
					word32 msg_nbytes,
					const byte *msg,
					byte *out,
					byte *tag)
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;
	struct mbedtls_gcm_context ctx;
	/*
	* OpenSSL: EVP_EncryptInit_ex()
	* nrf_oberon: Not needed
	*/

	/*
	* OpenSSL: EVP_Encrypt*() + EVP_aes_128_gcm()
	*/

	LOG_HEXDUMP_DBG(msg, msg_nbytes, "ct");
	LOG_HEXDUMP_DBG(key, 16, "key");
	LOG_HEXDUMP_DBG(iv, 16, "iv");

	mbedtls_gcm_init(&ctx);

	ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128);
	CHECK_RV(ret);

	ret = mbedtls_gcm_crypt_and_tag(&ctx,
		MBEDTLS_GCM_ENCRYPT,
		msg_nbytes,
		iv, 16,
		NULL, 0,
		msg,
		out,
		16, tag);

	mbedtls_gcm_free(&ctx);

	LOG_HEXDUMP_DBG(tag, 16, "tag");
	LOG_HEXDUMP_DBG(out, msg_nbytes, "out");

	LOG_DBG("_fm_crypto_aes128gcm_encrypt result: 0x%X", ret);

	return ret;
}

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
	struct mbedtls_gcm_context ctx;

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

	mbedtls_gcm_init(&ctx);

	ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128);
	CHECK_RV(ret);

	ret = mbedtls_gcm_auth_decrypt(&ctx,
		ct_nbytes,
		iv, 16,
		NULL, 0,
		tag, 16,
		ct,
		out);

	mbedtls_gcm_free(&ctx);

	LOG_HEXDUMP_DBG(out, ct_nbytes, "out");

	LOG_DBG("_fm_crypto_aes128gcm_decrypt result: 0x%X", ret);

	return ret;
}

int fm_crypto_decrypt_e3(const byte serverss[32],
			 word32 e3_nbytes,
			 const byte *e3,
			 word32 *out_nbytes,
			 byte *out)
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;
	struct {
		uint32_t k1[4];
		uint32_t iv1[4];
	} k_iv = {{0}, {0}};

	LOG_DBG("fm_crypto_decrypt_e3");

	/* E3 has the 16 byte tag appended. */
	if (e3_nbytes <= 16) {
		return -1;
	}

	if (*out_nbytes < e3_nbytes - 16) {
		return -1;
	}

	/*
	* Derive K1 and IV1.
	*
	* OpenSSL: Custom X9.63 KDF implementation using SHA256()
	*/
	/* The 16-byte symmetric key K1 and the 16-byte initialization vector
	 * IV1 must be generated as follows:
	 * K1 || IV1 = ANSI-X9.63-KDF(ServerSharedSecret, “PairingSession”)
	 * Where K1 is the first 16 bytes and IV1 the last 16 bytes of
	 * the KDF output.
	 */
	ret = ansi_x963_kdf(
		(uint8_t*)&k_iv, 32, /* Generated key+iv */
		serverss, 32, /* Key input (serverss) */
		KDF_LABEL_PAIRINGSESS, /* SharedInfo */
		STR_ARRAY_SIZE(KDF_LABEL_PAIRINGSESS));
	CHECK_RV_GOTO(ret, error);

	LOG_HEXDUMP_DBG(k_iv.k1, 16, "k1");
	LOG_HEXDUMP_DBG(k_iv.iv1, 16, "iv1");
	LOG_HEXDUMP_DBG(e3, e3_nbytes - 16, "ct");
	LOG_HEXDUMP_DBG(e3 + e3_nbytes - 16, 16, "tag");

	/*
	* OpenSSL: EVP_Decrypt*() + EVP_aes_128_gcm()
	*/
	ret = _fm_crypto_aes128gcm_decrypt(
		(uint8_t*)k_iv.k1,
		(uint8_t*)k_iv.iv1,
		e3_nbytes - 16,
		e3,
		e3 + e3_nbytes - 16,
		out);
	CHECK_RV_GOTO(ret, error);

	LOG_HEXDUMP_DBG(out, e3_nbytes - 16, "out");

	*out_nbytes = e3_nbytes - 16;
	return 0;

error:
	ocrypto_constant_time_fill_zero(&k_iv, sizeof(k_iv));
	ocrypto_constant_time_fill_zero(out, *out_nbytes);
	*out_nbytes = 0;
	LOG_DBG("error %d (0x%X)", ret, ret);
	return ret;
}

static int asn1_uint_decode(const uint8_t* asn1,
			    size_t asn1_len,
			    uint8_t* output,
			    size_t output_len)
{
	size_t tag = asn1[1];
	size_t uint_len = asn1[1];
	const uint8_t* uint_buf = &asn1[2];

	CHECK_RV_RET(asn1_len < 3, FMN_ERROR_CRYPTO_INVALID_INPUT);
	tag = asn1[0];
	uint_len = asn1[1];
	uint_buf = &asn1[2];
	CHECK_RV_RET(tag != ASN1_TAG_INTEGER, FMN_ERROR_CRYPTO_INVALID_INPUT);
	CHECK_RV_RET(uint_len > ASN1_VALUE_MAX_LEN,
		     FMN_ERROR_CRYPTO_INVALID_INPUT);
	CHECK_RV_RET(uint_len > asn1_len - 2, FMN_ERROR_CRYPTO_INVALID_INPUT);

	while (uint_len > 0 && uint_buf[0] == 0) {
		uint_len--;
		uint_buf++;
	}

	CHECK_RV_RET(uint_len > output_len, FMN_ERROR_CRYPTO_INVALID_INPUT);

	memset(output, 0, output_len - uint_len);
	memcpy(&output[output_len - uint_len], uint_buf, uint_len);

	return 2 + asn1[1];
}

static int asn1_to_ocrypto_p256(const uint8_t* asn1,
				size_t asn1_len,
				uint8_t* rs,
				size_t rs_len)
{
	int ret;
	size_t componet_size;
	const uint8_t* asn1_uint;
	size_t asn1_uint_len;

	CHECK_RV_RET(asn1_len < 6, FMN_ERROR_CRYPTO_INVALID_INPUT);
	CHECK_RV_RET(asn1[0] != ASN1_TAG_SEQUENCE,
		     FMN_ERROR_CRYPTO_INVALID_INPUT);
	CHECK_RV_RET(asn1[1] > ASN1_VALUE_MAX_LEN,
		     FMN_ERROR_CRYPTO_INVALID_INPUT);
	CHECK_RV_RET(rs_len % 2 != 0, FMN_ERROR_CRYPTO_INVALID_INPUT);

	componet_size = rs_len / 2;

	asn1_uint = &asn1[2];
	asn1_uint_len = asn1_len - 2;

	ret = asn1_uint_decode(asn1_uint, asn1_uint_len, &rs[0], componet_size);
	CHECK_RV_RET(ret < 0, ret);

	asn1_uint = &asn1[2 + ret];
	asn1_uint_len = asn1_len - 2 - ret;

	ret = asn1_uint_decode(asn1_uint,
			       asn1_uint_len,
			       &rs[componet_size],
			       componet_size);
	CHECK_RV_RET(ret < 0, ret);

	return 0;
}

int fm_crypto_verify_s2(const byte pub[65],
			word32 sig_nbytes,
			const byte *sig,
			word32 msg_nbytes,
			const byte *msg)
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;

	ecc_point pub_key = {0};
	uint8_t sig_raw[64] = {0};

	LOG_DBG("fm_crypto_verify_s2");
	LOG_HEXDUMP_DBG(pub, 65, "pub (BE)");
	LOG_HEXDUMP_DBG(sig, sig_nbytes, "sig (asn1)");
	LOG_HEXDUMP_DBG(msg, msg_nbytes, "msg");

	/*
	* OpenSSL: EC_KEY_new_by_curve_name(NID_X9_62_prime256v1)
	* nrf_oberon: No allocation needed
	*/
	/*
	* OpenSSL: EC_POINT_set_affine_coordinates_GFp()
	* nrf_oberon: Not needed
	*/

	/* Check that Uncompressed point is set */
	CHECK_RV_GOTO((pub[0] != 0x04), final);

	/*
	* OpenSSL: EC_POINT_is_on_curve()
	* nrf_oberon: Validity checked in ocrypto_curve_p256_from64bytes
	*/
	/* Import public key to check that it is valid */
	ret = ocrypto_curve_p256_from64bytes(&pub_key.point_p256, pub + 1);
	CHECK_RV_GOTO(ret != 0, final);

	LOG_HEXDUMP_DBG(pub_key.point_p256.x.w, 32,
			"pub_key.point_p256.x (LE)");
	LOG_HEXDUMP_DBG(pub_key.point_p256.y.w, 32,
			"pub_key.point_p256.y (LE)");

	/*
	* OpenSSL: SHA256()
	*/
	/*
	* OpenSSL: ECDSA_verify()
	* nrf_oberon: SHA + ECDSA integrated
	*/

	ret = asn1_to_ocrypto_p256(sig, sig_nbytes, sig_raw, 64);
	CHECK_RV_GOTO(ret != 0, final);

	LOG_HEXDUMP_DBG(sig_raw, 64, "sig_raw (BE)");

	/* Verify the message signature */
	ret = ocrypto_ecdsa_p256_verify(sig_raw,
					msg,
					msg_nbytes,
					pub + 1);
	CHECK_RV_GOTO(ret, final);

	return 0;

final:
	return ret;
}

int fm_crypto_authenticate_with_ksn(const byte serverss[32],
				    word32 msg_nbytes,
				    const byte *msg,
				    byte out[32])
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;
	uint8_t ksn[32] = {0};

	/*
	* Derive KSN.
	*
	* OpenSSL: Custom X9.63 KDF implementation using SHA256()
	*/

	/* To generate the NFC tap payload, KSN must be generated as follows,
	 * where KSN is a 32-byte symmetric key:
	 * KSN = ANSI-X9.63-KDF(ServerSharedSecret, “SerialNumberProtection”)
	 */
	ret = ansi_x963_kdf(
		ksn, 32, /* Generated KSN */
		serverss, 32, /* Key input (serverss) */
		KDF_LABEL_SNPROTECTION, /* SharedInfo */
		STR_ARRAY_SIZE(KDF_LABEL_SNPROTECTION));
	CHECK_RV_GOTO(ret, error);

	/*
	* OpenSSL: HMAC(EVP_sha256())
	*/
	/* Calculate HMAC from message and write into out */
	ocrypto_hmac_sha256(out, ksn, 32, msg, msg_nbytes);

	return 0;

error:
	ocrypto_constant_time_fill_zero(out, 32);
	return ret;
}

int fm_crypto_generate_seedk1(byte out[32])
{
	int ret;

	/*
	* OpenSSL: RAND_bytes()
	*/
	ret = generate_random(out, 32);
	return ret;
}

int fm_crypto_encrypt_to_server(const byte pub[65],
				word32 msg_nbytes,
				const byte *msg,
				word32 *out_nbytes,
				byte *out)
{
	int ret = FMN_ERROR_CRYPTO_NO_VALUE_SET;
	ecc_point pub_key = {0};
	ecc_key Q = {0};
	uint8_t common_secret[32] = {0};

	struct {
		uint32_t k[4];
		uint32_t iv[4];
	} k_iv = {0};

	uint8_t QP[2 * 65] = {0};

	LOG_DBG("fm_crypto_encrypt_to_server");

	/*
	* OpenSSL: EC_KEY_new_by_curve_name(NID_X9_62_prime256v1)
	* nrf_oberon: Allocation not needed
	*/

	/* Check that uncompressed point tag is set */
	CHECK_RV_GOTO((pub[0] != 0x04), error);

	LOG_HEXDUMP_DBG(pub, 65, "pub");

	/*
	* OpenSSL: EC_KEY_new_by_curve_name(NID_X9_62_prime256v1)
	*/
	/*
	* Import and check Q_E.
	*
	* OpenSSL: EC_POINT_set_affine_coordinates_GFp()
	*/
	/*
	* OpenSSL: EC_POINT_is_on_curve()
	* nrf_oberon: Validated in ocrypto_curve_p256_from64bytes
	*/
	/* Import public key and check that it is valid */
	ret = ocrypto_curve_p256_from64bytes(&pub_key.point_p256, pub + 1);
	CHECK_RV_GOTO(ret, error);

	/*
	* Generate ephemeral key.
	*
	* OpenSSL: EC_KEY_generate_key()
	*/
	/* 1. Generate an ephemeral P-256 key. */
	ret = ecc_gen_keypair(&Q, ECC_TYPE_P256);
	CHECK_RV_GOTO(ret, error);

	LOG_HEXDUMP_DBG(Q.private_key.scalar_p256.w, 32, "ephemeral prv (LE)");
	LOG_HEXDUMP_DBG(Q.public_key.point_p256.x.w, 32, "ephemeral pub.x (LE)");
	LOG_HEXDUMP_DBG(Q.public_key.point_p256.y.w, 32, "ephemeral pub.y (LE)");

	/*
	* Generate shared secret.
	*
	* OpenSSL: ECDH_compute_key()
	*/
	ret = ocrypto_ecdh_p256_common_secret(
		common_secret,
		Q.private_key.buffer,
		pub + 1);
	CHECK_RV_GOTO(ret, error);

	LOG_HEXDUMP_DBG(common_secret, 32, "common_secret");

	/*
	* OpenSSL: BN_bn2bin() + EC_POINT_get_affine_coordinates_GFp()
	* nrf_oberon: Not needed. Already in raw format
	*/

	/* Creating sharedinfo: Q || P */

	/* Set uncompressed tag for Q in QP */
	QP[0] = 0x04;
	/* Copy Q into QP */
	ocrypto_curve_p256_to64bytes(QP + 1, &Q.public_key.point_p256);

	/* Copy Point Q into out */
	ocrypto_constant_time_copy(out, QP, 65);

	/* Copy Point P (with uncompressed tag) to QP */
	ocrypto_constant_time_copy(QP + 65, pub, 65);

	/*
	* Derive key and IV.
	*
	* OpenSSL: Custom X9.63 KDF implementation using SHA256()
	*/

	/* 4. Derive 32 bytes of keying material as
	 * V = ANSI-X9.63-KDF(x(Z), Q || P).
	 */
	ret = ansi_x963_kdf(
		(uint8_t*) &k_iv, 32, /* Generated Key IV */
		common_secret, 32, /* Key input: common_secret */
		QP, sizeof(QP)); /* SharedInfo: QP */
	CHECK_RV_GOTO(ret, error);

	LOG_HEXDUMP_DBG((uint8_t*) &k_iv.k, 16, "key");
	LOG_HEXDUMP_DBG((uint8_t*) &k_iv.iv, 16, "iv");

	/*
	* Encrypt.
	*
	* OpenSSL: EVP_Encrypt*() + EVP_aes_128_gcm()
	*/
	/* 5. Set K = V[0..15], that is, the first 16 bytes of the KIV.
	 * 6. Set IV = V[16..31], that is, the last 16 bytes of the KIV.
	 * 7. Encrypt message M as (C,T) = AES-128-GCM(K, IV, M) without any additional authenticated
	 *   data. K is the 128-bit AES key, IV is the initialization vector, C is the ciphertext, and T is the 16-
	 *   byte authentication tag.
	 */
	ret = _fm_crypto_aes128gcm_encrypt(
		(uint8_t*) k_iv.k,
		(uint8_t*) k_iv.iv,
		msg_nbytes,
		msg,
		out + 65,
		out + 65 + msg_nbytes);
	CHECK_RV_GOTO(ret, error);

	/* Set the outut byte size */
	*out_nbytes = 65 + msg_nbytes + 16;

	return 0;

error:
	ocrypto_constant_time_fill_zero(&k_iv, sizeof(k_iv));
	ocrypto_constant_time_fill_zero(out, 65 + msg_nbytes + 16);
	*out_nbytes = 0;
	return ret;
}
