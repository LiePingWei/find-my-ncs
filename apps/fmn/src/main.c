/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <errno.h>
#include <sys/printk.h>
#include <zephyr.h>
#include <zephyr/types.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <fmna.h>

#include <settings/settings.h>

#include <dk_buttons_and_leds.h>

#define BT_ID_FMN 1
#define FMN_SN_LOOKUP_BUTTON DK_BTN1_MSK

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
		return;
	}

	printk("Connected\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr, level);
	} else {
		printk("Security failed: %s level %u err %d\n", addr, level,
			err);
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected        = connected,
	.disconnected     = disconnected,
	.security_changed = security_changed,
};

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing failed conn: %s, reason %d\n", addr, reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
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

static int fmna_initialize(void)
{
	int err;
	struct fmna_init_params init_params = {0};

	err = fmna_id_create(BT_ID_FMN);
	if (err) {
		printk("fmna_id_create failed (err %d)\n", err);
		return err;
	}

	init_params.bt_id = BT_ID_FMN;

	err = fmna_init(&init_params);
	if (err) {
		printk("fmna_init failed (err %d)\n", err);
		return err;
	}

	return err;
}

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	int err;
	uint32_t buttons = button_state & has_changed;

	if (buttons & FMN_SN_LOOKUP_BUTTON) {
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

	err = dk_buttons_init(button_changed);
	if (err) {
		printk("Cannot init buttons (err: %d)\n", err);
	}

	return err;
}

void main(void)
{
	int err;

	printk("Starting the FMN application\n");

	bt_conn_cb_register(&conn_callbacks);
	bt_conn_auth_cb_register(&conn_auth_callbacks);

	err = dk_library_initialize();
	if (err) {
		printk("DK library init failed (err %d)\n", err);
		return;
	}

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	err = fmna_initialize();
	if (err) {
		printk("FMNA init failed (err %d)\n", err);
		return;
	}

	printk("FMNA initialized\n");
}
