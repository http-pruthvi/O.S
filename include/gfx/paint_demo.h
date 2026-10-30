#ifndef GFX_PAINT_DEMO_H
#define GFX_PAINT_DEMO_H

/*
 * paint_demo.h:
 * Interactive Mouse Paint Demo Application (Milestone 4 GUI Foundation).
 *
 * Why this is needed:
 * Proves the full graphical foundation of Milestone 4:
 * 1. Linear framebuffer 1024x768x32 rendering.
 * 2. 3 MB off-screen back buffer with dynamic heap expansion.
 * 3. 2D graphics primitives (Bresenham line, filled rectangles, font rendering).
 * 4. Hardware PS/2 mouse driver with IRQ 12 event streaming.
 * 5. Software mouse cursor with tear-free save-under and dirty rectangle flushes.
 * 6. Clean exit back to the framebuffer console on ESC or Exit button.
 */

/* Launches the interactive paint demo; returns to console when ESC is pressed */
void paint_demo_run(void);

#endif /* GFX_PAINT_DEMO_H */
