/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <sys/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>

#include <fmna.h>

#include <settings/settings.h>

#include <dk_buttons_and_leds.h>

#define FMNA_BT_ID 1

#define FMNA_SOUND_DURATION K_SECONDS(5)

#define FMNA_SOUND_LED DK_LED1

#define FMNA_ADV_RESUME_BUTTON             DK_BTN1_MSK
#define FMNA_SN_LOOKUP_BUTTON              DK_BTN2_MSK
#define FMNA_FACTORY_SETTINGS_RESET_BUTTON DK_BTN4_MSK

static bool pairing_mode_exit;

static void sound_timeout_work_handle(struct k_work *item);

static K_DELAYED_WORK_DEFINE(sound_timeout_work, sound_timeout_work_handle);

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

static void sound_start(void)
{
	printk("Received a request from FMN to start playing sound\n");
	printk("Starting to play sound...\n");

	k_delayed_work_submit(&sound_timeout_work, FMNA_SOUND_DURATION);

	dk_set_led(FMNA_SOUND_LED, 1);
}

static void sound_stop(void)
{
	printk("Received a request from FMN to stop playing sound\n");

	k_delayed_work_cancel(&sound_timeout_work);

	sound_stop_indicate();
}

static struct fmna_sound_cb sound_callbacks = {
	.sound_start = sound_start,
	.sound_stop = sound_stop,
};

static void pairing_mode_exited(void)
{
	printk("Exited the FMN pairing mode\n");

	pairing_mode_exit = true;
}

static struct fmna_enable_cb enable_callbacks = {
	.pairing_mode_exited = pairing_mode_exited,
};

static int fmna_id_create(uint8_t id)
{
	int ret;
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	size_t count;

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

	err = fmna_id_create(FMNA_BT_ID);
	if (err) {
		printk("fmna_id_create failed (err %d)\n", err);
		return err;
	}

	enable_param.bt_id = FMNA_BT_ID;
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
