#include "fmna_keys.h"
#include "fmna_gatt_fmns.h"
#include "fmna_storage.h"
#include "crypto/fm_crypto.h"
#include "events/fmna_pair_event.h"
#include "events/fmna_event.h"

#include <sys/byteorder.h>
#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMN_ADK_LOG_LEVEL);

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

#define SERVER_SHARED_SECRET_BLEN 32
#define SESSION_NONCE_BLEN        32
#define SEEDS_BLEN                32
#define ICLOUD_IDENTIFIER_BLEN    60

#define SERIAL_NUMBER_RAW_BLEN   16

#define APPLE_SERVER_ENCRYPTION_KEY_BLEN       65
#define APPLE_SERVER_SIG_VERIFICATION_KEY_BLEN 65

#define PROD_DATA_MAX_LEN 8

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
	uint8_t icloud_id[ICLOUD_IDENTIFIER_BLEN];
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
	uint8_t  serial_number[SERIAL_NUMBER_RAW_BLEN];
	uint8_t  product_data[PROD_DATA_MAX_LEN];
	uint32_t fw_version;
	uint8_t  e1[E1_BLEN];
	uint8_t  seedk1[FMNA_SYMMETRIC_KEY_LEN];
};

struct __packed e4_encr_msg {
	uint8_t  software_auth_uuid[FMNA_SW_AUTH_UUID_BLEN];
	uint8_t  serial_number[SERIAL_NUMBER_RAW_BLEN];
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
static uint8_t server_shared_secret[SERVER_SHARED_SECRET_BLEN];
static struct fm_crypto_ckg_context ckg_ctx;

/* Serial number. */
static uint8_t serial_number[SERIAL_NUMBER_RAW_BLEN];

/* Server encryption and signature verification keys. */
static uint8_t q_e[APPLE_SERVER_ENCRYPTION_KEY_BLEN] = {0x04, 0x9c, 0xc5, 0xad, 0xdd, 0xd0, 0x29, 0xb7, 0x53, 0x5d, 0x30, 0xe6, 0xe5, 0xd1, 0x6d, 0xb7, 0xa8, 0xd2, 0x1b, 0x1b, 0x48, 0xb5, 0x5b, 0x19, 0xd5, 0xb1, 0x10, 0xe9, 0x5b, 0xf3, 0x15, 0x45, 0xe7, 0x74, 0xcf, 0x51, 0x8d, 0xeb, 0xbe, 0x3c, 0x71, 0x68, 0x33, 0xe4, 0x43, 0xf1, 0x14, 0x47, 0x6e, 0x5a, 0x4b, 0x05, 0x4e, 0x36, 0x75, 0x07, 0x05, 0x6e, 0x39, 0x95, 0xcc, 0x6b, 0x96, 0x90, 0x96};
static uint8_t q_a[APPLE_SERVER_SIG_VERIFICATION_KEY_BLEN] = {0x04, 0x33, 0x4c, 0x5a, 0x73, 0xfd, 0x61, 0xdf, 0x36, 0x43, 0x3f, 0xbc, 0x69, 0x92, 0x36, 0xe3, 0x98, 0xe4, 0x94, 0x12, 0xf3, 0xc0, 0xfd, 0xc4, 0xe5, 0xda, 0x0b, 0x41, 0x18, 0x77, 0x95, 0x17, 0x08, 0x71, 0x20, 0x88, 0x8e, 0x97, 0x92, 0x37, 0x76, 0xba, 0x48, 0xdc, 0x51, 0x7c, 0x0f, 0xa8, 0x7b, 0x9c, 0x62, 0xa9, 0xfe, 0xe9, 0x6b, 0x0f, 0x38, 0x40, 0x3f, 0x66, 0x9e, 0x1e, 0x67, 0x55, 0x60};

void fmna_product_data_get(uint64_t *product_data)
{
	BUILD_ASSERT(PROD_DATA_MAX_LEN == sizeof(*product_data));

	/* Swap due to the endianess */
	uint64_t product_data_temp = CONFIG_FMN_PRODUCT_DATA;
	sys_mem_swap(&product_data_temp, sizeof(product_data_temp));

	memcpy(product_data, &product_data_temp, sizeof(*product_data));
}

static inline char num_to_char(uint8_t nibble) {
	return (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
}

void fmna_serial_number_get(uint8_t *sn, uint8_t length) {
	uint8_t temp[8];
	uint16_t remaining = length;
	int i=0;

	/* XOR device ID and address to identify the device */
	*((uint32_t *)temp) =  NRF_FICR->DEVICEID[0];
	*((uint32_t *)temp) ^= NRF_FICR->DEVICEADDR[0];

	*((uint32_t *)(temp + 4)) =  NRF_FICR->DEVICEID[1];
	*((uint32_t *)(temp + 4)) ^= NRF_FICR->DEVICEADDR[1];

	/* Convert to a character string */
	for (; i < 8 && remaining; ++i) {
		sn[2 * i] = num_to_char((temp[i] & 0x0f));
		remaining--;
		if (remaining) {
			sn[2 * i + 1] = num_to_char(((temp[i] >> 4) & 0x0f));
			remaining--;
		}
	}

	/* Pad remaining with 'f' */
	if (remaining) {
		sn[i] = 'f';
		remaining--;
		i++;
	}
}

int fmna_pair_init(void)
{
	int err;

	err = fm_crypto_ckg_init(&ckg_ctx);
	if (err) {
		LOG_ERR("fm_crypto_ckg_init returned error: %d", err);
	}

	fmna_serial_number_get(serial_number, sizeof(serial_number));
	LOG_HEXDUMP_INF(serial_number, sizeof(serial_number),
			"Serial Number:");

	return err;
}

static int e2_msg_populate(struct fmna_initiate_pairing *init_pairing,
			   struct e2_encr_msg *e2_encr_msg)
{
	int err;

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

	memcpy(e2_encr_msg->serial_number,
	       serial_number,
	       sizeof(e2_encr_msg->serial_number));

	memcpy(e2_encr_msg->e1, init_pairing->e1, sizeof(e2_encr_msg->e1));

	memcpy(e2_encr_msg->seedk1, seedk1, sizeof(e2_encr_msg->seedk1));

	/* TODO: Get the real version number */
	e2_encr_msg->fw_version = (1 << 16) | (0 << 8) | 16;

	fmna_product_data_get((uint64_t *) e2_encr_msg->product_data);

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

	memcpy(e4_encr_msg->serial_number,
	       serial_number,
	       sizeof(e4_encr_msg->serial_number));

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

	err = fm_crypto_encrypt_to_server((const uint8_t *) q_e,
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

	err = fm_crypto_verify_s2(q_a,
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

	memcpy(c2, finalize_cmd->c2, sizeof(c2));

	/* Prepare Send Pairing Status response: C3, status and E4 */
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
	err = fm_crypto_encrypt_to_server((const uint8_t *) q_e,
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

	if (IS_ENABLED(CONFIG_FMN_HARDCODED_PAIRING)) {
		/* Use command buffer memory to store response data. */
		net_buf_simple_add(&buf_desc,
				   sizeof(struct fmna_send_pairing_data));
		memset(buf_desc.data, 0xFF, buf_desc.len);
	} else {
		err = pairing_data_generate(&buf_desc);
		if (err) {
			LOG_ERR("pairing_data_generate returned error: %d",
				err);

			/* TODO: Emit pairing failure event. */
			return;
		}
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

	if (IS_ENABLED(CONFIG_FMN_HARDCODED_PAIRING)) {
		/* Use command buffer memory to store response data. */
		net_buf_simple_add(&buf_desc,
				   sizeof(struct fmna_send_pairing_status));
		memset(buf_desc.data, 0xFF, buf_desc.len);
	} else {
		err = pairing_status_generate(&buf_desc);
		if (err) {
			LOG_ERR("pairing_status_generate returned error: %d",
				err);

			/* TODO: Emit pairing failure event. */
			return;
		}
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

	if (!IS_ENABLED(CONFIG_FMN_HARDCODED_PAIRING)) {
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
	}

	FMNA_EVENT_CREATE(event, FMNA_PAIRING_COMPLETED, conn);
	EVENT_SUBMIT(event);
}

static void pairing_cmd_handle(struct fmna_pair_event *pair_event)
{
	switch (pair_event->op) {
	case FMNA_INITIATE_PAIRING:
		initiate_pairing_cmd_handle(pair_event->conn,
					    &pair_event->buf);
		break;
	case FMNA_FINALIZE_PAIRING:
		finalize_pairing_cmd_handle(pair_event->conn,
					    &pair_event->buf);
		break;
	case FMNA_PAIRING_COMPLETE:
		pairing_complete_cmd_handle(pair_event->conn,
					    &pair_event->buf);
		break;
	default:
		LOG_ERR("FMNA: unexpected pairing command opcode: 0x%02X",
			pair_event->op);
	}
}

static bool event_handler(const struct event_header *eh)
{
	if (is_fmna_pair_event(eh)) {
		struct fmna_pair_event *event = cast_fmna_pair_event(eh);

		pairing_cmd_handle(event);

		return false;
	}

	return false;
}

EVENT_LISTENER(fmna_pair, event_handler);
EVENT_SUBSCRIBE(fmna_pair, fmna_pair_event);
