#include "drivers/video/gfx.h"
#include "drivers/video/gpu.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "kernel/time/timer.h"

#define COLS 10
#define ROWS 20
#define CELL 24
#define BOARD_W (COLS * CELL)
#define BOARD_H (ROWS * CELL)
#define PANEL_W 120
#define WIN_W (BOARD_W + PANEL_W + 6)
#define WIN_H (BOARD_H + 8 + WM_TITLEBAR_H)

static int board[ROWS][COLS];
static int score, lines, level;
static int game_over;
static unsigned long last_drop;
static int drop_interval;

static int cur_type, cur_rot, cur_x, cur_y;
static int next_type;

static unsigned int tr_seed = 1;
static int tr_rand(void) {
    tr_seed = tr_seed * 1103515245 + 12345;
    return (tr_seed >> 16) & 0x7FFF;
}

static const int pieces[7][4][16] = {
    {{0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0},{0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0},{0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0},{0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0}},
    {{1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0},{1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0},{1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0},{1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0}},
    {{0,1,0,0,1,1,1,0,0,0,0,0,0,0,0,0},{0,1,0,0,0,1,1,0,0,1,0,0,0,0,0,0},{0,0,0,0,1,1,1,0,0,1,0,0,0,0,0,0},{0,1,0,0,1,1,0,0,0,1,0,0,0,0,0,0}},
    {{0,1,1,0,1,1,0,0,0,0,0,0,0,0,0,0},{0,1,0,0,0,1,1,0,0,0,1,0,0,0,0,0},{0,0,0,0,0,1,1,0,1,1,0,0,0,0,0,0},{1,0,0,0,1,1,0,0,0,1,0,0,0,0,0,0}},
    {{1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0},{0,0,1,0,0,1,1,0,0,1,0,0,0,0,0,0},{0,0,0,0,1,1,0,0,0,1,1,0,0,0,0,0},{0,1,0,0,1,1,0,0,1,0,0,0,0,0,0,0}},
    {{1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0},{0,1,1,0,0,1,0,0,0,1,0,0,0,0,0,0},{0,0,0,0,1,1,1,0,0,0,1,0,0,0,0,0},{0,1,0,0,0,1,0,0,0,1,1,0,0,0,0,0}},
    {{0,0,1,0,1,1,1,0,0,0,0,0,0,0,0,0},{0,1,0,0,0,1,0,0,0,1,1,0,0,0,0,0},{0,0,0,0,1,1,1,0,1,0,0,0,0,0,0,0},{1,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0}},
};

static const uint32_t piece_colors[] = {0x00F0F0,0xF0F000,0xA000F0,0x00F000,0xF00000,0x0000F0,0xF0A000};


static int get_cell(int type, int rot, int i) { return pieces[type][rot][i]; }

static int collides(int type, int rot, int px, int py) {
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (get_cell(type, rot, y * 4 + x)) {
                int bx = px + x, by = py + y;
                if (bx < 0 || bx >= COLS || by >= ROWS) return 1;
                if (by >= 0 && board[by][bx]) return 1;
            }
    return 0;
}

static void lock(void) {
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (get_cell(cur_type, cur_rot, y * 4 + x)) {
                int bx = cur_x + x, by = cur_y + y;
                if (by >= 0 && by < ROWS && bx >= 0 && bx < COLS)
                    board[by][bx] = cur_type + 1;
            }
    int cleared = 0;
    for (int y = ROWS - 1; y >= 0; y--) {
        int full = 1;
        for (int x = 0; x < COLS; x++) if (!board[y][x]) { full = 0; break; }
        if (full) {
            for (int r = y; r > 0; r--)
                for (int x = 0; x < COLS; x++)
                    board[r][x] = board[r - 1][x];
            for (int x = 0; x < COLS; x++) board[0][x] = 0;
            cleared++;
            y++;
        }
    }
    if (cleared > 0) {
        lines += cleared;
        score += cleared * 100 * (cleared + 1) / 2;
        level = lines / 10;
        drop_interval = 500 - level * 30;
        if (drop_interval < 80) drop_interval = 80;
    }
}

static void spawn(void) {
    cur_type = next_type;
    cur_rot = 0;
    cur_x = COLS / 2 - 2;
    cur_y = -1;
    next_type = tr_rand() % 7;
    if (collides(cur_type, cur_rot, cur_x, cur_y)) {
        game_over = 1;
    }
}

static void rotate(void) {
    int nr = (cur_rot + 1) % 4;
    if (!collides(cur_type, nr, cur_x, cur_y))
        cur_rot = nr;
}

static int ghost_y(void) {
    int gy = cur_y;
    while (!collides(cur_type, cur_rot, cur_x, gy + 1)) gy++;
    return gy;
}

static void move(int dx, int dy) {
    if (!collides(cur_type, cur_rot, cur_x + dx, cur_y + dy)) {
        cur_x += dx; cur_y += dy;
    } else if (dy == 1) {
        lock();
        spawn();
    }
}

static void hard_drop(void) {
    while (!collides(cur_type, cur_rot, cur_x, cur_y + 1)) cur_y++;
    lock();
    spawn();
}

static void new_game(void) {
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            board[y][x] = 0;
    score = 0; lines = 0; level = 0;
    drop_interval = 500;
    game_over = 0;
    cur_type = tr_rand() % 7;
    next_type = tr_rand() % 7;
    cur_rot = 0;
    cur_x = COLS / 2 - 2;
    cur_y = -1;
    last_drop = 0;
}

static void draw_piece(int ox, int oy, int type, int rot, int cell_size) {
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (get_cell(type, rot, y * 4 + x))
                gfx_fill_rect(ox + x * cell_size + 1, oy + y * cell_size + 1,
                              cell_size - 2, cell_size - 2, piece_colors[type]);
}

static void tetris_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)w; (void)vx; (void)vy;
    gpu_accel_fill(x, y, w, h, 0x0D0D1A);

    unsigned long now = timer_get_ms();
    if (!game_over && now - last_drop > (unsigned long)drop_interval) {
        move(0, 1);
        last_drop = now;
    }

    int bx = x + 4;
    int by = y + 4;

    gfx_draw_rect_outline(bx - 1, by - 1, BOARD_W + 2, BOARD_H + 2, 1, 0x2A2A4A);

    for (int gy = 0; gy < ROWS; gy++)
        for (int gx = 0; gx < COLS; gx++) {
            uint32_t c = ((gx + gy) & 1) ? 0x15152A : 0x1A1A30;
            gfx_fill_rect(bx + gx * CELL, by + gy * CELL, CELL - 1, CELL - 1, c);
        }

    for (int gy = 0; gy < ROWS; gy++)
        for (int gx = 0; gx < COLS; gx++)
            if (board[gy][gx])
                gfx_fill_rect(bx + gx * CELL + 1, by + gy * CELL + 1,
                              CELL - 2, CELL - 2, piece_colors[board[gy][gx] - 1]);

    if (!game_over) {
        int gy = ghost_y();
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                if (get_cell(cur_type, cur_rot, i * 4 + j)) {
                    int px = cur_x + j, py = gy + i;
                    if (py >= 0)
                        gfx_draw_rect_outline(bx + px * CELL + 1, by + py * CELL + 1,
                                              CELL - 2, CELL - 2, 1, 0x555577);
                }

        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                if (get_cell(cur_type, cur_rot, i * 4 + j)) {
                    int px = cur_x + j, py = cur_y + i;
                    if (py >= 0)
                        gfx_fill_rect(bx + px * CELL + 1, by + py * CELL + 1,
                                      CELL - 2, CELL - 2, piece_colors[cur_type]);
                }
    }

    int px = x + BOARD_W + 10;
    gfx_draw_string_transparent(px, y + 8, "NEXT", 0x666688);
    draw_piece(px + 4, y + 24, next_type, 0, 18);

    char buf[24];
    int si;
    gfx_draw_string_transparent(px, y + 100, "SCORE", 0x666688);
    si = 0;
    int s = score;
    if (s >= 100000) buf[si++] = '0' + s / 100000;
    if (s >= 10000) buf[si++] = '0' + (s / 10000) % 10;
    if (s >= 1000) buf[si++] = '0' + (s / 1000) % 10;
    if (s >= 100) buf[si++] = '0' + (s / 100) % 10;
    if (s >= 10) buf[si++] = '0' + (s / 10) % 10;
    buf[si++] = '0' + s % 10;
    buf[si] = 0;
    gfx_draw_string_transparent(px, y + 116, buf, 0x34D399);

    gfx_draw_string_transparent(px, y + 144, "LINES", 0x666688);
    si = 0;
    int ln = lines;
    if (ln >= 100) buf[si++] = '0' + ln / 100;
    if (ln >= 10) buf[si++] = '0' + (ln / 10) % 10;
    buf[si++] = '0' + ln % 10;
    buf[si] = 0;
    gfx_draw_string_transparent(px, y + 160, buf, 0x8AB4F8);

    if (game_over) {
        gfx_draw_string_transparent(px, y + 200, "GAME OVER", 0xF87171);
        gfx_draw_string_transparent(px, y + 216, "R=restart", 0x666688);
    }
}

static void tetris_on_key(int id, char key) {
    (void)id;
    if (KEY_MATCH(key, 'r') || KEY_MATCH(key, 'R')) { new_game(); return; }
    if (game_over) return;
    if      (KEY_MATCH(key, KEY_LEFT))  move(-1, 0);
    else if (KEY_MATCH(key, KEY_RIGHT)) move(1, 0);
    else if (KEY_MATCH(key, KEY_DOWN))  move(0, 1);
    else if (KEY_MATCH(key, KEY_UP))    rotate();
    else if (key == ' ')                hard_drop();
}

void tetris_app(void) {
    tr_seed = timer_get_ms();
    new_game();
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    wm_open_window((fw - WIN_W) / 2, (fh - WIN_H) / 2, WIN_W, WIN_H,
                   "Tetris", 0x00F0F0, tetris_render, tetris_on_key, 0);
}
