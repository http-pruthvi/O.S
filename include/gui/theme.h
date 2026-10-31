#ifndef GUI_THEME_H
#define GUI_THEME_H

#include "lib/types.h"

/*
 * theme.h:
 * Unified Theme Engine & Color Token System for MyOS.
 *
 * Why this is needed:
 * Manages desktop visual presets (Catppuccin Mocha, Tokyo Night, Cyberpunk Neon,
 * Nord Frost, and Classic Retro). Provides standardized 32-bit RGB color tokens
 * for window chrome, title bars, HUD widgets, Waybar, Dock, and Lock Screen.
 */

typedef enum theme_id {
    THEME_CATPPUCCIN_MOCHA = 0, /* Default modern pastel dark theme */
    THEME_TOKYO_NIGHT,           /* Deep navy with cyan and purple neon */
    THEME_CYBERPUNK,             /* Dark abyss with magenta and laser cyan */
    THEME_NORD,                  /* Scandinavian arctic blue and cool grays */
    THEME_CLASSIC_RETRO,         /* Authentic Windows 95/XP teal & silver */
    THEME_COUNT
} theme_id_t;

typedef struct theme {
    const char* name;
    
    /* Surface Backgrounds */
    uint32_t bg_desktop;
    uint32_t bg_panel;
    uint32_t bg_card;
    uint32_t bg_window_client;

    /* Window Chrome */
    uint32_t title_active_bg;
    uint32_t title_active_fg;
    uint32_t title_inactive_bg;
    uint32_t title_inactive_fg;
    uint32_t border_active;
    uint32_t border_inactive;
    uint32_t btn_close_bg;

    /* Text & Accents */
    uint32_t text_primary;
    uint32_t text_secondary;
    uint32_t accent_primary;
    uint32_t accent_secondary;
    uint32_t accent_danger;

    /* Docks & Meter Gauges */
    uint32_t meter_fill;
    uint32_t meter_bg;
    uint32_t dock_bg;
    uint32_t dock_border;
} theme_t;

/* Initializes Theme Engine with default theme (Catppuccin Mocha) */
void theme_init(void);

/* Sets the active theme and updates desktop background */
void theme_set(theme_id_t id);

/* Returns pointer to the currently active theme */
const theme_t* theme_get_current(void);

/* Returns pointer to theme definition by ID */
const theme_t* theme_get_by_id(theme_id_t id);

/* Returns current active theme ID */
theme_id_t theme_get_current_id(void);

#endif /* GUI_THEME_H */
