#include "keyboard.h"
#include "idt.h"
#include "vga.h"
#include "io.h"
#include <stddef.h>
#include <stdint.h>

#define KBD_DATA  0x60
#define KBD_STATUS 0x64

/* US keyboard scancode set 1 — lowercase */
static const char sc_lower[128] = {
    0,   27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,   'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,   '\\','z','x','c','v','b','n','m',',','.','/',
    0,   '*', 0, ' ', 0,
    /* F1-F10, num lock, scroll lock... all 0 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,
    '7','8','9','-','4','5','6','+','1','2','3','0','.',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* US keyboard scancode set 1 — uppercase / shifted */
static const char sc_upper[128] = {
    0,   27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,   'A','S','D','F','G','H','J','K','L',':','"','~',
    0,   '|','Z','X','C','V','B','N','M','<','>','?',
    0,   '*', 0, ' ', 0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,
    '7','8','9','-','4','5','6','+','1','2','3','0','.',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* Circular keyboard buffer */
#define KBD_BUF_SIZE 256
static volatile char  kbd_buf[KBD_BUF_SIZE];
static volatile size_t kbd_head = 0;
static volatile size_t kbd_tail = 0;

static int shift_held  = 0;
static int caps_active = 0;

static void kbd_irq(regs_t *r) {
    (void)r;
    uint8_t sc = inb(KBD_DATA);

    /* Key released: high bit set */
    if (sc & 0x80) {
        uint8_t released = sc & 0x7F;
        if (released == 0x2A || released == 0x36) shift_held = 0;
        return;
    }

    /* Modifiers */
    if (sc == 0x2A || sc == 0x36) { shift_held = 1; return; }
    if (sc == 0x3A) { caps_active = !caps_active; return; }

    int use_upper = (shift_held ^ caps_active);
    char c = use_upper ? sc_upper[sc] : sc_lower[sc];
    if (!c) return;

    size_t next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next != kbd_tail) {
        kbd_buf[kbd_head] = c;
        kbd_head = next;
    }
}

void keyboard_init(void) {
    irq_install_handler(1, kbd_irq);
}

char keyboard_getchar(void) {
    while (kbd_tail == kbd_head)
        __asm__ volatile ("hlt");
    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return c;
}

size_t keyboard_readline(char *buf, size_t bufsz) {
    size_t i = 0;
    while (i < bufsz - 1) {
        char c = keyboard_getchar();
        if (c == '\n' || c == '\r') {
            vga_putchar('\n');
            break;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                vga_putchar('\b');
            }
        } else {
            buf[i++] = c;
            vga_putchar(c);
        }
    }
    buf[i] = '\0';
    return i;
}
