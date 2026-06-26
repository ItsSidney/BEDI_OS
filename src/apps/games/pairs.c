#include "drivers/video/gfx.h"
#include "drivers/video/gpu.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/mouse.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "gui/app_icons.h"
#include "kernel/time/timer.h"

#define GRID 6
#define CARDS (GRID * GRID)
#define PAIRS 18
#define CARD 48
#define GAP 4
#define PAD 14
#define WIN_W (GRID * (CARD + GAP) + PAD * 2 - GAP)
#define WIN_H (GRID * (CARD + GAP) + PAD * 2 + 20 - GAP + WM_TITLEBAR_H)

static int cards[CARDS];
static int revealed[CARDS];
static int matched[CARDS];
static int flip1, flip2;
static int flip_count;
static int moves;
static int game_won;
static unsigned long flip_back_time;
static int waiting_flip_back;
static int prev_mbtn;

static unsigned int pr_seed = 1;
static int pr_rand(void) {
    pr_seed = pr_seed * 1103515245 + 12345;
    return (pr_seed >> 16) & 0x7FFF;
}

static const char* pair_icon_names[] = {
    "Calc", "File", "Term", "Text", "Cale", "Games",
    "Syst", "Clock", "Bdro", "Proc", "PCI",  "Teac",
    "Pian", "Mine", "Snak", "Pairs", "Tetr", "Imag"
};

static void shuffle(void) {
    for (int i = 0; i < PAIRS; i++) {
        cards[i * 2] = i;
        cards[i * 2 + 1] = i;
    }
    for (int i = CARDS - 1; i > 0; i--) {
        int j = pr_rand() % (i + 1);
        int t = cards[i]; cards[i] = cards[j]; cards[j] = t;
    }
}

static void new_game(void) {
    for (int i = 0; i < CARDS; i++) {
        revealed[i] = 0;
        matched[i] = 0;
    }
    shuffle();
    flip1 = -1; flip2 = -1;
    flip_count = 0;
    moves = 0;
    game_won = 0;
    waiting_flip_back = 0;
    prev_mbtn = 0;
}

static void pairs_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)w; (void)vx; (void)vy;
    gpu_accel_fill(x, y, w, h, 0x0D0D1A);

    int mbtn = mouse_get_buttons();
    int mx = mouse_get_x();
    int my = mouse_get_y();

    int ox = x + (w - (GRID * (CARD + GAP) - GAP)) / 2;
    int oy = y + PAD;

    if (waiting_flip_back && timer_get_ms() > flip_back_time) {
        revealed[flip1] = 0;
        revealed[flip2] = 0;
        flip1 = -1; flip2 = -1;
        flip_count = 0;
        waiting_flip_back = 0;
    }

    if ((mbtn & 1) && !(prev_mbtn & 1) && !waiting_flip_back && !game_won) {
        for (int i = 0; i < CARDS; i++) {
            if (matched[i]) continue;
            int gx = i % GRID;
            int gy = i / GRID;
            int cx = ox + gx * (CARD + GAP);
            int cy = oy + gy * (CARD + GAP);
            if (mx >= cx && mx < cx + CARD && my >= cy && my < cy + CARD) {
                if (!revealed[i]) {
                    revealed[i] = 1;
                    flip_count++;
                    if (flip_count == 1) {
                        flip1 = i;
                    } else if (flip_count == 2) {
                        flip2 = i;
                        moves++;
                        if (cards[flip1] == cards[flip2]) {
                            matched[flip1] = 1;
                            matched[flip2] = 1;
                            flip1 = -1; flip2 = -1;
                            flip_count = 0;
                            int all = 1;
                            for (int j = 0; j < CARDS; j++)
                                if (!matched[j]) { all = 0; break; }
                            if (all) game_won = 1;
                        } else {
                            flip_back_time = timer_get_ms() + 600;
                            waiting_flip_back = 1;
                        }
                    }
                }
                break;
            }
        }
    }
    prev_mbtn = mbtn;

    for (int i = 0; i < CARDS; i++) {
        int gx = i % GRID;
        int gy = i / GRID;
        int cx = ox + gx * (CARD + GAP);
        int cy = oy + gy * (CARD + GAP);

        if (matched[i] || revealed[i]) {
            gfx_fill_rect_rounded(cx, cy, CARD, CARD, 5, 0x0D0D1A);
            gfx_draw_rect_rounded_outline(cx, cy, CARD, CARD, 5, 1, 0x444466);
            draw_app_icon(pair_icon_names[cards[i]], cx + 12, cy + 12);
        } else {
            gfx_fill_rect_rounded(cx, cy, CARD, CARD, 5, 0x1E1E3A);
            gfx_draw_rect_rounded_outline(cx, cy, CARD, CARD, 5, 1, 0x333355);
            int scx = cx + CARD / 2, scy = cy + CARD / 2;
            gfx_draw_line(scx, scy - 8, scx + 3, scy - 3, 0x555577);
            gfx_draw_line(scx + 3, scy - 3, scx + 8, scy - 3, 0x555577);
            gfx_draw_line(scx + 8, scy - 3, scx + 4, scy + 1, 0x555577);
            gfx_draw_line(scx + 4, scy + 1, scx + 6, scy + 6, 0x555577);
            gfx_draw_line(scx + 6, scy + 6, scx, scy + 3, 0x555577);
            gfx_draw_line(scx, scy + 3, scx - 6, scy + 6, 0x555577);
            gfx_draw_line(scx - 6, scy + 6, scx - 4, scy + 1, 0x555577);
            gfx_draw_line(scx - 4, scy + 1, scx - 8, scy - 3, 0x555577);
            gfx_draw_line(scx - 8, scy - 3, scx - 3, scy - 3, 0x555577);
            gfx_draw_line(scx - 3, scy - 3, scx, scy - 8, 0x555577);
        }
    }

    char buf[24];
    int si = 0;
    buf[si++] = 'M';
    buf[si++] = 'o';
    buf[si++] = 'v';
    buf[si++] = 'e';
    buf[si++] = 's';
    buf[si++] = ':';
    buf[si++] = ' ';
    if (moves >= 100) buf[si++] = '0' + moves / 100;
    if (moves >= 10) buf[si++] = '0' + (moves / 10) % 10;
    buf[si++] = '0' + moves % 10;
    buf[si] = 0;
    gfx_draw_string_transparent(x + PAD, y + h - 14, buf, 0x8AB4F8);

    if (game_won) {
        const char* again = "R=again";
        int aw = gfx_strlen(again) * 8;
        gfx_draw_string_transparent(x + w - aw - PAD, y + h - 14, again, 0x34D399);
    }
}

static void pairs_on_key(int id, char key) {
    (void)id;
    if (KEY_MATCH(key, 'r') || KEY_MATCH(key, 'R')) new_game();
}

void pairs_app(void) {
    pr_seed = timer_get_ms();
    new_game();
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    wm_open_window((fw - WIN_W) / 2, (fh - WIN_H) / 2, WIN_W, WIN_H,
                   "Pairs", 0xF472B6, pairs_render, pairs_on_key, 0);
}
