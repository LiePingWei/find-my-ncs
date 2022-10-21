/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

#include <fmna.h>

#include <zephyr/settings/settings.h>

#include <dk_buttons_and_leds.h>

#define FMNA_BT_ID 1

#define FMNA_PEER_SOUND_DURATION K_SECONDS(5)
#define FMNA_UT_SOUND_DURATION   K_SECONDS(1)

#define FMNA_SOUND_LED             DK_LED1
#define FMNA_MOTION_INDICATION_LED DK_LED2
#define FMNA_PAIRED_STATE_LED      DK_LED3
#define FMNA_PAIRING_MODE_LED      DK_LED3
#define FMNA_ACTIVATION_LED        DK_LED4

#define FMNA_PAIRING_MODE_BLINK_INTERVAL 500

#define FMNA_ADV_RESUME_BUTTON             DK_BTN1_MSK
#define FMNA_ACTIVATION_BUTTON             DK_BTN1_MSK
#define FMNA_SN_LOOKUP_BUTTON              DK_BTN2_MSK
#define FMNA_MOTION_INDICATION_BUTTON      DK_BTN3_MSK
#define FMNA_FACTORY_SETTINGS_RESET_BUTTON DK_BTN4_MSK
#define FMNA_BATTERY_LEVEL_CHANGE_BUTTON   DK_BTN4_MSK

#define FMNA_ACTIVATION_MIN_HOLD_TIME_MS 3000

#define FMNA_ACTIVATION_ERROR_RETRY_TIME K_SECONDS(1)

#define BATTERY_LEVEL_MAX         100
#define BATTERY_LEVEL_MIN         0
#define BATTERY_LEVEL_CHANGE_RATE 7

static bool paired;
static bool pairing_mode;
static bool motion_detection_enabled;
static bool motion_detected;
static uint8_t battery_level = BATTERY_LEVEL_MAX;

static void enable_work_handle(struct k_work *item);
static void disable_work_handle(struct k_work *item);
static void sound_timeout_work_handle(struct k_work *item);

static K_WORK_DELAYABLE_DEFINE(enable_work, enable_work_handle);
static K_WORK_DELAYABLE_DEFINE(disable_work, disable_work_handle);
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

static void pairing_failed(void)
{
	printk("FMN pairing has failed\n");
}

static void pairing_mode_exited(void)
{
	printk("Exited the FMN pairing mode\n");

	pairing_mode = false;

	dk_set_led(FMNA_PAIRING_MODE_LED, 0);
}

static void paired_state_changed(bool new_paired_state)
{
	printk("The FMN accessory transitioned to the %spaired state\n",
	       new_paired_state ? "" : "un");

	paired = new_paired_state;

	if (new_paired_state) {
		pairing_mode = false;
	}

	dk_set_led(FMNA_PAIRED_STATE_LED, new_paired_state);
}

static const struct fmna_enable_cb enable_callbacks = {
	.battery_level_request = battery_level_request,
	.pairing_failed = pairing_failed,
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

	dk_set_led(FMNA_ACTIVATION_LED, 1);

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

static void adv_resume_action_handle(void)
{
	int err;

	if (!paired) {
		err = fmna_resume();
		if (err) {
			printk("Cannot resume the FMN activity (err: %d)\n", err);
		} else {
			printk("%s the FMN pairing mode\n",
			       pairing_mode ? "Extending" : "Enabling");

			pairing_mode = true;
		}
	}
}

static void enable_work_handle(struct k_work *item)
{
	int err;
	struct fmna_enable_param enable_param = {0};

	enable_param.bt_id = FMNA_BT_ID;
	enable_param.init_battery_level = battery_level;
	enable_param.use_default_factory_settings = factory_settings_restore_check();

	err = fmna_enable(&enable_param, &enable_callbacks);
	if (err) {
		printk("fmna_enable failed (err %d)\n", err);

		k_work_reschedule(&enable_work, FMNA_ACTIVATION_ERROR_RETRY_TIME);
	} else {
		printk("FMN enabled\n");

		dk_set_led(FMNA_ACTIVATION_LED, 1);
	}
}

static void disable_work_handle(struct k_work *item)
{
	int err;

	err = fmna_disable();
	if (err) {
		printk("fmna_disable failed (err: %d)\n", err);

		k_work_reschedule(&disable_work, FMNA_ACTIVATION_ERROR_RETRY_TIME);
	} else {
		printk("FMN disabled\n");

		/* Reset the necessary flags. */
		pairing_mode = false;
		motion_detection_enabled = false;
		motion_detected = false;

		dk_set_led(FMNA_ACTIVATION_LED, 0);
	}
}

static void activation_action_handle(void)
{
	/* In case we are in error retry mode. */
	k_work_cancel_delayable(&enable_work);
	k_work_cancel_delayable(&disable_work);

	if (fmna_is_ready()) {
		disable_work_handle(NULL);
	} else {
		enable_work_handle(NULL);
	}
}

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	int err;
	uint32_t buttons = button_state & has_changed;

	if (has_changed & (FMNA_ADV_RESUME_BUTTON | FMNA_ACTIVATION_BUTTON)) {
		static int64_t prev_uptime;

		if (button_state & (FMNA_ADV_RESUME_BUTTON | FMNA_ACTIVATION_BUTTON)) {
			/* On button press */
			prev_uptime = k_uptime_get();
		} else {
			/* On button release */
			int64_t hold_time;

			hold_time = (k_uptime_get() - prev_uptime);
			if (hold_time > FMNA_ACTIVATION_MIN_HOLD_TIME_MS) {
				activation_action_handle();
			} else {
				adv_resume_action_handle();
			}
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

static void pairing_mode_indicate(void)
{
	uint32_t blink_status = 0;

	for (;;) {
		if (pairing_mode) {
			dk_set_led(FMNA_PAIRING_MODE_LED, (++blink_status) % 2);
		}

		k_sleep(K_MSEC(FMNA_PAIRING_MODE_BLINK_INTERVAL));
	}
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

	/* This function never returns. */
	pairing_mode_indicate();
}
