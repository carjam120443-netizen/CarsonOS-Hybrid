#include <stdint.h>
#include "include/arch.h"

/* Tell Limine which protocol revision this kernel supports. */
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[4] = {
    0xf6b8f4b39de7d1ae,
    0xfab91a6940fcb9cf,
    0x785c6ed015d3e316,
    0x181e920a7852b9d9
};

__attribute__((used, section(".limine_requests")))
volatile uint64_t limine_base_revision[3] = {
    0xf9562b2d5c95a6c8,
    0x6a7b384944536bdc,
    3
};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[2] = {
    0xadc0e0531bb10d03,
    0x9572709f31764c62
};

#define COM1 0x3F8

static void serial_init(void)
{
    arch_outb(COM1 + 1, 0x00); /* Disable interrupts. */
    arch_outb(COM1 + 3, 0x80); /* Enable divisor latch. */
    arch_outb(COM1 + 0, 0x03); /* 38400 baud. */
    arch_outb(COM1 + 1, 0x00);
    arch_outb(COM1 + 3, 0x03); /* 8 data bits, no parity, one stop bit. */
    arch_outb(COM1 + 2, 0xC7); /* Enable and clear FIFO. */
    arch_outb(COM1 + 4, 0x0B); /* IRQs enabled, RTS/DSR set. */
}

static int serial_is_transmit_ready(void)
{
    return (arch_inb(COM1 + 5) & 0x20) != 0;
}

static void serial_write_char(char c)
{
    while (!serial_is_transmit_ready()) {
        __asm__ volatile ("pause");
    }

    arch_outb(COM1, (uint8_t)c);
}

static void serial_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(*text++);
    }
}

__attribute__((noreturn))
void kernel_main(void)
{
    arch_disable_interrupts();
    serial_init();

    serial_write("\n");
    serial_write("CarsonOS Hybrid\n");
    serial_write("\nCarsonOS Hybrid kernel\n");
    serial_write("Architecture: x86_64\n");
    serial_write("Boot status: kernel entry reached\n");
    serial_write("Serial console: COM1 ready\n");
    serial_write("\nCPU interrupts remain disabled; entering halt loop.\n");

    for (;;) {
        arch_halt();
    }
}
