; ==============================================================================
; gdt_flush.asm - GDT Loading and Segment Register Reloading
; ==============================================================================
; Architecture: x86 32-bit protected mode
;
; Why this is needed:
; Executing 'lgdt' loads the GDTR register with the table's address and limit,
; but it does NOT automatically update the hidden segment descriptor caches inside
; the CPU registers (CS, DS, ES, FS, GS, SS).
;
; In x86, the Code Segment (CS) cannot be updated with a simple 'mov' instruction;
; it can only be reloaded via a far jump or far return.
; ==============================================================================

section .text
global gdt_flush

; void gdt_flush(uint32_t gdt_ptr_address);
gdt_flush:
    ; Retrieve pointer to gdt_ptr struct passed as first parameter on stack
    mov eax, [esp + 4]
    lgdt [eax]                                ; Load GDTR with table base & limit

    ; Far jump to flush CPU instruction pipeline and reload CS with 0x08
    jmp 0x08:.reload_cs

.reload_cs:
    ; Reload data segment registers with Kernel Data Selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret
