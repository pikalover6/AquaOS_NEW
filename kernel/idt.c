#include "idt.h"
#include "io.h"
#include "vga.h"
#include <stdint.h>
#include <stddef.h>

/* ---- IDT entry & pointer -------------------------------------------- */
struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

static void idt_set_gate(uint8_t num, void (*base)(void), uint16_t sel, uint8_t flags) {
    uint32_t b = (uint32_t)base;
    idt[num].base_lo = (uint16_t)(b & 0xFFFF);
    idt[num].base_hi = (uint16_t)((b >> 16) & 0xFFFF);
    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags;
}

/* ---- 8259 PIC remapping --------------------------------------------- */
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

static void pic_remap(void) {
    outb(PIC1_CMD,  0x11); io_wait();  /* init */
    outb(PIC2_CMD,  0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();  /* IRQ0-7  → INT 32-39 */
    outb(PIC2_DATA, 0x28); io_wait();  /* IRQ8-15 → INT 40-47 */
    outb(PIC1_DATA, 0x04); io_wait();  /* cascade */
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();  /* 8086 mode */
    outb(PIC2_DATA, 0x01); io_wait();
    outb(PIC1_DATA, 0x00);             /* unmask all IRQs */
    outb(PIC2_DATA, 0x00);
}

/* ---- Exception messages --------------------------------------------- */
static const char *exc_msgs[] = {
    "Division By Zero", "Debug", "NMI", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode",
    "No Coprocessor", "Double Fault", "Coprocessor Segment Overrun",
    "Bad TSS", "Segment Not Present", "Stack Fault",
    "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved"
};

void isr_handler(regs_t *r) {
    vga_set_fg(VGA_RED);
    vga_puts("\nException: ");
    if (r->int_no < 32) vga_puts(exc_msgs[r->int_no]);
    vga_puts("\nSystem halted.\n");
    for (;;) __asm__ volatile ("cli; hlt");
}

/* ---- IRQ dispatch table --------------------------------------------- */
static irq_handler_t irq_handlers[16];

void irq_install_handler(int irq, irq_handler_t h) {
    irq_handlers[irq] = h;
}

void irq_handler(regs_t *r) {
    /* Send EOI */
    if (r->int_no >= 40) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);

    int irq = (int)r->int_no - 32;
    if (irq >= 0 && irq < 16 && irq_handlers[irq])
        irq_handlers[irq](r);
}

/* ---- idt_init -------------------------------------------------------- */
void idt_init(void) {
    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uint32_t)&idt;

    /* Clear table */
    for (int i = 0; i < 256; i++) {
        idt[i].base_lo = 0; idt[i].sel = 0;
        idt[i].always0 = 0; idt[i].flags = 0; idt[i].base_hi = 0;
    }

    pic_remap();

#define SET(n, fn) idt_set_gate(n, fn, 0x08, 0x8E)
    SET( 0, isr0);  SET( 1, isr1);  SET( 2, isr2);  SET( 3, isr3);
    SET( 4, isr4);  SET( 5, isr5);  SET( 6, isr6);  SET( 7, isr7);
    SET( 8, isr8);  SET( 9, isr9);  SET(10, isr10); SET(11, isr11);
    SET(12, isr12); SET(13, isr13); SET(14, isr14); SET(15, isr15);
    SET(16, isr16); SET(17, isr17); SET(18, isr18); SET(19, isr19);
    SET(20, isr20); SET(21, isr21); SET(22, isr22); SET(23, isr23);
    SET(24, isr24); SET(25, isr25); SET(26, isr26); SET(27, isr27);
    SET(28, isr28); SET(29, isr29); SET(30, isr30); SET(31, isr31);
    SET(32, irq0);  SET(33, irq1);  SET(34, irq2);  SET(35, irq3);
    SET(36, irq4);  SET(37, irq5);  SET(38, irq6);  SET(39, irq7);
    SET(40, irq8);  SET(41, irq9);  SET(42, irq10); SET(43, irq11);
    SET(44, irq12); SET(45, irq13); SET(46, irq14); SET(47, irq15);
#undef SET

    idt_flush((uint32_t)&idtp);
}
