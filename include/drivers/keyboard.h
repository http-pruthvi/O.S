#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include "lib/types.h"

/*
 * keyboard.h:
 * PS/2 Keyboard Driver (Scancode Set 1).
 *
 * Why this is needed:
 * Translates raw hardware scancodes from port 0x60 into ASCII characters,
 * manages modifier states (Shift, Caps Lock), and buffers keystrokes for
 * the interactive console and shell.
 */

/* PS/2 Controller I/O Ports */
#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

/* Special Non-ASCII Function Keys */
#define KEY_F1 0x81
#define KEY_F2 0x82
#define KEY_F3 0x83
#define KEY_F4 0x84
#define KEY_F5 0x85
#define KEY_F6 0x86
#define KEY_F7 0x87
#define KEY_F8 0x42  /* PS/2 Scancode Set 1 make code for F8 */

/* Initializes PS/2 keyboard driver, unmasks IRQ 1 */
void keyboard_init(void);

/* Checks if at least one character is waiting in the input buffer */
bool keyboard_has_char(void);

/* Blocks until a character is available and returns it (uses 'hlt' loop) */
char keyboard_getchar(void);

#endif /* DRIVERS_KEYBOARD_H */
