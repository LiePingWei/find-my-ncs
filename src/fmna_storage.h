#ifndef FMNA_STORAGE_H_
#define FMNA_STORAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

#define FMNA_SW_AUTH_TOKEN_BLEN 1024
#define FMNA_SW_AUTH_UUID_BLEN  16

#define FMNA_MASTER_PUBLIC_KEY_LEN       57
#define FMNA_SYMMETRIC_KEY_LEN           32
#define FMNA_PRIMARY_KEY_INDEX_LEN       4
#define FMNA_CURRENT_KEYS_INDEX_DIFF_LEN 2
#define FMNA_SERVER_SHARED_SECRET_LEN    32
#define FMNA_SN_QUERY_COUNTER_LEN        sizeof(uint64_t)
#define FMNA_ICLOUD_ID_LEN               60

#define FMNA_STORAGE_PAIRING_ITEM_MAP(X)					     \
	X(FMNA_STORAGE_MASTER_PUBLIC_KEY, 0, FMNA_MASTER_PUBLIC_KEY_LEN)	     \
	X(FMNA_STORAGE_PRIMARY_SK, 1, FMNA_SYMMETRIC_KEY_LEN)			     \
	X(FMNA_STORAGE_SECONDARY_SK, 2, FMNA_SYMMETRIC_KEY_LEN)			     \
	X(FMNA_STORAGE_PRIMARY_KEY_INDEX, 3, FMNA_PRIMARY_KEY_INDEX_LEN)	     \
	X(FMNA_STORAGE_CURRENT_KEYS_INDEX_DIFF, 4, FMNA_CURRENT_KEYS_INDEX_DIFF_LEN) \
	X(FMNA_STORAGE_SERVER_SHARED_SECRET, 5, FMNA_SERVER_SHARED_SECRET_LEN)	     \
	X(FMNA_STORAGE_SN_QUERY_COUNTER, 6, FMNA_SN_QUERY_COUNTER_LEN)		     \
	X(FMNA_STORAGE_ICLOUD_ID, 7, FMNA_ICLOUD_ID_LEN)

#define FMNA_STORAGE_PAIRING_ITEM_ID_NAME(name) CONCAT(name, _ID)
#define FMNA_STORAGE_PAIRING_ITEM_ID_ENUM_DEF(name, value, len) \
	FMNA_STORAGE_PAIRING_ITEM_ID_NAME(name) = value,

enum fmna_storage_pairing_item_id {
	FMNA_STORAGE_PAIRING_ITEM_MAP(FMNA_STORAGE_PAIRING_ITEM_ID_ENUM_DEF)
};

/* General storage API */

int fmna_storage_init(bool delete_pairing_data);

/* API for accessing and manipulating provisioned data. */

int fmna_storage_uuid_load(uint8_t uuid_buf[FMNA_SW_AUTH_UUID_BLEN]);

int fmna_storage_auth_token_load(uint8_t token_buf[FMNA_SW_AUTH_TOKEN_BLEN]);

int fmna_storage_auth_token_update(
	const uint8_t token_buf[FMNA_SW_AUTH_TOKEN_BLEN]);

/* API for accessing and manipulating pairing data. */

int fmna_storage_pairing_item_store(enum fmna_storage_pairing_item_id item_id,
				    const uint8_t *item,
				    size_t item_len);

int fmna_storage_pairing_item_load(enum fmna_storage_pairing_item_id item_id,
				   uint8_t *item,
				   size_t item_len);

int fmna_storage_pairing_data_delete(void);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_STORAGE_H_ */
