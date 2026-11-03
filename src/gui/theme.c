#include "gui/theme.h"
#include "gui/wm.h"
#include "drivers/serial.h"

static theme_id_t s_current_theme_id = THEME_WINDOWS_11;

/* Static table defining all 6 theme palettes */
static const theme_t s_themes[THEME_COUNT] = {
    [THEME_WINDOWS_11] = {
        .name              = "Windows 11 Fluent",
        .bg_desktop        = 0x000F172A, /* Windows 11 Deep Slate Navy */
        .bg_panel          = 0x001C1C1C, /* Taskbar Dark Mica */
        .bg_card           = 0x00242424, /* Modern Card */
        .bg_window_client  = 0x001F1F1F, /* Clean dark client canvas */
        .title_active_bg   = 0x00202020, /* Modern Windows Titlebar */
        .title_active_fg   = 0x00FFFFFF, /* Pure White Title */
        .title_inactive_bg = 0x00181818, /* Inactive Title */
        .title_inactive_fg = 0x008A8A8A, /* Inactive Muted Silver */
        .border_active     = 0x000078D4, /* 1px Windows Accent Blue outline */
        .border_inactive   = 0x002D2D2D, /* Inactive 1px border */
        .btn_close_bg      = 0x00C42B1C, /* Windows Close Red */
        .text_primary      = 0x00FFFFFF, /* Crisp White */
        .text_secondary    = 0x00A0A0A0, /* Subtext */
        .accent_primary    = 0x000078D4, /* Iconic Windows Blue */
        .accent_secondary  = 0x00107C41, /* Office Green */
        .accent_danger     = 0x00E81123, /* Red */
        .meter_fill        = 0x000078D4,
        .meter_bg          = 0x00141414,
        .dock_bg           = 0x001C1C1C,
        .dock_border       = 0x00333333
    },
    [THEME_CATPPUCCIN_MOCHA] = {
        .name              = "Catppuccin Mocha",
        .bg_desktop        = 0x001E1E2E, /* Base */
        .bg_panel          = 0x00181825, /* Mantle */
        .bg_card           = 0x00181825, /* Mantle */
        .bg_window_client  = 0x001E1E2E, /* Base */
        .title_active_bg   = 0x00CBA6F7, /* Mauve */
        .title_active_fg   = 0x0011111B, /* Crust */
        .title_inactive_bg = 0x00313244, /* Surface0 */
        .title_inactive_fg = 0x00A6ADC8, /* Subtext0 */
        .border_active     = 0x00B4BEFE, /* Lavender */
        .border_inactive   = 0x0045475A, /* Surface1 */
        .btn_close_bg      = 0x00F38BA8, /* Red */
        .text_primary      = 0x00CDD6F4, /* Text */
        .text_secondary    = 0x00A6ADC8, /* Subtext0 */
        .accent_primary    = 0x0089B4FA, /* Blue */
        .accent_secondary  = 0x00A6E3A1, /* Green */
        .accent_danger     = 0x00F38BA8, /* Red */
        .meter_fill        = 0x00A6E3A1, /* Green */
        .meter_bg          = 0x0011111B, /* Crust */
        .dock_bg           = 0x00181825, /* Mantle */
        .dock_border       = 0x00CBA6F7  /* Mauve */
    },
    [THEME_TOKYO_NIGHT] = {
        .name              = "Tokyo Night",
        .bg_desktop        = 0x0024283B,
        .bg_panel          = 0x001F2335,
        .bg_card           = 0x001F2335,
        .bg_window_client  = 0x0024283B,
        .title_active_bg   = 0x007AA2F7,
        .title_active_fg   = 0x001D202F,
        .title_inactive_bg = 0x00292E42,
        .title_inactive_fg = 0x007982A9,
        .border_active     = 0x007AA2F7,
        .border_inactive   = 0x003B4261,
        .btn_close_bg      = 0x00F7768E,
        .text_primary      = 0x00C0CAF5,
        .text_secondary    = 0x007982A9,
        .accent_primary    = 0x007DCFFF,
        .accent_secondary  = 0x009ECE6A,
        .accent_danger     = 0x00F7768E,
        .meter_fill        = 0x009ECE6A,
        .meter_bg          = 0x0016161E,
        .dock_bg           = 0x001F2335,
        .dock_border       = 0x007AA2F7
    },
    [THEME_CYBERPUNK] = {
        .name              = "Cyberpunk Neon",
        .bg_desktop        = 0x000F091A,
        .bg_panel          = 0x00181028,
        .bg_card           = 0x00181028,
        .bg_window_client  = 0x00120B20,
        .title_active_bg   = 0x00FF007F,
        .title_active_fg   = 0x00FFFFFF,
        .title_inactive_bg = 0x002E1B4E,
        .title_inactive_fg = 0x00A080C0,
        .border_active     = 0x0000F0FF,
        .border_inactive   = 0x003B2260,
        .btn_close_bg      = 0x00FF2A6D,
        .text_primary      = 0x0000F0FF,
        .text_secondary    = 0x00D080FF,
        .accent_primary    = 0x0000F0FF,
        .accent_secondary  = 0x00FFE600,
        .accent_danger     = 0x00FF007F,
        .meter_fill        = 0x0000F0FF,
        .meter_bg          = 0x0008040C,
        .dock_bg           = 0x00181028,
        .dock_border       = 0x0000F0FF
    },
    [THEME_NORD] = {
        .name              = "Nord Frost",
        .bg_desktop        = 0x002E3440,
        .bg_panel          = 0x003B4252,
        .bg_card           = 0x003B4252,
        .bg_window_client  = 0x002E3440,
        .title_active_bg   = 0x005E81AC,
        .title_active_fg   = 0x00ECEFF4,
        .title_inactive_bg = 0x00434C5E,
        .title_inactive_fg = 0x00D8DEE9,
        .border_active     = 0x0088C0D0,
        .border_inactive   = 0x004C566A,
        .btn_close_bg      = 0x00BF616A,
        .text_primary      = 0x00ECEFF4,
        .text_secondary    = 0x00D8DEE9,
        .accent_primary    = 0x0088C0D0,
        .accent_secondary  = 0x00A3BE8C,
        .accent_danger     = 0x00BF616A,
        .meter_fill        = 0x00A3BE8C,
        .meter_bg          = 0x00242933,
        .dock_bg           = 0x003B4252,
        .dock_border       = 0x0088C0D0
    },
    [THEME_CLASSIC_RETRO] = {
        .name              = "Classic Retro",
        .bg_desktop        = 0x00008080,
        .bg_panel          = 0x00C0C0C0,
        .bg_card           = 0x00C0C0C0,
        .bg_window_client  = 0x00FFFFFF,
        .title_active_bg   = 0x00000080,
        .title_active_fg   = 0x00FFFFFF,
        .title_inactive_bg = 0x00808080,
        .title_inactive_fg = 0x00C0C0C0,
        .border_active     = 0x00000000,
        .border_inactive   = 0x00808080,
        .btn_close_bg      = 0x00C82828,
        .text_primary      = 0x00000000,
        .text_secondary    = 0x00404040,
        .accent_primary    = 0x00000080,
        .accent_secondary  = 0x00008000,
        .accent_danger     = 0x00C82828,
        .meter_fill        = 0x00008000,
        .meter_bg          = 0x00808080,
        .dock_bg           = 0x00C0C0C0,
        .dock_border       = 0x00808080
    }
};

void theme_init(void) {
    s_current_theme_id = THEME_WINDOWS_11;
    wm_set_wallpaper_color(s_themes[s_current_theme_id].bg_desktop);
    serial_writestring("[THEME] Theme engine initialized: Windows 11 Fluent Dark active.\n");
}

void theme_set(theme_id_t id) {
    if (id >= THEME_COUNT) return;
    s_current_theme_id = id;
    wm_set_wallpaper_color(s_themes[id].bg_desktop);
    serial_writestring("[THEME] Switched active theme to: ");
    serial_writestring(s_themes[id].name);
    serial_writestring("\n");
}

const theme_t* theme_get_current(void) {
    return &s_themes[s_current_theme_id];
}

const theme_t* theme_get_by_id(theme_id_t id) {
    if (id >= THEME_COUNT) return &s_themes[THEME_CATPPUCCIN_MOCHA];
    return &s_themes[id];
}

theme_id_t theme_get_current_id(void) {
    return s_current_theme_id;
}
