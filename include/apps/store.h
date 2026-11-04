#ifndef APPS_STORE_H
#define APPS_STORE_H

#include "lib/types.h"

/*
 * store.h:
 * Web App Store & Package Manager for MyOS.
 *
 * Allows browsing, searching, and installing applications from the web repository
 * or custom URLs into the RamFS (/apps and /bin), with live progress bars and
 * direct application launching.
 */

/* Initializes the Web App Store state */
void app_store_init(void);

/* Opens the Web App Store window */
void app_store_open(void);

/* Installs an app by package ID from the web catalog ('browser', 'hex', 'notes', etc.) */
bool app_store_install_by_id(const char* pkg_id);

/* Installs a custom web app from a web URL */
bool app_store_install_custom_url(const char* url, const char* name);

/* Checks if an app is installed */
bool app_store_is_installed(const char* pkg_id);

/* Launches an installed web app by package ID */
bool app_store_launch(const char* pkg_id);

/* Inspects a given file in Hex Editor Pro */
void launch_hex_editor_file(const char* filepath);

#endif /* APPS_STORE_H */
