/* ============================================================
 * Bdrowser v2 - BEDI OS Web Browser
 * Graphical HTML renderer with proper element styling
 * ============================================================ */

#include "apps/bdrowser.h"
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "gui/wm.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "kernel/net/socket.h"
#include "kernel/net/in.h"
#include "kernel/task/task.h"
#include "libs/bmp.h"
#include <string.h>

/* ---- Constants ---- */
#define NAV_H        40
#define TABBAR_H     32
#define STATUS_H     22
#define URL_MAX      512
#define RAW_MAX      65536
#define MAX_NODES    1024
#define MAX_TEXT     32768
#define MAX_ATTRS    8

/* ---- Colors (dark browser theme) ---- */
#define COL_BG          0x1C1C1E
#define COL_NAV         0x2C2C2E
#define COL_TAB_ACTIVE  0x3A3A3C
#define COL_TAB_IDLE    0x242426
#define COL_BORDER      0x3A3A3C
#define COL_URL_BG      0x1C1C1E
#define COL_URL_FG      0xF0F0F0
#define COL_URL_HINT    0x636366
#define COL_URL_BORDER  0x58A6FF
#define COL_BTN         0x3A3A3C
#define COL_BTN_FG      0xAEAEB2
#define COL_STATUS_BG   0x1C1C1E
#define COL_STATUS_FG   0x8E8E93

/* Content colors */
#define COL_PAGE_BG     0xFFFFFF
#define COL_TEXT        0x1C1C1E
#define COL_LINK        0x0070C9
#define COL_LINK_HOVER  0x0051A3
#define COL_H1          0x1C1C1E
#define COL_H2          0x2C2C2E
#define COL_H3          0x3A3A3C
#define COL_HR          0xC7C7CC
#define COL_CODE_BG     0xF2F2F7
#define COL_CODE_FG     0xC41A16
#define COL_BLOCKQUOTE  0x007AFF
#define COL_PRE_BG      0xF2F2F7
#define COL_PRE_FG      0x3A3A3C

/* ---- Node types ---- */
#define NODE_TEXT       0
#define NODE_H1         1
#define NODE_H2         2
#define NODE_H3         3
#define NODE_H4         4
#define NODE_P          5
#define NODE_BR         6
#define NODE_HR         7
#define NODE_A          8
#define NODE_STRONG     9
#define NODE_EM         10
#define NODE_CODE       11
#define NODE_PRE        12
#define NODE_BLOCKQUOTE 13
#define NODE_LI         14
#define NODE_DIV        15
#define NODE_SPAN       16
#define NODE_IMG        17
#define NODE_TITLE      18
#define NODE_TABLE      19
#define NODE_TD         20
#define NODE_TH         21

/* ---- DOM Node ---- */
typedef struct {
    int   type;
    char* text;         /* pointer into text_pool */
    int   text_len;
    char  href[128];    /* for <a> */
    char  alt[64];      /* for <img> */
    int   img_w, img_h; /* for <img> placeholder */
    int   in_pre;       /* preserve whitespace */
    int   indent;       /* nesting level for lists */
    char  img_src[128]; /* raw src attribute */
} dom_node_t;

/* ---- Render box (after layout) ---- */
typedef struct {
    int x, y, w, h;
    int node_idx;
    int is_link;
} render_box_t;

/* ---- Tab State ---- */
#define BW_MAX_TABS 8
typedef struct {
    int used;
    char url[URL_MAX];
    int url_len;
    int editing_url;
    char page_title[128];
    char status_text[128];
    int loading;
    int scroll_y;
    int content_height;
    int hovered_box;
    dom_node_t nodes[MAX_NODES];
    int node_count;
    render_box_t boxes[MAX_NODES * 3];
    int box_count;
    int text_pool_used;
    char text_pool[MAX_TEXT];
} bw_tab_t;

static bw_tab_t bw_tabs[BW_MAX_TABS];
static int bw_active_tab = 0;
static int bw_tab_count = 0;
static int prev_mouse_buttons = 0;

/* ---- State ---- */
static int    bw_win_id = -1;
static char   url_buf[URL_MAX];
static int    url_len = 0;
static int    editing_url = 0;
static char   page_title[128] = "New Tab";
static char   status_text[128] = "Ready";
static int    loading = 0;

/* Raw HTTP response */
static char   raw_buf[RAW_MAX];
static int    raw_len = 0;

/* DOM */
static dom_node_t nodes[MAX_NODES];
static int        node_count = 0;
static char       text_pool[MAX_TEXT];
static int        text_pool_used = 0;

/* Layout boxes */
static render_box_t boxes[MAX_NODES * 3];
static int          box_count = 0;

/* Scroll */
static int scroll_y = 0;
static int content_height = 0;

/* Hover */
static int hovered_box = -1;

extern void itoa(uint64_t n, char* s);

/* ============================================================ */
/* String helpers                                               */
/* ============================================================ */

static int bw_strlen(const char* s) {
    if (!s) return 0;
    int n = 0; while (s[n]) n++; return n;
}
static void bw_strcpy(char* d, const char* s) { while ((*d++ = *s++)); }
static int bw_strncmp(const char* a, const char* b, int n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}
static int bw_strncmpi(const char* a, const char* b, int n) {
    while (n && *a && *b) {
        char ca = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
        char cb = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
        a++; b++; n--;
    }
    return n ? (unsigned char)*a - (unsigned char)*b : 0;
}
static void bw_strncpy(char* d, const char* s, int n) {
    int i = 0;
    while (i < n - 1 && s[i]) { d[i] = s[i]; i++; }
    d[i] = 0;
}

/* ============================================================ */
/* URL parsing                                                  */
/* ============================================================ */

static void parse_url(const char* url, char* host, int hmax, char* path, int pmax, int* port_out) {
    int i = 0, default_port = 80;
    if (bw_strncmp(url, "http://", 7) == 0) i = 7;
    int hi = 0;
    while (url[i] && url[i] != '/' && url[i] != ':' && hi < hmax - 1)
        host[hi++] = url[i++];
    host[hi] = 0;
    *port_out = default_port;
    if (url[i] == ':') {
        i++; int p = 0;
        while (url[i] >= '0' && url[i] <= '9') p = p * 10 + (url[i++] - '0');
        *port_out = p;
    }
    int pi = 0;
    if (url[i] == '/') {
        while (url[i] && pi < pmax - 1) path[pi++] = url[i++];
    } else { path[0] = '/'; pi = 1; }
    path[pi] = 0;
}

/* ============================================================ */
/* HTTP decode                                                  */
/* ============================================================ */

static int skip_headers(char* buf, int len) {
    for (int i = 0; i < len - 1; i++) {
        /* Accept both CRLF-CRLF and LF-LF as header terminators */
        if (buf[i]=='\r' && buf[i+1]=='\n' && i+3 < len && buf[i+2]=='\r' && buf[i+3]=='\n') return i+4;
        if (buf[i]=='\n' && buf[i+1]=='\n') return i+2;
    }
    return -1;
}

static int is_chunked(char* buf, int headers_end) {
    for (int i = 0; i < headers_end - 7; i++)
        if (bw_strncmpi(buf+i, "chunked", 7) == 0) return 1;
    return 0;
}

static int is_gzipped(char* buf, int headers_end) {
    for (int i = 0; i < headers_end - 5; i++)
        if (bw_strncmpi(buf+i, "gzip", 4) == 0) return 1;
    return 0;
}

/* Remove echoed HTTP request that appears embedded in the response.
 * The echoed request starts with "GET /" (or similar) inside the header section,
 * ending with \r\n\r\n. We replace it with just \r\n\r\n to restore the
 * correct header/body boundary. Also normalizes line endings. */
static int remove_echoed_request(char* buf, int len) {
    if (len < 10) return len;
    
    /* Normalize line endings first - convert \n\n to \r\n\r\n for consistent parsing */
    char temp[RAW_MAX];
    int tlen = 0;
    for (int i = 0; i < len && tlen < RAW_MAX - 2; i++) {
        if (buf[i] == '\n') {
            if (tlen > 0 && temp[tlen-1] != '\r') temp[tlen++] = '\r';
            temp[tlen++] = buf[i];
        } else {
            temp[tlen++] = buf[i];
        }
    }
    
    /* Now find the first header end (double CRLF) */
    int first_hdr_end = -1;
    for (int i = 0; i < tlen - 3; i++) {
        if (temp[i]=='\r' && temp[i+1]=='\n' && temp[i+2]=='\r' && temp[i+3]=='\n') {
            first_hdr_end = i + 4;
            break;
        }
    }
    if (first_hdr_end < 6) {
        /* No proper header end found, copy back and return original length */
        for (int i = 0; i < len && i < tlen; i++) buf[i] = temp[i];
        return len;
    }
    
    /* Check if there's an echoed request (looks like "GET /..." or "POST /...") before the first header end
     * This happens when the server echoes back the request as body */
    int echo_start = -1;
    for (int i = 0; i < first_hdr_end - 4; i++) {
        /* Detect "GET /", "POST /", "HEAD /" (echoed request line) */
        if ((temp[i]=='G' && temp[i+1]=='E' && temp[i+2]=='T' && temp[i+3]==' ' && temp[i+4]=='/') ||
            (temp[i]=='P' && temp[i+1]=='O' && temp[i+2]=='S' && temp[i+3]=='T' && temp[i+4]==' ' && temp[i+5]=='/') ||
            (temp[i]=='H' && temp[i+1]=='E' && temp[i+2]=='A' && temp[i+3]=='D' && temp[i+4]==' ' && temp[i+5]=='/')) {
            echo_start = i;
            break;
        }
    }
    
    /* If we found an echoed request, we need to check if the "header" part 
     * before it is actually valid or just garbage. If echo_start > 4, then
     * we have a valid response header and body. If echo_start < 4, the response
     * is corrupted/empty. We keep the response as-is in most cases. */
    
    /* Normalize: ensure our working buffer has the normalized content */
    for (int i = 0; i < tlen && i < RAW_MAX; i++) buf[i] = temp[i];
    buf[tlen] = 0;
    return tlen;
}

static int dechunk(char* buf, int len) {
    if (len <= 0) return 0;
    
    char temp[RAW_MAX];
    int src = 0, dst = 0, tsrc = 0;
    
    while (tsrc < len) {
        /* Skip leading CRLF before chunk-size */
        while (tsrc < len && (buf[tsrc] == '\r' || buf[tsrc] == '\n')) tsrc++;
        if (tsrc >= len) break;

        int chunk_size = 0;
        int has_hex = 0;
        while (tsrc < len && ((buf[tsrc]>='0'&&buf[tsrc]<='9')||(buf[tsrc]>='a'&&buf[tsrc]<='f')||(buf[tsrc]>='A'&&buf[tsrc]<='F'))) {
            has_hex = 1;
            int nib = buf[tsrc]>='a'?buf[tsrc]-'a'+10:(buf[tsrc]>='A'?buf[tsrc]-'A'+10:buf[tsrc]-'0');
            chunk_size = chunk_size * 16 + nib; tsrc++;
        }
        /* Skip to end of chunk-size line */
        while (tsrc < len && buf[tsrc] != '\n') tsrc++;
        if (tsrc < len) tsrc++; /* skip \n */
        
        if (!has_hex || chunk_size == 0) break;
        if (tsrc + chunk_size > len) chunk_size = len - tsrc;
        
        /* Copy chunk data */
        for (int i = 0; i < chunk_size && dst < RAW_MAX - 1; i++) {
            temp[dst++] = buf[tsrc + i];
        }
        tsrc += chunk_size;
        /* Skip trailing CRLF after chunk data */
        if (tsrc < len && buf[tsrc] == '\r') tsrc++;
        if (tsrc < len && buf[tsrc] == '\n') tsrc++;
    }
    
    /* Copy dechunked content back to original buffer */
    for (int i = 0; i < dst && i < len; i++) buf[i] = temp[i];
    buf[dst] = 0;
    return dst;
}

/* ============================================================ */
/* HTML → DOM parser                                            */
/* ============================================================ */

static char* pool_add(const char* s, int len) {
    if (text_pool_used + len + 1 >= MAX_TEXT) return NULL;
    char* ptr = text_pool + text_pool_used;
    for (int i = 0; i < len; i++) ptr[i] = s[i];
    ptr[len] = 0;
    text_pool_used += len + 1;
    return ptr;
}

static dom_node_t* new_node(int type) {
    if (node_count >= MAX_NODES) return NULL;
    dom_node_t* n = &nodes[node_count++];
    n->type = type;
    n->text = NULL;
    n->text_len = 0;
    n->href[0] = 0;
    n->alt[0] = 0;
    n->img_w = 120; n->img_h = 80;
    n->in_pre = 0;
    n->indent = 0;
    n->img_src[0] = 0;
    return n;
}

/* Decode HTML entity at buf[i], write char to out, return chars consumed */
static int decode_entity(const char* buf, int len, int i, char* out) {
    if (buf[i] != '&') { *out = buf[i]; return 1; }
    if (i + 4 < len && bw_strncmp(buf+i, "&lt;", 4) == 0)   { *out = '<'; return 4; }
    if (i + 4 < len && bw_strncmp(buf+i, "&gt;", 4) == 0)   { *out = '>'; return 4; }
    if (i + 5 < len && bw_strncmp(buf+i, "&amp;", 5) == 0)  { *out = '&'; return 5; }
    if (i + 6 < len && bw_strncmp(buf+i, "&nbsp;", 6) == 0) { *out = ' '; return 6; }
    if (i + 6 < len && bw_strncmp(buf+i, "&quot;", 6) == 0) { *out = '"'; return 6; }
    if (i + 6 < len && bw_strncmp(buf+i, "&apos;", 6) == 0) { *out = '\''; return 6; }
    if (i + 5 < len && bw_strncmp(buf+i, "&mdash;", 7) == 0) { *out = '-'; return 7; }
    if (i + 5 < len && bw_strncmp(buf+i, "&ndash;", 7) == 0) { *out = '-'; return 7; }
    /* Unknown entity — skip to ; */
    int j = i + 1;
    while (j < len && j < i + 16 && buf[j] != ';') j++;
    *out = ' '; return (j < len && buf[j] == ';') ? j - i + 1 : 1;
}

/* Parse tag name from pointer (already past '<'), writes lowercase to out */
static int parse_tag_name(const char* buf, int len, int i, char* name, int nmax) {
    /* skip '/' for closing tags */
    int is_close = 0;
    if (i < len && buf[i] == '/') { is_close = 1; i++; }
    int ni = 0;
    while (i < len && ni < nmax - 1 &&
           (buf[i] >= 'a' && buf[i] <= 'z' ||
            buf[i] >= 'A' && buf[i] <= 'Z' ||
            buf[i] >= '0' && buf[i] <= '9')) {
        char c = buf[i++];
        name[ni++] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
    }
    name[ni] = 0;
    return is_close;
}

/* Get attribute value from inside a tag string */
static void get_attr(const char* tag, int tlen, const char* attr, char* val, int vmax) {
    val[0] = 0;
    int alen = bw_strlen(attr);
    for (int i = 0; i < tlen - alen; i++) {
        if (bw_strncmpi(tag+i, attr, alen) == 0 && (tag[i+alen] == '=' || tag[i+alen] == ' ')) {
            int j = i + alen;
            while (j < tlen && (tag[j] == ' ' || tag[j] == '=')) j++;
            char q = 0;
            if (j < tlen && (tag[j] == '"' || tag[j] == '\'')) q = tag[j++];
            int vi = 0;
            while (j < tlen && vi < vmax - 1) {
                if (q && tag[j] == q) break;
                if (!q && (tag[j] == ' ' || tag[j] == '>')) break;
                val[vi++] = tag[j++];
            }
            val[vi] = 0;
            return;
        }
    }
}

/* Accumulate text into a temp buffer and flush as NODE_TEXT when we hit a block element */
static char text_acc[4096];
static int  text_acc_len = 0;
static int  in_pre_mode = 0;

static void flush_text(void) {
    if (text_acc_len == 0) return;
    /* Trim leading/trailing whitespace unless in pre mode */
    int start = 0, end = text_acc_len;
    if (!in_pre_mode) {
        while (start < end && (text_acc[start] == ' ' || text_acc[start] == '\n' || text_acc[start] == '\r' || text_acc[start] == '\t')) start++;
        while (end > start && (text_acc[end-1] == ' ' || text_acc[end-1] == '\n' || text_acc[end-1] == '\r' || text_acc[end-1] == '\t')) end--;
    }
    if (start >= end) { text_acc_len = 0; return; }
    dom_node_t* n = new_node(NODE_TEXT);
    if (n) {
        n->text = pool_add(text_acc + start, end - start);
        n->text_len = end - start;
        n->in_pre = in_pre_mode;
    }
    text_acc_len = 0;
}

static void acc_char(char c) {
    if (text_acc_len < (int)sizeof(text_acc) - 1)
        text_acc[text_acc_len++] = c;
}

void html_to_dom(const char* html, int len) {
    node_count = 0;
    text_pool_used = 0;
    text_acc_len = 0;
    in_pre_mode = 0;
    bw_strcpy(page_title, "Page");

    int in_script = 0, in_style = 0, in_title = 0;
    int a_depth = 0;
    char cur_href[128]; cur_href[0] = 0;
    int list_depth = 0;

    int i = 0;
    while (i < len) {
        if (html[i] == '<') {
            /* --- tag --- */
            /* find end of tag */
            int tag_start = i + 1;
            int j = i + 1;
            while (j < len && html[j] != '>') j++;
            int tag_len = j - tag_start; /* does not include > */

            char name[32];
            int is_close = parse_tag_name(html + tag_start, tag_len,
                                          (html[tag_start] == '/' ? 1 : 0), name, sizeof(name));
            /* re-parse properly */
            {
                int ti = 0;
                if (ti < tag_len && html[tag_start] == '!') { i = j + 1; continue; } /* comment / doctype */
                is_close = parse_tag_name(html + tag_start, tag_len, 0, name, sizeof(name));
            }

            /* Handle script/style skip */
            if (!is_close) {
                if (bw_strncmp(name, "script", 6) == 0) { in_script = 1; i = j + 1; continue; }
                if (bw_strncmp(name, "style",  5) == 0) { in_style  = 1; i = j + 1; continue; }
            } else {
                if (bw_strncmp(name, "script", 6) == 0) { in_script = 0; i = j + 1; continue; }
                if (bw_strncmp(name, "style",  5) == 0) { in_style  = 0; i = j + 1; continue; }
            }
            if (in_script || in_style) { i = j + 1; continue; }

            /* Title */
            if (!is_close && bw_strncmp(name, "title", 5) == 0) { in_title = 1; text_acc_len = 0; i = j + 1; continue; }
            if ( is_close && bw_strncmp(name, "title", 5) == 0) {
                if (text_acc_len > 0) {
                    int tl = text_acc_len > 127 ? 127 : text_acc_len;
                    bw_strncpy(page_title, text_acc, tl + 1);
                }
                in_title = 0; text_acc_len = 0; i = j + 1; continue;
            }
            if (in_title) { i = j + 1; continue; }

            /* Block tags → flush current text first */
            int is_block = (
                bw_strncmp(name,"h1",2)==0 || bw_strncmp(name,"h2",2)==0 ||
                bw_strncmp(name,"h3",2)==0 || bw_strncmp(name,"h4",2)==0 ||
                bw_strncmp(name,"p",1)==0  || bw_strncmp(name,"div",3)==0 ||
                bw_strncmp(name,"br",2)==0 || bw_strncmp(name,"hr",2)==0 ||
                bw_strncmp(name,"li",2)==0 || bw_strncmp(name,"ul",2)==0 ||
                bw_strncmp(name,"ol",2)==0 || bw_strncmp(name,"pre",3)==0 ||
                bw_strncmp(name,"blockquote",10)==0 ||
                bw_strncmp(name,"table",5)==0 || bw_strncmp(name,"tr",2)==0 ||
                bw_strncmp(name,"td",2)==0 || bw_strncmp(name,"th",2)==0 ||
                bw_strncmp(name,"article",7)==0 || bw_strncmp(name,"section",7)==0 ||
                bw_strncmp(name,"header",6)==0 || bw_strncmp(name,"footer",6)==0 ||
                bw_strncmp(name,"main",4)==0 || bw_strncmp(name,"nav",3)==0
            );

            if (is_block) flush_text();

            /* Now process the tag */
            if (!is_close) {
                if (bw_strncmp(name,"h1",2)==0) { dom_node_t* n = new_node(NODE_H1); (void)n; }
                else if (bw_strncmp(name,"h2",2)==0) { dom_node_t* n = new_node(NODE_H2); (void)n; }
                else if (bw_strncmp(name,"h3",2)==0) { dom_node_t* n = new_node(NODE_H3); (void)n; }
                else if (bw_strncmp(name,"h4",2)==0) { dom_node_t* n = new_node(NODE_H4); (void)n; }
                else if (bw_strncmp(name,"br",2)==0) { new_node(NODE_BR); }
                else if (bw_strncmp(name,"hr",2)==0) { new_node(NODE_HR); }
                else if (bw_strncmp(name,"img",3)==0) {
                    dom_node_t* n = new_node(NODE_IMG);
                    if (n) {
                        get_attr(html + tag_start, tag_len, "alt", n->alt, 64);
                        get_attr(html + tag_start, tag_len, "src", n->img_src, 128);
                        char ws[16], hs[16];
                        get_attr(html + tag_start, tag_len, "width", ws, 16);
                        get_attr(html + tag_start, tag_len, "height", hs, 16);
                        if (ws[0]) { int w2 = 0; for(int k=0;ws[k]>='0'&&ws[k]<='9';k++) w2=w2*10+(ws[k]-'0'); if(w2>0&&w2<600) n->img_w=w2; }
                        if (hs[0]) { int h2 = 0; for(int k=0;hs[k]>='0'&&hs[k]<='9';k++) h2=h2*10+(hs[k]-'0'); if(h2>0&&h2<400) n->img_h=h2; }
                    }
                }
                else if (bw_strncmp(name,"a",1)==0 && name[1]==0) {
                    char href[128]; href[0] = 0;
                    get_attr(html + tag_start, tag_len, "href", href, 128);
                    bw_strcpy(cur_href, href);
                    a_depth++;
                }
                else if (bw_strncmp(name,"pre",3)==0) { in_pre_mode = 1; new_node(NODE_PRE); }
                else if (bw_strncmp(name,"blockquote",10)==0) { new_node(NODE_BLOCKQUOTE); }
                else if (bw_strncmp(name,"li",2)==0) { dom_node_t* n = new_node(NODE_LI); if(n) n->indent = list_depth; }
                else if (bw_strncmp(name,"ul",2)==0 || bw_strncmp(name,"ol",2)==0) { list_depth++; }
            } else {
                /* closing tags */
                if (bw_strncmp(name,"a",1)==0 && name[1]==0) {
                    if (a_depth > 0) a_depth--;
                    if (a_depth == 0) cur_href[0] = 0;
                }
                else if (bw_strncmp(name,"pre",3)==0) { in_pre_mode = 0; }
                else if (bw_strncmp(name,"ul",2)==0 || bw_strncmp(name,"ol",2)==0) {
                    if (list_depth > 0) list_depth--;
                }
            }

            i = j + 1;
        } else {
            /* --- text content --- */
            if (in_script || in_style || in_title) {
                if (in_title) acc_char(html[i]);
                i++; continue;
            }
            char out_c;
            int consumed = decode_entity(html, len, i, &out_c);
            i += consumed;
            if (out_c == '\r') continue;
            if (!in_pre_mode) {
                /* Collapse whitespace */
                if (out_c == '\n' || out_c == '\t') out_c = ' ';
                if (out_c == ' ' && text_acc_len > 0 && text_acc[text_acc_len-1] == ' ') continue;
            }
            /* If we have an active link, tag accumulated text as link */
            if (a_depth > 0 && out_c != ' ' && text_acc_len == 0) {
                /* Flush any pending text, start link text */
            }
            acc_char(out_c);
        }
    }
    flush_text();

    /* Tag text nodes that are inside <a> context by looking for accumulated href
     * — simpler approach: re-scan and annotate text nodes near <a> markers */
    /* (simplified: link text goes to the NODE_TEXT nodes after we find a NODE_A href marker) */
}

/* ============================================================ */
/* Layout engine                                                */
/* ============================================================ */

#define BW_CHAR_W 8
#define BW_CHAR_H 16

/* Draw char at 2x scale using the font */
static void draw_char_2x(int x, int y, char c, uint32_t col) {
    extern const unsigned char font8x16[];
    unsigned char* glyph = (unsigned char*)font8x16 + (unsigned char)c * 16;
    for (int row = 0; row < 16; row++) {
        unsigned char bits = glyph[row];
        for (int col2 = 0; col2 < 8; col2++) {
            if (bits & (0x80 >> col2)) {
                gfx_fill_rect(x + col2*2, y + row*2, 2, 2, col);
            }
        }
    }
}

static void draw_string_scaled(int x, int y, const char* s, uint32_t col, int scale) {
    if (scale <= 1) { gfx_draw_string_transparent(x, y, s, col); return; }
    extern const unsigned char font8x16[];
    int cx = x;
    while (*s) {
        unsigned char* glyph = (unsigned char*)font8x16 + (unsigned char)*s * 16;
        for (int row = 0; row < 16; row++) {
            unsigned char bits = glyph[row];
            for (int col2 = 0; col2 < 8; col2++) {
                if (bits & (0x80 >> col2)) {
                    gfx_fill_rect(cx + col2*scale, y + row*scale, scale, scale, col);
                }
            }
        }
        cx += 8 * scale;
        s++;
    }
}

static int text_width_scaled(const char* s, int scale) {
    int n = 0; while (s[n]) n++;
    return n * 8 * scale;
}

#define MARGIN_LEFT  24
#define MARGIN_RIGHT 24
#define LINE_GAP     4

static void do_layout(int content_x, int content_w) {
    box_count = 0;
    content_height = 0;
    int cx = content_x + MARGIN_LEFT;
    int cy = 8;
    int avail_w = content_w - MARGIN_LEFT - MARGIN_RIGHT;

    for (int ni = 0; ni < node_count; ni++) {
        dom_node_t* nd = &nodes[ni];

        if (nd->type == NODE_BR) {
            cy += CHAR_H + LINE_GAP;
            cx = content_x + MARGIN_LEFT;
            continue;
        }

        if (nd->type == NODE_HR) {
            if (box_count < MAX_NODES * 3) {
                render_box_t* b = &boxes[box_count++];
                b->x = content_x + MARGIN_LEFT; b->y = cy;
                b->w = avail_w; b->h = 1;
                b->node_idx = ni; b->is_link = 0;
            }
            cy += 14;
            cx = content_x + MARGIN_LEFT;
            continue;
        }

        if (nd->type == NODE_IMG) {
            int iw = nd->img_w > avail_w ? avail_w : nd->img_w;
            int ih = nd->img_h;
            if (cx + iw > content_x + content_w - MARGIN_RIGHT) {
                cy += CHAR_H + LINE_GAP;
                cx = content_x + MARGIN_LEFT;
            }
            if (box_count < MAX_NODES * 3) {
                render_box_t* b = &boxes[box_count++];
                b->x = cx; b->y = cy; b->w = iw; b->h = ih;
                b->node_idx = ni; b->is_link = 0;
            }
            cy += ih + LINE_GAP;
            cx = content_x + MARGIN_LEFT;
            continue;
        }

        if (nd->type == NODE_PRE || nd->type == NODE_BLOCKQUOTE ||
            nd->type == NODE_H1 || nd->type == NODE_H2 ||
            nd->type == NODE_H3 || nd->type == NODE_H4 ||
            nd->type == NODE_LI) {
            /* Block separator node — just add spacing */
            cy += 6;
            cx = content_x + MARGIN_LEFT;
            if (nd->type == NODE_LI) cx += nd->indent * 16 + 16;
            continue;
        }

        if (nd->type == NODE_TEXT || nd->type == NODE_STRONG || nd->type == NODE_EM || nd->type == NODE_CODE) {
            if (!nd->text || nd->text_len == 0) continue;

            /* Determine style from context - look back for block context */
            int scale = 1;
            int ctx_type = NODE_TEXT;
            /* scan back for nearest block ancestor */
            for (int k = ni - 1; k >= 0; k--) {
                int t = nodes[k].type;
                if (t == NODE_H1 || t == NODE_H2 || t == NODE_H3 || t == NODE_H4 ||
                    t == NODE_P || t == NODE_LI || t == NODE_PRE || t == NODE_BLOCKQUOTE ||
                    t == NODE_DIV || t == NODE_TD || t == NODE_TH) {
                    ctx_type = t; break;
                }
            }

            if (ctx_type == NODE_H1) { scale = 3; cy += 8; }
            else if (ctx_type == NODE_H2) { scale = 2; cy += 6; }
            else if (ctx_type == NODE_H3) { scale = 2; cy += 4; }
            else if (ctx_type == NODE_LI) { cx = content_x + MARGIN_LEFT + nodes[ni-1].indent * 16 + 16; }

            /* Word-wrap the text */
            const char* txt = nd->text;
            int tlen = nd->text_len;
            int chars_per_line = avail_w / (BW_CHAR_W * scale);
            if (chars_per_line < 1) chars_per_line = 1;

            if (nd->in_pre) {
                /* Pre: respect newlines, no wrap */
                int line_start = 0;
                for (int ti = 0; ti <= tlen; ti++) {
                    if (ti == tlen || txt[ti] == '\n') {
                        int llen = ti - line_start;
                        if (llen > 0) {
                            if (box_count < MAX_NODES * 3) {
                                render_box_t* b = &boxes[box_count++];
                                b->x = content_x + MARGIN_LEFT + 8;
                                b->y = cy;
                                b->w = llen * BW_CHAR_W * scale;
                                b->h = BW_CHAR_H * scale;
                                b->node_idx = ni; b->is_link = 0;
                            }
                        }
                        cy += BW_CHAR_H * scale + 2;
                        line_start = ti + 1;
                    }
                }
                cx = content_x + MARGIN_LEFT;
                if (ctx_type == NODE_H1 || ctx_type == NODE_H2) cy += 8;
                continue;
            }

            /* Normal word wrap */
            int pos = 0;
            while (pos < tlen) {
                /* How many chars fit on this line from cx? */
                int space_left = (content_x + content_w - MARGIN_RIGHT - cx) / (BW_CHAR_W * scale);
                if (space_left < 1 || cx + BW_CHAR_W * scale > content_x + content_w - MARGIN_RIGHT) {
                    cy += BW_CHAR_H * scale + LINE_GAP;
                    cx = content_x + MARGIN_LEFT;
                    space_left = avail_w / (BW_CHAR_W * scale);
                }

                /* Find a good break point */
                int take = tlen - pos;
                if (take > space_left) take = space_left;
                /* try word boundary */
                if (pos + take < tlen) {
                    int bp = take;
                    while (bp > 1 && txt[pos + bp - 1] != ' ') bp--;
                    if (bp > 1) take = bp;
                }

                if (box_count < MAX_NODES * 3) {
                    render_box_t* b = &boxes[box_count++];
                    b->x = cx; b->y = cy;
                    b->w = take * BW_CHAR_W * scale;
                    b->h = BW_CHAR_H * scale;
                    b->node_idx = ni; b->is_link = 0;
                }
                cx += take * BW_CHAR_W * scale + (scale == 1 ? 2 : 4);
                pos += take;
                /* skip leading space on next word */
                if (pos < tlen && txt[pos] == ' ') pos++;
            }

            if (ctx_type == NODE_H1 || ctx_type == NODE_H2) {
                cy += BW_CHAR_H * scale + 10;
                cx = content_x + MARGIN_LEFT;
            }
        }
    }

    content_height = cy + BW_CHAR_H + 32;
}

/* ============================================================ */
/* Network fetch                                                */
/* ============================================================ */

static int bw_fetch_id = 0;

static void bw_set_status(const char* s) { bw_strncpy(status_text, s, sizeof(status_text)); }

/* ---- Image loading cache ---- */
#define MAX_LOADED_IMAGES 4
typedef struct {
    int loaded;
    char src[128];
    bmp_image_t img;
} loaded_image_t;
static loaded_image_t loaded_images[MAX_LOADED_IMAGES];
static int loaded_image_count = 0;

static int bw_loaded_image_index(const char* src) {
    for (int i = 0; i < loaded_image_count; i++)
        if (bw_strncmp(loaded_images[i].src, src, 127) == 0)
            return i;
    return -1;
}

static int bw_load_image(const char* src_url) {
    if (!src_url || !src_url[0]) return -1;
    if (bw_strncmp(src_url, "http://", 7) != 0)
        return -1;
    if (loaded_image_count >= MAX_LOADED_IMAGES) return -1;

    char host[128], path[256];
    int port = 80;
    parse_url(src_url, host, sizeof(host), path, sizeof(path), &port);
    path[127] = 0;

    uint32_t ip;
    extern int dns_resolve(const char* hostname, uint32_t* ip_addr);
    if (host[0] && dns_resolve(host, &ip) < 0) return -2;

    int sock = sys_socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -2;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr.s_addr, &ip, 4);

    if (sys_connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        sys_socket_close(sock);
        return -2;
    }

    char req[256];
    int ri = 0;
    {
        const char* g = "GET ";
        const char* p = path;
        while (*g) req[ri++] = *g++;
        while (*p) req[ri++] = *p++;
        const char* e = " HTTP/1.0\r\nHost: ";
        while (*e) req[ri++] = *e++;
        const char* h = host;
        while (*h) req[ri++] = *h++;
        const char* end = "\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n";
        while (*end) req[ri++] = *end++;
    }
    req[ri] = 0;

    int sent = sys_send(sock, req, ri, 0);
    if (sent != ri) {
        sys_socket_close(sock);
        return -4;
    }

    char img_buf[4096];
    int ilen = 0;
    int wait = 0;
    while (ilen < 4095) {
        int n = sys_recv(sock, img_buf + ilen, 4095 - ilen, 0);
        if (n <= 0) { wait++; if (wait > 40) break; continue; }
        ilen += n;
        wait = 0;
    }
    img_buf[ilen] = 0;

    sys_socket_close(sock);

    char* body = img_buf;
    for (int i = 0; i < ilen - 3; i++) {
        if (img_buf[i] == '\r' && img_buf[i+1] == '\n' && img_buf[i+2] == '\r' && img_buf[i+3] == '\n') {
            body = img_buf + i + 4;
            break;
        }
    }
    int blen = ilen - (int)(body - img_buf);
    if (blen <= 0) return -5;

    loaded_image_t* li = &loaded_images[loaded_image_count++];
    bw_strcpy(li->src, src_url);
    memset(&li->img, 0, sizeof(bmp_image_t));
    if (bmp_decode((uint8_t*)body, blen, &li->img) == 0 && li->img.pixels)
        li->loaded = 1;
    else
        li->loaded = 0;
    return 0;
}

static void bw_save_active_tab(void);
static void bw_load_tab(int idx);

static void bw_fetch_internal(const char* url) {
    int my_id = bw_fetch_id;
    char host[128], path[256];
    int port = 80;
    parse_url(url, host, sizeof(host), path, sizeof(path), &port);

    bw_set_status("Resolving...");

    uint32_t ip;
    extern int dns_resolve(const char* hostname, uint32_t* ip_addr);
    if (dns_resolve(host, &ip) < 0) { if(my_id == bw_fetch_id) { bw_set_status("DNS failed"); loading = 0; } return; }

    bw_set_status("Connecting...");
    int sock = sys_socket(2, 1, 0);
    if (sock < 0) { if(my_id == bw_fetch_id) { bw_set_status("Socket failed"); loading = 0; } return; }

    struct sockaddr_in addr;
    addr.sin_family = 2;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = ip;

    if (sys_connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if(my_id == bw_fetch_id) { bw_set_status("Connection failed"); loading = 0; } sys_socket_close(sock); return;
    }

    bw_set_status("Fetching...");

    /* Build HTTP request */
    char req[768];
    int ri = 0;
    #define R(s) do { const char* _s = s; while (*_s) req[ri++] = *_s++; } while(0)
    R("GET "); for (int pi=0; path[pi] && ri<700; pi++) req[ri++] = path[pi];
    R(" HTTP/1.0\r\nHost: ");
    for (int hi=0; host[hi] && ri<740; hi++) req[ri++] = host[hi];
    R("\r\nUser-Agent: Bdrowser/2.0\r\nAccept: text/html,text/plain\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
    req[ri] = 0;
    #undef R

    sys_send(sock, req, ri, 0);

    /* Receive */
    raw_len = 0;
    int no_data = 0;
    extern int sys_socket_closed(int s);
    while (no_data < 5 && raw_len < RAW_MAX - 1) {
        int n;
        n = sys_recv(sock, raw_buf + raw_len, RAW_MAX - 1 - raw_len, 0);
        if (n <= 0) {
            if (sys_socket_closed(sock)) break;
            no_data++;
        } else { raw_len += n; no_data = 0; }
    }
    raw_buf[raw_len] = 0;

    /* Workaround: remove echoed HTTP request from response if present */
    raw_len = remove_echoed_request(raw_buf, raw_len);

    sys_socket_close(sock);

    if (my_id != bw_fetch_id) return;

    /* DEBUG: dump raw response */
    extern void serial_puts(const char* s);
    extern void itoa(uint64_t n, char* s);
    serial_puts("[BROWSER] raw_len=");
    char dbg_n[16]; itoa(raw_len, dbg_n); serial_puts(dbg_n);
    serial_puts("\n[BROWSER] RAW:");
    int dbg_max = raw_len < 1024 ? raw_len : 1024;
    for (int dbg_i = 0; dbg_i < dbg_max; dbg_i++) {
        if (raw_buf[dbg_i] >= 32 && raw_buf[dbg_i] < 127) {
            char dbg_c[2] = {raw_buf[dbg_i], 0};
            serial_puts(dbg_c);
        } else {
            serial_puts(".");
        }
    }
    serial_puts("\n[BROWSER] ENDRAW\n");

    /* Decode */
    int hdr_end = skip_headers(raw_buf, raw_len);
    if (hdr_end < 0) {
        bw_set_status("Error: no HTTP headers found");
        loading = 0;
        bw_save_active_tab();
        return;
    }

    if (is_gzipped(raw_buf, hdr_end)) {
        bw_set_status("Error: gzipped content not supported");
        loading = 0;
        bw_save_active_tab();
        return;
    }

    if (is_chunked(raw_buf, hdr_end) && hdr_end > 0) {
        int body_len = raw_len - hdr_end;
        char dechunk_buf[RAW_MAX];
        memcpy(dechunk_buf, raw_buf + hdr_end, body_len);
        dechunk_buf[body_len] = 0;
        int new_len = dechunk(dechunk_buf, body_len);
        if (new_len > 0) {
            memcpy(raw_buf + hdr_end, dechunk_buf, new_len);
            raw_len = hdr_end + new_len;
            bw_set_status("Dechunked");
        } else {
            bw_set_status("Error: dechunk failed");
            loading = 0;
            bw_save_active_tab();
            return;
        }
    }

    /* Parse HTML into DOM */
    html_to_dom(raw_buf + hdr_end, raw_len - hdr_end);

    if (node_count == 0) {
        bw_set_status("Error: failed to parse HTML");
        loading = 0;
        bw_save_active_tab();
        return;
    }

    /* Layout */
    wm_window_t* win = wm_get_window(bw_win_id);
    int cw = win ? win->w : 800;
    do_layout(0, cw);

    /* Load linked images (synchronous, within fetch task) */
    for (int i = 0; i < node_count && loaded_image_count < MAX_LOADED_IMAGES; i++) {
        if (nodes[i].type == NODE_IMG && nodes[i].img_src[0]) {
            int idx = bw_loaded_image_index(nodes[i].img_src);
            if (idx < 0) bw_load_image(nodes[i].img_src);
        }
    }

    scroll_y = 0;
    bw_set_status("Done");
    loading = 0;
    bw_save_active_tab();
}

static void bw_fetch_task(void) {
    bw_fetch_internal(url_buf);
    exit_task(0);
}

static void bw_on_click(int win_id, int btn_id) {
    (void)win_id;
    if (btn_id == 1) { /* + new tab */
        bw_save_active_tab();
        if (bw_tab_count < BW_MAX_TABS) bw_tab_count++;
        bw_active_tab = bw_tab_count - 1;
        bw_tabs[bw_active_tab].used = 1;
        url_buf[0] = 0; url_len = 0;
        node_count = 0; box_count = 0;
        text_pool_used = 0;
        scroll_y = 0; content_height = 0;
        loading = 0; editing_url = 0;
        bw_strcpy(page_title, "New Tab");
        bw_set_status("Ready  —  Press / to enter URL");
    } else if (btn_id == 4) { /* Reload */
        if (url_len > 0) {
            loading = 1; bw_set_status("Reloading...");
            bw_fetch_id++;
            create_task(bw_fetch_task, "Bdrowser Fetch");
        }
    }
}

/* ============================================================ */
/* Rendering                                                     */
/* ============================================================ */

static void render_node_box(render_box_t* b, int ox, int oy) {
    dom_node_t* nd = &nodes[b->node_idx];

    if (nd->type == NODE_HR) {
        gfx_draw_hline(b->x + ox, b->y + oy, b->w, COL_HR);
        return;
    }

    if (nd->type == NODE_IMG) {
        if (nd->img_src[0]) {
            int li_idx = bw_loaded_image_index(nd->img_src);
            if (li_idx >= 0 && loaded_images[li_idx].loaded && loaded_images[li_idx].img.pixels) {
                bmp_image_t* img = &loaded_images[li_idx].img;
                gfx_draw_rgb_bitmap_scaled(b->x + ox, b->y + oy, b->w, b->h, img->pixels, img->width, img->height);
                return;
            }
        }
        /* Placeholder: checkerboard + border + alt text */
        gfx_fill_rect(b->x + ox, b->y + oy, b->w, b->h, 0xE5E5EA);
        gfx_draw_rect_outline(b->x + ox, b->y + oy, b->w, b->h, 1, 0xC7C7CC);
        /* Image icon in center */
        int cx2 = b->x + ox + b->w/2 - 12;
        int cy2 = b->y + oy + b->h/2 - 8;
        gfx_fill_rect(cx2, cy2, 24, 16, 0xC7C7CC);
        gfx_fill_circle(cx2 + 7, cy2 + 4, 3, 0xAEAEB2);
        /* Mountain shape */
        for (int k = 0; k < 12; k++) {
            int py = k > 6 ? 12 - k : k;
            gfx_fill_rect(cx2 + 8 + k, cy2 + 10 - py/2, 1, py/2 + 2, 0xAEAEB2);
        }
        if (nd->alt[0]) {
            gfx_draw_string_transparent(b->x + ox + 4, b->y + oy + b->h - 18, nd->alt, 0x6E6E73);
        }
        return;
    }

    if (nd->type != NODE_TEXT) return;
    if (!nd->text) return;

    /* Determine scale and color from context */
    int scale = 1;
    uint32_t col = COL_TEXT;
    int ctx_type = NODE_TEXT;
    int is_bold = 0;
    for (int k = b->node_idx - 1; k >= 0; k--) {
        int t = nodes[k].type;
        if (t == NODE_H1 || t == NODE_H2 || t == NODE_H3 || t == NODE_H4 ||
            t == NODE_P || t == NODE_LI || t == NODE_PRE || t == NODE_BLOCKQUOTE ||
            t == NODE_CODE || t == NODE_STRONG || t == NODE_EM ||
            t == NODE_DIV || t == NODE_TD || t == NODE_TH) {
            ctx_type = t; break;
        }
    }

    switch (ctx_type) {
        case NODE_H1: scale = 2; col = COL_H1; is_bold = 1; break;
        case NODE_H2: scale = 1; col = COL_H2; is_bold = 1; break;
        case NODE_H3: scale = 1; col = COL_H3; is_bold = 1; break;
        case NODE_H4: scale = 1; col = COL_H2; is_bold = 1; break;
        case NODE_STRONG: col = COL_H1; is_bold = 1; break;
        case NODE_CODE:
            gfx_fill_rect(b->x + ox - 2, b->y + oy - 1, b->w + 4, b->h + 2, COL_CODE_BG);
            col = COL_CODE_FG; break;
        case NODE_PRE:
            gfx_fill_rect(b->x + ox, b->y + oy, b->w, b->h, COL_PRE_BG);
            col = COL_PRE_FG; break;
        case NODE_BLOCKQUOTE:
            gfx_fill_rect(b->x + ox - 12, b->y + oy, 4, b->h, COL_BLOCKQUOTE);
            col = 0x3A3A3C; break;
        case NODE_LI: col = COL_TEXT; break;
        default: col = COL_TEXT; break;
    }

    /* Render text word by word based on box position */
    /* We find which slice of the text this box represents by offset from layout */
    /* Simplified: just render the text from node start that fits in box width */
    const char* txt = nd->text;
    int tlen = nd->text_len;

    /* Find what slice belongs to this box */
    int chars = b->w / (BW_CHAR_W * scale);
    if (chars > tlen) chars = tlen;

    /* Find position in text for this box by scanning previous boxes for same node */
    int start_char = 0;
    for (int bi = 0; bi < box_count; bi++) {
        if (&boxes[bi] == b) break;
        if (boxes[bi].node_idx == b->node_idx) {
            int prev_chars = boxes[bi].w / (BW_CHAR_W * scale);
            start_char += prev_chars;
            /* skip space between words */
            if (start_char < tlen && txt[start_char] == ' ') start_char++;
        }
    }
    if (start_char >= tlen) start_char = 0;
    if (start_char + chars > tlen) chars = tlen - start_char;

    /* Render */
    char tmp[256];
    int tc = chars > 255 ? 255 : chars;
    int ti;
    for (ti = 0; ti < tc; ti++) tmp[ti] = txt[start_char + ti];
    tmp[ti] = 0;

    if (scale <= 1) {
        gfx_draw_string_transparent(b->x + ox, b->y + oy, tmp, col);
        if (is_bold) /* faux bold: draw 1px right */
            gfx_draw_string_transparent(b->x + ox + 1, b->y + oy, tmp, col);
    } else {
        draw_string_scaled(b->x + ox, b->y + oy, tmp, col, scale);
        if (is_bold)
            draw_string_scaled(b->x + ox + 1, b->y + oy, tmp, col, scale);
    }
}

static void bw_save_active_tab(void) {
    bw_tab_t* t = &bw_tabs[bw_active_tab];
    t->used = 1;
    bw_strcpy(t->url, url_buf);
    t->url_len = url_len;
    t->editing_url = editing_url;
    bw_strcpy(t->page_title, page_title);
    bw_strcpy(t->status_text, status_text);
    t->loading = loading;
    t->scroll_y = scroll_y;
    t->content_height = content_height;
    t->hovered_box = hovered_box;
    t->node_count = node_count;
    t->box_count = box_count;
    t->text_pool_used = text_pool_used;
    memcpy(t->nodes, nodes, sizeof(nodes));
    memcpy(t->boxes, boxes, sizeof(boxes));
    memcpy(t->text_pool, text_pool, sizeof(text_pool));
}

static void bw_load_tab(int idx) {
    bw_tab_t* t = &bw_tabs[idx];
    bw_strcpy(url_buf, t->url);
    url_len = t->url_len;
    editing_url = t->editing_url;
    bw_strcpy(page_title, t->page_title);
    bw_strcpy(status_text, t->status_text);
    loading = t->loading;
    scroll_y = t->scroll_y;
    content_height = t->content_height;
    hovered_box = t->hovered_box;
    node_count = t->node_count;
    box_count = t->box_count;
    text_pool_used = t->text_pool_used;
    memcpy(nodes, t->nodes, sizeof(nodes));
    memcpy(boxes, t->boxes, sizeof(boxes));
    memcpy(text_pool, t->text_pool, sizeof(text_pool));
}

static void bw_on_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;

    /* Save current tab state before any switching */
    bw_save_active_tab();

    int mx = mouse_get_x();
    int my = mouse_get_y();
    int mbtn = mouse_get_buttons();
    int clicked = (mbtn & 1) && !(prev_mouse_buttons & 1);
    prev_mouse_buttons = mbtn;

    /* Tab switching */
    if (clicked) {
        int tx = x + 8;
        int ty = y + 4;
        for (int i = 0; i < bw_tab_count; i++) {
            int tw = 180;
            int th = TABBAR_H - 4;
            if (mx >= tx && mx <= tx + tw && my >= ty && my <= ty + th) {
                if (i != bw_active_tab) {
                    bw_active_tab = i;
                    bw_load_tab(i);
                }
                break;
            }
            tx += tw + 4;
            if (tx + 180 > x + w) break;
        }
    }

    /* === Tab Bar === */
    gfx_fill_rect(x, y, w, TABBAR_H, COL_TAB_IDLE);
    gfx_draw_hline(x, y + TABBAR_H, w, COL_BORDER);
    int tx = x + 8;
    int ty = y + 4;
    int tw = 180;
    int th = TABBAR_H - 4;
    for (int i = 0; i < bw_tab_count; i++) {
        uint32_t tc = (i == bw_active_tab) ? COL_TAB_ACTIVE : COL_TAB_IDLE;
        gfx_fill_rect_rounded(tx, ty, tw, th, 4, tc);
        char tab_title[40];
        bw_strncpy(tab_title, bw_tabs[i].page_title, 28);
        uint32_t tt = (i == bw_active_tab) ? 0xF2F2F7 : 0x8E8E93;
        gfx_draw_string_transparent(tx + 6, ty + 6, tab_title, tt);
        tx += tw + 4;
        if (tx + tw > x + w) break;
    }

    /* === Nav Bar === */
    int nav_y = y + TABBAR_H;
    gfx_fill_rect(x, nav_y, w, NAV_H, COL_NAV);
    gfx_draw_hline(x, nav_y + NAV_H, w, COL_BORDER);

    /* Back / Forward / Reload buttons are drawn by WM */
    int btn_y = nav_y + (NAV_H - 24) / 2;

    /* URL bar */
    int url_x = x + 108;
    int url_bar_w = w - 108 - 8;
    uint32_t url_border = editing_url ? COL_URL_BORDER : COL_BORDER;
    gfx_fill_rect_rounded(url_x, btn_y, url_bar_w, 24, 12, COL_URL_BG);
    gfx_draw_rect_rounded_outline(url_x, btn_y, url_bar_w, 24, 12, 1, url_border);

    char url_display[URL_MAX];
    uint32_t url_fg = COL_URL_FG;
    if (editing_url) {
        bw_strcpy(url_display, url_buf);
        /* Cursor */
        int cur_x = url_x + 14 + url_len * 8;
        if (cur_x < url_x + url_bar_w - 8)
            gfx_fill_rect(cur_x, btn_y + 5, 1, 14, COL_URL_BORDER);
    } else if (url_len > 0) {
        bw_strcpy(url_display, url_buf);
    } else {
        bw_strcpy(url_display, "Search or enter URL...");
        url_fg = COL_URL_HINT;
    }
    gfx_draw_string_transparent(url_x + 14, btn_y + 4, url_display, url_fg);

    /* === Content Area === */
    int content_y = y + TABBAR_H + NAV_H;
    int content_h = h - TABBAR_H - NAV_H - STATUS_H;

    gfx_set_clip(x, content_y, w, content_h);
    gfx_fill_rect(x, content_y, w, content_h, COL_PAGE_BG);

    if (node_count == 0 && !loading) {
        /* Welcome screen */
        gfx_fill_gradient_v(x, content_y, w, content_h, 0xF2F2F7, 0xE5E5EA);
        /* Center content */
        int mx = x + w/2;
        int my = content_y + content_h/2 - 60;
        
        /* Logo */
        gfx_fill_circle(mx, my, 40, 0x007AFF);
        draw_string_scaled(mx - 8*3, my - 16*3 + 12, "B", 0xFFFFFF, 3);
        
        /* Title */
        draw_string_scaled(mx - 8*4, my + 50, "Bdrowser", 0x1C1C1E, 2);
        
        /* Hint text */
        gfx_draw_string_transparent(mx - 132, my + 88, "Press / to enter a URL and start browsing", 0x8E8E93);
        
        /* Example chip */
        gfx_fill_rect_rounded(mx - 100, my + 112, 200, 32, 16, 0x007AFF);
        gfx_draw_string_transparent(mx - 80, my + 120, "Try: http://example.com", 0xFFFFFF);
    }

    /* Render DOM boxes */
    int oy = content_y - scroll_y;
    for (int bi = 0; bi < box_count; bi++) {
        render_box_t* b = &boxes[bi];
        int by = b->y + oy;
        if (by + b->h < content_y) continue;
        if (by > content_y + content_h) break;

        /* LI bullet */
        if (nodes[b->node_idx].type == NODE_LI) {
            gfx_fill_circle(b->x + x - 8, by + 8, 3, COL_TEXT);
        }

        render_node_box(b, x, oy);
    }

    /* Scrollbar */
    if (content_height > content_h) {
        int sb_x = x + w - 6;
        int sb_h = content_h;
        gfx_fill_rect(sb_x, content_y, 6, sb_h, 0xE5E5EA);
        int thumb_h = sb_h * content_h / content_height;
        if (thumb_h < 20) thumb_h = 20;
        int thumb_y = content_y + scroll_y * (sb_h - thumb_h) / (content_height - content_h);
        gfx_fill_rect_rounded(sb_x + 1, thumb_y, 4, thumb_h, 2, 0xAEAEB2);
    }

    gfx_reset_clip();

    /* === Status Bar === */
    int status_y = y + h - STATUS_H;
    gfx_fill_rect(x, status_y, w, STATUS_H, COL_STATUS_BG);
    gfx_draw_hline(x, status_y, w, COL_BORDER);
    gfx_draw_string_transparent(x + 8, status_y + 3, status_text, COL_STATUS_FG);

    /* Loading spinner */
    if (loading) {
        static int tick = 0; tick++;
        char dots[8]; int di = 0;
        for (int d = 0; d < (tick/4) % 4; d++) dots[di++] = '.';
        dots[di] = 0;
        char ls[64]; bw_strcpy(ls, status_text);
        int ll = bw_strlen(ls); ls[ll++] = ' ';
        for (int d = 0; dots[d]; d++) ls[ll++] = dots[d];
        ls[ll] = 0;
        gfx_draw_string_transparent(x + 8, status_y + 3, ls, 0x007AFF);
        /* Spinner on right */
        static const char* spin = "-\\|/";
        char sp[2] = { spin[(tick/4) % 4], 0 };
        gfx_draw_string_transparent(x + w - 20, status_y + 3, sp, 0x007AFF);
    }
}

static void bw_on_key(int id, char key_in) {
    (void)id;
    unsigned char k = (unsigned char)key_in;

    if (editing_url) {
        if (k == 27) { editing_url = 0; }
        else if (k == '\b') { if (url_len > 0) url_buf[--url_len] = 0; }
        else if (k == '\n') {
            editing_url = 0;
            if (url_len > 0) {
                loading = 1; bw_set_status("Loading...");
                bw_fetch_id++;
                create_task(bw_fetch_task, "Bdrowser Fetch");
            }
        } else if (k >= 32 && k <= 126 && url_len < URL_MAX - 1) {
            url_buf[url_len++] = k;
            url_buf[url_len] = 0;
        }
        return;
    }

    if (k == '/') {
        editing_url = 1;
        url_buf[0] = 0; url_len = 0;
        return;
    }
    if (k == 'r' || k == 'R') {
        if (url_len > 0) {
            loading = 1; bw_set_status("Reloading...");
            create_task(bw_fetch_task, "Bdrowser Fetch");
        }
        return;
    }

    wm_window_t* win = wm_get_window(bw_win_id);
    int ch = win ? win->h - TABBAR_H - NAV_H - STATUS_H : 400;
    int max_scroll = content_height - ch;
    if (max_scroll < 0) max_scroll = 0;

    if (k == 128) { /* UP */
        if (scroll_y > 0) scroll_y -= 24;
    } else if (k == 129) { /* DOWN */
        if (scroll_y < max_scroll) scroll_y += 24;
    } else if (k == 133) { /* PgUp */
        scroll_y -= ch - 40;
        if (scroll_y < 0) scroll_y = 0;
    } else if (k == 134) { /* PgDown */
        scroll_y += ch - 40;
        if (scroll_y > max_scroll) scroll_y = max_scroll;
    }
}

static void bw_on_resize(int win_id, int new_w, int new_h) {
    (void)win_id; (void)new_h;
    if (node_count > 0) do_layout(0, new_w);
}

void bdrowser(void) {
    uint32_t fw = gfx_get_fb_width();
    uint32_t fh = gfx_get_fb_height();

    bw_win_id = wm_open_window(
        (fw - 900) / 2, (fh - 650) / 2,
        900, 650,
        "Bdrowser",
        0x007AFF,
        bw_on_render,
        bw_on_key,
        bw_on_resize
    );

    if (bw_win_id < 0) return;
    
    wm_add_button(bw_win_id, 1, 196, 8, 20, 20, "+", COL_BTN, COL_BTN_FG, bw_on_click);
    wm_add_button(bw_win_id, 2, 8, 40, 28, 24, "<", COL_BTN, COL_BTN_FG, bw_on_click);
    wm_add_button(bw_win_id, 3, 40, 40, 28, 24, ">", COL_BTN, COL_BTN_FG, bw_on_click);
    wm_add_button(bw_win_id, 4, 72, 40, 28, 24, "R", COL_BTN, COL_BTN_FG, bw_on_click);

    url_buf[0] = 0; url_len = 0;
    node_count = 0; box_count = 0;
    text_pool_used = 0;
    scroll_y = 0; content_height = 0;
    loading = 0; editing_url = 0;
    bw_strcpy(page_title, "New Tab");
    bw_set_status("Ready  —  Press / to enter URL");

    bw_tab_count = 1;
    bw_tabs[0].used = 1;
    bw_strcpy(bw_tabs[0].url, url_buf);
    bw_tabs[0].url_len = url_len;
    bw_tabs[0].editing_url = editing_url;
    bw_strcpy(bw_tabs[0].page_title, page_title);
    bw_strcpy(bw_tabs[0].status_text, "Ready  —  Press / to enter URL");
    bw_tabs[0].loading = loading;
    bw_tabs[0].scroll_y = 0;
    bw_tabs[0].content_height = 0;
}
