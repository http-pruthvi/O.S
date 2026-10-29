#include "gfx/fb.h"
#include "mm/paging.h"
#include "drivers/serial.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Global framebuffer metadata structure */
static fb_info_t g_fb_info = {
    .is_available = false,
    .phys_addr    = 0,
    .virt_addr    = 0,
    .pitch        = 0,
    .width        = 0,
    .height       = 0,
    .bpp          = 0,
    .type         = 0
};

static bool g_fb_mapped = false;

/*
 * fb_early_init:
 * Extracts and buffers VBE linear framebuffer fields from the Multiboot info
 * structure BEFORE paging is enabled.
 */
void fb_early_init(const multiboot_info_t* mboot_info) {
    memset(&g_fb_info, 0, sizeof(g_fb_info));
    g_fb_info.is_available = false;
    g_fb_mapped = false;

    if (!mboot_info) {
        serial_writestring("[FB] Error: Multiboot info pointer is NULL!\n");
        return;
    }

    /* Check if GRUB flagged framebuffer information as valid (bit 12) */
    if (!(mboot_info->flags & MULTIBOOT_INFO_FLAG_FRAMEBUFFER)) {
        serial_writestring("[FB] Notice: Multiboot framebuffer bit 12 is NOT set (flags: 0x");
        /* Print hex flags for debugging */
        uint32_t f = mboot_info->flags;
        char hex[9];
        for (int i = 7; i >= 0; i--) {
            uint8_t nibble = (f >> (i * 4)) & 0xF;
            hex[7 - i] = (nibble < 10) ? ('0' + nibble) : ('a' + (nibble - 10));
        }
        hex[8] = '\0';
        serial_writestring(hex);
        serial_writestring("). Fallback to VGA text mode (0xB8000).\n");
        return;
    }

    uint32_t addr_high = (uint32_t)(mboot_info->framebuffer_addr >> 32);
    uint32_t addr_low  = (uint32_t)(mboot_info->framebuffer_addr & 0xFFFFFFFF);

    /* Log raw Multiboot parameters to serial port for debugging */
    serial_writestring("[FB] Multiboot framebuffer detected:\n");
    kprintf("  [FB] Addr: 0x%x:%x | Pitch: %u | Resolution: %ux%u | BPP: %u | Type: %u\n",
            addr_high, addr_low,
            mboot_info->framebuffer_pitch,
            mboot_info->framebuffer_width,
            mboot_info->framebuffer_height,
            (uint32_t)mboot_info->framebuffer_bpp,
            (uint32_t)mboot_info->framebuffer_type);

    /* Validate mode: must be 32 bpp linear direct RGB mode */
    if (mboot_info->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB || mboot_info->framebuffer_bpp != 32) {
        serial_writestring("[FB] Error: Only 32-bit direct RGB linear framebuffer is supported. Fallback to VGA text.\n");
        return;
    }

    /* Copy into permanent kernel structure */
    g_fb_info.is_available = true;
    g_fb_info.phys_addr    = mboot_info->framebuffer_addr;
    g_fb_info.virt_addr    = 0; /* Set when mapped */
    g_fb_info.pitch        = mboot_info->framebuffer_pitch;
    g_fb_info.width        = mboot_info->framebuffer_width;
    g_fb_info.height       = mboot_info->framebuffer_height;
    g_fb_info.bpp          = mboot_info->framebuffer_bpp;
    g_fb_info.type         = mboot_info->framebuffer_type;
}

/*
 * fb_map_pages:
 * Maps the physical VRAM into the kernel's virtual page tables.
 * Must be called after paging_init() is complete.
 */
bool fb_map_pages(void) {
    if (!g_fb_info.is_available) {
        return false;
    }

    /* Round physical address down to 4 KB page boundary */
    uintptr_t phys_start = (uintptr_t)(g_fb_info.phys_addr & ~0xFFF);
    size_t total_bytes   = (size_t)(g_fb_info.pitch * g_fb_info.height);
    /* Round top address up to 4 KB page boundary */
    uintptr_t phys_end   = (uintptr_t)((g_fb_info.phys_addr + total_bytes + 4095) & ~0xFFF);
    size_t num_pages     = (phys_end - phys_start) / PAGE_SIZE_4K;

    /*
     * Identity-map the framebuffer virtual address directly to its physical address.
     * Flags: PAGE_PRESENT | PAGE_WRITE | PAGE_PCD (Page Cache Disable).
     * PCD ensures CPU writes go straight to VRAM rather than being buffered in L1/L2 cache.
     */
    for (size_t i = 0; i < num_pages; i++) {
        uintptr_t page_addr = phys_start + (i * PAGE_SIZE_4K);
        if (!paging_map_page(page_addr, page_addr, PAGE_WRITE | PAGE_PCD)) {
            serial_writestring("[FB] Critical: Failed to map framebuffer page in page directory!\n");
            g_fb_info.is_available = false;
            return false;
        }
    }

    g_fb_info.virt_addr = (uintptr_t)g_fb_info.phys_addr;

    /* Verify first and last page with paging_get_phys */
    uintptr_t first_phys = paging_get_phys(g_fb_info.virt_addr);
    uintptr_t last_phys  = paging_get_phys(g_fb_info.virt_addr + total_bytes - PAGE_SIZE_4K);

    if (first_phys == 0 || last_phys == 0) {
        serial_writestring("[FB] Error: Framebuffer physical mapping verification failed!\n");
        g_fb_info.is_available = false;
        return false;
    }

    g_fb_mapped = true;

    kprintf("[FB] Framebuffer mapped successfully: %u MB (%u pages) at virtual %p\n",
            (uint32_t)(total_bytes / (1024 * 1024)), (uint32_t)num_pages, (void*)g_fb_info.virt_addr);
    kprintf("     First page: %p -> %p | Last page: %p -> %p (PCD active)\n",
            (void*)g_fb_info.virt_addr, (void*)first_phys,
            (void*)(g_fb_info.virt_addr + total_bytes - PAGE_SIZE_4K), (void*)last_phys);

    return true;
}

/*
 * fb_get_info:
 * Returns read-only pointer to global framebuffer metadata.
 */
const fb_info_t* fb_get_info(void) {
    return &g_fb_info;
}

/*
 * fb_is_active:
 * Returns true if linear graphics mode was granted and virtual mapping is complete.
 */
bool fb_is_active(void) {
    return g_fb_info.is_available && g_fb_mapped;
}
