/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <tc_util.h>
#include <zephyr.h>
#include <ztest.h>

void test_collab(void);
void test_decrypt(void);
void test_ecdsa(void);
void test_ecies(void);
void test_keyroll(void);
void test_ssecret(void);

/*test case main entry*/
void test_main(void)
{
	ztest_test_suite(test_fmn_crypto,
		ztest_unit_test(test_collab),
		ztest_unit_test(test_decrypt),
		ztest_unit_test(test_ecdsa),
		ztest_unit_test(test_ecies),
		ztest_unit_test(test_keyroll),
		ztest_unit_test(test_ssecret));
	ztest_run_test_suite(test_fmn_crypto);
}
