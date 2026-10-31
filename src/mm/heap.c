#include "mm/heap.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "arch/lock.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Head of the heap block linked list */
static heap_block_t* heap_first_block = NULL;

/* Current mapped top of the virtual heap */
static uintptr_t heap_curr_end = HEAP_START_ADDR;

/*
 * heap_expand:
 * Expands the virtual heap by mapping physical frames from the PMM.
 *
 * Parameters:
 *   bytes - Number of bytes to expand (rounded up to 4 KB page multiple)
 *
 * Return:
 *   Pointer to the newly created free block, or NULL on failure.
 */
static heap_block_t* heap_expand(size_t bytes) {
    size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t actual_bytes = pages * PAGE_SIZE;

    /* Guard against integer wrap-around and expansion beyond max virtual ceiling */
    if (actual_bytes > HEAP_MAX_SIZE ||
        heap_curr_end + actual_bytes > HEAP_START_ADDR + HEAP_MAX_SIZE) {
        kprintf("[HEAP] Error: Exceeded maximum heap capacity of %u MB!\n",
                HEAP_MAX_SIZE / (1024 * 1024));
        return NULL;
    }

    uintptr_t new_region_start = heap_curr_end;

    for (size_t i = 0; i < pages; i++) {
        uintptr_t frame = pmm_alloc_frame();
        if (frame == 0) {
            kprintf("[HEAP] Error: Out of physical memory while expanding heap!\n");
            return NULL;
        }

        if (!paging_map_page(heap_curr_end, frame, PAGE_WRITE)) {
            kprintf("[HEAP] Error: Failed to map heap page at %p!\n", (void*)heap_curr_end);
            return NULL;
        }

        heap_curr_end += PAGE_SIZE;
    }

    /* Create a new free block header for the newly mapped region */
    heap_block_t* new_block = (heap_block_t*)new_region_start;
    new_block->magic    = HEAP_MAGIC;
    new_block->size     = (uint32_t)(actual_bytes - sizeof(heap_block_t));
    new_block->is_free  = 1;
    new_block->padding  = 0;
    new_block->next     = NULL;
    new_block->prev     = NULL;

    /* Append to the end of the existing list */
    if (!heap_first_block) {
        heap_first_block = new_block;
    } else {
        heap_block_t* curr = heap_first_block;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = new_block;
        new_block->prev = curr;

        /* If the preceding block was also free and contiguous, coalesce immediately */
        if (curr->is_free && ((uintptr_t)curr + sizeof(heap_block_t) + curr->size == (uintptr_t)new_block)) {
            curr->size += (uint32_t)(sizeof(heap_block_t) + new_block->size);
            curr->next = NULL;
            new_block = curr;
        }
    }

    return new_block;
}

/*
 * heap_init:
 * Initializes the heap subsystem with an initial 4 KB page at 0xD0000000.
 */
void heap_init(void) {
    heap_curr_end = HEAP_START_ADDR;
    heap_first_block = NULL;

    /* Allocate initial 4 KB page */
    heap_block_t* init_block = heap_expand(PAGE_SIZE);
    if (!init_block) {
        kprintf("[HEAP] Critical: Failed to initialize kernel heap!\n");
        return;
    }

    kprintf("[HEAP] Initialized at virtual %p (Initial payload: %u bytes, Header: %u bytes)\n",
            (void*)HEAP_START_ADDR, init_block->size, (uint32_t)sizeof(heap_block_t));
}

/*
 * kmalloc:
 * Allocates 'size' bytes on the heap.
 * Returns an 8-byte aligned pointer to usable memory, or NULL on failure.
 */
void* kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    /* Reject requests exceeding total max heap size upfront to prevent overflow */
    if (size > HEAP_MAX_SIZE) {
        kprintf("[HEAP] Error: Requested allocation (%u MB) exceeds max heap capacity (%u MB)!\n",
                (uint32_t)(size / (1024 * 1024)), (uint32_t)(HEAP_MAX_SIZE / (1024 * 1024)));
        return NULL;
    }

    uint32_t flags = irq_save();

    /* Round up requested size to an 8-byte multiple for alignment */
    size = (size + 7) & ~7;

    heap_block_t* curr = heap_first_block;
    heap_block_t* best_fit = NULL;

    /* First-fit search */
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            best_fit = curr;
            break;
        }
        curr = curr->next;
    }

    /* If no block is large enough, expand the heap */
    if (!best_fit) {
        size_t needed = sizeof(heap_block_t) + size;
        best_fit = heap_expand(needed);
        if (!best_fit) {
            irq_restore(flags);
            return NULL;
        }
    }

    /*
     * Block Splitting:
     * If the free block has enough excess space to hold another header + at least 8 bytes,
     * split the block into allocated and remaining free chunks.
     */
    size_t min_split_size = sizeof(heap_block_t) + 8;
    if (best_fit->size >= size + min_split_size) {
        uintptr_t new_addr = (uintptr_t)best_fit + sizeof(heap_block_t) + size;
        heap_block_t* remainder = (heap_block_t*)new_addr;

        remainder->magic   = HEAP_MAGIC;
        remainder->size    = (uint32_t)(best_fit->size - size - sizeof(heap_block_t));
        remainder->is_free = 1;
        remainder->padding = 0;
        remainder->next    = best_fit->next;
        remainder->prev    = best_fit;

        if (best_fit->next) {
            best_fit->next->prev = remainder;
        }
        best_fit->next = remainder;
        best_fit->size = (uint32_t)size;
    }

    best_fit->is_free = 0;

    void* result = (void*)((uintptr_t)best_fit + sizeof(heap_block_t));
    irq_restore(flags);

    /* Return pointer to usable payload immediately following the header */
    return result;
}

/*
 * kcalloc:
 * Allocates and zeroes memory for an array of num elements of size bytes.
 */
void* kcalloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

/*
 * kfree:
 * Deallocates a memory block and coalesces adjacent free blocks.
 * Validates pointer integrity via magic signature and catches double frees.
 */
void kfree(void* ptr) {
    if (!ptr) {
        return; /* Freeing NULL is a safe no-op */
    }

    uint32_t flags = irq_save();

    /* Validate 8-byte alignment */
    if (((uintptr_t)ptr % 8) != 0) {
        irq_restore(flags);
        kprintf("[HEAP] Error: Attempted to free unaligned pointer %p!\n", ptr);
        return;
    }

    /* Retrieve block header located immediately before payload */
    heap_block_t* block = (heap_block_t*)((uintptr_t)ptr - sizeof(heap_block_t));

    /* Verify magic signature to prevent heap corruption from arbitrary pointers */
    if (block->magic != HEAP_MAGIC) {
        irq_restore(flags);
        kprintf("[HEAP] Error: Corrupted header or invalid pointer %p (magic: 0x%x)!\n",
                ptr, block->magic);
        return;
    }

    /* Detect double frees */
    if (block->is_free == 1) {
        irq_restore(flags);
        kprintf("[HEAP] Error: Double free detected on pointer %p!\n", ptr);
        return;
    }

    block->is_free = 1;

    /*
     * Coalescing:
     * 1. Merge with next block if adjacent and free.
     */
    if (block->next && block->next->is_free) {
        uintptr_t expected_next = (uintptr_t)block + sizeof(heap_block_t) + block->size;
        if (expected_next == (uintptr_t)block->next) {
            block->size += (uint32_t)(sizeof(heap_block_t) + block->next->size);
            block->next = block->next->next;
            if (block->next) {
                block->next->prev = block;
            }
        }
    }

    /*
     * 2. Merge with previous block if adjacent and free.
     */
    if (block->prev && block->prev->is_free) {
        uintptr_t expected_curr = (uintptr_t)block->prev + sizeof(heap_block_t) + block->prev->size;
        if (expected_curr == (uintptr_t)block) {
            heap_block_t* prev = block->prev;
            prev->size += (uint32_t)(sizeof(heap_block_t) + block->size);
            prev->next = block->next;
            if (block->next) {
                block->next->prev = prev;
            }
        }
    }

    irq_restore(flags);
}

/*
 * heap_stats:
 * Computes live heap metrics by traversing all blocks.
 */
void heap_stats(size_t* used_bytes, size_t* free_bytes, size_t* block_count) {
    uint32_t flags = irq_save();

    size_t u = 0;
    size_t f = 0;
    size_t c = 0;

    heap_block_t* curr = heap_first_block;
    while (curr) {
        c++;
        if (curr->is_free) {
            f += curr->size;
        } else {
            u += curr->size;
        }
        curr = curr->next;
    }

    if (used_bytes)  *used_bytes  = u;
    if (free_bytes)  *free_bytes  = f;
    if (block_count) *block_count = c;

    irq_restore(flags);
}

/*
 * heap_get_break:
 * Returns the current virtual address break of the heap.
 */
uintptr_t heap_get_break(void) {
    return heap_curr_end;
}

/*
 * heap_dump:
 * Debug function to print all blocks and their status to VGA and serial.
 */
void heap_dump(void) {
    kprintf("[HEAP DUMP] Current Blocks:\n");
    heap_block_t* curr = heap_first_block;
    size_t idx = 0;

    while (curr) {
        kprintf("  #%u: [%s] Addr: %p, Size: %u B, Payload: %p, Next: %p, Prev: %p\n",
                idx++,
                curr->is_free ? "FREE" : "USED",
                (void*)curr,
                curr->size,
                (void*)((uintptr_t)curr + sizeof(heap_block_t)),
                (void*)curr->next,
                (void*)curr->prev);
        curr = curr->next;
    }
}
