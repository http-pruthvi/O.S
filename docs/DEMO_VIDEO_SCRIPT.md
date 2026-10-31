# Demo Video Recording Script & Storyboard
## 7-Minute Final Year Project Video Presentation

**Project**: 32-bit x86 Protected Mode Operating System (MyOS)  
**Presenter**: Pruthviraj Vinod Phuse  
**Target Duration**: 6 to 7 minutes  
**Format**: Screen recording (VirtualBox VM `OSProject` + `serial.log` monitor) with live voiceover  

---

## Scene 1: Introduction & Project Overview (0:00 – 0:45)

**Visuals**:
- Title slide on screen: "MyOS: 32-bit x86 Protected Mode Operating System — B.Tech Final Year Project".
- Visual Studio Code showing the clean repository directory structure (`src/`, `include/`, `docs/`, `Makefile`).
- Terminal ready to compile.

**Spoken Script**:
> "Hello everyone. My name is Pruthviraj Vinod Phuse, and today I am presenting **MyOS**, an original, 32-bit x86 protected mode operating system kernel developed entirely from scratch for my B.Tech Computer Engineering final year project.
> 
> Unlike typical operating system assignments that run in user space or rely on existing Linux libraries, MyOS is a monolithic kernel written in freestanding C and NASM assembly with zero runtime dependencies. It boots on bare x86 hardware, configures CPU segmentation and two-level paging, implements a 1024x768 linear framebuffer graphics engine, provides a windowed desktop environment with productivity apps, and runs a preemptive round-robin scheduler."

---

## Scene 2: Build & Boot Sequence (0:45 – 1:30)

**Visuals**:
- In WSL terminal, type `make clean && make iso && make run`.
- Show compilation of NASM files (`boot.asm`, `switch.asm`), C files, and linking of `myos.bin`.
- VirtualBox launches VM `OSProject`. GRUB menu appears briefly and boots into MyOS.
- Screen switches into high-resolution 1024x768x32 graphics mode.
- Split screen showing VirtualBox display on the left and `C:\os\serial.log` on the right.

**Spoken Script**:
> "We compile the kernel using a freestanding `i686-elf-gcc` cross-compiler and NASM into a 32-bit ELF binary, which is packaged into a bootable ISO with GRUB.
> 
> Here, GRUB verifies our Multiboot 1 header, switches the CPU into protected mode, configures the VBE linear framebuffer at 1024 by 768 resolution at 32 bits per pixel, and jumps to our kernel entry point at 1 MB.
> 
> As you can see in the serial log and on the framebuffer console, the kernel executes its initialization sequence:
> 1. Setting up the Global Descriptor Table (GDT) for a flat 4 GB memory space.
> 2. Installing the 256-gate Interrupt Descriptor Table (IDT).
> 3. Remapping the 8259 PIC to vectors 32 to 47 to avoid CPU exception collisions.
> 4. Initializing the 8254 timer at 100 Hz and unmasking the PS/2 keyboard and mouse.
> 5. Detecting 127 MB of usable RAM via the physical memory map.
> 6. Enabling two-level paging with Page 0 unmapped for NULL-pointer protection.
> 7. Initializing the dynamic kernel heap at virtual address `0xD0000000`.
> 8. Spawning the idle task, main task, and background monitor task with preemptive multitasking."

---

## Scene 3: Memory Diagnostics & Hardware Test Hooks (1:30 – 2:30)

**Visuals**:
- On the VM window, press **`[F3]`** (Memory Statistics).
- Show live PMM total frames, free frames, and heap statistics on screen and serial log.
- Press **`[F4]`** (Heap Stress Test). Show 20 blocks allocated, alternate blocks freed, and coalesced.
- Press **`[F8]`** (Heap Growth Test). Show 200 KB allocated across 10 blocks, PMM frame allocation, page table mapping, and reverse-order coalescing.

**Spoken Script**:
> "To verify that our low-level subsystems function reliably, the kernel includes interactive diagnostic hooks.
> 
> Pressing **F3** displays our live memory metrics: our bitmap Physical Memory Manager tracks 32,752 frames across 127 MB of RAM.
> 
> Pressing **F4** runs a 20-block heap fragmentation stress test. Notice how blocks are allocated, alternate blocks are freed, and when all blocks are released, our dynamic coalescing engine merges them back into a single contiguous free block.
> 
> Pressing **F8** exercises dynamic heap expansion: the heap requests new physical frames from the PMM, maps them into the page tables, flushes the CPU TLB with `invlpg`, and validates data integrity across 200 KB."

---

## Scene 4: Preemptive Multitasking & Process Table (2:30 – 3:30)

**Visuals**:
- Point to `serial.log` showing regular heartbeat messages from PID 2: `[BG-MONITOR] Heartbeat #10 | Active Tasks: 3`.
- Press **`[F12]`** on the keyboard. Show the active task table (`ps`) on screen.
- Press **`[F11]`** on the keyboard. Show `worker_1` (PID 3) spawning, computing across 3 seconds with timed sleep, and terminating cleanly.
- Press **`[F12]`** again to show active tasks return from 4 back to 3.

**Spoken Script**:
> "Now let us examine Milestone 7: Preemptive Multitasking.
> 
> In the background, task PID 2 (`bg_monitor`) is actively executing, sleeping for 500 milliseconds via `task_sleep_ms`, waking up on timer ticks, and logging periodic heartbeats without interfering with our main thread.
> 
> Pressing **F12** triggers our process table dump. We see three active tasks:
> - PID 0: the low-power `idle` task running `sti; hlt`.
> - PID 1: our interactive `kernel_main` thread.
> - PID 2: the background monitor daemon.
> 
> When I press **F11**, the kernel dynamically allocates an 8 KB stack, forges the stack frame, and spawns `worker_1` with PID 3. Notice how it executes its computation across 3 seconds using timed sleeps. When its entry function finishes, our assembly trampoline invokes `task_exit()`, and our zombie reaper safely deallocates its stack. Pressing **F12** confirms the task count cleanly returned to 3."

---

## Scene 5: GUI Foundation & Mouse Paint Demo (3:30 – 4:15)

**Visuals**:
- Press **`[F9]`** on the keyboard.
- The screen transitions into the 1024x768x32 Graphics Paint Demo.
- Smooth mouse cursor moves across the screen with zero flicker.
- Select red, blue, and green color swatches.
- Draw several smooth curves and shapes using the left mouse button.
- Click the "Clear" button to wipe the canvas.
- Press **`[ESC]`** to return cleanly to the framebuffer text console.

**Spoken Script**:
> "Pressing **F9** launches Milestone 4: our 2D Graphics Engine and Paint Demo.
> 
> Here, all rendering is performed in software at 1024x768x32 direct RGB. To eliminate flicker, we render to a 3 MB off-screen back buffer in system RAM and blit dirty rectangles to VRAM using `rep movsd`.
> 
> Notice the smooth software mouse cursor: we implement a save-under buffer that saves background pixels, renders the cursor sprite, and restores the background on movement.
> 
> As I draw with the mouse, Bresenham's line algorithm connects sample points, ensuring continuous strokes even on fast mouse movements. Pressing **Escape** returns us cleanly to the framebuffer console."

---

## Scene 6: Desktop Environment & Window Manager (4:15 – 5:30)

**Visuals**:
- Press **`[F10]`** on the keyboard.
- The Desktop Window Manager loads: Deep Slate Blue wallpaper, desktop shortcut icons on the left, taskbar at the bottom with Start button and digital clock, and showcase windows.
- Click and drag the "Widget Showcase" window by its title bar. Drag it over the "System Monitor" window, demonstrating Z-ordering and dropshadows.
- Click the [+] and [-] buttons on the Counter widget.
- Click the text box and type: "Hello from MyOS kernel!". Show real-time echo.
- Click the "Start" button on the taskbar: show the 3D beveled Start Menu popping up.

**Spoken Script**:
> "Pressing **F10** enters Milestone 5 and 6: our Desktop Window Manager.
> 
> Our Z-order compositor manages overlapping windows, title bars, and 3D beveled widgets.
> 
> As I drag a window by its title bar, notice that movement is clamped strictly within screen bounds so windows can never be dragged off-screen. When I click on a window beneath another, the compositor updates the Z-order list and brings it forward.
> 
> The widget framework handles mouse hit-testing and event dispatch: clicking [+] and [-] increments and decrements our counter state, and typing in the text box updates our live echo label.
> 
> At the bottom, our 40-pixel taskbar hosts a live digital clock updated on timer ticks, active window tabs, and a functional Start Menu."

---

## Scene 7: Desktop Productivity Applications Suite (5:30 – 6:30)

**Visuals**:
- From the Start Menu or desktop icons, launch the **Calculator**.
  - Click `1`, `2`, `5`, `*`, `8`, `=`, displaying `1000`.
  - Type `50 / 2 =`, displaying `25`.
- Launch the **Text Editor (Notepad)**.
  - Type two lines of text: "Operating System Final Project\nBuilt by Pruthviraj".
  - Press Enter to demonstrate line splitting, and Backspace to demonstrate line concatenation.
  - Point to the live status bar showing Line 2, Col 19, and character count.
- Launch the **Mini Spreadsheet**.
  - Enter numbers into cells: A1=10, A2=20, A3=30.
  - In cell A4, type `=SUM(A1:A3)` and press Enter. Cell displays `60`.
  - In cell B1, type `=AVG(A1:A3)` and press Enter. Cell displays `20`.
- Launch the **Real-Time Clock & Calendar**.
  - Show the large digital readout matching uptime and the monthly calendar.

**Spoken Script**:
> "Now let us look at our Milestone 6 Productivity Suite:
> 
> First, our **Calculator**: it features a full 4x5 button grid and a 32-bit arithmetic state machine with native x86 single-instruction division, supporting both mouse clicks and keyboard numeric typing.
> 
> Second, our **Text Editor**: it maintains an in-memory document buffer of 64 lines by 60 columns. It supports dynamic cursor rendering, line insertion on Enter, line concatenation on Backspace, and a real-time status bar.
> 
> Third, our **Mini Spreadsheet**: it provides a 60-cell matrix (columns A to F, rows 1 to 10). It features an editable formula bar and a recursive-descent formula evaluation engine supporting `=SUM`, `=AVG`, `=MIN`, and `=MAX` across ranges, as well as arithmetic formulas. Notice how changing a cell automatically updates dependent formulas.
> 
> Fourth, our **Real-Time Clock and Calendar**: it implements Sakamoto's constant-time day-of-week formula to render a monthly calendar alongside an uptime-synced digital clock."

---

## Scene 8: Conclusion & Defense Summary (6:30 – 7:00)

**Visuals**:
- Bring the System Monitor window forward and click "Refresh Metrics", showing `Active Tasks: 3 (Preemptive)`.
- Exit the desktop cleanly back to the framebuffer console using ESC.
- Show `C:\os\serial.log` with clean execution and zero memory leaks.
- Display closing slide with GitHub repository link: `https://github.com/http-pruthvi/O.S`.

**Spoken Script**:
> "In conclusion, MyOS successfully demonstrates that an operating system can be developed from bare-metal x86 hardware up to a modern graphical windowed environment with preemptive multitasking—completely from first principles without relying on third-party libraries.
> 
> The codebase is fully modular, documented, and published on GitHub. Thank you for your time, and I am now ready for the viva examination."

