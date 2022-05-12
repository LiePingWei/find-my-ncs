/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <fmna.h>

#include "crypto/fm_crypto.h"
#include "events/fmna_event.h"
#include "events/fmna_owner_event.h"
#include "fmna_gatt_fmns.h"
#include "fmna_product_plan.h"
#include "fmna_serial_number.h"
#include "fmna_state.h"
#include "fmna_storage.h"

#if defined(CONFIG_TRUSTED_EXECUTION_NONSECURE)
#if defined(CONFIG_BUILD_WITH_TFM)
#include <tfm_ioctl_api.h>
#elif defined(CONFIG_SPM_SERVICE_READ)
#include <secure_services.h>
#endif /* CONFIG_SPM_SERVICE_READ */
#else
#include <hal/nrf_ficr.h>
#endif /* CONFIG_TRUSTED_EXECUTION_NONSECURE */

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

#define SN_DEVICE_ID_WORD_LEN     2

#define SN_LOOKUP_INTERVAL        K_MINUTES(5)

#define SN_PAYLOAD_HMAC_LEN       32
#define SN_PAYLOAD_OP_LEN         4

struct __packed sn_hmac_payload {
	uint8_t serial_number[FMNA_SERIAL_NUMBER_BLEN];
	uint64_t counter;
	char op[SN_PAYLOAD_OP_LEN];
};

struct __packed sn_payload {
	uint8_t serial_number[FMNA_SERIAL_NUMBER_BLEN];
	uint64_t counter;
	uint8_t hmac[SN_PAYLOAD_HMAC_LEN];
	char op[SN_PAYLOAD_OP_LEN];
};

static void sn_lookup_timeout_handle(struct k_timer *timer_id);

static K_TIMER_DEFINE(sn_lookup_timer, sn_lookup_timeout_handle, NULL);

static bool is_lookup_enabled = false;

static void sn_lookup_timeout_handle(struct k_timer *timer_id)
{
	is_lookup_enabled = false;

	LOG_INF("Serial Number lookup disabled: timeout");
}

int fmna_serial_number_lookup_enable(void)
{
	if (!fmna_is_ready()) {
		return -EINVAL;
	}

	if (!IS_ENABLED(CONFIG_FMNA_CAPABILITY_BLE_SN_LOOKUP_ENABLED)) {
		return -ENOTSUP;
	}

	k_timer_start(&sn_lookup_timer, SN_LOOKUP_INTERVAL, K_NO_WAIT);
	is_lookup_enabled = true;

	LOG_INF("Serial Number lookup enabled");

	return 0;
}

#if defined(CONFIG_TRUSTED_EXECUTION_NONSECURE)

static int device_id_get(uint32_t *device_id, size_t device_id_len)
{
	int err;
	const size_t device_id_size = device_id_len * sizeof(*device_id);
	uint32_t device_id_addr = (uint32_t)NRF_FICR_S->INFO.DEVICEID;

	__ASSERT(sizeof(NRF_FICR_S->INFO.DEVICEID) == device_id_size,
		 "Length of the Device ID buffer misaligned with the corresponding FICR register.");

#if defined(CONFIG_BUILD_WITH_TFM)
	enum tfm_platform_err_t plt_err;

	err = 0;
	plt_err = tfm_platform_mem_read(device_id, device_id_addr, device_id_size, &err);
	if ((plt_err != TFM_PLATFORM_ERR_SUCCESS) || (err != 0)) {
		LOG_ERR("fmna_serial_number: cannot read FICR Device ID: plt_err %d, err: %d",
			plt_err, err);
		return -EACCES;
	}
#elif defined(CONFIG_SPM_SERVICE_READ)
	err = spm_request_read(device_id, device_id_addr, device_id_size);
	if (err) {
		LOG_ERR("fmna_serial_number: cannot read FICR Device ID: err %d", err);
		return err;
	}
#else
#error "Find My serial number: cannot read FICR Device ID in current configuration"
#endif /* CONFIG_SPM_SERVICE_READ */

	return 0;
}

#else

static int device_id_get(uint32_t *device_id, size_t device_id_len)
{
	for (size_t i = 0; i < device_id_len; i++) {
		device_id[i] = nrf_ficr_deviceid_get(NRF_FICR, i);
	}

	return 0;
}

#endif /* CONFIG_TRUSTED_EXECUTION_NONSECURE */

int fmna_serial_number_get(uint8_t serial_number[FMNA_SERIAL_NUMBER_BLEN])
{
	int err;

#if CONFIG_FMNA_CUSTOM_SERIAL_NUMBER
	err = fmna_storage_serial_number_load(serial_number);
	if (err) {
		LOG_ERR("fmna_serial_number: fmna_storage_serial_number_load err %d", err);
		return err;
	}
#else
	uint32_t device_id[SN_DEVICE_ID_WORD_LEN];
	uint8_t sn_temp[FMNA_SERIAL_NUMBER_BLEN + 1];
	size_t index;

	/* Use Device ID as a serial number. */
	err = device_id_get(device_id, ARRAY_SIZE(device_id));
	if (err) {
		LOG_ERR("fmna_serial_number: device_id_get returned err: %d", err);
		return err;
	}

	/* Convert to a character string */
	index = bin2hex((uint8_t *) device_id,
			sizeof(device_id),
			sn_temp,
			sizeof(sn_temp));

	/* Use a temporary buffer to protect memory segmenets next to serial number
	 * pointer. The bin2hex function writes a string terminator at the
	 * FMNA_SERIAL_NUMBER_BLEN array index. */
	memcpy(serial_number, sn_temp, FMNA_SERIAL_NUMBER_BLEN);

	/* Pad remaining with 'f' */
	while (index < FMNA_SERIAL_NUMBER_BLEN) {
		serial_number[index++] = 'f';
	}
#endif

	return 0;
}

int fmna_serial_number_enc_get(enum fmna_serial_number_enc_query_type query_type,
			       uint8_t sn_response[FMNA_SERIAL_NUMBER_ENC_BLEN])
{
	int err;
	uint64_t counter;
	struct sn_hmac_payload sn_hmac_payload;
	struct sn_payload sn_payload;
	uint8_t server_shared_secret[FMNA_SERVER_SHARED_SECRET_LEN];

	/* Clear the encrypted serial number initially in case of error. */
	memset(sn_response, 0, FMNA_SERIAL_NUMBER_ENC_BLEN);

	/* Initialize input parameters to zero. */
	memset(&sn_payload, 0, sizeof(sn_payload));
	memset(&sn_hmac_payload, 0, sizeof(sn_hmac_payload));

	err = fmna_storage_pairing_item_load(FMNA_STORAGE_SN_QUERY_COUNTER_ID,
					     (uint8_t *) &counter,
					     sizeof(counter));
	if (err) {
		LOG_ERR("fmna_serial_number: fmna_storage_pairing_item_load err %d", err);
		return err;
	}

	sn_payload.counter = counter;
	sn_hmac_payload.counter = counter;

	err = fmna_serial_number_get(sn_payload.serial_number);
	if (err) {
		LOG_ERR("fmna_serial_number: fmna_serial_number_get err %d", err);
		return err;
	}
	memcpy(sn_hmac_payload.serial_number,
	       sn_payload.serial_number,
	       sizeof(sn_hmac_payload.serial_number));

	switch (query_type) {
	case FMNA_SERIAL_NUMBER_ENC_QUERY_TYPE_TAP:
		strcpy(sn_hmac_payload.op, "tap");
		strcpy(sn_payload.op, "tap");
		break;
	case FMNA_SERIAL_NUMBER_ENC_QUERY_TYPE_BT:
		strcpy(sn_hmac_payload.op, "bt");
		strcpy(sn_payload.op, "bt");
		break;
	default:
		LOG_ERR("Invalid Serial Number Query Type");
		return -EINVAL;
	}

	err = fmna_storage_pairing_item_load(FMNA_STORAGE_SERVER_SHARED_SECRET_ID,
					     server_shared_secret,
					     sizeof(server_shared_secret));
	if (err) {
		LOG_ERR("fmna_serial_number: fmna_storage_pairing_item_load err %d", err);
		return err;
	}

	err = fm_crypto_authenticate_with_ksn(server_shared_secret,
					      sizeof(sn_hmac_payload),
					      (const uint8_t *) &sn_hmac_payload,
					      sn_payload.hmac);
	if (err) {
		LOG_ERR("fmna_serial_number: fm_crypto_authenticate_with_ksn err %d", err);
		return err;
	}

	uint32_t sn_response_len = FMNA_SERIAL_NUMBER_ENC_BLEN;
	err = fm_crypto_encrypt_to_server(fmna_pp_server_encryption_key,
					  sizeof(sn_payload),
					  (const uint8_t *) &sn_payload,
					  &sn_response_len,
					  sn_response);
	if (err) {
		LOG_ERR("fmna_serial_number: fm_crypto_encrypt_to_server err %d", err);

		/* Clear the encrypted serial number in case of fm_crypto_encrypt_to_server
		 * error.
		 */
		memset(sn_response, 0, FMNA_SERIAL_NUMBER_ENC_BLEN);
		return err;
	}

	return 0;
}

int fmna_serial_number_enc_counter_increase(uint32_t increment)
{
	int err;
	uint64_t counter = {0};

	__ASSERT(increment > 0, "fmna serial number increment must be greater than zero");

	err = fmna_storage_pairing_item_load(FMNA_STORAGE_SN_QUERY_COUNTER_ID,
					     (uint8_t *) &counter,
					     sizeof(counter));
	if (err) {
		LOG_ERR("fmna_serial_number: fmna_storage_pairing_item_load err %d", err);
		return err;
	}

	counter += increment;

	/* Store the Serial Number counter. */
	err = fmna_storage_pairing_item_store(FMNA_STORAGE_SN_QUERY_COUNTER_ID,
					      (uint8_t *) &counter,
					      sizeof(counter));
	if (err) {
		LOG_ERR("fmna_serial_number: fmna_storage_pairing_item_store err %d", err);
		return err;
	}

	LOG_INF("Serial Number query count: %llu", counter);

	FMNA_EVENT_CREATE(event, FMNA_EVENT_SERIAL_NUMBER_CNT_CHANGED, NULL);
	APP_EVENT_SUBMIT(event);

	return 0;
}

static void serial_number_request_handle(struct bt_conn *conn)
{
	int err;

	LOG_INF("Requesting Serial Number");

	if (fmna_state_is_paired() && is_lookup_enabled) {
		struct net_buf_simple sn_rsp_buf;
		uint8_t encrypted_sn_rsp[FMNA_SERIAL_NUMBER_ENC_BLEN];

		err = fmna_serial_number_enc_get(FMNA_SERIAL_NUMBER_ENC_QUERY_TYPE_BT,
						 encrypted_sn_rsp);
		if (err) {
			LOG_ERR("fmna_serial_number: fmna_serial_number_enc_get returned error: %d", err);
			return;
		}

		net_buf_simple_init_with_data(&sn_rsp_buf,
					      encrypted_sn_rsp,
					      sizeof(encrypted_sn_rsp));

		err = fmna_gatt_owner_cp_indicate(conn,
						  FMNA_GATT_OWNER_SERIAL_NUMBER_IND,
						  &sn_rsp_buf);
		if (err) {
			LOG_ERR("fmna_serial_number: fmna_gatt_owner_cp_indicate returned error: %d", err);
			return;
		}

		err = fmna_serial_number_enc_counter_increase(1);
		if (err) {
			LOG_ERR("fmna_serial_number: fmna_serial_number_enc_counter_increase"
				" returned error: %d", err);
			return;
		}

		LOG_INF("Responding to the Serial Number request");
	} else {
		uint16_t cmd_opcode;

		cmd_opcode = fmna_owner_event_to_gatt_cmd_opcode(FMNA_OWNER_EVENT_GET_SERIAL_NUMBER);
		FMNA_GATT_COMMAND_RESPONSE_BUILD(invalid_state_cmd_rsp, cmd_opcode, FMNA_GATT_RESPONSE_STATUS_INVALID_STATE);

		err = fmna_gatt_owner_cp_indicate(conn,
						  FMNA_GATT_OWNER_COMMAND_RESPONSE_IND,
						  &invalid_state_cmd_rsp);
		if (err) {
			LOG_ERR("fmna_serial_number: fmna_gatt_owner_cp_indicate returned error: %d", err);
			return;
		}

		LOG_INF("Rejecting the Serial Number request");
	}
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_fmna_owner_event(aeh)) {
		struct fmna_owner_event *event = cast_fmna_owner_event(aeh);

		switch (event->id) {
		case FMNA_OWNER_EVENT_GET_SERIAL_NUMBER:
			serial_number_request_handle(event->conn);
			break;
		default:
			break;
		}

		return false;
	}

	return false;
}

APP_EVENT_LISTENER(fmna_serial_number, app_event_handler);
APP_EVENT_SUBSCRIBE(fmna_serial_number, fmna_owner_event);
