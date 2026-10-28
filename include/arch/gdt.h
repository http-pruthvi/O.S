#ifndef ARCH_GDT_H
#define ARCH_GDT_H

#include "lib/types.h"

/*
 * gdt.h:
 * Global Descriptor Table (GDT) interface for 32-bit x86 protected mode.
 *
 * Why this is needed:
 * In x86 protected mode, memory segmentation is always active.
 * Even though we use a flat memory model (where all segments span 0 to 4 GB),
 * the CPU requires a valid GDT to know the base, limit, access privileges,
 * and execution permissions of code and data segments.
 */

/* Standard Kernel Segment Selectors (index << 3 | TI << 2 | RPL) */
#define GDT_KERNEL_CODE_SEGMENT 0x08  /* Index 1, GDT, Ring 0 */
#define GDT_KERNEL_DATA_SEGMENT 0x10  /* Index 2, GDT, Ring 0 */

/* Number of entries in our flat GDT (Null, Kernel Code, Kernel Data) */
#define GDT_ENTRY_COUNT 3

/*
 * gdt_entry:
 * Represents a single 8-byte x86 segment descriptor.
 */
typedef struct gdt_entry {
    uint16_t limit_low;     /* Lower 16 bits of segment limit */
    uint16_t base_low;      /* Lower 16 bits of segment base address */
    uint8_t  base_middle;   /* Middle 8 bits of base address (bits 16-23) */
    uint8_t  access;        /* Access byte (present, ring, executable, etc.) */
    uint8_t  granularity;   /* Granularity flags and upper 4 bits of limit */
    uint8_t  base_high;     /* Upper 8 bits of base address (bits 24-31) */
} __attribute__((packed)) gdt_entry_t;

/*
 * gdt_ptr:
 * Passed to the 'lgdt' assembly instruction.
 */
typedef struct gdt_ptr {
    uint16_t limit;         /* Table size in bytes minus 1 */
    uint32_t base;          /* Linear physical base address of GDT */
} __attribute__((packed)) gdt_ptr_t;

/* Initializes the flat memory model GDT and flushes segment registers */
void gdt_init(void);

#endif /* ARCH_GDT_H */
