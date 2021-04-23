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

static struct k_work mfi_token_display_work;

static void mfi_token_display_work_handler(struct k_work *work)
{
	int err;
	uint8_t uuid[FMNA_SW_AUTH_UUID_BLEN] = {0};
	uint8_t auth_token[FMNA_SW_AUTH_TOKEN_BLEN] = {0};
	uint8_t serial_number[FMNA_SERIAL_NUMBER_BLEN] = {0};

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
}

static void firmware_version_display(void)
{
	int err;
	struct fmna_version ver;

	err = fmna_version_fw_get(&ver);
	if (err) {
		LOG_ERR("fmna_version_fw_get returned error: %d", err);
		memset(&ver, 0, sizeof(ver));
	}

	LOG_INF("App firmware version: v%d.%d.%d",
		ver.major, ver.minor, ver.revision);
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
	err = fmna_battery_init(cb->battery_level_request);
	if (err) {
		LOG_ERR("fmna_battery_init returned error: %d", err);
		return err;
	}

	err = fmna_conn_init();
	if (err) {
		LOG_ERR("fmna_conn_init returned error: %d", err);
		return err;
	}

	err = fmna_storage_init(param->use_default_factory_settings);
	if (err) {
		LOG_ERR("fmna_storage_init returned error: %d", err);
		return err;
	}

	err = fmna_pair_init();
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

	firmware_version_display();

	/* MFi tokens use a lot of stack, offload display logic
	 * to the workqueue.
	 */
	k_work_init(&mfi_token_display_work, mfi_token_display_work_handler);
	k_work_submit(&mfi_token_display_work);

	return err;
}
