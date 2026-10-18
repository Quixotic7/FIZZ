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

/* ---- P1.3: line.html's headTrack, keyStrip, lock, knobMark and keyboard's pos */
const char *const UI_FIELD_NAME[UI_NFIELD] = {"NOTE", "VEL", "LEN", "MICRO", "RATCH", "COND", "CHNC", "TRIG",
                                              "SOUND", "LOCKS", "CLEAR"};

/* headTrack(c, left, n, name, lcol): a held step: STEP 5 · FIELD left, [n] the track right */
void ui_head_track(const char *left, int n, const char *name, qd_rgb lcol)
{
    const float w = ui_TW(name, 11.f, 500, 1.f);
    ui_T(left, 8.f, 17.f, 11.f, 500, 1.f, lcol, QD_LEFT);
    ui_T(name, 232.f, 17.f, 11.f, 500, 1.f, UI_G1, QD_RIGHT);
    ui_box(232.f - w - 21.f, 5.f, n, UI_G1);
}

/* keyStrip(c, cur, set): the black keys while a step is held, 3 | 2 | 3 | 2 | 1 */
void ui_key_strip(int cur, unsigned set)
{
    static const int GROUPS[5] = {3, 2, 3, 2, 1};
    float x = 11.f;
    int g, j, k = 0;
    for (g = 0; g < 5; g++) {
        for (j = 0; j < GROUPS[g]; j++, k++) {
            const int on = cur == k, has = (set >> k) & 1u;
            ui_RR(x, 228.f, 14.f, 8.f, 2.f, on ? QD_NONE : has ? UI_G1 : UI_G3, 1.f, on ? UI_K3 : QD_NONE);
            x += 18.f;
        }
        x += 6.f;
    }
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
