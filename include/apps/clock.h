#ifndef APPS_CLOCK_H
#define APPS_CLOCK_H

#include "lib/types.h"

/*
 * clock.h:
 * Milestone 6 Real-Time Clock & Monthly Calendar Application.
 *
 * Why this is needed:
 * Displays a live updating digital clock (driven by timer ticks),
 * system uptime counter, and an interactive monthly calendar with
 * month navigation and day highlighting.
 */

/* Launches or brings to front the Clock application window */
void app_clock_open(void);

/* Periodically called by desktop event loop when timer tick occurs */
void app_clock_update(void);

#endif /* APPS_CLOCK_H */
