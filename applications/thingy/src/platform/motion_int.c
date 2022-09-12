/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "motion_platform.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app);

static K_SEM_DEFINE(poll_sem, 0, 1);
static const struct device *motion_sensor;

int motion_platform_init(const struct device *sensor)
{
	if (!sensor) {
		return -EINVAL;
	}
	motion_sensor = sensor;

	return 0;
}

int motion_platfom_enable_drdy(sensor_trigger_handler_t cb)
{
	struct sensor_trigger trig;
	int err;

	trig.type = SENSOR_TRIG_DATA_READY;
	trig.chan = SENSOR_CHAN_GYRO_XYZ;

	if (!motion_sensor) {
		return -ESRCH;
	}

	err = sensor_trigger_set(motion_sensor, &trig, cb);
	if (err) {
		LOG_ERR("Failed to set trigger (err %d)", err);
	}

	return err;
}
