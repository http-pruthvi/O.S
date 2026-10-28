# MyOS - 32-bit x86 Hobby Operating System

A 32-bit x86 protected mode operating system kernel written from scratch in freestanding C and NASM assembly as a B.Tech Computer Engineering Final Year Project.

---

## Architecture Summary
- **Target Architecture**: x86 32-bit (i686) protected mode, flat memory model, identity-mapped at 1 MB.
- **Bootloader**: GRUB with Multiboot 1 standard (`0x1BADB002`).
- **Core Languages**: Freestanding C (GNU11) and NASM Assembly (ELF32). Zero libc dependencies.
- **Display**: VGA text mode 3 (80x25 characters) at physical address `0xB8000`.
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
│   └── milestone-1.md      # Milestone 1 report and viva Q&A
├── include/                # Header files
│   ├── drivers/
│   │   ├── serial.h        # COM1 UART 16550 serial driver
│   │   └── vga.h           # VGA 80x25 text mode driver
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
    ├── boot/
    │   └── boot.asm        # Multiboot header, 16 KB stack, _start entry
    ├── drivers/
    │   ├── serial.c        # COM1 driver (115200 8N1, FIFO enabled)
    │   └── vga.c           # VGA driver (cursor, scrolling, colors)
    ├── kernel/
    │   └── kernel.c        # kernel_main initialization and demo
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
