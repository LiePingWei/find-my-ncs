/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef _SPEAKER_PLATFORM_H_
#define _SPEAKER_PLATFORM_H_

/** @brief Initialize platform dependent speaker.
 *
 *  @return Zero on success or negative error code otherwise
 */
int speaker_platform_init(void);

/** @brief Enable speaker.
 *
 *  @return Zero on success or negative error code otherwise
 */
int speaker_platfom_enable(void);

/** @brief Disable speaker.
 *
 *  @return Zero on success or negative error code otherwise
 */
int speaker_platfom_disable(void);

#endif /* _SPEAKER_PLATFORM_H_ */
