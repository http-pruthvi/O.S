#ifndef APPS_TERMINAL_H
#define APPS_TERMINAL_H

/*
 * terminal.h:
 * Graphical Terminal Emulator and Interactive Command-Line Shell.
 *
 * Why this is needed:
 * Provides a windowed CLI interface inside the desktop GUI.
 * Users can execute filesystem commands (ls, cat, write, rm), process management
 * commands (ps, kill), system diagnostics (mem, uptime), and app launchers (calc, editor).
 */

/* Opens the graphical Terminal window or brings it to the foreground */
void app_terminal_open(void);

#endif /* APPS_TERMINAL_H */

