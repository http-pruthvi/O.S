; ==============================================================================
; paging_asm.asm - Assembly Helpers for x86 Paging
; ==============================================================================
; Architecture: x86 32-bit protected mode
;
; Why this is needed:
; Loading page directory address into CR3, reading the faulting address from CR2,
; setting the Paging bit (PG, bit 31) in CR0, and invalidating TLB entries ('invlpg')
; require dedicated processor instructions not available in standard C.
; ==============================================================================

section .text
global paging_load_cr3
global paging_enable
global paging_invalidate_tlb
global paging_read_cr2

; void paging_load_cr3(uint32_t pd_phys_address);
paging_load_cr3:
    mov eax, [esp + 4]
    mov cr3, eax                              ; Load physical base address of Page Directory into CR3
    ret

; void paging_enable(void);
paging_enable:
    mov eax, cr0
    or eax, 0x80000000                        ; Set bit 31 (PG - Paging Enable)
    mov cr0, eax                              ; Paging is now active!
    ret

; void paging_invalidate_tlb(uintptr_t virt_addr);
paging_invalidate_tlb:
    mov eax, [esp + 4]
    invlpg [eax]                              ; Invalidate single TLB entry for given virtual address
    ret

; uint32_t paging_read_cr2(void);
paging_read_cr2:
    mov eax, cr2                              ; Read linear address that triggered the page fault
    ret
