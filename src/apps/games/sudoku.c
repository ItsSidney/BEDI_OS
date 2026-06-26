#include "drivers/video/gfx.h"
#include "drivers/video/gpu.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/mouse.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "kernel/time/timer.h"
#include <string.h>

#define SUDOKU_SIZE 9
#define CELL 36
#define PAD 14
#define WIN_W (SUDOKU_SIZE * CELL + PAD * 2)
#define WIN_H (SUDOKU_SIZE * CELL + PAD * 2 + 24 + WM_TITLEBAR_H)

#define STATE_FIXED 0
#define STATE_USER 1
#define STATE_EMPTY 2

static int solution[SUDOKU_SIZE][SUDOKU_SIZE];
static int board[SUDOKU_SIZE][SUDOKU_SIZE];
static int state[SUDOKU_SIZE][SUDOKU_SIZE];
static int sel_x, sel_y;
static int game_won;
static int prev_mbtn;

static unsigned int sd_seed = 1;
static int sd_rand(void) {
    sd_seed = sd_seed * 1103515245 + 12345;
    return (sd_seed >> 16) & 0x7FFF;
}

static void shuffle_rows_in_band(int b[SUDOKU_SIZE][SUDOKU_SIZE]) {
    for (int band = 0; band < 3; band++) {
        int r0 = band * 3;
        for (int i = 2; i > 0; i--) {
            int j = sd_rand() % (i + 1);
            for (int x = 0; x < SUDOKU_SIZE; x++) {
                int tmp = b[r0 + i][x];
                b[r0 + i][x] = b[r0 + j][x];
                b[r0 + j][x] = tmp;
            }
        }
    }
}

static void shuffle_cols_in_stack(int b[SUDOKU_SIZE][SUDOKU_SIZE]) {
    for (int stack = 0; stack < 3; stack++) {
        int c0 = stack * 3;
        for (int i = 2; i > 0; i--) {
            int j = sd_rand() % (i + 1);
            for (int y = 0; y < SUDOKU_SIZE; y++) {
                int tmp = b[y][c0 + i];
                b[y][c0 + i] = b[y][c0 + j];
                b[y][c0 + j] = tmp;
            }
        }
    }
}

static void shuffle_bands(int b[SUDOKU_SIZE][SUDOKU_SIZE]) {
    for (int i = 2; i > 0; i--) {
        int j = sd_rand() % (i + 1);
        for (int r = 0; r < 3; r++) {
            int ri = i * 3 + r;
            int rj = j * 3 + r;
            for (int x = 0; x < SUDOKU_SIZE; x++) {
                int tmp = b[ri][x];
                b[ri][x] = b[rj][x];
                b[rj][x] = tmp;
            }
        }
    }
}

static void shuffle_stacks(int b[SUDOKU_SIZE][SUDOKU_SIZE]) {
    for (int i = 2; i > 0; i--) {
        int j = sd_rand() % (i + 1);
        for (int c = 0; c < 3; c++) {
            int ci = i * 3 + c;
            int cj = j * 3 + c;
            for (int y = 0; y < SUDOKU_SIZE; y++) {
                int tmp = b[y][ci];
                b[y][ci] = b[y][cj];
                b[y][cj] = tmp;
            }
        }
    }
}

static void shuffle_digits(int b[SUDOKU_SIZE][SUDOKU_SIZE]) {
    int a = sd_rand() % 9 + 1;
    int d = sd_rand() % 9 + 1;
    while (d == a) d = sd_rand() % 9 + 1;
    for (int y = 0; y < SUDOKU_SIZE; y++) {
        for (int x = 0; x < SUDOKU_SIZE; x++) {
            if (b[y][x] == a) b[y][x] = d;
            else if (b[y][x] == d) b[y][x] = a;
        }
    }
}

static void init_puzzle(void) {
    static const int solved[SUDOKU_SIZE][SUDOKU_SIZE] = {
        {1,2,3, 4,5,6, 7,8,9},
        {4,5,6, 7,8,9, 1,2,3},
        {7,8,9, 1,2,3, 4,5,6},
        {2,3,4, 5,6,7, 8,9,1},
        {5,6,7, 8,9,1, 2,3,4},
        {8,9,1, 2,3,4, 5,6,7},
        {3,4,5, 6,7,8, 9,1,2},
        {6,7,8, 9,1,2, 3,4,5},
        {9,1,2, 3,4,5, 6,7,8}
    };
    int base[SUDOKU_SIZE][SUDOKU_SIZE];
    memcpy(base, solved, sizeof(solved));

    shuffle_rows_in_band(base);
    shuffle_cols_in_stack(base);
    shuffle_bands(base);
    shuffle_stacks(base);
    shuffle_digits(base);

    memcpy(solution, base, sizeof(base));
    memcpy(board, base, sizeof(base));

    int cells_to_remove = 35 + sd_rand() % 16; /* 35..50 */
    int removed = 0;
    while (removed < cells_to_remove) {
        int y = sd_rand() % SUDOKU_SIZE;
        int x = sd_rand() % SUDOKU_SIZE;
        if (board[y][x] != 0) {
            board[y][x] = 0;
            state[y][x] = STATE_EMPTY;
            removed++;
        }
    }

    /* Ensure fixed states for non-empty cells */
    for (int y = 0; y < SUDOKU_SIZE; y++) {
        for (int x = 0; x < SUDOKU_SIZE; x++) {
            if (board[y][x] != 0) state[y][x] = STATE_FIXED;
        }
    }

    sel_x = -1; sel_y = -1;
    game_won = 0;
    prev_mbtn = 0;
}

static int in_same_box(int y1, int x1, int y2, int x2) {
    return (y1 / 3) == (y2 / 3) && (x1 / 3) == (x2 / 3);
}

static int check_win(void) {
    for (int y = 0; y < SUDOKU_SIZE; y++) {
        for (int x = 0; x < SUDOKU_SIZE; x++) {
            if (board[y][x] == 0) return 0;
            if (board[y][x] != solution[y][x]) return 0;
        }
    }
    return 1;
}

static void sudoku_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)w; (void)vx; (void)vy;
    gpu_accel_fill(x, y, w, h, 0x0D0D1A);

    int mbtn = mouse_get_buttons();
    int mx = mouse_get_x();
    int my = mouse_get_y();

    int ox = x + (w - (SUDOKU_SIZE * CELL)) / 2;
    int oy = y + PAD;

    /* Mouse handling */
    if ((mbtn & 1) && !(prev_mbtn & 1)) {
        int rel_x = mx - ox;
        int rel_y = my - oy;
        if (rel_x >= 0 && rel_x < SUDOKU_SIZE * CELL && rel_y >= 0 && rel_y < SUDOKU_SIZE * CELL) {
            sel_x = rel_x / CELL;
            sel_y = rel_y / CELL;
        }
    }
    prev_mbtn = mbtn;

    /* Draw cell backgrounds */
    for (int cy = 0; cy < SUDOKU_SIZE; cy++) {
        for (int cx = 0; cx < SUDOKU_SIZE; cx++) {
            int px = ox + cx * CELL;
            int py = oy + cy * CELL;
            uint32_t bg = 0x0D0D1A;

            if (sel_x == cx && sel_y == cy) {
                bg = 0x1E1E3A;
            } else if (sel_x >= 0 && (cx == sel_x || cy == sel_y || in_same_box(cy, cx, sel_y, sel_x))) {
                bg = 0x151528;
            }

            gfx_fill_rect(px, py, CELL, CELL, bg);
        }
    }

    /* Draw grids */
    for (int i = 0; i <= SUDOKU_SIZE; i++) {
        int thick = ((i % 3) == 0) ? 2 : 1;
        uint32_t clr = ((i % 3) == 0) ? 0x666688 : 0x333355;

        /* vertical */
        gfx_draw_rect_outline(ox + i * CELL, oy, thick, SUDOKU_SIZE * CELL, thick, clr);
        /* horizontal */
        gfx_draw_rect_outline(ox, oy + i * CELL, SUDOKU_SIZE * CELL, thick, thick, clr);
    }

    /* Draw numbers */
    for (int cy = 0; cy < SUDOKU_SIZE; cy++) {
        for (int cx = 0; cx < SUDOKU_SIZE; cx++) {
            if (board[cy][cx] == 0) continue;
            int px = ox + cx * CELL + CELL / 2 - 4;
            int py = oy + cy * CELL + CELL / 2 - 4;
            char buf[2] = { '0' + board[cy][cx], 0 };
            uint32_t clr = (state[cy][cx] == STATE_FIXED) ? 0xE4E6EA : 0x8AB4F8;
            gfx_draw_string_transparent(px, py, buf, clr);
        }
    }

    char buf[24];
    int si = 0;
    buf[si++] = 'M'; buf[si++] = 'o'; buf[si++] = 'v'; buf[si++] = 'e'; buf[si++] = 's'; buf[si++] = ':'; buf[si++] = ' ';
    /* simple move counter based on user entries */
    int moves = 0;
    for (int y = 0; y < SUDOKU_SIZE; y++)
        for (int x = 0; x < SUDOKU_SIZE; x++)
            if (state[y][x] == STATE_USER) moves++;
    if (moves >= 100) buf[si++] = '0' + moves / 100;
    if (moves >= 10)  buf[si++] = '0' + (moves / 10) % 10;
    buf[si++] = '0' + moves % 10;
    buf[si] = 0;
    gfx_draw_string_transparent(x + PAD, y + h - 14, buf, 0x8AB4F8);

    if (game_won) {
        const char* again = "N=new game";
        int aw = gfx_strlen(again) * 8;
        gfx_draw_string_transparent(x + w - aw - PAD, y + h - 14, again, 0x34D399);
    }
}

static void sudoku_on_key(int id, char key) {
    (void)id;
    if (sel_x < 0 || sel_y < 0) return;
    if (game_won) {
        if (KEY_MATCH(key, 'n') || KEY_MATCH(key, 'N')) init_puzzle();
        return;
    }
    if (key >= '1' && key <= '9') {
        if (state[sel_y][sel_x] != STATE_FIXED) {
            board[sel_y][sel_x] = key - '0';
            state[sel_y][sel_x] = STATE_USER;
            if (check_win()) game_won = 1;
        }
    } else if (key == '\b' || key == 127) {
        if (state[sel_y][sel_x] != STATE_FIXED) {
            board[sel_y][sel_x] = 0;
            state[sel_y][sel_x] = STATE_EMPTY;
        }
    } else if (KEY_MATCH(key, 'n') || KEY_MATCH(key, 'N')) {
        init_puzzle();
    }
}

void sudoku_app(void) {
    sd_seed = timer_get_ms();
    init_puzzle();
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    wm_open_window((fw - WIN_W) / 2, (fh - WIN_H) / 2, WIN_W, WIN_H,
                   "Sudoku", 0x8AB4F8, sudoku_render, sudoku_on_key, 0);
}
