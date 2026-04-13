#include "vga.h"
#include "io.h"

static uint16_t * const vga_buf = (uint16_t *)0xB8000;
static size_t vga_row, vga_col;
static uint8_t vga_color;

static inline uint8_t mk_color(vga_color_t fg, vga_color_t bg) {
    return (uint8_t)fg | ((uint8_t)bg << 4);
}

static inline uint16_t mk_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

static void update_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_WIDTH + vga_col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_init(void) {
    vga_row   = 0;
    vga_col   = 0;
    vga_color = mk_color(VGA_WHITE, VGA_BLACK);
    vga_clear();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    vga_color = mk_color(fg, bg);
}

void vga_set_fg(vga_color_t fg) {
    vga_color = mk_color(fg, (vga_color_t)((vga_color >> 4) & 0x0F));
}

vga_color_t vga_get_fg(void) {
    return (vga_color_t)(vga_color & 0x0F);
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buf[y * VGA_WIDTH + x] = mk_entry(' ', vga_color);
    vga_row = vga_col = 0;
    update_cursor();
}

static void scroll_up(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buf[y * VGA_WIDTH + x] = vga_buf[(y + 1) * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        vga_buf[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = mk_entry(' ', vga_color);
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        if (++vga_row == VGA_HEIGHT) { scroll_up(); vga_row = VGA_HEIGHT - 1; }
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            vga_buf[vga_row * VGA_WIDTH + vga_col] = mk_entry(' ', vga_color);
        }
    } else {
        vga_buf[vga_row * VGA_WIDTH + vga_col] = mk_entry(c, vga_color);
        if (++vga_col == VGA_WIDTH) {
            vga_col = 0;
            if (++vga_row == VGA_HEIGHT) { scroll_up(); vga_row = VGA_HEIGHT - 1; }
        }
    }
    update_cursor();
}

void vga_puts(const char *s) {
    while (*s) vga_putchar(*s++);
}

void vga_putint(int n) {
    /* Handle INT_MIN specially to avoid undefined negation overflow */
    if (n == (-2147483647 - 1)) { vga_puts("-2147483648"); return; }
    if (n < 0) { vga_putchar('-'); n = -n; }
    if (n >= 10) vga_putint(n / 10);
    vga_putchar('0' + (n % 10));
}
