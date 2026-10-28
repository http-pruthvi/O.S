#ifndef ARCH_IDT_H
#define ARCH_IDT_H

#include "lib/types.h"

/*
 * idt.h:
 * Interrupt Descriptor Table (IDT) interface for 32-bit x86.
 *
 * Why this is needed:
 * When a hardware interrupt or CPU exception occurs, the CPU indexes into the IDT
 * to find the entry point (Interrupt Service Routine) associated with that vector.
 */

#define IDT_ENTRY_COUNT 256

/* IDT Gate Type and Attribute Flags */
#define IDT_FLAG_PRESENT        0x80  /* Gate is present in memory (bit 7) */
#define IDT_FLAG_RING0          0x00  /* Descriptor Privilege Level 0 (bits 5-6) */
#define IDT_FLAG_RING3          0x60  /* Descriptor Privilege Level 3 (bits 5-6) */
#define IDT_FLAG_INT_GATE_32    0x0E  /* 32-bit Interrupt Gate (clears IF on entry) */
#define IDT_FLAG_TRAP_GATE_32   0x0F  /* 32-bit Trap Gate (leaves IF unchanged) */

/* Standard Kernel 32-bit Interrupt Gate: Present, Ring 0, Interrupt Gate (0x8E) */
#define IDT_ATTR_KERNEL_INT_GATE (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INT_GATE_32)

/* Standard User 32-bit Interrupt Gate (e.g. for int 0x80): Present, Ring 3 (0xEE) */
#define IDT_ATTR_USER_INT_GATE   (IDT_FLAG_PRESENT | IDT_FLAG_RING3 | IDT_FLAG_INT_GATE_32)

/*
 * idt_entry:
 * Represents one 8-byte interrupt gate descriptor.
 */
typedef struct idt_entry {
    uint16_t offset_low;    /* Lower 16 bits of ISR handler address */
    uint16_t selector;      /* Kernel code segment selector (0x08) */
    uint8_t  zero;          /* Unused reserved byte (must be zero) */
    uint8_t  type_attr;     /* Gate type and attribute flags (e.g. 0x8E) */
    uint16_t offset_high;   /* Upper 16 bits of ISR handler address */
} __attribute__((packed)) idt_entry_t;

/*
 * idt_ptr:
 * Passed to the 'lidt' assembly instruction.
 */
typedef struct idt_ptr {
    uint16_t limit;         /* Size of IDT in bytes minus 1 */
    uint32_t base;          /* Linear address of IDT */
} __attribute__((packed)) idt_ptr_t;

/* Sets an entry in the IDT */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

/* Initializes the IDT table with zeroed gates and loads IDTR */
void idt_init(void);

#endif /* ARCH_IDT_H */
