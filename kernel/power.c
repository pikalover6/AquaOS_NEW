#include "power.h"
#include "io.h"
#include "vga.h"

/*
 * Shutdown strategy:
 *  1. QEMU/Bochs debug port 0x604 (ISA ACPI shutdown)
 *  2. ACPI via port 0xB004 (older QEMU / Bochs)
 *  3. APM via port 0x8900
 *  4. Halt loop as last resort
 */
void power_shutdown(void) {
    vga_puts("\nShutting down...\n");

    /* QEMU -machine pc */
    outw(0x604, 0x2000);

    /* Bochs / old QEMU */
    outw(0xB004, 0x2000);

    /* QEMU >= 2.0 ACPI port */
    outw(0x4004, 0x3400);

    /* APM shutdown */
    outb(0x8900, 'S');
    outb(0x8900, 'h');
    outb(0x8900, 'u');
    outb(0x8900, 't');
    outb(0x8900, 'd');
    outb(0x8900, 'o');
    outb(0x8900, 'w');
    outb(0x8900, 'n');

    /* Should not reach here */
    vga_puts("Shutdown failed — safe to power off.\n");
    for (;;) __asm__ volatile ("cli; hlt");
}
