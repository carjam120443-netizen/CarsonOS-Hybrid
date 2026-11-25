#include <stdint.h>
#include "../../include/arch.h"
#include "../../include/pci.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_SERIAL_PORT 0x3F8
#define PCI_DIAGNOSTIC_LIMIT 16

struct pci_seen_device {
    uint8_t bus, device, function;
    uint16_t vendor, device_id;
};

static void pci_serial_char(char c)
{
    while ((arch_inb(PCI_SERIAL_PORT + 5) & 0x20u) == 0)
        __asm__ volatile ("pause");
    arch_outb(PCI_SERIAL_PORT, (uint8_t)c);
}

static void pci_serial_write(const char *s)
{
    while (*s) {
        if (*s == '\n') pci_serial_char('\r');
        pci_serial_char(*s++);
    }
}

static void pci_serial_u32(uint32_t value)
{
    char digits[10];
    unsigned count = 0;
    if (!value) {
        pci_serial_char('0');
        return;
    }
    while (value && count < sizeof(digits)) {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (count) pci_serial_char(digits[--count]);
}

static void pci_serial_hex16(uint16_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int shift = 12; shift >= 0; shift -= 4)
        pci_serial_char(hex[(value >> shift) & 0xFu]);
}

static uint32_t pci_read32(uint8_t b, uint8_t d, uint8_t f, uint8_t o)
{
    uint32_t address = 0x80000000u |
        ((uint32_t)b << 16) |
        ((uint32_t)d << 11) |
        ((uint32_t)f << 8) |
        ((uint32_t)o & 0xFCu);
    arch_outl(PCI_CONFIG_ADDRESS, address);
    return arch_inl(PCI_CONFIG_DATA);
}

static void pci_write32(uint8_t b, uint8_t d, uint8_t f,
                        uint8_t o, uint32_t value)
{
    uint32_t address = 0x80000000u |
        ((uint32_t)b << 16) |
        ((uint32_t)d << 11) |
        ((uint32_t)f << 8) |
        ((uint32_t)o & 0xFCu);
    arch_outl(PCI_CONFIG_ADDRESS, address);
    arch_outl(PCI_CONFIG_DATA, value);
}

int pci_enable_memory_bus_master(const struct pci_device_info *device)
{
    if (!device) return 0;
    uint16_t command = (uint16_t)pci_read32(
        device->bus, device->device, device->function, 4);
    command |= 6u;
    pci_write32(device->bus, device->device, device->function, 4, command);
    return (((uint16_t)pci_read32(
        device->bus, device->device, device->function, 4)) & 6u) == 6u;
}

/*
 * Scan PCI configuration space once. When the target NIC is absent, retain a
 * small sample of discovered devices so the serial log can distinguish a
 * broken config-I/O path from a device-ID mismatch.
 */
int pci_find_e1000(struct pci_device_info *out)
{
    struct pci_seen_device seen[PCI_DIAGNOSTIC_LIMIT];
    uint32_t seen_count = 0;
    uint32_t present_count = 0;
    uint32_t function_probes = 0;
    uint32_t config_probe = pci_read32(0, 0, 0, 0);
    int found = 0;

    if (!out) return 0;

    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t device = 0; device < 32; ++device) {
            uint32_t header_id = pci_read32(
                (uint8_t)bus, (uint8_t)device, 0, 0);
            ++function_probes;
            if ((uint16_t)header_id == 0xFFFFu) continue;

            uint32_t header = pci_read32(
                (uint8_t)bus, (uint8_t)device, 0, 0x0C);
            uint32_t function_count =
                ((header >> 16) & 0x80u) ? 8u : 1u;

            for (uint32_t function = 0;
                 function < function_count; ++function) {
                uint32_t id = function == 0 ? header_id :
                    pci_read32((uint8_t)bus, (uint8_t)device,
                               (uint8_t)function, 0);
                if (function != 0) ++function_probes;
                uint16_t vendor = (uint16_t)id;
                uint16_t device_id = (uint16_t)(id >> 16);
                if (vendor == 0xFFFFu) continue;

                ++present_count;
                if (seen_count < PCI_DIAGNOSTIC_LIMIT) {
                    seen[seen_count].bus = (uint8_t)bus;
                    seen[seen_count].device = (uint8_t)device;
                    seen[seen_count].function = (uint8_t)function;
                    seen[seen_count].vendor = vendor;
                    seen[seen_count].device_id = device_id;
                    ++seen_count;
                }

                if (vendor != 0x8086u || device_id != 0x100Eu)
                    continue;

                uint32_t class_info = pci_read32(
                    (uint8_t)bus, (uint8_t)device,
                    (uint8_t)function, 8);
                out->bus = (uint8_t)bus;
                out->device = (uint8_t)device;
                out->function = (uint8_t)function;
                out->vendor_id = vendor;
                out->device_id = device_id;
                out->class_code = (uint8_t)(class_info >> 24);
                out->subclass = (uint8_t)(class_info >> 16);
                out->bar0 = pci_read32(
                    (uint8_t)bus, (uint8_t)device,
                    (uint8_t)function, 0x10);
                out->bar1 = pci_read32(
                    (uint8_t)bus, (uint8_t)device,
                    (uint8_t)function, 0x14);
                found = 1;
                break;
            }
            if (found) break;
        }
        if (found) break;
    }

    if (!found) {
        pci_serial_write("PCI diagnostic: E1000 target not found; config-space scan summary\n");
        pci_serial_write("PCI diagnostic: bus 0 device 0 function 0 ID=0x");
        pci_serial_hex16((uint16_t)(config_probe >> 16));
        pci_serial_char(':');
        pci_serial_hex16((uint16_t)config_probe);
        pci_serial_write("\nPCI diagnostic: function probes=");
        pci_serial_u32(function_probes);
        pci_serial_write(" present functions=");
        pci_serial_u32(present_count);
        pci_serial_write("\n");

        for (uint32_t i = 0; i < seen_count; ++i) {
            pci_serial_write("PCI diagnostic: ");
            pci_serial_u32(seen[i].bus);
            pci_serial_char(':');
            pci_serial_u32(seen[i].device);
            pci_serial_char('.');
            pci_serial_u32(seen[i].function);
            pci_serial_write(" ID=0x");
            pci_serial_hex16(seen[i].vendor);
            pci_serial_char(':');
            pci_serial_hex16(seen[i].device_id);
            pci_serial_write("\n");
        }

        if (present_count > seen_count)
            pci_serial_write("PCI diagnostic: device sample capped at 16 entries\n");
        if (present_count == 0)
            pci_serial_write("PCI diagnostic: no devices detected; check CF8/CFC I/O access\n");
        else
            pci_serial_write("PCI diagnostic: devices exist, but none matched 8086:100E\n");
    }

    return found;
}
