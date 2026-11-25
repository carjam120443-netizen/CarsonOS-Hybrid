#include <stdint.h>
#include "../../include/arch.h"
#include "../../include/ata.h"
#define DATA 0x1F0
#define COUNT 0x1F2
#define LBA0 0x1F3
#define LBA1 0x1F4
#define LBA2 0x1F5
#define DRIVE 0x1F6
#define STATUS 0x1F7
#define COMMAND 0x1F7
#define BSY 0x80
#define DRQ 0x08
#define ERR 0x01
#define DF 0x20
static uint32_t sectors;
static int ready;

static void delay400(void)
{
    (void)arch_inb(STATUS);
    (void)arch_inb(STATUS);
    (void)arch_inb(STATUS);
    (void)arch_inb(STATUS);
}

static int wait_ready(int data)
{
    for (uint32_t i = 0; i < 1000000u; ++i) {
        uint8_t s = arch_inb(STATUS);
        if (s == 0 || s == 0xFF || (s & (ERR | DF))) return 0;
        if (!(s & BSY)) return data ? ((s & DRQ) != 0) : 1;
        __asm__ volatile("pause");
    }
    return 0;
}

int ata_init(void)
{
    ready = 0;
    sectors = 0;
    arch_outb(DRIVE, 0xA0);
    delay400();
    arch_outb(COUNT, 0);
    arch_outb(LBA0, 0);
    arch_outb(LBA1, 0);
    arch_outb(LBA2, 0);
    arch_outb(COMMAND, 0xEC);
    uint8_t s = arch_inb(STATUS);
    if (s == 0 || s == 0xFF) return 0;
    for (uint32_t i = 0; i < 1000000u; ++i) {
        s = arch_inb(STATUS);
        if (!(s & BSY)) break;
        __asm__ volatile("pause");
    }
    if (s == 0 || s == 0xFF || (s & (ERR | DF))) return 0;
    if (arch_inb(LBA1) != 0 || arch_inb(LBA2) != 0 || !wait_ready(1)) return 0;
    uint16_t id[256];
    for (unsigned i = 0; i < 256; ++i) id[i] = arch_inw(DATA);
    if (!(id[49] & (1u << 9))) return 0;
    sectors = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
    if (!sectors) return 0;
    ready = 1;
    return 1;
}

static int valid_lba(uint32_t lba, const uint8_t *buffer)
{
    return ready && buffer && lba < sectors && lba < 0x10000000u;
}

int ata_read_sector(uint32_t lba, uint8_t *buffer)
{
    if (!valid_lba(lba, buffer) || !wait_ready(0)) return 0;
    arch_outb(DRIVE, (uint8_t)(0xE0u | ((lba >> 24) & 15u)));
    delay400();
    arch_outb(COUNT, 1);
    arch_outb(LBA0, (uint8_t)lba);
    arch_outb(LBA1, (uint8_t)(lba >> 8));
    arch_outb(LBA2, (uint8_t)(lba >> 16));
    arch_outb(COMMAND, 0x20);
    if (!wait_ready(1)) return 0;
    for (unsigned i = 0; i < 256; ++i) {
        uint16_t word = arch_inw(DATA);
        buffer[i * 2] = (uint8_t)word;
        buffer[i * 2 + 1] = (uint8_t)(word >> 8);
    }
    delay400();
    return wait_ready(0);
}

/*
 * Write exactly one 512-byte sector using ATA PIO LBA28.
 * Callers must validate the target disk and sector before using this API.
 * No partitioning or formatting is performed by this low-level routine.
 */
int ata_write_sector(uint32_t lba, const uint8_t *buffer)
{
    if (!valid_lba(lba, buffer) || !wait_ready(0)) return 0;

    arch_outb(DRIVE, (uint8_t)(0xE0u | ((lba >> 24) & 15u)));
    delay400();
    arch_outb(COUNT, 1);
    arch_outb(LBA0, (uint8_t)lba);
    arch_outb(LBA1, (uint8_t)(lba >> 8));
    arch_outb(LBA2, (uint8_t)(lba >> 16));
    arch_outb(COMMAND, 0x30);
    if (!wait_ready(1)) return 0;

    for (unsigned i = 0; i < 256; ++i) {
        uint16_t word = (uint16_t)buffer[i * 2] |
                        ((uint16_t)buffer[i * 2 + 1] << 8);
        arch_outw(DATA, word);
    }

    delay400();
    if (!wait_ready(0)) return 0;

    /* Flush the drive's write cache before reporting completion. */
    arch_outb(COMMAND, 0xE7);
    delay400();
    return wait_ready(0);
}

int ata_is_ready(void) { return ready; }
uint32_t ata_sector_count(void) { return sectors; }
