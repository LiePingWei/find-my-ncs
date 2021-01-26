#ifndef FMNA_STORAGE_H_
#define FMNA_STORAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

#define FMNA_SW_AUTH_TOKEN_BLEN 1024
#define FMNA_SW_AUTH_UUID_BLEN  16

/* General storage API */

int fmna_storage_init(void);

/* API for accessing and manipulating provisioned data. */

int fmna_storage_uuid_load(uint8_t uuid_buf[FMNA_SW_AUTH_UUID_BLEN]);

int fmna_storage_auth_token_load(uint8_t token_buf[FMNA_SW_AUTH_TOKEN_BLEN]);

int fmna_storage_auth_token_update(
	const uint8_t token_buf[FMNA_SW_AUTH_TOKEN_BLEN]);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_STORAGE_H_ */