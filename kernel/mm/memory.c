#include <stdint.h>
#include "../include/memory.h"

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response {
    uint64_t revision;
    uint64_t entry_count;
    struct limine_memmap_entry **entries;
};

struct limine_memmap_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_memmap_response *response;
};

struct limine_hhdm_response {
    uint64_t revision;
    uint64_t offset;
};

struct limine_hhdm_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_hhdm_response *response;
};

__attribute__((used, section(".limine_requests"), aligned(8)))
static volatile struct limine_memmap_request memmap_request = {
    .id = {
        0xc7b1dd30df4c8b88, 0x0a82e883a194f07b,
        0x67cf3d9d378a806f, 0xe304acdfc50c3c62
    },
    .revision = 0,
    .response = 0
};

__attribute__((used, section(".limine_requests"), aligned(8)))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = {
        0xc7b1dd30df4c8b88, 0x0a82e883a194f07b,
        0x48dcf1cb8ad2b852, 0x63984e959a98244b
    },
    .revision = 0,
    .response = 0
};

static uint64_t usable_bytes;
static uint64_t region_count;
static uint64_t hhdm_offset;

void memory_init(void)
{
    usable_bytes = 0;
    region_count = 0;
    hhdm_offset = 0;

    struct limine_memmap_response *response = memmap_request.response;
    if (response && response->entries) {
        for (uint64_t i = 0; i < response->entry_count; ++i) {
            struct limine_memmap_entry *entry = response->entries[i];
            if (!entry) continue;
            if (entry->type == 0) {
                if (UINT64_MAX - usable_bytes >= entry->length)
                    usable_bytes += entry->length;
                ++region_count;
            }
        }
    }

    struct limine_hhdm_response *hhdm = hhdm_request.response;
    if (hhdm) hhdm_offset = hhdm->offset;
}

uint64_t memory_usable_bytes(void) { return usable_bytes; }
uint64_t memory_region_count(void) { return region_count; }
uint64_t memory_hhdm_offset(void) { return hhdm_offset; }
