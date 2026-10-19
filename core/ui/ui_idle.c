/* SPDX-License-Identifier: GPL-3.0-only */
/* Screen 1, idle (P1.3n): FIZZ, the logo screen. Open, no vessel: the drink is only its surface, a thin wavy line across
 * the screen, with fine bubbles rising to it, in thin grey lines on black (no colour, no fills).
 *   the surface  a thin line, gently undulating, rising into view from below and settling
 *   bubbles      small thin circles (a few px, a few a little bigger) rising and wobbling; at the surface each pops: a
 *                small ring opening, two short lines, now and then a fine droplet thrown up
 *   the fizz     the loop's peak: many more bubbles at once, faster, the surface lively, fine spray, and "FIZZ" (thin
 *                Inter Tight) rising out of it, staying through the settle
 * The loop (FZ_LOOP s): rise, fizz, settle, and everything fades so it restarts seamlessly.
 * Playing (a DAW's START while idle): the bubbles quicken, the surface lifts, and a little spray pops on each beat, more
 * on the bar's downbeat. Stateless and deterministic (seeded hashes of the clock and the beats). No footer. */
#include <math.h>

#include "ui.h"

#define FZ_LOOP 9.4f
#define CX 120.f
#define LEVEL_Y 168.f                                   /* the surface's resting height */
#define C_LINE fade(0xe8e8e8u)                          /* pure greys: no tint */
#define C_SOFT fade(0xa0a0a0u)
#define C_LOGO 0xf2f2f2u
#define NBUB 64

/* the loop's fade: the lines dim toward black */
static float g_fade = 1.f;
static qd_rgb fade(qd_rgb c)
{
    const float f = g_fade;
    return (qd_rgb)((uint32_t)((float)(c >> 16 & 0xffu) * f) << 16 | (uint32_t)((float)(c >> 8 & 0xffu) * f) << 8 |
                    (uint32_t)((float)(c & 0xffu) * f));
}

static float hash01(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return (float)(h >> 8) / 16777216.f;
}
static float clamp01(float x) { return x < 0.f ? 0.f : x > 1.f ? 1.f : x; }
static float seg(float t, float a, float b) { return clamp01((t - a) / (b - a)); }
static float bell(float t, float a, float b) { return sinf(UI_PI * seg(t, a, b)); }   /* 0 -> 1 -> 0 over a..b */

static void line2(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, col, lw, 0, QD_NONE);
}

typedef struct {
    float level;                                        /* the surface's mean y */
    float fill;                                         /* 0..1: the rise */
    float fizz;                                         /* 0..1..0: the peak */
    float alpha;                                        /* the loop's fade */
} fz_state_t;

static fz_state_t state_at(float u)
{
    fz_state_t s;
    s.fill = ui_ease(seg(u, 0.4f, 3.4f));
    s.fizz = bell(u, 4.2f, 7.4f);
    s.level = 252.f - s.fill * (252.f - LEVEL_Y) - 10.f * s.fizz;
    s.alpha = 1.f - seg(u, 8.4f, 9.2f);
    return s;
}

/* the surface's y at x: a gentle swell, livelier in the fizz */
static float surface_y(const fz_state_t *s, float x, float t, float lift)
{
    const float a = 1.f + 2.2f * s->fizz + 1.2f * lift;
    return s->level - 4.f * lift + a * (2.f * sinf(x * 0.035f + t * 1.6f) + 1.1f * sinf(x * 0.11f - t * 2.7f));
}

static void draw_surface(const fz_state_t *s, float t, float lift)
{
    float pts[2 * 49];
    int k;
    if (s->level > 246.f)
        return;
    for (k = 0; k <= 48; k++) {
        const float x = -2.f + 244.f * (float)k / 48.f;
        pts[2 * k] = x;
        pts[2 * k + 1] = surface_y(s, x, t, lift);
    }
    ui_P(pts, 49, C_LINE, 1.3f, 0, QD_NONE);
}

/* a fine droplet thrown up from (x0, y0): a tiny outlined circle on a short arc */
static void droplet(uint32_t seed, uint32_t j, float age, float x0, float y0)
{
    const float life = 0.9f, vx = (hash01(seed, j, 1) - 0.5f) * 60.f, vy = 60.f + 90.f * hash01(seed, j, 2);
    const float x = x0 + vx * age, y = y0 - vy * age + 160.f * age * age;
    if (age < 0.f || age > life || y > y0 + 2.f)
        return;
    qd_save();
    qd_alpha(1.f - age / life);
    ui_O(x, y, 0.8f + 0.7f * hash01(seed, j, 3), C_LINE, 0.9f, QD_NONE);
    qd_restore();
}

/* the bubbles: each rises from below at its own speed, wobbling, pops at the surface, and starts again elsewhere */
static void draw_bubbles(const fz_state_t *s, float t, float lift)
{
    const int n = (int)(14.f + (float)(NBUB - 14) * s->fizz + 10.f * lift);
    int k;
    if (s->fill <= 0.02f)
        return;
    for (k = 0; k < n && k < NBUB; k++) {
        const float big = hash01((uint32_t)k, 4, 1) > 0.85f ? 1.f : 0.f;
        const float r = (1.f + 1.2f * hash01((uint32_t)k, 5, 1)) * (1.f + 0.9f * big);
        const float sp = (24.f + 30.f * hash01((uint32_t)k, 2, 1) - 8.f * big) * (1.f + 1.4f * s->fizz + 0.8f * lift);
        const float rise = 250.f - (s->level - 6.f), pop = 0.35f, period = rise / sp + pop;
        const float ph = t + period * hash01((uint32_t)k, 3, 1);
        const uint32_t cyc = (uint32_t)(ph / period);
        const float a = ph - (float)cyc * period, x = 8.f + 224.f * hash01((uint32_t)k, cyc, 7);
        if (rise < 8.f)
            continue;
        if (a * sp < rise - r) {                        /* rising, wobbling, growing a little */
            const float y = 250.f - a * sp, g = 0.7f + 0.3f * (a * sp / rise);
            const float wx = x + 2.f * sinf(t * 5.f + (float)k * 1.7f) * (0.4f + 0.6f * (a * sp / rise));
            if (y < surface_y(s, wx, t, lift) + r + 1.f)
                continue;
            ui_O(wx, y, r * g, k % 3 ? C_SOFT : C_LINE, 0.9f, QD_NONE);
        } else {                                        /* popping at the surface */
            const float age = a - (rise - r) / sp, f = clamp01(age / pop);
            const float sy = surface_y(s, x, t, lift);
            qd_save();
            qd_alpha(1.f - f);
            ui_O(x, sy - 1.f, r + 4.f * f, C_LINE, 0.8f, QD_NONE);   /* the ring opening */
            line2(x - r - 1.f - 3.f * f, sy - 2.f - 3.f * f, x - r - 2.f - 5.f * f, sy - 4.f - 6.f * f, C_LINE, 0.9f);
            line2(x + r + 1.f + 3.f * f, sy - 2.f - 3.f * f, x + r + 2.f + 5.f * f, sy - 4.f - 6.f * f, C_LINE, 0.9f);
            qd_restore();
            if (hash01((uint32_t)k, cyc, 8) < 0.25f + 0.5f * s->fizz)   /* the odd fine droplet */
                droplet((uint32_t)k * 131u + cyc, 0u, age, x, sy - 2.f);
        }
    }
}

void ui_idle_draw(const ui_idle_t *v)
{
    const float t = v->t, u = fmodf(t, FZ_LOOP), bpm = (float)(v->bpm > 0 ? v->bpm : 120);
    const fz_state_t s = state_at(u);
    float lift = 0.f;
    int k;

    if (v->playing) {                                   /* the beat's pulse: 1 at the beat, fading over a beat */
        const double fr = v->beats - floor(v->beats);
        lift = (float)(1.0 - fr) * ((long)floor(v->beats) % 4 == 0 ? 1.f : 0.55f);
    }
    g_fade = s.alpha;
    draw_surface(&s, t, lift);
    draw_bubbles(&s, t, lift);
    for (k = 0; k < 26; k++) {                          /* the fizz's spray: fine droplets off the surface */
        const float x = 20.f + 200.f * hash01(9u, (uint32_t)k, 4);
        droplet(9u, (uint32_t)k, u - 4.6f - 2.0f * hash01(9u, (uint32_t)k, 5), x, surface_y(&s, x, t, lift));
    }
    if (v->playing && s.level < 246.f) {               /* the beats' spray */
        const long last = (long)floor(v->beats);
        long b;
        for (b = last - 1; b <= last; b++) {
            const int m = b % 4 == 0 ? 12 : 5;
            const float age = (float)(v->beats - (double)b) * 60.f / bpm;
            for (k = 0; k < m; k++) {
                const uint32_t seed = (uint32_t)b * 2654435761u + 11u;
                const float x = 30.f + 180.f * hash01(seed, (uint32_t)k, 6);
                droplet(seed, (uint32_t)k, age - 0.02f * (float)k, x, surface_y(&s, x, t, lift));
            }
        }
    }
    {   /* "FIZZ" rises out of the fizz and stays through the settle */
        const float r = ui_ease(seg(u, 4.9f, 6.3f));
        if (r > 0.f) {
            qd_save();
            qd_alpha(r * s.alpha);
            ui_T("FIZZ", CX, 112.f - 18.f * r, 46.f, 200, 7.f, C_LOGO, QD_CENTER);
            qd_restore();
        }
    }
    g_fade = 1.f;
}
