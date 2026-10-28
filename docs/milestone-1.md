# Milestone 1: Boot & Hello World

## 1. Goal & Deliverables
The objective of Milestone 1 is to establish a bare-metal 32-bit x86 kernel boot pipeline and print verified system information to both the VGA screen and COM1 serial output without relying on any external C library.

### What Was Built
1. **Multiboot 1 Bootloader Integration**: Compliant header (`0x1BADB002`) allowing GRUB to load our kernel at 1 MB in 32-bit protected mode.
2. **Early Kernel Assembly Stub (`boot.asm`)**: Sets up a 16 KB kernel stack in `.bss`, passes Multiboot information pointer (`EBX`) and magic (`EAX`) to C code, and manages safe CPU halt loops.
3. **Linker Script (`linker.ld`)**: Places the Multiboot header in the first 8 KB, aligns all sections (`.text`, `.rodata`, `.data`, `.bss`) to 4 KB page boundaries, and exports `_kernel_start` and `_kernel_end`.
4. **Local Freestanding Library**: Local fixed-width types (`types.h`), inline port I/O routines (`outb`, `inb`, `io_wait`), freestanding string routines (`strlen`, `memset`, `memcpy`, `memcmp`), and minimal `kprintf` supporting `%c`, `%s`, `%d`, `%u`, `%x`, `%p`, `%%`.
5. **VGA Text Mode Driver (`vga.c`/`.h`)**: Direct framebuffer driver at physical `0xB8000` supporting 80x25 characters, color attributes, tab stops, backspace, hardware cursor updates via CRT Controller registers (`0x3D4`/`0x3D5`), and vertical line scrolling.
6. **UART 16550 Serial Driver (`serial.c`/`.h`)**: COM1 driver configured for 115200 baud, 8N1, FIFO enabled, with polling transmission and CRLF translation.
7. **Kernel Main (`kernel.c`)**: Initializes drivers, validates Multiboot boot signature, displays welcome banner, parses available conventional and extended memory from GRUB, displays kernel memory addresses, and verifies screen scrolling by outputting 28 numbered test lines.
8. **ISO & Deployment Pipeline (`Makefile` & `grub.cfg`)**: Out-of-tree builds compiling with GCC 15 and NASM, producing bootable GRUB ISO via `grub-mkrescue`, and deploying to `/mnt/c/os/myos.iso`.

---

## 2. How It Works

```text
[ Power Button / Reset ]
          │
          ▼
   [ BIOS POST ]
   Checks hardware, loads MBR from CD-ROM/ISO
          │
          ▼
   [ GRUB Bootloader ]
   Reads ISO filesystem, loads /boot/myos.bin
   Verifies Multiboot magic 0x1BADB002
   Switches CPU to 32-bit Protected Mode (CR0.PE = 1)
   Passes EAX = 0x2BADB002, EBX = pointer to multiboot_info_t
   Jumps to kernel entry at 0x00100000
          │
          ▼
   [ boot.asm: _start ]
   Sets ESP = stack_top (16 KB stack in .bss)
   Pushes EBX (multiboot_info*), pushes EAX (magic)
   Calls kernel_main()
          │
          ▼
   [ kernel.c: kernel_main ]
   ├── serial_init(): Sets up COM1 UART (115200 8N1)
   ├── vga_init(): Clears 80x25 buffer at 0xB8000, enables cursor
   ├── Verifies EAX == 0x2BADB002
   ├── kprintf(): Dual-writes banner + memory stats to VGA & COM1
   └── Runs 28-line scroll test -> enters cli; hlt loop
```

---

## 3. Key Concepts Explained Simply

### 1. What is the Multiboot Specification and Header?
* **Problem**: Different bootloaders historically loaded kernels in incompatible ways, requiring developers to write complex 16-bit real mode boot sectors.
* **Solution**: The Multiboot standard (defined by the Free Software Foundation) creates a universal contract between bootloaders (like GRUB) and operating system kernels.
* **How it works**: The kernel embeds a 12-byte header in its first 8 KB containing a magic number (`0x1BADB002`), flags, and a checksum (`-(magic + flags)`). When GRUB spots this header, it switches the CPU to 32-bit protected mode, enables the A20 address line, loads our kernel at 1 MB, and hands over control with `EAX = 0x2BADB002` and `EBX = multiboot_info_t*`.

### 2. What is 32-bit Protected Mode?
* On power-up, an x86 CPU starts in **16-bit Real Mode** for backward compatibility with the 1978 Intel 8086 (addressing only 1 MB using segmented memory `segment:offset`).
* In **32-bit Protected Mode**, the CPU can address 4 GB of memory (`2^32` bytes) linearly, supports hardware memory protection rings (Ring 0 for kernel, Ring 3 for userspace), and supports virtual memory paging. GRUB performs the transition to protected mode before executing our kernel.

### 3. How does the VGA Text Buffer work?
* The PC architecture maps the physical address range `0x000B8000` to `0x000BFFFF` directly to video card memory for text mode 3 (80 columns by 25 rows).
* Each character on screen occupies exactly 2 bytes in memory:
  * **Byte 0 (Low Byte)**: ASCII character code (e.g. `'A'` = `0x41`).
  * **Byte 1 (High Byte)**: Attribute byte containing a 4-bit foreground color (bits 0-3) and a 4-bit background color (bits 4-7).
* Changing a character on screen simply requires writing a 16-bit word to `0xB8000 + 2 * (row * 80 + col)`.

### 4. How does the UART 16550 Serial Port work?
* The Universal Asynchronous Receiver-Transmitter (UART) is a dedicated microchip (or emulated virtual device) mapped to I/O ports starting at `0x3F8` (COM1).
* Unlike memory-mapped VGA, the CPU communicates with the UART using port I/O instructions (`inb` and `outb`).
* To send a byte, the kernel polls the **Line Status Register (LSR)** at port `0x3FD`. When bit 5 (Transmitter Holding Register Empty) reads `1`, the kernel writes the character to the **Data Register** at port `0x3F8`. VirtualBox captures this output and writes it to `C:\os\serial.log`.

### 5. What does the Linker Script (`linker.ld`) do?
* A compiler produces raw object files (`.o`) with relative addresses. The linker script instructs the GNU Linker (`ld`) where to position each section in physical memory.
* It ensures:
  1. The entry point is set to symbol `_start`.
  2. The kernel load address begins at 1 MB (`0x00100000`), keeping the lower 1 MB free for BIOS and hardware memory maps.
  3. The `.multiboot` section appears first so GRUB can locate it within the first 8 KB.
  4. Code (`.text`), read-only constants (`.rodata`), initialized data (`.data`), and zeroed stack/variables (`.bss`) are aligned to 4 KB page boundaries.

---

## 4. Files Created and Their Roles

| File | Role |
| :--- | :--- |
| [`src/boot/boot.asm`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/boot/boot.asm) | Multiboot header, 16 KB stack allocation, entry point `_start`, argument passing to `kernel_main`. |
| [`linker.ld`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/linker.ld) | Memory layout script positioning kernel at 1 MB, ordering sections, exporting `_kernel_start`/`_kernel_end`. |
| [`include/lib/types.h`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/include/lib/types.h) | Project-local freestanding types (`uint8_t`, `uint16_t`, `uint32_t`, `size_t`, `bool`, `NULL`). |
| [`include/lib/io.h`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/include/lib/io.h) | Inline assembly wrappers for hardware port I/O (`outb`, `inb`, `io_wait`). |
| [`include/lib/string.h`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/include/lib/string.h) / [`src/lib/string.c`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/lib/string.c) | Freestanding string and memory helpers (`strlen`, `memset`, `memcpy`, `memcmp`). |
| [`include/lib/kprintf.h`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/include/lib/kprintf.h) / [`src/lib/kprintf.c`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/lib/kprintf.c) | Kernel formatted print engine routing characters to both VGA and COM1 sinks. |
| [`include/drivers/vga.h`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/include/drivers/vga.h) / [`src/drivers/vga.c`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/drivers/vga.c) | 80x25 text mode driver at `0xB8000`, cursor update, backspace/newline/tab, line scrolling. |
| [`include/drivers/serial.h`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/include/drivers/serial.h) / [`src/drivers/serial.c`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/drivers/serial.c) | COM1 UART 16550 serial driver (115200 8N1, FIFO enabled, polling transmission). |
| [`include/kernel/multiboot.h`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/include/kernel/multiboot.h) | Multiboot header and information structure structs and flag bitmasks. |
| [`src/kernel/kernel.c`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/src/kernel/kernel.c) | Kernel main entry point, hardware initialization, multiboot validation, memory reporting, and scroll test. |
| [`iso/boot/grub/grub.cfg`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/iso/boot/grub/grub.cfg) | GRUB boot menu configuration loading `/boot/myos.bin`. |
| [`Makefile`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/Makefile) | Out-of-tree build rules for assembly, compilation, linking, ISO packaging, and deployment. |
| [`.gitignore`](file:///c:/Users/pruthvi/Desktop/Hobbies/O.S/.gitignore) | Filters build artifacts (`build/`, `*.o`, `*.bin`, `*.iso`). |

---

## 5. How to Build, Deploy & Test

### Build & Deploy in WSL2:
Run from `~/myos` in your WSL terminal:
```bash
make clean
make iso
make deploy
```
This produces `/mnt/c/os/myos.iso` on the Windows filesystem.

### Test in VirtualBox:
Run the VBoxManage creation script in Windows PowerShell (see Section 6 of main walkthrough) and start the VM.

---

## 6. Five Likely Viva Questions & Answers

### Q1: Why is the kernel loaded at 1 MB (`0x00100000`) instead of address 0x00000000?
**Answer**:
The first 1 MB of physical memory on x86 PCs is known as the "Real Mode Address Space" and contains legacy architectural mappings:
- `0x00000000 - 0x000003FF`: Real Mode Interrupt Vector Table (IVT)
- `0x00000400 - 0x000004FF`: BIOS Data Area (BDA)
- `0x000A0000 - 0x000BFFFF`: Video Display RAM (including VGA text mode at `0xB8000`)
- `0x000C0000 - 0x000FFFFF`: BIOS ROM and motherboard firmware
Loading the kernel at or above 1 MB guarantees that our code and stack will never overwrite vital hardware structures or video memory.

### Q2: Why must the Multiboot header be located in the first 8192 bytes of the binary?
**Answer**:
The Multiboot 1 specification dictates that bootloaders will only scan the first 8192 bytes (8 KB) of the OS executable image for the magic signature `0x1BADB002`. In `linker.ld`, we place the `.multiboot` section before `.text` to ensure it resides immediately at offset 0 of the ELF executable.

### Q3: Why is a kernel stack allocated in `boot.asm` before calling C code?
**Answer**:
When GRUB jumps to `_start`, the CPU is in 32-bit protected mode, but the stack pointer (`ESP`) is either uninitialized or pointing to bootloader memory that can be reclaimed at any time. C functions rely on a stack for pushing stack frames, local variables, return addresses, and parameter passing (`cdecl`). We allocate 16 KB of zeroed memory in `.bss` and load `ESP` with `stack_top` before calling `kernel_main`.

### Q4: Why do we compile with `-ffreestanding` and `-nostdlib`?
**Answer**:
- `-ffreestanding` informs GCC that the program is running without an underlying operating system or standard C runtime environment (no `glibc`, no default `main()` wrapper, and standard library headers like `<stdio.h>` or `<stdlib.h>` are not assumed).
- `-nostdlib` tells the linker not to link standard startup files (`crt0.o`) or standard system libraries, preventing linker errors for missing OS syscall implementations like `write()` or `exit()`.

### Q5: How does the kernel write to the screen without graphical drivers?
**Answer**:
The kernel uses the standard IBM PC VGA text mode 3 (80 columns x 25 rows). In this mode, the display hardware continuously scans physical memory at `0x000B8000` to refresh the CRT/LCD monitor. Each character cell consists of two consecutive bytes: the ASCII character code and an attribute byte (foreground/background color). By writing directly to `0xB8000`, characters appear immediately on screen without GPU drivers or graphics primitives.
