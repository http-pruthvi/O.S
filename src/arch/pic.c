#include "arch/pic.h"
#include "lib/io.h"

/*
 * pic_init:
 * Reconfigures and remaps Master and Slave 8259A PICs to vectors 32-47.
 * Masks all IRQ lines by default.
 */
void pic_init(void) {
    /* 1. ICW1: Start initialization sequence in cascade mode */
    outb(PIC1_COMMAND_PORT, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND_PORT, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();

    /* 2. ICW2: Vector offset mapping */
    outb(PIC1_DATA_PORT, PIC1_VECTOR_OFFSET); /* Master vectors: 32 - 39 */
    io_wait();
    outb(PIC2_DATA_PORT, PIC2_VECTOR_OFFSET); /* Slave vectors:  40 - 47 */
    io_wait();

    /* 3. ICW3: Cascade configuration */
    outb(PIC1_DATA_PORT, 0x04);               /* Tell Master that Slave is connected to IRQ2 (bit 2 = 100b) */
    io_wait();
    outb(PIC2_DATA_PORT, 0x02);               /* Tell Slave its cascade identity is 2 */
    io_wait();

    /* 4. ICW4: Set 8086/88 microprocessor architecture mode */
    outb(PIC1_DATA_PORT, PIC_ICW4_8086);
    io_wait();
    outb(PIC2_DATA_PORT, PIC_ICW4_8086);
    io_wait();

    /* 5. Mask all hardware interrupts by default (drivers unmask lines explicitly) */
    outb(PIC1_DATA_PORT, 0xFF);
    outb(PIC2_DATA_PORT, 0xFF);
}

/*
 * pic_send_eoi:
 * Issues End of Interrupt (EOI) command to PIC controllers.
 * If the interrupt was handled by the Slave PIC (IRQs 8-15), EOI must be sent
 * to both Slave and Master controllers.
 */
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND_PORT, PIC_EOI);
    }
    outb(PIC1_COMMAND_PORT, PIC_EOI);
}

/*
 * pic_unmask_irq:
 * Clears the mask bit for a given IRQ line to enable hardware interrupts.
 */
void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t  val;

    if (irq < 8) {
        port = PIC1_DATA_PORT;
    } else {
        port = PIC2_DATA_PORT;
        irq -= 8;
        /* When enabling an IRQ on the slave, the master cascade line (IRQ 2) must also be open */
        pic_unmask_irq(2);
    }

    val = (uint8_t)(inb(port) & ~(1 << irq));
    outb(port, val);
}

/*
 * pic_mask_irq:
 * Sets the mask bit for a given IRQ line to block hardware interrupts.
 */
void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t  val;

    if (irq < 8) {
        port = PIC1_DATA_PORT;
    } else {
        port = PIC2_DATA_PORT;
        irq -= 8;
    }

    val = (uint8_t)(inb(port) | (1 << irq));
    outb(port, val);
}

/*
 * pic_is_spurious_irq7:
 * Checks whether an incoming IRQ 7 is real or spurious noise.
 * Reads the Master PIC's In-Service Register (ISR). If bit 7 is clear, it is spurious.
 */
bool pic_is_spurious_irq7(void) {
    outb(PIC1_COMMAND_PORT, PIC_OCW3_READ_ISR);
    uint8_t isr = inb(PIC1_COMMAND_PORT);
    return !(isr & (1 << 7));
}

/*
 * pic_is_spurious_irq15:
 * Checks whether an incoming IRQ 15 is real or spurious noise.
 * Reads the Slave PIC's In-Service Register (ISR). If bit 7 is clear, it is spurious.
 */
bool pic_is_spurious_irq15(void) {
    outb(PIC2_COMMAND_PORT, PIC_OCW3_READ_ISR);
    uint8_t isr = inb(PIC2_COMMAND_PORT);
    return !(isr & (1 << 7));
}
