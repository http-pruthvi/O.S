; ==============================================================================
; boot.asm - Multiboot 1 Kernel Entry Point & Stack Setup
; ==============================================================================
; Architecture: x86 32-bit (i686) protected mode
; Bootloader:   GRUB (Multiboot 1 Specification)
;
; Why this file exists:
; When GRUB loads our 32-bit ELF kernel at 1 MB, the CPU is already in 32-bit
; protected mode with paging disabled. However, there is no stack configured.
; This assembly file provides:
; 1. The Multiboot header that GRUB searches for within the first 8 KB.
; 2. A 16 KB stack allocated in the BSS section.
; 3. The entry point (_start) that initializes ESP, sets up the calling convention,
;    passes GRUB parameters to kernel_main, and safely halts if the kernel returns.
; ==============================================================================

; ------------------------------------------------------------------------------
; Multiboot 1 Header Constants
; ------------------------------------------------------------------------------
MULTIBOOT_MAGIC     equ 0x1BADB002            ; Magic number identifying Multiboot header
MULTIBOOT_PAGE_ALIGN equ 1 << 0               ; Align loaded modules on 4 KB boundaries
MULTIBOOT_MEM_INFO   equ 1 << 1               ; Provide memory map information
MULTIBOOT_FLAGS     equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEM_INFO
MULTIBOOT_CHECKSUM  equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS) ; Checksum: magic + flags + checksum == 0

; ------------------------------------------------------------------------------
; Multiboot Header Section
; Must be 4-byte aligned and located within the first 8192 bytes of the ELF file.
; ------------------------------------------------------------------------------
section .multiboot
align 4
    dd MULTIBOOT_MAGIC                        ; Magic number
    dd MULTIBOOT_FLAGS                        ; Feature flags
    dd MULTIBOOT_CHECKSUM                     ; Checksum verification

; ------------------------------------------------------------------------------
; BSS Section: Kernel Stack Allocation
; Allocate 16 KB of uninitialized memory for early execution stack.
; Stacks grow downwards from high addresses to low addresses on x86.
; ------------------------------------------------------------------------------
section .bss
align 16
stack_bottom:
    resb 16384                                ; Reserve 16 KB for kernel stack
stack_top:

; ------------------------------------------------------------------------------
; Text Section: Entry Point (_start)
; ------------------------------------------------------------------------------
section .text
global _start
extern kernel_main

_start:
    ; 1. Set up the stack pointer (ESP) to point to the top of our allocated stack.
    ;    In x86, stack grows downwards, so ESP starts at the top (highest memory address).
    mov esp, stack_top

    ; 2. Reset EFLAGS register to a known clean state (clear direction flag, etc.)
    push 0
    popf

    ; 3. Call kernel_main using the System V x86 (cdecl) calling convention.
    ;    C signature: void kernel_main(uint32_t magic, multiboot_info_t* mboot_info)
    ;    Arguments are pushed onto the stack from right to left:
    ;    - ebx holds the 32-bit physical address of the Multiboot info structure
    ;    - eax holds the Multiboot magic value (0x2BADB002)
    push ebx                                  ; Argument 2: multiboot_info pointer
    push eax                                  ; Argument 1: multiboot magic value
    call kernel_main

    ; 4. Hang safely if kernel_main ever returns.
    ;    Disable interrupts so the CPU won't wake up on timer or hardware IRQs.
    cli
.halt:
    hlt                                       ; Put CPU into low-power halt state
    jmp .halt                                 ; If an NMI wakes the CPU, halt again
