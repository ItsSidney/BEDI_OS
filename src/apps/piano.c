#include "drivers/video/gfx.h"
#include "drivers/video/gpu.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"
#include "drivers/audio/audio.h"
#include "gui/wm.h"
#include "kernel/time/timer.h"

#define NUM_WHITE 14
#define NUM_BLACK 10
#define NUM_KEYS (NUM_WHITE + NUM_BLACK)
#define KEY_W 24
#define KEY_H 120
#define BLACK_W 14
#define BLACK_H 80
#define PIANO_W (NUM_WHITE * KEY_W)
#define WIN_W (PIANO_W + 20)
#define WIN_H (KEY_H + 30 + WM_TITLEBAR_H)

static int prev_down[NUM_KEYS];
static int note_channel[NUM_KEYS];

typedef struct {
    int white_idx;
    int is_black;
    int x;
    uint32_t freq;
    uint8_t scancode;
} key_info_t;

static key_info_t keys[NUM_KEYS];

static uint32_t freqs[24] = {
    261,277,293,311,329,349,369,392,415,440,466,493,
    523,554,587,622,659,698,739,783,830,880,932,987
};

static uint8_t scancodes[NUM_KEYS] = {
    44,31,45,32,46,47,34,48,35,49,36,50,
    16,3,17,4,18,19,6,20,7,21,8,22
};

static void init_keys(void) {
    int black_pos[NUM_BLACK] = {0,1,3,4,5,7,8,10,11,12};
    int bi = 0;
    for (int i = 0; i < NUM_KEYS; i++) {
        if (bi < NUM_BLACK && i == black_pos[bi] + bi) {
            keys[i].is_black = 1;
            bi++;
        } else {
            keys[i].is_black = 0;
        }
        keys[i].scancode = scancodes[i];
        keys[i].freq = freqs[i];
        prev_down[i] = 0;
        note_channel[i] = -1;
    }

    bi = 0; int wi = 0;
    for (int i = 0; i < NUM_KEYS; i++) {
        if (bi < NUM_BLACK && i == black_pos[bi] + bi) {
            keys[i].is_black = 1;
            keys[i].white_idx = wi - 1;
            bi++;
        } else {
            keys[i].is_black = 0;
            keys[i].white_idx = wi;
            wi++;
        }
    }

    for (int i = 0; i < NUM_KEYS; i++) {
        if (keys[i].is_black) {
            int w = keys[i].white_idx;
            keys[i].x = w * KEY_W + 17;
        } else {
            keys[i].x = keys[i].white_idx * KEY_W;
        }
    }
}

static void piano_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx; (void)vy;
    gpu_accel_fill(x, y, w, h, 0x1a1a2e);

    int ox = x + 10;
    int oy = y + 10;

    audio_update();

    for (int i = 0; i < NUM_KEYS; i++) {
        int down = keyboard_is_key_down(keys[i].scancode);
        if (down && !prev_down[i]) {
            int ch = -1;
            for (int c = 0; c < AUDIO_MAX_CHANNELS; c++) {
                int used = 0;
                for (int k = 0; k < NUM_KEYS; k++) {
                    if (note_channel[k] == c) { used = 1; break; }
                }
                if (!used) { ch = c; break; }
            }
            if (ch < 0) {
                for (int k = 0; k < NUM_KEYS; k++) {
                    if (note_channel[k] >= 0) {
                        ch = note_channel[k];
                        note_channel[k] = -1;
                        break;
                    }
                }
            }
            if (ch >= 0) {
                audio_set_adsr(ch, 5, 20, 180, 80);
                audio_play_note(ch, WAVE_TRIANGLE, keys[i].freq, 200, 0);
                note_channel[i] = ch;
            }
        } else if (!down && prev_down[i]) {
            if (note_channel[i] >= 0) {
                audio_stop_channel(note_channel[i]);
                note_channel[i] = -1;
            }
        }
        prev_down[i] = down;
    }

    for (int i = 0; i < NUM_KEYS; i++) {
        if (keys[i].is_black) continue;
        int kx = ox + keys[i].x;
        int ky = oy + 4;
        int pressed = prev_down[i];
        uint32_t bg = pressed ? 0xCCCCCC : 0xFFFFFF;
        uint32_t border = 0x555555;
        gfx_fill_rect(kx, ky, KEY_W - 1, KEY_H, bg);
        gfx_draw_rect_outline(kx, ky, KEY_W - 1, KEY_H, 1, border);
    }

    for (int i = 0; i < NUM_KEYS; i++) {
        if (!keys[i].is_black) continue;
        int kx = ox + keys[i].x;
        int ky = oy;
        int pressed = prev_down[i];
        uint32_t bg = pressed ? 0x555555 : 0x111111;
        gfx_fill_rect(kx, ky, BLACK_W, BLACK_H, bg);
        gfx_draw_rect_outline(kx, ky, BLACK_W, BLACK_H, 1, 0x333333);
    }

}

static void piano_on_key(int win_id, char key) {
    (void)win_id;
    (void)key;
}

void piano_app(void) {
    audio_init();
    audio_set_mute(0);
    init_keys();

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    wm_open_window((fw - WIN_W) / 2, (fh - WIN_H) / 2, WIN_W, WIN_H,
                   "Piano", 0x8AB4F8, piano_render, piano_on_key, 0);
}
