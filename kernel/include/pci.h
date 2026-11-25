#ifndef CARSONOS_PCI_H
#define CARSONOS_PCI_H

#include <stdint.h>

struct pci_device_info {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t bar0;
    uint32_t bar1;
    uint8_t class_code;
    uint8_t subclass;
};

int pci_find_e1000(struct pci_device_info *out);

#endif
