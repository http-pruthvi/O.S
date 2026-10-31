#ifndef PROC_SCHEDULER_H
#define PROC_SCHEDULER_H

#include "proc/task.h"

/*
 * scheduler.h:
 * Preemptive Round-Robin Scheduler Interface.
 *
 * Why this is needed:
 * Manages CPU time-sharing across multiple kernel tasks.
 * Maintains the ready queue of runnable tasks, a sleeping queue of tasks
 * waiting on timer ticks, and cleans up terminated zombie tasks.
 */

/* External assembly prototypes from switch.asm */
extern void switch_context(uint32_t** old_esp, uint32_t* new_esp);
extern void task_entry_trampoline(void);

/* Initializes scheduler queues and state */
void scheduler_init(void);

/* Adds a task to the tail of the ready queue */
void scheduler_add_ready(task_t* task);

/* Removes a task from the ready queue */
void scheduler_remove_ready(task_t* task);

/*
 * schedule:
 * Core scheduler dispatch routine.
 * Picks the next runnable task and performs context switch if different from current.
 */
void schedule(void);

/*
 * scheduler_tick:
 * Invoked by PIT timer IRQ0 handler at 100 Hz.
 * Updates the current task's time-slice accounting, wakes up sleeping tasks whose
 * target tick has arrived, and triggers preemption if the quantum expires.
 */
void scheduler_tick(void);

/*
 * scheduler_cleanup_zombies:
 * Deallocates stacks and Task Control Blocks of tasks in the TASK_ZOMBIE state.
 */
void scheduler_cleanup_zombies(void);

/* Dumps the list of all active tasks and their runtime statistics to serial */
void scheduler_dump_tasks(void);

#endif /* PROC_SCHEDULER_H */
