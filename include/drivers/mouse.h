#ifndef DRIVERS_MOUSE_H
#define DRIVERS_MOUSE_H

#include "lib/types.h"

/*
 * mouse.h:
 * PS/2 Mouse Driver (Auxiliary Device on 8042 Controller, IRQ 12).
 *
 * Why this is needed:
 * Translates hardware mouse interrupts and 3-byte movement packets into
 * relative (dx, dy) motion and button press/release events for the GUI desktop
 * and paint applications.
 */

/* PS/2 Controller I/O Ports */
#define MOUSE_DATA_PORT   0x60
#define MOUSE_STATUS_PORT 0x64

/* Mouse Button Bitmasks */
#define MOUSE_BTN_LEFT    (1 << 0)
#define MOUSE_BTN_RIGHT   (1 << 1)
#define MOUSE_BTN_MIDDLE  (1 << 2)

/* Mouse Event Structure */
typedef struct {
    int32_t dx;         /* Relative horizontal movement (+ = right, - = left) */
    int32_t dy;         /* Relative vertical movement (+ = down, - = up) */
    uint8_t buttons;    /* Current button state bitmask (MOUSE_BTN_*) */
    uint8_t raw[3];     /* Raw 3-byte hardware packet for diagnostics */
} mouse_event_t;

/* Initializes PS/2 mouse, configures 8042 controller, unmasks IRQ 12 & IRQ 2 */
bool mouse_init(void);

/* Returns true if an unconsumed mouse event is waiting in the ring buffer */
bool mouse_has_event(void);

/* Dequeues next mouse event from the buffer (logs first 5 packets to serial) */
bool mouse_poll_event(mouse_event_t* event);

/* Returns the current clamped screen coordinates of the mouse */
void mouse_get_position(int* x, int* y);

/* Sets the tracked position of the mouse */
void mouse_set_position(int x, int y);

#endif /* DRIVERS_MOUSE_H */
