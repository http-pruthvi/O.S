#ifndef APPS_CONTROL_H
#define APPS_CONTROL_H

/*
 * control.h:
 * Windows-style Control Panel & Personalization Settings.
 *
 * Why this is needed:
 * Allows user to dynamically configure desktop themes, wallpaper palettes,
 * inspect kernel security status, and view hardware specifications.
 */

/* Opens or focuses the Control Panel application window */
void app_control_open(void);

#endif /* APPS_CONTROL_H */
