/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "fmna_adv.h"
#include "fmna_battery.h"
#include "fmna_product_plan.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/hci_vs.h>
#include <sys/byteorder.h>
#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

#define ADV_TX_POWER 4 /* 4dBm */

#define UNPAIRED_ADV_INTERVAL    0x0030 /* 30 ms */
#define PAIRED_ADV_INTERVAL      0x0C80 /* 2 s */
#define PAIRED_ADV_INTERVAL_FAST 0x0030 /* 30 ms */

/* Unpaired advertiser timeout 10 min in [N * 10 ms]:
 * 10 * 60 * 100 * 10ms = 10 * 60 s = 10 min
 */
#define UNPAIRED_ADV_TIMEOUT (10*60*100)

#define BT_ADDR_LEN sizeof(((bt_addr_t *) NULL)->val)

#define FMN_SVC_PAYLOAD_UUID             0xFD44
#define FMN_SVC_PAYLOAD_ACC_CATEGORY_LEN 8
#define FMN_SVC_PAYLOAD_RESERVED_LEN     4

#define PAIRED_ADV_APPLE_ID                     0x004C
#define PAIRED_ADV_PAYLOAD_TYPE                 0x12
#define PAIRED_ADV_STATUS_MAINTAINED_BIT_POS    2
#define PAIRED_ADV_STATUS_FIXED_BIT_POS         5
#define PAIRED_ADV_STATUS_BATTERY_STATE_BIT_POS 6
#define PAIRED_ADV_STATUS_BATTERY_STATE_MASK    (BIT(6) & BIT(7))
#define PAIRED_ADV_OPT_ADDR_TYPE_BIT_POS        6
#define PAIRED_ADV_OPT_ADDR_TYPE_MASK           (0x03 << PAIRED_ADV_OPT_ADDR_TYPE_BIT_POS)

#define SEPARATED_ADV_REM_PUBKEY_LEN (FMNA_PUBLIC_KEY_LEN - BT_ADDR_LEN)
#define SEPARATED_ADV_HINT_INDEX      5


struct __packed unpaired_adv_payload {
	uint16_t uuid;
	uint8_t product_data[FMNA_PP_PRODUCT_DATA_LEN];
	uint8_t acc_category[FMN_SVC_PAYLOAD_ACC_CATEGORY_LEN];
	uint8_t reserved[FMN_SVC_PAYLOAD_RESERVED_LEN];
	uint8_t battery_state;
};

struct __packed paired_adv_payload_header {
	uint16_t apple_id;
	uint8_t type;
	uint8_t len;
};

struct __packed nearby_adv_payload {
	struct paired_adv_payload_header hdr;
	uint8_t status;
	uint8_t opt;
};

struct __packed separated_adv_payload {
	struct paired_adv_payload_header hdr;
	uint8_t status;
	uint8_t rem_pubkey[SEPARATED_ADV_REM_PUBKEY_LEN];
	uint8_t opt;
	uint8_t hint;
};

union adv_payload {
	struct unpaired_adv_payload unpaired;
	struct nearby_adv_payload nearby;
	struct separated_adv_payload separated;
};

struct adv_start_config {
	struct {
		const struct bt_data *payload;
		size_t len;
	} ad;
	const struct bt_le_ext_adv_cb *cb;

	uint32_t interval;
	uint16_t timeout;
};

static uint8_t bt_id;
static union adv_payload adv_payload;
static struct bt_le_ext_adv *adv_set = NULL;
static bool pending_timeout;
static fmna_adv_timeout_cb_t unpaired_adv_timeout_cb;

static void ext_adv_connected(struct bt_le_ext_adv *adv,
			      struct bt_le_ext_adv_connected_info *info)
{
	int err;
	struct bt_conn_info conn_info;

	err = bt_conn_get_info(info->conn, &conn_info);
	if (err) {
		LOG_ERR("bt_conn_get_info returned error: %d", err);
		return;
	}

	/* Indicate to the sent callback that the advertising completed
	 * because of a new connection.
	 */
	pending_timeout = false;

	LOG_INF("Connected with the following local identity: %d", conn_info.id);
}

static const struct bt_le_ext_adv_cb ext_adv_callbacks = {
	.connected = ext_adv_connected,
};

void unpaired_adv_sent(struct bt_le_ext_adv *adv,
		       struct bt_le_ext_adv_sent_info *info)
{
	__ASSERT(unpaired_adv_timeout_cb,
		 "The unpaired_adv_timeout_cb callback is not populated. ");

	if (!pending_timeout) {
		return;
	}
	pending_timeout = false;

	LOG_DBG("Unpaired advertising timeout");

	if (unpaired_adv_timeout_cb) {
		unpaired_adv_timeout_cb();
	}
}

static const struct bt_le_ext_adv_cb unpaired_adv_callbacks = {
	.sent = unpaired_adv_sent,
	.connected = ext_adv_connected,
};

static void bt_ext_advertising_tx_power_set(uint16_t handle)
{
	int err;
	struct bt_hci_cp_vs_write_tx_power_level *cp;
	struct bt_hci_rp_vs_write_tx_power_level *rp;
	struct net_buf *buf;
	struct net_buf *rsp = NULL;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, sizeof(*cp));
	if (!buf) {
		LOG_ERR("fmna_adv: cannot allocate buffer to set TX power");
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);
	cp->handle_type = BT_HCI_VS_LL_HANDLE_TYPE_ADV;
	cp->tx_power_level = ADV_TX_POWER;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, buf, &rsp);
	if (err) {
		LOG_ERR("fmna_adv: cannot set TX power (err: %d)", err);
		return;
	}

	rp = (struct bt_hci_rp_vs_write_tx_power_level *) rsp->data;
	LOG_DBG("Advertising TX power set to %d dBm", rp->selected_tx_power);

	net_buf_unref(rsp);
}

static int bt_ext_advertising_stop(void)
{
	int err;

	if (adv_set) {
		err = bt_le_ext_adv_stop(adv_set);
		if (err) {
			LOG_ERR("bt_le_ext_adv_stop returned error: %d", err);
			return err;
		}

		err = bt_le_ext_adv_delete(adv_set);
		if (err) {
			LOG_ERR("bt_le_ext_adv_delete returned error: %d", err);
			return err;
		}

		adv_set = NULL;
	} else {
		LOG_WRN("Trying to stop advertising without storing the advertising set");
	}

	return 0;
}

static int bt_ext_advertising_start(const struct adv_start_config *config)
{
	int err;
	struct bt_le_adv_param param = {0};
	struct bt_le_ext_adv_start_param ext_adv_start_param = {0};
	uint16_t adv_handle;

	if (adv_set) {
		LOG_ERR("Advertising set is already claimed");
		return -EAGAIN;
	}

	param.id = bt_id;
	param.options = BT_LE_ADV_OPT_CONNECTABLE;
	param.interval_min = config->interval;
	param.interval_max = config->interval;

	ext_adv_start_param.timeout = config->timeout;

	err = bt_le_ext_adv_create(&param, config->cb, &adv_set);
	if (err) {
		LOG_ERR("bt_le_ext_adv_create returned error: %d", err);
		return err;
	}

	err = bt_le_ext_adv_set_data(adv_set, config->ad.payload, config->ad.len, NULL, 0);
	if (err) {
		LOG_ERR("bt_le_ext_adv_set_data returned error: %d", err);
		return err;
	}

	/* TODO: Migrate to the proper BLE API for retrieving advertising handle once
	 *       it is added and implemented.
	 */
	adv_handle = bt_le_ext_adv_get_index(adv_set);
	bt_ext_advertising_tx_power_set(adv_handle);

	err = bt_le_ext_adv_start(adv_set, &ext_adv_start_param);
	if (err) {
		LOG_ERR("bt_le_ext_adv_start returned error: %d", err);
		return err;
	}

	return err;
}

static int id_addr_reconfigure(bt_addr_le_t *addr)
{
	int ret;
	char addr_str[BT_ADDR_LE_STR_LEN];

	ret = bt_id_reset(bt_id, addr, NULL);
	if (ret == -EALREADY) {
		/* Still using the same Public Key as an advertising address. */
		return 0;
	} else if (ret < 0) {
		LOG_ERR("bt_id_reset returned error: %d", ret);
		return ret;
	}

	if (addr) {
		bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
		LOG_INF("FMN identity address reconfigured to: %s",
			addr_str);
	}

	return 0;
}

static void unpaired_adv_payload_encode(struct unpaired_adv_payload *svc_payload)
{
	memset(svc_payload, 0, sizeof(*svc_payload));

	sys_put_le16(FMN_SVC_PAYLOAD_UUID, (uint8_t *) &svc_payload->uuid);

	memcpy(svc_payload->product_data, fmna_pp_product_data, sizeof(svc_payload->product_data));

	svc_payload->acc_category[0] = CONFIG_FMNA_CATEGORY;

	svc_payload->battery_state = fmna_battery_state_get();
}

int fmna_adv_start_unpaired(bool change_address)
{
	static const struct bt_data unpaired_ad[] = {
		BT_DATA(BT_DATA_SVC_DATA16, (uint8_t *) &adv_payload,
			sizeof(struct unpaired_adv_payload)),
	};

	int err;
	struct adv_start_config start_config = {0};

	/* Stop any ongoing advertising. */
	err = bt_ext_advertising_stop();
	if (err) {
		LOG_ERR("bt_ext_advertising_stop returned error: %d", err);
		return err;
	}

	/* Change the identity address if requested. */
	if (change_address) {
		bt_addr_le_t addr;

		bt_addr_le_copy(&addr, BT_ADDR_LE_ANY);

		err = id_addr_reconfigure(&addr);
		if (err) {
			LOG_ERR("id_addr_reconfigure returned error: %d", err);
			return err;
		}
	}

	/* Encode the FMN Service payload for advertising data set. */
	unpaired_adv_payload_encode(&adv_payload.unpaired);

	start_config.ad.payload = unpaired_ad;
	start_config.ad.len = ARRAY_SIZE(unpaired_ad);
	start_config.cb = &unpaired_adv_callbacks;
	start_config.interval = UNPAIRED_ADV_INTERVAL;
	start_config.timeout = UNPAIRED_ADV_TIMEOUT;
	err = bt_ext_advertising_start(&start_config);
	if (err) {
		LOG_ERR("bt_ext_advertising_start returned error: %d", err);
		return err;
	}

	/* Indicate to the sent callback that the advertising completed
	 * because of a timeout.
	 */
	pending_timeout = true;

	LOG_INF("FMN advertising started for the Unpaired state");

	return err;
}

static void paired_addr_encode(bt_addr_le_t *addr, const uint8_t pubkey[FMNA_PUBLIC_KEY_LEN])
{
	addr->type = BT_ADDR_LE_RANDOM;
	sys_memcpy_swap(addr->a.val, pubkey, sizeof(addr->a.val));
	BT_ADDR_SET_STATIC(&addr->a);
}

static void paired_adv_header_encode(struct paired_adv_payload_header *hdr,
				     size_t payload_len)
{
	sys_put_le16(PAIRED_ADV_APPLE_ID, (uint8_t *) &hdr->apple_id);
	hdr->type = PAIRED_ADV_PAYLOAD_TYPE;
	hdr->len = (payload_len - sizeof(*hdr));
}

static void nearby_adv_payload_encode(struct nearby_adv_payload *adv_payload,
				      const struct fmna_adv_nearby_config *config)
{
	uint8_t opt;
	enum fmna_battery_state battery_state;

	memset(adv_payload, 0, sizeof(*adv_payload));

	paired_adv_header_encode(&adv_payload->hdr, sizeof(*adv_payload));

	battery_state = fmna_battery_state_get();

	if (config->is_maintained) {
		adv_payload->status |= BIT(PAIRED_ADV_STATUS_MAINTAINED_BIT_POS);
	}
	adv_payload->status |= BIT(PAIRED_ADV_STATUS_FIXED_BIT_POS);
	adv_payload->status |= (battery_state << PAIRED_ADV_STATUS_BATTERY_STATE_BIT_POS) &
		PAIRED_ADV_STATUS_BATTERY_STATE_MASK;

	opt = (config->primary_key[0] & PAIRED_ADV_OPT_ADDR_TYPE_MASK);
	opt = (opt >> PAIRED_ADV_OPT_ADDR_TYPE_BIT_POS);
	adv_payload->opt = opt;
}

int fmna_adv_start_nearby(const struct fmna_adv_nearby_config *config)
{
	static const struct bt_data nearby_ad[] = {
		BT_DATA(BT_DATA_MANUFACTURER_DATA, (uint8_t *) &adv_payload,
			sizeof(struct nearby_adv_payload)),
	};

	int err;
	bt_addr_le_t addr;
	struct adv_start_config start_config = {0};

	/* Stop any ongoing advertising. */
	err = bt_ext_advertising_stop();
	if (err) {
		LOG_ERR("bt_ext_advertising_stop returned error: %d", err);
		return err;
	}

	nearby_adv_payload_encode(&adv_payload.nearby, config);

	/*
	 * Reconfigure the BT address after coming back from the Separated
	 * state. Each address reconfiguration changes the BLE identity and
	 * removes BLE bonds.
	 */
	paired_addr_encode(&addr, config->primary_key);
	err = id_addr_reconfigure(&addr);
	if (err) {
		LOG_ERR("id_addr_reconfigure returned error: %d", err);
		return err;
	}

	start_config.ad.payload = nearby_ad;
	start_config.ad.len = ARRAY_SIZE(nearby_ad);
	start_config.cb = &ext_adv_callbacks;
	start_config.interval = config->fast_mode ?
		PAIRED_ADV_INTERVAL_FAST : PAIRED_ADV_INTERVAL;
	err = bt_ext_advertising_start(&start_config);
	if (err) {
		LOG_ERR("bt_ext_advertising_start returned error: %d", err);
		return err;
	}

	LOG_INF("FMN advertising started for the Nearby state");

	return 0;
}

static void separated_adv_payload_encode(struct separated_adv_payload *adv_payload,
					 const struct fmna_adv_separated_config *config)
{
	uint8_t opt;
	enum fmna_battery_state battery_state;

	memset(adv_payload, 0, sizeof(*adv_payload));

	paired_adv_header_encode(&adv_payload->hdr, sizeof(*adv_payload));

	battery_state = fmna_battery_state_get();

	if (config->is_maintained) {
		adv_payload->status |= BIT(PAIRED_ADV_STATUS_MAINTAINED_BIT_POS);
	}
	adv_payload->status |= BIT(PAIRED_ADV_STATUS_FIXED_BIT_POS);
	adv_payload->status |= (battery_state << PAIRED_ADV_STATUS_BATTERY_STATE_BIT_POS) &
		PAIRED_ADV_STATUS_BATTERY_STATE_MASK;

	memcpy(adv_payload->rem_pubkey,
	       config->separated_key + BT_ADDR_LEN,
	       sizeof(adv_payload->rem_pubkey));

	opt = (config->separated_key[0] & PAIRED_ADV_OPT_ADDR_TYPE_MASK);
	opt = (opt >> PAIRED_ADV_OPT_ADDR_TYPE_BIT_POS);
	adv_payload->opt = opt;

	adv_payload->hint = config->primary_key[SEPARATED_ADV_HINT_INDEX];
}

int fmna_adv_start_separated(const struct fmna_adv_separated_config *config)
{
	static const struct bt_data separated_ad[] = {
		BT_DATA(BT_DATA_MANUFACTURER_DATA, (uint8_t *) &adv_payload,
			sizeof(struct separated_adv_payload)),
	};

	int err;
	bt_addr_le_t addr;
	struct adv_start_config start_config = {0};

	/* Stop any ongoing advertising. */
	err = bt_ext_advertising_stop();
	if (err) {
		LOG_ERR("bt_ext_advertising_stop returned error: %d", err);
		return err;
	}

	separated_adv_payload_encode(&adv_payload.separated, config);

	/*
	 * Reconfigure the BT address after coming back from the Separated
	 * state. Each address reconfiguration changes the BLE identity and
	 * removes BLE bonds.
	 */
	paired_addr_encode(&addr, config->separated_key);
	err = id_addr_reconfigure(&addr);
	if (err) {
		LOG_ERR("id_addr_reconfigure returned error: %d", err);
		return err;
	}

	start_config.ad.payload = separated_ad;
	start_config.ad.len = ARRAY_SIZE(separated_ad);
	start_config.cb = &ext_adv_callbacks;
	start_config.interval = config->fast_mode ?
		PAIRED_ADV_INTERVAL_FAST : PAIRED_ADV_INTERVAL;
	err = bt_ext_advertising_start(&start_config);
	if (err) {
		LOG_ERR("bt_ext_advertising_start returned error: %d", err);
		return err;
	}

	LOG_INF("FMN advertising started for the Separated state");

	return 0;
}

int fmna_adv_unpaired_cb_register(fmna_adv_timeout_cb_t cb)
{
	if (!cb) {
		return -EINVAL;
	}

	unpaired_adv_timeout_cb = cb;

	return 0;
}

int fmna_adv_init(uint8_t id)
{
	if (id == BT_ID_DEFAULT) {
		LOG_ERR("The default identity cannot be used for FMN");
		return -EINVAL;
	}

	bt_id = id;

	id = bt_id_reset(bt_id, NULL, NULL);
	if (id != bt_id) {
		LOG_ERR("FMN identity cannot be found: %d", bt_id);
		return id;
	}

	return 0;
}
