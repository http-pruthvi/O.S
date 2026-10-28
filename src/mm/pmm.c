#include "mm/pmm.h"
#include "mm/memmap.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Linker symbols defining the kernel physical boundaries */
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

/*
 * Static 128 KB bitmap in .bss tracking up to 4 GB of physical memory.
 * Sizing this statically eliminates any dynamic allocation chicken-and-egg problem.
 * 1 bit represents one 4 KB frame: 1 = in use/reserved, 0 = free.
 */
static uint8_t pmm_bitmap[PMM_BITMAP_SIZE];

static uint32_t total_memory_frames = 0;
static uint32_t used_memory_frames  = 0;
static uint32_t free_memory_frames  = 0;
static uint32_t last_alloc_index    = 0;

/* Bitmap Helper Functions */
static inline void pmm_bitmap_set(uint32_t frame) {
    pmm_bitmap[frame / 8] |= (uint8_t)(1 << (frame % 8));
}

static inline void pmm_bitmap_clear(uint32_t frame) {
    pmm_bitmap[frame / 8] &= (uint8_t)~(1 << (frame % 8));
}

static inline bool pmm_bitmap_test(uint32_t frame) {
    return (pmm_bitmap[frame / 8] & (1 << (frame % 8))) != 0;
}

/*
 * pmm_init_region:
 * Marks all 4 KB frames in a memory range as FREE (0).
 */
static void pmm_init_region(uintptr_t base, size_t size) {
    uint32_t start_frame = base / PAGE_SIZE;
    uint32_t frame_count = size / PAGE_SIZE;

    for (uint32_t i = 0; i < frame_count; i++) {
        uint32_t frame = start_frame + i;
        if (frame < total_memory_frames) {
            if (pmm_bitmap_test(frame)) {
                pmm_bitmap_clear(frame);
                used_memory_frames--;
                free_memory_frames++;
            }
        }
    }
}

/*
 * pmm_deinit_region:
 * Marks all 4 KB frames in a memory range as USED/RESERVED (1).
 */
static void pmm_deinit_region(uintptr_t base, size_t size) {
    uint32_t start_frame = base / PAGE_SIZE;
    /* Round up size to page boundary so partial pages are fully reserved */
    uint32_t frame_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint32_t i = 0; i < frame_count; i++) {
        uint32_t frame = start_frame + i;
        if (frame < total_memory_frames) {
            if (!pmm_bitmap_test(frame)) {
                pmm_bitmap_set(frame);
                used_memory_frames++;
                free_memory_frames--;
            }
        }
    }
}

/*
 * pmm_init:
 * Initializes the PMM bitmap.
 * Starts with everything marked used, frees usable memory map regions,
 * then re-reserves the first 1 MB, the kernel image, and bootloader structures.
 */
void pmm_init(const multiboot_info_t* mboot_info) {
    uint32_t highest_addr = memmap_get_highest_usable_addr();
    if (highest_addr == 0) {
        highest_addr = 128 * 1024 * 1024; /* Fallback to 128 MB */
    }

    total_memory_frames = highest_addr / PAGE_SIZE;
    if (total_memory_frames > PMM_MAX_FRAMES) {
        total_memory_frames = PMM_MAX_FRAMES;
    }

    /* 1. Mark all memory as USED by default (0xFF) */
    memset(pmm_bitmap, 0xFF, PMM_BITMAP_SIZE);
    used_memory_frames = total_memory_frames;
    free_memory_frames = 0;
    last_alloc_index   = 0;

    /* 2. Mark usable regions from memory map as FREE */
    size_t count = memmap_get_region_count();
    for (size_t i = 0; i < count; i++) {
        const memmap_region_t* reg = memmap_get_region(i);
        if (reg && reg->type == MEMMAP_TYPE_USABLE) {
            if (reg->base_addr < 0x100000000ULL) {
                uintptr_t base = (uintptr_t)reg->base_addr;
                size_t len = (size_t)reg->length;
                if ((uint64_t)base + len > 0x100000000ULL) {
                    len = 0xFFFFFFFF - base;
                }
                pmm_init_region(base, len);
            }
        }
    }

    /* 3. Re-reserve Frame 0 and the first 1 MB (0x0 to 0x100000) */
    pmm_deinit_region(0x0, 0x100000);

    /* 4. Re-reserve the Kernel Image (includes .text, .rodata, .data, .bss, and PMM bitmap).
     * Rule: Round _kernel_end UP to a page boundary. */
    uintptr_t kstart = (uintptr_t)_kernel_start;
    uintptr_t kend   = (((uintptr_t)_kernel_end) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    pmm_deinit_region(kstart, kend - kstart);

    /* 5. Re-reserve original Multiboot info structure and mmap buffer */
    if (mboot_info) {
        pmm_deinit_region((uintptr_t)mboot_info, sizeof(multiboot_info_t));
        if (mboot_info->flags & MULTIBOOT_INFO_FLAG_MMAP) {
            pmm_deinit_region((uintptr_t)mboot_info->mmap_addr, mboot_info->mmap_length);
        }
    }

    kprintf("[PMM] Initialized: %u total frames (%u MB), %u free, %u used\n",
            total_memory_frames,
            (total_memory_frames * 4) / 1024,
            free_memory_frames,
            used_memory_frames);
}

/*
 * pmm_alloc_frame:
 * Finds and allocates a free 4 KB physical frame.
 * Uses last_alloc_index for fast sequential search.
 *
 * Return:
 *   32-bit physical base address of the allocated frame, or 0 if Out of Memory.
 */
uintptr_t pmm_alloc_frame(void) {
    if (free_memory_frames == 0) {
        kprintf("[PMM] Error: Out of Physical Memory!\n");
        return 0;
    }

    /* 32-bit fast scan helper */
    uint32_t* dword_bitmap = (uint32_t*)pmm_bitmap;
    uint32_t dword_count = total_memory_frames / 32;
    uint32_t start_dword = last_alloc_index / 32;

    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t start = (pass == 0) ? start_dword : 0;
        uint32_t end   = (pass == 0) ? dword_count : start_dword;

        for (uint32_t d = start; d < end; d++) {
            if (dword_bitmap[d] != 0xFFFFFFFF) {
                /* At least one free frame in this 32-frame chunk */
                for (uint32_t bit = 0; bit < 32; bit++) {
                    uint32_t frame = d * 32 + bit;
                    if (frame < total_memory_frames && !pmm_bitmap_test(frame)) {
                        pmm_bitmap_set(frame);
                        used_memory_frames++;
                        free_memory_frames--;
                        last_alloc_index = frame + 1;
                        return (uintptr_t)(frame * PAGE_SIZE);
                    }
                }
            }
        }
    }

    return 0;
}

/*
 * pmm_free_frame:
 * Returns a 4 KB physical frame back to the free pool.
 * Validates alignment and detects double frees.
 *
 * Parameters:
 *   phys_addr - Physical address of the frame
 */
void pmm_free_frame(uintptr_t phys_addr) {
    if (phys_addr == 0) {
        kprintf("[PMM] Error: Attempted to free NULL frame!\n");
        return;
    }

    if ((phys_addr & (PAGE_SIZE - 1)) != 0) {
        kprintf("[PMM] Error: Frame 0x%x is not 4KB aligned!\n", phys_addr);
        return;
    }

    uint32_t frame = phys_addr / PAGE_SIZE;
    if (frame >= total_memory_frames) {
        kprintf("[PMM] Error: Frame 0x%x is out of range!\n", phys_addr);
        return;
    }

    if (!pmm_bitmap_test(frame)) {
        kprintf("[PMM] Error: Double free detected on frame 0x%x!\n", phys_addr);
        return;
    }

    pmm_bitmap_clear(frame);
    used_memory_frames--;
    free_memory_frames++;

    /* Optimize next search */
    if (frame < last_alloc_index) {
        last_alloc_index = frame;
    }
}

uint32_t pmm_total_frames(void) {
    return total_memory_frames;
}

uint32_t pmm_used_frames(void) {
    return used_memory_frames;
}

uint32_t pmm_free_frames(void) {
    return free_memory_frames;
}
