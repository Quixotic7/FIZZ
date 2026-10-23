/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
 * not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* FIZZ voice DSP primitives: fixed-point helpers, oscillators, soft clipping, the state-variable filter, a DC
 * blocker and an integer square root. Integer arithmetic only; signed right shifts are arithmetic (floor). */

#ifndef FZ_DSP_H
#define FZ_DSP_H

#include <stdint.h>

#include "fz_tables.h"

#define CTL_LOG2 5
#if (1 << CTL_LOG2) != CTL
#error "CTL_LOG2 does not match CTL"
#endif

/* nominal output level of one voice at full scale */
#define VOICE_FS 24000

static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/* Q15 multiply, the product kept in 32 bits */
static inline int32_t mulq15(int32_t a, int32_t b)
{
    return (a * b) >> 15;
}

/* sine of a 32-bit phase: top 10 bits pick the table step, the next 15 bits interpolate */
static inline int32_t sine_i(uint32_t ph)
{
    uint32_t idx = ph >> 22;
    int32_t lo = SINE[idx];
    int32_t hi = SINE[(idx + 1u) & 1023u];
    int32_t frac = (int32_t)((ph >> 7) & 0x7FFFu);
    return lo + (((hi - lo) * frac) >> 15);
}

static inline int32_t osc_sine(uint32_t ph)
{
    return sine_i(ph);
}

/* triangle from the top 17 bits of the phase: -32768 at 0, up to 32767 at half, back down */
static inline int32_t osc_tri(uint32_t ph)
{
    int32_t s = (int32_t)(ph >> 15);
    return s < 65536 ? s - 32768 : 98303 - s;
}

/* odd tanh-shaped saturator: |x| in Q15 / 128 steps along TANH_Q15, flat beyond the table's end */
static inline int32_t softclip(int32_t x)
{
    int32_t mag, out, idx;
    /* |INT32_MIN| is not representable; any magnitude past the table end gives the same result */
    if (x < 0)
        mag = (x == INT32_MIN) ? INT32_MAX : -x;
    else
        mag = x;
    idx = mag >> 8;
    if (idx >= 256) {
        out = TANH_Q15[256];
    } else {
        int32_t t0 = TANH_Q15[idx];
        out = t0 + (((TANH_Q15[idx + 1] - t0) * (mag & 255)) >> 8);
    }
    return x < 0 ? -out : out;
}

/* linear below the knee k, saturating above it */
static inline int32_t soft_knee(int32_t y, int32_t k)
{
    int32_t mag = y < 0 ? (y == INT32_MIN ? INT32_MAX : -y) : y;
    int32_t out;
    if (mag <= k)
        return y;
    out = k + (softclip((mag - k) * 2) >> 1);
    return y < 0 ? -out : out;
}

/* trapezoidal state-variable filter coefficients, Q13 (a1) and Q13-derived (a2, a3) */
typedef struct {
    int32_t a1, a2, a3;
} tsvf_t;

/* cut: 0..127 << 8 (an index into SVF_G with an 8-bit fraction); k: the damping in Q12 */
static inline void tsvf_coef_k(tsvf_t *c, int32_t cut, int32_t k)
{
    int32_t idx, g, den;
    cut = clamp(cut, 0, 127 << 8);
    idx = cut >> 8;
    g = SVF_G[idx];
    if (idx < 127)
        g += ((SVF_G[idx + 1] - g) * (cut & 255)) >> 8;
    den = 4096 + ((g * (g + k)) >> 12);
    c->a1 = (int32_t)(33554432u / (uint32_t)den);
    c->a2 = (c->a1 * g) >> 12;
    c->a3 = (c->a2 * g) >> 12;
}

/* the same coefficients from g kept in Q20; a2 carries 5 more fractional bits in f2 (0..31) */
static inline void fz_svf_coef_fine(int32_t cut, int32_t k, int32_t *a1, int32_t *a2, int32_t *a3, int32_t *f2)
{
    int32_t idx, g, den, c1;
    int64_t big2, big3;
    cut = clamp(cut, 0, 127 << 8);
    idx = cut >> 8;
    g = (int32_t)SVF_G[idx] << 8;
    if (idx < 127)
        g += ((int32_t)SVF_G[idx + 1] - (int32_t)SVF_G[idx]) * (cut & 255);
    den = 4096 + (int32_t)(((int64_t)g * (g + ((int64_t)k << 8))) >> 28);
    c1 = (int32_t)((4096u << 13) / (uint32_t)den);
    big2 = ((int64_t)c1 * g) >> 15;
    big3 = (big2 * g) >> 20;
    *a1 = c1;
    *a2 = (int32_t)(big2 >> 5);
    *f2 = (int32_t)(big2 & 31);
    *a3 = (int32_t)(big3 >> 5);
}

/* one sample of the filter; s1, s2 are the integrator states, v1 the band-pass and v2 the low-pass output */
static inline void fz_svf_tick(int32_t a1, int32_t a2, int32_t a3, int32_t f2, int32_t *s1, int32_t *s2, int32_t x,
                               int32_t *v1, int32_t *v2)
{
    int32_t z1 = *s1, z2 = *s2;
    int32_t v3 = x - z2;
    int32_t band = (a1 * z1 + a2 * v3 + ((f2 * v3) >> 5) + 4096) >> 13;
    int32_t low = z2 + ((a2 * z1 + a3 * v3 + ((f2 * z1) >> 5) + 4096) >> 13);
    *s1 = clamp(2 * band - z1, -150000, 150000);
    *s2 = clamp(2 * low - z2, -150000, 150000);
    *v1 = band;
    *v2 = low;
}

/* DC blocker: a one-pole average of x (Q6, time constant 4096 samples) with its rounding error fed back */
typedef struct {
    int32_t acc, rem;
} fz_dc_t;

static inline int32_t fz_dc_step(fz_dc_t *s, int32_t x)
{
    int32_t e = x * 64 - s->acc + s->rem;
    int32_t d = e >> 12;
    s->rem = e - d * 4096;
    s->acc += d;
    return x - ((s->acc + 32) >> 6);
}

/* floor(sqrt(v)) for any 32-bit v, bit by bit */
static inline uint32_t fz_isqrt(uint32_t v)
{
    uint32_t root = 0;
    uint32_t bit = 1u << 30;
    while (bit > v)
        bit >>= 2;
    while (bit != 0) {
        if (v >= root + bit) {
            v -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

/* master level curve of a 10-bit pot */
static inline uint32_t fz_master_of_pot(uint32_t k)
{
    return (k * k) >> 8;
}

#endif /* FZ_DSP_H */
