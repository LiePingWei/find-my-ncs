#ifndef FMNA_PAIR_H_
#define FMNA_PAIR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

#define C1_BLEN                32
#define C2_BLEN                89
#define C3_BLEN                60

#define E1_BLEN                113
#define E2_BLEN                1326
#define E3_BLEN                1040
#define E4_BLEN                1286

#define S2_BLEN                100

#define SESSION_NONCE_BLEN     32
#define SEEDS_BLEN             32
#define ICLOUD_IDENTIFIER_BLEN 60

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

#ifdef __cplusplus
}
#endif


#endif /* FMNA_PAIR_H_ */