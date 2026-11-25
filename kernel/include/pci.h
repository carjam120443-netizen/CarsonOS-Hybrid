#ifndef CARSONOS_PCI_H
#define CARSONOS_PCI_H
#include <stdint.h>
struct pci_device_info { uint8_t bus,device,function; uint16_t vendor_id,device_id; uint32_t bar0,bar1; uint8_t class_code,subclass; };
int pci_find_e1000(struct pci_device_info *out);
int pci_enable_memory_bus_master(const struct pci_device_info *device);
#endif
