/* SPDX-License-Identifier: GPL-3.0-only */
/* Screen 1, idle (P1.3m): FIZZ, the logo screen. The top of a glass, close up (its sides run off the bottom edge), in an
 * illustrative outlined style: thin light lines on black, no colour. Black under a shape only where it must hide what
 * is behind it (the foam's circles, the drips, the straw), so overlaps read as in a drawing.
 *   the glass   the rim as an ellipse, the sides running down off the frame, a highlight stroke
 *   the straw   an outlined tube standing diagonally, its stripes as lines across it
 *   the drink   its surface as a wavy line rising into view; bubbles as small circles rising to it; cold dots on the glass
 *   the foam    overlapping circles, the back ones hidden by the front ones, bulging above the rim and spilling over
 *   the spill   drips as outlined teardrops running down the outside; spray as tiny circles fizzing up and falling
 *   the logo    "FIZZ" in thin Inter Tight rising above the foam as it peaks, staying while it settles
 * The loop (FZ_LOOP s): the surface rises into view, the foam swells over the rim, it overflows (drips, spray, FIZZ
 * rising), settles to a head, and everything but the glass and the straw fades, so the loop restarts seamlessly.
 * Playing (a DAW's START while idle): the bubble streams quicken and the spray pulses on each beat, a bigger surge on
 * the bar's downbeat. Stateless and deterministic (seeded hashes of the clock and the beats); a few dozen shapes.
 * No footer: the logo screen is only the glass and FIZZ (the owner, P1.3m). */
#include <math.h>

#include "ui.h"

#define FZ_LOOP 9.4f
#define CX 120.f
#define RIM_Y 122.f                                     /* the rim's centre line */
#define RIM_RX 98.f                                     /* .. its half width (the ellipse's) */
#define RIM_RY 9.f
#define TAPER 0.1f                                      /* the sides come in this many px a px down */
#define C_LINE fade(0xe8e8e8u)                    /* pure greys: no tint */
#define C_SOFT fade(0x9c9c9cu)
#define C_LOGO 0xf2f2f2u
#define LW 1.3f

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
/* the loop's fade: the lines dim toward black (not an alpha, so the black under the shapes keeps hiding what is
 * behind them while they fade) */
static float g_fade = 1.f;
static qd_rgb fade(qd_rgb c)
{
    const float f = g_fade;
    return (qd_rgb)((uint32_t)((float)(c >> 16 & 0xffu) * f) << 16 | (uint32_t)((float)(c >> 8 & 0xffu) * f) << 8 |
                    (uint32_t)((float)(c & 0xffu) * f));
}
static float clamp01(float x) { return x < 0.f ? 0.f : x > 1.f ? 1.f : x; }
static float seg(float t, float a, float b) { return clamp01((t - a) / (b - a)); }
static float half_w(float y) { return RIM_RX - TAPER * (y - RIM_Y); }   /* the inside half width at y */

static void line2(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, col, lw, 0, QD_NONE);
}

typedef struct {
    float level, foam, spill, settle, alpha, fill;
} gl_state_t;

static gl_state_t state_at(float u)
{
    gl_state_t s;
    const float f = ui_ease(seg(u, 0.4f, 3.6f)), build = ui_ease(seg(u, 3.2f, 5.4f));
    s.fill = f;
    s.settle = ui_ease(seg(u, 6.6f, 8.0f));
    s.level = 252.f - f * (252.f - (RIM_Y + 12.f)) + (f > 0.f && f < 1.f ? 1.5f * sinf(u * 10.f) : 0.f);
    s.foam = f > 0.9f ? (6.f + 70.f * build) * (1.f - 0.68f * s.settle) : 0.f;
    s.spill = seg(u, 5.0f, 5.6f) * (1.f - s.settle);
    s.alpha = 1.f - seg(u, 8.4f, 9.2f);
    return s;
}

/* the straw: an outlined tube from below the frame up past the rim, black inside, its stripes as lines */
static void draw_straw(void)
{
    const float x0 = 84.f, y0 = 264.f, x1 = 184.f, y1 = 70.f, w = 5.f;
    const float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy), nx = -dy / len * w, ny = dx / len * w;
    const float tube[8] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    int k;
    ui_P(tube, 4, C_LINE, LW, 1, 0x000000u);
    for (k = 1; k < 17; k++) {                          /* the stripes, slanted across the tube */
        const float a = (float)k / 17.f, cx = x0 + dx * a, cy = y0 + dy * a;
        line2(cx + nx * 0.85f - dx / len * 3.f, cy + ny * 0.85f - dy / len * 3.f, cx - nx * 0.85f + dx / len * 3.f,
              cy - ny * 0.85f + dy / len * 3.f, C_SOFT, 1.f);
    }
}

static void draw_glass(void)
{
    qd_ellipse(CX, RIM_Y, RIM_RX, RIM_RY, C_LINE, LW, QD_NONE);
    line2(CX - RIM_RX, RIM_Y, CX - half_w(240.f), 240.f, C_LINE, LW);
    line2(CX + RIM_RX, RIM_Y, CX + half_w(240.f), 240.f, C_LINE, LW);
    line2(CX - RIM_RX + 10.f, RIM_Y + 16.f, CX - half_w(232.f) + 8.f, 232.f, C_SOFT, 1.f);   /* a highlight */
    line2(CX - RIM_RX + 15.f, RIM_Y + 22.f, CX - RIM_RX + 18.f, RIM_Y + 48.f, C_SOFT, 1.f);
}

/* the drink's surface: a wavy line across the glass at its level */
static void draw_surface(const gl_state_t *s, float t)
{
    float pts[2 * 33];
    const float y = s->level, hw = half_w(y) - 1.f;
    int k;
    if (y > 240.f)
        return;
    for (k = 0; k <= 32; k++) {
        const float x = CX - hw + 2.f * hw * (float)k / 32.f;
        pts[2 * k] = x;
        pts[2 * k + 1] = y + 1.6f * sinf(x * 0.09f + t * 3.f) + 0.8f * sinf(x * 0.23f - t * 5.f);
    }
    ui_P(pts, 33, C_LINE, LW, 0, QD_NONE);
}

/* bubbles: small outlined circles rising from below the frame to the surface */
static void draw_bubbles(const gl_state_t *s, float t, float pulse)
{
    int k;
    const float top = s->level + 4.f, depth = 252.f - top;
    if (s->fill <= 0.02f || depth < 6.f)
        return;
    for (k = 0; k < 20; k++) {
        const float sp = (22.f + 30.f * hash01((uint32_t)k, 2, 1)) * (1.f - 0.45f * s->settle) + 50.f * pulse;
        const float y = 250.f - fmodf(t * sp + 211.f * hash01((uint32_t)k, 3, 1), depth);
        const float hw = half_w(y) - 6.f, x = CX + (hash01((uint32_t)k, 1, 1) * 2.f - 1.f) * hw;
        const float r = 1.2f + 1.8f * hash01((uint32_t)k, 4, 1) * (0.6f + 0.4f * (250.f - y) / depth);
        ui_O(x + 1.5f * sinf(t * 4.f + (float)k), y, r, C_SOFT, 1.f, QD_NONE);
    }
}

/* cold dots on the glass, more as it fills */
static void draw_condensation(const gl_state_t *s)
{
    int k;
    const int n = (int)(30.f * s->fill);
    for (k = 0; k < n; k++) {
        const float y = RIM_Y + 18.f + 100.f * hash01((uint32_t)k, 5, 2);
        const float side = hash01((uint32_t)k, 6, 2) < 0.5f ? -1.f : 1.f;
        const float x = CX + side * (half_w(y) - 3.f - 10.f * hash01((uint32_t)k, 7, 2));
        ui_O(x, y, 0.8f + 0.9f * hash01((uint32_t)k, 8, 2), C_SOFT, 0.9f, QD_NONE);
    }
}

/* the foam: overlapping outlined circles, back to front (the higher ones behind), black inside so the front ones hide
 * the back ones' lines; bulging in the middle, wider than the rim when it spills */
#define NFOAM 34
static void draw_foam(const gl_state_t *s, float t)
{
    float fx[NFOAM], fy[NFOAM], fr[NFOAM];
    int order[NFOAM], k, j;
    if (s->foam <= 0.5f)
        return;
    for (k = 0; k < NFOAM; k++) {
        const float a = hash01((uint32_t)k, 9, 3) * 2.f - 1.f, b = hash01((uint32_t)k, 10, 3);
        const float wide = half_w(s->level) - 6.f + 6.f * s->spill;
        const float hump = 1.f - a * a;
        fx[k] = CX + a * wide;
        fy[k] = s->level - s->foam * (0.1f + 0.9f * hump * b) + 0.8f * sinf(t * 2.f + (float)k);
        fr[k] = (7.f + 9.f * hash01((uint32_t)k, 11, 3)) * (0.45f + 0.55f * clamp01(s->foam / 40.f));
        order[k] = k;
    }
    for (k = 1; k < NFOAM; k++)                         /* back (higher) first */
        for (j = k; j > 0 && fy[order[j - 1]] > fy[order[j]]; j--) {
            const int x = order[j];
            order[j] = order[j - 1];
            order[j - 1] = x;
        }
    for (k = 0; k < NFOAM; k++) {
        const int i = order[k];
        ui_O(fx[i], fy[i], fr[i], C_LINE, LW, 0x000000u);
    }
}

/* a drip: an outlined teardrop with its round end at (x, y), its point up */
static void teardrop(float x, float y, float r)
{
    qd_path_begin();
    qd_move_to(x, y - r * 3.f);
    qd_quad_to(x + r * 1.1f, y - r * 1.2f, x + r, y);
    qd_path_arc(x, y, r, 0.f, UI_PI);
    qd_quad_to(x - r * 1.1f, y - r * 1.2f, x, y - r * 3.f);
    qd_path_close();
    qd_path_fill(0x000000u);
    qd_path_begin();
    qd_move_to(x, y - r * 3.f);
    qd_quad_to(x + r * 1.1f, y - r * 1.2f, x + r, y);
    qd_path_arc(x, y, r, 0.f, UI_PI);
    qd_quad_to(x - r * 1.1f, y - r * 1.2f, x, y - r * 3.f);
    qd_path_stroke(C_LINE, LW);
}

static void draw_drips(float u)
{
    int k;
    for (k = 0; k < 5; k++) {
        const float t0 = 5.1f + 0.3f * (float)k, a = (u - t0) / 2.4f;
        const float side = k & 1 ? 1.f : -1.f, len = 30.f + 60.f * hash01((uint32_t)k, 12, 4);
        float y, x;
        if (a <= 0.f)
            continue;
        y = RIM_Y + 4.f + len * ui_ease(clamp01(a)) + (a > 1.f ? 12.f * (a - 1.f) : 0.f);   /* (then it creeps on) */
        x = CX + side * (half_w(y) + 4.f + 3.f * hash01((uint32_t)k, 13, 4));
        if (y > 250.f)
            continue;
        line2(CX + side * (RIM_RX + 3.f), RIM_Y, x, y - 7.f, C_LINE, 1.f);   /* its trail down the glass */
        teardrop(x, y, 3.2f + 1.2f * hash01((uint32_t)k, 14, 4));
    }
}

/* spray: tiny outlined circles fizzing up from the foam's top and falling */
static void droplet(uint32_t seed, uint32_t j, float age, float top)
{
    const float vx = (hash01(seed, j, 1) - 0.5f) * 90.f, vy = 70.f + 80.f * hash01(seed, j, 2);
    const float x = CX + (hash01(seed, j, 3) - 0.5f) * 110.f + vx * age, y = top - vy * age + 130.f * age * age;
    if (age < 0.f || age > 1.2f || y > 240.f)
        return;
    qd_save();
    qd_alpha(1.f - age / 1.2f);
    ui_O(x, y, 1.f + 1.1f * hash01(seed, j, 5), C_LINE, 1.f, 0x000000u);
    qd_restore();
}

void ui_idle_draw(const ui_idle_t *v)
{
    const float t = v->t, u = fmodf(t, FZ_LOOP), bpm = (float)(v->bpm > 0 ? v->bpm : 120);
    const gl_state_t s = state_at(u);
    const float top = s.level - s.foam;
    float pulse = 0.f;
    int k;

    if (v->playing) {                                   /* the beat's pulse: 1 at the beat, fading over a beat */
        const double fr = v->beats - floor(v->beats);
        pulse = (float)(1.0 - fr) * ((long)floor(v->beats) % 4 == 0 ? 1.f : 0.55f);
    }
    g_fade = 1.f;
    draw_straw();                                       /* the glass and the straw stay; the rest fades */
    g_fade = s.alpha;
    draw_surface(&s, t);
    draw_bubbles(&s, t, pulse);
    draw_condensation(&s);
    g_fade = 1.f;
    draw_glass();
    g_fade = s.alpha;
    draw_drips(u);                                      /* (under the foam, which hides where they start) */
    draw_foam(&s, t);
    for (k = 0; k < 30; k++)                            /* the overflow's spray */
        droplet(7u, (uint32_t)k, u - 5.0f - 1.6f * hash01(7u, (uint32_t)k, 9), top);
    if (v->playing) {                                   /* the beats' spray */
        const long last = (long)floor(v->beats);
        long b;
        for (b = last - 2; b <= last; b++) {
            const int m = b % 4 == 0 ? 14 : 6;
            const float age = (float)(v->beats - (double)b) * 60.f / bpm;
            for (k = 0; k < m; k++)
                droplet((uint32_t)b * 2654435761u + 11u, (uint32_t)k, age - 0.02f * (float)k,
                        s.foam > 1.f ? top : s.level);
        }
    }
    {   /* "FIZZ" rises above the foam as it peaks, stays as it settles */
        const float r = ui_ease(seg(u, 5.0f, 6.2f));
        if (r > 0.f) {
            qd_save();
            qd_alpha(r * s.alpha);
            ui_T("FIZZ", CX, 64.f - 24.f * r, 40.f, 200, 6.f, C_LOGO, QD_CENTER);
            qd_restore();
        }
    }
    g_fade = 1.f;
}
