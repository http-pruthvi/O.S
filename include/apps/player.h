#ifndef APPS_PLAYER_H
#define APPS_PLAYER_H

#include "lib/types.h"
#include "gui/window.h"

/*
 * player.h:
 * Milestone 10 Media Player & Chiptune Synthesizer Application.
 *
 * Why this is needed:
 * Provides an authentic retro Winamp / Windows Media Player inspired audio
 * workstation for MyOS. Generates multi-octave square wave chiptune melodies
 * using the x86 8254 Programmable Interval Timer (PIT Channel 2) and PC speaker.
 * Features an animated 8-band frequency spectrum visualizer with peak hold &
 * fall-off decay, track metadata HUD card, playback controls, and a retro
 * gaming and classical music playlist.
 */

/*
 * PC Speaker Driver Helper Functions
 */

/* Plays a continuous square-wave tone at the given frequency in Hertz */
void speaker_play_tone(uint32_t freq_hz);

/* Safely silences the PC speaker by gating PIT Channel 2 and data enable */
void speaker_stop(void);

/*
 * Media Player Application Lifecycle & Event API
 */

/*
 * player_app_create:
 * Creates and initializes the Media Player window at specified screen coordinates.
 *
 * Parameters:
 *   x, y - Initial top-left screen position for the window.
 *
 * Return:
 *   Pointer to the created window_t object, or existing instance if already open.
 */
window_t* player_app_create(int32_t x, int32_t y);

/*
 * app_player_open:
 * Opens or focuses the Media Player application window at default desktop coordinates.
 */
void app_player_open(void);

/*
 * player_tick:
 * Periodically called (e.g. from the desktop event loop or timer callback)
 * to advance musical note synthesis, handle note releases/rests, and
 * animate the 8-band frequency spectrum visualizer with peak fall-off.
 */
void player_tick(void);

#endif /* APPS_PLAYER_H */
