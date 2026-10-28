#ifndef MM_PMM_H
#define MM_PMM_H

#include "lib/types.h"
#include "kernel/multiboot.h"

/*
 * pmm.h:
 * Physical Memory Manager (PMM) - 4 KB Frame Allocator.
 *
 * Why this is needed:
 * The CPU's memory management unit (MMU) operates on physical memory in 4 KB chunks
 * called frames. The PMM tracks which physical frames are free and which are in use,
 * handing out frames to the paging system and kernel heap.
 */

#define PAGE_SIZE           4096
#define PMM_FRAMES_PER_BYTE 8
#define PMM_MAX_FRAMES      (0x100000000ULL / PAGE_SIZE) /* 1,048,576 frames for 4 GB */
#define PMM_BITMAP_SIZE     (PMM_MAX_FRAMES / PMM_FRAMES_PER_BYTE) /* 131,072 bytes (128 KB) */

/* Initializes the physical memory bitmap using the parsed memory map */
void pmm_init(const multiboot_info_t* mboot_info);

/* Allocates a single 4 KB physical frame. Returns physical address (0 = Out of Memory) */
uintptr_t pmm_alloc_frame(void);

/* Frees a previously allocated 4 KB physical frame */
void pmm_free_frame(uintptr_t phys_addr);

/* Returns total tracked physical frames */
uint32_t pmm_total_frames(void);

/* Returns total currently allocated/used frames */
uint32_t pmm_used_frames(void);

/* Returns total currently available/free frames */
uint32_t pmm_free_frames(void);

#endif /* MM_PMM_H */
