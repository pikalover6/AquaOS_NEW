# AquaOS Makefile
# Builds a Multiboot-compliant kernel ELF then creates a bootable ISO image
# using GRUB (grub-mkrescue).  Targets also support checking/running with QEMU.
#
# Requirements:
#   nasm, gcc (with -m32 / multilib), ld, grub-mkrescue, xorriso, qemu-system-i386

CC      := gcc
AS      := nasm
LD      := ld

CFLAGS  := -m32 -std=c99 -ffreestanding -O2 -Wall -Wextra \
           -Wno-unused-parameter \
           -fno-stack-protector -fno-builtin \
           -Ikernel

ASFLAGS := -f elf32
LDFLAGS := -m elf_i386 -T link.ld --oformat elf32-i386

KERNEL  := aquaos.elf
ISO     := aquaos.iso

# Source files
C_SRCS  := kernel/kernel.c \
            kernel/gdt.c   \
            kernel/idt.c   \
            kernel/vga.c   \
            kernel/string.c \
            kernel/timer.c  \
            kernel/keyboard.c \
            kernel/rtc.c   \
            kernel/ata.c   \
            kernel/fat.c   \
            kernel/power.c \
            kernel/shell.c

ASM_SRCS := boot/boot.asm \
            kernel/isr.asm

C_OBJS   := $(C_SRCS:.c=.o)
ASM_OBJS := $(ASM_SRCS:.asm=.o)
OBJS     := $(ASM_OBJS) $(C_OBJS)

.PHONY: all clean iso run

all: $(KERNEL)

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

iso: $(KERNEL)
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/aquaos.elf
	printf 'set timeout=0\nset default=0\n\nmenuentry "AquaOS" {\n    multiboot /boot/aquaos.elf\n    boot\n}\n' \
	    > isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir

run: iso
	qemu-system-i386 -cdrom $(ISO) -m 32M

clean:
	rm -f $(OBJS) $(KERNEL) $(ISO)
	rm -rf isodir
