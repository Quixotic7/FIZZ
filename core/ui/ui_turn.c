/* SPDX-License-Identifier: GPL-3.0-only */
/* A knob turned (mockups/line.html s21..s25, s16; PLAN.md §9a rule 5): the knob takes the whole screen while it turns.
 * line.html's takeover(): a knob mark and the name in the knob's colour at the top, the context grey at the right, a
 * drawing of what the knob does, the value big at the foot. A p-lock on a held step: a padlock by the name, the
 * track's own value ghosted (dashed grey in the drawings that have a position, small grey under the big value).
 *   FREQ   the filter's curve (s21)              DECAY  the envelope's decay, a red dot down it each beat (s22)
 *   HARM   the wave growing harmonics (s23)      FDBK   a spiral winding back into itself (s24)
 *   ALGO   FM TONE's routing: C, A, B1, B2 wired, modulators above their targets, carriers into the speaker
 *   TEMPO / SWING  s16's metronome, the BPM and the swing; swing over 50 % swings the pendulum unequal
 *   RATE   a picker over the pattern's clock and the track's (s25)
 *   LEN    the track's loop (its steps) over the eight pages
 *   LIMIT  the 16-voice pool, the limit outlined, the voices lit
 *   TSWING the track's swing on / off: a small metronome swinging unequal or even
 *   PAGE   (SELECT) the eight pages as rows of sixteen steps: the one edited outlined in the knob's colour with its
 *          trigs in it, the pages in the loop grey and the others dark, each one's start..end as a line under its
 *          row, the playing page's number red; "PAGE 3" big
 *   SCALE  (SHIFT + ALGORITHM) the scale's name big, the scales before and after it small above and below, its notes
 *          on a little one-octave keybed from the root (scale_mask: bit k, k semitones over the root), the root white
 *   ROOT   (SHIFT + PRESETS) the root big, the same: its neighbours, the scale's notes from it on the keybed
 * Every motion comes from the record (t, beat, beats, the clocks' positions). */
#include <math.h>

#include "ui.h"

static const float DASH[2] = {2.f, 3.f};

static void line2(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, col, lw, 0, QD_NONE);
}

/* ---------------------------------------------------------------- FREQ (s21's freqDraw) */
static void grid(void)
{
    float x;
    for (x = 30.f; x < 230.f; x += 26.f)
        line2(x, 36.f, x, 158.f, 0x141418u, 1.f);
    line2(14.f, 158.f, 226.f, 158.f, UI_G3, 1.f);
}

static float freq_y(float x, float xc)
{
    const float u = (x - xc) / 9.f, bump = 34.f * expf(-u * u), d = (x > xc ? x - xc : 0.f) * 1.9f;
    const float y = 78.f - bump + d;
    return y < 158.f ? y : 158.f;
}

static void freq_path(float xc, int closed)
{
    float x;
    qd_path_begin();
    if (closed)
        qd_move_to(14.f, 158.f);
    for (x = 14.f; x <= 226.f; x += 2.f) {
        if (x == 14.f && !closed)
            qd_move_to(x, freq_y(x, xc));
        else
            qd_line_to(x, freq_y(x, xc));
    }
    if (closed) {
        qd_line_to(226.f, 158.f);
        qd_path_close();
    }
}

static void d_freq(const ui_turn_t *v, qd_rgb col)
{
    const float xc = 20.f + v->v * 200.f;
    grid();
    qd_save();
    qd_alpha(0.16f);
    freq_path(xc, 1);
    qd_path_fill(col);
    qd_restore();
    if (v->lock) {
        qd_save();
        qd_dash(DASH, 2);
        freq_path(20.f + v->ghost_v * 200.f, 0);
        qd_path_stroke(UI_G2, 1.3f);
        qd_restore();
    }
    freq_path(xc, 0);
    qd_path_stroke(col, 1.6f);
    ui_O(xc, 78.f - 34.f, 3.f, QD_NONE, 1.f, col);
}

/* ---------------------------------------------------------------- DECAY (s22) */
static float decay_y(float x, float end) { return 158.f - 114.f * expf(-(x - 24.f) / (end - 24.f) * 3.f); }

static void decay_path(float end)
{
    float x;
    qd_path_begin();
    qd_move_to(16.f, 158.f);
    qd_line_to(24.f, 44.f);
    for (x = 24.f; x <= 226.f; x += 2.f)
        qd_line_to(x, decay_y(x, end));
}

static void d_decay(const ui_turn_t *v, qd_rgb col)
{
    const float end = 26.f + v->v * 330.f, dx = 24.f + v->beat * 200.f;
    grid();
    if (v->lock) {
        qd_save();
        qd_dash(DASH, 2);
        decay_path(26.f + v->ghost_v * 330.f);
        qd_path_stroke(UI_G2, 1.3f);
        qd_restore();
    }
    decay_path(end);
    qd_path_stroke(col, 1.6f);
    ui_O(24.f, 44.f, 3.f, QD_NONE, 1.f, col);
    ui_O(dx, decay_y(dx, end), 2.f, QD_NONE, 1.f, UI_RED);      /* a red dot down the curve each beat */
}

/* ---------------------------------------------------------------- HARM (s23) */
/* harmonic k's amplitude at h (0..26): k 1 always, the next ones fade in as h grows */
static float harm_a(int k, float h)
{
    float a;
    if (k == 1)
        return 1.f;
    a = h / 2.f - (float)(k - 2) * 0.6f;
    return (a < 0.f ? 0.f : a > 1.f ? 1.f : a) / (float)k;
}

/* the wave; a negative HARM (it shapes C) alternates the harmonics' signs */
static void harm_path(float hv, float t)
{
    const float h = fabsf(hv);
    float x, n = 0.f;
    int k;
    for (k = 1; k <= 7; k++)
        n += harm_a(k, h);
    qd_path_begin();
    for (x = 14.f; x <= 226.f; x += 1.5f) {
        const float u = (x - 14.f) / 212.f * UI_TAU * 2.f - t * 3.f;
        float y = 0.f;
        for (k = 1; k <= 7; k++)
            y += harm_a(k, h) * sinf((float)k * u) * (hv < 0.f && !(k & 1) ? -1.f : 1.f);
        y = 98.f - y / (n * 0.7f > 1.f ? n * 0.7f : 1.f) * 50.f;
        if (x == 14.f)
            qd_move_to(x, y);
        else
            qd_line_to(x, y);
    }
}

static void d_harm(const ui_turn_t *v, qd_rgb col)
{
    const float h = (float)v->value / 100.f;
    int k;
    line2(14.f, 98.f, 226.f, 98.f, UI_G3, 1.f);
    if (v->lock) {
        qd_save();
        qd_dash(DASH, 2);
        harm_path(v->ghost_v * 52.f - 26.f, v->t);
        qd_path_stroke(UI_G2, 1.3f);
        qd_restore();
    }
    harm_path(h, v->t);
    qd_path_stroke(col, 1.6f);
    for (k = 1; k <= 7; k++) {                          /* the comb: which harmonics sound */
        const float a = harm_a(k, fabsf(h)), x = 60.f + (float)k * 16.f;
        line2(x, 168.f, x, 168.f - a * 18.f, a > 0.f ? col : UI_G3, 2.f);
    }
}

/* ---------------------------------------------------------------- FDBK (s24) */
static void spiral(float value, float t, float *ex, float *ey)
{
    const float turns = 1.f + value / 9.f, cx = 120.f, cy = 100.f, end = turns * UI_TAU;
    float a, x = cx, y = cy;
    qd_path_begin();
    for (a = 0.f; a <= end; a += 0.08f) {
        const float r = 4.f + a / end * 58.f;
        x = cx + cosf(a - t * 2.f) * r, y = cy + sinf(a - t * 2.f) * r;
        if (a == 0.f)
            qd_move_to(x, y);
        else
            qd_line_to(x, y);
    }
    *ex = x, *ey = y;
}

static void d_fdbk(const ui_turn_t *v, qd_rgb col)
{
    float x, y;
    if (v->lock) {
        qd_save();
        qd_dash(DASH, 2);
        spiral(v->ghost_v * 127.f, v->t, &x, &y);
        qd_path_stroke(UI_G2, 1.2f);
        qd_restore();
    }
    spiral((float)v->value, v->t, &x, &y);
    qd_path_stroke(col, 1.5f);
    ui_O(x, y, 3.f, QD_NONE, 1.f, col);
}

/* ---------------------------------------------------------------- ALGO */
/* FM TONE's eight routings, from core/voice/quad/eng_quad.c QUAD_ALGO: per algorithm the modulators of C, A, B1, B2
 * as bitmasks (bit 0 C, 1 A, 2 B1, 3 B2), the feedback operator, the X and Y outputs' carriers */
enum { OP_C, OP_A, OP_B1, OP_B2 };
static const struct {
    uint8_t mod[4], fb, x, y;
} ALGO[8] = {
    {{0x6, 0, 0x8, 0}, OP_A, 0x1, 0x4},  {{0x2, 0, 0x8, 0}, OP_B2, 0x1, 0x4}, {{0x2, 0, 0x2, 0x2}, OP_A, 0x9, 0x4},
    {{0x2, 0x4, 0x8, 0}, OP_B2, 0x1, 0x4}, {{0x2, 0xC, 0, 0}, OP_B1, 0x1, 0x2}, {{0xA, 0, 0xA, 0}, OP_A, 0x1, 0x4},
    {{0x2, 0, 0x8, 0}, OP_A, 0x3, 0xC},  {{0x2, 0, 0, 0}, OP_B1, 0x9, 0x4},
};

static void arrow(float x0, float y0, float x1, float y1, qd_rgb col)
{
    const float a = atan2f(y1 - y0, x1 - x0);
    const float h[6] = {x1 - cosf(a - 0.5f) * 5.f, y1 - sinf(a - 0.5f) * 5.f, x1, y1, x1 - cosf(a + 0.5f) * 5.f,
                        y1 - sinf(a + 0.5f) * 5.f};
    line2(x0, y0, x1, y1, col, 1.2f);
    ui_P(h, 3, col, 1.2f, 0, QD_NONE);
}

static void d_algo(const ui_turn_t *v)
{
    static const char *const NAME[4] = {"C", "A", "B1", "B2"};
    static const qd_rgb COL[4] = {UI_K1, UI_K2, UI_K3, UI_K3};   /* s12: KNOB 1..3 the ratios of C, A, B */
    const int al = (v->value < 1 ? 1 : v->value > 8 ? 8 : v->value) - 1;
    const float bus = 172.f, spx = 212.f;
    const unsigned car = ALGO[al].x | ALGO[al].y;
    int lev[4] = {0, 0, 0, 0}, it, o, t, row, maxlev = 0, nflow = 0;
    float x[4], y[4], left = 240.f, r;

    /* the rows: an operator one above the highest of its targets */
    for (it = 0; it < 4; it++)
        for (o = 0; o < 4; o++)
            for (t = 0; t < 4; t++)
                if ((ALGO[al].mod[t] >> o) & 1u && t != o && lev[o] < lev[t] + 1)
                    lev[o] = lev[t] + 1;
    for (o = 0; o < 4; o++)
        if (lev[o] > maxlev)
            maxlev = lev[o];
    r = maxlev >= 3 ? 12.f : 15.f;                      /* a chain of four: smaller, closer */
    /* the places: the bottom row spread, each row above over the mean of its targets, then kept apart */
    for (row = 0; row <= maxlev; row++) {
        int ids[4], n = 0, i, j;
        float want = 0.f, got = 0.f;
        for (o = 0; o < 4; o++)
            if (lev[o] == row)
                ids[n++] = o;
        for (i = 0; i < n; i++) {
            const int op = ids[i];
            y[op] = (maxlev >= 3 ? 146.f : 140.f) - (float)row * (maxlev >= 3 ? 32.f : 36.f);
            if (row == 0) {
                x[op] = 100.f - (float)(n - 1) * 25.f + (float)i * 50.f;
            } else {
                float s = 0.f;
                int m = 0;
                for (t = 0; t < 4; t++)
                    if ((ALGO[al].mod[t] >> op) & 1u && t != op)
                        s += x[t], m++;
                x[op] = m ? s / (float)m : 100.f;
            }
        }
        for (i = 1; i < n; i++)                         /* sort by place */
            for (j = i; j > 0 && x[ids[j]] < x[ids[j - 1]]; j--) {
                const int tmp = ids[j];
                ids[j] = ids[j - 1], ids[j - 1] = tmp;
            }
        for (i = 0; i < n; i++)
            want += x[ids[i]];
        for (i = 1; i < n; i++)
            if (x[ids[i]] < x[ids[i - 1]] + 44.f)
                x[ids[i]] = x[ids[i - 1]] + 44.f;
        for (i = 0; i < n; i++)
            got += x[ids[i]];
        for (i = 0; i < n; i++)
            x[ids[i]] += (want - got) / (float)(n ? n : 1);
    }

    /* the wires: modulator -> target, a dot flowing along each */
    for (t = 0; t < 4; t++)
        for (o = 0; o < 4; o++) {
            float dx, dy, d, u;
            if (!((ALGO[al].mod[t] >> o) & 1u) || t == o)
                continue;
            dx = x[t] - x[o], dy = y[t] - y[o], d = sqrtf(dx * dx + dy * dy);
            dx /= d, dy /= d;
            arrow(x[o] + dx * r, y[o] + dy * r, x[t] - dx * (r + 2.f), y[t] - dy * (r + 2.f), UI_G1);
            u = fmodf(v->t * 1.3f + (float)nflow++ * 0.37f, 1.f);
            ui_O(x[o] + dx * r + (dx * (d - 2.f * r - 2.f)) * u, y[o] + dy * r + (dy * (d - 2.f * r - 2.f)) * u, 1.6f,
                 QD_NONE, 1.f, UI_K3);
        }
    /* the feedback loop over its operator */
    {
        const int f = ALGO[al].fb;
        const float cx = x[f], cy = y[f] - r - 4.f;
        qd_arc(cx, cy, 5.5f, UI_PI * 0.75f, UI_PI * 2.25f, UI_G1, 1.2f);
    }
    /* the carriers into the speaker: down to the bus (one above the bottom row: from its right side) */
    for (o = 0; o < 4; o++) {
        if (!((car >> o) & 1u))
            continue;
        if (lev[o] == 0) {
            line2(x[o], y[o] + r, x[o], bus, UI_G1, 1.2f);
            if (x[o] < left)
                left = x[o];
        } else {
            const float p[6] = {x[o] + r, y[o], x[o] + r + 8.f, y[o], x[o] + r + 8.f, bus};
            ui_P(p, 3, UI_G1, 1.2f, 0, QD_NONE);
            if (x[o] + r + 8.f < left)
                left = x[o] + r + 8.f;
        }
    }
    line2(left, bus, spx - 10.f, bus, UI_G1, 1.2f);
    {   /* the speaker */
        const float b[8] = {spx - 10.f, bus - 5.f, spx - 4.f, bus - 5.f, spx - 4.f, bus + 5.f, spx - 10.f, bus + 5.f};
        const float c[8] = {spx - 4.f, bus - 5.f, spx + 3.f, bus - 11.f, spx + 3.f, bus + 11.f, spx - 4.f, bus + 5.f};
        ui_P(b, 4, UI_G1, 1.2f, 1, QD_NONE);
        ui_P(c, 4, UI_G1, 1.2f, 0, QD_NONE);
        qd_arc(spx + 3.f, bus, 7.f, -0.75f, 0.75f, UI_G1, 1.2f);
    }
    for (o = 0; o < 4; o++) {                           /* the operators */
        ui_O(x[o], y[o], r, COL[o], 1.5f, UI_BG);
        ui_T(NAME[o], x[o], y[o] + 3.5f, 9.f, 600, 1.f, COL[o], QD_CENTER);
    }
}

/* ---------------------------------------------------------------- TEMPO / SWING / TSWING (s16's metronome) */
/* the metronome as s16 draws it, moved so its pendulum's pivot (190, 180) sits at (px, py), at scale s; the pendulum
 * swings once a beat; with swing over 50 the two swings of a pair of beats take swing % and the rest */
static void metronome(float px, float py, float s, int beats, float beat, int swing, int dots)
{
    const float sf = (float)(swing < 50 ? 50 : swing > 80 ? 80 : swing) / 100.f;
    const float pr = ((float)(beats & 1) + beat) / 2.f;
    float f, side, ang, L = 116.f, tx, ty, wx, wy;
    int i;
    if (pr < sf)
        f = pr / sf, side = -1.f;
    else
        f = (pr - sf) / (1.f - sf), side = 1.f;
    ang = side * cosf(UI_PI * f) * 0.42f;
    qd_save();
    qd_translate(px - 190.f * s, py - 180.f * s);
    qd_scale(s, s);
    {
        const float body[8] = {158.f, 220.f, 222.f, 220.f, 204.f, 50.f, 176.f, 50.f};
        ui_P(body, 4, UI_G1, 1.5f, 1, QD_NONE);
    }
    line2(166.f, 190.f, 214.f, 190.f, UI_G2, 1.f);
    tx = 190.f + sinf(ang) * L, ty = 180.f - cosf(ang) * L;
    line2(190.f, 180.f, tx, ty, UI_K3, 1.5f);
    wx = 190.f + sinf(ang) * L * 0.62f, wy = 180.f - cosf(ang) * L * 0.62f;
    ui_RR(wx - 6.f, wy - 4.f, 12.f, 8.f, 2.f, UI_K3, 1.2f, UI_BG);
    ui_O(190.f, 180.f, 2.5f, QD_NONE, 1.f, UI_G1);
    if (dots)                                           /* the bar's beats */
        for (i = 0; i < 4; i++) {
            const int on = i == ((beats % 4) + 4) % 4;
            ui_O(172.f + (float)i * 12.f, 205.f, 2.5f, on ? QD_NONE : UI_G2, 1.f, on ? UI_K4 : QD_NONE);
        }
    qd_restore();
}

static void d_tempo(const ui_turn_t *v, qd_rgb col)
{
    char s[16];
    const int sw = v->kind == UI_TK_SWING;
    int n;
    ui_T(ui_itoa(s, v->bpm), 10.f, 110.f, 66.f, 200, 0.f, sw ? UI_G1 : col, QD_LEFT);
    ui_lab("BPM", 12.f, 130.f, UI_G1, QD_LEFT);
    ui_itoa(s, v->swing);
    for (n = 0; s[n]; n++)
        ;
    s[n] = '%', s[n + 1] = 0;
    ui_T(s, 10.f, 190.f, sw ? 40.f : 32.f, 200, 0.f, UI_K2, QD_LEFT);
    ui_lab("SWING", 12.f, 206.f, sw ? UI_K2 : UI_G1, QD_LEFT);
    metronome(190.f, 180.f, 1.f, v->beats, v->beat, v->swing, 1);
}

/* ---------------------------------------------------------------- RATE (s25) */
static void d_rate(const ui_turn_t *v, qd_rgb col)
{
    char name[16], n[12];
    int c, k;
    name[0] = 0;
    {
        const char *a = "TRACK ";
        int i = 0, j = 0;
        while (a[i])
            name[j++] = a[i++];
        ui_itoa(n, v->track);
        for (i = 0; n[i]; i++)
            name[j++] = n[i];
        name[j] = 0;
    }
    for (c = 0; c < 2; c++) {                           /* the pattern's clock, the track's */
        const float cx = c ? 162.f : 78.f, pos = c ? v->track_pos : v->pat_pos;
        const qd_rgb cc = c ? col : UI_G2;
        const float a = -UI_PI / 2.f + pos / 16.f * UI_TAU;
        ui_O(cx, 92.f, 34.f, cc, 1.5f, QD_NONE);
        for (k = 0; k < 16; k++) {
            const float b = -UI_PI / 2.f + (float)k / 16.f * UI_TAU;
            ui_O(cx + cosf(b) * 34.f, 92.f + sinf(b) * 34.f, 1.3f, QD_NONE, 1.f, cc);
        }
        line2(cx, 92.f, cx + cosf(a) * 26.f, 92.f + sinf(a) * 26.f, c ? UI_RED : UI_G1, 1.5f);
        ui_T(c ? name : "PATTERN", cx, 146.f, 8.f, 600, 1.f, UI_G1, QD_CENTER);
    }
    if (v->prev)
        ui_T(v->prev, 30.f, 214.f, 16.f, 300, 0.f, UI_G2, QD_CENTER);
    if (v->next)
        ui_T(v->next, 210.f, 214.f, 16.f, 300, 0.f, UI_G2, QD_CENTER);
    ui_T(v->val, 120.f, 222.f, 52.f, 200, 0.f, col, QD_CENTER);
}

/* ---------------------------------------------------------------- LEN: the track's length over its 8 pages */
static void d_len(const ui_turn_t *v, qd_rgb col)
{
    char n[12];
    int p;
    for (p = 0; p < 8; p++) {
        const float x = 14.f + (float)p * 27.f, y = 64.f, w = 23.f, h = 76.f;
        const int st = v->value - p * 16, in = st > 0;
        const float f = (float)(st < 0 ? 0 : st > 16 ? 16 : st) / 16.f;
        ui_RR(x, y, w, h, 3.f, in ? col : UI_G3, in ? 1.2f : 1.f, QD_NONE);
        if (in) {                                       /* the steps in it, filled as LEN's held bar is */
            qd_save();
            qd_alpha(0.3f);
            ui_RR(x + 3.f, y + 3.f, (w - 6.f) * f, h - 6.f, 1.5f, QD_NONE, 1.f, col);
            qd_restore();
        }
        ui_T(ui_itoa(n, p * 16 + 1), x + w / 2.f, 156.f, 8.f, 500, 0.f, in ? UI_G1 : UI_G2, QD_CENTER);
    }
    ui_lab("PAGES", 120.f, 50.f, UI_G2, QD_CENTER);
}

/* ---------------------------------------------------------------- LIMIT: the 16-voice pool */
static void d_limit(const ui_turn_t *v, qd_rgb col)
{
    char s[24], n[12];
    int i, k = 0;
    for (i = 0; i < 16; i++) {
        const float x = 29.f + (float)(i % 8) * 26.f, y = 76.f + (float)(i / 8) * 34.f;
        const int lim = i < v->value, use = i < v->voices;
        ui_O(x, y, 7.f, lim ? col : UI_G3, lim ? 1.5f : 1.2f, use ? (lim ? col : UI_G2) : QD_NONE);
    }
    ui_itoa(n, v->voices);
    for (i = 0; n[i]; i++)
        s[k++] = n[i];
    {
        const char *a = " SOUNDING";
        for (i = 0; a[i]; i++)
            s[k++] = a[i];
    }
    s[k] = 0;
    ui_lab(s, 120.f, 150.f, UI_G1, QD_CENTER);
}

/* ---------------------------------------------------------------- PAGE: the eight pages as rows */
static void d_page(const ui_turn_t *v, qd_rgb col)
{
    static const char *const NUM[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};
    int p, k;
    for (p = 0; p < 8; p++) {
        const float y = 42.f + (float)p * 17.f, x0 = 44.f, sp = 11.f;
        const int ed = p == v->page, in = (v->loop >> p) & 1;
        const int st = v->pstart[p] > 15 ? 15 : v->pstart[p];
        const int en = v->pend[p] < st ? st : v->pend[p] > 15 ? 15 : v->pend[p];
        const qd_rgb tc = ed ? col : in ? UI_G1 : UI_G2;
        if (ed)
            ui_RR(10.f, y - 8.f, 220.f, 16.f, 5.f, col, 1.2f, QD_NONE);
        ui_T(NUM[p], 22.f, y + 3.5f, 10.f, ed ? 600 : 500, 0.f, p == v->play_page ? UI_RED : tc, QD_CENTER);
        for (k = 0; k < 16; k++) {
            const float x = x0 + (float)k * sp + (float)(k / 4) * 2.f;
            if (k < st || k > en)
                ui_O(x, y, 0.9f, QD_NONE, 1.f, UI_G3);
            else if (v->trig[p][k])
                ui_O(x, y, 2.2f, QD_NONE, 1.f, tc);
            else
                ui_O(x, y, 1.4f, ed || in ? UI_G2 : UI_G3, 1.f, QD_NONE);
        }
        if (st > 0 || en < 15) {                        /* its own range, under the row */
            const float xa = x0 + (float)st * sp + (float)(st / 4) * 2.f;
            const float xb = x0 + (float)en * sp + (float)(en / 4) * 2.f;
            line2(xa - 2.f, y + 5.f, xb + 2.f, y + 5.f, tc, 1.f);
        }
    }
}

/* ---------------------------------------------------------------- SCALE / ROOT: a picker over a little keybed */
static void d_scale(const ui_turn_t *v, qd_rgb col)
{
    static const char *const NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int root = ((v->root % 12) + 12) % 12;
    const float kx = 36.f, kw = 168.f, ky = 150.f, kh = 54.f;   /* one octave: half of a two-octave ui_keyboard */
    int s;
    if (v->prev)
        ui_T(v->prev, 120.f, 48.f, 12.f, 400, 1.f, UI_G2, QD_CENTER);
    if (v->val[0]) {                                    /* the name, as big as fits */
        const float w = ui_TW(v->val, 40.f, 200, 0.f);
        ui_T(v->val, 120.f, 98.f, w > 216.f ? 40.f * 216.f / w : 40.f, 200, 0.f, col, QD_CENTER);
    }
    if (v->next)
        ui_T(v->next, 120.f, 128.f, 12.f, 400, 1.f, UI_G2, QD_CENTER);
    qd_save();
    qd_clip_rect(kx - 2.f, ky - 3.f, kw + 3.f, kh + 6.f);
    ui_keyboard(kx, ky, kw * 2.f, kh, 48, 0);
    qd_restore();
    for (s = 0; s < 12; s++) {                          /* the scale's notes from the root, folded into the octave */
        const int pc = (root + s) % 12;
        int b;
        float x;
        if (!((v->scale_mask >> s) & 1u))
            continue;
        x = ui_keyboard_x(kx, kw * 2.f, 48, 48 + pc, &b);
        ui_O(x, b ? ky + 22.f : ky + kh - 10.f, s ? 3.2f : 4.2f, QD_NONE, 1.f, s ? col : UI_K3);
    }
    {
        int b;
        const float x = ui_keyboard_x(kx, kw * 2.f, 48, 48 + root, &b);
        ui_T(NOTE[root], x, ky + kh + 14.f, 9.f, 600, 0.f, UI_K3, QD_CENTER);
    }
}

/* ---------------------------------------------------------------- the frame */
void ui_turn_draw(const ui_turn_t *v)
{
    const qd_rgb col = UI_K[v->knob & 3];
    const char *val = v->val;
    char pg[8] = "PAGE 1";
    ui_knob_mark(14.f, 12.f, v->v < 0.f ? 0.f : v->v > 1.f ? 1.f : v->v, col);
    ui_T(v->name, 26.f, 17.f, 11.f, 600, 1.f, col, QD_LEFT);
    if (v->lock)
        ui_lock(26.f + ui_TW(v->name, 11.f, 600, 1.f) + 4.f, 6.f, col, 1.f);
    if (v->ctx)
        ui_T(v->ctx, 232.f, 17.f, 11.f, 400, 1.f, UI_G2, QD_RIGHT);

    switch (v->kind) {
    case UI_TK_FREQ: d_freq(v, col); break;
    case UI_TK_DECAY: d_decay(v, col); break;
    case UI_TK_HARM: d_harm(v, col); break;
    case UI_TK_FDBK: d_fdbk(v, col); break;
    case UI_TK_ALGO: d_algo(v); break;
    case UI_TK_TEMPO:
    case UI_TK_SWING: d_tempo(v, col); val = 0; break;
    case UI_TK_RATE: d_rate(v, col); val = 0; break;
    case UI_TK_LEN: d_len(v, col); break;
    case UI_TK_LIMIT: d_limit(v, col); break;
    case UI_TK_PAGE:
        d_page(v, col);
        if (!val[0]) {
            pg[5] = (char)('1' + (v->page < 0 ? 0 : v->page > 7 ? 7 : v->page));
            val = pg;
        }
        break;
    case UI_TK_SCALE:
    case UI_TK_ROOT: d_scale(v, col); val = 0; break;
    case UI_TK_TSWING:
        metronome(120.f, 128.f, 0.62f, v->beats, v->beat, v->value ? (v->swing > 50 ? v->swing : 62) : 50, 0);
        if (!val[0])
            val = v->value ? "ON" : "OFF";
        break;
    default: break;
    }
    if (val)
        ui_T(val, 120.f, 222.f, 58.f, 200, 0.f, col, QD_CENTER);
    if (v->lock && v->ghost[0])
        ui_T(v->ghost, 120.f, 236.f, 8.f, 500, 0.f, UI_G2, QD_CENTER);
}

/* ---------------------------------------------------------------- a brief full-screen word */
void ui_confirm_draw(const char *big, const char *small, qd_rgb col)
{
    if (big) {                                          /* as big as fits */
        const float w = ui_TW(big, 34.f, 200, 0.f);
        ui_T(big, 120.f, 128.f, w > 224.f ? 34.f * 224.f / w : 34.f, 200, 0.f, col, QD_CENTER);
    }
    if (small)
        ui_T(small, 120.f, 152.f, 11.f, 400, 0.f, UI_G1, QD_CENTER);
}
