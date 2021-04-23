#ifndef FMNA_VERSION_H_
#define FMNA_VERSION_H_

#ifdef  __cplusplus
extern 'C' {
#endif

#include <zephyr.h>

struct fmna_version {
	uint16_t major;
	uint8_t minor;
	uint8_t revision;
	uint32_t build_num;
};

int fmna_version_fw_get(struct fmna_version *ver);

#ifdef __cplusplus
}
#endif

#endif /* FMNA_VERSION_H_ */
