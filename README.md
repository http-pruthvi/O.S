# MyOS - 32-bit x86 Hobby Operating System

A 32-bit x86 protected mode operating system kernel written from scratch in freestanding C and NASM assembly as a B.Tech Computer Engineering Final Year Project.

---

## Architecture Summary
- **Target Architecture**: x86 32-bit (i686) protected mode, flat memory model, identity-mapped at 1 MB.
- **Bootloader**: GRUB with Multiboot 1 standard (`0x1BADB002`).
- **Core Languages**: Freestanding C (GNU11) and NASM Assembly (ELF32). Zero libc dependencies.
- **Display**: Linear Framebuffer (1024x768x32 direct RGB) via VBE/Multiboot 1 with VGA text mode fallback at `0xB8000`.
- **Input Devices**: PS/2 Keyboard (IRQ 1) and PS/2 Mouse (IRQ 12).
- **Debugging & Logging**: Dual-output via `kprintf` to both VGA screen and COM1 UART serial port (`0x3F8`).
- **Target Test Environment**: Oracle VirtualBox (VM name: `OSProject`, 128 MB RAM, 1 CPU, BIOS firmware).

---

## Project Structure
```text
myos/
├── Makefile                # Build, ISO packaging, and deploy targets
├── linker.ld               # Kernel physical memory layout at 1 MB
├── README.md               # Project documentation & usage guide
├── AGENTS.md               # System rules and master brief (single source of truth)
├── .gitignore              # Build artifacts filter
├── docs/                   # Viva prep, reports, and architecture diagrams
│   ├── PROJECT_BRIEF.md    # Reference copy of the project brief
│   ├── architecture.md     # Boot flow sequence and memory maps
│   ├── milestone-1.md      # Milestone 1 report and viva Q&A
│   ├── milestone-2.md      # Milestone 2 report and viva Q&A
│   ├── milestone-3.md      # Milestone 3 report and viva Q&A
│   └── milestone-4.md      # Milestone 4 report and viva Q&A
├── include/                # Header files
│   ├── arch/               # Architecture & hardware abstraction
│   │   ├── gdt.h           # Global Descriptor Table
│   │   ├── idt.h           # Interrupt Descriptor Table
│   │   ├── isr.h           # CPU Exception handlers & registers_t frame
│   │   ├── pic.h           # 8259A PIC initialization & EOI
│   │   └── irq.h           # Hardware IRQ dispatch table
│   ├── drivers/
│   │   ├── serial.h        # COM1 UART 16550 serial driver
│   │   ├── vga.h           # VGA output driver (console routing)
│   │   ├── timer.h         # PIT 8254 timer driver (100 Hz, uptime)
│   │   ├── keyboard.h      # PS/2 keyboard driver (Scancode Set 1)
│   │   └── mouse.h         # PS/2 mouse driver (IRQ 12, packet parser)
│   ├── gui/                # Window Manager & Widgets (Milestone 5)
│   │   ├── window.h        # Window structures, title bars & client canvas
│   │   ├── wm.h            # Z-order compositor, taskbar & dragging
│   │   ├── widget.h        # Buttons, labels & single-line text boxes
│   │   └── desktop.h       # Milestone 5 interactive desktop environment
│   ├── gfx/                # Graphical Subsystem
│   │   ├── fb.h            # Linear Framebuffer parser & paging mapper
│   │   ├── font.h          # 8x16 bitmap font definition
│   │   ├── fb_console.h    # 128x48 terminal emulator over framebuffer
│   │   ├── gfx.h           # 2D primitives & 3 MB off-screen back buffer
│   │   ├── cursor.h        # Software arrow cursor & save-under engine
│   │   └── paint_demo.h    # Interactive mouse paint demo application
│   ├── mm/                 # Memory Management
│   │   ├── memmap.h        # Multiboot memory map parser
│   │   ├── pmm.h           # Bitmap Physical Memory Manager
│   │   ├── paging.h        # Two-Level 32-bit Paging & Page Faults
│   │   └── heap.h          # Dynamic Kernel Heap (kmalloc, kfree)
│   ├── kernel/
│   │   └── multiboot.h     # Multiboot 1 struct and flag definitions
│   └── lib/
│       ├── io.h            # Hardware port I/O helpers (outb, inb)
│       ├── kprintf.h       # Dual-sink formatted print engine
│       ├── string.h        # Freestanding string and memory utilities
│       └── types.h         # Fixed-width integer types
├── iso/
│   └── boot/
│       └── grub/
│           └── grub.cfg    # GRUB boot menu entry configuration
└── src/                    # Implementation source code
    ├── arch/
    │   ├── gdt.c / gdt_flush.asm   # GDT setup and far jump reload
    │   ├── idt.c                   # IDT setup and lidt
    │   ├── isr.c / isr_stubs.asm   # CPU exception panic dump & int 0x80
    │   ├── pic.c                   # 8259 PIC remap & spurious IRQ handling
    │   └── irq.c / irq_stubs.asm   # IRQ dispatcher & assembly stubs
    ├── mm/
    │   ├── memmap.c                # Memory map traversal & buffering
    │   ├── pmm.c                   # 128 KB bitmap frame allocator
    │   ├── paging.c / paging_asm.asm # Page Directory & CR3/CR0.PG
    │   └── heap.c                  # Dynamic heap (split & coalesce)
    ├── boot/
    │   └── boot.asm        # Multiboot header, 16 KB stack, _start entry
    ├── drivers/
    │   ├── serial.c        # COM1 driver (115200 8N1, FIFO enabled)
    │   ├── vga.c           # VGA driver (cursor, scrolling, vga_put_at)
    │   ├── timer.c         # PIT timer (100 Hz, uptime screen clock)
    │   ├── keyboard.c      # PS/2 keyboard (scancode set 1, ring buffer)
    │   └── mouse.c         # PS/2 mouse driver (IRQ 12, packet parser)
    ├── gfx/
    │   ├── fb.c            # Framebuffer detection & paging mapper
    │   ├── font8x16.c      # 8x16 bitmap font table (ASCII 32-126)
    │   ├── fb_console.c    # 128x48 terminal emulator over framebuffer
    │   ├── gfx.c           # 2D primitives & 3 MB off-screen back buffer
    │   ├── cursor.c        # Software arrow cursor & save-under engine
    │   └── paint_demo.c    # Interactive mouse paint demo application
    ├── gui/
    │   ├── window.c        # Window rendering, frame, title bar & client canvas
    │   ├── wm.c            # Z-order compositor, taskbar, dragging & hit test
    │   ├── widget.c        # 3D relief buttons, labels & text boxes
    │   └── desktop.c       # Desktop showcase environment with 3 windows
    ├── kernel/
    │   └── kernel.c        # kernel_main init order, echo loop & test hooks
    └── lib/
        ├── kprintf.c       # kprintf parser (%c, %s, %d, %u, %x, %p)
        └── string.c        # strlen, memset, memcpy, memcmp
```

---

## Building and Deploying

The kernel is developed and built natively in WSL2 (Ubuntu 24.04):

```bash
# 1. Navigate to project root in WSL
cd ~/myos

# 2. Build the kernel binary
make build

# 3. Build bootable GRUB ISO image
make iso

# 4. Deploy ISO to Windows filesystem (C:\os\myos.iso)
make deploy
```

---

## Testing in VirtualBox

1. Ensure `C:\os\myos.iso` has been deployed.
2. In Windows PowerShell, create and launch the VM using `VBoxManage`:
   See [`docs/milestone-1.md`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/docs/milestone-1.md) for the exact command script.
3. Observe the system banner and memory info on the VM screen, and inspect `C:\os\serial.log` for matching output.

### Interactive Test Hooks
- **`[F1]`**: Divide-by-Zero exception (`#DE`)
- **`[F2]`**: Software interrupt (`int 0x80`)
- **`[F3]`**: Live PMM physical frames and heap allocation statistics
- **`[F4]`**: 20-block heap stress and coalescing verification test
- **`[F5]`**: 10-frame PMM physical allocation and free test
- **`[F6]`**: Page Fault exception via NULL pointer read (`0x00000000`)
- **`[F7]`**: Page Fault exception via unmapped write (`0xDEADB000`)
- **`[F8]`**: Heap Growth Test (allocates 200 KB across 10 blocks of 20 KB to force expansion past 64 KB, validates pattern integrity, verifies PMM frame consumption, checks page table mappings, tests reverse-order `kfree` coalescing, and verifies 100 MB oversize rejection)
- **`[F9]`**: Mouse Paint Demo (launches interactive 1024x768x32 graphics paint demo with 3 MB off-screen back buffer, Bresenham line painting, palette selection, clear button, software arrow cursor with save-under buffer, and ESC key return)
- **`[F10]`**: Desktop Window Manager (launches full graphical windowed desktop environment featuring 3 overlapping windows, interactive buttons with 3D relief, live text input box with keyboard focus, system monitor displaying PMM and Heap stats, taskbar with window tabs and uptime clock, title bar dragging clamped to screen bounds, and ESC key return to console)
