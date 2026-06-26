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

// Personalization
typedef struct {
    int accent_color_idx;     // 0=blue, 1=green, 2=purple, 3=cyan, 4=orange, 5=pink
    int clock_24h;            // 1=24h, 0=12h
    int mouse_sensitivity;    // 1-3
    int theme;                // 0=Dark, 1=Light
    int anim_speed;           // 0=Off, 1=Slow, 2=Normal
    int font_shadow;          // 0/1
    int window_transparency;  // 0-255
    int compact_mode;         // 0/1
    int system_volume;        // 0-100
} personalization_t;

personalization_t* get_personalization(void);
uint32_t get_theme_color(int color_id);
uint32_t get_accent_color(void);

// Apps/Settings
void show_time_settings(void);
void show_sound_settings(void);
void gui_system_shutdown(void);
void gui_toggle_start_menu(void);
int gui_is_topbar_cfg_open(void);
void gui_handle_topbar_cfg_key(char key);

// Apps
void process_viewer_app(void);

// Login / Session
const char* gui_get_current_username(void);

#endif
