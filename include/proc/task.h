#ifndef PROC_TASK_H
#define PROC_TASK_H

#include "lib/types.h"

/*
 * task.h:
 * Kernel Task Data Structures and Management Interface.
 *
 * Why this is needed:
 * Milestone 7 brings multitasking to the operating system.
 * A task represents an independent thread of execution with its own stack,
 * processor register context, and scheduling state.
 * In this single address-space kernel model, all tasks share the identity-mapped
 * kernel page directory and run in Ring 0.
 */

#define TASK_NAME_MAX_LEN 32
#define TASK_STACK_SIZE   8192   /* 8 KB per task stack */
#define TASK_DEFAULT_TIME_SLICE 10 /* 10 timer ticks = 100 ms at 100 Hz */

/* Task execution states */
typedef enum task_state {
    TASK_EMBRYO,    /* Task is currently being allocated and set up */
    TASK_READY,     /* Task is runnable and waiting in the ready queue */
    TASK_RUNNING,   /* Task is currently executing on the CPU */
    TASK_SLEEPING,  /* Task is suspended until a timer tick target */
    TASK_BLOCKED,   /* Task is waiting on a lock or event */
    TASK_ZOMBIE     /* Task has finished execution; waiting for stack deallocation */
} task_state_t;

/* Function pointer signature for task entry points */
typedef void (*task_entry_t)(void* arg);

/*
 * Task Control Block (TCB)
 */
typedef struct task {
    uint32_t esp;                   /* Saved stack pointer when task is suspended (offset 0) */
    uint32_t id;                    /* Unique Task / Process ID (PID) */
    char name[TASK_NAME_MAX_LEN];   /* Human-readable task name */
    task_state_t state;             /* Current lifecycle state */

    uint8_t* stack_base;            /* Base pointer of allocated 8 KB stack */
    uint32_t stack_size;            /* Total size of the stack */
    bool is_boot_task;              /* True if this task uses the initial boot stack (do not kfree) */

    uint32_t wake_tick;             /* Target timer tick count for waking up if TASK_SLEEPING */
    uint32_t ticks_run;             /* Total cumulative timer ticks this task has spent running */
    uint32_t time_slice;            /* Remaining ticks in the current quantum */

    struct task* next;              /* Next pointer in ready queue or sleep queue */
    struct task* all_next;          /* Next pointer in global list of all active tasks */
} task_t;

/* Initializes task management subsystem (creates PID 0 Idle Task and PID 1 Boot Task) */
void task_init(void);

/* Creates a new kernel task with an 8 KB stack, enqueuing it in the ready queue */
task_t* task_create(const char* name, task_entry_t entry, void* arg);

/* Terminates the current task, transition to TASK_ZOMBIE, and invokes the scheduler */
void task_exit(void);

/* Puts the current task to sleep for a specified duration in milliseconds */
void task_sleep_ms(uint32_t ms);

/* Voluntarily yields remaining time slice to another ready task */
void task_yield(void);

/* Returns pointer to the currently executing task */
task_t* task_get_current(void);

/* Updates current task pointer (used during context switches) */
void task_set_current(task_t* task);

/* Returns head of the global linked list containing all active tasks */
task_t* task_get_all(void);

/* Returns the total number of tasks in the system */
uint32_t task_get_count(void);

#endif /* PROC_TASK_H */
