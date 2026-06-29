#include "apps/httpviewer.h"
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "gui/wm.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "kernel/net/socket.h"
#include "kernel/net/in.h"
#include "kernel/task/task.h"
#include "kernel/mem/kheap.h"
#include <string.h>

extern void serial_puts(const char* s);
extern void itoa(uint64_t n, char* s);
extern void sleep_task(uint32_t ms);
extern void exit_task(int code);
extern int create_task(void (*fn)(void), const char* name);
extern int dns_resolve(const char* hostname, uint32_t* ip_addr);
extern void net_poll(void);
extern uint32_t timer_get_ms(void);

#define BW_CHAR_W 8
#define BW_LINE_H 16
#define RAW_MAX 49152

static int bw_win_id = -1;
static char raw_buf[RAW_MAX];
static int raw_len = 0;
static int scroll_y = 0;
static int content_h = 0;
static int bw_loading = 0;
static int bw_fetch_id = 0;
static char url_buf[256];
static int url_len = 0;
static int editing_url = 0;
static char url_text[256];
static int url_text_len = 0;
static int fetch_timed_out = 0;
static uint32_t fetch_start = 0;

static int bw_strlen(const char* s) {
    int n = 0; while (s[n]) n++; return n;
}

static void parse_url(const char* url, char* host, int hmax, char* path, int pmax, int* port_out) {
    int i = 0, default_port = 80;
    if (bw_strlen(url) > 7 && url[0]=='h' && url[1]=='t' && url[2]=='t' && url[3]=='p' && url[4]==':' && url[5]=='/' && url[6]=='/') i = 7;
    int hi = 0;
    while (url[i] && url[i] != '/' && url[i] != ':' && hi < hmax - 1) host[hi++] = url[i++];
    host[hi] = 0;
    *port_out = default_port;
    if (url[i] == ':') { i++; int p = 0; while (url[i] >= '0' && url[i] <= '9') p = p * 10 + (url[i++] - '0'); *port_out = p; }
    int pi = 0;
    if (url[i] == '/') { while (url[i] && pi < pmax - 1) path[pi++] = url[i++]; }
    else { path[0] = '/'; pi = 1; }
    path[pi] = 0;
}

static void bw_fetch_internal(const char* url) {
    int my_id = bw_fetch_id;
    char host[128], path[256];
    int port = 80;
    parse_url(url, host, sizeof(host), path, sizeof(path), &port);
    uint32_t ip;
    if (dns_resolve(host, &ip) < 0) { if(my_id == bw_fetch_id) { bw_loading = 0; } return; }
    int sock = sys_socket(2, 1, 0);
    if (sock < 0) { if(my_id == bw_fetch_id) { bw_loading = 0; } return; }
    struct sockaddr_in addr;
    addr.sin_family = 2;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = ip;
    if (sys_connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if(my_id == bw_fetch_id) { bw_loading = 0; } sys_socket_close(sock); return;
    }
    char req[768];
    int ri = 0;
    #define R(s) do { const char* _s = s; while (*_s) req[ri++] = *_s++; } while(0)
    R("GET "); for (int pi2=0; path[pi2] && ri<700; pi2++) req[ri++] = path[pi2];
    R(" HTTP/1.0\r\nHost: ");
    for (int hi2=0; host[hi2] && ri<740; hi2++) req[ri++] = host[hi2];
    R("\r\nUser-Agent: HTTPViewer/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n");
    req[ri] = 0;
    #undef R
    sys_send(sock, req, ri, 0);
    raw_len = 0;
    int no_data = 0;
    while (no_data < 5 && raw_len < RAW_MAX - 1) {
        int n = sys_recv(sock, raw_buf + raw_len, RAW_MAX - 1 - raw_len, 0);
        if (n <= 0) { if (sys_socket_closed(sock)) break; no_data++; }
        else { raw_len += n; no_data = 0; }
    }
    raw_buf[raw_len] = 0;
    sys_socket_close(sock);
    if (my_id != bw_fetch_id) return;
    bw_loading = 0;
}

static void bw_fetch_task(void) {
    bw_fetch_internal(url_buf);
    exit_task(0);
}

static void bw_start_fetch(void) {
    bw_loading = 1;
    bw_fetch_id++;
    raw_len = 0;
    scroll_y = 0;
    fetch_timed_out = 0;
    fetch_start = timer_get_ms();
    create_task(bw_fetch_task, "HTTPViewer Fetch");
}

static void bw_on_render(int win_id, int x, int y, int w, int h, int vx, int vy) {
    (void)win_id; (void)vx; (void)vy;
    int content_y = y + 36;
    int content_h_avail = h - 36;
    gfx_fill_rect(x, y, w, 36, 0x1C1C1E);
    gfx_draw_string_transparent(x + 8, y + 10, "URL:", 0x8E8E93);
    gfx_fill_rect(x + 44, y + 6, w - 56, 24, editing_url ? 0x2C2C2E : 0x3A3A3C);
    char display_url[260];
    if (editing_url) {
        int i;
        for (i = 0; i < url_text_len && i < 255; i++) display_url[i] = url_text[i];
        display_url[i] = 0;
    } else {
        int i;
        for (i = 0; i < url_len && i < 255; i++) display_url[i] = url_buf[i];
        display_url[i] = 0;
    }
    gfx_draw_string_transparent(x + 48, y + 11, display_url[0] ? display_url : "Press / to enter URL", display_url[0] ? 0xFFFFFF : 0x6E6E73);
    gfx_fill_rect(x, content_y, w, content_h_avail, 0x1A1D24);
    if (bw_loading) {
        gfx_draw_string_transparent(x + w/2 - 40, content_y + content_h_avail/2, "Loading...", 0x8E8E93);
        return;
    }
    if (raw_len > 0) {
        int oy = content_y - scroll_y;
        int total_h = 0;
        int i = 0;
        while (i < raw_len) {
            int line_start = i;
            while (i < raw_len && raw_buf[i] != '\n') i++;
            int line_len = i - line_start;
            if (i < raw_len && raw_buf[i] == '\n') i++;
            int ly = oy + total_h;
            if (ly + BW_LINE_H >= content_y && ly < content_y + content_h_avail) {
                char dbuf[256];
                int di = 0;
                for (int j = 0; j < line_len && di < 254; j++) {
                    char c = raw_buf[line_start + j];
                    dbuf[di++] = (c >= 32 && c < 127) ? c : '.';
                }
                dbuf[di] = 0;
                gfx_draw_string_transparent(x + 8, ly, dbuf, 0xFFFFFF);
            }
            total_h += BW_LINE_H;
        }
        content_h = total_h;
    } else if (!bw_loading) {
        if (fetch_timed_out)
            gfx_draw_string_transparent(x + w/2 - 70, content_y + content_h_avail/2, "Connection timed out", 0xFF4444);
        else
            gfx_draw_string_transparent(x + w/2 - 80, content_y + content_h_avail/2, "Press / to enter URL", 0x8E8E93);
    }
    if (content_h > content_h_avail) {
        int sb_x = x + w - 6;
        gfx_fill_rect(sb_x, content_y, 6, content_h_avail, 0x2C2C2E);
        int thumb_h = content_h_avail * content_h_avail / content_h;
        if (thumb_h < 16) thumb_h = 16;
        int sb_range = content_h - content_h_avail;
        if (sb_range > 0) {
            int thumb_y = content_y + scroll_y * (content_h_avail - thumb_h) / sb_range;
            gfx_fill_rect_rounded(sb_x + 1, thumb_y, 4, thumb_h, 2, 0x8E8E93);
        }
    }
}

static void bw_on_key(int win_id, char key) {
    (void)win_id;
    unsigned char k = (unsigned char)key;
    if (k == '/' && !editing_url) {
        editing_url = 1;
        url_text[0] = 0;
        url_text_len = 0;
        return;
    }
    if (k == 27) { editing_url = 0; return; }
    if ((k == '\n' || k == '\r') && editing_url) {
        editing_url = 0;
        url_text[url_text_len] = 0;
        int i;
        for (i = 0; i < url_text_len && i < 255; i++) url_buf[i] = url_text[i];
        url_buf[i] = 0;
        url_len = i;
        bw_start_fetch();
        return;
    }
    if (editing_url) {
        if (k == '\b' && url_text_len > 0) url_text_len--;
        else if (k >= 32 && k < 127 && url_text_len < 250) { url_text[url_text_len++] = k; url_text[url_text_len] = 0; }
        return;
    }
    if (k == KEY_UP) { scroll_y -= 40; if (scroll_y < 0) scroll_y = 0; }
    if (k == KEY_DOWN) { scroll_y += 40; }
    if (k == KEY_PAGE_UP) { scroll_y -= 300; if (scroll_y < 0) scroll_y = 0; }
    if (k == KEY_PAGE_DOWN) { scroll_y += 300; }
}

static void bw_timeout_task(void) {
    while (1) {
        sleep_task(100);
        if (bw_loading && timer_get_ms() - fetch_start > 3000) {
            bw_fetch_id++;
            bw_loading = 0;
            fetch_timed_out = 1;
            return;
        }
        if (!bw_loading) return;
    }
}

void httpviewer(void) {
    uint32_t fw = gfx_get_fb_width();
    uint32_t fh = gfx_get_fb_height();
    bw_win_id = wm_open_window((fw - 900) / 2, (fh - 650) / 2, 900, 650, "HTTP Viewer", 0x007AFF, bw_on_render, bw_on_key, NULL);
    if (bw_win_id < 0) return;
    url_buf[0] = 0; url_len = 0;
    raw_len = 0; scroll_y = 0; content_h = 0;
    bw_loading = 0; editing_url = 0; fetch_timed_out = 0;
    {
        const char* test_url = "http://10.0.2.2:8080/";
        int i;
        for (i = 0; test_url[i] && i < 255; i++) url_buf[i] = test_url[i];
        url_buf[i] = 0;
        url_len = i;
        bw_start_fetch();
        create_task(bw_timeout_task, "HTTPViewer Timer");
    }
}
