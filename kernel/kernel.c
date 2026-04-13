#include "gdt.h"
#include "idt.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "fat.h"
#include "shell.h"
#include "string.h"
#include <stdint.h>

/* ---- Boot animation (cat ASCII art + beeps) ------------------------- */
static void boot_animation(void) {
    vga_set_fg(VGA_BLUE);
    vga_clear();
    vga_puts("      /\"; ;:; ; \"\\\n");  beep(422, 444);
    vga_puts("    (:;/\\ - /\\;;)\n");    beep(440, 444);
    vga_puts("   (:;{  o -  }:;)\n");   beep(422, 444);
    vga_puts("    (:;\\__Y__/;;)-----------,,_\n");  beep(440, 444);
    vga_puts("   (::         ;;               \\\n"); beep(422, 444);
    vga_puts("    (,,,)~(,,,)`-._______________)");  beep(440, 444);
    vga_puts("\nWelcome to AquaOS\n");   beep(422, 888);
}

/* ---- Boot menu ------------------------------------------------------ */
static void print_boot_menu(void) {
    vga_clear();
    vga_set_fg(VGA_WHITE);
    vga_puts("Aqua OS Version 0.40        ______  ______  __  __  ______  ______  ______   \n");
    vga_puts("Powered by Cosmos          /\\  __ \\/\\  __ \\/\\ \\/\\ \\/\\  __ \\/\\  __ \\/\\  ___\\  \n");
    vga_puts("Options:                   \\ \\  __ \\ \\ \\/\\_\\ \\ \\_\\ \\ \\  __ \\ \\ \\/\\ \\ \\___  \\ \n");
    vga_puts("1: Boot Aqua OS normally    \\ \\_\\ \\_\\ \\___\\_\\ \\_____\\ \\_\\ \\_\\ \\_____\\/\\_____\\ \n");
    vga_puts("2: Boot without FileSystem   \\/_/\\/_/\\/___/_/\\/_____/\\/_/\\/_/\\/_____/\\/_____/\n");
    vga_puts("Thanks to ");
    vga_set_fg(VGA_LIGHT_MAGENTA);
    vga_puts("Kammerl.de ");
    vga_set_fg(VGA_WHITE);
    vga_puts("for the logo!\n");
    vga_set_fg(VGA_BLUE);
    vga_puts("Chosen Option: ");
    vga_set_fg(VGA_WHITE);
}

static void print_aqua_logo(void) {
    vga_set_fg(VGA_LIGHT_MAGENTA);
    vga_puts("_______                     _______________\n");
    vga_puts("___    |_____ ____  _______ __  __ \\_  ___/\n");
    vga_puts("__  /| |  __ `/  / / /  __ `/  / / /____ \\ \n");
    vga_puts("_  ___ / /_/ // /_/ // /_/ // /_/ /____/ / \n");
    vga_puts("/_/  |_\\__, / \\__,_/ \\__,_/ \\____/ /____/  \n");
    vga_puts("         /_/                               \n");
}

/* ---- Kernel main ---------------------------------------------------- */
void kernel_main(void) {
    /* Hardware init */
    gdt_init();
    vga_init();
    idt_init();
    __asm__ volatile ("sti");  /* enable interrupts */
    timer_init(1000);          /* 1000 Hz PIT → 1 ms resolution */
    keyboard_init();

    /* Boot animation */
    boot_animation();

    /* Read boot option */
    vga_clear();
    print_boot_menu();
    char option[64];
    keyboard_readline(option, sizeof(option));

    int fs_enabled = 0;
    vga_clear();

    /* ---- ms-dos easter egg ---------------------------------------- */
    if (strstr(option, "ms-dos")) {
        vga_set_fg(VGA_WHITE);
        vga_puts("Starting MS-DOS...\n");
        beep(0, 0);
        vga_putchar('\n');
        vga_puts("C:\\>");
        char dummy[64]; keyboard_readline(dummy, sizeof(dummy));
        vga_puts("Starting AquaOS.exe...\n");
        beep(1975, 200); beep(1318, 200); beep(880, 200); beep(987, 300);
        vga_puts("Do you want to initiate the FAT driver? Y/n: ");
        char yesorno[4]; keyboard_readline(yesorno, sizeof(yesorno));
        beep(0, 0);
        if (strcmp(yesorno, "Y") == 0) {
            vga_puts("OK\n");
            if (fat_init() == 0) fs_enabled = 1;
        } else if (strcmp(yesorno, "n") == 0) {
            vga_puts("OK\n");
        } else {
            vga_puts("Invalid answer, not enabling...\n");
        }
        vga_clear();
        print_aqua_logo();
        vga_set_fg(VGA_CYAN);
        vga_puts("Aqua OS version 0.40 by Kai Howard.\n");
        vga_puts("For help, run command: help.\n");
        if (fs_enabled)
            vga_puts("FileSystem is not completed yet so it will be buggy.\n");
        vga_puts("Thanks to the CosmosOS team, it is the base for this project.\n");

    /* ---- option 1: normal boot ------------------------------------ */
    } else if (strstr(option, "1")) {
        if (fat_init() == 0) fs_enabled = 1;
        print_aqua_logo();
        vga_set_fg(VGA_CYAN);
        vga_puts("Aqua OS version 0.40 by Kai Howard.\n");
        vga_puts("For help, run command: help.\n");
        vga_puts("FileSystem is not completed yet so it will be buggy.\n");
        vga_puts("Thanks to the CosmosOS team, it is the base for this project.\n");

    /* ---- option 2: no filesystem ---------------------------------- */
    } else if (strstr(option, "2")) {
        print_aqua_logo();
        vga_set_fg(VGA_CYAN);
        vga_puts("Aqua OS version 0.40 by Kai Howard.\n");
        vga_puts("For help, run command: help.\n");
        vga_puts("Thanks to the CosmosOS team, it is the base for this project.\n");
        vga_set_fg(VGA_RED);
        vga_puts("WARNING: You have booted without a filesytem. DO NOT try to use it!\n");

    /* ---- invalid option ------------------------------------------ */
    } else {
        vga_set_fg(VGA_LIGHT_MAGENTA);
        vga_puts("You have not chosen a valid boot option and\n");
        vga_puts("therefore, you have booted into a broken system.\n");
        vga_puts("Please reboot.\n");
        vga_set_fg(VGA_BLUE);
        char garbage[128];
        vga_puts("AquaOS @ nowhere (reboot now!!): ");
        keyboard_readline(garbage, sizeof(garbage));
        vga_puts("I told ya to reboot!: ");
        keyboard_readline(garbage, sizeof(garbage));
        vga_puts("Sigh...: ");
        keyboard_readline(garbage, sizeof(garbage));
        vga_puts("Fine, whatever: ");
    }

    /* Enter the interactive shell */
    shell_run(fs_enabled);

    /* Should never return */
    for (;;) __asm__ volatile ("cli; hlt");
}
