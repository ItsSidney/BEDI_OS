#include "math_engine.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════
 *  Vector 2
 * ═══════════════════════════════════════════════ */
vec2_t vec2(float x, float y) { vec2_t v; v.x=x; v.y=y; return v; }
vec2_t vec2_add(vec2_t a, vec2_t b) { return vec2(a.x+b.x, a.y+b.y); }
vec2_t vec2_sub(vec2_t a, vec2_t b) { return vec2(a.x-b.x, a.y-b.y); }
vec2_t vec2_mul(vec2_t a, float s) { return vec2(a.x*s, a.y*s); }
vec2_t vec2_div(vec2_t a, float s) { return vec2(a.x/s, a.y/s); }
float  vec2_dot(vec2_t a, vec2_t b) { return a.x*b.x + a.y*b.y; }
float  vec2_len(vec2_t a) { return sqrtf(a.x*a.x + a.y*a.y); }
vec2_t vec2_norm(vec2_t a) { float l=vec2_len(a); return l>0 ? vec2(a.x/l,a.y/l) : vec2(0,0); }

/* ═══════════════════════════════════════════════
 *  Vector 3
 * ═══════════════════════════════════════════════ */
vec3_t vec3(float x, float y, float z) { vec3_t v; v.x=x; v.y=y; v.z=z; return v; }
vec3_t vec3_add(vec3_t a, vec3_t b) { return vec3(a.x+b.x, a.y+b.y, a.z+b.z); }
vec3_t vec3_sub(vec3_t a, vec3_t b) { return vec3(a.x-b.x, a.y-b.y, a.z-b.z); }
vec3_t vec3_mul(vec3_t a, float s) { return vec3(a.x*s, a.y*s, a.z*s); }
vec3_t vec3_div(vec3_t a, float s) { return vec3(a.x/s, a.y/s, a.z/s); }
float  vec3_dot(vec3_t a, vec3_t b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
vec3_t vec3_cross(vec3_t a, vec3_t b) {
    return vec3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}
float  vec3_len(vec3_t a) { return sqrtf(a.x*a.x + a.y*a.y + a.z*a.z); }
vec3_t vec3_norm(vec3_t a) { float l=vec3_len(a); return l>0 ? vec3(a.x/l,a.y/l,a.z/l) : vec3(0,0,0); }

/* ═══════════════════════════════════════════════
 *  Vector 4
 * ═══════════════════════════════════════════════ */
vec4_t vec4(float x, float y, float z, float w) { vec4_t v; v.x=x; v.y=y; v.z=z; v.w=w; return v; }

/* ═══════════════════════════════════════════════
 *  Matrix 4×4
 * ═══════════════════════════════════════════════ */
mat4_t mat4_identity(void) {
    mat4_t m = {{{0}}};
    m.m[0][0]=1; m.m[1][1]=1; m.m[2][2]=1; m.m[3][3]=1;
    return m;
}
mat4_t mat4_mul(mat4_t a, mat4_t b) {
    mat4_t r = {{{0}}};
    for (int i=0;i<4;i++) for (int j=0;j<4;j++)
        for (int k=0;k<4;k++) r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}
vec4_t mat4_mul_v(mat4_t m, vec4_t v) {
    return vec4(
        m.m[0][0]*v.x + m.m[0][1]*v.y + m.m[0][2]*v.z + m.m[0][3]*v.w,
        m.m[1][0]*v.x + m.m[1][1]*v.y + m.m[1][2]*v.z + m.m[1][3]*v.w,
        m.m[2][0]*v.x + m.m[2][1]*v.y + m.m[2][2]*v.z + m.m[2][3]*v.w,
        m.m[3][0]*v.x + m.m[3][1]*v.y + m.m[3][2]*v.z + m.m[3][3]*v.w
    );
}
mat4_t mat4_translate(float x, float y, float z) {
    mat4_t m=mat4_identity();
    m.m[0][3]=x; m.m[1][3]=y; m.m[2][3]=z;
    return m;
}
mat4_t mat4_scale(float x, float y, float z) {
    mat4_t m=mat4_identity();
    m.m[0][0]=x; m.m[1][1]=y; m.m[2][2]=z;
    return m;
}
mat4_t mat4_rotate_x(float rad) {
    mat4_t m=mat4_identity();
    float c=cosf(rad), s=sinf(rad);
    m.m[1][1]=c; m.m[1][2]=-s; m.m[2][1]=s; m.m[2][2]=c;
    return m;
}
mat4_t mat4_rotate_y(float rad) {
    mat4_t m=mat4_identity();
    float c=cosf(rad), s=sinf(rad);
    m.m[0][0]=c; m.m[0][2]=s; m.m[2][0]=-s; m.m[2][2]=c;
    return m;
}
mat4_t mat4_rotate_z(float rad) {
    mat4_t m=mat4_identity();
    float c=cosf(rad), s=sinf(rad);
    m.m[0][0]=c; m.m[0][1]=-s; m.m[1][0]=s; m.m[1][1]=c;
    return m;
}
mat4_t mat4_perspective(float fov_rad, float aspect, float near, float far) {
    mat4_t m = {{{0}}};
    float f = 1.0f / tanf(fov_rad * 0.5f);
    m.m[0][0] = f / aspect;
    m.m[1][1] = f;
    m.m[2][2] = (far + near) / (near - far);
    m.m[2][3] = (2.0f * far * near) / (near - far);
    m.m[3][2] = -1.0f;
    return m;
}
mat4_t mat4_look_at(vec3_t eye, vec3_t target, vec3_t up) {
    vec3_t f = vec3_norm(vec3_sub(target, eye));
    vec3_t s = vec3_norm(vec3_cross(f, up));
    vec3_t u = vec3_cross(s, f);
    mat4_t m = mat4_identity();
    m.m[0][0]=s.x; m.m[0][1]=s.y; m.m[0][2]=s.z;
    m.m[1][0]=u.x; m.m[1][1]=u.y; m.m[1][2]=u.z;
    m.m[2][0]=-f.x; m.m[2][1]=-f.y; m.m[2][2]=-f.z;
    m.m[0][3]=-vec3_dot(s,eye); m.m[1][3]=-vec3_dot(u,eye); m.m[2][3]=vec3_dot(f,eye);
    return m;
}

/* ═══════════════════════════════════════════════
 *  Quaternion
 * ═══════════════════════════════════════════════ */
quat_t quat(float x, float y, float z, float w) { quat_t q; q.x=x; q.y=y; q.z=z; q.w=w; return q; }
quat_t quat_identity(void) { return quat(0,0,0,1); }
quat_t quat_mul(quat_t a, quat_t b) {
    return quat(
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    );
}
quat_t quat_rotate(float rad, float ax, float ay, float az) {
    float len = sqrtf(ax*ax+ay*ay+az*az);
    if (len==0) return quat_identity();
    float s = sinf(rad*0.5f);
    return quat(ax/len*s, ay/len*s, az/len*s, cosf(rad*0.5f));
}
vec3_t quat_apply(quat_t q, vec3_t v) {
    quat_t p = quat(v.x, v.y, v.z, 0);
    quat_t qc = quat(-q.x, -q.y, -q.z, q.w);
    quat_t r = quat_mul(quat_mul(q, p), qc);
    return vec3(r.x, r.y, r.z);
}
mat4_t quat_to_mat4(quat_t q) {
    mat4_t m = mat4_identity();
    float xx=q.x*q.x, yy=q.y*q.y, zz=q.z*q.z;
    float xy=q.x*q.y, xz=q.x*q.z, yz=q.y*q.z;
    float wx=q.w*q.x, wy=q.w*q.y, wz=q.w*q.z;
    m.m[0][0]=1-2*(yy+zz); m.m[0][1]=2*(xy-wz);   m.m[0][2]=2*(xz+wy);
    m.m[1][0]=2*(xy+wz);   m.m[1][1]=1-2*(xx+zz); m.m[1][2]=2*(yz-wx);
    m.m[2][0]=2*(xz-wy);   m.m[2][1]=2*(yz+wx);   m.m[2][2]=1-2*(xx+yy);
    return m;
}

/* ═══════════════════════════════════════════════
 *  Expression Parser (Recursive Descent)
 * ═══════════════════════════════════════════════ */

static const char* expr_src;
static int expr_pos;
static double expr_var_x;
static char expr_err[128];

/* --- Lexer --- */
typedef enum {
    TOK_EOF, TOK_NUM, TOK_X, TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV,
    TOK_POW, TOK_LP, TOK_RP, TOK_COMMA,
    TOK_SIN, TOK_COS, TOK_TAN, TOK_SQRT, TOK_ABS, TOK_LOG, TOK_LN, TOK_EXP
} tok_t;
static tok_t tok_cur;
static double tok_val;

static int is_alpha(char c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z'); }

static void next_tok(void) {
    while (expr_src[expr_pos]==' '||expr_src[expr_pos]=='\t') expr_pos++;
    char c = expr_src[expr_pos];
    
    if (c == 0) { tok_cur = TOK_EOF; return; }
    if (c == '+') { expr_pos++; tok_cur = TOK_PLUS; return; }
    if (c == '-') { expr_pos++; tok_cur = TOK_MINUS; return; }
    if (c == '*') {
        if (expr_src[expr_pos+1] == '*') { expr_pos+=2; tok_cur = TOK_POW; return; }
        expr_pos++; tok_cur = TOK_MUL; return;
    }
    if (c == '/') { expr_pos++; tok_cur = TOK_DIV; return; }
    if (c == '^') { expr_pos++; tok_cur = TOK_POW; return; }
    if (c == '(') { expr_pos++; tok_cur = TOK_LP; return; }
    if (c == ')') { expr_pos++; tok_cur = TOK_RP; return; }
    if (c == ',') { expr_pos++; tok_cur = TOK_COMMA; return; }

    if (c == 'x' || c == 'X') {
        expr_pos++; tok_cur = TOK_X; return;
    }

    if ((c >= '0' && c <= '9') || c == '.') {
        char buf[64]; int bi = 0;
        while (bi < 63 && ((expr_src[expr_pos]>='0'&&expr_src[expr_pos]<='9')||expr_src[expr_pos]=='.'))
            buf[bi++] = expr_src[expr_pos++];
        buf[bi] = 0;
        tok_val = atof(buf);
        tok_cur = TOK_NUM;
        return;
    }

    if (is_alpha(c)) {
        char buf[32]; int bi = 0;
        while (bi < 31 && is_alpha(expr_src[expr_pos]))
            buf[bi++] = expr_src[expr_pos++];
        buf[bi] = 0;
        if      (!strcmp(buf,"sin"))  tok_cur=TOK_SIN;
        else if (!strcmp(buf,"cos"))  tok_cur=TOK_COS;
        else if (!strcmp(buf,"tan"))  tok_cur=TOK_TAN;
        else if (!strcmp(buf,"sqrt")) tok_cur=TOK_SQRT;
        else if (!strcmp(buf,"abs"))  tok_cur=TOK_ABS;
        else if (!strcmp(buf,"log"))  tok_cur=TOK_LOG;
        else if (!strcmp(buf,"ln"))   tok_cur=TOK_LN;
        else if (!strcmp(buf,"exp"))  tok_cur=TOK_EXP;
        else {
            snprintf(expr_err,128,"Unknown function: %s", buf);
            tok_cur=TOK_EOF;
        }
        return;
    }

    snprintf(expr_err,128,"Unexpected character: %c", c);
    tok_cur = TOK_EOF;
}

static double parse_expr(void);

static double parse_primary(void) {
    if (tok_cur == TOK_NUM) { double v=tok_val; next_tok(); return v; }
    if (tok_cur == TOK_X) { next_tok(); return expr_var_x; }
    if (tok_cur == TOK_MINUS) { next_tok(); return -parse_primary(); }
    if (tok_cur == TOK_LP) {
        next_tok(); double v = parse_expr();
        if (tok_cur == TOK_RP) next_tok();
        else snprintf(expr_err,128,"Missing )");
        return v;
    }
    /* Function calls */
    tok_t ft = tok_cur;
    if (ft >= TOK_SIN && ft <= TOK_EXP) {
        next_tok();
        if (tok_cur == TOK_LP) next_tok();
        double arg = parse_expr();
        if (tok_cur == TOK_RP) next_tok();
        switch (ft) {
            case TOK_SIN:  return sin(arg);
            case TOK_COS:  return cos(arg);
            case TOK_TAN:  return tan(arg);
            case TOK_SQRT: return arg<0 ? (snprintf(expr_err,128,"sqrt of negative"),0.0) : sqrt(arg);
            case TOK_ABS:  return fabs(arg);
            case TOK_LOG:  return arg<=0 ? (snprintf(expr_err,128,"log of <=0"),0.0) : log10(arg);
            case TOK_LN:   return arg<=0 ? (snprintf(expr_err,128,"ln of <=0"),0.0) : log(arg);
            case TOK_EXP:  return exp(arg);
            default: return 0;
        }
    }
    snprintf(expr_err,128,"Unexpected token");
    return 0;
}

static double parse_pow(void) {
    double v = parse_primary();
    if (tok_cur == TOK_POW) { next_tok(); double e = parse_pow(); v = pow(v,e); }
    return v;
}

static double parse_mul(void) {
    double v = parse_pow();
    while (tok_cur == TOK_MUL || tok_cur == TOK_DIV) {
        tok_t op = tok_cur; next_tok();
        double rhs = parse_pow();
        if (op == TOK_MUL) v *= rhs; else v /= rhs;
    }
    return v;
}

static double parse_expr(void) {
    double v = parse_mul();
    while (tok_cur == TOK_PLUS || tok_cur == TOK_MINUS) {
        tok_t op = tok_cur; next_tok();
        double rhs = parse_mul();
        if (op == TOK_PLUS) v += rhs; else v -= rhs;
    }
    return v;
}

double expr_eval(const char* expr, double x) {
    expr_err[0] = 0;
    expr_src = expr;
    expr_pos = 0;
    expr_var_x = x;
    next_tok();
    double r = parse_expr();
    if (expr_err[0]) return 0.0 / 0.0;
    return r;
}

const char* expr_error(void) {
    return expr_err[0] ? expr_err : NULL;
}

/* ═══════════════════════════════════════════════
 *  Numerical Methods
 * ═══════════════════════════════════════════════ */
double newton(double (*f)(double), double (*fp)(double), double guess, int max_iter, double tol) {
    double x = guess;
    for (int i = 0; i < max_iter; i++) {
        double fx = f(x);
        if (fabs(fx) < tol) return x;
        double fpx = fp ? fp(x) : deriv(f, x);
        if (fabs(fpx) < 1e-15) break;
        x -= fx / fpx;
    }
    return x;
}

double deriv(double (*f)(double), double x) {
    double h = 1e-8;
    return (f(x + h) - f(x - h)) / (2.0 * h);
}

double integrate(double (*f)(double), double a, double b, int n) {
    if (n % 2) n++;
    double h = (b - a) / n;
    double s = f(a) + f(b);
    for (int i = 1; i < n; i++) {
        double x = a + i * h;
        s += (i % 2) ? 4.0 * f(x) : 2.0 * f(x);
    }
    return s * h / 3.0;
}
