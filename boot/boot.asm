; boot/boot.asm — Multiboot header + kernel entry point
; Also provides gdt_flush and idt_flush called from C

; Mark stack as non-executable
section .note.GNU-stack noalloc noexec nowrite progbits

MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
FLAGS    equ  MBALIGN | MEMINFO
MAGIC    equ  0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384      ; 16 KiB kernel stack
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov  esp, stack_top
    push 0              ; reset EFLAGS
    popf
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang

; void gdt_flush(uint32_t gdt_ptr)
global gdt_flush
gdt_flush:
    mov  eax, [esp+4]
    lgdt [eax]
    mov  ax, 0x10       ; data segment selector (index 2)
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    jmp  0x08:.flush    ; far jump to code segment (index 1)
.flush:
    ret

; void idt_flush(uint32_t idt_ptr)
global idt_flush
idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret
