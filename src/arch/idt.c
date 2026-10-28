#include "arch/idt.h"
#include "lib/string.h"

/* 256 IDT descriptors and IDTR pointer */
static idt_entry_t idt_entries[IDT_ENTRY_COUNT];
static idt_ptr_t   idt_desc;

/*
 * idt_load:
 * Inline assembly helper to load the IDTR register using 'lidt'.
 */
static inline void idt_load(const idt_ptr_t* ptr) {
    __asm__ volatile ("lidt (%0)" : : "r"(ptr));
}

/*
 * idt_set_gate:
 * Configures a single IDT descriptor entry.
 *
 * Parameters:
 *   num       - Interrupt vector number (0 - 255)
 *   base      - Physical entry address of assembly ISR stub
 *   sel       - Kernel code segment selector (0x08)
 *   flags     - Gate attributes (e.g. 0x8E for 32-bit Ring 0 interrupt gate)
 */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].offset_low  = (uint16_t)(base & 0xFFFF);
    idt_entries[num].offset_high = (uint16_t)((base >> 16) & 0xFFFF);
    idt_entries[num].selector    = sel;
    idt_entries[num].zero        = 0;
    idt_entries[num].type_attr   = flags;
}

/*
 * idt_init:
 * Clears the 256-entry IDT table and loads the IDTR register.
 */
void idt_init(void) {
    idt_desc.limit = (uint16_t)(sizeof(idt_entry_t) * IDT_ENTRY_COUNT - 1);
    idt_desc.base  = (uint32_t)&idt_entries;

    /* Initialize all 256 gates to zero (unhandled interrupts will trigger double fault / GPF) */
    memset(&idt_entries, 0, sizeof(idt_entry_t) * IDT_ENTRY_COUNT);

    /* Load the IDT into the CPU */
    idt_load(&idt_desc);
}
