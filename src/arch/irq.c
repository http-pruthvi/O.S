#include "arch/irq.h"
#include "arch/idt.h"
#include "arch/gdt.h"
#include "arch/pic.h"
#include "lib/io.h"
#include "lib/string.h"

/* External IRQ assembly stubs defined in irq_stubs.asm */
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

/* Table of registered IRQ callback functions */
static irq_handler_t irq_handlers[IRQ_COUNT];

/*
 * irq_init:
 * Installs all 16 IRQ stubs (vectors 32-47) into the IDT.
 */
void irq_init(void) {
    memset(irq_handlers, 0, sizeof(irq_handlers));

    idt_set_gate(32, (uint32_t)irq0,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(33, (uint32_t)irq1,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(34, (uint32_t)irq2,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(35, (uint32_t)irq3,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(36, (uint32_t)irq4,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(37, (uint32_t)irq5,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(38, (uint32_t)irq6,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(39, (uint32_t)irq7,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(40, (uint32_t)irq8,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(41, (uint32_t)irq9,  GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(42, (uint32_t)irq10, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(43, (uint32_t)irq11, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(44, (uint32_t)irq12, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(45, (uint32_t)irq13, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(46, (uint32_t)irq14, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
    idt_set_gate(47, (uint32_t)irq15, GDT_KERNEL_CODE_SEGMENT, IDT_ATTR_KERNEL_INT_GATE);
}

/*
 * irq_register_handler:
 * Binds an interrupt handler callback to an IRQ line (0 to 15).
 */
void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < IRQ_COUNT) {
        irq_handlers[irq] = handler;
    }
}

/*
 * irq_unregister_handler:
 * Clears the handler callback for an IRQ line.
 */
void irq_unregister_handler(uint8_t irq) {
    if (irq < IRQ_COUNT) {
        irq_handlers[irq] = NULL;
    }
}

/*
 * irq_dispatch:
 * Central hardware IRQ dispatcher invoked by irq_common_stub.
 * Handles spurious IRQ detection and dispatches to registered driver callbacks.
 */
void irq_dispatch(registers_t* regs) {
    if (!regs) {
        return;
    }

    uint8_t irq = (uint8_t)(regs->int_no - PIC1_VECTOR_OFFSET);

    /* 1. Spurious IRQ 7 check (Master PIC noise):
     * If bit 7 of Master ISR is clear, this is a spurious interrupt.
     * Do NOT send an EOI.
     */
    if (irq == 7) {
        if (pic_is_spurious_irq7()) {
            return;
        }
    }

    /* 2. Spurious IRQ 15 check (Slave PIC noise):
     * If bit 7 of Slave ISR is clear, this is a spurious interrupt.
     * Send EOI to Master only (to clear cascade IRQ 2), but NOT to Slave.
     */
    if (irq == 15) {
        if (pic_is_spurious_irq15()) {
            outb(PIC1_COMMAND_PORT, PIC_EOI);
            return;
        }
    }

    /* 3. Send standard End-of-Interrupt to acknowledge PIC before invoking driver handler.
     * Crucial for multitasking: If a driver callback (e.g. timer IRQ 0) triggers a preemptive
     * context switch (schedule()), the PIC must already be acknowledged so subsequent hardware
     * interrupts are not permanently blocked while the current task is suspended.
     */
    pic_send_eoi(irq);

    /* 4. Invoke driver callback if registered */
    if (irq_handlers[irq] != NULL) {
        irq_handlers[irq](regs);
    }
}
