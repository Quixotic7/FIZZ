/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.6c: the MIDI FX screen (ui.h ui_mfx_t; mockups/fizz-midifx.html, drawing for drawing). Built through ui_layer.c
 * (the core's source lists stay as they are). */
#include <math.h>
#include <string.h>

#include "ui.h"
#include "../seq/chord.h"
#include "../seq/mfx.h"
#include "../seq/scale.h"

const qd_rgb UI_MFX_COL[11] = {0x8d8d96u, 0x3f8cffu, 0xff2d45u, 0xff8268u, 0xff6fb4u, 0xa86cffu,
                               0xffd23fu, 0xff5418u, 0x28e1ffu, 0x6f72ffu, 0xffa21fu};
static const qd_rgb MF_TP[8] = {0x3f7dffu, 0x1fd47eu, 0xf1f0eau, 0xff6a2cu, 0xff3ca0u, 0xffd23fu, 0x9664ffu, 0x28e1ffu};
#define MF_MINT UI_MFX_MINT
#define MF_TAU 6.28318530718f

/* ---------------------------------------------------------------- the helpers (the mock-up's) */
static float mf_clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static float mf_ease(float x) { x = mf_clamp(x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }
static float mf_lerp(float a, float b, float u) { return a + (b - a) * u; }
static float mf_hash(float n)
{
    const float s = sinf(n * 127.1f + 311.7f) * 43758.5453f;
    return s - floorf(s);
}
static void mf_alpha(float a)
{
    qd_save();
    qd_alpha(qd_get_alpha() * mf_clamp(a, 0.f, 1.f));
}
static void mf_P(const float *p, int n, qd_rgb c, float lw, float a)
{
    mf_alpha(a);
    ui_P(p, n, c, lw, 0, QD_NONE);
    qd_restore();
}
static void mf_Pd(const float *p, int n, qd_rgb c, float lw, float a, float on, float off)
{
    const float d[2] = {on, off};
    mf_alpha(a);
    qd_dash(d, 2);
    ui_P(p, n, c, lw, 0, QD_NONE);
    qd_restore();
}
static void mf_L(float x0, float y0, float x1, float y1, qd_rgb c, float lw, float a)
{
    const float p[4] = {x0, y0, x1, y1};
    mf_P(p, 2, c, lw, a);
}
static void mf_G(const float *p, int n, qd_rgb c, float lw, float a)   /* neon: P2.6f (the owner: the glows slowed the
                                                         * unit's frames) the line alone, no wide faint stroke under it */
{
    mf_P(p, n, c, lw, a);
}
static void mf_O(float x, float y, float r, qd_rgb col, float lw, qd_rgb fill, float a)
{
    mf_alpha(a);
    ui_O(x, y, r < 0.1f ? 0.1f : r, col, lw, fill);
    qd_restore();
}
static void mf_dot(float x, float y, float r, qd_rgb c, float a)   /* (P2.6f: no halo) */
{
    mf_O(x, y, r, QD_NONE, 1.f, c, a);
}
static void mf_RR(float x, float y, float w, float h, float r, qd_rgb col, float lw, qd_rgb fill, float a, int dash)
{
    const float d[2] = {2.f, 2.f};
    mf_alpha(a);
    if (dash)
        qd_dash(d, 2);
    ui_RR(x, y, w, h, r, col, lw, fill);
    qd_restore();
}
static void mf_Q(float x0, float y0, float cx, float cy, float x1, float y1, qd_rgb c, float lw, float a)
{
    float p[2 * 17];
    int i;
    for (i = 0; i <= 16; i++) {
        const float u = (float)i / 16.f;
        p[2 * i] = (1.f - u) * (1.f - u) * x0 + 2.f * (1.f - u) * u * cx + u * u * x1;
        p[2 * i + 1] = (1.f - u) * (1.f - u) * y0 + 2.f * (1.f - u) * u * cy + u * u * y1;
    }
    mf_P(p, 17, c, lw, a);
}
static void mf_GQ(float x0, float y0, float cx, float cy, float x1, float y1, qd_rgb c, float lw, float a)   /* (P2.6f: the
                                                         * curve alone) */
{
    mf_Q(x0, y0, cx, cy, x1, y1, c, lw, a);
}
static void mf_qpt(float x0, float y0, float cx, float cy, float x1, float y1, float u, float *x, float *y)
{
    *x = (1.f - u) * (1.f - u) * x0 + 2.f * (1.f - u) * u * cx + u * u * x1;
    *y = (1.f - u) * (1.f - u) * y0 + 2.f * (1.f - u) * u * cy + u * u * y1;
}
static void mf_along(const float *p, int n, float u, float *x, float *y)   /* a point a fraction u along a polyline */
{
    float L = 0.f, seg[16], d;
    int i;
    for (i = 1; i < n && i <= 16; i++) {
        seg[i - 1] = hypotf(p[2 * i] - p[2 * i - 2], p[2 * i + 1] - p[2 * i - 1]);
        L += seg[i - 1];
    }
    d = mf_clamp(u, 0.f, 1.f) * L;
    for (i = 0; i < n - 1; i++) {
        if (d <= seg[i] || i == n - 2) {
            const float v = seg[i] > 0.f ? d / seg[i] : 0.f;
            *x = mf_lerp(p[2 * i], p[2 * i + 2], v);
            *y = mf_lerp(p[2 * i + 1], p[2 * i + 3], v);
            return;
        }
        d -= seg[i];
    }
    *x = p[0], *y = p[1];
}
static void mf_T(const char *s, float x, float y, float size, int w, float ls, qd_rgb col, int al, float a)
{
    mf_alpha(a);
    ui_T(s, x, y, size, w, ls, col, al);
    qd_restore();
}
static void mf_hexagon(float x, float y, float r, qd_rgb col, float lw, int dashed, float a)
{
    float p[14];
    int k;
    for (k = 0; k <= 6; k++) {
        const float an = -UI_PI / 2.f + (float)(k % 6) * MF_TAU / 6.f;
        p[2 * k] = x + cosf(an) * r, p[2 * k + 1] = y + sinf(an) * r;
    }
    if (dashed)
        mf_Pd(p, 7, col, lw, a, 2.f, 2.f);
    else
        mf_P(p, 7, col, lw, a);
}
void ui_mfx_hex(float x, float y, float r, int n, qd_rgb col, float lw, int dashed)
{
    mf_hexagon(x, y, r, col, lw, dashed, 1.f);
    if (n > 0) {
        char s[2] = {(char)('0' + n % 10), 0};
        const float sz = r * 1.2f < 12.f ? 12.f : r * 1.2f;
        ui_T(s, x, y + sz * 0.36f, sz, 600, 0.f, col, QD_CENTER);
    }
}
static void mf_sq(float x, float y, int n, float s, int ring)   /* a track's colour square */
{
    ui_RR(x, y, s, s, 2.f, QD_NONE, 1.f, MF_TP[(n - 1) & 7]);
    if (ring)
        ui_RR(x - 3.f, y - 3.f, s + 6.f, s + 6.f, 3.5f, UI_K3, 1.2f, QD_NONE);
}

/* each type's small glyph, in a box of 2 s px (the mock-up's glyph()) */
void ui_mfx_glyph(int ty, float cx, float cy, float s, qd_rgb col, float lw)
{
    const float u = s / 8.f;
#define X_(v) (cx + (v) * u)
#define Y_(v) (cy + (v) * u)
#define DOT(x, y, r) ui_O(X_(x), Y_(y), (r) * u, QD_NONE, 1.f, col)
#define RING(x, y, r) ui_O(X_(x), Y_(y), (r) * u, col, lw, QD_NONE)
    switch (ty) {
    case QD_MFX_ARP: {
        const float p[12] = {X_(-7), Y_(6), X_(-3), Y_(6), X_(-3), Y_(1), X_(1), Y_(1), X_(1), Y_(-4), X_(6), Y_(-4)};
        ui_P(p, 6, col, lw, 0, QD_NONE);
        DOT(6, -4, 1.9f);
        break;
    }
    case QD_MFX_CHANCE: {
        const float p[4] = {X_(1.5f), Y_(-7), X_(1.5f), Y_(3)};
        DOT(-6.5f, -2, 1.7f), DOT(-2, -2, 1.7f), DOT(5.5f, -2, 1.7f);
        ui_P(p, 2, col, lw, 0, QD_NONE);
        RING(5.5f, 5.5f, 1.7f);
        break;
    }
    case QD_MFX_REPEAT: {
        static const float H[4][3] = {{-7, -2, 6}, {-2, 2, 4}, {2, 5, 2.4f}, {5, 7.2f, 1.2f}};
        int k;
        for (k = 0; k < 4; k++)
            mf_Q(X_(H[k][0]), Y_(6), X_((H[k][0] + H[k][1]) / 2.f), Y_(6 - 2 * H[k][2]), X_(H[k][1]), Y_(6), col, lw, 1.f);
        DOT(-4.5f, -0.2f, 2);
        break;
    }
    case QD_MFX_HARM: {
        const float a[4] = {X_(-7), Y_(-5), X_(7), Y_(-5)}, b[4] = {X_(-7), Y_(0), X_(3), Y_(0)},
                    c[4] = {X_(-7), Y_(5), X_(-1), Y_(5)}, d[4] = {X_(-7), Y_(-7.5f), X_(-7), Y_(7.5f)};
        ui_P(a, 2, col, lw, 0, QD_NONE), ui_P(b, 2, col, lw, 0, QD_NONE);
        ui_P(c, 2, col, lw, 0, QD_NONE), ui_P(d, 2, col, lw, 0, QD_NONE);
        break;
    }
    case QD_MFX_TRANSPOSE: {
        const float a[4] = {X_(5.5f), Y_(-5), X_(2), Y_(-6.6f)}, b[4] = {X_(5.5f), Y_(-5), X_(3.6f), Y_(-1.8f)};
        DOT(-5, 5, 2.2f);
        mf_Q(X_(-3.5f), Y_(2.5f), X_(-3), Y_(-6), X_(5.5f), Y_(-5), col, lw, 1.f);
        ui_P(a, 2, col, lw, 0, QD_NONE), ui_P(b, 2, col, lw, 0, QD_NONE);
        break;
    }
    case QD_MFX_SCALER: {
        const float a[4] = {X_(-2.5f), Y_(-6), X_(-2.5f), Y_(6)}, b[4] = {X_(2.5f), Y_(-6), X_(2.5f), Y_(6)};
        ui_RR(X_(-7.5f), Y_(-6.5f), 15 * u, 13 * u, 1.5f * u, col, lw, QD_NONE);
        ui_P(a, 2, col, lw, 0, QD_NONE), ui_P(b, 2, col, lw, 0, QD_NONE);
        ui_RR(X_(-4), Y_(-6.5f), 3 * u, 7 * u, 0.6f, QD_NONE, 1.f, col);
        ui_RR(X_(1), Y_(-6.5f), 3 * u, 7 * u, 0.6f, QD_NONE, 1.f, col);
        break;
    }
    case QD_MFX_RANDOM:
        ui_RR(X_(-7), Y_(-7), 14 * u, 14 * u, 2.5f * u, col, lw, QD_NONE);
        DOT(-3.3f, -3.3f, 1.3f), DOT(3.3f, -3.3f, 1.3f), DOT(0, 0, 1.3f), DOT(-3.3f, 3.3f, 1.3f), DOT(3.3f, 3.3f, 1.3f);
        break;
    case QD_MFX_CHORD: {
        const float a[4] = {X_(1.2f), Y_(5), X_(1.2f), Y_(-8)};
        DOT(-1.5f, 5, 2.3f), DOT(-1.5f, 0.5f, 2.3f), DOT(-1.5f, -4, 2.3f);
        ui_P(a, 2, col, lw, 0, QD_NONE);
        break;
    }
    case QD_MFX_MONO: {
        const float a[6] = {X_(-7), Y_(-5.5f), X_(-3), Y_(-5.5f), X_(1.5f), Y_(0)}, b[4] = {X_(-7), Y_(0), X_(1.5f), Y_(0)},
                    c[6] = {X_(-7), Y_(5.5f), X_(-3), Y_(5.5f), X_(1.5f), Y_(0)}, d[4] = {X_(1.5f), Y_(0), X_(6.5f), Y_(0)};
        ui_P(a, 3, col, lw, 0, QD_NONE), ui_P(b, 2, col, lw, 0, QD_NONE);
        ui_P(c, 3, col, lw, 0, QD_NONE), ui_P(d, 2, col, lw, 0, QD_NONE);
        DOT(6.8f, 0, 1.6f);
        break;
    }
    case QD_MFX_SELECTOR: {
        const float a[4] = {X_(-7), Y_(0), X_(-2), Y_(0)}, b[4] = {X_(-2), Y_(0), X_(7), Y_(-5.5f)},
                    c[4] = {X_(-2), Y_(0), X_(7), Y_(0)}, d[4] = {X_(-2), Y_(0), X_(7), Y_(5.5f)};
        ui_P(a, 2, col, lw, 0, QD_NONE);
        DOT(-2, 0, 1.6f);
        ui_P(b, 2, col, lw, 0, QD_NONE);
        mf_P(c, 2, col, lw, 0.4f), mf_P(d, 2, col, lw, 0.4f);
        break;
    }
    default:
        break;
    }
#undef X_
#undef Y_
#undef DOT
#undef RING
}

/* a slot's tile */
static void mf_tile(float x, float y, float w, int ty, int sel, int byp, float glow, float a)
{
    if (sel) {
        y -= 3.f;
        mf_RR(x + 1.f, y + w + 2.f, w - 2.f, 3.f, 1.5f, QD_NONE, 1.f, 0x1d1d24u, a, 0);
    }
    if (!ty) {
        mf_RR(x, y, w, w, 4.f, QD_NONE, 1.f, UI_BG, a, 0);
        mf_RR(x, y, w, w, 4.f, sel ? UI_K3 : UI_G2, sel ? 1.8f : 1.f, QD_NONE, a * (sel ? 0.95f : 0.55f), !sel);
        return;
    }
    {
        const qd_rgb col = UI_MFX_COL[ty];
        const float ta = (byp ? 0.32f : 1.f) * a;
        mf_RR(x, y, w, w, 4.f, QD_NONE, 1.f, UI_BG, a, 0);
        if (glow > 0.f)
            mf_RR(x - 2.f, y - 2.f, w + 4.f, w + 4.f, 6.f, QD_NONE, 1.f, col, 0.22f * glow * a, 0);
        if (sel)
            mf_RR(x - 2.5f, y - 2.5f, w + 5.f, w + 5.f, 6.f, UI_K3, 1.8f, QD_NONE, a, 0);
        mf_RR(x, y, w, w, 4.f, col, 1.4f, QD_NONE, ta * (sel ? 1.f : 0.8f), 0);
        mf_alpha(ta);
        ui_mfx_glyph(ty, x + w / 2.f, y + w / 2.f, w * 0.31f, col, w > 30.f ? 2.f : 1.35f);
        qd_restore();
        if (byp)
            mf_L(x + 2.5f, y + w - 2.5f, x + w - 2.5f, y + 2.5f, UI_G1, 1.5f, a);
    }
}

/* ---------------------------------------------------------------- the head, the rail, the foot */
#define MF_RX(i) (10.f + (float)(i) * 28.f)
#define MF_RY 44.f
#define MF_RW 22.f
static void mf_head(int npg, int pg, const char *title, qd_rgb tcol, int g, const uint8_t *tr, int ntr, int track)
{
    float x = 8.f;
    int i;
    if (npg > 1) {
        for (i = 0; i < npg; i++)
            ui_O(x + 3.f + (float)i * 9.f, 13.f, 2.6f, i == pg ? QD_NONE : UI_G1, 1.f, i == pg ? UI_K3 : QD_NONE);
        x += (float)npg * 9.f + 4.f;
    }
    ui_T(title, x, 17.f, 12.f, 600, 1.f, tcol, QD_LEFT);
    x += ui_TW(title, 12.f, 600, 1.f) + 15.f;
    if (g > 0)
        ui_mfx_hex(x, 12.5f, 10.f, g, MF_MINT, 1.5f, 0);
    else if (g == 0)
        mf_hexagon(x, 12.5f, 10.f, UI_G1, 1.5f, 1, 1.f);
    for (i = 0; i < ntr; i++) {
        const int n = tr[ntr - 1 - i];
        mf_sq(222.f - (float)i * 17.f, 7.5f, n, 10.f, n == track);
    }
    mf_L(8.f, 25.f, 232.f, 25.f, UI_G3, 1.f, 1.f);
}

/* notes running along the rail: a dot stream; an ARP or REPEAT starts a faster stream, CHANCE drops some, HARM / CHORD
 * stack three (the mock-up's railFlow) */
static void mf_flow(const uint8_t *type, const uint8_t *byp, float t, float *hits)
{
    struct { float x, rate; qd_rgb col; int id; } st[9];
    int ns = 0, si, i;
    float items_x[8];
    st[ns].x = 2.f, st[ns].rate = 1.7f, st[ns].col = UI_K3, st[ns].id = 0, ns++;
    for (i = 0; i < 8; i++) {
        items_x[i] = MF_RX(i) + MF_RW / 2.f;
        if (type[i] && !byp[i] && (type[i] == QD_MFX_ARP || type[i] == QD_MFX_REPEAT))
            st[ns].x = items_x[i], st[ns].rate = 6.f, st[ns].col = UI_MFX_COL[type[i]], st[ns].id = i + 1, ns++;
    }
    for (si = 0; si < ns; si++) {
        const float end = si + 1 < ns ? st[si + 1].x : 238.f, speed = 64.f, life = (end - st[si].x) / speed;
        int k;
        for (k = (int)floorf((t - life) * st[si].rate) - 1; k <= (int)floorf(t * st[si].rate); k++) {
            const float age = t - (float)k / st[si].rate, x = st[si].x + age * speed;
            qd_rgb col = st[si].col;
            int mult = 1, m;
            float fell = -1.f;
            if (age < 0.f || age > life)
                continue;
            for (i = 0; i < 8; i++) {
                const float sx = items_x[i];
                if (!type[i] || byp[i] || sx <= st[si].x + 0.5f || sx > end)
                    continue;
                if (fabsf(x - sx) < 13.f) {
                    const float h = 1.f - fabsf(x - sx) / 13.f;
                    hits[i] = h > hits[i] ? h : hits[i];
                }
                if (sx > x)
                    continue;
                if (type[i] == QD_MFX_CHANCE && mf_hash((float)k * 7.31f + (float)i * 3.f + (float)st[si].id * 17.f) > 0.62f) {
                    fell = x - sx;
                    col = UI_MFX_COL[QD_MFX_CHANCE];
                    break;
                }
                if (type[i] == QD_MFX_HARM || type[i] == QD_MFX_CHORD)
                    mult = 3;
                col = UI_MFX_COL[type[i]];
            }
            if (fell >= 0.f) {
                mf_O(x, MF_RY + fell * 0.7f, 1.6f, QD_NONE, 1.f, col, 1.f - fell / 22.f);
                continue;
            }
            for (m = 0; m < mult; m++)
                mf_dot(x, MF_RY + ((float)m - (float)(mult - 1) / 2.f) * 4.4f, 1.8f, col, 1.f);
        }
    }
}

/* P1.6e: where slot i shows while slot `from` is previewed at `to` (the ones between making room) */
static int mf_dpos(int i, int from, int to)
{
    if (from < to && i > from && i <= to)
        return i - 1;
    if (to < from && i >= to && i < from)
        return i + 1;
    return i;
}
static void mf_tri(float x, float y, float dir, qd_rgb c, float a)   /* a small arrow head, pointing dir (-1 / +1) */
{
    const float p[6] = {x - 2.5f * dir, y - 4.f, x + 2.5f * dir, y, x - 2.5f * dir, y + 4.f};
    mf_alpha(a);
    ui_P(p, 3, c, 1.6f, 1, c);
    qd_restore();
}

static void mf_rail(const ui_mfx_t *v)
{
    float hits[8] = {0}, arrow[6] = {232.f, MF_RY - 3.5f, 237.f, MF_RY, 232.f, MF_RY + 3.5f};
    int i;
    mf_L(2.f, MF_RY, 238.f, MF_RY, UI_G2, 1.2f, 1.f);
    mf_P(arrow, 3, UI_G2, 1.2f, 1.f);
    if (v->flow)
        mf_flow(v->type, v->byp, v->t, hits);
    for (i = 0; i < 8; i++) {                           /* a SELECTOR's block: its next slots bracketed */
        if (v->type[i] == QD_MFX_SELECTOR && !v->byp[i]) {
            const int n = v->v[QD_ML_LEN] < 0 ? 0 : v->v[QD_ML_LEN], e = i + n > 7 ? 7 : i + n;
            if (n > 0 && i < 7) {
                const float p[8] = {MF_RX(i + 1) - 1.f, 62.f, MF_RX(i + 1) - 1.f, 64.f, MF_RX(e) + MF_RW + 1.f, 64.f,
                                    MF_RX(e) + MF_RW + 1.f, 62.f};
                mf_P(p, 4, UI_MFX_COL[QD_MFX_SELECTOR], 1.2f, 0.8f);
            }
        }
    }
    for (i = 0; i < 8; i++) {
        float x = MF_RX(i);
        const float y = MF_RY - MF_RW / 2.f;
        float g = hits[i] > v->glow[i] ? hits[i] : v->glow[i];
        float yo = 0.f;
        if (v->lift_from >= 0 && i == v->lift_to)    /* (the data already moved: the carried one is there) */
            continue;
        if (v->drag_from >= 0) {                        /* P1.6e: a slot held: drawn apart; the others making room */
            if (i == v->drag_from)
                continue;
            x = mf_lerp(MF_RX(mf_dpos(i, v->drag_from, v->drag_prev)), MF_RX(mf_dpos(i, v->drag_from, v->drag_to)),
                        mf_ease(v->drag_u));
        }
        if (v->drop >= 0 && i == v->drop)               /* .. dropped: landing */
            yo = -7.f * (1.f - mf_ease(v->drop_u));
        if (v->lift_from >= 0 && v->lift_to > v->lift_from && i >= v->lift_from && i < v->lift_to)
            x = mf_lerp(MF_RX(i + 1), MF_RX(i), mf_ease((v->lift_u - 0.2f) / 0.5f));
        if (v->lift_from >= 0 && v->lift_to < v->lift_from && i > v->lift_to && i <= v->lift_from)
            x = mf_lerp(MF_RX(i - 1), MF_RX(i), mf_ease((v->lift_u - 0.2f) / 0.5f));
        if (v->arm > 0.f && i == v->sel)
            x += sinf(v->t * 46.f) * 2.2f * v->arm;
        mf_tile(x, y + yo, MF_RW, v->type[i], i == v->sel && v->lift_from < 0, v->byp[i], g, 1.f);
        if (i == v->f1_src) {                           /* P1.6e: F1's copy came from here: marked */
            mf_RR(x - 3.5f, y - 6.5f, MF_RW + 7.f, MF_RW + 7.f, 6.f, UI_K2, 1.4f, QD_NONE, 1.f, 1);
            ui_icon(UI_IC_COPY, x + MF_RW / 2.f, y + MF_RW + 12.f, 0.9f, UI_K2);
        }
        if (v->arm > 0.f && i == v->sel) {
            mf_L(x - 1.f, y - 4.f, x + MF_RW + 1.f, y + MF_RW - 2.f, UI_RED, 2.2f, 1.f);
            mf_L(x + MF_RW + 1.f, y - 4.f, x - 1.f, y + MF_RW - 2.f, UI_RED, 2.2f, 1.f);
        }
    }
    if (v->drag_from >= 0) {                            /* P1.6e: the slot held: lifted, at its preview place; arrows
                                                         * either side (it can move) */
        const float x = mf_lerp(MF_RX(v->drag_prev), MF_RX(v->drag_to), mf_ease(v->drag_u));
        const float y = MF_RY - MF_RW / 2.f - 7.f * v->drag_up;
        if (v->drag_up > 0.05f)
            mf_RR(x + 2.f, MF_RY + MF_RW / 2.f + 1.f, MF_RW - 4.f, 3.f, 1.5f, QD_NONE, 1.f, 0x2a2a33u, v->drag_up, 0);
        mf_tile(x, y, MF_RW, v->type[v->drag_from], 1, v->byp[v->drag_from], v->drag_up, 1.f);
        if (v->drag_up > 0.f) {
            const float pa = v->drag_up * (v->drag_moved ? 0.55f : 0.65f + 0.35f * sinf(v->t * 9.f));
            const float off = v->drag_moved ? 0.f : 1.5f * sinf(v->t * 9.f);
            if (v->drag_to > 0)
                mf_tri(x - 6.f - off, y + MF_RW / 2.f, -1.f, UI_K3, pa);
            if (v->drag_to < 7)
                mf_tri(x + MF_RW + 6.f + off, y + MF_RW / 2.f, 1.f, UI_K3, pa);
        }
    }
    if (v->lift_from >= 0) {                            /* picked up: raised above the rail, carried, dropped */
        const float u = v->lift_u;
        const float up = u < 0.15f ? mf_ease(u / 0.15f) : u > 0.75f ? 1.f - mf_ease((u - 0.75f) / 0.15f) : 1.f;
        const float x = mf_lerp(MF_RX(v->lift_from), MF_RX(v->lift_to), mf_ease((u - 0.2f) / 0.5f));
        const float y = MF_RY - MF_RW / 2.f - 9.f * up;
        if (up > 0.05f)
            mf_RR(x + 2.f, MF_RY + MF_RW / 2.f + 1.f, MF_RW - 4.f, 3.f, 1.5f, QD_NONE, 1.f, 0x2a2a33u, up, 0);
        mf_tile(x, y, MF_RW, v->type[v->lift_to], 1, 0, 0.f, 1.f);
    }
}

static void mf_foot(const ui_mfx_t *v, float a)
{
    static const qd_rgb KC[4] = {UI_K1, UI_K2, UI_K3, UI_K4};
    int i;
    for (i = 0; i < 4; i++) {
        const float x = 31.f + (float)i * 59.3f;
        float s = 19.f;
        if (!v->has[i])
            continue;
        mf_T(v->lab[i] ? v->lab[i] : "", x, 204.f, 12.f, 500, 0.f, UI_G1, QD_CENTER, a);
        while (s > 12.f && ui_TW(v->val[i], s, 400, 0.f) > 55.f)   /* (P1.6g: AS PLAYED fits: down to 12 px) */
            s -= 1.f;
        mf_T(v->val[i], x, 228.f, s, 400, 0.f, KC[i], QD_CENTER, a);
        if (v->last == i)
            mf_L(x - 12.f, 209.f, x + 12.f, 209.f, KC[i], 1.6f, a);
    }
}

/* ---------------------------------------------------------------- the big drawings (the middle, y 84..188) */
static float mf_arp_y(int p, int lo) { return 178.f - (float)(p - lo) * 3.55f; }
/* P1.6g: the ARP's picture: the PATTERN's real walk (its notes in play, or C E G dim) as a staircase of clean, even
 * lines over RANGE octaves (the MOD lane's rests gaps, the TRANSPOSE lane's offsets in it), morphing from the last one
 * when it changes, the playing step's dot running along it; at the left a small vertical keyboard on the same scale,
 * its keys lit for the notes the arp plays from (a latch over it in HOLD / 1-ST) */
static void mf_arp(const ui_mfx_t *v)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_ARP];
    const int ns = v->nseq > 0 ? v->nseq : 1, latched = v->v[QD_MA_MODE] == QD_AM_HOLD || v->v[QD_MA_MODE] == QD_AM_1ST;
    const float x0 = 50.f, da = v->nheld ? 1.f : 0.6f, mo = mf_ease(v->morph);
    float ys[48], w, f;
    int i, lo = 127, hi = 0, cur;
    for (i = 0; i < ns; i++)
        lo = v->seq[i] < lo ? v->seq[i] : lo, hi = v->seq[i] > hi ? v->seq[i] : hi;
    for (i = 0; i < v->nheld; i++)
        lo = v->held_p[i] < lo ? v->held_p[i] : lo;
    lo -= 1;
    if (hi - lo > 28)                                   /* (a wide range: squeezed into the middle) */
        lo = hi - 28;
    for (i = 0; i < ns; i++) {                          /* the walk's heights, morphing from the last one */
        const float yn = mf_clamp(mf_arp_y(v->seq[i], lo), 88.f, 184.f);
        if (mo < 1.f && v->nprev > 0) {
            const float yp = mf_clamp(mf_arp_y(v->prev[i < v->nprev ? i : v->nprev - 1], lo), 88.f, 184.f);
            ys[i] = mf_lerp(yp, yn, mo);
        } else {
            ys[i] = yn;
        }
    }
    mf_Pd((const float[]){46.f, mf_arp_y(lo + 13, lo) + 0.5f, 230.f, mf_arp_y(lo + 13, lo) + 0.5f}, 2, UI_G3, 1.f, 1.f, 2.f, 4.f);
    mf_Pd((const float[]){46.f, mf_arp_y(lo + 25, lo) + 0.5f, 230.f, mf_arp_y(lo + 25, lo) + 0.5f}, 2, UI_G3, 1.f, 1.f, 2.f, 4.f);
    {   /* the keyboard strip: a key a semitone, on the staircase's scale; lit: the notes in play */
        int p;
        for (p = lo + 1; p <= lo + 28 && p < 128; p++) {
            const float y = mf_arp_y(p, lo);
            const int bk = (0x54a >> (p % 12)) & 1;     /* (C# D# F# G# A#) */
            int in = -1, k;
            if (y < 90.f || y > 184.f)
                continue;
            for (k = 0; k < v->nheld; k++)
                if (v->held_p[k] == p)
                    in = k;
            if (in >= 0)
                mf_RR(14.f, y - 1.5f, bk ? 13.f : 22.f, 3.f, 1.f, QD_NONE, 1.f,
                      MF_TP[(v->held_t[in] - 1) & 7], 1.f, 0);
            else
                mf_RR(14.f, y - 1.5f, bk ? 13.f : 22.f, 3.f, 1.f, QD_NONE, 1.f, bk ? UI_G3 : UI_G2, 1.f, 0);
        }
        if (latched) {                                  /* HOLD / 1-ST: a latch over the strip */
            const float hy = 86.f;
            mf_O(14.f, hy, 3.f, col, 1.4f, UI_BG, 1.f);
            mf_L(17.f, hy, 36.f, hy, col, 1.4f, 1.f);
            mf_L(36.f, hy, 36.f, hy + 5.f, col, 1.4f, 1.f);
        }
    }
    w = (228.f - x0) / (float)ns;
    cur = v->arp_run && v->nheld ? v->arp_step % ns : (int)(v->t * 5.f) % ns;
    f = v->arp_run && v->nheld ? 0.5f : fmodf(v->t * 5.f, 1.f);
    {
        const float gate = mf_clamp((float)v->v[QD_MA_GATE] / 100.f, 0.05f, 1.f);
        for (i = 0; i < ns; i++) {                      /* each step a clean line, even (no glow) */
            const float x = x0 + (float)i * w;
            if (v->gap[i])
                continue;
            if (i > 0 && !v->gap[i - 1])
                mf_L(x, ys[i - 1], x, ys[i], col, 1.f, 0.45f * da);
            mf_L(x + 1.f, ys[i], x + 1.f + mf_clamp((w - 2.f) * gate, 1.5f, w), ys[i], col, 2.f, (i == cur ? 1.f : 0.7f) * da);
        }
        if (!v->gap[cur]) {                             /* the playing step: its dot, a clean ring as it strikes */
            const float xc = x0 + (float)cur * w + 1.f + (w - 2.f) * gate * mf_clamp(f / gate, 0.f, 1.f);
            mf_dot(xc, ys[cur], 3.2f, v->nheld ? MF_TP[(v->held_t[0] - 1) & 7] : UI_K3, da);
            if (f < 0.3f)
                mf_O(x0 + (float)cur * w + 1.f, ys[cur], 4.f + f * 26.f, col, 1.f, QD_NONE, (1.f - f / 0.3f) * da);
        }
    }
}

static void mf_chance(int pct, float t, float a)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_CHANCE];
    const float y = 162.f, gx = 122.f, speed = 56.f, rate = 3.f;
    char s[8];
    int k;
    s[0] = 0;
    {
        char *q = ui_itoa(s, pct);
        strcat(q, "%");
    }
    mf_T(s, 120.f, 134.f, 42.f, 300, 0.f, col, QD_CENTER, a);
    mf_G((const float[]){gx, y - 19.f, gx, y - 8.f}, 2, col, 2.2f, a);
    mf_G((const float[]){gx, y + 8.f, gx, y + 19.f}, 2, col, 2.2f, a);
    for (k = (int)floorf((t - 5.f) * rate); (float)k <= t * rate; k++) {
        const float age = t - (float)k / rate, x = 10.f + age * speed;
        const int pass = mf_hash((float)k * 3.17f) < (float)pct / 100.f;
        if (x < 10.f || x > 232.f)
            continue;
        if (x < gx || pass) {
            mf_dot(x, y, 3.3f, x < gx ? UI_K3 : col, a * (x > 210.f ? (232.f - x) / 22.f : 1.f));
        } else {
            const float d = x - gx;
            mf_O(gx + d * 0.45f, y + d * d * 0.02f, 3.3f, col, 1.3f, QD_NONE, a * mf_clamp(1.f - d / 55.f, 0.f, 1.f));
        }
    }
}

static void mf_repeat(const ui_mfx_t *v)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_REPEAT];
    const int n = v->v[QD_MR_HITS] < 1 ? 1 : v->v[QD_MR_HITS] > 6 ? 6 : v->v[QD_MR_HITS], fade = v->v[QD_MR_FADE];
    const float fl = 180.f, period = 2.2f, u = fmodf(v->t, period) / period, pos = u * ((float)n + 0.6f);
    const int cur = (int)pos;
    const float f = pos - (float)cur, span = 176.f / (float)n;
    float vel[6], bx, by;
    int i;
    for (i = 0; i < n; i++) {
        const int a = fade < 0 ? 100 : 100 - fade, b = fade < 0 ? 100 + fade : 100;
        vel[i] = (float)(n > 1 ? a + (b - a) * i / (n - 1) : b) / 100.f;
    }
    mf_L(12.f, fl, 228.f, fl, UI_G2, 1.2f, 1.f);
    mf_dot(22.f, 104.f, 4.f, UI_K3, 1.f);
    mf_Pd((const float[]){22.f, 110.f, 22.f, fl - 2.f}, 2, UI_G2, 1.f, 1.f, 2.f, 3.f);
    for (i = 0; i < n; i++) {
        const float x = 52.f + (float)i * span, h = 74.f * vel[i];
        const int lit = cur == i && f < 0.35f;
        mf_G((const float[]){x, fl, x, fl - h}, 2, col, lit ? 3.4f : 2.4f, (lit ? 1.f : 0.55f) * (0.45f + 0.55f * vel[i]));
        if (lit)
            mf_O(x, fl, 6.f + f * 40.f, col, 1.2f, QD_NONE, 1.f - f / 0.35f);
    }
    if (cur >= n) {
        bx = 52.f + (float)(n - 1) * span + f * 30.f;
        by = fl - 6.f - 10.f * sinf(UI_PI * mf_clamp(f, 0.f, 1.f));
    } else {
        const float xa = cur == 0 ? 22.f : 52.f + (float)(cur - 1) * span, xb = 52.f + (float)cur * span;
        const float H = cur == 0 ? 70.f : 64.f * vel[cur - 1];
        bx = mf_lerp(xa, xb, f);
        by = cur == 0 ? mf_lerp(104.f, fl - 6.f, f * f) : fl - 6.f - H * 4.f * f * (1.f - f);
    }
    mf_dot(bx, by, 5.5f, col, cur >= n ? 1.f - f : 1.f);
}

static float mf_harm_y(int p) { return 176.f - (float)p * 5.5f; }
static void mf_harm(const ui_mfx_t *v, float a, int still)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_HARM];
    static const uint8_t ID[6] = {QD_MH_N2, QD_MH_N3, QD_MH_N4, QD_MH_N5, QD_MH_N6, QD_MH_N7};
    static const qd_rgb KC[6] = {UI_K1, UI_K2, UI_K3, UI_K1, UI_K2, UI_K3};
    const float kx = 14.f, kw = 26.f, u = fmodf(v->t, 2.2f) / 2.2f;
    int p, j, nn = 0, ints[7], ik[7];
    mf_RR(kx, mf_harm_y(15) - 2.75f, kw, mf_harm_y(-1) - mf_harm_y(15) + 2.75f, 2.f, UI_G2, 1.2f, QD_NONE, a, 0);
    for (p = 0; p <= 15; p++) {
        const int pc = p % 12;
        if (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10) {
            mf_RR(kx, mf_harm_y(p) - 2.2f, 15.f, 4.4f, 1.f, QD_NONE, 1.f, UI_G2, a, 0);
            mf_L(kx + 15.f, mf_harm_y(p), kx + kw, mf_harm_y(p), UI_G3, 1.f, a);
        }
        if (pc == 4 || pc == 11)
            mf_L(kx, mf_harm_y(p) - 2.75f, kx + kw, mf_harm_y(p) - 2.75f, UI_G3, 1.f, a);
    }
    if (v->v[QD_MH_ORIG])
        ints[nn] = 0, ik[nn++] = -1;
    for (j = 0; j < 6; j++)
        if (v->v[ID[j]] && v->v[ID[j]] >= -1 && v->v[ID[j]] <= 16)
            ints[nn] = v->v[ID[j]], ik[nn++] = j;
    for (j = 0; j < nn; j++) {
        const float g = still ? 1.f : mf_ease((u - (float)j * 0.1f) / 0.12f), y = mf_harm_y(ints[j] < 0 ? 0 : ints[j]);
        const float x1 = mf_lerp(48.f, 176.f, g);
        char s[8];
        if (g <= 0.f)
            continue;
        mf_G((const float[]){48.f, y, x1, y}, 2, ik[j] >= 0 ? col : UI_K3, 3.2f, a * (ik[j] >= 0 ? 1.f : 0.9f));
        mf_dot(kx + kw - 4.f, y, 2.4f, ik[j] >= 0 ? col : UI_K3, a);
        if (ik[j] >= 0) {
            s[0] = ints[j] > 0 ? '+' : 0, s[1] = 0;
            ui_itoa(s + (ints[j] > 0), ints[j]);
            mf_T(s, 186.f, y + 5.f, 15.f, 500, 0.f, KC[ik[j]], QD_LEFT, a * g);
        }
    }
}

static void mf_notehead(float x, float y, qd_rgb col, int fill, float a)
{
    float p[2 * 17];
    int i;
    for (i = 0; i <= 16; i++) {
        const float an = (float)i / 16.f * MF_TAU, ex = cosf(an) * 8.f, ey = sinf(an) * 5.6f;
        p[2 * i] = x + ex * cosf(-0.35f) - ey * sinf(-0.35f);
        p[2 * i + 1] = y + ex * sinf(-0.35f) + ey * cosf(-0.35f);
    }
    mf_alpha(a);
    ui_P(p, 17, col, 1.6f, 1, fill ? col : QD_NONE);
    qd_restore();
}
static void mf_transpose(const ui_mfx_t *v)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_TRANSPOSE];
    const float Ax = 34.f, Ay = 150.f, Mx = 118.f, My = 126.f, Bx = 204.f, By = 172.f, u = fmodf(v->t, 2.4f) / 2.4f;
    char s[8];
    mf_Pd((const float[]){14.f, 150.f, 226.f, 150.f}, 2, UI_G3, 1.f, 1.f, 2.f, 4.f);
    mf_GQ(Ax + 6.f, Ay - 6.f, 70.f, 76.f, Mx, My, UI_K1, 1.6f, 0.85f);
    mf_GQ(Mx, My, 168.f, 92.f, Bx - 7.f, By - 7.f, UI_K2, 1.6f, 0.85f);
    mf_P((const float[]){Bx - 7.f, By - 7.f, Bx - 15.f, By - 9.f}, 2, UI_K2, 1.6f, 1.f);
    mf_P((const float[]){Bx - 7.f, By - 7.f, Bx - 9.f, By - 15.f}, 2, UI_K2, 1.6f, 1.f);
    mf_O(Mx, My, 3.f, UI_G1, 1.2f, UI_BG, 1.f);
    mf_notehead(Ax, Ay, UI_K3, 1, 1.f);
    mf_notehead(Bx, By, col, u > 0.75f, u > 0.75f ? 1.f : 0.6f);
    s[0] = v->v[QD_MT_SEMI] > 0 ? '+' : 0, s[1] = 0;
    ui_itoa(s + (v->v[QD_MT_SEMI] > 0), v->v[QD_MT_SEMI]);
    ui_T(s, 64.f, 104.f, 16.f, 500, 0.f, UI_K1, QD_CENTER);
    s[0] = v->v[QD_MT_OCT] > 0 ? '+' : 0, s[1] = 0;
    ui_itoa(s + (v->v[QD_MT_OCT] > 0), v->v[QD_MT_OCT]);
    ui_T(s, 170.f, 112.f, 16.f, 500, 0.f, UI_K2, QD_CENTER);
    if (u < 0.75f) {
        const float w = u / 0.75f;
        float x, y;
        if (w < 0.5f)
            mf_qpt(Ax + 6.f, Ay - 6.f, 70.f, 76.f, Mx, My, w * 2.f, &x, &y);
        else
            mf_qpt(Mx, My, 168.f, 92.f, Bx - 7.f, By - 7.f, w * 2.f - 1.f, &x, &y);
        mf_dot(x, y, 4.2f, col, 1.f);
    } else {
        mf_O(Bx, By, 8.f + (u - 0.75f) * 60.f, col, 1.3f, QD_NONE, 1.f - (u - 0.75f) / 0.25f);
    }
}

/* a horizontal keyboard: nw white keys kw wide from x0 (C first); pos[pitch 0..23] its dot */
static void mf_keyboard(float x0, float y0, float kw, int nw, float h, float bh, uint32_t inscale, int mark_out,
                        float *px, float *py)
{
    static const int WP[7] = {0, 2, 4, 5, 7, 9, 11};
    int i;
    for (i = 0; i < nw; i++) {
        const int p = (i / 7) * 12 + WP[i % 7];
        const float x = x0 + (float)i * kw;
        ui_RR(x, y0, kw - 1.5f, h, 2.f, mark_out && !((inscale >> (p % 12)) & 1u) ? UI_G3 : UI_G2, 1.1f, 0x050507u);
        if (p < 24)
            px[p] = x + (kw - 1.5f) / 2.f, py[p] = y0 + h - 9.f;
    }
    for (i = 0; i < nw - 1; i++) {
        const int p = (i / 7) * 12 + WP[i % 7];
        float x;
        if (p % 12 == 4 || p % 12 == 11)
            continue;
        x = x0 + (float)(i + 1) * kw - 0.75f - kw * 0.32f;
        ui_RR(x, y0 - 1.f, kw * 0.64f, bh, 2.f, UI_G1, 1.f, 0x121216u);
        if (p + 1 < 24)
            px[p + 1] = x + kw * 0.32f, py[p + 1] = y0 + bh - 8.f;
    }
}
static void mf_scaler(const ui_mfx_t *v, uint32_t mask)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_SCALER];
    float px[24], py[24];
    int p, k, from = -1, to = -1, n = 0;
    memset(px, 0, sizeof px), memset(py, 0, sizeof py);
    mf_keyboard(10.f, 112.f, 22.f, 10, 72.f, 42.f, mask, 1, px, py);
    for (p = 0; p < 17; p++)
        if ((mask >> (p % 12)) & 1u)
            mf_dot(px[p], py[p], 2.6f, col, 0.55f);
    for (p = 1; p < 17; p++)                            /* the notes out of the scale, in turn: each slides down */
        n += !((mask >> (p % 12)) & 1u);
    if (n) {
        const int pick = (int)(v->t / 1.5f) % n;
        for (k = 0, p = 1; p < 17; p++)
            if (!((mask >> (p % 12)) & 1u) && k++ == pick)
                from = p;
        for (to = from; to > 0 && !((mask >> (to % 12)) & 1u); to--)
            ;
    }
    if (from >= 0) {
        const float u = fmodf(v->t, 1.5f) / 1.5f, Ax = px[from], Ay = py[from], Bx = px[to], By = py[to];
        if (u < 0.3f) {
            mf_dot(Ax, mf_lerp(90.f, Ay, mf_ease(u / 0.3f)), 4.f, UI_K3, 1.f);
        } else if (u < 0.45f) {
            mf_dot(Ax, Ay, 4.f, UI_K3, 1.f);
        } else if (u < 0.8f) {
            const float w = mf_ease((u - 0.45f) / 0.35f), mx = (Ax + Bx) / 2.f, my = (Ay < By ? Ay : By) - 22.f;
            float x, y;
            mf_qpt(Ax, Ay, mx, my, Bx, By, w, &x, &y);
            mf_dot(x, y, 4.f, w > 0.5f ? col : UI_K3, 1.f);
        } else {
            mf_dot(Bx, By, 4.4f, col, 1.f);
            mf_O(Bx, By, 5.f + (u - 0.8f) * 60.f, col, 1.2f, QD_NONE, 1.f - (u - 0.8f) / 0.2f);
        }
    }
}

static void mf_random(const ui_mfx_t *v)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_RANDOM];
    const int tick = (int)floorf(v->t * 2.4f);
    const float f = fmodf(v->t * 2.4f, 1.f), fresh = 1.f - mf_clamp(f / 0.3f, 0.f, 1.f);
    const int pr = (int)(mf_hash((float)tick * 1.3f) * 7.f), ppr = (int)(mf_hash((float)(tick - 1) * 1.3f) * 7.f);
    const float pw = (float)v->v[QD_MD_PITCH] / 12.f, vw = (float)v->v[QD_MD_VEL] / 127.f, lw = (float)v->v[QD_MD_LEN] / 100.f;
    const float d[2] = {4.f, 3.f};
    int r;
    qd_save(), qd_dash(d, 2);
    ui_RR(16.f, 90.f, 52.f, 94.f, 5.f, UI_K1, 1.3f, QD_NONE);
    ui_RR(90.f, 104.f, 52.f, 46.f, 5.f, UI_K2, 1.3f, QD_NONE);
    ui_RR(164.f, 126.f, 64.f, 24.f, 5.f, UI_K3, 1.3f, QD_NONE);
    qd_restore();
    for (r = 0; r < 7; r++)                             /* PITCH: the box's rows, the note's own white */
        mf_L(24.f, 99.f + (float)r * 13.f, 60.f, 99.f + (float)r * 13.f, r == 3 ? UI_K3 : UI_G3, r == 3 ? 1.4f : 1.f,
             r == 3 ? 0.7f : 1.f);
    {
        const float sp = pw * 3.f;
        const float y = 99.f + (3.f + ((float)pr - 3.f) * mf_clamp(sp / 3.f, 0.f, 1.f)) * 13.f;
        const float yp = 99.f + (3.f + ((float)ppr - 3.f) * mf_clamp(sp / 3.f, 0.f, 1.f)) * 13.f;
        mf_dot(42.f, yp, 3.f, col, 0.25f * (1.f - fresh));
        mf_dot(42.f, y, 4.f, col, 1.f);
        if (fresh > 0.f)
            mf_O(42.f, y, 5.f + (1.f - fresh) * 10.f, col, 1.2f, QD_NONE, fresh);
    }
    {   /* VELOCITY: a bar whose top lands anywhere in its range */
        const float vy = 127.f + (mf_hash((float)tick * 2.7f + 5.f) * 2.f - 1.f) * 19.f * vw;
        mf_L(96.f, 127.f, 136.f, 127.f, UI_K3, 1.4f, 0.7f);
        mf_G((const float[]){116.f, 184.f, 116.f, vy}, 2, col, 3.f, 1.f);
        mf_dot(116.f, vy, 3.6f, col, 1.f);
    }
    {   /* LENGTH: a note bar that only grows */
        const float lx = 208.f + mf_hash((float)tick * 3.9f + 2.f) * 18.f * lw;
        mf_L(170.f, 132.f, 170.f, 144.f, UI_K3, 1.2f, 0.7f);
        mf_L(208.f, 128.f, 208.f, 148.f, UI_K3, 1.4f, 0.7f);
        mf_G((const float[]){170.f, 138.f, lx, 138.f}, 2, col, 4.f, 1.f);
        mf_L(lx, 133.f, lx, 143.f, col, 1.6f, 1.f);
    }
}

static void mf_chord(const ui_mfx_t *v, const uint8_t *off, int n)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_CHORD];
    const float u = fmodf(v->t, 1.8f) / 1.8f;
    float px[24], py[24];
    int j;
    memset(px, 0, sizeof px), memset(py, 0, sizeof py);
    mf_keyboard(8.f, 140.f, 16.f, 14, 46.f, 27.f, 0xFFFu, 0, px, py);
    mf_T(v->chord, 120.f, 124.f, 34.f, 300, 0.f, col, QD_CENTER, mf_ease(u / 0.35f));
    for (j = 0; j < n; j++) {
        const float g = mf_ease((u - (float)j * 0.08f) / 0.1f);
        const int p = 9 + off[j];                       /* (on A) */
        if (g <= 0.f || p >= 24)
            continue;
        mf_dot(px[p], py[p], j ? 3.2f : 3.6f, j ? col : UI_K3, g);
        if (j)
            mf_L(px[p], py[p] - 6.f, px[p], mf_lerp(py[p] - 6.f, py[p] - 14.f, g), col, 1.f, 0.4f * g);
    }
}

static void mf_mono(float t)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_MONO];
    static const float YS[4] = {96.f, 122.f, 150.f, 176.f};
    const float mx = 136.f, my = 136.f, per = 0.75f;
    const int n0 = (int)floorf(t / per);
    int i, n;
    for (i = 0; i < 4; i++)
        mf_G((const float[]){14.f, YS[i], 66.f, YS[i], mx, my}, 3, col, 1.4f, 0.45f);
    mf_G((const float[]){mx, my, 228.f, my}, 2, col, 1.6f, 0.45f);
    mf_O(mx, my, 4.f, col, 1.6f, UI_BG, 1.f);
    for (n = n0 - 3; n <= n0; n++) {
        static const int LN[4] = {0, 2, 1, 3};
        const float tn = (float)n * per, age = t - tn, travel = 0.55f;
        const int ln = LN[((n % 4) + 4) % 4];
        if (age < 0.f)
            continue;
        if (age < travel) {
            const float p[6] = {14.f, YS[ln], 66.f, YS[ln], mx, my};
            float x, y;
            mf_along(p, 3, age / travel, &x, &y);
            mf_dot(x, y, 3.4f, UI_K3, 1.f);
            continue;
        }
        {
            const float out = age - travel, x = mx + (out < per ? out : per) * 120.f;
            if (out < per) {
                mf_G((const float[]){mx, my, x, my}, 2, col, 3.4f, 1.f);
                mf_dot(x, my, 3.6f, UI_K3, 1.f);
            } else {
                const float fz = (out - per) / 0.25f;
                if (fz < 1.f)
                    mf_L(mx + (x - mx) * fz, my, x, my, col, 3.f, 1.f - fz);
            }
        }
    }
}

static void mf_selector(const ui_mfx_t *v)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_SELECTOR];
    const float sx = 72.f, sy = 136.f, tx = 146.f, ox = 228.f, per = 1.3f;
    const int n = (int)floorf(v->t / per), len = v->v[QD_ML_LEN] < 1 ? 1 : v->v[QD_ML_LEN] > 3 ? 3 : v->v[QD_ML_LEN];
    const float u = fmodf(v->t, per) / per;
    float ty[3];
    int ch, prev, i;
    for (i = 0; i < len; i++)
        ty[i] = len == 1 ? sy : 104.f + (168.f - 104.f) * (float)i / (float)(len - 1);
    ch = v->v[QD_ML_MODE] == QD_SM_UP ? n % len : v->v[QD_ML_MODE] == QD_SM_DOWN ? (len - 1 - n % len)
                                                  : (int)(mf_hash((float)n * 5.3f) * (float)len);
    prev = v->v[QD_ML_MODE] == QD_SM_UP ? (n + len - 1) % len : v->v[QD_ML_MODE] == QD_SM_DOWN ? (len - 1 - (n + len - 1) % len)
                                                                  : (int)(mf_hash((float)(n - 1) * 5.3f) * (float)len);
    mf_G((const float[]){12.f, sy, sx, sy}, 2, col, 1.6f, 0.7f);
    for (i = 0; i < len; i++) {
        mf_G((const float[]){sx, sy, tx - 18.f, ty[i]}, 2, col, 1.6f, i == ch ? 0.75f : 0.22f);
        mf_G((const float[]){tx + 18.f, ty[i], 204.f, sy, ox, sy}, 3, col, 1.4f, i == ch ? 0.6f : 0.22f);
    }
    for (i = 0; i < len; i++) {
        const int k = v->sel + 1 + i;
        mf_tile(tx - 16.f, ty[i] - 16.f, 32.f, k < 8 ? v->type[k] : 0, 0, 0, i == ch && u > 0.45f && u < 0.7f ? 1.f : 0.f,
                i == ch ? 1.f : 0.45f);
    }
    {
        const float a0 = atan2f(ty[prev] - sy, tx - 18.f - sx), a1 = atan2f(ty[ch] - sy, tx - 18.f - sx);
        const float an = mf_lerp(a0, a1, mf_ease(u / 0.18f));
        float x, y;
        mf_G((const float[]){sx, sy, sx + cosf(an) * 26.f, sy + sinf(an) * 26.f}, 2, UI_K3, 2.6f, 1.f);
        mf_O(sx, sy, 4.f, UI_K3, 1.6f, UI_BG, 1.f);
        if (u < 0.25f) {
            mf_along((const float[]){12.f, sy, sx, sy}, 2, u / 0.25f, &x, &y);
            mf_dot(x, y, 3.6f, UI_K3, 1.f);
        } else if (u < 0.45f) {
            mf_along((const float[]){sx, sy, tx - 18.f, ty[ch]}, 2, (u - 0.25f) / 0.2f, &x, &y);
            mf_dot(x, y, 3.6f, col, 1.f);
        } else if (u >= 0.65f) {
            mf_along((const float[]){tx + 18.f, ty[ch], 204.f, sy, ox, sy}, 3, (u - 0.65f) / 0.35f, &x, &y);
            mf_dot(x, y, 3.6f, col, 1.f);
        }
    }
}

static void mf_empty(float t)
{
    const float cx = 120.f, cy = 136.f, pulse = 0.5f + 0.5f * sinf(t * 3.f);
    const int hi = (int)(t * 1.5f) % 10;
    int i;
    mf_RR(cx - 27.f, cy - 27.f, 54.f, 54.f, 9.f, UI_K3, 1.7f, QD_NONE, 0.45f + 0.35f * pulse, 1);
    mf_L(cx - 9.f, cy, cx + 9.f, cy, UI_K3, 1.8f, 0.6f + 0.3f * pulse);
    mf_L(cx, cy - 9.f, cx, cy + 9.f, UI_K3, 1.8f, 0.6f + 0.3f * pulse);
    for (i = 0; i < 10; i++) {
        const float an = -UI_PI / 2.f + (float)i * MF_TAU / 10.f + t * 0.25f;
        mf_alpha(i == hi ? 1.f : 0.3f);
        ui_mfx_glyph(i + 1, cx + cosf(an) * 86.f, cy + sinf(an) * 43.f, 7.f, UI_MFX_COL[i + 1], 1.4f);
        qd_restore();
    }
}

/* PRESETS: the types as a rolodex (NONE first) */
static void mf_roll(float pos)
{
    int k;
    for (k = 0; k < QD_MFX_NTYPE; k++) {
        const float d = (float)k - pos, ad = fabsf(d) < 1.f ? fabsf(d) : 1.f;
        const float y = 136.f + d * 20.f + (d < 0.f ? -1.f : d > 0.f ? 1.f : 0.f) * ad * 12.f;
        const float s = mf_lerp(22.f, 14.f, ad), a = mf_clamp(1.f - fabsf(d) * 0.4f, 0.f, 1.f);
        const qd_rgb col = k ? UI_MFX_COL[k] : UI_G1;
        const float gs = mf_lerp(13.f, 7.f, ad);
        if (fabsf(d) > 1.75f)
            continue;
        if (ad < 0.5f)
            mf_RR(34.f, y - 25.f, 172.f, 50.f, 8.f, col, 1.8f, QD_NONE, 1.f - ad * 2.f, 0);
        if (k) {
            mf_alpha(a);
            ui_mfx_glyph(k, 64.f, y, gs, col, mf_lerp(2.f, 1.4f, ad));
            qd_restore();
        } else {
            mf_RR(64.f - gs, y - gs, gs * 2.f, gs * 2.f, 3.f, UI_G1, 1.2f, QD_NONE, a, 1);
        }
        mf_T(qd_mfx_type_name(k), 86.f, y + s * 0.36f, s < 12.f ? 12.f : s, 400, 0.f, ad < 0.5f ? col : UI_K3, QD_LEFT, a);
    }
    {
        const float at = pos / (float)(QD_MFX_NTYPE - 1);
        mf_L(229.f, 90.f, 229.f, 184.f, UI_G3, 2.f, 1.f);
        mf_L(229.f, 90.f + 94.f * at - 6.f, 229.f, 90.f + 94.f * at + 6.f, MF_MINT, 3.f, 1.f);
    }
}

/* the arp's lanes */
static void mf_lane_cells(int len, int sel, int cur)
{
    int i;
    for (i = 0; i < 16; i++) {
        const float x = 8.f + (float)i * 14.f;
        const int in = i < len;
        if (i == sel)
            continue;
        mf_RR(x, 152.f, 13.f, 32.f, 3.f, in ? UI_G1 : UI_G3, 1.f, i == cur ? 0x132548u : QD_NONE, in ? 0.8f : 0.6f, !in);
    }
}
static void mf_modlane(const ui_mfx_t *v)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_ARP];
    const int len = v->v[QD_MA_MLEN] < 1 ? 1 : v->v[QD_MA_MLEN] > 16 ? 16 : v->v[QD_MA_MLEN], sel = v->lane_sel;
    const int cur = v->lane_cur >= 0 ? v->lane_cur : (int)(v->t * 5.f) % len;
    static const int SEQ[6] = {0, 4, 7, 12, 16, 19};
    int i, ai = 0, last[3], nl = 0;
#define PY(p) (140.f - ((float)(p) + 12.f) * 1.05f)
    mf_lane_cells(len, sel, cur);
    for (i = 0; i < len; i++) {                         /* what the lane makes of the arp: each step's notes above it */
        const int m = v->lane[0][i], on = i == cur;
        const float x = 8.f + (float)i * 14.f;
        int ps[6], np = 0, k;
        switch (m) {
        case QD_AL_PLAY: ps[np++] = SEQ[ai % 6], ai++; break;
        case QD_AL_REPEAT: for (k = 0; k < nl; k++) ps[np++] = last[k]; break;
        case QD_AL_LO: ps[np++] = -12; break;
        case QD_AL_HI: ps[np++] = 31; break;
        case QD_AL_CHORD: ps[np++] = 0, ps[np++] = 4, ps[np++] = 7; break;
        case QD_AL_PWR: ps[np++] = 0, ps[np++] = 19; break;
        case QD_AL_TIE:
            if (nl)
                mf_G((const float[]){x - 4.f, PY(last[0]), x + 11.f, PY(last[0])}, 2, col, 3.4f, on ? 1.f : 0.55f);
            break;
        case QD_AL_REST: break;
        default: ps[np++] = SEQ[(m - QD_AL_N1) % 6]; break;
        }
        for (k = 0; k < np; k++)
            mf_G((const float[]){x + 2.f, PY(ps[k]), x + 11.f, PY(ps[k])}, 2, col, 3.4f, on ? 1.f : 0.55f);
        if (np) {
            nl = np > 3 ? 3 : np;
            for (k = 0; k < nl; k++)
                last[k] = ps[k];
        }
    }
#undef PY
    for (i = 0; i < 16; i++) {
        char s[12];
        const int in = i < len;
        qd_mfx_lane_text(0, v->lane[0][i], s);
        if (!strcmp(s, "x"))
            strcpy(s, "\xc3\x97");                      /* (x: a times sign) */
        mf_T(s, 8.f + (float)i * 14.f + 6.5f, i == sel ? 172.f : 174.f, 14.f, 600, 0.f,
             i == sel ? UI_K3 : in ? (i == cur ? UI_K3 : col) : UI_G2, QD_CENTER, 1.f);
    }
    mf_RR(8.f + (float)sel * 14.f - 2.f, 148.f, 17.f, 37.f, 4.f, UI_K3, 1.8f, QD_NONE, 1.f, 0);
}
static void mf_trlane(const ui_mfx_t *v)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_ARP];
    const int len = v->v[QD_MA_TLEN] < 1 ? 1 : v->v[QD_MA_TLEN] > 16 ? 16 : v->v[QD_MA_TLEN], sel = v->lane_sel;
    const int cur = v->lane_cur >= 0 ? v->lane_cur : (int)(v->t * 5.f) % len;
    const float z = 172.f;
    int i;
    for (i = 0; i < 16; i++) {
        const float x = 8.f + (float)i * 14.f;
        const int in = i < len, val = v->lane[1][i], on = i == cur;
        if (in)
            mf_L(x + 1.f, z, x + 12.f, z, UI_G1, 1.f, 1.f);
        else
            mf_Pd((const float[]){x + 1.f, z, x + 12.f, z}, 2, UI_G3, 1.f, 1.f, 2.f, 2.f);
        if (!in)
            continue;
        if (val) {
            const float h = (float)val * 1.6f;
            mf_G((const float[]){x + 6.5f, z - 3.f, x + 6.5f, z - 3.f - (h > 75.f ? 75.f : h < -10.f ? -10.f : h)}, 2,
                 i == sel ? UI_K3 : col, 5.f, on || i == sel ? 1.f : 0.55f);
        } else {
            mf_O(x + 6.5f, z - 4.f, 1.6f, QD_NONE, 1.f, col, on ? 1.f : 0.5f);
        }
        if (on)
            mf_L(x + 1.f, z + 6.f, x + 12.f, z + 6.f, UI_K3, 2.f, 1.f);
    }
    {
        const float sx = 8.f + (float)sel * 14.f;
        char s[12];
        mf_RR(sx - 2.f, 92.f, 17.f, 90.f, 4.f, UI_K3, 1.6f, QD_NONE, 1.f, 0);
        qd_mfx_lane_text(1, v->lane[1][sel], s);
        ui_T(s, sx > 160.f ? sx - 10.f : sx + 26.f, 108.f, 18.f, 400, 0.f, UI_K1, sx > 160.f ? QD_RIGHT : QD_LEFT);
    }
}

/* ALGORITHM: the track's group, the five groups as mini rails, the track's square moving */
static void mf_gsel(const ui_mfx_t *v)
{
    const float hx = 24.f + v->gsel_pos * 38.4f;
    int gi, i;
    mf_RR(hx - 18.f, 82.f, 36.f, 155.f, 7.f, MF_MINT, 1.6f, QD_NONE, 1.f, 0);
    ui_T("OFF", 24.f, 103.f, 12.f, 600, 0.f, UI_G1, QD_CENTER);
    mf_Pd((const float[]){24.f, 112.f, 24.f, 196.f}, 2, UI_G2, 1.2f, 1.f, 2.f, 3.f);
    for (gi = 1; gi <= 5; gi++) {
        const float x = 24.f + (float)gi * 38.4f;
        const int on = (int)(v->gsel_pos + 0.5f) == gi;
        mf_alpha(on ? 1.f : 0.8f);
        ui_mfx_hex(x, 97.f, 11.f, gi, on ? MF_MINT : UI_G1, 1.5f, 0);
        qd_restore();
        mf_L(x, 112.f, x, 198.f, UI_G3, 1.f, 1.f);
        for (i = 0; i < 8; i++) {
            const float y = 112.f + (float)i * 11.f;
            const int ty = v->gtype[gi - 1][i];
            if (ty)
                mf_RR(x - 8.f, y, 16.f, 9.f, 2.f, QD_NONE, 1.f, UI_MFX_COL[ty], on ? 1.f : 0.7f, 0);
            else
                mf_RR(x - 8.f, y, 16.f, 9.f, 2.f, UI_G3, 1.f, UI_BG, 1.f, 0);
        }
        for (i = 0; i < v->gtrn[gi - 1]; i++)
            mf_sq(x - (float)v->gtrn[gi - 1] * 6.f + 1.f + (float)i * 12.f, 204.f, v->gtr[gi - 1][i], 10.f, 0);
    }
    mf_sq(hx - 5.5f, 220.f, v->track, 11.f, 1);
}

/* a track routed OFF: the notes straight to the voices; the five groups dim at the foot */
static void mf_off(const ui_mfx_t *v)
{
    const float y = 160.f;
    int k, gi;
    mf_Pd((const float[]){2.f, MF_RY, 238.f, MF_RY}, 2, UI_G3, 1.2f, 1.f, 2.f, 4.f);
    ui_T("OFF", 120.f, 128.f, 38.f, 300, 0.f, UI_G1, QD_CENTER);
    mf_sq(14.f, y - 7.f, v->track, 14.f, 0);
    mf_G((const float[]){34.f, y, 196.f, y}, 2, MF_TP[(v->track - 1) & 7], 1.4f, 0.5f);
    mf_alpha(1.f);
    ui_P((const float[]){204.f, y - 6.f, 210.f, y - 6.f, 218.f, y - 12.f, 218.f, y + 12.f, 210.f, y + 6.f, 204.f, y + 6.f}, 6,
         UI_K3, 1.4f, 1, QD_NONE);
    qd_restore();
    for (k = 0; k < 6; k++)
        mf_dot(36.f + fmodf(v->t * 60.f + (float)k * 30.f, 160.f), y, 3.f, MF_TP[(v->track - 1) & 7], 1.f);
    for (gi = 1; gi <= 5; gi++)
        ui_mfx_hex(60.f + (float)(gi - 1) * 30.f, 216.f, 10.f, gi, UI_G2, 1.5f, 1);
}

void ui_mfx_draw(const ui_mfx_t *v)
{
    const int ty = v->sel >= 0 && v->sel < 8 ? v->type[v->sel] : 0;
    const int npg = ty ? qd_mfx_npages(ty) : 0;
    if (v->gsel) {                                      /* ALGORITHM turning: the five groups */
        mf_head(0, 0, "MIDI FX", MF_MINT, (int)(v->gsel_pos + 0.5f), v->tr, v->ntr, v->track);
        mf_gsel(v);
        return;
    }
    mf_head(npg, v->page, "MIDI FX", MF_MINT, v->g, v->tr, v->ntr, v->track);
    if (!v->g && !((v->f1 || v->f2) && v->fstep)) {
        mf_off(v);
        if (v->steps_frame)
            ui_RR(0.f, 0.f, 240.f, 240.f, 8.f, UI_STEP_FRAME, 3.f, QD_NONE);
        return;
    }
    if (v->g)
        mf_rail(v);
    if ((v->f1 || v->f2) && v->fstep) {                 /* P1.6h, STEPS: F1 / F2 on a step: the step as a card, then
                                                         * where it goes (the step view's F1 / F2) */
        const int full = v->f2 ? v->f2_hand >= 0 : v->st_copied;
        const qd_rgb ac = v->f1 || full ? UI_K2 : UI_K4;
        ui_step_card(40.f, 100.f + (full ? 2.f * sinf(v->t * 4.f) : 0.f), 64.f, 60.f, full, v->st_type, v->st_nnote,
                     v->st_note);
        if (full) {
            mf_L(112.f, 130.f, 140.f, 130.f, ac, 1.6f, 1.f);
            mf_tri(142.f, 130.f, 1.f, ac, 1.f);
        }
        ui_icon(full ? UI_IC_PASTE : v->f1 ? UI_IC_COPY : UI_IC_CUT, 176.f, 130.f, 2.6f, ac);
        if (v->steps_frame)
            ui_RR(0.f, 0.f, 240.f, 240.f, 8.f, UI_STEP_FRAME, 3.f, QD_NONE);
        return;
    }
    if (v->f1 || v->f2) {                               /* P1.6e: F1 / F2 held: what they do (the step view's) */
        const int copied = v->f1 && v->f1_src >= 0, hand = v->f2 ? v->f2_hand : copied ? v->type[v->f1_src] : -1;
        const qd_rgb ac = v->f1 ? UI_K2 : hand >= 0 ? UI_K2 : UI_K4;
        if (hand >= 0) {                                /* the copy / the hand's effect, lifted, then where it goes */
            mf_RR(54.f, 166.f, 36.f, 4.f, 2.f, QD_NONE, 1.f, 0x2a2a33u, 1.f, 0);
            mf_tile(50.f, 108.f + 2.f * sinf(v->t * 4.f), 44.f, hand, 1, 0, 0.6f, 1.f);
            mf_L(108.f, 130.f, 140.f, 130.f, ac, 1.6f, 1.f);
            mf_tri(142.f, 130.f, 1.f, ac, 1.f);
            ui_icon(UI_IC_PASTE, 176.f, 130.f, 2.6f, ac);
        } else if (v->f1) {                             /* nothing picked yet: COPY */
            mf_RR(50.f, 108.f, 44.f, 44.f, 6.f, UI_G2, 1.2f, QD_NONE, 1.f, 1);
            ui_icon(UI_IC_COPY, 72.f, 130.f, 2.2f, UI_G1);
            ui_icon(UI_IC_COPY, 176.f, 130.f, 2.6f, ac);
        } else {                                        /* F2, the hand empty: PICK UP */
            mf_RR(50.f, 108.f, 44.f, 44.f, 6.f, UI_G2, 1.2f, QD_NONE, 1.f, 1);
            ui_icon(UI_IC_CUT, 176.f, 130.f, 2.6f, ac);
        }
        if (v->steps_frame)
            ui_RR(0.f, 0.f, 240.f, 240.f, 8.f, UI_STEP_FRAME, 3.f, QD_NONE);
        return;
    }
    if (v->roll) {                                      /* PRESETS: the types' rolodex */
        qd_save();
        qd_clip_rect(0.f, 84.f, 240.f, 104.f);
        mf_roll(v->roll_pos);
        qd_restore();
        return;
    }
    if (ty)
        mf_T(qd_mfx_type_name(ty), 10.f, 80.f, 14.f, 500, 0.5f, UI_MFX_COL[ty], QD_LEFT, v->byp_sel ? 0.45f : 1.f);
    qd_save();
    qd_clip_rect(0.f, 84.f, 240.f, 104.f);
    if (v->byp_sel)                                     /* bypassed: its picture faded, the notes running round it */
        qd_alpha(qd_get_alpha() * 0.28f);
    switch (ty) {
    case QD_MFX_NONE: mf_empty(v->t); break;
    case QD_MFX_ARP:
        if (v->page == 2)
            mf_modlane(v);
        else if (v->page == 3)
            mf_trlane(v);
        else
            mf_arp(v);
        break;
    case QD_MFX_CHANCE: mf_chance(v->v[QD_MC_CHANCE], v->t, 1.f); break;
    case QD_MFX_REPEAT: mf_repeat(v); break;
    case QD_MFX_HARM: mf_harm(v, 1.f, 0); break;
    case QD_MFX_TRANSPOSE: mf_transpose(v); break;
    case QD_MFX_SCALER: {
        uint32_t mask = 0;
        int p;
        for (p = 0; p < 12; p++)
            mask |= qd_scale_has(v->v[QD_MZ_SCALE], v->v[QD_MZ_ROOT], 60 + p) ? 1u << p : 0u;
        mf_scaler(v, mask);
        break;
    }
    case QD_MFX_RANDOM: mf_random(v); break;
    case QD_MFX_CHORD: {
        const qd_shape_t *sh = &QD_SHAPE[v->v[QD_MK_SHAPE] > 0 && v->v[QD_MK_SHAPE] < QD_NSHAPE ? v->v[QD_MK_SHAPE] : 1];
        uint8_t off[6];
        int n = v->v[QD_MK_NOTES] < 1 ? 1 : v->v[QD_MK_NOTES] > 6 ? 6 : v->v[QD_MK_NOTES], j;
        for (j = 0; j < n; j++)
            off[j] = (uint8_t)((j % (sh->n + 1) ? sh->off[j % (sh->n + 1) - 1] : 0) + 12 * (j / (sh->n + 1)));
        mf_chord(v, off, n);
        break;
    }
    case QD_MFX_MONO: mf_mono(v->t); break;
    case QD_MFX_SELECTOR: mf_selector(v); break;
    default: break;
    }
    qd_restore();
    if (v->arm > 0.f) {                                 /* B11 armed: a red X over the picture, shaking */
        const float sh = sinf(v->t * 46.f) * 2.5f;
        mf_G((const float[]){70.f + sh, 94.f, 170.f + sh, 184.f}, 2, UI_RED, 4.f, 1.f);
        mf_G((const float[]){170.f + sh, 94.f, 70.f + sh, 184.f}, 2, UI_RED, 4.f, 1.f);
    }
    if (ty && v->lift_from < 0)
        mf_foot(v, v->byp_sel ? 0.4f : 1.f);
    if (v->steps_frame)                                 /* P1.6e: STEPS' steps held: the step's frame */
        ui_RR(0.f, 0.f, 240.f, 240.f, 8.f, UI_STEP_FRAME, 3.f, QD_NONE);
}

/* a held step's MIDI FX: m 0 TRACK (the track's group g), 1 OFF, 2.. a group */
void ui_mfx_step_draw(int step, int m, int g, const uint8_t type[8], const char *trig, float t)
{
    char head[32], n[12];
    const float pulse = 0.5f + 0.5f * sinf(t * 3.f);
    int i;
    ui_RR(0.f, 0.f, 240.f, 240.f, 8.f, UI_STEP_FRAME, 3.f, QD_NONE);
    head[0] = 0;
    strcat(head, "STEP "), strcat(head, ui_itoa(n, step)), strcat(head, " \xc2\xb7 MIDI FX");
    ui_T(head, 10.f, 19.f, 12.f, 600, 1.f, UI_K3, QD_LEFT);
    mf_L(10.f, 27.f, 230.f, 27.f, UI_G3, 1.f, 1.f);
    if (g > 0) {
        mf_hexagon(120.f, 92.f, 36.f + pulse * 2.f, MF_MINT, 6.f, 0, 0.12f);
        ui_mfx_hex(120.f, 92.f, 34.f, g, MF_MINT, 2.4f, 0);
        for (i = 0; i < 8; i++)
            mf_tile(24.f + (float)i * 24.f, 146.f, 18.f, type[i], 0, 0, 0.f, 1.f);
        mf_L(12.f, 155.f, 24.f, 155.f, UI_G2, 1.f, 1.f);
        mf_L(216.f, 155.f, 228.f, 155.f, UI_G2, 1.f, 1.f);
        for (i = 0; i < 7; i++)
            mf_L(42.f + (float)i * 24.f, 155.f, 48.f + (float)i * 24.f, 155.f, UI_G2, 1.f, 1.f);
    } else {
        mf_hexagon(120.f, 92.f, 34.f, UI_G1, 2.f, 1, 1.f);
        ui_T("OFF", 120.f, 104.f, 30.f, 300, 0.f, UI_G1, QD_CENTER);
    }
    if (m == QD_SMFX_TRACK)                             /* (TRACK: the track's group, said small under it) */
        ui_T("TRACK", 120.f, 140.f - (g > 0 ? 0.f : -10.f), 12.f, 600, 1.f, UI_G1, QD_CENTER);
    ui_T("TRIG", 31.f, 204.f, 12.f, 500, 0.f, UI_G1, QD_CENTER);
    ui_T(trig ? trig : "NOTE", 31.f, 228.f, 19.f, 400, 0.f, UI_K1, QD_CENTER);
    ui_T("MIDI FX", 90.f, 204.f, 12.f, 500, 0.f, UI_G1, QD_CENTER);
    if (m == QD_SMFX_TRACK)
        ui_T("TRACK", 90.f, 228.f, 17.f, 400, 0.f, UI_K2, QD_CENTER);
    else if (g > 0)
        ui_mfx_hex(90.f, 222.f, 11.f, g, UI_K2, 1.5f, 0);
    else
        ui_T("OFF", 90.f, 228.f, 19.f, 400, 0.f, UI_K2, QD_CENTER);
    mf_L(78.f, 209.f, 102.f, 209.f, UI_K2, 1.6f, 1.f);
}
