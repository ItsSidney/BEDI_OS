#include "drivers/video/gfx.h"
#include "drivers/video/gpu.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "gui/wm.h"
#include "kernel/time/timer.h"

#define MINE_W 16
#define MINE_H 16
#define MINE_COUNT 40
#define CELL 24
#define PAD 12
#define WIN_W (MINE_W * CELL + PAD * 2)
#define WIN_H (MINE_H * CELL + PAD * 2 + 24 + WM_TITLEBAR_H)

#define STATE_HIDDEN 0
#define STATE_REVEALED 1
#define STATE_FLAGGED 2

static unsigned int mr_seed = 1;
static int mr_rand(void) {
    mr_seed = mr_seed * 1103515245 + 12345;
    return (mr_seed >> 16) & 0x7FFF;
}

static int grid[MINE_H][MINE_W];
static int state[MINE_H][MINE_W];
static int game_over;
static int game_won;
static int first_click;
static int mines_placed;
static int revealed_count;
static int flag_count;
static int prev_mbtn;

static int adj(int y, int x) {
    int c = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (dy == 0 && dx == 0) continue;
            int ny = y + dy, nx = x + dx;
            if (ny >= 0 && ny < MINE_H && nx >= 0 && nx < MINE_W && grid[ny][nx])
                c++;
        }
    return c;
}

static void place_mines(int avoid_y, int avoid_x) {
    int placed = 0;
    while (placed < MINE_COUNT) {
        int y = mr_rand() % MINE_H;
        int x = mr_rand() % MINE_W;
        if (grid[y][x]) continue;
        if (y >= avoid_y - 1 && y <= avoid_y + 1 && x >= avoid_x - 1 && x <= avoid_x + 1) continue;
        grid[y][x] = 1;
        placed++;
    }
    mines_placed = 1;
}

static void reveal(int y, int x) {
    if (y < 0 || y >= MINE_H || x < 0 || x >= MINE_W) return;
    if (state[y][x] != STATE_HIDDEN) return;

    state[y][x] = STATE_REVEALED;
    revealed_count++;

    if (grid[y][x]) {
        game_over = 1;
        return;
    }

    if (adj(y, x) == 0) {
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                reveal(y + dy, x + dx);
    }
}

static void chord(int y, int x) {
    if (state[y][x] != STATE_REVEALED) return;
    if (grid[y][x]) return;
    int flags = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int ny = y + dy, nx = x + dx;
            if (ny >= 0 && ny < MINE_H && nx >= 0 && nx < MINE_W && state[ny][nx] == STATE_FLAGGED)
                flags++;
        }
    if (flags == adj(y, x)) {
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int ny = y + dy, nx = x + dx;
                if (ny >= 0 && ny < MINE_H && nx >= 0 && nx < MINE_W && state[ny][nx] == STATE_HIDDEN)
                    reveal(ny, nx);
            }
    }
}

static void toggle_flag(int y, int x) {
    if (y < 0 || y >= MINE_H || x < 0 || x >= MINE_W) return;
    if (state[y][x] == STATE_HIDDEN) {
        state[y][x] = STATE_FLAGGED;
        flag_count++;
    } else if (state[y][x] == STATE_FLAGGED) {
        state[y][x] = STATE_HIDDEN;
        flag_count--;
    }
}

static int check_win(void) {
    return revealed_count == MINE_W * MINE_H - MINE_COUNT;
}

static void new_game(void) {
    for (int y = 0; y < MINE_H; y++)
        for (int x = 0; x < MINE_W; x++) {
            grid[y][x] = 0;
            state[y][x] = STATE_HIDDEN;
        }
    game_over = 0;
    game_won = 0;
    first_click = 1;
    mines_placed = 0;
    revealed_count = 0;
    flag_count = 0;
}

static void mines_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)w; (void)vx; (void)vy;
    gpu_accel_fill(x, y, w, h, 0x1a1a2e);

    int mbtn = mouse_get_buttons();
    int mx = mouse_get_x();
    int my = mouse_get_y();

    int ox = x + (w - MINE_W * CELL) / 2;
    int oy = y + (h - MINE_H * CELL) / 2;

    if ((mbtn & 1) && !(prev_mbtn & 1)) {
        int gx = (mx - ox) / CELL;
        int gy = (my - oy) / CELL;
        if (gx >= 0 && gx < MINE_W && gy >= 0 && gy < MINE_H && !game_over && !game_won) {
            if (first_click) {
                place_mines(gy, gx);
                first_click = 0;
            }
            if (state[gy][gx] == STATE_HIDDEN) {
                reveal(gy, gx);
                if (game_over) {
                    for (int yy = 0; yy < MINE_H; yy++)
                        for (int xx = 0; xx < MINE_W; xx++)
                            if (grid[yy][xx] && state[yy][xx] == STATE_HIDDEN)
                                state[yy][xx] = STATE_REVEALED;
                } else if (check_win()) {
                    game_won = 1;
                }
            } else if (state[gy][gx] == STATE_REVEALED) {
                chord(gy, gx);
                if (game_over) {
                    for (int yy = 0; yy < MINE_H; yy++)
                        for (int xx = 0; xx < MINE_W; xx++)
                            if (grid[yy][xx] && state[yy][xx] == STATE_HIDDEN)
                                state[yy][xx] = STATE_REVEALED;
                } else if (check_win()) {
                    game_won = 1;
                }
            }
        }
    }
    if ((mbtn & 2) && !(prev_mbtn & 2)) {
        int gx = (mx - ox) / CELL;
        int gy = (my - oy) / CELL;
        if (gx >= 0 && gx < MINE_W && gy >= 0 && gy < MINE_H && !game_over && !game_won) {
            toggle_flag(gy, gx);
        }
    }
    prev_mbtn = mbtn;

    for (int gy = 0; gy < MINE_H; gy++) {
        for (int gx = 0; gx < MINE_W; gx++) {
            int cx = ox + gx * CELL;
            int cy = oy + gy * CELL;
            uint32_t bg;
            if (state[gy][gx] == STATE_REVEALED) {
                bg = (grid[gy][gx] && game_over) ? 0xFF4444 : 0xCCCCCC;
                gfx_fill_rect(cx, cy, CELL - 1, CELL - 1, bg);
                if (grid[gy][gx] && game_over) {
                    gfx_draw_string_transparent(cx + 6, cy + 4, "*", 0x000000);
                } else if (grid[gy][gx]) {
                    gfx_draw_string_transparent(cx + 6, cy + 4, "*", 0x000000);
                } else {
                    int n = adj(gy, gx);
                    if (n > 0) {
                        static const uint32_t nums[] = {0, 0x0000FF, 0x008000, 0xFF0000, 0x000080, 0x800000, 0x008080, 0x000000, 0x808080};
                        uint32_t nc = (n < 9) ? nums[n] : 0x000000;
                        char d[2] = {(char)('0' + n), 0};
                        gfx_draw_string_transparent(cx + 7, cy + 4, d, nc);
                    }
                }
            } else if (state[gy][gx] == STATE_FLAGGED) {
                bg = 0x3A3A5C;
                gfx_fill_rect(cx, cy, CELL - 1, CELL - 1, bg);
                gfx_draw_string_transparent(cx + 5, cy + 4, "F", 0xFF4444);
            } else {
                bg = ((gx + gy) & 1) ? 0x2A2A4A : 0x333355;
                gfx_fill_rect(cx, cy, CELL - 1, CELL - 1, bg);
            }
        }
    }

    char buf[24];
    int bi = 0;
    buf[bi++] = 'M';
    buf[bi++] = 'i';
    buf[bi++] = 'n';
    buf[bi++] = 'e';
    buf[bi++] = 's';
    buf[bi++] = ':';
    buf[bi++] = ' ';
    int rem = MINE_COUNT - flag_count;
    if (rem >= 100) buf[bi++] = '0' + rem / 100;
    if (rem >= 10) buf[bi++] = '0' + (rem / 10) % 10;
    buf[bi++] = '0' + rem % 10;
    buf[bi] = 0;
    gfx_draw_string_transparent(x + PAD, y + 4, buf, 0x8AB4F8);

    if (game_over) {
        gfx_draw_string_transparent(x + PAD, y + h - 16, "Click R to restart", 0xFF6666);
    } else if (game_won) {
        gfx_draw_string_transparent(x + PAD, y + h - 16, "You Win! Click R", 0x34D399);
    }
}

static void mines_on_key(int id, char key) {
    (void)id;
    if (KEY_MATCH(key, 'r') || KEY_MATCH(key, 'R')) {
        new_game();
    }
}

void mines_app(void) {
    mr_seed = timer_get_ms();
    new_game();
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    wm_open_window((fw - WIN_W) / 2, (fh - WIN_H) / 2, WIN_W, WIN_H,
                   "Mines", 0xF87171, mines_render, mines_on_key, 0);
}
