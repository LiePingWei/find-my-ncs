/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef _MOTION_PLATFORM_H_
#define _MOTION_PLATFORM_H_

#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>

/* Sampling rate (in samples per second) of the motion detection sensor (gyroscope). */
#define MOTION_PLATFORM_SAMPLES_PER_SEC	10

/** @brief Initialize platform dependent motion detector.
 *
 *  @param[in] sensor Motion sensor device pointer.
 *
 *  @return Zero on success or negative error code otherwise
 */
int motion_platform_init(const struct device *sensor);

/** @brief Enable data ready callback.
 *
 *  @param[in] cb Data ready callback.
 *
 *  @return Zero on success or negative error code otherwise
 */
int motion_platfom_enable_drdy(sensor_trigger_handler_t cb);

#endif /* _MOTION_PLATFORM_H_ */
