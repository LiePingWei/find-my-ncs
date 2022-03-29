/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "crypto/fm_crypto.h"
#include "events/fmna_event.h"
#include "events/fmna_owner_event.h"
#include "fmna_gatt_fmns.h"
#include "fmna_product_plan.h"
#include "fmna_serial_number.h"
#include "fmna_storage.h"

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

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

static bool is_paired = false;
static bool is_lookup_enabled = false;

static void sn_lookup_timeout_handle(struct k_timer *timer_id)
{
	is_lookup_enabled = false;

	LOG_INF("Serial Number lookup disabled: timeout");
}

int fmna_serial_number_lookup_enable(void)
{
	if (!IS_ENABLED(CONFIG_FMNA_CAPABILITY_BLE_SN_LOOKUP_ENABLED)) {
		return -ENOTSUP;
	}

	k_timer_start(&sn_lookup_timer, SN_LOOKUP_INTERVAL, K_NO_WAIT);
	is_lookup_enabled = true;

	LOG_INF("Serial Number lookup enabled");

	return 0;
}

int fmna_serial_number_get(uint8_t serial_number[FMNA_SERIAL_NUMBER_BLEN])
{
#if CONFIG_FMNA_CUSTOM_SERIAL_NUMBER
	int err;

	err = fmna_storage_serial_number_load(serial_number);
	if (err) {
		LOG_ERR("fmna_serial_number: fmna_storage_serial_number_load err %d", err);
		return err;
	}
#else
	uint8_t temp[8];
	size_t index;

	/* XOR device ID and address to identify the device */
	*((uint32_t *)temp) =  NRF_FICR->DEVICEID[0];
	*((uint32_t *)temp) ^= NRF_FICR->DEVICEADDR[0];

	*((uint32_t *)(temp + 4)) =  NRF_FICR->DEVICEID[1];
	*((uint32_t *)(temp + 4)) ^= NRF_FICR->DEVICEADDR[1];

	/* Convert to a character string */
	index = bin2hex(temp, sizeof(temp), serial_number, FMNA_SERIAL_NUMBER_BLEN);

	/* Pad remaining with 'f' */
	while (index < FMNA_SERIAL_NUMBER_BLEN) {
		serial_number[index++] = 'f';
	}
#endif

	return 0;
}

static void encrypted_sn_response_build(enum fmna_serial_number_enc_query_type query_type,
					uint8_t sn_response[FMNA_SERIAL_NUMBER_ENC_BLEN])
{
	int err;
	struct sn_hmac_payload sn_hmac_payload;
	struct sn_payload sn_payload;
	uint8_t server_shared_secret[FMNA_SERVER_SHARED_SECRET_LEN];

	/* Clear the encrypted serial number initially in case of error. */
	memset(sn_response, 0, FMNA_SERIAL_NUMBER_ENC_BLEN);

	err = fmna_storage_pairing_item_load(FMNA_STORAGE_SN_QUERY_COUNTER_ID,
					     (uint8_t *) &sn_payload.counter,
					     sizeof(sn_payload.counter));
	if (err) {
		LOG_ERR("fmna_serial_number: fmna_storage_pairing_item_load err %d", err);
		return;
	}

	sn_payload.counter++;
	sn_hmac_payload.counter = sn_payload.counter;

	/* Store the Serial Number counter. */
	err = fmna_storage_pairing_item_store(FMNA_STORAGE_SN_QUERY_COUNTER_ID,
					      (uint8_t *) &sn_payload.counter,
					      sizeof(sn_payload.counter));
	if (err) {
		LOG_ERR("fmna_serial_number: fmna_storage_pairing_item_store err %d", err);
		return;
	}

	LOG_INF("Serial Number query count: %llu", sn_payload.counter);

	err = fmna_serial_number_get(sn_payload.serial_number);
	if (err) {
		LOG_ERR("fmna_serial_number: fmna_serial_number_get err %d", err);
		return;
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
		return;
	}

	err = fmna_storage_pairing_item_load(FMNA_STORAGE_SERVER_SHARED_SECRET_ID,
					     server_shared_secret,
					     sizeof(server_shared_secret));
	if (err) {
		LOG_ERR("fmna_serial_number: fmna_storage_pairing_item_load err %d", err);
		return;
	}

	err = fm_crypto_authenticate_with_ksn(server_shared_secret,
					      sizeof(sn_hmac_payload),
					      (const uint8_t *) &sn_hmac_payload,
					      sn_payload.hmac);
	if (err) {
		LOG_ERR("fmna_serial_number: fm_crypto_authenticate_with_ksn err %d", err);
		return;
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
		return;
	}
}

void fmna_serial_number_enc_get(
	enum fmna_serial_number_enc_query_type query_type,
	uint8_t serial_number_enc[FMNA_SERIAL_NUMBER_ENC_BLEN])
{
	encrypted_sn_response_build(query_type, serial_number_enc);
}

static void pairing_completed_handle(struct bt_conn *conn)
{
	is_paired = true;
}

static void serial_number_request_handle(struct bt_conn *conn)
{
	int err;

	LOG_INF("Requesting Serial Number");

	if (is_paired && is_lookup_enabled) {
		struct net_buf_simple sn_rsp_buf;
		uint8_t encrypted_sn_rsp[FMNA_SERIAL_NUMBER_ENC_BLEN];

		encrypted_sn_response_build(FMNA_SERIAL_NUMBER_ENC_QUERY_TYPE_BT, encrypted_sn_rsp);

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
	if (is_fmna_event(aeh)) {
		struct fmna_event *event = cast_fmna_event(aeh);

		switch (event->id) {
		case FMNA_EVENT_BONDED:
		case FMNA_EVENT_PAIRING_COMPLETED:
			pairing_completed_handle(event->conn);
			break;
		default:
			break;
		}

		return false;
	}

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
APP_EVENT_SUBSCRIBE(fmna_serial_number, fmna_event);
APP_EVENT_SUBSCRIBE(fmna_serial_number, fmna_owner_event);
