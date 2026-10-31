#include "proc/scheduler.h"
#include "proc/task.h"
#include "arch/lock.h"
#include "mm/heap.h"
#include "lib/kprintf.h"
#include "drivers/timer.h"

static task_t* ready_queue_head = NULL;
static task_t* ready_queue_tail = NULL;

/*
 * scheduler_init:
 * Initializes scheduler queues.
 */
void scheduler_init(void) {
    ready_queue_head = NULL;
    ready_queue_tail = NULL;
    kprintf("[SCHED] Round-Robin Scheduler initialized (Quantum: %u ms)\n",
            TASK_DEFAULT_TIME_SLICE * 10);
}

/*
 * scheduler_add_ready:
 * Appends a runnable task to the tail of the FIFO ready queue.
 */
void scheduler_add_ready(task_t* task) {
    if (!task || task->id == 0) {
        /* PID 0 (idle task) is never enqueued into the ready queue */
        return;
    }

    uint32_t flags = irq_save();

    /* Guard against duplicate enqueue */
    task_t* curr = ready_queue_head;
    while (curr) {
        if (curr == task) {
            irq_restore(flags);
            return;
        }
        curr = curr->next;
    }

    task->next = NULL;
    if (!ready_queue_head) {
        ready_queue_head = task;
        ready_queue_tail = task;
    } else {
        ready_queue_tail->next = task;
        ready_queue_tail = task;
    }

    irq_restore(flags);
}

/*
 * scheduler_remove_ready:
 * Removes a task from the ready queue if present.
 */
void scheduler_remove_ready(task_t* task) {
    if (!task) {
        return;
    }

    uint32_t flags = irq_save();

    task_t* prev = NULL;
    task_t* curr = ready_queue_head;

    while (curr) {
        if (curr == task) {
            if (prev) {
                prev->next = curr->next;
            } else {
                ready_queue_head = curr->next;
            }
            if (curr == ready_queue_tail) {
                ready_queue_tail = prev;
            }
            curr->next = NULL;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    irq_restore(flags);
}

/*
 * scheduler_has_ready:
 * Checks if any task is waiting in the ready queue.
 */
bool scheduler_has_ready(void) {
    return (ready_queue_head != NULL);
}

/*
 * scheduler_pick_next:
 * Dequeues and returns the next task from the head of the ready queue.
 */
task_t* scheduler_pick_next(void) {
    if (!ready_queue_head) {
        return NULL;
    }

    task_t* task = ready_queue_head;
    ready_queue_head = ready_queue_head->next;
    if (!ready_queue_head) {
        ready_queue_tail = NULL;
    }
    task->next = NULL;
    return task;
}

/*
 * scheduler_cleanup_zombies:
 * Scans the global task list and deallocates memory for terminated (TASK_ZOMBIE) tasks.
 * Crucially, an active task must never free its own stack while running on it,
 * so zombie stacks are safely collected here when another task is active.
 */
void scheduler_cleanup_zombies(void) {
    uint32_t flags = irq_save();

    task_t* curr = task_get_all();
    task_t* active = task_get_current();

    while (curr) {
        task_t* next = curr->all_next;

        if (curr->state == TASK_ZOMBIE && curr != active) {
            /* Safely unlink from global task list */
            task_unlink(curr);

            /* Free task stack if dynamically allocated */
            if (!curr->is_boot_task && curr->stack_base) {
                kfree(curr->stack_base);
                curr->stack_base = NULL;
            }

            /* Free Task Control Block */
            kfree(curr);
        }

        curr = next;
    }

    irq_restore(flags);
}

/*
 * schedule:
 * Core context switch and scheduling dispatcher.
 */
void schedule(void) {
    uint32_t flags = irq_save();

    task_t* current = task_get_current();
    if (!current) {
        irq_restore(flags);
        return;
    }

    /* 1. If currently running task is still runnable, move back to ready queue */
    if (current->state == TASK_RUNNING) {
        current->state = TASK_READY;
        current->time_slice = TASK_DEFAULT_TIME_SLICE;
        if (current->id != 0) {
            scheduler_add_ready(current);
        }
    }

    /* 2. Pick next runnable task from ready queue (or fall back to idle task) */
    task_t* next = scheduler_pick_next();
    if (!next) {
        next = task_get_idle();
    }

    /* 3. If next task is identical to current, keep running */
    if (next == current) {
        current->state = TASK_RUNNING;
        irq_restore(flags);
        return;
    }

    /* 4. Switch context to the new task */
    next->state = TASK_RUNNING;
    task_set_current(next);

    switch_context(&current->esp, next->esp);

    /* 5. Upon resuming, reap any dead zombie tasks and restore interrupt state */
    scheduler_cleanup_zombies();

    irq_restore(flags);
}

/*
 * scheduler_tick:
 * Invoked on every PIT timer interrupt (100 Hz).
 */
void scheduler_tick(void) {
    uint32_t current_tick = timer_get_ticks();

    /* 1. Check sleeping tasks and wake up those whose target tick has arrived */
    task_t* task = task_get_all();
    while (task) {
        if (task->state == TASK_SLEEPING) {
            if ((int32_t)(current_tick - task->wake_tick) >= 0) {
                task->state = TASK_READY;
                task->time_slice = TASK_DEFAULT_TIME_SLICE;
                scheduler_add_ready(task);
            }
        }
        task = task->all_next;
    }

    /* 2. Track CPU runtime for currently running task */
    task_t* current = task_get_current();
    if (!current) {
        return;
    }

    current->ticks_run++;

    /* 3. Decrement time slice and preempt if quantum has expired */
    if (current->time_slice > 0) {
        current->time_slice--;
    }

    /* If time slice expired and other tasks are waiting, preempt */
    if (current->time_slice == 0 && scheduler_has_ready()) {
        schedule();
    }
}

/*
 * scheduler_dump_tasks:
 * Dumps a formatted task table to COM1 serial and display for debugging and ps inspection.
 */
void scheduler_dump_tasks(void) {
    uint32_t flags = irq_save();

    kprintf("\n=== Active Kernel Tasks (Total: %u) ===\n", task_get_count());
    kprintf(" PID  State      Ticks    Stack (B)  Name\n");
    kprintf("----------------------------------------------\n");

    task_t* curr = task_get_all();
    while (curr) {
        const char* state_str = "UNKNOWN ";
        switch (curr->state) {
            case TASK_EMBRYO:   state_str = "EMBRYO  "; break;
            case TASK_READY:    state_str = "READY   "; break;
            case TASK_RUNNING:  state_str = "RUNNING "; break;
            case TASK_SLEEPING: state_str = "SLEEPING"; break;
            case TASK_BLOCKED:  state_str = "BLOCKED "; break;
            case TASK_ZOMBIE:   state_str = "ZOMBIE  "; break;
        }

        kprintf(" %3u  %s  %7u  %9u  %s%s\n",
                curr->id,
                state_str,
                curr->ticks_run,
                curr->stack_size,
                curr->name,
                (curr == task_get_current()) ? " [*]" : "");

        curr = curr->all_next;
    }
    kprintf("----------------------------------------------\n\n");

    irq_restore(flags);
}
