# System Architecture & Boot Flow

This document details the architectural layout, memory map, and end-to-end boot sequence of the 32-bit x86 Hobby Operating System.

---

## 1. End-to-End Boot Flow

```mermaid
sequenceDiagram
    autonumber
    participant Hardware as PC Hardware / BIOS
    participant Bootloader as GRUB (Multiboot 1)
    participant BootStub as boot.asm (_start)
    participant Kernel as kernel.c (kernel_main)
    participant Drivers as VGA & COM1 Drivers

    Hardware->>Bootloader: 1. Power-on -> BIOS POST -> loads GRUB from ISO
    Note over Bootloader: GRUB switches CPU to 32-bit Protected Mode (CR0.PE = 1)<br/>Paging disabled, A20 line enabled
    Bootloader->>Bootloader: 2. Reads ELF binary, finds Multiboot magic (0x1BADB002) within first 8 KB
    Bootloader->>BootStub: 3. Loads kernel to physical 0x00100000 (1 MB) and jumps to _start<br/>EAX = 0x2BADB002, EBX = multiboot_info_t*
    BootStub->>BootStub: 4. Sets up 16 KB kernel stack (ESP = stack_top), clears EFLAGS
    BootStub->>Kernel: 5. Pushes EBX, pushes EAX, calls kernel_main(magic, mboot_info)
    Kernel->>Drivers: 6. Calls serial_init() [115200 8N1 on COM1]
    Kernel->>Drivers: 7. Calls vga_init() [Clears 80x25 buffer at 0xB8000, homes cursor]
    Kernel->>Kernel: 8. Verifies magic == 0x2BADB002
    Kernel->>Kernel: 9. gdt_init() [Flushes GDT: code 0x08, data 0x10]
    Kernel->>Kernel: 10. idt_init() & isr_init() [256 gates, exceptions 0-31, int 0x80]
    Kernel->>Kernel: 11. pic_init() [Remap master 32-39, slave 40-47, spurious filters]
    Kernel->>Kernel: 12. irq_init() [Installs IRQ stubs 32-47]
    Kernel->>Drivers: 13. timer_init(100) [PIT Channel 0 @ 100 Hz, unmasks IRQ 0]
    Kernel->>Drivers: 14. keyboard_init() [PS/2 IRQ 1, ring buffer, unmasks IRQ 1]
    Kernel->>Kernel: 15. STI enables CPU hardware interrupts
    Kernel->>Drivers: 16. Enters interactive typing echo console loop & test hooks
```

---

## 2. Physical Memory Map (1 MB Identity-Mapped)

The kernel is loaded at 1 MB (`0x00100000`) to avoid conflicts with BIOS and legacy hardware reserved memory in low RAM:

```text
+-----------------------------------+ 0xFFFFFFFF (4 GB)
|                                   |
|      Unmapped Extended Memory     |
|                                   |
+-----------------------------------+
|      Available Extended RAM       |
+-----------------------------------+ _kernel_end (~0x00105000)
|      .bss (Stack 16 KB)           |
|      .data (Initialized globals)  |
|      .rodata (String literals)    |
|      .text (Executable code)      |
|      .multiboot (GRUB header)     |
+-----------------------------------+ 0x00100000 (1 MB) [_kernel_start]
|      BIOS ROM & Shadow RAM        |
+-----------------------------------+ 0x000C0000
|      VGA Text Buffer (0xB8000)    | -> 80x25 characters (4000 bytes)
+-----------------------------------+ 0x000A0000 (Video RAM)
|      Extended BIOS Data Area(EBDA)|
+-----------------------------------+ 0x0009FC00
|      Conventional RAM (Free)      |
+-----------------------------------+ 0x00007E00
|      MBR Boot Sector Area         |
+-----------------------------------+ 0x00007C00
|      BIOS Data Area (BDA)         |
+-----------------------------------+ 0x00000400
|      Real Mode IVT (256 vectors)  |
+-----------------------------------+ 0x00000000
```

---

## 3. Dual-Sink Kernel Logging Architecture

All kernel messages passed to `kprintf()` are mirrored across two hardware paths simultaneously:

```text
                      +-------------------+
                      |   kprintf(...)    |
                      +---------+---------+
                                |
                                v
                      +-------------------+
                      |      kputc(c)     |
                      +----+---------+----+
                           |         |
            +--------------+         +--------------+
            |                                       |
            v                                       v
  +--------------------+                  +--------------------+
  |    vga_putchar     |                  |   serial_putchar   |
  +---------+----------+                  +---------+----------+
            |                                       |
            v                                       v
  [0xB8000 Video RAM]                     [Port 0x3F8 UART]
  80x25 CRT Text Display                  COM1 Serial Output
  (Visible on Screen)                     (Redirected to C:\os\serial.log)
```

1. **VGA Path**: Writes 16-bit entries (ASCII character + color attribute byte) directly to physical memory at `0xB8000`. Handles line feeds (`\n`), carriage returns (`\r`), tab stops (`\t`), backspaces (`\b`), line scrolling (rows 1..24 shifted up), and updates the blinking hardware CRT cursor via I/O ports `0x3D4` / `0x3D5`.
2. **Serial Path**: Writes ASCII bytes directly to COM1 base port `0x3F8`. Polling Line Status Register (`0x3FD`) bit 5 guarantees bytes are not dropped. Bare `\n` is converted to CRLF (`\r\n`) so log files viewed in Windows editors format correctly.

---

## 4. Descriptor Tables & Interrupt Architecture (Milestone 2)

```text
+-------------------+      +-------------------+      +-------------------+
|     GDT (4 GB)    |      |    IDT (256)      |      |   PIC (Remapped)  |
| 0x00: Null        |      | 0-31: Exceptions  |      | Master: 32 - 39   |
| 0x08: Kernel Code |      | 32-47: IRQs       |      | Slave:  40 - 47   |
| 0x10: Kernel Data |      | 128: Software INT |      | Cascaded at IRQ 2 |
+-------------------+      +-------------------+      +-------------------+
```

### Stack Frame Layout Note (`registers_t`)
> **Important Architectural Note**:
> In Milestone 2, the kernel executes entirely in **Ring 0**. Because interrupts occur without a change in privilege level, the x86 CPU pushes only `EFLAGS`, `CS`, and `EIP` upon interrupt entry. The CPU does **NOT** push `useresp` or `ss` onto the stack.
>
> In **Milestone 5**, when Ring 3 user mode is introduced, the CPU will automatically push `useresp` and `ss` upon entering Ring 0 from Ring 3. The `registers_t` structure will be extended in Milestone 5 to include those fields.

---

## 5. Memory Management Architecture (Milestone 3)

```text
Physical Memory (PMM)                Virtual Memory (Paging)
+----------------------------+       +----------------------------+ 0xFFFFFFFF
| Available Frames (Type 1)  |       | Unmapped Address Space     |
| (Managed by 128KB Bitmap)  |       +----------------------------+ 0xD4000000 (64MB)
+----------------------------+       | Virtual Kernel Heap        |
| Reserved Bootloader & Mmap |  ==>  | (Grows upwards on demand)  |
+----------------------------+       +----------------------------+ 0xD0000000 (Heap Start)
| Kernel Image & .bss Bitmap |       | Unmapped Address Space     |
+----------------------------+       +----------------------------+ 0x10000000 (256MB Cap)
| BIOS ROM & VGA Buffer      |       | Identity-Mapped RAM        |
+----------------------------+       | (Virtual == Physical)      |
| Frame 0 (0x0 - 0xFFF)      |       +----------------------------+ 0x00001000 (4KB)
+----------------------------+ 0x0   | UNMAPPED Page 0 (NULL Trap)|
                                     +----------------------------+ 0x00000000
```

* **PMM (Bitmap)**: Statically tracks 1,048,576 4 KB frames using 131,072 bytes (128 KB) in `.bss`.
* **Two-Level Paging**:
  * Page Directory points to 32–64 Page Tables mapping physical memory.
  * Identity maps all detected physical RAM (capped at 256 MB) so page-table frames allocated from anywhere in RAM can be written through their physical addresses.
  * Page 0 is left unmapped so any zero-pointer dereference immediately causes a hardware Page Fault (`#PF`).
* **Kernel Heap**:
  * Base virtual address `0xD0000000`, expands page-by-page up to 64 MB.
  * 24-byte headers (`magic`, `size`, `is_free`, `padding`, `next`, `prev`), guaranteed 8-byte payload alignment, bi-directional coalescing on `kfree()`.

---

## 6. GUI Foundation Architecture (Milestone 4)

```text
+-----------------------------------------------------------------------------------+
|                            Display & Input Subsystem                              |
+-----------------------------------------------------------------------------------+
|  Hardware Inputs:                                                                 |
|    - PS/2 Keyboard: IRQ 1 (Port 0x60/0x64), Scancode Set 1, Circular Buffer       |
|    - PS/2 Mouse:    IRQ 12 (Aux Port 0x64/0x60), 3-Byte Packet Assembly,          |
|                     Event Coalescing, Cascade IRQ 2 Unmasked                      |
+-----------------------------------------------------------------------------------+
|  Display Layer (1024x768x32 Linear Framebuffer):                                  |
|    - Multiboot 1 Mode Request: Flags bit 2, Mode Type 0, 1024x768x32              |
|    - Physical MMIO Base: 0xE0000000 (VirtualBox VRAM), 768 Pages (3 MB)          |
|    - Page Mapping: Mapped with PAGE_PCD (Page Cache Disable) to prevent tearing  |
|    - 8x16 Bitmap Font: 95 public-domain ASCII glyphs (32-126)                     |
|    - Text Console: 128x48 character grid stored in RAM (console_cells[48][128])  |
|      Scrolling reads strictly from system RAM with zero VRAM reads                |
+-----------------------------------------------------------------------------------+
|  2D Graphics Engine & Software Cursor:                                            |
|    - Off-Screen Back Buffer: 3 MB allocated from Heap at 0xD0000000 via kmalloc    |
|    - Primitives: Bresenham lines, thick brush lines, filled/outline rectangles    |
|    - Dirty Rect Flusher: gfx_flush_rect copies only modified bounding boxes        |
|    - Software Cursor: 12x19 arrow sprite rendered directly to VRAM                |
|      Save-under RAM buffer restores background without touching back buffer       |
|      Flushes overlapping cursor automatically hide and restore it                 |
+-----------------------------------------------------------------------------------+
```

### Component Interaction:
1. **ISR Safety**: Neither the timer (IRQ 0) nor mouse (IRQ 12) interrupt handlers perform drawing operations or call `kprintf()`. The timer sets `timer_clock_dirty = true` for the main loop, while the mouse handler enqueues movement deltas into a volatile ring buffer.
2. **Back Buffer Isolation**: The back buffer in system RAM contains only application and canvas pixels, never the cursor.
3. **Save-Under Cursor**: When the cursor moves, the old 12x19 area is restored to VRAM from `save_under`, and the new 12x19 area is saved from the clean back buffer and drawn directly in VRAM. Mouse moves never trigger full-screen flushes.

---

## 7. Window Manager and Widgets Architecture (Milestone 5)

```text
+-----------------------------------------------------------------------------------+
|                        Window Manager & Compositor Layer                          |
+-----------------------------------------------------------------------------------+
|  Window Data Structure (window_t):                                                |
|    - Coordinates: (x, y), Dimensions: (width, height)                             |
|    - Client Area: (client_x, client_y), Size: (client_w, client_h)                |
|    - Canvas Buffer: Isolated RAM allocation (client_w * client_h * 4 bytes)       |
|    - Flags: VISIBLE, FOCUSED, MOVABLE, MINIMIZED                                  |
|    - Widgets List: Doubly linked list of child UI controls                        |
+-----------------------------------------------------------------------------------+
|  Z-Order Compositing Pipeline (Painter's Algorithm):                              |
|    1. Redraw Desktop Background (Solid 0x003A6EA5 or pattern)                    |
|    2. Render Taskbar at Screen Bottom (y = 736..767, Start button, tabs, clock)  |
|    3. Traverse Z-Order List from Bottom to Top (wm_bottom_window -> prev):        |
|       - Draw 2px 3D outer border                                                  |
|       - Draw Title Bar (Active Blue 0x001F487E or Inactive Gray 0x007A8A9E)       |
|       - Draw Window Title Text & Close Button [X]                                 |
|       - Blit window client_buffer into Off-Screen Back Buffer with clipping       |
|    4. Flush Dirty Rectangle to VRAM (gfx_flush_rect) with cursor protection       |
+-----------------------------------------------------------------------------------+
|  Input & Event Dispatch Flow:                                                     |
|    - Mouse Click -> Top-to-Bottom Window Hit Test (wm_top_window -> next)         |
|      * Title Bar: Initiates window drag; clamps (x, y) to desktop bounds          |
|      * Close Button [X]: Destroys window and child widgets, frees client_buffer   |
|      * Client Area: Dispatches mouse click to child widget hierarchy              |
|    - Keyboard Event -> Dispatched to currently focused text box widget            |
+-----------------------------------------------------------------------------------+
|  Widget Framework (widget_t):                                                     |
|    - Button: 3D beveled relief (light top-left, dark bottom-right; inverted click)|
|    - Label: Single-line string rendering with customizable text colors            |
|    - Textbox: Sunken border, caret cursor, keyboard buffer append, backspace      |
+-----------------------------------------------------------------------------------+
```

### Key Architectural Invariants:
1. **Client Memory Isolation**: Applications write only to their window's `client_buffer`. An application never directly modifies the screen back buffer or VRAM. This guarantees that window dragging and overlapping window composition never corrupt application contents.
2. **Opposite Traversal Invariant**: Compositing *always* traverses **Bottom-to-Top** (so foreground windows paint over background windows), while hit-testing *always* traverses **Top-to-Bottom** (so the topmost visible window intercepts mouse clicks).
3. **Desktop Boundaries**: Window dragging enforces strict clamping: a window's title bar cannot be dragged above the top of the screen (`y >= 0`) or below the taskbar (`y <= 736 - title_bar_height`), preventing orphaned or unreachable windows.

---

## 8. Desktop Productivity Applications Architecture (Milestone 6)

```text
+-----------------------------------------------------------------------------------+
|                        Desktop Productivity Applications Layer                    |
+-----------------------------------------------------------------------------------+
|  Launcher & Environment Orchestration (apps/launcher.c):                          |
|    - Desktop Icons: 6 stylized 2D shortcut glyphs on background (x = 24)          |
|    - Start Menu: 3D beveled popup (220x218) above taskbar with "MyOS" banner      |
|    - Single-Instance Management: Open app or bring existing window to front      |
|    - Clean Console Handoff: Controlled exit back to framebuffer console           |
+-----------------------------------------------------------------------------------+
|  Productivity Applications Suite:                                                 |
|                                                                                   |
|  1. Calculator (apps/calc.c):                                                     |
|     - 4x5 button matrix & LCD display readout box                                 |
|     - 32-bit integer arithmetic state machine (+, -, *, /, C, CE, +/-, =)         |
|     - Native x86 idiv instruction; divide-by-zero & overflow protection           |
|     - Dual mouse click & keyboard numeric key dispatch                            |
|                                                                                   |
|  2. Text Editor (apps/editor.c):                                                  |
|     - Multi-line document buffer (64 lines x 60 columns)                          |
|     - Dynamic caret rendering and cursor row/col tracking                         |
|     - Enter newline splitting & Backspace line concatenation                      |
|     - Real-time character/line metrics status bar & sample document loader        |
|                                                                                   |
|  3. Mini Spreadsheet (apps/sheet.c):                                              |
|     - 2D grid matrix of 60 cells (6 columns A-F x 10 rows 1-10)                   |
|     - Formula bar showing active cell reference (e.g. B3) & editable formula text |
|     - Two-pass formula evaluation engine: =SUM, =AVG, =MIN, =MAX, cell math       |
|     - Automatic dependency recalculation on cell modification                     |
|                                                                                   |
|  4. Real-Time Clock & Calendar (apps/clock.c):                                    |
|     - Digital time readout (HH:MM:SS) updated continuously on PIT timer ticks     |
|     - System uptime counter in seconds                                            |
|     - Sakamoto's O(1) day-of-week calendar with month navigation & day highlight  |
+-----------------------------------------------------------------------------------+
```

### Cooperative In-Kernel Application Model:
In Milestone 6, productivity applications run as cooperative in-kernel tasks in Ring 0. Each application allocates a dedicated window and client buffer, registering event hooks:
- `on_key`: Directs keyboard character input to the active application (number keys in Calculator, text editing in Editor, formula typing in Spreadsheet).
- `on_click`: Dispatches mouse clicks inside the client canvas (cell selection in Spreadsheet, text cursor positioning in Editor).
- `on_close`: Safely tears down application resources, clears window pointers, and informs the Z-order compositor.

---

## 9. Layer 7 — Processes & Preemptive Scheduler Architecture

Milestone 7 transitions the operating system from a single-threaded kernel loop into a preemptive multitasking system.

```text
+-----------------------------------------------------------------------------------+
|                        Processes & Preemptive Scheduler Layer                     |
+-----------------------------------------------------------------------------------+
|  Synchronization Primitives (arch/lock.h):                                        |
|    - irq_save(): Atomically pushes EFLAGS, clears IF (CLI), returns saved flags   |
|    - irq_restore(flags): Restores original EFLAGS (restoring IF state)            |
|    - Re-entrant mutual exclusion for scheduler queues, allocators, and console   |
+-----------------------------------------------------------------------------------+
|  Task Management (proc/task.c, include/proc/task.h):                              |
|    - Task Control Block (TCB): PID, state, saved ESP, ticks_run, time_slice       |
|    - Dedicated 8 KB Stacks: Allocated from heap, 16-byte aligned                  |
|    - Initial Forged Stack: Pops registers and 'ret' jumps to task_entry_trampoline|
|    - System Tasks: PID 0 (Idle hlt task), PID 1 (Main/Desktop), PID 2 (Monitor)   |
|    - Dynamic Tasks: Created on demand via task_create(name, entry, arg)           |
+-----------------------------------------------------------------------------------+
|  Assembly Context Switch & Trampoline (proc/switch.asm):                          |
|    - switch_context(&old_esp, new_esp): Saves/restores eflags, ebp, edi, esi, ebx|
|    - Atomic stack swap: mov [old_esp], esp; mov esp, [new_esp]                    |
|    - task_entry_trampoline: Executes 'sti', invokes entry(arg), calls task_exit() |
+-----------------------------------------------------------------------------------+
|  Preemptive Round-Robin Scheduler (proc/scheduler.c, include/proc/scheduler.h):   |
|    - FIFO Ready Queue: Runnable tasks executed round-robin                        |
|    - Time Slice: 10 PIT timer ticks (100 ms at 100 Hz frequency)                  |
|    - Timer Preemption: PIT IRQ0 decrements slice and invokes schedule()           |
|    - Early PIC EOI: pic_send_eoi(irq) issued BEFORE switch_context                |
|    - Timed Sleep Queue: task_sleep_ms(ms) suspended until target tick arrives     |
|    - Automatic Zombie Reaper: Frees 8 KB stack and TCB when task finishes         |
|    - Inspection: scheduler_dump_tasks() outputs full ps table to serial / screen  |
+-----------------------------------------------------------------------------------+
```

### Multitasking State Machine:
```text
  task_create()
       |
       v
   [ READY ] <--------- (Time slice expired / task_yield) ---------+
       |                                                           |
       | pick_next() & switch_context()                            |
       v                                                           |
  [ RUNNING ] -----------------------------------------------------+
       |                                                           ^
       | task_sleep_ms(ms)                                         | Timer ticks >= wake_tick
       v                                                           |
  [ SLEEPING ] ----------------------------------------------------+
       |
       | Task returns / task_exit()
       v
   [ ZOMBIE ] ---> scheduler_cleanup_zombies() ---> [ Memory Freed ]
```






