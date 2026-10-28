#include "drivers/serial.h"
#include "lib/io.h"
#include "lib/string.h"

/* Flag indicating whether COM1 was successfully initialized */
static bool serial_ready = false;

/*
 * serial_is_transmit_empty:
 * Checks whether the UART transmit holding register is empty and ready for a new byte.
 * Bit 5 of Line Status Register (LSR) is set to 1 when empty.
 */
static inline int serial_is_transmit_empty(void) {
    return inb(SERIAL_LSR_PORT(COM1_PORT)) & 0x20;
}

/*
 * serial_init:
 * Initializes COM1 serial port to 115200 baud, 8 data bits, no parity, 1 stop bit (8N1).
 *
 * Why this is needed:
 * Hardware UART chips power on in an unconfigured state. We must set baud rate divisor,
 * frame format, and enable FIFO buffers before transmission can work reliably.
 *
 * Return:
 *   0 on success
 */
int serial_init(void) {
    /* 1. Disable all UART interrupts during early boot (we use polling transmission) */
    outb(SERIAL_IER_PORT(COM1_PORT), 0x00);

    /* 2. Enable Divisor Latch Access Bit (DLAB, bit 7) to configure baud rate */
    outb(SERIAL_LCR_PORT(COM1_PORT), 0x80);

    /* 3. Set baud rate divisor to 1 (115200 baud: 115200 / 1 = 115200 baud) */
    outb(SERIAL_DATA_PORT(COM1_PORT), 0x01); /* Low byte divisor */
    outb(SERIAL_IER_PORT(COM1_PORT), 0x00);  /* High byte divisor */

    /* 4. Configure frame format: 8 data bits, no parity, 1 stop bit (8N1).
     * Writing 0x03 also automatically clears the DLAB bit. */
    outb(SERIAL_LCR_PORT(COM1_PORT), 0x03);

    /* 5. Enable FIFO, clear transmit/receive queues, and set 14-byte threshold (0xC7) */
    outb(SERIAL_FIFO_PORT(COM1_PORT), 0xC7);

    /* 6. Modem Control Register: set Data Terminal Ready (DTR) and Request to Send (RTS),
     * and enable auxiliary output 2 (OUT2) for hardware interrupts */
    outb(SERIAL_MCR_PORT(COM1_PORT), 0x0B);

    serial_ready = true;
    return 0;
}

/*
 * serial_putchar:
 * Transmits a single byte over COM1.
 * Blocks until the UART transmitter holding register is empty.
 * Translates '\n' to "\r\n" for proper line rendering in serial terminal logs.
 *
 * Parameters:
 *   c - Character to transmit
 */
void serial_putchar(char c) {
    if (!serial_ready) {
        return;
    }

    /* Convert bare linefeed to CRLF for serial console compatibility */
    if (c == '\n') {
        while (!serial_is_transmit_empty()) {
            /* Busy-wait until transmit buffer is ready */
        }
        outb(SERIAL_DATA_PORT(COM1_PORT), '\r');
    }

    while (!serial_is_transmit_empty()) {
        /* Busy-wait until transmit buffer is ready */
    }
    outb(SERIAL_DATA_PORT(COM1_PORT), (uint8_t)c);
}

/*
 * serial_write:
 * Outputs a buffer of characters of specified length to COM1.
 *
 * Parameters:
 *   data - Pointer to character buffer
 *   size - Number of bytes to transmit
 */
void serial_write(const char* data, size_t size) {
    if (!data) return;
    for (size_t i = 0; i < size; i++) {
        serial_putchar(data[i]);
    }
}

/*
 * serial_writestring:
 * Outputs a null-terminated string to COM1.
 *
 * Parameters:
 *   str - Pointer to null-terminated ASCII string
 */
void serial_writestring(const char* str) {
    if (!str) return;
    serial_write(str, strlen(str));
}
