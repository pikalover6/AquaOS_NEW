# AquaOS — Assembly + C Rewrite

AquaOS is a bare-metal x86 operating system written in **x86 assembly and C**, reimplementing the functionality of the original Cosmos/C# version while following the conventions of how real operating systems are built.

## Architecture

| Component | File(s) | Description |
|-----------|---------|-------------|
| Bootloader | `boot/boot.asm` | GRUB Multiboot header, stack setup, `gdt_flush`/`idt_flush` stubs |
| GDT | `kernel/gdt.c/.h` | Global Descriptor Table (code + data segments, protected mode) |
| IDT + PIC | `kernel/idt.c/.h`, `kernel/isr.asm` | Interrupt Descriptor Table, 8259 PIC remapping, ISR/IRQ dispatch |
| VGA | `kernel/vga.c/.h` | Text-mode display driver (80×25, 16 colours, scrolling, cursor) |
| Keyboard | `kernel/keyboard.c/.h` | PS/2 keyboard IRQ driver (scancode set 1 → ASCII, readline) |
| Timer | `kernel/timer.c/.h` | PIT-based 1 kHz tick counter; PC-speaker beep (freq + duration) |
| RTC | `kernel/rtc.c/.h` | CMOS real-time clock read + formatted print |
| ATA | `kernel/ata.c/.h` | ATA PIO 28-bit LBA sector read/write (primary master) |
| FAT16 | `kernel/fat.c/.h` | FAT16 filesystem: ls, cd, mkdir, rmdir, create, read, write, append, unlink |
| Power | `kernel/power.c/.h` | ACPI S5 shutdown (QEMU/Bochs port trick + APM fallback) |
| Shell | `kernel/shell.c/.h` | Interactive command shell matching original AquaOS command set |
| Strings | `kernel/string.c/.h` | Freestanding string/memory library (no libc) |
| I/O | `kernel/io.h` | Inline `inb`/`outb`/`inw`/`outw`/`inl`/`outl` port I/O |

## Shell Commands

| Command | Description |
|---------|-------------|
| `help` | List all commands |
| `about` | OS information |
| `shutdown` | Power off the system |
| `clear` | Clear the screen |
| `ls` | List files and directories |
| `cd <dir>` | Change directory |
| `mkdir <dir>` | Create a directory |
| `deldir <dir>` | Delete a directory |
| `mkfile <file>` | Create an empty file |
| `delfile <file>` | Delete a file |
| `readfile <file>` | Print file contents |
| `echo <text>` | Echo text |
| `gettime` | Display current date and time (RTC) |
| `scribble` | Append text to a file |
| `erase` | Line-by-line file editor |
| `run <file>` | Execute an `.aquapp` script |
| `creator` | Display KAI ASCII art |

## Building

### Requirements

- `nasm` (>= 2.15)
- `gcc` with `-m32` / multilib support (`gcc-multilib` on Debian/Ubuntu)
- `binutils` (`ld`)
- `grub-mkrescue` + `xorriso` (for the ISO target)
- `qemu-system-i386` (for the `run` target)

### Compile

```bash
make          # produces aquaos.elf
make iso      # produces aquaos.iso (requires grub-mkrescue)
make run      # builds ISO and boots in QEMU
make clean    # remove build artefacts
```

## Boot Options

When booted, AquaOS shows a menu:

| Option | Effect |
|--------|--------|
| `1` | Boot normally (mounts FAT16 filesystem) |
| `2` | Boot without filesystem |
| `ms-dos` | Easter egg — simulates an MS-DOS startup before loading AquaOS |

## Aquapp Scripting

AquaOS supports simple `.aquapp` scripts (run with `run <filename>`):

```
info = My Program
writeline(Hello, world!)
inputget(Enter your name: )
write(You entered: )
sayinput
newline
```
