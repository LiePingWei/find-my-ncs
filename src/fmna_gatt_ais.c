/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/types.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include "fmna_battery.h"
#include "fmna_product_plan.h"
#include "fmna_version.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

#define BT_UUID_AIS_VAL \
	BT_UUID_128_ENCODE(0x87290102, 0x3C51, 0x43B1, 0xA1A9, 0x11B9DC38478B)
#define BT_UUID_AIS BT_UUID_DECLARE_128(BT_UUID_AIS_VAL)

#define BT_UUID_AIS_CHRC_BASE(_chrc_id) \
	BT_UUID_128_ENCODE((0x6AA50000 + _chrc_id), 0x6352, 0x4D57, 0xA7B4, 0x003A416FBB0B)

#define BT_UUID_AIS_PRODUCT_DATA      BT_UUID_DECLARE_128(BT_UUID_AIS_CHRC_BASE(0x0001))
#define BT_UUID_AIS_MANUFACTURER_NAME BT_UUID_DECLARE_128(BT_UUID_AIS_CHRC_BASE(0x0002))
#define BT_UUID_AIS_MODEL_NAME        BT_UUID_DECLARE_128(BT_UUID_AIS_CHRC_BASE(0x0003))

#define BT_UUID_AIS_ACC_CATEGORY     BT_UUID_DECLARE_128(BT_UUID_AIS_CHRC_BASE(0x0005))
#define BT_UUID_AIS_ACC_CAPABILITIES BT_UUID_DECLARE_128(BT_UUID_AIS_CHRC_BASE(0x0006))
#define BT_UUID_AIS_FW_VERSION       BT_UUID_DECLARE_128(BT_UUID_AIS_CHRC_BASE(0x0007))
#define BT_UUID_AIS_FMN_VERSION      BT_UUID_DECLARE_128(BT_UUID_AIS_CHRC_BASE(0x0008))
#define BT_UUID_AIS_BATTERY_TYPE     BT_UUID_DECLARE_128(BT_UUID_AIS_CHRC_BASE(0x0009))
#define BT_UUID_AIS_BATTERY_LEVEL    BT_UUID_DECLARE_128(BT_UUID_AIS_CHRC_BASE(0x000A))

#if CONFIG_FMNA_BATTERY_TYPE_POWERED
#define BATTERY_TYPE 0
#elif CONFIG_FMNA_BATTERY_TYPE_NON_RECHARGEABLE
#define BATTERY_TYPE 1
#elif CONFIG_FMNA_BATTERY_TYPE_RECHARGEABLE
#define BATTERY_TYPE 2
#else
#error "FMN Battery level is not set"
#endif

#define ACC_CATEGORY_LEN 8

enum acc_capabilities {
	BT_ACC_CAPABILITIES_PLAY_SOUND    = 0,
	BT_ACC_CAPABILITIES_DETECT_MOTION = 1,
	BT_ACC_CAPABILITIES_NFC_SN_LOOKUP = 2,
	BT_ACC_CAPABILITIES_BLE_SN_LOOKUP = 3,
	BT_ACC_CAPABILITIES_FW_UPDATE_SVC = 4,
};

static ssize_t product_data_read(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr,
				 void *buf, uint16_t len, uint16_t offset)
{
	LOG_INF("AIS Product Data read, handle: %u, conn: %p",
		attr->handle, (void *) conn);

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &fmna_pp_product_data, sizeof(fmna_pp_product_data));
}

static ssize_t manufacturer_name_read(struct bt_conn *conn,
				      const struct bt_gatt_attr *attr,
				      void *buf, uint16_t len, uint16_t offset)
{
	LOG_INF("AIS Manufacturer Name read, handle: %u, conn: %p",
		attr->handle, (void *) conn);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 strlen(attr->user_data));
}

static ssize_t model_name_read(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr,
			       void *buf, uint16_t len, uint16_t offset)
{
	LOG_INF("AIS Model Name read, handle: %u, conn: %p",
		attr->handle, (void *) conn);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 strlen(attr->user_data));
}

static ssize_t acc_category_read(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr,
				 void *buf, uint16_t len, uint16_t offset)
{
	uint8_t acc_category[ACC_CATEGORY_LEN] = {0};

	LOG_INF("AIS Accessory Category read, handle: %u, conn: %p",
		attr->handle, (void *) conn);

	acc_category[0] = CONFIG_FMNA_CATEGORY;

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 acc_category, sizeof(acc_category));
}

static ssize_t acc_capabilities_read(struct bt_conn *conn,
				     const struct bt_gatt_attr *attr,
				     void *buf, uint16_t len, uint16_t offset)
{
	uint32_t acc_capabilities = 0;

	LOG_INF("AIS Accessory Capabilities read, handle: %u, conn: %p",
		attr->handle, (void *) conn);

	WRITE_BIT(acc_capabilities, BT_ACC_CAPABILITIES_PLAY_SOUND,
		  IS_ENABLED(CONFIG_FMNA_CAPABILITY_PLAY_SOUND_ENABLED));
	WRITE_BIT(acc_capabilities, BT_ACC_CAPABILITIES_DETECT_MOTION,
		  IS_ENABLED(CONFIG_FMNA_CAPABILITY_DETECT_MOTION_ENABLED));
	WRITE_BIT(acc_capabilities, BT_ACC_CAPABILITIES_NFC_SN_LOOKUP,
		  IS_ENABLED(CONFIG_FMNA_CAPABILITY_NFC_SN_LOOKUP_ENABLED));
	WRITE_BIT(acc_capabilities, BT_ACC_CAPABILITIES_BLE_SN_LOOKUP,
		  IS_ENABLED(CONFIG_FMNA_CAPABILITY_BLE_SN_LOOKUP_ENABLED));
	WRITE_BIT(acc_capabilities, BT_ACC_CAPABILITIES_FW_UPDATE_SVC,
		  IS_ENABLED(CONFIG_FMNA_CAPABILITY_FW_UPDATE_ENABLED));

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &acc_capabilities, sizeof(acc_capabilities));
}

static ssize_t fw_version_read(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr,
			       void *buf, uint16_t len, uint16_t offset)
{
	int err;
	uint32_t fw_version;
	struct fmna_version ver;

	err = fmna_version_fw_get(&ver);
	if (err) {
		LOG_ERR("AIS Firmware Version read: Firmware Version read failed");
		memset(&ver, 0, sizeof(ver));
	}

	fw_version = FMNA_VERSION_ENCODE(ver);

	LOG_INF("AIS Firmware Version read, handle: %u, conn: %p",
		attr->handle, (void *) conn);

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &fw_version, sizeof(fw_version));
}

static ssize_t fmn_version_read(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				void *buf, uint16_t len, uint16_t offset)
{
	struct fmna_version ver_desc = {
		.major = 1,
		.minor = 0,
		.revision = 0,
	};
	uint32_t fmn_spec_version = FMNA_VERSION_ENCODE(ver_desc);

	LOG_INF("AIS Find My Network Version read, handle: %u, conn: %p",
		attr->handle, (void *) conn);

	/* TODO: Make version configurable. */

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &fmn_spec_version, sizeof(fmn_spec_version));
}

static ssize_t battery_type_read(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr,
				 void *buf, uint16_t len, uint16_t offset)
{
	uint8_t battery_type = BATTERY_TYPE;

	LOG_INF("AIS Battery Type read, handle: %u, conn: %p",
		attr->handle, (void *) conn);

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &battery_type, sizeof(battery_type));
}

static ssize_t battery_level_read(struct bt_conn *conn,
				  const struct bt_gatt_attr *attr,
				  void *buf, uint16_t len, uint16_t offset)
{
	uint8_t battery_level;

	LOG_INF("AIS Battery Level read, handle: %u, conn: %p",
		attr->handle, (void *) conn);

	battery_level = fmna_battery_state_get();

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &battery_level, sizeof(battery_level));
}


/* Accessory information service Declaration */
BT_GATT_SERVICE_DEFINE(ais_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_AIS),
	BT_GATT_CHARACTERISTIC(BT_UUID_AIS_PRODUCT_DATA,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       product_data_read, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_AIS_MANUFACTURER_NAME,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       manufacturer_name_read, NULL,
			       CONFIG_FMNA_MANUFACTURER_NAME),
	BT_GATT_CHARACTERISTIC(BT_UUID_AIS_MODEL_NAME,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       model_name_read, NULL,
			       CONFIG_FMNA_MODEL_NAME),
	BT_GATT_CHARACTERISTIC(BT_UUID_AIS_ACC_CATEGORY,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       acc_category_read, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_AIS_ACC_CAPABILITIES,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       acc_capabilities_read, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_AIS_FW_VERSION,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       fw_version_read, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_AIS_FMN_VERSION,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       fmn_version_read, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_AIS_BATTERY_TYPE,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       battery_type_read, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_AIS_BATTERY_LEVEL,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       battery_level_read, NULL, NULL),
);
