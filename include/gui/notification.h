#ifndef GUI_NOTIFICATION_H
#define GUI_NOTIFICATION_H

#include "lib/types.h"
#include "gui/theme.h"

/*
 * notification.h:
 * Desktop Toast Notification System for MyOS.
 *
 * Provides non-blocking visual alert cards that pop up in the top-right corner
 * of the screen to inform the user of system events, hardware alerts, and app status.
 */

#define NOTIFY_MAX_TITLE   32
#define NOTIFY_MAX_MESSAGE 64
#define NOTIFY_MAX_HISTORY 10

typedef enum {
    NOTIFY_INFO = 0,
    NOTIFY_SUCCESS,
    NOTIFY_WARNING,
    NOTIFY_SECURITY
} notify_type_t;

typedef struct {
    char title[NOTIFY_MAX_TITLE];
    char message[NOTIFY_MAX_MESSAGE];
    notify_type_t type;
    uint32_t ticks_remaining;
    bool active;
} notification_t;

/* Initializes the notification subsystem */
void notification_init(void);

/* Pushes a new toast notification */
void notification_push(notify_type_t type, const char* title, const char* message);

/* Ticks notification timers (called on timer ticks) */
void notification_tick(void);

/* Handles mouse click; returns true if click dismissed active toast */
bool notification_handle_click(int32_t mx, int32_t my);

/* Renders active toast alert card onto the backbuffer */
void notification_render(uint32_t* backbuf, uint32_t fb_width, uint32_t fb_height, const theme_t* theme);

#endif /* GUI_NOTIFICATION_H */
