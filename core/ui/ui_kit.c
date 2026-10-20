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
/* P1.3v (PLAN §9a's clarity rules): a label is 12 px, 600 (was 8 px) */
void ui_lab(const char *s, float x, float y, qd_rgb col, int align) { ui_T(s, x, y, 12.f, 600, 0.5f, col, align); }

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
    ui_RR(x, y, 16.f, 16.f, 2.f, col, 1.2f, QD_NONE);
    ui_T(ui_itoa(s, n), x + 8.f, y + 12.5f, 12.f, 600, 0.f, col, QD_CENTER);
}

/* head(c, left, right, { n, lcol, rcol }) */
void ui_head(const char *left, const char *right, int n, qd_rgb lcol, qd_rgb rcol)
{
    float x = 8.f;
    if (n) {
        ui_box(8.f, 5.f, n, UI_K3);
        x = 29.f;
    }
    ui_T(left, x, 18.f, 12.f, 600, 0.8f, lcol, QD_LEFT);   /* (P1.3v: 12 px, 600) */
    if (right)
        ui_T(right, 232.f, 18.f, 12.f, 500, 0.8f, rcol, QD_RIGHT);
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
                                              "FUNC", "SOUND", "P-LOCK"};
/* a colour a mode, all bright on black and none of them red (red is the playhead); P-LOCK (P1.3c) takes LOCKS' sand:
 * the padlock's colour everywhere (P1.3d: LOCKS is gone, B10 reserved; its slot keeps the sand) */
const qd_rgb UI_MODE_COL[UI_NFIELD] = {0x3f7dffu, 0x1fd47eu, 0x36d6e7u, 0xa879ffu, 0xff6a2cu,
                                       0xffd23fu, 0xff52c0u, 0xf1f0eau, 0x9fe6ffu, 0xb5e853u, 0xd8c39au};   /* (P1.3w:
                                                         * FUNC ice blue at B9, SOUND's lime at B10) */

/* headTrack(c, left, n, name, lcol): a held step: STEP 5 · FIELD left, [n] the track right */
void ui_head_track(const char *left, int n, const char *name, qd_rgb lcol)
{
    const float w = ui_TW(name, 12.f, 500, 0.8f);
    ui_T(left, 8.f, 18.f, 12.f, 600, 0.8f, lcol, QD_LEFT);
    ui_T(name, 232.f, 18.f, 12.f, 500, 0.8f, UI_G1, QD_RIGHT);
    ui_box(232.f - w - 22.f, 5.f, n, UI_G1);
}

int ui_fn_icon(int f)
{
    static const int IC[7] = {UI_IC_NONE, UI_IC_RESET, UI_IC_LEFT, UI_IC_RIGHT, UI_IC_FLIP, UI_IC_DICE, UI_IC_DICE_FN};
    return f >= 7 ? UI_IC_JUMP : f > 0 ? IC[f] : UI_IC_NONE;
}

/* the strip's keys: 18 px wide, 2 apart in a group, the groups 3 | 2 | 3 | 2 six apart, B11 eight further */
float ui_strip_x(int k)
{
    static const float X[11] = {3.f, 23.f, 43.f, 67.f, 87.f, 111.f, 131.f, 151.f, 175.f, 195.f, 221.f};
    return X[k < 0 ? 0 : k > 10 ? 10 : k];
}

/* ---------------------------------------------------------------- P1.3v: icons and the visual key map */
static void ln(float x0, float y0, float x1, float y1, qd_rgb c, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, c, lw, 0, QD_NONE);
}

/* a note value's glyph: 0 1/1 (a hollow head), 1 1/2 (hollow, a stem), 2 1/4 (filled, a stem), 3.. 1/8 .. (flags) */
static void note_glyph(int v, float cx, float cy, float s, qd_rgb c)
{
    const float hx = cx - 1.5f * s, hy = cy + 4.f * s;
    int f;
    qd_ellipse(hx, hy, 2.6f * s, 1.9f * s, c, 1.1f, v >= 2 ? c : QD_NONE);
    if (v >= 1)
        ln(hx + 2.4f * s, hy - 0.5f * s, hx + 2.4f * s, hy - 9.f * s, c, 1.1f);
    for (f = 0; f < v - 2 && f < 4; f++)
        ln(hx + 2.4f * s, hy - 9.f * s + (float)f * 2.4f * s, hx + 5.2f * s, hy - 6.5f * s + (float)f * 2.4f * s, c,
           1.1f);
}

void ui_icon(int id, float cx, float cy, float s, qd_rgb c)
{
    switch (id) {
    case UI_IC_CLEAR:                                   /* a cross */
        ln(cx - 4.f * s, cy - 4.f * s, cx + 4.f * s, cy + 4.f * s, c, 1.4f);
        ln(cx + 4.f * s, cy - 4.f * s, cx - 4.f * s, cy + 4.f * s, c, 1.4f);
        break;
    case UI_IC_COPY:                                    /* two cards */
        ui_RR(cx - 5.f * s, cy - 5.f * s, 7.f * s, 8.f * s, 1.5f, c, 1.1f, QD_NONE);
        ui_RR(cx - 2.f * s, cy - 2.f * s, 7.f * s, 8.f * s, 1.5f, c, 1.1f, UI_BG);
        break;
    case UI_IC_PASTE: {                                 /* an arrow down into a card */
        const float a[6] = {cx - 3.f * s, cy - 2.f * s, cx, cy + 1.f * s, cx + 3.f * s, cy - 2.f * s};
        ui_RR(cx - 5.f * s, cy - 1.f * s, 10.f * s, 6.f * s, 1.5f, c, 1.1f, QD_NONE);
        ln(cx, cy - 7.f * s, cx, cy + 1.f * s, c, 1.3f);
        ui_P(a, 3, c, 1.3f, 0, QD_NONE);
        break;
    }
    case UI_IC_UNDO: {                                  /* a hook arrow back */
        const float a[6] = {cx - 2.f * s, cy - 6.f * s, cx - 5.f * s, cy - 3.f * s, cx - 2.f * s, cy};
        qd_arc(cx, cy + 0.5f * s, 4.f * s, -UI_PI / 2.f, UI_PI / 2.f, c, 1.3f);
        ln(cx - 5.f * s, cy - 3.5f * s, cx, cy - 3.5f * s, c, 1.3f);
        ln(cx - 3.f * s, cy + 4.5f * s, cx, cy + 4.5f * s, c, 1.3f);
        ui_P(a, 3, c, 1.3f, 0, QD_NONE);
        break;
    }
    case UI_IC_CUT:                                     /* scissors */
        ui_O(cx - 3.f * s, cy + 4.f * s, 1.8f * s, c, 1.1f, QD_NONE);
        ui_O(cx + 3.f * s, cy + 4.f * s, 1.8f * s, c, 1.1f, QD_NONE);
        ln(cx - 2.f * s, cy + 2.5f * s, cx + 3.f * s, cy - 6.f * s, c, 1.2f);
        ln(cx + 2.f * s, cy + 2.5f * s, cx - 3.f * s, cy - 6.f * s, c, 1.2f);
        break;
    case UI_IC_BACK: {                                  /* an arrow left */
        const float a[6] = {cx - 1.f * s, cy - 4.f * s, cx - 5.f * s, cy, cx - 1.f * s, cy + 4.f * s};
        ln(cx - 5.f * s, cy, cx + 5.f * s, cy, c, 1.4f);
        ui_P(a, 3, c, 1.4f, 0, QD_NONE);
        break;
    }
    case UI_IC_PLAY: {
        const float a[6] = {cx - 3.f * s, cy - 5.f * s, cx + 4.f * s, cy, cx - 3.f * s, cy + 5.f * s};
        ui_P(a, 3, c, 1.f, 1, c);
        break;
    }
    case UI_IC_STOP:
        ui_RR(cx - 4.f * s, cy - 4.f * s, 8.f * s, 8.f * s, 1.f, QD_NONE, 1.f, c);
        break;
    case UI_IC_DOT:
        ui_O(cx, cy, 2.2f * s, QD_NONE, 1.f, c);
        break;
    case UI_IC_TRI:                                     /* a triplet: three heads under a bracket */
        ln(cx - 6.f * s, cy - 2.f * s, cx - 6.f * s, cy - 5.f * s, c, 1.f);
        ln(cx - 6.f * s, cy - 5.f * s, cx + 6.f * s, cy - 5.f * s, c, 1.f);
        ln(cx + 6.f * s, cy - 5.f * s, cx + 6.f * s, cy - 2.f * s, c, 1.f);
        ui_O(cx - 4.f * s, cy + 3.f * s, 1.6f * s, QD_NONE, 1.f, c);
        ui_O(cx, cy + 3.f * s, 1.6f * s, QD_NONE, 1.f, c);
        ui_O(cx + 4.f * s, cy + 3.f * s, 1.6f * s, QD_NONE, 1.f, c);
        break;
    case UI_IC_RESET:                                   /* a circular arrow */
        qd_arc(cx, cy, 4.5f * s, -UI_PI * 0.2f, UI_PI * 1.5f, c, 1.3f);
        ln(cx + 4.5f * s, cy - 1.5f * s, cx + 4.5f * s, cy - 5.f * s, c, 1.3f);
        ln(cx + 4.5f * s, cy - 1.5f * s, cx + 1.5f * s, cy - 1.5f * s, c, 1.3f);
        break;
    case UI_IC_SCALE: {                                 /* a staircase */
        const float a[10] = {cx - 6.f * s, cy + 5.f * s, cx - 2.f * s, cy + 5.f * s, cx - 2.f * s, cy,
                             cx + 2.f * s, cy, cx + 2.f * s, cy - 5.f * s};
        ui_P(a, 5, c, 1.3f, 0, QD_NONE);
        ln(cx + 2.f * s, cy - 5.f * s, cx + 6.f * s, cy - 5.f * s, c, 1.3f);
        break;
    }
    case UI_IC_SWING:                                   /* a long and a short beat */
        ln(cx - 6.f * s, cy + 5.f * s, cx - 6.f * s, cy - 5.f * s, c, 1.4f);
        ln(cx + 2.f * s, cy + 5.f * s, cx + 2.f * s, cy - 5.f * s, c, 1.4f);
        ln(cx + 6.f * s, cy + 5.f * s, cx + 6.f * s, cy - 5.f * s, c, 1.4f);
        ln(cx - 6.f * s, cy + 5.f * s, cx + 6.f * s, cy + 5.f * s, c, 1.f);
        break;
    case UI_IC_REC:
        ui_O(cx, cy, 4.5f * s, QD_NONE, 1.f, c);
        break;
    case UI_IC_NOW: {                                   /* a lightning bolt */
        const float a[12] = {cx + 2.f * s, cy - 6.f * s, cx - 4.f * s, cy + 1.f * s, cx, cy + 1.f * s,
                             cx - 2.f * s, cy + 6.f * s, cx + 4.f * s, cy - 1.f * s, cx, cy - 1.f * s};
        ui_P(a, 6, c, 1.f, 1, c);
        break;
    }
    case UI_IC_RAMP:                                    /* rising ticks */
        ln(cx - 5.f * s, cy + 5.f * s, cx - 5.f * s, cy + 2.f * s, c, 1.6f);
        ln(cx - 1.7f * s, cy + 5.f * s, cx - 1.7f * s, cy - 0.5f * s, c, 1.6f);
        ln(cx + 1.7f * s, cy + 5.f * s, cx + 1.7f * s, cy - 3.f * s, c, 1.6f);
        ln(cx + 5.f * s, cy + 5.f * s, cx + 5.f * s, cy - 5.5f * s, c, 1.6f);
        break;
    case UI_IC_STRUM:                                   /* dots up a diagonal */
        ui_O(cx - 4.f * s, cy + 4.f * s, 1.5f * s, QD_NONE, 1.f, c);
        ui_O(cx, cy, 1.5f * s, QD_NONE, 1.f, c);
        ui_O(cx + 4.f * s, cy - 4.f * s, 1.5f * s, QD_NONE, 1.f, c);
        break;
    case UI_IC_KEY:                                     /* a white key: a tall rounded cell */
        ui_RR(cx - 3.5f * s, cy - 7.f * s, 7.f * s, 14.f * s, 1.5f, c, 1.1f, QD_NONE);
        break;
    case UI_IC_LEFT:                                    /* P1.3w: REVERSE: an arrow left */
    case UI_IC_RIGHT: {                                 /* FORWARD: an arrow right */
        const float k = id == UI_IC_LEFT ? -1.f : 1.f;
        const float a[6] = {cx + k * 1.f * s, cy - 4.f * s, cx + k * 5.f * s, cy, cx + k * 1.f * s, cy + 4.f * s};
        ln(cx - 5.f * s, cy, cx + 5.f * s, cy, c, 1.4f);
        ui_P(a, 3, c, 1.4f, 0, QD_NONE);
        break;
    }
    case UI_IC_FLIP: {                                  /* FLIP: an arrow each way, one over the other */
        const float a[6] = {cx + 2.f * s, cy - 6.f * s, cx + 5.f * s, cy - 3.f * s, cx + 2.f * s, cy};
        const float b[6] = {cx - 2.f * s, cy, cx - 5.f * s, cy + 3.f * s, cx - 2.f * s, cy + 6.f * s};
        ln(cx - 5.f * s, cy - 3.f * s, cx + 5.f * s, cy - 3.f * s, c, 1.3f);
        ln(cx - 5.f * s, cy + 3.f * s, cx + 5.f * s, cy + 3.f * s, c, 1.3f);
        ui_P(a, 3, c, 1.3f, 0, QD_NONE);
        ui_P(b, 3, c, 1.3f, 0, QD_NONE);
        break;
    }
    case UI_IC_DICE:                                    /* RANDOM JUMP: a die (three pips) */
    case UI_IC_DICE_FN:                                 /* RANDOM FUNCTION: a die (four pips) */
        ui_RR(cx - 5.5f * s, cy - 5.5f * s, 11.f * s, 11.f * s, 2.5f * s, c, 1.2f, QD_NONE);
        if (id == UI_IC_DICE) {
            ui_O(cx - 2.6f * s, cy - 2.6f * s, 1.1f * s, QD_NONE, 1.f, c);
            ui_O(cx, cy, 1.1f * s, QD_NONE, 1.f, c);
            ui_O(cx + 2.6f * s, cy + 2.6f * s, 1.1f * s, QD_NONE, 1.f, c);
        } else {
            ui_O(cx - 2.5f * s, cy - 2.5f * s, 1.1f * s, QD_NONE, 1.f, c);
            ui_O(cx + 2.5f * s, cy - 2.5f * s, 1.1f * s, QD_NONE, 1.f, c);
            ui_O(cx - 2.5f * s, cy + 2.5f * s, 1.1f * s, QD_NONE, 1.f, c);
            ui_O(cx + 2.5f * s, cy + 2.5f * s, 1.1f * s, QD_NONE, 1.f, c);
        }
        break;
    case UI_IC_JUMP: {                                  /* JUMP n: an arc over, landing */
        const float a[6] = {cx + 2.f * s, cy + 1.f * s, cx + 5.f * s, cy + 4.f * s, cx + 7.f * s, cy};
        qd_path_begin();
        qd_move_to(cx - 6.f * s, cy + 4.f * s);
        qd_quad_to(cx - 1.f * s, cy - 9.f * s, cx + 5.f * s, cy + 4.f * s);
        qd_path_stroke(c, 1.3f);
        ui_P(a, 3, c, 1.3f, 0, QD_NONE);
        break;
    }
    default:
        if (id >= UI_IC_NOTE && id < UI_IC_NOTE + 7)
            note_glyph(id - UI_IC_NOTE, cx, cy, s, c);
        break;
    }
}

/* the eleven black keys where they are on the panel, each with an icon or a number for what it does in this layer:
 * lit filled in its colour (the icon black), dim dark, the others outlined in their colour; no text but numbers */
void ui_keymap(const ui_kkey_t k[11], float y, float h)
{
    int i;
    char n[12];
    for (i = 0; i < 11; i++) {
        const float x = ui_strip_x(i), w = i == 10 ? 16.f : 18.f, cx = x + w / 2.f, cy = y + h / 2.f;
        const qd_rgb c = k[i].col ? k[i].col : UI_K3;
        const qd_rgb oc = k[i].lit ? QD_NONE : k[i].dim ? UI_G3 : k[i].icon == UI_IC_NONE && !k[i].num ? UI_G3 : c;
        const qd_rgb ic = k[i].lit ? UI_BG : k[i].dim ? UI_G3 : c;
        if (k[i].icon == UI_IC_NONE && !k[i].num && !k[i].lit) {   /* nothing here: a dark speck */
            ui_O(cx, cy, 1.f, QD_NONE, 1.f, UI_G3);
            continue;
        }
        ui_RR(x, y, w, h, 3.f, oc, 1.2f, k[i].lit ? c : QD_NONE);
        if (k[i].num)
            ui_T(ui_itoa(n, k[i].num), cx, cy + 4.5f, 12.f, 600, 0.f, ic, QD_CENTER);
        else if (k[i].icon != UI_IC_NONE)
            ui_icon(k[i].icon, cx, cy, 1.f, ic);
        if (k[i].mark)                                  /* a line under it (the track's choice) */
            qd_line(x + 3.f, y + h + 3.f, x + w - 3.f, y + h + 3.f, UI_G1, 1.5f);
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
