#ifndef GFX_FB_H
#define GFX_FB_H

#include "lib/types.h"
#include "kernel/multiboot.h"

/*
 * fb.h:
 * Linear Framebuffer Management & Virtual Memory Mapping.
 *
 * Why this is needed:
 * GRUB configures VBE graphics mode (1024x768x32) and hands off a physical
 * address to video memory (typically at 0xE0000000). Because this address is
 * above early identity-mapped RAM (0 to 128 MB), this module saves the
 * Multiboot video parameters before paging is turned on and maps the
 * physical video memory into the kernel's virtual page tables.
 */

typedef struct fb_info {
    bool      is_available;    /* True if 32-bit linear RGB mode was granted by GRUB */
    uint64_t  phys_addr;       /* Physical MMIO base address */
    uintptr_t virt_addr;       /* Mapped virtual address (identity-mapped) */
    uint32_t  pitch;           /* Bytes per scanline (e.g. 4096) */
    uint32_t  width;           /* Screen width in pixels (1024) */
    uint32_t  height;          /* Screen height in pixels (768) */
    uint8_t   bpp;             /* Bits per pixel (32) */
    uint8_t   type;            /* Framebuffer type (1 = RGB) */
} fb_info_t;

/* Parses and buffers framebuffer info from Multiboot structure BEFORE paging */
void fb_early_init(const multiboot_info_t* mboot_info);

/* Maps the linear framebuffer into virtual memory after paging_init() */
bool fb_map_pages(void);

/* Returns pointer to global framebuffer info structure */
const fb_info_t* fb_get_info(void);

/* Returns true if 32-bit linear graphics mode is active and mapped */
bool fb_is_active(void);

#endif /* GFX_FB_H */
