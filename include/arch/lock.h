#ifndef ARCH_LOCK_H
#define ARCH_LOCK_H

#include "lib/types.h"

/*
 * lock.h:
 * Re-entrant Interrupt Masking and Synchronization Primitives.
 *
 * Why this is needed:
 * In a multitasking OS with preemptive timer interrupts and multiple kernel tasks,
 * data structures like scheduler queues, memory allocators, and console outputs
 * can be accessed concurrently or re-entrantly.
 * On a single-CPU x86 system, disabling interrupts (CLI) guarantees mutual exclusion.
 * However, naive cli/sti breaks when functions are nested (an inner function calling
 * sti would prematurely re-enable interrupts while the outer function is still in
 * a critical section).
 *
 * The solution is irq_save() and irq_restore():
 * - irq_save() pushes EFLAGS, saves its value, and clears the Interrupt Flag (CLI).
 * - irq_restore() restores the previously saved EFLAGS. If interrupts were enabled
 *   before irq_save(), they are restored to enabled; if they were already disabled,
 *   they remain disabled.
 */

/*
 * irq_save:
 * Atomically saves the current EFLAGS register and disables hardware interrupts.
 *
 * Return:
 *   The 32-bit EFLAGS register prior to clearing the interrupt flag.
 */
static inline uint32_t irq_save(void) {
    uint32_t flags;
    /*
     * pushfl: pushes 32-bit EFLAGS onto the stack
     * popl %0: pops it into the output register 'flags'
     * cli: clears the Interrupt Flag (IF, bit 9), disabling maskable interrupts
     * "memory" clobber: acts as a compiler optimization barrier so memory operations
     * are not reordered across the lock boundary.
     */
    __asm__ volatile (
        "pushfl\n\t"
        "popl %0\n\t"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );
    return flags;
}

/*
 * irq_restore:
 * Restores the EFLAGS register from a previously saved value.
 * If the interrupt flag was set prior to irq_save(), this re-enables interrupts.
 *
 * Parameters:
 *   flags - The EFLAGS value returned by an earlier irq_save() call.
 */
static inline void irq_restore(uint32_t flags) {
    /*
     * pushl %0: pushes saved flags onto the stack
     * popfl: pops value into EFLAGS, restoring IF (bit 9) and other CPU flags
     * "cc" clobber: tells compiler condition codes are modified
     * "memory" clobber: memory barrier
     */
    __asm__ volatile (
        "pushl %0\n\t"
        "popfl"
        :
        : "r"(flags)
        : "memory", "cc"
    );
}

/*
 * irq_enable:
 * Unconditionally enables hardware interrupts (STI).
 */
static inline void irq_enable(void) {
    __asm__ volatile ("sti" : : : "memory");
}

/*
 * irq_disable:
 * Unconditionally disables hardware interrupts (CLI).
 */
static inline void irq_disable(void) {
    __asm__ volatile ("cli" : : : "memory");
}

/*
 * irq_is_enabled:
 * Checks whether hardware interrupts are currently enabled in EFLAGS.
 *
 * Return:
 *   true if bit 9 (IF) of EFLAGS is set, false otherwise.
 */
static inline bool irq_is_enabled(void) {
    uint32_t flags;
    __asm__ volatile (
        "pushfl\n\t"
        "popl %0"
        : "=r"(flags)
        :
        : "memory"
    );
    return (flags & (1 << 9)) != 0;
}

#endif /* ARCH_LOCK_H */
