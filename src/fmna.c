/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "fmna_adv.h"
#include "fmna_battery.h"
#include "fmna_conn.h"
#include "fmna_keys.h"
#include "fmna_pair.h"
#include "fmna_serial_number.h"
#include "fmna_storage.h"
#include "fmna_state.h"
#include "fmna_version.h"

#include <fmna.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(fmna, CONFIG_FMNA_LOG_LEVEL);

BUILD_ASSERT(CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE >= 4096,
	"The workqueue stack size is too small for the FMN");

static struct k_work basic_display_work;

static void basic_display_work_handler(struct k_work *work)
{
	int err;
	uint8_t uuid[FMNA_SW_AUTH_UUID_BLEN] = {0};
	uint8_t auth_token[FMNA_SW_AUTH_TOKEN_BLEN] = {0};
	uint8_t serial_number[FMNA_SERIAL_NUMBER_BLEN] = {0};
	struct fmna_version ver;

	err = fmna_storage_uuid_load(uuid);
	if (err == -ENOENT) {
		LOG_WRN("MFi Token UUID not found: "
			"please provsion a token to the device");
	} else if (err) {
		LOG_ERR("fmna_storage_uuid_load returned error: %d", err);
	} else {
		LOG_HEXDUMP_INF(uuid, sizeof(uuid), "SW UUID:");
	}

	err = fmna_storage_auth_token_load(auth_token);
	if (err == -ENOENT) {
		LOG_WRN("MFi Authentication Token not found: "
			"please provsion a token to the device");
	} else if (err) {
		LOG_ERR("fmna_storage_auth_token_load returned error: %d",
			err);
	} else {
		LOG_HEXDUMP_INF(auth_token, 16, "SW Authentication Token:");
		LOG_INF("(... %d more bytes ...)", FMNA_SW_AUTH_TOKEN_BLEN - 16);
	}

	fmna_serial_number_get(serial_number);
	LOG_HEXDUMP_INF(serial_number, sizeof(serial_number),
			"Serial Number:");

	err = fmna_version_fw_get(&ver);
	if (err) {
		LOG_ERR("fmna_version_fw_get returned error: %d", err);
		memset(&ver, 0, sizeof(ver));
	}

	LOG_INF("Application firmware version: v%d.%d.%d",
		ver.major, ver.minor, ver.revision);

	if (IS_ENABLED(CONFIG_FMNA_QUALIFICATION)) {
		LOG_WRN("The FMN stack is configured for qualification");
		LOG_WRN("The qualification configuration should not be used for production");
	}
}

int fmna_enable(const struct fmna_enable_param *param,
		const struct fmna_enable_cb *cb)
{
	int err;

	/* Verify the input parameters. */
	if (!param || !cb) {
		return -EINVAL;
	}

	/* Verify the state of FMN dependencies. */
	err = bt_enable(NULL);
	if (err != -EALREADY) {
		LOG_ERR("FMN: BLE stack should be enabled");
		return -ENOPROTOOPT;
	}

	/* Register enable callbacks. */
	err = fmna_adv_unpaired_cb_register(cb->pairing_mode_exited);
	if (err) {
		LOG_ERR("fmna_adv_unpaired_cb_register returned error: %d", err);
		return err;
	}

	/* Initialize FMN modules. */
	err = fmna_battery_init(param->init_battery_level, cb->battery_level_request);
	if (err) {
		LOG_ERR("fmna_battery_init returned error: %d", err);
		return err;
	}

	err = fmna_conn_init(param->bt_id);
	if (err) {
		LOG_ERR("fmna_conn_init returned error: %d", err);
		return err;
	}

	err = fmna_storage_init(param->use_default_factory_settings);
	if (err) {
		LOG_ERR("fmna_storage_init returned error: %d", err);
		return err;
	}

	err = fmna_pair_init(param->bt_id);
	if (err) {
		LOG_ERR("fmna_pair_init returned error: %d", err);
		return err;
	}

	err = fmna_keys_init(param->bt_id);
	if (err) {
		LOG_ERR("fmna_keys_init returned error: %d", err);
		return err;
	}

	err = fmna_state_init(param->bt_id);
	if (err) {
		LOG_ERR("fmna_state_init returned error: %d", err);
		return err;
	}

	/* MFi tokens use a lot of stack, offload basic display logic
	 * to the workqueue.
	 */
	k_work_init(&basic_display_work, basic_display_work_handler);
	k_work_submit(&basic_display_work);

	return err;
}
