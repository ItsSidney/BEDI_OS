#include "gui/gui.h"
#include "gui/wm.h"
#include "drivers/video/gfx.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "kernel/net/if.h"
#include "kernel/net/in.h"
#include "kernel/net/socket.h"
#include "kernel/net/dns.h"
#include "kernel/time/timer.h"
#include "drivers/video/framebuffer.h"
#include "kernel/task/task.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

extern int dns_resolve(const char* hostname, uint32_t* ip_addr);
extern void net_poll(void);

#define ND_WIN_W 800
#define ND_WIN_H 580

#define ND_BUF_LINES 512
#define ND_LINE_LEN 128
#define ND_INPUT_LEN 80
#define ND_RECV_BUF 4096
#define ND_POLL_TIMEOUT 8000

static int nd_win_id = -1;
static int nd_busy;
static int nd_current_action; /* 0-5 */
static char nd_input[ND_INPUT_LEN];
static int nd_input_len;
static int nd_input_active;

static struct ifnet* nd_iface;

static char nd_log_lines[ND_BUF_LINES][ND_LINE_LEN];
static int nd_log_count;
static int nd_log_scroll;

static char nd_recv_buf[ND_RECV_BUF];

static uint32_t nd_op_start_ms;
static int nd_op_has_progress;
static int nd_op_progress_current;
static int nd_op_progress_total;

static int nd_hover_btn = -1;
static int nd_hover_log;

/* ---- Log helpers ---- */
static void nd_log(const char* s, uint32_t color_hint) {
    (void)color_hint;
    if (nd_log_count >= ND_BUF_LINES) {
        for (int i = 1; i < ND_BUF_LINES; i++)
            memcpy(nd_log_lines[i-1], nd_log_lines[i], ND_LINE_LEN);
        nd_log_count--;
    }
    int sl = 0;
    while (s[sl] && sl < ND_LINE_LEN - 1) {
        nd_log_lines[nd_log_count][sl] = s[sl];
        sl++;
    }
    nd_log_lines[nd_log_count][sl] = 0;
    nd_log_count++;
    nd_log_scroll = nd_log_count;
}

#define nd_logf(fmt, ...) do { char nd_buf_[ND_LINE_LEN]; snprintf(nd_buf_, ND_LINE_LEN, fmt, ##__VA_ARGS__); nd_log(nd_buf_, 0); } while(0)

/* ---- Helpers ---- */
static void nd_refresh_iface(void) {
    nd_iface = if_list_head();
}

static int nd_get_ip(const char* host, uint32_t* ip) {
    /* Try as dotted decimal first */
    int is_ip = 1;
    uint32_t ip_val = 0;
    int octets = 0, cur = 0;
    for (int i = 0; host[i]; i++) {
        if (host[i] >= '0' && host[i] <= '9') cur = cur * 10 + (host[i] - '0');
        else if (host[i] == '.') { ip_val = (ip_val << 8) | (cur & 0xFF); cur = 0; octets++; }
        else { is_ip = 0; break; }
    }
    if (is_ip && octets == 3) {
        ip_val = (ip_val << 8) | (cur & 0xFF);
        *ip = htonl(ip_val);
        return 0;
    }
    return dns_resolve(host, ip);
}

static void nd_ip_str(uint32_t ip_nbo, char* out, int out_len) {
    uint32_t h = ntohl(ip_nbo);
    snprintf(out, out_len, "%u.%u.%u.%u",
        (h >> 24) & 0xFF, (h >> 16) & 0xFF, (h >> 8) & 0xFF, h & 0xFF);
}

static void nd_poll_recv(int sock, int timeout_ms) {
    uint32_t t0 = timer_get_ms();
    while (timer_get_ms() - t0 < (uint32_t)timeout_ms) {
        net_poll();
        int n = sys_recv(sock, nd_recv_buf, ND_RECV_BUF - 1, 0);
        if (n > 0) {
            nd_recv_buf[n] = 0;
            return;
        }
        if (n < 0 || sys_socket_closed(sock)) return;
        sleep_task(10);
    }
}

/* ---- Actions ---- */

static void nd_action_ping(void) {
    nd_log("--- PING ---", 0);
    if (nd_input_len == 0) { nd_log("Enter a hostname or IP first", 0); goto end; }
    nd_input[nd_input_len] = 0;
    nd_logf("Target: %s", nd_input);

    uint32_t ip;
    uint32_t dns_t0 = timer_get_ms();
    if (nd_get_ip(nd_input, &ip) < 0) { nd_log("DNS: FAILED", 0); goto end; }
    uint32_t dns_ms = timer_get_ms() - dns_t0;
    char ips[24]; nd_ip_str(ip, ips, 24);
    nd_logf("DNS: %u ms  ->  %s", dns_ms, ips);

    int ok = 0, fail = 0;
    uint32_t min_ms = 9999, max_ms = 0, total_ms = 0;
    nd_op_has_progress = 1;
    nd_op_progress_current = 0;
    nd_op_progress_total = 4;

    for (int i = 0; i < 4; i++) {
        nd_op_progress_current = i;
        uint32_t t0 = timer_get_ms();
        int r = sys_ping(ip);
        uint32_t ms = timer_get_ms() - t0;
        if (r == 0) {
            ok++; if (ms < min_ms) min_ms = ms; if (ms > max_ms) max_ms = ms; total_ms += ms;
            nd_logf("  seq=%d  %u ms  OK", i+1, ms);
        } else {
            fail++;
            nd_logf("  seq=%d  TIMEOUT", i+1);
        }
        if (i < 3) sleep_task(200);
    }

    nd_op_has_progress = 0;
    if (ok > 0)
        nd_logf("Results: %d/%d  min/avg/max = %u/%u/%u ms", ok, 4, min_ms, total_ms/ok, max_ms);
    else
        nd_log("Results: All packets lost", 0);
end:
    nd_log("---", 0);
}

static void nd_action_dns(void) {
    nd_log("--- DNS Lookup ---", 0);
    if (nd_input_len == 0) { nd_log("Enter a hostname first", 0); goto end; }
    nd_input[nd_input_len] = 0;
    nd_logf("Query: %s", nd_input);

    uint32_t t0 = timer_get_ms();
    uint32_t ip;
    if (dns_resolve(nd_input, &ip) < 0) {
        nd_logf("FAILED  (%u ms)", timer_get_ms() - t0);
        goto end;
    }
    uint32_t ms = timer_get_ms() - t0;
    char ips[24]; nd_ip_str(ip, ips, 24);
    nd_logf("Result: %s  (%u ms)", ips, ms);
    nd_logf("Hex: 0x%08X", ntohl(ip));
end:
    nd_log("---", 0);
}

static void nd_action_connect(void) {
    nd_log("--- TCP Connect ---", 0);
    if (nd_input_len == 0) { nd_log("Enter host:port first (e.g. google.com:80)", 0); goto end; }
    nd_input[nd_input_len] = 0;

    char host[64]; int port = 80;
    int pos = 0, pi = 0;
    while (nd_input[pos] && nd_input[pos] != ':' && pi < 62) host[pi++] = nd_input[pos++];
    host[pi] = 0;
    if (nd_input[pos] == ':') { pos++; port = 0; while (nd_input[pos] >= '0' && nd_input[pos] <= '9') port = port * 10 + (nd_input[pos++] - '0'); }

    nd_logf("Target: %s : %d", host, port);

    uint32_t ip;
    uint32_t dns_t0 = timer_get_ms();
    if (nd_get_ip(host, &ip) < 0) { nd_log("DNS: FAILED", 0); goto end; }
    uint32_t dns_ms = timer_get_ms() - dns_t0;
    char ips[24]; nd_ip_str(ip, ips, 24);
    nd_logf("DNS: %u ms  ->  %s", dns_ms, ips);

    int sock = sys_socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { nd_log("Socket: FAILED", 0); goto end; }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = ip;

    uint32_t conn_t0 = timer_get_ms();
    int res = sys_connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    uint32_t conn_ms = timer_get_ms() - conn_t0;

    if (res != 0) {
        nd_logf("Connect: %u ms  REFUSED / TIMEOUT", conn_ms);
        sys_socket_close(sock); goto end;
    }
    nd_logf("Connect: %u ms  SUCCESS", conn_ms);

    if (port == 80 || port == 8080) {
        nd_log("Sending HTTP GET ...", 0);
        char req[512];
        int ri = snprintf(req, 512, "GET / HTTP/1.0\r\nHost: %s\r\nUser-Agent: BEDI-OS/3.0\r\nConnection: close\r\n\r\n", host);
        sys_send(sock, req, ri > 511 ? 511 : ri, 0);
        net_poll(); sleep_task(50);

        uint32_t recv_t0 = timer_get_ms();
        int total = 0;
        uint32_t ttfb = 0;
        while (timer_get_ms() - recv_t0 < ND_POLL_TIMEOUT) {
            net_poll();
            int n = sys_recv(sock, nd_recv_buf + total, ND_RECV_BUF - total - 1, 0);
            if (n > 0) {
                if (ttfb == 0) ttfb = timer_get_ms() - recv_t0;
                total += n; nd_recv_buf[total] = 0;
                if (total >= ND_RECV_BUF - 1) break;
            } else if (n < 0 || sys_socket_closed(sock)) break;
            sleep_task(10);
        }
        uint32_t resp_ms = timer_get_ms() - recv_t0;
        nd_logf("Response: %d bytes  (%u ms, TTFB=%u ms)", total, resp_ms, ttfb);

        if (total > 0) {
            int si = 0;
            while (si < total && nd_recv_buf[si] != '\r' && nd_recv_buf[si] != '\n') si++;
            if (si > 0 && si < 64) {
                char st[64]; memcpy(st, nd_recv_buf, si); st[si] = 0;
                nd_logf("Status: %s", st);
                if (strstr(st, "200")) nd_log("HTTP: OK", 0);
                else if (strstr(st, "301") || strstr(st, "302")) nd_log("HTTP: Redirect", 0);
                else if (strstr(st, "404")) nd_log("HTTP: Not Found", 0);
                else if (strstr(st, "500")) nd_log("HTTP: Server Error", 0);
            }
            int preview_start = si + 1;
            if (preview_start < total) {
                int plen = total - preview_start;
                if (plen > 200) plen = 200;
                nd_log("--- Body preview ---", 0);
                int off = 0;
                while (off < plen) {
                    int end = off + 100;
                    if (end > plen) end = plen;
                    char ln[120]; int li = 0;
                    for (int j = off; j < end && li < 118; j++) {
                        char c = nd_recv_buf[preview_start + j];
                        if (c == '\r') continue;
                        if (c == '\n') { if (li < 117) { ln[li++] = ' '; ln[li++] = '|'; } }
                        else ln[li++] = c;
                    }
                    ln[li] = 0;
                    nd_log(ln, 0);
                    off = end;
                }
            }
        } else nd_log("No data received", 0);
    } else nd_logf("Port %d: OPEN", port);

    sys_socket_close(sock);
end:
    nd_log("---", 0);
}

static void nd_action_speed(void) {
    nd_log("--- Speed Test ---", 0);
    if (nd_input_len == 0) { nd_log("Enter hostname:port (e.g. httpbin.org:80)", 0); goto end; }
    nd_input[nd_input_len] = 0;

    char host[64]; int port = 80;
    int pos = 0, pi = 0;
    while (nd_input[pos] && nd_input[pos] != ':' && pi < 62) host[pi++] = nd_input[pos++];
    host[pi] = 0;
    if (nd_input[pos] == ':') { pos++; port = 0; while (nd_input[pos] >= '0' && nd_input[pos] <= '9') port = port * 10 + (nd_input[pos++] - '0'); }

    nd_logf("Target: %s : %d", host, port);

    uint32_t ip;
    if (nd_get_ip(host, &ip) < 0) { nd_log("DNS: FAILED", 0); goto end; }
    char ips[24]; nd_ip_str(ip, ips, 24);
    nd_logf("Resolved: %s", ips);

    int sock = sys_socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { nd_log("Socket: FAILED", 0); goto end; }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = ip;

    uint32_t conn_t0 = timer_get_ms();
    if (sys_connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        nd_logf("Connect: FAILED  (%u ms)", timer_get_ms() - conn_t0);
        sys_socket_close(sock); goto end;
    }
    uint32_t conn_ms = timer_get_ms() - conn_t0;
    nd_logf("Connect: %u ms  OK", conn_ms);

    /* Request a decent-sized response */
    char req[512];
    int ri = snprintf(req, 512,
        "GET /bytes/10240 HTTP/1.0\r\n"
        "Host: %s\r\n"
        "User-Agent: BEDI-OS/3.0\r\n"
        "Connection: close\r\n\r\n", host);
    sys_send(sock, req, ri > 511 ? 511 : ri, 0);
    net_poll(); sleep_task(50);

    nd_op_has_progress = 1;
    nd_op_progress_current = 0;
    nd_op_progress_total = 100;

    uint32_t t0 = timer_get_ms();
    int total = 0;
    uint32_t ttfb = 0;
    uint32_t last_update = t0;

    while (timer_get_ms() - t0 < 10000) {
        net_poll();
        int n = sys_recv(sock, nd_recv_buf, ND_RECV_BUF - 1, 0);
        if (n > 0) {
            if (ttfb == 0) ttfb = timer_get_ms() - t0;
            total += n;
            uint32_t now = timer_get_ms();
            if (now - last_update > 300) {
                last_update = now;
                int pct = (now - t0) * 100 / 10000;
                if (pct > 99) pct = 99;
                nd_op_progress_current = pct;
            }
        }
        if (n < 0 || sys_socket_closed(sock)) break;
        sleep_task(10);
    }
    uint32_t elapsed = timer_get_ms() - t0;

    nd_op_has_progress = 0;
    sys_socket_close(sock);

    if (total == 0) {
        nd_log("Speed test: No data received", 0);
        goto end;
    }

    /* Calculate speeds (integer math — no FPU) */
    if (elapsed == 0) elapsed = 1;
    uint64_t bytes_per_sec = (uint64_t)total * 1000 / elapsed;
    uint32_t kbps = (uint32_t)(bytes_per_sec / 1024);
    uint32_t mbps_x100 = (uint32_t)(bytes_per_sec * 800 / (1024 * 1024));

    nd_logf("Downloaded: %d bytes  (%u ms)", total, elapsed);
    nd_logf("TTFB: %u ms", ttfb);
    nd_logf("Speed: %u KB/s  (%u.%02u Mbps)", kbps, mbps_x100 / 100, mbps_x100 % 100);
    if (mbps_x100 < 10) nd_log("Connection: Very slow", 0);
    else if (mbps_x100 < 100) nd_log("Connection: Slow", 0);
    else if (mbps_x100 < 500) nd_log("Connection: Moderate", 0);
    else if (mbps_x100 < 2000) nd_log("Connection: Fast", 0);
    else nd_log("Connection: Very fast", 0);

    nd_log("--- Test complete ---", 0);
end:
    nd_log("---", 0);
}

static void nd_action_portscan(void) {
    nd_log("--- Port Scan ---", 0);
    if (nd_input_len == 0) { nd_log("Enter a hostname first", 0); goto end; }
    nd_input[nd_input_len] = 0;
    nd_logf("Target: %s", nd_input);

    uint32_t ip;
    if (nd_get_ip(nd_input, &ip) < 0) { nd_log("DNS: FAILED", 0); goto end; }
    char ips[24]; nd_ip_str(ip, ips, 24);
    nd_logf("Resolved: %s", ips);

    int ports[] = {21,22,23,25,53,80,110,143,443,445,8080,8443,3306,5432,6379,27017};
    int np = sizeof(ports)/sizeof(ports[0]);
    nd_op_has_progress = 1;
    nd_op_progress_current = 0;
    nd_op_progress_total = np;

    int open_count = 0;
    for (int i = 0; i < np; i++) {
        nd_op_progress_current = i;
        int sock = sys_socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        struct sockaddr_in a;
        a.sin_family = AF_INET;
        a.sin_port = htons((uint16_t)ports[i]);
        a.sin_addr.s_addr = ip;
        uint32_t t0 = timer_get_ms();
        int r = sys_connect(sock, (struct sockaddr*)&a, sizeof(a));
        uint32_t ms = timer_get_ms() - t0;
        if (r == 0) {
            nd_logf("  Port %-5d  OPEN   (%u ms)", ports[i], ms);
            open_count++;
            sleep_task(10);
        }
        sys_socket_close(sock);
    }

    nd_op_has_progress = 0;
    nd_logf("Result: %d/%d ports open", open_count, np);
end:
    nd_log("---", 0);
}

static void nd_action_info(void) {
    nd_log("--- Network Info ---", 0);
    nd_refresh_iface();

    if (!nd_iface) { nd_log("No network interfaces found", 0); goto end; }

    struct ifnet* ifp = nd_iface;
    char ips[24]; nd_ip_str(ifp->if_ip, ips, 24);
    nd_logf("Interface: %s", ifp->if_xname);
    nd_logf("IP: %s", ips);
    nd_logf("MAC: %02X:%02X:%02X:%02X:%02X:%02X",
        ifp->if_hwaddr[0], ifp->if_hwaddr[1], ifp->if_hwaddr[2],
        ifp->if_hwaddr[3], ifp->if_hwaddr[4], ifp->if_hwaddr[5]);
    nd_logf("Flags: %s%s  MTU: %u",
        (ifp->if_flags & IFF_UP) ? "UP" : "DOWN",
        (ifp->if_flags & IFF_RUNNING) ? "+RUNNING" : "",
        ifp->if_mtu);
    nd_logf("Rx: %llu pkts  Err: %llu  Tx: %llu pkts  Err: %llu",
        ifp->if_ipackets, ifp->if_ierrors, ifp->if_opackets, ifp->if_oerrors);

    /* Count interfaces */
    int count = 0;
    for (struct ifnet* it = if_list_head(); it; it = it->if_next) count++;
    nd_logf("Active interfaces: %d", count);

end:
    nd_log("---", 0);
}

/* ---- Render ---- */
static void nd_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    uint32_t bg      = 0x0D1117;
    uint32_t card    = 0x161B22;
    uint32_t border  = 0x30363D;
    uint32_t text    = 0xE6EDF3;
    uint32_t dim     = 0x8B949E;
    uint32_t accent  = get_accent_color();
    uint32_t success = 0x3FB950;
    uint32_t err_clr = 0xF85149;
    uint32_t warn    = 0xD29922;

    gfx_fill_rect(x, y, w, h, bg);

    int HEADER_H = 28;
    /* ---- Header ---- */
    gfx_fill_rect(x, y, w, HEADER_H, card);
    gfx_draw_hline(x, y + HEADER_H, w, border);
    gfx_draw_string_transparent(x + 10, y + 6, "Network Debug v3", 0xFFFFFF);
    if (nd_busy) {
        char busy[48];
        snprintf(busy, 48, "[working... %u]", (timer_get_ms() - nd_op_start_ms) / 1000);
        gfx_draw_string_transparent(x + w - 90, y + 6, busy, warn);
    }

    int cw = w - 16;
    int cx = x + 8;

    /* ---- Interface card ---- */
    nd_refresh_iface();
    int iface_y = y + HEADER_H + 4;
    int iface_h = 54;
    gfx_fill_rect(cx, iface_y, cw, iface_h, card);
    gfx_draw_rect_outline(cx, iface_y, cw, iface_h, 1, border);

    if (nd_iface) {
        char ips[24]; nd_ip_str(nd_iface->if_ip, ips, 24);
        char line1[100];
        snprintf(line1, 100, "%s  |  %s  |  %02X:%02X:%02X:%02X:%02X:%02X",
            nd_iface->if_xname, ips,
            nd_iface->if_hwaddr[0], nd_iface->if_hwaddr[1], nd_iface->if_hwaddr[2],
            nd_iface->if_hwaddr[3], nd_iface->if_hwaddr[4], nd_iface->if_hwaddr[5]);
        gfx_draw_string_transparent(cx + 8, iface_y + 5, line1, text);

        char line2[80];
        snprintf(line2, 80, "Rx: %llu  Tx: %llu  Err: %llu  MTU: %u",
            nd_iface->if_ipackets, nd_iface->if_opackets,
            nd_iface->if_ierrors + nd_iface->if_oerrors, nd_iface->if_mtu);
        gfx_draw_string_transparent(cx + 8, iface_y + 22, line2, dim);

        /* Status dots */
        int dot_x = cx + cw - 100;
        int is_up = (nd_iface->if_flags & IFF_UP) != 0;
        int is_run = (nd_iface->if_flags & IFF_RUNNING) != 0;
        gfx_fill_circle(dot_x, iface_y + 12, 4, is_up ? success : err_clr);
        gfx_draw_string_transparent(dot_x + 8, iface_y + 6, is_up ? "UP" : "DOWN", is_up ? success : err_clr);
        gfx_fill_circle(dot_x + 40, iface_y + 12, 4, is_run ? success : err_clr);
        gfx_draw_string_transparent(dot_x + 48, iface_y + 6, is_run ? "RUN" : "STOP", is_run ? success : err_clr);
    } else {
        gfx_draw_string_transparent(cx + 8, iface_y + 20, "No network interface found", err_clr);
    }

    /* ---- Action buttons + input ---- */
    int action_y = iface_y + iface_h + 4;
    int input_h = 24;
    int btn_h = 24;
    int action_h = input_h + 4 + btn_h;

    /* Input box */
    int input_w = cw - 150;
    gfx_fill_rect(cx, action_y, input_w, input_h, bg);
    gfx_draw_rect_outline(cx, action_y, input_w, input_h, 1, nd_input_active ? accent : border);

    char disp[ND_INPUT_LEN + 4];
    int di = 0; disp[di++] = '>';
    for (int i = 0; i < nd_input_len && di < ND_INPUT_LEN + 2; i++) disp[di++] = nd_input[i];
    if (nd_input_active && (timer_get_ms() / 500) % 2) disp[di++] = '_';
    disp[di] = 0;
    gfx_draw_string_transparent(cx + 6, action_y + 4, disp, text);

    /* Action buttons (row above input) */
    const char* labels[] = {"PING", "DNS", "CONN", "SPEED", "SCAN", "INFO"};
    int btn_count = 6;
    int btn_w = (cw - (btn_count - 1) * 4) / btn_count;
    if (btn_w > 90) btn_w = 90;

    int btn_row_y = action_y;
    int btn_x = cx + cw - (btn_w * btn_count + (btn_count - 1) * 4);

    for (int i = 0; i < btn_count; i++) {
        int is_sel = (nd_current_action == i);
        int is_hover = (nd_hover_btn == i);
        uint32_t btn_bg = is_sel ? accent : card;
        if (is_hover && !is_sel) btn_bg = 0x21262D;
        if (nd_busy && is_sel) btn_bg = warn;

        gfx_fill_rect_rounded(btn_x, btn_row_y, btn_w, btn_h, 4, btn_bg);
        gfx_draw_rect_rounded_outline(btn_x, btn_row_y, btn_w, btn_h, 4, 1,
            is_sel ? accent : border);

        uint32_t fg = is_sel ? 0xFFFFFF : (is_hover ? text : dim);
        int tx = btn_x + (btn_w - (int)strlen(labels[i]) * 8) / 2;
        gfx_draw_string_transparent(tx, btn_row_y + 4, labels[i], fg);
        btn_x += btn_w + 4;
    }

    /* ---- Progress bar ---- */
    if (nd_op_has_progress && nd_op_progress_total > 0) {
        int bar_y = action_y + input_h + 4;
        int bar_h = 4;
        gfx_fill_rect(cx, bar_y, cw, bar_h, card);
        int fill = nd_op_progress_current * cw / nd_op_progress_total;
        if (fill > 0) gfx_fill_rect(cx, bar_y, fill, bar_h, accent);
    }

    /* ---- Log area ---- */
    int log_y = action_y + action_h + 4;
    if (nd_op_has_progress && nd_op_progress_total > 0) log_y += 8;
    int log_h = h - log_y - 22;
    int lh = 14;

    gfx_fill_rect(cx, log_y, cw, log_h, bg);
    if (log_h > 0) {
        int vis = log_h / lh;
        int start = nd_log_scroll - vis;
        if (start < 0) start = 0;

        /* Scrollbar */
        if (nd_log_count > vis) {
            int sb_x = x + w - 10;
            gfx_fill_rect(sb_x, log_y, 8, log_h, card);
            int thumb_h = (log_h * vis) / nd_log_count;
            if (thumb_h < 8) thumb_h = 8;
            int max_s = nd_log_count - vis;
            int ty = log_y + (start * (log_h - thumb_h)) / max_s;
            gfx_fill_rect(sb_x + 1, ty, 6, thumb_h, border);
        }

        for (int i = 0; i < vis && start + i < nd_log_count; i++) {
            int ly = log_y + i * lh;
            int li = start + i;
            const char* s = nd_log_lines[li];

            uint32_t col = dim;
            if (s[0] == '-') col = dim;
            else if (strstr(s, "FAILED") || strstr(s, "REFUSED") || strstr(s, "TIMEOUT") || strstr(s, "Error"))
                col = err_clr;
            else if (strstr(s, "OK") || strstr(s, "SUCCESS") || strstr(s, "OPEN"))
                col = success;
            else if (strstr(s, "---") || strstr(s, "Result") || strstr(s, "Results"))
                col = 0xFFFFFF;
            else if (strstr(s, "Speed") || strstr(s, "Mbps") || strstr(s, "KB/s") || strstr(s, "Very"))
                col = warn;
            else
                col = text;

            /* Add colored dot for status lines */
            if (strstr(s, "OK") && !strstr(s, "HTTP")) {
                char prefix[ND_LINE_LEN + 4];
                snprintf(prefix, ND_LINE_LEN + 4, "  %s", s);
                gfx_draw_string_transparent(cx + 6, ly, prefix, col);
            } else {
                gfx_draw_string_transparent(cx + 6, ly, s, col);
            }
        }

        /* Empty state */
        if (nd_log_count == 0) {
            gfx_draw_string_transparent(cx + 6, log_y + 6,
                "Type a hostname/IP above, select an action, and press Enter", dim);
        }
    }

    /* ---- Footer ---- */
    int foot_y = y + h - 18;
    gfx_fill_rect(x, foot_y, w, 18, card);
    gfx_draw_hline(x, foot_y, w, border);
    gfx_draw_string_transparent(x + 10, foot_y + 1,
        "Enter: run  |  Tab: switch  |  Q: Close", 0x484F58);
}

/* ---- Mouse ---- */
static void nd_mouse(int id, int mx, int my, int mb) {
    (void)id;
    wm_window_t* win = wm_get_window(nd_win_id);
    if (!win) return;

    int w = win->w, h = win->h - WM_TITLEBAR_H;

    int wheel = mouse_get_wheel_delta();
    if (wheel != 0) {
        nd_log_scroll -= wheel * 3;
        if (nd_log_scroll < 0) nd_log_scroll = 0;
        if (nd_log_scroll > nd_log_count) nd_log_scroll = nd_log_count;
        mouse_clear_wheel_delta();
        return;
    }

    int HEADER_H = 28, cw = w - 16, cx = 8;
    int iface_y = HEADER_H + 4, iface_h = 54;
    int action_y = iface_y + iface_h + 4, input_h = 24, btn_h = 24;
    int input_w = cw - 150;

    nd_hover_btn = -1;
    if (my >= action_y && my < action_y + btn_h) {
        const char* labels[] = {"PING", "DNS", "CONN", "SPEED", "SCAN", "INFO"};
        int btn_count = 6;
        int btn_w = (cw - (btn_count - 1) * 4) / btn_count;
        if (btn_w > 90) btn_w = 90;
        int btn_x = cx + cw - (btn_w * btn_count + (btn_count - 1) * 4);
        for (int i = 0; i < btn_count; i++) {
            if (mx >= btn_x && mx < btn_x + btn_w) {
                nd_hover_btn = i;
                if (mb & 1 && !nd_busy) {
                    nd_current_action = i;
                    /* Action triggered on mouse up via key handler or immediately */

                }
                break;
            }
            btn_x += btn_w + 4;
        }
    }

    if (!(mb & 1)) return;

    /* Click on input */
    if (my >= action_y && my < action_y + input_h && mx >= cx && mx < cx + input_w) {
        nd_input_active = 1;
        return;
    }

    /* Click on buttons */
    if (my >= action_y && my < action_y + btn_h) {
        const char* labels[] = {"PING", "DNS", "CONN", "SPEED", "SCAN", "INFO"};
        int btn_count = 6;
        int btn_w = (cw - (btn_count - 1) * 4) / btn_count;
        if (btn_w > 90) btn_w = 90;
        int btn_x = cx + cw - (btn_w * btn_count + (btn_count - 1) * 4);
        for (int i = 0; i < btn_count; i++) {
            if (mx >= btn_x && mx < btn_x + btn_w) {
                if (!nd_busy) {
                    nd_current_action = i;
                    switch (i) {
                        case 0: nd_busy = 1; nd_op_start_ms = timer_get_ms(); nd_action_ping(); nd_busy = 0; break;
                        case 1: nd_busy = 1; nd_op_start_ms = timer_get_ms(); nd_action_dns(); nd_busy = 0; break;
                        case 2: nd_busy = 1; nd_op_start_ms = timer_get_ms(); nd_action_connect(); nd_busy = 0; break;
                        case 3: nd_busy = 1; nd_op_start_ms = timer_get_ms(); nd_action_speed(); nd_busy = 0; break;
                        case 4: nd_busy = 1; nd_op_start_ms = timer_get_ms(); nd_action_portscan(); nd_busy = 0; break;
                        case 5: nd_busy = 1; nd_op_start_ms = timer_get_ms(); nd_action_info(); nd_busy = 0; break;
                    }
                }
                return;
            }
            btn_x += btn_w + 4;
        }
    }
}

/* ---- Keyboard ---- */
static void nd_key(int id, char key) {
    (void)id;
    unsigned char k = (unsigned char)key;

    if (k == 'q' || k == 'Q') {
        wm_close_window(nd_win_id); nd_win_id = -1; return;
    }

    if (k == '\t') {
        if (!nd_busy) nd_current_action = (nd_current_action + 1) % 6;
        return;
    }

    /* Action keys */
    if (k == '1') { if (!nd_busy) { nd_current_action = 0; nd_key(id, '\n'); } return; }
    if (k == '2') { if (!nd_busy) { nd_current_action = 1; nd_key(id, '\n'); } return; }
    if (k == '3') { if (!nd_busy) { nd_current_action = 2; nd_key(id, '\n'); } return; }
    if (k == '4') { if (!nd_busy) { nd_current_action = 3; nd_key(id, '\n'); } return; }
    if (k == '5') { if (!nd_busy) { nd_current_action = 4; nd_key(id, '\n'); } return; }
    if (k == '6') { if (!nd_busy) { nd_current_action = 5; nd_key(id, '\n'); } return; }

    if (k == '\n' && !nd_busy) {
        nd_op_has_progress = 0;
        nd_busy = 1;
        nd_op_start_ms = timer_get_ms();
        switch (nd_current_action) {
            case 0: nd_action_ping(); break;
            case 1: nd_action_dns(); break;
            case 2: nd_action_connect(); break;
            case 3: nd_action_speed(); break;
            case 4: nd_action_portscan(); break;
            case 5: nd_action_info(); break;
        }
        nd_busy = 0;
        return;
    }

    if (KEY_MATCH(k, KEY_UP)) {
        nd_log_scroll -= 10;
        if (nd_log_scroll < 0) nd_log_scroll = 0;
        return;
    }
    if (KEY_MATCH(k, KEY_DOWN)) {
        nd_log_scroll += 10;
        if (nd_log_scroll > nd_log_count) nd_log_scroll = nd_log_count;
        return;
    }
    if (KEY_MATCH(k, KEY_PAGE_UP)) {
        nd_log_scroll -= 40;
        if (nd_log_scroll < 0) nd_log_scroll = 0;
        return;
    }
    if (KEY_MATCH(k, KEY_PAGE_DOWN)) {
        nd_log_scroll += 40;
        if (nd_log_scroll > nd_log_count) nd_log_scroll = nd_log_count;
        return;
    }

    if (k >= 32 && k <= 126 && nd_input_len < ND_INPUT_LEN - 1) {
        nd_input[nd_input_len++] = (char)k;
        nd_input_active = 1;
        return;
    }
    if (KEY_MATCH(k, '\b')) {
        if (nd_input_len > 0) nd_input_len--;
        return;
    }
}

static void nd_resize(int id, int w, int h) { (void)id; (void)w; (void)h; }

void netdebug_app(void) {
    if (wm_get_window(nd_win_id)) { wm_bring_to_front(nd_win_id); return; }

    memset(nd_log_lines, 0, sizeof(nd_log_lines));
    nd_log_count = 0;
    nd_log_scroll = 0;
    nd_input_len = 0;
    nd_input_active = 0;
    nd_current_action = 0;
    nd_busy = 0;
    nd_op_has_progress = 0;
    nd_hover_btn = -1;
    nd_refresh_iface();

    nd_log("Network Debug v3", 0);
    nd_log("Type a hostname/IP, select action (1-6), Enter to run", 0);
    nd_log("Examples: google.com  |  8.8.8.8  |  httpbin.org:80", 0);
    nd_log("Ready.", 0);

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    nd_win_id = wm_open_window(
        (fw - ND_WIN_W) / 2, (fh - ND_WIN_H) / 2,
        ND_WIN_W, ND_WIN_H,
        "Network Debug v3", 0x58A6FF,
        nd_render, nd_key, nd_resize
    );
    wm_set_mouse_handler(nd_win_id, nd_mouse);
}
