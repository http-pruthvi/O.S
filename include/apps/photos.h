#ifndef APPS_PHOTOS_H
#define APPS_PHOTOS_H

#include "lib/types.h"
#include "gui/window.h"

/*
 * photos.h:
 * Graphical Photos & Image Viewer Application for MyOS.
 *
 * Why this is needed:
 * Provides a dedicated desktop photo viewer with support for Netpbm PPM
 * (P3 ASCII and P6 binary) images, real-time image filters (grayscale and inversion),
 * multiple zoom factors (1x to 6x), and interactive gallery navigation.
 * Integrates directly with the Virtual File System (RamFS) and provides pre-seeded
 * demo imagery.
 */

/*
 * photos_app_create:
 * Creates and initializes the Photos application window at screen position (x, y).
 * If the application is already open, brings the existing window to the front.
 * Returns pointer to the created or focused window_t instance.
 */
window_t* photos_app_create(int32_t x, int32_t y);

/*
 * photos_preseed_images:
 * Pre-seeds sample PPM image files in the RamFS root filesystem ("/sample.ppm"
 * and "/avatar.ppm") if they are not already present.
 */
void photos_preseed_images(void);

/*
 * app_photos_open:
 * Convenience launcher that opens or focuses the Photos application window
 * at standard default desktop coordinates (x = 140, y = 100).
 */
void app_photos_open(void);

#endif /* APPS_PHOTOS_H */
