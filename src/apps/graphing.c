#include "gui/gui.h"
#include "gui/wm.h"
#include "drivers/video/gfx.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "drivers/video/framebuffer.h"
#include "kernel/time/timer.h"
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define GR_WIN_W 960
#define GR_WIN_H 680

#define MAX_EXPR 8
#define EXPR_LEN 128
#define MAX_PTS 800
#define MAX_PARAMS 6

#define PANEL_X 12
#define PANEL_Y 12
#define PANEL_W 300
#define EXPR_ROW_H 34
#define PARAM_ROW_H 26
#define BOTTOM_BAR_H 30

#define PI 3.14159265358979323846
#define EULER 2.71828182845904523536

static const uint32_t gr_colors[] = {
    0xE53935, 0x1E88E5, 0x43A047, 0xFB8C00,
    0x8E24AA, 0x00ACC1, 0xF4511E, 0x3949AB
};
#define COLORS_COUNT (sizeof(gr_colors)/sizeof(gr_colors[0]))

typedef enum {
    TK_EOF, TK_NUM, TK_X, TK_PARAM, TK_CONST,
    TK_PLUS, TK_MINUS, TK_MUL, TK_DIV, TK_POW,
    TK_LP, TK_RP, TK_EQ,
    TK_SIN, TK_COS, TK_TAN, TK_SQRT, TK_ABS, TK_LOG, TK_LN, TK_EXP
} tk_t;

typedef struct {
    char text[EXPR_LEN];
    int len;
    int enabled;
    uint32_t color;
    double pts[MAX_PTS];
    int pt_count;
    int parse_ok;
    char error[64];
    double params[MAX_PARAMS];
    double param_min[MAX_PARAMS];
    double param_max[MAX_PARAMS];
    int param_count;
    char param_names[MAX_PARAMS];
    int editing;
    int cursor;
} Expr;

static int gr_win_id = -1;
static Expr gr_exprs[MAX_EXPR];
static int gr_expr_count;
static int gr_active;
static int gr_add_mode;
static char gr_add_buf[EXPR_LEN];
static int gr_add_len;
static int gr_add_cursor;

static double gr_min_x = -10, gr_max_x = 10;
static double gr_min_y = -10, gr_max_y = 10;
static int gr_auto_y_enabled;
static int gr_show_grid = 1;
static int gr_show_axes = 1;
static int gr_show_coords;
static double gr_coord_x, gr_coord_y;

static int gr_drag_pan;
static int gr_drag_slider;
static int gr_drag_slider_expr;
static int gr_drag_slider_idx;
static int gr_drag_pan_sx, gr_drag_pan_sy;
static double gr_drag_pan_mnx, gr_drag_pan_mxx, gr_drag_pan_mny, gr_drag_pan_mxy;

static int gr_hover_expr = -1;
static int gr_hover_delete = -1;
static int gr_hover_toggle = -1;
static int gr_hover_color = -1;
static int gr_hover_slider = -1;
static int gr_hover_slider_expr = -1;
static int gr_hover_slider_idx = -1;

static int gr_help_shown;

/* ---- Parser ---- */
static const char* gp_src;
static int gp_pos;
static double gp_xval;
static double gp_pvals[26];
static int gp_ok;
static char gp_err[128];
static tk_t gp_tok;
static double gp_tokval;
static int gp_used_params[26];
static int gp_need_implicit;

static void gp_next(void) {
    while (gp_src[gp_pos] == ' ' || gp_src[gp_pos] == '\t') gp_pos++;
    char c = gp_src[gp_pos];

    if (gp_need_implicit) {
        if ((c >= '0' && c <= '9') || c == '.' || c == '(' ||
            c == 'x' || c == 'X' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            gp_need_implicit = 0;
            gp_tok = TK_MUL;
            return;
        }
    }
    gp_need_implicit = 0;

    if (!c) { gp_tok = TK_EOF; return; }
    if (c == '+') { gp_pos++; gp_tok = TK_PLUS; return; }
    if (c == '-') { gp_pos++; gp_tok = TK_MINUS; return; }
    if (c == '*') { gp_pos++; gp_tok = TK_MUL; return; }
    if (c == '/') { gp_pos++; gp_tok = TK_DIV; return; }
    if (c == '^') { gp_pos++; gp_tok = TK_POW; return; }
    if (c == '(') { gp_pos++; gp_tok = TK_LP; gp_need_implicit = 1; return; }
    if (c == ')') { gp_pos++; gp_tok = TK_RP; gp_need_implicit = 1; return; }
    if (c == '=') { gp_pos++; gp_tok = TK_EQ; return; }
    if (c == 'x' || c == 'X') { gp_pos++; gp_tok = TK_X; gp_need_implicit = 1; return; }
    if ((c >= '0' && c <= '9') || c == '.') {
        char buf[64]; int bi = 0;
        while (bi < 63 && ((gp_src[gp_pos] >= '0' && gp_src[gp_pos] <= '9') || gp_src[gp_pos] == '.'))
            buf[bi++] = gp_src[gp_pos++];
        buf[bi] = 0;
        gp_tokval = atof(buf);
        gp_tok = TK_NUM;
        gp_need_implicit = 1;
        return;
    }
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        char buf[32]; int bi = 0;
        while (bi < 31 && ((gp_src[gp_pos] >= 'a' && gp_src[gp_pos] <= 'z') || (gp_src[gp_pos] >= 'A' && gp_src[gp_pos] <= 'Z')))
            buf[bi++] = gp_src[gp_pos++];
        buf[bi] = 0;

        if      (!strcmp(buf,"sin"))  { gp_tok = TK_SIN; gp_need_implicit = 1; return; }
        else if (!strcmp(buf,"cos"))  { gp_tok = TK_COS; gp_need_implicit = 1; return; }
        else if (!strcmp(buf,"tan"))  { gp_tok = TK_TAN; gp_need_implicit = 1; return; }
        else if (!strcmp(buf,"sqrt")) { gp_tok = TK_SQRT; gp_need_implicit = 1; return; }
        else if (!strcmp(buf,"abs"))  { gp_tok = TK_ABS; gp_need_implicit = 1; return; }
        else if (!strcmp(buf,"log"))  { gp_tok = TK_LOG; gp_need_implicit = 1; return; }
        else if (!strcmp(buf,"ln"))   { gp_tok = TK_LN; gp_need_implicit = 1; return; }
        else if (!strcmp(buf,"exp"))  { gp_tok = TK_EXP; gp_need_implicit = 1; return; }
        else if (!strcmp(buf,"pi"))   { gp_tokval = PI; gp_tok = TK_NUM; gp_need_implicit = 1; return; }
        else if (!strcmp(buf,"e"))    { gp_tokval = EULER; gp_tok = TK_NUM; gp_need_implicit = 1; return; }

        if (bi == 1) {
            char p = buf[0];
            if (p >= 'a' && p <= 'z') {
                gp_used_params[p - 'a'] = 1;
                gp_tok = TK_PARAM;
                gp_tokval = p;
                gp_need_implicit = 1;
                return;
            }
        }
        snprintf(gp_err,128,"Unknown: %s", buf);
        gp_tok = TK_EOF; gp_ok = 0;
        return;
    }
    snprintf(gp_err,128,"Unexpected '%c'", c);
    gp_tok = TK_EOF; gp_ok = 0;
}

static double gp_expr(void);

static double gp_prim(void) {
    if (gp_tok == TK_NUM) { double v = gp_tokval; gp_next(); return v; }
    if (gp_tok == TK_X)   { gp_next(); return gp_xval; }
    if (gp_tok == TK_PARAM) {
        int idx = (int)gp_tokval - 'a';
        gp_next();
        if (idx >= 0 && idx < 26) return gp_pvals[idx];
        return 0;
    }
    if (gp_tok == TK_CONST) { double v = gp_tokval; gp_next(); return v; }
    if (gp_tok == TK_MINUS) { gp_next(); return -gp_prim(); }
    if (gp_tok == TK_PLUS)  { gp_next(); return gp_prim(); }
    if (gp_tok == TK_LP) {
        gp_next(); double v = gp_expr();
        if (gp_tok == TK_RP) gp_next();
        else { snprintf(gp_err,128,"Missing )"); gp_ok = 0; }
        return v;
    }
    tk_t fn = gp_tok;
    if (fn >= TK_SIN && fn <= TK_EXP) {
        gp_next();
        int parens = 0;
        if (gp_tok == TK_LP) { gp_next(); parens = 1; }
        double a = gp_expr();
        if (parens && gp_tok == TK_RP) gp_next();
        switch (fn) {
            case TK_SIN:  return sin(a);
            case TK_COS:  return cos(a);
            case TK_TAN:  return tan(a);
            case TK_SQRT: if (a<0) { snprintf(gp_err,128,"sqrt(neg)"); gp_ok=0; return 0; } return sqrt(a);
            case TK_ABS:  return fabs(a);
            case TK_LOG:  return a>0 ? log10(a) : 0;
            case TK_LN:   return a>0 ? log(a) : 0;
            case TK_EXP:  return exp(a);
            default: return 0;
        }
    }
    snprintf(gp_err,128,"Syntax error");
    gp_ok = 0;
    return 0;
}

static double gp_pow(void) {
    double v = gp_prim();
    if (gp_tok == TK_POW) { gp_next(); v = pow(v, gp_pow()); }
    return v;
}

static double gp_mul(void) {
    double v = gp_pow();
    while (gp_tok == TK_MUL || gp_tok == TK_DIV) {
        tk_t op = gp_tok; gp_next();
        double r = gp_pow();
        v = (op == TK_MUL) ? v * r : (r != 0 ? v / r : 0);
    }
    return v;
}

static double gp_expr(void) {
    double v = gp_mul();
    while (gp_tok == TK_PLUS || gp_tok == TK_MINUS) {
        tk_t op = gp_tok; gp_next();
        double r = gp_mul();
        v = (op == TK_PLUS) ? v + r : v - r;
    }
    return v;
}

/* Evaluate expression, storing into pt. Returns 1 if valid. */
static int gp_eval(const char* expr, double x, double* pvals, double* out) {
    gp_src = expr; gp_pos = 0; gp_xval = x;
    gp_err[0] = 0; gp_ok = 1; gp_need_implicit = 0;
    memset(gp_used_params, 0, sizeof(gp_used_params));
    memcpy(gp_pvals, pvals, sizeof(double) * 26);
    gp_next();
    double r = gp_expr();
    if (gp_ok) { *out = r; return 1; }
    return 0;
}

/* Detect which params are used in an expression */
static void gr_detect_params(Expr* e) {
    e->param_count = 0;
    memset(e->param_names, 0, sizeof(e->param_names));
    memset(gp_used_params, 0, sizeof(gp_used_params));
    gp_src = e->text; gp_pos = 0; gp_need_implicit = 0;
    gp_ok = 1; gp_err[0] = 0;
    memset(gp_pvals, 0, sizeof(gp_pvals));
    gp_next();
    while (gp_tok != TK_EOF && gp_ok) gp_expr();

    for (int i = 0; i < 26 && e->param_count < MAX_PARAMS; i++) {
        if (gp_used_params[i] && i != ('x' - 'a')) {
            e->param_names[e->param_count] = 'a' + i;
            e->params[e->param_count] = 1.0;
            e->param_min[e->param_count] = -10;
            e->param_max[e->param_count] = 10;
            e->param_count++;
        }
    }
}

/* Recompute all points for an expression */
static void gr_recompute(Expr* e) {
    e->pt_count = 0;
    e->parse_ok = 1;
    e->error[0] = 0;
    if (e->len == 0) return;

    double pvals[26];
    memset(pvals, 0, sizeof(pvals));
    for (int i = 0; i < e->param_count; i++)
        pvals[e->param_names[i] - 'a'] = e->params[i];

    double step = (gr_max_x - gr_min_x) / (MAX_PTS - 1);
    if (step <= 0) return;

    for (int i = 0; i < MAX_PTS; i++) {
        double x = gr_min_x + i * step;
        double val;
        if (gp_eval(e->text, x, pvals, &val)) {
            e->pts[i] = val;
            e->pt_count++;
        } else {
            e->pts[i] = 0.0/0.0;
            e->pt_count++;
            if (e->parse_ok) {
                e->parse_ok = 0;
                snprintf(e->error, 64, "%s", gp_err);
            }
        }
    }
}

static void gr_recompute_all(void) {
    for (int i = 0; i < gr_expr_count; i++)
        if (gr_exprs[i].len > 0)
            gr_recompute(&gr_exprs[i]);
}

static void gr_fit_view(void) {
    double ymin = 1e100, ymax = -1e100;
    int has_data = 0;
    for (int i = 0; i < gr_expr_count; i++) {
        if (!gr_exprs[i].enabled || gr_exprs[i].len == 0) continue;
        for (int j = 0; j < gr_exprs[i].pt_count; j++) {
            double v = gr_exprs[i].pts[j];
            if (v == v && v > -1e100 && v < 1e100) {
                if (v < ymin) ymin = v;
                if (v > ymax) ymax = v;
                has_data = 1;
            }
        }
    }
    if (has_data) {
        double pad = (ymax - ymin) * 0.1;
        if (pad < 0.5) pad = 0.5;
        gr_min_y = ymin - pad;
        gr_max_y = ymax + pad;
        if (gr_min_y >= gr_max_y) { gr_min_y -= 1; gr_max_y += 1; }
        gr_recompute_all();
    }
}

/* ---- Strip "y = " or "f(x) = " prefix ---- */
static const char* gr_strip_eq(const char* s) {
    while (*s == ' ' || *s == '\t') s++;
    if ((*s == 'y' || *s == 'Y') && *(s+1) == '=') return gr_strip_eq(s + 2);
    if ((*s == 'f' || *s == 'F') && *(s+1) == '(' && *(s+2) == 'x' && *(s+3) == ')' && *(s+4) == '=') return gr_strip_eq(s + 5);
    return s;
}

/* ---- Convert param value to slider x position ---- */
static int param_to_sx(double val, double mn, double mx, int track_x, int track_w) {
    if (mx <= mn) return track_x;
    double frac = (val - mn) / (mx - mn);
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    return track_x + (int)(frac * (track_w - 1));
}

static double sx_to_param(int sx, int track_x, int track_w, double mn, double mx) {
    if (track_w <= 1) return mn;
    double frac = (double)(sx - track_x) / (track_w - 1);
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    return mn + frac * (mx - mn);
}

/* ---- Render ---- */
static void gr_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;

    uint32_t canvas_bg = 0xFFFFFF;
    uint32_t grid_c    = 0xE8E8E8;
    uint32_t subgrid_c = 0xF2F2F2;
    uint32_t axis_c    = 0x444444;
    uint32_t text_c    = 0x1A1A1A;
    uint32_t dim_c     = 0x999999;
    uint32_t panel_bg  = 0xF8F9FA;
    uint32_t border_c  = 0xDEE2E6;
    uint32_t tooltip_bg = 0x212529;

    int graph_x = x, graph_y = y;
    int graph_w = w, graph_h = h - BOTTOM_BAR_H;

    /* Background */
    gfx_fill_rect(graph_x, graph_y, graph_w, graph_h, canvas_bg);

    /* Graph area with padding */
    int pad = 16;
    int gx = graph_x + pad;
    int gy = graph_y + pad;
    int gw = graph_w - pad * 2;
    int gh = graph_h - pad * 2 + BOTTOM_BAR_H;

    gfx_push_clip(gx, gy, gw, gh);

    /* Sub-grid */
    if (gr_show_grid) {
        int subdivs = 40;
        for (int i = 1; i < subdivs; i++) {
            int lx = gx + (i * gw) / subdivs;
            gfx_draw_vline(lx, gy, gh, subgrid_c);
            int ly = gy + (i * gh) / subdivs;
            gfx_draw_hline(gx, ly, gw, subgrid_c);
        }
    }

    /* Main grid */
    if (gr_show_grid) {
        int divs = 8;
        for (int i = 1; i < divs; i++) {
            int lx = gx + (i * gw) / divs;
            gfx_draw_vline(lx, gy, gh, grid_c);
            int ly = gy + (i * gh) / divs;
            gfx_draw_hline(gx, ly, gw, grid_c);
        }
    }

    /* Axes */
    if (gr_show_axes) {
        if (gr_min_x < 0 && gr_max_x > 0) {
            int ax = gx + (int)((-gr_min_x) / (gr_max_x - gr_min_x) * gw);
            if (ax >= gx && ax < gx + gw) gfx_draw_vline(ax, gy, gh, axis_c);
        }
        if (gr_min_y < 0 && gr_max_y > 0) {
            int ay = gy + (int)(gr_max_y / (gr_max_y - gr_min_y) * gh);
            if (ay >= gy && ay < gy + gh) gfx_draw_hline(gx, ay, gw, axis_c);
        }
    }

    /* Tick labels */
    char lbl[24];
    int lbl_y = gy + gh + 2;
    if (lbl_y < graph_y + graph_h) {
        snprintf(lbl, 24, "%.1f", gr_min_x);
        gfx_draw_string_transparent(gx + 2, lbl_y, lbl, dim_c);
        snprintf(lbl, 24, "%.1f", gr_max_x);
        gfx_draw_string_transparent(gx + gw - (int)strlen(lbl) * 8 - 2, lbl_y, lbl, dim_c);
    }
    snprintf(lbl, 24, "%.1f", gr_max_y);
    gfx_draw_string_transparent(gx + gw - 40, gy + 2, lbl, dim_c);
    snprintf(lbl, 24, "%.1f", gr_min_y);
    gfx_draw_string_transparent(gx + gw - 40, gy + gh - 14, lbl, dim_c);

    /* Plot curves */
    for (int ei = 0; ei < gr_expr_count; ei++) {
        Expr* e = &gr_exprs[ei];
        if (!e->enabled || e->pt_count < 2) continue;
        int prev_valid = 0, prev_px = 0, prev_py = 0;
        for (int i = 0; i < e->pt_count; i++) {
            double val = e->pts[i];
            int valid = (val == val) && val > -1e100 && val < 1e100;
            int px = gx + (int)((double)i / (MAX_PTS - 1) * gw);
            int py = gy + (int)((gr_max_y - val) / (gr_max_y - gr_min_y) * gh);
            if (valid && py >= gy - 2 && py <= gy + gh + 2) {
                if (prev_valid) gfx_draw_line(prev_px, prev_py, px, py, e->color);
                prev_valid = 1; prev_px = px; prev_py = py;
            } else {
                prev_valid = 0;
            }
        }
    }

    /* Coordinate tooltip */
    if (gr_show_coords) {
        int cx = gx + (int)((gr_coord_x - gr_min_x) / (gr_max_x - gr_min_x) * gw);
        int cy = gy + (int)((gr_max_y - gr_coord_y) / (gr_max_y - gr_min_y) * gh);
        if (cx >= gx && cx <= gx + gw && cy >= gy && cy <= gy + gh) {
            char coord[48];
            snprintf(coord, 48, "(%.3f, %.3f)", gr_coord_x, gr_coord_y);
            int tw = (int)strlen(coord) * 8 + 12, th = 22;
            int tx = cx + 12, ty = cy - 20;
            if (tx + tw > gx + gw) tx = cx - tw - 4;
            if (ty < gy) ty = cy + 12;
            gfx_fill_rect(tx, ty, tw, th, tooltip_bg);
            gfx_draw_string_transparent(tx + 6, ty + 3, coord, 0xFFFFFF);
        }
    }

    gfx_pop_clip();

    /* ---- Expression Panel ---- */
    int pw = PANEL_W;
    int ph = graph_h - PANEL_Y - 4;
    int px = graph_x + PANEL_X, py = graph_y + PANEL_Y;

    gfx_fill_rect(px, py, pw, ph, panel_bg);
    gfx_draw_rect_outline(px, py, pw, ph, 1, border_c);

    int cur_y = py + 6;

    for (int ei = 0; ei < gr_expr_count; ei++) {
        Expr* e = &gr_exprs[ei];
        int row_y = cur_y;

        /* Color dot */
        int dot_r = 7, dot_x = px + 14, dot_y = row_y + EXPR_ROW_H / 2;
        int dot_hover = (gr_hover_expr == ei && gr_hover_color == ei);
        gfx_fill_circle(dot_x, dot_y, dot_r, dot_hover ? gfx_lighten(e->color, 30) : e->color);
        gfx_draw_circle(dot_x, dot_y, dot_r, gfx_darken(e->color, 40));

        /* Expression text */
        char display[EXPR_LEN + 4];
        int di = 0;
        if (e->editing) {
            for (int i = 0; i < e->len && di < EXPR_LEN + 2; i++) {
                if (i == e->cursor && (timer_get_ms() / 500) % 2)
                    display[di++] = '|';
                display[di++] = e->text[i];
            }
            if (di == e->cursor && (timer_get_ms() / 500) % 2)
                display[di++] = '|';
            display[di] = 0;
        } else {
            for (int i = 0; i < e->len && di < EXPR_LEN + 2; i++)
                display[di++] = e->text[i];
            display[di] = 0;
        }
        uint32_t expr_color = e->enabled ? text_c : dim_c;
        gfx_draw_string_transparent(dot_x + 16, row_y + (EXPR_ROW_H - 16) / 2, display, expr_color);

        /* Toggle eye */
        int eye_x = px + pw - 50, eye_y = row_y + 4, eye_s = 26;
        int eye_cx = eye_x + eye_s / 2, eye_cy = eye_y + eye_s / 2 + 2;
        uint32_t eye_color = e->enabled ? (gr_hover_toggle == ei ? get_accent_color() : text_c) : dim_c;
        gfx_draw_circle(eye_cx, eye_cy, 6, eye_color);
        gfx_draw_circle(eye_cx, eye_cy, 3, eye_color);
        if (!e->enabled) {
            gfx_draw_line(eye_x + 2, eye_y + 2, eye_x + eye_s - 2, eye_y + eye_s - 2, dim_c);
        }

        /* Delete X */
        int del_x = px + pw - 22, del_y = row_y + 4, del_s = 20;
        uint32_t del_color = (gr_hover_delete == ei) ? 0xE53935 : dim_c;
        gfx_draw_string_transparent(del_x + 5, del_y + 2, "x", del_color);

        /* Separator line */
        gfx_draw_hline(px + 4, row_y + EXPR_ROW_H, pw - 8, border_c);

        /* Parameter sliders */
        cur_y = row_y + EXPR_ROW_H + 2;
        for (int pi = 0; pi < e->param_count; pi++) {
            int srow_y = cur_y;
            char pname[4] = { e->param_names[pi], '=', 0 };
            gfx_draw_string_transparent(dot_x + 4, srow_y, pname, dim_c);

            int track_x = px + 50;
            int track_w = pw - 50 - 56;
            int track_y = srow_y + PARAM_ROW_H / 2 - 3;
            int track_h = 6;

            gfx_fill_rect_rounded(track_x, track_y, track_w, track_h, 3, 0xDEE2E6);
            int hx = param_to_sx(e->params[pi], e->param_min[pi], e->param_max[pi], track_x, track_w);
            int is_hover = (gr_hover_slider_expr == ei && gr_hover_slider_idx == pi);
            gfx_fill_circle(hx, track_y + track_h / 2, 6, is_hover ? 0x666666 : e->color);

            char val_str[16];
            snprintf(val_str, 16, "%.2f", e->params[pi]);
            gfx_draw_string_transparent(track_x + track_w + 6, srow_y, val_str, text_c);

            cur_y = srow_y + PARAM_ROW_H;
        }
    }

    /* Add expression row */
    {
        int add_y = cur_y + 2;
        gfx_draw_hline(px + 4, add_y - 2, pw - 8, border_c);

        if (gr_add_mode) {
            char display[EXPR_LEN + 4];
            int di = 0;
            for (int i = 0; i < gr_add_len && di < EXPR_LEN + 2; i++) {
                if (i == gr_add_cursor && (timer_get_ms() / 500) % 2)
                    display[di++] = '|';
                display[di++] = gr_add_buf[i];
            }
            if (di == gr_add_cursor && (timer_get_ms() / 500) % 2)
                display[di++] = '|';
            display[di] = 0;
            gfx_draw_string_transparent(px + 14, add_y + 2, display, text_c);
        } else {
            gfx_draw_string_transparent(px + 14, add_y + 2, "+ Click to add expression ...", dim_c);
        }
    }

    /* ---- Bottom Toolbar ---- */
    int bar_y = graph_y + graph_h;
    gfx_fill_rect(graph_x, bar_y, graph_w, BOTTOM_BAR_H, panel_bg);
    gfx_draw_hline(graph_x, bar_y, graph_w, border_c);

    int btn_y = bar_y + 2, btn_h = BOTTOM_BAR_H - 4, btn_gap = 4;
    int b_x = graph_x + btn_gap;
    const char* btns[] = { "Fit", "Auto-Y", "Grid", "Axes", "Clear", "Help" };
    int bn = sizeof(btns) / sizeof(btns[0]);
    for (int i = 0; i < bn; i++) {
        int tw = (int)strlen(btns[i]) * 8 + 16;
        uint32_t bg = 0xE9ECEF, fg = text_c;
        if ((i == 2 && gr_show_grid) || (i == 3 && gr_show_axes) || (i == 1 && gr_auto_y_enabled)) {
            bg = get_accent_color(); fg = 0xFFFFFF;
        }
        gfx_fill_rect_rounded(b_x, btn_y, tw, btn_h, 4, bg);
        gfx_draw_string_transparent(b_x + (tw - (int)strlen(btns[i]) * 8) / 2, btn_y + (btn_h - 16) / 2, btns[i], fg);
        b_x += tw + btn_gap;
    }
}

/* ---- Mouse ---- */
static void gr_mouse(int id, int mx, int my, int mb) {
    (void)id;
    wm_window_t* win = wm_get_window(gr_win_id);
    if (!win) return;
    if (gr_help_shown) return;

    int h = win->h - WM_TITLEBAR_H;
    int graph_w = win->w;
    int graph_h = h - BOTTOM_BAR_H;
    int pad = 16;
    int gx = pad, gy = pad;
    int gw = graph_w - pad * 2;
    int gh = graph_h - pad * 2 + BOTTOM_BAR_H;

    int px = PANEL_X, py = PANEL_Y;
    int pw = PANEL_W;
    int ph = graph_h - PANEL_Y - 4;

    int wheel = mouse_get_wheel_delta();

    /* ---- Wheel zoom ---- */
    if (wheel != 0) {
        double cx = (gr_min_x + gr_max_x) * 0.5;
        double cy = (gr_min_y + gr_max_y) * 0.5;
        double xr = (gr_max_x - gr_min_x) * 0.5;
        double yr = (gr_max_y - gr_min_y) * 0.5;
        double step = 0.15;
        if (wheel > 0) { xr *= (1 - step); yr *= (1 - step); }
        else { xr /= (1 - step); yr /= (1 - step); }
        if (xr < 0.001) xr = 0.001;
        if (yr < 0.001) yr = 0.001;
        gr_min_x = cx - xr; gr_max_x = cx + xr;
        gr_min_y = cy - yr; gr_max_y = cy + yr;
        gr_recompute_all();
        mouse_clear_wheel_delta();
        return;
    }

    int in_graph = (mx >= 0 && mx < graph_w && my >= 0 && my < graph_h);
    int in_panel = (mx >= px && mx < px + pw && my >= py && my < py + ph);

    /* ---- Mouse move: update hover state ---- */
    if (!(mb & 1)) {
        gr_hover_expr = -1;
        gr_hover_delete = -1;
        gr_hover_toggle = -1;
        gr_hover_color = -1;
        gr_hover_slider = -1;
        gr_hover_slider_expr = -1;
        gr_hover_slider_idx = -1;
        gr_show_coords = 0;

        if (in_graph) {
            int rel_x = mx - gx;
            int rel_y = my - gy;
            if (rel_x >= 0 && rel_x < gw && rel_y >= 0 && rel_y < gh) {
                gr_coord_x = gr_min_x + (double)rel_x / gw * (gr_max_x - gr_min_x);
                gr_coord_y = gr_max_y - (double)rel_y / gh * (gr_max_y - gr_min_y);
                gr_show_coords = 1;
            }
        }

        if (in_panel) {
            int cur_y = py + 6;
            for (int ei = 0; ei < gr_expr_count; ei++) {
                Expr* e = &gr_exprs[ei];
                if (my >= cur_y && my < cur_y + EXPR_ROW_H) {
                    gr_hover_expr = ei;
                    /* Delete */
                    int del_x = px + pw - 22;
                    if (mx >= del_x && mx < del_x + 20) gr_hover_delete = ei;
                    /* Toggle eye */
                    int eye_x = px + pw - 50;
                    if (mx >= eye_x && mx < eye_x + 26) gr_hover_toggle = ei;
                    /* Color dot */
                    int dot_x = px + 14;
                    if (mx >= dot_x - 7 && mx <= dot_x + 7) gr_hover_color = ei;
                }
                cur_y += EXPR_ROW_H + 2;
                /* Check slider hovers */
                for (int pi = 0; pi < e->param_count; pi++) {
                    int srow_y = cur_y;
                    int track_x = px + 50;
                    int track_w = pw - 50 - 56;
                    int track_y = srow_y + PARAM_ROW_H / 2 - 3;
                    int track_h = 6;
                    int hx = param_to_sx(e->params[pi], e->param_min[pi], e->param_max[pi], track_x, track_w);
                    if (my >= srow_y && my < srow_y + PARAM_ROW_H) {
                        if (mx >= hx - 8 && mx <= hx + 8 && my >= track_y - 4 && my <= track_y + track_h + 4) {
                            gr_hover_slider = 1;
                            gr_hover_slider_expr = ei;
                            gr_hover_slider_idx = pi;
                        }
                    }
                    cur_y += PARAM_ROW_H;
                }
            }
        }

    }

    /* ---- Mouse click ---- */
    if (mb & 1) {
        if (in_panel) {
            /* End editing any expression */
            for (int ei = 0; ei < gr_expr_count; ei++)
                gr_exprs[ei].editing = 0;
            gr_add_mode = 0;

            /* Check expression rows */
            int cur_y = py + 6;
            for (int ei = 0; ei < gr_expr_count; ei++) {
                Expr* e = &gr_exprs[ei];
                if (my >= cur_y && my < cur_y + EXPR_ROW_H) {
                    int del_x = px + pw - 22;
                    int eye_x = px + pw - 50;
                    int dot_x = px + 14;

                    if (mx >= del_x && mx < del_x + 20) {
                        /* Delete */
                        for (int j = ei; j < gr_expr_count - 1; j++)
                            gr_exprs[j] = gr_exprs[j + 1];
                        gr_expr_count--;
                        if (gr_expr_count < 0) gr_expr_count = 0;
                        return;
                    }
                    if (mx >= eye_x && mx < eye_x + 26) {
                        e->enabled = !e->enabled;
                        return;
                    }
                    if (mx >= dot_x - 7 && mx <= dot_x + 7) {
                        /* Cycle color */
                        uint32_t cur = e->color;
                        int ci;
                        for (ci = 0; ci < (int)COLORS_COUNT; ci++)
                            if (gr_colors[ci] == cur) break;
                        e->color = gr_colors[(ci + 1) % COLORS_COUNT];
                        return;
                    }
                    /* Click on expression text → edit */
                    e->editing = 1;
                    e->cursor = e->len;
                    gr_active = ei;
                    return;
                }
                cur_y += EXPR_ROW_H + 2;
                /* Check slider clicks */
                for (int pi = 0; pi < e->param_count; pi++) {
                    int srow_y = cur_y;
                    int track_x = px + 50;
                    int track_w = pw - 50 - 56;
                    int track_y = srow_y + PARAM_ROW_H / 2 - 3;
                    int track_h = 6;
                    int hx = param_to_sx(e->params[pi], e->param_min[pi], e->param_max[pi], track_x, track_w);
                    if (my >= srow_y && my < srow_y + PARAM_ROW_H) {
                        if (mx >= track_x && mx < track_x + track_w) {
                            gr_drag_slider = 1;
                            gr_drag_slider_expr = ei;
                            gr_drag_slider_idx = pi;
                            e->params[pi] = sx_to_param(mx, track_x, track_w, e->param_min[pi], e->param_max[pi]);
                            gr_recompute(e);
                            if (gr_auto_y_enabled) gr_fit_view();
                            return;
                        }
                    }
                    cur_y += PARAM_ROW_H;
                }
            }

            /* Click on add area */
            {
                int add_y = cur_y + 2;
                if (my >= add_y && my < add_y + EXPR_ROW_H) {
                    gr_add_mode = 1;
                    gr_add_len = 0;
                    gr_add_cursor = 0;
                    return;
                }
            }
        }

        /* Check toolbar buttons */
        {
            int bar_y = graph_h;
            if (my >= bar_y && my < bar_y + BOTTOM_BAR_H) {
                int btn_y = bar_y + 2, btn_h = BOTTOM_BAR_H - 4, btn_gap = 4;
                int b_x = btn_gap;
                const char* btns[] = { "Fit", "Auto-Y", "Grid", "Axes", "Clear", "Help" };
                for (int i = 0; i < 6; i++) {
                    int tw = (int)strlen(btns[i]) * 8 + 16;
                    if (mx >= b_x && mx < b_x + tw && my >= btn_y && my < btn_y + btn_h) {
                        if (i == 0) { gr_fit_view(); }
                        if (i == 1) { gr_auto_y_enabled = !gr_auto_y_enabled; if (gr_auto_y_enabled) gr_fit_view(); }
                        if (i == 2) { gr_show_grid = !gr_show_grid; }
                        if (i == 3) { gr_show_axes = !gr_show_axes; }
                        if (i == 4) {
                            for (int ei = 0; ei < gr_expr_count; ei++)
                                gr_exprs[ei].enabled = 0;
                            gr_expr_count = 0;
                            gr_add_mode = 0;
                            gr_add_len = 0;
                        }
                        if (i == 5) { gr_help_shown = 1; }
                        return;
                    }
                    b_x += tw + btn_gap;
                }
            }
        }

        /* Click on graph → pan */
        if (in_graph) {
            gr_drag_pan = 1;
            gr_drag_pan_sx = mx; gr_drag_pan_sy = my;
            gr_drag_pan_mnx = gr_min_x; gr_drag_pan_mxx = gr_max_x;
            gr_drag_pan_mny = gr_min_y; gr_drag_pan_mxy = gr_max_y;
            /* Also end editing */
            for (int ei = 0; ei < gr_expr_count; ei++)
                gr_exprs[ei].editing = 0;
            gr_add_mode = 0;
        }
    }

    /* ---- Drag pan ---- */
    if (gr_drag_pan && (mb & 1)) {
        if (gw > 0 && gh > 0) {
            double dx = (double)(mx - gr_drag_pan_sx) / gw * (gr_drag_pan_mxx - gr_drag_pan_mnx);
            double dy = (double)(my - gr_drag_pan_sy) / gh * (gr_drag_pan_mxy - gr_drag_pan_mny);
            gr_min_x = gr_drag_pan_mnx - dx;
            gr_max_x = gr_drag_pan_mxx - dx;
            gr_min_y = gr_drag_pan_mny + dy;
            gr_max_y = gr_drag_pan_mxy + dy;
            gr_recompute_all();
        }
    }

    /* ---- Drag slider ---- */
    if (gr_drag_slider && (mb & 1)) {
        Expr* e = &gr_exprs[gr_drag_slider_expr];
        int track_x = px + 50;
        int track_w = pw - 50 - 56;
        if (mx >= track_x && mx < track_x + track_w) {
            e->params[gr_drag_slider_idx] = sx_to_param(mx, track_x, track_w,
                e->param_min[gr_drag_slider_idx], e->param_max[gr_drag_slider_idx]);
            gr_recompute(e);
            if (gr_auto_y_enabled) gr_fit_view();
        }
    }

    /* ---- Mouse up ---- */
    if (!(mb & 1)) {
        gr_drag_pan = 0;
        gr_drag_slider = 0;
    }
}

/* ---- Keyboard ---- */
static void gr_key(int id, char key) {
    (void)id;
    unsigned char k = (unsigned char)key;

    if (k == 'q' || k == 'Q') { wm_close_window(gr_win_id); gr_win_id = -1; return; }
    if (k == 'h' || k == 'H') { gr_help_shown = !gr_help_shown; return; }

    if (gr_help_shown) { if (k == 27) gr_help_shown = 0; return; }

    /* Check if any expression is being edited */
    int editing_idx = -1;
    for (int i = 0; i < gr_expr_count; i++) {
        if (gr_exprs[i].editing) { editing_idx = i; break; }
    }

    if (editing_idx >= 0) {
        Expr* e = &gr_exprs[editing_idx];
        if (k == '\n') {
            e->editing = 0;
            e->len = (int)strlen(e->text);
            const char* stripped = gr_strip_eq(e->text);
            if (stripped != e->text) {
                int sl = (int)strlen(stripped);
                memmove(e->text, stripped, sl + 1);
                e->len = sl;
            }
            gr_detect_params(e);
            gr_recompute(e);
            e->color = gr_colors[editing_idx % COLORS_COUNT];
            if (gr_auto_y_enabled) gr_fit_view();
            return;
        }
        if (k == 27) { e->editing = 0; return; }
        if (KEY_MATCH(k, KEY_LEFT)) { if (e->cursor > 0) e->cursor--; return; }
        if (KEY_MATCH(k, KEY_RIGHT)) { if (e->cursor < e->len) e->cursor++; return; }
        if (KEY_MATCH(k, '\b') || KEY_MATCH(k, 127)) {
            if (e->cursor > 0) {
                for (int j = e->cursor - 1; j < e->len - 1; j++)
                    e->text[j] = e->text[j + 1];
                e->len--;
                e->cursor--;
                e->text[e->len] = 0;
            }
            return;
        }
        if (k >= 32 && k <= 126 && e->len < EXPR_LEN - 1) {
            for (int j = e->len; j > e->cursor; j--)
                e->text[j] = e->text[j - 1];
            e->text[e->cursor] = k;
            e->len++;
            e->cursor++;
            e->text[e->len] = 0;
        }
    } else if (gr_add_mode) {
        if (k == '\n') {
            if (gr_add_len > 0) {
                gr_add_buf[gr_add_len] = 0;
                if (gr_expr_count < MAX_EXPR) {
                    Expr* e = &gr_exprs[gr_expr_count];
                    memset(e, 0, sizeof(Expr));
                    const char* stripped = gr_strip_eq(gr_add_buf);
                    int sl = (int)strlen(stripped);
                    if (sl > EXPR_LEN - 1) sl = EXPR_LEN - 1;
                    memcpy(e->text, stripped, sl);
                    e->text[sl] = 0;
                    e->len = sl;
                    e->enabled = 1;
                    e->color = gr_colors[gr_expr_count % COLORS_COUNT];
                    e->editing = 0;
                    gr_detect_params(e);
                    gr_recompute(e);
                    gr_expr_count++;
                    gr_add_len = 0;
                    gr_add_mode = 0;
                    if (gr_auto_y_enabled) gr_fit_view();
                }
            }
            return;
        }
        if (k == 27) { gr_add_mode = 0; gr_add_len = 0; return; }
        if (KEY_MATCH(k, KEY_LEFT)) { if (gr_add_cursor > 0) gr_add_cursor--; return; }
        if (KEY_MATCH(k, KEY_RIGHT)) { if (gr_add_cursor < gr_add_len) gr_add_cursor++; return; }
        if (KEY_MATCH(k, '\b') || KEY_MATCH(k, 127)) {
            if (gr_add_cursor > 0) {
                for (int j = gr_add_cursor - 1; j < gr_add_len - 1; j++)
                    gr_add_buf[j] = gr_add_buf[j + 1];
                gr_add_len--;
                gr_add_cursor--;
                gr_add_buf[gr_add_len] = 0;
            }
            return;
        }
        if (k >= 32 && k <= 126 && gr_add_len < EXPR_LEN - 1) {
            for (int j = gr_add_len; j > gr_add_cursor; j--)
                gr_add_buf[j] = gr_add_buf[j - 1];
            gr_add_buf[gr_add_cursor] = k;
            gr_add_len++;
            gr_add_cursor++;
            gr_add_buf[gr_add_len] = 0;
        }
    } else {
        /* Not editing anything — if user types, start add mode */
        if (k >= 32 && k <= 126 && !(k == 'q' || k == 'Q' || k == 'h' || k == 'H')) {
            gr_add_mode = 1;
            gr_add_len = 0;
            gr_add_cursor = 0;
            gr_add_buf[gr_add_len++] = k;
            gr_add_buf[gr_add_len] = 0;
            return;
        }
        if (KEY_MATCH(k, KEY_UP)) {
            for (int i = gr_expr_count - 1; i >= 0; i--) {
                if (gr_exprs[i].enabled) {
                    gr_exprs[i].editing = 1;
                    gr_exprs[i].cursor = gr_exprs[i].len;
                    break;
                }
            }
        }
    }
}

static void gr_resize(int id, int w, int h) { (void)id; (void)w; (void)h; }

void graphing_app(void) {
    if (wm_get_window(gr_win_id)) { wm_bring_to_front(gr_win_id); return; }

    memset(gr_exprs, 0, sizeof(gr_exprs));
    gr_expr_count = 0;
    gr_add_mode = 0;
    gr_add_len = 0;
    gr_add_cursor = 0;
    gr_active = -1;
    gr_min_x = -10; gr_max_x = 10;
    gr_min_y = -10; gr_max_y = 10;
    gr_auto_y_enabled = 0;
    gr_show_grid = 1;
    gr_show_axes = 1;
    gr_show_coords = 0;
    gr_drag_pan = 0;
    gr_drag_slider = 0;
    gr_hover_expr = -1;
    gr_help_shown = 0;

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    gr_win_id = wm_open_window(
        (fw - GR_WIN_W) / 2, (fh - GR_WIN_H) / 2,
        GR_WIN_W, GR_WIN_H,
        "Desmos-like Graphing Calculator", 0x1A73E8,
        gr_render, gr_key, gr_resize
    );
    if (gr_win_id >= 0) wm_set_mouse_handler(gr_win_id, gr_mouse);
}
