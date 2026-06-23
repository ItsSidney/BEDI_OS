#include "gui/wm.h"
#include "drivers/video/gfx.h"
#include "filesystem/filesystem.h"
#include <stdint.h>
#include <string.h>
#include "kernel/mem/kheap.h"

#define BDIM_MAX_LINES 256
#define BDIM_MAX_COLS  128
#define BDIM_AUTO_MAX  64
#define MAX_TOKENS 128

static char bdim_buf[BDIM_MAX_LINES][BDIM_MAX_COLS];
static int bdim_lines = 1;
static int cx = 0, cy = 0;
static int scroll_y = 0;
static int mode = 0; // 0=NORMAL, 1=INSERT, 2=COMMAND, 3=VISUAL
static char cmd_buf[64];
static int cmd_len = 0;
static char current_file[64];
static int win_id = -1;
static int modified = 0;
static int show_lines = 1;
static int visual_anchor = 0;
static int tab_w = 4;

// ── Syntax highlighting ─────────────────────────────────────
typedef enum { TT_KEYWORD=0, TT_TYPE=1, TT_FUNC=2, TT_NUM=3, TT_STRING=4,
               TT_COMMENT=5, TT_CONST=6, TT_OP=7, TT_BRACE=8, TT_ID=9, TT_OTHER=10 } TT;

typedef struct { TT type; int start; int len; } Tok;

static uint32_t tok_color(TT t) {
    switch(t) {
        case TT_KEYWORD: return 0xC586C0; // purple (if, else, while, for, return)
        case TT_TYPE:    return 0x4EC9B0; // teal (int, double, bool, string, void)
        case TT_FUNC:    return 0xDCDCAA; // yellow (print, input, exit, main)
        case TT_NUM:     return 0xB5CEA8; // soft green (numbers)
        case TT_STRING:  return 0xCE9178; // orange-warm (strings)
        case TT_COMMENT: return 0x6A9955; // muted green (// and /* */)
        case TT_CONST:   return 0x569CD6; // bright blue (true, false, endl)
        case TT_OP:      return 0xD4D4D4; // light gray (operators)
        case TT_BRACE:   return 0xFFD700; // gold (braces, parens, semicolon, comma)
        default:         return 0xCCCCCC; // light gray (identifiers, other)
    }
}

// Actual BEDIC keywords with categories
typedef struct { const char* word; const char* desc; int cat; } ACItem;
// cat: 0=type, 1=ctrl, 2=func, 3=const, 4=other

static const ACItem bedic_keywords[] = {
    {"int",     "[type]  32-bit integer", 0},
    {"double",  "[type]  Fixed-point decimal", 0},
    {"bool",    "[type]  Boolean (true/false)", 0},
    {"string",  "[type]  Text string", 0},
    {"void",    "[type]  No return value", 0},
    {"if",      "[ctrl]  Conditional branch", 1},
    {"else",    "[ctrl]  Else branch", 1},
    {"while",   "[ctrl]  Loop while condition", 1},
    {"for",     "[ctrl]  C-style for loop", 1},
    {"return",  "[ctrl]  Return from main()", 1},
    {"print",   "[func]  Print values to console", 2},
    {"input",   "[func]  Read input from user", 2},
    {"exit",    "[func]  Exit with status code", 2},
    {"main",    "[func]  Program entry point", 2},
    {"true",    "[const] Boolean true", 3},
    {"false",   "[const] Boolean false", 3},
    {"endl",    "[const] Newline constant", 3},
    {0,0,0}
};

static int is_kw(const char* w, int* cat_out) {
    for (int i=0; bedic_keywords[i].word; i++) {
        if (strcmp(w, bedic_keywords[i].word) == 0) {
            if (cat_out) *cat_out = bedic_keywords[i].cat;
            return 1;
        }
    }
    return 0;
}

static int wc(char c) {
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';
}

static int tokenize(char* line, Tok* toks) {
    int n=0, i=0;
    while (line[i] && i<BDIM_MAX_COLS && n<MAX_TOKENS) {
        if (line[i]==' '||line[i]=='\t') { i++; continue; }
        if (line[i]=='#' && i==0) {
            int s=i; while(line[i]) i++; toks[n++]=(Tok){TT_COMMENT,s,i-s}; continue;
        }
        if (line[i]=='/' && line[i+1]=='/') {
            int s=i; while(line[i]) i++; toks[n++]=(Tok){TT_COMMENT,s,i-s}; break;
        }
        if (line[i]=='/' && line[i+1]=='*') {
            int s=i; i+=2;
            while(!(line[i]=='*'&&line[i+1]=='/')&&line[i]) i++;
            if(line[i]=='*') i+=2;
            toks[n++]=(Tok){TT_COMMENT,s,i-s}; continue;
        }
        if (line[i]=='"') {
            int s=i; i++;
            while(line[i]!='"'&&line[i]) { if(line[i]=='\\'&&line[i+1]) i++; i++; }
            if(line[i]=='"') i++;
            toks[n++]=(Tok){TT_STRING,s,i-s}; continue;
        }
        if (line[i]=='\'') {
            int s=i; i++;
            if(line[i]=='\\'&&line[i+1]) i+=2; else i++;
            if(line[i]=='\'') i++;
            toks[n++]=(Tok){TT_STRING,s,i-s}; continue;
        }
        if ((line[i]>='0'&&line[i]<='9')||(line[i]=='.'&&line[i+1]>='0')) {
            int s=i;
            while((line[i]>='0'&&line[i]<='9')||line[i]=='.'||line[i]=='e'||line[i]=='E') {
                if((line[i]=='e'||line[i]=='E')&&(line[i+1]=='+'||line[i+1]=='-')) i++;
                i++;
            }
            toks[n++]=(Tok){TT_NUM,s,i-s}; continue;
        }
        if (wc(line[i])) {
            int s=i;
            while(wc(line[i])) i++;
            char w[32]; int l=i-s;
            for(int j=0;j<l&&j<31;j++) w[j]=line[s+j]; w[l]=0;
            int cat;
            if (is_kw(w, &cat)) {
                TT tt = TT_KEYWORD;
                if (cat == 0) tt = TT_TYPE;
                else if (cat == 2) tt = TT_FUNC;
                else if (cat == 3) tt = TT_CONST;
                else tt = TT_KEYWORD;
                toks[n++]=(Tok){tt,s,l};
            } else {
                toks[n++]=(Tok){TT_ID,s,l};
            }
            continue;
        }
        // operators
        if (line[i]=='+'&&line[i+1]=='+'||line[i]=='-'&&line[i+1]=='-'||
            line[i]=='-'&&line[i+1]=='>'||line[i]=='='&&line[i+1]=='='||
            line[i]=='!'&&line[i+1]=='='||line[i]=='<'&&line[i+1]=='='||
            line[i]=='>'&&line[i+1]=='='||line[i]=='&'&&line[i+1]=='&'||
            line[i]=='|'&&line[i+1]=='|') { toks[n++]=(Tok){TT_OP,i,2}; i+=2; continue; }
        if (line[i]=='+'||line[i]=='-'||line[i]=='*'||line[i]=='/'||
            line[i]=='='||line[i]=='<'||line[i]=='>'||line[i]=='!'||
            line[i]=='&'||line[i]=='|'||line[i]=='%') { toks[n++]=(Tok){TT_OP,i,1}; i++; continue; }
        if (line[i]=='{'||line[i]=='}'||line[i]=='('||line[i]==')'||
            line[i]=='['||line[i]==']'||line[i]==';'||line[i]==',') {
            toks[n++]=(Tok){TT_BRACE,i,1}; i++; continue;
        }
        toks[n++]=(Tok){TT_OTHER,i,1}; i++;
    }
    return n;
}

// ── Autocomplete ─────────────────────────────────────────────
static char suggestions[BDIM_AUTO_MAX][24];
static char sug_desc[BDIM_AUTO_MAX][36];
static uint32_t sug_col[BDIM_AUTO_MAX];
static int sug_count=0, sug_sel=0, sug_active=0;
static int sug_cx=0, sug_cy=0, sug_plen=0;

static void build_suggest(const char* pfx) {
    sug_count=0;
    int pl=strlen(pfx);
    if (!pl) {
        // Show all keywords when no prefix typed
        for (int i=0; bedic_keywords[i].word && sug_count<BDIM_AUTO_MAX; i++) {
            int k=0; while(bedic_keywords[i].word[k]&&k<23) suggestions[sug_count][k]=bedic_keywords[i].word[k],k++;
            suggestions[sug_count][k]=0;
            k=0; while(bedic_keywords[i].desc[k]&&k<35) sug_desc[sug_count][k]=bedic_keywords[i].desc[k],k++;
            sug_desc[sug_count][k]=0;
            uint32_t cols[]={0x4EC9B0,0xC586C0,0xDCDCAA,0x569CD6,0xCCCCCC};
            sug_col[sug_count]=cols[bedic_keywords[i].cat];
            sug_count++;
        }
        return;
    }
    for (int i=0; bedic_keywords[i].word && sug_count<BDIM_AUTO_MAX; i++) {
        int ok=1;
        for (int j=0; pfx[j]; j++) {
            if (bedic_keywords[i].word[j]!=pfx[j]) { ok=0; break; }
        }
        if (ok) {
            int k=0; while(bedic_keywords[i].word[k]&&k<23) suggestions[sug_count][k]=bedic_keywords[i].word[k],k++;
            suggestions[sug_count][k]=0;
            k=0; while(bedic_keywords[i].desc[k]&&k<35) sug_desc[sug_count][k]=bedic_keywords[i].desc[k],k++;
            sug_desc[sug_count][k]=0;
            uint32_t cols[]={0x4EC9B0,0xC586C0,0xDCDCAA,0x569CD6,0xCCCCCC};
            sug_col[sug_count]=cols[bedic_keywords[i].cat];
            sug_count++;
        }
    }
    if (sug_count>0 && sug_sel>=sug_count) sug_sel=sug_count-1;
}

static int ends_with(const char* n, const char* e) {
    int nl=strlen(n),el=strlen(e);
    if(nl<el) return 0;
    return strcmp(n+nl-el,e)==0;
}

static void refresh_ac(void) {
    if (!ends_with(current_file,".bc") && !ends_with(current_file,".bedic")) { sug_active=0; return; }
    sug_plen=0;
    int len=0; while(bdim_buf[cy][len]) len++;
    for (int i=cx-1; i>=0; i--) {
        char c=bdim_buf[cy][i];
        if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_') sug_plen++;
        else break;
    }
    // Always show suggestions when: we just typed a word, or we're in a word
    // But only if we're actually editing code
    if (sug_plen<24) {
        char pfx[24]; int pi=0;
        for (int j=cx-sug_plen; j<cx; j++) pfx[pi++]=bdim_buf[cy][j];
        pfx[pi]=0;
        build_suggest(pfx);
        if (sug_count>0) { sug_active=1; sug_cx=cx; sug_cy=cy; }
        else sug_active=0;
    } else sug_active=0;
}

static void accept_sug(void) {
    if (!sug_active || sug_sel>=sug_count) return;
    char* w=suggestions[sug_sel];
    int wlen=strlen(w);
    int start=sug_cx-sug_plen;
    int len=0; while(bdim_buf[cy][len]) len++;
    int delta=wlen-sug_plen;
    if (delta>0) for(int i=len;i>=start+sug_plen;i--) bdim_buf[cy][i+delta]=bdim_buf[cy][i];
    else if(delta<0) for(int i=start+sug_plen;i<=len;i++) bdim_buf[cy][i+delta]=bdim_buf[cy][i];
    for (int i=0;i<wlen;i++) bdim_buf[cy][start+i]=w[i];
    bdim_buf[cy][start+wlen]=0;
    cx=start+wlen;
    sug_active=0;
    modified=1;
}

// ── File I/O ─────────────────────────────────────────────────
static void load(const char* fn) {
    bdim_lines=1; cx=0; cy=0; modified=0;
    for(int i=0;i<BDIM_MAX_LINES;i++) for(int j=0;j<BDIM_MAX_COLS;j++) bdim_buf[i][j]=0;
    if(!fn){ current_file[0]=0; return; }
    int i=0; while(fn[i]&&i<63){current_file[i]=fn[i];i++;}current_file[i]=0;
    char raw[8192];
    int fd=fs_open(current_file,0);
    if(fd>=0){
        int bytes=fs_read(fd,raw,8191);
        if(bytes>0){
            raw[bytes]=0; int r=0,c=0;
            for(int k=0;k<bytes;k++){
                if(raw[k]=='\n'){r++;c=0;if(r>=BDIM_MAX_LINES)break;}
                else if(raw[k]!='\r'){if(c<BDIM_MAX_COLS-1)bdim_buf[r][c++]=raw[k];}
            }
            bdim_lines=r+1;
        }
        fs_close(fd);
    }
}

static void save(void) {
    if(!current_file[0]) return;
    char raw[8192]; int pos=0;
    for(int r=0;r<bdim_lines;r++){
        for(int c=0;c<BDIM_MAX_COLS;c++){if(bdim_buf[r][c]==0)break;if(pos<8191)raw[pos++]=bdim_buf[r][c];}
        if(r<bdim_lines-1&&pos<8191) raw[pos++]='\n';
    }
    fs_touch(current_file);
    int fd=fs_open(current_file,0);
    if(fd>=0){fs_truncate(current_file);fs_write(fd,raw,pos);fs_close(fd);}
    modified=0;
}

static char clipboard[256];
static int clip_len=0;

// ── Input ────────────────────────────────────────────────────
static void on_key(int id, char key_in) {
    unsigned char k=(unsigned char)key_in;
    int is_bc = ends_with(current_file,".bc")||ends_with(current_file,".bedic");

    if (sug_active) {
        if(k==27){sug_active=0;return;}
        else if(k=='\n'||k=='\t'){accept_sug();return;}
        else if(k==128||k==130){if(sug_sel>0)sug_sel--;return;}
        else if(k==129||k==131){if(sug_sel<sug_count-1)sug_sel++;return;}
        else sug_active=0;
    }

    if (mode==0) { // NORMAL
        if(k=='i') mode=1;
        else if(k=='I'){mode=1;cx=0;}
        else if(k=='A'){mode=1;int len=0;while(bdim_buf[cy][len])len++;cx=len;}
        else if(k=='o'){
            if(bdim_lines<BDIM_MAX_LINES){
                for(int r=bdim_lines;r>cy+1;r--)for(int c=0;c<BDIM_MAX_COLS;c++)bdim_buf[r][c]=bdim_buf[r-1][c];
                bdim_buf[cy+1][0]=0;bdim_lines++;cy++;cx=0;mode=1;
            }
        }
        else if(k=='O'){
            if(bdim_lines<BDIM_MAX_LINES){
                for(int r=bdim_lines;r>cy;r--)for(int c=0;c<BDIM_MAX_COLS;c++)bdim_buf[r][c]=bdim_buf[r-1][c];
                bdim_buf[cy][0]=0;bdim_lines++;mode=1;cx=0;
            }
        }
        else if(k=='x'){
            int len=0;while(bdim_buf[cy][len])len++;
            if(cx<len){for(int i=cx;i<len;i++)bdim_buf[cy][i]=bdim_buf[cy][i+1];modified=1;}
        }
        else if(k=='d'&&cx==0){
            // Delete whole line
            for(int r=cy;r<bdim_lines-1;r++)for(int c=0;c<BDIM_MAX_COLS;c++)bdim_buf[r][c]=bdim_buf[r+1][c];
            bdim_lines--;if(cy>=bdim_lines)cy=bdim_lines-1;cx=0;modified=1;
        }
        else if(k=='y'){
            // Yank line into clipboard
            int len=0;while(bdim_buf[cy][len]&&len<255)len++;
            for(int i=0;i<len;i++)clipboard[i]=bdim_buf[cy][i];
            clip_len=len;clipboard[len]=0;
        }
        else if(k=='p'){
            // Paste below
            if(clip_len>0&&bdim_lines<BDIM_MAX_LINES){
                for(int r=bdim_lines;r>cy+1;r--)for(int c=0;c<BDIM_MAX_COLS;c++)bdim_buf[r][c]=bdim_buf[r-1][c];
                for(int i=0;i<clip_len&&i<BDIM_MAX_COLS-1;i++)bdim_buf[cy+1][i]=clipboard[i];
                bdim_buf[cy+1][clip_len]=0;bdim_lines++;modified=1;
            }
        }
        else if(k=='P'){
            // Paste above
            if(clip_len>0&&bdim_lines<BDIM_MAX_LINES){
                for(int r=bdim_lines;r>cy;r--)for(int c=0;c<BDIM_MAX_COLS;c++)bdim_buf[r][c]=bdim_buf[r-1][c];
                for(int i=0;i<clip_len&&i<BDIM_MAX_COLS-1;i++)bdim_buf[cy][i]=clipboard[i];
                bdim_buf[cy][clip_len]=0;bdim_lines++;modified=1;
            }
        }
        else if(k=='v'){mode=3;visual_anchor=cx;}
        else if(k=='u'&&cx>0) cx--;
        else if(k=='U'&&cx<BDIM_MAX_COLS-1&&bdim_buf[cy][cx]!=0) cx++;
        else if(k=='k'&&cy>0){cy--;int len=0;while(bdim_buf[cy][len])len++;if(cx>=len)cx=len>0?len-1:0;}
        else if(k=='j'&&cy<bdim_lines-1){cy++;int len=0;while(bdim_buf[cy][len])len++;if(cx>=len)cx=len>0?len-1:0;}
        else if(k=='g'){cy=0;cx=0;}
        else if(k=='G'){cy=bdim_lines-1;cx=0;}
        else if(k==':'){mode=2;cmd_len=0;cmd_buf[0]=0;}
        else if(k=='\t'){}
    }
    else if (mode==3) { // VISUAL
        if(k=='y'){
            // Yank selected text
            int a=visual_anchor,b=cx;
            if(a>b){int t=a;a=b;b=t;}
            int len=0;while(bdim_buf[cy][len])len++;
            if(a>=len)a=len-1;if(b>=len)b=len-1;if(a<0)a=0;
            if(a<b){
                clip_len=b-a;
                for(int i=0;i<clip_len&&i<255;i++)clipboard[i]=bdim_buf[cy][a+i];
                clipboard[clip_len]=0;
            }
            mode=0;
        }
        else if(k=='d'){
            int a=visual_anchor,b=cx;
            if(a>b){int t=a;a=b;b=t;}
            int len=0;while(bdim_buf[cy][len])len++;
            if(b>=len)b=len-1;
            if(a<b){
                for(int i=a;i<len-(b-a);i++)bdim_buf[cy][i]=bdim_buf[cy][i+(b-a)];
                bdim_buf[cy][len-(b-a)]=0;cx=a;modified=1;
            }
            mode=0;
        }
        else if(k==27) mode=0;
        else if(k=='h'&&cx>0) cx--;
        else if(k=='l'&&cx<BDIM_MAX_COLS-1&&bdim_buf[cy][cx]!=0) cx++;
        else if(k=='k'&&cy>0){cy--;int len=0;while(bdim_buf[cy][len])len++;if(cx>=len)cx=len>0?len-1:0;}
        else if(k=='j'&&cy<bdim_lines-1){cy++;int len=0;while(bdim_buf[cy][len])len++;if(cx>=len)cx=len>0?len-1:0;}
    }
    else if (mode==1) { // INSERT
        if(k==27){mode=0;if(cx>0)cx--;}
        else if(k=='\b'){
            if(cx>0){for(int i=cx;i<BDIM_MAX_COLS-1;i++)bdim_buf[cy][i-1]=bdim_buf[cy][i];cx--;modified=1;}
            else if(cy>0){
                int pl=0;while(bdim_buf[cy-1][pl])pl++;int cl=0;while(bdim_buf[cy][cl])cl++;
                for(int i=0;i<cl&&pl+i<BDIM_MAX_COLS-1;i++)bdim_buf[cy-1][pl+i]=bdim_buf[cy][i];
                for(int r=cy;r<bdim_lines-1;r++)for(int c=0;c<BDIM_MAX_COLS;c++)bdim_buf[r][c]=bdim_buf[r+1][c];
                bdim_lines--;cy--;cx=pl;modified=1;
            }
        }
        else if(k=='\n'){
            if(bdim_lines<BDIM_MAX_LINES){
                for(int r=bdim_lines;r>cy+1;r--)for(int c=0;c<BDIM_MAX_COLS;c++)bdim_buf[r][c]=bdim_buf[r-1][c];
                int cur=0;while(bdim_buf[cy][cur])cur++;
                int nx=0;for(int c=cx;c<cur;c++){bdim_buf[cy+1][nx++]=bdim_buf[cy][c];bdim_buf[cy][c]=0;}
                bdim_buf[cy+1][nx]=0;bdim_lines++;cy++;cx=0;modified=1;
            }
        }
        else if(k=='\t'){
            for(int t=0;t<tab_w;t++){
                int len=0;while(bdim_buf[cy][len])len++;
                if(len<BDIM_MAX_COLS-1){for(int i=len;i>cx;i--)bdim_buf[cy][i]=bdim_buf[cy][i-1];bdim_buf[cy][cx++]=' ';}
            }
            modified=1;
        }
        else if(k==130&&cx>0) cx--;
        else if(k==131){int len=0;while(bdim_buf[cy][len])len++;if(cx<len)cx++;}
        else if(k==128&&cy>0){cy--;int len=0;while(bdim_buf[cy][len])len++;if(cx>len)cx=len;}
        else if(k==129&&cy<bdim_lines-1){cy++;int len=0;while(bdim_buf[cy][len])len++;if(cx>len)cx=len;}
        else if(k==133) cy=(cy-10<0)?0:cy-10;
        else if(k==134) cy=(cy+10>=bdim_lines)?bdim_lines-1:cy+10;
        else if(k==135) cx=0;
        else if(k==136){int len=0;while(bdim_buf[cy][len])len++;cx=len;}
        else if(k>=32&&k<=126){
            int len=0;while(bdim_buf[cy][len])len++;
            if(len>=BDIM_MAX_COLS-2&&bdim_lines<BDIM_MAX_LINES){
                for(int r=bdim_lines;r>cy+1;r--)for(int c=0;c<BDIM_MAX_COLS;c++)bdim_buf[r][c]=bdim_buf[r-1][c];
                bdim_buf[cy+1][0]=0;bdim_lines++;cy++;cx=0;len=0;
            }
            len=0;while(bdim_buf[cy][len])len++;
            if(len<BDIM_MAX_COLS-1){for(int i=len;i>cx;i--)bdim_buf[cy][i]=bdim_buf[cy][i-1];bdim_buf[cy][cx++]=k;modified=1;}
            if(is_bc) refresh_ac();
        }
    }
    else if (mode==2) { // COMMAND
        if(k==27) mode=0;
        else if(k=='\b'&&cmd_len>0) cmd_buf[--cmd_len]=0;
        else if(k=='\n'){
            int did=0;
            if(strcmp(cmd_buf,"wq")==0||strcmp(cmd_buf,"wq!")==0){save();did=1;sug_active=0;wm_close_window(id);return;}
            if(strcmp(cmd_buf,"w")==0){save();did=1;}
            if(strcmp(cmd_buf,"q")==0){did=1;sug_active=0;wm_close_window(id);return;}
            if(strcmp(cmd_buf,"q!")==0){did=1;sug_active=0;wm_close_window(id);return;}
            if(strcmp(cmd_buf,"qa")==0||strcmp(cmd_buf,"qa!")==0){did=1;sug_active=0;wm_close_window(id);return;}
            if(strcmp(cmd_buf,"help")==0){
                did=1;
                // Show help text in buffer
                bdim_lines=0;
                const char* help_text[]={
                    "bdim - BEDIC Editor",
                    "====================",
                    "",
                    "NORMAL MODE:",
                    "  i          Insert mode at cursor",
                    "  I          Insert mode at line start",
                    "  A          Append at end of line",
                    "  o          Open new line below",
                    "  O          Open new line above",
                    "  x          Delete char under cursor",
                    "  dd         Delete whole line",
                    "  yy         Yank (copy) line to clipboard",
                    "  p          Paste clipboard below line",
                    "  P          Paste clipboard above line",
                    "  v          Visual mode (select with arrows)",
                    "  u          Cursor left",
                    "  U          Cursor right",
                    "  h          Cursor left",
                    "  l          Cursor right",
                    "  k          Cursor up",
                    "  j          Cursor down",
                    "  g          Go to first line",
                    "  G          Go to last line",
                    "  :          Enter command mode",
                    "  Tab        (unused in normal mode)",
                    "",
                    "INSERT MODE:",
                    "  Esc        Return to normal mode",
                    "  Backspace  Delete char before cursor",
                    "  Enter      Split line",
                    "  Tab        Insert 4 spaces",
                    "  Arrows     Move cursor",
                    "  PgUp/PgDn  Scroll 10 lines",
                    "  Home/End   Line start/end",
                    "  Printable  Insert character",
                    "",
                    "VISUAL MODE:",
                    "  h/j/k/l   Extend selection",
                    "  y         Yank selected text",
                    "  d         Delete selected text",
                    "  Esc       Exit visual mode",
                    "",
                    "COMMAND MODE:",
                    "  :w        Save file",
                    "  :q        Quit (modified files prevent quit)",
                    "  :q!       Force quit without saving",
                    "  :wq       Save and quit",
                    "  :e file   Open a different file",
                    "  :help     Display this help screen",
                    "  :set nu   Show line numbers",
                    "  :set nonu Hide line numbers",
                    "",
                    "AUTOCOMPLETE:",
                    "  Suggestions appear as you type code",
                    "  Tab/Enter Accept selected suggestion",
                    "  Esc       Dismiss suggestion list",
                    "  Up/Down   Navigate suggestions",
                    "",
                    "KEYWORD CATEGORIES:",
                    "  [type]    Data types (int, double, string...)",
                    "  [ctrl]    Control flow (if, while, for...)",
                    "  [func]    Built-in functions (print, input...)",
                    "  [const]   Constants (true, false, endl...)",
                    "",
                    "STATUS BAR:",
                    "  -- MODE --     Current mode indicator",
                    "  [+]           File has unsaved changes",
                    "  .bc / .txt    File type",
                    "  42:7          Line 42, Column 7",
                    0
                };
                for(int hi=0;help_text[hi];hi++){
                    int j=0;
                    while(help_text[hi][j]&&j<BDIM_MAX_COLS-1){
                        bdim_buf[hi][j]=help_text[hi][j];j++;
                    }
                    bdim_buf[hi][j]=0;
                    bdim_lines++;
                }
                mode=0;cx=0;cy=0;
            }
            if(!did&&strncmp(cmd_buf,"e ",2)==0){
                const char* fn=cmd_buf+2;
                if(fn[0]){save();load(fn);mode=0;}
            }
            if(!did&&strcmp(cmd_buf,"set nu")==0){show_lines=1;mode=0;}
            if(!did&&strcmp(cmd_buf,"set nonu")==0){show_lines=0;mode=0;}
            if(!did) mode=0;
        }
        else if(k>=32&&k<=126&&cmd_len<62){cmd_buf[cmd_len++]=k;cmd_buf[cmd_len]=0;}
    }
}

// ── Rendering ────────────────────────────────────────────────
static void on_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx;(void)vy;(void)id;
    gfx_fill_rect(x,y,w,h,0x1E1E1E);

    int gutter = show_lines ? 40 : 0;
    int vis=(h-40)/16;
    if(cy<scroll_y) scroll_y=cy;
    if(cy>=scroll_y+vis) scroll_y=cy-vis+1;
    if(scroll_y<0) scroll_y=0;

    int is_bc = ends_with(current_file,".bc")||ends_with(current_file,".bedic");
    Tok toks[MAX_TOKENS];

    // ── Title bar ──
    gfx_fill_rect(x,y,w,20,0x2D2D2D);
    gfx_fill_rect(x,y+19,w,1,0x569CD6);
    char title[48]; int ti=0;
    const char* fn=current_file[0]?current_file:"untitled.bc";
    for(int i=0;fn[i]&&ti<36;i++) title[ti++]=fn[i];
    if(modified&&ti<35){title[ti++]=' ';title[ti++]='*';}
    title[ti]=0;
    gfx_fill_rect(x+6,y+4,8,10,is_bc?0x569CD6:0x636366);
    gfx_draw_string_transparent(x+20,y+3,title,0xCCCCCC);

    // ── Background passes: gutter fills, line highlight ──
    for(int r=0;r<vis;r++){
        int br=scroll_y+r;
        int ry=y+24+r*16;
        if(br>=bdim_lines) break;
        if(show_lines){
            gfx_fill_rect(x+gutter-48,ry-1,48,16,br==cy?0x313131:0x252526);
        }
        if(br==cy){
            gfx_fill_rect(x+gutter,ry-1,w-gutter,16,0x2A2D2E);
        }
    }

    // ── Code lines ──
    for(int r=0;r<vis;r++){
        int br=scroll_y+r;
        int ry=y+24+r*16;
        if(br>=bdim_lines){
            gfx_draw_string_transparent(x+5+gutter,ry,"~",0x3C3C3C);
            continue;
        }

        if(show_lines){
            char ln[8]; int li=0;
            int n=br+1;
            if(n==0)ln[li++]='0';
            else{char tmp[8];int ti2=0;while(n){tmp[ti2++]='0'+(n%10);n/=10;}for(int j=ti2-1;j>=0;j--)ln[li++]=tmp[j];}
            ln[li]=0;
            gfx_draw_string_transparent(x+gutter-li*8-8,ry,ln,br==cy?0xCCCCCC:0x636366);
        }

        if(mode==3&&br==cy){
            int a=visual_anchor,b=cx;
            if(a>b){int t=a;a=b;b=t;}
            int len=0;while(bdim_buf[cy][len])len++;
            if(b>len)b=len;if(a<0)a=0;
            if(a<b) gfx_fill_rect(x+gutter+a*8,ry-1,(b-a)*8,16,0x264F78);
        }

        if(is_bc){
            int cnt=tokenize(bdim_buf[br],toks);
            int dx=x+gutter+5;
            for(int t=0;t<cnt;t++){
                uint32_t col=tok_color(toks[t].type);
                char s[2]={0,0};
                for(int j=0;j<toks[t].len;j++){
                    s[0]=bdim_buf[br][toks[t].start+j];
                    gfx_draw_string_transparent(dx,ry,s,col);
                    dx+=8;
                }
            }
        } else {
            char s[2]={0,0};
            for(int c=0;c<BDIM_MAX_COLS;c++){
                if(bdim_buf[br][c]==0) break;
                s[0]=bdim_buf[br][c];
                gfx_draw_string_transparent(x+gutter+5+c*8,ry,s,0xCCCCCC);
            }
        }
    }

    // ── Gutter separator ──
    if(show_lines) gfx_fill_rect(x+gutter-1,y+21,1,h-41,0x3A3A3C);

    // ── Scrollbar ──
    if(bdim_lines>vis){
        int sb_x=x+w-6; int sb_h=h-40;
        gfx_fill_rect(sb_x,y+20,6,sb_h,0x1E1E1E);
        int thumb_h=sb_h*vis/bdim_lines;
        if(thumb_h<12) thumb_h=12;
        int thumb_y=y+20+sb_h*scroll_y/(bdim_lines-vis);
        gfx_fill_rect(sb_x,thumb_y,6,thumb_h,0x555555);
    }

    // ── Cursor ──
    if(cy>=scroll_y&&cy<scroll_y+vis){
        int cyy=y+24+(cy-scroll_y)*16;
        if(mode==1) gfx_draw_rect_outline(x+gutter+5+cx*8,cyy,2,16,1,0xAED6FF);
        else if(mode==3) gfx_fill_rect(x+gutter+5+cx*8,cyy,8,16,0x569CD6);
        else gfx_fill_rect(x+gutter+5+cx*8,cyy,8,16,0x888888);
    }

    // ── Autocomplete dropdown ──
    if(sug_active&&sug_count>0){
        int dx=x+gutter+5+sug_cx*8;
        int dy=y+24+(sug_cy-scroll_y)*16+18;
        if(dy+sug_count*16>y+h-20) dy=y+24+(sug_cy-scroll_y)*16-sug_count*16-2;
        int max_w=160;
        for(int i=0;i<sug_count;i++){
            int wlen=strlen(suggestions[i]);
            int dlen=strlen(sug_desc[i]);
            int total=16 + wlen*8 + dlen*8;
            if(total>max_w) max_w=total;
        }
        int dw=max_w+12, dh=sug_count*16+4;
        if(dx+dw>x+w-5) dx=x+w-5-dw;
        if(dx<x+5) dx=x+5; if(dw>w-10) dw=w-10;
        gfx_fill_rect(dx+2,dy+2,dw,dh,0x000000);
        gfx_fill_rect(dx,dy,dw,dh,0x252526);
        gfx_fill_rect(dx,dy,dw,18,0x2D2D2D);
        gfx_draw_string_transparent(dx+6,dy+1,"Completion",0x888888);
        gfx_draw_rect_outline(dx,dy,dw,dh,1,0x569CD6);
        for(int i=0;i<sug_count;i++){
            int iy=dy+2+i*16;
            if(i==sug_sel) gfx_fill_rect(dx+1,iy+18,dw-2,16,0x094771);
            else if(i%2==0) gfx_fill_rect(dx+1,iy+18,dw-2,16,0x1E1E1E);
            gfx_fill_rect(dx+4,iy+18+4,4,4,sug_col[i]);
            gfx_draw_string_transparent(dx+12,iy+18+1,suggestions[i],0xE0E0E0);
            int wlen=strlen(suggestions[i]);
            int desc_max=(dw-16-wlen*8-2)/8;
            char desc_buf[40]; int di=0;
            for(int j=0;sug_desc[i][j]&&j<desc_max&&j<35;j++) desc_buf[di++]=sug_desc[i][j];
            if(sug_desc[i][desc_max]&&di>2){desc_buf[di-2]='.';desc_buf[di-1]='.';desc_buf[di]=0;}
            else desc_buf[di]=0;
            gfx_draw_string_transparent(dx+14+wlen*8,iy+18+1,desc_buf,0x636366);
        }
    }

    // ── Status bar ──
    gfx_fill_rect(x,y+h-20,w,20,0x007ACC);
    uint32_t mode_col=0; const char* mode_lbl="";
    if(mode==0){mode_col=0x4EC9B0;mode_lbl="NORMAL";}
    else if(mode==1){mode_col=0x569CD6;mode_lbl="INSERT";}
    else if(mode==2){mode_col=0xC586C0;mode_lbl="CMD";}
    else if(mode==3){mode_col=0xDCDCAA;mode_lbl="VISUAL";}
    gfx_fill_rect(x+2,y+h-18,46,16,mode_col);
    gfx_draw_string_transparent(x+6,y+h-18,mode_lbl,mode_col==0xDCDCAA?0x1E1E1E:0xFFFFFF);

    // Command text
    if(mode==2){
        char cb[40]; int ci=0; cb[ci++]=':';
        for(int i=0;cmd_buf[i]&&ci<38;i++) cb[ci++]=cmd_buf[i]; cb[ci]=0;
        gfx_draw_string_transparent(x+52,y+h-18,cb,0xFFFFFF);
    } else if(modified){
        gfx_fill_rect(x+52,y+h-18,16,16,0xE06C75);
        gfx_draw_string_transparent(x+56,y+h-18,"+",0x1E1E1E);
    }

    // Right info
    char info[40]; int ip=0;
    int ln=cy+1;char lb[8];int li=0;
    if(ln==0)lb[li++]='0';else{int t=ln;char tmp[8];int ti2=0;while(t){tmp[ti2++]='0'+(t%10);t/=10;}for(int j=ti2-1;j>=0;j--)lb[li++]=tmp[j];}
    for(int j=0;j<li;j++)info[ip++]=lb[j];
    info[ip++]=':';
    int cn=cx+1;li=0;
    if(cn==0)lb[li++]='0';else{int t=cn;char tmp[8];int ti2=0;while(t){tmp[ti2++]='0'+(t%10);t/=10;}for(int j=ti2-1;j>=0;j--)lb[li++]=tmp[j];}
    for(int j=0;j<li;j++)info[ip++]=lb[j];
    info[ip++]=' ';info[ip++]=' ';
    const char* ft=is_bc?".bc":".txt";
    for(int i=0;ft[i]&&ip<38;i++) info[ip++]=ft[i];
    info[ip]=0;
    gfx_draw_string_transparent(x+w-5-ip*8,y+h-18,info,0xFFFFFF);
}

static void on_resize(int wid, int w, int h) { (void)wid; (void)w; (void)h; }

void bdim_app(const char* filename) {
    if(win_id>=0){wm_window_t* w=wm_get_window(win_id);if(w){wm_bring_to_front(win_id);return;}}
    load(filename);
    mode=0; sug_active=0; modified=0; show_lines=1;
    win_id=wm_open_window(40,40,900,650,"bdim - BEDIC Editor",0x007ACC,on_render,on_key,on_resize);
}
