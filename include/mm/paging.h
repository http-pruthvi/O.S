#ifndef MM_PAGING_H
#define MM_PAGING_H

#include "lib/types.h"
#include "arch/isr.h"

/*
 * paging.h:
 * Two-Level 32-bit Paging Subsystem.
 *
 * Why this is needed:
 * Paging provides hardware virtual memory isolation and protection.
 * A 32-bit linear address is divided into:
 *   - Directory Index (Bits 22-31): 10 bits -> selects 1 of 1024 Page Tables
 *   - Table Index     (Bits 12-21): 10 bits -> selects 1 of 1024 4 KB Pages
 *   - Offset          (Bits 0-11):  12 bits -> offset within 4096-byte page
 */

#define PAGE_ENTRIES_PER_TABLE 1024
#define PAGE_SIZE_4K           4096
#define PAGE_TABLE_SPAN        (PAGE_ENTRIES_PER_TABLE * PAGE_SIZE_4K) /* 4 MB */

/* Page Entry Flags */
#define PAGE_PRESENT 0x01  /* Bit 0: Page is present in physical memory */
#define PAGE_WRITE   0x02  /* Bit 1: Page is read/write (0 = read-only) */
#define PAGE_USER    0x04  /* Bit 2: Page is accessible in user mode (0 = supervisor) */
#define PAGE_PWT     0x08  /* Bit 3: Write-through caching */
#define PAGE_PCD     0x10  /* Bit 4: Page-level Cache Disable (used for video MMIO) */

/* Maximum physical RAM identity-mapped at boot */
#define PAGING_IDENTITY_MAX_RAM (256 * 1024 * 1024) /* 256 MB */

/* Initializes two-level paging, identity-maps RAM, leaves Page 0 unmapped, and enables CR0.PG */
void paging_init(void);

/* Maps a 4 KB virtual page to a physical frame with specified flags */
bool paging_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags);

/* Unmaps a 4 KB virtual page and flushes the TLB */
void paging_unmap_page(uintptr_t virt);

/* Translates a virtual address to its corresponding physical address (returns 0 if unmapped) */
uintptr_t paging_get_phys(uintptr_t virt);

/* Page Fault Exception Handler (Vector 14 / #PF) */
void page_fault_handler(registers_t* regs);

/* Flushes TLB for a single virtual address via invlpg */
void paging_invalidate_tlb(uintptr_t virt_addr);

/* Reads the CR2 register containing the linear fault address */
uint32_t paging_read_cr2(void);

#endif /* MM_PAGING_H */
