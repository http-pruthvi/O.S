#ifndef LIB_IO_H
#define LIB_IO_H

#include "types.h"

/*
 * io.h:
 * Hardware Port I/O Helpers for x86.
 *
 * Why this is needed:
 * The x86 architecture uses a separate I/O address space (ports 0x0000 to 0xFFFF)
 * distinct from physical memory. The CPU accesses these ports using specialized
 * assembly instructions ('in' and 'out'). Since C has no native port I/O syntax,
 * we wrap these instructions in static inline functions with GCC inline assembly.
 */

/*
 * outb:
 * Sends an 8-bit byte value to an I/O port.
 *
 * Parameters:
 *   port - The 16-bit hardware port address (e.g. 0x3F8 for COM1).
 *   val  - The 8-bit data byte to write to the port.
 */
static inline void outb(uint16_t port, uint8_t val) {
    /* "a"(val): put val in register AL.
     * "Nd"(port): put port in register DX (or use 8-bit constant if <= 255).
     * "volatile": prevents the compiler from optimizing out or reordering this I/O.
     */
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * inb:
 * Reads an 8-bit byte value from an I/O port.
 *
 * Parameters:
 *   port - The 16-bit hardware port address.
 *
 * Return:
 *   The 8-bit data byte read from the hardware.
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    /* "=a"(ret): read result from register AL into variable ret.
     * "Nd"(port): put port in register DX.
     */
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * outw:
 * Sends a 16-bit word value to an I/O port.
 */
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * inw:
 * Reads a 16-bit word value from an I/O port.
 */
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * outl:
 * Sends a 32-bit double-word value to an I/O port.
 */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * inl:
 * Reads a 32-bit double-word value from an I/O port.
 */
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * io_wait:
 * Waits for an I/O operation to complete by writing to an unused port.
 *
 * Why this is needed:
 * Some legacy hardware components (like the programmable interrupt controller)
 * require a brief settling time between successive port operations.
 * Port 0x80 is traditionally used for POST checkpoint codes and is safe to write to,
 * providing approximately a 1-4 microsecond bus delay.
 */
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif /* LIB_IO_H */
