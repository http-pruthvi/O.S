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
- Display: Linear Framebuffer (1024x768x32 RGB) via VBE/Multiboot 1, with VGA text mode fallback at 0xB8000.
- Input: PS/2 Keyboard (IRQ 1) and PS/2 Mouse (IRQ 12).
- Debug output: all kernel logging goes to BOTH active display (VGA text or FB console) and COM1 serial.
- Scope: Mouse-driven graphical desktop OS with window manager and productivity mini-apps. Single CPU, no SMP, no ACPI, no USB, no networking, no CLI shell.

## 4. Architecture Overview (Layers)
- Layer 0 - Boot: boot.asm (Multiboot header with video mode request, stack, call kernel_main)
- Layer 1 - Arch/CPU: GDT, IDT, ISR/IRQ stubs, PIC remap, PIT timer
- Layer 2 - Drivers: serial (COM1), keyboard (PS/2), mouse (PS/2, IRQ 12), timer
- Layer 3 - Memory: physical memory manager (bitmap), paging (including framebuffer mapping), kernel heap (kmalloc/kfree)
- Layer 4 - GUI Foundation: linear framebuffer, 8x16 bitmap font, framebuffer text console, drawing primitives, back buffer, software cursor (Milestone 4)
- Layer 5 - Window Manager & Widgets: windows, title bars, dragging, z-order, buttons, labels, text boxes, taskbar, event dispatch (Milestone 5)
- Layer 6 - Desktop Apps: calculator, text editor (Notepad-like), mini spreadsheet (Excel-like), clock (Milestone 6)
- Layer 7 - Processes & Scheduler: kernel tasks, context switch (asm), round-robin scheduler (parked plan) (Milestone 7)
- Support: lib/ with kernel helpers (string functions, minimal kprintf, panic).

Boot flow: Power on -> BIOS -> GRUB -> switches to VBE linear framebuffer 1024x768x32 -> loads kernel ELF at 1 MB -> jumps to _start in boot.asm with EAX/EBX set -> boot.asm sets up stack, passes multiboot info to kernel_main -> kernel_main initializes drivers, maps memory and framebuffer, and enters GUI mode.

## 5. Target Folder Structure (create only what the current milestone needs; add the rest as we go)
```text
myos/
  src/
    boot/        boot.asm
    arch/        gdt, idt, isr, irq, pic, pit        (Milestone 2)
    drivers/     vga, serial, keyboard, mouse        (mouse in M4)
    mm/          pmm, paging, heap                    (Milestone 3)
    gfx/         fb_console, font8x16, gfx, cursor   (Milestone 4)
    gui/         window, widget, event                (Milestone 5)
    apps/        editor, calc, sheet, clock           (Milestone 6)
    proc/        task, scheduler, switch.asm          (Milestone 7)
    lib/         string, kprintf, panic
    kernel/      kernel.c (kernel_main and init order)
  include/       header files mirroring src/ layout
  docs/          PROJECT_BRIEF.md, architecture notes, milestone notes, diagrams
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
- M1 (DONE): Boot & Hello World: boot stub, linker script, VGA + serial output, build/ISO/deploy pipeline
- M2 (DONE): Interrupts & Drivers: GDT, IDT, ISRs, PIC remap, PIT timer, PS/2 keyboard
- M3 (DONE): Memory: physical memory manager (using multiboot memory map), paging, kernel heap (with F8 growth test)
- M4 (DONE): GUI Foundation: graphics mode (linear framebuffer 1024x768x32), framebuffer text console, drawing primitives, 8x16 font, PS/2 mouse driver (IRQ12), software mouse cursor, mouse paint demo
- M5: Window Manager and Widgets: windows, title bars, dragging, z-order, buttons, labels, text boxes, taskbar, event dispatch
- M6: Apps: calculator, text editor (Notepad-like), mini spreadsheet (Excel-like), clock (apps run as windows inside the kernel)
- M7: Processes & Scheduler: kernel tasks, context switch, round-robin scheduler (parked plan)
- M8: Docs, report, demo video, viva prep
(Optional if time remains: syscalls and user mode, ramdisk filesystem so editor can save files, 64-bit port).
