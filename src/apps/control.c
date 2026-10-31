#include "apps/control.h"
#include "apps/settings.h"

/*
 * Legacy Control Panel entry point:
 * Forwards to the modern System Settings & Control Center.
 */
void app_control_open(void) {
    app_settings_open();
}
