/*
 * serial2.c – COM2 interrupt-driven serial driver
 *
 * Interrupt-driven receive with a 256-byte circular ring buffer and
 * blocking (polling) transmit.  COM2 sits at I/O base 0x2F8 on IRQ 3
 * (master PIC, IDT vector 35).  The driver reuses the register-offset
 * macros already defined for COM1 in drivers/serial.h.
 */

#include "drivers/serial2.h"
#include "drivers/serial.h"     /* SERIAL_*_PORT macros, serial_writestring */
#include "lib/io.h"             /* outb(), inb()                            */
#include "lib/string.h"         /* strlen()                                 */
#include "arch/irq.h"           /* irq_register_handler()                   */
#include "arch/pic.h"           /* pic_unmask_irq()                         */

/* ------------------------------------------------------------------ */
/*  Ring buffer – written by the ISR, read by consumer functions      */
/* ------------------------------------------------------------------ */

#define SERIAL2_RING_SIZE  256

static volatile uint8_t  s_ring[SERIAL2_RING_SIZE];
static volatile uint32_t s_ring_head = 0;   /* ISR writes here  */
static volatile uint32_t s_ring_tail = 0;   /* Consumer reads here */

/* ------------------------------------------------------------------ */
/*  Line-assembly buffer – persists across serial2_read_line() calls  */
/* ------------------------------------------------------------------ */

static char     s_line_buf[256];
static uint32_t s_line_pos = 0;

/* ------------------------------------------------------------------ */
/*  Transmit helpers                                                  */
/* ------------------------------------------------------------------ */

/* LSR bit 5: Transmitter Holding Register Empty */
#define LSR_THRE  0x20

/* LSR bit 0: Data Ready */
#define LSR_DR    0x01

/*
 * wait_tx_ready – spin until the transmit holding register is empty.
 *
 * This is the same blocking strategy COM1 uses; acceptable for debug
 * output where throughput is not critical.
 */
static void wait_tx_ready(void)
{
    while ((inb(SERIAL_LSR_PORT(COM2_PORT)) & LSR_THRE) == 0) {
        /* busy-wait */
    }
}

/* ------------------------------------------------------------------ */
/*  IRQ handler                                                       */
/* ------------------------------------------------------------------ */

/*
 * serial2_drain_hardware – Reads all available bytes from UART FIFO into ring buffer.
 */
static void serial2_drain_hardware(void)
{
    while (inb(SERIAL_LSR_PORT(COM2_PORT)) & LSR_DR) {
        uint8_t  byte = inb(SERIAL_DATA_PORT(COM2_PORT));
        uint32_t next = (s_ring_head + 1) % SERIAL2_RING_SIZE;

        if (next != s_ring_tail) {          /* room in the buffer */
            s_ring[s_ring_head] = byte;
            s_ring_head = next;
        }
        /* else: buffer full – drop the byte to prevent overwrite */
    }
}

/*
 * serial2_irq_handler – IRQ 3 handler for COM2 receive.
 */
static void serial2_irq_handler(registers_t* regs)
{
    (void)regs;  /* unused – we only care about the UART registers */
    serial2_drain_hardware();
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/*
 * serial2_init – Bring up COM2 at 115200-8N1 with RX interrupts.
 *
 * The sequence mirrors the standard 16550 UART initialisation used
 * by the COM1 driver, with the addition of the IRQ handler hookup.
 */
void serial2_init(void)
{
    /* 1. Disable all UART interrupts while we reconfigure. */
    outb(SERIAL_IER_PORT(COM2_PORT), 0x00);

    /* 2. Enable DLAB (Divisor Latch Access Bit) so we can set baud. */
    outb(SERIAL_LCR_PORT(COM2_PORT), 0x80);

    /* 3. Set baud-rate divisor to 1 → 115200 baud (low byte, high byte). */
    outb(SERIAL_DATA_PORT(COM2_PORT), COM2_DIVISOR);   /* DLL = 0x01 */
    outb(SERIAL_IER_PORT(COM2_PORT), 0x00);            /* DLH = 0x00 */

    /* 4. Clear DLAB + set word length 8 bits, no parity, 1 stop bit (8N1). */
    outb(SERIAL_LCR_PORT(COM2_PORT), 0x03);

    /* 5. Enable FIFO, clear TX/RX queues, 14-byte trigger level. */
    outb(SERIAL_FIFO_PORT(COM2_PORT), 0xC7);

    /*
     * 6. Modem Control Register: assert DTR, RTS, and OUT2.
     *    OUT2 must be set on x86 PCs for the UART to deliver IRQs
     *    through the PIC – it gates the interrupt output pin.
     */
    outb(SERIAL_MCR_PORT(COM2_PORT), 0x0B);

    /* 7. Enable only the "Received Data Available" interrupt (IER bit 0). */
    outb(SERIAL_IER_PORT(COM2_PORT), 0x01);

    /* 8. Register our handler for IRQ 3 with the kernel IRQ subsystem and unmask on PIC. */
    irq_register_handler(COM2_IRQ, serial2_irq_handler);
    pic_unmask_irq(COM2_IRQ);

    /*
     * 9. Drain any stale bytes sitting in the FIFO from before we
     *    initialised.  Without this the first IRQ could deliver
     *    garbage left over from a previous boot or reset.
     */
    while (inb(SERIAL_LSR_PORT(COM2_PORT)) & LSR_DR) {
        (void)inb(SERIAL_DATA_PORT(COM2_PORT));
    }

    /* 10. Announce on COM1 so it shows in the debug serial log. */
    serial_writestring("[SERIAL2] COM2 initialized at 0x2F8, IRQ 3, 115200 baud\n");
}

/*
 * serial2_has_data – Returns true if the ring buffer is non-empty.
 */
bool serial2_has_data(void)
{
    serial2_drain_hardware();
    return s_ring_head != s_ring_tail;
}

/*
 * serial2_read_byte – Pop one byte from the ring buffer.
 *
 * Pre-condition: serial2_has_data() returned true.
 */
uint8_t serial2_read_byte(void)
{
    uint8_t byte = s_ring[s_ring_tail];
    s_ring_tail = (s_ring_tail + 1) % SERIAL2_RING_SIZE;
    return byte;
}

/*
 * serial2_read_line – Assemble a complete '\n'-terminated line.
 *
 * Drains available bytes from the ring buffer into a persistent
 * line-assembly buffer (s_line_buf).  If a newline is found the
 * completed line is copied to `buf` (null-terminated, '\n' stripped)
 * and its length is returned.  If no complete line is ready yet the
 * function returns 0 and leaves partial data in the assembly buffer
 * for the next call.
 *
 * Parameters:
 *   buf – caller-supplied destination (at least `max` bytes)
 *   max – capacity of `buf` including the null terminator
 *
 * Returns: length of the line (>0) on success, 0 if incomplete.
 */
int serial2_read_line(char* buf, size_t max)
{
    /* Pull bytes from the ring buffer into the line-assembly buffer. */
    while (serial2_has_data()) {
        uint8_t byte = serial2_read_byte();

        if (byte == '\n') {
            /*
             * Complete line found.  Copy up to max-1 characters into
             * the caller's buffer and null-terminate.
             */
            uint32_t copy_len = s_line_pos;
            if (copy_len >= max) {
                copy_len = (uint32_t)(max - 1);
            }

            for (uint32_t i = 0; i < copy_len; i++) {
                buf[i] = s_line_buf[i];
            }
            buf[copy_len] = '\0';

            /* Reset the assembly buffer for the next line. */
            s_line_pos = 0;
            return (int)copy_len;
        }

        /* Guard against overflowing the assembly buffer. */
        if (s_line_pos < sizeof(s_line_buf) - 1) {
            s_line_buf[s_line_pos++] = (char)byte;
        }
        /* else: line too long – drop excess characters */
    }

    /* No newline encountered yet; partial data stays in s_line_buf. */
    return 0;
}

/*
 * serial2_write – Blocking transmit of `len` bytes over COM2.
 *
 * Each byte waits for the Transmitter Holding Register to empty
 * before being sent.
 */
void serial2_write(const char* data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        wait_tx_ready();
        outb(SERIAL_DATA_PORT(COM2_PORT), (uint8_t)data[i]);
    }
}

/*
 * serial2_write_string – Transmit a null-terminated string over COM2.
 */
void serial2_write_string(const char* str)
{
    serial2_write(str, strlen(str));
}
