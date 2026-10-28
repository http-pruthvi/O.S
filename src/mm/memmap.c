#include "mm/memmap.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "drivers/vga.h"

/* Kernel-owned buffer storing parsed physical memory regions */
static memmap_region_t saved_regions[MEMMAP_MAX_REGIONS];
static size_t region_count = 0;

static uint64_t total_memory_bytes = 0;
static uint64_t usable_memory_bytes = 0;
static uint32_t highest_usable_addr = 0;

/*
 * memmap_type_to_string:
 * Returns human-readable label for Multiboot memory type.
 */
static const char* memmap_type_to_string(uint32_t type) {
    switch (type) {
        case MEMMAP_TYPE_USABLE:       return "Usable RAM";
        case MEMMAP_TYPE_RESERVED:     return "Reserved";
        case MEMMAP_TYPE_ACPI_RECLAIM: return "ACPI Reclaimable";
        case MEMMAP_TYPE_ACPI_NVS:     return "ACPI NVS";
        case MEMMAP_TYPE_BADRAM:       return "Bad RAM";
        default:                       return "Unknown";
    }
}

/*
 * memmap_init:
 * Iterates through the Multiboot memory map, copies regions into kernel memory,
 * calculates total usable RAM, and displays the memory layout.
 */
void memmap_init(const multiboot_info_t* mboot_info) {
    region_count = 0;
    total_memory_bytes = 0;
    usable_memory_bytes = 0;
    highest_usable_addr = 0;

    if (!mboot_info || !(mboot_info->flags & MULTIBOOT_INFO_FLAG_MMAP)) {
        kprintf("[MEMMAP] Error: Multiboot memory map not provided!\n");
        return;
    }

    uintptr_t mmap_start = mboot_info->mmap_addr;
    uintptr_t mmap_end   = mmap_start + mboot_info->mmap_length;
    multiboot_mmap_entry_t* entry = (multiboot_mmap_entry_t*)mmap_start;

    kprintf("[MEMMAP] Physical BIOS Memory Map:\n");

    while ((uintptr_t)entry < mmap_end && region_count < MEMMAP_MAX_REGIONS) {
        /* Construct 64-bit base address and length from entry fields */
        uint64_t base = ((uint64_t)entry->addr_high << 32) | entry->addr_low;
        uint64_t len  = ((uint64_t)entry->len_high << 32) | entry->len_low;
        uint32_t type = entry->type;

        /* Save copy in kernel buffer */
        saved_regions[region_count].base_addr = base;
        saved_regions[region_count].length    = len;
        saved_regions[region_count].type      = type;
        region_count++;

        total_memory_bytes += len;

        if (type == MEMMAP_TYPE_USABLE) {
            usable_memory_bytes += len;
            uint64_t region_end = base + len;
            /* Track highest usable 32-bit physical RAM address (ignore > 4GB) */
            if (region_end <= 0x100000000ULL && (uint32_t)region_end > highest_usable_addr) {
                highest_usable_addr = (uint32_t)region_end;
            }
        }

        /* Print region details (print 32-bit hex for clarity on x86) */
        uint32_t base32 = (uint32_t)base;
        uint32_t len_kb = (uint32_t)(len / 1024);
        kprintf("  [Region %d] Base: 0x%x, Length: %u KB (%s)\n",
                (int)region_count - 1, base32, len_kb, memmap_type_to_string(type));

        /*
         * Stride Advancement:
         * In Multiboot 1, entry->size specifies the size of the record EXCLUDING
         * the 4-byte size field itself. We must advance by (size + 4) bytes.
         */
        entry = (multiboot_mmap_entry_t*)((uint8_t*)entry + entry->size + sizeof(uint32_t));
    }

    uint32_t usable_mb = (uint32_t)(usable_memory_bytes / (1024 * 1024));
    kprintf("[MEMMAP] Total Usable RAM: %u MB (Highest Usable Addr: 0x%x)\n",
            usable_mb, highest_usable_addr);
}

size_t memmap_get_region_count(void) {
    return region_count;
}

const memmap_region_t* memmap_get_region(size_t index) {
    if (index < region_count) {
        return &saved_regions[index];
    }
    return NULL;
}

uint64_t memmap_get_total_memory(void) {
    return total_memory_bytes;
}

uint64_t memmap_get_usable_memory(void) {
    return usable_memory_bytes;
}

uint32_t memmap_get_highest_usable_addr(void) {
    return highest_usable_addr;
}
