# 32-bit x86 Hobby OS (B.Tech Final Year Project) - Project Brief & Rules

## 1. Project Context
- Final-year B.Tech Computer Engineering student building an operating system kernel from scratch as final year project.
- Must understand and be able to explain every line of code for viva and project report.
- Simplicity and clarity beat cleverness and performance.
- Code must be original (follow standard OSDev concepts, but do not paste large blocks from existing projects or tutorials).
- Everything must be documented well enough to reuse in project report.

## 2. Environment
- Development: Linux or WSL2 (Ubuntu). Project folder lives inside the Linux filesystem (or synced via WSL).
- Toolchain: i686-elf-gcc cross compiler + binutils, NASM, GNU Make, GRUB tools (grub-mkrescue), xorriso, mtools.
- Test platform: VirtualBox only. VM name: OSProject, OS type "Other" (32-bit), 128 MB RAM, 1 CPU, BIOS (EFI off), I/O APIC off, ISO attached as IDE DVD, boot order optical first.
- Serial logging: VM COM1 (0x3F8, IRQ 4) is redirected to a file on the Windows side: C:\os\serial.log. Pasted back to AI when debugging.
- Deploy flow: build in WSL -> Makefile "deploy" copies myos.iso to /mnt/c/os/myos.iso -> run in VirtualBox. (If on native Linux, deploy target is skipped.)
- Do NOT run QEMU unless explicitly asked. VirtualBox has no GDB stub; for logging rely on VGA + serial output.

## 3. Technical Decisions (Fixed - Do not change without asking)
- Architecture: x86 32-bit (i686) protected mode, flat memory model, identity-mapped kernel (no higher-half kernel).
- Boot: GRUB with Multiboot 1 (magic 0x1BADB002). GRUB passes EAX = 0x2BADB002 and EBX = pointer to multiboot info structure. Kernel is loaded at 1 MB (0x00100000).
- Language: freestanding C (gnu99 or gnu11) + NASM assembly. No libc, no external libraries.
- Compiler flags: -ffreestanding -O2 -Wall -Wextra -fno-stack-protector -fno-pic -m32 (as applicable to i686-elf-gcc); linker flags: -nostdlib, link libgcc.
- Display: VGA text mode 80x25 at 0xB8000.
- Debug output: all kernel logging goes to BOTH VGA and COM1 serial.
- Single CPU, no SMP, no ACPI, no USB, no networking, no GUI. These are out of scope.

## 4. Architecture Overview (Layers)
- Layer 0 - Boot: boot.asm (Multiboot header, stack, call kernel_main)
- Layer 1 - Arch/CPU: GDT, IDT, ISR/IRQ stubs, PIC remap, PIT timer
- Layer 2 - Drivers: VGA text, serial (COM1), keyboard (PS/2), timer
- Layer 3 - Memory: physical memory manager (bitmap), paging, kernel heap (kmalloc/kfree)
- Layer 4 - Process: PCB/task structures, context switch (asm), round-robin scheduler
- Layer 5 - Syscall/User mode: int 0x80 interface, ring 3, TSS
- Layer 6 - Filesystem: initrd/ramdisk, simple VFS (open/read/close/readdir)
- Layer 7 - Shell: command parser, built-in commands, program execution
- Support: lib/ with kernel helpers (string functions, minimal kprintf, panic).

Boot flow: Power on -> BIOS -> GRUB -> loads kernel ELF at 1 MB -> jumps to _start in boot.asm with EAX/EBX set -> boot.asm sets up stack, passes multiboot info to kernel_main -> kernel_main initializes drivers and prints.

## 5. Target Folder Structure (create only what the current milestone needs; add the rest as we go)
```text
myos/
  src/
    boot/        boot.asm
    arch/        gdt, idt, isr, irq, pic, pit        (Milestone 2)
    drivers/     vga, serial, keyboard                (vga+serial in M1)
    mm/          pmm, paging, heap                    (Milestone 3)
    proc/        task, scheduler, switch.asm          (Milestone 4)
    syscall/     syscall table and handlers           (Milestone 5)
    fs/          initrd, vfs                          (Milestone 6)
    shell/       shell, commands                      (Milestone 7)
    lib/         string, kprintf, panic
    kernel/      kernel.c (kernel_main and init order)
  include/       header files mirroring src/ layout
  docs/          PROJECT_BRIEF.md, architecture notes, milestone notes, diagrams (Mermaid or ASCII)
  iso/boot/grub/ grub.cfg
  linker.ld
  Makefile
  README.md
```

## 6. Coding Standards
- One module per .c/.h pair, small files, include guards in all headers.
- Fixed-width types (uint8_t, uint16_t, uint32_t) from a project-local types header (no libc stdint dependency issues; freestanding stdint.h/stddef.h from the compiler is fine).
- Every non-trivial line commented with WHY, not just WHAT. Every function has a short header comment (purpose, parameters, return).
- Consistent naming: snake_case functions, module prefix (vga_putchar, serial_write, gdt_init).
- No magic numbers: use named constants/macros for ports, addresses, flags.
- Inline assembly only where necessary, wrapped in small named helper functions (outb, inb, io_wait, etc.).
- Compile with zero warnings.

## 7. Workflow Rules
- Work ONLY on the milestone named. Never build ahead (for example, do not add GDT/IDT during Milestone 1).
- For each milestone: (1) give a short plan and wait for approval, (2) implement, (3) build and deploy, (4) state exactly what should be seen on screen and in serial.log, (5) update docs.
- When an error, build log or serial.log is pasted, diagnose root cause before changing code, and explain simply.
- Prefer smallest change that fixes a problem. Do not refactor unrelated files.
- Git: initialize a repo; one commit per logical step with clear messages like "m1: add multiboot boot stub". Add a .gitignore for build outputs (build/, *.o, *.iso, *.bin).
- If a decision has trade-offs, present 2-3 options briefly and recommend one, then wait for choice.

## 8. Documentation Rules (for report)
After every milestone, update README.md and add docs/milestone-N.md containing:
- Goal and what was built
- How it works (plain language, with a small diagram in Mermaid or ASCII)
- Key concepts explained simply (e.g., what is the Multiboot header, what is the VGA buffer)
- Files created/changed and their roles
- How to build, deploy and test, plus expected output
- 5 likely viva questions with short answers
Keep a running docs/architecture.md that is updated as layers get added.

## 9. Roadmap (16 weeks)
- M1 (Wk 1-2): Boot & Hello World: boot stub, linker script, VGA + serial output, build/ISO/deploy pipeline
- M2 (Wk 3-4): Interrupts & Drivers: GDT, IDT, ISRs, PIC remap, PIT timer, PS/2 keyboard
- M3 (Wk 5-6): Memory: physical memory manager (using multiboot memory map), paging, kernel heap
- M4 (Wk 7-9): Processes: task structure, context switching, round-robin scheduler
- M5 (Wk 10-11): Syscalls & user mode: int 0x80, TSS, ring 3
- M6 (Wk 12-13): Filesystem: initrd/ramdisk, simple VFS
- M7 (Wk 14-15): Shell: ls, cat, clear, help, run program
- M8 (Wk 16): Docs, report, demo video, viva prep
