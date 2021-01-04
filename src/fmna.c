#include "fmna_pair.h"

#include <logging/log.h>

LOG_MODULE_REGISTER(fmna, CONFIG_FMN_ADK_LOG_LEVEL);

int fmna_init(void)
{
	return fmna_pair_init();
}
