#ifndef FMNA_KEYS_H_
#define FMNA_KEYS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

#define FMNA_MASTER_PUBLIC_KEY_LEN 57
#define FMNA_SYMMETRIC_KEY_LEN     32
#define FMNA_PUBLIC_KEY_LEN        28

struct fmna_keys_init {
	uint8_t master_pk[FMNA_MASTER_PUBLIC_KEY_LEN];
	uint8_t primary_sk[FMNA_SYMMETRIC_KEY_LEN];
	uint8_t secondary_sk[FMNA_SYMMETRIC_KEY_LEN];
};

int fmna_keys_primary_key_get(uint8_t primary_key[FMNA_PUBLIC_KEY_LEN]);

int fmna_keys_separated_key_get(uint8_t separated_key[FMNA_PUBLIC_KEY_LEN]);

void fmna_keys_nearby_state_notify(void);

int fmna_keys_reset(const struct fmna_keys_init *init_keys);

int fmna_keys_init(uint8_t id);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_KEYS_H_ */