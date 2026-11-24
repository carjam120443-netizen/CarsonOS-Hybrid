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
    .id = {0x9d5827dcd881dd75, 0xa3148604f6fab11b, 0, 0},
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

    rect(0,0,fb->width,fb->height,BG);
    uint64_t pw = fb->width > 900 ? 868 : fb->width - 32;
    uint64_t px = (fb->width-pw)/2;
    uint64_t py = fb->height > 500 ? (fb->height-420)/2 : 16;
    rect(px,py,pw,4,ACCENT);
    rect(px,py+4,pw,416,PANEL);

    uint32_t scale = fb->width >= 800 ? 5 : 3;
    uint64_t title_width = 8 * 6 * scale;
    text("CARSONOS",(fb->width-title_width)/2,py+55,ACCENT,scale);
    text("HYBRID KERNEL",px+32,py+145,TEXT,fb->width >= 800 ? 3 : 2);
    rect(px+32,py+190,pw-64,2,ACCENT);
    text("BOOT SUCCESSFUL",px+32,py+220,TEXT,2);
    text("ARCH: X86_64",px+32,py+260,MUTED,2);
    text("SERIAL: COM1 READY",px+32,py+290,MUTED,2);
    text("EARLY DEVELOPMENT BUILD",px+32,py+350,ACCENT,1);
    serial_write("Framebuffer: initialized\n");
}

__attribute__((noreturn))
void kernel_main(void)
{
    arch_disable_interrupts();
    serial_init();
    serial_write("\nCarsonOS Hybrid\n");
    serial_write("Architecture: x86_64\n");
    serial_write("Boot status: kernel entry reached\n");
    framebuffer_init();
    serial_write("Serial console: COM1 ready\n");
    serial_write("CPU interrupts remain disabled; entering halt loop.\n");
    for (;;) arch_halt();
}
