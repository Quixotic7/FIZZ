/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* T1: TOOLS' screens (ui.h ui_tools_t; mockups/fizz-tools.html, screens 1, 2, 5, 6, 9, 11, 12, 13). Built through
 * ui_layer.c (it includes this file). Text only for the values, the tool's name popped on a switch, the scope's
 * takeover; everything else is a picture: the ring of steps (ROTATE), the mirror (REVERSE), the polygon of hits
 * (EUCLID), the pages (LENGTH), the lane at the foot (solid kept, ring new, faded going). */
#include "../seq/tools.h"

static const char *const TL_NAME[QD_TL_N] = {"ROTATE", "REVERSE", "RANDOM", "EUCLID", "GRIDS", "HUMANIZE", "LENGTH"};
static const char *const TL_SCOPE[QD_TS_N] = {"SELECTED", "PAGE", "TRACK", "PATTERN"};

static void tl_alpha(float a)                           /* (a part of the alpha now; qd_restore after) */
{
    qd_save();
    qd_alpha(qd_get_alpha() * a);
}
static void tl_line(float x0, float y0, float x1, float y1, qd_rgb c, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, c, lw, 0, QD_NONE);
}

/* each tool as a little picture, centred at x, y */
void ui_tools_icon(int i, float x, float y, int on)
{
    const qd_rgb col = on ? UI_K3 : QD_TL_BUILT(i) ? UI_G2 : UI_G3;
    const float lw = on ? 1.8f : 1.5f;
    switch (i) {
    case QD_TL_ROTATE: {                                /* a turning arrow */
        const float ex = x + cosf(1.9f) * 6.f, ey = y + sinf(1.9f) * 6.f;
        const float h[6] = {ex + 3.5f, ey - 1.f, ex, ey, ex + 0.5f, ey - 4.f};
        qd_arc(x, y, 6.f, -2.6f, 1.9f, col, lw);
        ui_P(h, 3, col, lw, 0, QD_NONE);
        break;
    }
    case QD_TL_REVERSE: {                               /* two arrows the other way round */
        const float a[6] = {x + 2.f, y - 6.f, x + 5.f, y - 3.f, x + 2.f, y};
        const float b[6] = {x - 2.f, y, x - 5.f, y + 3.f, x - 2.f, y + 6.f};
        tl_line(x - 7.f, y - 3.f, x + 5.f, y - 3.f, col, lw);
        ui_P(a, 3, col, lw, 0, QD_NONE);
        tl_line(x + 7.f, y + 3.f, x - 5.f, y + 3.f, col, lw);
        ui_P(b, 3, col, lw, 0, QD_NONE);
        break;
    }
    case QD_TL_RANDOM:                                  /* a die */
        ui_RR(x - 7.f, y - 7.f, 14.f, 14.f, 3.f, col, lw, QD_NONE);
        ui_O(x - 3.f, y - 3.f, 1.4f, QD_NONE, 1.f, col);
        ui_O(x + 3.f, y + 3.f, 1.4f, QD_NONE, 1.f, col);
        ui_O(x, y, 1.4f, QD_NONE, 1.f, col);
        break;
    case QD_TL_EUCLID: {                                /* a ring, three hits */
        int k;
        ui_O(x, y, 6.5f, col, lw * 0.8f, QD_NONE);
        for (k = 0; k < 3; k++) {
            const float a = -UI_PI / 2.f + (float)k * UI_TAU / 3.f;
            ui_O(x + cosf(a) * 6.5f, y + sinf(a) * 6.5f, 2.f, QD_NONE, 1.f, col);
        }
        break;
    }
    case QD_TL_GRIDS:                                   /* the map, a dot */
        ui_RR(x - 7.f, y - 7.f, 14.f, 14.f, 2.f, col, lw * 0.8f, QD_NONE);
        tl_line(x, y - 7.f, x, y + 7.f, col, 1.f);
        tl_line(x - 7.f, y, x + 7.f, y, col, 1.f);
        ui_O(x + 3.f, y - 3.f, 2.2f, QD_NONE, 1.f, col);
        break;
    case QD_TL_HUMANIZE: {                              /* a wobble over the grid */
        float p[30];
        int u;
        for (u = -7; u <= 7; u++)
            p[(u + 7) * 2] = x + (float)u, p[(u + 7) * 2 + 1] = y + sinf((float)u * 1.3f) * 2.5f + (u % 3 ? 0.f : 1.5f);
        ui_P(p, 15, col, lw, 0, QD_NONE);
        tl_line(x - 7.f, y + 6.f, x + 7.f, y + 6.f, col, 1.f);
        break;
    }
    default: {                                          /* LENGTH: a page, and a dashed one after it */
        static const float D[2] = {2.f, 2.f};
        ui_RR(x - 8.f, y - 4.f, 7.f, 8.f, 1.5f, col, lw * 0.8f, QD_NONE);
        qd_save();
        qd_dash(D, 2);
        ui_RR(x + 1.f, y - 4.f, 7.f, 8.f, 1.5f, col, lw * 0.8f, QD_NONE);
        qd_restore();
        break;
    }
    }
}

/* the scope: PAGE one block, TRACK a long lane, PATTERN the lanes */
void ui_tools_scope_icon(int sc, float x, float y, qd_rgb col, float s)
{
    qd_save();
    qd_translate(x, y);
    qd_scale(s, s);
    if (sc == QD_TS_SEL) {                              /* a few steps picked out of a row */
        ui_RR(-11.f, -3.f, 6.f, 6.f, 1.f, QD_NONE, 1.f, col);
        ui_RR(-3.f, -3.f, 6.f, 6.f, 1.f, UI_G2, 1.2f, QD_NONE);
        ui_RR(5.f, -3.f, 6.f, 6.f, 1.f, QD_NONE, 1.f, col);
    } else if (sc == QD_TS_PAGE) {
        ui_RR(-5.f, -6.f, 10.f, 12.f, 1.5f, col, 1.6f, col);
        tl_line(-11.f, 0.f, -7.f, 0.f, UI_G2, 1.4f);
        tl_line(7.f, 0.f, 11.f, 0.f, UI_G2, 1.4f);
    } else if (sc == QD_TS_TRACK) {
        ui_RR(-11.f, -4.f, 22.f, 8.f, 1.5f, col, 1.6f, col);
    } else {
        static const uint8_t TR[4] = {0, 1, 3, 4};
        int k;
        for (k = 0; k < 4; k++)
            ui_RR(-11.f, -7.f + (float)k * 4.f, 22.f, 3.f, 1.f, QD_NONE, 1.f, k == 1 ? col : UI_TRACK_PAL[TR[k]]);
    }
    qd_restore();
}

static void tl_head(const ui_tools_t *v)
{
    int i;
    for (i = 0; i < QD_TL_N; i++) {
        ui_tools_icon(i, 13.f + (float)i * 21.f, 14.f, i == v->tool);
        if (i == v->tool)
            tl_line(4.f + (float)i * 21.f, 27.f, 22.f + (float)i * 21.f, 27.f, UI_K3, 2.f);
    }
    if (v->nolive) {                                    /* drawn only: no undo (the arrow crossed out) */
        ui_icon(UI_IC_UNDO, 160.f, 14.f, 1.f, UI_KSEL);
        tl_line(154.f, 20.f, 166.f, 8.f, UI_KSEL, 1.6f);
    } else if (v->pending) {                            /* a change pending (heard and seen) */
        tl_alpha(0.6f + 0.4f * sinf(v->t * 8.f));
        ui_O(160.f, 14.f, 3.f, QD_NONE, 1.f, UI_K4);
        qd_restore();
    }
    if (v->scope != QD_TS_PATTERN) {
        char n[12];
        ui_RR(172.f, 5.f, 18.f, 18.f, 2.f, v->tcol, 1.4f, QD_NONE);
        ui_T(ui_itoa(n, v->track), 181.f, 19.f, 13.f, 600, 0.f, v->tcol, QD_CENTER);
    } else {
        static const float D[2] = {2.f, 2.f};
        qd_save();
        qd_dash(D, 2);
        ui_RR(172.f, 5.f, 18.f, 18.f, 2.f, UI_G1, 1.4f, QD_NONE);
        qd_restore();
    }
    ui_tools_scope_icon(v->scope, 216.f, 14.f, UI_K3, 1.f);
}

static float tl_noteh(const ui_tools_t *v, int i, float span)   /* a step's note as a height 0..span */
{
    return v->note[i] && v->hi > v->lo ? span * (float)(v->note[i] - v->lo) / (float)(v->hi - v->lo) : 0.f;
}

/* the lane at the foot: the page as it will be */
static void tl_lane(const ui_tools_t *v, int heights)
{
    const float y = 206.f;
    const float pulse = 0.6f + 0.4f * sinf(v->t * 6.f);
    int i;
    for (i = 0; i < 16; i++) {
        const float x = 8.f + (float)i * 14.f, h = heights ? 8.f + tl_noteh(v, i, 10.f) : 18.f;
        if (v->on[i] && v->was[i]) {
            ui_RR(x, y + 18.f - h, 13.f, h, 2.f, QD_NONE, 1.f, v->tcol);
        } else if (v->on[i]) {
            tl_alpha(pulse);
            ui_RR(x, y + 18.f - h, 13.f, h, 2.f, UI_K3, 1.8f, QD_NONE);
            qd_restore();
        } else if (v->was[i]) {
            tl_alpha(0.22f);
            ui_RR(x, y + 18.f - h, 13.f, h, 2.f, QD_NONE, 1.f, v->tcol);
            qd_restore();
        } else {
            ui_RR(x, y + 14.f, 13.f, 4.f, 1.f, QD_NONE, 1.f, i % 4 ? UI_G3 : UI_G2);
        }
    }
    if (v->play >= 0)
        tl_line(8.f + (float)v->play * 14.f + 6.f, y + 22.f, 8.f + (float)v->play * 14.f + 6.f, y + 26.f, UI_RED, 2.4f);
    for (i = 0; i < 16; i++)                            /* the selected steps: a mark over each */
        if ((v->sel >> i) & 1)
            ui_RR(8.f + (float)i * 14.f + 3.f, y - 7.f, 7.f, 3.f, 1.f, QD_NONE, 1.f, UI_KSEL);
}

/* the ring both ROTATE and EUCLID draw: n points round it (n > 32: the hits only), on[i] a hit in col; here: the one
 * playing (bigger) */
#define TL_CX 120.f
#define TL_CY 106.f
#define TL_R 64.f
static void tl_ring(int n, const uint8_t *on, qd_rgb col, int here)
{
    int i;
    ui_O(TL_CX, TL_CY, TL_R, UI_G3, 1.5f, QD_NONE);
    for (i = 0; i < n; i++) {
        const float a = -UI_PI / 2.f + (float)i * UI_TAU / (float)n, x = TL_CX + cosf(a) * TL_R, y = TL_CY + sinf(a) * TL_R;
        if (on[i]) {                                    /* (no glows: the owner's, for the frame rate) */
            ui_O(x, y, n > 32 ? 3.f : i == here ? 9.5f : 7.f, QD_NONE, 1.f, col);
        } else if (n <= 32) {
            ui_O(x, y, 2.6f, QD_NONE, 1.f, i == 0 ? UI_K3 : UI_G2);
        }
    }
}

/* ROTATE: the page as a ring of steps; the sweep and the offset in KNOB 1's blue */
static void tl_rotate(const ui_tools_t *v)
{
    char s[16];
    int off = v->rot;
    tl_ring(16, v->on, v->tcol, v->play);
    off = off > 16 ? 16 : off < -16 ? -16 : off;        /* (the sweep: at most a turn) */
    if (off)
        qd_arc(TL_CX, TL_CY, TL_R - 22.f, -UI_PI / 2.f + (off < 0 ? (float)off * UI_TAU / 16.f : 0.f),
               -UI_PI / 2.f + (off > 0 ? (float)off * UI_TAU / 16.f : 0.f), UI_K1, 3.f);
    s[0] = v->rot > 0 ? '+' : 0, s[1] = 0;
    ui_itoa(s + (v->rot > 0), v->rot);
    ui_T(s, TL_CX, TL_CY + 15.f, 40.f, 700, 0.f, UI_K1, QD_CENTER);
    tl_lane(v, 0);
}

/* REVERSE: the steps as bars (their notes' heights) about the mirror line */
static void tl_reverse(const ui_tools_t *v)
{
    static const float D[2] = {3.f, 4.f};
    const float cy = 108.f;
    int i;
    qd_save();
    qd_dash(D, 2);
    tl_line(120.f, 40.f, 120.f, 176.f, v->flip ? UI_K4 : UI_K3, 1.4f);
    qd_restore();
    for (i = 0; i < 16; i++) {
        const float x = 14.f + (float)i * 13.6f, h = 18.f + tl_noteh(v, i, 50.f);
        if (v->on[i]) {
            ui_RR(x - 6.f, cy - h / 2.f, 12.f, h, 3.f, QD_NONE, 1.f, v->tcol);
        } else {
            ui_O(x, cy, 2.2f, QD_NONE, 1.f, UI_G2);
        }
    }
    tl_lane(v, 1);
}

/* EUCLID: the ring and its polygon of hits; hits big (KNOB 1), the length (KNOB 2), the rotation's tick (KNOB 3), the
 * landing's three pictures (ALGORITHM) */
static void tl_euclid(const ui_tools_t *v)
{
    const int n = v->len < 1 ? 1 : v->len > QD_NSTEP ? QD_NSTEP : v->len;
    uint8_t hit[QD_NSTEP];
    float poly[2 * 64];
    char s[12];
    int i, np = 0, m;
    for (i = 0; i < n; i++)
        if ((hit[i] = (uint8_t)qd_tl_euclid_hit(i, v->hits, n, v->erot)) != 0) {
            const float a = -UI_PI / 2.f + (float)i * UI_TAU / (float)n;
            if (np < 64)
                poly[np * 2] = TL_CX + cosf(a) * TL_R, poly[np * 2 + 1] = TL_CY + sinf(a) * TL_R;
            np++;
        }
    if (np > 2 && np <= 64) {
        tl_alpha(0.10f);
        ui_P(poly, np, QD_NONE, 2.f, 1, UI_K1);
        qd_restore();
        ui_P(poly, np, UI_K1, 2.f, 1, QD_NONE);
    }
    tl_ring(n, hit, UI_K1, v->epos);
    if (v->epos >= 0) {
        const float a = -UI_PI / 2.f + (float)v->epos * UI_TAU / (float)n;
        tl_line(TL_CX, TL_CY, TL_CX + cosf(a) * (TL_R - 14.f), TL_CY + sinf(a) * (TL_R - 14.f), UI_RED, 2.f);
    }
    if (v->erot) {
        const float a = -UI_PI / 2.f - (float)v->erot * UI_TAU / (float)n;
        ui_O(TL_CX + cosf(a) * (TL_R + 12.f), TL_CY + sinf(a) * (TL_R + 12.f), 3.f, QD_NONE, 1.f, UI_K3);
    }
    ui_T(ui_itoa(s, v->hits), TL_CX, TL_CY + 6.f, 40.f, 700, 0.f, UI_K1, QD_CENTER);
    ui_T(ui_itoa(s, n), TL_CX, TL_CY + 30.f, 18.f, 600, 0.f, UI_K2, QD_CENTER);
    for (m = 0; m < QD_EU_N; m++) {
        const float x = 196.f, y = 50.f + (float)m * 22.f;
        const qd_rgb col = m == v->emode ? UI_K4 : UI_G2;
        ui_RR(x, y, 14.f, 14.f, 2.f, col, 1.6f, m == QD_EU_NEW && m == v->emode ? col : QD_NONE);
        if (m == QD_EU_ADD) {
            tl_line(x + 7.f, y + 3.f, x + 7.f, y + 11.f, col, 1.8f);
            tl_line(x + 3.f, y + 7.f, x + 11.f, y + 7.f, col, 1.8f);
        } else if (m == QD_EU_MASK) {
            ui_RR(x + 4.f, y + 4.f, 6.f, 6.f, 1.f, QD_NONE, 1.f, col);
        }
    }
    tl_lane(v, 0);
}

/* LENGTH: the eight pages (looped filled, a new one outlined, one leaving faded), the steps big, x2 / /2 */
static void tl_length(const ui_tools_t *v)
{
    char s[12];
    int p;
    for (p = 0; p < 8; p++) {
        const float x = 12.f + (float)p * 27.f, y = 70.f;
        const int now = (v->loop >> p) & 1, was = (v->loop0 >> p) & 1;
        if (now && was) {
            ui_RR(x, y, 23.f, 60.f, 3.f, QD_NONE, 1.f, v->tcol);
        } else if (now) {
            ui_RR(x, y, 23.f, 60.f, 3.f, UI_K3, 1.8f, QD_NONE);
        } else if (was) {
            tl_alpha(0.22f);
            ui_RR(x, y, 23.f, 60.f, 3.f, QD_NONE, 1.f, v->tcol);
            qd_restore();
        } else {
            ui_RR(x, y, 23.f, 60.f, 3.f, UI_G3, 1.4f, QD_NONE);
        }
    }
    ui_T(ui_itoa(s, v->count), 120.f, 182.f, 40.f, 700, 0.f, UI_K3, QD_CENTER);
    {   /* the two ways: halving left, doubling right (the way taken bright) */
        const qd_rgb l = v->lpow < 0 ? UI_K3 : UI_G1, r = v->lpow > 0 ? UI_K3 : UI_G1;
        const float a[6] = {40.f, 168.f, 32.f, 174.f, 40.f, 180.f}, b[6] = {200.f, 168.f, 208.f, 174.f, 200.f, 180.f};
        ui_P(a, 3, l, 2.4f, 0, QD_NONE);
        ui_P(b, 3, r, 2.4f, 0, QD_NONE);
        tl_line(50.f, 174.f, 58.f, 174.f, l, 1.6f);     /* (a division sign, drawn) */
        ui_O(54.f, 170.f, 1.2f, QD_NONE, 1.f, l);
        ui_O(54.f, 178.f, 1.2f, QD_NONE, 1.f, l);
        ui_T("2", 61.f, 180.f, 14.f, 600, 0.f, l, QD_LEFT);
        ui_T("\xc3\x97" "2", 188.f, 180.f, 14.f, 600, 0.f, r, QD_RIGHT);
    }
}

void ui_tools_draw(const ui_tools_t *v)
{
    tl_head(v);
    switch (v->tool) {
    case QD_TL_ROTATE: tl_rotate(v); break;
    case QD_TL_REVERSE: tl_reverse(v); break;
    case QD_TL_EUCLID: tl_euclid(v); break;
    case QD_TL_LENGTH: tl_length(v); break;
    default: tl_lane(v, 0); break;
    }
}

/* the popup: a tool's name (kind 0), the scope's takeover (1: SELECTED / PAGE / TRACK / PATTERN, the chosen big), a
 * message (2: the tool's name and what happened). No frame: a dark band under the words (the caller's alpha fades it) */
static const char *const TL_MSG[] = {"KEPT", "REVERTED"};
void ui_tools_pop(int kind, int msg, int tool, int scope)
{
    int k;
    if (kind != 1) {
        qd_fill_rect(0.f, 84.f, 240.f, kind ? 72.f : 56.f, UI_BG);
        ui_T(TL_NAME[tool % QD_TL_N], 120.f, 126.f, 34.f, 700, 2.f, UI_K3, QD_CENTER);
        if (kind)
            ui_T(TL_MSG[msg & 1], 120.f, 150.f, 20.f, 700, 1.f, msg ? UI_K4 : UI_K2, QD_CENTER);
        return;
    }
    qd_fill_rect(0.f, 30.f, 240.f, 210.f, UI_BG);
    for (k = 0; k < QD_TS_N; k++) {
        const int on = k == scope;
        const float y = 62.f + (float)k * 46.f;
        ui_tools_scope_icon(k, 52.f, y - 6.f, on ? UI_K3 : UI_G2, 1.8f);
        ui_T(TL_SCOPE[k], 96.f, y + 2.f, on ? 26.f : 16.f, on ? 700 : 500, 0.f, on ? UI_K3 : UI_G2, QD_LEFT);
    }
}
