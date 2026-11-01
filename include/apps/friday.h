#ifndef APPS_FRIDAY_H
#define APPS_FRIDAY_H

#include "lib/types.h"

/*
 * friday.h:
 * Friday AI Assistant Subsystem for MyOS.
 *
 * Why this is needed:
 * Friday is an AI assistant that receives text-based command packets from a
 * host Python daemon over COM2 serial (UART at 0x2F8). It parses each command,
 * dispatches OS actions (launch apps, switch themes, lock screen, query RAM),
 * renders an animated AI orb on the desktop, plays PC speaker confirmation
 * chimes, and pushes bilingual toast notifications.
 *
 * Protocol:
 *   Commands arrive as newline-terminated ASCII strings on COM2.
 *   Responses (for queries) are sent back over COM2 in the same format.
 *   Example: "CMD:LAUNCH:CALC\n" -> launches the calculator app.
 */

/* Friday operational states (displayed under the animated orb) */
typedef enum {
    FRIDAY_IDLE = 0,   /* Awaiting commands, blue pulse           */
    FRIDAY_LISTENING,  /* COM2 data incoming, cyan pulse          */
    FRIDAY_THINKING,   /* Parsing / preparing action, violet pulse */
    FRIDAY_EXECUTING   /* Action dispatched, green pulse           */
} friday_state_t;

/*
 * friday_init:
 * Initializes the Friday AI subsystem.
 * Sets up COM2 IRQ-driven serial, marks Friday as online, logs to COM1,
 * and pushes a startup notification toast.
 */
void friday_init(void);

/*
 * friday_tick:
 * Called every desktop tick (~100 Hz) to poll COM2 for complete command lines,
 * parse and dispatch commands, advance the orb animation phase, manage the
 * chime state machine, and decay the EXECUTING visual countdown.
 */
void friday_tick(void);

/*
 * friday_is_online:
 * Returns true if Friday has been initialized and is ready to receive commands.
 */
bool friday_is_online(void);

/*
 * friday_get_state:
 * Returns the current operational state (IDLE, LISTENING, THINKING, EXECUTING).
 */
friday_state_t friday_get_state(void);

/*
 * friday_render_orb:
 * Draws the animated AI orb directly into the provided back buffer.
 * The orb consists of pulsing concentric circles in the lower-right desktop
 * area with a state label underneath.
 *
 * Parameters:
 *   backbuf  - Pointer to the 32-bit ARGB pixel back buffer.
 *   fb_width - Framebuffer width in pixels (typically 1024).
 *   fb_height - Framebuffer height in pixels (typically 768).
 */
void friday_render_orb(uint32_t* backbuf, uint32_t fb_width, uint32_t fb_height);

#endif /* APPS_FRIDAY_H */
