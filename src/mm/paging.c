#include "mm/paging.h"
#include "mm/pmm.h"
#include "mm/memmap.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Assembly helpers defined in paging_asm.asm */
extern void paging_load_cr3(uint32_t pd_phys);
extern void paging_enable(void);

/*
 * Static kernel Page Directory (1024 entries = 4 KB, 4KB page aligned).
 * Placed in .bss so it is allocated at compile time.
 */
static uint32_t kernel_page_directory[PAGE_ENTRIES_PER_TABLE] __attribute__((aligned(PAGE_SIZE_4K)));

/*
 * Static Page Tables for early identity mapping (up to 64 tables = 256 MB RAM).
 * Sized 256 KB in .bss. Pre-allocating this eliminates dynamic allocations
 * while establishing the initial identity map before paging is turned on.
 */
static uint32_t kernel_identity_tables[64][PAGE_ENTRIES_PER_TABLE] __attribute__((aligned(PAGE_SIZE_4K)));

/*
 * page_fault_handler:
 * Invoked when CPU triggers Vector 14 (#PF).
 * Reads CR2 to identify the faulting linear address, decodes error bits,
 * prints a comprehensive panic screen, and safely halts.
 */
void page_fault_handler(registers_t* regs) {
    uint32_t fault_addr = paging_read_cr2();
    uint32_t err = regs ? regs->err_code : 0;

    /* Decode error code bits */
    bool not_present = (err & 0x01) == 0;
    bool is_write    = (err & 0x02) != 0;
    bool is_user     = (err & 0x04) != 0;
    bool rsvd_violation = (err & 0x08) != 0;
    bool instruction_fetch = (err & 0x10) != 0;

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    kprintf("\n=========================== PAGE FAULT PANIC ===========================\n");
    kprintf(" FAULT ADDRESS (CR2): %p\n", (void*)fault_addr);
    kprintf(" ERROR CODE: 0x%x -> [ %s, %s, %s%s%s ]\n",
            err,
            not_present ? "Page Not Present" : "Page-Level Protection Violation",
            is_write ? "Write Access" : "Read Access",
            is_user ? "User Mode" : "Supervisor Mode",
            rsvd_violation ? ", Reserved Bit Set" : "",
            instruction_fetch ? ", Instruction Fetch" : "");
    kprintf("------------------------------------------------------------------------\n");
    if (regs) {
        kprintf(" CS:EIP: 0x%x:%p   EFLAGS: 0x%x\n", regs->cs, (void*)regs->eip, regs->eflags);
        kprintf(" EAX: 0x%x   EBX: 0x%x   ECX: 0x%x   EDX: 0x%x\n",
                regs->eax, regs->ebx, regs->ecx, regs->edx);
        kprintf(" ESI: 0x%x   EDI: 0x%x   EBP: 0x%x   ESP: %p\n",
                regs->esi, regs->edi, regs->ebp, (void*)(uintptr_t)(regs + 1));
    }
    kprintf("========================================================================\n");
    kprintf(" System halted safely.\n");

    /* Halt CPU */
    __asm__ volatile ("cli");
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/*
 * paging_init:
 * Builds the page directory, identity-maps all detected RAM up to 256 MB,
 * leaves Page 0 (0x0 - 0xFFF) UNMAPPED for NULL pointer trap, loads CR3, and sets CR0.PG.
 */
void paging_init(void) {
    /* 1. Clear Page Directory */
    memset(kernel_page_directory, 0, sizeof(kernel_page_directory));

    /* 2. Determine physical RAM to identity map */
    uint32_t ram_to_map = memmap_get_highest_usable_addr();
    if (ram_to_map == 0) {
        ram_to_map = 128 * 1024 * 1024; /* Fallback to 128 MB */
    }

    if (ram_to_map > PAGING_IDENTITY_MAX_RAM) {
        kprintf("[PAGING] Notice: Physical RAM (0x%x) exceeds 256 MB cap; identity map capped at 256 MB.\n",
                ram_to_map);
        ram_to_map = PAGING_IDENTITY_MAX_RAM;
    }

    /* Calculate number of 4 MB page tables required */
    uint32_t num_tables = (ram_to_map + PAGE_TABLE_SPAN - 1) / PAGE_TABLE_SPAN;
    if (num_tables > 64) {
        num_tables = 64;
    }

    /* 3. Build Identity Map */
    for (uint32_t t = 0; t < num_tables; t++) {
        /* Set Directory Entry: point to physical address of identity page table */
        kernel_page_directory[t] = ((uintptr_t)&kernel_identity_tables[t]) | PAGE_PRESENT | PAGE_WRITE;

        /* Populate Page Table entries */
        for (uint32_t p = 0; p < PAGE_ENTRIES_PER_TABLE; p++) {
            uintptr_t phys = (t * PAGE_TABLE_SPAN) + (p * PAGE_SIZE_4K);

            if (t == 0 && p == 0) {
                /*
                 * Page 0 (0x00000000 to 0x00000FFF) is UNMAPPED!
                 * Leaving this entry not-present ensures any NULL pointer dereference
                 * instantly triggers a Page Fault (#PF).
                 */
                kernel_identity_tables[t][p] = 0;
            } else {
                /* Identity map: virtual address equals physical address */
                kernel_identity_tables[t][p] = phys | PAGE_PRESENT | PAGE_WRITE;
            }
        }
    }

    /* 4. Load CR3 with physical base address of Page Directory */
    paging_load_cr3((uint32_t)&kernel_page_directory);

    /* 5. Enable Paging by setting bit 31 in CR0 */
    paging_enable();

    kprintf("[PAGING] Initialized: Identity-mapped %u MB RAM (Page 0 unmapped for NULL traps)\n",
            num_tables * 4);
}

/*
 * paging_map_page:
 * Maps a virtual address to a physical frame with specified flags.
 * Allocates a new Page Table from PMM if the Page Directory entry does not exist yet.
 */
bool paging_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags) {
    uint32_t pde_idx = virt >> 22;
    uint32_t pte_idx = (virt >> 12) & 0x3FF;

    /* Check if Page Directory entry is present */
    if (!(kernel_page_directory[pde_idx] & PAGE_PRESENT)) {
        /* Allocate a frame for the new Page Table from PMM */
        uintptr_t pt_frame = pmm_alloc_frame();
        if (pt_frame == 0) {
            kprintf("[PAGING] Error: Out of physical memory for new page table!\n");
            return false;
        }

        /*
         * Clear new page table.
         * Safe to access directly because all physical RAM is identity-mapped!
         */
        memset((void*)pt_frame, 0, PAGE_SIZE_4K);

        /* Install new Page Table into Directory */
        kernel_page_directory[pde_idx] = pt_frame | flags | PAGE_PRESENT | PAGE_WRITE;
    }

    /* Retrieve Page Table address */
    uint32_t* pt = (uint32_t*)(kernel_page_directory[pde_idx] & ~0xFFF);

    /* Set Page Table Entry */
    pt[pte_idx] = (phys & ~0xFFF) | flags | PAGE_PRESENT;

    /* Invalidate TLB for this virtual page */
    paging_invalidate_tlb(virt);

    return true;
}

/*
 * paging_unmap_page:
 * Unmaps a virtual page and invalidates its TLB entry.
 */
void paging_unmap_page(uintptr_t virt) {
    uint32_t pde_idx = virt >> 22;
    uint32_t pte_idx = (virt >> 12) & 0x3FF;

    if (kernel_page_directory[pde_idx] & PAGE_PRESENT) {
        uint32_t* pt = (uint32_t*)(kernel_page_directory[pde_idx] & ~0xFFF);
        pt[pte_idx] = 0;
        paging_invalidate_tlb(virt);
    }
}

/*
 * paging_get_phys:
 * Translates a virtual address into its underlying physical address.
 * Returns 0 if the page is not mapped.
 */
uintptr_t paging_get_phys(uintptr_t virt) {
    uint32_t pde_idx = virt >> 22;
    uint32_t pte_idx = (virt >> 12) & 0x3FF;

    if (!(kernel_page_directory[pde_idx] & PAGE_PRESENT)) {
        return 0;
    }

    uint32_t* pt = (uint32_t*)(kernel_page_directory[pde_idx] & ~0xFFF);
    if (!(pt[pte_idx] & PAGE_PRESENT)) {
        return 0;
    }

    return (pt[pte_idx] & ~0xFFF) | (virt & 0xFFF);
}
