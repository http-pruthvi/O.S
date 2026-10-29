#ifndef KERNEL_MULTIBOOT_H
#define KERNEL_MULTIBOOT_H

#include "lib/types.h"

/*
 * multiboot.h:
 * Multiboot Specification (Version 1) Definitions.
 *
 * Why this is needed:
 * GRUB complies with the Multiboot standard. Before transferring control
 * to our kernel, GRUB puts a signature in EAX (0x2BADB002) and a pointer
 * to the multiboot_info structure in EBX. This structure tells the kernel
 * about detected RAM size, command-line arguments, BIOS boot drives, and memory maps.
 */

/* The magic number passed by a Multiboot-compliant bootloader in EAX */
#define MULTIBOOT_BOOTLOADER_MAGIC      0x2BADB002

/* The magic number required in the Multiboot kernel header */
#define MULTIBOOT_HEADER_MAGIC          0x1BADB002

/* Flags used in the Multiboot header */
#define MULTIBOOT_FLAG_PAGE_ALIGN       (1 << 0)  /* Align modules on 4KB page boundaries */
#define MULTIBOOT_FLAG_MEMORY_INFO      (1 << 1)  /* Provide memory information (mem_lower, mem_upper) */
#define MULTIBOOT_FLAG_VIDEO_MODE       (1 << 2)  /* Request video mode from bootloader */

/* Combined header flags requested from GRUB */
#define MULTIBOOT_HEADER_FLAGS          (MULTIBOOT_FLAG_PAGE_ALIGN | MULTIBOOT_FLAG_MEMORY_INFO | MULTIBOOT_FLAG_VIDEO_MODE)

/* Flags present in the multiboot_info_t struct indicating valid fields */
#define MULTIBOOT_INFO_FLAG_MEM         (1 << 0)  /* mem_lower and mem_upper are valid */
#define MULTIBOOT_INFO_FLAG_BOOT_DEVICE (1 << 1)  /* boot_device is valid */
#define MULTIBOOT_INFO_FLAG_CMDLINE     (1 << 2)  /* cmdline is valid */
#define MULTIBOOT_INFO_FLAG_MODS        (1 << 3)  /* mods fields are valid */
#define MULTIBOOT_INFO_FLAG_AOUT_SYMS   (1 << 4)  /* a.out symbol table is valid */
#define MULTIBOOT_INFO_FLAG_ELF_SHDR    (1 << 5)  /* ELF section header table is valid */
#define MULTIBOOT_INFO_FLAG_MMAP        (1 << 6)  /* Memory map buffer is valid */
#define MULTIBOOT_INFO_FLAG_VBE         (1 << 11) /* VBE info is valid */
#define MULTIBOOT_INFO_FLAG_FRAMEBUFFER (1 << 12) /* Linear framebuffer info is valid */

/* Framebuffer type definitions per Multiboot specification */
#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED 0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB     1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT 2

/*
 * Memory Map Entry:
 * Represents one entry in the BIOS memory map list passed by GRUB.
 */
typedef struct multiboot_mmap_entry {
    uint32_t size;             /* Size of this entry in bytes (excluding the size field itself) */
    uint32_t addr_low;         /* Base physical address (lower 32 bits) */
    uint32_t addr_high;        /* Base physical address (upper 32 bits) */
    uint32_t len_low;          /* Length of memory region in bytes (lower 32 bits) */
    uint32_t len_high;         /* Length of memory region in bytes (upper 32 bits) */
    uint32_t type;             /* 1 = Available RAM, others = reserved/ACPI/bad memory */
} __attribute__((packed)) multiboot_mmap_entry_t;

/*
 * Multiboot Information Structure:
 * Populated by GRUB and pointed to by EBX at kernel entry.
 */
typedef struct multiboot_info {
    uint32_t flags;            /* Bitmask showing which of the following fields are valid */

    /* Available memory from BIOS (valid if flags & MULTIBOOT_INFO_FLAG_MEM) */
    uint32_t mem_lower;        /* Lower memory size in KB (0 to 640 KB conventional RAM) */
    uint32_t mem_upper;        /* Upper memory size in KB (above 1 MB, extended RAM) */

    /* Boot device identifier (valid if flags & MULTIBOOT_INFO_FLAG_BOOT_DEVICE) */
    uint32_t boot_device;

    /* Kernel command-line physical address (valid if flags & MULTIBOOT_INFO_FLAG_CMDLINE) */
    uint32_t cmdline;

    /* Boot modules list (valid if flags & MULTIBOOT_INFO_FLAG_MODS) */
    uint32_t mods_count;
    uint32_t mods_addr;

    /* Symbol table info (valid if flags & MULTIBOOT_INFO_FLAG_ELF_SHDR or AOUT) */
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;

    /* Memory map buffer (valid if flags & MULTIBOOT_INFO_FLAG_MMAP) */
    uint32_t mmap_length;      /* Total length of memory map buffer in bytes */
    uint32_t mmap_addr;        /* Physical address of the memory map buffer */

    /* Drives buffer */
    uint32_t drives_length;
    uint32_t drives_addr;

    /* ROM configuration table */
    uint32_t config_table;

    /* Bootloader name string */
    uint32_t boot_loader_name;

    /* APM table */
    uint32_t apm_table;

    /* Video graphics fields */
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    /* Multiboot 0.6.96+ Linear Framebuffer fields (valid if flags & MULTIBOOT_INFO_FLAG_FRAMEBUFFER) */
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  framebuffer_red_field_position;
    uint8_t  framebuffer_red_mask_size;
    uint8_t  framebuffer_green_field_position;
    uint8_t  framebuffer_green_mask_size;
    uint8_t  framebuffer_blue_field_position;
    uint8_t  framebuffer_blue_mask_size;
} __attribute__((packed)) multiboot_info_t;

#endif /* KERNEL_MULTIBOOT_H */
