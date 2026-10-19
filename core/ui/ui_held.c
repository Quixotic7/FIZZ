/* SPDX-License-Identifier: GPL-3.0-only */
/* A held step's modes (mockups/line.html s4..s10, s19, s20; PLAN.md §9a rule 7, §8b): one layout. "STEP n · MODE"
 * top left, the track top right, the value big, the mode's drawing, the value strip at the foot (B1..B10 the mode's
 * ten values written in the keys, the step's filled in the mode's colour, the track's default underlined, B11 CLR
 * outlined while the step overrides the mode).
 * The value and the drawing's own part are in the mode's colour when the step overrides the mode; on the track's
 * default they are grey-white, with a small DEFAULT tag top right (the step plays the track's value). A part that
 * is another mode's value is drawn in that mode's colour (VEL's ramp ticks RATCH's, LEN's tie TRIG's, MICRO's strum
 * NOTE's, COND's chance CHNC's and CHNC's condition COND's).
 * is_default: the track's default for the mode (ALGORITHM turned with nothing held): "DEFAULT · MODE", the drawing
 * in the mode's colour, no strip.
 *   NOTE   the chord name big (squeezing in), the notes on a keybed, their names under them; the entry octave and
 *          the track's scale small top left ("OCT 4 · D DORIAN"). P1.3c: five notes, each dot and name in the colour
 *          of the knob that turns it (note 1 SELECT's yellow, notes 2..5 KNOB 1..4's blue, green, white, orange) and
 *          the five knobs over the keybed's right end, filled where they hold a note
 *   VEL    a mallet over a drum, raised by the velocity, striking when the step plays; the ramp as four ticks
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

#include "ui.h"

/* the colours a mode draws with: k[0] the mode's (grey-white on the track's default), k[1] the other mode's part;
 * grey for an empty step */
typedef struct {
    qd_rgb k[2];
    qd_rgb wh;                          /* white parts that are no value (the mallet, the ball, the chord name) */
    int empty;
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
    if (p->empty)
        ui_T("EMPTY STEP", x, y - size * 0.32f, 11.f, 600, 1.f, UI_G1, QD_LEFT);
    else
        ui_T(s, x, y, size, 200, 0.f, p->k[0], QD_LEFT);
}

static void line2(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, col, lw, 0, QD_NONE);
}

/* ---------------------------------------------------------------- NOTE (s4) */
static void f_note(const ui_held_t *v, const pal_t *p)
{
    const float sq = 0.55f + 0.45f * ui_ease(v->squeeze / 0.18f);
    int i, lo = 48, mn = 127;

    if (p->empty) {
        ui_T("EMPTY STEP", 120.f, 76.f, 11.f, 600, 1.f, UI_G1, QD_CENTER);
    } else if (v->root[0]) {                            /* root big, quality small, squeezed in from narrow */
        const float wr = ui_TW(v->root, 70.f, 200, 0.f), wq = v->qual[0] ? ui_TW(v->qual, 30.f, 300, 0.f) : 0.f;
        const float w = (wr + wq + 2.f) * sq, x = 120.f - w / 2.f;
        qd_save();
        qd_translate(x, 98.f);
        qd_scale(sq, 1.f);
        ui_T(v->root, 0.f, 0.f, 70.f, 200, 0.f, p->wh, QD_LEFT);
        qd_restore();
        if (v->qual[0]) {
            qd_save();
            qd_translate(x + (wr + 2.f) * sq, 64.f);
            qd_scale(sq, 1.f);
            ui_T(v->qual, 0.f, 0.f, 30.f, 300, 0.f, p->wh, QD_LEFT);
            qd_restore();
        }
    } else if (v->names[0]) {                           /* not a chord the namer knows: the notes' names */
        qd_save();
        qd_translate(120.f, 88.f);
        qd_scale(sq, 1.f);
        ui_T(v->names, 0.f, 0.f, 26.f, 300, 0.5f, p->wh, QD_CENTER);
        qd_restore();
    }

    for (i = 0; i < v->nnote && i < 5; i++)             /* the keybed from the C at or below the lowest note */
        if (v->note[i] < mn)
            mn = v->note[i];
    if (v->nnote > 0)
        lo = mn / 12 * 12;
    ui_keyboard(14.f, 120.f, 212.f, 70.f, lo, 0);
    for (i = 0; i < v->nnote && i < 5; i++) {
        int b, k;
        const int d = v->note[i] - lo;
        qd_rgb col = p->k[0];
        float x;
        if (d < 0 || d >= 24)
            continue;
        if (!p->empty)                                  /* the colour of the knob that turns it */
            for (k = 0; k < 5; k++)
                if (v->slot_note[k] == v->note[i]) {
                    col = k ? UI_K[k - 1] : UI_KSEL;
                    break;
                }
        x = ui_keyboard_x(14.f, 212.f, lo, v->note[i], &b);
        ui_O(x, b ? 152.f : 180.f, 3.8f, QD_NONE, 1.f, col);
        ui_T(v->note_name[i], x, i % 2 ? 215.f : 204.f, 9.f, 600, 0.f, col, QD_CENTER);
    }
    if (v->knobs) {                                     /* the five knobs: SELECT, KNOB 1..4 */
        static const char *const KN[5] = {"S", "1", "2", "3", "4"};
        int k;
        for (k = 0; k < 5; k++) {
            const float x = 162.f + (float)k * 15.f;
            const qd_rgb col = k ? UI_K[k - 1] : UI_KSEL;
            const int on = v->slot_note[k] && !p->empty;
            ui_O(x, 110.f, 5.5f, col, 1.2f, on ? col : QD_NONE);
            ui_T(KN[k], x, 112.5f, 7.f, 700, 0.f, on ? UI_BG : col, QD_CENTER);
        }
    }
    {   /* the entry octave (B1's), the track's scale */
        char s[40], n[12];
        s[0] = 0;
        cat(cat(s, "OCT "), ui_itoa(n, v->entry_oct));
        if (v->scale_name && v->scale_name[0])
            cat(cat(s, " \xc2\xb7 "), v->scale_name);
        ui_lab(s, 8.f, 37.f, UI_G1, QD_LEFT);
    }
}

/* ---------------------------------------------------------------- VEL (s5) */
static void f_vel(const ui_held_t *v, const pal_t *p)
{
    const float cx = 82.f, top = 158.f, px = 204.f, py = 146.f;
    const float rest = atan2f(154.f - py, 104.f - px);
    float lift, ang, hx, hy;
    char s[12];
    int i;

    big(p, ui_itoa(s, v->vel), 12.f, 96.f, 64.f);
    /* the drum */
    qd_ellipse(cx, top, 52.f, 11.f, UI_G1, 1.5f, QD_NONE);
    qd_ellipse_arc(cx, top + 34.f, 52.f, 11.f, 0.f, UI_PI, UI_G1, 1.5f);
    line2(cx - 52.f, top, cx - 52.f, top + 34.f, UI_G1, 1.5f);
    line2(cx + 52.f, top, cx + 52.f, top + 34.f, UI_G1, 1.5f);
    for (i = 0; i < 6; i++) {
        const float x = cx - 45.f + (float)i * 18.f;
        line2(x, top + 10.f + (float)(i % 2) * 3.f, x + 9.f, top + 32.f + (float)(i % 2) * 3.f, UI_G2, 1.f);
    }
    /* the mallet: down fast when the step plays, rests on the skin, rises again; raised while it has not played */
    if (v->play < 0.f)
        lift = 1.f;
    else if (v->play < 0.06f)
        lift = 1.f - v->play / 0.06f;
    else if (v->play < 0.25f)
        lift = 0.f;
    else if (v->play < 0.5f)
        lift = (v->play - 0.25f) / 0.25f;
    else
        lift = 1.f;
    ang = rest + 0.95f * ((float)v->vel / 127.f) * lift;
    hx = px + cosf(ang) * 96.f, hy = py + sinf(ang) * 96.f;
    line2(px, py, hx, hy, p->wh, 2.f);
    ui_O(hx, hy, 6.f, p->wh, 1.5f, UI_BG);
    ui_O(px, py, 2.f, QD_NONE, 1.f, UI_G1);
    if (v->play >= 0.f && v->play < 0.12f && !p->empty) {   /* three blue lines jump off the skin */
        static const float D[3][2] = {{-1.f, -0.6f}, {0.f, -1.f}, {1.f, -0.6f}};
        qd_save();
        qd_alpha(1.f - v->play / 0.12f);
        for (i = 0; i < 3; i++)
            line2(104.f + D[i][0] * 10.f, 144.f + D[i][1] * 8.f, 104.f + D[i][0] * 18.f, 144.f + D[i][1] * 15.f,
                  p->k[0], 1.3f);
        qd_restore();
    }
    /* KNOB 2, the ratchet's ramp: four hits' velocities as green ticks */
    for (i = 0; i < 4; i++) {
        const float x = 190.f + (float)i * 9.f, h = (float)ramp_vel(v->vel, v->ramp, i, 4) / 127.f * 22.f;
        line2(x, 206.f, x, 206.f - (h < 1.f ? 1.f : h), p->k[1], 2.f);
    }
    ui_lab("RAMP", 190.f, 217.f, UI_G1, QD_LEFT);
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
    if (!p->empty && !inf && v->len_name)
        ui_lab("STEPS", 12.f + ui_TW(v->len_name, 64.f, 200, 0.f) + 6.f, 92.f, UI_G1, QD_LEFT);
    /* the ruler: the steps from two before the held one, the beats' ticks taller */
    for (i = 0; i <= 10; i++) {
        const float x = x0 + (float)i * sw;
        const int st = wrap_step(v->step + i - 2, v->tracklen);
        line2(x, y + 10.f, x, y + ((st - 1) % 4 == 0 ? -2.f : 4.f), i == 2 ? UI_K3 : UI_G2, 1.f);
        if (i < 10)
            ui_T(ui_itoa(s, st), x + sw / 2.f, y + 26.f, 8.f, i == 2 ? 600 : 500, 0.f, i == 2 ? UI_K3 : UI_G2,
                 QD_CENTER);
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
    if (!p->empty)
        ui_T("/24", 12.f + ui_TW(s, 56.f, 200, 0.f) + 6.f, 82.f, 18.f, 300, 0.f, UI_G1, QD_LEFT);
    line2(10.f, base, 230.f, base, UI_G2, 1.f);
    for (k = -12 + (int)off; k <= 36 + (int)off; k++) {
        const int bg = k % 24 == 0, mid = k % 6 == 0;
        line2(MX(k), base, MX(k), base - (bg ? 76.f : mid ? 7.f : 3.f), bg && k == 0 ? UI_K3 : UI_G2, bg ? 1.2f : 1.f);
    }
    ui_T(ui_itoa(n, wrap_step(v->step - 1, v->tracklen)), MX(off < 0.f ? -24 : -12), base + 14.f, 8.f, 500, 0.f,
         UI_G2, QD_CENTER);
    ui_T(ui_itoa(n, wrap_step(v->step, v->tracklen)), MX(0), base + 14.f, 8.f, 600, 0.f, UI_K3, QD_CENTER);
    if (off == 0.f)
        ui_T(ui_itoa(n, wrap_step(v->step + 1, v->tracklen)), MX(24), base + 14.f, 8.f, 500, 0.f, UI_G2, QD_CENTER);
    /* the start flag (KNOB 1) */
    sx = MX(v->micro);
    line2(sx, base, sx, 98.f, p->k[0], 1.5f);
    {
        const float f[6] = {sx, 98.f, sx + 9.f, 102.f, sx, 106.f};
        ui_P(f, 3, p->k[0], 1.2f, 1, p->k[0]);
    }
    /* the strum's staircase (KNOB 2): note j at micro + j * strum, lit once the walk passes it */
    for (j = 0; j < v->nnote && j < 5; j++) {
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
    } else if (v->cond_text) {
        ui_T(v->cond_text, 120.f, cy + 4.f, 11.f, 400, 0.f, UI_G1, QD_CENTER);
    }
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
        ui_T(s, 120.f, 188.f, 34.f, 200, 0.f, p->k[1], QD_CENTER);
    } else {                                            /* CHNC: the chance big, its dots; the condition small */
        ui_T(s, 120.f, 84.f, 60.f, 200, 0.f, p->k[0], QD_CENTER);
        chance_dots(v, p->k[0], 102.f);
        cond_draw(v, p->k[1], 176.f, 30.f, 196.f, 5.f, 20.f, 1);
    }
}

/* ---------------------------------------------------------------- SOUND (s10) */
static void cassette(float x, float y, float w, float h, const char *name, const char *num, qd_rgb col, qd_rgb ncol,
                     float reel)
{
    int j, k;
    ui_RR(x, y, w, h, 6.f, col, 1.5f, QD_NONE);
    ui_RR(x + 10.f, y + 8.f, w - 20.f, 26.f, 3.f, UI_G2, 1.f, QD_NONE);
    ui_T(num, x + 15.f, y + 18.f, 8.f, 600, 1.f, col, QD_LEFT);
    if (name)
        ui_T(name, x + w / 2.f, y + 29.f, 13.f, 500, 1.f, ncol, QD_CENTER);
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
        ui_T(s, 120.f, 40.f + dy, 10.f, 500, 1.f, UI_G2, QD_CENTER);
        ui_RR(52.f, 46.f + dy, 136.f, 14.f, 5.f, UI_G3, 1.2f, QD_NONE);
    }
    cassette(40.f, 64.f + dy, 160.f, 78.f, v->sound_cur, v->sound > 0 ? three(num, v->sound) : "TRACK",
             p->empty ? UI_G2 : locked || v->is_default ? p->k[0] : DEF_COL, locked ? UI_K3 : UI_G1, reel);
    if (v->sound_next) {
        ui_RR(52.f, 148.f + dy, 136.f, 30.f, 5.f, UI_G3, 1.2f, QD_NONE);
        s[0] = 0;
        cat(cat(cat(s, three(num, v->sound + 1)), "  "), v->sound_next);
        ui_T(s, 120.f, 168.f + dy, 10.f, 500, 1.f, UI_G2, QD_CENTER);
    }
    qd_restore();
    if (p->empty)
        ui_T("EMPTY STEP", 120.f, 192.f, 8.f, 600, 1.f, UI_G1, QD_CENTER);
    /* the page: a square a step, a dot where a step has a sound lock (white: this sound), the held one blue */
    for (i = 0; i < 16; i++) {
        const float x = 13.f + (float)i * 13.5f + (float)(i / 4) * 2.f;
        const int h = i == v->held_col;
        ui_RR(x, 196.f, 10.f, 10.f, 2.f, h ? QD_NONE : UI_G2, 1.f, h ? (v->is_default ? UI_G2 : p->k[0]) : QD_NONE);
        if (!h && v->page_sound[i])
            ui_O(x + 5.f, 201.f, 1.2f, QD_NONE, 1.f, v->page_sound[i] == v->sound ? UI_K3 : UI_G1);
    }
    if (v->nlocks > 0) {
        char n[12];
        s[0] = 0;
        cat(cat(cat(s, "+"), ui_itoa(n, v->nlocks)), v->nlocks == 1 ? " LOCK ON TOP" : " LOCKS ON TOP");
        ui_lab(s, 227.f, 217.f, UI_G1, QD_RIGHT);
    }
    if (!v->is_default) {                               /* the bank of ten on the keys */
        char n[12];
        s[0] = 0;
        cat(cat(cat(cat(s, "BANK "), ui_itoa(n, v->bank * 10 + 1)), "\xe2\x80\x93"), ui_itoa(n, v->bank * 10 + 10));
        ui_lab(s, 13.f, 217.f, UI_G1, QD_LEFT);
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
        ui_T(NAMES[i], x, y + 30.f, 10.f, 600, 1.f, i == cur ? p->k[0] : UI_G2, QD_CENTER);
    }
    ui_T(TXT[cur + 1], 120.f, 180.f, 11.f, 400, 0.f, UI_G1, QD_CENTER);
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

void ui_held_draw(const ui_held_t *v)
{
    const int field = v->field >= 0 && v->field < UI_NFIELD ? v->field : UI_F_NOTE;
    const int lit = v->is_default || v->ovr;
    pal_t p;
    char left[48], n[12];

    p.empty = v->type == 0 && field != UI_F_TRIG && !v->is_default;
    p.k[0] = p.empty ? UI_G2 : lit ? UI_MODE_COL[field] : DEF_COL;
    p.k[1] = p.empty ? UI_G2 : UI_MODE_COL[other_mode(field)];
    p.wh = p.empty ? UI_G1 : UI_K3;

    left[0] = 0;                                        /* "STEP 5 · NOTE", several held: "STEP 5+2 · NOTE" */
    if (v->is_default) {
        cat(left, "DEFAULT");
    } else {
        cat(cat(left, "STEP "), ui_itoa(n, v->step));
        if (v->nheld > 1)
            cat(cat(left, "+"), ui_itoa(n, v->nheld - 1));
    }
    cat(cat(left, " \xc2\xb7 "), UI_FIELD_NAME[field]);
    ui_head_track(left, v->track, v->name, UI_K3);
    if (v->latch && !v->is_default)                     /* P1.3d: selected by the step latch (LFO), not held */
        ui_lab("SELECTED", 8.f, 28.f, UI_G1, QD_LEFT);
    if ((!lit && !p.empty) || (p.empty && (field == UI_F_COND || field == UI_F_CHNC))) {
        /* a tag top right: the step plays the track's value (COND / CHNC, which draw no EMPTY STEP: an empty step) */
        const char *tag = p.empty ? "EMPTY STEP" : "DEFAULT";
        const float w = ui_TW(tag, 7.f, 600, 1.f) + 8.f;
        ui_RR(232.f - w, 26.f, w, 12.f, 3.f, UI_G2, 1.f, QD_NONE);
        ui_T(tag, 232.f - w / 2.f + 0.5f, 34.5f, 7.f, 600, 1.f, UI_G1, QD_CENTER);
    }

    switch (field) {
    case UI_F_NOTE: f_note(v, &p); break;
    case UI_F_VEL: f_vel(v, &p); break;
    case UI_F_LEN: f_len(v, &p); break;
    case UI_F_MICRO: f_micro(v, &p); break;
    case UI_F_RATCH: f_ratch(v, &p); break;
    case UI_F_COND:
    case UI_F_CHNC: f_cond(v, &p); break;
    case UI_F_SOUND: f_sound(v, &p); break;
    case UI_F_TRIG: f_trig(v, &p); break;
    default: break;
    }
    if (!v->is_default)
        ui_value_strip((char (*)[6])v->vk, v->vk_lit, v->vk_def, v->vk_none, v->clr, UI_MODE_COL[field]);
}
