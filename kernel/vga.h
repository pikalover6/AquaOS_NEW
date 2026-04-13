#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>

/* VGA 16-color palette — matches .NET ConsoleColor enum values */
typedef enum {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,   /* DarkYellow */
    VGA_LIGHT_GREY    = 7,   /* Gray / White */
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,  /* DarkMagenta equivalent */
    VGA_YELLOW        = 14,  /* (light brown / yellow) */
    VGA_WHITE         = 15,
} vga_color_t;

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

void        vga_init(void);
void        vga_clear(void);
void        vga_set_color(vga_color_t fg, vga_color_t bg);
void        vga_set_fg(vga_color_t fg);
vga_color_t vga_get_fg(void);
void        vga_putchar(char c);
void        vga_puts(const char *s);
void        vga_putint(int n);

#endif /* VGA_H */
