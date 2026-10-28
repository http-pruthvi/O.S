#include "kernel/multiboot.h"
#include "arch/gdt.h"
#include "arch/idt.h"
#include "arch/isr.h"
#include "arch/pic.h"
#include "arch/irq.h"
#include "mm/memmap.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "mm/heap.h"
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
 * Uses inline assembly to force a divide-by-zero that cannot be optimized away.
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
 * print_memory_statistics:
 * Test hook for F3 key.
 * Displays live physical frame counts and heap allocation metrics.
 */
static void print_memory_statistics(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("\n======================= MEMORY STATISTICS =======================\n");
    uint32_t total_f = pmm_total_frames();
    uint32_t used_f  = pmm_used_frames();
    uint32_t free_f  = pmm_free_frames();
    kprintf("[PMM]  Total frames: %u (%u MB)\n", total_f, (total_f * 4) / 1024);
    kprintf("[PMM]  Used frames:  %u (%u MB)\n", used_f, (used_f * 4) / 1024);
    kprintf("[PMM]  Free frames:  %u (%u MB)\n", free_f, (free_f * 4) / 1024);

    size_t heap_used = 0, heap_free = 0, heap_blocks = 0;
    heap_stats(&heap_used, &heap_free, &heap_blocks);
    kprintf("[HEAP] Used payload: %u bytes\n", (uint32_t)heap_used);
    kprintf("[HEAP] Free payload: %u bytes\n", (uint32_t)heap_free);
    kprintf("[HEAP] Block count:  %u\n", (uint32_t)heap_blocks);
    kprintf("=================================================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

/*
 * test_heap_stress_and_coalesce:
 * Test hook for F4 key.
 * Allocates 20 blocks of varying sizes, frees alternate blocks, allocates again,
 * frees all blocks, and verifies complete coalescing back into a single free block.
 */
static void test_heap_stress_and_coalesce(void) {
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("\n>>> [TEST HOOK] F4 pressed: Running Heap Stress & Coalescing Test <<<\n");

    void* blocks[20];
    size_t sizes[20] = {
        16, 32, 64, 48, 128, 256, 30, 80, 512, 100,
        24, 72, 120, 200, 60, 400, 300, 150, 800, 64
    };

    /* 1. Allocate 20 blocks */
    kprintf("  Step 1: Allocating 20 blocks with varying sizes (16 B - 800 B)...\n");
    for (int i = 0; i < 20; i++) {
        blocks[i] = kmalloc(sizes[i]);
        if (!blocks[i]) {
            kprintf("  [FAIL] kmalloc failed at block %d\n", i);
            vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            return;
        }
    }

    /* 2. Free every other block (even indices: 0, 2, 4...) */
    kprintf("  Step 2: Freeing alternate blocks (even indices)...\n");
    for (int i = 0; i < 20; i += 2) {
        kfree(blocks[i]);
        blocks[i] = NULL;
    }

    /* 3. Re-allocate into freed holes */
    kprintf("  Step 3: Re-allocating into freed memory holes...\n");
    for (int i = 0; i < 20; i += 2) {
        blocks[i] = kmalloc(sizes[i] / 2 + 8);
    }

    /* 4. Free all blocks */
    kprintf("  Step 4: Freeing all remaining blocks...\n");
    for (int i = 0; i < 20; i++) {
        if (blocks[i]) {
            kfree(blocks[i]);
            blocks[i] = NULL;
        }
    }

    /* 5. Check coalescing */
    size_t used = 0, free_b = 0, count = 0;
    heap_stats(&used, &free_b, &count);
    heap_dump();

    if (used == 0 && count == 1) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("  [PASS] Heap stress test passed! All blocks coalesced into 1 free block.\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  [FAIL] Coalescing verification failed! (Used: %u, Blocks: %u)\n",
                (uint32_t)used, (uint32_t)count);
    }
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

/*
 * test_pmm_alloc_free:
 * Test hook for F5 key.
 * Allocates 10 frames from PMM, prints physical addresses, frees them,
 * and verifies free frame count returns exactly to the initial count.
 */
static void test_pmm_alloc_free(void) {
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("\n>>> [TEST HOOK] F5 pressed: Running PMM Allocation & Free Test <<<\n");

    uint32_t start_free = pmm_free_frames();
    kprintf("  Initial free frames: %u\n", start_free);

    uintptr_t frames[10];
    kprintf("  Allocating 10 physical frames:\n");
    for (int i = 0; i < 10; i++) {
        frames[i] = pmm_alloc_frame();
        kprintf("    Frame #%d: %p\n", i, (void*)frames[i]);
    }

    uint32_t mid_free = pmm_free_frames();
    kprintf("  Free frames after allocation: %u (Decremented by %u)\n",
            mid_free, start_free - mid_free);

    kprintf("  Freeing 10 physical frames in reverse order...\n");
    for (int i = 9; i >= 0; i--) {
        pmm_free_frame(frames[i]);
    }

    uint32_t end_free = pmm_free_frames();
    kprintf("  Final free frames: %u\n", end_free);

    if (end_free == start_free) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("  [PASS] PMM frame allocator and deallocator verified successfully!\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("  [FAIL] Frame leak detected! (Expected: %u, Got: %u)\n",
                start_free, end_free);
    }
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

/*
 * trigger_null_page_fault:
 * Test hook for F6 key.
 * Attempts to read from virtual address 0x00000000.
 * Must use volatile to prevent compiler optimization.
 */
static void trigger_null_page_fault(void) {
    kprintf("\n[TEST HOOK] F6 pressed: Reading address 0x0 (NULL pointer dereference)...\n");
    volatile uint32_t* null_ptr = (volatile uint32_t*)0x0;
    volatile uint32_t val = *null_ptr;
    (void)val;
}

/*
 * trigger_unmapped_page_fault:
 * Test hook for F7 key.
 * Attempts to write to unmapped address 0xDEADB000.
 * Must use volatile to prevent compiler optimization.
 */
static void trigger_unmapped_page_fault(void) {
    kprintf("\n[TEST HOOK] F7 pressed: Writing to unmapped address 0xDEADB000...\n");
    volatile uint32_t* unmapped_ptr = (volatile uint32_t*)0xDEADB000;
    *unmapped_ptr = 0xCAFEBABE;
}

/*
 * kernel_main:
 * Milestone 3 Kernel Entry Point.
 */
void kernel_main(uint32_t magic, multiboot_info_t* mboot_info) {
    /* Step 1: Initialize serial port (COM1) */
    serial_init();

    /* Step 2: Initialize VGA text display */
    vga_init();

    /* Step 3: Validate Multiboot magic */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("\n[PANIC] Invalid Multiboot magic: 0x%x (expected 0x%x)\n",
                magic, MULTIBOOT_BOOTLOADER_MAGIC);
        halt_system();
    }

    /* Welcome Banner */
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("================================================================\n");
    kprintf("     MyOS - 32-bit x86 Kernel (Milestone 3: Memory Management)  \n");
    kprintf("================================================================\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("[OK] Serial COM1 initialized (115200 8N1)\n");
    kprintf("[OK] VGA text mode initialized (80x25 at 0xB8000)\n");

    /* Step 4: GDT */
    gdt_init();
    kprintf("[OK] GDT initialized (Flat 4GB code & data segments)\n");

    /* Step 5: IDT */
    idt_init();
    kprintf("[OK] IDT initialized (256 gates)\n");

    /* Step 6: CPU Exceptions */
    isr_init();
    kprintf("[OK] CPU exception handlers registered (Vectors 0-31, 128)\n");

    /* Step 7: 8259 PIC Remap */
    pic_init();
    kprintf("[OK] 8259 PIC remapped (Master: 32-39, Slave: 40-47)\n");

    /* Step 8: IRQs */
    irq_init();
    kprintf("[OK] Hardware IRQ dispatch table initialized\n");

    /* Step 9: PIT Timer */
    timer_init(100);
    kprintf("[OK] PIT timer initialized (100 Hz, Channel 0)\n");

    /* Step 10: Keyboard */
    keyboard_init();
    kprintf("[OK] PS/2 keyboard driver initialized (IRQ 1)\n");

    /* Step 11: Multiboot Memory Map (Parsed and copied into kernel buffers BEFORE paging) */
    memmap_init(mboot_info);
    kprintf("[OK] Multiboot memory map parsed & buffered in kernel memory\n");

    /* Step 12: Physical Memory Manager (Bitmap Allocator) */
    pmm_init(mboot_info);
    kprintf("[OK] Physical memory manager initialized (%u MB free)\n",
            (pmm_free_frames() * 4) / 1024);

    /* Step 13: Two-Level Paging (Identity maps RAM, Page 0 unmapped) */
    paging_init();
    kprintf("[OK] Two-level paging enabled (CR3 loaded, CR0.PG set)\n");

    /* Step 14: Dynamic Kernel Heap */
    heap_init();
    kprintf("[OK] Dynamic kernel heap initialized at virtual %p\n", (void*)HEAP_START_ADDR);

    /* Step 15: Enable hardware interrupts ONLY after memory subsystems are complete */
    __asm__ volatile ("sti");
    kprintf("[OK] Hardware interrupts enabled (STI)\n\n");

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf(">>> Interactive Console & Memory Test Hooks Ready <<<\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Key Bindings:\n");
    kprintf("  - [F1]: Divide by Zero (#DE)       [F2]: Software Interrupt (int 0x80)\n");
    kprintf("  - [F3]: Memory Statistics          [F4]: Heap Stress & Coalesce Test\n");
    kprintf("  - [F5]: PMM Alloc/Free Test        [F6]: Page Fault: NULL Read (0x0)\n");
    kprintf("  - [F7]: Page Fault: Unmapped Write (0xDEADB000)\n\n");

    uint32_t last_serial_uptime = 0;

    /* Interactive Console Loop */
    while (1) {
        /* Log uptime to serial every 5 seconds */
        uint32_t current_uptime = timer_get_uptime_sec();
        if (current_uptime >= last_serial_uptime + 5) {
            last_serial_uptime = current_uptime;
            serial_writestring("[UPTIME] System running: ");
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

        /* Keystroke handling */
        if (keyboard_has_char()) {
            char c = keyboard_getchar();
            unsigned char uc = (unsigned char)c;

            if (uc == KEY_F1) {
                trigger_divide_by_zero();
            } else if (uc == KEY_F2) {
                trigger_software_interrupt();
            } else if (uc == KEY_F3) {
                print_memory_statistics();
            } else if (uc == KEY_F4) {
                test_heap_stress_and_coalesce();
            } else if (uc == KEY_F5) {
                test_pmm_alloc_free();
            } else if (uc == KEY_F6) {
                trigger_null_page_fault();
            } else if (uc == KEY_F7) {
                trigger_unmapped_page_fault();
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
