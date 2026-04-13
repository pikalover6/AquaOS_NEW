#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void gdt_init(void);

/* Implemented in boot.asm */
extern void gdt_flush(uint32_t ptr);

#endif /* GDT_H */
