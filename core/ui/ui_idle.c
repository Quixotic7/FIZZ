/* SPDX-License-Identifier: GPL-3.0-only */
/* Screen 1, idle (P1.3l): FIZZ, a glass filling up and fizzing over: the brand moment, the one screen with warm fills.
 * A tapered tumbler in thin light lines on black, a pink-and-white straw standing in it, and a loop of FZ_LOOP seconds:
 *   0.0  the empty glass
 *   0.6  the pour: an amber stream from above, the level rising with a wobble, tiny bubbles rising inside, cold dots
 *        (condensation) gathering on the glass
 *   3.6  the foam head builds: a bulging cluster of white circles swelling above the rim
 *   5.2  it overflows: foam spills over the rim and runs down the outside in drips, fine spray fizzes up and falls;
 *        "FIZZ" rises out of the foam as it peaks
 *   6.6  it settles: the foam shrinks to a head, the bubbles calm
 *   8.4  the drink fades out (the glass stays), and the loop starts again from the empty glass
 * Playing (a DAW's START while idle): the spray and the bubble streams pulse on each beat, a bigger surge on the bar's
 * downbeat. Stateless and deterministic: every particle is a seeded hash of its index, drawn from the clock and the
 * beats. The pattern and the tempo at the foot, the play mark between them (outlined stopped, filled playing). */
#include <math.h>

#include "ui.h"

#define FZ_LOOP 9.4f
#define GL_TOP 76.f                                     /* the rim */
#define GL_BOT 182.f                                    /* the inside of the base */
#define GL_TW 44.f                                      /* half widths at the rim and at the base */
#define GL_BW 35.f
#define CX 120.f
#define C_GLASS 0xd8d8deu
#define C_DRINK 0x9a4e14u
#define C_BAND 0xe0a050u
#define C_FOAM 0xf6f1e8u
#define C_STRAW 0xff4f6eu

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
static float seg(float t, float a, float b) { return clamp01((t - a) / (b - a)); }   /* 0 before a, 1 after b */
static float half_w(float y) { return GL_BW + (GL_TW - GL_BW) * (GL_BOT - y) / (GL_BOT - GL_TOP); }   /* inside */

static void line2(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, col, lw, 0, QD_NONE);
}

/* the drink's state at loop time u: its level (y of the surface), the foam's height over the surface (px), the spill
 * 0..1 (overflowing), the content's alpha (the end's fade) */
typedef struct {
    float level, foam, spill, alpha, fill, settle;
} gl_state_t;

static gl_state_t state_at(float u)
{
    gl_state_t s;
    const float f = ui_ease(seg(u, 0.6f, 3.8f)), build = ui_ease(seg(u, 3.4f, 5.4f)), settle = ui_ease(seg(u, 6.6f, 8.0f));
    s.fill = f;
    s.level = GL_BOT - f * (GL_BOT - GL_TOP - 6.f) + (f > 0.f && f < 1.f ? 1.6f * sinf(u * 11.f) * (1.f - f) : 0.f);
    s.foam = (2.f + 26.f * build) * (1.f - 0.62f * settle) * (f > 0.05f ? 1.f : 0.f);
    s.spill = seg(u, 5.0f, 5.6f) * (1.f - settle);
    s.settle = settle;
    s.alpha = 1.f - seg(u, 8.4f, 9.2f);
    return s;
}

static void draw_straw(void)
{
    static const float dash[2] = {4.f, 5.f};
    const float x0 = 104.f, y0 = 172.f, x1 = 166.f, y1 = 38.f;
    qd_line(x0, y0, x1, y1, C_STRAW, 6.f);
    qd_save();
    qd_dash(dash, 2);
    qd_line(x0, y0, x1, y1, 0xffffffu, 3.f);
    qd_restore();
}

/* the drink inside the glass from the level down */
static void draw_drink(const gl_state_t *s)
{
    const float y = s->level < GL_TOP ? GL_TOP : s->level;
    if (s->fill <= 0.f)
        return;
    qd_save();
    qd_alpha(0.86f * s->alpha);
    qd_path_begin();
    qd_move_to(CX - half_w(y), y);
    qd_line_to(CX + half_w(y), y);
    qd_line_to(CX + GL_BW, GL_BOT);
    qd_line_to(CX - GL_BW, GL_BOT);
    qd_path_close();
    qd_path_fill(C_DRINK);
    qd_alpha(0.55f * s->alpha);                         /* the lighter band under the surface */
    qd_path_begin();
    qd_move_to(CX - half_w(y), y);
    qd_line_to(CX + half_w(y), y);
    qd_line_to(CX + half_w(y + 9.f), y + 9.f);
    qd_line_to(CX - half_w(y + 9.f), y + 9.f);
    qd_path_close();
    qd_path_fill(C_BAND);
    qd_restore();
}

/* the bubbles inside: streams rising from the base to the level, quicker on the beat */
static void draw_bubbles(const gl_state_t *s, float t, float pulse)
{
    int k;
    if (s->fill <= 0.05f)
        return;
    qd_save();
    qd_alpha(s->alpha * (1.f - 0.5f * s->settle));
    for (k = 0; k < 18; k++) {
        const float x0 = CX - 28.f + 56.f * hash01((uint32_t)k, 1, 1);
        const float sp = 26.f + 30.f * hash01((uint32_t)k, 2, 1) + 40.f * pulse;
        const float depth = GL_BOT - s->level;
        float y;
        if (depth < 8.f)
            continue;
        y = GL_BOT - 2.f - fmodf(t * sp + 97.f * hash01((uint32_t)k, 3, 1), depth - 4.f);
        ui_O(x0 + 1.5f * sinf(t * 5.f + (float)k), y, 0.9f + 0.9f * hash01((uint32_t)k, 4, 1), 0xffd9a8u, 1.f,
             QD_NONE);
    }
    qd_restore();
}

/* cold dots on the glass, more as it fills */
static void draw_condensation(const gl_state_t *s)
{
    int k;
    const int n = (int)(26.f * s->fill);
    qd_save();
    qd_alpha(0.55f * s->alpha);
    for (k = 0; k < n; k++) {
        const float y = GL_TOP + 8.f + (GL_BOT - GL_TOP - 14.f) * hash01((uint32_t)k, 5, 2);
        const float side = hash01((uint32_t)k, 6, 2) < 0.5f ? -1.f : 1.f;
        const float x = CX + side * (half_w(y) + 2.f - 6.f * hash01((uint32_t)k, 7, 2));
        ui_O(x, y, 0.7f + 0.8f * hash01((uint32_t)k, 8, 2), QD_NONE, 1.f, 0xe8eef6u);
    }
    qd_restore();
}

/* the foam head: a bulging cluster of white circles over the surface, spilling past the rim when it overflows */
static void draw_foam(const gl_state_t *s)
{
    int k;
    if (s->foam <= 0.5f)
        return;
    qd_save();
    qd_alpha(s->alpha);
    for (k = 0; k < 26; k++) {
        const float a = hash01((uint32_t)k, 9, 3), b = hash01((uint32_t)k, 10, 3);
        const float wide = half_w(s->level) + 6.f * s->spill;
        const float x = CX + (a * 2.f - 1.f) * wide;
        const float hump = 1.f - (a * 2.f - 1.f) * (a * 2.f - 1.f);   /* higher in the middle: it bulges */
        const float y = s->level - s->foam * (0.25f + 0.75f * hump * b);
        const float r = (4.f + 5.f * hash01((uint32_t)k, 11, 3)) * (0.5f + 0.5f * clamp01(s->foam / 20.f));
        ui_O(x, y, r, 0xb8b4aeu, 1.f, C_FOAM);
    }
    qd_restore();
}

/* the overflow's drips down the outside of the glass */
static void draw_drips(float u, const gl_state_t *s)
{
    int k;
    for (k = 0; k < 4; k++) {
        const float t0 = 5.1f + 0.35f * (float)k, a = (u - t0) / 2.2f;
        const float side = k & 1 ? 1.f : -1.f, len = 30.f + 50.f * hash01((uint32_t)k, 12, 4);
        float yh, x;
        if (a <= 0.f)
            continue;
        yh = GL_TOP + len * ui_ease(clamp01(a));
        x = CX + side * (half_w(yh) + 2.5f);           /* down the outside of the wall */
        qd_save();
        qd_alpha(s->alpha * (1.f - s->settle * 0.7f));
        qd_line(CX + side * (GL_TW + 2.5f), GL_TOP, x, yh, C_FOAM, 4.f);
        ui_O(x, yh + 2.f, 3.4f, QD_NONE, 1.f, C_FOAM);
        qd_restore();
    }
}

/* spray: fine droplets fizzing up from the foam and falling; launched through the overflow, and on each beat when
 * playing (bigger on the downbeat) */
static void droplet(uint32_t seed, uint32_t j, float age, float top, float alpha)
{
    const float vx = (hash01(seed, j, 1) - 0.5f) * 70.f, vy = 60.f + 70.f * hash01(seed, j, 2);
    const float x = CX + (hash01(seed, j, 3) - 0.5f) * 60.f + vx * age, y = top - vy * age + 120.f * age * age;
    const qd_rgb col = hash01(seed, j, 4) < 0.7f ? C_FOAM : C_BAND;
    if (age < 0.f || age > 1.1f || y > GL_BOT)
        return;
    qd_save();
    qd_alpha(alpha * (1.f - age / 1.1f));
    ui_O(x, y, 0.9f + 0.7f * hash01(seed, j, 5), QD_NONE, 1.f, col);
    qd_restore();
}

void ui_idle_draw(const ui_idle_t *v)
{
    const float t = v->t, u = fmodf(t, FZ_LOOP), bpm = (float)(v->bpm > 0 ? v->bpm : 120);
    const gl_state_t s = state_at(u);
    const float top = s.level - s.foam;
    float pulse = 0.f;
    char bpms[12];
    int k;

    if (v->playing) {                                   /* the beat's pulse: 1 at the beat, fading over a beat */
        const double fr = v->beats - floor(v->beats);
        pulse = (float)(1.0 - fr) * ((long)floor(v->beats) % 4 == 0 ? 1.f : 0.55f);
    }

    draw_straw();
    draw_drink(&s);
    draw_bubbles(&s, t, pulse);
    if (s.fill > 0.f && s.fill < 1.f && u < 3.9f) {     /* the pour: an amber stream from above */
        const float w = 3.f * (1.f - seg(u, 3.3f, 3.9f)) + 0.5f;
        qd_save();
        qd_alpha(0.9f);
        qd_line(CX - 6.f + 0.8f * sinf(t * 23.f), 0.f, CX - 6.f, s.level, C_DRINK, w);
        qd_restore();
    }
    draw_foam(&s);
    draw_drips(u, &s);
    {   /* the glass: thin light lines (in front of the drink) */
        const float g[8] = {CX - GL_TW - 1.f, GL_TOP, CX - GL_BW - 1.f, GL_BOT + 1.f, CX + GL_BW + 1.f, GL_BOT + 1.f,
                            CX + GL_TW + 1.f, GL_TOP};
        ui_P(g, 4, C_GLASS, 1.4f, 0, QD_NONE);
        line2(CX - GL_BW + 2.f, GL_BOT + 5.f, CX + GL_BW - 2.f, GL_BOT + 5.f, C_GLASS, 1.2f);   /* the thick base */
        qd_save();
        qd_alpha(0.35f);
        line2(CX - GL_TW + 7.f, GL_TOP + 10.f, CX - GL_BW + 6.f, GL_BOT - 14.f, 0xffffffu, 2.f);   /* a highlight */
        qd_restore();
    }
    draw_condensation(&s);
    for (k = 0; k < 26; k++)                            /* the overflow's spray */
        droplet(7u, (uint32_t)k, u - 5.0f - 1.4f * hash01(7u, (uint32_t)k, 9), top, s.alpha);
    if (v->playing) {                                   /* the beats' spray */
        const long last = (long)floor(v->beats);
        long b;
        for (b = last - 2; b <= last; b++) {
            const int m = b % 4 == 0 ? 14 : 6;
            const float age = (float)(v->beats - (double)b) * 60.f / bpm;
            for (k = 0; k < m; k++)
                droplet((uint32_t)b * 2654435761u + 11u, (uint32_t)k, age - 0.02f * (float)k,
                        s.fill > 0.1f ? top : GL_BOT - 4.f, 1.f);
        }
    }
    {   /* "FIZZ" rises out of the foam as it peaks, stays as it settles, fades with the drink */
        const float r = ui_ease(seg(u, 5.0f, 6.2f));
        if (r > 0.f) {
            qd_save();
            qd_alpha(r * s.alpha);
            ui_T("FIZZ", CX, 46.f - 14.f * r, 36.f, 200, 5.f, UI_K3, QD_CENTER);
            qd_restore();
        }
    }

    ui_T(v->pattern, 10.f, 222.f, 22.f, 300, 0.f, UI_K3, QD_LEFT);
    ui_itoa(bpms, v->bpm);
    ui_T(bpms, 232.f, 222.f, 22.f, 300, 0.f, UI_K1, QD_RIGHT);
    ui_lab("BPM", 232.f, 196.f, UI_G2, QD_RIGHT);
    ui_lab("PATTERN", 10.f, 196.f, UI_G2, QD_LEFT);
    {   /* the play mark: outlined when stopped, filled when playing */
        const float tri[6] = {114.f, 206.f, 114.f, 220.f, 126.f, 213.f};
        ui_P(tri, 3, UI_G1, 1.3f, 1, v->playing ? UI_G1 : QD_NONE);
    }
}
