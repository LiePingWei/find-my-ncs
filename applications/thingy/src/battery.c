/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "battery.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app);

#define VBATT			DT_PATH(vbatt)
#define BATTERY_ADC_GAIN	ADC_GAIN_1


static const struct gpio_dt_spec bat_mon_en = GPIO_DT_SPEC_GET(VBATT, power_gpios);
static const struct device *adc;
static struct adc_sequence adc_seq;
static struct adc_channel_cfg adc_cfg;
static int16_t adc_raw_data;


int battery_init(void)
{
	int err;

	adc = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR(VBATT));
	if (!device_is_ready(adc)) {
		LOG_ERR("ADC device %s is not ready", adc->name);
		return -ENOENT;
	}

	if (!device_is_ready(bat_mon_en.port)) {
		LOG_ERR("BAT_MON_EN enable is not ready");
		return -EIO;
	}

	err = gpio_pin_configure_dt(&bat_mon_en, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Can't configure BAT_MON_EN pin (err %d)", err);
		return err;
	}

	adc_seq = (struct adc_sequence){
		.channels = BIT(0),
		.buffer = &adc_raw_data,
		.buffer_size = sizeof(adc_raw_data),
		.oversampling = 4,
		.calibrate = true,
		.resolution = 14
	};

	adc_cfg = (struct adc_channel_cfg){
		.gain = BATTERY_ADC_GAIN,
		.reference = ADC_REF_INTERNAL,
		.acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
		.input_positive = SAADC_CH_PSELP_PSELP_AnalogInput0 + DT_IO_CHANNELS_INPUT(VBATT)
	};

	return adc_channel_setup(adc, &adc_cfg);
}

static int battery_meas_prep(void)
{
	int err;

	err = gpio_pin_set_dt(&bat_mon_en, 1);
	if (err) {
		LOG_ERR("Can't turn on BAT_MON_EN pin (err %d)", err);
		return err;
	}

	/* Wait for voltage to stabilize */
	k_msleep(1);

	return err;
}

static uint8_t volatge_to_lipo_soc(int32_t val)
{
	/*
	 * soc[%] = ((val - v_min)/(v_max - v_min)) * 100%
	 * soc[%] = ((val - 2500mV)/(4200mV - 2500mV)) * 100%
	 * soc[%] = (val - 2500mV)/(1700mV / 100) %
	 * soc[%] = (val - 2500mV)/17 %
	 */
	__ASSERT(val >= 2500, "Invalid value of battery voltage, got %i mV", val);

	val -= 2500;
	val /= 17;

	return (uint8_t) val;
}

int battery_measure(uint8_t *charge)
{
	int err;
	int32_t val_mv;

	err = battery_meas_prep();
	if (err) {
		return err;
	}

	err = adc_read(adc, &adc_seq);
	if (err) {
		LOG_ERR("Can't read ADC (err %d)", err);
		return err;
	}

	val_mv = adc_raw_data;
	adc_raw_to_millivolts(adc_ref_internal(adc),
			      adc_cfg.gain,
			      adc_seq.resolution,
			      &val_mv);

	val_mv *= DT_PROP(VBATT, full_ohms);
	val_mv /= DT_PROP(VBATT, output_ohms);

	*charge = volatge_to_lipo_soc(val_mv);

	return gpio_pin_set_dt(&bat_mon_en, 0);
}
