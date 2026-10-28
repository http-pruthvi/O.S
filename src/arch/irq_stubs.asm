; ==============================================================================
; irq_stubs.asm - Hardware IRQ Assembly Stubs
; ==============================================================================
; Architecture: x86 32-bit protected mode
;
; Why this is needed:
; Hardware interrupts from the 8259 PIC trigger vectors 32 to 47.
; These assembly stubs save CPU register state, switch segment registers
; to kernel data, invoke the C irq_dispatch() dispatcher, and restore registers.
; ==============================================================================

section .text
extern irq_dispatch

%macro IRQ_STUB 2
global irq%1
irq%1:
    push 0                                    ; Push dummy error code
    push %2                                   ; Push interrupt vector number
    jmp irq_common_stub
%endmacro

; ------------------------------------------------------------------------------
; IRQ Stubs 0 to 15 (Vectors 32 to 47)
; ------------------------------------------------------------------------------
IRQ_STUB 0,  32                               ; IRQ 0: PIT Timer
IRQ_STUB 1,  33                               ; IRQ 1: PS/2 Keyboard
IRQ_STUB 2,  34                               ; IRQ 2: PIC Cascade Line
IRQ_STUB 3,  35                               ; IRQ 3: COM2 Serial
IRQ_STUB 4,  36                               ; IRQ 4: COM1 Serial
IRQ_STUB 5,  37                               ; IRQ 5: LPT2 / Sound
IRQ_STUB 6,  38                               ; IRQ 6: Floppy Controller
IRQ_STUB 7,  39                               ; IRQ 7: LPT1 / Master Spurious
IRQ_STUB 8,  40                               ; IRQ 8: Real-Time Clock (RTC)
IRQ_STUB 9,  41                               ; IRQ 9: ACPI / Free
IRQ_STUB 10, 42                               ; IRQ 10: Free
IRQ_STUB 11, 43                               ; IRQ 11: Free
IRQ_STUB 12, 44                               ; IRQ 12: PS/2 Mouse
IRQ_STUB 13, 45                               ; IRQ 13: FPU Math Coprocessor
IRQ_STUB 14, 46                               ; IRQ 14: Primary IDE Controller
IRQ_STUB 15, 47                               ; IRQ 15: Secondary IDE / Slave Spurious

; ------------------------------------------------------------------------------
; Common IRQ Entry Stub
; ------------------------------------------------------------------------------
irq_common_stub:
    ; 1. Save general registers
    pusha

    ; 2. Save segment registers
    push ds
    push es
    push fs
    push gs

    ; 3. Clear direction flag (DF=0) for C ABI compliance
    cld

    ; 4. Set segment registers to Kernel Data Selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 5. Push stack pointer as argument to irq_dispatch(registers_t* regs)
    push esp
    call irq_dispatch
    add esp, 4

    ; 6. Restore original segment registers
    pop gs
    pop fs
    pop es
    pop ds

    ; 7. Restore general registers
    popa

    ; 8. Clean up dummy error code and vector number (8 bytes)
    add esp, 8

    ; 9. Return from interrupt
    iret
