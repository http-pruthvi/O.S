/*
 * serial2.h – COM2 interrupt-driven serial driver interface
 *
 * Provides an IRQ-driven receive path (256-byte ring buffer) and
 * blocking transmit for the second PC serial port (0x2F8, IRQ 3).
 * Used to receive commands or data from an external device while
 * COM1 remains the kernel debug/log channel.
 */

#ifndef DRIVERS_SERIAL2_H
#define DRIVERS_SERIAL2_H

#include "lib/types.h"

/* COM2 hardware constants */
#define COM2_PORT       0x2F8   /* Base I/O port for COM2               */
#define COM2_IRQ        3       /* IRQ line (master PIC, vector 35)     */
#define COM2_BAUD       115200  /* Target baud rate                     */
#define COM2_DIVISOR    1       /* 115200 / 115200 = 1                  */

/*
 * serial2_init – Initialise the COM2 UART.
 *
 * Configures 115200-8N1, enables the receive-data-available interrupt,
 * registers the IRQ 3 handler, and drains any stale FIFO bytes.
 */
void serial2_init(void);

/*
 * serial2_has_data – Check whether the ring buffer contains data.
 *
 * Returns: true if at least one byte is available to read.
 */
bool serial2_has_data(void);

/*
 * serial2_read_byte – Pop one byte from the receive ring buffer.
 *
 * The caller MUST check serial2_has_data() first; behaviour is
 * undefined if the buffer is empty.
 *
 * Returns: the next byte in FIFO order.
 */
uint8_t serial2_read_byte(void);

/*
 * serial2_read_line – Assemble a complete line from received data.
 *
 * Drains the ring buffer into an internal line-assembly buffer.
 * When a newline ('\n') is found the line (without the newline) is
 * copied into `buf`, null-terminated, and its length returned.
 *
 * If no complete line is available yet the function returns 0 and
 * does NOT consume partial data from the assembly buffer.
 *
 * Parameters:
 *   buf – destination buffer (must hold at least `max` bytes)
 *   max – maximum bytes to write, including the null terminator
 *
 * Returns: length of the completed line (>0), or 0 if incomplete.
 */
int serial2_read_line(char* buf, size_t max);

/*
 * serial2_write – Transmit raw bytes over COM2 (blocking).
 *
 * Parameters:
 *   data – pointer to the byte array
 *   len  – number of bytes to send
 */
void serial2_write(const char* data, size_t len);

/*
 * serial2_write_string – Transmit a null-terminated string over COM2.
 *
 * Convenience wrapper around serial2_write().
 */
void serial2_write_string(const char* str);

#endif /* DRIVERS_SERIAL2_H */
