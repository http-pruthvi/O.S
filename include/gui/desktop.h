#ifndef GUI_DESKTOP_H
#define GUI_DESKTOP_H

/*
 * desktop.h:
 * Milestone 5 Desktop Environment and Interactive Showcase.
 *
 * Why this is needed:
 * Launches the full graphical desktop environment, managing overlapping
 * interactive windows, widgets (buttons, labels, text input fields),
 * window dragging, z-order focus, and taskbar management.
 */

#include "lib/types.h"

/* Launches the desktop window manager environment; exits back to console on ESC */
void desktop_run(void);

/* Returns true if the graphical desktop event loop is currently active */
bool desktop_is_running(void);

#endif /* GUI_DESKTOP_H */
