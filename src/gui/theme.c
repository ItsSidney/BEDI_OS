#include "gui/gui.h"
#include <string.h>

static personalization_t prefs;

personalization_t* get_personalization(void) { return &prefs; }

static uint32_t g_custom_colors[THEME_ROLE_COUNT];

static const uint32_t dark_roles[THEME_ROLE_COUNT] = {
    [THEME_ROLE_BACKGROUND]       = 0x0F1115,
    [THEME_ROLE_SURFACE]          = 0x1A1D24,
    [THEME_ROLE_SURFACE_VARIANT]  = 0x252830,
    [THEME_ROLE_PRIMARY]          = 0xE4E6EA,
    [THEME_ROLE_ON_PRIMARY]       = 0x0F1115,
    [THEME_ROLE_SECONDARY]        = 0x9CA3AF,
    [THEME_ROLE_ON_SECONDARY]     = 0xE4E6EA,
    [THEME_ROLE_TERTIARY]         = 0x6B7280,
    [THEME_ROLE_ERROR]            = 0xF28B82,
    [THEME_ROLE_OUTLINE]          = 0x2C303A,
    [THEME_ROLE_OVERLAY]          = 0x000000,
    [THEME_ROLE_SURFACE_TINT]     = 0x2C303A,
    [THEME_ROLE_INVERSE_SURFACE]  = 0xE4E6EA,
    [THEME_ROLE_INVERSE_ON_SURFACE]= 0x111827,
    [THEME_ROLE_SHADOW]           = 0x000000,
    [THEME_ROLE_SCROLLBAR]        = 0x4D5059,
    [THEME_ROLE_DISABLED]         = 0x6B7280,
    [THEME_ROLE_BUTTON_BG]        = 0x252830,
    [THEME_ROLE_BUTTON_TEXT]      = 0xE4E6EA,
    [THEME_ROLE_BUTTON_HOVER]     = 0x2C303A,
    [THEME_ROLE_MENU_BG]          = 0x1A1D24,
    [THEME_ROLE_MENU_ITEM_HOVER]  = 0x252830,
    [THEME_ROLE_MENU_ITEM_SELECTED]= 0x2C303A,
    [THEME_ROLE_WINDOW_BG]        = 0x1A1D24,
    [THEME_ROLE_WINDOW_TITLE]     = 0x252830,
    [THEME_ROLE_WINDOW_BORDER]    = 0x2C303A,
    [THEME_ROLE_TASKBAR_BG]       = 0x1A1D24,
    [THEME_ROLE_TASKBAR_TEXT]     = 0xE4E6EA,
    [THEME_ROLE_ACCENT]           = 0x8AB4F8,
};

uint32_t theme_get_color(theme_role_t role) {
    if (role < 0 || role >= THEME_ROLE_COUNT) return 0xFF0000;
    if (g_custom_colors[role] != 0) return g_custom_colors[role];
    return dark_roles[role];
}

void theme_set_custom_color(theme_role_t role, uint32_t color) {
    if (role < 0 || role >= THEME_ROLE_COUNT) return;
    g_custom_colors[role] = color;
}

void theme_reset_custom(void) {
    memset(g_custom_colors, 0, sizeof(g_custom_colors));
}

uint32_t get_accent_color(void) {
    static const uint32_t accent_colors[] = {
        0x8AB4F8, 0x81C995, 0xC58AF9, 0x78D9EC, 0xF28B82, 0xF2CC8C,
        0x669DF6, 0x5BB974, 0xAF5CF7, 0x4ECDC4, 0xE8EAED, 0x9AA0A6,
        0xFF6B6B, 0x4ECDC4, 0xFFE66D, 0xFF9F1C, 0x2EC4B6, 0xE71D36,
        0x7209B7, 0x3A86FF, 0xFB5607, 0xFFBE0B, 0x8338EC, 0xFF006E
    };
    if ((size_t)prefs.accent_color_idx < sizeof(accent_colors)/sizeof(accent_colors[0]))
        return accent_colors[prefs.accent_color_idx];
    return accent_colors[0];
}

void theme_init(void) {
    memset(&prefs, 0, sizeof(prefs));
    prefs.accent_color_idx = 0;
    prefs.clock_24h = 1;
    prefs.mouse_sensitivity = 2;
    prefs.theme = 1; /* always dark */
    prefs.bg_idx = 0;
    prefs.bg_pattern = 0;
    prefs.bg_pattern_size = 1;
    prefs.accent_idx = 0;
    prefs.font_idx = 0;
    prefs.btn_idx = 0;
    prefs.corner_radius = 0;
    prefs.font_size = 1;
    prefs.contrast = 0;
    prefs.saturation = 0;
    prefs.transparency = 0;
    memset(g_custom_colors, 0, sizeof(g_custom_colors));
}
