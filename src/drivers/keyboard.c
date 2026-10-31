#include "drivers/keyboard.h"
#include "arch/irq.h"
#include "arch/pic.h"
#include "lib/io.h"

#define KEYBOARD_BUFFER_SIZE 256

/* Circular FIFO keystroke buffer */
static volatile char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile size_t buffer_head = 0;
static volatile size_t buffer_tail = 0;

/* Modifier and prefix states */
static volatile bool shift_active = false;
static volatile bool caps_lock_active = false;
static volatile bool extended_prefix = false;

/* Scancode Set 1 (Unshifted) lookup table */
static const char scancode_unshifted[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

/* Scancode Set 1 (Shifted) lookup table */
static const char scancode_shifted[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

/*
 * buffer_put:
 * Enqueues a character into the circular buffer. Drops characters if full.
 */
static void buffer_put(char c) {
    size_t next_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next_head != buffer_tail) {
        keyboard_buffer[buffer_head] = c;
        buffer_head = next_head;
    }
}

/*
 * keyboard_handler:
 * IRQ 1 interrupt service routine.
 * Reads scancodes from port 0x60 and translates to ASCII or function keys.
 */
static void keyboard_handler(registers_t* regs) {
    (void)regs; /* Unused */

    /* Check status register (port 0x64):
     * Bit 0 must be 1 (output buffer has data).
     * Bit 5 must be 0 (data is from keyboard, not aux/mouse device).
     */
    uint8_t status = inb(KEYBOARD_STATUS_PORT);
    if ((status & 0x01) == 0 || (status & 0x20) != 0) {
        return;
    }

    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    /* 1. Handle extended key prefix (0xE0) */
    if (scancode == 0xE0) {
        extended_prefix = true;
        return;
    }

    if (extended_prefix) {
        /* Ignore extended scancodes (arrows, insert, delete, etc.) */
        extended_prefix = false;
        return;
    }

    /* 2. Key Release (Break codes: bit 7 is set) */
    if (scancode & 0x80) {
        uint8_t released_code = scancode & 0x7F;
        if (released_code == 0x2A || released_code == 0x36) {
            /* Left Shift (0x2A) or Right Shift (0x36) released */
            shift_active = false;
        }
        return;
    }

    /* 3. Key Press (Make codes: bit 7 is clear) */
    if (scancode == 0x2A || scancode == 0x36) {
        /* Left or Right Shift pressed */
        shift_active = true;
        return;
    }

    if (scancode == 0x3A) {
        /* Caps Lock toggled */
        caps_lock_active = !caps_lock_active;
        return;
    }

    /* Function Key Test Hooks */
    if (scancode == 0x3B) {
        /* F1 key pressed */
        buffer_put((char)KEY_F1);
        return;
    }

    if (scancode == 0x3C) {
        /* F2 key pressed */
        buffer_put((char)KEY_F2);
        return;
    }

    if (scancode == 0x3D) {
        /* F3 key pressed */
        buffer_put((char)KEY_F3);
        return;
    }

    if (scancode == 0x3E) {
        /* F4 key pressed */
        buffer_put((char)KEY_F4);
        return;
    }

    if (scancode == 0x3F) {
        /* F5 key pressed */
        buffer_put((char)KEY_F5);
        return;
    }

    if (scancode == 0x40) {
        /* F6 key pressed */
        buffer_put((char)KEY_F6);
        return;
    }

    if (scancode == 0x41) {
        /* F7 key pressed */
        buffer_put((char)KEY_F7);
        return;
    }

    if (scancode == 0x42) {
        /* F8 key pressed (Scancode 0x42) */
        buffer_put((char)KEY_F8);
        return;
    }

    if (scancode == 0x43) {
        /* F9 key pressed (Scancode 0x43) */
        buffer_put((char)KEY_F9);
        return;
    }

    if (scancode == 0x44) {
        /* F10 key pressed (Scancode 0x44) */
        buffer_put((char)KEY_F10);
        return;
    }

    /* 4. Character translation */
    if (scancode < 128) {
        char unshifted = scancode_unshifted[scancode];
        char shifted   = scancode_shifted[scancode];

        if (unshifted == 0) {
            return;
        }

        char out_char = unshifted;

        if (unshifted >= 'a' && unshifted <= 'z') {
            /* Letters: Shift XOR Caps Lock produces uppercase */
            bool uppercase = (shift_active ^ caps_lock_active);
            out_char = uppercase ? shifted : unshifted;
        } else {
            /* Non-letter characters (numbers, punctuation): Shift selects upper symbol */
            out_char = shift_active ? shifted : unshifted;
        }

        buffer_put(out_char);
    }
}

/*
 * keyboard_init:
 * Registers keyboard_handler on IRQ 1 and unmasks the line on the PIC.
 */
void keyboard_init(void) {
    buffer_head = 0;
    buffer_tail = 0;
    shift_active = false;
    caps_lock_active = false;
    extended_prefix = false;

    irq_register_handler(1, keyboard_handler);
    pic_unmask_irq(1);
}

/*
 * keyboard_has_char:
 * Returns true if an unconsumed keystroke is waiting in the input buffer.
 */
bool keyboard_has_char(void) {
    return buffer_head != buffer_tail;
}

/*
 * keyboard_getchar:
 * Blocking read that waits until a character is available.
 * Uses the 'hlt' instruction in the wait loop to sleep between interrupts.
 */
char keyboard_getchar(void) {
    while (buffer_head == buffer_tail) {
        __asm__ volatile ("hlt");
    }

    char c = keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}
