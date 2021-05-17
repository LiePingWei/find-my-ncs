/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_VERSION_H_
#define FMNA_VERSION_H_

#ifdef  __cplusplus
extern 'C' {
#endif

#include <zephyr.h>

#define FMNA_VERSION_ENCODE(fmna_version) (               \
	((uint32_t)(fmna_version.major) << 16) |         \
	(((uint32_t)(fmna_version.minor) & 0xFF) << 8) | \
	((uint32_t)(fmna_version.revision) & 0xFF))

struct fmna_version {
	uint16_t major;
	uint8_t minor;
	uint8_t revision;
	uint32_t build_num;
};

int fmna_version_fw_get(struct fmna_version *ver);

#ifdef __cplusplus
}
#endif

#endif /* FMNA_VERSION_H_ */
