// ============================================================
//  BEDI OS — Window Manager
//  Manages multiple windows with drag, z-order, and taskbar
// ============================================================
#ifndef WM_H
#define WM_H

#include <stdint.h>

#define WM_MAX_WINDOWS 8
#define WM_TITLEBAR_H  32
#define WM_TASKBAR_H   36
#define WM_MIN_W       200
#define WM_MIN_H       120

// Window flags
#define WM_FLAG_VISIBLE   0x01
#define WM_FLAG_DRAGGING  0x02
#define WM_FLAG_FOCUSED   0x04
#define WM_FLAG_CLOSABLE  0x08
#define WM_FLAG_RESIZING  0x10

typedef void (*wm_render_cb)(int id, int x, int y, int w, int h, int vx, int vy);
typedef void (*wm_key_cb)(int id, char key);
typedef void (*wm_click_cb)(int id, int btn_id);
typedef void (*wm_resize_cb)(int id, int w, int h);

typedef struct {
    int id;
    int x, y, w, h;
    int view_x, view_y;       // Viewport offset for scrolling
    int content_w, content_h; // Total size of content
    char title[64];
    uint32_t accent_color;
    int flags;
    int z_order;
    
    // Close button hit area
    int close_btn_x, close_btn_y, close_btn_w, close_btn_h;

    // Drag/Resize state
    int drag_offset_x;
    int drag_offset_y;
    int start_w, start_h;
    
    // App callbacks
    wm_render_cb on_render;
    wm_key_cb on_key;
    wm_resize_cb on_resize;
    
    // Button system (reuse ui_button_t concept)
    int button_count;
    struct {
        int id;
        int x, y, w, h;
        char label[32];
        uint32_t bg_color, fg_color;
        wm_click_cb on_click;
        int is_hovered;
        int is_active;
    } buttons[512];
    
    // App-specific data pointer
    void* app_data;
} wm_window_t;

// Initialize window manager
void wm_init(void);

// Open a new window, returns window ID or -1 if full
int wm_open_window(int x, int y, int w, int h, const char* title, 
                   uint32_t accent, wm_render_cb render, wm_key_cb key_handler,
                   wm_resize_cb resize_handler);

// Close a window by ID
void wm_close_window(int id);

// Add button to a window
void wm_add_button(int win_id, int btn_id, int x, int y, int w, int h,
                   const char* label, uint32_t bg, uint32_t fg, wm_click_cb cb);

// Clear all buttons from a window
void wm_clear_buttons(int win_id);

// Set button active state
void wm_set_button_active(int win_id, int btn_id, int active);

// Get focused window ID (-1 if none)
int wm_get_focused(void);

// Get window by ID
wm_window_t* wm_get_window(int id);

// Get open window count
int wm_get_window_count(void);

// Main tick — process input and render all windows
// Returns 0 if desktop should continue, 1 if terminal requested
int wm_tick(void);

// Set app data for a window
void wm_set_app_data(int win_id, void* data);
void* wm_get_app_data(int win_id);

// Run a single-window app (blocking, for backward compat)
void wm_run_single(int win_id);

// Get window by slot index (for taskbar iteration), returns NULL if empty
wm_window_t* wm_get_window_by_index(int index);

// Bring window to front
void wm_bring_to_front(int win_id);

#endif
