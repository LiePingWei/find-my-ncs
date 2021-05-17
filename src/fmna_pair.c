/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "fmna_keys.h"
#include "fmna_gatt_fmns.h"
#include "fmna_product_plan.h"
#include "fmna_serial_number.h"
#include "fmna_storage.h"
#include "fmna_version.h"
#include "crypto/fm_crypto.h"
#include "events/fmna_pair_event.h"
#include "events/fmna_event.h"

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

/* Parameter length definitions. */
#define C1_BLEN 32
#define C2_BLEN 89
#define C3_BLEN 60

#define E1_BLEN 113
#define E2_BLEN 1326
#define E3_BLEN 1040
#define E4_BLEN 1286

#define H1_BLEN 32

#define S2_BLEN 100

#define SESSION_NONCE_BLEN        32
#define SEEDS_BLEN                32

/* FMN pairing command and response descriptors. */
struct __packed fmna_initiate_pairing {
	uint8_t session_nonce[SESSION_NONCE_BLEN];
	uint8_t e1[E1_BLEN];
};

struct __packed fmna_send_pairing_data {
	uint8_t c1[C1_BLEN];
	uint8_t e2[E2_BLEN];
};

struct __packed fmna_finalize_pairing {
	uint8_t c2[C2_BLEN];
	uint8_t e3[E3_BLEN];
	uint8_t seeds[SEEDS_BLEN];
	uint8_t icloud_id[FMNA_ICLOUD_ID_LEN];
	uint8_t s2[S2_BLEN];
};

struct __packed fmna_send_pairing_status {
	uint8_t  c3[C3_BLEN];
	uint32_t status;
	uint8_t  e4[E4_BLEN];
};

struct __packed e2_encr_msg {
	uint8_t  session_nonce[SESSION_NONCE_BLEN];
	char     software_auth_token[FMNA_SW_AUTH_TOKEN_BLEN];
	uint8_t  software_auth_uuid[FMNA_SW_AUTH_UUID_BLEN];
	uint8_t  serial_number[FMNA_SERIAL_NUMBER_BLEN];
	uint8_t  product_data[FMNA_PP_PRODUCT_DATA_LEN];
	uint32_t fw_version;
	uint8_t  e1[E1_BLEN];
	uint8_t  seedk1[FMNA_SYMMETRIC_KEY_LEN];
};

struct __packed e4_encr_msg {
	uint8_t  software_auth_uuid[FMNA_SW_AUTH_UUID_BLEN];
	uint8_t  serial_number[FMNA_SERIAL_NUMBER_BLEN];
	uint8_t  session_nonce[SESSION_NONCE_BLEN];
	uint8_t  e1[E1_BLEN];
	uint8_t  latest_sw_token[FMNA_SW_AUTH_TOKEN_BLEN];
	uint32_t status;
};

struct __packed s2_verif_msg {
	uint8_t  software_auth_uuid[FMNA_SW_AUTH_UUID_BLEN];
	uint8_t  session_nonce[SESSION_NONCE_BLEN];
	uint8_t  seeds[SEEDS_BLEN];
	uint8_t  h1[H1_BLEN];
	uint8_t  e1[E1_BLEN];
	uint8_t  e3[E3_BLEN];
};

/* Crypto state variables. */
static uint8_t session_nonce[SESSION_NONCE_BLEN];
static uint8_t e1[E1_BLEN];
static uint8_t seedk1[FMNA_SYMMETRIC_KEY_LEN];
static struct fm_crypto_ckg_context ckg_ctx;

int fmna_pair_init(void)
{
	int err;

	err = fm_crypto_ckg_init(&ckg_ctx);
	if (err) {
		LOG_ERR("fm_crypto_ckg_init returned error: %d", err);
	}

	return err;
}

static void pairing_peer_disconnect(struct bt_conn *conn)
{
	int err;

	err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if (err) {
		LOG_ERR("bt_conn_disconnect returned error: %d", err);
	}
}

static int e2_msg_populate(struct fmna_initiate_pairing *init_pairing,
			   struct e2_encr_msg *e2_encr_msg)
{
	int err;
	struct fmna_version ver;

	memcpy(e2_encr_msg->session_nonce,
	       init_pairing->session_nonce,
	       sizeof(e2_encr_msg->session_nonce));

	err = fmna_storage_uuid_load(e2_encr_msg->software_auth_uuid);
	if (err) {
		return err;
	}

	err = fmna_storage_auth_token_load(e2_encr_msg->software_auth_token);
	if (err) {
		return err;
	}

	fmna_serial_number_get(e2_encr_msg->serial_number);

	memcpy(e2_encr_msg->e1, init_pairing->e1, sizeof(e2_encr_msg->e1));

	memcpy(e2_encr_msg->seedk1, seedk1, sizeof(e2_encr_msg->seedk1));

	err = fmna_version_fw_get(&ver);
	if (err) {
		LOG_ERR("FMNA Pair: Firmware Version read failed");
		memset(&ver, 0, sizeof(ver));
	}

	e2_encr_msg->fw_version = FMNA_VERSION_ENCODE(ver);

	memcpy(e2_encr_msg->product_data, fmna_pp_product_data, sizeof(e2_encr_msg->product_data));

	return err;
}

static int e4_msg_populate(struct e4_encr_msg *e4_encr_msg)
{
	int err;

	memcpy(e4_encr_msg->session_nonce,
	       session_nonce,
	       sizeof(e4_encr_msg->session_nonce));

	err = fmna_storage_uuid_load(e4_encr_msg->software_auth_uuid);
	if (err) {
		return err;
	}

	fmna_serial_number_get(e4_encr_msg->serial_number);

	memcpy(e4_encr_msg->e1, e1, sizeof(e4_encr_msg->e1));

	err = fmna_storage_auth_token_load(e4_encr_msg->latest_sw_token);
	if (err) {
		return err;
	}

	e4_encr_msg->status = 0;

	return err;
}

static int s2_verif_msg_populate(struct fmna_finalize_pairing *finalize_cmd,
				 struct s2_verif_msg *s2_verif_msg)
{
	int err;

	memcpy(s2_verif_msg->session_nonce,
	       session_nonce,
	       sizeof(s2_verif_msg->session_nonce));

	err = fmna_storage_uuid_load(s2_verif_msg->software_auth_uuid);
	if (err) {
		return err;
	}

	memcpy(s2_verif_msg->seeds,
	       finalize_cmd->seeds,
	       sizeof(s2_verif_msg->seeds));

	memcpy(s2_verif_msg->e1, e1, sizeof(s2_verif_msg->e1));
	memcpy(s2_verif_msg->e3, finalize_cmd->e3, sizeof(s2_verif_msg->e3));

	return fm_crypto_sha256(sizeof(finalize_cmd->c2),
				finalize_cmd->c2,
				s2_verif_msg->h1);
}

static int pairing_data_generate(struct net_buf_simple *buf)
{
	int err;
	uint8_t c1[C1_BLEN];
	uint8_t *e2;
	uint32_t e2_blen;
	struct e2_encr_msg e2_encr_msg = {0};
	struct fmna_initiate_pairing *initiate_cmd =
		(struct fmna_initiate_pairing *) buf->data;

	/* Store the command parameters that are required by the
	 * successive pairing operations.
	 */
	memcpy(session_nonce, initiate_cmd->session_nonce,
	       sizeof(session_nonce));
	memcpy(e1, initiate_cmd->e1, sizeof(e1));

	/* Generate C1, SeedK1, and E2. */
	err = fm_crypto_ckg_gen_c1(&ckg_ctx, c1);
	if (err) {
		LOG_ERR("fm_crypto_ckg_gen_c1 err %d", err);
		return err;
	}

	err = fm_crypto_generate_seedk1(seedk1);
	if (err) {
		LOG_ERR("fm_crypto_generate_seedk1 err %d", err);
		return err;
	}

	err = e2_msg_populate(initiate_cmd, &e2_encr_msg);
	if (err) {
		LOG_ERR("e2_msg_populate err %d", err);
		return err;
	}

	e2_blen = E2_BLEN;

	/* Prepare Send Pairing Data response */
	net_buf_simple_add_mem(buf, c1, sizeof(c1));
	e2 = net_buf_simple_add(buf, e2_blen);

	err = fm_crypto_encrypt_to_server(fmna_pp_server_encryption_key,
					  sizeof(e2_encr_msg),
					  (const uint8_t *) &e2_encr_msg,
					  &e2_blen,
					  e2);
	if (err) {
		LOG_ERR("fm_crypto_encrypt_to_server err %d", err);
		return err;
	}

	return err;
}

static int pairing_status_generate(struct net_buf_simple *buf)
{
	int err;
	uint8_t *status_data;
	uint32_t e3_decrypt_plaintext_blen;
	uint32_t e4_blen;
	uint8_t c2[C2_BLEN];
	uint8_t server_shared_secret[FMNA_SERVER_SHARED_SECRET_LEN];
	uint64_t sn_query_count = 0;
	union {
		struct e4_encr_msg  e4_encr;
		struct s2_verif_msg s2_verif;
	} msg = {0};
	struct fmna_finalize_pairing *finalize_cmd =
		(struct fmna_finalize_pairing *) buf->data;

	/* Derive the Shared Secret. */
	err = fm_crypto_derive_server_shared_secret(finalize_cmd->seeds,
						    seedk1,
						    server_shared_secret);
	if (err) {
		LOG_ERR("fm_crypto_derive_server_shared_secret err %d", err);
		return err;
	}

	/* Validate S2 */
	err = s2_verif_msg_populate(finalize_cmd, &msg.s2_verif);
	if (err) {
		LOG_ERR("s2_verif_msg_populate err %d", err);
		return err;
	}

	err = fm_crypto_verify_s2(fmna_pp_server_sig_verification_key,
				  sizeof(finalize_cmd->s2),
				  finalize_cmd->s2,
				  sizeof(msg.s2_verif),
				  (const uint8_t *) &msg.s2_verif);
	if (err) {
		LOG_ERR("fm_crypto_verify_s2 err %d", err);
		return err;
	}

	/* Use E4 encryption message descriptor to store the new token. */
	memset(msg.e4_encr.latest_sw_token, 0,
	       sizeof(msg.e4_encr.latest_sw_token));

	/* Decrypt E3 message. */
	e3_decrypt_plaintext_blen = sizeof(msg.e4_encr.latest_sw_token);
	err = fm_crypto_decrypt_e3((const uint8_t *) server_shared_secret,
				   sizeof(finalize_cmd->e3),
				   (const uint8_t *) finalize_cmd->e3,
				   &e3_decrypt_plaintext_blen,
				   msg.e4_encr.latest_sw_token);
	if (err) {
		LOG_ERR("fm_crypto_decrypt_e3 err %d", err);
		return err;
	}

	/* Update the SW Authentication Token in the storage module. */
	err = fmna_storage_auth_token_update(msg.e4_encr.latest_sw_token);
	if (err) {
		LOG_ERR("fmna_storage_auth_token_update err %d", err);
		return err;
	}

	/* Store pairing items that are available at this point. The second and final
	 * set of pairing information is stored at the very end of the FMN pairing
	 * process by the keys module.
	 */
	err = fmna_storage_pairing_item_store(FMNA_STORAGE_SERVER_SHARED_SECRET_ID,
					      server_shared_secret,
					      sizeof(server_shared_secret));
	if (err) {
		LOG_ERR("fmna_pair: cannot store Server Shared Secret");
		return err;
	}

	err = fmna_storage_pairing_item_store(FMNA_STORAGE_SN_QUERY_COUNTER_ID,
					      (uint8_t *) &sn_query_count,
					      sizeof(sn_query_count));
	if (err) {
		LOG_ERR("fmna_pair: cannot store Serial Nubmer query counter");
		return err;
	}

	err = fmna_storage_pairing_item_store(FMNA_STORAGE_ICLOUD_ID_ID,
					      finalize_cmd->icloud_id,
					      sizeof(finalize_cmd->icloud_id));
	if (err) {
		LOG_ERR("fmna_pair: cannot store iCloud ID");
		return err;
	}

	/* Prepare Send Pairing Status response: C3, status and E4 */
	memcpy(c2, finalize_cmd->c2, sizeof(c2));

	status_data = net_buf_simple_add(buf, C3_BLEN);
	err = fm_crypto_ckg_gen_c3(&ckg_ctx, c2, status_data);
	if (err) {
		LOG_ERR("fm_crypto_ckg_gen_c3 err %d", err);
		return err;
	}

	status_data = net_buf_simple_add(buf, sizeof(uint32_t));
	memset(status_data, 0, sizeof(uint32_t));

	err = e4_msg_populate(&msg.e4_encr);
	if (err) {
		LOG_ERR("e4_msg_populate err %d", err);
		return err;
	}

	e4_blen = E4_BLEN;
	status_data = net_buf_simple_add(buf, e4_blen);
	err = fm_crypto_encrypt_to_server(fmna_pp_server_encryption_key,
					  sizeof(msg.e4_encr),
					  (const uint8_t *) &msg.e4_encr,
					  &e4_blen,
					  status_data);
	if (err) {
		LOG_ERR("fm_crypto_encrypt_to_server err %d", err);
		return err;
	}

	return err;
}

static void initiate_pairing_cmd_handle(struct bt_conn *conn,
					struct fmna_pair_buf *buf)
{
	int err;
	struct net_buf_simple buf_desc;

	LOG_INF("FMNA: RX: Initiate pairing command");

	/* Initialize buffer descriptor */
	net_buf_simple_init_with_data(&buf_desc, buf->data, sizeof(buf->data));
	net_buf_simple_reset(&buf_desc);

	err = pairing_data_generate(&buf_desc);
	if (err) {
		LOG_ERR("pairing_data_generate returned error: %d",
			err);

		pairing_peer_disconnect(conn);
		return;
	}

	err = fmna_gatt_pairing_cp_indicate(conn, FMNA_GATT_PAIRING_DATA_IND, &buf_desc);
	if (err) {
		LOG_ERR("fmns_pairing_data_indicate returned error: %d", err);
	}
}

static void finalize_pairing_cmd_handle(struct bt_conn *conn,
					struct fmna_pair_buf *buf)
{
	int err;
	struct net_buf_simple buf_desc;

	LOG_INF("FMNA: RX: Finalize pairing command");

	/* Initialize buffer descriptor */
	net_buf_simple_init_with_data(&buf_desc, buf->data, sizeof(buf->data));
	net_buf_simple_reset(&buf_desc);

	err = pairing_status_generate(&buf_desc);
	if (err) {
		LOG_ERR("pairing_status_generate returned error: %d",
			err);

		pairing_peer_disconnect(conn);
		return;
	}

	err = fmna_gatt_pairing_cp_indicate(conn, FMNA_GATT_PAIRING_STATUS_IND, &buf_desc);
	if (err) {
		LOG_ERR("fmns_pairing_status_indicate returned error: %d",
			err);
	}
}

static void pairing_complete_cmd_handle(struct bt_conn *conn,
					struct fmna_pair_buf *buf)
{
	int err;
	struct fmna_keys_init init_keys = {0};

	LOG_INF("FMNA: RX: Pairing complete command");

	err = fm_crypto_ckg_finish(&ckg_ctx,
					init_keys.master_pk,
					init_keys.primary_sk,
					init_keys.secondary_sk);
	if (err) {
		LOG_ERR("fm_crypto_ckg_finish: %d", err);
	}

	fm_crypto_ckg_free(&ckg_ctx);

	/* Reinitialize context for future pairing attempts. */
	err = fm_crypto_ckg_init(&ckg_ctx);
	if (err) {
		LOG_ERR("fm_crypto_ckg_init returned error: %d", err);
	}

	err = fmna_keys_service_start(&init_keys);
	if (err) {
		LOG_ERR("fmna_keys_service_start: %d", err);
	}

	FMNA_EVENT_CREATE(event, FMNA_EVENT_PAIRING_COMPLETED, conn);
	EVENT_SUBMIT(event);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_fmna_pair_event(eh)) {
		struct fmna_pair_event *event = cast_fmna_pair_event(eh);

		switch (event->id) {
		case FMNA_PAIR_EVENT_INITIATE_PAIRING:
			initiate_pairing_cmd_handle(event->conn, &event->buf);
			break;
		case FMNA_PAIR_EVENT_FINALIZE_PAIRING:
			finalize_pairing_cmd_handle(event->conn, &event->buf);
			break;
		case FMNA_PAIR_EVENT_PAIRING_COMPLETE:
			pairing_complete_cmd_handle(event->conn,&event->buf);
			break;
		default:
			LOG_ERR("FMNA: unexpected pairing command opcode: 0x%02X",
				event->id);
		}

		return false;
	}

	return false;
}

EVENT_LISTENER(fmna_pair, event_handler);
EVENT_SUBSCRIBE(fmna_pair, fmna_pair_event);
