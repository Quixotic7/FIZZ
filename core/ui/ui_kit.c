/* SPDX-License-Identifier: GPL-3.0-only */
/* mockups/line.html's drawing helpers over qd_gfx (ui.h): the screens port from the mock-ups call for call. */
#include <math.h>

#include "ui.h"

const qd_rgb UI_K[4] = {UI_K1, UI_K2, UI_K3, UI_K4};

/* n as decimal into out (12 bytes are enough): no stdio in the screens */
char *ui_itoa(char *out, int n)
{
    char tmp[12];
    int i = 0, k = 0;
    unsigned u = n < 0 ? 0u - (unsigned)n : (unsigned)n;
    do {
        tmp[i++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u);
    if (n < 0)
        out[k++] = '-';
    while (i)
        out[k++] = tmp[--i];
    out[k] = 0;
    return out;
}

float ui_ease(float x) { return x < 0.f ? 0.f : x > 1.f ? 1.f : x * x * (3.f - 2.f * x); }

/* T(c, s, x, y, {s, w, ls, col, al}) */
float ui_T(const char *s, float x, float y, float size, int weight, float ls, qd_rgb col, int align)
{
    qd_font f;
    f.size = size;
    f.weight = weight;
    f.col = col;
    f.align = align;
    f.ls = ls;
    return qd_text(s, x, y, &f);
}

/* TW(c, s, {s, w, ls}) */
float ui_TW(const char *s, float size, int weight, float ls)
{
    qd_font f;
    f.size = size;
    f.weight = weight;
    f.col = UI_K3;
    f.align = QD_LEFT;
    f.ls = ls;
    return qd_text_width(s, &f);
}

/* lab(c, s, x, y, col, al): T(.., { s: 8, w: 600, ls: 1 }) */
void ui_lab(const char *s, float x, float y, qd_rgb col, int align) { ui_T(s, x, y, 8.f, 600, 1.f, col, align); }

/* P(c, pts, col, lw, { close, fill }): round caps and joins (the state's, set by qd_begin) */
void ui_P(const float *pts, int npts, qd_rgb col, float lw, int close, qd_rgb fill)
{
    qd_polyline(pts, npts, close, col, lw, fill);
}

/* O(c, x, y, r, col, lw, fill): a full circle */
void ui_O(float x, float y, float r, qd_rgb col, float lw, qd_rgb fill) { qd_circle(x, y, r, col, lw, fill); }

/* RR(c, x, y, w, h, r, col, lw, fill): roundRect(x + lw / 2, y + lw / 2, w - lw, h - lw, r) */
void ui_RR(float x, float y, float w, float h, float r, qd_rgb col, float lw, qd_rgb fill)
{
    qd_rrect(x + lw / 2.f, y + lw / 2.f, w - lw, h - lw, r, col, lw, fill);
}

/* box(c, x, y, n, col): the track's number in a little box, OP-1 style */
void ui_box(float x, float y, int n, qd_rgb col)
{
    char s[12];
    ui_RR(x, y, 15.f, 15.f, 2.f, col, 1.2f, QD_NONE);
    ui_T(ui_itoa(s, n), x + 7.5f, y + 11.5f, 10.f, 500, 0.f, col, QD_CENTER);
}

/* head(c, left, right, { n, lcol, rcol }) */
void ui_head(const char *left, const char *right, int n, qd_rgb lcol, qd_rgb rcol)
{
    float x = 8.f;
    if (n) {
        ui_box(8.f, 5.f, n, UI_K3);
        x = 29.f;
    }
    ui_T(left, x, 17.f, 11.f, 500, 1.f, lcol, QD_LEFT);
    if (right)
        ui_T(right, 232.f, 17.f, 11.f, 400, 1.f, rcol, QD_RIGHT);
}

/* keyboard(c, x0, y0, w, h, lo, held): 14 white keys from lo, the black keys over them */
void ui_keyboard(float x0, float y0, float w, float h, int lo, const qd_rgb *fill)
{
    static const int WHITE[7] = {0, 2, 4, 5, 7, 9, 11}, BLACK[7] = {1, 3, -1, 6, 8, 10, -1};
    const float kw = w / 14.f, bw = kw * 0.62f, bh = h * 0.6f;
    int i;
    for (i = 0; i < 14; i++) {
        const int n = lo + (i / 7) * 12 + WHITE[i % 7];
        const qd_rgb f = fill && n >= 0 && n < 128 ? fill[n] : QD_NONE;
        ui_RR(x0 + (float)i * kw, y0, kw + 1.f, h, 2.f, UI_G2, 1.f, f);
    }
    for (i = 0; i < 14; i++) {
        int n;
        qd_rgb f;
        if (i % 7 == 2 || i % 7 == 6 || i == 13)
            continue;
        n = lo + (i / 7) * 12 + BLACK[i % 7];
        f = fill && n >= 0 && n < 128 && fill[n] != QD_NONE ? fill[n] : UI_BG;
        ui_RR(x0 + (float)(i + 1) * kw - bw / 2.f, y0 - 1.f, bw, bh, 2.f, UI_G2, 1.f, f);
    }
}

/* ---- P1.3b: the hold modes, line.html's headTrack, lock, knobMark and keyboard's pos, the black keys' strip */
const char *const UI_FIELD_NAME[UI_NFIELD] = {"NOTE", "VEL", "LEN", "MICRO", "RATCH", "CHNC", "COND", "TRIG",
                                              "SOUND", "LOCKS"};
/* a colour a mode, all bright on black and none of them red (red is the playhead) */
const qd_rgb UI_MODE_COL[UI_NFIELD] = {0x3f7dffu, 0x1fd47eu, 0x36d6e7u, 0xa879ffu, 0xff6a2cu,
                                       0xffd23fu, 0xff52c0u, 0xf1f0eau, 0xb5e853u, 0xd8c39au};

/* headTrack(c, left, n, name, lcol): a held step: STEP 5 · FIELD left, [n] the track right */
void ui_head_track(const char *left, int n, const char *name, qd_rgb lcol)
{
    const float w = ui_TW(name, 11.f, 500, 1.f);
    ui_T(left, 8.f, 17.f, 11.f, 500, 1.f, lcol, QD_LEFT);
    ui_T(name, 232.f, 17.f, 11.f, 500, 1.f, UI_G1, QD_RIGHT);
    ui_box(232.f - w - 21.f, 5.f, n, UI_G1);
}

/* the strip's keys: 18 px wide, 2 apart in a group, the groups 3 | 2 | 3 | 2 six apart, B11 eight further */
float ui_strip_x(int k)
{
    static const float X[11] = {3.f, 23.f, 43.f, 67.f, 87.f, 111.f, 131.f, 151.f, 175.f, 195.f, 221.f};
    return X[k < 0 ? 0 : k > 10 ? 10 : k];
}

/* a label inside a key, squeezed to fit */
static void key_label(const char *s, float cx, float y, float w, qd_rgb col, int weight)
{
    const float tw = ui_TW(s, 7.f, weight, 0.3f);
    if (tw <= w) {
        ui_T(s, cx, y, 7.f, weight, 0.3f, col, QD_CENTER);
        return;
    }
    qd_save();
    qd_translate(cx, y);
    qd_scale(w / tw, 1.f);
    ui_T(s, 0.f, 0.f, 7.f, weight, 0.3f, col, QD_CENTER);
    qd_restore();
}

/* the eleven black keys at the foot, each with what it does here written in it: lit filled in col (its label black),
 * hot outlined in col, faint dark, the others grey outlines; mark: a grey line under the key (the track's default) */
void ui_strip(const char *const lab[11], unsigned lit, unsigned mark, unsigned faint, unsigned hot, qd_rgb col)
{
    const float y = 221.f, h = 14.f;
    int k;
    for (k = 0; k < 11; k++) {
        const float x = ui_strip_x(k), w = k == 10 ? 16.f : 18.f, cx = x + w / 2.f;
        const int l = (lit >> k) & 1u, f = !l && ((faint >> k) & 1u), ho = !l && !f && ((hot >> k) & 1u);
        const qd_rgb oc = l ? QD_NONE : f ? UI_G3 : ho ? col : UI_G2;
        ui_RR(x, y, w, h, 3.f, oc, ho ? 1.2f : 1.f, l ? col : QD_NONE);
        if (lab && lab[k] && lab[k][0])
            key_label(lab[k], cx, y + 9.5f, w - 3.f, l ? UI_BG : f ? UI_G3 : ho ? col : UI_G1, l ? 700 : 600);
        if ((mark >> k) & 1u)
            qd_line(x + 3.f, 238.f, x + w - 3.f, 238.f, UI_G1, 1.5f);
    }
}

/* the value strip (§8b): B1..B10 the mode's ten values, B11 CLR (outlined in the mode's colour while the step
 * overrides it) */
void ui_value_strip(char lab[10][6], unsigned lit, unsigned def, unsigned none, int clr, qd_rgb col)
{
    const char *l[11];
    int k;
    for (k = 0; k < 10; k++)
        l[k] = lab[k];
    l[10] = "CLR";
    ui_strip(l, lit & 0x3ffu, def & 0x3ffu, (none & 0x3ffu) | (clr ? 0u : 1u << 10), clr ? 1u << 10 : 0u, col);
}

/* lock(c, x, y, col, s): a padlock, 7 px wide at s 1 */
void ui_lock(float x, float y, qd_rgb col, float s)
{
    qd_save();
    qd_translate(x, y);
    qd_scale(s, s);
    ui_RR(0.f, 3.f, 7.f, 5.5f, 1.f, QD_NONE, 1.f, col);
    qd_arc(3.5f, 3.f, 2.2f, UI_PI, UI_TAU, col, 1.2f);
    qd_restore();
}

/* knobMark(c, x, y, v, col): a little knob, its pointer at v over 270 degrees */
void ui_knob_mark(float x, float y, float v, qd_rgb col)
{
    const float a = UI_PI * 0.75f + v * UI_PI * 1.5f;
    const float p[4] = {x, y, x + cosf(a) * 5.f, y + sinf(a) * 5.f};
    ui_O(x, y, 6.f, col, 1.3f, QD_NONE);
    ui_P(p, 2, col, 1.3f, 0, QD_NONE);
}

/* keyboard's pos(n): a white key's centre, or a black key's (the boundary of the white keys it sits on) */
float ui_keyboard_x(float x0, float w, int lo, int n, int *black)
{
    static const int WI[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    static const int ISB[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    const float kw = w / 14.f;
    const int d = n - lo, o = d >= 0 ? d / 12 : -((11 - d) / 12), s = d - o * 12, wi = WI[s] + o * 7;
    if (black)
        *black = ISB[s];
    return ISB[s] ? x0 + (float)wi * kw + kw : x0 + (float)wi * kw + kw / 2.f;
}
