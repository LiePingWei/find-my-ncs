/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/ztest.h>

#include "fm_crypto.h"

/*
 * <https://tools.ietf.org/html/rfc6979#appendix-A.2.5>
 *
 * ECDSA signature with SHA-256, message = "sample".
 */
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

static const byte sig[] = {
	0x30, 0x44, 0x02, 0x20,
	0xef, 0xd4, 0x8b, 0x2a, 0xac, 0xb6, 0xa8, 0xfd,
	0x11, 0x40, 0xdd, 0x9c, 0xd4, 0x5e, 0x81, 0xd6,
	0x9d, 0x2c, 0x87, 0x7b, 0x56, 0xaa, 0xf9, 0x91,
	0xc3, 0x4d, 0x0e, 0xa8, 0x4e, 0xaf, 0x37, 0x16,
	0x02, 0x20,
	0xf7, 0xcb, 0x1c, 0x94, 0x2d, 0x65, 0x7c, 0x41,
	0xd4, 0x36, 0xc7, 0xa1, 0xb6, 0xe2, 0x9f, 0x65,
	0xf3, 0xe9, 0x00, 0xdb, 0xb9, 0xaf, 0xf4, 0x06,
	0x4d, 0xc4, 0xab, 0x2f, 0x84, 0x3a, 0xcd, 0xa8
};

/* Same as above, one byte off. */
static const byte sig_invalid[] = {
	0x30, 0x44, 0x02, 0x20,
	0xef, 0xd4, 0x8b, 0x2a, 0xac, 0xb6, 0xa8, 0xfd,
	0x11, 0x40, 0xdd, 0x9c, 0xd4, 0x5e, 0x81, 0xd6,
	0x9d, 0x2c, 0x87, 0x7b, 0x56, 0xaa, 0xf9, 0x91,
	0xc3, 0x4d, 0x0e, 0xa8, 0x4e, 0xaf, 0x37, 0x16,
	0x02, 0x20,
	0xf7, 0xcb, 0x1c, 0x94, 0x2d, 0x65, 0x7c, 0x41,
	0xd4, 0x36, 0xc7, 0xa1, 0xb6, 0xe2, 0x9f, 0x65,
	0xf3, 0xe9, 0x00, 0xdb, 0xb9, 0xaf, 0xf4, 0x06,
	0x4d, 0xc4, 0xab, 0x2f, 0x84, 0x3a, 0xcd, 0xa7
};

/* Same as above, one byte short. */
static const byte sig_short[] = {
	0x30, 0x44, 0x02, 0x20,
	0xef, 0xd4, 0x8b, 0x2a, 0xac, 0xb6, 0xa8, 0xfd,
	0x11, 0x40, 0xdd, 0x9c, 0xd4, 0x5e, 0x81, 0xd6,
	0x9d, 0x2c, 0x87, 0x7b, 0x56, 0xaa, 0xf9, 0x91,
	0xc3, 0x4d, 0x0e, 0xa8, 0x4e, 0xaf, 0x37, 0x16,
	0x02, 0x20,
	0xf7, 0xcb, 0x1c, 0x94, 0x2d, 0x65, 0x7c, 0x41,
	0xd4, 0x36, 0xc7, 0xa1, 0xb6, 0xe2, 0x9f, 0x65,
	0xf3, 0xe9, 0x00, 0xdb, 0xb9, 0xaf, 0xf4, 0x06,
	0x4d, 0xc4, 0xab, 0x2f, 0x84, 0x3a, 0xcd
};

static const byte msg[] = "sample";

ZTEST(suite_fmn_crypto, test_ecdsa)
{
	zassert_equal(fm_crypto_verify_s2(Q, sizeof(sig), sig, sizeof(msg) - 1, msg), 0, "");

	/* Ensure that points not on the curve are rejected. */
	zassert_not_equal(fm_crypto_verify_s2(Q_invalid, sizeof(sig), sig, sizeof(msg) - 1, msg), 0, "");

	/* Negative test vectors. */
	zassert_not_equal(fm_crypto_verify_s2(Q, sizeof(sig_invalid), sig_invalid, sizeof(msg) - 1, msg), 0, "");
	zassert_not_equal(fm_crypto_verify_s2(Q, sizeof(sig_short), sig_short, sizeof(msg) - 1, msg), 0, "");
	zassert_not_equal(fm_crypto_verify_s2(Q, sizeof(sig), sig, sizeof(msg) - 2, msg), 0, "");
}
