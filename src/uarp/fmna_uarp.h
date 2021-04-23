#ifndef FMNA_UARP_H_
#define FMNA_UARP_H_

#include <stdint.h>

#include <net/buf.h>

typedef uint32_t (*fmna_uarp_send_message_fn)(struct net_buf_simple *buf);

bool fmna_uarp_init(fmna_uarp_send_message_fn send_message_callback);

void fmna_uarp_controller_add(void);

void fmna_uarp_controller_remove(void);

void fmna_uarp_recv_message(struct net_buf_simple *buf);

void fmna_uarp_send_message_complete(void);

int fmna_uarp_img_confirm(void);

#endif /* FMNA_UARP_H_ */
