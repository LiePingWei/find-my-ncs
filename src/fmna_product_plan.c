#include <kernel.h>
#include <init.h>
#include <string.h>
#include <sys/byteorder.h>

#include "fmna_product_plan.h"

BUILD_ASSERT(CONFIG_FMN_PRODUCT_DATA != 0,
	"The FMN Product Data configuration must be set");
BUILD_ASSERT(CONFIG_FMN_PRODUCT_DATA < UINT64_MAX,
	"The FMN Product Data value is too large");

/* Product Data (PD) configuration derived from CONFIG_FMN_PRODUCT_DATA */
uint8_t fmna_pp_product_data[FMNA_PP_PRODUCT_DATA_LEN];

/* Server Keys for the Nordic Product Plan.
 * Please define these keys in your application when using your product plan.
 */
#if CONFIG_FMN_NORDIC_PRODUCT_PLAN

/* Server encryption key: Q_E */
const uint8_t fmna_pp_server_encryption_key[] = {
	0x04, 0x9c, 0xc5, 0xad, 0xdd, 0xd0, 0x29, 0xb7, 0x53, 0x5d, 0x30, 0xe6, 0xe5, 0xd1, 0x6d, 0xb7, 0xa8, 0xd2, 0x1b, 0x1b, 0x48, 0xb5, 0x5b, 0x19, 0xd5, 0xb1, 0x10, 0xe9, 0x5b, 0xf3, 0x15, 0x45, 0xe7, 0x74, 0xcf, 0x51, 0x8d, 0xeb, 0xbe, 0x3c, 0x71, 0x68, 0x33, 0xe4, 0x43, 0xf1, 0x14, 0x47, 0x6e, 0x5a, 0x4b, 0x05, 0x4e, 0x36, 0x75, 0x07, 0x05, 0x6e, 0x39, 0x95, 0xcc, 0x6b, 0x96, 0x90, 0x96
};

/* Server signature verification key: Q_A */
const uint8_t fmna_pp_server_sig_verification_key[] = {
	0x04, 0x33, 0x4c, 0x5a, 0x73, 0xfd, 0x61, 0xdf, 0x36, 0x43, 0x3f, 0xbc, 0x69, 0x92, 0x36, 0xe3, 0x98, 0xe4, 0x94, 0x12, 0xf3, 0xc0, 0xfd, 0xc4, 0xe5, 0xda, 0x0b, 0x41, 0x18, 0x77, 0x95, 0x17, 0x08, 0x71, 0x20, 0x88, 0x8e, 0x97, 0x92, 0x37, 0x76, 0xba, 0x48, 0xdc, 0x51, 0x7c, 0x0f, 0xa8, 0x7b, 0x9c, 0x62, 0xa9, 0xfe, 0xe9, 0x6b, 0x0f, 0x38, 0x40, 0x3f, 0x66, 0x9e, 0x1e, 0x67, 0x55, 0x60
};

#endif

static int product_plan_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	sys_put_be64(CONFIG_FMN_PRODUCT_DATA, fmna_pp_product_data);

	return 0;
}

SYS_INIT(product_plan_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
