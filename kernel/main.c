#include <stdint.h>
#include "include/arch.h"

struct limine_framebuffer {
    uint64_t address, width, height, pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size, red_mask_shift;
    uint8_t green_mask_size, green_mask_shift;
    uint8_t blue_mask_size, blue_mask_shift;
};
struct limine_framebuffer_response {
    uint64_t revision, framebuffer_count;
    struct limine_framebuffer **framebuffers;
};
struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_framebuffer_response *response;
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[4] = {
    0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf,
    0x785c6ed015d3e316, 0x181e920a7852b9d9
};
__attribute__((used, section(".limine_requests")))
volatile uint64_t limine_base_revision[3] = {
    0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, 3
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = {0xc7b1dd30df4c8b88, 0x0a82e883a194f07b,
            0x9d5827dcd881dd75, 0xa3148604f6fab11b},
    .revision = 0,
    .response = 0
};
__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[2] = {
    0xadc0e0531bb10d03, 0x9572709f31764c62
};

#define COM1 0x3F8
#define BG 0x08111F
#define PANEL 0x101E30
#define ACCENT 0x35D0C5
#define TEXT 0xE8F1FA
#define MUTED 0x8DA6BD

static struct limine_framebuffer *fb;

static void serial_init(void)
{
    arch_outb(COM1 + 1, 0);
    arch_outb(COM1 + 3, 0x80);
    arch_outb(COM1, 3);
    arch_outb(COM1 + 1, 0);
    arch_outb(COM1 + 3, 3);
    arch_outb(COM1 + 2, 0xC7);
    arch_outb(COM1 + 4, 0x0B);
}
static void serial_write_char(char c)
{
    while ((arch_inb(COM1 + 5) & 0x20) == 0) __asm__ volatile ("pause");
    arch_outb(COM1, (uint8_t)c);
}
static void serial_write(const char *s)
{
    while (*s) {
        if (*s == '\n') serial_write_char('\r');
        serial_write_char(*s++);
    }
}
static uint32_t component(uint8_t value, uint8_t bits)
{
    if (!bits || bits > 8) return 0;
    return ((uint32_t)value * ((1u << bits) - 1u) + 127u) / 255u;
}
static void pixel(uint64_t x, uint64_t y, uint32_t color)
{
    if (!fb || x >= fb->width || y >= fb->height) return;
    uint32_t r = component((uint8_t)(color >> 16), fb->red_mask_size);
    uint32_t g = component((uint8_t)(color >> 8), fb->green_mask_size);
    uint32_t b = component((uint8_t)color, fb->blue_mask_size);
    uint32_t value = (r << fb->red_mask_shift) |
                     (g << fb->green_mask_shift) |
                     (b << fb->blue_mask_shift);
    volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)fb->address
                        + y * fb->pitch + x * (fb->bpp / 8);
    for (uint16_t i = 0; i < fb->bpp / 8; ++i)
        p[i] = (uint8_t)(value >> (i * 8));
}
static void rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t c)
{
    for (uint64_t yy = 0; yy < h; ++yy)
        for (uint64_t xx = 0; xx < w; ++xx) pixel(x + xx, y + yy, c);
}

/* 5x7 glyphs: A-Z, 0, 1, 6, 8, colon, dash, underscore and space. */
static uint8_t glyph(char c, uint8_t row)
{
    static const uint8_t alpha[26][7] = {
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
        {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
        {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
        {14,4,4,4,4,4,14},{7,2,2,2,18,18,12},
        {17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
        {14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
        {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},
        {17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
    };
    static const uint8_t zero[7]={14,17,19,21,25,17,14};
    static const uint8_t one[7]={4,12,4,4,4,4,14};
    static const uint8_t six[7]={14,16,16,30,17,17,14};
    static const uint8_t eight[7]={14,17,17,14,17,17,14};
    static const uint8_t four[7]={18,18,18,31,2,2,2};
    if (row >= 7) return 0;
    /* Input is stored in lowercase; the bitmap font is uppercase-only. */
    if (c >= 'a' && c <= 'z') c = (char)(c - ('a' - 'A'));
    if (c >= 'A' && c <= 'Z') return alpha[c-'A'][row];
    switch (c) {
        case '0': return zero[row]; case '1': return one[row];
        case '6': return six[row]; case '8': return eight[row]; case '4': return four[row];
        case ':': return (row == 2 || row == 5) ? 4 : 0;
        case '-': return row == 3 ? 14 : 0;
        case '_': return row == 6 ? 31 : 0;
        default: return 0;
    }
}
static void text(const char *s, uint64_t x, uint64_t y,
                 uint32_t color, uint32_t scale)
{
    while (*s) {
        if (*s == ' ') { x += 6 * scale; ++s; continue; }
        for (uint8_t row=0; row<7; ++row) {
            uint8_t bits=glyph(*s,row);
            for (uint8_t col=0; col<5; ++col)
                if (bits & (1u << (4-col)))
                    rect(x+col*scale,y+row*scale,scale,scale,color);
        }
        x += 6 * scale;
        ++s;
    }
}

#define KBD_DATA 0x60
#define KBD_STATUS 0x64
static char command[64];
static unsigned command_len;
static uint64_t term_x, term_y, term_w, term_h;
static uint64_t info_x, info_w;
static unsigned focused_window;
static int alt_pressed;
static uint64_t mouse_x, mouse_y;
static uint8_t mouse_packet[3];
static unsigned mouse_packet_index;

static void draw_window(uint64_t x, uint64_t y, uint64_t w, uint64_t h,
                        const char *title, unsigned id)
{
    uint32_t border = focused_window == id ? ACCENT : MUTED;
    rect(x, y, w, h, border);
    rect(x + 2, y + 2, w - 4, h - 4, PANEL);
    rect(x + 2, y + 2, w - 4, 25, focused_window == id ? 0x19364A : 0x172536);
    text(title, x + 12, y + 10, focused_window == id ? ACCENT : TEXT, 1);
    text("X", x + w - 20, y + 10, MUTED, 1);
}

static void draw_input(void)
{
    uint64_t y = term_y + term_h - 43;
    rect(term_x + 12, y, term_w - 24, 30, 0x0A1725);
    text("CARSONOS >", term_x + 22, y + 10, ACCENT, 1);
    text(command, term_x + 100, y + 10, TEXT, 1);
}

static void output_line(const char *s, unsigned row, uint32_t color)
{
    text(s, term_x + 20, term_y + 66 + row * 26, color, 2);
}

static void draw_cursor(void)
{
    for (int row = 0; row < 14; ++row) {
        for (int col = 0; col <= row / 2; ++col) {
            uint64_t x = mouse_x + (uint64_t)col;
            uint64_t y = mouse_y + (uint64_t)row;
            pixel(x, y, (col == 0 || col == row / 2) ? 0x07111D : 0xFFFFFF);
        }
    }
}

static void draw_switcher(void)
{
    const uint64_t widths[3] = {176, 216, 256};
    const uint64_t heights[3] = {62, 76, 90};
    for (unsigned frame = 0; frame < 3; ++frame) {
        uint64_t w = widths[frame], h = heights[frame];
        uint64_t x = fb->width > w ? (fb->width - w) / 2 : 0;
        uint64_t y = fb->height > h ? (fb->height - h) / 2 : 0;
        rect(x, y, w, h, ACCENT);
        rect(x + 2, y + 2, w - 4, h - 4, PANEL);
        text("ALT TAB", x + 14, y + 12, ACCENT, 1);
        text(focused_window == 0 ? "CARSONSHELL" : "SYSTEM INFO",
             x + 14, y + 36, TEXT, 1);
        for (volatile unsigned delay = 0; delay < 1800000; ++delay)
            __asm__ volatile ("pause");
    }
}

static void draw_desktop(void)
{
    rect(0, 0, fb->width, fb->height, BG);

    term_x = 24;
    term_y = 42;
    term_w = fb->width * 59 / 100;
    if (term_w < 360) term_w = 360;
    if (term_w > fb->width - 220) term_w = fb->width - 220;
    term_h = fb->height - 86;
    if (term_h < 230) term_h = fb->height - 50;
    info_x = term_x + term_w + 14;
    info_w = fb->width - info_x - 24;

    draw_window(term_x, term_y, term_w, term_h, "CARSONSHELL - TERMINAL", 0);
    draw_window(info_x, term_y + 36, info_w, 210, "SYSTEM INFO", 1);

    text("CARSONOS HYBRID", info_x + 12, term_y + 94, ACCENT, 1);
    text("KERNEL: X86_64", info_x + 12, term_y + 122, TEXT, 1);
    text("BOOT: LIMINE", info_x + 12, term_y + 150, TEXT, 1);
    text("SHELL: READY", info_x + 12, term_y + 178, TEXT, 1);

    rect(0, fb->height - 30, fb->width, 30, 0x101E30);
    rect(0, fb->height - 30, fb->width, 2, ACCENT);
    text("CARSONOS", 12, fb->height - 20, ACCENT, 1);
    text("TAB: SWITCH WINDOW", 120, fb->height - 20, MUTED, 1);

    draw_input();
    /* This lamp reports that the kernel is running, not measured CPU usage. */
    rect(fb->width > 104 ? fb->width - 104 : 0, fb->height - 21, 7, 7, 0x55D68B);
    text("CPU ACTIVE", fb->width > 91 ? fb->width - 91 : 0,
         fb->height - 20, MUTED, 1);
    draw_cursor();
}

static int command_is(const char *expected)
{
    unsigned i = 0;
    while (command[i] && expected[i] && command[i] == expected[i]) ++i;
    return command[i] == 0 && expected[i] == 0;
}
static void run_command(void)
{
    command[command_len] = 0;
    rect(term_x + 12, term_y + 54, term_w - 24, term_h - 110, PANEL);
    if (!command_len) { draw_input(); return; }
    if (command_is("help")) {
        output_line("HELP - SHOW COMMANDS", 0, TEXT);
        output_line("CLEAR - CLEAR SCREEN", 1, TEXT);
        output_line("ABOUT - ABOUT CARSONOS", 2, TEXT);
    } else if (command_is("clear")) {
        rect(term_x + 12, term_y + 54, term_w - 24, term_h - 110, PANEL);
    } else if (command_is("about")) {
        output_line("CARSONOS HYBRID", 0, ACCENT);
        output_line("A TINY X86_64 KERNEL.", 1, TEXT);
        output_line("BUILT FROM SCRATCH.", 2, TEXT);
    } else {
        output_line("UNKNOWN COMMAND - TYPE HELP", 0, MUTED);
    }
    command_len = 0;
    command[0] = 0;
    draw_input();
}
static char key_character(uint8_t sc)
{
    static const char keys[58] = {
        [16]='q',[17]='w',[18]='e',[19]='r',[20]='t',[21]='y',[22]='u',
        [23]='i',[24]='o',[25]='p',[30]='a',[31]='s',[32]='d',[33]='f',
        [34]='g',[35]='h',[36]='j',[37]='k',[38]='l',[44]='z',[45]='x',
        [46]='c',[47]='v',[48]='b',[49]='n',[50]='m',[57]=' '
    };
    return sc < 58 ? keys[sc] : 0;
}
static void keyboard_poll(void)
{
    uint8_t status = arch_inb(KBD_STATUS);
    if (!(status & 1) || (status & 0x20)) return;
    uint8_t sc = arch_inb(KBD_DATA);

    if (sc == 0x38) { alt_pressed = 1; return; }
    if (sc == 0xB8) { alt_pressed = 0; return; }
    if (sc & 0x80) return;

    if (sc == 0x0f) {
        focused_window ^= 1u;
        draw_desktop();
        if (alt_pressed) draw_switcher();
        return;
    }
    if (focused_window != 0) return;

    if (sc == 0x1c) { run_command(); return; }
    if (sc == 0x0e) {
        if (command_len) command[--command_len] = 0;
        draw_input();
        return;
    }
    char ch = key_character(sc);
    if (ch && command_len < sizeof(command)-1) {
        command[command_len++] = ch;
        command[command_len] = 0;
        draw_input();
    }
}

#define MOUSE_STATUS 0x64
#define MOUSE_DATA 0x60

static int ps2_wait_input(void)
{
    for (unsigned i = 0; i < 100000; ++i) {
        if (!(arch_inb(MOUSE_STATUS) & 2)) return 1;
        __asm__ volatile ("pause");
    }
    return 0;
}
static int ps2_wait_output(void)
{
    for (unsigned i = 0; i < 100000; ++i) {
        if (arch_inb(MOUSE_STATUS) & 1) return 1;
        __asm__ volatile ("pause");
    }
    return 0;
}
static int mouse_write(uint8_t value)
{
    if (!ps2_wait_input()) return 0;
    arch_outb(MOUSE_STATUS, 0xD4);
    if (!ps2_wait_input()) return 0;
    arch_outb(MOUSE_DATA, value);
    return 1;
}
static int mouse_read_ack(void)
{
    if (!ps2_wait_output()) return 0;
    return arch_inb(MOUSE_DATA) == 0xFA;
}
static void mouse_init(void)
{
    mouse_x = fb->width / 2;
    mouse_y = fb->height / 2;
    serial_write("PS/2 mouse: init entered\n");

    if (!ps2_wait_input()) {
        serial_write("PS/2 mouse: controller busy before enable\n");
        return;
    }

    arch_outb(MOUSE_STATUS, 0xA8);
    serial_write("PS/2 mouse: auxiliary port enable command sent\n");

    if (!ps2_wait_input()) {
        serial_write("PS/2 mouse: controller busy before device command\n");
        return;
    }

    if (!mouse_write(0xF4)) {
        serial_write("PS/2 mouse: could not send enable command\n");
        return;
    }
    serial_write("PS/2 mouse: enable command sent\n");

    if (!mouse_read_ack()) {
        serial_write("PS/2 mouse: no ACK received\n");
        return;
    }

    serial_write("PS/2 mouse: polling enabled\n");
}
static void mouse_poll(void)
{
    uint8_t status = arch_inb(MOUSE_STATUS);
    if (!(status & 1) || !(status & 0x20)) return;
    uint8_t data = arch_inb(MOUSE_DATA);
    if (mouse_packet_index == 0 && !(data & 0x08)) return;
    mouse_packet[mouse_packet_index++] = data;
    if (mouse_packet_index < 3) return;
    mouse_packet_index = 0;
    if (mouse_packet[0] & 0xC0) return;
    int dx = (int)(int8_t)mouse_packet[1];
    int dy = (int)(int8_t)mouse_packet[2];
    int64_t nx = (int64_t)mouse_x + dx;
    int64_t ny = (int64_t)mouse_y - dy;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx >= (int64_t)fb->width) nx = (int64_t)fb->width - 1;
    if (ny >= (int64_t)fb->height) ny = (int64_t)fb->height - 1;
    if ((uint64_t)nx == mouse_x && (uint64_t)ny == mouse_y) return;
    mouse_x = (uint64_t)nx;
    mouse_y = (uint64_t)ny;
    draw_desktop();
}

static void framebuffer_init(void)
{
    struct limine_framebuffer_response *response = framebuffer_request.response;
    if (!response || !response->framebuffer_count || !response->framebuffers ||
        !response->framebuffers[0]) {
        serial_write("Framebuffer: unavailable\n");
        return;
    }

    fb = response->framebuffers[0];
    if (!fb->address || !fb->width || !fb->height ||
        (fb->bpp != 24 && fb->bpp != 32) || fb->memory_model != 1) {
        fb = 0;
        serial_write("Framebuffer: unsupported format\n");
        return;
    }
    serial_write("Framebuffer: initialized\n");
}

__attribute__((noreturn))
void kernel_main(void)
{
    arch_disable_interrupts();
    serial_init();
    serial_write("\nCarsonOS Hybrid\n");
    serial_write("Boot status: kernel entry reached\n");
    framebuffer_init();
    if (fb) {
        focused_window = 0;
        mouse_init();
        draw_desktop();
        serial_write("CarsonWM: desktop ready\n");
        serial_write("CarsonShell: ready\n");
    }
    for (;;) {
        mouse_poll();
        keyboard_poll();
        __asm__ volatile ("pause");
    }
}
