; ==============================================================================
; isr_stubs.asm - Low-level CPU Exception and Interrupt Assembly Stubs
; ==============================================================================
; Architecture: x86 32-bit protected mode
;
; Why this is needed:
; When an interrupt or exception triggers, the CPU hardware pushes EFLAGS, CS,
; and EIP onto the stack (and an error code for certain exceptions).
; To invoke high-level C handlers safely:
; 1. Stubs for exceptions without an error code push a dummy 0 to keep the stack uniform.
; 2. Stubs push the vector number.
; 3. The common stub saves all general-purpose and segment registers.
; 4. Direction flag is cleared ('cld') per System V ABI requirements before calling C.
; 5. Data segment registers are set to Kernel Data (0x10).
; 6. A pointer to this stack frame is passed to isr_handler().
; ==============================================================================

section .text
extern isr_handler

; Macro for exceptions that DO NOT push an error code (pushes dummy 0)
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push 0                                    ; Push dummy error code
    push %1                                   ; Push interrupt number
    jmp isr_common_stub
%endmacro

; Macro for exceptions that DO push an error code (error code already on stack)
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push %1                                   ; Push interrupt number
    jmp isr_common_stub
%endmacro

; ------------------------------------------------------------------------------
; Exception Stubs 0 to 31
; ------------------------------------------------------------------------------
ISR_NOERRCODE 0                               ; 0: Divide By Zero (#DE)
ISR_NOERRCODE 1                               ; 1: Debug (#DB)
ISR_NOERRCODE 2                               ; 2: Non-Maskable Interrupt (NMI)
ISR_NOERRCODE 3                               ; 3: Breakpoint (#BP)
ISR_NOERRCODE 4                               ; 4: Overflow (#OF)
ISR_NOERRCODE 5                               ; 5: Bound Range Exceeded (#BR)
ISR_NOERRCODE 6                               ; 6: Invalid Opcode (#UD)
ISR_NOERRCODE 7                               ; 7: Device Not Available (#NM)
ISR_ERRCODE   8                               ; 8: Double Fault (#DF) - has error code
ISR_NOERRCODE 9                               ; 9: Coprocessor Segment Overrun
ISR_ERRCODE   10                              ; 10: Invalid TSS (#TS) - has error code
ISR_ERRCODE   11                              ; 11: Segment Not Present (#NP) - has error code
ISR_ERRCODE   12                              ; 12: Stack-Segment Fault (#SS) - has error code
ISR_ERRCODE   13                              ; 13: General Protection Fault (#GP) - has error code
ISR_ERRCODE   14                              ; 14: Page Fault (#PF) - has error code
ISR_NOERRCODE 15                              ; 15: Reserved by Intel
ISR_NOERRCODE 16                              ; 16: x87 FPU Floating-Point Error (#MF)
ISR_ERRCODE   17                              ; 17: Alignment Check (#AC) - has error code
ISR_NOERRCODE 18                              ; 18: Machine Check (#MC)
ISR_NOERRCODE 19                              ; 19: SIMD Floating-Point Exception (#XM)
ISR_NOERRCODE 20                              ; 20: Virtualization Exception (#VE)
ISR_ERRCODE   21                              ; 21: Control Protection Exception (#CP)
ISR_NOERRCODE 22                              ; 22: Reserved
ISR_NOERRCODE 23                              ; 23: Reserved
ISR_NOERRCODE 24                              ; 24: Reserved
ISR_NOERRCODE 25                              ; 25: Reserved
ISR_NOERRCODE 26                              ; 26: Reserved
ISR_NOERRCODE 27                              ; 27: Reserved
ISR_NOERRCODE 28                              ; 28: Reserved
ISR_NOERRCODE 29                              ; 29: Reserved
ISR_NOERRCODE 30                              ; 30: Security Exception
ISR_NOERRCODE 31                              ; 31: Reserved

; ------------------------------------------------------------------------------
; Software Interrupt Test Stub (Vector 128 / 0x80)
; ------------------------------------------------------------------------------
ISR_NOERRCODE 128

; ------------------------------------------------------------------------------
; Common ISR Entry Stub
; ------------------------------------------------------------------------------
isr_common_stub:
    ; 1. Save all general-purpose registers (EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX)
    pusha

    ; 2. Save user/kernel segment registers
    push ds
    push es
    push fs
    push gs

    ; 3. Clear direction flag (DF=0) as required by System V C ABI for string ops
    cld

    ; 4. Reload segment registers with Kernel Data Selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 5. Push stack pointer (ESP) as pointer argument to registers_t struct
    push esp
    call isr_handler
    add esp, 4                                ; Clean up pushed parameter

    ; 6. Restore original segment registers
    pop gs
    pop fs
    pop es
    pop ds

    ; 7. Restore general-purpose registers
    popa

    ; 8. Clean up pushed interrupt number and error code (2 * 4 = 8 bytes)
    add esp, 8

    ; 9. Return from interrupt (pops EIP, CS, EFLAGS)
    iret
