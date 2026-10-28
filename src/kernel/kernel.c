#include "kernel/multiboot.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "lib/kprintf.h"
#include "lib/types.h"

/* Linker symbols defining kernel physical boundary addresses */
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

/*
 * halt_system:
 * Safely halts the processor in an infinite loop after disabling interrupts.
 */
static void halt_system(void) {
    __asm__ volatile ("cli");
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/*
 * kernel_main:
 * Kernel Entry Point called from boot.asm.
 *
 * Parameters:
 *   magic      - Value in EAX passed by bootloader (expected: 0x2BADB002)
 *   mboot_info - Pointer in EBX to Multiboot information structure
 *
 * Execution flow:
 * 1. Initialize COM1 serial logging.
 * 2. Initialize VGA text display (80x25, address 0xB8000).
 * 3. Validate bootloader magic signature.
 * 4. Display Welcome banner and project details.
 * 5. Parse and display Multiboot memory statistics.
 * 6. Display kernel binary memory boundaries from linker script.
 * 7. Demonstrate and verify VGA scrolling functionality.
 */
void kernel_main(uint32_t magic, multiboot_info_t* mboot_info) {
    /* 1. Initialize serial port (COM1) for host logging */
    serial_init();

    /* 2. Initialize VGA text mode screen */
    vga_init();

    /* 3. Verify that the kernel was booted by a Multiboot-compliant bootloader (GRUB) */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("\n[PANIC] Invalid Multiboot magic: 0x%x (expected 0x%x)\n",
                magic, MULTIBOOT_BOOTLOADER_MAGIC);
        kprintf("Halting system.\n");
        halt_system();
    }

    /* 4. Display kernel welcome banner */
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("================================================================\n");
    kprintf("        MyOS - 32-bit x86 Protected Mode Kernel (v0.1.0)        \n");
    kprintf("           B.Tech Computer Engineering Final Year Project       \n");
    kprintf("================================================================\n\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("[BOOT] Bootloader: GRUB (Multiboot 1 compliant)\n");
    kprintf("[BOOT] Multiboot magic: 0x%x [VALID]\n", magic);

    /* 5. Parse memory info from Multiboot structure */
    if (mboot_info && (mboot_info->flags & MULTIBOOT_INFO_FLAG_MEM)) {
        uint32_t lower_kb = mboot_info->mem_lower;
        uint32_t upper_kb = mboot_info->mem_upper;
        uint32_t total_mb = (lower_kb + upper_kb + 1024) / 1024;

        kprintf("[MEM]  Lower memory:  %u KB (conventional)\n", lower_kb);
        kprintf("[MEM]  Upper memory:  %u KB (extended)\n", upper_kb);
        kprintf("[MEM]  Total RAM:     %u MB\n", total_mb);
    } else {
        kprintf("[MEM]  Warning: Multiboot memory info not provided by bootloader\n");
    }

    /* 6. Display kernel memory boundaries from linker symbols */
    uintptr_t kstart = (uintptr_t)_kernel_start;
    uintptr_t kend   = (uintptr_t)_kernel_end;
    uint32_t  ksize  = (uint32_t)(kend - kstart);

    kprintf("[INFO] Kernel start:  %p\n", (void*)kstart);
    kprintf("[INFO] Kernel end:    %p\n", (void*)kend);
    kprintf("[INFO] Kernel size:   %u bytes (%u KB)\n\n", ksize, (ksize + 1023) / 1024);

    /* 7. Verify scrolling functionality by printing > 25 lines */
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[TEST] Running 28-line scrolling verification test...\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (int i = 1; i <= 28; i++) {
        kprintf("  [Line %d/28] VGA & Serial dual-logging scrolling test\n", i);
    }

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("\n[SUCCESS] Milestone 1 Boot & Hello World completed successfully!\n");
    kprintf("[READY]   System idle. Awaiting next milestone commands.\n");

    /* Halt CPU safely */
    halt_system();
}
