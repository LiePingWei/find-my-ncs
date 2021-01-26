#include "fmna_pair.h"
#include "fmna_storage.h"

#include <logging/log.h>

LOG_MODULE_REGISTER(fmna, CONFIG_FMN_ADK_LOG_LEVEL);

static struct k_work mfi_token_display_work;

static void mfi_token_display_work_handler(struct k_work *work)
{
	int err;
	uint8_t uuid[FMNA_SW_AUTH_UUID_BLEN] = {0};
	uint8_t auth_token[FMNA_SW_AUTH_TOKEN_BLEN] = {0};

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
}

int fmna_init(void)
{
	int err;

	err = fmna_storage_init();
	if (err) {
		LOG_ERR("fmna_storage_init returned error: %d", err);
		return err;
	}

	err = fmna_pair_init();
	if (err) {
		LOG_ERR("fmna_pair_init returned error: %d", err);
		return err;
	}

	/* MFi tokens use a lot of stack, offload display logic
	 * to the workqueue.
	 */
	k_work_init(&mfi_token_display_work, mfi_token_display_work_handler);
	k_work_submit(&mfi_token_display_work);

	return err;
}
