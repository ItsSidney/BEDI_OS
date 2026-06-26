#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/video/gpu.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "kernel/time/timer.h"

#include <string.h>

static unsigned int rng_seed = 1;
static int my_rand(void) {
    rng_seed = rng_seed * 1103515245 + 12345;
    return (rng_seed >> 16) & 0x7FFF;
}

/* Grid/board config */
#define GRID_SIZE 4
#define CELL_SIZE 86
#define CELL_GAP  8
#define BOARD_PAD 24
#define SCORE_PAD 40
#define WIN_W (BOARD_PAD * 2 + GRID_SIZE * CELL_SIZE + (GRID_SIZE - 1) * CELL_GAP)
#define WIN_H (BOARD_PAD * 2 + SCORE_PAD + GRID_SIZE * CELL_SIZE + (GRID_SIZE - 1) * CELL_GAP + WM_TITLEBAR_H)

static uint32_t tile_colors[GRID_SIZE * GRID_SIZE + 1] = {
    0x1E1E32,                       /* 0 empty */
    0x3C3C5A, 0x5C4033, 0x7C4A2E,   /* 2, 4, 8 */
    0xA65D2A, 0xC75B24, 0xE07B24,   /* 16, 32, 64 */
    0xE3B341, 0xE3C441, 0xF2D16B,   /* 128, 256, 512 */
    0xF6D365, 0xF7E37D, 0xF8E78E    /* 1024, 2048, 4096+ */
};
static uint32_t txt_colors[GRID_SIZE * GRID_SIZE + 1] = {
    0x000000,
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
    0xFFFFFF
};

static int board[GRID_SIZE][GRID_SIZE];
static int score;
static int game_over;
static int won;
static int anim_frame;

static unsigned long last_tick;

static int cell_x(int col) {
    return BOARD_PAD + col * (CELL_SIZE + CELL_GAP);
}
static int cell_y(int row) {
    return BOARD_PAD + SCORE_PAD + row * (CELL_SIZE + CELL_GAP);
}

static int tile_index(int v) {
    if (v <= 0) return 0;
    if (v >= 4096) return 12;
    int idx = 0;
    int t = v;
    while (t > 1 && idx < 12) { t >>= 1; idx++; }
    return idx;
}

static void draw_tile(int x, int y, int value) {
    int idx = tile_index(value);
    uint32_t bg = tile_colors[idx];
    uint32_t fg = txt_colors[idx];

    gfx_fill_rect(x + 2, y + 2, CELL_SIZE - 4, CELL_SIZE - 4, bg);
    gfx_draw_rect_outline(x + 2, y + 2, CELL_SIZE - 4, CELL_SIZE - 4, 1, 0x000000);

    if (value == 0) return;

    char buf[8];
    int len = 0;
    int tmp = value;
    if (tmp == 0) { buf[len++] = '0'; }
    else {
        char rev[8]; int rlen = 0;
        while (tmp > 0 && rlen < 7) { rev[rlen++] = '0' + (tmp % 10); tmp /= 10; }
        while (rlen > 0) buf[len++] = rev[--rlen];
    }
    buf[len] = 0;

    int sw = gfx_strlen(buf) * 8;
    int sh = 12;
    int tx = x + (CELL_SIZE - sw) / 2;
    int ty = y + (CELL_SIZE - sh) / 2 - (sh / 2);
    gfx_draw_string_transparent(tx, ty, buf, fg);
}

static void spawn_tile(void) {
    int empty[GRID_SIZE * GRID_SIZE][2];
    int count = 0;
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            if (board[r][c] == 0) {
                empty[count][0] = r;
                empty[count][1] = c;
                count++;
            }
    if (count == 0) return;
    int pick = my_rand() % count;
    int r = empty[pick][0];
    int c = empty[pick][1];
    board[r][c] = (my_rand() % 10) == 0 ? 4 : 2;
}

static void reset_game(void) {
    memset(board, 0, sizeof(board));
    score = 0;
    game_over = 0;
    won = 0;
    anim_frame = 0;
    last_tick = 0;
    spawn_tile();
    spawn_tile();
}

static int slide_row_left(int row[GRID_SIZE]) {
    int moved = 0;
    int tmp[GRID_SIZE] = {0};
    int pos = 0;

    for (int i = 0; i < GRID_SIZE; i++) {
        if (row[i] == 0) continue;
        tmp[pos++] = row[i];
    }

    int merged[GRID_SIZE] = {0};
    for (int i = 0; i < GRID_SIZE - 1; i++) {
        if (tmp[i] && tmp[i] == tmp[i + 1]) {
            tmp[i] *= 2;
            score += tmp[i];
            if (tmp[i] == 2048 && !won) won = 1;
            tmp[i + 1] = 0;
            merged[i] = 1;
        }
    }

    int out[GRID_SIZE] = {0};
    pos = 0;
    for (int i = 0; i < GRID_SIZE; i++) {
        if (tmp[i] == 0) continue;
        out[pos++] = tmp[i];
    }

    for (int i = 0; i < GRID_SIZE; i++) {
        if (row[i] != out[i]) moved = 1;
        row[i] = out[i];
    }
    return moved;
}

static int rotate_board(void) {
    int tmp[GRID_SIZE][GRID_SIZE];
    memcpy(tmp, board, sizeof(board));
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            board[c][GRID_SIZE - 1 - r] = tmp[r][c];
    return 1;
}

static int move(int dir) {
    if (game_over) return 0;
    int moved = 0;

    /* dir: 0=up,1=right,2=down,3=left -> rotate so target row becomes left */
    int rotations = 0;
    if (dir == 0) rotations = 3;      /* up    -> rotate 3x -> left */
    else if (dir == 1) rotations = 2; /* right -> rotate 2x -> left */
    else if (dir == 2) rotations = 1; /* down  -> rotate 1x -> left */
    else if (dir == 3) rotations = 0; /* left  -> already left */

    for (int i = 0; i < rotations; i++) rotate_board();

    for (int r = 0; r < GRID_SIZE; r++) {
        if (slide_row_left(board[r])) moved = 1;
    }

    for (int i = 0; i < (4 - rotations) % 4; i++) rotate_board();

    if (moved) {
        spawn_tile();
        if (won) game_over = 1;
        else {
            int can = 0;
            for (int r = 0; r < GRID_SIZE && !can; r++)
                for (int c = 0; c < GRID_SIZE && !can; c++) {
                    if (board[r][c] == 0) { can = 1; break; }
                    int v = board[r][c];
                    if (c + 1 < GRID_SIZE && board[r][c + 1] == v) can = 1;
                    if (r + 1 < GRID_SIZE && board[r + 1][c] == v) can = 1;
                }
            if (!can) game_over = 1;
        }
    }
    return moved;
}

static void game2048_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    gpu_accel_fill(x, y, w, h, 0x0f1729);

    unsigned long now = timer_get_ms();
    if (now - last_tick > 35) {
        anim_frame++;
        last_tick = now;
    }

    int ox = x + (w - (GRID_SIZE * CELL_SIZE + (GRID_SIZE - 1) * CELL_GAP)) / 2;
    int oy = y + (h - (GRID_SIZE * CELL_SIZE + (GRID_SIZE - 1) * CELL_GAP)) / 2 + 6;

    /* Score / status */
    char status[48];
    int si = 0;
    if (game_over) {
        const char* go = won ? "YOU WIN!" : "GAME OVER";
        while (*go) status[si++] = *go++;
        status[si++] = ' ';
    }
    status[si++] = 'S'; status[si++] = 'c'; status[si++] = 'o';
    status[si++] = 'r'; status[si++] = 'e'; status[si++] = ':'; status[si++] = ' ';
    if (score >= 10000) status[si++] = '0' + score / 10000;
    if (score >= 1000)  status[si++] = '0' + (score / 1000) % 10;
    if (score >= 100)   status[si++] = '0' + (score / 100) % 10;
    if (score >= 10)    status[si++] = '0' + (score / 10) % 10;
    status[si++] = '0' + score % 10;
    status[si] = 0;

    gfx_draw_string_transparent(
        x + (w - gfx_strlen(status) * 8) / 2,
        y + 8,
        status,
        game_over ? 0xFF6666 : 0x8AB4F8
    );

    /* Board background */
    gfx_fill_rect(ox - 8, oy - 8,
                  GRID_SIZE * CELL_SIZE + (GRID_SIZE - 1) * CELL_GAP + 16,
                  GRID_SIZE * CELL_SIZE + (GRID_SIZE - 1) * CELL_GAP + 16,
                  0x1C2438);

    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            int cx = ox + c * (CELL_SIZE + CELL_GAP);
            int cy = oy + r * (CELL_SIZE + CELL_GAP);
            draw_tile(cx, cy, board[r][c]);
        }
    }

    if (game_over) {
        gfx_draw_string_transparent(
            x + (w - 18 * 8) / 2,
            y + h - 20,
            "Press R to restart",
            0x888888
        );
    }
}

static void game2048_on_key(int id, char key) {
    (void)id;
    if (KEY_MATCH(key, 'r') || KEY_MATCH(key, 'R')) { reset_game(); return; }
    if (game_over) return;

    if      (KEY_MATCH(key, KEY_UP))    move(0);
    else if (KEY_MATCH(key, KEY_RIGHT)) move(1);
    else if (KEY_MATCH(key, KEY_DOWN))  move(2);
    else if (KEY_MATCH(key, KEY_LEFT))  move(3);
    else if (KEY_MATCH(key, 'w') || KEY_MATCH(key, 'W')) move(0);
    else if (KEY_MATCH(key, 'd') || KEY_MATCH(key, 'D')) move(1);
    else if (KEY_MATCH(key, 's') || KEY_MATCH(key, 'S')) move(2);
    else if (KEY_MATCH(key, 'a') || KEY_MATCH(key, 'A')) move(3);
}

void game_2048_app(void) {
    rng_seed = timer_get_ms();
    reset_game();
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = WIN_W > (int)fw - 80 ? (int)fw - 80 : WIN_W;
    int win_h = WIN_H > (int)fh - 80 ? (int)fh - 80 : WIN_H;
    wm_open_window((fw - win_w) / 2, (fh - win_h) / 2, win_w, win_h,
                   "2048", 0xE3B341, game2048_render, game2048_on_key, 0);
}
