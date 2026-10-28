#include "drivers/timer.h"
#include "arch/irq.h"
#include "arch/pic.h"
#include "drivers/vga.h"
#include "lib/io.h"

/* Global tick counter incremented on every IRQ 0 tick */
static volatile uint32_t timer_ticks = 0;
static uint32_t timer_frequency = 100;

/*
 * format_uptime_clock:
 * Formats uptime into a fixed-width clock string "[Uptime: MM:SS]"
 * without using libc or calling kprintf from interrupt context.
 */
static void format_uptime_clock(uint32_t total_sec, char* buf) {
    uint32_t mins = total_sec / 60;
    uint32_t secs = total_sec % 60;

    buf[0] = '[';
    buf[1] = 'U'; buf[2] = 'p'; buf[3] = 't'; buf[4] = 'i'; buf[5] = 'm'; buf[6] = 'e';
    buf[7] = ':'; buf[8] = ' ';
    buf[9]  = (char)('0' + ((mins / 10) % 10));
    buf[10] = (char)('0' + (mins % 10));
    buf[11] = ':';
    buf[12] = (char)('0' + ((secs / 10) % 10));
    buf[13] = (char)('0' + (secs % 10));
    buf[14] = ']';
    buf[15] = '\0';
}

/*
 * timer_handler:
 * Hardware IRQ 0 interrupt callback.
 * Invoked 100 times per second.
 *
 * Rule: kprintf is NEVER called inside this ISR handler.
 * Only the tick counter and the top-right screen clock are updated.
 */
static void timer_handler(registers_t* regs) {
    (void)regs; /* Unused parameter */
    timer_ticks++;

    /* Update top-right screen clock once per second (every 100 ticks) */
    if ((timer_ticks % timer_frequency) == 0) {
        char clock_buf[16];
        uint32_t seconds = timer_ticks / timer_frequency;
        format_uptime_clock(seconds, clock_buf);

        /* Write clock in top-right corner (column 65, row 0) in White on Blue (0x1F) */
        vga_put_at(65, 0, clock_buf, 0x1F);
    }
}

/*
 * timer_init:
 * Configures PIT Channel 0 for Mode 3 square wave generator at requested frequency,
 * registers timer_handler on IRQ 0, and unmasks IRQ 0 in the PIC.
 */
void timer_init(uint32_t frequency) {
    timer_frequency = (frequency > 0) ? frequency : 100;
    uint32_t divisor = PIT_BASE_FREQUENCY / timer_frequency;

    /* Clamp divisor to 16-bit range */
    if (divisor > 0xFFFF) {
        divisor = 0xFFFF;
    } else if (divisor < 1) {
        divisor = 1;
    }

    /* Send command byte (0x36: Channel 0, Lobyle/Hibyte, Mode 3, 16-bit binary) */
    outb(PIT_COMMAND_PORT, PIT_MODE_SQUARE_WAVE);

    /* Write divisor byte by byte to Channel 0 data port */
    outb(PIT_CHANNEL0_DATA_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_DATA_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    /* Register callback on IRQ 0 and unmask line */
    irq_register_handler(0, timer_handler);
    pic_unmask_irq(0);
}

/*
 * timer_get_ticks:
 * Returns the current tick count.
 */
uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

/*
 * timer_get_uptime_sec:
 * Returns elapsed uptime in full seconds.
 */
uint32_t timer_get_uptime_sec(void) {
    return timer_ticks / timer_frequency;
}

/*
 * timer_sleep_ms:
 * Suspends CPU execution until the requested number of milliseconds has elapsed.
 * Uses the 'hlt' instruction inside the loop so the CPU idles in low-power mode
 * until the next hardware interrupt fires.
 *
 * Parameters:
 *   ms - Duration to wait in milliseconds
 */
void timer_sleep_ms(uint32_t ms) {
    uint32_t ticks_to_wait = (ms * timer_frequency + 999) / 1000;
    uint32_t target_ticks  = timer_ticks + ticks_to_wait;

    while (timer_ticks < target_ticks) {
        __asm__ volatile ("hlt");
    }
}
