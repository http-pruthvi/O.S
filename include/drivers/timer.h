#ifndef DRIVERS_TIMER_H
#define DRIVERS_TIMER_H

#include "lib/types.h"

/*
 * timer.h:
 * 8254 Programmable Interval Timer (PIT) Driver.
 *
 * Why this is needed:
 * The PIT provides a reliable hardware clock that interrupts the CPU at a fixed
 * frequency (configured to 100 Hz = 10 ms intervals). This enables time tracking,
 * sleep delays, and will serve as the foundation for preemptive multitasking in Milestone 4.
 */

/* PIT Oscillator Base Frequency in Hz */
#define PIT_BASE_FREQUENCY 1193182

/* PIT Hardware I/O Ports */
#define PIT_CHANNEL0_DATA_PORT 0x40  /* Channel 0 data port (connected to IRQ0) */
#define PIT_COMMAND_PORT       0x43  /* Mode/Command register */

/* Mode 3 (Square Wave Generator), Channel 0, Loblyte/Hibyte, 16-bit binary (0x36) */
#define PIT_MODE_SQUARE_WAVE   0x36

/* Initializes PIT Channel 0 at the specified frequency (e.g. 100 Hz) and unmasks IRQ 0 */
void timer_init(uint32_t frequency);

/* Returns the total number of timer ticks elapsed since boot */
uint32_t timer_get_ticks(void);

/* Returns the system uptime in full elapsed seconds */
uint32_t timer_get_uptime_sec(void);

/* Formats the current uptime into a fixed-width clock string "[Uptime: MM:SS]" */
void timer_format_clock(char* buf);

/* Checks if the uptime clock needs a display refresh (used by main loop in graphics mode) */
bool timer_is_clock_dirty(void);

/* Clears the clock dirty refresh flag */
void timer_clear_clock_dirty(void);

/* Delays execution for the specified number of milliseconds using CPU halt */
void timer_sleep_ms(uint32_t ms);

#endif /* DRIVERS_TIMER_H */
