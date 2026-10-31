# Milestone 7: Processes & Scheduler — Final Report

## 1. Goal and Overview
The goal of Milestone 7 is to transition the operating system from a single-threaded kernel loop into a true multitasking operating system with preemptive round-robin scheduling. 

In this milestone, we implemented:
1. **Task Control Block (`task_t`)**: Per-task data structures tracking Process ID (PID), lifecycle states, runtime tick accounting, and dedicated kernel stacks.
2. **Dedicated Kernel Stacks**: Each dynamically spawned task receives an independent 8 KB kernel stack allocated from the kernel heap, 16-byte aligned.
3. **Assembly Context Switch (`switch_context`)**: A lean, low-level routine in NASM assembly that preserves callee-saved registers (`ebp`, `ebx`, `esi`, `edi`) and `eflags`, swaps the CPU stack pointer (`esp`), and restores execution context.
4. **Task Trampoline (`task_entry_trampoline`)**: A forged stack entry landing pad that enables hardware interrupts (`sti`), invokes the task entry function with its argument in accordance with the 32-bit x86 cdecl ABI, and cleanly directs returning tasks to `task_exit()`.
5. **Preemptive Round-Robin Scheduler**: Driven by the PIT timer interrupt at 100 Hz (10 ms ticks). Every task gets a default time quantum of 10 ticks (100 ms). If the quantum expires and other runnable tasks are waiting in the ready queue, the scheduler preempts the active task.
6. **Early PIC EOI Acknowledgment**: Sent *before* invoking interrupt callbacks so that preemptive task switches never block hardware interrupts.
7. **Thread-Safe Critical Sections**: Re-entrant interrupt masking via `irq_save()` and `irq_restore()` protecting scheduler queues, memory allocators (`kmalloc`, `kfree`, `pmm`), and dual-sink formatted printing (`kprintf`).
8. **Timed Sleeping (`task_sleep_ms`)**: Suspends tasks onto a sleeping queue and automatically wakes them when target timer ticks arrive.
9. **Automatic Zombie Reaping**: Deallocates stacks and TCBs of terminated tasks without tasks attempting to free their own active stack.
10. **Full Desktop & Interactive Test Hooks**:
    - **PID 0**: Background `idle` task executing `sti; hlt` when no tasks are ready.
    - **PID 1**: Interactive `kernel_main` / Desktop Window Manager.
    - **PID 2**: Background monitor daemon logging periodic heartbeats.
    - **`[F11]`**: Dynamically spawns concurrent compute worker tasks.
    - **`[F12]`**: Dumps formatted `ps` task table to COM1 serial and display.

---

## 2. Architecture and How It Works

### Context Switch Flow Diagram
```text
           +---------------------------------------------+
           | Current Task Running (e.g. PID 1 / Desktop) |
           +---------------------------------------------+
                                  |
              Preemption (Timer IRQ0) or task_yield()
                                  |
                                  v
                        schedule() in scheduler.c
                                  |
                                  v
                    switch_context(&old->esp, new->esp)
                                  |
           +----------------------+----------------------+
           | 1. pushf                                   |
           | 2. push ebp, edi, esi, ebx                 |
           | 3. mov [old_esp], esp  (Save old stack ptr)|
           | 4. mov esp, [new_esp]  (Load new stack ptr)|
           | 5. pop ebx, esi, edi, ebp                  |
           | 6. popf                                    |
           | 7. ret                                     |
           +--------------------------------------------+
                                  |
                   Switched to New Task Context
                                  |
        +-------------------------+-------------------------+
        |                                                   |
        v                                                   v
[Resumed Task]                                    [Brand New Task]
Returns from schedule() into                      Jumps to task_entry_trampoline
interrupted code or yield caller                  1. sti (enable interrupts)
                                                  2. pop eax (entry_fn)
                                                  3. call eax (execute entry)
                                                  4. call task_exit
```

### Initial Forged Stack for Newly Spawned Tasks
```text
High Address (Stack Base + 8192 bytes, 16-byte aligned)
+------------------------------------+
| void* arg                          | <--- [esp + 4] inside entry: first parameter
+------------------------------------+
| entry_fn pointer                   | <--- Popped into EAX by task_entry_trampoline
+------------------------------------+
| task_entry_trampoline Address      | <--- Popped into EIP by 'ret' in switch_context
+------------------------------------+
| Initial EFLAGS: 0x00000202         | <--- Popped by 'popf' (IF=1 bit 9 enables interrupts)
+------------------------------------+
| Initial EBP:    0x00000000         | <--- Popped by 'pop ebp'
+------------------------------------+
| Initial EDI:    0x00000000         | <--- Popped by 'pop edi'
+------------------------------------+
| Initial ESI:    0x00000000         | <--- Popped by 'pop esi'
+------------------------------------+
| Initial EBX:    0x00000000         | <--- Popped by 'pop ebx'
+------------------------------------+ <--- Initial task->esp points here
Low Address
```

---

## 3. Key Concepts Explained Simply

### What is a Context Switch?
A CPU core can execute only one instruction stream at any single moment. A **context switch** is the mechanism of saving all volatile processor registers (stack pointer, instruction pointer, flags, general-purpose registers) belonging to one task, and loading the saved register state of another task. The CPU seamlessly resumes executing the new task as if it had never been paused.

### Cooperative vs. Preemptive Multitasking
- **Cooperative Multitasking**: A task runs indefinitely until it voluntarily relinquishes the CPU by calling `yield()` or `sleep()`. If a task enters an infinite loop or hangs, the entire operating system freezes.
- **Preemptive Multitasking**: A hardware timer interrupt periodically interrupts the CPU regardless of what the current task is doing. If the current task has exhausted its allocated time slice (quantum), the kernel forcibly preempts it and assigns the CPU to the next ready task.

### Why Must the Idle Task Use `HLT`?
If all application tasks are sleeping or waiting for input, the CPU still needs something to run. If the idle task ran an empty `while (1) {}` busy loop, the CPU would consume 100% of the host machine's processing power. By executing `sti; hlt`, the CPU enters a low-power quiescent state and halts instruction execution until the next hardware interrupt (such as the timer or keyboard) arrives.

### Why Re-Entrant `irq_save()` and `irq_restore()`?
Disabling interrupts with `cli` and enabling them with `sti` is insufficient in modular operating systems. If function `A` disables interrupts and calls helper function `B`, and `B` executes `sti` before returning, interrupts would be prematurely enabled while `A` is still inside its critical section. `irq_save()` reads and pushes the CPU's `EFLAGS` register and returns it, allowing `irq_restore(flags)` to restore `EFLAGS` to its exact previous state.

---

## 4. Files Created and Modified

| File | Status | Role |
| :--- | :--- | :--- |
| `include/arch/lock.h` | **NEW** | Re-entrant interrupt masking primitives (`irq_save`, `irq_restore`, `irq_is_enabled`). |
| `include/proc/task.h` | **NEW** | Task Control Block (`task_t`), states, stack definitions, task management prototypes. |
| `include/proc/scheduler.h` | **NEW** | Preemptive round-robin scheduler interface, ready queue, context switch prototypes. |
| `src/proc/switch.asm` | **NEW** | Assembly `switch_context` register swap and `task_entry_trampoline` landing pad. |
| `src/proc/task.c` | **NEW** | Task lifecycle implementation, 8 KB stack forging, idle task (PID 0), boot task (PID 1). |
| `src/proc/scheduler.c` | **NEW** | FIFO ready queue, time-slice accounting, sleeping queue wakeups, zombie reaper, `ps` table dump. |
| `src/arch/irq.c` | **MOD** | Updated to acknowledge PIC with `pic_send_eoi` *before* driver callbacks. |
| `src/drivers/timer.c` | **MOD** | Integrated `scheduler_tick()` into IRQ0 callback for preemptive time-slice tracking. |
| `src/mm/heap.c` | **MOD** | Protected `kmalloc` and `kfree` critical sections with `irq_save` / `irq_restore`. |
| `src/mm/pmm.c` | **MOD** | Protected `pmm_alloc_frame` and `pmm_free_frame` bitmap operations with `irq_save` / `irq_restore`. |
| `src/lib/kprintf.c` | **MOD** | Protected formatted printing with `irq_save`/`irq_restore` and added minimum width parsing (`%3u`, `%7u`). |
| `include/drivers/keyboard.h` | **MOD** | Added scancodes for `KEY_F11` (0x57) and `KEY_F12` (0x58). |
| `src/drivers/keyboard.c` | **MOD** | Mapped make codes `0x57` and `0x58` to F11 and F12 buffer events. |
| `src/kernel/kernel.c` | **MOD** | Initialized tasking and scheduler, spawned background monitor (PID 2), wired F11/F12 hooks. |
| `src/gui/desktop.c` | **MOD** | Added live task count in System Monitor window, wired F11/F12 hooks within desktop environment. |

---

## 5. How to Build, Deploy, and Test

### Building and Launching
Run from WSL in `/home/pruthvi/myos` or via Make:
```bash
make clean
make iso
make run
```
The automated script powers off VirtualBox VM `OSProject`, copies the fresh ISO to `C:\os\myos.iso`, and starts the VM.

### Interactive Verification
1. **Boot Inspection**:
   Examine `C:\os\serial.log` to confirm initialization:
   ```text
   [TASK] Multitasking initialized: Boot Task (PID 1), Idle Task (PID 0)
   [SCHED] Round-Robin Scheduler initialized (Quantum: 100 ms)
   [OK] Background monitor task spawned (PID 2)
   ```
2. **Concurrent Heartbeats**:
   Notice regular periodic heartbeats appearing in `C:\os\serial.log` while interacting with the system:
   ```text
   [BG-MONITOR] Heartbeat #10 | Active Tasks: 3
   [UPTIME] System running: 5 seconds
   ```
3. **Task Table Dump (`[F12]`)**:
   Press **`[F12]`** (either in text console or GUI desktop) to print the active task table:
   ```text
   === Active Kernel Tasks (Total: 3) ===
    PID  State      Ticks    Stack (B)  Name
   ----------------------------------------------
      2  SLEEPING        1       8192  bg_monitor
      0  READY           0       8192  idle
      1  RUNNING      4974          0  kernel_main [*]
   ----------------------------------------------
   ```
4. **Dynamic Compute Task (`[F11]`)**:
   Press **`[F11]`** to spawn a concurrent worker task:
   ```text
   [TASK] Successfully spawned 'worker_1' with PID 3
   [WORKER 1] Background worker task (PID 3) started!
   [WORKER 1] Progress: step 1/3 complete (PID 3)
   [WORKER 1] Progress: step 2/3 complete (PID 3)
   [WORKER 1] Progress: step 3/3 complete (PID 3)
   [WORKER 1] Task finished all steps. Exiting cleanly (zombie reaper will free stack).
   ```
5. **Desktop Window Manager (`[F10]`)**:
   Press **`[F10]`** to enter the desktop. Open the "System Monitor" window, click "Refresh Metrics", and observe `Active Tasks: 3 (Preemptive)` while moving windows and typing in Notepad!

---

## 6. Viva Questions and Answers

### Q1: How does `switch_context` save and restore CPU state without using a full TSS per task?
**Answer**:
Modern 32-bit x86 kernels use software context switching rather than hardware task switching via the Task State Segment (TSS). In `switch_context`, only the callee-saved registers (`ebp`, `ebx`, `esi`, `edi`) and `EFLAGS` are pushed onto the current task's stack. The current stack pointer is saved in the task structure (`task->esp`), the new task's stack pointer is loaded into `ESP`, and the registers are popped from the new stack. When `ret` executes, it naturally pops the return address off the new stack.

### Q2: Why does each task require its own independent kernel stack?
**Answer**:
A stack stores local function variables, call return addresses, and interrupt frames. If multiple tasks shared the same stack, a preemptive interrupt or context switch would cause one task's function calls and stack frames to overwrite and corrupt another task's stack frames. An independent 8 KB stack per task ensures complete isolation of execution state.

### Q3: What is the purpose of the `task_entry_trampoline`?
**Answer**:
When a task is switched to for the very first time, it has no prior execution history to return to with `ret`. The initial stack is forged so that `ret` jumps to `task_entry_trampoline`. The trampoline executes `sti` to guarantee interrupts are enabled, calls the task's entry point function with its parameter, and if the entry function ever returns, it calls `task_exit()` to ensure the task terminates cleanly.

### Q4: Why can't a terminating task free its own stack inside `task_exit()`?
**Answer**:
If a task called `kfree(task->stack_base)` while executing `task_exit()`, it would deallocate the very memory page it is actively running on. If an interrupt struck or subsequent function calls were made, the stack would be corrupted or trigger page faults. Instead, `task_exit()` sets the task state to `TASK_ZOMBIE` and calls `schedule()`. The zombie reaper deallocates the dead task's stack only after switching to a different task's stack.

### Q5: Why must `pic_send_eoi` be issued before calling `schedule()` during timer preemption?
**Answer**:
The 8259 Programmable Interrupt Controller (PIC) sets an In-Service Register (ISR) bit when it delivers an interrupt and ignores subsequent interrupts on that line until an End-of-Interrupt (EOI) command is received. If a task switch occurred before sending EOI, the PIC would remain blocked. Other tasks would run with IRQ0 blocked, preventing future timer ticks and permanently halting preemption. Sending EOI before switching context keeps the PIC receptive to future hardware interrupts.

