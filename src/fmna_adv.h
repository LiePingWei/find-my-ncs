#ifndef FMNA_ADV_H_
#define FMNA_ADV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

#include "fmna_keys.h"

#define FMNA_ADV_SEPARATED_HINT_INDEX 5

int fmna_adv_start_unpaired(bool change_address);

int fmna_adv_start_nearby(const uint8_t pubkey[FMNA_PUBLIC_KEY_LEN]);

int fmna_adv_start_separated(const uint8_t pubkey[FMNA_PUBLIC_KEY_LEN], uint8_t hint);

int fmna_adv_init(uint8_t id);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_ADV_H_ */