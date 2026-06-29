#ifndef MATH_ENGINE_H
#define MATH_ENGINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── 2D Vector ─── */
typedef struct { float x, y; } vec2_t;
vec2_t vec2(float x, float y);
vec2_t vec2_add(vec2_t a, vec2_t b);
vec2_t vec2_sub(vec2_t a, vec2_t b);
vec2_t vec2_mul(vec2_t a, float s);
vec2_t vec2_div(vec2_t a, float s);
float  vec2_dot(vec2_t a, vec2_t b);
float  vec2_len(vec2_t a);
vec2_t vec2_norm(vec2_t a);

/* ─── 3D Vector ─── */
typedef struct { float x, y, z; } vec3_t;
vec3_t vec3(float x, float y, float z);
vec3_t vec3_add(vec3_t a, vec3_t b);
vec3_t vec3_sub(vec3_t a, vec3_t b);
vec3_t vec3_mul(vec3_t a, float s);
vec3_t vec3_div(vec3_t a, float s);
float  vec3_dot(vec3_t a, vec3_t b);
vec3_t vec3_cross(vec3_t a, vec3_t b);
float  vec3_len(vec3_t a);
vec3_t vec3_norm(vec3_t a);

/* ─── 4D Vector ─── */
typedef struct { float x, y, z, w; } vec4_t;
vec4_t vec4(float x, float y, float z, float w);

/* ─── 4×4 Matrix ─── */
typedef struct { float m[4][4]; } mat4_t;
mat4_t mat4_identity(void);
mat4_t mat4_mul(mat4_t a, mat4_t b);
vec4_t mat4_mul_v(mat4_t m, vec4_t v);
mat4_t mat4_translate(float x, float y, float z);
mat4_t mat4_scale(float x, float y, float z);
mat4_t mat4_rotate_x(float rad);
mat4_t mat4_rotate_y(float rad);
mat4_t mat4_rotate_z(float rad);
mat4_t mat4_perspective(float fov_rad, float aspect, float near, float far);
mat4_t mat4_look_at(vec3_t eye, vec3_t target, vec3_t up);

/* ─── Quaternion ─── */
typedef struct { float x, y, z, w; } quat_t;
quat_t quat(float x, float y, float z, float w);
quat_t quat_identity(void);
quat_t quat_mul(quat_t a, quat_t b);
quat_t quat_rotate(float rad, float ax, float ay, float az);
vec3_t quat_apply(quat_t q, vec3_t v);
mat4_t quat_to_mat4(quat_t q);

/* ─── Expression Parser ─── */
/* Evaluates a mathematical expression in x.
   Supports: + - * / ^ ( ) sin cos tan sqrt abs log ln exp
   Variables: x
   Returns the result, or NAN on error. */
double expr_eval(const char* expr, double x);
/* Returns error message if last eval failed, else NULL. */
const char* expr_error(void);

/* ─── Numerical ─── */
/* Solve f(x)=0 using Newton's method starting at guess. */
double newton(double (*f)(double), double (*fp)(double), double guess, int max_iter, double tol);
/* Numeric derivative of f at x. */
double deriv(double (*f)(double), double x);
/* Approximate integral of f from a to b using Simpson's rule (n steps, n even). */
double integrate(double (*f)(double), double a, double b, int n);

#ifdef __cplusplus
}
#endif

#endif /* MATH_ENGINE_H */
