#include "math_engine.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    /* Vector tests */
    vec3_t a = vec3(1,2,3), b = vec3(4,5,6);
    vec3_t c = vec3_cross(a, b);
    printf("Cross (1,2,3)x(4,5,6) = (%g,%g,%g)\n", c.x, c.y, c.z);

    /* Matrix tests */
    mat4_t rot = mat4_rotate_y(3.14159 / 4);
    vec4_t p = vec4(1,0,0,1);
    vec4_t r = mat4_mul_v(rot, p);
    printf("Rotate (1,0,0) by 45deg Y = (%g,%g,%g,%g)\n", r.x, r.y, r.z, r.w);

    /* Expression tests */
    printf("sin(x) at x=0: %g\n", expr_eval("sin(x)", 0));
    printf("x^2+2*x+1 at x=3: %g\n", expr_eval("x^2+2*x+1", 3));
    printf("sqrt(16): %g\n", expr_eval("sqrt(16)", 0));
    printf("log(100): %g\n", expr_eval("log(100)", 0));

    /* Newton's method: solve x^2 - 4 = 0 */
    double f_sq(double x) { return x*x - 4; }
    double fp_sq(double x) { return 2*x; }
    double root = newton(f_sq, fp_sq, 3, 100, 1e-10);
    printf("Newton guess 3 for x^2-4: %g (expected ~2)\n", root);

    /* Integrate sin(x) from 0 to pi */
    double f_sin(double x) { return sin(x); }
    double integral = integrate(f_sin, 0, 3.14159, 100);
    printf("Integral sin(x) 0..pi: %g (expected ~2)\n", integral);

    return 0;
}
