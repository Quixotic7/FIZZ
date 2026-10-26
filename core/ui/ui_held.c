/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.3v (PLAN §9a's clarity rules): no text under 12 px, no explanatory lines, no value strip, no RAMP / STEPS / /24
 * words, the ruler's and the tape's numbers only the held step's, the SOUND bank line and lock count as padlocks; the
 * DEFAULT tag a 12 px word; B11's reset an icon. What follows is the original layout, so amended.
 * A held step's modes (mockups/line.html s4..s10, s19, s20; PLAN.md §9a rule 7, §8b): one layout. "STEP n · MODE"
 * top left, the track top right, the value big, the mode's drawing, the value strip at the foot (B1..B10 the mode's
 * ten values written in the keys, the step's filled in the mode's colour, the track's default underlined, B11 CLR
 * outlined while the step overrides the mode).
 * The value and the drawing's own part are in the mode's colour when the step overrides the mode; on the track's
 * default they are grey-white, with a small DEFAULT tag top right (the step plays the track's value). A part that
 * is another mode's value is drawn in that mode's colour (VEL's ramp ticks RATCH's, LEN's tie TRIG's, MICRO's strum
 * NOTE's, COND's chance CHNC's and CHNC's condition COND's).
 * is_default: the track's default for the mode (a default turned with nothing held, or its mode key held, P1.3e):
 * "TRACK DEFAULT · MODE", the drawing in the mode's colour, no strip; with the mode key held a "B11 RESET" mark at
 * the foot (lit while the default differs from the track's initial value). P1.3e: NOTE has six notes, the sixth in
 * ALGORITHM's pink (2026-10-26: KNOB 1 the whole chord, KNOB 2..4 notes 2..4, ALGORITHM note 5 while the step or B1
 * is physically held; SELECT the page; a default: the same). P1.3f: no legend (the colours are it); a sixth note (from
 * the keys) stays pink.
 *   NOTE   the chord name big (squeezing in), the notes on a keybed, their names under them; the entry octave and
 *          the track's scale small top left ("OCT 4 · D DORIAN"). P1.3c: five notes, each dot and name in the colour
 *          of the knob that turns it (notes 1..4 KNOB 1..4's blue, green, white, orange, note 5 ALGORITHM's pink)
 *          and the five knobs over the keybed's right end, filled where they hold a note
 *   VEL    (P1.5d) a caped stunt rider doing a wheelie, riding left: the wheelie as high as the velocity, the cape
 *          streaming, speed lines; an extra kick when the step plays; the ramp as four ticks top left. P1.5e: neon
 *   LEN    the note as a bar over a ruler of the next steps, the notes before and after it; TIE a slur from the
 *          note before
 *   MICRO  the step as a tape measure: the start flag, the strum's staircase, a slow red playhead lighting the notes
 *   RATCH  a ball bouncing count times along a dotted path, each bounce as high as its hit's velocity (the ramp)
 *   CHNC / COND  the condition big with its passes as circles; the chance as thirty dots, reshuffled each pass
 *          (CHNC: the chance big, the condition small)
 *   SOUND  the sound lock as a cassette (pool numbers of three digits), its neighbours peeking; the page's sound
 *          locks as dots; the bank of ten on the keys ("BANK 11-20")
 *   TRIG   NOTE / LOCK / TIE, the step's boxed, one line saying what it does
 * An empty step (type 0; TRIG aside) draws its mode grey with EMPTY STEP in place of the big value. Positions,
 * sizes and widths as line.html's screens. */
#include <math.h>
#include "fz_math.h"                   /* (the drawing's own sinf .. atan2f: the flash diet) */

#include "ui.h"

/* the colours a mode draws with: k[0] the mode's (grey-white on the track's default), k[1] the other mode's part;
 * grey for an empty step */
typedef struct {
    qd_rgb k[2];
    qd_rgb wh;                          /* white parts that are no value (the mallet, the ball, the chord name) */
    int empty;
    int nobig;                          /* P1.3r: EDIT draws the big value itself (the drawing scaled apart) */
    float pull;                         /* P1.3s, EDIT: CHNC's condition / COND's chance pulled up this much */
} pal_t;

#define DEF_COL 0xb4b4bcu               /* a value on the track's default: grey-white */

static char *cat(char *d, const char *s)
{
    char *e = d;
    while (*e)
        e++;
    while (s && *s)
        *e++ = *s++;
    *e = 0;
    return d;
}

/* a pool number with three digits ("015"), "---" for 0 */
static char *three(char *s, int n)
{
    if (n <= 0) {
        s[0] = s[1] = s[2] = '-';
    } else {
        s[0] = (char)('0' + (n / 100) % 10);
        s[1] = (char)('0' + (n / 10) % 10);
        s[2] = (char)('0' + n % 10);
    }
    s[3] = 0;
    return s;
}

/* a step number wrapped at the track's length, 1-based */
static int wrap_step(int s, int len)
{
    if (len < 1)
        len = 16;
    s = (s - 1) % len;
    return (s < 0 ? s + len : s) + 1;
}

/* the sequencer's ramp (core/seq/seq.c ramp_vel): hit h of n at velocity v; ramp QD_RAMP_* 0 flat, 1 up, 2 down,
 * 3 accent */
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

/* KNOB 1's value big at x, y, or EMPTY STEP */
static void big(const pal_t *p, const char *s, float x, float y, float size)
{
    if (p->nobig)
        return;
    if (p->empty)
        ui_T("EMPTY STEP", x, y - size * 0.32f, 14.f, 600, 0.8f, UI_G1, QD_LEFT);
    else
        ui_T(s, x, y, size, 200, 0.f, p->k[0], QD_LEFT);
}

static void line2(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, col, lw, 0, QD_NONE);
}

/* ---------------------------------------------------------------- NOTE (s4) */
/* P1.3t (the owner): the keybed low (just over EDIT's row), the step's notes as one readable line right over it, each in
 * its knob's colour in slot order (1 .. 4, A, the sixth grey; the colours are the legend: no 1 2 3 4 A marks, no
 * "A = ALL"), the dots on the keys; no names under the keys; only the scale's name small top left (no entry octave:
 * the octave takeover and the OCT LEDs show it). The chord's name big (the full screen) when it is one; else none */
#define KB_Y 140.f                                      /* the keybed's top (70 tall) */
static qd_rgb slot_col(int k) { return k == 4 ? UI_KALG : k < 4 ? UI_K[k] : UI_G1; }   /* (2026-10-26: KNOB 1..4 notes
                                                         * 1..4, ALGORITHM note 5) */

static void note_line(const ui_held_t *v, const pal_t *p, float y)
{
    const char *nm[6];
    qd_rgb col[6];
    float size = 15.f, w = 0.f, x;
    const float gap = 10.f;
    int n = 0, i, k;
    for (k = 0; k < 6; k++)                             /* slot order, each in its knob's colour */
        if (v->slot_note[k])
            for (i = 0; i < v->nnote && i < 6; i++)
                if (v->note[i] == v->slot_note[k]) {
                    nm[n] = v->note_name[i];
                    col[n++] = p->empty ? UI_G2 : slot_col(k);
                    break;
                }
    if (!n)                                             /* (no slots: the notes low to high) */
        for (i = 0; i < v->nnote && i < 6; i++) {
            nm[n] = v->note_name[i];
            col[n++] = p->k[0];
        }
    if (!n)
        return;
    for (i = 0; i < n; i++)
        w += ui_TW(nm[i], size, 500, 0.5f) + (i ? gap : 0.f);
    if (w > 220.f) {                                    /* six long names: a touch smaller, never overlapping */
        size *= 220.f / w;
        w = 220.f;
    }
    if (size < 12.f)                                    /* (P1.3v: never under 12 px) */
        size = 12.f;
    x = 120.f - w / 2.f;
    for (i = 0; i < n; i++) {
        ui_T(nm[i], x, y, size, 500, 0.5f, col[i], QD_LEFT);
        x += ui_TW(nm[i], size, 500, 0.5f) + gap * size / 15.f;
    }
}

/* the chord's name: the root big and thin, the quality as a superscript, squeezed in from narrow as it changes; centred
 * at cx, its baseline at y */
static void chord_name(const ui_held_t *v, const pal_t *p, float cx, float y, float size)
{
    const float sq = 0.55f + 0.45f * ui_ease(v->squeeze / 0.18f), qs = size * 0.43f;
    const float wr = ui_TW(v->root, size, 200, 0.f), wq = v->qual[0] ? ui_TW(v->qual, qs, 300, 0.f) : 0.f;
    const float w = (wr + wq + 2.f) * sq, x = cx - w / 2.f;
    qd_save();
    qd_translate(x, y);
    qd_scale(sq, 1.f);
    ui_T(v->root, 0.f, 0.f, size, 200, 0.f, p->wh, QD_LEFT);
    if (v->qual[0])
        ui_T(v->qual, wr + 2.f, -size * 0.49f, qs, 300, 0.f, p->wh, QD_LEFT);
    qd_restore();
}

static void f_note(const ui_held_t *v, const pal_t *p)
{
    int i, lo = 53, mn = 127;

    if (p->nobig) {
        /* (P1.3r: EDIT puts the chord's name in its head, the scale under it) */
    } else if (p->empty) {
        ui_T("EMPTY STEP", 120.f, 86.f, 14.f, 600, 0.8f, UI_G1, QD_CENTER);
    } else if (v->root[0]) {                            /* root big, quality small, squeezed in from narrow */
        chord_name(v, p, 120.f, 104.f, 70.f);
    }
    note_line(v, p, KB_Y - 9.f);

    for (i = 0; i < v->nnote && i < 6; i++)             /* the keybed from the F at or below the lowest note (the FM-1's keys start on F) */
        if (v->note[i] < mn)
            mn = v->note[i];
    if (v->nnote > 0 && mn >= 5)                        /* (no F under C0..E0: those stay off the keybed) */
        lo = mn - (mn + 7) % 12;
    ui_keyboard(14.f, KB_Y, 212.f, 70.f, lo, 0);
    for (i = 0; i < v->nnote && i < 6; i++) {
        int b, k;
        const int d = v->note[i] - lo;
        qd_rgb col = p->k[0];
        float x;
        if (d < 0 || d >= 24)
            continue;
        if (!p->empty)                                  /* the colour of the knob that turns it */
            for (k = 0; k < 6; k++)
                if (v->slot_note[k] == v->note[i]) {
                    col = slot_col(k);
                    break;
                }
        x = ui_keyboard_x(14.f, 212.f, lo, v->note[i], &b);
        ui_O(x, b ? KB_Y + 32.f : KB_Y + 60.f, 3.8f, QD_NONE, 1.f, col);
    }
    if (!p->nobig && !v->band && v->scale_name && v->scale_name[0])   /* the track's scale, small (P1.5p: not under
                                                                         * the knob band) */
        ui_lab(v->scale_name, 8.f, 37.f, UI_G1, QD_LEFT);
}

/* ---------------------------------------------------------------- VEL (P1.5d: the caped stunt rider) */
/* P1.5d (the owner; mockups/fizz-vel-rider.html rider(), wheel(), screen()): VEL is a caped stunt rider doing a wheelie,
 * riding to the left. The wheelie turns about the rear axle (the rear wheel stays on the road): pitch ((u - 0.12) /
 * 0.88)^1.15 x 55 degrees (0 below), u = vel / 127; the wheels spin faster with it, the cape droops when slow and
 * streams flat, waving, when fast; the road's dashes and the speed lines run right at the speed. The value big top
 * right, the ramp's ticks top left. When the step plays: an extra kick (the front wheel up a few degrees, settling) and
 * a puff of speed lines. The bike is drawn in its own frame (the rear hub at 0, 0, the front hub at 60, 0, facing +x)
 * and placed by rd_* (mirrored, turned and scaled here: no rotation in the gfx layer); strokes scale, never under 1.2 */
static float rd_tx, rd_ty, rd_a, rd_b, rd_c, rd_d, rd_k, rd_ty_lift;
static void rd_pt(float x, float y, float *X, float *Y)
{
    *X = rd_tx + rd_a * x + rd_b * y;
    *Y = rd_ty + rd_c * x + rd_d * y;
}
static float rd_lw(float lw) { lw *= rd_k; return lw < 1.2f ? 1.2f : lw; }

/* P1.5e (the owner's reference, mockups/png/fizz-vel-rider-reference.png; fizz-vel-rider.html version 2): NEON. P2.6d
 * (the owner, from the unit: "get rid of the glowing outlines maybe and some of the smaller details"): every line is
 * itself only (it was stroked three times, 3.4x and 2x its width faint under it); no fins, lamp, boot, inner wheel
 * rings or bus windows (the owner then: the cape's stars and the helmet's visor and sparkle back, single strokes). A shape is built
 * into the path by rn_build (a polyline, a curve through midpoints, a circle, an arc; in the rider's frame through
 * rd_pt, or the screen's with rd_screen) and stroked by rn_glow */
#define RN_BLUE 0x2fb4ffu                               /* the wheels, the frame, the rider's body, arm and helmet */
#define RN_RED 0xff3b4fu                                /* the cape, the tank, the seat, the legs, the boot */
#define RN_CREAM 0xffe7b0u                              /* the exhaust, the fork, the bars, the wheels' ticks */
static qd_rgb rn_blue = RN_BLUE, rn_red = RN_RED, rn_cream = RN_CREAM;   /* (an empty step: all grey) */
enum { RN_POLY, RN_CURVE, RN_CIRC, RN_ARC };
typedef struct {
    int kind, n, close;
    const float *p;
    float x, y, r, a0, a1;
} rn_shape_t;
static void rd_screen(void)                             /* (the screen's own frame: the speed lines) */
{
    rd_tx = rd_ty = rd_b = rd_c = 0.f;
    rd_a = rd_d = rd_k = 1.f;
}
static void rn_build(const rn_shape_t *s)
{
    float X, Y, MX, MY;
    int i;
    qd_path_begin();
    if (s->kind == RN_CIRC || s->kind == RN_ARC) {
        float a0 = 0.f, a1 = UI_TAU;
        rd_pt(s->x, s->y, &X, &Y);
        if (s->kind == RN_ARC) {                        /* (mirrored and turned: the angles flip, the arc runs back) */
            const float p = atan2f(-rd_c, -rd_a);
            a0 = UI_PI - s->a1 + p;
            a1 = UI_PI - s->a0 + p;
        }
        qd_move_to(X + cosf(a0) * s->r * rd_k, Y + sinf(a0) * s->r * rd_k);
        qd_path_arc(X, Y, s->r * rd_k, a0, a1);
        return;
    }
    rd_pt(s->p[0], s->p[1], &X, &Y);
    qd_move_to(X, Y);
    if (s->kind == RN_POLY) {
        for (i = 1; i < s->n; i++) {
            rd_pt(s->p[2 * i], s->p[2 * i + 1], &X, &Y);
            qd_line_to(X, Y);
        }
    } else {
        for (i = 0; i < (s->close ? s->n : s->n - 1); i++) {
            const int j = (i + 1) % s->n;
            rd_pt(s->p[2 * i], s->p[2 * i + 1], &X, &Y);
            rd_pt((s->p[2 * i] + s->p[2 * j]) * 0.5f, (s->p[2 * i + 1] + s->p[2 * j + 1]) * 0.5f, &MX, &MY);
            qd_quad_to(X, Y, MX, MY);
        }
        if (!s->close) {
            rd_pt(s->p[2 * (s->n - 1)], s->p[2 * (s->n - 1) + 1], &X, &Y);
            qd_line_to(X, Y);
        }
    }
    if (s->close)
        qd_path_close();
}
static void rn_glow(const rn_shape_t *s, qd_rgb col, float lw)
{
    rn_build(s);                                        /* P2.6d (the owner: the glow off, it cost the unit's frame
                                                         * most): the line alone */
    qd_path_stroke(col, rd_lw(lw));
}
static void n_line(const float *p, int n, qd_rgb col, float lw)
{
    const rn_shape_t s = {RN_POLY, n, 0, p, 0, 0, 0, 0, 0};
    rn_glow(&s, col, lw);
}
static void n_curve(const float *p, int n, qd_rgb col, float lw, int close)
{
    const rn_shape_t s = {RN_CURVE, n, close, p, 0, 0, 0, 0, 0};
    rn_glow(&s, col, lw);
}
static void n_circ(float x, float y, float r, qd_rgb col, float lw)   /* P2.6d: the drawing's own circle (a ring, no
                                                         * path) */
{
    float X, Y;
    rd_pt(x, y, &X, &Y);
    qd_circle(X, Y, r * rd_k, col, rd_lw(lw), QD_NONE);
}
static void n_seg(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    n_line(p, 2, col, lw);
}
static void n_arc(float x, float y, float r, float a0, float a1, qd_rgb col, float lw)
{
    const rn_shape_t s = {RN_ARC, 0, 0, 0, x, y, r, a0, a1};
    rn_glow(&s, col, lw);
}
static void n_sparkle(float x, float y, float r, qd_rgb col)   /* a star: two short strokes */
{
    n_seg(x - r, y, x + r, y, col, 1.2f);
    n_seg(x, y - r, x, y + r, col, 1.2f);
}
static void n_tube(const float *p, int n, qd_rgb col, float w)   /* a limb: a wide line, a black core (two lines) */
{
    const rn_shape_t s = {RN_POLY, n, 0, p, 0, 0, 0, 0, 0};
    n_line(p, n, col, w);
    rn_build(&s);
    qd_path_stroke(UI_BG, (w - 2.6f) * rd_k);
}

static void rd_wheel(float x, float y, float spin)      /* a blue ring, a red hub; a cream tick turning inside (P2.6d:
                                                         * the inner ring gone) */
{
    n_circ(x, y, 16.f, rn_blue, 1.6f);
    n_circ(x, y, 3.6f, rn_red, 1.4f);
    n_seg(x + cosf(spin) * 7.f, y + sinf(spin) * 7.f, x + cosf(spin) * 9.5f, y + sinf(spin) * 9.5f, rn_cream, 1.3f);
}

/* the bike and the rider in their own frame, facing +x: the rear hub at 0, 0, the front hub at 60, 0, the road at
 * y 16; u the velocity 0..1, t seconds */
static void rider(float u, float t)
{
    const float sx = 20.f, sy = -43.f, L = 30.f + 26.f * u, sag = (1.f - u) * 14.f;
    const float w1 = sinf(t * (5.f + 9.f * u)) * (1.f + 2.5f * u), w2 = sinf(t * (5.f + 9.f * u) + 1.7f) * (1.5f + 3.f * u);
    const float ex = sx - L, ey = sy + sag, spin = t * (3.f + 26.f * u);
    {   /* the cape, behind everything: a flag from the shoulders streaming back at shoulder height, its end forked;
         * slow it hangs a little and is short, fast it is long, flat and waving */
        const float cape[18] = {sx + 1.f, sy - 3.f, sx - L * 0.35f, sy - 5.f + sag * 0.3f + w1,
                                sx - L * 0.7f, sy - 6.f + sag * 0.7f + w2, ex - 4.f, ey - 9.f + w2,
                                ex + 4.f, ey - 1.f + w1 * 0.5f, ex - 2.f, ey + 6.f + w1,
                                sx - L * 0.6f, sy + 5.f + sag * 0.75f + w1, sx - L * 0.25f, sy + 6.f + sag * 0.3f,
                                sx - 1.f, sy + 5.f};
        n_curve(cape, 9, rn_red, 1.5f, 1);
        n_sparkle(sx - L * 0.45f, sy + sag * 0.45f + w1 * 0.5f, 2.6f, rn_cream);   /* (P2.6d: back, the owner) */
        n_sparkle(sx - L * 0.7f, sy - 1.f + sag * 0.7f + w2 * 0.5f, 1.8f, rn_cream);
    }
    /* the bike: wheels, the blue frame, the red tank and seat, the cream exhaust, fork and bars (P2.6d: the fins and
     * the lamp gone) */
    rd_wheel(0.f, 0.f, spin);
    rd_wheel(60.f, 0.f, spin);
    {
        static const float back[6] = {0, 0, 20, -10, 44, -18}, down[6] = {44, -18, 34, -2, 20, -10};
        static const float tank[12] = {16, -20, 22, -26, 40, -26, 46, -21, 40, -17, 22, -17};
        static const float seat[10] = {-4, -18, 0, -22, 16, -22, 18, -19, 4, -17};
        static const float exh[10] = {30, -3, 26, 1, 16, 2, 4, 1, -4, -3};
        static const float bars[6] = {49, -30, 45, -35, 41, -35};
        n_line(back, 3, rn_blue, 1.5f);                 /* the swingarm and the backbone */
        n_line(down, 3, rn_blue, 1.5f);                 /* the down tube */
        n_curve(tank, 6, rn_red, 1.5f, 1);
        n_curve(seat, 5, rn_red, 1.5f, 1);
        n_curve(exh, 5, rn_cream, 1.3f, 0);
        n_seg(60.f, 0.f, 49.f, -30.f, rn_cream, 1.6f);  /* the fork */
        n_line(bars, 3, rn_cream, 1.4f);
    }
    {   /* the rider: red legs as tubes, the blue body and arm, the blue helmet, its sparkle and visor (P2.6d: the boot
         * gone) */
        static const float leg[6] = {12, -26, 29, -21, 25, -6};
        static const float body[4] = {13, -26, 21, -44}, arm[6] = {22, -42, 33, -37, 43, -35};
        n_tube(leg, 3, rn_red, 5.2f);
        n_tube(body, 2, rn_blue, 6.f);
        n_tube(arm, 3, rn_blue, 4.6f);
        n_circ(26.f, -53.f, 8.5f, rn_blue, 1.6f);
        n_arc(27.f, -53.f, 6.f, -0.9f, 0.9f, rn_blue, 1.3f);
        n_sparkle(22.5f, -55.f, 2.2f, rn_cream);
    }
}

/* P1.5f / P1.5g (the owner: "something special" at the maximum, then "the motorcycle should visually stay in the
 * same spot"; fizz-vel-rider.html version 2, show()): VEL 127 is the stunt show under the same bike as 126 (its place,
 * its scale, its full wheelie). The world scrolls right under it at RD_SHOW_V px/s, once every RD_JUMP_S s: a cream
 * takeoff ramp (its foot met first, 26 wide, 10 high), eight neon buses in the tracks' colours (14 wide, 17 tall, 3
 * apart), a cream landing ramp. The rear wheel rides up the ramp, flies an arc 16 high over the buses (the nose up
 * by 0.18 sin(pi f)), lands on the far ramp; three firework bursts over the buses as their middle passes the bike.
 * rd_show draws the world and gives the bike's lift (px up) and pitch change; nothing else about the bike moves */
#define RD_JUMP_S 2.6f
#define RD_SHOW_V 200.f
#define RD_RAMP_W 26.f
#define RD_RAMP_H 10.f
#define RD_BUS_W 14.f
#define RD_BUS_GAP 3.f
#define RD_ARC 16.f
static void rd_fireworks(float x, float y, float age, float seed)   /* a burst: out, falling a little, fading */
{
    const float a0 = qd_get_alpha();
    int i;
    if (age < 0.f || age > 1.f)
        return;
    for (i = 0; i < 18; i++) {
        const float a = (float)i * UI_TAU / 18.f + seed, r = 6.f + 34.f * sqrtf(age);
        const float px = x + cosf(a) * r, py = y + sinf(a) * r + 14.f * age * age, tail = 5.f * (1.f - age);
        qd_save();
        qd_alpha(a0 * (1.f - age));
        n_seg(px, py, px - cosf(a) * tail, py - sinf(a) * tail, i % 3 == 0 ? rn_blue : i % 3 == 1 ? rn_red : rn_cream,
              1.3f);
        qd_restore();
    }
}
static void rd_show(float t, float rx, float gy, float *lift, float *dp)
{
    const float s = fmodf(t, RD_JUMP_S), off = RD_SHOW_V * s;
    const float F = -40.f + off, L = F - RD_RAMP_W;    /* the takeoff ramp: its foot (met first), its lip */
    const float B0 = L - 6.f, LL = B0 - 8.f * (RD_BUS_W + RD_BUS_GAP) - 4.f, LF = LL - RD_RAMP_W;   /* the buses, the
                                                         * landing ramp's lip and foot */
    const float L0 = -40.f - RD_RAMP_W, LL0 = L0 - 6.f - 8.f * (RD_BUS_W + RD_BUS_GAP) - 4.f;
    const float tTop = (rx - (L0 + LL0) / 2.f) / RD_SHOW_V;   /* when the middle of the buses passes the bike */
    int i;
    rd_screen();
    {
        const float r1[6] = {F, gy, L, gy - RD_RAMP_H, L, gy}, r2[6] = {LF, gy, LL, gy - RD_RAMP_H, LL, gy};
        n_line(r1, 3, rn_cream, 1.6f);
        n_line(r2, 3, rn_cream, 1.6f);
    }
    for (i = 0; i < 8; i++) {                           /* the buses: a body, a window line, two wheels */
        const float x = B0 - (float)(i + 1) * (RD_BUS_W + RD_BUS_GAP) + RD_BUS_GAP, h = 17.f, y = gy - h - 3.f;
        const float w = RD_BUS_W, r = 2.5f;
        const float b[16] = {x + r, y, x + w - r, y, x + w, y + r, x + w, y + h - r, x + w - r, y + h,
                             x + r, y + h, x, y + h - r, x, y + r};
        const rn_shape_t sh = {RN_POLY, 8, 1, b, 0, 0, 0, 0, 0};
        const qd_rgb col = rn_blue == RN_BLUE ? UI_TRACK_PAL[i] : UI_G2;
        if (x > 250.f || x + w < -10.f)
            continue;
        rn_glow(&sh, col, 1.4f);                        /* (P2.6d: the window line gone) */
        n_circ(x + 3.5f, gy - 3.f, 2.f, col, 1.1f);
        n_circ(x + w - 3.5f, gy - 3.f, 2.f, col, 1.1f);
    }
    *lift = 0.f, *dp = 0.f;
    if (rx <= F && rx > L) {                            /* up the takeoff ramp */
        *lift = RD_RAMP_H * (F - rx) / RD_RAMP_W;
    } else if (rx <= L && rx > LL) {                    /* the arc over the buses */
        const float f = (L - rx) / (L - LL);
        *lift = RD_RAMP_H + RD_ARC * 4.f * f * (1.f - f);
        *dp = 0.18f * sinf(UI_PI * f);
    } else if (rx <= LL && rx > LF) {                   /* down the landing ramp */
        *lift = RD_RAMP_H * (rx - LF) / RD_RAMP_W;
    }
    rd_fireworks(52.f, 70.f, (s - tTop) / 0.75f, 0.2f); /* fireworks over the buses, left of the number */
    rd_fireworks(26.f, 112.f, (s - tTop - 0.1f) / 0.75f, 1.1f);
    rd_fireworks(96.f, 52.f, (s - tTop - 0.18f) / 0.75f, 2.3f);
}

static void f_vel(const ui_held_t *v, const pal_t *p)
{
    const float u = v->vel < 0 ? 0.f : v->vel > 127 ? 1.f : (float)v->vel / 127.f;
    const float gy = 206.f, rx = 138.f, k = 1.4f, sp = 20.f + 260.f * u;
    const float play = v->play >= 0.f && v->play < 0.4f && !p->empty ? v->play / 0.4f : -1.f;
    float pitch = powf(u > 0.12f ? (u - 0.12f) / 0.88f : 0.f, 1.15f) * (55.f * UI_PI / 180.f);
    char s[12];
    int i, n;

    rn_blue = p->empty ? UI_G2 : RN_BLUE;
    rn_red = p->empty ? UI_G2 : RN_RED;
    rn_cream = p->empty ? UI_G1 : RN_CREAM;
    qd_save();
    qd_translate(0.f, -8.f);                            /* (the mock-up's own coordinates: the body is drawn 8 lower) */
    if (play >= 0.f && v->vel < 127)                    /* the step plays: an extra kick, the front wheel up, settling */
        pitch += sinf(play * UI_PI) * (1.f - play) * (9.f * UI_PI / 180.f);
    /* the road: its edge and its dashes running past (to the right) at the speed */
    line2(0.f, gy, 240.f, gy, UI_G2, 1.2f);
    for (i = 0; i < 6; i++) {
        const float x = fmodf(v->t * sp + (float)i * 48.f, 288.f) - 24.f;
        line2(x, gy + 12.f, x + 18.f, gy + 12.f, UI_G2, 1.6f);
    }
    /* speed lines behind (right of) the rear wheel: dashed rows, blue, cream and red, more and longer with the speed */
    rd_screen();
    n = 1 + (int)lroundf(2.f * u);
    for (i = 0; i < n && i < 3; i++) {
        const qd_rgb cols[3] = {rn_blue, rn_cream, rn_red};
        const float y = gy - 30.f + (float)i * 9.f, run = fmodf(v->t * sp * 0.6f + (float)i * 13.f, 26.f);
        const float L = 6.f + 14.f * u;
        int d;
        for (d = 0; d < 2; d++) {
            const float x = rx + 30.f + run + (float)d * (L + 6.f);
            n_seg(x, y, x + L, y, cols[i], 1.4f);
        }
    }
    if (play >= 0.f && v->vel < 127) {                  /* .. and a puff of them off the rear wheel as it kicks */
        qd_save();
        qd_alpha(qd_get_alpha() * (1.f - play));
        for (i = 0; i < 5; i++) {
            const float x = rx + 26.f + play * 40.f + (float)(i % 2) * 10.f, y = gy - 8.f - (float)i * 9.f;
            n_seg(x, y, x + 14.f + (float)(i % 3) * 5.f, y, i % 3 == 0 ? rn_blue : i % 3 == 1 ? rn_cream : rn_red,
                  1.3f);
        }
        qd_restore();
    }
    if (v->vel >= 127) {                                /* P1.5g: the stunt show under the same bike: its lift, its nose */
        float lift, dp;
        rd_show(v->t, rx, gy, &lift, &dp);
        if (v->play >= 0.f && v->play < 0.75f && !p->empty)   /* the step plays: a burst of its own over the buses */
            rd_fireworks(70.f, 92.f, v->play / 0.75f, 0.7f);
        pitch += dp;
        rd_ty_lift = lift;
    } else {
        rd_ty_lift = 0.f;
    }
    /* the wheelie: about the rear axle, mirrored to ride left */
    rd_k = k;
    rd_tx = rx, rd_ty = gy - 16.f * k - rd_ty_lift;
    rd_a = -k * cosf(pitch), rd_b = -k * sinf(pitch);
    rd_c = -k * sinf(pitch), rd_d = k * cosf(pitch);
    rider(u, v->t);
    /* the value big, top right; the ratchet's ramp as four ticks, top left */
    if (!p->nobig && !v->band) {                        /* (P1.5p: the band carries the value while it shows) */
        if (p->empty)
            big(p, "", 12.f, 84.f, 64.f);
        else
            ui_T(ui_itoa(s, v->vel), 232.f, 84.f, 64.f, 200, 0.f, p->k[0], QD_RIGHT);
    }
    for (i = 0; i < 4 && (p->nobig || !v->band); i++) {   /* (EDIT, nobig: a little lower, under its head) */
        const float x = 10.f + (float)i * 8.f, h = (float)ramp_vel(v->vel, v->ramp, i, 4) / 127.f * 16.f;
        const float y = p->nobig ? 80.f : 46.f;
        line2(x, y, x, y - (h < 1.f ? 1.f : h), p->k[1], 2.f);
    }
    qd_restore();
}

/* ---------------------------------------------------------------- LEN (s6) */
static void f_len(const ui_held_t *v, const pal_t *p)
{
    static const float dash[2] = {2.f, 3.f};
    const float x0 = 14.f, sw = 21.f, y = 172.f, xh = x0 + 2.f * sw;   /* the held step at ruler slot 2 */
    const int inf = v->len_steps < 0.f;
    float L = inf ? 8.f : v->len_steps, w;
    char s[12];
    int i;

    big(p, v->len_name, 12.f, 92.f, 64.f);
    /* the ruler: the steps from two before the held one, the beats' ticks taller */
    for (i = 0; i <= 10; i++) {
        const float x = x0 + (float)i * sw;
        const int st = wrap_step(v->step + i - 2, v->tracklen);
        line2(x, y + 10.f, x, y + ((st - 1) % 4 == 0 ? -2.f : 4.f), i == 2 ? UI_K3 : UI_G2, 1.f);
        if (i == 2)                                     /* (P1.3v: the held step's number only, 12 px) */
            ui_T(ui_itoa(s, st), x + sw / 2.f, y + 27.f, 12.f, 600, 0.f, UI_K3, QD_CENTER);
    }
    line2(x0, y + 10.f, x0 + 10.f * sw, y + 10.f, UI_G2, 1.f);

    qd_save();
    qd_clip_rect(0.f, 26.f, 240.f, 160.f);
    if (v->has_prev) {                                  /* the note before (TIE: green) and the slur from it */
        const float xp = x0 + (2.f + v->prev_at) * sw + 1.f, wp = v->prev_len * sw - 2.f;
        ui_RR(xp, y - 26.f, wp < 3.f ? 3.f : wp, 12.f, wp < 12.f ? (wp < 3.f ? 1.5f : wp / 2.f) : 6.f,
              v->tie ? p->k[1] : UI_G2, 1.2f, QD_NONE);
        qd_save();
        if (!v->tie)
            qd_dash(dash, 2);
        qd_path_begin();
        qd_move_to(xp + 8.f, y - 30.f);
        qd_quad_to((xp + 8.f + xh + 10.f) / 2.f, y - 62.f, xh + 10.f, y - 30.f);
        qd_path_stroke(v->tie ? p->k[1] : UI_G3, v->tie ? 1.5f : 1.2f);
        qd_restore();
    }
    if (v->next_at > 0.f) {                             /* the note after */
        const float xn = x0 + (2.f + v->next_at) * sw + 1.f, wn = v->next_len * sw - 2.f;
        ui_RR(xn, y - 26.f, wn < 3.f ? 3.f : wn, 12.f, wn < 12.f ? (wn < 3.f ? 1.5f : wn / 2.f) : 6.f, UI_G2, 1.2f,
              QD_NONE);
    }
    /* the held note: a blue bar from its step (INF: to the ruler's end, an arrow) */
    if (2.f + L > 10.f)
        L = 8.f;
    w = L * sw - 2.f - (inf ? 6.f : 0.f);
    if (w < 3.f)
        w = 3.f;
    qd_save();
    qd_alpha(0.22f);
    ui_RR(xh + 1.f, y - 26.f, w, 12.f, w < 12.f ? w / 2.f : 6.f, QD_NONE, 1.5f, p->k[0]);
    qd_restore();
    ui_RR(xh + 1.f, y - 26.f, w, 12.f, w < 12.f ? w / 2.f : 6.f, p->k[0], 1.5f, QD_NONE);
    if (inf) {
        const float xe = x0 + 10.f * sw + 2.f, a[6] = {xe - 4.f, y - 25.f, xe + 1.f, y - 20.f, xe - 4.f, y - 15.f};
        line2(xh + w - 1.f, y - 20.f, xe, y - 20.f, p->k[0], 1.5f);
        ui_P(a, 3, p->k[0], 1.5f, 0, QD_NONE);
    }
    qd_restore();
}

/* ---------------------------------------------------------------- MICRO (s7) */
static void f_micro(const ui_held_t *v, const pal_t *p)
{
    /* ticks -12 .. +36 around the step (24 a step); a start earlier than -12 shifts the window by half a step */
    const float off = v->micro < -12 ? -12.f : 0.f, base = 172.f;
    char s[16], n[12];
    float sx;
    int k, j;
#define MX(k) (22.f + ((float)(k) - off + 12.f) * 4.25f)

    s[0] = 0;
    cat(s, v->micro > 0 ? "+" : "");
    cat(s, ui_itoa(n, v->micro));
    big(p, s, 12.f, 82.f, 56.f);
    line2(10.f, base, 230.f, base, UI_G2, 1.f);
    for (k = -12 + (int)off; k <= 36 + (int)off; k++) {
        const int bg = k % 24 == 0, mid = k % 6 == 0;
        line2(MX(k), base, MX(k), base - (bg ? 76.f : mid ? 7.f : 3.f), bg && k == 0 ? UI_K3 : UI_G2, bg ? 1.2f : 1.f);
    }
    ui_T(ui_itoa(n, wrap_step(v->step, v->tracklen)), MX(0), base + 16.f, 12.f, 600, 0.f, UI_K3, QD_CENTER);
    /* the start flag (KNOB 1) */
    sx = MX(v->micro);
    line2(sx, base, sx, 98.f, p->k[0], 1.5f);
    {
        const float f[6] = {sx, 98.f, sx + 9.f, 102.f, sx, 106.f};
        ui_P(f, 3, p->k[0], 1.2f, 1, p->k[0]);
    }
    /* the strum's staircase (KNOB 2): note j at micro + j * strum, lit once the walk passes it */
    for (j = 0; j < v->nnote && j < 6; j++) {
        const int st = v->micro + j * v->strum;
        const float x = MX(st), yy = 158.f - (float)j * 11.f;
        const int on = v->walk >= (float)st;
        if (x > 236.f)
            continue;
        ui_O(x, yy, 2.2f, QD_NONE, 1.f, on ? p->k[1] : UI_G2);
        if (x + 3.f < 228.f)
            line2(x + 3.f, yy, 228.f, yy, on ? p->k[1] : UI_G2, 2.f);
    }
    if (v->walk > -12.f + off && v->walk < 36.f + off)  /* the slow red playhead */
        line2(MX(v->walk), 96.f, MX(v->walk), base, UI_RED, 1.f);
#undef MX
}

/* ---------------------------------------------------------------- RATCH (s8) */
static void f_ratch(const ui_held_t *v, const pal_t *p)
{
    static const float dots[2] = {1.f, 4.f};
    const float g = 200.f, H = 90.f;
    const int n = v->count < 1 ? 1 : v->count > 8 ? 8 : v->count;
    float xs[8], hv[8], u, bx, by;
    char s[16], num[12];
    int i, k;

    s[0] = 0;
    cat(cat(s, "\xc3\x97"), ui_itoa(num, n));           /* "×4" */
    big(p, s, 12.f, 82.f, 56.f);
    for (i = 0; i < n; i++) {                           /* the hits from x 30 to 180, each as high as its velocity */
        const float h = (float)ramp_vel(v->vel, v->ramp, i, n) / 127.f;
        xs[i] = n > 1 ? 30.f + (float)i * 150.f / (float)(n - 1) : 30.f;
        hv[i] = h < 0.06f ? 0.06f : h;
    }
#define ARC(i, uu, ox, oy)                                                                                         \
    do {                                                                                                           \
        const float xa = xs[i], xb = (i) < n - 1 ? xs[(i) + 1] : 226.f;                                            \
        const float hh = ((i) < n - 1 ? hv[(i) + 1] : 0.15f) * H * ((i) < n - 1 ? 1.f : 0.6f);                     \
        ox = xa + (xb - xa) * (uu);                                                                                \
        oy = g - 4.f - sinf((uu) * UI_PI) * hh;                                                                    \
    } while (0)
    line2(14.f, g, 226.f, g, UI_G2, 1.f);
    /* the dotted path: in from the top left, then a bounce after each hit */
    qd_save();
    qd_dash(dots, 2);
    qd_path_begin();
    qd_move_to(14.f, g - 4.f - H * hv[0]);
    qd_quad_to(24.f, g - 4.f - H * hv[0] * 0.6f, xs[0], g - 4.f);
    for (i = 0; i < n; i++)
        for (k = 1; k <= 24; k++) {
            float px, py;
            ARC(i, (float)k / 24.f, px, py);
            qd_line_to(px, py);
        }
    qd_path_stroke(UI_G1, 1.2f);
    qd_restore();
    for (i = 0; i < n; i++)                             /* KNOB 2, the ramp: the hits' velocities under the floor */
        line2(xs[i], g + 3.f, xs[i], g + 3.f + hv[i] * 12.f, p->k[1], 2.f);
    /* the ball, slowed down: in, then along the bounces */
    u = v->ball * ((float)n + 0.5f) - 0.5f;
    if (u < 0.f) {
        const float q = u + 0.5f;
        bx = 14.f + (xs[0] - 14.f) * q * 2.f;
        by = g - 4.f - H * hv[0] * (1.f - q * q * 4.f);
    } else {
        int j = (int)u;
        if (j > n - 1)
            j = n - 1;
        ARC(j, u - (float)j > 1.f ? 1.f : u - (float)j, bx, by);
    }
#undef ARC
    ui_O(bx, by, 5.f, p->wh, 1.5f, UI_BG);
}

/* ---------------------------------------------------------------- COND / CHNC (s9) */
static void cond_draw(const ui_held_t *v, qd_rgb col, float y, float size, float cy, float r, float sp, int fit)
{
    float sz = size;
    const float w = ui_TW(v->cond_name, size, 200, 0.f);
    int i;
    if (fit && w > 216.f)                               /* ALWAYS, NOT FILL ..: as big as fits */
        sz = size * 216.f / w;
    ui_T(v->cond_name, 120.f, y, sz, 200, 0.f, col, QD_CENTER);
    if (v->ab_b > 0) {                                  /* A:B: b circles, the a-th filled, the ring on the pass */
        for (i = 0; i < v->ab_b && i < 8; i++) {
            const float x = 120.f - (float)(v->ab_b - 1) * sp / 2.f + (float)i * sp;
            const int a = i == v->ab_a - 1;
            ui_O(x, cy, r, a ? QD_NONE : UI_G2, 1.2f, a ? col : QD_NONE);
            if (i == v->pass)
                ui_O(x, cy, r + 3.5f, UI_K3, 1.f, QD_NONE);
        }
    }                                                   /* (P1.3v: no explanatory line under the name) */
}

static void chance_dots(const ui_held_t *v, qd_rgb col, float y0)
{
    uint8_t lit[30];
    uint32_t s = v->shuffle * 2654435761u + 0x9e3779b9u;
    const int nl = (v->chance * 30 + 50) / 100;
    int i;
    for (i = 0; i < 30; i++)
        lit[i] = (uint8_t)(i < nl);
    for (i = 29; i > 0; i--) {                          /* a deterministic shuffle, new with each pass */
        int j;
        uint8_t t;
        s = s * 1664525u + 1013904223u;
        j = (int)((s >> 8) % (uint32_t)(i + 1));
        t = lit[i], lit[i] = lit[j], lit[j] = t;
    }
    for (i = 0; i < 30; i++)
        ui_O(66.f + (float)(i % 10) * 12.f, y0 + (float)(i / 10) * 11.f, 2.4f, lit[i] ? QD_NONE : UI_G2, 1.f,
             lit[i] ? col : QD_NONE);
}

static void f_cond(const ui_held_t *v, const pal_t *p)
{
    char s[12];
    ui_itoa(s, v->chance);
    cat(s, "%");
    if (v->field != UI_F_CHNC) {                        /* COND: the condition big, the chance's dots */
        cond_draw(v, p->k[0], 86.f, 60.f, 108.f, 6.f, 24.f, 1);
        chance_dots(v, p->k[1], 128.f);
        ui_T(s, 120.f, 188.f - p->pull, 34.f, 200, 0.f, p->k[1], QD_CENTER);
    } else {                                            /* CHNC: the chance big, its dots; the condition small */
        ui_T(s, 120.f, 84.f, 60.f, 200, 0.f, p->k[0], QD_CENTER);
        chance_dots(v, p->k[0], 102.f);
        cond_draw(v, p->k[1], 176.f - p->pull, 30.f, 196.f - p->pull, 5.f, 20.f, 1);
    }
}

/* ---------------------------------------------------------------- SOUND (s10) */
static void cassette(float x, float y, float w, float h, const char *name, const char *num, qd_rgb col, qd_rgb ncol,
                     float reel)
{
    int j, k;
    ui_RR(x, y, w, h, 6.f, col, 1.5f, QD_NONE);
    ui_RR(x + 10.f, y + 6.f, w - 20.f, 30.f, 3.f, UI_G2, 1.f, QD_NONE);
    ui_T(num, x + 15.f, y + 19.f, 12.f, 600, 0.5f, col, QD_LEFT);
    if (name)
        ui_T(name, x + w / 2.f, y + 33.f, 13.f, 500, 1.f, ncol, QD_CENTER);
    for (j = 0; j < 2; j++) {
        const float rx = x + w * (j ? 0.68f : 0.32f), ry = y + h - 17.f;
        ui_O(rx, ry, 9.f, UI_G1, 1.3f, QD_NONE);
        for (k = 0; k < 3; k++) {
            const float an = reel + (float)k * UI_TAU / 3.f + (float)j;
            line2(rx + cosf(an) * 3.f, ry + sinf(an) * 3.f, rx + cosf(an) * 7.f, ry + sinf(an) * 7.f, UI_G1, 1.2f);
        }
    }
    line2(x + w * 0.32f + 9.f, y + h - 8.f, x + w * 0.68f - 9.f, y + h - 8.f, UI_G2, 1.f);
}

static void f_sound(const ui_held_t *v, const pal_t *p)
{
    const float dy = ui_ease(v->roll) * 22.f * (v->dir < 0 ? -1.f : 1.f);
    /* the reels turn while the step sounds (a second after it plays), else they stand */
    const float reel = v->play < 0.f ? 0.f : 2.5f * (v->t - v->play + (v->play < 1.f ? v->play : 1.f));
    const int locked = v->sound > 0 && !p->empty;
    char s[40], num[8];
    int i;

    qd_save();
    qd_clip_rect(0.f, 26.f, 240.f, 158.f);
    if (v->sound_prev) {
        s[0] = 0;
        cat(cat(cat(s, three(num, v->sound - 1)), "  "), v->sound_prev);
        ui_T(s, 120.f, 41.f + dy, 12.f, 500, 0.5f, UI_G2, QD_CENTER);
        ui_RR(52.f, 46.f + dy, 136.f, 14.f, 5.f, UI_G3, 1.2f, QD_NONE);
    }
    cassette(40.f, 64.f + dy, 160.f, 78.f, v->sound_cur, v->sound > 0 ? three(num, v->sound) : "TRACK",
             p->empty ? UI_G2 : locked || v->is_default ? p->k[0] : DEF_COL, locked ? UI_K3 : UI_G1, reel);
    if (v->sound_next) {
        ui_RR(52.f, 148.f + dy, 136.f, 30.f, 5.f, UI_G3, 1.2f, QD_NONE);
        s[0] = 0;
        cat(cat(cat(s, three(num, v->sound + 1)), "  "), v->sound_next);
        ui_T(s, 120.f, 169.f + dy, 12.f, 500, 0.5f, UI_G2, QD_CENTER);
    }
    qd_restore();
    if (p->empty)
        ui_T("EMPTY STEP", 120.f, 192.f, 12.f, 600, 0.8f, UI_G1, QD_CENTER);
    /* the page: a square a step, a dot where a step has a sound lock (white: this sound), the held one blue */
    for (i = 0; i < 16; i++) {
        const float x = 13.f + (float)i * 13.5f + (float)(i / 4) * 2.f;
        const int h = i == v->held_col;
        ui_RR(x, 196.f, 10.f, 10.f, 2.f, h ? QD_NONE : UI_G2, 1.f, h ? (v->is_default ? UI_G2 : p->k[0]) : QD_NONE);
        if (!h && v->page_sound[i])
            ui_O(x + 5.f, 201.f, 1.2f, QD_NONE, 1.f, v->page_sound[i] == v->sound ? UI_K3 : UI_G1);
    }
    {   /* P1.3v: the step's p-locks as padlocks (was "+2 LOCKS ON TOP"); no BANK line (the octave pop-up and the OCT
         * LEDs show the bank) */
        int k;
        for (k = 0; k < v->nlocks && k < 4; k++)
            ui_lock(222.f - (float)k * 12.f, 212.f, UI_MODE_COL[UI_F_PLOCK], 1.2f);
    }
}

/* ---------------------------------------------------------------- TRIG (s19) */
static void f_trig(const ui_held_t *v, const pal_t *p)
{
    static const char *const NAMES[3] = {"NOTE", "LOCK", "TIE"};
    static const char *const TXT[4] = {"an empty step: plays nothing", "strikes its notes",
                                       "changes the sound, no new note", "slides the held note, legato"};
    const int cur = v->type >= 1 && v->type <= 3 ? v->type - 1 : -1;
    const float y = 100.f;
    int i;
    for (i = 0; i < 3; i++) {
        const float x = 44.f + (float)i * 76.f;
        const qd_rgb col = i == cur ? p->k[0] : UI_G2;
        if (i == cur)
            ui_RR(x - 34.f, y - 46.f, 68.f, 84.f, 8.f, p->k[0], 1.2f, QD_NONE);
        if (i == 0)
            ui_O(x, y - 4.f, 13.f, col, 1.5f, i == cur ? p->k[0] : QD_NONE);
        if (i == 1)
            ui_lock(x - 11.f, y - 18.f, col, 3.1f);
        if (i == 2) {
            ui_O(x - 14.f, y + 4.f, 4.f, QD_NONE, 1.f, col);
            ui_O(x + 14.f, y + 4.f, 4.f, QD_NONE, 1.f, col);
            qd_path_begin();
            qd_move_to(x - 14.f, y - 2.f);
            qd_quad_to(x, y - 26.f, x + 14.f, y - 2.f);
            qd_path_stroke(col, 1.5f);
        }
        ui_T(NAMES[i], x, y + 31.f, 12.f, 600, 0.8f, i == cur ? p->k[0] : UI_G2, QD_CENTER);
    }
    (void)TXT;                                          /* (P1.3v: no sentence under them) */
}

/* ---------------------------------------------------------------- FUNC (P1.3w) */
/* the function big as its icon, its one-word name under it (the full screen; EDIT has it in the head); a jump: its
 * target on a strip of the target's page (its number at the left) */
static void f_func(const ui_held_t *v, const pal_t *p)
{
    const int ic = ui_fn_icon(v->func);
    if (ic == UI_IC_NONE)
        ui_O(120.f, 96.f, 6.f, p->k[0], 1.5f, QD_NONE);  /* none: an empty ring */
    else
        ui_icon(ic, 120.f, 96.f, 5.f, p->k[0]);
    if (!p->nobig && v->func_name[0])
        ui_T(v->func_name, 120.f, 166.f, 34.f, 200, 0.f, p->k[0], QD_CENTER);
    if (v->func >= UI_FN_JUMP && v->func < UI_FN_LEAP) {
        char n[12];
        int i;
        ui_T(ui_itoa(n, v->jump_page + 1), 16.f, 206.f, 12.f, 600, 0.f, UI_G1, QD_CENTER);
        for (i = 0; i < 16; i++) {
            const float x = 28.f + (float)i * 12.5f + (float)(i / 4) * 2.f;
            const int tgt = i == v->jump_step;
            ui_RR(x, 194.f, 10.f, 16.f, 2.f, tgt ? QD_NONE : UI_G2, 1.f, tgt ? p->k[0] : QD_NONE);
            if (!tgt && v->jump_trig[i])
                ui_O(x + 5.f, 202.f, 1.6f, QD_NONE, 1.f, UI_G1);
        }
    }
}

/* ---------------------------------------------------------------- the frame */
/* the mode that owns a mode's second part */
static int other_mode(int field)
{
    switch (field) {
    case UI_F_VEL: return UI_F_RATCH;                   /* the ramp */
    case UI_F_LEN: return UI_F_TRIG;                    /* the tie */
    case UI_F_MICRO: return UI_F_NOTE;                  /* the strum */
    case UI_F_COND: return UI_F_CHNC;
    case UI_F_CHNC: return UI_F_COND;
    default: return field;
    }
}

/* the palette and the head: "STEP 5 · VEL" / "TRACK DEFAULT · VEL" (P1.3r: no track box or name: a pop-up shows only
 * what is edited), the DEFAULT / EMPTY STEP tag (P1.3t: no SELECTED) */
static float g_tag_x;                                   /* P1.3s: EDIT puts the tag in the head row: its right end */
static int g_edit_head;
/* the palette of v (its colours: the body's and the head's) */
static int held_pal(const ui_held_t *v, pal_t *p)
{
    const int field = v->field >= 0 && v->field < UI_NFIELD ? v->field : UI_F_NOTE;
    const int lit = v->is_default || v->ovr;
    p->empty = v->type == 0 && field != UI_F_TRIG && !v->is_default;
    p->k[0] = p->empty ? UI_G2 : v->is_default ? (field == UI_F_VEL ? UI_MODE_COL[UI_F_VEL] : UI_TRACK_COL)
            : lit ? UI_MODE_COL[field] : DEF_COL;           /* (P1.5o: the default's VEL value green, as a step's) */
    p->k[1] = p->empty ? UI_G2 : UI_MODE_COL[other_mode(field)];
    p->wh = p->empty ? UI_G1 : UI_K3;
    p->nobig = 0;
    p->pull = 0.f;
    return field;
}
static int held_head(const ui_held_t *v, pal_t *p)
{
    const int field = held_pal(v, p);
    const int lit = v->is_default || v->ovr;
    char left[48], n[12];

    if (!v->is_default && !v->follow)                   /* P1.5d (the frames the other way round): a held step (or the
                                                         * latch's selection) has the frame, a track's default none
                                                         * (P1.5q: nor the step playing, followed: none held) */
        ui_RR(0.f, 0.f, (float)QD_W, (float)QD_H, 8.f, UI_STEP_FRAME, 3.f, QD_NONE);

    left[0] = 0;                                        /* "STEP 5 · NOTE", several held: "STEP 5+2 · NOTE" */
    if (v->is_default) {
        cat(left, "TRACK DEFAULT");
    } else {
        cat(cat(left, "STEP "), ui_itoa(n, v->step));
        if (v->nheld > 1)
            cat(cat(left, "+"), ui_itoa(n, v->nheld - 1));
    }
    cat(cat(left, " \xc2\xb7 "), UI_FIELD_NAME[field]);
    ui_T(left, 8.f, 18.f, 12.f, 600, 0.8f, v->is_default ? UI_TRACK_COL : UI_K3, QD_LEFT);
    /* (P1.3t: no SELECTED: the row and the LFO light say it) */
    if (field != UI_F_PLOCK && ((!lit && !p->empty) || (p->empty && (field == UI_F_COND || field == UI_F_CHNC)))) {
        /* a tag top right: the step plays the track's value (COND / CHNC, which draw no EMPTY STEP: an empty step) */
        const char *tag = p->empty ? "EMPTY" : "DEFAULT";   /* (P1.3v: a 12 px word, no 7 px box) */
        const float w = ui_TW(tag, 12.f, 600, 0.5f);
        const float x = g_edit_head ? 8.f + ui_TW(left, 12.f, 600, 0.8f) + 8.f : 232.f - w;
        ui_T(tag, x, 18.f, 12.f, 600, 0.5f, UI_G1, QD_LEFT);
        g_tag_x = x + w;
    }
    return field;
}

/* P1.3s: a mode key held, B11 resets its default: a small word (lit when it differs from the track's initial value) */
static void reset_word(const ui_held_t *v, float y)
{
    ui_icon(UI_IC_RESET, 224.f, y, 1.1f, v->reset == 1 ? UI_K3 : UI_G3);   /* (P1.3v: an icon, was "B11 RESET") */
}

static void held_body(const ui_held_t *v, const pal_t *p, int field)
{
    switch (field) {
    case UI_F_NOTE: f_note(v, p); break;
    case UI_F_VEL: f_vel(v, p); break;
    case UI_F_LEN: f_len(v, p); break;
    case UI_F_MICRO: f_micro(v, p); break;
    case UI_F_RATCH: f_ratch(v, p); break;
    case UI_F_COND:
    case UI_F_CHNC: f_cond(v, p); break;
    case UI_F_SOUND: f_sound(v, p); break;
    case UI_F_TRIG: f_trig(v, p); break;
    case UI_F_FUNC: f_func(v, p); break;
    default: break;
    }
}

/* P1.3s (the owner): no value strip (the black keys still work; the drawing says the value); the drawing 8 px lower
 * into the space it left; a mode key held: "B11 RESET" small in the head */
void ui_held_draw(const ui_held_t *v)
{
    pal_t p;
    const int field = held_head(v, &p);
    qd_save();
    qd_translate(0.f, 8.f);
    held_body(v, &p, field);
    qd_restore();
    if (v->is_default && v->reset)
        reset_word(v, 13.f);
}

/* ================================================================ P1.3r / P1.3s: the EDIT view ===
 * P1.3s (the owner: the drawings as big as on the held screens): the head carries the value, right-aligned and large
 * ("STEP 21 · NOTE ...... D#dim"); under it each mode's drawing at full size (scale 1, full width), moved up into the
 * top area (y 26 .. 178) with only padding trimmed; a small sub-label under the head where one is needed ("KEYS OCT 4 ·
 * CHROMATIC"; P1.3v: no other sub-labels). No value strip (the black keys still work: the row and the drawing
 * show the value). The foot: the page's row as STEPS' open row (cells 42 px). CHNC / COND keep their big value inside
 * the drawing (the dots and the passes are drawn round it). No takeover pops up over it: the top is the pop-up. */
#define ED_TOP 26.f
#define ED_BOT 178.f                                    /* the top area's foot (the row's marker room below) */
#define ED_ROW_Y 180.f
#define ED_ROW_OPEN 58.f                                /* its cells 42 px */

/* a full-size drawing moved up by dy (scale 1) */
static void moved(const ui_held_t *v, const pal_t *p, int field, float dy)
{
    qd_save();
    qd_translate(0.f, dy);
    held_body(v, p, field);
    qd_restore();
}

/* the head's value, right-aligned, as big as fits beside the head's left part */
static void head_value(const ui_held_t *v, const pal_t *p, const char *val)
{
    char left[48], n[12];
    float size = 24.f, room;
    left[0] = 0;
    if (v->is_default) {
        cat(left, "TRACK DEFAULT");
    } else {
        cat(cat(left, "STEP "), ui_itoa(n, v->step));
        if (v->nheld > 1)
            cat(cat(left, "+"), ui_itoa(n, v->nheld - 1));
    }
    cat(cat(left, " \xc2\xb7 "), UI_FIELD_NAME[v->field >= 0 && v->field < UI_NFIELD ? v->field : 0]);
    room = 224.f - (g_tag_x > 0.f ? g_tag_x - 8.f : ui_TW(left, 12.f, 600, 0.8f)) - 10.f;
    if (!val || !val[0])
        return;
    if (ui_TW(val, size, 200, 0.f) > room)
        size = size * room / ui_TW(val, size, 200, 0.f);
    if (size < 12.f)                                    /* (no room: the drawing says it, e.g. SOUND's cassette) */
        return;
    ui_T(val, 232.f, 21.f, size, 200, 0.f, p->empty ? UI_G1 : p->k[0], QD_RIGHT);
}

static void ed_plock(const ui_held_t *v, const ui_lane_t *lane, char *val)
{
    const qd_rgb col = UI_MODE_COL[UI_F_PLOCK];
    char n[12];
    val[0] = 0;
    if (v->is_default) {                                /* (P1.3v: the padlock alone, no sentences) */
        ui_lock(99.f, 58.f, UI_G2, 6.f);
    } else {
        const int c = (v->step - 1) % 16, nl = lane->col[c < 0 ? 0 : c].nlock;
        cat(cat(val, ui_itoa(n, nl)), nl == 1 ? " LOCK" : " LOCKS");
        ui_lock(99.f, 58.f, nl ? col : UI_G2, 6.f);
    }
}

/* P1.5z: the head's value of v as the EDIT view's switch below makes it (for the head of another model than the
 * picture's: EDIT following shows the track's default's) */
static void ed_head_val(const ui_held_t *v, const ui_lane_t *lane, int field, char *val)
{
    static const char *const TRIGS[4] = {"", "NOTE", "LOCK", "TIE"};
    char n[12];
    val[0] = 0;
    switch (field) {
    case UI_F_VEL: cat(val, ui_itoa(n, v->vel)); break;
    case UI_F_LEN: cat(val, v->len_name ? v->len_name : ""); break;
    case UI_F_MICRO: cat(cat(val, v->micro > 0 ? "+" : ""), ui_itoa(n, v->micro)); break;
    case UI_F_RATCH: cat(cat(val, "\xc3\x97"), ui_itoa(n, v->count < 1 ? 1 : v->count > 8 ? 8 : v->count)); break;
    case UI_F_TRIG: cat(val, TRIGS[v->type >= 1 && v->type <= 3 ? v->type : 0]); break;
    case UI_F_SOUND: cat(val, v->sound_cur ? v->sound_cur : "TRACK SOUND"); break;
    case UI_F_FUNC: cat(val, v->func_name); break;
    case UI_F_PLOCK:
        if (!v->is_default) {
            const int c = (v->step - 1) % 16, nl = lane->col[c < 0 ? 0 : c].nlock;
            cat(cat(val, ui_itoa(n, nl)), nl == 1 ? " LOCK" : " LOCKS");
        }
        break;
    default: break;
    }
}

void ui_edit_draw(const ui_held_t *v, const ui_lane_t *lane)
{
    pal_t p, hp;
    int field;
    char val[40], n[12], sub[48];
    const ui_held_t *hv = v->head ? v->head : v;        /* (P1.5z: following: the head the track's default's) */

    g_edit_head = 1;
    g_tag_x = 0.f;
    field = held_head(hv, &hp);
    g_edit_head = 0;
    p = hp;
    if (hv != v)
        held_pal(v, &p);                                /* (the picture: the step playing's colours) */

    p.nobig = 1;
    val[0] = sub[0] = 0;
    qd_save();
    qd_clip_rect(0.f, ED_TOP - 4.f, 240.f, ED_BOT - ED_TOP + 4.f);   /* (nothing spills onto the row) */
    if (v->dim)                                         /* P1.5q: following, the playhead on an empty step: the last
                                                         * step played, dimmed a little */
        qd_alpha(qd_get_alpha() * 0.6f);
    switch (field) {
    case UI_F_NOTE:                                     /* the keybed full width, the note line over it */
        if (v->scale_name && v->scale_name[0])
            cat(sub, v->scale_name);
        moved(v, &p, field, -34.f);                     /* (the notes 131, the keybed 140 .. 210 -> 97, 106 .. 176) */
        if (v->root[0] && !p.empty)                     /* P1.3u: the chord's name big, centred in the space over
                                                         * the notes (none: the space stays empty) */
            chord_name(v, &p, 120.f, 78.f, 46.f);
        break;
    case UI_F_VEL:                                      /* P1.5d: the rider, its road at the area's foot; the velocity in
                                                         * the head */
        cat(val, ui_itoa(n, v->vel));                   /* (the head's value: the rider has the area) */
        moved(v, &p, field, -30.f);
        break;
    case UI_F_LEN:                                      /* the ruler and the bar */
        cat(val, v->len_name ? v->len_name : "");
        moved(v, &p, field, -56.f);                     /* (108 .. 198 -> 52 .. 142) */
        break;
    case UI_F_MICRO:                                    /* the tape measure */
        cat(val, v->micro > 0 ? "+" : "");
        cat(val, ui_itoa(n, v->micro));
        moved(v, &p, field, -52.f);                     /* (94 .. 186 -> 42 .. 134) */
        break;
    case UI_F_RATCH:                                    /* the bouncing ball */
        cat(cat(val, "\xc3\x97"), ui_itoa(n, v->count < 1 ? 1 : v->count > 8 ? 8 : v->count));
        moved(v, &p, field, -62.f);                     /* (106 .. 215 -> 44 .. 153) */
        break;
    case UI_F_COND:                                     /* the passes and the dots (their value inside) */
    case UI_F_CHNC:
        p.nobig = 0;                                    /* (their value big in the drawing: none in the head) */
        if (field == UI_F_CHNC) {
            p.pull = 14.f;                              /* (42 .. 201: the condition 14 up, then 16 up: 26 .. 171) */
            moved(v, &p, field, -16.f);
        } else {
            p.pull = 4.f;                               /* (44 .. 188: the chance 4 up, then 10 up: 34 .. 174) */
            moved(v, &p, field, -10.f);
        }
        break;
    case UI_F_TRIG: {                                   /* the three icons */
        static const char *const NAMES[4] = {"", "NOTE", "LOCK", "TIE"};
        cat(val, NAMES[v->type >= 1 && v->type <= 3 ? v->type : 0]);
        p.nobig = 0;
        moved(v, &p, field, -14.f);                     /* (54 .. 184 -> 40 .. 170) */
        break;
    }
    case UI_F_SOUND:                                    /* the cassette (the page's squares and the bank cut: the row
                                                         * shows the page) */
        cat(val, v->sound_cur ? v->sound_cur : "TRACK SOUND");
        p.nobig = 0;
        moved(v, &p, field, -4.f);                      /* (30 .. 182 -> 26 .. 178) */
        break;
    case UI_F_FUNC:                                     /* P1.3w: the icon big, the name in the head */
        cat(val, v->func_name);
        moved(v, &p, field, -36.f);                     /* (the icon 96 -> 60, the jump strip 194 .. 210 -> 158 .. 174) */
        break;
    case UI_F_PLOCK:
        ed_plock(v, lane, val);
        break;
    default:
        break;
    }
    qd_restore();
    if (hv != v)                                        /* P1.5z: the head's value: the track's default's */
        ed_head_val(hv, lane, field, val);
    if (hp.empty) {
        val[0] = 0;
        cat(val, "EMPTY STEP");
    }
    head_value(hv, &hp, val);
    if (sub[0])
        ui_lab(sub, 8.f, 31.f, UI_G1, QD_LEFT);
    if (v->is_default && v->reset)                      /* the mode key held: B11 resets the default */
        reset_word(v, 36.f);
    ui_steps_row(lane, ED_ROW_Y, ED_ROW_OPEN);
}
