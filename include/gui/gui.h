#ifndef GUI_H
#define GUI_H

#include <stdint.h>

#define TASKBAR_H 30

void start_gui(void);
void gui_shell(void);
void draw_premium_wallpaper(void);
void draw_taskbar(void);
void desktop_tick(void);
void desk_add_app_icon(int app_idx);
void desk_add_file_icon(const char* path, const char* display_name);
void desk_remove_icon_by_path(const char* path);
void desk_add_app_icon(int app_idx);
void desk_add_file_icon(const char* path, const char* display_name);

// ── Monolithic Theme System ─────────────────────────────────────────
typedef enum {
    THEME_ROLE_BACKGROUND = 0,
    THEME_ROLE_SURFACE,
    THEME_ROLE_SURFACE_VARIANT,
    THEME_ROLE_PRIMARY,
    THEME_ROLE_ON_PRIMARY,
    THEME_ROLE_SECONDARY,
    THEME_ROLE_ON_SECONDARY,
    THEME_ROLE_TERTIARY,
    THEME_ROLE_ERROR,
    THEME_ROLE_OUTLINE,
    THEME_ROLE_OVERLAY,
    THEME_ROLE_SURFACE_TINT,
    THEME_ROLE_INVERSE_SURFACE,
    THEME_ROLE_INVERSE_ON_SURFACE,
    THEME_ROLE_SHADOW,
    THEME_ROLE_SCROLLBAR,
    THEME_ROLE_DISABLED,
    THEME_ROLE_BUTTON_BG,
    THEME_ROLE_BUTTON_TEXT,
    THEME_ROLE_BUTTON_HOVER,
    THEME_ROLE_MENU_BG,
    THEME_ROLE_MENU_ITEM_HOVER,
    THEME_ROLE_MENU_ITEM_SELECTED,
    THEME_ROLE_WINDOW_BG,
    THEME_ROLE_WINDOW_TITLE,
    THEME_ROLE_WINDOW_BORDER,
    THEME_ROLE_TASKBAR_BG,
    THEME_ROLE_TASKBAR_TEXT,
    THEME_ROLE_ACCENT,
    THEME_ROLE_COUNT
} theme_role_t;

// Personalization
typedef struct {
    int accent_color_idx;
    int clock_24h;
    int mouse_sensitivity;
    int theme;                // 0=Light, 1=Dark, 2=Blue Default
    int bg_idx;                // background palette index
    int bg_pattern;            // background pattern index
    int bg_pattern_size;       // 1,2,4
    int accent_idx;            // accent palette index
    int font_idx;              // font color palette index
    int btn_idx;               // button palette index
    int corner_radius;         // 0=sharp, 2, 4, 6, 8
    int font_size;             // 0=small, 1=medium, 2=large
    int contrast;              // 0=normal, 1=high
    int saturation;            // 0=normal, 1=vivid
    int transparency;          // 0=off, values 1-10
} personalization_t;

personalization_t* get_personalization(void);
uint32_t theme_get_color(theme_role_t role);
void theme_set_custom_color(theme_role_t role, uint32_t color);
void theme_reset_custom(void);
uint32_t get_accent_color(void);

// Apps/Settings
void show_sound_settings(void);
void gui_system_shutdown(void);
void gui_toggle_start_menu(void);

#endif
