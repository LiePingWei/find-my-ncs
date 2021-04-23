#include "fmna_version.h"

#include <logging/log.h>

#include <dfu/mcuboot.h>
#include <pm_config.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

#if defined(CONFIG_FMNA_UARP)

#define IMAGE0_ID PM_MCUBOOT_PRIMARY_ID

int fmna_version_fw_get(struct fmna_version *ver)
{
	int err;
	struct mcuboot_img_header header;

	if (!ver) {
		return -EINVAL;
	}

	err = boot_read_bank_header(IMAGE0_ID, &header, sizeof(header));
	if (err) {
		LOG_ERR("fma_version: boot_read_bank_header returned error: %d", err);
		memset(ver, 0, sizeof(*ver));
		return err;
	}

	ver->major = header.h.v1.sem_ver.major;
	ver->minor = header.h.v1.sem_ver.minor;
	ver->revision = header.h.v1.sem_ver.revision;
	ver->build_num = header.h.v1.sem_ver.build_num;

	return 0;
}
#else

int fmna_version_fw_get(struct fmna_version *ver)
{
	ver->major = CONFIG_FMNA_FIRMWARE_VERSION_MAJOR;
	ver->minor = CONFIG_FMNA_FIRMWARE_VERSION_MINOR;
	ver->revision = CONFIG_FMNA_FIRMWARE_VERSION_REVISION;
	ver->build_num = 0;
	
	return 0;
}
#endif /* defined(CONFIG_FMNA_UARP) */
