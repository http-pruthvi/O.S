# Master Viva Defense Preparation Guide
## 40 Comprehensive Questions & In-Depth Answers for Operating Systems Defense

---

### Category 1: Boot & Multiboot 1 Protocol

#### Q1: What is the Multiboot specification and why did you use Multiboot 1 instead of writing a custom 512-byte MBR bootloader?
**Answer**:
A traditional BIOS Master Boot Record (MBR) is restricted to 512 bytes on disk. In 512 bytes, one must enable the A20 line, read disk sectors using slow BIOS INT 13h calls, switch to 32-bit protected mode, parse an executable format, and jump to the kernel. This adds substantial boilerplate without architectural value.
The Multiboot standard (created by the Free Software Foundation) establishes a formal contract between bootloaders (like GRUB) and kernels. By placing a valid Multiboot header in the first 8 KB of the kernel ELF, GRUB automatically loads our kernel into physical memory at 1 MB, enables the A20 line, switches the CPU into 32-bit protected mode (`CR0.PE = 1`), initializes video modes (such as VBE linear framebuffer), and passes system information (memory map, framebuffer pointer) in the `multiboot_info_t` structure. This ensures the project focuses on operating system design rather than bootloader quirks.

#### Q2: What are the fields of the Multiboot 1 header and how does GRUB verify it?
**Answer**:
The Multiboot 1 header is located in `boot.asm` aligned on a 4-byte boundary:
1. `magic`: Must be `0x1BADB002`.
2. `flags`: Bitmask specifying requested features. We set bit 0 (align modules on 4 KB page boundaries), bit 1 (request memory map), and bit 2 (request video mode). Total flags: `0x00000007`.
3. `checksum`: A 32-bit unsigned value calculated such that:
   $$\text{magic} + \text{flags} + \text{checksum} = 0 \pmod{2^{32}}$$
   $$\text{checksum} = -(\text{magic} + \text{flags})$$
When GRUB loads the kernel binary, it scans the first 8 KB and computes the sum of these three fields. If the 32-bit sum equals zero, GRUB confirms the kernel is Multiboot compliant and executes it.

#### Q3: Why is the kernel loaded at 1 MB (`0x00100000`) instead of 0x00000000?
**Answer**:
The first 1 MB of physical memory on x86 IBM PC compatible systems (known as conventional memory and the Upper Memory Area) is reserved for legacy hardware mappings:
- `0x00000000`–`0x000003FF`: Real-Mode Interrupt Vector Table (IVT)
- `0x00000400`–`0x000004FF`: BIOS Data Area (BDA)
- `0x00080000`–`0x0009FFFF`: Extended BIOS Data Area (EBDA)
- `0x000A0000`–`0x000BFFFF`: Video Display Memory (VGA text buffer at `0xB8000`, VGA graphics)
- `0x000C0000`–`0x000FFFFF`: Expansion ROMs and BIOS Motherboard Firmware
Loading a kernel below 1 MB would risk overwriting BIOS tables or video buffers. Memory starting at 1 MB (`0x00100000`, extended memory) is guaranteed contiguous, free RAM.

#### Q4: What values are passed in registers to `_start` when GRUB transfers control?
**Answer**:
GRUB follows the Multiboot specification:
- `EAX`: Contains the bootloader magic value `0x2BADB002` (confirming GRUB loaded the OS).
- `EBX`: Contains the 32-bit physical address pointing to the `multiboot_info_t` data structure populated by GRUB.
Our entry stub in `boot.asm` pushes `EBX` and `EAX` onto the newly allocated stack and calls `kernel_main(magic, mboot_info)`.

---

### Category 2: CPU Architecture, Segmentation & Interrupts

#### Q5: What is the Global Descriptor Table (GDT) and why is it necessary in 32-bit protected mode even if we use paging?
**Answer**:
On x86 processors, memory addressing in protected mode is governed by hardware segmentation. Even if an operating system relies entirely on paging for virtual memory translation, the x86 CPU architecture enforces segmentation and requires at least two valid segment descriptors: one for code execution (`CS`) and one for data/stack access (`DS`, `SS`).
We configure a **flat memory model** where both code and data segments span the full 4 GB address space (Base = `0x00000000`, Limit = `0xFFFFF`, Granularity = 4 KB, making $0xFFFFF \times 4096 = 4\text{ GB}$). This effectively neutralizes segmentation, ensuring logical addresses match linear addresses, while fulfilling x86 hardware requirements.

#### Q6: How is the GDT loaded into the CPU, and why is a far jump required?
**Answer**:
The GDT is registered using the `lgdt` instruction, which loads the 6-byte GDT descriptor pointer (2 bytes size limit, 4 bytes linear base address).
However, executing `lgdt` does not immediately reload the `CS` (Code Segment) register because `CS` cannot be loaded with a simple `mov` instruction. The CPU requires a **far jump** (`jmp 0x08:.flush` where `0x08` is the kernel code selector). The far jump forces the CPU pipeline to flush, serialization to occur, and the `CS` register to latch the new descriptor permissions and ring level. Segment registers `DS`, `ES`, `FS`, `GS`, and `SS` are then reloaded with the kernel data selector (`0x10`).

#### Q7: What is the Interrupt Descriptor Table (IDT) and what is the difference between an Interrupt Gate and a Trap Gate?
**Answer**:
The IDT is a table of 256 8-byte descriptors mapping interrupt vectors (0–255) to handler addresses in memory.
- **Interrupt Gate (`0x8E`)**: Automatically clears the CPU Interrupt Flag (`IF = 0`) upon entering the handler. This disables hardware interrupts, preventing nested interrupts until explicitly re-enabled or until `iret` restores the previous `EFLAGS`.
- **Trap Gate (`0x8F`)**: Leaves the Interrupt Flag (`IF`) unchanged. Interrupts remain enabled inside the handler.
We use **Interrupt Gates** for all hardware IRQs and CPU exceptions to prevent race conditions and stack overflows caused by nested interrupts.

#### Q8: Why did the 8259 PIC need to be remapped?
**Answer**:
In original IBM PC design, Master PIC IRQs 0–7 were wired to CPU interrupt vectors 8–15. However, Intel reserved vectors 0–31 for CPU exceptions (e.g., Vector 8 = Double Fault, Vector 13 = General Protection Fault, Vector 14 = Page Fault).
If the PIC was not remapped, a hardware timer interrupt on IRQ 0 would trigger vector 8, which the CPU interprets as a Double Fault exception panic. We remap the Master PIC to vectors 32–39 (`0x20`–`0x27`) and Slave PIC to vectors 40–47 (`0x28`–`0x2F`).

#### Q9: What are spurious interrupts and how are they handled?
**Answer**:
Spurious interrupts occur when an interrupt request line experiences electrical noise or drops its request line before the PIC receives an acknowledgment from the CPU.
- **Master PIC Spurious IRQ 7**: If bit 7 of the Master In-Service Register (ISR) is clear, it is spurious. We return immediately **without sending an EOI**.
- **Slave PIC Spurious IRQ 15**: If bit 7 of the Slave ISR is clear, it is spurious. We send an EOI only to the Master PIC (to clear the cascade IRQ 2 line) but **do not send an EOI to the Slave PIC**.

---

### Category 3: Device Drivers

#### Q10: How does UART 16550 serial communication work on x86?
**Answer**:
The 16550 UART serial chip is controlled via I/O ports starting at base address `0x3F8` for `COM1`.
1. The Divisor Latch Access Bit (DLAB, bit 7 of Line Control Register) is enabled to set the baud rate generator.
2. The base clock (1.8432 MHz / 16 = 115200 Hz) is divided by 1 (`divisor = 1`) to configure 115200 baud.
3. DLAB is cleared, and 8 bits, no parity, 1 stop bit (8N1) is selected.
4. The 14-byte FIFO buffer is enabled.
5. Transmitting a byte involves polling the Line Status Register (`0x3FD`) until bit 5 (Transmitter Holding Register Empty) is 1, then writing the byte to the data port (`0x3F8`).

#### Q11: How is the 8254 PIT timer configured to fire at exactly 100 Hz?
**Answer**:
The 8254 PIT oscillator runs at an internal crystal frequency of 1.193182 MHz ($1,193,182\text{ Hz}$).
To generate an interrupt rate of 100 Hz, we calculate the 16-bit divisor:
$$\text{Divisor} = \frac{1,193,182}{100} \approx 11,931 \quad (\text{hex: } 0x2E9B)$$
We write the command byte `0x36` to port `0x43` (Channel 0, access low byte then high byte, Mode 3 square wave, 16-bit binary), then send the low byte (`0x9B`) and high byte (`0x2E`) to data port `0x40`.

#### Q12: How do the PS/2 Keyboard and PS/2 Mouse share port 0x60 without conflicting?
**Answer**:
Both keyboard and mouse communicate through the 8042 PS/2 controller using data port `0x60` and status register port `0x64`.
When an interrupt fires:
- Status register bit 0 indicates output buffer full ($1 = \text{data ready}$).
- Status register bit 5 indicates source: $0 = \text{keyboard}$, $1 = \text{mouse/auxiliary device}$.
The keyboard ISR inspects bit 5; if set, it returns immediately so it does not swallow mouse bytes. Conversely, the mouse driver inspects status bit 5 to ensure only mouse packets are read.

#### Q13: How does the PS/2 mouse packet state machine maintain synchronization?
**Answer**:
Standard PS/2 mouse streaming delivers 3 consecutive bytes per movement event:
- Byte 0: Flags (buttons, signs, overflow). **Bit 3 is always 1**.
- Byte 1: X movement delta.
- Byte 2: Y movement delta.
If a byte is dropped or corrupt, the driver checks bit 3 of Byte 0. If bit 3 is not 1, the driver knows it has lost alignment, discards the byte, and resets its state machine to search for the true header byte.

---

### Category 4: Memory Management (PMM, Paging & Heap)

#### Q14: How does your Physical Memory Manager (PMM) track RAM and what is its time complexity?
**Answer**:
The PMM uses a 128 KB static bitmap in `.bss` where each bit corresponds to one 4 KB physical frame. 128 KB of bitmap contains $131,072 \times 8 = 1,048,576\text{ bits}$, tracking up to:
$$1,048,576 \times 4096\text{ bytes} = 4\text{ GB of physical RAM}$$
Allocation uses a 32-bit word scanning helper and a roving pointer (`last_alloc_index`). It scans 32 frames per comparison (`dword != 0xFFFFFFFF`). Best-case allocation time complexity is $O(1)$; worst-case across memory is $O(N)$ where $N$ is total frames. Deallocation is $O(1)$ via direct bit-clearing (`frame / 8` and `frame % 8`).

#### Q15: Explain two-level paging on 32-bit x86. How is a 32-bit linear address translated?
**Answer**:
In 32-bit protected mode with 4 KB pages, a 32-bit virtual address is split into three fields:
```text
31             22 21             12 11                      0
+----------------+-----------------+------------------------+
| Directory (10) | Page Table (10) |   Offset in Page (12)  |
+----------------+-----------------+------------------------+
```
1. Bits 22–31 (10 bits) index into the **Page Directory** (holding 1024 4-byte Page Directory Entries, PDEs). The Page Directory physical base is loaded in the `CR3` register.
2. The PDE points to the physical address of a **Page Table**.
3. Bits 12–21 (10 bits) index into the Page Table (holding 1024 4-byte Page Table Entries, PTEs).
4. The PTE provides the 20-bit physical frame base address.
5. Bits 0–11 (12 bits) represent the exact byte offset ($0 \text{ to } 4095$) within that 4 KB frame:
   $$\text{Physical Address} = (\text{PTE Frame Base}) + \text{Offset}$$

#### Q16: Why is Page 0 left unmapped in your paging tables?
**Answer**:
In C, the `NULL` pointer is defined as `(void*)0`. If physical address 0 was mapped to valid virtual memory, writing to a NULL pointer (`*ptr = 10` where `ptr = NULL`) would silently corrupt low memory without crashing.
By marking Page 0 (`0x00000000`–`0x00000FFF`) as **NOT PRESENT** in Page Table 0, any attempt to read or write through a NULL pointer immediately triggers an x86 Page Fault exception (`#PF`, Vector 14). This traps software bugs at runtime.

#### Q17: What registers and error codes are associated with a Page Fault (`#PF`)?
**Answer**:
When a Page Fault occurs:
1. The CPU automatically latches the 32-bit linear address that attempted the illegal access into control register **`CR2`**.
2. The CPU pushes an **Error Code** onto the stack before jumping to ISR 14:
   - Bit 0 ($P$): $0 = \text{page not present}$, $1 = \text{protection violation}$.
   - Bit 1 ($W/R$): $0 = \text{read access}$, $1 = \text{write access}$.
   - Bit 2 ($U/S$): $0 = \text{supervisor/kernel mode}$, $1 = \text{user mode}$.
Our panic handler reads `CR2` via inline assembly (`mov eax, cr2`) and decodes the error code to display diagnostic fault information.

#### Q18: What is the Translation Lookaside Buffer (TLB) and how do you invalidate a page mapping?
**Answer**:
The TLB is a high-speed hardware cache on the CPU that stores recent virtual-to-physical address translations.
When the kernel modifies or creates a page mapping in RAM, the CPU might continue using an old cached translation in the TLB. To synchronize the CPU, the kernel executes the `invlpg` assembly instruction:
```c
__asm__ volatile ("invlpg (%0)" : : "r"(virtual_addr) : "memory");
```
This invalidates the specific address entry in the TLB without requiring a full flush of all cached translations (which reloading `CR3` would cause).

#### Q19: How does the kernel heap (`kmalloc`/`kfree`) handle block splitting, coalescing, and expansion?
**Answer**:
The heap resides at `0xD0000000`. Memory is tracked using an intrusive doubly-linked list with 24-byte block headers:
- **Splitting**: When a free block is larger than the requested size plus header and minimum payload ($\ge \text{header} + 8$), the block is split into an allocated chunk and a new free block remainder.
- **Coalescing**: When `kfree` is called, the block is marked free. It checks if the physically adjacent next block is free; if so, their sizes and pointers are merged. It then checks if the preceding block is free; if so, it merges backwards. This prevents external heap fragmentation.
- **Expansion**: If no free block satisfies `kmalloc`, `heap_expand` requests new 4 KB physical frames from the PMM, maps them into the page tables, flushes the TLB via `invlpg`, and appends the new region as a free block.

---

### Category 5: Linear Framebuffer Graphics & 2D Engine

#### Q20: What is a Linear Framebuffer and how does it differ from legacy VGA text mode?
**Answer**:
- **VGA Text Mode (`0xB8000`)**: Legacy 80x25 character grid where each cell consists of 2 bytes (ASCII character + 8-bit attribute byte). The hardware character generator translates ASCII into glyphs. It cannot draw arbitrary pixels.
- **Linear Framebuffer**: A contiguous physical video memory aperture provided by VBE (VESA BIOS Extensions) through Multiboot. Each pixel on the screen is represented directly by a 32-bit truecolor memory value (`0x00RRGGBB`). Memory address calculation is linear:
  $$\text{Offset} = (y \times \text{Pitch}) + (x \times 4)$$
  This enables smooth window rendering, 2D primitives, fonts, and software cursors.

#### Q21: What is the difference between screen width and pitch in a linear framebuffer?
**Answer**:
Screen width is the number of active display pixels per row (e.g., 1024 pixels). Pitch (or stride) is the total number of bytes per scanline in video memory.
Because graphics hardware often aligns scanlines to 64, 128, or 256-byte hardware boundaries for memory controller efficiency, pitch is often greater than $\text{width} \times \text{bytes\_per\_pixel}$. In our 1024-pixel 32-bit display, pitch is $1024 \times 4 = 4096\text{ bytes}$. If pitch is ignored, rows wrap improperly and images render diagonally skewed.

#### Q22: Why does the graphics engine use an off-screen back buffer?
**Answer**:
Reading and writing directly to VRAM over the PCI bus is significantly slower than system RAM and causes visual tearing and flickering when windows are redrawn or dragged.
We allocate a 3 MB off-screen back buffer in system RAM (`kmalloc(pitch * height)`). All drawing primitives (lines, rectangles, glyphs) render into system RAM. Once a drawing pass completes, `gfx_flush_rect()` copies only the modified dirty rectangle to VRAM using hardware-accelerated 32-bit string moves (`rep movsd`).

#### Q23: How does the software mouse cursor avoid leaving trails on the screen?
**Answer**:
Hardware cursor support is not available in standard VBE linear framebuffers. We implement a software arrow cursor:
1. Before drawing the 12x19 cursor sprite, the pixels currently on the screen beneath the cursor are saved into a dedicated **save-under buffer**.
2. The cursor sprite is rendered with color-key transparency (magenta `0x00FF00FF` pixels are skipped).
3. When the mouse moves, the saved pixels from the save-under buffer are restored to the screen, erasing the cursor.
4. The background at the new mouse position is saved, and the cursor is drawn at the new coordinates.
5. The back buffer never contains the cursor; any dirty rectangle flush overlapping the cursor hides the cursor, flushes VRAM, and restores the cursor.

---

### Category 6: Window Manager & Desktop Productivity Apps

#### Q24: What is Z-ordering and how does your compositor manage overlapping windows?
**Answer**:
Z-ordering is the management of window depth along the visual Z-axis (front-to-back ordering).
The Window Manager maintains a linked list of windows ordered from bottom-most (first) to top-most (last).
- When drawing, the compositor iterates from bottom to top: background $\to$ bottom windows $\to$ top window $\to$ taskbar.
- When a user clicks inside a window, that window is unlinked and moved to the tail of the list, making it the top-most focused window, and the screen is repainted.

#### Q25: How does window dragging work without corrupting other windows?
**Answer**:
When a left mouse button press occurs within a window's title bar, the Window Manager engages dragging mode, recording the initial mouse offset relative to the window origin.
As the mouse moves:
1. The new window coordinates are computed and clamped so the title bar cannot leave the screen boundaries ($0 \le x \le \text{Screen Width} - w$, $0 \le y \le \text{Taskbar Y} - h$).
2. The union bounding box of the old window rectangle and new window rectangle is marked dirty.
3. The compositor redraws the desktop background and overlapping windows within that bounding rectangle into the back buffer and flushes it to VRAM.

#### Q26: How does the Spreadsheet formula engine evaluate formulas like `=SUM(A1:A5)`?
**Answer**:
The formula engine in `src/apps/sheet.c` implements a recursive evaluation parser:
1. If the cell text starts with `'='`, it identifies the function prefix (`SUM`, `AVG`, `MIN`, `MAX`).
2. It parses the range arguments (e.g., column range `'A'` to `'A'`, row range `1` to `5`).
3. It iterates through the target cells, reads their evaluated values, and computes the aggregate.
4. It also supports basic binary arithmetic operators (`+`, `-`, `*`, `/`) between cell references (e.g., `=A1+B1`).
5. Whenever a cell's value is modified, dependent formulas update automatically.

---

### Category 7: Processes, Concurrency & Preemptive Scheduler

#### Q27: What is the structure of a Task Control Block (TCB) in MyOS?
**Answer**:
Defined in `include/proc/task.h`:
```c
typedef struct task {
    uint32_t esp;                   /* Saved stack pointer (offset 0) */
    uint32_t id;                    /* Process / Task ID */
    char name[TASK_NAME_MAX_LEN];   /* Human-readable task name */
    task_state_t state;             /* READY, RUNNING, SLEEPING, ZOMBIE */
    uint8_t* stack_base;            /* Base pointer of allocated 8 KB stack */
    uint32_t stack_size;            /* Stack size (8192 bytes) */
    bool is_boot_task;              /* Flag to protect initial boot stack */
    uint32_t wake_tick;             /* Target tick for sleeping tasks */
    uint32_t ticks_run;             /* Total cumulative runtime ticks */
    uint32_t time_slice;            /* Remaining ticks in current quantum */
    struct task* next;              /* Ready queue pointer */
    struct task* all_next;          /* Global all-tasks list pointer */
} task_t;
```

#### Q28: Walk through the assembly instructions of `switch_context`. Why are volatile registers not saved?
**Answer**:
```nasm
switch_context:
    pushf                ; 1. Preserve EFLAGS (including Interrupt Flag)
    push ebp             ; 2. Preserve callee-saved registers per x86 C ABI
    push edi
    push esi
    push ebx
    mov eax, [esp + 24]  ; 3. Load address of old task's esp field
    mov [eax], esp       ; 4. Save current stack pointer into old task struct
    mov esp, [esp + 28]  ; 5. Load new task's saved esp into ESP
    pop ebx              ; 6. Restore callee-saved registers from new stack
    pop esi
    pop edi
    pop ebp
    popf                 ; 7. Restore EFLAGS (restoring new task's IF state)
    ret                  ; 8. Return to new task's return address
```
**Why volatile registers (`EAX`, `ECX`, `EDX`) are not saved**:
According to the System V x86 C calling convention, `EAX`, `ECX`, and `EDX` are caller-saved registers. Any C compiler guarantees that the caller of `switch_context` (the scheduler) has already pushed or discarded any values in those registers before making the function call. Only callee-saved registers (`EBX`, `ESI`, `EDI`, `EBP`) must be preserved across function calls.

#### Q29: How is a new task's stack forged so that `switch_context` can transition to it?
**Answer**:
When `task_create()` allocates an 8 KB stack, the task has never run before and has no natural stack frame. We forge its stack so it mirrors the exact layout created by `switch_context`:
- `sp[-1] = arg`: The parameter passed to the entry function.
- `sp[-2] = entry_fn`: The function pointer to execute.
- `sp[-3] = task_entry_trampoline`: The address `ret` pops into `EIP`.
- `sp[-4] = 0x00000202`: Initial `EFLAGS` (Bit 9 = 1, ensuring interrupts are enabled).
- `sp[-5] = 0`: Initial `EBP`.
- `sp[-6] = 0`: Initial `EDI`.
- `sp[-7] = 0`: Initial `ESI`.
- `sp[-8] = 0`: Initial `EBX`.
`task->esp` is initialized pointing to `sp[-8]`. When `switch_context` runs, it pops the zeroed registers, pops `EFLAGS`, and executes `ret`, which jumps directly into `task_entry_trampoline`.

#### Q30: What does the `task_entry_trampoline` do?
**Answer**:
In `src/proc/switch.asm`:
```nasm
task_entry_trampoline:
    sti                  ; Ensure hardware interrupts are enabled
    pop eax              ; Pop entry function pointer into EAX
    call eax             ; Execute entry_fn(arg)
    call task_exit       ; Clean termination if entry function returns
.hang:
    cli
    hlt
    jmp .hang
```
It enables interrupts, calls the entry function with `arg` at `[esp]`, and if the function returns, it calls `task_exit()` to transition the task into `TASK_ZOMBIE`.

#### Q31: How does preemptive multitasking work with the PIT timer interrupt?
**Answer**:
1. Every 10 ms (100 Hz), the 8254 PIT fires hardware IRQ 0.
2. The CPU saves `EFLAGS`, `CS`, `EIP` and jumps to `irq0` $\to$ `irq_common_stub` $\to$ `irq_dispatch(regs)`.
3. `irq_dispatch` issues `pic_send_eoi(0)` to acknowledge the PIC.
4. `timer_handler` increments `timer_ticks` and invokes `scheduler_tick()`.
5. `scheduler_tick()` decrements the running task's `time_slice`.
6. If `time_slice == 0` and other tasks are ready in the ready queue, `schedule()` is invoked.
7. `schedule()` calls `switch_context(&current->esp, next->esp)`, swapping stacks and resuming another task.

#### Q32: Why did `pic_send_eoi` need to be issued before `schedule()` during preemption?
**Answer**:
The 8259 PIC maintains an In-Service Register (ISR) bit indicating an interrupt is currently being serviced. Until an End-of-Interrupt (EOI, command `0x20`) is sent to the PIC, the PIC will not dispatch any further interrupts on that line or lower-priority lines.
If `pic_send_eoi` was called at the end of `irq_dispatch` (after the driver callback), a preemptive context switch inside the callback would switch tasks *before* the EOI was sent. The PIC would remain permanently locked with IRQ0 in service, preventing all future timer ticks and permanently halting preemption. Moving `pic_send_eoi` before the callback ensures the PIC is always receptive to future hardware interrupts.

#### Q33: How does the zombie reaper prevent memory corruption when a task exits?
**Answer**:
If a terminating task called `kfree(task->stack_base)` inside `task_exit()`, it would free the very memory page its CPU stack pointer (`ESP`) is currently pointing to. Any subsequent instruction or interrupt would corrupt the heap.
Instead, `task_exit()` sets the task state to `TASK_ZOMBIE` and calls `schedule()`. When the scheduler switches to another task, the zombie reaper runs from the context of that *other* task's stack, unlinks the dead task with `task_unlink()`, and frees its 8 KB stack and `task_t` descriptor safely.

#### Q34: What is the role of PID 0 (Idle Task)?
**Answer**:
If all application tasks are sleeping (waiting on `task_sleep_ms`) or blocked on I/O, the scheduler has no ready tasks to pick.
The idle task runs with PID 0. It executes:
```c
__asm__ volatile ("sti\n\thlt" : : : "memory");
```
`HLT` suspends the CPU in a low-power quiescent state until the next hardware interrupt (timer, keyboard, or mouse) occurs. Placing `sti` immediately before `hlt` is an atomic x86 instruction pair guaranteed to enable interrupts without a window where an interrupt can strike before halting.

---

### Category 8: Freestanding C & Embedded Engineering

#### Q35: What does `-ffreestanding` mean and what headers can a freestanding C kernel use?
**Answer**:
A **hosted** environment is a normal user-space C application with access to the full C standard library (`stdio.h`, `stdlib.h`, `math.h`, POSIX syscalls).
A **freestanding** environment is an operating system kernel running on bare metal where no standard C library exists. The compiler cannot assume any runtime libraries, initialization stubs (`crt0`), or heap allocators exist.
Under ISO C standards, a freestanding implementation is only permitted to provide six headers:
- `<stdint.h>` (fixed-width integer types)
- `<stddef.h>` (`size_t`, `NULL`, `ptrdiff_t`)
- `<stdarg.h>` (`va_list`, `va_start`, `va_arg`, `va_end`)
- `<stdbool.h>` (`bool`, `true`, `false`)
- `<float.h>` (floating-point limits)
- `<limits.h>` (integer limits)
All string functions (`strlen`, `memcpy`, `memset`), printing (`kprintf`), and memory allocators must be implemented from scratch.

#### Q36: Why did 64-bit integer division fail to compile on 32-bit x86 freestanding GCC?
**Answer**:
The 32-bit x86 architecture has native hardware instructions for 8-bit, 16-bit, and 32-bit integer division (`div` and `idiv`), but lacks a 64-bit division instruction.
When a C program divides a 64-bit integer (`long long` or `uint64_t`), GCC automatically generates a runtime library call to `__divdi3` or `__udivdi3`. In a hosted application, this symbol is resolved by `libgcc.a`. In a strict freestanding environment without runtime bloat, linking fails with an `undefined reference to '__divdi3'` error.
We resolved this by clamping calculations to 32-bit signed integers, which compiles directly to native single-instruction `idiv`.

#### Q37: Why are `-fno-stack-protector` and `-fno-pic` required in the compiler flags?
**Answer**:
- **`-fno-stack-protector`**: GCC's stack smashing protection inserts code to check a canary value stored in the thread-local control block (`%gs:0x14`). In a bare-metal kernel, thread-local storage and stack canaries do not exist yet; leaving it enabled results in crashes and undefined references to `__stack_chk_fail`.
- **`-fno-pic`** (Position Independent Code): Generates absolute memory references rather than Global Offset Table (GOT) references relative to the instruction pointer. Since our kernel is linked to run at absolute physical address `0x00100000`, PIC is unnecessary and adds runtime indirection overhead.

#### Q38: How does inline assembly with constraints work in GCC?
**Answer**:
GCC inline assembly uses the format:
```c
__asm__ volatile (
    "assembly template"
    : output operands
    : input operands
    : clobbered registers
);
```
- `"volatile"`: Tells the compiler the assembly has side-effects (e.g. port I/O or disabling interrupts) and must not be deleted or reordered by the optimizer.
- Constraints:
  - `"a"`: EAX / AL register
  - `"Nd"`: DX register (or 8-bit immediate constant)
  - `"=r"`: Any general-purpose register for output
  - `"memory"`: Acts as a compiler memory barrier, forcing the compiler to flush cached memory values before the assembly and reload them afterwards.

#### Q39: What is Sakamoto's algorithm used in the Clock application?
**Answer**:
Sakamoto's algorithm is an $O(1)$ constant-time mathematical formula to compute the day of the week ($0 = \text{Sunday}, 6 = \text{Saturday}$) for any Gregorian calendar date:
```c
int day_of_week(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}
```
It computes leap year offsets without iterative loops or tables, making it optimal for an in-kernel calendar.

#### Q40: What makes MyOS an original engineering project suitable for a B.Tech degree?
**Answer**:
Rather than following simple online copy-paste tutorials that stop at VGA text mode printing:
1. MyOS implements a complete **8-layer architectural stack** from boot to GUI to multitasking.
2. It implements a **1024x768x32 linear framebuffer graphics engine** with custom 2D primitives and font rendering.
3. It features an interactive **Window Manager with Z-order compositing, taskbar, and Start Menu**.
4. It includes four functional **in-kernel productivity applications** (Calculator, Text Editor, Spreadsheet with formulas, Clock).
5. It implements **preemptive round-robin multitasking** with independent 8 KB stacks and assembly context switching.
6. Every module is documented, verified with test hooks `[F1]`–`[F12]`, and version-controlled with zero external library bloat.
