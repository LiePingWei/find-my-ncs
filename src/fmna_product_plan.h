#ifndef FMNA_PRODUCT_PLAN_H_
#define FMNA_PRODUCT_PLAN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

#define FMNA_PP_PRODUCT_DATA_LEN                8
#define FMNA_PP_SERVER_ENCRYPTION_KEY_LEN       65
#define FMNA_PP_SERVER_SIG_VERIFICATION_KEY_LEN 65

extern uint8_t fmna_pp_product_data[FMNA_PP_PRODUCT_DATA_LEN];

extern const uint8_t fmna_pp_server_encryption_key[FMNA_PP_SERVER_ENCRYPTION_KEY_LEN];

extern const uint8_t fmna_pp_server_sig_verification_key[FMNA_PP_SERVER_SIG_VERIFICATION_KEY_LEN];

#ifdef __cplusplus
}
#endif


#endif /* FMNA_PRODUCT_PLAN_H_ */