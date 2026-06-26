#include "drivers/video/gfx.h"
#include "drivers/video/gpu.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "gui/wm.h"
#include "kernel/time/timer.h"

#define WIN_W 300
#define CONTENT_H 440
#define WIN_H (CONTENT_H + WM_TITLEBAR_H)

#define GRAVITY 18
#define FLAP_VEL -140
#define MAX_FALL 280

#define PIPE_W 32
#define PIPE_SPEED 90
#define GAP_H 136
#define SPAWN_INTERVAL 100

#define GROUND_H 44
#define BIRD_X 70
#define BIRD_R 7

#define MAX_PIPES 4

typedef struct { int x, gap_y, scored; } Pipe;

static int bird_y, bird_vy;
static Pipe pipes[MAX_PIPES];
static int pipe_count, spawn_timer, score, high_score;
static int state, death_timer, ground_ofs;
static unsigned long last_tick;
static unsigned int rng;
static int prev_mbtn;

static int rng_next(void) {
    rng = rng * 1103515245 + 12345;
    return (rng >> 16) & 0x7FFF;
}

static void reset(void) {
    bird_y = CONTENT_H / 2 - 20;
    bird_vy = 0;
    pipe_count = 0;
    spawn_timer = 50;
    score = 0;
    state = 0;
    death_timer = 0;
    ground_ofs = 0;
}

static void spawn_pipe(void) {
    if (pipe_count >= MAX_PIPES) return;
    int min_gap = 50;
    int max_gap = CONTENT_H - GROUND_H - GAP_H - 50;
    if (max_gap < min_gap) max_gap = min_gap + 20;
    pipes[pipe_count].x = WIN_W + 10;
    pipes[pipe_count].gap_y = min_gap + rng_next() % (max_gap - min_gap + 1);
    pipes[pipe_count].scored = 0;
    pipe_count++;
}

static void update(void) {
    if (state == 0 || state == 2) return;

    bird_vy += GRAVITY;
    if (bird_vy > MAX_FALL) bird_vy = MAX_FALL;
    bird_y += bird_vy / 10;
    ground_ofs = (ground_ofs + 2) % 16;

    for (int i = 0; i < pipe_count; i++) {
        pipes[i].x -= PIPE_SPEED / 10;
        if (!pipes[i].scored && pipes[i].x + PIPE_W < BIRD_X) {
            pipes[i].scored = 1;
            score++;
            if (score > high_score) high_score = score;
        }
    }

    int nc = 0;
    for (int i = 0; i < pipe_count; i++)
        if (pipes[i].x > -PIPE_W) pipes[nc++] = pipes[i];
    pipe_count = nc;

    spawn_timer++;
    if (spawn_timer >= SPAWN_INTERVAL) {
        spawn_timer = 0;
        spawn_pipe();
    }

    if (bird_y + BIRD_R * 2 >= CONTENT_H - GROUND_H) {
        state = 2;
        death_timer = 30;
        return;
    }
    if (bird_y < 0) { bird_y = 0; bird_vy = 0; }

    for (int i = 0; i < pipe_count; i++) {
        int px = pipes[i].x, py = pipes[i].gap_y;
        int bx = BIRD_X + 1, by = bird_y + 1, bw = BIRD_R * 2 - 2, bh = BIRD_R * 2 - 2;
        if (bx + bw > px && bx < px + PIPE_W) {
            if (by < py || by + bh > py + GAP_H) {
                state = 2;
                death_timer = 30;
                return;
            }
        }
    }
}

static void draw_bird(int x, int y) {
    int bx = x + BIRD_X;
    int by = y + bird_y;

    if (state == 2 && death_timer > 0 && death_timer < 25) {
        by += (30 - death_timer) * 4;
    }

    int cx = bx + BIRD_R, cy = by + BIRD_R;
    int tilt = -bird_vy / 20;
    if (tilt < -16) tilt = -16;
    if (tilt > 12) tilt = 12;

    gfx_fill_circle(cx, cy + tilt / 4, BIRD_R, 0xFFD700);
    gfx_draw_circle(cx, cy + tilt / 4, BIRD_R, 0xCC9900);

    int wing_cy = cy + tilt / 4 + 2;
    if (state == 1) {
        int wt = (int)(timer_get_ms() / 80) % 2;
        wing_cy += wt * 2;
    }
    gfx_fill_circle(cx - 2, wing_cy, 3, 0xE6B800);

    int eye_x = cx + 3 - tilt / 10;
    int eye_y = cy + tilt / 4 - 2 - tilt / 10;
    if (state == 2) {
        gfx_fill_circle(eye_x, eye_y, 2, 0xFF4444);
        gfx_fill_circle(eye_x + 1, eye_y, 1, 0x000000);
    } else {
        gfx_fill_circle(eye_x, eye_y, 2, 0xFFFFFF);
        gfx_fill_circle(eye_x + 1, eye_y, 1, 0x000000);
    }

    gfx_fill_rect(cx + 5 + tilt / 10, cy + tilt / 4 - 2, 5, 3, 0xFF8C00);
}

static void draw_pipes(int x, int y) {
    int gy = y + CONTENT_H - GROUND_H;
    for (int i = 0; i < pipe_count; i++) {
        int px = x + pipes[i].x;
        int py = y + pipes[i].gap_y;
        int bt = py + GAP_H;

        gfx_fill_rect(px, y, PIPE_W, py - y, 0x4CAF50);
        gfx_fill_rect(px + 2, y, 3, py - y, 0x66BB6A);
        gfx_fill_rect(px + PIPE_W - 3, y, 2, py - y, 0x388E3C);

        gfx_fill_rect(px - 3, py - 20, PIPE_W + 6, 20, 0x4CAF50);
        gfx_fill_rect(px - 1, py - 20, 3, 20, 0x66BB6A);
        gfx_fill_rect(px + PIPE_W - 2, py - 18, 2, 18, 0x388E3C);

        gfx_fill_rect(px, bt, PIPE_W, gy - bt, 0x4CAF50);
        gfx_fill_rect(px + 2, bt, 3, gy - bt, 0x66BB6A);
        gfx_fill_rect(px + PIPE_W - 3, bt, 2, gy - bt, 0x388E3C);

        gfx_fill_rect(px - 3, bt, PIPE_W + 6, 20, 0x4CAF50);
        gfx_fill_rect(px - 1, bt, 3, 20, 0x66BB6A);
        gfx_fill_rect(px + PIPE_W - 2, bt + 2, 2, 18, 0x388E3C);
    }
}

static void draw_ground(int x, int y) {
    int gy = y + CONTENT_H - GROUND_H;
    gfx_fill_rect(x, gy, WIN_W, GROUND_H, 0x8D6E63);
    gfx_fill_rect(x, gy, WIN_W, 3, 0x4CAF50);
    for (int i = -ground_ofs; i < WIN_W; i += 8) {
        gfx_fill_rect(x + i, gy + 8, 2, 2, 0x7B5B4F);
        gfx_fill_rect(x + i + 4, gy + 20, 2, 2, 0x7B5B4F);
    }
    for (int i = -ground_ofs + 4; i < WIN_W; i += 8) {
        gfx_fill_rect(x + i, gy + 14, 2, 2, 0x7B5B4F);
    }
}

static void draw_score(int x, int y) {
    char buf[8];
    int si = 0, s = score;
    if (s >= 1000) buf[si++] = '0' + (s / 1000) % 10;
    if (s >= 100)  buf[si++] = '0' + (s / 100) % 10;
    if (s >= 10)   buf[si++] = '0' + (s / 10) % 10;
    buf[si++] = '0' + s % 10; buf[si] = 0;

    int sw = gfx_strlen(buf);
    gfx_draw_string_transparent(x + (WIN_W - sw) / 2 + 1, y + 21, buf, 0x00000044);
    gfx_draw_string_transparent(x + (WIN_W - sw) / 2, y + 20, buf, 0xFFFFFF);
}

static void flappy_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)w; (void)vx; (void)vy;

    int mbtn = mouse_get_buttons();
    int click = (mbtn & 1) && !(prev_mbtn & 1);
    prev_mbtn = mbtn;

    if (state == 0 && click) { state = 1; bird_vy = FLAP_VEL; }
    else if (state == 1 && click && bird_vy > -100) bird_vy = FLAP_VEL;

    unsigned long now = timer_get_ms();
    if (now - last_tick >= 20) {
        update();
        last_tick = now;
    }
    if (death_timer > 0) death_timer--;

    gpu_accel_fill(x, y, WIN_W, CONTENT_H, 0x87CEEB);
    draw_pipes(x, y);
    draw_ground(x, y);
    draw_bird(x, y);
    draw_score(x, y);

    if (state == 0) {
        gfx_draw_string_transparent(x + (WIN_W - gfx_strlen("Flappy Bird")) / 2, y + CONTENT_H / 3, "Flappy Bird", 0xFFFFFF);
        gfx_draw_string_transparent(x + (WIN_W - gfx_strlen("SPACE or Click")) / 2, y + CONTENT_H / 2 + 20, "SPACE or Click", 0x444444);
    }

    if (state == 2 && death_timer <= 20) {
        gfx_fill_rect(x + 35, y + CONTENT_H / 2 - 40, WIN_W - 70, 95, 0x000000AA);

        gfx_draw_string_transparent(x + (WIN_W - gfx_strlen("Game Over")) / 2, y + CONTENT_H / 2 - 28, "Game Over", 0xFF4444);

        char sb[8], hb[8];
        int si = 0, s = score;
        if (s >= 1000) sb[si++] = '0' + (s / 1000) % 10;
        if (s >= 100)  sb[si++] = '0' + (s / 100) % 10;
        if (s >= 10)   sb[si++] = '0' + (s / 10) % 10;
        sb[si++] = '0' + s % 10; sb[si] = 0;
        si = 0; s = high_score;
        if (s >= 1000) hb[si++] = '0' + (s / 1000) % 10;
        if (s >= 100)  hb[si++] = '0' + (s / 100) % 10;
        if (s >= 10)   hb[si++] = '0' + (s / 10) % 10;
        hb[si++] = '0' + s % 10; hb[si] = 0;

        gfx_draw_string_transparent(x + 55, y + CONTENT_H / 2 - 6, "Score:", 0xCCCCCC);
        gfx_draw_string_transparent(x + 115, y + CONTENT_H / 2 - 6, sb, 0xFFFFFF);
        gfx_draw_string_transparent(x + 55, y + CONTENT_H / 2 + 12, "Best:", 0xCCCCCC);
        gfx_draw_string_transparent(x + 115, y + CONTENT_H / 2 + 12, hb, 0xFFDD44);

        gfx_draw_string_transparent(x + (WIN_W - gfx_strlen("R to restart")) / 2, y + CONTENT_H / 2 + 34, "R to restart", 0xCCCCCC);
    }
}

static void on_key(int id, char key) {
    (void)id;
    if (state == 2 && (KEY_MATCH(key, 'r') || KEY_MATCH(key, 'R'))) {
        reset(); return;
    }
    if (key == ' ' || KEY_MATCH(key, KEY_UP)) {
        if (state == 0) { state = 1; bird_vy = FLAP_VEL; }
        else if (state == 1 && bird_vy > -100) bird_vy = FLAP_VEL;
    }
}

void flappy_app(void) {
    reset();
    rng = timer_get_ms();
    spawn_pipe();
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    wm_open_window((fw - WIN_W) / 2, (fh - WIN_H) / 2, WIN_W, WIN_H,
                   "Flappy Bird", 0x87CEEB, flappy_render, on_key, 0);
}
