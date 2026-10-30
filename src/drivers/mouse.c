#include "drivers/mouse.h"
#include "arch/irq.h"
#include "arch/pic.h"
#include "drivers/serial.h"
#include "lib/io.h"
#include "lib/kprintf.h"

#define MOUSE_QUEUE_SIZE 64
#define MOUSE_TIMEOUT    100000

/* Circular ring buffer for mouse events */
static volatile mouse_event_t mouse_queue[MOUSE_QUEUE_SIZE];
static volatile size_t queue_head = 0;
static volatile size_t queue_tail = 0;

/* Current tracked mouse coordinates (starts at screen center 512, 384) */
static int mouse_x = 512;
static int mouse_y = 384;

/* Current packet assembly state */
static uint8_t packet_bytes[3];
static uint8_t packet_cycle = 0;

/* Counter for logging the first 5 raw packets to COM1 serial */
static uint32_t raw_packets_logged = 0;

/*
 * mouse_wait_write:
 * Bounded polling until the PS/2 controller input buffer is empty (Status bit 1 == 0).
 */
static bool mouse_wait_write(void) {
    uint32_t timeout = MOUSE_TIMEOUT;
    while (timeout > 0) {
        if ((inb(MOUSE_STATUS_PORT) & 0x02) == 0) {
            return true;
        }
        timeout--;
    }
    return false;
}

/*
 * mouse_wait_read:
 * Bounded polling until data is available to read from port 0x60 (Status bit 0 == 1).
 */
static bool mouse_wait_read(void) {
    uint32_t timeout = MOUSE_TIMEOUT;
    while (timeout > 0) {
        if ((inb(MOUSE_STATUS_PORT) & 0x01) != 0) {
            return true;
        }
        timeout--;
    }
    return false;
}

/*
 * mouse_send_cmd:
 * Sends a single-byte command to the mouse via the 0xD4 prefix and returns ACK.
 */
static uint8_t mouse_send_cmd(uint8_t cmd) {
    if (!mouse_wait_write()) {
        return 0;
    }
    outb(MOUSE_STATUS_PORT, 0xD4); /* Route next data byte to auxiliary device */

    if (!mouse_wait_write()) {
        return 0;
    }
    outb(MOUSE_DATA_PORT, cmd);

    if (!mouse_wait_read()) {
        return 0;
    }
    return inb(MOUSE_DATA_PORT);
}

/*
 * process_mouse_packet:
 * Parses a complete 3-byte PS/2 packet and enqueues or coalesces the event.
 * RULE: No drawing and no kprintf in this function (called from IRQ 12 context).
 */
static void process_mouse_packet(void) {
    uint8_t b0 = packet_bytes[0];
    uint8_t b1 = packet_bytes[1];
    uint8_t b2 = packet_bytes[2];

    /* Discard packets with overflow flags set */
    if (b0 & 0xC0) {
        return;
    }

    /* Compute relative horizontal delta */
    int32_t dx = (int32_t)b1;
    if (b0 & 0x10) {
        dx |= 0xFFFFFF00; /* Sign extend negative value */
    }

    /* Compute relative vertical delta (PS/2 positive is up, screen positive is down) */
    int32_t dy = (int32_t)b2;
    if (b0 & 0x20) {
        dy |= 0xFFFFFF00; /* Sign extend negative value */
    }
    dy = -dy; /* Invert Y to match screen top-to-bottom coordinates */

    uint8_t buttons = b0 & 0x07; /* Bit 0: Left, Bit 1: Right, Bit 2: Middle */

    /*
     * Event Coalescing:
     * If the queue is non-empty and the button state hasn't changed since the previous event,
     * accumulate dx and dy into the previous event instead of creating a new queue entry.
     */
    if (queue_head != queue_tail) {
        size_t prev_idx = (queue_head + MOUSE_QUEUE_SIZE - 1) % MOUSE_QUEUE_SIZE;
        if (mouse_queue[prev_idx].buttons == buttons) {
            mouse_queue[prev_idx].dx += dx;
            mouse_queue[prev_idx].dy += dy;
            mouse_queue[prev_idx].raw[0] = b0;
            mouse_queue[prev_idx].raw[1] = b1;
            mouse_queue[prev_idx].raw[2] = b2;
            return;
        }
    }

    /* Check for queue overflow: drop the oldest event if full */
    size_t next_head = (queue_head + 1) % MOUSE_QUEUE_SIZE;
    if (next_head == queue_tail) {
        queue_tail = (queue_tail + 1) % MOUSE_QUEUE_SIZE; /* Drop oldest */
    }

    /* Enqueue new event */
    mouse_queue[queue_head].dx = dx;
    mouse_queue[queue_head].dy = dy;
    mouse_queue[queue_head].buttons = buttons;
    mouse_queue[queue_head].raw[0] = b0;
    mouse_queue[queue_head].raw[1] = b1;
    mouse_queue[queue_head].raw[2] = b2;
    queue_head = next_head;
}

/*
 * mouse_handler:
 * Hardware IRQ 12 interrupt handler.
 * Drains all pending auxiliary bytes from the 8042 controller.
 * RULE: No drawing and no kprintf!
 */
static void mouse_handler(registers_t* regs) {
    (void)regs;

    while ((inb(MOUSE_STATUS_PORT) & 0x21) == 0x21) {
        uint8_t byte = inb(MOUSE_DATA_PORT);

        if (packet_cycle == 0) {
            /* Bit 3 of byte 0 is always 1 in standard PS/2 mouse packets */
            if ((byte & 0x08) == 0) {
                continue; /* Desynchronized; discard until packet alignment bit is found */
            }
            packet_bytes[0] = byte;
            packet_cycle = 1;
        } else if (packet_cycle == 1) {
            packet_bytes[1] = byte;
            packet_cycle = 2;
        } else if (packet_cycle == 2) {
            packet_bytes[2] = byte;
            packet_cycle = 0;
            process_mouse_packet();
        }
    }
}

/*
 * log_raw_packet:
 * Formats and writes the first 5 raw packets to COM1 serial for debugging.
 */
static void log_raw_packet(uint32_t num, const mouse_event_t* ev) {
    char buf[96];
    char num_char = (char)('1' + (num - 1));

    static const char hex[] = "0123456789ABCDEF";

    int idx = 0;
    const char prefix[] = "[MOUSE] Packet #";
    for (int i = 0; prefix[i]; i++) buf[idx++] = prefix[i];
    buf[idx++] = num_char;

    const char raw_lbl[] = ": raw [0x";
    for (int i = 0; raw_lbl[i]; i++) buf[idx++] = raw_lbl[i];
    buf[idx++] = hex[(ev->raw[0] >> 4) & 0x0F];
    buf[idx++] = hex[ev->raw[0] & 0x0F];

    buf[idx++] = ','; buf[idx++] = ' '; buf[idx++] = '0'; buf[idx++] = 'x';
    buf[idx++] = hex[(ev->raw[1] >> 4) & 0x0F];
    buf[idx++] = hex[ev->raw[1] & 0x0F];

    buf[idx++] = ','; buf[idx++] = ' '; buf[idx++] = '0'; buf[idx++] = 'x';
    buf[idx++] = hex[(ev->raw[2] >> 4) & 0x0F];
    buf[idx++] = hex[ev->raw[2] & 0x0F];

    const char btn_lbl[] = "] btn=";
    for (int i = 0; btn_lbl[i]; i++) buf[idx++] = btn_lbl[i];
    buf[idx++] = (char)('0' + (ev->buttons & 0x07));
    buf[idx++] = '\n';
    buf[idx++] = '\0';

    serial_writestring(buf);
}

/*
 * mouse_has_event:
 * Checks if at least one mouse event is waiting to be processed.
 */
bool mouse_has_event(void) {
    return (queue_head != queue_tail);
}

/*
 * mouse_poll_event:
 * Dequeues the next event from the queue, updates clamped screen coordinates,
 * and logs the first 5 raw packets to serial.
 */
bool mouse_poll_event(mouse_event_t* event) {
    if (queue_head == queue_tail) {
        return false;
    }

    mouse_event_t ev = mouse_queue[queue_tail];
    queue_tail = (queue_tail + 1) % MOUSE_QUEUE_SIZE;

    /* Update accumulated screen position */
    mouse_x += ev.dx;
    mouse_y += ev.dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= 1024) mouse_x = 1023;
    if (mouse_y >= 768) mouse_y = 767;

    /* Log first 5 packets to serial */
    if (raw_packets_logged < 5) {
        raw_packets_logged++;
        log_raw_packet(raw_packets_logged, &ev);
    }

    if (event) {
        *event = ev;
    }
    return true;
}

/*
 * mouse_get_position:
 * Returns current clamped screen coordinates of the mouse.
 */
void mouse_get_position(int* x, int* y) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
}

/*
 * mouse_set_position:
 * Sets tracked mouse coordinates.
 */
void mouse_set_position(int x, int y) {
    mouse_x = x;
    mouse_y = y;
}

/*
 * mouse_init:
 * Initializes the PS/2 mouse hardware and unmasks IRQ 12 on the PIC.
 */
bool mouse_init(void) {
    /* Step 1: Flush output buffer */
    int timeout = 10000;
    while ((inb(MOUSE_STATUS_PORT) & 0x01) && --timeout > 0) {
        inb(MOUSE_DATA_PORT);
    }
    if (timeout == 0) {
        serial_writestring("[MOUSE] Init failed: Step 1 (Flush output buffer timeout)\n");
        kprintf("[FAIL] PS/2 mouse: Step 1 (Flush output buffer timeout)\n");
        return false;
    }

    /* Step 2: Enable auxiliary device port */
    if (!mouse_wait_write()) {
        serial_writestring("[MOUSE] Init failed: Step 2 (Wait write timeout before 0xA8)\n");
        kprintf("[FAIL] PS/2 mouse: Step 2 (Port 0x64 write timeout for 0xA8)\n");
        return false;
    }
    outb(MOUSE_STATUS_PORT, 0xA8);

    /* Step 3: Read controller command byte */
    if (!mouse_wait_write()) {
        serial_writestring("[MOUSE] Init failed: Step 3 (Wait write timeout before 0x20)\n");
        kprintf("[FAIL] PS/2 mouse: Step 3 (Port 0x64 write timeout for 0x20)\n");
        return false;
    }
    outb(MOUSE_STATUS_PORT, 0x20);

    if (!mouse_wait_read()) {
        serial_writestring("[MOUSE] Init failed: Step 4 (Wait read timeout for config byte)\n");
        kprintf("[FAIL] PS/2 mouse: Step 4 (Config byte read timeout)\n");
        return false;
    }
    uint8_t config = inb(MOUSE_DATA_PORT);

    /* Step 4: Enable IRQ 12 (bit 1) and enable mouse clock line (clear bit 5) */
    config |= 0x02;   /* Set bit 1: Enable IRQ 12 */
    config &= ~0x20;  /* Clear bit 5: Enable mouse clock line */

    if (!mouse_wait_write()) {
        serial_writestring("[MOUSE] Init failed: Step 5 (Wait write timeout before 0x60)\n");
        kprintf("[FAIL] PS/2 mouse: Step 5 (Port 0x64 write timeout for 0x60)\n");
        return false;
    }
    outb(MOUSE_STATUS_PORT, 0x60);

    if (!mouse_wait_write()) {
        serial_writestring("[MOUSE] Init failed: Step 6 (Wait write timeout for new config byte)\n");
        kprintf("[FAIL] PS/2 mouse: Step 6 (Config byte write timeout)\n");
        return false;
    }
    outb(MOUSE_DATA_PORT, config);

    /* Step 5: Send Set Defaults (0xF6) */
    uint8_t ack = mouse_send_cmd(0xF6);
    if (ack != 0xFA) {
        serial_writestring("[MOUSE] Init failed: Step 7 (Set defaults 0xF6 no ACK)\n");
        kprintf("[FAIL] PS/2 mouse: Step 7 (Set defaults 0xF6 ACK failed: 0x%02x)\n", ack);
        return false;
    }

    /* Step 6: Read Device ID (0xF2) */
    ack = mouse_send_cmd(0xF2);
    if (ack != 0xFA) {
        serial_writestring("[MOUSE] Init failed: Step 8 (Read ID 0xF2 no ACK)\n");
        kprintf("[FAIL] PS/2 mouse: Step 8 (Read ID 0xF2 ACK failed: 0x%02x)\n", ack);
        return false;
    }
    if (!mouse_wait_read()) {
        serial_writestring("[MOUSE] Init failed: Step 9 (Device ID read timeout)\n");
        kprintf("[FAIL] PS/2 mouse: Step 9 (Device ID read timeout)\n");
        return false;
    }
    uint8_t device_id = inb(MOUSE_DATA_PORT);

    /* Step 7: Enable Data Reporting (0xF4) */
    ack = mouse_send_cmd(0xF4);
    if (ack != 0xFA) {
        serial_writestring("[MOUSE] Init failed: Step 10 (Enable reporting 0xF4 no ACK)\n");
        kprintf("[FAIL] PS/2 mouse: Step 10 (Enable reporting 0xF4 ACK failed: 0x%02x)\n", ack);
        return false;
    }

    /* Step 8: Register IRQ 12 handler and unmask PIC */
    irq_register_handler(12, mouse_handler);
    pic_unmask_irq(2);   /* Unmask Cascade line on Master PIC */
    pic_unmask_irq(12);  /* Unmask Mouse IRQ 12 on Slave PIC */

    serial_writestring("[OK] PS/2 mouse initialized (ID 0x00)\n");
    kprintf("[OK] PS/2 mouse initialized (ID 0x%02x)\n", device_id);

    return true;
}
