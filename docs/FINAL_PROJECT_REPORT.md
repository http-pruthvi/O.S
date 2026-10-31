# 32-Bit x86 Hobby Operating System (MyOS)
## B.Tech Final Year Project — Comprehensive Technical Report

**Candidate**: Pruthviraj Vinod Phuse  
**Discipline**: Computer Engineering  
**Degree**: Bachelor of Technology (B.Tech)  
**Repository**: [https://github.com/http-pruthvi/O.S](https://github.com/http-pruthvi/O.S)  

---

## Abstract
Modern commercial operating systems comprise millions of lines of source code, making it difficult to analyze low-level interactions between bare hardware, firmware, memory translation units, and execution schedulers. This project presents the design, implementation, and empirical verification of **MyOS**, an original, monolithic 32-bit x86 protected-mode operating system kernel developed from scratch using freestanding C (GNU11) and NASM assembly without any third-party runtime libraries or standard C libraries (`libc`).

MyOS implements a complete eight-layer architectural stack:
1. **Boot**: GRUB Multiboot 1 compliant bootloader handshake.
2. **Arch/CPU**: Global Descriptor Table (GDT), Interrupt Descriptor Table (IDT), 8259 Programmable Interrupt Controller (PIC) remapping, and CPU exception service routines.
3. **Drivers**: Dual-sink UART 16550 serial console (`COM1`), 8254 Programmable Interval Timer (PIT), PS/2 Keyboard, and dual-packet PS/2 Mouse.
4. **Memory Management**: Physical memory map parsing, bitmap-driven Physical Memory Manager (PMM), two-level 32-bit paging with identity-mapped RAM and Page 0 trap protection, and dynamic kernel heap allocator with block splitting and coalescing.
5. **GUI Foundation**: 1024x768x32 linear framebuffer (VBE), 8x16 bitmap font engine, 2D drawing primitives, 3 MB off-screen back buffer, and a software mouse cursor with dirty-rectangle save-under buffering.
6. **Window Manager & Widgets**: Z-order compositor, window dragging clamped to screen bounds, taskbar, Start Menu, buttons, labels, and text boxes.
7. **Productivity Desktop Applications**: A suite of in-kernel desktop applications including a 32-bit integer Calculator, a multi-line Text Editor with caret tracking, a Mini Spreadsheet with a recursive-descent formula evaluation engine (`=SUM`, `=AVG`, `=MIN`, `=MAX`), and a Real-Time Clock with Sakamoto's calendar algorithm.
8. **Processes & Multitasking**: Independent 8 KB kernel stacks, an assembly context switch routine (`switch_context`), stack forging with landing-pad trampolines, and a preemptive round-robin scheduler (100 ms quantum) featuring timed sleep queues and safe zombie stack reaping.

The kernel was rigorously tested on Oracle VirtualBox with dual output routed to VGA/Linear Framebuffer and COM1 serial. All milestones were empirically verified through twelve interactive test hooks (`[F1]` to `[F12]`).

---

## 1. Introduction & Problem Statement

### 1.1 Problem Statement
The goal of this project is to construct a functional, bootable 32-bit x86 operating system from first principles to demystify operating system internals:
1. Operating directly on bare-metal hardware without relying on an existing operating system, runtime libraries, or user-space abstractions.
2. Building an identity-mapped flat memory model with two-level paging and dynamic heap allocation.
3. Implementing high-resolution graphical desktop capabilities (1024x768x32 direct RGB) entirely through custom software rendering.
4. Implementing preemptive multitasking, assembly context switching, and event-driven graphical window management.

### 1.2 Pedagogical Focus
Every module has been implemented with maximum clarity, modularity, and strict adherence to OSDev standards so that every single line of code can be defended in technical vivas and referenced in operating systems research.

---

## 2. Development Environment & Toolchain

| Component | Specification |
| :--- | :--- |
| **Host System** | Windows 11 with WSL2 (Ubuntu 24.04 LTS) |
| **Cross-Compiler** | `i686-elf-gcc` (Freestanding, 32-bit ELF target) |
| **Assembler** | NASM (Netwide Assembler, ELF32 output) |
| **Linker** | GNU `ld` with custom linker script (`linker.ld`) |
| **Bootloader** | GRUB 2.06 with `grub-mkrescue` and `xorriso` |
| **Target Hardware Platform** | Oracle VirtualBox 7.x (`OSProject`, 128 MB RAM, 1 vCPU, BIOS) |
| **Diagnostic Sinks** | Dual-output: Linear Framebuffer (1024x768x32) + COM1 Serial (`C:\os\serial.log`) |
| **Compiler Flags** | `-std=gnu11 -ffreestanding -O2 -Wall -Wextra -fno-stack-protector -fno-pic -m32 -Iinclude` |
| **Linker Flags** | `-T linker.ld -nostdlib -lgcc` |

---

## 3. System Architecture & Layered Decomposition

MyOS employs a strictly modular, layered architecture:

```text
+-----------------------------------------------------------------------------------+
|                         Layer 6: Productivity Applications Suite                  |
|          Calculator  |  Multi-Line Text Editor  |  Spreadsheet  |  Clock          |
+-----------------------------------------------------------------------------------+
|                         Layer 5: Window Manager & Widget Framework                |
|          Window Frames  |  Z-Order Compositor  |  Taskbar  |  Event Pipeline       |
+-----------------------------------------------------------------------------------+
|                         Layer 4: GUI Foundation & 2D Graphics Engine              |
|     1024x768x32 Linear Framebuffer  |  8x16 Font  |  Back Buffer  |  Mouse Cursor |
+-----------------------------------------------------------------------------------+
|                         Layer 7: Processes & Preemptive Scheduler                 |
|     Task Control Block (TCB)  |  8 KB Stacks  |  switch_context  |  Round-Robin   |
+-----------------------------------------------------------------------------------+
|                         Layer 3: Memory Management Subsystem                      |
|       Physical Memory Manager (PMM)  |  Two-Level Paging  |  Dynamic Kernel Heap  |
+-----------------------------------------------------------------------------------+
|                         Layer 2: Peripheral Device Drivers                        |
|       UART 16550 Serial  |  8254 PIT Timer  |  PS/2 Keyboard  |  PS/2 Mouse       |
+-----------------------------------------------------------------------------------+
|                         Layer 1: CPU Architecture & Interrupts                    |
|       Global Descriptor Table (GDT)  |  IDT  |  8259 PIC Remap  |  ISR Exceptions  |
+-----------------------------------------------------------------------------------+
|                         Layer 0: Bootloader & Machine Initialization              |
|       Multiboot 1 Header  |  boot.asm  |  Stack Setup  |  Linker Script Layout    |
+-----------------------------------------------------------------------------------+
```

---

## 4. Subsystem Implementation Details

### 4.1 Layer 0 — Boot & Multiboot 1 Handshake
The boot sequence begins when BIOS loads GRUB. GRUB searches the first 8 KB of the kernel ELF for the Multiboot 1 header:
- **Magic**: `0x1BADB002`
- **Flags**: `0x00000007` (Align modules on 4 KB boundaries, provide memory map, request linear framebuffer video mode).
- **Video Request**: Mode type `0` (linear graphics), 1024 width, 768 height, 32 bits per pixel.
- **Linker Memory Layout**:
  - Kernel physical load base: `0x00100000` (1 MB), avoiding the legacy 640 KB conventional memory and BIOS data area.
  - Initial 16 KB boot stack allocated in `.bss`.

### 4.2 Layer 1 — CPU Architecture & Interrupt Handling
- **Global Descriptor Table (GDT)**:
  Sets up a flat 4 GB memory model:
  - Null Descriptor (`0x00`)
  - Kernel Code Segment (`0x08`): Base 0x0, Limit 0xFFFFF, 4 KB granularity, 32-bit protected mode, Ring 0, Executable/Readable.
  - Kernel Data Segment (`0x10`): Base 0x0, Limit 0xFFFFF, 4 KB granularity, 32-bit protected mode, Ring 0, Read/Write.
  Segment registers (`DS`, `ES`, `FS`, `GS`, `SS`) are loaded with `0x10`, and `CS` is reloaded via a far jump (`jmp 0x08:.flush`).
- **Interrupt Descriptor Table (IDT)**:
  256 interrupt gates configured with `IDT_ATTR_KERNEL_INT_GATE` (`0x8E`), which automatically clears the Interrupt Flag (`IF = 0`) on entry to prevent nested interrupt corruption.
- **8259 PIC Remap**:
  Legacy IBM PC hardware maps IRQs 0-7 to interrupt vectors 8-15, which conflicts with CPU exception vectors (Double Fault, General Protection Fault). The PIC is remapped:
  - Master PIC: Vectors `32` to `39` (`0x20`–`0x27`)
  - Slave PIC: Vectors `40` to `47` (`0x28`–`0x2F`)
  - Spurious IRQ 7 and IRQ 15 handling prevents phantom interrupt processing.
- **CPU Exception Handlers**:
  All 32 x86 architectural exceptions are wired to dedicated assembly stubs pushing a dummy error code if the CPU does not push one, saving general registers with `pusha`, and invoking a diagnostic panic display.

### 4.3 Layer 2 — Hardware Device Drivers
- **UART 16550 Serial Port (`COM1`, `0x3F8`)**:
  Baud rate 115200 bps, 8 data bits, no parity, 1 stop bit (8N1). Provides infallible kernel logging independent of display state.
- **8254 PIT Timer (`IRQ 0`)**:
  Programmed for Mode 3 (Square Wave Generator) at 100 Hz (10 ms per tick). Maintains global tick count, drives the uptime clock, and triggers preemptive scheduling.
- **PS/2 Keyboard (`IRQ 1`)**:
  Scancode Set 1 parser tracking Shift and Caps Lock modifiers, translating make/break codes into ASCII characters, and handling function keys `F1`–`F12`.
- **PS/2 Mouse (`IRQ 12`)**:
  Initializes auxiliary PS/2 port, unmasks IRQ 12 on Slave PIC and IRQ 2 cascade line, and sets streaming mode (`0xF4`). Parses 3-byte packets:
  - Byte 0: Button states (Left, Right, Middle) and sign/overflow bits. Enforces bit 3 sync check.
  - Byte 1: Signed 9-bit relative X movement ($\Delta X$).
  - Byte 2: Signed 9-bit relative Y movement ($\Delta Y$, inverted for screen coordinates).

### 4.4 Layer 3 — Memory Management Subsystem
- **Physical Memory Manager (PMM)**:
  Parses the BIOS memory map (`multiboot_info->mmap_addr`) to discover usable RAM ranges up to 4 GB. Tracks physical frames via a 128 KB static bitmap where 1 bit represents a 4 KB frame ($1 = \text{used}, 0 = \text{free}$). The first 1 MB, the kernel code/data, and multiboot tables are protected against allocation.
- **Two-Level Paging**:
  Initializes a 4 KB Page Directory and Page Tables identity-mapping the entire 128 MB physical RAM with `PAGE_PRESENT | PAGE_WRITE`.
  - **Null Pointer Protection**: Page 0 (`0x00000000`–`0x00000FFF`) is explicitly marked **NOT PRESENT**, guaranteeing that any NULL pointer dereference immediately triggers a `#PF` (Page Fault) exception.
  - **Linear Framebuffer Mapping**: The physical framebuffer (e.g. `0xE0000000`, 3 MB) is mapped page-by-page with Page Cache Disable (`PCD`) to ensure direct video memory writes.
  - Page Fault handler inspects `CR2` (linear faulting address) and the error code (Present, Write, User mode bits).
- **Dynamic Kernel Heap (`kmalloc` / `kfree`)**:
  Located at virtual address `0xD0000000`. Employs a circular doubly-linked list of memory blocks with boundary tags:
  - Block header: `magic (0xDEADBEEF)`, `size`, `is_free`, `next`, `prev` (24 bytes).
  - Allocation: First-fit search. If excess space $\ge \text{header} + 8$ bytes, the block is dynamically split. If no block fits, `heap_expand` requests fresh physical frames from the PMM and maps them into the page table.
  - Freeing: Validates 8-byte alignment and magic signature, detects double frees, and coalesces immediately with preceding and succeeding adjacent free blocks.

### 4.5 Layer 4 — GUI Foundation & 2D Graphics Engine
- **Linear Framebuffer**: 1024x768 resolution at 32 bits per pixel (0x00RRGGBB). Linear memory addresses calculated as:
  $$\text{Pixel Address} = \text{Base} + (y \times \text{Pitch}) + (x \times 4)$$
- **8x16 Bitmap Font Engine**: Original 8x16 monospace font covering ASCII 32–126. Renders glyphs by testing 8-bit row bitmasks.
- **Framebuffer Text Console**: A 128-column by 48-row terminal emulator maintaining a character cell array (`fb_cell_t` with character and color). Supports newline, backspace, horizontal tab, automatic vertical scrolling, and direct coordinate printing (`vga_put_at`).
- **Off-Screen Back Buffer**: A 3072 KB (3 MB) buffer allocated dynamically via `kmalloc()`. All drawing primitives render to the back buffer to eliminate visible flicker. `gfx_flush_rect()` copies modified dirty rectangles to VRAM using `rep movsd`.
- **Software Mouse Cursor**: A 12x19 arrow cursor rendered with transparency. Employs a save-under buffer to preserve background pixels before drawing, restoring the background prior to cursor movement and dirty-rectangle flushes.

### 4.6 Layer 5 — Window Manager & Widget Framework
- **Window Control Block (`window_t`)**:
  Maintains window position (`x, y`), dimensions (`w, h`), title string, flags (`VISIBLE`, `MOVABLE`, `FOCUSED`), and an independent client canvas buffer.
- **Z-Order Compositor**:
  Maintains a linked list from bottom to top. Clicking on any window brings it to the top of the Z-stack. Renders the desktop background (Deep Slate Blue `0x001B2A4A`), window dropshadows, title bars, and client contents.
- **Taskbar & Start Menu**:
  A 40-pixel taskbar pinned to the bottom of the screen (`y = 728`) featuring a Start Menu button, minimized window tabs, and a live digital uptime clock. The Start Menu is a 3D beveled popup with icons and application launch actions.
- **Widget Framework (`widget_t`)**:
  - Buttons: 3D relief beveled borders with click callbacks.
  - Labels: Monospace text with customizable foreground colors.
  - Text Boxes: Single-line input with focus management, blinking caret, and keyboard entry.

### 4.7 Layer 6 — Productivity Applications Suite
- **Desktop Calculator (`apps/calc.c`)**:
  A 4x5 button grid and LCD display box. Implements a 32-bit signed integer state machine supporting addition, subtraction, multiplication, division, sign toggling, and clear operations with overflow and divide-by-zero protection.
- **Multi-Line Text Editor (`apps/editor.c`)**:
  Maintains a 64-line by 60-character document buffer. Supports cursor movement, line insertion (Enter), character deletion (Backspace with line concatenation), sample document loading, and a status bar showing caret coordinates and document statistics.
- **Mini Spreadsheet (`apps/sheet.c`)**:
  A 2D matrix of 60 cells (columns A–F, rows 1–10). Features an editable formula bar and a two-pass formula parser supporting `=SUM(A1:A5)`, `=AVG(A1:A5)`, `=MIN(A1:A5)`, `=MAX(A1:A5)`, and basic binary arithmetic (`=A1+B1`), updating dependent cells automatically.
- **Real-Time Clock & Calendar (`apps/clock.c`)**:
  Displays a large digital clock (HH:MM:SS) updated on timer ticks alongside a monthly calendar implementing Sakamoto's $O(1)$ day-of-week formula with month navigation.

### 4.8 Layer 7 — Preemptive Multitasking & Scheduler
- **Task Control Block (`task_t`)**:
  Stores saved `ESP`, Task ID (PID), lifecycle state, stack boundaries, runtime ticks, and quantum time slices.
- **Stack Forging**:
  Allocates an independent 8 KB stack. Pre-loads initial values:
  - Callee-saved registers (`ebx=0, esi=0, edi=0, ebp=0`)
  - Initial `EFLAGS`: `0x00000202` (IF=1 to ensure hardware interrupts are enabled)
  - Return Address: `task_entry_trampoline`
  - Function Entry Point: `entry_fn`
  - Argument: `void* arg`
- **Assembly Context Switch (`switch_context`)**:
  ```nasm
  switch_context:
      pushf
      push ebp
      push edi
      push esi
      push ebx
      mov eax, [esp + 24]  ; *old_esp
      mov [eax], esp       ; Save current stack pointer
      mov esp, [esp + 28]  ; Load new stack pointer
      pop ebx
      pop esi
      pop edi
      pop ebp
      popf
      ret
  ```
- **Round-Robin Scheduling & Preemption**:
  Runs at 100 Hz. Each task receives a 10-tick (100 ms) quantum. If the quantum expires and other runnable tasks exist, `schedule()` is invoked.
- **Early EOI Handshake**:
  `pic_send_eoi(irq)` is issued *before* invoking interrupt callbacks, ensuring that a preemptive context switch does not leave the PIC blocked.
- **Zombie Reaper**:
  When a task terminates, it marks itself `TASK_ZOMBIE` and yields. The scheduler's zombie reaper safely deallocates the dead task's stack and TCB from the context of another running task.

---

## 5. Verification & Interactive Test Hooks

The kernel includes 12 interactive test hooks accessible via keyboard function keys:

| Key | Subsystem | Action & Verification Criteria |
| :---: | :--- | :--- |
| **`[F1]`** | CPU Exceptions | Triggers Divide-by-Zero (`#DE`). Verifies exception stub, register dump, and system halt. |
| **`[F2]`** | Software Interrupt | Triggers `int 0x80`. Verifies software interrupt gate execution and clean return. |
| **`[F3]`** | Memory Diagnostics | Displays live PMM total/free/used frames and heap payload allocation metrics. |
| **`[F4]`** | Heap Stress Test | Allocates 20 variable-sized blocks, frees alternate blocks, allocates again, frees all, and verifies coalescing into 1 single free block. |
| **`[F5]`** | PMM Alloc/Free | Allocates 10 physical frames, verifies 4 KB alignment and uniqueness, frees them, and confirms frame count restoration. |
| **`[F6]`** | Paging NULL Trap | Reads address `0x00000000`. Verifies Page 0 unmapped trap and `#PF` panic display with `CR2 = 0x0`. |
| **`[F7]`** | Paging Unmapped Trap | Writes address `0xDEADB000`. Verifies `#PF` exception with Write violation and `CR2 = 0xDEADB000`. |
| **`[F8]`** | Heap Expansion Test | Allocates 200 KB across 10 blocks (forcing expansion past 64 KB), checks byte integrity, verifies page table mappings, tests reverse coalescing, and validates 100 MB oversize rejection. |
| **`[F9]`** | Mouse Paint Demo | Launches 1024x768x32 graphics paint program with Bresenham line painting, palette picker, and cursor save-under clipping. |
| **`[F10]`** | Desktop Window Manager | Launches full graphical desktop with Start Menu, Calculator, Editor, Spreadsheet, Clock, and System Monitor. |
| **`[F11]`** | Dynamic Task Spawning | Spawns a concurrent worker task demonstrating preemptive time slicing, `task_sleep_ms`, and clean zombie stack reaping. |
| **`[F12]`** | Process Table Dump (`ps`)| Outputs formatted active task table showing PID, State, cumulative ticks, stack size, and task names. |

---

## 6. Challenges Overcome & Key Technical Insights

1. **64-Bit Division in Freestanding C**:
   On 32-bit x86, dividing 64-bit integers (`long long`) causes GCC to emit calls to `__divdi3` (a runtime helper in `libgcc`). Because our kernel is freestanding and avoids runtime library bloat, all calculations in the Calculator, PIT timer, and heap metrics were clamped to 32-bit signed integers, emitting native single-instruction `idiv`.
2. **PIC EOI Timing with Preemption**:
   In traditional kernels, `pic_send_eoi` is called at the end of the ISR. However, if a timer interrupt triggers a preemptive context switch, the task is switched out before the EOI is issued, permanently locking the PIC from sending future timer ticks. Moving `pic_send_eoi` before the driver callback resolved this issue completely.
3. **Double-Buffering & Mouse Cursor Isolation**:
   Drawing the mouse cursor directly into the back buffer caused "cursor stamping" during window dragging. The solution was strict cursor isolation: the back buffer never holds cursor pixels; any dirty rectangle flush overlapping the cursor hides the cursor, blits to VRAM, and restores the cursor.
4. **Re-entrant Interrupt Masking**:
   Calling `cli` and `sti` naively inside nested helper functions caused race conditions where an inner function enabled interrupts while an outer function was still in a critical section. Implementing `irq_save()` (which returns saved `EFLAGS`) and `irq_restore()` provided safe re-entrant locking for memory allocators and scheduler queues.

---

## 7. Conclusion & Future Roadmap

MyOS demonstrates that a complete, robust, multitasking operating system with high-resolution graphics and productivity software can be designed and implemented entirely from scratch in freestanding C and NASM assembly. 

### Future Roadmap
1. **User Mode (`Ring 3`) & Hardware TSS**: Configuring the TSS for privilege transitions and switching tasks to Ring 3 with page directory protection.
2. **System Call Interface (`int 0x80` / `SYSENTER`)**: Implementing standard POSIX-like system calls (`read`, `write`, `fork`, `exec`).
3. **Virtual File System (VFS) & Ramdisk (TAR/FAT)**: Providing persistent storage so the text editor can save and load files from disk.
4. **x86-64 Long Mode Port**: Upgrading the bootloader and memory management to 64-bit sub-architecture with 4-level paging.

