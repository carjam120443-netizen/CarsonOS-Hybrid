#include <stdint.h>
#include "include/arch.h"

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
    serial_write("  ______                           ____   _____\n");
    serial_write(" / ____/___ _________  ____  ____  / __/  / ___/\n");
    serial_write("/ /   / __ `/ ___/ _ \/ __ \/ __ \/ /_    \\__ \\n");
    serial_write("/ /___/ /_/ / /  /  __/ /_/ / / / / __/   ___/ /\n");
    serial_write("\\____/\\__,_/_/   \\___/\\____/_/ /_/_/     /____/\n");
    serial_write("\nCarsonOS Hybrid kernel\n");
    serial_write("Architecture: x86_64\n");
    serial_write("Boot status: kernel entry reached\n");
    serial_write("Serial console: COM1 ready\n");
    serial_write("\nCPU interrupts remain disabled; entering halt loop.\n");

    for (;;) {
        arch_halt();
    }
}
