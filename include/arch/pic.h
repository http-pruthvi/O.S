#ifndef ARCH_PIC_H
#define ARCH_PIC_H

#include "lib/types.h"

/*
 * pic.h:
 * 8259A Programmable Interrupt Controller (PIC) interface.
 *
 * Why this is needed:
 * The PC architecture uses two cascaded 8259A PIC chips to handle hardware interrupts.
 * By default, the BIOS maps Master IRQs 0-7 to CPU interrupt vectors 8-15.
 * In 32-bit protected mode, vectors 8-15 conflict directly with CPU exceptions
 * (such as #DF Double Fault and #GP General Protection Fault).
 * Therefore, we must remap Master IRQs to vectors 32-39 and Slave IRQs to vectors 40-47.
 */

/* Master PIC I/O Ports */
#define PIC1_COMMAND_PORT 0x20
#define PIC1_DATA_PORT    0x21

/* Slave PIC I/O Ports */
#define PIC2_COMMAND_PORT 0xA0
#define PIC2_DATA_PORT    0xA1

/* Vector Offsets after remapping */
#define PIC1_VECTOR_OFFSET 32  /* Master IRQ 0-7 mapped to vectors 32-39 */
#define PIC2_VECTOR_OFFSET 40  /* Slave IRQ 8-15 mapped to vectors 40-47 */

/* PIC Commands and Initialization Words */
#define PIC_EOI             0x20  /* End-of-Interrupt command code */
#define PIC_ICW1_ICW4       0x01  /* Flag indicating ICW4 is required */
#define PIC_ICW1_INIT       0x10  /* Initialization command */
#define PIC_ICW4_8086       0x01  /* 8086/88 mode */
#define PIC_OCW3_READ_ISR   0x0B  /* OCW3 command to read In-Service Register (ISR) */

/* Initializes and remaps both PICs to vectors 32-47 and masks all IRQ lines */
void pic_init(void);

/* Sends End of Interrupt (EOI) to appropriate PIC chips */
void pic_send_eoi(uint8_t irq);

/* Unmasks a specific IRQ line to allow hardware interrupts */
void pic_unmask_irq(uint8_t irq);

/* Masks a specific IRQ line to block hardware interrupts */
void pic_mask_irq(uint8_t irq);

/* Checks if an incoming IRQ 7 is a spurious interrupt from Master PIC */
bool pic_is_spurious_irq7(void);

/* Checks if an incoming IRQ 15 is a spurious interrupt from Slave PIC */
bool pic_is_spurious_irq15(void);

#endif /* ARCH_PIC_H */
