/* ==============================================================================
 * linker.ld - Linker Script for 32-bit x86 Hobby OS Kernel
 * ==============================================================================
 * Architecture: x86 32-bit ELF (i686)
 *
 * Why this file exists:
 * By default, GCC links programs to start at address 0x08048000 for Linux userspace.
 * For a bare-metal kernel booted by GRUB:
 * 1. The entry point must be _start (in boot.asm).
 * 2. The kernel image must be loaded at 1 MB (0x00100000), leaving the first 1 MB
 *    reserved for BIOS Data Area (BDA), VGA text buffer (0xB8000), and ROM expansions.
 * 3. The .multiboot section must be placed first so the Multiboot header appears
 *    within the first 8 KB of the ELF file.
 * 4. We export symbols _kernel_start and _kernel_end so the C kernel knows its
 *    exact memory boundaries at runtime.
 * ============================================================================== */

ENTRY(_start)

SECTIONS
{
    /* Begin placing sections at 1 MB (0x100000) */
    . = 1M;

    /* Mark the beginning of the kernel in physical memory */
    _kernel_start = .;

    /* Multiboot header section: MUST be placed at the very start of the binary */
    .multiboot ALIGN(4K) :
    {
        KEEP(*(.multiboot))
    }

    /* Executable code (.text) */
    .text ALIGN(4K) :
    {
        *(.text*)
    }

    /* Read-only data (constants, string literals) */
    .rodata ALIGN(4K) :
    {
        *(.rodata*)
    }

    /* Initialized read-write data (.data) */
    .data ALIGN(4K) :
    {
        *(.data*)
    }

    /* Uninitialized read-write data and stack (.bss) */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss*)
    }

    /* Mark the end of the kernel in physical memory */
    _kernel_end = .;
}
