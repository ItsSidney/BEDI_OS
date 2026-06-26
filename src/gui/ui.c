// ============================================================
//  BEDI OS — Sound Settings UI
// ============================================================
#include "gui/ui.h"
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/mouse.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "drivers/audio/audio.h"
#include "gui/gui.h"
#include <stddef.h>

static int g_sound_win = -1;

static void sound_on_key(int id, char key) {
    if (key == 27) {
        g_sound_win = -1;
        wm_close_window(id);
    }
}

static void sound_on_mute(int win_id, int btn_id) {
    audio_set_mute(!audio_is_muted());
}

static void sound_vol_down(int win_id, int btn_id) {
    audio_set_master_volume(audio_get_master_volume() - 10);
}

static void sound_vol_up(int win_id, int btn_id) {
    audio_set_master_volume(audio_get_master_volume() + 10);
}

static void sound_render(int id, int x, int y, int w, int h, int vx, int vy) {
    personalization_t* p = get_personalization();
    uint32_t bg       = (p->theme == 0) ? 0x15171D : 0xF4F6F8;
    uint32_t text_clr = (p->theme == 0) ? 0xE4E6EA : 0x202124;
    uint32_t muted    = (p->theme == 0) ? 0x6D7079 : 0x9CA3AF;
    uint32_t accent   = get_accent_color();
    int vol = audio_get_master_volume();

    gfx_fill_rect(x, y, w, h, bg);
    gfx_draw_string_transparent(x + 20, y + 20 - vy, "SOUND SETTINGS", accent);

    char buf[16];
    int n = 0;
    if (vol >= 100) buf[n++] = '0' + (vol / 100); vol %= 100;
    if (n || vol >= 10) buf[n++] = '0' + (vol / 10); vol %= 10;
    buf[n++] = '0' + vol;
    buf[n++] = 0;

    int label_y = y + 70 - vy;
    gfx_draw_string_transparent(x + (w - n * 8) / 2, label_y, buf, text_clr);
    gfx_draw_string_transparent(x + 20, label_y + 14, audio_is_muted() ? "MUTED" : "ACTIVE", muted);

    int btn_y = label_y + 38;
    gfx_fill_rect(x + 20, btn_y, 80, 28, muted);
    gfx_draw_string_transparent(x + 32, btn_y + 8, "VOL -", 0x000000);
    gfx_fill_rect(x + 120, btn_y, 80, 28, muted);
    gfx_draw_string_transparent(x + 132, btn_y + 8, "VOL +", 0x000000);
    gfx_fill_rect(x + 220, btn_y, 100, 28, audio_is_muted() ? accent : muted);
    gfx_draw_string_transparent(x + 232, btn_y + 8, audio_is_muted() ? "UNMUTE" : "MUTE",
                               audio_is_muted() ? 0x000000 : 0xFFFFFF);
}

void show_sound_settings(void) {
    if (g_sound_win >= 0) {
        wm_close_window(g_sound_win);
        g_sound_win = -1;
        return;
    }

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int w = 340, h = 180;
    int x = (fw - w) / 2;
    int y = (fh - h) / 2;

    g_sound_win = wm_open_window(x, y, w, h, "Sound Settings", get_accent_color(),
                                 sound_render, sound_on_key, NULL);

    wm_add_button(g_sound_win, 1, 20, 110, 80, 28, "VOL -", 0x6B7280, 0x000000, sound_vol_down);
    wm_add_button(g_sound_win, 2, 120, 110, 80, 28, "VOL +", 0x6B7280, 0x000000, sound_vol_up);
    wm_add_button(g_sound_win, 3, 220, 110, 100, 28, "MUTE", 0xEF4444, 0xFFFFFF, sound_on_mute);
}
