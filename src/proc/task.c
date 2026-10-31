#include "proc/task.h"
#include "proc/scheduler.h"
#include "arch/lock.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/kprintf.h"
#include "drivers/timer.h"

/* Initial EFLAGS value for new tasks: IF=1 (bit 9), reserved bit 1=1 */
#define TASK_INITIAL_EFLAGS 0x00000202

static uint32_t next_pid = 0;
static task_t* current_task = NULL;
static task_t* idle_task = NULL;
static task_t* boot_task = NULL;
static task_t* all_tasks_head = NULL;
static uint32_t total_tasks = 0;

/*
 * idle_task_entry:
 * Executes continuously when no other tasks are ready to run.
 * Uses 'sti; hlt' to halt CPU execution until the next hardware interrupt arrives,
 * saving CPU cycles and VM host processor usage.
 */
static void idle_task_entry(void* arg) {
    (void)arg;
    while (1) {
        /*
         * The x86 architecture guarantees that STI takes effect only AFTER the
         * instruction following it completes. Placing 'sti' directly before 'hlt'
         * guarantees an atomic transition into the halted state with interrupts enabled,
         * preventing any race where an interrupt occurs right before hlt.
         */
        __asm__ volatile ("sti\n\thlt" : : : "memory");
    }
}

/*
 * task_init:
 * Initializes the multitasking subsystem.
 * Converts the active kernel boot thread into PID 1 ("kernel_main"),
 * and initializes the background PID 0 ("idle") task.
 */
void task_init(void) {
    uint32_t flags = irq_save();

    /* 1. Allocate and configure the initial boot task (PID 1) */
    boot_task = (task_t*)kmalloc(sizeof(task_t));
    if (!boot_task) {
        irq_restore(flags);
        kprintf("[TASK] Fatal: Failed to allocate boot task!\n");
        return;
    }

    memset(boot_task, 0, sizeof(task_t));
    boot_task->id = 1;
    strncpy(boot_task->name, "kernel_main", TASK_NAME_MAX_LEN - 1);
    boot_task->state = TASK_RUNNING;
    boot_task->stack_base = NULL;      /* Uses original boot stack from boot.asm */
    boot_task->stack_size = 0;
    boot_task->is_boot_task = true;
    boot_task->time_slice = TASK_DEFAULT_TIME_SLICE;
    boot_task->next = NULL;
    boot_task->all_next = NULL;

    current_task = boot_task;
    all_tasks_head = boot_task;
    total_tasks = 1;
    next_pid = 2; /* PID 0 is idle, PID 1 is boot task, subsequent start from 2 */

    /* 2. Initialize the idle task (PID 0) */
    idle_task = (task_t*)kmalloc(sizeof(task_t));
    if (!idle_task) {
        irq_restore(flags);
        kprintf("[TASK] Fatal: Failed to allocate idle task!\n");
        return;
    }

    memset(idle_task, 0, sizeof(task_t));
    idle_task->id = 0;
    strncpy(idle_task->name, "idle", TASK_NAME_MAX_LEN - 1);
    idle_task->state = TASK_READY;
    idle_task->is_boot_task = false;
    idle_task->time_slice = TASK_DEFAULT_TIME_SLICE;
    idle_task->next = NULL;

    /* Allocate dedicated 8 KB stack for idle task */
    uint8_t* idle_stack = (uint8_t*)kmalloc(TASK_STACK_SIZE);
    idle_task->stack_base = idle_stack;
    idle_task->stack_size = TASK_STACK_SIZE;

    /* Forge idle stack */
    uintptr_t stack_top = (uintptr_t)(idle_stack + TASK_STACK_SIZE);
    stack_top &= ~0xF;
    uint32_t* sp = (uint32_t*)stack_top;

    *(--sp) = 0;                                 /* Argument: NULL */
    *(--sp) = (uint32_t)idle_task_entry;         /* Entry function */
    *(--sp) = (uint32_t)task_entry_trampoline;   /* Return address for switch_context 'ret' */
    *(--sp) = TASK_INITIAL_EFLAGS;               /* Initial EFLAGS (IF=1) */
    *(--sp) = 0;                                 /* Initial EBP */
    *(--sp) = 0;                                 /* Initial EDI */
    *(--sp) = 0;                                 /* Initial ESI */
    *(--sp) = 0;                                 /* Initial EBX */

    idle_task->esp = (uint32_t)sp;

    /* Add idle task to global tasks list */
    idle_task->all_next = all_tasks_head;
    all_tasks_head = idle_task;
    total_tasks++;

    irq_restore(flags);
    kprintf("[TASK] Multitasking initialized: Boot Task (PID 1), Idle Task (PID 0)\n");
}

/*
 * task_create:
 * Creates and initializes a new kernel task with an 8 KB stack.
 *
 * Parameters:
 *   name  - Human-readable name (up to 31 chars).
 *   entry - Task entry function pointer.
 *   arg   - Arbitrary argument pointer passed to the entry function.
 *
 * Return:
 *   Pointer to new task_t, or NULL on allocation failure.
 */
task_t* task_create(const char* name, task_entry_t entry, void* arg) {
    if (!entry) {
        return NULL;
    }

    uint32_t flags = irq_save();

    /* 1. Allocate Task Control Block */
    task_t* task = (task_t*)kmalloc(sizeof(task_t));
    if (!task) {
        irq_restore(flags);
        kprintf("[TASK] Error: Failed to allocate task struct for '%s'\n", name ? name : "unnamed");
        return NULL;
    }

    memset(task, 0, sizeof(task_t));

    /* 2. Allocate 8 KB stack from kernel heap */
    uint8_t* stack_base = (uint8_t*)kmalloc(TASK_STACK_SIZE);
    if (!stack_base) {
        kfree(task);
        irq_restore(flags);
        kprintf("[TASK] Error: Failed to allocate 8 KB stack for '%s'\n", name ? name : "unnamed");
        return NULL;
    }

    /* 3. Configure TCB metadata */
    task->id = next_pid++;
    if (name) {
        strncpy(task->name, name, TASK_NAME_MAX_LEN - 1);
        task->name[TASK_NAME_MAX_LEN - 1] = '\0';
    } else {
        strncpy(task->name, "unnamed", TASK_NAME_MAX_LEN - 1);
    }

    task->state = TASK_READY;
    task->stack_base = stack_base;
    task->stack_size = TASK_STACK_SIZE;
    task->is_boot_task = false;
    task->wake_tick = 0;
    task->ticks_run = 0;
    task->time_slice = TASK_DEFAULT_TIME_SLICE;
    task->next = NULL;

    /* 4. Forge stack for switch_context and trampoline */
    uintptr_t stack_top = (uintptr_t)(stack_base + TASK_STACK_SIZE);
    stack_top &= ~0xF; /* 16-byte align */
    uint32_t* sp = (uint32_t*)stack_top;

    *(--sp) = (uint32_t)arg;                   /* [esp + 4] inside entry: first argument */
    *(--sp) = (uint32_t)entry;                 /* popped into EAX by task_entry_trampoline */
    *(--sp) = (uint32_t)task_entry_trampoline; /* return address popped by 'ret' in switch_context */
    *(--sp) = TASK_INITIAL_EFLAGS;             /* popped by 'popf' (IF=1) */
    *(--sp) = 0;                               /* popped by 'pop ebp' */
    *(--sp) = 0;                               /* popped by 'pop edi' */
    *(--sp) = 0;                               /* popped by 'pop esi' */
    *(--sp) = 0;                               /* popped by 'pop ebx' */

    task->esp = (uint32_t)sp;

    /* 5. Link into global all-tasks list */
    task->all_next = all_tasks_head;
    all_tasks_head = task;
    total_tasks++;

    /* 6. Enqueue into scheduler ready queue */
    scheduler_add_ready(task);

    irq_restore(flags);
    return task;
}

/*
 * task_exit:
 * Invoked when a task terminates. Transitions task to TASK_ZOMBIE
 * and dispatches the scheduler. Deallocation is performed by the zombie reaper.
 */
void task_exit(void) {
    irq_disable();

    if (current_task) {
        current_task->state = TASK_ZOMBIE;
    }

    schedule();

    /* Unreachable: the scheduler never switches back to a zombie task */
    while (1) {
        __asm__ volatile ("cli\n\thlt");
    }
}

/*
 * task_sleep_ms:
 * Puts the current task to sleep for the specified duration.
 */
void task_sleep_ms(uint32_t ms) {
    if (ms == 0) {
        task_yield();
        return;
    }

    uint32_t flags = irq_save();

    /* 1 tick = 10 ms at 100 Hz; round up to avoid waking too early */
    uint32_t ticks_to_sleep = (ms + 9) / 10;
    if (ticks_to_sleep == 0) {
        ticks_to_sleep = 1;
    }

    current_task->wake_tick = timer_get_ticks() + ticks_to_sleep;
    current_task->state = TASK_SLEEPING;

    /* schedule() switches to another task; when woken up, it returns here */
    schedule();

    irq_restore(flags);
}

/*
 * task_yield:
 * Cooperatively yields remaining quantum to another ready task.
 */
void task_yield(void) {
    uint32_t flags = irq_save();
    schedule();
    irq_restore(flags);
}

/*
 * task_get_current:
 * Returns the currently running task.
 */
task_t* task_get_current(void) {
    return current_task;
}

/*
 * task_get_idle:
 * Returns pointer to the background idle task (PID 0).
 */
task_t* task_get_idle(void) {
    return idle_task;
}

/*
 * task_set_current:
 * Updates the current task pointer.
 */
void task_set_current(task_t* task) {
    current_task = task;
}

/*
 * task_get_all:
 * Returns the global linked list of all active tasks.
 */
task_t* task_get_all(void) {
    return all_tasks_head;
}

/*
 * task_unlink:
 * Safely unlinks a task from the global all-tasks list.
 */
void task_unlink(task_t* task) {
    if (!task) return;

    task_t* prev = NULL;
    task_t* curr = all_tasks_head;

    while (curr) {
        if (curr == task) {
            if (prev) {
                prev->all_next = curr->all_next;
            } else {
                all_tasks_head = curr->all_next;
            }
            curr->all_next = NULL;
            if (total_tasks > 0) {
                total_tasks--;
            }
            break;
        }
        prev = curr;
        curr = curr->all_next;
    }
}

/*
 * task_get_count:
 * Returns total number of tasks in the system.
 */
uint32_t task_get_count(void) {
    return total_tasks;
}

/*
 * task_by_id:
 * Searches for an active task by PID.
 */
task_t* task_by_id(uint32_t pid) {
    uint32_t flags = irq_save();
    task_t* curr = all_tasks_head;
    while (curr) {
        if (curr->id == pid) {
            irq_restore(flags);
            return curr;
        }
        curr = curr->all_next;
    }
    irq_restore(flags);
    return NULL;
}

/*
 * task_kill:
 * Terminates a task by PID. Protected system tasks (PID 0 and 1) cannot be killed.
 */
bool task_kill(uint32_t pid) {
    if (pid == 0 || pid == 1) {
        kprintf("[TASK] Warning: Cannot kill protected system task %u!\n", pid);
        return false;
    }

    uint32_t flags = irq_save();
    task_t* target = NULL;
    task_t* curr = all_tasks_head;
    while (curr) {
        if (curr->id == pid) {
            target = curr;
            break;
        }
        curr = curr->all_next;
    }

    if (!target) {
        irq_restore(flags);
        return false;
    }

    /* Remove from ready queue if pending */
    if (target->state == TASK_READY) {
        scheduler_remove_ready(target);
    }

    target->state = TASK_ZOMBIE;
    kprintf("[TASK] Task %u ('%s') terminated\n", target->id, target->name);

    if (target == current_task) {
        irq_restore(flags);
        schedule();
        return true;
    }

    irq_restore(flags);
    return true;
}

