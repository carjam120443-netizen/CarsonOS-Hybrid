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

/* Render complete frames in RAM before presenting them to the framebuffer. */
#define BACKBUFFER_MAX_WIDTH 1920
#define BACKBUFFER_MAX_HEIGHT 1080
static uint32_t backbuffer[BACKBUFFER_MAX_WIDTH * BACKBUFFER_MAX_HEIGHT];
static int backbuffer_enabled;

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
static void serial_write_u64(uint64_t value)
{
    char digits[21];
    unsigned count = 0;
    if (value == 0) { serial_write_char('0'); return; }
    while (value && count < sizeof(digits)) {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    }
    while (count) serial_write_char(digits[--count]);
}
static uint32_t component(uint8_t value, uint8_t bits)
{
    if (!bits || bits > 8) return 0;
    return ((uint32_t)value * ((1u << bits) - 1u) + 127u) / 255u;
}
static void pixel(uint64_t x, uint64_t y, uint32_t color)
{
    if (!fb || x >= fb->width || y >= fb->height) return;
    if (backbuffer_enabled) {
        backbuffer[y * fb->width + x] = color;
        return;
    }
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

static void flush_region(uint64_t x, uint64_t y, uint64_t w, uint64_t h)
{
    if (!fb || !backbuffer_enabled || x >= fb->width || y >= fb->height)
        return;

    uint64_t x_end = w > fb->width - x ? fb->width : x + w;
    uint64_t y_end = h > fb->height - y ? fb->height : y + h;
    volatile uint8_t *base = (volatile uint8_t *)(uintptr_t)fb->address;

    for (uint64_t yy = y; yy < y_end; ++yy) {
        for (uint64_t xx = x; xx < x_end; ++xx) {
            uint32_t color = backbuffer[yy * fb->width + xx];
            uint32_t r = component((uint8_t)(color >> 16), fb->red_mask_size);
            uint32_t g = component((uint8_t)(color >> 8), fb->green_mask_size);
            uint32_t b = component((uint8_t)color, fb->blue_mask_size);
            uint32_t value = (r << fb->red_mask_shift) |
                             (g << fb->green_mask_shift) |
                             (b << fb->blue_mask_shift);
            volatile uint8_t *p = base + yy * fb->pitch + xx * (fb->bpp / 8);
            for (uint16_t i = 0; i < fb->bpp / 8; ++i)
                p[i] = (uint8_t)(value >> (i * 8));
        }
    }
}

static void flush_framebuffer(void)
{
    if (!fb || !backbuffer_enabled) return;
    serial_write("CarsonWM diagnostic: framebuffer flush entered\n");
    flush_region(0, 0, fb->width, fb->height);
    serial_write("CarsonWM diagnostic: framebuffer flush completed\n");
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
static int left_button_down;
static int mouse_ready;
static int app_menu_open;
static int window_visible[2] = {1, 1};
static int window_minimized[2];
static int window_maximized[2];
static uint64_t window_x[2], window_y[2], window_w[2], window_h[2];

static void draw_window(uint64_t x, uint64_t y, uint64_t w, uint64_t h,
                        const char *title, unsigned id, int diagnose)
{
    if (diagnose) serial_write(id == 0 ? "CarsonWM diagnostic: terminal window draw entered\n" : "CarsonWM diagnostic: system window draw entered\n");
    uint32_t border = focused_window == id ? ACCENT : MUTED;
    rect(x, y, w, h, border);
    if (diagnose) serial_write("CarsonWM diagnostic: window border painted\n");
    rect(x + 2, y + 2, w - 4, h - 4, PANEL);
    if (diagnose) serial_write("CarsonWM diagnostic: window body painted\n");
    rect(x + 2, y + 2, w - 4, 25, focused_window == id ? 0x19364A : 0x172536);
    if (diagnose) serial_write("CarsonWM diagnostic: window titlebar painted\n");
    text(title, x + 12, y + 10, focused_window == id ? ACCENT : TEXT, 1);
    if (diagnose) serial_write("CarsonWM diagnostic: window title painted\n");

    /* Window controls: minimize, maximize/restore, close. */
    uint64_t controls_x = x + w - 66;
    text("-", controls_x + 5, y + 10, TEXT, 1);
    rect(controls_x + 25, y + 9, 10, 10, TEXT);
    rect(controls_x + 27, y + 11, 6, 6, 0x172536);
    text("X", controls_x + 47, y + 10, TEXT, 1);
    if (diagnose) serial_write("CarsonWM diagnostic: window controls painted\n");
    if (diagnose) serial_write(id == 0 ? "CarsonWM diagnostic: terminal window draw completed\n" : "CarsonWM diagnostic: system window draw completed\n");
}

static void draw_input(int present)
{
    uint64_t y = term_y + term_h - 43;
    rect(term_x + 12, y, term_w - 24, 30, 0x0A1725);
    text("CARSONOS >", term_x + 22, y + 10, ACCENT, 1);
    text(command, term_x + 100, y + 10, TEXT, 1);
    if (present)
        flush_region(term_x + 12, y, term_w - 24, 30);
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
    /* Static overlay: no animation, delay loop, or intermediate flushes. */
    const uint64_t w = 256;
    const uint64_t h = 90;
    uint64_t x = fb->width > w ? (fb->width - w) / 2 : 0;
    uint64_t y = fb->height > h ? (fb->height - h) / 2 : 0;

    rect(x, y, w, h, ACCENT);
    rect(x + 2, y + 2, w - 4, h - 4, PANEL);
    text("ALT TAB", x + 14, y + 12, ACCENT, 1);
    text(focused_window == 0 ? "CARSONSHELL" : "SYSTEM INFO",
         x + 14, y + 36, TEXT, 1);
    flush_framebuffer();
}

static uint64_t app_menu_x(void) { return 8; }
static uint64_t app_menu_y(void) { return fb->height - 30 - 126; }

static void draw_app_menu(void)
{
    if (!app_menu_open) return;
    uint64_t x = app_menu_x();
    uint64_t y = app_menu_y();

    rect(x, y, 220, 126, ACCENT);
    rect(x + 2, y + 2, 216, 122, PANEL);
    text("BUILT IN APPS", x + 14, y + 12, ACCENT, 1);
    rect(x + 8, y + 34, 204, 1, MUTED);
    text("CARSONSHELL", x + 14, y + 48, TEXT, 1);
    text("SYSTEM INFO", x + 14, y + 84, TEXT, 1);
}

static void draw_desktop(void)
{
    static int first_draw = 1;
    int diagnose = first_draw;
    first_draw = 0;
    if (diagnose) serial_write("CarsonWM diagnostic: desktop draw entered\n");

    rect(0, 0, fb->width, fb->height, BG);
    if (diagnose) serial_write("CarsonWM diagnostic: background painted\n");
    if (diagnose) serial_write("CarsonWM diagnostic: calculating window geometry\n");

    if (diagnose) {
        serial_write("CarsonWM geometry: framebuffer width="); serial_write_u64(fb->width);
        serial_write(" height="); serial_write_u64(fb->height); serial_write("\n");
    }
    term_x = 24;
    term_y = 42;
    if (diagnose) serial_write("CarsonWM geometry: base position set\n");
    term_w = fb->width * 59 / 100;
    if (diagnose) { serial_write("CarsonWM geometry: initial terminal width="); serial_write_u64(term_w); serial_write("\n"); }
    if (fb->width > 220 && term_w > fb->width - 220) term_w = fb->width - 220;
    if (diagnose) { serial_write("CarsonWM geometry: terminal width bounded="); serial_write_u64(term_w); serial_write("\n"); }
    if (term_w < 360 && fb->width >= 384) term_w = 360;
    term_h = fb->height > 86 ? fb->height - 86 : fb->height;
    if (diagnose) { serial_write("CarsonWM geometry: terminal height="); serial_write_u64(term_h); serial_write("\n"); }
    if (term_h < 230 && fb->height > 50) term_h = fb->height - 50;
    info_x = term_x + term_w + 14;
    if (diagnose) { serial_write("CarsonWM geometry: info x="); serial_write_u64(info_x); serial_write("\n"); }
    info_w = fb->width > info_x + 24 ? fb->width - info_x - 24 : 0;
    if (diagnose) { serial_write("CarsonWM geometry: info width="); serial_write_u64(info_w); serial_write("\n"); }

    if (diagnose) serial_write("CarsonWM geometry: storing terminal X\\n");
    window_x[0] = term_x;
    if (diagnose) serial_write("CarsonWM geometry: storing terminal Y\\n");
    window_y[0] = term_y;
    if (diagnose) serial_write("CarsonWM geometry: storing terminal width\\n");
    window_w[0] = term_w;
    if (diagnose) serial_write("CarsonWM geometry: storing terminal height\\n");
    window_h[0] = term_h;
    if (diagnose) serial_write("CarsonWM geometry: storing info X\\n");
    window_x[1] = info_x;
    if (diagnose) serial_write("CarsonWM geometry: storing info Y\\n");
    window_y[1] = term_y + 36;
    if (diagnose) serial_write("CarsonWM geometry: storing info width\\n");
    window_w[1] = info_w;
    if (diagnose) serial_write("CarsonWM geometry: storing info height\\n");
    window_h[1] = 210;
    if (diagnose) serial_write("CarsonWM geometry: default window bounds stored\\n");

    if (window_maximized[0]) {
        window_x[0] = 8; window_y[0] = 8;
        window_w[0] = fb->width > 16 ? fb->width - 16 : fb->width;
        window_h[0] = fb->height > 46 ? fb->height - 46 : fb->height;
    }
    if (diagnose) serial_write("CarsonWM geometry: terminal maximize state applied\n");
    if (window_maximized[1]) {
        window_x[1] = 8; window_y[1] = 8;
        window_w[1] = fb->width > 16 ? fb->width - 16 : fb->width;
        window_h[1] = fb->height > 46 ? fb->height - 46 : fb->height;
    }
    if (diagnose) serial_write("CarsonWM geometry: system maximize state applied\n");

    /* Keep terminal content and input coordinates aligned when maximized. */
    term_x = window_x[0]; term_y = window_y[0];
    term_w = window_w[0]; term_h = window_h[0];
    if (diagnose) serial_write("CarsonWM diagnostic: window geometry calculated\n");

    if (window_visible[0] && !window_minimized[0])
        draw_window(window_x[0], window_y[0], window_w[0], window_h[0],
                    "CARSONSHELL - TERMINAL", 0, diagnose);
    if (window_visible[1] && !window_minimized[1])
        draw_window(window_x[1], window_y[1], window_w[1], window_h[1],
                    "SYSTEM INFO", 1, diagnose);
    if (diagnose) serial_write("CarsonWM diagnostic: both window draws completed\n");

    /* Keep the system information panel free of the duplicate CarsonOS label. */
    if (diagnose) serial_write("CarsonWM diagnostic: system info content starting\n");
    if (window_visible[1] && !window_minimized[1]) {
        uint64_t ix = window_x[1] + 12;
        uint64_t iy = window_y[1] + 62;
        text("KERNEL: X86_64", ix, iy, TEXT, 1);
        text("BOOT: LIMINE", ix, iy + 28, TEXT, 1);
        text("SHELL: READY", ix, iy + 56, TEXT, 1);
    }
    if (diagnose) serial_write("CarsonWM diagnostic: system info content completed\n");

    if (diagnose) serial_write("CarsonWM diagnostic: taskbar drawing started\n");
    rect(0, fb->height - 30, fb->width, 30, 0x101E30);
    rect(0, fb->height - 30, fb->width, 2, ACCENT);
    text("CARSONOS", 12, fb->height - 20, ACCENT, 1);
    text("TAB: SWITCH WINDOW", 120, fb->height - 20, MUTED, 1);
    if (diagnose) serial_write("CarsonWM diagnostic: taskbar drawing completed\n");

    if (diagnose) serial_write("CarsonWM diagnostic: app menu drawing started\n");
    draw_app_menu();
    if (diagnose) serial_write("CarsonWM diagnostic: app menu drawing completed\n");
    if (window_visible[0] && !window_minimized[0])
        draw_input(0);
    if (diagnose) serial_write("CarsonWM diagnostic: terminal input area painted\n");
    /* This lamp reports that the kernel is running, not measured CPU usage. */
    rect(fb->width > 104 ? fb->width - 104 : 0, fb->height - 21, 7, 7, 0x55D68B);
    text("CPU ACTIVE", fb->width > 91 ? fb->width - 91 : 0,
         fb->height - 20, MUTED, 1);
    if (diagnose) serial_write("CarsonWM diagnostic: CPU status lamp painted\n");
    if (diagnose) serial_write("CarsonWM diagnostic: desktop content painted\n");
    draw_cursor();
    if (diagnose) serial_write("CarsonWM diagnostic: cursor painted\n");
    if (diagnose) serial_write("CarsonWM diagnostic: about to flush framebuffer\n");
    if (diagnose) serial_write("CarsonWM diagnostic: full framebuffer flush starting\n");
    flush_framebuffer();
    if (diagnose) serial_write("CarsonWM diagnostic: full framebuffer flush returned\n");
    if (diagnose) serial_write("CarsonWM diagnostic: desktop draw completed\n");
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
    if (!command_len) { draw_input(1); return; }
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
    draw_input(0);
    flush_region(term_x + 12, term_y + 54, term_w - 24, term_h - 110);
    flush_region(term_x + 12, term_y + term_h - 43, term_w - 24, 30);
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
    if (focused_window != 0 || !window_visible[0] || window_minimized[0]) return;

    if (sc == 0x1c) { run_command(); return; }
    if (sc == 0x0e) {
        if (command_len) command[--command_len] = 0;
        draw_input(0);
        return;
    }
    char ch = key_character(sc);
    if (ch && command_len < sizeof(command)-1) {
        command[command_len++] = ch;
        command[command_len] = 0;
        draw_input(0);
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
    mouse_ready = 0;
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

    mouse_ready = 1;
    serial_write("PS/2 mouse: polling enabled\n");
}
static void handle_mouse_click(void)
{
    /* Check title-bar controls before ordinary window focus. */
    for (unsigned id = 0; id < 2; ++id) {
        if (!window_visible[id] || window_minimized[id]) continue;
        uint64_t x = window_x[id], y = window_y[id];
        uint64_t w = window_w[id];
        if (mouse_x >= x + w - 68 && mouse_x < x + w &&
            mouse_y >= y + 2 && mouse_y < y + 27) {
            uint64_t local_x = mouse_x - (x + w - 68);
            if (local_x < 22) {
                window_minimized[id] = 1;
            } else if (local_x < 44) {
                window_maximized[id] = !window_maximized[id];
            } else {
                window_visible[id] = 0;
                window_minimized[id] = 0;
            }
            focused_window = id;
            return;
        }
        if (mouse_x >= x && mouse_x < x + w &&
            mouse_y >= y && mouse_y < y + 27) {
            focused_window = id;
            return;
        }
    }

    uint64_t taskbar_y = fb->height - 30;
    if (mouse_y >= taskbar_y && mouse_x < 100) {
        app_menu_open = !app_menu_open;
        return;
    }

    if (app_menu_open) {
        uint64_t x = app_menu_x();
        uint64_t y = app_menu_y();
        if (mouse_x >= x && mouse_x < x + 220 &&
            mouse_y >= y + 36 && mouse_y < y + 70) {
            focused_window = 0;
            window_visible[0] = 1;
            window_minimized[0] = 0;
            app_menu_open = 0;
            return;
        }
        if (mouse_x >= x && mouse_x < x + 220 &&
            mouse_y >= y + 72 && mouse_y < y + 106) {
            focused_window = 1;
            window_visible[1] = 1;
            window_minimized[1] = 0;
            app_menu_open = 0;
            return;
        }
        if (mouse_x < x || mouse_x >= x + 220 ||
            mouse_y < y || mouse_y >= y + 126)
            app_menu_open = 0;
    }
}

static void mouse_poll(void)
{
    if (!mouse_ready) return;
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

    int moved = (uint64_t)nx != mouse_x || (uint64_t)ny != mouse_y;
    mouse_x = (uint64_t)nx;
    mouse_y = (uint64_t)ny;

    int left_now = (mouse_packet[0] & 1) != 0;
    int clicked = left_now && !left_button_down;
    left_button_down = left_now;

    if (clicked) {
        serial_write("CarsonWM input: left click detected\n");
        handle_mouse_click();
        serial_write("CarsonWM input: click handler returned\n");
    }
    if (moved || clicked) {
        serial_write("CarsonWM input: desktop redraw starting\n");
        draw_desktop();
        serial_write("CarsonWM input: desktop redraw returned\n");
    }
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
    backbuffer_enabled = fb->width <= BACKBUFFER_MAX_WIDTH &&
                         fb->height <= BACKBUFFER_MAX_HEIGHT;
    if (backbuffer_enabled)
        serial_write("Framebuffer: initialized (backbuffer enabled)\n");
    else
        serial_write("Framebuffer: initialized (direct-render fallback)\n");
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
        serial_write("CarsonWM diagnostic: mouse_init returned\n");
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
