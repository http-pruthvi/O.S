#ifndef DRIVERS_SERIAL_H
#define DRIVERS_SERIAL_H

#include "lib/types.h"

/*
 * serial.h:
 * COM1 (UART 16550) Serial Port Driver.
 *
 * Why this is needed:
 * Serial port communication provides an essential debugging channel on x86 PCs.
 * In virtual environments like VirtualBox, COM1 can be piped directly to a host file
 * (e.g. C:\os\serial.log), capturing early boot logs even if VGA graphics fail.
 */

#define COM1_PORT 0x3F8

/* Serial Port Register Offsets from Base */
#define SERIAL_DATA_PORT(base)          ((base) + 0) /* Data register / Divisor latch low */
#define SERIAL_IER_PORT(base)           ((base) + 1) /* Interrupt Enable / Divisor latch high */
#define SERIAL_FIFO_PORT(base)          ((base) + 2) /* FIFO Control Register */
#define SERIAL_LCR_PORT(base)           ((base) + 3) /* Line Control Register */
#define SERIAL_MCR_PORT(base)           ((base) + 4) /* Modem Control Register */
#define SERIAL_LSR_PORT(base)           ((base) + 5) /* Line Status Register */

/* Initializes COM1 port to 115200 baud, 8N1, FIFO enabled. Returns 0 on success, -1 on failure */
int serial_init(void);

/* Transmits a single byte over COM1, blocking until the transmitter holding register is empty */
void serial_putchar(char c);

/* Transmits a buffer of bytes over COM1 */
void serial_write(const char* data, size_t size);

/* Transmits a null-terminated string over COM1 */
void serial_writestring(const char* str);

#endif /* DRIVERS_SERIAL_H */
