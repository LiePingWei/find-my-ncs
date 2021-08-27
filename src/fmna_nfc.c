/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "events/fmna_event.h"

#include "fmna_battery.h"
#include "fmna_nfc.h"
#include "fmna_product_plan.h"
#include "fmna_serial_number.h"
#include "fmna_state.h"
#include "fmna_version.h"

#include <bluetooth/bluetooth.h>

#include <nfc_t2t_lib.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/uri_msg.h>

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

#define NDEF_MSG_BUF_SIZE	512
#define FMNA_URL_MAX_SIZE	512

/* Every byte requires two character encoding in ASCII.
 * One additional character for NULL terminator.
 */
#define BT_ADDR_STRING_LEN             (6*2 + 1)
#define PRODUCT_DATA_STRING_LEN        (FMNA_PP_PRODUCT_DATA_LEN * 2 + 1)
#define FMNA_SERIAL_NUMBER_ENC_STR_LEN (2 * FMNA_SERIAL_NUMBER_ENC_BLEN + 1)

static const char *base_url = "found.apple.com/accessory?pid=%s&b=%02x&fv=%08x";
static const char *unpaired_url_suffix = "&bt=%s&sr=%s";
static const char *paired_url_suffix = "&e=%s&op=tap";

static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];
static uint8_t bt_id;
static uint8_t battery_state;
static bool paired_state;
static bool is_initialized;

static void nfc_callback(void *context,
			 enum nfc_t2t_event event,
			 const uint8_t *data,
			 size_t data_length)
{
	/* Implementation not needed */
}

static int fmna_nfc_url_prepare(char *url, size_t url_max_size)
{
	int ret;
	size_t base_url_len;
	size_t suffix_max_size;
	struct fmna_version ver;
	uint32_t fw_version_le;
	char product_plan_str[PRODUCT_DATA_STRING_LEN];

	/* Convert Product Data binary array to a string. */
	ret = snprintk(
		product_plan_str,
		sizeof(product_plan_str),
		"%02x%02x%02x%02x%02x%02x%02x%02x",
		fmna_pp_product_data[0], fmna_pp_product_data[1],
		fmna_pp_product_data[2], fmna_pp_product_data[3],
		fmna_pp_product_data[4], fmna_pp_product_data[5],
		fmna_pp_product_data[6], fmna_pp_product_data[7]);
	if (!((ret > 0) && (ret < sizeof(product_plan_str)))) {
		LOG_ERR("FMN NFC: snprintk product_plan_str err %d", ret);
		return -EINVAL;
	}

	/* Get the firmware version and encode it in little endian. */
	ret = fmna_version_fw_get(&ver);
	if (ret) {
		LOG_ERR("FMN NFC: Firmware Version read failed: %d", ret);
		memset(&ver, 0, sizeof(ver));
	}
	fw_version_le = (ver.revision << 24) | (ver.minor << 16) |
		(ver.major << 8 & 0xFF00) | (ver.major >> 8 & 0x00FF);

	/* Encode common values in the URL. */
	ret = snprintk((char *) url, url_max_size, base_url,
		product_plan_str, battery_state, fw_version_le);
	if (!((ret > 0) && (ret < url_max_size))) {
		LOG_ERR("FMN NFC: snprintk base url err %d", ret);
		return -EINVAL;
	}

	base_url_len = strlen(url);
	suffix_max_size = url_max_size - base_url_len;

	if (paired_state) {
		uint8_t serial_number_enc[FMNA_SERIAL_NUMBER_ENC_BLEN];
		char serial_number_enc_str[FMNA_SERIAL_NUMBER_ENC_STR_LEN] = {0};

		fmna_serial_number_enc_get(
			FMNA_SERIAL_NUMBER_ENC_QUERY_TYPE_TAP,
			serial_number_enc);
		if (!bin2hex(
			serial_number_enc, sizeof(serial_number_enc),
			serial_number_enc_str, sizeof(serial_number_enc_str))) {
			LOG_ERR("FMN NFC: bin2hex error");
		}

		ret = snprintk(
			(char *) url + base_url_len,
			suffix_max_size,
			paired_url_suffix,
			serial_number_enc_str);
		if (!((ret > 0) && (ret < suffix_max_size))) {
			LOG_ERR("FMN NFC: snprintk paired_url_suffix err %d", ret);
			return -EINVAL;
		}
	} else {
		char addr_str[BT_ADDR_STRING_LEN];
		bt_addr_le_t *addr;
		bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
		size_t count = ARRAY_SIZE(addrs);
		uint8_t serial_number[FMNA_SERIAL_NUMBER_BLEN + 1] = {0};

		bt_id_get(addrs, &count);
		if (bt_id >= count) {
			return -EINVAL;
		}
		addr = &addrs[bt_id];
		ret = snprintk(
			addr_str,
			sizeof(addr_str),
			"%02x%02x%02x%02x%02x%02x",
			addr->a.val[5], addr->a.val[4], addr->a.val[3],
			addr->a.val[2], addr->a.val[1], addr->a.val[0]);
		if (!((ret > 0) && (ret < sizeof(addr_str)))) {
			LOG_ERR("FMN NFC: snprintk addr_str err %d", ret);
			return -EINVAL;
		}

		fmna_serial_number_get(serial_number);

		ret = snprintk(
			(char *) url + base_url_len,
			suffix_max_size,
			unpaired_url_suffix,
			addr_str,
			serial_number);
		if (!((ret > 0) && (ret < suffix_max_size))) {
			LOG_ERR("FMN NFC: snprintk unpaired_url_suffix err %d", ret);
			return -EINVAL;
		}
	}

	return 0;
}

static int fmna_nfc_buffer_setup(void)
{
	int err;
	char url[FMNA_URL_MAX_SIZE] = {0};
	uint32_t ndef_size = ARRAY_SIZE(ndef_msg_buf);

	err = fmna_nfc_url_prepare(url, sizeof(url));
	if (err) {
		LOG_ERR("fmna_nfc_url_prepare returned error: %d", err);
		return err;
	}

	err = nfc_ndef_uri_msg_encode(NFC_URI_HTTPS,
				      url,
				      strlen(url),
				      ndef_msg_buf,
				      &ndef_size);
	if (err) {
		LOG_ERR("nfc_ndef_uri_msg_encode returned error: %d", err);
		return err;
	}

	err = nfc_t2t_payload_set(ndef_msg_buf, ndef_size);
	if (err) {
		LOG_ERR("nfc_t2t_payload_set returned error: %d", err);
		return err;
	}

	err = nfc_t2t_emulation_start();
	if (err) {
		LOG_ERR("nfc_t2t_emulation_start returned error: %d", err);
		return err;
	}

	LOG_DBG("FMN NFC: updated the NDEF buffer with a new Find My URI");

	return 0;
}

static void fmna_nfc_buffer_update(void)
{
	int err;

	err = nfc_t2t_emulation_stop();
	if (err) {
		LOG_ERR("nfc_t2t_emulation_stop returned error: %d", err);
	}

	fmna_nfc_buffer_setup();
}

int fmna_nfc_init(uint8_t id)
{
	int err;

	bt_id = id;
	battery_state = fmna_battery_state_get_no_cb();
	paired_state = fmna_state_is_paired();

	err = nfc_t2t_setup(nfc_callback, NULL);
	if (err) {
		LOG_ERR("nfc_t2t_setup returned error: %d", err);
		return err;
	}

	err = fmna_nfc_buffer_setup();
	if (err) {
		LOG_ERR("fmna_nfc_buffer_setup returned error: %d", err);
		return err;
	}

	is_initialized = true;

	LOG_INF("FMN NFC: NFC capability is enabled");

	return 0;
}

static void battery_level_changed(void)
{
	uint8_t current_state;

	current_state = fmna_battery_state_get_no_cb();
	if (current_state != battery_state) {
		battery_state = current_state;

		fmna_nfc_buffer_update();
	}
}

static void state_changed(void)
{
	bool current_paired_state;

	current_paired_state = fmna_state_is_paired();
	if (current_paired_state != paired_state){
		paired_state = current_paired_state;

		fmna_nfc_buffer_update();
	}
}

static bool event_handler(const struct event_header *eh)
{
	if (!is_initialized) {
		return false;
	}

	if (is_fmna_event(eh)) {
		struct fmna_event *event = cast_fmna_event(eh);

		switch (event->id) {
		case FMNA_EVENT_BATTERY_LEVEL_CHANGED:
			battery_level_changed();
			break;
		case FMNA_EVENT_STATE_CHANGED:
			state_changed();
			break;
		default:
			break;
		}

		return false;
	}

	return false;
}

EVENT_LISTENER(fmna_nfc, event_handler);
EVENT_SUBSCRIBE(fmna_nfc, fmna_event);
