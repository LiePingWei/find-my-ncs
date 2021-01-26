#include "fmna_storage.h"

#include <string.h>

#include <logging/log.h>
#include <settings/settings.h>
#include <sys/byteorder.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMN_ADK_LOG_LEVEL);

#define FMNA_STORAGE_TREE "fmna"

#define FMNA_STORAGE_NODE_CONNECTOR "/"

#define FMNA_STORAGE_BRANCH_PROVISIONING "provisioning"

#define FMNA_STORAGE_PROVISIONING_UUID_KEY       998
#define FMNA_STORAGE_PROVISIONING_AUTH_TOKEN_KEY 999

#define FMNA_STORAGE_NODE_CONNECT(_child_node, ...)		\
	__VA_ARGS__ FMNA_STORAGE_NODE_CONNECTOR _child_node

#define FMNA_STORAGE_SUBTREE_BUILD(_branch) 			\
	FMNA_STORAGE_NODE_CONNECT(_branch, FMNA_STORAGE_TREE)

#define FMNA_STORAGE_LEAF_NODE_BUILD(_branch, _leaf_node)	\
	FMNA_STORAGE_NODE_CONNECT(				\
		STRINGIFY(_leaf_node),				\
		FMNA_STORAGE_SUBTREE_BUILD(_branch))

struct settings_item {
	const char *branch_name;
	uint16_t key_id;
	uint8_t *buf;
	uint16_t len;
};

struct settings_meta_item {
	struct settings_item *item;
	bool is_loaded;
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

int fmna_storage_uuid_load(uint8_t uuid_buf[FMNA_SW_AUTH_UUID_BLEN])
{
	char *uuid_node = FMNA_STORAGE_LEAF_NODE_BUILD(
		FMNA_STORAGE_BRANCH_PROVISIONING,
		FMNA_STORAGE_PROVISIONING_UUID_KEY);
	struct settings_item uuid_item = {
		.branch_name = FMNA_STORAGE_BRANCH_PROVISIONING,
		.key_id = FMNA_STORAGE_PROVISIONING_UUID_KEY,
		.buf = uuid_buf,
		.len = FMNA_SW_AUTH_UUID_BLEN,
	};

	return fmna_storage_direct_load(uuid_node, &uuid_item);
}

int fmna_storage_auth_token_load(uint8_t token_buf[FMNA_SW_AUTH_TOKEN_BLEN])
{
	char *token_node = FMNA_STORAGE_LEAF_NODE_BUILD(
		FMNA_STORAGE_BRANCH_PROVISIONING,
		FMNA_STORAGE_PROVISIONING_AUTH_TOKEN_KEY);
	struct settings_item token_item = {
		.branch_name = FMNA_STORAGE_BRANCH_PROVISIONING,
		.key_id = FMNA_STORAGE_PROVISIONING_AUTH_TOKEN_KEY,
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
		FMNA_STORAGE_PROVISIONING_AUTH_TOKEN_KEY);

	return settings_save_one(token_node, token_buf,
				 FMNA_SW_AUTH_TOKEN_BLEN);
}

int fmna_storage_init(void)
{
	int err;

	err = settings_subsys_init();
	if (err) {
		LOG_ERR("settings_subsys_init returned error: %d", err);
		return err;
	}

	return err;
}
