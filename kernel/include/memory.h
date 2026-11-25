#ifndef CARSONOS_MEMORY_H
#define CARSONOS_MEMORY_H

#include <stdint.h>

void memory_init(void);
uint64_t memory_usable_bytes(void);
uint64_t memory_region_count(void);
uint64_t memory_hhdm_offset(void);

#endif
