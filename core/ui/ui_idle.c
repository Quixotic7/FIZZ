/* SPDX-License-Identifier: GPL-3.0-only */
/* Screen 1, idle (P1.3k): FIZZ. The screen is the drink: a thin surface line near the top, "FIZZ" big in thin Inter
 * Tight, and bubbles rising from the bottom in the four knob colours, each a thin circle. A bubble wobbles side to side
 * as it rises, grows a little and speeds up, and pops at the surface: a ring that opens and fades, and a few short
 * splash lines. The odd bubble catches on a letter and clings a moment before it lets go.
 *   stopped   a gentle, lazy stream
 *   playing   the stream quickens with the tempo, a burst of bubbles on every beat and a bigger fizz on each bar's
 *             downbeat (beats counted by the transport; the bursts of the last beats still rise after a stop)
 * Stateless and deterministic: every bubble is a function of the view's clocks and a seeded hash of its slot and its
 * birth (no state kept, nothing random at run time), so a frame is a function of its view. A few dozen bubbles at most.
 * The pattern and the tempo at the foot as before, the play mark between them (outlined stopped, filled playing). */
#include <math.h>

#include "ui.h"

#define FZ_BOTTOM 186.f                                 /* where bubbles are born */
#define FZ_SURFACE 46.f                                 /* .. and pop */
#define FZ_POP 0.32f                                    /* the pop's ring and splash, seconds */
#define FZ_NSTREAM 14                                   /* the lazy stream's slots */

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

/* one bubble: born at x0 (radius r0, colour col) `age` seconds ago, rising at v0 px/s and speeding up; cling: the
 * letter band's y it holds at for `hold` s (0: none) */
typedef struct {
    float x0, r0, v0, wob, freq, ph, cling_y, hold;
    int col;
} fz_bubble_t;

static float rise(const fz_bubble_t *b, float a) { return b->v0 * a + 14.f * a * a; }   /* px risen after a s */

/* the time a bubble takes to rise d px */
static float time_to(const fz_bubble_t *b, float d)
{
    return (-b->v0 + sqrtf(b->v0 * b->v0 + 4.f * 14.f * d)) / (2.f * 14.f);
}

static void line2(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, col, lw, 0, QD_NONE);
}

static void draw_bubble(const fz_bubble_t *b, float age)
{
    const qd_rgb col = UI_K[b->col & 3];
    const float total = time_to(b, FZ_BOTTOM - FZ_SURFACE);
    const float tc = b->hold > 0.f ? time_to(b, FZ_BOTTOM - b->cling_y) : 0.f;
    float a = age, y, x, r, k;
    if (age < 0.f)
        return;
    if (b->hold > 0.f && age > tc)                      /* caught on a letter: it holds, then lets go */
        a = age < tc + b->hold ? tc : age - b->hold;
    k = a / (total > 0.f ? total : 1.f);
    if (k <= 1.f) {
        const int held = b->hold > 0.f && age > tc && age < tc + b->hold;
        y = FZ_BOTTOM - rise(b, a);
        x = b->x0 + b->wob * (0.4f + 0.6f * k) * sinf(b->freq * a + b->ph);
        r = b->r0 * (1.f + 0.45f * k);
        if (held)                                       /* clinging: a little tremble, squashed */
            x += 0.6f * sinf(age * 40.f);
        if (k < 0.08f) {                                /* (it appears: a fade in) */
            qd_save();
            qd_alpha(k / 0.08f);
            ui_O(x, y, r, col, 1.2f, QD_NONE);
            qd_restore();
        } else {
            ui_O(x, y, r, col, 1.2f, QD_NONE);
        }
        return;
    }
    {   /* popped: a ring that opens and fades, three short splash lines */
        const float p = (age - (b->hold > 0.f ? b->hold : 0.f) - total) / FZ_POP;
        const float xs = b->x0 + b->wob * sinf(b->freq * total + b->ph), rs = b->r0 * 1.45f;
        int i;
        if (p < 0.f || p >= 1.f)
            return;
        qd_save();
        qd_alpha(1.f - p);
        ui_O(xs, FZ_SURFACE, rs + 7.f * p, col, 1.f, QD_NONE);
        for (i = -1; i <= 1; i++) {
            const float an = -UI_PI / 2.f + (float)i * 0.7f, d0 = rs + 3.f + 6.f * p, d1 = d0 + 3.f;
            line2(xs + cosf(an) * d0, FZ_SURFACE + sinf(an) * d0, xs + cosf(an) * d1, FZ_SURFACE + sinf(an) * d1, col,
                  1.f);
        }
        qd_restore();
    }
}

/* the lazy stream: slot k's n-th bubble */
static void stream_bubble(int k, int n, float rate, fz_bubble_t *b)
{
    const uint32_t s = (uint32_t)k, m = (uint32_t)n;
    b->x0 = 16.f + 208.f * hash01(s, m, 1);
    b->r0 = 1.8f + 2.6f * hash01(s, m, 2);
    b->v0 = (18.f + 22.f * hash01(s, m, 3)) * rate;
    b->wob = 1.5f + 3.5f * hash01(s, m, 4);
    b->freq = 2.f + 2.5f * hash01(s, m, 5);
    b->ph = UI_TAU * hash01(s, m, 6);
    b->col = (int)(hash01(s, m, 7) * 4.f);
    b->hold = 0.f;
    if (hash01(s, m, 8) < 0.14f && b->x0 > 44.f && b->x0 < 196.f) {   /* the odd one clings to a letter */
        b->cling_y = 86.f + 34.f * hash01(s, m, 9);
        b->hold = 0.5f + 0.9f * hash01(s, m, 10);
    }
}

/* a beat's burst: bubble j of beat n (the downbeat's bigger and wider) */
static void burst_bubble(long n, int j, int down, fz_bubble_t *b)
{
    const uint32_t s = (uint32_t)n * 2654435761u, m = (uint32_t)j;
    const float c = 120.f + (hash01(s, 99, 1) - 0.5f) * 120.f;   /* the burst's centre */
    b->x0 = c + (hash01(s, m, 2) - 0.5f) * (down ? 150.f : 70.f);
    b->x0 = b->x0 < 10.f ? 10.f : b->x0 > 230.f ? 230.f : b->x0;
    b->r0 = (down ? 2.2f : 1.6f) + 2.4f * hash01(s, m, 3);
    b->v0 = 48.f + 40.f * hash01(s, m, 4);
    b->wob = 1.f + 2.5f * hash01(s, m, 5);
    b->freq = 3.f + 3.f * hash01(s, m, 6);
    b->ph = UI_TAU * hash01(s, m, 7);
    b->col = (int)(hash01(s, m, 8) * 4.f);
    b->hold = 0.f;
}

void ui_idle_draw(const ui_idle_t *v)
{
    const float t = v->t;
    const float bpm = (float)(v->bpm > 0 ? v->bpm : 120);
    const float rate = v->playing ? 0.8f + 0.6f * bpm / 120.f : 1.f;
    char bpms[12];
    int k, n;

    {   /* the surface: a thin line, a slow ripple */
        float pts[2 * 25];
        for (k = 0; k < 25; k++) {
            const float x = 8.f + (float)k * 9.33f;
            pts[2 * k] = x;
            pts[2 * k + 1] = FZ_SURFACE + 1.2f * sinf(x * 0.045f + t * 1.4f);
        }
        ui_P(pts, 25, UI_G2, 1.f, 0, QD_NONE);
    }
    ui_T("FIZZ", 120.f, 128.f, 70.f, 200, 6.f, UI_K3, QD_CENTER);

    for (k = 0; k < FZ_NSTREAM; k++) {                  /* the lazy stream */
        const float period = (2.6f + 3.4f * hash01((uint32_t)k, 0, 11)) / (v->playing ? 1.f + 0.5f * bpm / 120.f : 1.f);
        const float off = period * hash01((uint32_t)k, 0, 12);
        const int now = (int)floorf((t - off) / period);
        for (n = now - 3; n <= now; n++) {
            fz_bubble_t b;
            stream_bubble(k, n, rate, &b);
            draw_bubble(&b, t - (off + (float)n * period));
        }
    }
    if (v->playing || v->since_stop >= 0.f) {           /* the beats' bursts (still rising a moment after a stop) */
        const long last = (long)floor(v->beats);
        const float spb = 60.f / bpm, extra = v->playing ? 0.f : v->since_stop;
        long bt;
        for (bt = last - 8; bt <= last; bt++) {
            const int down = bt >= 0 && bt % 4 == 0, m = down ? 9 : 4;
            const float age = (float)(v->beats - (double)bt) * spb + extra;
            int j;
            if (bt < 0 || age > 4.f)
                continue;
            for (j = 0; j < m; j++) {
                fz_bubble_t b;
                burst_bubble(bt, j, down, &b);
                draw_bubble(&b, age - 0.03f * (float)j);
            }
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
