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
#include <zephyr/bluetooth/conn.h>

enum fmna_pair_status {
	FMNA_PAIR_STATUS_SUCCESS,
	FMNA_PAIR_STATUS_FAILURE,
};

typedef void (*fmna_pair_status_changed_t)(struct bt_conn *conn,
					   enum fmna_pair_status status);

int fmna_pair_init(uint8_t bt_id, fmna_pair_status_changed_t cb);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_PAIR_H_ */
