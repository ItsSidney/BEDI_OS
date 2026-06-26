// ============================================================
//  BEDI OS — Weather App
//  Fetches real-time weather from Open-Meteo (no API key)
//  Preset cities, background fetch, live wind/direction display
// ============================================================

#include "apps/weather.h"
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "gui/wm.h"
#include "gui/gui.h"
#include "kernel/net/socket.h"
#include "kernel/net/dns.h"
#include "kernel/task/task.h"
#include <string.h>
#include <stdint.h>

/* Minimal prototypes for functions used by weather.c that are not
   provided by the BEDI OS libc headers. */
void exit_task(int code);
int create_task(void (*fn)(void), const char* name);
void* kmalloc(uint32_t size);
void kfree(void* ptr);
void gfx_fill_circle(int cx, int cy, int r, uint32_t color);
void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
void gfx_draw_rect_outline(int x, int y, int w, int h, int thickness, uint32_t color);
void gfx_draw_hshadow(int x, int y, int w, int h, int radius, uint32_t color);
void gfx_draw_hline(int x, int y, int w, uint32_t color);
void gfx_draw_vline(int x, int y, int h, uint32_t color);
void gfx_draw_bevel_rect(int x, int y, int w, int h, uint32_t bg, int sunken);
void gfx_draw_window_custom(int x, int y, int w, int h, const char* title, uint32_t accent, int is_square);
void gfx_push_clip(int x, int y, int w, int h);
void gfx_pop_clip(void);
void gfx_reset_clip(void);
uint32_t gfx_lighten(uint32_t c, int amount);
uint32_t gfx_darken(uint32_t c, int amount);
void gfx_draw_button(int x, int y, int w, int h, const char* label, uint32_t bg, uint32_t fg, int hover);
void gfx_fill_rect_rounded(int x, int y, int w, int h, int radius, uint32_t color);
void gfx_draw_string_transparent(int x, int y, const char* s, uint32_t color);
void gfx_draw_shadow(int x, int y, int w, int h, int radius);
void sleep_task(uint32_t ms);

#define WEATHER_FIXED_SHIFT 10
static int weather_sin_fixed(int deg) {
    while (deg >= 360) deg -= 360;
    while (deg < 0) deg += 360;
    if (deg <= 90) return (int)(32768 * deg * 3.14159265 / 180.0);
    if (deg <= 180) return (int)(32768 * (180 - deg) * 3.14159265 / 180.0);
    if (deg <= 270) return -(int)(32768 * (deg - 180) * 3.14159265 / 180.0);
    return -(int)(32768 * (360 - deg) * 3.14159265 / 180.0);
}

#define WEATHER_W 420
#define WEATHER_H 540
#define MAX_RESP 4096
#define WEATHER_BTN_AREA 48

static const char* const main_cities[] = {
    "Tokyo", "Seoul", "New York", "London", "Paris", "Sydney"
};
static const double city_lat[] = {35.6762, 37.5665, 40.7128, 51.5074, 48.8566, -33.8688};
static const double city_lon[] = {139.6503, 126.9780, -74.0060, -0.1278, 2.3522, 151.2093};

static int weather_win_id = -1;
static int sel_city = 0;
static double temperature = 0.0;
static double windspeed = 0.0;
static int winddir = 0;
static int weathercode = -1;
static int fetching = 0;
static char fetch_status[64] = "Ready";
static char city_display[32] = "Tokyo";
static char updated_time[32] = "";

static int total_cities() {
    return sizeof(main_cities) / sizeof(main_cities[0]);
}

static const char* weather_label(int code) {
    switch (code) {
        case 0:  return "Clear Sky";
        case 1:  return "Mainly Clear";
        case 2:  return "Partly Cloudy";
        case 3:  return "Overcast";
        case 45: return "Fog";
        case 48: return "Rime Fog";
        case 51: return "Light Drizzle";
        case 53: return "Drizzle";
        case 55: return "Dense Drizzle";
        case 61: return "Light Rain";
        case 63: return "Rain";
        case 65: return "Heavy Rain";
        case 71: return "Light Snow";
        case 73: return "Snow";
        case 75: return "Heavy Snow";
        case 80: return "Showers";
        case 81: return "Heavy Showers";
        case 82: return "Violent Showers";
        case 95: return "Thunderstorm";
        case 96: return "T-storm Hail";
        case 99: return "Severe T-storm";
        default: return "Unknown";
    }
}

static const char* weather_strstr(const char* haystack, const char* needle) {
    if (!*needle) return haystack;
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return haystack;
    }
    return 0;
}

static int json_get_double(const char* buf, const char* key, double* out) {
    char pat[64];
    int kl = strlen(key);
    if (kl + 4 > 64) return 0;
    pat[0] = '"';
    memcpy(pat + 1, key, kl);
    pat[kl + 1] = '"';
    pat[kl + 2] = ':';
    pat[kl + 3] = 0;

    const char* p = weather_strstr(buf, pat);
    if (!p) return 0;
    p += kl + 3;
    while (*p == ' ' || *p == '\t') p++;

    double val = 0.0, frac = 0.1;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    while (*p >= '0' && *p <= '9') {
        val = val * 10.0 + (*p - '0');
        p++;
    }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            val += (*p - '0') * frac;
            frac *= 0.1;
            p++;
        }
    }
    *out = neg ? -val : val;
    return 1;
}

static int json_get_int(const char* buf, const char* key, int* out) {
    double d;
    if (!json_get_double(buf, key, &d)) return 0;
    *out = (int)d;
    return 1;
}

static void weather_fetch_body(void) {
    fetching = 1;
    strcpy(fetch_status, "Resolving...");

    uint32_t ip;
    if (dns_resolve("api.open-meteo.com", &ip) < 0 || ip == 0) {
        strcpy(fetch_status, "DNS failed");
        fetching = 0;
        return;
    }

    strcpy(fetch_status, "Connecting...");
    int sock = sys_socket(2, 1, 0);
    if (sock < 0) {
        strcpy(fetch_status, "Socket error");
        fetching = 0;
        return;
    }

    struct sockaddr_in addr;
    addr.sin_family = 2;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = ip;

    if (sys_connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        strcpy(fetch_status, "Connect failed");
        sys_socket_close(sock);
        fetching = 0;
        return;
    }

    strcpy(fetch_status, "Fetching...");
    char req[512];
    int ri = 0;
    const char* base =
        "GET /v1/forecast?latitude=";
    const char* mid1 = "&longitude=";
    const char* mid2 = "&current_weather=true HTTP/1.1\r\n"
        "Host: api.open-meteo.com\r\n"
        "User-Agent: BEDI-Weather/1.0\r\n"
        "Accept: application/json\r\n"
        "Accept-Encoding: identity\r\n"
        "Connection: close\r\n\r\n";

    ri = 0;
    int i = 0;
    while (base[i] && ri < 500) req[ri++] = base[i++];
    {
        double lat = city_lat[sel_city];
        char tmp[32];
        int ti = 0;
        if (lat < 0) { tmp[ti++] = '-'; lat = -lat; }
        int ip = (int)lat;
        tmp[ti++] = (ip / 10) + '0';
        tmp[ti++] = (ip % 10) + '0';
        tmp[ti++] = '.';
        int fr = (int)((lat - ip) * 10.0 + 0.5);
        tmp[ti++] = fr + '0';
        tmp[ti] = 0;
        int j = 0;
        while (tmp[j] && ri < 500) req[ri++] = tmp[j++];
    }
    i = 0;
    while (mid1[i] && ri < 500) req[ri++] = mid1[i++];
    {
        double lon = city_lon[sel_city];
        char tmp[32];
        int ti = 0;
        if (lon < 0) { tmp[ti++] = '-'; lon = -lon; }
        int ip = (int)lon;
        tmp[ti++] = (ip / 10) + '0';
        tmp[ti++] = (ip % 10) + '0';
        tmp[ti++] = '.';
        int fr = (int)((lon - ip) * 10.0 + 0.5);
        tmp[ti++] = fr + '0';
        tmp[ti] = 0;
        int j = 0;
        while (tmp[j] && ri < 500) req[ri++] = tmp[j++];
    }
    i = 0;
    while (mid2[i] && ri < 500) req[ri++] = mid2[i++];
    req[ri] = 0;

    sys_send(sock, req, ri, 0);

    char resp[MAX_RESP];
    int rlen = 0;
    int no_data = 0;
    while (no_data < 20 && rlen < MAX_RESP - 1) {
        int n = sys_recv(sock, resp + rlen, MAX_RESP - 1 - rlen, 0);
        if (n <= 0) {
            no_data++;
            if (no_data < 20) sleep_task(20);
        } else {
            rlen += n;
            no_data = 0;
        }
    }
    resp[rlen] = 0;

    sys_socket_close(sock);

    const char* body = weather_strstr(resp, "\r\n\r\n");
    if (!body) body = weather_strstr(resp, "\n\n");
    int body_off = body ? (int)(body - resp) + (body[0] == '\r' ? 4 : 2) : 0;

    const char* json = resp + body_off;

    double tmp = 0.0;
    if (json_get_double(json, "temperature", &tmp)) {
        temperature = tmp;
        weathercode = -1;
        json_get_int(json, "weathercode", &weathercode);
        json_get_double(json, "windspeed", &windspeed);
        json_get_int(json, "winddirection", &winddir);
        memcpy(city_display, main_cities[sel_city], strlen(main_cities[sel_city]) + 1);
        strcpy(fetch_status, "Updated");
        // Simplified time
        updated_time[0] = 0;
    } else {
        strcpy(fetch_status, "Parse error");
    }
    fetching = 0;
}

static void weather_fetch_task(void) {
    weather_fetch_body();
    exit_task(0);
}

static void weather_refresh(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    if (fetching) return;
    create_task(weather_fetch_task, "Weather Fetch");
}

static void weather_prev_city(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    if (fetching) return;
    sel_city--;
    if (sel_city < 0) sel_city = total_cities() - 1;
    create_task(weather_fetch_task, "Weather Fetch");
}

static void weather_next_city(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    if (fetching) return;
    sel_city++;
    if (sel_city >= total_cities()) sel_city = 0;
    create_task(weather_fetch_task, "Weather Fetch");
}

static void draw_weather_icon(int x, int y, int code, uint32_t sun_col, uint32_t cloud_col) {
    if (code < 0) {
        // Unknown / empty
        gfx_fill_circle(x + 12, y + 12, 8, 0x30363D);
        return;
    }
    if (code <= 2) {
        // Sun
        gfx_fill_circle(x + 12, y + 10, 6, sun_col);
        for (int i = 0; i < 8; i++) {
            int ang = i * 45;
            int cx = x + 12 + (int)((8 + 3) * 0.707 * (i % 2 == 0 ? 1 : 0.7) * (i < 4 ? 1 : -1));
            // Simple rays
        }
        // Simplify: just sun circle
        gfx_fill_circle(x + 12, y + 10, 7, sun_col);
        gfx_fill_circle(x + 12, y + 10, 4, 0x0D1117);
        if (code >= 1) {
            gfx_fill_rect(x + 10, y + 14, 14, 6, cloud_col);
            gfx_fill_circle(x + 10, y + 14, 4, cloud_col);
            gfx_fill_circle(x + 18, y + 14, 5, cloud_col);
        }
    } else if (code == 3) {
        // Cloud
        gfx_fill_circle(x + 8, y + 12, 6, cloud_col);
        gfx_fill_circle(x + 16, y + 10, 7, cloud_col);
        gfx_fill_circle(x + 20, y + 14, 5, cloud_col);
        gfx_fill_rect(x + 6, y + 12, 16, 6, cloud_col);
    } else {
        // Rain / snow / thunderstorm - cloud with rain lines
        gfx_fill_circle(x + 8, y + 10, 6, cloud_col);
        gfx_fill_circle(x + 16, y + 8, 7, cloud_col);
        gfx_fill_rect(x + 6, y + 10, 16, 5, cloud_col);
        if (code >= 45 && code <= 48) {
            // Fog lines
            for (int i = 0; i < 3; i++) {
                gfx_draw_hline(x + 6, y + 18 + i * 4, 16, cloud_col);
            }
        } else if (code >= 51 && code <= 65) {
            // Rain drops
            for (int i = 0; i < 4; i++) {
                gfx_draw_line(x + 8 + i * 4, y + 18, x + 6 + i * 4, y + 24, 0x58A6FF);
            }
        } else if (code >= 71 && code <= 75) {
            // Snowflakes
            gfx_fill_rect(x + 8, y + 18, 2, 2, 0xFFFFFF);
            gfx_fill_rect(x + 14, y + 20, 2, 2, 0xFFFFFF);
            gfx_fill_rect(x + 18, y + 18, 2, 2, 0xFFFFFF);
        } else if (code >= 80) {
            // Heavy rain + cloud darker
            for (int i = 0; i < 5; i++) {
                gfx_draw_line(x + 6 + i * 3, y + 18, x + 4 + i * 3, y + 26, 0x58A6FF);
            }
        }
        if (code >= 95) {
            // Lightning bolt
            gfx_fill_rect(x + 16, y + 16, 4, 4, 0xF0883E);
            gfx_draw_line(x + 18, y + 20, x + 14, y + 26, 0xF0883E);
            gfx_draw_line(x + 14, y + 26, x + 22, y + 26, 0xF0883E);
        }
    }
}

static void weather_on_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    personalization_t* p = get_personalization();
    uint32_t bg       = (p->theme == 0) ? 0x0D1117 : 0xF6F8FA;
    uint32_t card_bg  = (p->theme == 0) ? 0x161B22 : 0xFFFFFF;
    uint32_t text     = (p->theme == 0) ? 0xE6EDF3 : 0x1F2937;
    uint32_t muted    = (p->theme == 0) ? 0x8B949E : 0x6B7280;
    uint32_t border   = (p->theme == 0) ? 0x30363D : 0xE5E7EB;
    uint32_t accent   = get_accent_color();

    gfx_fill_rect(x, y, w, h, bg);

    int hdr_y = y + 48;
    gfx_fill_rect(x + 12, hdr_y, w - 24, 72, card_bg);
    gfx_draw_rect_outline(x + 12, hdr_y, w - 24, 72, 1, border);
    gfx_draw_string_transparent(x + 24, hdr_y + 12, "Current Weather", accent);
    gfx_draw_string_transparent(x + 24, hdr_y + 32, city_display, text);
    gfx_draw_string_transparent(x + 24, hdr_y + 52, fetch_status, muted);

    // Weather icon + temperature
    int temp_card_y = hdr_y + 84;
    gfx_fill_rect(x + 12, temp_card_y, w - 24, 120, card_bg);
    gfx_draw_rect_outline(x + 12, temp_card_y, w - 24, 120, 1, border);

    // Sun accent for icon
    uint32_t sun_col = 0xFBBF24;
    uint32_t cloud_col = (p->theme == 0) ? 0x30363D : 0xD1D5DB;
    draw_weather_icon(x + 20, temp_card_y + 10, weathercode, sun_col, cloud_col);

    // Temperature
    int tx = x + 80;
    char tstr[16];
    double tv = temperature;
    int ti = 0;
    if (tv < 0) { tstr[ti++] = '-'; tv = -tv; }
    int ip = (int)tv;
    if (weathercode >= 0) {
        tstr[ti++] = (ip / 10) + '0';
        tstr[ti++] = (ip % 10) + '0';
        tstr[ti++] = '.';
        int fr = (int)((tv - ip) * 10.0 + 0.5);
        tstr[ti++] = fr + '0';
        tstr[ti++] = 0;
    } else {
        tstr[ti++] = '-'; tstr[ti++] = '-'; tstr[ti++] = '.'; tstr[ti++] = '-'; tstr[ti++] = 0;
    }

    gfx_draw_string_transparent(tx, temp_card_y + 16, tstr, text);
    gfx_draw_string_transparent(tx + gfx_strlen(tstr) * 8, temp_card_y + 24, "°C", muted);

    const char* lbl = weather_label(weathercode);
    gfx_draw_string_transparent(x + 24, temp_card_y + 70, lbl, text);
    gfx_draw_string_transparent(x + 24, temp_card_y + 88, "Updated: just now", muted);

    // Details card
    int det_y = temp_card_y + 132;
    gfx_fill_rect(x + 12, det_y, w - 24, 100, card_bg);
    gfx_draw_rect_outline(x + 12, det_y, w - 24, 100, 1, border);
    gfx_draw_string_transparent(x + 24, det_y + 12, "Details", accent);

    // Wind speed
    char ws[16];
    if (weathercode >= 0) {
        double wv = windspeed;
        int ti = 0;
        int ip = (int)wv;
        ws[ti++] = (ip / 10) + '0';
        ws[ti++] = (ip % 10) + '0';
        ws[ti++] = '.';
        int fr = (int)((wv - ip) * 10.0 + 0.5);
        ws[ti++] = fr + '0';
        ws[ti++] = ' ';
        ws[ti++] = 'k'; ws[ti++] = 'm'; ws[ti++] = '/'; ws[ti++] = 'h';
        ws[ti] = 0;
    } else {
        strcpy(ws, "-- km/h");
    }
    gfx_draw_string_transparent(x + 24, det_y + 36, "Wind", muted);
    gfx_draw_string_transparent(x + 24, det_y + 52, ws, text);

    // Wind direction
    char wd[16];
    if (weathercode >= 0) {
        int wd_val = winddir;
        int ti = 0;
        if (wd_val >= 100) { wd[ti++] = (wd_val / 100) + '0'; wd_val %= 100; }
        else if (wd_val >= 10) { wd[ti++] = (wd_val / 10) + '0'; wd_val %= 10; }
        wd[ti++] = wd_val + '0';
        wd[ti++] = '\260';
        wd[ti] = 0;
    } else {
        strcpy(wd, "--\260");
    }
    gfx_draw_string_transparent(x + 200, det_y + 36, "Direction", muted);
    gfx_draw_string_transparent(x + 200, det_y + 52, wd, text);

    // City selector
    int sel_y = det_y + 112;
    gfx_draw_string_transparent(x + 24, sel_y, "City:", muted);
    gfx_draw_string_transparent(x + 80, sel_y, city_display, text);

    // Compass indicator
    if (weathercode >= 0 && winddir >= 0) {
        int cx = x + w - 50;
        int cy = det_y + 45;
        gfx_draw_line(cx, cy, cx, cy - 14, border);
        gfx_draw_string_transparent(cx + 4, cy - 14, "N", muted);
        gfx_draw_line(cx, cy, cx, cy - 10, accent);
        gfx_draw_line(cx, cy, cx + 4, cy + 4, accent);
        gfx_draw_line(cx, cy, cx - 4, cy + 4, accent);
    }
}

static void weather_on_key(int id, char key) {
    (void)id;
    if (key == 27) { wm_close_window(id); }
}

static void setup_weather_buttons(void) {
    wm_clear_buttons(weather_win_id);
    personalization_t* p = get_personalization();
    uint32_t btn_bg = (p->theme == 0) ? 0x21262D : 0xF3F4F6;
    uint32_t txt = (p->theme == 0) ? 0xE8EAED : 0x1F2937;
    int bw = 50, bh = 28, by = 8;
    int x1 = 12, x2 = WEATHER_W - 62, x3 = WEATHER_W / 2 - 25;
    wm_add_button(weather_win_id, 1, x1, by, bw, bh, "<", btn_bg, txt, weather_prev_city);
    wm_add_button(weather_win_id, 2, x2, by, bw, bh, ">", btn_bg, txt, weather_next_city);
    int rx = x3;
    int rw = 70;
    wm_add_button(weather_win_id, 3, rx, by, rw, bh, "Refresh", btn_bg, txt, weather_refresh);
}

static void weather_on_resize(int win_id, int w, int h) {
    (void)win_id; (void)w; (void)h;
    setup_weather_buttons();
}

void weather_app(void) {
    if (wm_get_window(weather_win_id)) { wm_bring_to_front(weather_win_id); return; }

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int wx = (fw - WEATHER_W) / 2;
    int wy = (fh - WEATHER_H) / 2;

    weather_win_id = wm_open_window(wx, wy, WEATHER_W, WEATHER_H,
        "Weather", get_accent_color(),
        weather_on_render, weather_on_key, weather_on_resize);
    if (weather_win_id < 0) return;

    setup_weather_buttons();
    // Auto-fetch on open
    create_task(weather_fetch_task, "Weather Fetch");
}
