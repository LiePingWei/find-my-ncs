/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef APP_BATTERY_H_
#define APP_BATTERY_H_


#include <stdint.h>

/** @brief Initialize battery level sensing peripherals.
 *
 *  This function initializes the ADC and the battery monitor enable GPIO pin.
 *
 *  @return Zero on success or negative error code otherwise
 */
int battery_init(void);

/** @brief Measure the battery level.
 *
 *  This function performs measurment of the battery level.
 *  After the measurment the battery monitor enable pin is turned off.
 *
 *  @param charge State of charging expressed as percentage.
 *
 *  @return Zero on success or negative error code otherwise
 */
int battery_measure(uint8_t *charge);

#endif
