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
#include "gfx/fb.h"
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

#define HEAP_GROWTH_TEST_BLOCKS 10
#define HEAP_GROWTH_TEST_BLOCK_SIZE (20 * 1024) /* 20 KB per block = 200 KB total */
#define HEAP_OVERSIZED_TEST_SIZE (100 * 1024 * 1024) /* 100 MB request */

/*
 * test_heap_growth:
 * Test hook for F8 key.
 * Exercises dynamic heap expansion (heap_expand) by allocating 200 KB total
 * (10 blocks of 20 KB), which forces the kernel heap to expand well past its
 * initial 64 KB boundary by requesting frames from PMM and mapping them.
 * Verifies byte pattern integrity, heap break growth, PMM frame consumption,
 * page table mappings via paging_get_phys(), reverse-order kfree coalescing,
 * and out-of-bounds allocation rejection.
 */
static void test_heap_growth(void) {
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("\n>>> [TEST HOOK] F8 pressed: Running Heap Growth Test (Past 64 KB) <<<\n");

    /* Sub-check A: Record initial heap and PMM statistics before growth */
    uintptr_t break_before = heap_get_break();
    size_t used_before = 0, free_before = 0, count_before = 0;
    heap_stats(&used_before, &free_before, &count_before);
    uint32_t pmm_free_before = pmm_free_frames();

    kprintf("  [BEFORE] Heap break: %p | Free: %u B | Blocks: %u | PMM free frames: %u\n",
            (void*)break_before, (uint32_t)free_before, (uint32_t)count_before, pmm_free_before);

    /* Sub-check B: Allocate 10 blocks of 20 KB (forces heap growth past 64 KB) */
    void* blocks[HEAP_GROWTH_TEST_BLOCKS];
    bool alloc_ok = true;

    kprintf("  [ALLOC]  Allocating %d blocks of %u KB each (%u KB total)...\n",
            HEAP_GROWTH_TEST_BLOCKS, HEAP_GROWTH_TEST_BLOCK_SIZE / 1024,
            (HEAP_GROWTH_TEST_BLOCKS * HEAP_GROWTH_TEST_BLOCK_SIZE) / 1024);

    for (int i = 0; i < HEAP_GROWTH_TEST_BLOCKS; i++) {
        blocks[i] = kmalloc(HEAP_GROWTH_TEST_BLOCK_SIZE);
        if (!blocks[i]) {
            kprintf("  [FAIL] kmalloc returned NULL for block #%d\n", i);
            alloc_ok = false;
            break;
        }

        /* Verify 8-byte alignment required by system architecture */
        if (((uintptr_t)blocks[i] & 0x7) != 0) {
            kprintf("  [FAIL] Block #%d (%p) is not 8-byte aligned\n", i, blocks[i]);
            alloc_ok = false;
            break;
        }
    }

    if (!alloc_ok) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf(">>> [FAIL] Heap allocation failed during growth! <<<\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        /* Clean up any allocated blocks */
        for (int i = 0; i < HEAP_GROWTH_TEST_BLOCKS; i++) {
            if (blocks[i]) kfree(blocks[i]);
        }
        return;
    }

    /* Fill each block with a unique deterministic byte pattern */
    for (int i = 0; i < HEAP_GROWTH_TEST_BLOCKS; i++) {
        uint8_t* p = (uint8_t*)blocks[i];
        for (size_t j = 0; j < HEAP_GROWTH_TEST_BLOCK_SIZE; j++) {
            /* Distinct formula per block ensures cross-block leaks are detected */
            p[j] = (uint8_t)((i * 37 + j) & 0xFF);
        }
    }

    /* Read back and verify byte pattern across all 10 blocks */
    bool pattern_ok = true;
    for (int i = 0; i < HEAP_GROWTH_TEST_BLOCKS; i++) {
        uint8_t* p = (uint8_t*)blocks[i];
        for (size_t j = 0; j < HEAP_GROWTH_TEST_BLOCK_SIZE; j++) {
            uint8_t expected = (uint8_t)((i * 37 + j) & 0xFF);
            if (p[j] != expected) {
                kprintf("  [FAIL] Data mismatch in block #%d at offset %u (expected 0x%x, got 0x%x)\n",
                        i, (uint32_t)j, expected, p[j]);
                pattern_ok = false;
                break;
            }
        }
        if (!pattern_ok) break;
    }

    if (pattern_ok) {
        kprintf("  [PASS] All 10 blocks verified (8-byte aligned, data integrity intact).\n");
    } else {
        kprintf("  [FAIL] Memory integrity corruption detected!\n");
    }

    /* Sub-check C: Record stats after growth */
    uintptr_t break_after = heap_get_break();
    uint32_t pmm_free_after = pmm_free_frames();
    uint32_t growth_bytes = (uint32_t)(break_after - break_before);
    uint32_t pmm_frame_delta = pmm_free_before - pmm_free_after;

    kprintf("  [AFTER]  Heap break: %p (increased by %u B / %u pages)\n",
            (void*)break_after, growth_bytes, growth_bytes / PAGE_SIZE_4K);
    kprintf("  [AFTER]  PMM free frames: %u (decreased by %u frames)\n",
            pmm_free_after, pmm_frame_delta);

    bool growth_ok = (break_after > break_before) && (pmm_free_after < pmm_free_before);
    if (growth_ok) {
        kprintf("  [PASS] Heap break advanced and PMM free frames decreased.\n");
    } else {
        kprintf("  [FAIL] Heap break did not expand or PMM frame count did not decrease!\n");
    }

    /* Sub-check D: Verify first and last heap pages are actively mapped in paging tables */
    uintptr_t first_page_virt = HEAP_START_ADDR;
    uintptr_t last_page_virt = break_after - PAGE_SIZE_4K;
    uintptr_t first_page_phys = paging_get_phys(first_page_virt);
    uintptr_t last_page_phys = paging_get_phys(last_page_virt);

    kprintf("  [PAGE]   First heap page (%p) mapped to physical: %p\n",
            (void*)first_page_virt, (void*)first_page_phys);
    kprintf("  [PAGE]   Last heap page  (%p) mapped to physical: %p\n",
            (void*)last_page_virt, (void*)last_page_phys);

    bool pages_mapped_ok = (first_page_phys != 0) && (last_page_phys != 0);
    if (pages_mapped_ok) {
        kprintf("  [PASS] Both first and last heap pages verified as present in page tables.\n");
    } else {
        kprintf("  [FAIL] Heap boundary virtual pages are unmapped in page tables!\n");
    }

    /* Sub-check E: Free all blocks in reverse order and verify coalescing */
    kprintf("  [FREE]   Freeing all 10 blocks in reverse order...\n");
    for (int i = HEAP_GROWTH_TEST_BLOCKS - 1; i >= 0; i--) {
        if (blocks[i]) {
            kfree(blocks[i]);
            blocks[i] = NULL;
        }
    }

    size_t used_freed = 0, free_freed = 0, count_freed = 0;
    heap_stats(&used_freed, &free_freed, &count_freed);
    uint32_t expected_total_free = (uint32_t)(break_after - HEAP_START_ADDR - sizeof(heap_block_t));

    kprintf("  [FREE]   Post-free stats: Blocks: %u | Used: %u B | Free: %u B (Expected: %u B)\n",
            (uint32_t)count_freed, (uint32_t)used_freed, (uint32_t)free_freed, expected_total_free);

    bool coalesce_ok = (count_freed == 1) && (used_freed == 0) && (free_freed == expected_total_free);
    if (coalesce_ok) {
        kprintf("  [PASS] All blocks coalesced into 1 free block covering the entire heap.\n");
    } else {
        kprintf("  [FAIL] Coalescing check failed! (Blocks: %u, Expected: 1)\n", (uint32_t)count_freed);
    }

    /* Sub-check F: Request allocation exceeding max capacity (100 MB) */
    kprintf("  [OVERFLOW] Requesting 100 MB (exceeds max heap capacity)...\n");
    void* oversize_ptr = kmalloc(HEAP_OVERSIZED_TEST_SIZE);
    bool overflow_ok = (oversize_ptr == NULL);
    if (overflow_ok) {
        kprintf("  [PASS] kmalloc(100 MB) correctly returned NULL without kernel panic.\n");
    } else {
        kprintf("  [FAIL] kmalloc(100 MB) unexpectedly succeeded (ptr: %p)!\n", oversize_ptr);
        kfree(oversize_ptr);
    }

    /* Sub-check G: Final summary verdict */
    bool overall_pass = alloc_ok && pattern_ok && growth_ok && pages_mapped_ok && coalesce_ok && overflow_ok;
    if (overall_pass) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf(">>> [PASS] F8: Heap Growth Test completed successfully! <<<\n\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf(">>> [FAIL] F8: Heap Growth Test encountered failures! <<<\n\n");
    }
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
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

    /* Step 11b: Buffer Multiboot linear framebuffer info BEFORE paging */
    fb_early_init(mboot_info);

    /* Step 12: Physical Memory Manager (Bitmap Allocator) */
    pmm_init(mboot_info);
    kprintf("[OK] Physical memory manager initialized (%u MB free)\n",
            (pmm_free_frames() * 4) / 1024);

    /* Step 13: Two-Level Paging (Identity maps RAM, Page 0 unmapped) */
    paging_init();
    kprintf("[OK] Two-level paging enabled (CR3 loaded, CR0.PG set)\n");

    /* Step 13b: Map Linear Framebuffer into page tables */
    if (fb_map_pages()) {
        kprintf("[OK] Linear Framebuffer mapped into page tables\n");
    }

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
    kprintf("  - [F7]: Page Fault: Unmapped Write [F8]: Heap Growth Test (Past 64 KB)\n\n");

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
            } else if (uc == KEY_F8) {
                test_heap_growth();
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
