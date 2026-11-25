#include <stdint.h>
#include "../../include/arch.h"
#include "../../include/pci.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static uint32_t pci_read32(uint8_t bus, uint8_t device,
                           uint8_t function, uint8_t offset)
{
    uint32_t address = 0x80000000u |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)function << 8) |
        ((uint32_t)offset & 0xFCu);
    arch_outl(PCI_CONFIG_ADDRESS, address);
    return arch_inl(PCI_CONFIG_DATA);
}

int pci_find_e1000(struct pci_device_info *out)
{
    if (!out) return 0;

    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t device = 0; device < 32; ++device) {
            uint32_t id0 = pci_read32((uint8_t)bus, (uint8_t)device, 0, 0);
            uint16_t vendor0 = (uint16_t)id0;
            if (vendor0 == 0xFFFFu) continue;

            uint32_t header = pci_read32((uint8_t)bus, (uint8_t)device, 0, 0x0C);
            uint8_t header_type = (uint8_t)(header >> 16);
            uint32_t functions = (header_type & 0x80u) ? 8u : 1u;

            for (uint32_t function = 0; function < functions; ++function) {
                uint32_t id = function == 0 ? id0 :
                    pci_read32((uint8_t)bus, (uint8_t)device,
                               (uint8_t)function, 0);
                uint16_t vendor = (uint16_t)id;
                uint16_t device_id = (uint16_t)(id >> 16);
                if (vendor != 0x8086u || device_id != 0x100Eu) continue;

                uint32_t class_reg = pci_read32((uint8_t)bus, (uint8_t)device,
                                                (uint8_t)function, 0x08);
                out->bus = (uint8_t)bus;
                out->device = (uint8_t)device;
                out->function = (uint8_t)function;
                out->vendor_id = vendor;
                out->device_id = device_id;
                out->class_code = (uint8_t)(class_reg >> 24);
                out->subclass = (uint8_t)(class_reg >> 16);
                out->bar0 = pci_read32((uint8_t)bus, (uint8_t)device,
                                       (uint8_t)function, 0x10);
                out->bar1 = pci_read32((uint8_t)bus, (uint8_t)device,
                                       (uint8_t)function, 0x14);
                return 1;
            }
        }
    }
    return 0;
}
