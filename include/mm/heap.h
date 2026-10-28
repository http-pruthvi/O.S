#ifndef MM_HEAP_H
#define MM_HEAP_H

#include "lib/types.h"

/*
 * heap.h:
 * Dynamic Kernel Heap Allocator (kmalloc, kcalloc, kfree).
 *
 * Why this is needed:
 * Complex OS subsystems (such as process control blocks, file descriptors,
 * and command buffers) require dynamically sized memory blocks allocated at runtime.
 * The heap sits at virtual address 0xD0000000 and expands on demand by mapping
 * physical frames from the PMM.
 */

#define HEAP_START_ADDR  0xD0000000
#define HEAP_MAX_SIZE    (64 * 1024 * 1024) /* Maximum expansion: 64 MB */
#define HEAP_MAGIC       0x48454150         /* "HEAP" magic signature */

/*
 * heap_block_t:
 * Header prefixed to every allocated and free block in the heap.
 *
 * Requirements:
 * - is_free is uint32_t
 * - No __attribute__((packed))
 * - Sized to an exact multiple of 8 bytes (24 bytes)
 * - Guarantees returned payload pointers are 8-byte aligned
 */
typedef struct heap_block {
    uint32_t magic;            /* 0x48454150 ("HEAP") */
    uint32_t size;             /* Usable payload size in bytes (excluding header) */
    uint32_t is_free;          /* 1 = Free, 0 = Allocated */
    uint32_t padding;          /* Padding to ensure struct size is a multiple of 8 */
    struct heap_block* next;   /* Pointer to next block in address order */
    struct heap_block* prev;   /* Pointer to previous block in address order */
} heap_block_t;

_Static_assert(sizeof(heap_block_t) % 8 == 0, "heap_block_t size must be a multiple of 8");

/* Initializes the heap at HEAP_START_ADDR with an initial mapped page */
void heap_init(void);

/* Allocates a block of contiguous memory of at least 'size' bytes (8-byte aligned) */
void* kmalloc(size_t size);

/* Allocates and zeroes memory for an array of num elements of size bytes each */
void* kcalloc(size_t num, size_t size);

/* Frees a previously allocated memory block, coalescing adjacent free blocks */
void kfree(void* ptr);

/* Retrieves current heap metrics (used payload bytes, free bytes, block count) */
void heap_stats(size_t* used_bytes, size_t* free_bytes, size_t* block_count);

/* Prints debug dump of all blocks and their status */
void heap_dump(void);

#endif /* MM_HEAP_H */
