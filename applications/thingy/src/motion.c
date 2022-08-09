/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <drivers/sensor.h>
#include <device.h>
#include <drivers/gpio.h>
#include <devicetree.h>
#include <logging/log.h>

LOG_MODULE_DECLARE(app);

#include "motion.h"


#define GYRO_TH		0.43625
#define GYRO_SPS	10

#define GYRO_CALC_ROT(data)	((data) / (GYRO_SPS))

#define MPU_PWR_NODE	DT_ALIAS(mpu_pwr)
#define MPU_PWR		DT_GPIO_CTLR(MPU_PWR_NODE, enable_gpios)
#define MPU_PWR_PIN	DT_GPIO_PIN(MPU_PWR_NODE, enable_gpios)
#define MPU_PWR_FLAGS	DT_GPIO_FLAGS(MPU_PWR_NODE, enable_gpios)

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
	struct sensor_trigger trig;
	int err;

	sensor = DEVICE_DT_GET_ANY(invensense_mpu9250);
	if (!sensor) {
		LOG_ERR("No sensor device found");
		return -EIO;
	}

	if (!device_is_ready(sensor)) {
		LOG_ERR("Device %s is not ready.", sensor->name);
		return -EIO;
	}

	trig.type = SENSOR_TRIG_DATA_READY;
	trig.chan = SENSOR_CHAN_GYRO_XYZ;

	err = sensor_trigger_set(sensor, &trig, sensor_drdy);
	if (err) {
		LOG_ERR("Failed to set trigger (err %d)", err);
		return err;
	}

	motion_stop();

	return 0;
}

static int mpu_pwr_init(const struct device *pwr)
{
	int err;

	pwr = DEVICE_DT_GET(MPU_PWR);
	if (!pwr) {
		LOG_ERR("Can't get binding for MPU_PWR");
		return -EIO;
	}

	err = gpio_pin_configure(pwr, MPU_PWR_PIN, GPIO_OUTPUT_ACTIVE | MPU_PWR_FLAGS);
	if (err) {
		LOG_ERR("Error while configuring MPU_PWR (err %d)", err);
		return err;
	}

	k_msleep(50);

	return 0;
}

#if CONFIG_SENSOR_INIT_PRIORITY <= CONFIG_MPU_VDD_PWR_CTRL_INIT_PRIORITY
#error MPU_VDD_PWR_CTRL_INIT_PRIORITY must be lower than SENSOR_INIT_PRIORITY
#endif

SYS_INIT(mpu_pwr_init, POST_KERNEL, CONFIG_MPU_VDD_PWR_CTRL_INIT_PRIORITY);
