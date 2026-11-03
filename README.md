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
│   ├── milestone-4.md      # Milestone 4 report and viva Q&A
│   ├── milestone-5.md      # Milestone 5 report and viva Q&A
│   ├── milestone-6.md      # Milestone 6 report and viva Q&A
│   ├── milestone-7.md      # Milestone 7 report and viva Q&A
│   ├── milestone-9.md      # Milestone 9 VFS, RamFS, Task Manager, File Explorer, Terminal & Win11 UI
│   ├── security_audit_report.html # Interactive Playwright 5-Suite Security Verification Dashboard
│   ├── FINAL_PROJECT_REPORT.md    # Comprehensive B.Tech Thesis Project Report
│   ├── VIVA_PREPARATION_GUIDE.md  # 40-Question Master Viva Defense Guide
│   ├── DEMO_VIDEO_SCRIPT.md       # 7-Minute Screen Recording Video Storyboard
│   └── PRESENTATION_SLIDES.md     # 15-Slide Final Project Defense Presentation Deck
├── include/                # Header files
│   ├── proc/               # Processes & Scheduler (Milestone 7)
│   │   ├── task.h          # Task Control Block (TCB), states & task API
│   │   └── scheduler.h     # Preemptive round-robin scheduler & queues
│   ├── apps/               # Desktop Productivity Applications (Milestone 6)
│   │   ├── friday.h        # Friday Multilingual AI Assistant subsystem & animated orb
│   │   ├── launcher.h      # Desktop launcher, shortcut icons & Start menu
│   │   ├── calc.h          # Scientific 5-column desktop calculator (arithmetic, sqrt, x^2, 1/x, %)
│   │   ├── editor.h        # Multi-line text editor with VFS save/load
│   │   ├── sheet.h         # Mini spreadsheet with formula engine
│   │   ├── clock.h         # Live digital clock & monthly calendar
│   │   ├── control.h       # Control Panel with wallpaper theme selector & security monitor
│   │   ├── minesweeper.h   # Classic Windows Minesweeper (9x9, 10 mines, digital LED readouts)
│   │   ├── snake.h         # Retro Snake Arcade game with score tracking
│   │   └── store.h         # Web App Store & Package Manager (1-click web install to RamFS)
│   ├── arch/               # Architecture & hardware abstraction
│   │   ├── lock.h          # Re-entrant interrupt masking (irq_save, irq_restore)
│   │   ├── gdt.h           # Global Descriptor Table
│   │   ├── idt.h           # Interrupt Descriptor Table
│   │   ├── isr.h           # CPU Exception handlers & registers_t frame
│   │   ├── pic.h           # 8259A PIC initialization & EOI
│   │   └── irq.h           # Hardware IRQ dispatch table
│   ├── drivers/
│   │   ├── serial.h        # COM1 UART 16550 serial driver
│   │   ├── serial2.h       # COM2 UART interrupt-driven driver (0x2F8, IRQ 3, ring buffer)
│   │   ├── vga.h           # VGA output driver (console routing)
│   │   ├── timer.h         # PIT 8254 timer driver (100 Hz, uptime)
│   │   ├── keyboard.h      # PS/2 keyboard driver (Scancode Set 1)
│   │   └── mouse.h         # PS/2 mouse driver (IRQ 12, packet parser)
│   ├── fs/                 # Storage & Virtual File System (Milestone 9)
│   │   ├── vfs.h           # VFS node structures, file descriptor table & API
│   │   └── ramfs.h         # In-memory RAM filesystem driver interface
│   ├── gui/                # Window Manager & Widgets (Milestone 5 & Riced Suite)
│   │   ├── window.h        # Window structures, title bars & client canvas
│   │   ├── wm.h            # Z-order compositor, taskbar & dragging
│   │   ├── widget.h        # Buttons, labels & single-line text boxes
│   │   ├── desktop.h       # Desktop environment & hotkey dispatcher
│   │   ├── theme.h         # Dynamic 6-palette theme engine (Windows 11 Fluent Dark default)
│   │   ├── waybar.h        # Top Waybar status bar with workspace pills & telemetry chips
│   │   ├── dock.h          # Centered floating pill dock with active pips
│   │   ├── hud.h           # Conky desktop telemetry & 2x typographic clock HUD cards
│   │   └── lockscreen.h    # Hyprlock alpha-dimmed modal lock screen & PIN authentication
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
    ├── apps/
    │   ├── friday.c        # Friday AI Assistant command dispatcher, PC speaker chimes, animated orb
    │   ├── launcher.c      # Desktop shortcut icons & Start menu popup
    │   ├── calc.c          # 5-column Scientific Calculator & 32-bit arithmetic
    │   ├── editor.c        # Multi-line text buffer, caret, status bar & VFS save/load
    │   ├── sheet.c         # 6x10 spreadsheet, formula parser (=SUM, =AVG)
    │   ├── clock.c         # Digital clock, uptime & Sakamoto calendar
    │   ├── explorer.c      # Graphical File Explorer (browse, create, delete, open in editor)
    │   ├── taskmgr.c       # Graphical Task Manager (process list, memory stats, task_kill)
    │   ├── terminal.c      # Desktop Terminal CLI Shell (ls, cat, write, rm, ps, kill, etc.)
    │   ├── settings.c      # Modern 4-Tab System Settings (Themes, Audio, Security, Power)
    │   ├── control.c       # Legacy Control Panel entry point forwarded to Settings
    │   ├── photos.c        # Photos / Image Viewer (PPM P3/P6, Grayscale, Invert, 1x-6x Zoom)
    │   ├── player.c        # Media Player & Chiptune Synthesizer (PIT Ch2, 4 tracks, animated EQ)
    │   ├── breakout.c      # Retro 8-bit Breakout Arcade (fixed-point physics, paddle deflection)
    │   ├── minesweeper.c   # Classic Minesweeper (9x9 grid, 10 mines, 7-seg LED, smiley, BFS)
    │   ├── snake.c         # Retro Snake Arcade (20x20 grid, WASD/arrows, food apple, high score)
│   └── store.c         # Web App Store (catalog, package install, custom URLs, web launchers)
    ├── fs/
    │   ├── vfs.c           # Virtual File System abstraction & 32 file descriptors
    │   └── ramfs.c         # In-memory RAM filesystem driver with dynamic reallocation
    ├── proc/
    │   ├── task.c          # Task lifecycle, 8 KB stack forging, idle & boot tasks
    │   ├── scheduler.c     # Preemptive round-robin, sleeping queue & zombie reaper
    │   └── switch.asm      # switch_context assembly routine & trampoline
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
    │   ├── serial2.c       # COM2 driver (0x2F8, IRQ 3, RX ring buffer, TX blocking)
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
    │   ├── window.c        # Windows 11 Fluent window rendering, 1px borders, vector caption buttons [—][□][✕]
    │   ├── wm.c            # Z-order compositor, Windows 11 40px taskbar with 4-pane Start & active pills
    │   ├── widget.c        # Modern flat widgets (1px borders, accent focus rings, dark textboxes)
    │   ├── desktop.c       # Desktop environment & hotkey dispatcher ([F1]-[F12])
    │   ├── theme.c         # Dynamic 6-palette theme engine (Windows 11 Fluent Dark default)
    │   ├── waybar.c        # Top status bar with [MyOS] launcher chip, chips & clock
    │   ├── dock.c          # Centered floating 12-icon pill dock with active pips
    │   ├── hud.c           # Conky desktop HUD cards & scaled typography
    │   ├── lockscreen.c    # Hyprlock alpha-dimmed lock screen & PIN entry
    │   ├── startmenu.c     # Windows Start Menu popup (dynamic taskbar anchoring, app tiles, power)
    │   └── notification.c  # Desktop Toast Notification system with progress bar
    ├── kernel/
    │   └── kernel.c        # kernel_main init order, echo loop & test hooks
    └── lib/
        ├── kprintf.c       # kprintf parser (%c, %s, %d, %u, %x, %p)
        └── string.c        # strlen, memset, memcpy, memcmp, strlcpy, strlcat
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
- **`[F10]`**: Desktop Window Manager & Productivity Apps (launches full graphical windowed desktop environment featuring Start Menu, desktop shortcut icons, and productivity apps, alongside the System Monitor and About dialogs, with title bar dragging clamped to screen bounds and ESC/Shutdown exit)
- **`[F11]`**: Spawn Compute Worker Task (dynamically creates and enqueues a multi-second concurrent background worker task demonstrating time-slice preemption, timed sleep `task_sleep_ms`, and clean zombie stack deallocation upon exit)
- **`[F12]`**: Process & Task Table Dump (prints formatted `ps` process table containing PID, State, cumulative timer ticks, stack size, and task name to COM1 serial and active screen)

### Desktop Environment Hotkeys
Once inside the Desktop Window Manager:
- **`[F1]`**: Toggle Modern Drop-Down Start Menu Popup
- **`[F2]`**: Open Photos & Image Viewer (PPM P3 ASCII / P6 Binary, Zoom, Filters)
- **`[F3]`**: Open Media Player & Chiptune Synthesizer (PC Speaker audio, 4 tunes, spectrum visualizer)
- **`[F4]`**: Open Retro 8-Bit Breakout Arcade Game (fixed-point physics, brick breaking)
- **`[F5]`**: Toggle Conky Desktop Telemetry HUD Cards
- **`[F6]`**: Lock Screen Session (Hyprlock with PIN `1234`)
- **`[F7]`**: Open System Settings & Control Center (4 Tabs: Display, Audio, Security, Power)
- **`[F8]`**: Open Graphical File Explorer (Browse RamFS, Create Note, Delete, Open in Editor)
- **`[F9]`**: Open Desktop Terminal CLI Shell (`ls`, `cat`, `write`, `rm`, `ps`, `kill`, `http_pruthvi command code 0`)
- **`[F10]`**: Open Retro Minesweeper Game (9x9 grid, 10 mines, 7-segment LED, smiley)
- **`[F11]`**: Spawn Concurrent Background Worker Task
- **`[F12]`**: Open Graphical Task Manager (process list, memory stats, task kill)
- **`[ESC]`**: Clean Exit from Desktop back to Framebuffer Console

---

## Friday AI Multilingual Voice Assistant & Host Coprocessor Bridge

Friday OS features an embedded AI assistant subsystem named **"Friday"** that provides multilingual voice and text control over the operating system.

### Architecture Overview
- **Host Bridge Daemon**: Runs on Windows host (`scripts/friday_client.py`), capturing natural language voice/text from microphone or keyboard, mapping diverse queries into compact protocol packets.
- **COM2 Hardware Transport**: VirtualBox COM2 UART (`0x2F8`, IRQ 3) acts as a high-speed virtual serial pipeline exposed via TCP socket server on `localhost:2345`.
- **Interrupt-Driven Kernel Driver**: `src/drivers/serial2.c` buffers incoming packets in a 256-byte circular ring buffer via IRQ 3.
- **Kernel Dispatcher & Animated AI Orb**: `src/apps/friday.c` parses packets, executes OS actions (app launches, theme switching, security locking, RAM queries), produces PC speaker confirmation chimes, shows toast notifications, and renders an animated glowing AI orb on the desktop back buffer.

### Multilingual Natural Language Support
Friday recognizes commands across three languages:
1. **Marathi (मराठी)**:
   - *"कॅल्क्युलेटर उघड"* -> Launches Scientific Calculator
   - *"गाणी लाव"* -> Plays Chiptune Media Player
   - *"फाइल्स दाखव"* -> Opens Graphical File Explorer
   - *"रॅम किती आहे"* -> Queries physical memory manager
   - *"स्क्रीन लॉक कर"* -> Activates Hyprlock screen lock
   - *"सायबरपंक थीम लाव"* -> Switches to Cyberpunk Neon theme
2. **Hinglish**:
   - *"terminal open kar"* -> Launches Terminal CLI
   - *"cyberpunk theme lagao"* -> Switches to Cyberpunk theme
   - *"breakout game lagao"* -> Launches Breakout arcade
   - *"music bajao"* -> Starts audio synthesizer
   - *"RAM kitni hai"* -> Telemetry query
3. **English**:
   - *"open calculator"*, *"open terminal"*, *"play breakout"*, *"lock screen"*, *"how much ram"*

### Emergency Remote Self-Destruct
- Initiated via: `http_pruthvi command code 0` or voice command *"सर्व हटवा"* / *"self destruct"*.
- Wipes volatile RAM structures, alerts user, and terminates cleanly.

---

## Windows 11 Fluent Dark UI & Automated Playwright Security Audit

### Windows 11 Fluent Dark Desktop
MyOS features a modern Windows 11 Fluent Dark aesthetic designed for high legibility, clean geometry, and snappy desktop interaction:
- **Crisp 1px Geometry**: Thin 1px outer window borders and modern flat controls replace legacy 3D beveled borders.
- **Vector Caption Buttons**: Title bars feature native Windows 11 vector glyphs — horizontal minimize bar `[—]`, square maximize outline `[□]`, and diagonal close cross `[✕]` with red hover highlight.
- **Windows 11 Taskbar**: 40px bottom bar with the signature 4-pane blue Start logo, running application tabs with accent blue indicator pills, Friday AI tray chip, and digital clock.
- **Rising Start Menu**: Bottom-anchored popup menu featuring user profile header, search bar, 10 application tiles, and power actions.
- **Dark Fluent Canvas**: Deep navy desktop (`#0F172A`), dark Mica taskbar (`#1C1C1C`), and charcoal client windows (`#1F1F1F`).

### Automated Playwright Security Audit Suite
A comprehensive 5-suite automated security testing pipeline (`scripts/security_audit_playwright.py`) verifies kernel resilience and isolation:
1. **Hyprlock Screen Lock PIN Enforcement**: PIN-authenticated modal guard against unauthorized session access.
2. **Emergency Self-Destruct Token Auth**: Strict token verification (`TOKEN_AUTH_OK`) preventing unauthenticated zeroization.
3. **COM2 UART Buffer Overflow & Fuzzing**: Validates that 4 KB random fuzz payloads and 1000-packet bursts to VirtualBox COM2 (TCP 2345) are safely handled by the circular ring buffer without buffer overrun.
4. **Memory Privilege & Page Fault Trapping**: Verifies MMU fault vectors (`#PF`) and null pointer protection (`0x00000000`).
5. **VFS RamFS Path Traversal Prevention**: Confirms boundary enforcement and invalid path rejection across the virtual file system.

The test suite generates an interactive glassmorphic report dashboard (`docs/security_audit_report.html`) and executes automated browser assertions via Playwright with Microsoft Edge (`channel="msedge"`), validating a 100% test pass rate.

### Web App Store & Package Manager
MyOS features an integrated **Web App Store** (`src/apps/store.c`, `include/apps/store.h`) allowing users to download and install applications from the web repository or custom URLs directly into the in-memory RamFS:
- **Verified Web Catalog**: Curated suite of 6 installable applications:
  - **Web Browser Lite**: HTTP/HTML web navigation client with bookmarks & URL viewport.
  - **Hex Editor Pro**: Interactive binary hexadecimal inspector and memory viewer.
  - **Markdown Notes**: Distraction-free markdown documentation reader.
  - **IRC Chat Client**: Lightweight IRC messaging client for local or network rooms.
  - **System Benchmark**: Real-time CPU arithmetic, memory bandwidth, and FPS diagnostics.
  - **Mandelbrot Explorer**: Multi-color fractal mathematical generator.
- **1-Click Web Installation**: Generates package manifests (`/<id>.app`) and executable stubs (`/<id>`) in RamFS, plays an audio chime, pushes a success toast notification, and dynamically registers the app into the Start Menu and Search engine.
- **Taskbar Pinned Apps Bar**: 7 essential apps pinned directly on the Start Bar (Files, Terminal, Editor, Calc, Store, Settings, Task Manager) for instant access.
- **Live Search Flyout**: Real-time keystroke filtering across all built-in and web applications with Enter to launch.
