/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* Screen 1, idle (P1.3p): FIZZ, the logo screen, as mockups/fizz-champagne-particles.html. P1.3o's champagne close-up
 * (after the owner's reference, ReferenceImages/FizzyBubbles/Champagne01) in the app's colours, line work on black:
 *   far     faint double rings, slow
 *   specks  single pixels
 *   middle  small crisp circles, a lit arc on the bigger ones, now and then two touching
 *   bed     small bubbles forming along the bottom and lifting off
 *   near    big bubbles: black inside (they hide what is behind them), a rim in their colour, and tiny bubbles floating
 *           around inside, turning with the bubble (whole turns a loop) and wandering, always inside the rim. No lit
 *           arc, no reflection (the owner, P1.3p).
 *   the word  FIZZ as particles: points sampled from the letters (Inter Tight 300, 56 px, each letter its colour) on a
 *           2 px grid. They rise out of the bubbles and swirl into place quickly (about 0.55 s, stagger included);
 *           the solid word appears in the letters' colours as they arrive and they fade under it; it holds; then it
 *           breaks back into particles that fizz apart upward and fade.
 * Darker toward the top. Three near bubbles pass behind the word, four in front of it.
 * The loop (FZ_LOOP s) is seamless: every motion makes whole cycles a loop, the word comes and goes inside it.
 * Playing (a DAW's START while idle): a soft surge of bubbles from the bed on each beat, more on the bar's downbeat.
 * Deterministic (seeded hashes of the loop time and the beats). The letters' points are sampled once (qd_text_mask)
 * and kept: a pure function of the font, so every instance computes the same. No footer. */
#include <math.h>

#include "ui.h"
#include "../seq/qd_div.h"                    /* (P2.4: qd_ratio_f) */

#define FZ_LOOP 11.f
#define W 240.f
#define H 240.f
#define BED_Y 226.f

/* the app's colours: the knobs (blue, green, white, orange) and the modes' (pink, yellow, violet, cyan) */
static const qd_rgb PAL[8] = {UI_K1, UI_K2, UI_K3, UI_K4, 0xff3ca0u, UI_KSEL, 0x9664ffu, 0x28e1ffu};
#define NPAL 8

/* the word's timing (loop seconds) */
#define WD_GATHER 4.0f                                  /* the first particles leave */
#define WD_STAGGER 0.12f                                /* .. the last ones this much later */
#define WD_TRAVEL 0.42f                                 /* each one's flight: all in place by 4.54 s */
#define WD_SOLID0 4.50f                                 /* the solid word fades in .. */
#define WD_SOLID1 4.62f
#define WD_PFADE1 4.70f                                 /* .. the particles out under it by here */
#define WD_BREAK0 8.78f                                 /* the word breaks: the particles back in place .. */
#define WD_BREAK1 8.90f                                 /* .. the solid word gone */
#define WD_FIZZ 8.86f                                   /* the particles leave from here (+ up to WD_FSTAG) .. */
#define WD_FSTAG 0.6f
#define WD_FADE 0.9f                                    /* .. fading over this: all gone by 10.36 s */

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

/* a colour at brightness a (over black: an alpha without the cost of one) */
static qd_rgb dim(qd_rgb c, float a)
{
    const float f = clamp01(a);
    return (qd_rgb)((uint32_t)((float)(c >> 16 & 0xffu) * f) << 16 | (uint32_t)((float)(c >> 8 & 0xffu) * f) << 8 |
                    (uint32_t)((float)(c & 0xffu) * f));
}
static float darken(float y) { return clamp01(0.35f + 0.65f * y / H); }   /* darker toward the top */

/* a rise that loops exactly: k whole trips a loop */
static float rise(float u, int k, float phase, float r)
{
    const float span = H + 2.f * r, a = u / FZ_LOOP * (float)k + phase;
    return H + r - (a - floorf(a)) * span;
}
/* a sway: m whole cycles a loop */
static float sway(float u, int m, float ph) { return sinf(UI_TAU * ((float)m * u / FZ_LOOP) + ph); }

/* ---------------------------------------------------------------- the word's points */
#define WD_MAXP 520
#define WD_SIZE 56.f
#define WD_WEIGHT 300
#define WD_LS 6.f
#define WD_BASE 140.f                                   /* the baseline: the capitals centred on the screen */
#define WD_BAND0 92                                     /* the band the letters are rasterised in */
#define WD_BANDH 60
static const char *const WD_CH[4] = {"F", "I", "Z", "Z"};
static const qd_rgb WD_COL[4] = {UI_K1, UI_K2, UI_K4, 0xff3ca0u};
static struct {
    int ready, n;
    float lx[4];                                        /* each letter's pen x */
    uint8_t px[WD_MAXP], py[WD_MAXP];                   /* (P2.1c: each point's column and row in the band, bytes:
                                                         * they are on the sample grid) */
    uint8_t letter[WD_MAXP];
} wd;
#define WD_TX(k) ((float)wd.px[k] + 0.5f)
#define WD_TY(k) ((float)(WD_BAND0 + wd.py[k]) + 0.5f)

static void wd_build(void)
{
    static uint8_t mask[240];                           /* (P2.1b: a row at a time: the rows sampled) */
    qd_font f;
    float x, total = 0.f;
    int n, i, j;
    f.size = WD_SIZE;
    f.weight = WD_WEIGHT;
    f.col = UI_K3;
    f.align = QD_LEFT;
    f.ls = 0.f;
    for (n = 0; n < 4; n++)
        total += qd_text_width(WD_CH[n], &f) + (n < 3 ? WD_LS : 0.f);
    x = (W - total) / 2.f;
    wd.n = 0;
    for (n = 0; n < 4; n++) {
        wd.lx[n] = x;
        for (j = 0; j < WD_BANDH; j += 2)
            if (qd_text_mask_rows(WD_CH[n], x, WD_BASE - (float)WD_BAND0, &f, mask, 240, j, 1))
                for (i = 0; i < 240; i += 2)
                    if (mask[i] >= 140 && wd.n < WD_MAXP) {
                        wd.px[wd.n] = (uint8_t)i;
                        wd.py[wd.n] = (uint8_t)j;
                        wd.letter[wd.n] = (uint8_t)n;
                        wd.n++;
                    }
        x += qd_text_width(WD_CH[n], &f) + WD_LS;
    }
    wd.ready = 1;
}

void ui_idle_word(float t, float *solid, float *parts)
{
    const float u = fmodf(t, FZ_LOOP);
    float s = 0.f, p = 0.f;
    if (u >= WD_GATHER && u < WD_PFADE1) {              /* gathering, then the handover */
        s = ui_ease(seg(u, WD_SOLID0, WD_SOLID1));
        p = 1.f - ui_ease(seg(u, WD_SOLID1 - 0.06f, WD_PFADE1));
    } else if (u >= WD_PFADE1 && u < WD_BREAK0) {       /* the hold: solid */
        s = 1.f;
    } else if (u >= WD_BREAK0 && u < WD_FIZZ + WD_FSTAG + WD_FADE) {   /* the break, the fizz */
        s = 1.f - seg(u, WD_BREAK0, WD_BREAK1);
        p = seg(u, WD_BREAK0, WD_BREAK0 + 0.08f) * (1.f - seg(u, WD_FIZZ + WD_FSTAG, WD_FIZZ + WD_FSTAG + WD_FADE));
    }
    if (solid)
        *solid = s;
    if (parts)
        *parts = p;
}

static void draw_word_solid(float a)
{
    int n;
    if (a <= 0.f)
        return;
    qd_save();
    qd_alpha(a);
    for (n = 0; n < 4; n++)
        ui_T(WD_CH[n], wd.lx[n], WD_BASE, WD_SIZE, WD_WEIGHT, 0.f, WD_COL[n], QD_LEFT);
    qd_restore();
}

static void draw_word_particles(float u, float pa)
{
    int k;
    if (pa <= 0.f)
        return;
    for (k = 0; k < wd.n; k++) {
        const uint32_t q = (uint32_t)k;
        const float r = 0.55f + 0.5f * hash01(q, 3, 9), ph = UI_TAU * hash01(q, 4, 9);
        float x, y, a;
        if (u < WD_BREAK0) {                            /* gathering: up from below, a little swirl on the way */
            const float d0 = WD_GATHER + WD_STAGGER * hash01(q, 5, 9), g = ui_ease((u - d0) / WD_TRAVEL);
            const float sx = W * hash01(q, 6, 9), sy = H + 8.f + 40.f * hash01(q, 7, 9);
            const float sw = (1.f - g) * 12.f * (hash01(q, 8, 9) - 0.5f);
            if (u < d0)
                continue;
            x = sx + (WD_TX(k) - sx) * g + sinf((1.f - g) * 6.f + ph) * sw;
            y = sy + (WD_TY(k) - sy) * g + cosf((1.f - g) * 5.f + ph) * fabsf(sw);
            a = (0.35f + 0.65f * g) * pa;
        } else {                                        /* fizzing apart upward */
            const float v = u - (WD_FIZZ + WD_FSTAG * hash01(q, 10, 9));
            if (v <= 0.f) {
                x = WD_TX(k);
                y = WD_TY(k);
                a = pa;
            } else {
                x = WD_TX(k) + (hash01(q, 11, 9) - 0.5f) * 30.f * v + sinf(v * 4.f + ph) * 2.f;
                y = WD_TY(k) - (12.f + 30.f * hash01(q, 12, 9)) * v - 10.f * v * v;
                a = (1.f - ui_ease(v / WD_FADE)) * seg(u, WD_BREAK0, WD_BREAK0 + 0.08f);
            }
        }
        if (a > 0.01f)
            ui_O(x, y, r, dim(WD_COL[wd.letter[k]], a), 0.9f, QD_NONE);
    }
}

/* ---------------------------------------------------------------- the bubbles */
static void far_bokeh(float u)
{
    int i;
    for (i = 0; i < 14; i++) {
        const uint32_t q = (uint32_t)i;
        const float r = 6.f + 9.f * hash01(q, 1, 1), y = rise(u, 1, hash01(q, 2, 1), r);
        const float x = W * hash01(q, 3, 1) + 4.f * sway(u, 1, (float)i);
        const qd_rgb c = dim(PAL[i % NPAL], 0.16f * darken(y));
        ui_O(x, y, r, c, 1.f, QD_NONE);
        ui_O(x, y, r * 0.8f, c, 1.f, QD_NONE);
    }
}

static void specks(float u)
{
    int i;
    for (i = 0; i < 56; i++) {
        const uint32_t q = (uint32_t)i;
        const float y = rise(u, 2 + i % 3, hash01(q, 2, 2), 1.f), x = W * hash01(q, 3, 2) + 2.f * sway(u, 3, (float)i);
        qd_fill_rect(floorf(x), floorf(y), 1.f, 1.f, dim(PAL[(i * 3) % NPAL], 0.75f * darken(y)));
    }
}

static void middle(float u)
{
    int i;
    for (i = 0; i < 34; i++) {
        const uint32_t q = (uint32_t)i;
        const float r = 1.5f + 3.f * hash01(q, 1, 3), y = rise(u, 2 + (int)(r / 2.f + 0.5f), hash01(q, 2, 3), r);
        const float x = W * hash01(q, 3, 3) + (1.f + r * 0.4f) * sway(u, 2, (float)i), a = 0.9f * darken(y);
        const qd_rgb c = dim(PAL[(i * 5) % NPAL], a);
        ui_O(x, y, r, c, 1.f, QD_NONE);
        if (r > 3.f)
            qd_arc(x, y, r * 0.6f, 3.6f, 4.4f, dim(0xffffffu, a * 0.8f), 1.f);
        if (hash01(q, 4, 3) > 0.85f)                    /* now and then two touching */
            ui_O(x + r * 1.7f, y - r * 0.6f, r * 0.7f, c, 1.f, QD_NONE);
    }
}

static void bed(float u)
{
    int i;
    for (i = 0; i < 26; i++) {
        const uint32_t q = (uint32_t)i;
        const float per = FZ_LOOP / (float)(3 + i % 3), a = u / per + hash01(q, 1, 4), f = a - floorf(a);
        const float x = 8.f + (W - 16.f) * hash01(q, 2, 4), r = 0.8f + 2.4f * ui_ease(f / 0.6f);
        const float y = H - 6.f - (f > 0.6f ? (f - 0.6f) / 0.4f * 40.f : 0.f) - 5.f * hash01(q, 3, 4);
        ui_O(x, y, r, dim(PAL[(i * 7) % NPAL], 0.85f * (1.f - ui_ease((f - 0.8f) / 0.2f))), 1.f, QD_NONE);
    }
}

/* a big near bubble: black inside, its rim in its colour, the tiny bubbles inside turning with it and wandering */
static void near_bubble(float u, int i)
{
    const uint32_t q = (uint32_t)i;
    const float r = 9.f + 13.f * hash01(q, 1, 5);
    const float y = rise(u, r > 16.f ? 2 : 1, hash01(q, 2, 5), r);
    const float x = 20.f + (W - 40.f) * hash01(q, 3, 5) + 5.f * sway(u, 1, (float)i * 2.f), a = darken(y);
    const float spin = (i % 2 ? 1.f : -1.f) * UI_TAU / FZ_LOOP * (float)(1 + i % 3);   /* whole turns a loop */
    const int n = 10 + (int)(hash01(q, 4, 5) * 9.f);
    int j;
    ui_O(x, y, r, dim(PAL[(i * 3 + 1) % NPAL], a), 1.2f, 0x000000u);
    for (j = 0; j < n; j++) {
        const uint32_t s = q * 37u + (uint32_t)j;
        const float br = 0.6f + 1.3f * hash01(s, 1, 6), ph = UI_TAU * hash01(s, 2, 6);
        float rr = r * (0.2f + 0.55f * hash01(s, 3, 6)) + r * 0.12f * sway(u, 1 + j % 3, ph);
        float ang;
        if (rr > r - br - 1.5f)
            rr = r - br - 1.5f;
        ang = UI_TAU * hash01(s, 4, 6) + u * spin + 0.6f * sway(u, 2 + j % 2, ph * 2.f);
        ui_O(x + cosf(ang) * rr, y + sinf(ang) * rr, br, dim(PAL[(j + i) % NPAL], 0.9f * a), 0.9f, QD_NONE);
    }
}

void ui_idle_draw(const ui_idle_t *v)
{
    const float t = v->t, u = fmodf(t, FZ_LOOP), bpm = (float)(v->bpm > 0 ? v->bpm : 120);
    float solid, parts;
    int i, k;

    if (!wd.ready)
        wd_build();
    ui_idle_word(t, &solid, &parts);
    far_bokeh(u);
    specks(u);
    middle(u);
    bed(u);
    if (v->playing) {                                   /* the beats: a soft surge from the bed on each */
        const long last = v->beat;
        long b;
        for (b = last - 9; b <= last; b++) {
            const uint32_t s = (uint32_t)b * 2654435761u + 17u;
            const int m = b % 4 == 0 ? 7 : 3;
            const float age = qd_ratio_f((int64_t)(last - b) * v->beat_den + v->beat_num, v->beat_den) * 60.f / bpm;
            for (k = 0; k < m; k++) {
                const float r = 1.5f + 2.5f * hash01(s, (uint32_t)k, 1), sp = 60.f + 40.f * hash01(s, (uint32_t)k, 2);
                const float y = BED_Y - sp * age, x = 12.f + 216.f * hash01(s, (uint32_t)k, 3);
                if (b < 0 || y < -10.f)
                    continue;
                ui_O(x + 3.f * sinf(age * 2.5f + (float)k), y, r * clamp01(age * 4.f),
                     dim(PAL[(uint32_t)(b + k) % NPAL], 0.95f * darken(y)), 1.f, QD_NONE);
            }
        }
    }
    for (i = 0; i < 3; i++)                             /* three pass behind the word */
        near_bubble(u, i);
    draw_word_solid(solid);
    draw_word_particles(u, parts);
    for (i = 3; i < 7; i++)                             /* four in front of it */
        near_bubble(u, i);
}
