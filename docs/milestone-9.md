# Milestone 9: Virtual File System (VFS), In-Memory RamFS & Desktop System Applications

## 1. Goal and What Was Built

Milestone 9 transitions MyOS into a complete, storage-aware graphical desktop operating system. It introduces a modular Virtual File System (VFS) abstraction, an in-memory mutable filesystem (RamFS), and a suite of core system applications running inside the window manager:

1. **Virtual File System (VFS)**: A clean POSIX-like abstraction layer providing uniform file operations (`vfs_open`, `vfs_read`, `vfs_write`, `vfs_close`, `vfs_readdir`, `vfs_finddir`, `vfs_create`, `vfs_delete`) and decoupled from specific filesystem storage backends.
2. **In-Memory RAM Filesystem (RamFS)**: A dynamic, read-write filesystem residing in the kernel heap. Pre-seeds standard documents (`/welcome.txt`, `/readme.txt`, `/system.nfo`, `/todo.txt`) on startup and supports dynamic file creation, expansion, and deletion.
3. **Graphical File Explorer**: A desktop file manager displaying directories, files, icons, sizes, and permissions in real time. Features single-click file selection, creation of new note files, file deletion, and direct opening of text documents into the Notepad editor.
4. **Graphical Task Manager**: A process and performance monitor querying the round-robin scheduler. Displays a live process table (PID, Task Name, State, Runtime Ticks, Time Slice), physical memory and heap metrics, task refresh, worker task spawning, and interactive process termination (`task_kill`).
5. **Desktop Terminal Emulator**: An interactive windowed command-line shell with an 80x25 character matrix, ANSI-style green prompt (`myos:/$`), backspace editing, vertical line scrolling, and built-in shell commands (`help`, `clear`, `ls`, `cat`, `write`, `rm`, `ps`, `kill`, `mem`, `uptime`, `echo`, `calc`, `editor`, `files`, `top`).
6. **Text Editor VFS Integration**: Connects the multi-line Notepad editor to the VFS with "Save" and "Open" functionality.

---

## 2. Architecture & How It Works

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Desktop User Interface                            │
│  ┌──────────────────┐  ┌──────────────────┐  ┌───────────────────────────┐  │
│  │  File Explorer   │  │   Task Manager   │  │    Terminal CLI Shell     │  │
│  │ (Browsing, Open) │  │ (ps, kill, stats)│  │ (ls, cat, write, rm, ps)  │  │
│  └────────┬─────────┘  └────────┬─────────┘  └─────────────┬─────────────┘  │
│           │                     │                          │                │
│           │                     ▼                          │                │
│           │            ┌──────────────────┐                │                │
│           │            │ Kernel Scheduler │                │                │
│           │            │ (task_kill, TCBs)│                │                │
│           │            └──────────────────┘                │                │
│           ▼                                                ▼                │
├─────────────────────────────────────────────────────────────────────────────┤
│                     Virtual File System (VFS) Layer                         │
│  - File Descriptor Table (32 active open handles)                           │
│  - vfs_lookup(), vfs_open(), vfs_read(), vfs_write(), vfs_close()           │
├─────────────────────────────────────────────────────────────────────────────┤
│                     In-Memory RAMFS Storage Driver                          │
│  - Dynamic Node Hierarchy: / (root)                                         │
│    ├── welcome.txt   ├── readme.txt   ├── system.nfo   ├── todo.txt         │
│  - Dynamic Heap Buffer Reallocation on File Write                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                     Kernel Memory & Hardware Subsystems                     │
│  - Physical Memory Manager (PMM Bitmap) & Two-Level Paging (4 KB Pages)     │
│  - Kernel Dynamic Heap (kmalloc / kfree)                                    │
│  - Preemptive Round-Robin Multitasking (100 Hz Timer Quantum)               │
└─────────────────────────────────────────────────────────────────────────────┘
```

### VFS Operation Dispatch Flow

When any application requests a file operation (e.g. `cat welcome.txt` or File Explorer opening a file):

1. **Path Resolution (`vfs_lookup`)**: Breaks down the path into tokens (`/` followed by `welcome.txt`), calling `finddir()` on directory nodes until reaching the target `vfs_node_t`.
2. **Handle Allocation (`vfs_open`)**: Allocates an available file descriptor in the system `vfs_file_t` table, increments node reference count, and initializes seek offset to 0.
3. **Data Transfer (`vfs_read` / `vfs_write`)**: Delegates to the underlying filesystem driver's `read()` or `write()` function pointer, advancing the file offset accordingly.
4. **Closing (`vfs_close`)**: Decrements reference count, calls driver `close()`, and marks descriptor slot free.

---

## 3. Key Concepts Explained Simply

### What is a Virtual File System (VFS)?
In Unix and modern operating systems, the VFS is an architectural abstraction that provides standard system calls (`open`, `read`, `write`, `close`) regardless of the underlying hardware or storage format. The application interacts solely with `vfs_node_t` pointers, allowing RamFS, FAT, ext2, or network filesystems to be swapped without changing user applications.

### In-Memory RamFS
A RAM disk treats a portion of system memory as a storage volume. Because it runs purely in RAM, read and write operations are instantaneous and require no physical disk controller commands. File data buffers dynamically resize using `kmalloc` and `memcpy` as text is appended.

### Process Termination (`task_kill`)
Terminating a task requires careful synchronization:
- Protected system tasks (PID 0 Idle Task and PID 1 Kernel Boot Task) can never be terminated.
- If a target task is currently in the `TASK_READY` queue, it is removed from the FIFO queue via `scheduler_remove_ready()`.
- Its state is transitioned to `TASK_ZOMBIE`. The scheduler's zombie reaper safely deallocates the task's 8 KB stack and TCB once context switches to another active thread.

---

## 4. Files Created and Changed

| File | Status | Description |
|---|---|---|
| `include/fs/vfs.h` | **Created** | VFS node structure, directory entry definitions, file descriptor table API |
| `src/fs/vfs.c` | **Created** | Core VFS implementation: path resolution, open file table, read/write dispatch |
| `include/fs/ramfs.h` | **Created** | In-memory RAM filesystem definitions and pre-seed helper |
| `src/fs/ramfs.c` | **Created** | RamFS driver implementing node read, write, create, unlink, readdir, finddir |
| `include/apps/taskmgr.h` | **Created** | Task Manager window interface prototype |
| `src/apps/taskmgr.c` | **Created** | Task Manager GUI: process table, memory usage metrics, End Task button |
| `include/apps/explorer.h` | **Created** | File Explorer window interface prototype |
| `src/apps/explorer.c` | **Created** | File Explorer GUI: file list, size/permission indicators, New File & Delete |
| `include/apps/terminal.h` | **Created** | Terminal emulator interface prototype |
| `src/apps/terminal.c` | **Created** | Terminal GUI: character grid, command parser (`ls`, `cat`, `write`, `ps`, `kill`) |
| `include/apps/editor.h` | **Updated** | Added `app_editor_open_file()` prototype |
| `src/apps/editor.c` | **Updated** | Wired "Save" and "Open" actions directly to VFS |
| `include/apps/launcher.h` | **Updated** | Expanded Start Menu dimensions and added launcher shortcuts |
| `src/apps/launcher.c` | **Updated** | Added desktop icons and Start Menu entries for Explorer, TaskMgr, and Terminal |
| `include/proc/task.h` | **Updated** | Added `task_by_id()` and `task_kill()` prototypes |
| `src/proc/task.c` | **Updated** | Implemented `task_by_id()` and `task_kill()` with system task protection |
| `include/lib/string.h` | **Updated** | Added `strcat`, `strncat`, `strncmp`, `strlcpy`, `strlcat` bounded string prototypes |
| `src/lib/string.c` | **Updated** | Implemented `strcat`, `strncat`, `strncmp`, and OpenBSD-style `strlcpy`/`strlcat` |
| `include/apps/control.h` | **Created** | Control Panel application interface with wallpaper themes and kernel security readout |
| `src/apps/control.c` | **Created** | Control Panel GUI: theme buttons (Teal, Navy, Slate, Forest), security status, hardware specs |
| `include/apps/minesweeper.h` | **Created** | Classic Windows Minesweeper game interface |
| `src/apps/minesweeper.c` | **Created** | Minesweeper: 9x9 grid, 10 mines, 7-seg LED readouts, animated smiley, safe first-click, BFS flood fill |
| `include/apps/snake.h` | **Created** | Retro Snake Arcade game interface |
| `src/apps/snake.c` | **Created** | Snake Arcade: 20x20 grid, WASD/arrows, food apple, high score, zero warning build |
| `src/apps/calc.c` | **Updated** | Upgraded to 5-column Scientific Calculator (sqrt, x^2, 1/x, %, DEL) |
| `src/gui/desktop.c` | **Updated** | Added shortcut hooks `[F7]` (Control Panel), `[F8]` (Explorer), `[F9]` (Terminal), `[F10]` (Minesweeper), `[F12]` (Task Manager) |
| `src/kernel/kernel.c` | **Updated** | Initialized VFS and RamFS during boot sequence; updated banner to Milestone 9 |

---

## 5. Build, Test, and Expected Output

### Build & Deploy Commands
```bash
# In WSL2:
cd ~/myos
make clean && make build
make iso
make deploy

# Launch VirtualBox VM:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\os\run.ps1" -SourceIso "C:\os\myos.iso"
```

### Serial Output (`C:\os\serial.log`)
```text
================================================================
  MyOS - 32-bit x86 Kernel (Milestone 9: VFS, RamFS, Desktop Apps) 
================================================================
[OK] Serial COM1 initialized (115200 8N1)
[OK] GDT initialized (Flat 4GB code & data segments)
[OK] IDT initialized (256 gates)
...
[OK] Dynamic kernel heap initialized at virtual 0xd0000000
[VFS] Virtual File System initialized (32 file descriptors)
[VFS] Mounted root node at '/'
[RAMFS] RamFS initialized with 4 pre-seeded documents
[OK] Virtual File System & RamFS mounted at '/'
[TASK] Multitasking initialized: Boot Task (PID 1), Idle Task (PID 0)
[SCHED] Round-Robin Scheduler initialized (Quantum: 100 ms)
[OK] Background monitor task spawned (PID 2)
[OK] Hardware interrupts enabled (STI)

>>> [TEST HOOK] F10 pressed: Starting Desktop Window Manager <<<
[DESKTOP] Launching Desktop Window Manager environment...
[GFX] Back buffer allocated successfully.
[WM] Window Manager initialized successfully.
[DESKTOP] Desktop active. Drag windows, click widgets, ESC to exit.
[DESKTOP] F8 pressed: Opening File Explorer...
[EXPLORER] File Explorer window opened
[DESKTOP] F9 pressed: Opening Terminal...
[TERMINAL] Terminal window opened
[DESKTOP] F12 pressed: Opening Task Manager...
[TASKMGR] Task Manager window opened
```

---

## 6. Viva Defense Questions & Answers

### Q1: Why is an abstract VFS necessary instead of calling driver read/write directly?
**Answer:** Without a VFS, every user program or GUI window would need to know the internal data structures of every storage device. The VFS acts as a common contract (`vfs_node_t` function table) so programs call `vfs_read()` and `vfs_write()` identically regardless of whether the target file is on RamFS, an IDE hard drive, a CD-ROM, or a network pipe.

### Q2: How does RamFS dynamically expand files when writing beyond the initial allocation?
**Answer:** When `ramfs_write` receives an offset + size exceeding the node's current `capacity`, it calculates a new capacity (doubling the current size), allocates a new buffer via `kmalloc`, copies the existing data using `memcpy`, updates the pointer, and deallocates the old buffer using `kfree`.

### Q3: Why can PID 0 and PID 1 not be killed in Task Manager?
**Answer:** PID 0 is the CPU Idle Task, which keeps the processor halted safely (`sti; hlt`) when no other threads are ready. PID 1 is the kernel boot thread (`kernel_main` and desktop event loop). Terminating either thread would leave the kernel without a scheduling baseline, triggering a triple fault or kernel crash.

### Q4: How does the Terminal emulator render characters and scroll vertically?
**Answer:** The Terminal maintains an internal grid of 65 columns by 18 rows. When text reaches the bottom row, `term_scroll_up()` shifts rows $1 \dots (N-1)$ into rows $0 \dots (N-2)$, blanks the bottom row, and resets the cursor to the bottom line before rendering into the window's 32-bit client canvas.

### Q5: How are file open descriptors managed in the kernel?
**Answer:** The kernel maintains a global descriptor table (`s_file_table[32]`). Calling `vfs_open()` finds an unused descriptor slot, binds it to the resolved `vfs_node_t*`, resets the seek offset to 0, increments the node's reference count, and returns the descriptor index as a handle for future read/write operations.

---

## 7. Kernel Security Hardening & Defenses

To ensure robust operation and protect against common kernel vulnerabilities, Milestone 9 incorporates multi-layer defensive programming across the freestanding C codebase:

1. **OpenBSD Bounded String Utilities (`strlcpy` and `strlcat`)**:
   Standard C library string functions like `strcpy` and `strcat` are historically responsible for thousands of buffer overflow exploits. MyOS implements bounded `strlcpy` and `strlcat` which take the full buffer capacity as an explicit parameter, guarantee NUL-termination under all conditions, and truncate safely if text exceeds buffer bounds.
2. **VFS Path Traversal & Sandbox Sanitization**:
   `vfs_lookup()` checks path components for illegal directory traversal sequences (`.` and `..`), canonicalizing them to strictly prevent sandbox escape outside the virtual root `/`.
3. **RamFS Integer Wrap-Around & Overflow Guards**:
   Writing to files guards against arithmetic overflow (`offset + size < offset` or exceeding `RAMFS_MAX_FILE_SIZE`), preventing kernel heap heap-spray or buffer corruption.
4. **Task Stack Canary Protection (`0xDEADBEEF`)**:
   Every kernel task allocated an 8 KB stack has a canary magic word (`0xDEADBEEF`) placed at the base of the stack frame. `task_check_canary(task_t* task)` validates that deep recursion or unbounded stack usage has not breached the stack perimeter.
5. **Architectural Memory Protection**:
   Virtual Page 0 (`0x00000000`) is marked absent in the page directory, immediately trapping NULL pointer dereferences with a Page Fault exception (`#PF`), preventing silent corruption.

---

## 8. Windows Suite Expansion & Retro OG Games

### 1. Control Panel
- **Desktop Personalization**: Live wallpaper theme switcher supporting 4 bespoke palettes (Teal, Classic Navy, Slate Blue, Forest Green).
- **Security Readout**: Real-time validation of Page 0 trap, Stack Canaries, VFS Sandbox, and Bounded String drivers.
- **Hardware Specs**: System summary displaying x86 32-bit architecture, 1024x768x32 linear framebuffer, and 100 Hz preemptive timer.

### 2. Classic Windows Minesweeper
- **Board Configuration**: Authentic 9x9 grid with 10 hidden naval mines and 71 safe cells.
- **Fair First-Click Protection**: Dynamically generates mine distribution using hardware timer jitter only after the player's first Dig click, strictly protecting the clicked coordinate and its 8 neighboring cells.
- **BFS Flood-Fill Reveal**: Queue-based breadth-first search reveals connected zero-mine regions without consuming kernel stack space.
- **Retro Windows 95 Visuals**: Sunken 3-character 7-segment digital LED readouts for remaining mines and elapsed timer; interactive yellow smiley button reflecting game state (normal, sunglasses on victory, shocked on detonation); toggleable Dig/Flag mode with red triangular pennants.

### 3. Retro Snake Arcade
- **Gameplay**: 20x20 grid with 14x14 pixel cells, smooth WASD and arrow key control, instantaneous keyboard responsiveness, food spawning, real-time score display, and high score tracking across sessions.

### 4. Scientific Calculator Upgrades
- Expanded to a 5-column desktop keypad adding Integer Square Root (`sqrt`), Square (`x^2`), Reciprocal (`1/x`), Percentage (`%`), and Backspace (`DEL`).


