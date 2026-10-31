#ifndef APPS_SETTINGS_H
#define APPS_SETTINGS_H

#include "lib/types.h"

/*
 * settings.h:
 * Modern Tabbed System Settings Application for MyOS.
 *
 * Why this is needed:
 * Implements a comprehensive control center covering:
 * 1. Display & Personalization (Themes, Wallpapers, HUD toggle).
 * 2. Sound & Audio (PC speaker frequency tones and volume test).
 * 3. Users & Security (User accounts, PIN configuration, kernel shields).
 * 4. System & Power (Hardware specifications, reboot, safe shutdown).
 */

/* Opens or focuses the System Settings application window */
void app_settings_open(void);

#endif /* APPS_SETTINGS_H */
