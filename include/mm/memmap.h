#ifndef MM_MEMMAP_H
#define MM_MEMMAP_H

#include "lib/types.h"
#include "kernel/multiboot.h"

/*
 * memmap.h:
 * Stride-Safe Multiboot Memory Map Parser.
 *
 * Why this is needed:
 * Before enabling paging, the kernel must know what physical RAM addresses exist,
 * which regions are safe to use (Type 1), and which are reserved for hardware/BIOS.
 * We copy all relevant entries into a kernel-owned buffer before paging is enabled,
 * avoiding any pointer dereference issues after virtual memory is turned on.
 */

#define MEMMAP_MAX_REGIONS 64

/* Multiboot Memory Map Type Constants */
#define MEMMAP_TYPE_USABLE          1  /* Usable RAM */
#define MEMMAP_TYPE_RESERVED        2  /* Reserved / Hardware mapped */
#define MEMMAP_TYPE_ACPI_RECLAIM    3  /* ACPI reclaimable memory */
#define MEMMAP_TYPE_ACPI_NVS        4  /* ACPI non-volatile storage */
#define MEMMAP_TYPE_BADRAM          5  /* Defective memory modules */

/*
 * memmap_region_t:
 * Safe copy of a physical memory region using 64-bit bounds.
 */
typedef struct memmap_region {
    uint64_t base_addr;     /* Physical starting address */
    uint64_t length;        /* Region length in bytes */
    uint32_t type;          /* Memory type (1 = usable RAM, others = reserved) */
} memmap_region_t;

/* Parses and copies the Multiboot memory map into kernel storage, printing the map */
void memmap_init(const multiboot_info_t* mboot_info);

/* Returns the number of memory regions saved */
size_t memmap_get_region_count(void);

/* Returns pointer to a specific memory region */
const memmap_region_t* memmap_get_region(size_t index);

/* Returns total physical RAM in bytes (usable + reserved) */
uint64_t memmap_get_total_memory(void);

/* Returns total usable RAM in bytes */
uint64_t memmap_get_usable_memory(void);

/* Returns highest address of detected usable RAM (capped at 4 GB for 32-bit kernel) */
uint32_t memmap_get_highest_usable_addr(void);

#endif /* MM_MEMMAP_H */
