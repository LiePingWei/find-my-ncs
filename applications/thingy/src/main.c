/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <hal/nrf_power.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL);

#include <zephyr/settings/settings.h>
#include <dk_buttons_and_leds.h>

#include <fmna.h>

#include "speaker.h"
#include "battery.h"
#include "motion.h"


#define FMNA_BT_ID			1
#define FMNA_PEER_SOUND_DURATION	K_SECONDS(5)
#define FMNA_UT_SOUND_DURATION		K_MSEC(250)

#define FMNA_INIT_LED			DK_LED1
#define FMNA_BATTERY_REQUEST_LED	DK_LED2
#define FMNA_MOTION_DETECTION_LED	DK_LED3

#define FMNA_FACTORY_SETTINGS_RESET_BUTTON		DK_BTN1_MSK
#define FMNA_ADV_RESUME_SN_LOOKUP_BUTTON		DK_BTN1_MSK
#define FMNA_SN_LOOKUP_BUTTON_MIN_HOLD_TIME_MS		2000
#define FMNA_FACTORY_RESET_BUTTON_MIN_HOLD_TIME_MS	5000

enum signal_request {
	SIGNAL_SN_LOOKUP,
	SIGNAL_PAIR_RESUME,
	SIGNAL_FACTORY_RESET
};

static void sound_timeout_work_handle(struct k_work *item);
static K_WORK_DELAYABLE_DEFINE(sound_timeout_work, sound_timeout_work_handle);

static bool pairing_mode_exit;


static void sound_stop_indicate(void)
{
	LOG_INF("Stopping the sound from being played");

	speaker_off();
}

static void sound_timeout_work_handle(struct k_work *item)
{
	int err;

	err = fmna_sound_completed_indicate();
	if (err) {
		LOG_ERR("fmna_sound_completed_indicate failed (err %d)", err);
		return;
	}

	LOG_INF("Sound playing timed out");

	sound_stop_indicate();
}

static void sound_start(enum fmna_sound_trigger sound_trigger)
{
	k_timeout_t sound_timeout;

	if (sound_trigger == FMNA_SOUND_TRIGGER_UT_DETECTION) {
		LOG_INF("Play sound action triggered by the Unwanted Tracking "
		       "Detection");

		sound_timeout = FMNA_UT_SOUND_DURATION;
	} else {
		LOG_INF("Received a request from FMN to start playing sound "
		       "from the connected peer");

		sound_timeout = FMNA_PEER_SOUND_DURATION;
	}

	k_work_reschedule(&sound_timeout_work, sound_timeout);

	speaker_on();

	LOG_INF("Starting to play sound...");
}

static void sound_stop(void)
{
	LOG_INF("Received a request from FMN to stop playing sound");

	k_work_cancel_delayable(&sound_timeout_work);

	sound_stop_indicate();
}

static const struct fmna_sound_cb sound_callbacks = {
	.sound_start = sound_start,
	.sound_stop = sound_stop,
};

static void motion_detection_start(void)
{
	LOG_INF("Starting motion detection...");
	dk_set_led(FMNA_MOTION_DETECTION_LED, 1);

	motion_start();
}

static bool motion_detection_period_expired(void)
{
	bool is_detected;

	is_detected = motion_check();

	if (is_detected) {
		LOG_INF("Motion detected in the last period");
	} else {
		LOG_INF("No motion detected in the last period");
	}

	motion_reset();

	return is_detected;
}

static void motion_detection_stop(void)
{
	LOG_INF("Stopping motion detection...");
	dk_set_led(FMNA_MOTION_DETECTION_LED, 0);

	motion_stop();
}

static const struct fmna_motion_detection_cb motion_detection_callbacks = {
	.motion_detection_start = motion_detection_start,
	.motion_detection_period_expired = motion_detection_period_expired,
	.motion_detection_stop = motion_detection_stop,
};

static void battery_level_request(void)
{
	uint8_t charge;
	int err;

	dk_set_led(FMNA_BATTERY_REQUEST_LED, 1);

	err = battery_measure(&charge);
	if (!err) {
		fmna_battery_level_set(charge);
	} else {
		LOG_ERR("Battery measurment failed (err %d)", err);
	}

	dk_set_led(FMNA_BATTERY_REQUEST_LED, 0);
}

static void pairing_mode_exited(void)
{
	LOG_INF("Exited the FMN pairing mode");

	pairing_mode_exit = true;
}

static const struct fmna_enable_cb enable_callbacks = {
	.battery_level_request = battery_level_request,
	.pairing_mode_exited = pairing_mode_exited,
};

static int fmna_id_create(uint8_t id)
{
	int ret;
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	size_t count = ARRAY_SIZE(addrs);

	bt_id_get(addrs, &count);
	if (id < count) {
		return 0;
	}

	do {
		ret = bt_id_create(NULL, NULL);
		if (ret) {
			return ret;
		}
	} while (ret != id);

	return 0;
}

static bool factory_settings_restore_check(void)
{
	uint8_t reg;

	/* Check GPREGRET for SYS_REBOOT_COLD */
	reg = nrf_power_gpregret_get(NRF_POWER);

	if (reg == SYS_REBOOT_COLD) {
		/* Restore GPREGRET to SYS_REBOOT_WARM = 0 */
		nrf_power_gpregret_set(NRF_POWER, SYS_REBOOT_WARM);

		return true;
	}

	return false;
}

static int fmna_initialize(void)
{
	int err;
	uint8_t battery_level;
	struct fmna_enable_param enable_param = {0};

	err = fmna_sound_cb_register(&sound_callbacks);
	if (err) {
		LOG_ERR("fmna_sound_cb_register failed (err %d)", err);
		return err;
	}

	err = fmna_motion_detection_cb_register(&motion_detection_callbacks);
	if (err) {
		LOG_ERR("fmna_motion_detection_cb_register failed (err %d)", err);
		return err;
	}

	err = fmna_id_create(FMNA_BT_ID);
	if (err) {
		LOG_ERR("fmna_id_create failed (err %d)", err);
		return err;
	}

	err = battery_measure(&battery_level);
	if (err) {
		LOG_ERR("Failed to measure battery voltage (err %d)", err);
		return err;
	}

	enable_param.bt_id = FMNA_BT_ID;
	enable_param.init_battery_level = battery_level;
	enable_param.use_default_factory_settings = factory_settings_restore_check();

	err = fmna_enable(&enable_param, &enable_callbacks);
	if (err) {
		LOG_ERR("fmna_enable failed (err %d)", err);
		return err;
	}

	LOG_INF("FMNA initialized");

	return 0;
}

static int ble_stack_initialize(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	err = settings_load();
	if (err) {
		LOG_ERR("Settings loading failed (err %d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	return 0;
}

static int user_request_signal(enum signal_request request)
{
	int err;
	int32_t ms;

	err = speaker_on();
	if (err) {
		return err;
	}

	switch (request) {
	case SIGNAL_PAIR_RESUME:
		ms = CONFIG_PAIR_RESUME_SOUND_DUARTION;
		LOG_INF("Signal request for pairing resume");
		break;
	case SIGNAL_SN_LOOKUP:
		ms = CONFIG_SN_LOOKUP_SOUND_DURATION;
		LOG_INF("Signal request for serial number lookup");
		break;
	case SIGNAL_FACTORY_RESET:
		ms = CONFIG_FACTORY_RESET_SOUND_DURATION;
		LOG_INF("Signal request for factory reset");
		break;
	default:
		LOG_ERR("Unexpected signal request: %u", request);
		ms = 0;
		break;
	}

	err = k_msleep(ms);
	if (err) {
		return err;
	}

	return speaker_off();
}

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	int err;
	static int64_t prev_uptime;
	int64_t hold_time;
	uint32_t buttons = button_state & has_changed;

	if (buttons & FMNA_ADV_RESUME_SN_LOOKUP_BUTTON) {
		prev_uptime = k_uptime_get();
	} else {
		hold_time = (k_uptime_get() - prev_uptime);

		if ((hold_time > FMNA_SN_LOOKUP_BUTTON_MIN_HOLD_TIME_MS) &&
		    (hold_time < FMNA_FACTORY_RESET_BUTTON_MIN_HOLD_TIME_MS)) {
			err = fmna_serial_number_lookup_enable();
			if (err) {
				LOG_ERR("Cannot enable FMN Serial Number lookup (err: %d)", err);
			} else {
				LOG_INF("FMN Serial Number lookup enabled");
				user_request_signal(SIGNAL_SN_LOOKUP);
			}
		} else if (hold_time < FMNA_FACTORY_RESET_BUTTON_MIN_HOLD_TIME_MS) {
			err = fmna_resume();
			if (err) {
				LOG_ERR("Cannot resume the FMN activity (err: %d)", err);
			} else {
				LOG_INF("FMN pairing mode resumed");
				user_request_signal(SIGNAL_PAIR_RESUME);
			}
		} else {
			LOG_INF("Resetting to factory setting");
			user_request_signal(SIGNAL_FACTORY_RESET);
			/* Sets GPREGRET to SYS_REBOOT_COLD = 1 */
			sys_reboot(SYS_REBOOT_COLD);
		}
	}

}

static int dk_library_initialize(void)
{
	int err;

	err = dk_leds_init();
	if (err) {
		LOG_ERR("LEDs init failed (err %d)", err);
		return err;
	}

	err = dk_buttons_init(button_changed);
	if (err) {
		LOG_ERR("Buttons init failed (err: %d)", err);
		return err;
	}

	LOG_INF("DK library initialized");

	return 0;
}

void main(void)
{
	int err;

	err = dk_library_initialize();
	if (err) {
		LOG_ERR("DK library init failed (err %d)", err);
		return;
	}

	dk_set_led(FMNA_INIT_LED, 1);

	err = motion_init();
	if (err) {
		LOG_ERR("Motion detection init failed (err %d)", err);
		return;
	}

	err = battery_init();
	if (err) {
		LOG_ERR("Battery level measurment init failed (err %d)", err);
		return;
	}

	err = speaker_init();
	if (err) {
		LOG_ERR("Speaker init failed (err %d)", err);
		return;
	}

	err = ble_stack_initialize();
	if (err) {
		LOG_ERR("BLE stack init failed (err %d)", err);
		return;
	}

	err = fmna_initialize();
	if (err) {
		LOG_ERR("FMNA init failed (err %d)", err);
		return;
	}

	dk_set_led(FMNA_INIT_LED, 0);
}
