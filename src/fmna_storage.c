/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include "events/fmna_event.h"
#include "fmna_storage.h"

#include <string.h>
#include <stdlib.h>

#include <logging/log.h>
#include <settings/settings.h>
#include <sys/byteorder.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

#define FMNA_STORAGE_TREE "fmna"

#define FMNA_STORAGE_NODE_CONNECTOR "/"

#define FMNA_STORAGE_BRANCH_PROVISIONING "provisioning"
#define FMNA_STORAGE_BRANCH_PAIRING      "pairing"

#define FMNA_STORAGE_PROVISIONING_SERIAL_NUMBER_KEY 997
#define FMNA_STORAGE_PROVISIONING_UUID_KEY          998
#define FMNA_STORAGE_PROVISIONING_AUTH_TOKEN_KEY    999

#define FMNA_STORAGE_PAIRING_ITEM_KEY_DIGIT_LEN 2
#define FMNA_STORAGE_PAIRING_ITEM_KEY_FMT \
	"%0" STRINGIFY(FMNA_STORAGE_PAIRING_ITEM_KEY_DIGIT_LEN) "d"
#define FMNA_STORAGE_PAIRING_ITEM_KEY_LEN		\
	sizeof(FMNA_STORAGE_PAIRING_LEAF_NODE_FMT) -	\
	sizeof(FMNA_STORAGE_PAIRING_ITEM_KEY_FMT) +	\
	FMNA_STORAGE_PAIRING_ITEM_KEY_DIGIT_LEN +	\
	sizeof(char)
#define FMNA_STORAGE_PAIRING_LEAF_NODE_FMT 		\
	FMNA_STORAGE_LEAF_NODE_BUILD(			\
		FMNA_STORAGE_BRANCH_PAIRING,		\
		FMNA_STORAGE_PAIRING_ITEM_KEY_FMT)

#define FMNA_STORAGE_NODE_CONNECT(_child_node, ...)		\
	__VA_ARGS__ FMNA_STORAGE_NODE_CONNECTOR _child_node

#define FMNA_STORAGE_SUBTREE_BUILD(_branch) 			\
	FMNA_STORAGE_NODE_CONNECT(_branch, FMNA_STORAGE_TREE)

#define FMNA_STORAGE_LEAF_NODE_BUILD(_branch, _leaf_node)	\
	FMNA_STORAGE_NODE_CONNECT(				\
		_leaf_node,					\
		FMNA_STORAGE_SUBTREE_BUILD(_branch))


#define FMNA_STORAGE_PAIRING_ITEM_ID_NAME_ARRAY_DEF(name, value, len) \
	FMNA_STORAGE_PAIRING_ITEM_ID_NAME(name),
#define FMNA_STORAGE_PAIRING_ITEM_ID_NAME_STR_ARRAY_DEF(name, value, len) \
	[FMNA_STORAGE_PAIRING_ITEM_ID_NAME(name)] = STRINGIFY(name),

#define FMNA_STORAGE_PAIRING_ITEM_LEN_NAME(name) CONCAT(name, _LEN)
#define FMNA_STORAGE_PAIRING_ITEM_LEN_ENUM_DEF(name, value, len) \
	FMNA_STORAGE_PAIRING_ITEM_LEN_NAME(name) = len,
#define FMNA_STORAGE_PAIRING_ITEM_LEN_NAME_ARRAY_DEF(name, value, len) \
	FMNA_STORAGE_PAIRING_ITEM_LEN_NAME(name),

enum fmna_storage_pairing_item_len {
	FMNA_STORAGE_PAIRING_ITEM_MAP(FMNA_STORAGE_PAIRING_ITEM_LEN_ENUM_DEF)
};

struct settings_item {
	uint8_t *buf;
	size_t len;
};

struct settings_meta_item {
	struct settings_item *item;
	bool is_loaded;
};

static const enum fmna_storage_pairing_item_id pairing_item_ids[] = {
	FMNA_STORAGE_PAIRING_ITEM_MAP(FMNA_STORAGE_PAIRING_ITEM_ID_NAME_ARRAY_DEF)
};

int settings_load_direct(const char *key, size_t len, settings_read_cb read_cb,
			 void *cb_arg, void *param)
{
	int rc;
	struct settings_meta_item *meta_item = param;
	struct settings_item *item = meta_item->item;

	if (key) {
		LOG_ERR("settings_load_direct_cb: unexpected key value: %s", key);
		return -EINVAL;
	}

	if (len != item->len) {
		LOG_ERR("settings_load_direct_cb: %d != %d", len, item->len);
		return -EINVAL;
	}

	rc = read_cb(cb_arg, item->buf, item->len);
	if (rc >= 0) {
		meta_item->is_loaded = true;
		return 0;
	}

	return rc;
}

static int fmna_storage_direct_load(char *key, struct settings_item *item)
{
	int err;
	struct settings_meta_item meta_item = {
		.item = item,
		.is_loaded = false,
	};

	err = settings_load_subtree_direct(key, settings_load_direct,
					   &meta_item);
	if (err) {
		LOG_ERR("settings_load_subtree_direct returned error: %d",
			err);
		return err;
	}

	if (!meta_item.is_loaded) {
		return -ENOENT;
	}

	return err;
}

#if CONFIG_FMNA_CUSTOM_SERIAL_NUMBER
int fmna_storage_serial_number_load(uint8_t sn_buf[FMNA_SERIAL_NUMBER_BLEN])
{
	char *sn_node = FMNA_STORAGE_LEAF_NODE_BUILD(
		FMNA_STORAGE_BRANCH_PROVISIONING,
		STRINGIFY(FMNA_STORAGE_PROVISIONING_SERIAL_NUMBER_KEY));
	struct settings_item sn_item = {
		.buf = sn_buf,
		.len = FMNA_SERIAL_NUMBER_BLEN,
	};

	return fmna_storage_direct_load(sn_node, &sn_item);
}
#endif

int fmna_storage_uuid_load(uint8_t uuid_buf[FMNA_SW_AUTH_UUID_BLEN])
{
	char *uuid_node = FMNA_STORAGE_LEAF_NODE_BUILD(
		FMNA_STORAGE_BRANCH_PROVISIONING,
		STRINGIFY(FMNA_STORAGE_PROVISIONING_UUID_KEY));
	struct settings_item uuid_item = {
		.buf = uuid_buf,
		.len = FMNA_SW_AUTH_UUID_BLEN,
	};

	return fmna_storage_direct_load(uuid_node, &uuid_item);
}

int fmna_storage_auth_token_load(uint8_t token_buf[FMNA_SW_AUTH_TOKEN_BLEN])
{
	char *token_node = FMNA_STORAGE_LEAF_NODE_BUILD(
		FMNA_STORAGE_BRANCH_PROVISIONING,
		STRINGIFY(FMNA_STORAGE_PROVISIONING_AUTH_TOKEN_KEY));
	struct settings_item token_item = {
		.buf = token_buf,
		.len = FMNA_SW_AUTH_TOKEN_BLEN,
	};

	return fmna_storage_direct_load(token_node, &token_item);
}

int fmna_storage_auth_token_update(
	const uint8_t token_buf[FMNA_SW_AUTH_TOKEN_BLEN])
{
	char *token_node = FMNA_STORAGE_LEAF_NODE_BUILD(
		FMNA_STORAGE_BRANCH_PROVISIONING,
		STRINGIFY(FMNA_STORAGE_PROVISIONING_AUTH_TOKEN_KEY));

	return settings_save_one(token_node, token_buf,
				 FMNA_SW_AUTH_TOKEN_BLEN);
}

static int pairing_item_leaf_node_encode(enum fmna_storage_pairing_item_id item_id,
					 char pairing_leaf_node[FMNA_STORAGE_PAIRING_ITEM_KEY_LEN])
{
	int err;
	const size_t pairing_leaf_node_size = FMNA_STORAGE_PAIRING_ITEM_KEY_LEN;
	const char *pairing_leaf_node_fmt = FMNA_STORAGE_PAIRING_LEAF_NODE_FMT;

	err = snprintk(pairing_leaf_node,
		       pairing_leaf_node_size,
		       pairing_leaf_node_fmt,
		       item_id);
	if ((err > 0) || (err < pairing_leaf_node_size)) {
		return 0;
	} else {
		LOG_ERR("fmna_storage: snprintk returned %d", err);
		return -ERANGE;
	}
}

int fmna_storage_pairing_item_store(enum fmna_storage_pairing_item_id item_id,
				    const uint8_t *item,
				    size_t item_len)
{
	int err;
	char pairing_leaf_node[FMNA_STORAGE_PAIRING_ITEM_KEY_LEN];

	err = pairing_item_leaf_node_encode(item_id, pairing_leaf_node);
	if (err) {
		return err;
	}

	return settings_save_one(pairing_leaf_node, item, item_len);
}

int fmna_storage_pairing_item_load(enum fmna_storage_pairing_item_id item_id,
				   uint8_t *item,
				   size_t item_len)
{
	int err;
	char pairing_leaf_node[FMNA_STORAGE_PAIRING_ITEM_KEY_LEN];
	struct settings_item pairing_item = {
		.buf = item,
		.len = item_len,
	};

	err = pairing_item_leaf_node_encode(item_id, pairing_leaf_node);
	if (err) {
		return err;
	}

	return fmna_storage_direct_load(pairing_leaf_node, &pairing_item);
}

int fmna_storage_pairing_data_delete(void)
{
	int err;
	char pairing_leaf_node[FMNA_STORAGE_PAIRING_ITEM_KEY_LEN];

	for (size_t i = 0; i < ARRAY_SIZE(pairing_item_ids); i++) {
		err = pairing_item_leaf_node_encode(pairing_item_ids[i], pairing_leaf_node);
		if (err) {
			return err;
		}

		err = settings_delete(pairing_leaf_node);
		if (err) {
			LOG_ERR("settings_delete returned error: %d", err);
			return err;
		}
	}

	return 0;
}

static int pairing_branch_load(const char      *key,
			       size_t           len,
			       settings_read_cb read_cb,
			       void            *cb_arg,
			       void            *param)
{
	enum fmna_storage_pairing_item_id item_id;
	char *key_end;
	uint32_t *pairing_data_flags;
	static const enum fmna_storage_pairing_item_len pairing_item_lens[] = {
		FMNA_STORAGE_PAIRING_ITEM_MAP(FMNA_STORAGE_PAIRING_ITEM_LEN_NAME_ARRAY_DEF)
	};

	/* Validate if the pairing item ID is stored in correct format. */
	item_id = strtol(key, &key_end, 10);
	if ((key_end - key) != FMNA_STORAGE_PAIRING_ITEM_KEY_DIGIT_LEN) {
		LOG_ERR("fmna_storage_pairing_data_check: item ID has incorrect format: %s", key);
		return -ENOTSUP;
	}

	/* Validate if the pairing item ID has expected length. */
	if (len != pairing_item_lens[item_id]) {
		LOG_ERR("fmna_storage_pairing_data_check: item with the %d ID has unexpected "
			"length: %d != %d", item_id, len, pairing_item_lens[item_id]);
		return -ENOTSUP;
	}

	/* Indicate that the pairing item is stored in the Settings. */
	pairing_data_flags = (uint32_t *) param;
	WRITE_BIT(*pairing_data_flags, item_id, 0);

	return 0;
}

static int fmna_storage_pairing_data_check(void)
{
	int err;
	uint32_t pairing_data_flags;
	uint32_t pairing_data_mask = 0;
	const uint8_t pairing_data_mask_bit_cnt = sizeof(pairing_data_mask) * __CHAR_BIT__;
	const char *pairing_item_branch =
		FMNA_STORAGE_SUBTREE_BUILD(FMNA_STORAGE_BRANCH_PAIRING);
	static const char *pairing_item_strs[] = {
		FMNA_STORAGE_PAIRING_ITEM_MAP(FMNA_STORAGE_PAIRING_ITEM_ID_NAME_STR_ARRAY_DEF)
	};

	BUILD_ASSERT(ARRAY_SIZE(pairing_item_ids) < pairing_data_mask_bit_cnt,
		"FMN Pairing data set mask is too small");

	/* Set bits for items that are considered part of FMN pairing data set. */
	for (size_t i = 0; i < ARRAY_SIZE(pairing_item_ids); i++) {
		if (pairing_item_ids[i] >= pairing_data_mask_bit_cnt) {
			LOG_ERR("fmna_storage_pairing_data_check: mask is too small to "
				"store this item ID: %d", pairing_item_ids[i]);
			return -ENOTSUP;
		}

		if (pairing_data_mask & (BIT(pairing_item_ids[i]))) {
			LOG_ERR("fmna_storage_pairing_data_check: duplicated item ID: %d",
				pairing_item_ids[i]);
			return -ENOTSUP;
		}

		WRITE_BIT(pairing_data_mask, pairing_item_ids[i], 1);
	}

	pairing_data_flags = pairing_data_mask;
	err = settings_load_subtree_direct(pairing_item_branch,
					   pairing_branch_load,
					   &pairing_data_flags);
	if (err) {
		LOG_ERR("settings_load_subtree_direct returned error: %d", err);
		return err;
	}

	if (pairing_data_flags) {
		if (pairing_data_flags != pairing_data_mask) {
			/* Part of pairing data items are avaiable in the storage. */
			uint8_t shift_step;
			uint8_t bit_index = pairing_data_mask_bit_cnt - 1;

			LOG_WRN("FMN pairing information is not complete in the storage");
			LOG_WRN("Missing the following pairing items:");
			while (pairing_data_flags) {
				shift_step = __CLZ(pairing_data_flags);
				shift_step = (shift_step) ? shift_step : 1;
				bit_index -= shift_step;
				pairing_data_flags = pairing_data_flags << shift_step;

				if (pairing_data_flags) {
					LOG_WRN("\t%s", pairing_item_strs[bit_index]);
				}
			};
		} else {
			LOG_INF("FMN pairing information is not present in the storage");
		}
	} else {
		LOG_INF("FMN pairing information detected in the storage");

		FMNA_EVENT_CREATE(event, FMNA_EVENT_BONDED, NULL);
		APP_EVENT_SUBMIT(event);
	}

	return err;
}

int fmna_storage_init(bool delete_pairing_data)
{
	int err;

	err = settings_subsys_init();
	if (err) {
		LOG_ERR("settings_subsys_init returned error: %d", err);
		return err;
	}

	if (delete_pairing_data) {
		LOG_INF("FMN: Performing reset to default factory settings");
		return fmna_storage_pairing_data_delete();
	} else {
		return fmna_storage_pairing_data_check();
	}
}
