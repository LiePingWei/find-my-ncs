/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "motion_platform.h"

#include <errno.h>

#define SENSOR_VALUE_SET(_var, _int, _frac) \
  _var.val1 = _int; \
  _var.val2 = _frac

#define SENSOR_VALUE_SCALE_250_RAD_PER_SEC(_var) \
  SENSOR_VALUE_SET(_var, 250, 0)

#define SENSOR_VALUE_OVERSAMPLING_DISABLE(_var) \
  SENSOR_VALUE_SET(_var, 1, 0)

#define SENSOR_VALUE_SAMPLING_FREQ_25_HZ(_var) \
  SENSOR_VALUE_SET(_var, 25, 0)

static K_SEM_DEFINE(poll_sem, 0, 1);
static const struct device *motion_sensor;
static sensor_trigger_handler_t drdy_cb;

static void poll_thread(void)
{
	k_sem_take(&poll_sem, K_FOREVER);

	while(1) {
		if (drdy_cb) {
			drdy_cb(motion_sensor, NULL);
		}

		k_msleep(MSEC_PER_SEC / MOTION_PLATFORM_SAMPLES_PER_SEC);
	}
}

K_THREAD_DEFINE(gyro_poll, CONFIG_MOTION_POLL_THREAD_STACK_SIZE, poll_thread,
		NULL, NULL, NULL, CONFIG_MOTION_POLL_THREAD_PRIORITY, 0, 0);

int motion_platform_init(const struct device *sensor)
{
	struct sensor_value scale;
	struct sensor_value sampling_freq;
	struct sensor_value oversampling;
	int err;

	if (!sensor) {
		return -EINVAL;
	}
	motion_sensor = sensor;

	SENSOR_VALUE_SCALE_250_RAD_PER_SEC(scale);
	err = sensor_attr_set(sensor, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_FULL_SCALE,
			      &scale);
	if (err) {
		return err;
	}

	SENSOR_VALUE_OVERSAMPLING_DISABLE(oversampling);
	err = sensor_attr_set(sensor, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_OVERSAMPLING,
			      &oversampling);
	if (err) {
		return err;
	}

	SENSOR_VALUE_SAMPLING_FREQ_25_HZ(sampling_freq);
	err = sensor_attr_set(sensor, SENSOR_CHAN_GYRO_XYZ,
			      SENSOR_ATTR_SAMPLING_FREQUENCY,
			      &sampling_freq);

	return err;
}

int motion_platfom_enable_drdy(sensor_trigger_handler_t cb)
{
	if (!cb) {
		return -EINVAL;
	}

	drdy_cb = cb;
	k_sem_give(&poll_sem);
	return 0;
}
