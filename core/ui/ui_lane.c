/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* Screen 2, the step view (PLAN.md §8b; mockups/line.html s2's frame): the page edited, drawn as the current hold
 * mode's lane inside the sixteen white keys' outlines (2026-11-13: COND, TRIG, FUNC, SLIDE and SOUND as a 4 x 4 grid
 * instead: see grid below; the lanes for them here went).
 *
 * The frame, every mode: the top line with the track's box and name, the mode's name small in its colour, the tempo
 * grey and the pattern; under it the eight pages as dashes, each drawn over its own start..end only (a dark hairline
 * for the rest), in the loop grey, out of it dark, the page edited white and thicker, the playing page's place red (on
 * the page edited: the playhead's own place along its dash). P1.3v: no tempo in the top line; page follow off a
 * crossed arrow (was FOLLOW OFF in 8 px). The sixteen outlines (the playhead's column lighter, a
 * held one white, a step outside the page's start..end dotted and empty), the red playhead, the beats as small dots
 * over the columns (P1.3s; were the step numbers under them); a page change slides the lane in from the side; page follow off: a small FOLLOW OFF left of the tempo.
 *
 * In the columns, the mode's lane. A step that overrides the mode is drawn in the mode's colour, a step on the track's
 * default grey, the step the playhead is in white; the track's default itself is drawn across the page (dashed) and
 * named at the foot, so what a default change would move is in sight:
 *   NOTE   the piano roll (P1.3): the notes as bars at their pitch and length, micro-timing off the column, a
 *          ratchet dashed into its hits; the default chord's notes as dashed lines; under a column its other
 *          overrides as a little stack of dots in their modes' colours (a chance, a condition, a sound lock, p-locks)
 *   VEL    a bar up from the floor as high as the velocity; the default a dashed line at its height
 *   LEN    the gate as a bar over the steps it covers, a row a step (a staircase, so long notes never hide each
 *          other); an overriding step shows the default's length dashed under its own
 *   MICRO  each step's start as a flag off its column's grid line (the offset joined at the foot); the default
 *          offset dashed in every column
 *   RATCH  the hits as a ladder of ticks up from the floor, each as wide as its hit's velocity (the ramp); the
 *          default count a dashed line
 *   CHNC   ten dots up the column, as many lit as the chance's tens; the default a dashed line
 *   P-LOCK (P1.3c, B11) the NOTE roll, the steps with p-locks in the padlock's colour, and under each column a padlock
 *          with its lock count (a held step's column outlined white: holding keeps this screen, the knobs lock it)
 * A LOCK step (trigless) in NOTE, VEL, LEN, MICRO and RATCH is a small ring: it plays no note. */
#include <math.h>
#include "fz_math.h"                   /* (the drawing's own sinf .. atan2f: the flash diet) */
#include <string.h>

#include "ui.h"

#define TOP 36.f                                        /* the outlines' top and bottom */
#define BOT 210.f
#define COLW 12.f
#define XEND (9.f + 15.f * 13.5f + 6.f + COLW)          /* the last outline's right edge */

static const float DASH[2] = {2.f, 3.f};

static __attribute__((noinline)) float col_x(int i) { return 9.f + (float)i * 13.5f + (float)(i / 4) * 2.f; }

#define line2 qd_line                           /* (a line: qd_gfx's own) */

static void dashed(float x0, float y0, float x1, float y1, qd_rgb col)
{
    qd_save();
    qd_dash(DASH, 2);
    line2(x0, y0, x1, y1, col, 1.f);
    qd_restore();
}

/* the default's colour: grey, or (P1.3d) the mode's while a knob turns the default */
static qd_rgb def_col = UI_G1;

/* the default's dashed line across the page */
static void def_line(float y) { dashed(col_x(0), y, XEND, y, def_col); }

/* the sequencer's ramp (core/seq/seq.c ramp_vel): hit h of n at velocity v; ramp 0 flat, 1 up, 2 down, 3 accent */
static int ramp_vel(int v, int ramp, int h, int n)
{
    int x = v;
    if (n > 1) {
        if (ramp == 1)
            x = v * (h + 1) / n;
        else if (ramp == 2)
            x = v * (n - h) / n;
        else if (ramp == 3)
            x = h ? v * 5 / 8 : v;
    }
    return x < 1 ? 1 : x > 127 ? 127 : x;
}

/* a column's colour: the playhead's white, an override the mode's, the track's default grey */
static __attribute__((noinline)) qd_rgb ccol(const ui_lane_t *v, const ui_lcol_t *c)
{
    return c->lit ? UI_K3 : c->ovr ? UI_MODE_COL[v->mode] : UI_G1;
}

/* a bar outlined in col, filled faintly (solid when lit) */
static void bar(float x, float y, float w, float h, qd_rgb col, int lit)
{
    const float r = (w < h ? w : h) / 2.f;
    if (!lit) {
        qd_save();
        qd_alpha(0.28f);
        ui_RR(x, y, w, h, r, QD_NONE, 1.f, col);
        qd_restore();
    }
    ui_RR(x, y, w, h, r, col, 1.2f, lit ? col : QD_NONE);
}

/* a trigless LOCK step: a small ring */
static void lock_ring(float cx, float y, const ui_lcol_t *c)
{
    ui_O(cx, y, 3.f, c->held ? UI_K3 : UI_G1, 1.2f, QD_NONE);
}

/* ---------------------------------------------------------------- NOTE: the piano roll */
static void l_note(const ui_lane_t *v)
{
    const int span = v->span > 0 ? v->span : 16;
    const float k = 150.f / (float)span;
    int i, j, r;
#define NY(n) (200.f - (float)((int)(n) - v->lo) * k)

    for (j = 0; j < v->def_nnote && j < 6; j++) {      /* the default chord, dashed */
        const float y = NY(v->def_note[j]);
        if (y >= TOP + 2.f && y <= BOT - 2.f)
            def_line(y);
    }
    qd_save();
    qd_clip_rect(0.f, TOP, 240.f, BOT - TOP);
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(col_x(i) + 6.f, 123.f, c);
        } else {
            const float x0 = col_x(i) + 1.f + (float)c->micro / 24.f * 13.5f;
            float w = c->len * 13.5f - 2.f;
            const int nr = c->ratch > 1 ? c->ratch : 1;
            const qd_rgb fill = c->lit ? UI_K3 : c->held ? QD_NONE : c->ovr ? UI_MODE_COL[v->mode] : UI_G1;
            const qd_rgb line = c->held && !c->lit ? UI_K3 : QD_NONE;
            if (!(w < 238.f - x0))                      /* INF and long notes: to the page's end */
                w = 238.f - x0;
            if (w < 3.f)
                w = 3.f;
            for (j = 0; j < c->nnote && j < 6; j++) {
                const float y = NY(c->note[j]);
                if (nr > 1) {
                    for (r = 0; r < nr; r++) {
                        const float pw = w / (float)nr - 1.5f;
                        ui_RR(x0 + (float)r * w / (float)nr, y - 2.f, pw < 1.5f ? 1.5f : pw, 4.f, 2.f, line, 1.f,
                              fill);
                    }
                } else {
                    ui_RR(x0, y - 2.f, w, 4.f, 2.f, line, 1.f, fill);
                }
            }
        }
    }
    qd_restore();
#undef NY
    for (i = 0; i < 16; i++) {                          /* the markers under a column: its other overrides */
        const ui_lcol_t *c = &v->col[i];
        const float mx = col_x(i) + 6.f;
        float y = 215.f;
        if (!c->in_range || !c->on)
            continue;
        if (c->chance != v->def_chance)
            ui_O(mx, y, 1.6f, QD_NONE, 1.f, UI_MODE_COL[UI_F_CHNC]), y += 4.5f;
        if (strcmp(c->cond, v->def_cond) != 0)
            ui_O(mx, y, 1.6f, QD_NONE, 1.f, UI_MODE_COL[UI_F_COND]), y += 4.5f;
        if (c->sound)
            ui_O(mx, y, 1.6f, QD_NONE, 1.f, UI_MODE_COL[UI_F_SOUND]), y += 4.5f;
        if (c->nlock)
            ui_O(mx, y, 1.6f, QD_NONE, 1.f, UI_LOCK_COL);
    }
}

/* ---------------------------------------------------------------- VEL: bars up from the floor */
static void l_vel(const ui_lane_t *v)
{
    const float floor = 204.f, H = 158.f;
    int i;
    def_line(floor - (float)v->def_vel / 127.f * H);
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float x = col_x(i);
        float h;
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(x + 6.f, floor - 6.f, c);
            continue;
        }
        h = (float)c->vel / 127.f * H;
        if (h < 6.f)
            h = 6.f;
        bar(x + 3.f, floor - h, 6.f, h, ccol(v, c), c->lit);
    }
}

/* ---------------------------------------------------------------- LEN: bars over the steps, a row a step */
static void len_bar(float x, float y, float len, qd_rgb col, int lit, int ghost)
{
    const int inf = !(len < 64.f);
    float w = len * 13.5f - 2.f;
    int cut = 0;
    if (inf || x + w > XEND + 1.f)
        w = XEND + 1.f - x, cut = 1;
    if (w < 4.f)
        w = 4.f;
    if (ghost) {
        qd_save();
        qd_dash(DASH, 2);
        ui_RR(x, y, w, 6.f, 3.f, col, 1.f, QD_NONE);
        qd_restore();
        return;
    }
    bar(x, y, w, 6.f, col, lit);
    if (cut) {                                          /* it goes on past the page: a chevron */
        const float a[6] = {XEND + 3.f, y, XEND + 6.f, y + 3.f, XEND + 3.f, y + 6.f};
        ui_P(a, 3, col, 1.2f, 0, QD_NONE);
    }
}

static void l_len(const ui_lane_t *v)
{
    int i;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float x = col_x(i) + 1.f, y = 43.f + (float)i * 10.2f;
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(x + 5.f, y + 3.f, c);
            continue;
        }
        if (c->ovr)                                     /* the default's length under it, dashed */
            len_bar(x, y, v->def_len, UI_G2, 0, 1);
        len_bar(x, y, c->len, ccol(v, c), c->lit, 0);
    }
}

/* ---------------------------------------------------------------- MICRO: the start flags off the grid */
static void l_micro(const ui_lane_t *v)
{
    const float k = 13.5f / 24.f, top = 72.f, foot = 200.f;
    int i;
    for (i = 0; i < 16; i++) {                          /* the grid, the default's offset dashed */
        const float cx = col_x(i) + 6.f;
        if (!v->col[i].in_range)
            continue;
        line2(cx, foot - 3.f, cx, foot + 3.f, UI_G2, 1.f);
        dashed(cx + (float)v->def_micro * k, top + 6.f, cx + (float)v->def_micro * k, foot,
               v->def_hot ? def_col : UI_G2);
    }
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float cx = col_x(i) + 6.f, sx = cx + (float)c->micro * k;
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(cx, foot - 10.f, c);
            continue;
        }
        line2(sx, foot, sx, top, col, 1.5f);
        {
            const float f[6] = {sx, top, sx + 7.f, top + 4.f, sx, top + 8.f};
            ui_P(f, 3, col, 1.2f, 1, col);
        }
        if (c->micro)                                   /* the offset, joined to the grid at the foot */
            line2(cx, foot, sx, foot, col, 1.5f);
        ui_O(cx, foot, 1.6f, QD_NONE, 1.f, col);
    }
}

/* ---------------------------------------------------------------- RATCH: the hits as a ladder */
static void l_ratch(const ui_lane_t *v)
{
    const float foot = 198.f, sp = 14.f;
    int i, h;
    def_line(foot - (float)((v->def_ratch < 1 ? 1 : v->def_ratch) - 1) * sp - sp / 2.f);
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float cx = col_x(i) + 6.f;
        const int n = c->ratch < 1 ? 1 : c->ratch > 8 ? 8 : c->ratch;
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(cx, foot, c);
            continue;
        }
        for (h = 0; h < n; h++) {
            const float w = 2.f + 8.f * (float)ramp_vel(127, c->ramp, h, n) / 127.f;
            line2(cx - w / 2.f, foot - (float)h * sp, cx + w / 2.f, foot - (float)h * sp, col, 2.f);
        }
    }
}

/* ---------------------------------------------------------------- CHNC: ten dots, lit by chance */
static void l_chnc(const ui_lane_t *v)
{
    const float foot = 196.f, sp = 15.f;
    int i, d;
    def_line(foot - (float)((v->def_chance + 5) / 10) * sp + sp / 2.f);
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float cx = col_x(i) + 6.f;
        const int nl = (c->chance + 5) / 10;
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        for (d = 0; d < 10; d++) {
            const float y = foot - (float)d * sp;
            if (d < nl)
                ui_O(cx, y, 2.4f, QD_NONE, 1.f, col);
            else
                ui_O(cx, y, 2.f, UI_G3, 1.f, QD_NONE);
        }
    }
}

/* ---------------------------------------------------------------- 2026-11-13: COND, TRIG, FUNC, SLIDE and SOUND as a
 * 4 x 4 grid (mockups/fizz-roll-grid.html, the takes the owner chose): a box a step, a row a beat. An empty step a dark
 * outline; a trig on the track's value a grey box, its value grey; a step with its own value tinted and outlined in the
 * mode's colour (SOUND: the sound's own, by its pool number; TRIG's LOCK: STEPS' violet), its value big. The playhead a
 * red bar along its box's foot (the box flashing white as it plays); held / selected outlined white.
 *   COND   an A:B a ring cut into B passes, the A-th lit, "A:B" inside; a named condition big; ALWAYS a whole ring
 *   TRIG   NOTE a disc, LOCK a padlock, TIE a ring with its slur over from the step before (STEPS' colours)
 *   FUNC   the function's icon big; a JUMP / LEAP its number beside it and a dashed arrow to the box it lands in
 *          (white while its step plays; a target off the page: none)
 *   SLIDE  pitch over the step: the note's dot rising to an arrow, TIME how far across the climb takes; LEGATO a slur
 *          under it; an OFF lock a crossed ring
 *   SOUND  the pool number big, its name under it */
#define GW 55.f
#define GH 44.f
static float gx(int i) { return 4.f + (float)(i & 3) * 59.f; }
static float gy(int i) { return 35.f + (float)(i >> 2) * 48.f; }

static void faint(float a)                              /* .. until qd_restore */
{
    qd_save();
    qd_alpha(qd_get_alpha() * a);
}

int ui_lane_jump_to(const ui_lane_t *v, int i)
{
    const int f = v->col[i].func;
    int t;
    if (!v->col[i].on || f < UI_FN_JUMP)
        return -1;
    if (f >= UI_FN_LEAP) {                              /* LEAP n: n on (round the page when it loops alone) */
        t = i + f - UI_FN_LEAP - 15;
        if (v->loop == 1u << (v->page & 7))
            t &= 15;
    } else {                                            /* JUMP n: the track's step n */
        t = f - UI_FN_JUMP - v->page * 16;
    }
    return t >= 0 && t < 16 && t != i ? t : -1;
}

/* the dashed arrow from box i's top right to box t's, bulging away from the grid's middle, on a black halo */
static FZ_COLD void jump_arrow(int i, int t, qd_rgb col)
{
    static const float DA[2] = {5.f, 3.5f};
    const float x0 = gx(i) + 39.5f, y0 = gy(i) + 6.f, x1 = gx(t) + 39.5f, y1 = gy(t) + 9.f;
    const float mx = (x0 + x1) / 2.f, my = (y0 + y1) / 2.f, dx = x1 - x0, dy = y1 - y0;
    const float d = hypotf(dx, dy) + 0.01f, b = (d * 0.3f > 26.f ? d * 0.3f : 26.f) / d;
    float px = -dy * b, py = dx * b, ux, uy, n;
    int k;
    if ((mx - 120.f) * px + (my - 129.f) * py < 0.f)
        px = -px, py = -py;
    for (k = 0; k < 2; k++) {
        qd_save();
        if (k)
            qd_dash(DA, 2);
        qd_path_begin();
        qd_move_to(x0, y0);
        qd_quad_to(mx + px, my + py, x1, y1);
        qd_path_stroke(k ? col : UI_BG, k ? 2.2f : 6.f);
        qd_restore();
    }
    ux = x1 - mx - px, uy = y1 - my - py, n = hypotf(ux, uy) + 0.01f;
    ux = ux / n * 7.f, uy = uy / n * 7.f;               /* the head: 7 px back, 0.5 rad either side */
    {
        const float C = 0.8776f, S = 0.4794f;
        const float a[6] = {x1 + ux * 0.29f, y1 + uy * 0.29f, x1 - ux * C - uy * S, y1 - uy * C + ux * S,
                            x1 - ux * C + uy * S, y1 - uy * C - ux * S};
        ui_P(a, 3, UI_BG, 1.5f, 1, col);
    }
}

/* a function's icon (ui_kit's) at s, its strokes 1.9 times thicker */
static void big_icon(int f, float cx, float cy, float s, qd_rgb col)
{
    qd_save();
    qd_translate(cx, cy);
    qd_scale(1.9f, 1.9f);
    ui_icon(ui_fn_icon(f), 0.f, 0.f, s / 1.9f, col);
    qd_restore();
}

static __attribute__((noinline)) FZ_COLD void grid(const ui_lane_t *v)
{
    static const float DOTS[2] = {1.f, 4.f};
    const int mode = v->mode, ph = v->pos >= 0.f ? (int)v->pos : -1;
    const qd_rgb mc = UI_MODE_COL[mode];
    char s[12];
    int i, k;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float x = gx(i), y = gy(i), cx = x + GW / 2.f, cy = y + GH / 2.f;
        int own = c->ovr;
        qd_rgb col = mc;
        if (!c->in_range) {                             /* outside the page's start..end: dotted */
            qd_save();
            qd_dash(DOTS, 2);
            ui_RR(x, y, GW, GH, 7.f, UI_G3, 1.f, QD_NONE);
            qd_restore();
            continue;
        }
        if (!c->on) {
            ui_RR(x, y, GW, GH, 7.f, UI_G3, 1.f, QD_NONE);   /* (the mockup's near-black fill: not worth its pixels) */
            continue;
        }
        if (mode == UI_F_SOUND) {                       /* a sound its own colour (the track palette less white, and
                                                         * SOUND's lime) */
            k = (c->sound - 1) & 7;
            own = c->sound != 0;
            col = k == 7 ? mc : UI_TRACK_PAL[k + (k >= 2)];
        } else if (mode == UI_F_TRIG && c->type == 2) {
            col = UI_STEP_GHOST;
        }
        if (own)
            ui_RR(x, y, GW, GH, 7.f, col, 1.5f, QD_NONE);   /* (2026-11-14, owner: the boxes black inside) */
        else
            ui_RR(x, y, GW, GH, 7.f, UI_G2, 1.f, QD_NONE), col = UI_G1;
        if (i == ph) {                                  /* playing: a flash, fading over the step */
            faint(0.24f - 0.2f * (v->pos - (float)ph));
            ui_RR(x, y, GW, GH, 7.f, QD_NONE, 1.f, UI_K3);
            qd_restore();
        }
        switch (mode) {
        case UI_F_COND:
            if (!c->cond[0]) {                          /* ALWAYS: every pass */
                faint(0.55f);
                ui_O(cx, cy, 13.f, col, 4.f, QD_NONE);
                qd_restore();
            } else if (c->cond[1] == ':') {             /* A:B (2026-11-14, owner: no ring, the value big) */
                ui_T(c->cond, cx, cy + 8.5f, 24.f, 700, 0.f, col, QD_CENTER);
            } else {
                ui_T(c->cond, cx, cy + 6.5f, ui_TW(c->cond, 18.f, 700, 0.f) > 46.f ? 15.f : 18.f, 700, 0.f, col,
                     QD_CENTER);
            }
            break;
        case UI_F_TRIG:
            col = c->type == 2 ? UI_STEP_GHOST : UI_STEP_OWN;   /* (STEPS' colours, the box says own or not) */
            if (c->type == 2) {                         /* LOCK: a padlock */
                qd_arc(cx, cy - 7.f, 6.5f, UI_PI, UI_TAU, col, 3.f);
                qd_line(cx - 6.5f, cy - 7.f, cx - 6.5f, cy - 3.f, col, 3.f);
                qd_line(cx + 6.5f, cy - 7.f, cx + 6.5f, cy - 3.f, col, 3.f);
                ui_RR(cx - 11.5f, cy - 5.f, 23.f, 17.f, 4.f, col, 2.f, UI_BG);
                ui_O(cx, cy + 2.5f, 2.2f, QD_NONE, 1.f, col);
            } else if (c->type == 3) {                  /* TIE: a ring, its slur from the step before */
                ui_O(cx + 6.f, cy + 3.f, 9.f, col, 3.f, QD_NONE);
                if (i & 3) {
                    qd_path_begin();
                    qd_move_to(cx - 47.f, cy - 5.f);
                    qd_quad_to(cx - 26.5f, cy - 21.f, cx + 2.f, cy - 7.f);
                    qd_path_stroke(col, 3.f);
                }
            } else {
                ui_O(cx, cy, 10.f, QD_NONE, 1.f, col);
            }
            break;
        case UI_F_FUNC: {
            const int f = c->func;
            if (!f) {
                ui_O(cx, cy, 4.5f, QD_NONE, 1.f, col);
            } else if (f >= UI_FN_JUMP) {               /* JUMP n: its target's number; LEAP: its n, signed */
                const int lp = f >= UI_FN_LEAP, n = lp ? f - UI_FN_LEAP - 15 : f - UI_FN_JUMP + 1;
                s[0] = n < 0 ? '-' : '+';
                ui_itoa(s + 1, n < 0 ? -n : n);
                big_icon(f, cx - 9.f, cy + (lp ? -2.f : 1.f), lp ? 1.7f : 1.8f, col);
                ui_T(s + !lp, cx + 16.f, cy + (lp ? 6.f : 7.f), lp ? 16.f : 18.f, 700, 0.f, col, QD_CENTER);
            } else {
                big_icon(f, cx, cy, 2.5f, col);
            }
            break;
        }
        case UI_F_SLIDE:
            if (c->port == 1) {                         /* an OFF lock: a crossed ring */
                ui_O(cx, cy, 9.f, col, 2.8f, QD_NONE);
                qd_line(cx - 6.5f, cy + 6.5f, cx + 6.5f, cy - 6.5f, col, 2.8f);
            } else if (c->port) {                       /* ON / LEGATO: the glide, TIME how far across */
                const float x0 = x + 9.f, xe = x + GW - 8.f, y0 = cy + 6.f, y1 = cy - 10.f;
                const float xr = x0 + (xe - x0) * (0.12f + 0.88f * (float)(c->ptime > 127 ? 127 : c->ptime) / 127.f);
                const float xm = (x0 + xr) / 2.f, a[6] = {xe - 6.f, y1 - 4.5f, xe + 1.f, y1, xe - 6.f, y1 + 4.5f};
                qd_path_begin();
                qd_move_to(x0, y0);
                qd_cubic_to(xm, y0, xm, y1, xr, y1);
                qd_line_to(xe - 1.f, y1);
                qd_path_stroke(col, 3.2f);
                ui_O(x0, y0, 4.5f, QD_NONE, 1.f, col);
                ui_P(a, 3, col, 2.6f, 0, QD_NONE);
                if (c->port == 3) {                     /* LEGATO: the note held on */
                    qd_path_begin();
                    qd_move_to(x0 - 1.f, y0 + 6.f);
                    qd_quad_to((x0 + xe) / 2.f, y0 + 15.f, xe, y0 + 6.f);
                    qd_path_stroke(col, 2.4f);
                }
            } else {
                ui_O(cx, cy, 5.f, QD_NONE, 1.f, col);
            }
            break;
        default:                                        /* SOUND */
            if (!c->sound) {                            /* the track's sound */
                faint(0.7f);
                ui_O(cx, cy, 6.f, col, 2.5f, QD_NONE);
                qd_restore();
                break;
            }
            ui_T(ui_itoa(s, c->sound), cx, cy + 3.f, 24.f, 700, 0.f, col, QD_CENTER);
            if (c->sname) {
                for (k = 0; k < 7 && c->sname[k]; k++)
                    s[k] = c->sname[k];
                s[k] = 0;
                ui_T(s, cx, cy + 18.f, 12.f, 600, 0.f, col, QD_CENTER);
            }
            break;
        }
    }
    for (i = 0; i < 16 && mode == UI_F_FUNC; i++) {     /* where the jumps land */
        const int t = ui_lane_jump_to(v, i);
        if (t >= 0 && v->col[i].in_range)
            jump_arrow(i, t, i == ph ? UI_K3 : mc);
    }
    if (ph >= 0 && ph < 16) {                           /* the playhead: a red bar, glowing */
        const float x = gx(ph), y = gy(ph) + GH;
        faint(0.3f);
        ui_RR(x + 6.f, y - 7.f, GW - 12.f, 7.f, 3.5f, QD_NONE, 1.f, UI_RED);
        qd_restore();
        ui_RR(x + 8.f, y - 5.f, GW - 16.f, 3.f, 1.5f, QD_NONE, 1.f, UI_RED);
    }
    for (i = 0; i < 16; i++)                            /* held / selected */
        if (v->col[i].held)
            ui_RR(gx(i) - 3.f, gy(i) - 3.f, GW + 6.f, GH + 6.f, 10.f, UI_K3, 2.f, QD_NONE);
}

/* ---------------------------------------------------------------- the lane */
static void lane(const ui_lane_t *v)
{
    const int ph = v->pos >= 0.f ? (int)v->pos : -1;

    int i;

    if (v->mode >= UI_F_COND) {                         /* COND .. SOUND: the grid (a row a beat) */
        grid(v);
        return;
    }
    for (i = 0; i < 16; i++) {                          /* the keys' outlines */
        const ui_lcol_t *c = &v->col[i];
        if (!c->in_range) {                             /* outside the page's start..end: dark and dotted */
            static const float DOTS[2] = {1.f, 4.f};
            qd_save();
            qd_dash(DOTS, 2);
            ui_RR(col_x(i), TOP, COLW, BOT - TOP, 3.f, UI_G3, 1.f, QD_NONE);
            qd_restore();
            continue;
        }
        ui_RR(col_x(i), TOP, COLW, BOT - TOP, 3.f, c->held ? UI_K3 : UI_G3, 1.f, QD_NONE);   /* (P1.3s: the playhead's
                                                         * column no lighter: the red line says it) */
    }
    switch (v->mode) {
    case UI_F_VEL: l_vel(v); break;
    case UI_F_LEN: l_len(v); break;
    case UI_F_MICRO: l_micro(v); break;
    case UI_F_RATCH: l_ratch(v); break;
    case UI_F_CHNC: l_chnc(v); break;
    default: l_note(v); break;
    }
    if (ph >= 0 && ph < 16) {                           /* the playhead */
        const float px = col_x(ph) + (v->pos - (float)ph) * 13.5f;
        line2(px, 33.f, px, 213.f, UI_RED, 1.f);
    }
    for (i = 0; i < 16; i += 4)                         /* P1.3s: the beats as small dots over the columns (were
                                                         * the step numbers under them), the first a little brighter */
        ui_O(col_x(i) + COLW / 2.f, 32.f, i ? 1.1f : 1.3f, QD_NONE, 1.f, i ? UI_G2 : UI_G1);
}

/* ---------------------------------------------------------------- P1.3j: the STEPS view */
/* The track's eight pages as rows of sixteen steps in fours, the page edited unfolded: its row tall, its steps as the F2
 * map draws them (tall rounded cells) with the step's notes inside, a short bar at each note's pitch (the page's notes
 * fitted to the row, at least 16 semitones, as the piano roll). The other pages are compact rows above and below it: a
 * trig a soft white cell, a LOCK step a ring, a step outside its page's range a dark speck. A page change folds the old
 * row as the new one unfolds (grow[], eased).
 *   colours  P1.3q, a step's kind, open and folded alike: its own notes (the NOTE override) a warm white fill, the
 *            track's notes (a trig without it) a cyan fill, the notes inside as dark bars; a ghost (LOCK: no note) a
 *            violet outline with a dot and a ring; off an outline. A step overriding the mode: a thin bar of the mode's
 *            colour under it (not in NOTE, where the fill says it). Held or selected: outlined white, and on the open
 *            row its length as a line under the cells (from its micro offset; INF or past the page: an arrow). On the
 *            open row a fill's height is the velocity (from the bottom), a ghost's velocity a tick. The pages out of
 *            the loop dim
 *   motion   the playhead: a red marker over the unfolded row that hops from step to step,
 *            a red tick under a folded row's cell (P1.3s: no outline: only the selection is outlined); a trig pops
 *            as it plays (its fill white, fading) and its notes light red while it sounds; stopped, the unfolded row's
 *            outlines breathe (slowly) */
#define OV_X0 20.f
#define OV_TOP 33.f
/* P1.3p: the unfolded row at about half its old height (118), its space given to the folded rows: 7 x 20 + 62 = 202,
 * as before (7 x 12 + 118) */
#define OV_FOLD 20.f                                    /* a folded row's pitch (its cells 17 px) */
#define OV_OPEN 62.f                                    /* an unfolded row's (its cells OV_OPEN - 16) */
static float ov_x(int s) { return OV_X0 + (float)s * 13.f + (float)(s / 4) * 2.f; }
#define OV_W 11.5f

/* P1.3q: a held / selected step's length on the open row, under the cells: from the step (with its micro offset)
 * across as many cells as it lasts; INF, or a gate past the page's end: to the end, with an arrow */
static float ov_xf(float f)                             /* a step position (fractional) -> x */
{
    const int s = f < 0.f ? 0 : f >= 16.f ? 15 : (int)f;
    return ov_x(s) + (f - (float)s) * 13.f;
}
static void len_line(const ui_lcol_t *lc, int i, float y, qd_rgb col)
{
    const float f0 = (float)i + (float)lc->micro / 24.f, f1 = f0 + (lc->len > 0.15f ? lc->len : 0.15f);
    const int arrow = lc->inf || f1 > 16.f;
    const float x0 = ov_xf(f0) + 1.f, x1 = arrow ? ov_x(15) + OV_W + 2.f : ov_xf(f1) - 1.5f;
    line2(x0, y - 2.f, x0, y + 1.5f, col, 1.2f);       /* where it starts */
    line2(x0, y, x1 > x0 + 1.f ? x1 : x0 + 1.f, y, col, 1.5f);
    if (arrow) {
        const float tri[6] = {x1 + 3.f, y, x1 - 1.f, y - 2.5f, x1 - 1.f, y + 2.5f};
        ui_P(tri, 3, col, 1.f, 1, col);
    }
}

qd_rgb ui_steps_fill(uint8_t c)
{
    if (!(c & UI_C_RANGE) || !(c & UI_C_ON) || (c & UI_C_LOCK))
        return QD_NONE;
    return (c & UI_C_OWN) ? UI_STEP_OWN : UI_STEP_TRACK;
}
qd_rgb ui_steps_ink(uint8_t c)
{
    return (c & UI_C_OWN) ? 0x3a3326u : 0x10343bu;
}

/* one page's row at y, unfolded g (0..1) to an open height of open (P1.3r: EDIT draws the page edited alone, taller);
 * its height */
static float steps_row(const ui_lane_t *v, int p, float y, float g, float open)
{
    static const char *const NUM[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};
    const qd_rgb mc = UI_MODE_COL[v->mode];
    const int ph = v->play_step;
    int i, j;
    {
        const float rowh = OV_FOLD + (open - OV_FOLD) * g, ch = rowh - 3.f - 13.f * g;
        const int in = (v->loop >> p) & 1, ed = p == v->page;
        const float cy = y + (g > 0.f ? 10.f * g : 0.f);           /* (an unfolded row: room for the marker) */
        qd_save();
        qd_alpha(in || ed ? 1.f : 0.36f);
        ui_T(NUM[p], OV_X0 - 10.f, cy + (g > 0.5f ? 14.f : ch / 2.f + 4.5f), 12.f, ed ? 700 : 600, 0.f,
             ed ? UI_K3 : in ? UI_G1 : UI_G2, QD_CENTER);
        for (i = 0; i < 16; i++) {
            const uint8_t c = v->cell[p][i];
            const int abs = p * 16 + i;
            const float x = ov_x(i), r = g > 0.5f ? 3.f : 2.f;
            float pop = 0.f;
            if (abs == v->flash_step && v->flash_age >= 0.f && v->flash_age < 0.3f && (c & UI_C_ON))
                pop = 1.f - v->flash_age / 0.3f;
            if (!(c & UI_C_RANGE)) {
                ui_O(x + OV_W / 2.f, cy + ch / 2.f, 0.9f, QD_NONE, 1.f, UI_G3);
                continue;
            }
            /* P1.3q: the step's kind: own notes warm white, the track's notes cyan (filled); a ghost (LOCK: no note)
             * a violet outline with a dot and its ring; off: an outline as before */
            {
                const int on = (c & UI_C_ON) != 0, lock = (c & UI_C_LOCK) != 0, open = g >= 0.5f;
                const qd_rgb fill = ui_steps_fill(c);
                const ui_lcol_t *lc = open && ed ? &v->col[i] : 0;
                const int sounding = lc && abs == v->flash_step && v->flash_age >= 0.f && v->flash_age < lc->len * 0.15f;
                const float e = 0.f;                    /* (P1.3s: a pop no longer grows the cell or rings it: only
                                                         * the selection is outlined) */
                if (fill != QD_NONE) {                  /* own / the track's notes: filled */
                    /* P1.3q: on the open row the fill's height is the velocity (from the bottom; the cell outlined) */
                    const float vf = lc ? (float)lc->vel / 127.f : 1.f;
                    const float fh = (ch + 2.f * e) * (vf < 0.12f ? 0.12f : vf > 1.f ? 1.f : vf);
                    const float fy = cy + ch + e - fh;
                    if (fh < ch + 2.f * e - 0.5f)
                        ui_RR(x - e, cy - e, OV_W + 2.f * e, ch + 2.f * e, r, UI_G2, 1.f, QD_NONE);
                    ui_RR(x - e, fy, OV_W + 2.f * e, fh, fh < 2.f * r ? fh / 2.f : r, QD_NONE, 1.f,
                          pop > 0.f ? UI_K3 : fill);
                    if (lc && lc->nnote) {              /* the notes: dark on the fill, light above it (red as they
                                                         * sound) */
                        const int span = v->span > 0 ? v->span : 16;
                        const float semi = (ch - 10.f) / (float)span;   /* P1.3p: a semitone thick (1.5 .. 3) */
                        const float bh = semi < 1.5f ? 1.5f : semi > 3.f ? 3.f : semi;
                        for (j = 0; j < lc->nnote && j < 6; j++) {
                            const float ny = cy + ch - 5.f - (float)((int)lc->note[j] - v->lo) * semi;
                            if (ny < cy + 2.f || ny > cy + ch - 2.f)
                                continue;
                            ui_RR(x + 2.f, ny - bh / 2.f, OV_W - 4.f, bh, bh / 2.f, QD_NONE, 1.f,
                                  sounding || pop > 0.f ? UI_RED : ny >= fy ? ui_steps_ink(c) : fill);
                        }
                    }
                } else if (on && lock) {                /* a ghost: violet outline, a dot, its ring */
                    const float cx = x + OV_W / 2.f, cyy = cy + ch / 2.f;
                    qd_save();
                    if (open && v->play_page < 0 && ed)   /* stopped: the open row breathes, slowly */
                        qd_alpha(0.62f + 0.38f * (0.5f + 0.5f * sinf(v->t * 2.2f + (float)i * 0.25f)));
                    ui_RR(x - e, cy - e, OV_W + 2.f * e, ch + 2.f * e, r, UI_STEP_GHOST, open ? 1.2f : 1.f, QD_NONE);
                    qd_restore();
                    ui_O(cx, cyy, open ? 3.5f : 2.6f, UI_STEP_GHOST, 1.f, QD_NONE);
                    ui_O(cx, cyy, 1.1f, QD_NONE, 1.f, UI_STEP_GHOST);
                    if (lc) {                           /* P1.3q: its velocity, a tick at that height */
                        const float ty = cy + ch - 3.f - (ch - 6.f) * (float)lc->vel / 127.f;
                        line2(cx - 3.f, ty, cx + 3.f, ty, UI_STEP_GHOST, 1.2f);
                    }
                } else {                                /* off: as before */
                    qd_save();
                    if (open && v->play_page < 0 && ed)
                        qd_alpha(0.62f + 0.38f * (0.5f + 0.5f * sinf(v->t * 2.2f + (float)i * 0.25f)));
                    ui_RR(x, cy, OV_W, ch, r, open ? UI_G2 : UI_G3, 1.f, QD_NONE);
                    qd_restore();
                }
                if (on && (c & UI_C_OVR) && v->mode != UI_F_NOTE)   /* the mode's bar under it (not NOTE: the fill) */
                    line2(x + 2.5f, cy + ch + (open ? 2.5f : 1.5f), x + OV_W - 2.5f, cy + ch + (open ? 2.5f : 1.5f), mc,
                          1.5f);
                if (lc && fill != QD_NONE && (c & UI_C_HELD))   /* P1.3q: a held / selected step's length */
                    len_line(lc, i, cy + ch + 5.5f, fill);
                if (c & UI_C_TIE) {                     /* a TIE: a slur in from the step before (black under it) */
                    int pass;
                    for (pass = 0; pass < 2; pass++) {
                        qd_path_begin();
                        qd_move_to(x - 4.f, cy + 6.f);
                        qd_quad_to(x - 1.f, cy - 1.f, x + 3.f, cy + 4.f);
                        qd_path_stroke(pass ? UI_MODE_COL[UI_F_TRIG] : 0x000000u, pass ? 1.2f : 3.f);
                    }
                }
            }
            if ((c & UI_C_FUNC) && (c & UI_C_RANGE))   /* P1.3w: a step function: a dot in FUNC's colour */
                ui_O(x + OV_W - 1.5f, cy + 1.5f, 2.f, UI_BG, 1.f, UI_MODE_COL[UI_F_FUNC]);
            if (abs == ph && g < 0.5f)                  /* the playhead on a folded row: a red tick under the cell
                                                         * (P1.3s: no outline, which only the selection has) */
                line2(x + 2.f, cy + ch + 1.5f, x + OV_W - 2.f, cy + ch + 1.5f, UI_RED, 1.5f);
            if (c & UI_C_HELD)
                ui_RR(x - 2.f, cy - 2.f, OV_W + 4.f, ch + 4.f, r + 1.5f, UI_K3, 1.3f, QD_NONE);
        }
        if (g >= 0.5f && ph >= p * 16 && ph < p * 16 + 16) {   /* the marker: hops step to step (no bounce) */
            const float hx = ov_x(ph % 16) + OV_W / 2.f, tri[6] = {hx - 3.5f, cy - 8.f, hx + 3.5f, cy - 8.f, hx, cy - 3.f};
            ui_P(tri, 3, UI_RED, 1.f, 1, UI_RED);
            if (v->pos >= 0.f)
                line2(hx, cy + ch + 1.f, hx, cy + ch + 5.f, UI_RED, 1.f);
        }
        qd_restore();
        return rowh;
    }
}

static void steps_grid(const ui_lane_t *v)
{
    float y = OV_TOP;
    int p;
    for (p = 0; p < 8; p++)
        y += steps_row(v, p, y, v->grow[p] < 0.f ? 0.f : v->grow[p] > 1.f ? 1.f : v->grow[p], OV_OPEN);
}

/* P1.3r: the EDIT view's row: the page edited alone, unfolded, at y (its cells open - 16 px tall) */
void ui_steps_row(const ui_lane_t *v, float y, float open)
{
    const int p = v->page >= 0 && v->page < 8 ? v->page : 0;
    steps_row(v, p, y, 1.f, open);
}

/* the default named at the foot (not NOTE: its chord is on the roll), a dashed swatch before it for the modes that
 * draw it dashed */
static void def_name(const ui_lane_t *v)
{
    const int sw = v->mode == UI_F_VEL || v->mode == UI_F_LEN || v->mode == UI_F_MICRO || v->mode == UI_F_RATCH ||
                   v->mode == UI_F_CHNC;
    float x = col_x(0);
    if (v->mode == UI_F_NOTE || !v->def_label[0])
        return;
    if (sw) {
        dashed(x, 219.f, x + 10.f, 219.f, def_col);
        x += 15.f;
    }
    ui_lab(v->def_label, x, v->mode >= UI_F_COND ? 236.f : 222.f, def_col, QD_LEFT);   /* (the grid: under it) */
}

void ui_lane_draw(const ui_lane_t *v)
{
    const int mode = v->mode >= 0 && v->mode < UI_NFIELD ? v->mode : UI_F_NOTE;
    ui_lane_t m;
    char bpm[12];
    float mx;
    int p;

    m = *v;                                             /* the mode, made safe */
    m.mode = mode;
    v = &m;
    def_col = v->def_hot ? UI_MODE_COL[mode] : UI_G1;

    ui_head(v->name, v->pattern, v->track, UI_K3, UI_G1);
    mx = ui_head_x(v->track) + ui_TW(v->name, 12.f, 600, 0.8f) + 8.f;   /* the mode in its colour after the name (P1.3v: 12 px; the
                                                         * tempo no longer here: its takeover and F3 show it) */
    ui_lab(UI_FIELD_NAME[mode], mx, 18.f, UI_MODE_COL[mode], QD_LEFT);
    mx += ui_TW(UI_FIELD_NAME[mode], 12.f, 600, 0.5f);
    (void)bpm;

    for (p = 0; p < 8 && !v->steps; p++) {              /* the pages: each over its start..end (STEPS: its rows) */
        const float x = 10.f + (float)p * 28.f, W = 22.f;
        const int s = v->pstart[p] > 15 ? 15 : v->pstart[p], e = v->pend[p] < s ? s : v->pend[p] > 15 ? 15 : v->pend[p];
        const float xs = x + (float)s / 16.f * W, xe = x + (float)(e + 1) / 16.f * W;
        const int in = (v->loop >> p) & 1, ed = p == v->page;
        if (s > 0 || e < 15)
            line2(x, 27.f, x + W, 27.f, 0x1c1c22u, 1.f);
        line2(xs, 27.f, xe, 27.f, ed ? UI_K3 : in ? UI_G1 : UI_G3, ed ? 2.f : 1.5f);
        if (p == v->play_page) {                        /* where the playhead is */
            if (ed && v->pos >= 0.f) {
                const float px = x + v->pos / 16.f * W;
                line2(px, 24.f, px, 30.f, UI_RED, 1.5f);
            } else {
                const float c = (xs + xe) / 2.f;
                line2(c - 3.f, 27.f, c + 3.f, 27.f, UI_RED, 2.f);
            }
        }
    }

    if (v->slide != 0.f && !v->steps) {                 /* a page change: the lane slides in (STEPS: the overview
                                                         * stays, its outline moves) */
        const float a = 1.f - fabsf(v->slide);
        qd_save();
        qd_alpha(a < 0.f ? 0.f : a);
        qd_translate(v->slide * 40.f, 0.f);
        lane(v);
        qd_restore();
    } else if (v->steps) {
        steps_grid(v);
    } else {
        lane(v);
    }
    if (!v->steps)
        def_name(v);
    if (v->latch) {                                     /* P1.3d: the step latch, after the mode */
        ui_lab("LATCH", mx + 8.f, 18.f, UI_K3, QD_LEFT);
        mx += 8.f + ui_TW("LATCH", 12.f, 600, 0.5f);
    }
    if (!v->follow) {                                   /* P1.3v: page follow off: an arrow into a bar, crossed (was
                                                         * FOLLOW OFF in 8 px), left of the pattern */
        const float x = 232.f - ui_TW(v->pattern ? v->pattern : "", 12.f, 500, 0.8f) - 16.f, y = 12.5f;
        line2(x - 6.f, y, x + 3.f, y, UI_G1, 1.3f);
        line2(x + 6.f, y - 6.f, x + 6.f, y + 6.f, UI_G1, 1.3f);
        line2(x - 7.f, y + 6.f, x + 8.f, y - 6.f, UI_K4, 1.3f);
    }
}
