/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef APP_MOTION_H_
#define APP_MOTION_H_

/** @brief Reset motion detection.
 *
 *  This function schedules a reset of the motion detection state.
 */
void motion_reset(void);

/** @brief Stop motion detection.
 *
 *  This function stops the motion detection and resets the motion detection state.
 */
void motion_stop(void);

/** @brief Start motion detection.
 *
 *  This function starts the motion detection and resets the motion detection state.
 */
void motion_start(void);

/** @brief Check if motion was detected.
 *
 *  This checks if motion was detected during detection period.
 *
 *  @return True if motion was detected, false otherwise.
 */
bool motion_check(void);

/** @brief Initialize motion detection.
 *
 *  This initializes the motion detection peripherals.
 *
 *  @return Zero on success or negative error code otherwise
 */
int motion_init(void);

#endif
