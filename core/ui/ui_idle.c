/* SPDX-License-Identifier: GPL-3.0-only */
/* Screen 1, idle (P1.3o): FIZZ, the logo screen. A submerged close-up of champagne bubbles (the owner's reference,
 * ReferenceImages/FizzyBubbles/Champagne01), drawn grey on black in an outlined, illustrative style. Depth in layers:
 *   far     bokeh: soft dim grey discs, out of focus, rising slowly
 *   specks  fine dots everywhere
 *   middle  many small crisp thin circles; now and then a pair touching
 *   bed     a loose band of small bubbles at the bottom, forming (growing) and leaving, feeding the rise
 *   near    a few large bubbles: black inside, a thin rim, a bright arc on the light's side (upper left), a faint
 *           reflection opposite, and tiny bubbles inside; the bigger the faster
 * Everything is darker toward the top. "FIZZ" (thin Inter Tight) is centred, with near bubbles passing behind it and in
 * front of it (black inside, so the overlap reads).
 * The loop (FZ_LOOP s, seamless: every motion makes a whole number of cycles per loop): a calm, steady rise, and once a
 * loop a gentle swell of bubbles from the bed, "the fizz", which brings FIZZ in; FIZZ fades before the loop ends.
 * Playing (a DAW's START while idle): a soft surge of bubbles from the bed on each beat, more on the bar's downbeat.
 * Stateless and deterministic (seeded hashes of the loop time and the beats); about 150 shapes. No footer. */
#include <math.h>

#include "ui.h"

#define FZ_LOOP 11.f
#define CX 120.f
#define BED_Y 226.f                                     /* the bed's middle */
#define N_BOKEH 14
#define N_SPECK 56
#define N_MID 34
#define N_BED 26
#define N_NEAR 7
#define N_SWELL 26

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

/* a grey of level v (0..255), darker toward the top of the screen; on the panel's 5-bit red / blue steps, so the dim
 * greys do not pick up a green or purple tint from the 6-bit green */
static qd_rgb grey_at(float v, float y)
{
    const float f = 0.45f + 0.55f * clamp01(y / 240.f);
    const uint32_t s5 = (uint32_t)(clamp01(v * f / 255.f) * 31.f + 0.5f), g = s5 * 255u / 31u;
    return (qd_rgb)(g << 16 | g << 8 | g);
}

/* a rise: y from the bottom (below the screen by pad) to the top (above it by pad), cycles whole rises per loop */
static float rise_y(float u, float phase, int cycles, float pad)
{
    const float span = 240.f + 2.f * pad, a = phase + (float)cycles * u / FZ_LOOP;
    return 240.f + pad - span * (a - floorf(a));
}
/* a sideways drift: whole cycles per loop */
static float drift(float u, uint32_t k, uint32_t layer, float amp)
{
    return amp * sinf(UI_TAU * (u / FZ_LOOP * (float)(1 + (int)(hash01(k, layer, 31) * 2.f)) + hash01(k, layer, 32)));
}

/* a crisp small bubble: a thin circle, a speck of light on the top left when it is big enough */
static void small_bubble(float x, float y, float r, float v)
{
    ui_O(x, y, r, grey_at(v, y), 0.9f, QD_NONE);
    if (r > 2.6f)
        qd_arc(x, y, r - 0.9f, UI_PI * 1.1f, UI_PI * 1.4f, grey_at(v + 40.f, y), 0.9f);
}

/* a near bubble: black inside, a thin rim, a bright arc where the light comes from, a faint one opposite, and tiny
 * bubbles inside (more toward its bottom) */
static void near_bubble(float x, float y, float r, uint32_t k)
{
    int j;
    const int n = 5 + (int)(r * 0.6f);
    ui_O(x, y, r, grey_at(150.f, y), 1.f, 0x000000u);
    qd_arc(x, y, r - 1.2f, UI_PI * 1.02f, UI_PI * 1.48f, grey_at(250.f, y), 1.8f);
    qd_arc(x, y, r - 1.f, UI_PI * 0.12f, UI_PI * 0.38f, grey_at(150.f, y), 1.1f);
    for (j = 0; j < n; j++) {
        const float a = UI_TAU * hash01(k, (uint32_t)j, 41), d = r * 0.74f * sqrtf(hash01(k, (uint32_t)j, 42));
        const float bx = x + d * cosf(a), by = y + r * 0.12f + 0.75f * d * sinf(a);   /* lower: centred below the middle */
        ui_O(bx, by, 0.7f + 1.1f * hash01(k, (uint32_t)j, 43) * (r / 20.f), grey_at(185.f, by), 0.8f, QD_NONE);
    }
}

static void draw_near(float u, int front)
{
    int k;
    for (k = 0; k < N_NEAR; k++) {
        const uint32_t q = (uint32_t)k;
        const float r = 9.f + 13.f * hash01(q, 1, 5);
        const int cyc = r > 16.f ? 3 : 2;               /* the bigger, the faster */
        const float y = rise_y(u, hash01(q, 2, 5), cyc, r + 4.f);
        const float x = 24.f + 192.f * ((float)k + hash01(q, 3, 5)) / (float)N_NEAR + drift(u, q, 5, 6.f);
        if ((k & 1) != front)
            continue;
        near_bubble(x, y, r, q);
    }
}

void ui_idle_draw(const ui_idle_t *v)
{
    const float t = v->t, u = fmodf(t, FZ_LOOP), bpm = (float)(v->bpm > 0 ? v->bpm : 120);
    const float swell = bell(u, 2.6f, 7.0f);
    int k;

    for (k = 0; k < N_BOKEH; k++) {                     /* far: soft dim discs */
        const uint32_t q = (uint32_t)k;
        const float r = 6.f + 12.f * hash01(q, 1, 1), y = rise_y(u, hash01(q, 2, 1), 1, r);
        const float x = 240.f * hash01(q, 3, 1) + drift(u, q, 1, 4.f);
        const float g = 26.f + 22.f * hash01(q, 4, 1);
        ui_O(x, y, r, QD_NONE, 0.f, grey_at(g * 0.7f, y));
        ui_O(x, y, r * 0.72f, QD_NONE, 0.f, grey_at(g, y));
    }
    for (k = 0; k < N_SPECK; k++) {                     /* specks */
        const uint32_t q = (uint32_t)k;
        const float y = rise_y(u, hash01(q, 2, 2), 1 + (k % 3 == 0), 4.f);
        const float x = 240.f * hash01(q, 3, 2) + drift(u, q, 2, 3.f);
        ui_O(x, y, 0.6f + 0.5f * hash01(q, 4, 2), QD_NONE, 0.f, grey_at(110.f + 90.f * hash01(q, 5, 2), y));
    }
    for (k = 0; k < N_MID; k++) {                       /* middle: crisp small bubbles, now and then a pair */
        const uint32_t q = (uint32_t)k;
        const float r = 1.4f + 2.6f * hash01(q, 1, 3), y = rise_y(u, hash01(q, 2, 3), 1 + (r > 3.f), 8.f);
        const float x = 240.f * hash01(q, 3, 3) + drift(u, q, 3, 5.f);
        small_bubble(x, y, r, 200.f);
        if (k % 9 == 4)
            small_bubble(x + 0.35f * r, y - r - 0.75f * r, 0.75f * r, 200.f);
    }
    for (k = 0; k < N_BED; k++) {                       /* the bed: bubbles forming, then leaving upward */
        const uint32_t q = (uint32_t)k;
        const float per = FZ_LOOP / (float)(4 + (int)(hash01(q, 1, 4) * 4.f)), a = fmodf(u + per * hash01(q, 2, 4), per);
        const float f = a / per, r = (1.2f + 2.2f * hash01(q, 3, 4)) * clamp01(f * 2.5f);
        const float x = 240.f * ((float)k + hash01(q, 4, 4)) / (float)N_BED;
        const float y = BED_Y + 12.f * (hash01(q, 5, 4) - 0.5f) - (f > 0.7f ? 60.f * (f - 0.7f) * (f - 0.7f) * 10.f : 0.f);
        if (r > 0.3f)
            small_bubble(x, y, r * (1.f + 0.3f * swell),
                         (170.f + 50.f * swell) * (f > 0.7f ? 1.f - (f - 0.7f) / 0.3f : 1.f) + 10.f);
    }
    for (k = 0; k < N_SWELL; k++) {                     /* the fizz: a swell from the bed once a loop */
        const uint32_t q = (uint32_t)k;
        const float born = 2.8f + 2.4f * hash01(q, 1, 6), age = u - born;
        const float r = 1.6f + (k % 6 == 0 ? 6.f : 3.f) * hash01(q, 2, 6), sp = 62.f + 34.f * hash01(q, 3, 6);
        const float y = BED_Y - sp * age, x = 16.f + 208.f * hash01(q, 4, 6) + 4.f * sinf(age * 2.f + (float)k);
        if (age < 0.f || y < -10.f)
            continue;
        small_bubble(x, y, r * clamp01(age * 3.f), 215.f);
    }
    if (v->playing) {                                   /* the beats: a soft surge from the bed on each */
        const long last = (long)floor(v->beats);
        long b;
        for (b = last - 9; b <= last; b++) {
            const uint32_t s = (uint32_t)b * 2654435761u + 17u;
            const int m = b % 4 == 0 ? 7 : 3;
            const float age = (float)(v->beats - (double)b) * 60.f / bpm;
            for (k = 0; k < m; k++) {
                const float r = 1.5f + 2.5f * hash01(s, (uint32_t)k, 1), sp = 60.f + 40.f * hash01(s, (uint32_t)k, 2);
                const float y = BED_Y - sp * age, x = 12.f + 216.f * hash01(s, (uint32_t)k, 3);
                if (b < 0 || y < -10.f)
                    continue;
                small_bubble(x + 3.f * sinf(age * 2.5f + (float)k), y, r * clamp01(age * 4.f), 225.f);
            }
        }
    }
    draw_near(u, 0);                                    /* the near ones behind the logo */
    {   /* "FIZZ": in with the swell, out before the loop ends */
        const float r = ui_ease(seg(u, 4.0f, 5.6f)) * (1.f - ui_ease(seg(u, 9.4f, 10.7f)));
        if (r > 0.f) {
            qd_save();
            qd_alpha(r);
            ui_T("FIZZ", CX, 126.f - 8.f * r, 52.f, 200, 8.f, 0xf2f2f2u, QD_CENTER);
            qd_restore();
        }
    }
    draw_near(u, 1);                                    /* .. and in front of it */
}
