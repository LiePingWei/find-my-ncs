/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "speaker_platform.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app);

#define SPK_PWR_NODE	DT_ALIAS(spk_pwr)

static const struct gpio_dt_spec spk_pwr = GPIO_DT_SPEC_GET(SPK_PWR_NODE, enable_gpios);

int speaker_platform_init(void)
{
	int err;

	if (!device_is_ready(spk_pwr.port)) {
		LOG_ERR("SPK_PWR is not ready");
		return -EIO;
	}

	err = gpio_pin_configure_dt(&spk_pwr, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Can't configure SPK_PWR (err %d)", err);
		return err;
	}

	return 0;
}

int speaker_platfom_enable(void)
{
	return gpio_pin_set_dt(&spk_pwr, 1);
}

int speaker_platfom_disable(void)
{
	return gpio_pin_set_dt(&spk_pwr, 0);
}
