/* SPDX-License-Identifier: GPL-3.0-only */
/* mockups/line.html's drawing helpers over qd_gfx (ui.h): the screens port from the mock-ups call for call. */
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
