#ifndef ARCH_IRQ_H
#define ARCH_IRQ_H

#include "lib/types.h"
#include "arch/isr.h"

/*
 * irq.h:
 * Hardware Interrupt Request (IRQ) dispatcher interface.
 *
 * Why this is needed:
 * Peripheral devices (PIT timer, PS/2 keyboard, serial ports) notify the CPU
 * of hardware events via IRQ lines 0-15 mapped to IDT vectors 32-47.
 * This module manages driver registration and dispatches interrupts to their handlers.
 */

#define IRQ_COUNT 16

/* Function pointer signature for IRQ callback handlers */
typedef void (*irq_handler_t)(registers_t* regs);

/* Installs IRQ 0-15 gates in the IDT and prepares handler table */
void irq_init(void);

/* Registers a driver callback function for a specific IRQ line (0-15) */
void irq_register_handler(uint8_t irq, irq_handler_t handler);

/* Unregisters a driver callback function for an IRQ line */
void irq_unregister_handler(uint8_t irq);

/* Common IRQ dispatcher called from assembly in irq_stubs.asm */
void irq_dispatch(registers_t* regs);

#endif /* ARCH_IRQ_H */
