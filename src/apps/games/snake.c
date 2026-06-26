#include "drivers/video/gfx.h"
#include "drivers/video/gpu.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "kernel/time/timer.h"

static unsigned int rng_seed = 1;
static int my_rand(void) {
    rng_seed = rng_seed * 1103515245 + 12345;
    return (rng_seed >> 16) & 0x7FFF;
}

#define GRID_W 25
#define GRID_H 20
#define CELL 18
#define WIN_W (GRID_W * CELL + 40)
#define WIN_H (GRID_H * CELL + 40 + WM_TITLEBAR_H)

#define MAX_SNAKE (GRID_W * GRID_H)

typedef struct { int x, y; } pos_t;

static pos_t snake[MAX_SNAKE];
static int snake_len;
static int dx, dy;
static int next_dx, next_dy;
static int food_x, food_y;
static int score;
static int game_over;

static unsigned long last_move;

static void spawn_food(void) {
    int occupied;
    do {
        occupied = 0;
        food_x = (my_rand() % GRID_W);
        food_y = (my_rand() % GRID_H);
        for (int i = 0; i < snake_len; i++) {
            if (snake[i].x == food_x && snake[i].y == food_y) {
                occupied = 1; break;
            }
        }
    } while (occupied);
}

static void reset_game(void) {
    snake_len = 4;
    for (int i = 0; i < snake_len; i++) {
        snake[i].x = GRID_W / 2 - i;
        snake[i].y = GRID_H / 2;
    }
    dx = 1; dy = 0;
    next_dx = 1; next_dy = 0;
    score = 0;
    game_over = 0;
    last_move = 0;
    spawn_food();
}

static void update_snake(void) {
    if (game_over) return;

    dx = next_dx; dy = next_dy;

    int nx = snake[0].x + dx;
    int ny = snake[0].y + dy;

    if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) {
        game_over = 1;
        return;
    }

    for (int i = 0; i < snake_len; i++) {
        if (snake[i].x == nx && snake[i].y == ny) {
            game_over = 1;
            return;
        }
    }

    int ate = (nx == food_x && ny == food_y);

    for (int i = snake_len - 1; i > 0; i--)
        snake[i] = snake[i - 1];
    snake[0].x = nx;
    snake[0].y = ny;

    if (ate) {
        snake_len++;
        snake[snake_len - 1] = snake[snake_len - 2];
        score++;
        spawn_food();
    }
}

static void snake_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)w; (void)vx; (void)vy;
    gpu_accel_fill(x, y, w, h, 0x1a1a2e);

    unsigned long now = timer_get_ms();
    if (now - last_move > 180) {
        update_snake();
        last_move = now;
    }

    int ox = (w - GRID_W * CELL) / 2 + x;
    int oy = (h - GRID_H * CELL) / 2 + y + 6;

    for (int gy = 0; gy < GRID_H; gy++) {
        for (int gx = 0; gx < GRID_W; gx++) {
            uint32_t c = ((gx + gy) & 1) ? 0x1E1E32 : 0x222244;
            gfx_fill_rect(ox + gx * CELL, oy + gy * CELL, CELL - 1, CELL - 1, c);
        }
    }

    for (int i = 0; i < snake_len; i++) {
        float t = (float)i / snake_len;
        int r = 30 + (int)(t * 50);
        int g = 180 - (int)(t * 80);
        int b = 30 + (int)(t * 20);
        uint32_t col = (r << 16) | (g << 8) | b;
        gfx_fill_rect(ox + snake[i].x * CELL + 1, oy + snake[i].y * CELL + 1,
                      CELL - 3, CELL - 3, col);
    }

    gfx_fill_rect(ox + food_x * CELL + 2, oy + food_y * CELL + 2,
                  CELL - 5, CELL - 5, 0xFF4444);

    char score_str[16];
    int si = 0;
    if (game_over) {
        const char* go = "GAME OVER";
        while (*go) score_str[si++] = *go++;
        score_str[si++] = ' ';
    }
    score_str[si++] = 'S';
    score_str[si++] = 'c';
    score_str[si++] = 'o';
    score_str[si++] = 'r';
    score_str[si++] = 'e';
    score_str[si++] = ':';
    score_str[si++] = ' ';
    if (score >= 100) score_str[si++] = '0' + score / 100;
    if (score >= 10) score_str[si++] = '0' + (score / 10) % 10;
    score_str[si++] = '0' + score % 10;
    score_str[si] = 0;
    gfx_draw_string_transparent(x + (w - gfx_strlen(score_str) * 8) / 2, y + 4, score_str, game_over ? 0xFF6666 : 0x8AB4F8);

    if (game_over) {
        gfx_draw_string_transparent(x + (w - 18 * 8) / 2, y + h - 16, "Press R to restart", 0x888888);
    }
}

static void snake_on_key(int id, char key) {
    (void)id;
    if (KEY_MATCH(key, 'r') || KEY_MATCH(key, 'R')) {
        reset_game();
        return;
    }
    if (game_over) return;

    if      (KEY_MATCH(key, KEY_UP)    || KEY_MATCH(key, 'w') || KEY_MATCH(key, 'W')) {
        if (dy != 1) { next_dx = 0; next_dy = -1; }
    }
    else if (KEY_MATCH(key, KEY_DOWN)  || KEY_MATCH(key, 's') || KEY_MATCH(key, 'S')) {
        if (dy != -1) { next_dx = 0; next_dy = 1; }
    }
    else if (KEY_MATCH(key, KEY_LEFT)  || KEY_MATCH(key, 'a') || KEY_MATCH(key, 'A')) {
        if (dx != 1) { next_dx = -1; next_dy = 0; }
    }
    else if (KEY_MATCH(key, KEY_RIGHT) || KEY_MATCH(key, 'd') || KEY_MATCH(key, 'D')) {
        if (dx != -1) { next_dx = 1; next_dy = 0; }
    }
}

void snake_app(void) {
    rng_seed = timer_get_ms();
    reset_game();
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    wm_open_window((fw - WIN_W) / 2, (fh - WIN_H) / 2, WIN_W, WIN_H,
                   "Snake", 0x34D399, snake_render, snake_on_key, 0);
}
