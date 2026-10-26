/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The drawing's own sin, cos, exp, log, pow, tanh, hypot, fmod and atan2 (2026-10-26, the flash diet): small
 * approximations (a thousandth or better, plenty for pixels and animation) in ui_kit.c, in place of the C library's,
 * whose sinf / cosf alone carry 2.7 KB of argument reduction and powf 1.3 KB. Include it AFTER <math.h> in the units
 * that draw (the ui_*.c units, view.c, ctl_leds.c, qd_gfx_fast.c, the port's app.c): the names below then mean ours.
 * The sound never uses these (the voice and the effects have their tables and integer paths). */
#ifndef FZ_MATH_H
#define FZ_MATH_H

float fz_sinf(float x);
float fz_cosf(float x);
float fz_expf(float x);
float fz_logf(float x);
float fz_powf(float x, float y);                        /* x > 0 (x <= 0: 0) */
float fz_tanhf(float x);
float fz_hypotf(float x, float y);
float fz_fmodf(float x, float y);
float fz_atan2f(float y, float x);

#undef sinf
#undef cosf
#undef expf
#undef logf
#undef powf
#undef tanhf
#undef hypotf
#undef fmodf
#undef atan2f
#define sinf fz_sinf
#define cosf fz_cosf
#define expf fz_expf
#define logf fz_logf
#define powf fz_powf
#define tanhf fz_tanhf
#define hypotf fz_hypotf
#define fmodf fz_fmodf
#define atan2f fz_atan2f

#ifdef FZ_MATH_IMPL                     /* the definitions: ui_kit.c (and a test that links no ui_kit) */
#include <stdint.h>
#include <math.h>
/* sin on [-pi, pi] after a reduction: a parabola and its refinement (error under 0.001); the rest from exp2 / log2 by
 * the float's bits and short polynomials (relative error under 0.001). Never for the sound. */
float fz_sinf(float x)
{
    float y;
    x = x - 6.2831853f * floorf((x + 3.1415927f) * 0.15915494f);   /* -pi .. pi */
    y = 1.2732395f * x - 0.4052847f * x * fabsf(x);
    return 0.225f * (y * fabsf(y) - y) + y;
}
float fz_cosf(float x) { return fz_sinf(x + 1.5707963f); }
float fz_expf(float x)
{
    union { float f; uint32_t u; } r;
    float n, f, p;
    x = x < -80.f ? -80.f : x > 80.f ? 80.f : x;
    x *= 1.442695f;                                     /* 2^x now */
    n = floorf(x);
    f = x - n;
    p = 1.f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * 0.0096181f)));
    r.u = (uint32_t)((int32_t)n + 127) << 23;
    return r.f * p;
}
float fz_logf(float x)
{
    union { float f; uint32_t u; } r;
    float m, l2;
    int e;
    if (!(x > 0.f))
        return -80.f;
    r.f = x;
    e = (int)((r.u >> 23) & 255u) - 127;
    r.u = (r.u & 0x7fffffu) | 0x3f800000u;              /* the mantissa as 1 .. 2 */
    m = r.f;
    l2 = -1.7417939f + m * (2.8212026f + m * (-1.4699568f + m * (0.44717955f - m * 0.056570851f)));   /* log2(m) */
    return ((float)e + l2) * 0.6931472f;
}
float fz_powf(float x, float y) { return x > 0.f ? fz_expf(y * fz_logf(x)) : 0.f; }
float fz_tanhf(float x)
{
    float e;
    if (x > 5.f)
        return 1.f;
    if (x < -5.f)
        return -1.f;
    e = fz_expf(2.f * x);
    return (e - 1.f) / (e + 1.f);
}
float fz_hypotf(float x, float y) { return sqrtf(x * x + y * y); }
float fz_fmodf(float x, float y)                        /* (the sign of x, as C's; |x / y| under 2^31) */
{
    return y == 0.f ? 0.f : x - (float)(int32_t)(x / y) * y;
}
float fz_atan2f(float y, float x)
{
    const float ax = fabsf(x), ay = fabsf(y);
    const float a = (ax > ay ? ay : ax) / ((ax > ay ? ax : ay) + 1e-20f), s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax)
        r = 1.5707963f - r;
    if (x < 0.f)
        r = 3.1415927f - r;
    return y < 0.f ? -r : r;
}
#endif

#endif
