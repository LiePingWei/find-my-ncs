/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "speaker.h"

#include <device.h>
#include <devicetree.h>
#include <drivers/pwm.h>
#include <drivers/gpio.h>
#include <logging/log.h>

LOG_MODULE_DECLARE(app);

#define SPK_NODE	DT_NODELABEL(pwm_spk0)
#define SPK_CTLR	DT_PWMS_CTLR(SPK_NODE)
#define SPK_CHANNEL	DT_PWMS_CHANNEL(SPK_NODE)
#define SPK_FLAGS	DT_PWMS_FLAGS(SPK_NODE)

#define SPK_PWR_NODE	DT_NODELABEL(spk_pwr)
#define SPK_PWR		DT_GPIO_LABEL(SPK_PWR_NODE, enable_gpios)
#define SPK_PWR_PIN	DT_GPIO_PIN(SPK_PWR_NODE, enable_gpios)
#define SPK_PWR_FLAGS	DT_GPIO_FLAGS(SPK_PWR_NODE, enable_gpios)

#define SPK_PER_US(freq)	(1000000 / (freq))

static const struct device *spk_pwm;
static const struct device *spk_pwr;
static uint32_t spk_per_us;


int speaker_init(void)
{
	int err;

	spk_pwm = DEVICE_DT_GET(SPK_CTLR);
	if (!device_is_ready(spk_pwm)) {
		LOG_ERR("PWM device %s is not ready", spk_pwm->name);
		return -EIO;
	}

	spk_per_us = SPK_PER_US(CONFIG_SPK_FREQ);
	err = pwm_pin_set_usec(spk_pwm, SPK_CHANNEL, spk_per_us, spk_per_us/2, SPK_FLAGS);
	if (err) {
		LOG_ERR("Can't initiate PWM (err %d)", err);
		return err;
	}

	spk_pwr = device_get_binding(SPK_PWR);
	if (!spk_pwr) {
		LOG_ERR("Can't get binfing for SPK_PWR");
		return -EIO;
	}

	err = gpio_pin_configure(spk_pwr, SPK_PWR_PIN, GPIO_OUTPUT_INACTIVE | SPK_PWR_FLAGS);
	if (err) {
		LOG_ERR("Can't configure SPK_PWR (err %d)", err);
		return err;
	}

	return 0;
}

int speaker_on(void)
{
	return gpio_pin_set(spk_pwr, SPK_PWR_PIN, 1);
}

int speaker_off(void)
{
	return gpio_pin_set(spk_pwr, SPK_PWR_PIN, 0);
}
