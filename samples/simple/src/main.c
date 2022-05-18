/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr.h>
#include <sys/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>

#include <fmna.h>

#include <settings/settings.h>

#include <dk_buttons_and_leds.h>

#define FMNA_BT_ID 1

#define FMNA_PEER_SOUND_DURATION K_SECONDS(5)
#define FMNA_UT_SOUND_DURATION   K_SECONDS(1)

#define FMNA_SOUND_LED             DK_LED1
#define FMNA_MOTION_INDICATION_LED DK_LED2
#define FMNA_PAIRED_STATE_LED      DK_LED3

#define FMNA_ADV_RESUME_BUTTON             DK_BTN1_MSK
#define FMNA_SN_LOOKUP_BUTTON              DK_BTN2_MSK
#define FMNA_MOTION_INDICATION_BUTTON      DK_BTN3_MSK
#define FMNA_FACTORY_SETTINGS_RESET_BUTTON DK_BTN4_MSK
#define FMNA_BATTERY_LEVEL_CHANGE_BUTTON   DK_BTN4_MSK

#define BATTERY_LEVEL_MAX         100
#define BATTERY_LEVEL_MIN         0
#define BATTERY_LEVEL_CHANGE_RATE 7

static bool pairing_mode_exit;
static bool motion_detection_enabled;
static bool motion_detected;
static uint8_t battery_level = BATTERY_LEVEL_MAX;

static void sound_timeout_work_handle(struct k_work *item);

static K_WORK_DELAYABLE_DEFINE(sound_timeout_work, sound_timeout_work_handle);

static void sound_stop_indicate(void)
{
	printk("Stopping the sound from being played\n");

	dk_set_led(FMNA_SOUND_LED, 0);
}

static void sound_timeout_work_handle(struct k_work *item)
{
	int err;

	err = fmna_sound_completed_indicate();
	if (err) {
		printk("fmna_sound_completed_indicate failed (err %d)\n", err);
		return;
	}

	printk("Sound playing timed out\n");

	sound_stop_indicate();
}

static void sound_start(enum fmna_sound_trigger sound_trigger)
{
	k_timeout_t sound_timeout;

	if (sound_trigger == FMNA_SOUND_TRIGGER_UT_DETECTION) {
		printk("Play sound action triggered by the Unwanted Tracking "
		       "Detection\n");

		sound_timeout = FMNA_UT_SOUND_DURATION;
	} else {
		printk("Received a request from FMN to start playing sound "
		       "from the connected peer\n");

		sound_timeout = FMNA_PEER_SOUND_DURATION;
	}
	k_work_reschedule(&sound_timeout_work, sound_timeout);

	dk_set_led(FMNA_SOUND_LED, 1);

	printk("Starting to play sound...\n");
}

static void sound_stop(void)
{
	printk("Received a request from FMN to stop playing sound\n");

	k_work_cancel_delayable(&sound_timeout_work);

	sound_stop_indicate();
}

static const struct fmna_sound_cb sound_callbacks = {
	.sound_start = sound_start,
	.sound_stop = sound_stop,
};

static void motion_detection_start(void)
{
	printk("Starting motion detection...\n");

	motion_detection_enabled = true;
}

static bool motion_detection_period_expired(void)
{
	bool is_detected;

	is_detected = motion_detected;
	motion_detected = false;

	dk_set_led(FMNA_MOTION_INDICATION_LED, 0);

	if (is_detected) {
		printk("Motion detected in the last period\n");
	} else {
		printk("No motion detected in the last period\n");
	}

	return is_detected;
}

static void motion_detection_stop(void)
{
	printk("Stopping motion detection...\n");

	motion_detection_enabled = false;
	motion_detected = false;

	dk_set_led(FMNA_MOTION_INDICATION_LED, 0);
}

static const struct fmna_motion_detection_cb motion_detection_callbacks = {
	.motion_detection_start = motion_detection_start,
	.motion_detection_period_expired = motion_detection_period_expired,
	.motion_detection_stop = motion_detection_stop,
};

static void battery_level_request(void)
{
	/* No need to implement because the simulated battery level
	 * is always up to date.
	 */

	printk("Battery level request\n");
}

static void pairing_mode_exited(void)
{
	printk("Exited the FMN pairing mode\n");

	pairing_mode_exit = true;
}

static void paired_state_changed(bool paired)
{
	printk("The FMN accessory transitioned to the %spaired state\n",
	       paired ? "" : "un");

	dk_set_led(FMNA_PAIRED_STATE_LED, paired);
}

static const struct fmna_enable_cb enable_callbacks = {
	.battery_level_request = battery_level_request,
	.pairing_mode_exited = pairing_mode_exited,
	.paired_state_changed = paired_state_changed,
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
		if (ret < 0) {
			return ret;
		}
	} while (ret != id);

	return 0;
}

static bool factory_settings_restore_check(void)
{
	uint32_t button_state;

	dk_read_buttons(&button_state, NULL);

	return (button_state & FMNA_FACTORY_SETTINGS_RESET_BUTTON);
}

static int fmna_initialize(void)
{
	int err;
	struct fmna_enable_param enable_param = {0};

	err = fmna_sound_cb_register(&sound_callbacks);
	if (err) {
		printk("fmna_sound_cb_register failed (err %d)\n", err);
		return err;
	}

	err = fmna_motion_detection_cb_register(&motion_detection_callbacks);
	if (err) {
		printk("fmna_motion_detection_cb_register failed (err %d)\n", err);
		return err;
	}

	err = fmna_id_create(FMNA_BT_ID);
	if (err) {
		printk("fmna_id_create failed (err %d)\n", err);
		return err;
	}

	enable_param.bt_id = FMNA_BT_ID;
	enable_param.init_battery_level = battery_level;
	enable_param.use_default_factory_settings = factory_settings_restore_check();

	err = fmna_enable(&enable_param, &enable_callbacks);
	if (err) {
		printk("fmna_enable failed (err %d)\n", err);
		return err;
	}

	return err;
}

static int ble_stack_initialize(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return err;
	}

	err = settings_load();
	if (err) {
		printk("Settings loading failed (err %d)\n", err);
		return err;
	}

	printk("Bluetooth initialized\n");

	return 0;
}

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	int err;
	uint32_t buttons = button_state & has_changed;

	if (buttons & FMNA_ADV_RESUME_BUTTON) {
		if (pairing_mode_exit) {
			err = fmna_resume();
			if (err) {
				printk("Cannot resume the FMN activity (err: %d)\n", err);
			} else {
				printk("FMN pairing mode resumed\n");
			}

			pairing_mode_exit = false;
		}
	}

	if (buttons & FMNA_SN_LOOKUP_BUTTON) {
		err = fmna_serial_number_lookup_enable();
		if (err) {
			printk("Cannot enable FMN Serial Number lookup (err: %d)\n", err);
		} else {
			printk("FMN Serial Number lookup enabled\n");
		}
	}

	if (buttons & FMNA_MOTION_INDICATION_BUTTON) {
		if (motion_detection_enabled) {
			motion_detected = true;
			dk_set_led(FMNA_MOTION_INDICATION_LED, 1);

			printk("Motion detected\n");
		} else {
			printk("Motion detection is disabled\n");
		}
	}

	if (buttons & FMNA_BATTERY_LEVEL_CHANGE_BUTTON) {
		battery_level = (battery_level > BATTERY_LEVEL_CHANGE_RATE) ?
			(battery_level - BATTERY_LEVEL_CHANGE_RATE) : BATTERY_LEVEL_MAX;

		err = fmna_battery_level_set(battery_level);
		if (err) {
			printk("fmna_battery_level_set failed (err %d)\n", err);
		} else {
			printk("Setting battery level to: %d %%\n", battery_level);
		}
	}
}

static int dk_library_initialize(void)
{
	int err;

	err = dk_leds_init();
	if (err) {
		printk("LEDs init failed (err %d)\n", err);
		return err;
	}

	err = dk_buttons_init(button_changed);
	if (err) {
		printk("Buttons init failed (err: %d)\n", err);
		return err;
	}

	return 0;
}

void main(void)
{
	int err;

	printk("Starting the FMN application\n");

	err = dk_library_initialize();
	if (err) {
		printk("DK library init failed (err %d)\n", err);
		return;
	}

	err = ble_stack_initialize();
	if (err) {
		printk("BLE stack init failed (err %d)\n", err);
		return;
	}

	err = fmna_initialize();
	if (err) {
		printk("FMNA init failed (err %d)\n", err);
		return;
	}

	printk("FMNA initialized\n");
}
