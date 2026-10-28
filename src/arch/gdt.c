#include "arch/gdt.h"

/* GDT entry table and pointer structure */
static gdt_entry_t gdt_entries[GDT_ENTRY_COUNT];
static gdt_ptr_t   gdt_desc;

/* External assembly routine in gdt_flush.asm */
extern void gdt_flush(uint32_t gdt_ptr_addr);

/*
 * gdt_set_gate:
 * Configures a single descriptor table entry.
 *
 * Parameters:
 *   num         - Index of the descriptor entry (0 to 2)
 *   base        - 32-bit linear base address of segment (0 for flat model)
 *   limit       - 32-bit segment limit (0xFFFFFFFF for 4 GB)
 *   access      - Access byte defining privilege, executable, and direction bits
 *   granularity - Granularity byte defining 4KB page scaling and 32-bit mode
 */
static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    /* Set base address fields */
    gdt_entries[num].base_low    = (uint16_t)(base & 0xFFFF);
    gdt_entries[num].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt_entries[num].base_high   = (uint8_t)((base >> 24) & 0xFF);

    /* Set limit fields (with 4KB page scaling, limit 0xFFFFF covers 4 GB) */
    gdt_entries[num].limit_low   = (uint16_t)(limit & 0xFFFF);

    /* Upper 4 bits of granularity byte store bits 16-19 of the limit */
    gdt_entries[num].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt_entries[num].granularity |= (granularity & 0xF0);

    /* Access privilege byte */
    gdt_entries[num].access      = access;
}

/*
 * gdt_init:
 * Builds the 3-entry flat model GDT and flushes segment registers.
 *
 * Why flat model:
 * Paging (which we will introduce in Milestone 3) handles memory protection
 * and virtual address translation. In 32-bit x86, we set segmentation to flat
 * (base=0, limit=4 GB) so that virtual addresses directly match logical offsets.
 */
void gdt_init(void) {
    /* 1. Limit is the table size in bytes minus 1 */
    gdt_desc.limit = (uint16_t)(sizeof(gdt_entry_t) * GDT_ENTRY_COUNT - 1);
    gdt_desc.base  = (uint32_t)&gdt_entries;

    /* Entry 0: Null descriptor (required by Intel x86 architecture) */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* Entry 1: Kernel Code Segment
     * Base: 0x00000000, Limit: 0xFFFFF (4 GB with 4KB page granularity)
     * Access: 0x9A = 10011010b (Present, Ring 0, Code/Data, Executable, Readable)
     * Granularity: 0xCF = 11001111b (4KB granularity, 32-bit 386 protected mode)
     */
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);

    /* Entry 2: Kernel Data Segment
     * Base: 0x00000000, Limit: 0xFFFFF (4 GB with 4KB page granularity)
     * Access: 0x92 = 10010010b (Present, Ring 0, Code/Data, Writable)
     * Granularity: 0xCF = 11001111b (4KB granularity, 32-bit 386 protected mode)
     */
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);

    /* Load GDTR register and reload segment registers */
    gdt_flush((uint32_t)&gdt_desc);
}
