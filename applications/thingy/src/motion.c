/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app);

#include "motion.h"
#include "platform/motion_platform.h"

#define GYRO_TH		0.43625

#define GYRO_CALC_ROT(data)	((data) / (MOTION_PLATFORM_SAMPLES_PER_SEC))

#define GYRO_NODE	DT_ALIAS(gyro)
#define GYRO_PWR_NODE	DT_ALIAS(gyro_pwr)

struct gyro_data {
	double data_x;
	double data_z;
	size_t count;
};

static bool motion_detection_enabled;
static bool motion_reset_en;
static struct gyro_data *motion_data;

static void sensor_drdy(const struct device *dev,
			const struct sensor_trigger *trig)
{
	struct sensor_value val;
	static struct gyro_data gyro[2];
	static size_t cur_data;

	sensor_sample_fetch(dev);

	if (motion_reset_en) {
		cur_data = 0;
		motion_data = &gyro[cur_data];
		motion_data->data_x = 0;
		motion_data->data_z = 0;
		motion_data->count = 0;
		motion_reset_en = false;
	}

	if (motion_detection_enabled) {
		gyro[cur_data].data_x = motion_data->data_x;
		gyro[cur_data].data_z = motion_data->data_z;
		gyro[cur_data].count = motion_data->count;

		sensor_channel_get(dev, SENSOR_CHAN_GYRO_X, &val);
		gyro[cur_data].data_x += sensor_value_to_double(&val);
		sensor_channel_get(dev, SENSOR_CHAN_GYRO_Z, &val);
		gyro[cur_data].data_z += sensor_value_to_double(&val);
		gyro[cur_data].count++;

		motion_data = &gyro[cur_data];

		cur_data = ((cur_data + 1) % 2);
	}
}

void motion_reset(void)
{
	motion_reset_en = true;
}

void motion_stop(void)
{
	motion_detection_enabled = false;
	motion_reset();
}

void motion_start(void)
{
	motion_detection_enabled = true;
	motion_reset();
}

bool motion_check(void)
{
	double gyro_delta;

	__ASSERT(motion_data->count != 0, "Count number has to be greater than zero");

	gyro_delta = GYRO_CALC_ROT(motion_data->data_z);

	if ((gyro_delta > GYRO_TH) || (gyro_delta < -GYRO_TH)) {
		return true;
	}

	gyro_delta = GYRO_CALC_ROT(motion_data->data_x);

	if ((gyro_delta > GYRO_TH) || (gyro_delta < -GYRO_TH)) {
		return true;
	}

	return false;
}

int motion_init(void)
{
	const struct device *sensor;
	int err;

	sensor = DEVICE_DT_GET(GYRO_NODE);
	if (!sensor) {
		LOG_ERR("No sensor device found");
		return -EIO;
	}

	if (!device_is_ready(sensor)) {
		LOG_ERR("Device %s is not ready.", sensor->name);
		return -EIO;
	}

	err = motion_platform_init(sensor);
	if (err) {
		LOG_ERR("Initializing platform dependent motion detection failed (err %d)", err);
		return err;
	}

	motion_stop();

	err = motion_platfom_enable_drdy(sensor_drdy);
	if (err) {
		LOG_ERR("Initializing data ready mechanism failed (err %d)", err);
	}

	return err;
}

static int gyro_pwr_init(const struct device *unused)
{
	int err;
	const struct gpio_dt_spec pwr = GPIO_DT_SPEC_GET(GYRO_PWR_NODE, enable_gpios);

	if (!device_is_ready(pwr.port)) {
		LOG_ERR("GYRO_PWR is not ready");
		return -EIO;
	}

	err = gpio_pin_configure_dt(&pwr, GPIO_OUTPUT_ACTIVE);
	if (err) {
		LOG_ERR("Error while configuring GYRO_PWR (err %d)", err);
		return err;
	}

	k_msleep(50);

	return 0;
}

SYS_INIT(gyro_pwr_init, POST_KERNEL, 80);
