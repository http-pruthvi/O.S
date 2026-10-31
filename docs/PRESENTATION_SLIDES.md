# B.Tech Final Year Project Defense Presentation Deck
## Project: Design and Implementation of a 32-bit x86 Protected Mode Operating System (MyOS)

**Candidate**: Pruthviraj Vinod Phuse  
**Degree**: Bachelor of Technology in Computer Engineering  
**Slides Count**: 15 Slides with Speaker Notes  

---

### Slide 1: Title Slide
- **Title**: MyOS: A 32-Bit x86 Protected Mode Operating System
- **Subtitle**: Monolithic Kernel Design with Preemptive Multitasking and Graphical Desktop Environment
- **Candidate**: Pruthviraj Vinod Phuse
- **Department**: Department of Computer Engineering
- **Repository**: [https://github.com/http-pruthvi/O.S](https://github.com/http-pruthvi/O.S)
- *Speaker Note*: "Respected panel members, I present MyOS, an x86 operating system built from scratch without standard libraries."

---

### Slide 2: Problem Statement & Motivation
- **Abstract Problem**: Commercial operating systems abstract hardware interactions behind millions of lines of code.
- **Core Objectives**:
  - Boot bare-metal x86 hardware using Multiboot 1 protocol.
  - Implement protected mode CPU tables (GDT, IDT, PIC remap).
  - Construct physical and virtual memory management (PMM, two-level paging, dynamic heap).
  - Develop a software-rendered graphical desktop (1024x768x32 linear framebuffer) with productivity mini-apps.
  - Build a preemptive round-robin scheduler with independent 8 KB stacks and assembly context switching.
- *Speaker Note*: "The project aims to construct every layer of a modern OS kernel from first principles."

---

### Slide 3: Overall System Architecture (8 Layers)
- **Layer 0**: Bootloader & Multiboot 1 Handshake (`boot.asm`, `linker.ld`)
- **Layer 1**: CPU Architecture (`gdt.c`, `idt.c`, `isr.c`, `pic.c`)
- **Layer 2**: Device Drivers (UART 16550 Serial, 8254 PIT, PS/2 Keyboard & Mouse)
- **Layer 3**: Memory Management (Bitmap PMM, Two-Level Paging, Dynamic Heap)
- **Layer 4**: GUI Foundation (Linear Framebuffer, 8x16 Font, 2D Primitives, Software Cursor)
- **Layer 5**: Window Manager & Widgets (Z-order Compositor, Taskbar, Start Menu, Buttons, Text Boxes)
- **Layer 6**: Productivity Applications Suite (Calculator, Text Editor, Spreadsheet, Clock)
- **Layer 7**: Preemptive Multitasking & Scheduler (`task.c`, `scheduler.c`, `switch.asm`)
- *Speaker Note*: "Our architecture separates hardware abstraction from memory, graphical composition, and scheduling."

---

### Slide 4: Boot & CPU Layer
- **Multiboot 1 Handshake**:
  - Magic `0x1BADB002`, Flags `0x00000007` (requests VBE linear framebuffer).
  - Kernel loaded at physical 1 MB (`0x00100000`) to protect legacy BIOS/IVT memory.
- **Segmentation (GDT)**: Flat 4 GB model (Code 0x08, Data 0x10) with pipeline far jump flush.
- **Interrupts (IDT & PIC)**:
  - 256 gates using Interrupt Gate attribute (`0x8E`) to clear `IF = 0` automatically.
  - 8259 PIC remapped (Master 32–39, Slave 40–47) to eliminate CPU exception collisions.
  - Spurious IRQ 7 and IRQ 15 hardware filters.
- *Speaker Note*: "By remapping the PIC, we ensure hardware interrupts never collide with x86 CPU exceptions."

---

### Slide 5: Memory Management: PMM & Two-Level Paging
- **Physical Memory Manager (PMM)**:
  - 128 KB static bitmap tracking 32,752 frames across 127 MB usable RAM.
  - 32-bit fast scan helper with $O(1)$ best-case allocation and $O(1)$ deallocation.
- **Two-Level 32-bit Paging**:
  - 4 KB Page Directory and Page Tables identity-mapping 128 MB physical RAM.
  - Linear framebuffer (3 MB at `0xE0000000`) mapped with Page Cache Disable (`PCD`).
  - **NULL Pointer Trap**: Page 0 (`0x0`–`0x1000`) marked NOT PRESENT; catches NULL dereferences via `#PF`.
- *Speaker Note*: "Leaving Page 0 unmapped turns accidental NULL pointer dereferences into immediate hardware page faults."

---

### Slide 6: Dynamic Kernel Heap Allocator (`kmalloc` / `kfree`)
- **Location**: Virtual address `0xD0000000` (expandable up to 128 MB).
- **Structure**: Circular doubly-linked list with 24-byte boundary tag headers (`magic 0xDEADBEEF`).
- **Mechanisms**:
  - **First-Fit Search**: Allocates requested 8-byte aligned size.
  - **Block Splitting**: Splits oversized free blocks ($\ge \text{header} + 8$).
  - **Demand Expansion**: Calls `heap_expand` to map fresh PMM physical frames and invalidates TLB via `invlpg`.
  - **Bidirectional Coalescing**: Automatically merges adjacent free blocks on `kfree`.
- *Speaker Note*: "Bidirectional coalescing merges both forward and backward, eliminating external fragmentation."

---

### Slide 7: Graphical Subsystem & 2D Engine
- **Display Mode**: Linear Framebuffer 1024x768x32 direct RGB (4096 bytes pitch).
- **Double Buffering**:
  - 3 MB back buffer in system RAM (`kmalloc`).
  - All drawing primitives (Bresenham lines, rectangles, fills, text) render into back buffer.
  - `gfx_flush_rect()` copies modified dirty rectangles to VRAM via `rep movsd`, eliminating visual flicker.
- **Monospace Font Engine**: Monospace 8x16 font table covering printable ASCII 32–126.
- **Framebuffer Console**: 128x48 terminal emulator with cell array, scrolling, and direct coordinate printing.
- *Speaker Note*: "Drawing exclusively to the back buffer eliminates tearing and maximizes rendering throughput."

---

### Slide 8: Software Mouse Cursor with Save-Under Buffering
- **Challenge**: Standard VBE linear framebuffers lack hardware cursor support.
- **Implementation**:
  - 12x19 arrow cursor sprite rendered with magenta color-key transparency.
  - **Save-Under Buffer**: Backs up pixels beneath the cursor before rendering.
  - **Restoration**: Restores background pixels prior to cursor movement and dirty rectangle flushes.
  - **Strict Back-Buffer Isolation**: Cursor pixels are never stored in the back buffer, preventing ghost trails during window drags.
- *Speaker Note*: "By keeping the cursor strictly out of the back buffer, window dragging never stamps cursor artifacts."

---

### Slide 9: Window Manager & Z-Order Compositor
- **Window Data Structure**: Position (`x, y`), dimensions (`w, h`), flags, title, and client canvas buffer.
- **Z-Order Compositor**:
  - Linked list of windows ordered from bottom-most to top-most.
  - Renders desktop wallpaper, window dropshadows, title bars, and client contents.
  - Clicking any window elevates it to the top of the Z-stack.
- **Title Bar Dragging**:
  - Mouse movement translates window coordinates with screen boundary clamping.
  - Flushes union dirty bounding box covering old and new window positions.
- **Taskbar & Start Menu**: Bottom-pinned 40px bar with Start button, window tabs, and live clock.
- *Speaker Note*: "Clamping title bar coordinates ensures windows can never be dragged off-screen."

---

### Slide 10: In-Kernel Productivity Applications Suite
- **1. Desktop Calculator**:
  - 4x5 button matrix, LCD display, 32-bit signed integer state machine.
  - Single-instruction native `idiv` with divide-by-zero protection. Dual mouse and keyboard typing.
- **2. Multi-Line Text Editor (Notepad)**:
  - In-memory 64x60 character document buffer.
  - Dynamic caret rendering, Enter line insertion, Backspace line concatenation, and status bar.
- **3. Mini Spreadsheet**:
  - 60-cell matrix (A–F, 1–10) with editable formula bar.
  - Two-pass recursive formula evaluator supporting `=SUM`, `=AVG`, `=MIN`, `=MAX`, and cell arithmetic.
- **4. Real-Time Clock & Calendar**:
  - HH:MM:SS digital display and monthly calendar powered by Sakamoto's $O(1)$ day-of-week formula.
- *Speaker Note*: "All applications run inside the kernel with native window integration and event dispatching."

---

### Slide 11: Processes & Preemptive Multitasking
- **Task Control Block (`task_t`)**: Tracks PID, state (`READY`, `RUNNING`, `SLEEPING`, `ZOMBIE`), stack, runtime ticks.
- **Independent 8 KB Stacks**: Dedicated kernel stacks allocated from heap, aligned to 16 bytes for C ABI.
- **Assembly Context Switch (`switch_context`)**:
  - Pushes `EFLAGS` and callee-saved registers (`ebp, edi, esi, ebx`).
  - Swaps stack pointers: `mov [old_esp], esp; mov esp, [new_esp]`.
  - Pops registers and executes `ret` into the new task's execution context.
- **Stack Forging & Trampoline**:
  - New tasks forged with initial register state and return address pointing to `task_entry_trampoline`.
  - Trampoline executes `sti`, calls `entry(arg)`, and automatically calls `task_exit()` upon completion.
- *Speaker Note*: "Only callee-saved registers are preserved because the C calling convention guarantees the caller saves volatile registers."

---

### Slide 12: Preemption, Timed Sleeping & Zombie Reaping
- **Round-Robin Scheduling**:
  - 100 Hz PIT timer interrupt decrements active quantum (10 ticks = 100 ms).
  - Triggers `schedule()` when quantum expires if runnable tasks are queued.
- **Early PIC EOI Handshake**:
  - `pic_send_eoi(0)` issued *before* calling driver callbacks.
  - Prevents PIC from locking future hardware interrupts across context switches.
- **Timed Sleeping (`task_sleep_ms`)**:
  - Suspends tasks onto sleeping queue until `pit_ticks >= wake_tick`.
- **Zombie Stack Reaper**:
  - Terminated tasks mark `TASK_ZOMBIE` and yield.
  - A running task unlinks the zombie via `task_unlink()` and deallocates its stack and TCB safely.
- *Speaker Note*: "Early EOI ensures that preempting during an ISR never leaves the PIC locked for other tasks."

---

### Slide 13: Empirical Verification & Test Hooks (`[F1]`–`[F12]`)
- **CPU & Exceptions**: `[F1]` Divide-by-zero panic, `[F2]` Software interrupt `int 0x80`.
- **Memory Subsystem**: `[F3]` Live PMM/Heap stats, `[F4]` 20-block heap stress test, `[F5]` PMM frame test.
- **Paging Protection**: `[F6]` Page 0 NULL read trap, `[F7]` Unmapped write trap, `[F8]` 200 KB heap expansion test.
- **Graphics & GUI**: `[F9]` Mouse paint demo, `[F10]` Desktop Window Manager environment.
- **Multitasking**:
  - `[F11]` Dynamically spawns concurrent worker tasks computing with timed sleeps.
  - `[F12]` Dumps live formatted `ps` process table to COM1 serial and screen.
- *Speaker Note*: "Every architectural layer is backed by a dedicated, repeatable interactive test hook."

---

### Slide 14: Challenges Overcome & Technical Insights
- **Freestanding 64-bit Division**:
  - Replaced 64-bit integer division with 32-bit signed integers to eliminate GCC's `__divdi3` runtime dependency.
- **Preemptive EOI Ordering**:
  - Discovered that late EOI blocks future hardware timer ticks across context switches; solved via early PIC EOI.
- **Zombie Reaper Head Unlinking**:
  - Prevented double-free errors by implementing `task_unlink()` to safely unlink tasks from the head of the all-tasks list.
- **Cursor Save-Under Clipping**:
  - Clamped cursor bounding boxes against right and bottom screen edges to prevent buffer overflow.
- *Speaker Note*: "Diagnosing low-level bugs required analyzing register dumps, PIC state registers, and serial logs."

---

### Slide 15: Conclusion & Future Scope
- **Project Deliverables**:
  - Functional 8-layer x86 operating system kernel with GUI and preemptive multitasking.
  - Comprehensive thesis documentation, 40-question viva defense guide, and demo video script.
  - Clean GitHub repository: [https://github.com/http-pruthvi/O.S](https://github.com/http-pruthvi/O.S).
- **Future Enhancements**:
  - User Mode (`Ring 3`) with Hardware TSS task switching.
  - POSIX System Call Interface (`int 0x80` / `sysenter`).
  - Virtual File System (VFS) with RAM-disk (FAT/TAR) persistent storage.
  - 64-bit Long Mode port.
- *Speaker Note*: "Thank you for your time. I welcome questions from the examination committee."
