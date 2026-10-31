#ifndef APPS_TASKMGR_H
#define APPS_TASKMGR_H

/*
 * taskmgr.h:
 * Graphical Task Manager Application.
 *
 * Why this is needed:
 * Provides real-time visibility into running kernel tasks, execution states,
 * CPU runtime accounting, and physical/heap memory allocation.
 * Allows interactive process inspection and process termination (kill).
 */

/* Opens the graphical Task Manager window or brings it to the foreground */
void app_taskmgr_open(void);

#endif /* APPS_TASKMGR_H */

