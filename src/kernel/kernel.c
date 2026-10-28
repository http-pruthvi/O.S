#include "kernel/multiboot.h"
#include "arch/gdt.h"
#include "arch/idt.h"
#include "arch/isr.h"
#include "arch/pic.h"
#include "arch/irq.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "drivers/timer.h"
#include "drivers/keyboard.h"
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
 * trigger_divide_by_zero:
 * Test hook for F1 key.
 * Uses inline assembly to force a divide-by-zero that cannot be optimized away by GCC.
 */
static void trigger_divide_by_zero(void) {
    kprintf("\n[TEST HOOK] F1 pressed: Triggering Divide-by-Zero exception (#DE)...\n");
    __asm__ volatile (
        "mov $0, %%ecx\n\t"
        "div %%ecx\n\t"
        : : : "eax", "edx", "ecx"
    );
}

/*
 * trigger_software_interrupt:
 * Test hook for F2 key.
 * Executes 'int 0x80' to demonstrate a software interrupt gate returning cleanly.
 */
static void trigger_software_interrupt(void) {
    kprintf("\n[TEST HOOK] F2 pressed: Triggering software interrupt (int 0x80)...\n");
    __asm__ volatile ("int $0x80");
}

/*
 * kernel_main:
 * Milestone 2 Kernel Entry Point.
 *
 * Sequence:
 * 1. Initialize serial & VGA
 * 2. Validate Multiboot magic
 * 3. Initialize GDT, IDT, ISRs, PIC, IRQs, Timer, Keyboard
 * 4. Enable CPU interrupts (STI)
 * 5. Enter interactive keystroke echo loop with live uptime logging & test hooks
 */
void kernel_main(uint32_t magic, multiboot_info_t* mboot_info) {
    /* Step 1: Initialize serial port (COM1) */
    serial_init();

    /* Step 2: Initialize VGA text display */
    vga_init();

    /* Step 3: Validate bootloader magic */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("\n[PANIC] Invalid Multiboot magic: 0x%x (expected 0x%x)\n",
                magic, MULTIBOOT_BOOTLOADER_MAGIC);
        halt_system();
    }

    /* Welcome Banner */
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("================================================================\n");
    kprintf("   MyOS - 32-bit x86 Kernel (Milestone 2: Interrupts & Drivers) \n");
    kprintf("================================================================\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("[OK] Serial COM1 initialized (115200 8N1)\n");
    kprintf("[OK] VGA text mode initialized (80x25 at 0xB8000)\n");

    /* Memory Info from Multiboot */
    if (mboot_info && (mboot_info->flags & MULTIBOOT_INFO_FLAG_MEM)) {
        uint32_t total_mb = (mboot_info->mem_lower + mboot_info->mem_upper + 1024) / 1024;
        kprintf("[OK] RAM detected: %u MB (Lower: %u KB, Upper: %u KB)\n",
                total_mb, mboot_info->mem_lower, mboot_info->mem_upper);
    }

    /* Step 4: Initialize Global Descriptor Table (GDT) */
    gdt_init();
    kprintf("[OK] GDT initialized (Flat 4GB code & data segments)\n");

    /* Step 5: Initialize Interrupt Descriptor Table (IDT) */
    idt_init();
    kprintf("[OK] IDT initialized (256 gates cleared)\n");

    /* Step 6: Install CPU Exception Service Routines */
    isr_init();
    kprintf("[OK] CPU exception handlers registered (Vectors 0-31, 128)\n");

    /* Step 7: Remap 8259 PIC (Master: 32-39, Slave: 40-47) */
    pic_init();
    kprintf("[OK] 8259 PIC remapped (Master: 32-39, Slave: 40-47)\n");

    /* Step 8: Initialize IRQ Dispatcher */
    irq_init();
    kprintf("[OK] Hardware IRQ dispatch table initialized\n");

    /* Step 9: Initialize Programmable Interval Timer (100 Hz) */
    timer_init(100);
    kprintf("[OK] PIT timer initialized (100 Hz, Channel 0)\n");

    /* Step 10: Initialize PS/2 Keyboard Driver */
    keyboard_init();
    kprintf("[OK] PS/2 keyboard driver initialized (IRQ 1)\n");

    /* Step 11: Enable CPU hardware interrupts */
    __asm__ volatile ("sti");
    kprintf("[OK] Hardware interrupts enabled (STI)\n\n");

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf(">>> Interactive Echo Console Ready <<<\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Instructions:\n");
    kprintf("  - Type any text: echoed to VGA and logged to COM1 serial.\n");
    kprintf("  - Press [F1]: Triggers Divide-by-Zero exception (#DE panic dump).\n");
    kprintf("  - Press [F2]: Triggers Software Interrupt (int 0x80 test hook).\n\n");

    /* Track last uptime logged to serial (log every 5 seconds) */
    uint32_t last_serial_uptime = 0;

    /* Step 12: Interactive Console Loop */
    while (1) {
        /* Check if 5 seconds have elapsed to log uptime to serial */
        uint32_t current_uptime = timer_get_uptime_sec();
        if (current_uptime >= last_serial_uptime + 5) {
            last_serial_uptime = current_uptime;
            /* Log exclusively to COM1 serial without cluttering the interactive screen */
            char log_msg[64];
            (void)log_msg;
            serial_writestring("[UPTIME] System running: ");
            /* Format uptime seconds manually to avoid screen pollution */
            uint32_t s = current_uptime;
            char s_buf[16];
            int s_idx = 0;
            if (s == 0) {
                s_buf[s_idx++] = '0';
            } else {
                while (s > 0) {
                    s_buf[s_idx++] = (char)('0' + (s % 10));
                    s /= 10;
                }
            }
            for (int i = s_idx - 1; i >= 0; i--) {
                serial_putchar(s_buf[i]);
            }
            serial_writestring(" seconds\n");
        }

        /* Check for keystrokes */
        if (keyboard_has_char()) {
            char c = keyboard_getchar();

            if ((unsigned char)c == KEY_F1) {
                trigger_divide_by_zero();
            } else if ((unsigned char)c == KEY_F2) {
                trigger_software_interrupt();
            } else if (c == '\n') {
                kprintf("\n");
            } else if (c == '\b') {
                kprintf("\b");
            } else {
                kprintf("%c", c);
            }
        } else {
            /* Sleep until next hardware interrupt */
            __asm__ volatile ("hlt");
        }
    }
}
