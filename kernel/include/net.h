#ifndef CARSONOS_NET_H
#define CARSONOS_NET_H
#include <stdint.h>
#include "pci.h"
int e1000_init(const struct pci_device_info *device);
void e1000_poll(void);
int e1000_is_ready(void);
int e1000_link_up(void);
uint64_t e1000_received_packets(void);
int e1000_send_frame(const void *data, uint16_t length);
int e1000_receive_frame(void *buffer, uint16_t capacity, uint16_t *received);
#endif
