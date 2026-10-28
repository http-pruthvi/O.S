#ifndef ARCH_ISR_H
#define ARCH_ISR_H

#include "lib/types.h"

/*
 * isr.h:
 * CPU Exception & Interrupt Service Routine (ISR) definitions.
 *
 * Why this is needed:
 * When the CPU encounters a hardware exception (e.g. divide by zero, page fault)
 * or a software interrupt, it executes the corresponding IDT gate.
 * The assembly stub pushes register state onto the stack and invokes isr_handler().
 */

/*
 * registers_t:
 * Exact memory layout of the CPU registers pushed onto the stack
 * during interrupt entry and execution of our common assembly stub.
 *
 * Note: Since the kernel runs entirely in Ring 0 in Milestone 2,
 * the CPU does not switch privilege levels, so useresp and ss are NOT
 * pushed by hardware. These fields will be added in Milestone 5 when
 * Ring 3 user mode is introduced.
 */
typedef struct registers {
    /* Segment registers saved by our assembly stub */
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    /* Pushed by 'pusha' in Intel hardware order */
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_ignored; /* ESP value pushed by pusha (not restored by popa) */
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    /* Pushed by our individual ISR stub */
    uint32_t int_no;      /* Interrupt vector number (0 - 31, or 128) */
    uint32_t err_code;    /* Error code (or dummy 0 if not provided by CPU) */

    /* Pushed automatically by CPU hardware upon interrupt entry */
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} __attribute__((packed)) registers_t;

/* Initializes CPU exception gates (0-31) and software interrupt gate 128 in IDT */
void isr_init(void);

/* Common C handler called by isr_common_stub in isr_stubs.asm */
void isr_handler(registers_t* regs);

#endif /* ARCH_ISR_H */
