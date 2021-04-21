#ifndef FMNA_SERIAL_NUMBER_H_
#define FMNA_SERIAL_NUMBER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

#define FMNA_SERIAL_NUMBER_BLEN 16

void fmna_serial_number_get(uint8_t serial_number[FMNA_SERIAL_NUMBER_BLEN]);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_SERIAL_NUMBER_H_ */
