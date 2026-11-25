#ifndef CARSONOS_ARCH_H
#define CARSONOS_ARCH_H

#include <stdint.h>

void arch_outb(uint16_t port, uint8_t value);
uint8_t arch_inb(uint16_t port);
uint32_t arch_inl(uint16_t port);
void arch_outl(uint16_t port, uint32_t value);
void arch_disable_interrupts(void);
void arch_halt(void);

#endif
