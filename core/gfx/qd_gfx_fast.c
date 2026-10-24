/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* qd_gfx on the device's own scanline primitives (Phase 2: the owner's "vector first, then fast integer primitives
 * where the unit's numbers say"; the unit drew the idle screen in 660 ms with plutovg). The whole of qd_gfx.h, drawn
 * straight into the strip's RGB565 rows: no whole-screen buffer, no heap, no 64-bit arithmetic, no libm in the pixel
 * loops (the square root, floor and ceiling are its own).
 *
 *   - Every shape is cut to the strip (and the clip) before any work: its box under the transform first (a shape that
 *     misses the strip costs a few multiplies), then only the rows of the strip are rasterised.
 *   - A path is flattened as it is built, in the screen's pixels (curves and arcs to 0.15 px), into a fixed point
 *     array (QF_MAXPT).
 *   - Fills: exact area coverage, a row at a time (signed area accumulated along the row's cells, then summed across
 *     it: the non-zero rule as |winding| clamped to 1). A rectangle on the screen's axes and a circle need no path.
 *   - Strokes: always round caps and joins (the screens use nothing else), so a stroke is the union of its pieces:
 *     capsules (a segment and its round ends), and for circles, arcs and rounded rectangles under a plain transform
 *     analytic rings and straight sides. A pixel's coverage is its centre's distance to the piece, box-filtered across
 *     the pen's edge (the exact area of a straight edge; a small ring is sampled four times, for its curve); the union
 *     is the largest coverage, kept in a strip-sized buffer and blended once (a translucent glow is never blended twice
 *     where its pieces overlap). Dashes cut the segments.
 *   - Text: Inter Tight's 4-bit atlases (core/gfx/qd_fast_font.h, tools/gen_fast_font.c) at the sizes the screens
 *     draw most, each glyph moved by its pen's fraction of a pixel (1/16 px, bilinear); any other size, a squeezed or
 *     turned text, and the text masks are the glyphs' own outlines, filled. The widths are plutovg's float sums of the
 *     same advances, to the bit.
 *   - Alpha blending in RGB565 (red and blue in one word, green in another, 64 alpha steps).
 *   - P2.4's frame hash (qd_hash_begin / end): every drawing call hashes its arguments and the state instead.
 * Floats are single precision (the unit's FPU); the pixel loops are integer where they can be.
 *
 * Not drawn as plutovg does (none of it used by the screens today): butt and square caps, miter and bevel joins
 * (drawn round); a clip under a turn (its box); qd_path_clip (the path's box); layers (as plutovg's device build:
 * qd_layer_begin returns 0 and the caller draws directly); text outside the charset (ASCII and . x +- deg - -- ... ->,
 * qd_fast_font.h) is .notdef. A non-uniform scale strokes with the mean width. */
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "qd_gfx_be.h"                         /* (P2.6: the drawing calls as qdb_*: qd_gfx_rec.c in front) */
#include "qd_gfx.h"
#include "../qd_config.h"
#include "qd_fast_font.h"
#ifdef QD_DEVICE
#include "qd_arena.c"                           /* (the borrowers' arena, pattern.h qd_tmp_borrow: plutovg's file
                                                 * built it; this backend draws without it) */
#endif

#ifndef QF_MAXPT
#define QF_MAXPT 512                            /* a path's flattened points (device px): the screens' largest
                                                 * is 288 (render_frames); the charset at 120 px, filled a part at
                                                 * a time, 369 (test_gfx_fast) */
#endif
#ifndef QF_USE_ATLAS
#define QF_USE_ATLAS 1                          /* (0: every text from its outlines; the bench's comparison) */
#endif
#define QF_MAXCT 64                             /* .. its contours */
#define QF_TOL 0.15f                            /* flattening tolerance, px */
#define QF_PI 3.14159265f
#if defined(__GNUC__)                           /* (the pixel helpers inlined at -Os too: the unit's build) */
#define QF_INL static inline __attribute__((always_inline))
#else
#define QF_INL static inline
#endif
#define QF_TAU 6.28318531f

/* ---------------------------------------------------------------- the state */
typedef struct { float a, b, c, d, e, f; } qf_m;   /* x' = a x + c y + e, y' = b x + d y + f (plutovg's) */
typedef struct {
    qf_m m;
    float alpha;
    float cx0, cy0, cx1, cy1;                   /* the clip, screen px */
    float lw;
    float dash[8];
    int ndash;
} qf_st;
static qf_st S, STK[24];
static int depth;

/* the target: the strip (rows y0 .. y0 + sh of the screen) or a text mask */
static uint16_t qf_px[QD_W * QD_STRIP_H] __attribute__((aligned(4)));
static int y0s, sh = QD_STRIP_H, inited;
static uint8_t *mk;                             /* (a text mask's bytes: mk_w wide, its rows from mk_r0) */
static int mk_w, mk_r0, mk_rows;
static int t_r0, t_r1, t_c0, t_c1;              /* the target's rows and columns [r0, r1), [c0, c1) */

/* a paint's colour and alpha, its rows and columns after the clip, the clip's edge coverage (0..256) */
static uint16_t c565;
static uint32_t c_rb, c_g;                      /* (the colour's red and blue, its green, in place for mix) */
static unsigned cr8, cg8, cb8, ga;              /* (ga: the global alpha, 0..255) */
static int p_r0, p_r1, p_c0, p_c1;
static int p_fl, p_fr;                          /* the clip's first and last column's coverage */
static int p_yt, p_yb;                          /* the clip's partial top and bottom rows (-9: none) .. */
static unsigned p_at, p_ab;                     /* .. their alpha (the global alpha times their coverage) */

/* the coverage buffers: a row's for fills (float cells) and text; the strip's for strokes (bytes) and its span a row */
static float acc[QD_W + 2];
#define QF_ROW (QD_W + 16)                      /* (a coverage row: 4-byte aligned, a word past its end) */
static uint32_t row8w[QF_ROW / 4], scovw[QD_STRIP_H][QF_ROW / 4];
#define row8 ((uint8_t *)row8w)
#define scov(ri) ((uint8_t *)scovw[ri])
static int16_t slo[QD_STRIP_H], shi[QD_STRIP_H];

/* the path: flattened, in screen px */
static float px_[QF_MAXPT], py_[QF_MAXPT];
static int16_t ct0[QF_MAXCT + 1];              /* a contour's first point (ct0[nct]: the end) */
static uint8_t ctc[QF_MAXCT];                   /* .. closed */
static int npt, nct, open_ct;                   /* (open_ct: a contour is being built) */
static float bx0, by0, bx1, by1;                /* the path's box */
static unsigned qf_overflow;                    /* points that did not fit (the tests read it) */
static int qf_peak_pt, qf_peak_ct;              /* .. the most a path has held */
unsigned qd_fast_overflows(void) { return qf_overflow; }
void qd_fast_peak(int *points, int *contours)
{
    if (points)
        *points = qf_peak_pt;
    if (contours)
        *contours = qf_peak_ct;
}

/* 8-bit channels to 5 / 6 bits (plutovg's rounding, qd_gfx_plutovg.c) and back */
static uint8_t L5[256], L6[256], E5[32], E6[64];

QF_INL float qf_abs(float x) { return x < 0.f ? -x : x; }
/* floor and ceiling to int without the library's (screen coordinates: clamped far outside it) */
QF_INL int qf_floor(float x)
{
    int i;
    x = x < -1e6f ? -1e6f : x > 1e6f ? 1e6f : x;
    i = (int)x;
    return i - (x < (float)i);
}
QF_INL int qf_ceil(float x)
{
    int i;
    x = x < -1e6f ? -1e6f : x > 1e6f ? 1e6f : x;
    i = (int)x;
    return i + (x > (float)i);
}
/* a square root without the library's (the unit's libm has no instruction for it): the reciprocal root's estimate,
 * two Newton steps (relative error ~5e-6) */
QF_INL float qf_sqrt(float x)
{
    union {
        float f;
        uint32_t u;
    } v;
    float y;
    if (x <= 0.f)
        return 0.f;
    v.f = x;
    v.u = 0x5f3759dfu - (v.u >> 1);
    y = v.f;
    y = y * (1.5f - 0.5f * x * y * y);
    y = y * (1.5f - 0.5f * x * y * y);
    return x * y;
}

static void qf_tables(void)
{
    unsigned i;
    for (i = 0; i < 256; i++)
        L5[i] = (uint8_t)((i * 31u + 127u) / 255u), L6[i] = (uint8_t)((i * 63u + 127u) / 255u);
    for (i = 0; i < 32; i++)
        E5[i] = (uint8_t)((i * 255u + 15u) / 31u);
    for (i = 0; i < 64; i++)
        E6[i] = (uint8_t)((i * 255u + 31u) / 63u);
}

int qd_init(void)
{
    if (!inited) {
        qf_tables();
        inited = 1;
    }
    return 1;
}
void qd_shutdown(void) { depth = 0; }

static void st_reset(void)
{
    S.m.a = 1.f, S.m.b = 0.f, S.m.c = 0.f, S.m.d = 1.f, S.m.e = 0.f, S.m.f = 0.f;
    S.alpha = 1.f;
    S.cx0 = 0.f, S.cy0 = 0.f, S.cx1 = (float)QD_W, S.cy1 = (float)QD_H;
    S.lw = 1.f;
    S.ndash = 0;
    depth = 0;
    npt = nct = open_ct = 0;
}

int qd_strip_rows(void) { return sh; }
void qd_set_strip_rows(int h) { sh = h < 1 ? 1 : h > QD_STRIP_H ? QD_STRIP_H : h; }

static void tgt_strip(void)
{
    mk = 0;
    t_r0 = y0s, t_r1 = y0s + sh > QD_H ? QD_H : y0s + sh;
    t_c0 = 0, t_c1 = QD_W;
}

/* ---------------------------------------------------------------- P2.4: a frame hashed, nothing drawn */
/* qd_hash_begin .. qd_hash_end: every call that would draw hashes what it would draw (its arguments, the transform
 * and the alpha at the time, the colours and widths, the clip and dash as they are set) and draws nothing: the same
 * hash, the same pixels (app.h qd_app_frame_changed) */
static int hashing;
static uint32_t hsh;
static void hb(const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    size_t i;
    for (i = 0; i < n; i++) {
        hsh ^= b[i];
        hsh *= 0x01000193u;
    }
}
static void hu(uint32_t u) { hb(&u, sizeof u); }
static void hf(float f) { hb(&f, sizeof f); }
static void hstate(void)
{
    hb(&S.m, sizeof S.m);
    hf(S.alpha);
}
static void hcall(uint32_t tag, const float *v, int n, qd_rgb c0, qd_rgb c1)   /* a call: its tag, numbers, colours */
{
    hu(tag);
    hb(v, (size_t)n * sizeof *v);
    hu(c0), hu(c1);
    hstate();
}

void qd_begin(void) { qd_begin_strip(0); }
void qd_begin_strip(int y0)
{
    int r;
    qd_init();
    y0s = y0;
    st_reset();
    tgt_strip();
    memset(qf_px, 0, sizeof(uint16_t) * (size_t)QD_W * (size_t)sh);   /* black */
    for (r = 0; r < QD_STRIP_H; r++)
        slo[r] = QD_W, shi[r] = -1;
}

void qd_end(uint16_t *lcd_be) { qd_end_strip(lcd_be); }
static void strip_out(uint16_t *to)
{
    const int n = QD_W * (t_r1 - t_r0);
    int i = 0;
    if (!((uintptr_t)to & 3u)) {                        /* (P2.6c: two pixels a word; QD_W even, qf_px aligned) */
        const uint32_t *s = (const uint32_t *)(const void *)qf_px;
        uint32_t *d = (uint32_t *)(void *)to;
        for (; i + 1 < n; i += 2) {
            const uint32_t w = s[i >> 1];
            d[i >> 1] = ((w >> 8) & 0x00FF00FFu) | ((w << 8) & 0xFF00FF00u);
        }
    }
    for (; i < n; i++) {
        const uint16_t c = qf_px[i];
        to[i] = (uint16_t)((c >> 8) | (c << 8));
    }
}
void qd_end_strip(uint16_t *lcd_be)
{
    if (lcd_be)
        strip_out(lcd_be + (size_t)y0s * QD_W);
}
void qd_hash_begin(void)
{
    qd_begin_strip(0);
    hashing = 1;
    hsh = 2166136261u;
}
uint32_t qd_hash_end(void)
{
    hashing = 0;
    return hsh;
}
const uint16_t *qd_end_strip_rows(uint16_t *rows_be)
{
    uint16_t *to = rows_be ? rows_be : qf_px;           /* (none: in place) */
    strip_out(to);
    return to;
}

/* ---------------------------------------------------------------- the state calls */
void qd_save(void)
{
    if (depth < (int)(sizeof STK / sizeof STK[0]))
        STK[depth] = S;
    depth++;
}
void qd_restore(void)
{
    if (depth <= 0)
        return;
    depth--;
    if (depth < (int)(sizeof STK / sizeof STK[0]))
        S = STK[depth];
}
static void mul(qf_m *r, const qf_m *l, const qf_m *rt)   /* plutovg_matrix_multiply: l first, then rt */
{
    qf_m o;
    o.a = l->a * rt->a + l->b * rt->c;
    o.b = l->a * rt->b + l->b * rt->d;
    o.c = l->c * rt->a + l->d * rt->c;
    o.d = l->c * rt->b + l->d * rt->d;
    o.e = l->e * rt->a + l->f * rt->c + rt->e;
    o.f = l->e * rt->b + l->f * rt->d + rt->f;
    *r = o;
}
void qd_translate(float x, float y)
{
    const qf_m t = {1.f, 0.f, 0.f, 1.f, x, y};
    mul(&S.m, &t, &S.m);
}
void qd_scale(float sx, float sy)
{
    const qf_m t = {sx, 0.f, 0.f, sy, 0.f, 0.f};
    mul(&S.m, &t, &S.m);
}
void qd_rotate(float a)
{
    const float c = cosf(a), s = sinf(a);
    const qf_m t = {c, s, -s, c, 0.f, 0.f};
    mul(&S.m, &t, &S.m);
}
void qd_alpha(float a) { S.alpha = a < 0.f ? 0.f : a > 1.f ? 1.f : a; }
float qd_get_alpha(void) { return S.alpha; }
void qd_line_width(float w) { S.lw = w; }
void qd_line_cap(int cap) { (void)cap; }          /* (round only) */
void qd_line_join(int join) { (void)join; }
void qd_dash(const float *on_off, int n)
{
    int i;
    float sum = 0.f;
    if (hashing) {
        hu(0x44415348u), hu((uint32_t)n);
        if (n > 0 && on_off)
            hb(on_off, (size_t)n * sizeof *on_off);
    }
    S.ndash = 0;
    if (!on_off || n <= 0)
        return;
    n = n > 8 ? 8 : n;
    for (i = 0; i < n; i++)
        S.dash[i] = on_off[i] < 0.f ? 0.f : on_off[i], sum += S.dash[i];
    if (sum <= 0.f)
        return;
    if (n & 1) {                                        /* (an odd pattern repeats twice, as the canvas's) */
        for (i = 0; i < n && n + i < 8; i++)
            S.dash[n + i] = S.dash[i];
        n = 2 * n > 8 ? 8 : 2 * n;
    }
    S.ndash = n;
}

static void map(const qf_m *m, float x, float y, float *X, float *Y)
{
    *X = x * m->a + y * m->c + m->e;
    *Y = x * m->b + y * m->d + m->f;
}
static float m_scale(const qf_m *m) { return qf_sqrt(qf_abs(m->a * m->d - m->b * m->c)); }

/* a user box's screen box (its four corners) */
static void box_dev(const qf_m *m, float x, float y, float w, float h, float *X0, float *Y0, float *X1, float *Y1)
{
    float xs[4], ys[4];
    int i;
    map(m, x, y, &xs[0], &ys[0]);
    map(m, x + w, y, &xs[1], &ys[1]);
    map(m, x, y + h, &xs[2], &ys[2]);
    map(m, x + w, y + h, &xs[3], &ys[3]);
    *X0 = *X1 = xs[0], *Y0 = *Y1 = ys[0];
    for (i = 1; i < 4; i++) {
        *X0 = xs[i] < *X0 ? xs[i] : *X0, *X1 = xs[i] > *X1 ? xs[i] : *X1;
        *Y0 = ys[i] < *Y0 ? ys[i] : *Y0, *Y1 = ys[i] > *Y1 ? ys[i] : *Y1;
    }
}

void qd_clip_rect(float x, float y, float w, float h)
{
    if (hashing) {
        const float v[4] = {x, y, w, h};
        hcall(0x434C5052u, v, 4, 0, 0);
        return;
    }
    float X0, Y0, X1, Y1;
    box_dev(&S.m, x, y, w, h, &X0, &Y0, &X1, &Y1);
    S.cx0 = X0 > S.cx0 ? X0 : S.cx0, S.cy0 = Y0 > S.cy0 ? Y0 : S.cy0;
    S.cx1 = X1 < S.cx1 ? X1 : S.cx1, S.cy1 = Y1 < S.cy1 ? Y1 : S.cy1;
}

/* the screen box (x0, y0) .. (x1, y1) can touch the target inside the clip */
static int dev_visible(float x0, float y0, float x1, float y1)
{
    const float r0 = (float)t_r0 > S.cy0 ? (float)t_r0 : S.cy0, r1 = (float)t_r1 < S.cy1 ? (float)t_r1 : S.cy1;
    const float c0 = (float)t_c0 > S.cx0 ? (float)t_c0 : S.cx0, c1 = (float)t_c1 < S.cx1 ? (float)t_c1 : S.cx1;
    return y1 > r0 && y0 < r1 && x1 > c0 && x0 < c1 && r1 > r0 && c1 > c0;
}

int qd_box_visible(float x, float y, float w, float h, float lw)
{
    float X0, Y0, X1, Y1, pad;
    if (hashing)
        return 1;
    box_dev(&S.m, x, y, w, h, &X0, &Y0, &X1, &Y1);
    pad = (lw > 0.f ? 0.5f * lw * m_scale(&S.m) : 0.f) + 2.f;
    return dev_visible(X0 - pad, Y0 - pad, X1 + pad, Y1 + pad);
}

/* ---------------------------------------------------------------- the paint's setup and the row composer */
static void paint_setup(qd_rgb col, float y0, float y1)
{
    const unsigned r = (col >> 16) & 255u, g = (col >> 8) & 255u, b = col & 255u;
    float c0 = S.cx0 > (float)t_c0 ? S.cx0 : (float)t_c0, c1 = S.cx1 < (float)t_c1 ? S.cx1 : (float)t_c1;
    float r0 = S.cy0 > (float)t_r0 ? S.cy0 : (float)t_r0, r1 = S.cy1 < (float)t_r1 ? S.cy1 : (float)t_r1;
    int i0, i1;
    cr8 = r, cg8 = g, cb8 = b;
    c565 = (uint16_t)(L5[r] << 11 | L6[g] << 5 | L5[b]);
    c_rb = c565 & 0xF81Fu, c_g = c565 & 0x07E0u;
    ga = (unsigned)(S.alpha * 255.f + 0.5f);
    /* rows: the shape's, the target's, the clip's */
    r0 = y0 > r0 ? y0 : r0, r1 = y1 < r1 ? y1 : r1;
    p_r0 = qf_floor(r0), p_r1 = qf_ceil(r1);
    p_r0 = p_r0 < t_r0 ? t_r0 : p_r0, p_r1 = p_r1 > t_r1 ? t_r1 : p_r1;
    {   /* the clip's partial rows */
        const int yt = qf_floor(S.cy0), yb = qf_ceil(S.cy1) - 1;
        const float ft = ((float)(yt + 1) < S.cy1 ? (float)(yt + 1) : S.cy1) - S.cy0;
        const float fb = S.cy1 - ((float)yb > S.cy0 ? (float)yb : S.cy0);
        p_yt = ft < 0.999f ? yt : -9, p_yb = fb < 0.999f ? yb : -9;
        p_at = (unsigned)((float)ga * (ft > 0.f ? ft : 0.f) + 0.5f);
        p_ab = (unsigned)((float)ga * (fb > 0.f ? fb : 0.f) + 0.5f);
    }
    /* columns: the clip's whole ones and its two edge columns' coverage */
    if (c1 <= c0) {
        p_c0 = p_c1 = 0;
        return;
    }
    i0 = qf_floor(c0), i1 = qf_ceil(c1);
    p_c0 = i0 < t_c0 ? t_c0 : i0, p_c1 = i1 > t_c1 ? t_c1 : i1;
    p_fl = (int)(256.f * ((float)(i0 + 1) < c1 ? (float)(i0 + 1) - c0 : c1 - c0) + 0.5f);
    p_fr = i1 - 1 > i0 ? (int)(256.f * (c1 - (float)(i1 - 1)) + 0.5f) : p_fl;
}

/* src over d at a (1..254 of 255), in RGB565: red and blue in one word, green in another, the alpha in 64 steps (the
 * fields have the room: 5 + 6 bits each), rounded */
QF_INL uint16_t mix(uint16_t d, unsigned a)
{
    const uint32_t a6 = (a * 65u + 128u) >> 8, ia = 64u - a6;   /* (0..64) */
    const uint32_t rb = ((c_rb * a6 + (d & 0xF81Fu) * ia + 0x10020u) >> 6) & 0xF81Fu;
    const uint32_t g = ((c_g * a6 + (d & 0x07E0u) * ia + 0x400u) >> 6) & 0x07E0u;
    return (uint16_t)(rb | g);
}

/* row y's coverage cov[x0 .. x1] (0..255) blended in the paint's colour (or written to the text mask), the clip's
 * edges and the global alpha applied; cov is cleared */
static void put_row(int y, int x0, int x1, uint8_t *cov)
{
    unsigned ar;
    int x;
    if (y < p_r0 || y >= p_r1 || x1 < p_c0 || x0 >= p_c1) {
        if (x1 >= x0)
            memset(cov + x0, 0, (size_t)(x1 - x0 + 1));
        return;
    }
    if (x0 < p_c0) {
        memset(cov + x0, 0, (size_t)(p_c0 - x0));
        x0 = p_c0;
    }
    if (x1 >= p_c1) {
        memset(cov + p_c1, 0, (size_t)(x1 - p_c1 + 1));
        x1 = p_c1 - 1;
    }
    if (mk) {                                           /* a text mask: the coverage itself */
        uint8_t *m = mk + (size_t)(y - mk_r0) * (size_t)mk_w;
        for (x = x0; x <= x1; x++) {
            m[x] = cov[x] > m[x] ? cov[x] : m[x];
            cov[x] = 0;
        }
        return;
    }
    ar = y == p_yt ? p_at : y == p_yb ? p_ab : ga;   /* (the clip's partial rows) */
    if (p_fl < 256 && x0 == p_c0)                       /* (the clip's edge columns) */
        cov[x0] = (uint8_t)((cov[x0] * (unsigned)p_fl + 128u) >> 8);
    if (p_fr < 256 && x1 == p_c1 - 1)
        cov[x1] = (uint8_t)((cov[x1] * (unsigned)p_fr + 128u) >> 8);
    {
        uint16_t *p = qf_px + (size_t)(y - y0s) * QD_W;
        if (ar >= 255u) {
            for (x = x0; x <= x1; x++) {
                unsigned c = cov[x];
                if (!c) {
                    if (!(x & 3))                       /* (empty words: four pixels at a time) */
                        while (x + 3 <= x1 && !*(const uint32_t *)(const void *)(cov + x))
                            x += 4;
                    c = x <= x1 ? cov[x] : 0u;
                    if (!c)
                        continue;
                }
                cov[x] = 0;
                if (c >= 255u)
                    p[x] = c565;
                else
                    p[x] = mix(p[x], c);
            }
        } else if (ar) {
            for (x = x0; x <= x1; x++) {
                unsigned c = cov[x], a;
                if (!c) {
                    if (!(x & 3))
                        while (x + 3 <= x1 && !*(const uint32_t *)(const void *)(cov + x))
                            x += 4;
                    c = x <= x1 ? cov[x] : 0u;
                    if (!c)
                        continue;
                }
                cov[x] = 0;
                a = (c * ar + 128u) >> 8;               /* (c x ar / 255, near enough: a <= 254) */
                if (a)
                    p[x] = mix(p[x], a);
            }
        } else {
            memset(cov + x0, 0, (size_t)(x1 - x0 + 1));
        }
    }
}

/* ---------------------------------------------------------------- the path */
static void path_reset(void)
{
    npt = nct = open_ct = 0;
    bx0 = by0 = 1e30f, bx1 = by1 = -1e30f;
}
void qd_path_begin(void)
{
    if (hashing)
        hu(0x50424547u);
    path_reset();
}

static void add_dev(float X, float Y)                   /* a point onto the open contour */
{
    if (npt >= QF_MAXPT) {
        qf_overflow++;
        return;
    }
    px_[npt] = X, py_[npt] = Y, npt++;
    qf_peak_pt = npt > qf_peak_pt ? npt : qf_peak_pt;
    bx0 = X < bx0 ? X : bx0, bx1 = X > bx1 ? X : bx1;
    by0 = Y < by0 ? Y : by0, by1 = Y > by1 ? Y : by1;
}
static void move_dev(float X, float Y)
{
    if (nct >= QF_MAXCT || npt >= QF_MAXPT) {
        qf_overflow++;
        open_ct = 0;
        return;
    }
    ct0[nct] = (int16_t)npt;
    ctc[nct] = 0;
    nct++;
    qf_peak_ct = nct > qf_peak_ct ? nct : qf_peak_ct;
    ct0[nct] = (int16_t)npt;
    open_ct = 1;
    add_dev(X, Y);
    ct0[nct] = (int16_t)npt;
}
static void line_dev(float X, float Y)
{
    if (!open_ct) {
        move_dev(X, Y);
        return;
    }
    add_dev(X, Y);
    ct0[nct] = (int16_t)npt;
}
static int cur_dev(float *X, float *Y)                  /* the current point (screen) */
{
    if (!open_ct || npt == 0)
        return 0;
    *X = px_[npt - 1], *Y = py_[npt - 1];
    return 1;
}

void qd_move_to(float x, float y)
{
    float X, Y;
    if (hashing) {
        const float v[2] = {x, y};
        hcall(0x4D4F5645u, v, 2, 0, 0);
        return;
    }
    map(&S.m, x, y, &X, &Y);
    move_dev(X, Y);
}
void qd_line_to(float x, float y)
{
    float X, Y;
    if (hashing) {
        const float v[2] = {x, y};
        hu(0x4C494E45u), hb(v, sizeof v);
        return;
    }
    map(&S.m, x, y, &X, &Y);
    line_dev(X, Y);
}

static void quad_dev(float X0, float Y0, float X1, float Y1, float X2, float Y2)
{
    const float ddx = X0 - 2.f * X1 + X2, ddy = Y0 - 2.f * Y1 + Y2;
    const float dd = qf_sqrt(ddx * ddx + ddy * ddy);
    int n = qf_ceil(qf_sqrt(dd * (1.f / (4.f * QF_TOL)))), i;
    n = n < 1 ? 1 : n > 64 ? 64 : n;
    for (i = 1; i <= n; i++) {
        const float t = (float)i / (float)n, u = 1.f - t;
        line_dev(u * u * X0 + 2.f * u * t * X1 + t * t * X2, u * u * Y0 + 2.f * u * t * Y1 + t * t * Y2);
    }
}
static void cubic_dev(float X0, float Y0, float X1, float Y1, float X2, float Y2, float X3, float Y3)
{
    const float ax = X0 - 2.f * X1 + X2, ay = Y0 - 2.f * Y1 + Y2, bx = X1 - 2.f * X2 + X3, by = Y1 - 2.f * Y2 + Y3;
    const float m1 = ax * ax + ay * ay, m2 = bx * bx + by * by, mm = qf_sqrt(m1 > m2 ? m1 : m2);
    int n = qf_ceil(qf_sqrt(mm * (3.f / (4.f * QF_TOL)))), i;
    n = n < 1 ? 1 : n > 96 ? 96 : n;
    for (i = 1; i <= n; i++) {
        const float t = (float)i / (float)n, u = 1.f - t;
        const float k0 = u * u * u, k1 = 3.f * u * u * t, k2 = 3.f * u * t * t, k3 = t * t * t;
        line_dev(k0 * X0 + k1 * X1 + k2 * X2 + k3 * X3, k0 * Y0 + k1 * Y1 + k2 * Y2 + k3 * Y3);
    }
}

void qd_quad_to(float cx, float cy, float x, float y)
{
    float X0, Y0, X1, Y1, X2, Y2;
    if (hashing) {
        const float v[4] = {cx, cy, x, y};
        hu(0x51554144u), hb(v, sizeof v);
        return;
    }
    map(&S.m, cx, cy, &X1, &Y1);
    map(&S.m, x, y, &X2, &Y2);
    if (!cur_dev(&X0, &Y0)) {
        move_dev(X1, Y1);
        X0 = X1, Y0 = Y1;
    }
    quad_dev(X0, Y0, X1, Y1, X2, Y2);
}
void qd_cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y)
{
    float X0, Y0, X1, Y1, X2, Y2, X3, Y3;
    if (hashing) {
        const float v[6] = {c1x, c1y, c2x, c2y, x, y};
        hu(0x43554249u), hb(v, sizeof v);
        return;
    }
    map(&S.m, c1x, c1y, &X1, &Y1);
    map(&S.m, c2x, c2y, &X2, &Y2);
    map(&S.m, x, y, &X3, &Y3);
    if (!cur_dev(&X0, &Y0)) {
        move_dev(X1, Y1);
        X0 = X1, Y0 = Y1;
    }
    cubic_dev(X0, Y0, X1, Y1, X2, Y2, X3, Y3);
}

/* an ellipse's arc (the user's centre and radii) from a0 through da (radians, signed): its first point joined to the
 * open contour (join) or a new one */
static void arc_user(float cx, float cy, float rx, float ry, float a0, float da, int join)
{
    const qf_m *m = &S.m;
    const float sx = qf_sqrt(m->a * m->a + m->b * m->b), sy = qf_sqrt(m->c * m->c + m->d * m->d);
    const float rd = (rx * sx > ry * sy ? rx * sx : ry * sy);
    float step, c, s, cd, sd, X, Y;
    int n, i;
    if (rd > QF_TOL * 1.01f)                            /* (the sagitta's angle 2 acos(1 - tol / r), from below) */
        step = 2.f * qf_sqrt(2.f * QF_TOL / rd) * 0.98f;
    else
        step = QF_PI / 2.f;
    step = step < 0.02f ? 0.02f : step;
    n = qf_ceil(qf_abs(da) / step);
    n = n < 1 ? 1 : n > 512 ? 512 : n;
    c = cosf(a0), s = sinf(a0);
    cd = cosf(da / (float)n), sd = sinf(da / (float)n);
    map(m, cx + rx * c, cy + ry * s, &X, &Y);
    if (join && open_ct)
        line_dev(X, Y);
    else
        move_dev(X, Y);
    for (i = 1; i <= n; i++) {
        if (i == n) {
            c = cosf(a0 + da), s = sinf(a0 + da);
        } else {
            const float c2 = c * cd - s * sd;
            s = s * cd + c * sd, c = c2;
        }
        map(m, cx + rx * c, cy + ry * s, &X, &Y);
        line_dev(X, Y);
    }
}
/* plutovg_path_add_arc's sweep, clockwise: a0 -> a1, a whole turn at most, 0 nothing */
static float sweep_cw(float a0, float a1)
{
    float da = a1 - a0;
    if (qf_abs(da) > QF_TAU)
        da = QF_TAU;
    else if (da != 0.f && da < 0.f)
        da += QF_TAU;
    return da;
}
void qd_path_arc(float cx, float cy, float r, float a0, float a1)
{
    if (hashing) {
        const float v[5] = {cx, cy, r, a0, a1};
        hcall(0x50415243u, v, 5, 0, 0);
        return;
    }
    const float da = sweep_cw(a0, a1);
    if (da != 0.f)
        arc_user(cx, cy, r, r, a0, da, 1);
}
void qd_path_close(void)
{
    if (hashing) {
        hu(0x434C4F53u);
        return;
    }
    if (nct > 0 && open_ct) {
        float X = px_[ct0[nct - 1]], Y = py_[ct0[nct - 1]];
        ctc[nct - 1] = 1;
        open_ct = 0;
        /* (a line after a close starts again from the contour's start, as the canvas) */
        move_dev(X, Y);
        open_ct = 2;                                    /* (a pending start: dropped if nothing follows) */
    }
}

/* ---------------------------------------------------------------- fills: the row's cells */
static int a_lo, a_hi;
static void acc_x(float xa, float xb, float sdy)      /* an edge crossing the row from x xa to xb, its signed height */
{
    float d, ff;
    int i, i1;
    if (xa > xb) {
        const float t = xa;
        xa = xb, xb = t;
    }
    if (xb <= 0.f) {
        acc[0] += sdy;
        a_lo = 0, a_hi = a_hi < 1 ? 1 : a_hi;
        return;
    }
    if (xa >= (float)QD_W)
        return;
    d = xb - xa;
    if (xa < 0.f) {                                     /* (the part left of the screen: a full height at column 0) */
        const float k = -xa / d;
        acc[0] += sdy * k;
        sdy -= sdy * k;
        xa = 0.f;
        d = xb;
    }
    if (xb > (float)QD_W) {                             /* (the part right of it: nothing seen) */
        sdy *= ((float)QD_W - xa) / d;
        xb = (float)QD_W;
        d = xb - xa;
    }
    i = (int)xa;
    a_lo = i < a_lo ? i : a_lo;
    if (d < 1e-6f || (int)xb == i || ((float)(i + 1) >= xb)) {   /* one column */
        const float xm = 0.5f * (xa + xb) - (float)i;
        acc[i] += sdy * (1.f - xm);
        acc[i + 1] += sdy * xm;
        a_hi = i + 1 > a_hi ? i + 1 : a_hi;
        return;
    }
    i1 = qf_ceil(xb) - 1;
    ff = sdy / d;                                       /* (height per unit of x) */
    {
        float x = xa;
        int k;
        for (k = i; k <= i1; k++) {
            const float xe = (float)(k + 1) < xb ? (float)(k + 1) : xb;
            const float dy = ff * (xe - x), xm = 0.5f * (x + xe) - (float)k;
            acc[k] += dy * (1.f - xm);
            acc[k + 1] += dy * xm;
            x = xe;
        }
    }
    a_hi = i1 + 1 > a_hi ? i1 + 1 : a_hi;
}

static void acc_edge(float x0, float y0, float x1, float y1, float ry)
{
    float ya, yb, xa, xb, s;
    if (y0 == y1)
        return;
    if (y0 < y1) {
        if (y1 <= ry || y0 >= ry + 1.f)
            return;
        s = 1.f;
    } else {
        if (y0 <= ry || y1 >= ry + 1.f)
            return;
        s = -1.f;
    }
    {
        const float lo = y0 < y1 ? y0 : y1, hi = y0 < y1 ? y1 : y0, k = (x1 - x0) / (y1 - y0);
        ya = lo > ry ? lo : ry;
        yb = hi < ry + 1.f ? hi : ry + 1.f;
        xa = x0 + (ya - y0) * k;
        xb = x0 + (yb - y0) * k;
    }
    acc_x(xa, xb, s * (yb - ya));
}

/* the path filled (non-zero), rows p_r0 .. p_r1 */
static void fill_path(void)
{
    int y;
    if (npt < 3 || p_r1 <= p_r0)
        return;
    for (y = p_r0; y < p_r1; y++) {
        const float ry = (float)y;
        int k;
        if (by1 <= ry || by0 >= ry + 1.f)
            continue;
        a_lo = QD_W + 1, a_hi = -1;
        for (k = 0; k < nct; k++) {
            const int i0 = ct0[k], i1 = ct0[k + 1];
            int i;
            if (i1 - i0 < 2)
                continue;
            for (i = i0; i < i1; i++) {
                const int j = i + 1 < i1 ? i + 1 : i0;  /* (each contour closed for the fill) */
                const float ya = py_[i], yb = py_[j];
                if ((ya <= ry && yb <= ry) || (ya >= ry + 1.f && yb >= ry + 1.f))
                    continue;
                acc_edge(px_[i], ya, px_[j], yb, ry);
            }
        }
        if (a_hi < 0)
            continue;
        {   /* the cells summed across the row into coverage */
            float sum = 0.f;
            int x, xe = a_hi < QD_W ? a_hi : QD_W - 1;
            for (x = a_lo; x <= xe; x++) {
                float v;
                sum += acc[x];
                acc[x] = 0.f;
                v = sum < 0.f ? -sum : sum;
                row8[x] = (uint8_t)(v >= 1.f ? 255 : (int)(v * 255.f + 0.5f));
            }
            for (; x <= a_hi && x <= QD_W; x++)
                acc[x] = 0.f;
            put_row(y, a_lo, xe, row8);
        }
    }
}

/* ---------------------------------------------------------------- strokes: coverage into the strip's buffer */
/* A stroke is the union of its pieces' coverage (the largest a pixel gets), kept in scov and blended once
 * (stroke_end). Each piece's coverage at a pixel is the pen's band [-hw, hw] across the pixel at its centre's distance
 * d from the piece: min(hw, d + 0.5) - max(-hw, d - 0.5), i.e. hw + 0.5 - d, at most min(2 hw, 1). */
static float s_hw, s_ro, s_ri, s_full;                  /* the half width, the reach (hw + 0.5), the inside (hw - 0.5) */
static unsigned s_cfull;                                /* .. the full coverage (255, or a thin pen's 2 hw) */
static int s_rows0, s_rows1;                            /* (the rows touched, for the composer) */

static void stroke_begin(float lw_dev)
{
    s_hw = 0.5f * lw_dev;
    s_ro = s_hw + 0.5f, s_ri = s_hw - 0.5f;
    s_full = s_hw >= 0.5f ? 1.f : 2.f * s_hw;
    s_cfull = (unsigned)(s_full * 255.f + 0.5f);
    s_rows0 = QD_H, s_rows1 = -1;
}
static void stroke_end(void)
{
    int y;
    for (y = s_rows0; y < s_rows1; y++) {
        const int ri = y - y0s;
        if (shi[ri] >= slo[ri])
            put_row(y, slo[ri], shi[ri], scov(ri));
        slo[ri] = QD_W, shi[ri] = -1;
    }
}
/* row y's pixels x0 .. x1 (clipped to the paint's columns) about to be written: their span kept; 0 none left */
static uint8_t *cov_row(int y, int *x0, int *x1)
{
    int ri;
    *x0 = *x0 < p_c0 ? p_c0 : *x0, *x1 = *x1 >= p_c1 ? p_c1 - 1 : *x1;
    if (*x1 < *x0)
        return 0;
    ri = y - y0s;
    slo[ri] = (int16_t)(*x0 < slo[ri] ? *x0 : slo[ri]);
    shi[ri] = (int16_t)(*x1 > shi[ri] ? *x1 : shi[ri]);
    s_rows0 = y < s_rows0 ? y : s_rows0, s_rows1 = y + 1 > s_rows1 ? y + 1 : s_rows1;
    return scov(ri);
}
QF_INL void cov_max(uint8_t *cv, int x, float d)        /* the pen's coverage at distance d */
{
    const float v = s_ro - d;
    unsigned c;
    if (v <= 0.f)
        return;
    c = v >= s_full ? s_cfull : (unsigned)(v * 255.f + 0.5f);
    if (c > cv[x])
        cv[x] = (uint8_t)c;
}
static void rows_of(float y0, float y1, int *r0, int *r1)   /* the rows of [y0, y1] within the paint's */
{
    *r0 = qf_floor(y0), *r1 = qf_ceil(y1);
    *r0 = *r0 < p_r0 ? p_r0 : *r0, *r1 = *r1 > p_r1 ? p_r1 : *r1;
}

/* a pen's dot at (cx, cy) (a segment of no length) */
static void dot(float cx, float cy)
{
    int y, r0, r1;
    rows_of(cy - s_ro, cy + s_ro, &r0, &r1);
    for (y = r0; y < r1; y++) {
        const float dy = (float)y + 0.5f - cy, h2 = s_ro * s_ro - dy * dy;
        float h;
        int x, x0, x1;
        uint8_t *cv;
        if (h2 <= 0.f)
            continue;
        h = qf_sqrt(h2);
        x0 = qf_ceil(cx - h - 0.5f), x1 = qf_floor(cx + h - 0.5f);
        if (!(cv = cov_row(y, &x0, &x1)))
            continue;
        for (x = x0; x <= x1; x++) {
            const float dx = (float)x + 0.5f - cx;
            cov_max(cv, x, qf_sqrt(dx * dx + dy * dy));
        }
    }
}

/* a segment along a row (y fixed): its body's coverage is the row's alone, its caps the dots' */
static void hseg(float xa, float xb, float yy)
{
    int y, r0, r1;
    if (xa > xb) {
        const float t = xa;
        xa = xb, xb = t;
    }
    rows_of(yy - s_ro, yy + s_ro, &r0, &r1);
    for (y = r0; y < r1; y++) {
        const float dy = qf_abs((float)y + 0.5f - yy), h2 = s_ro * s_ro - dy * dy;
        float h, vb;
        unsigned cb;
        int x, x0, x1, b0, b1;
        uint8_t *cv;
        if (h2 <= 0.f)
            continue;
        h = qf_sqrt(h2);
        x0 = qf_ceil(xa - h - 0.5f), x1 = qf_floor(xb + h - 0.5f);
        if (!(cv = cov_row(y, &x0, &x1)))
            continue;
        vb = s_ro - dy;
        cb = vb >= s_full ? s_cfull : (unsigned)(vb * 255.f + 0.5f);
        b0 = qf_ceil(xa - 0.5f), b1 = qf_floor(xb - 0.5f);   /* (the body's pixel centres) */
        b0 = b0 < x0 ? x0 : b0, b1 = b1 > x1 ? x1 : b1;
        for (x = x0; x < b0; x++) {
            const float dx = (float)x + 0.5f - xa;
            cov_max(cv, x, qf_sqrt(dx * dx + dy * dy));
        }
        for (x = b0; x <= b1; x++)
            if (cb > cv[x])
                cv[x] = (uint8_t)cb;
        for (x = (b1 + 1 > x0 ? b1 + 1 : x0); x <= x1; x++) {
            const float dx = (float)x + 0.5f - xb;
            cov_max(cv, x, qf_sqrt(dx * dx + dy * dy));
        }
    }
}

/* a segment along a column (x fixed) */
static void vseg(float xx, float ya, float yb)
{
    int y, r0, r1, x0c, x1c;
    float vcol[8];
    if (ya > yb) {
        const float t = ya;
        ya = yb, yb = t;
    }
    x0c = qf_ceil(xx - s_ro - 0.5f), x1c = qf_floor(xx + s_ro - 0.5f);
    rows_of(ya - s_ro, yb + s_ro, &r0, &r1);
    {   /* the body's coverage by column (the same on every row of it) */
        int x;
        for (x = x0c; x <= x1c && x - x0c < 8; x++)
            vcol[x - x0c] = s_ro - qf_abs((float)x + 0.5f - xx);
    }
    for (y = r0; y < r1; y++) {
        const float yc = (float)y + 0.5f;
        int x, x0 = x0c, x1 = x1c;
        uint8_t *cv;
        if (!(cv = cov_row(y, &x0, &x1)))
            continue;
        if (yc >= ya && yc <= yb && x1c - x0c < 8) {
            for (x = x0; x <= x1; x++) {
                const float v = vcol[x - x0c];
                unsigned c;
                if (v <= 0.f)
                    continue;
                c = v >= s_full ? s_cfull : (unsigned)(v * 255.f + 0.5f);
                if (c > cv[x])
                    cv[x] = (uint8_t)c;
            }
        } else {
            const float dy = yc < ya ? yc - ya : yc > yb ? yc - yb : 0.f;
            for (x = x0; x <= x1; x++) {
                const float dx = (float)x + 0.5f - xx;
                cov_max(cv, x, qf_sqrt(dx * dx + dy * dy));
            }
        }
    }
}

/* a segment a-b with round caps: along its body the distance is the cross product (no root), past its ends the
 * distance to the end */
static void capsule(float ax, float ay, float bx, float by)
{
    const float dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy, ro = s_ro, ri = s_ri;
    float L, ux, uy, iux, iuy;
    int y, r0, r1;
    rows_of((ay < by ? ay : by) - ro, (ay > by ? ay : by) + ro, &r0, &r1);
    if (r0 >= r1)
        return;                                         /* (P2.6c: not in this strip's rows: nothing worked out) */
    if (L2 < 1e-6f) {
        dot(ax, ay);
        return;
    }
    if (qf_abs(dy) < 1e-4f) {
        hseg(ax, bx, 0.5f * (ay + by));
        return;
    }
    if (qf_abs(dx) < 1e-4f) {
        vseg(0.5f * (ax + bx), ay, by);
        return;
    }
    L = qf_sqrt(L2);
    ux = dx / L, uy = dy / L;
    iux = 1.f / ux, iuy = 1.f / uy;
    for (y = r0; y < r1; y++) {
        const float yc = (float)y + 0.5f, wy = yc - ay, ey = yc - by;
        float lo, hi, il = 1.f, ir = 0.f, p, q, t, cr, xc;
        int x, x0, x1;
        uint8_t *cv;
        /* the body's span: |wx uy - wy ux| <= ro and 0 <= wx ux + wy uy <= L (wx = x - ax) */
        p = (wy * ux - ro) * iuy, q = (wy * ux + ro) * iuy;
        lo = p < q ? p : q, hi = p < q ? q : p;
        p = -wy * uy * iux, q = (L - wy * uy) * iux;
        lo = (p < q ? p : q) > lo ? (p < q ? p : q) : lo;
        hi = (p < q ? q : p) < hi ? (p < q ? q : p) : hi;
        if (lo <= hi)
            lo += ax, hi += ax;
        else
            lo = 1e30f, hi = -1e30f;
        if (wy < ro && wy > -ro) {                      /* .. and the caps' */
            const float h = qf_sqrt(ro * ro - wy * wy);
            lo = ax - h < lo ? ax - h : lo, hi = ax + h > hi ? ax + h : hi;
        }
        if (ey < ro && ey > -ro) {
            const float h = qf_sqrt(ro * ro - ey * ey);
            lo = bx - h < lo ? bx - h : lo, hi = bx + h > hi ? bx + h : hi;
        }
        if (lo > hi)
            continue;
        x0 = qf_ceil(lo - 0.5f), x1 = qf_floor(hi - 0.5f);
        if (!(cv = cov_row(y, &x0, &x1)))
            continue;
        if (ri > 0.f) {                                 /* the body's inside: full */
            float a, b;
            p = (wy * ux - ri) * iuy, q = (wy * ux + ri) * iuy;
            a = p < q ? p : q, b = p < q ? q : p;
            p = -wy * uy * iux, q = (L - wy * uy) * iux;
            a = (p < q ? p : q) > a ? (p < q ? p : q) : a;
            b = (p < q ? q : p) < b ? (p < q ? q : p) : b;
            if (a <= b)
                il = a + ax, ir = b + ax;
        }
        xc = (float)x0 + 0.5f;
        t = (xc - ax) * ux + wy * uy;                   /* (along the segment, and across it: by x, increments) */
        cr = (xc - ax) * uy - wy * ux;
        for (x = x0; x <= x1; x++, xc += 1.f, t += ux, cr += uy) {
            if (cv[x] >= s_cfull)
                continue;                               /* (P2.6c: full already: nothing can raise it; a polyline's
                                                         * joints are mostly this, a neighbour's cap or body) */
            if (xc >= il && xc <= ir) {
                if (s_cfull > cv[x])
                    cv[x] = (uint8_t)s_cfull;
            } else if (t >= 0.f && t <= L) {
                cov_max(cv, x, qf_abs(cr));
            } else {
                const float ex = t < 0.f ? xc - ax : xc - bx, ez = t < 0.f ? wy : ey;
                cov_max(cv, x, qf_sqrt(ex * ex + ez * ez));
            }
        }
    }
}

/* a circle's arc (screen: centre, radius, a0 and the sweep da > 0, clockwise on the screen): the ring's distance
 * inside the sweep, the ends' (round caps) outside it; da >= 2 pi a whole ring */
QF_INL int wedge_in(float px, float py, float sx, float sy, float ex, float ey, float da)
{
    const float c1 = sx * py - sy * px, c2 = px * ey - py * ex;   /* (s x p, p x e) */
    return da <= QF_PI ? (c1 >= 0.f && c2 >= 0.f) : !(c1 < 0.f && c2 < 0.f);
}
static void ring_v(float cx, float cy, float r, float sx, float sy, float ex, float ey, float da)
{
    const float ro = r + s_ro, rin = r - s_ro;
    const int whole = da >= QF_TAU - 1e-4f, small = r < 3.5f;
    float P0x = 0.f, P0y = 0.f, P1x = 0.f, P1y = 0.f;
    float y0 = cy - ro, y1 = cy + ro;
    int y, r0, r1;
    if (!whole) {
        P0x = cx + r * sx, P0y = cy + r * sy, P1x = cx + r * ex, P1y = cy + r * ey;
        {   /* the rows: the arc's box (its ends, and its lowest and highest points if it passes them) */
            float lo = P0y < P1y ? P0y : P1y, hi = P0y < P1y ? P1y : P0y;
            if (wedge_in(0.f, 1.f, sx, sy, ex, ey, da))
                hi = cy + r;
            if (wedge_in(0.f, -1.f, sx, sy, ex, ey, da))
                lo = cy - r;
            y0 = lo - s_ro, y1 = hi + s_ro;
        }
    }
    rows_of(y0, y1, &r0, &r1);
    for (y = r0; y < r1; y++) {
        const float yc = (float)y + 0.5f, dy = yc - cy, ho2 = ro * ro - dy * dy;
        float ho, hin = -1.f;
        int x, x0, x1, side;
        uint8_t *cv;
        if (ho2 <= 0.f)
            continue;
        ho = qf_sqrt(ho2);
        if (rin > 0.f && rin * rin - dy * dy > 0.f)
            hin = qf_sqrt(rin * rin - dy * dy);
        for (side = 0; side < 2; side++) {              /* (left of the hole, right of it; one span with none) */
            if (hin < 0.f) {
                if (side)
                    break;
                x0 = qf_ceil(cx - ho - 0.5f), x1 = qf_floor(cx + ho - 0.5f);
            } else if (!side) {
                x0 = qf_ceil(cx - ho - 0.5f), x1 = qf_floor(cx - hin - 0.5f);
            } else {
                x0 = qf_ceil(cx + hin - 0.5f), x1 = qf_floor(cx + ho - 0.5f);
            }
            if (!(cv = cov_row(y, &x0, &x1)))
                continue;
            for (x = x0; x <= x1; x++) {
                const float dx = (float)x + 0.5f - cx;
                if (!whole && !wedge_in(dx, dy, sx, sy, ex, ey, da)) {
                    {
                        const float qx = (float)x + 0.5f - P0x, qy = yc - P0y, wx = (float)x + 0.5f - P1x,
                                    wy = yc - P1y;
                        const float d0 = qx * qx + qy * qy, d1 = wx * wx + wy * wy;
                        cov_max(cv, x, qf_sqrt(d0 < d1 ? d0 : d1));
                        continue;
                    }
                }
                if (small) {                            /* (a small ring: four quarter pixels, its curve) */
                    float v = 0.f;
                    int k;
                    for (k = 0; k < 4; k++) {
                        const float ux = dx + (k & 1 ? 0.25f : -0.25f), uy = dy + (k & 2 ? 0.25f : -0.25f);
                        const float d = qf_abs(qf_sqrt(ux * ux + uy * uy) - r);
                        float o = (s_hw < d + 0.25f ? s_hw : d + 0.25f) - (-s_hw > d - 0.25f ? -s_hw : d - 0.25f);
                        v += o > 0.f ? o : 0.f;
                    }
                    v *= 0.5f;                          /* (each quarter's band over its half width) */
                    v = v > 1.f ? 1.f : v;
                    if (v > 0.f) {
                        const unsigned c = (unsigned)(v * 255.f + 0.5f);
                        if (c > cv[x])
                            cv[x] = (uint8_t)c;
                    }
                    continue;
                }
                cov_max(cv, x, qf_abs(qf_sqrt(dx * dx + dy * dy) - r));
            }
        }
    }
}

static void ring(float cx, float cy, float r, float a0, float da)
{
    if (da >= QF_TAU - 1e-4f)
        ring_v(cx, cy, r, 1.f, 0.f, 1.f, 0.f, QF_TAU);
    else
        ring_v(cx, cy, r, cosf(a0), sinf(a0), cosf(a0 + da), sinf(a0 + da), da);
}

/* the path's segments, dashed if the state says so (the pattern from each contour's start, in user units: the
 * screen's are the transform's scale times them) */
static float d_len[8];
static int d_n, d_i, d_on;
static float d_left;
static void seg_dashed(float ax, float ay, float bx, float by)
{
    const float dx = bx - ax, dy = by - ay, L = qf_sqrt(dx * dx + dy * dy);
    float t = 0.f;
    if (L <= 0.f) {
        if (d_on)
            capsule(ax, ay, bx, by);
        return;
    }
    while (t < L) {
        const float step = d_left < L - t ? d_left : L - t;
        if (d_on && step > 0.f)
            capsule(ax + dx * (t / L), ay + dy * (t / L), ax + dx * ((t + step) / L), ay + dy * ((t + step) / L));
        t += step;
        d_left -= step;
        if (d_left <= 0.f) {
            d_i = (d_i + 1) % d_n;
            d_on = !d_on;
            d_left = d_len[d_i];
            if (d_on && d_left <= 0.f)                  /* (a zero dash: a dot) */
                dot(ax + dx * (t / L), ay + dy * (t / L));
        }
    }
}
static void dash_begin(void)
{
    const float sc = m_scale(&S.m);
    int i;
    d_n = S.ndash;
    for (i = 0; i < d_n; i++)
        d_len[i] = S.dash[i] * sc;
    d_i = 0, d_on = 1, d_left = d_len[0];
}

static void stroke_path(float lw_dev)
{
    int k;
    if (npt < 1 || p_r1 <= p_r0 || lw_dev <= 0.f)
        return;
    stroke_begin(lw_dev);
    for (k = 0; k < nct; k++) {
        const int i0 = ct0[k], i1 = ct0[k + 1], closed = ctc[k];
        int i;
        if (i1 - i0 < 2)
            continue;
        if (S.ndash) {
            dash_begin();
            for (i = i0; i + 1 < i1; i++)
                seg_dashed(px_[i], py_[i], px_[i + 1], py_[i + 1]);
            if (closed)
                seg_dashed(px_[i1 - 1], py_[i1 - 1], px_[i0], py_[i0]);
        } else {
            for (i = i0; i + 1 < i1; i++)
                capsule(px_[i], py_[i], px_[i + 1], py_[i + 1]);
            if (closed)
                capsule(px_[i1 - 1], py_[i1 - 1], px_[i0], py_[i0]);
        }
    }
    stroke_end();
}

/* ---------------------------------------------------------------- painting the path */
static void paint(qd_rgb stroke, float lw, qd_rgb fill, int keep)
{
    const float sc = m_scale(&S.m);
    const int st = stroke != QD_NONE && lw > 0.f;
    const float lwd = lw * sc, pad = (st ? 0.5f * lwd : 0.f) + 1.f;
    if (hashing) {
        const float v[2] = {lw, (float)keep};
        hcall(0x50414E54u, v, 2, stroke, fill);
        return;
    }
    if (mk || npt == 0 || (!st && fill == QD_NONE) || !dev_visible(bx0 - pad, by0 - pad, bx1 + pad, by1 + pad)) {
        if (!keep)
            path_reset();
        return;
    }
    if (fill != QD_NONE) {
        paint_setup(fill, by0, by1);
        if (ga)
            fill_path();
    }
    if (st) {
        paint_setup(stroke, by0 - pad, by1 + pad);
        if (ga)
            stroke_path(lwd);
    }
    if (!keep)
        path_reset();
}

void qd_path_fill(qd_rgb col) { paint(QD_NONE, 0.f, col, 0); }
void qd_path_stroke(qd_rgb col, float lw) { paint(col, lw > 0.f ? lw : S.lw, QD_NONE, 0); }
void qd_path_stroke_keep(qd_rgb col, float lw) { paint(col, lw > 0.f ? lw : S.lw, QD_NONE, 1); }
void qd_path_paint(qd_rgb stroke, float lw, qd_rgb fill) { paint(stroke, lw > 0.f ? lw : S.lw, fill, 0); }
void qd_path_clip(void)                                 /* (the path's box: the screens do not use it) */
{
    if (npt) {
        S.cx0 = bx0 > S.cx0 ? bx0 : S.cx0, S.cy0 = by0 > S.cy0 ? by0 : S.cy0;
        S.cx1 = bx1 < S.cx1 ? bx1 : S.cx1, S.cy1 = by1 < S.cy1 ? by1 : S.cy1;
    }
    path_reset();
}

/* ---------------------------------------------------------------- shapes */
/* a shape's user box (with the stroke's reach) misses the target: nothing built */
static int shape_out(float x, float y, float w, float h, qd_rgb stroke, float lw)
{
    float X0, Y0, X1, Y1, pad;
    box_dev(&S.m, x, y, w, h, &X0, &Y0, &X1, &Y1);
    pad = (stroke != QD_NONE && lw > 0.f ? 0.5f * lw * m_scale(&S.m) : 0.f) + 1.f;
    return !dev_visible(X0 - pad, Y0 - pad, X1 + pad, Y1 + pad);
}

void qd_line(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    if (hashing) {
        const float hv[5] = {x0, y0, x1, y1, lw};
        hcall(0x4C494E31u, hv, 5, col, 0);
        return;
    }
    if (col == QD_NONE || lw <= 0.f)
        return;
    if (shape_out(x0 < x1 ? x0 : x1, y0 < y1 ? y0 : y1, qf_abs(x1 - x0), qf_abs(y1 - y0), col, lw))
        return;
    path_reset();
    qd_move_to(x0, y0);
    qd_line_to(x1, y1);
    paint(col, lw, QD_NONE, 0);
}

void qd_polyline(const float *xy, int npts, int closed, qd_rgb stroke, float lw, qd_rgb fill)
{
    int i;
    float x0, y0, x1, y1;
    if (!xy || npts < 1)
        return;
    if (hashing) {
        const float hv[2] = {lw, (float)closed};
        hb(xy, (size_t)npts * 2u * sizeof *xy);
        hcall(0x504F4C59u, hv, 2, stroke, fill);
        return;
    }
    x0 = x1 = xy[0], y0 = y1 = xy[1];
    for (i = 1; i < npts; i++) {
        const float x = xy[2 * i], y = xy[2 * i + 1];
        x0 = x < x0 ? x : x0, x1 = x > x1 ? x : x1, y0 = y < y0 ? y : y0, y1 = y > y1 ? y : y1;
    }
    if (shape_out(x0, y0, x1 - x0, y1 - y0, stroke, lw))
        return;
    path_reset();
    qd_move_to(xy[0], xy[1]);
    for (i = 1; i < npts; i++)
        qd_line_to(xy[2 * i], xy[2 * i + 1]);
    if (closed)
        qd_path_close();
    paint(stroke, lw, fill, 0);
}

void qd_ellipse(float cx, float cy, float rx, float ry, qd_rgb stroke, float lw, qd_rgb fill)
{
    if (hashing) {
        const float hv[5] = {cx, cy, rx, ry, lw};
        hcall(0x454C4C49u, hv, 5, stroke, fill);
        return;
    }
    if (rx <= 0.f || ry <= 0.f || shape_out(cx - rx, cy - ry, 2.f * rx, 2.f * ry, stroke, lw))
        return;
    path_reset();
    arc_user(cx, cy, rx, ry, -QF_PI / 2.f, QF_TAU, 0);
    qd_path_close();
    paint(stroke, lw, fill, 0);
}

void qd_ellipse_arc(float cx, float cy, float rx, float ry, float a0, float a1, qd_rgb stroke, float lw)
{
    float da;
    if (hashing) {
        const float hv[7] = {cx, cy, rx, ry, a0, a1, lw};
        hcall(0x454C4152u, hv, 7, stroke, 0);
        return;
    }
    if (rx <= 0.f || ry <= 0.f || shape_out(cx - rx, cy - ry, 2.f * rx, 2.f * ry, stroke, lw))
        return;
    da = sweep_cw(a0, a1);
    path_reset();
    if (da != 0.f)
        arc_user(cx, cy, rx, ry, a0, da, 0);
    paint(stroke, lw, QD_NONE, 0);
}

/* a rectangle on the screen's axes (x0, y0) .. (x1, y1), its edges' exact coverage */
static void rect_dev(float x0, float y0, float x1, float y1)
{
    int y, x, i0, i1;
    float fl, fr;
    if (x1 <= x0 || y1 <= y0)
        return;
    x0 = x0 < 0.f ? 0.f : x0, x1 = x1 > (float)QD_W ? (float)QD_W : x1;
    if (x1 <= x0)
        return;
    i0 = qf_floor(x0), i1 = qf_ceil(x1) - 1;
    if (i1 == i0) {
        fl = fr = x1 - x0;
    } else {
        fl = (float)(i0 + 1) - x0, fr = x1 - (float)i1;
    }
    for (y = p_r0; y < p_r1; y++) {
        const float a = (float)y > y0 ? (float)y : y0, b = (float)(y + 1) < y1 ? (float)(y + 1) : y1;
        const float fy = b - a;
        unsigned cy;
        if (fy <= 0.f)
            continue;
        cy = (unsigned)(fy * 255.f + 0.5f);
        row8[i0] = (uint8_t)((unsigned)(fl * (float)cy + 0.5f));
        for (x = i0 + 1; x < i1; x++)
            row8[x] = (uint8_t)cy;
        if (i1 > i0)
            row8[i1] = (uint8_t)((unsigned)(fr * (float)cy + 0.5f));
        put_row(y, i0, i1, row8);
    }
}

static int axis_aligned(void) { return S.m.b == 0.f && S.m.c == 0.f; }

/* the transform a turn and a uniform scale (no mirror, no squeeze): its scale, and its turn in *rot; 0 when not */
static float similar(float *rot)
{
    const qf_m *m = &S.m;
    const float s2 = m->a * m->a + m->b * m->b;
    if (s2 <= 0.f || qf_abs(m->a - m->d) > 1e-5f * (qf_abs(m->a) + 1e-6f) + 1e-7f ||
        qf_abs(m->b + m->c) > 1e-5f * (qf_abs(m->b) + 1e-6f) + 1e-7f)
        return 0.f;
    *rot = m->b == 0.f ? (m->a > 0.f ? 0.f : QF_PI) : atan2f(m->b, m->a);
    return qf_sqrt(s2);
}

/* a disc's fill (screen): its edge's coverage by distance */
static void disc_fill(float cx, float cy, float r)
{
    const float ro = r + 0.5f, ri = r - 0.5f, cap = r < 0.5642f ? QF_PI * r * r : 1.f;
    int y, r0, r1;
    r0 = qf_floor(cy - ro), r1 = qf_ceil(cy + ro);
    r0 = r0 < p_r0 ? p_r0 : r0, r1 = r1 > p_r1 ? p_r1 : r1;
    for (y = r0; y < r1; y++) {
        const float dy = (float)y + 0.5f - cy, h2 = ro * ro - dy * dy;
        float h, hi = -1.f;
        int x, x0, x1;
        if (h2 <= 0.f)
            continue;
        h = qf_sqrt(h2);
        if (ri > 0.f && ri * ri - dy * dy > 0.f)
            hi = qf_sqrt(ri * ri - dy * dy);
        x0 = qf_ceil(cx - h - 0.5f), x1 = qf_floor(cx + h - 0.5f);
        x0 = x0 < p_c0 ? p_c0 : x0, x1 = x1 >= p_c1 ? p_c1 - 1 : x1;
        if (x1 < x0)
            continue;
        for (x = x0; x <= x1; x++) {
            const float dx = (float)x + 0.5f - cx;
            if (dx <= hi && dx >= -hi) {
                row8[x] = 255;
            } else {
                float v = ro - qf_sqrt(dx * dx + dy * dy);
                v = v > cap ? cap : v;
                row8[x] = (uint8_t)(v <= 0.f ? 0 : (int)(v * 255.f + 0.5f));
            }
        }
        put_row(y, x0, x1, row8);
    }
}

void qd_circle(float cx, float cy, float r, qd_rgb stroke, float lw, qd_rgb fill)
{
    float rot, sc, X, Y;
    if (hashing) {
        const float hv[4] = {cx, cy, r, lw};
        hcall(0x43495243u, hv, 4, stroke, fill);
        return;
    }
    if (r <= 0.f || shape_out(cx - r, cy - r, 2.f * r, 2.f * r, stroke, lw) || mk)
        return;
    sc = S.ndash ? 0.f : similar(&rot);
    if (sc > 0.f) {                                     /* (the screen's circle: analytic) */
        map(&S.m, cx, cy, &X, &Y);
        if (fill != QD_NONE) {
            paint_setup(fill, Y - r * sc - 1.f, Y + r * sc + 1.f);
            if (ga)
                disc_fill(X, Y, r * sc);
        }
        if (stroke != QD_NONE && lw > 0.f) {
            const float hw = 0.5f * lw * sc;
            paint_setup(stroke, Y - r * sc - hw - 1.f, Y + r * sc + hw + 1.f);
            if (ga) {
                stroke_begin(lw * sc);
                ring(X, Y, r * sc, 0.f, QF_TAU);
                stroke_end();
            }
        }
        return;
    }
    path_reset();
    arc_user(cx, cy, r, r, 0.f, QF_TAU, 0);
    qd_path_close();
    paint(stroke, lw, fill, 0);
}

void qd_arc(float cx, float cy, float r, float a0, float a1, qd_rgb stroke, float lw)
{
    float da, rot, sc, X, Y;
    if (hashing) {
        const float hv[6] = {cx, cy, r, a0, a1, lw};
        hcall(0x41524320u, hv, 6, stroke, 0);
        return;
    }
    if (r <= 0.f || stroke == QD_NONE || lw <= 0.f || mk || shape_out(cx - r, cy - r, 2.f * r, 2.f * r, stroke, lw))
        return;
    da = sweep_cw(a0, a1);
    if (da == 0.f)
        return;
    sc = similar(&rot);
    if (sc > 0.f && !S.ndash) {
        const float hw = 0.5f * lw * sc;
        map(&S.m, cx, cy, &X, &Y);
        paint_setup(stroke, Y - r * sc - hw - 1.f, Y + r * sc + hw + 1.f);
        if (ga) {
            stroke_begin(lw * sc);
            ring(X, Y, r * sc, a0 + rot, da);
            stroke_end();
        }
        return;
    }
    path_reset();
    arc_user(cx, cy, r, r, a0, da, 0);
    paint(stroke, lw, QD_NONE, 0);
}

void qd_rrect(float x, float y, float w, float h, float r, qd_rgb stroke, float lw, qd_rgb fill)
{
    float rt, bt;
    const int plain = S.m.b == 0.f && S.m.c == 0.f && S.m.a == S.m.d && S.m.a > 0.f;   /* (on the screen's axes) */
    if (w <= 0.f || h <= 0.f || mk)
        return;
    if (hashing) {
        const float hv[6] = {x, y, w, h, r, lw};
        hcall(0x52524543u, hv, 6, stroke, fill);
        return;
    }
    r = r > w / 2.f ? w / 2.f : r;
    r = r > h / 2.f ? h / 2.f : r;
    r = r < 0.f ? 0.f : r;
    if (shape_out(x, y, w, h, stroke, lw))
        return;
    rt = x + w, bt = y + h;
    if (fill != QD_NONE) {
        if (r == 0.f && axis_aligned()) {               /* (a plain rectangle's fill: no path) */
            float X0, Y0, X1, Y1;
            box_dev(&S.m, x, y, w, h, &X0, &Y0, &X1, &Y1);
            paint_setup(fill, Y0, Y1);
            if (ga)
                rect_dev(X0, Y0, X1, Y1);
        } else {
            path_reset();
            qd_move_to(x, y + r);
            arc_user(x + r, y + r, r, r, QF_PI, QF_PI / 2.f, 1);
            qd_line_to(rt - r, y);
            arc_user(rt - r, y + r, r, r, -QF_PI / 2.f, QF_PI / 2.f, 1);
            qd_line_to(rt, bt - r);
            arc_user(rt - r, bt - r, r, r, 0.f, QF_PI / 2.f, 1);
            qd_line_to(x + r, bt);
            arc_user(x + r, bt - r, r, r, QF_PI / 2.f, QF_PI / 2.f, 1);
            qd_path_close();
            paint(QD_NONE, 0.f, fill, 0);
        }
    }
    if (stroke == QD_NONE || lw <= 0.f)
        return;
    if (plain && !S.ndash) {                            /* (the outline: four sides and four quarter rings) */
        const float s = S.m.a, X0 = x * s + S.m.e, Y0 = y * s + S.m.f, X1 = rt * s + S.m.e, Y1 = bt * s + S.m.f;
        const float R = r * s, hw = 0.5f * lw * s;
        paint_setup(stroke, Y0 - hw - 1.f, Y1 + hw + 1.f);
        if (!ga)
            return;
        stroke_begin(lw * s);
        if (X1 - X0 > 2.f * R + 1e-4f) {
            hseg(X0 + R, X1 - R, Y0);
            hseg(X0 + R, X1 - R, Y1);
        }
        if (Y1 - Y0 > 2.f * R + 1e-4f) {
            vseg(X0, Y0 + R, Y1 - R);
            vseg(X1, Y0 + R, Y1 - R);
        }
        if (R > 0.f) {
            ring_v(X0 + R, Y0 + R, R, -1.f, 0.f, 0.f, -1.f, QF_PI / 2.f);   /* (the corners: quarter rings) */
            ring_v(X1 - R, Y0 + R, R, 0.f, -1.f, 1.f, 0.f, QF_PI / 2.f);
            ring_v(X1 - R, Y1 - R, R, 1.f, 0.f, 0.f, 1.f, QF_PI / 2.f);
            ring_v(X0 + R, Y1 - R, R, 0.f, 1.f, -1.f, 0.f, QF_PI / 2.f);
        }
        stroke_end();
        return;
    }
    path_reset();
    if (r == 0.f) {
        qd_move_to(x, y);
        qd_line_to(rt, y);
        qd_line_to(rt, bt);
        qd_line_to(x, bt);
    } else {
        qd_move_to(x, y + r);
        arc_user(x + r, y + r, r, r, QF_PI, QF_PI / 2.f, 1);
        qd_line_to(rt - r, y);
        arc_user(rt - r, y + r, r, r, -QF_PI / 2.f, QF_PI / 2.f, 1);
        qd_line_to(rt, bt - r);
        arc_user(rt - r, bt - r, r, r, 0.f, QF_PI / 2.f, 1);
        qd_line_to(x + r, bt);
        arc_user(x + r, bt - r, r, r, QF_PI / 2.f, QF_PI / 2.f, 1);
    }
    qd_path_close();
    paint(stroke, lw, QD_NONE, 0);
}

void qd_fill_rect(float x, float y, float w, float h, qd_rgb col)
{
    if (hashing) {
        const float hv[4] = {x, y, w, h};
        hcall(0x46524543u, hv, 4, col, 0);
        return;
    }
    if (w <= 0.f || h <= 0.f || col == QD_NONE || shape_out(x, y, w, h, QD_NONE, 0.f))
        return;
    if (axis_aligned()) {
        float X0, Y0, X1, Y1;
        box_dev(&S.m, x, y, w, h, &X0, &Y0, &X1, &Y1);
        paint_setup(col, Y0, Y1);
        if (ga && !mk)
            rect_dev(X0, Y0, X1, Y1);
        return;
    }
    path_reset();
    qd_move_to(x, y);
    qd_line_to(x + w, y);
    qd_line_to(x + w, y + h);
    qd_line_to(x, y + h);
    qd_path_close();
    paint(QD_NONE, 0.f, col, 0);
}

/* ---------------------------------------------------------------- layers: none (as plutovg's device build) */
int qd_layer_begin(int slot, int w, int h)
{
    (void)slot, (void)w, (void)h;
    return 0;
}
int qd_layer_resume(int slot)
{
    (void)slot;
    return 0;
}
void qd_layer_end(void) {}
void qd_layer_draw(int slot, float x, float y, float w, float h) { (void)slot, (void)x, (void)y, (void)w, (void)h; }

/* ---------------------------------------------------------------- text */
static int weight_index(int w)                          /* the nearest of 300 500 700, a tie to the heavier */
{
    int i, best = 0, bd = 1 << 30;
    for (i = 0; i < QF_NW; i++) {
        const int d = QF_WEIGHT[i] > w ? QF_WEIGHT[i] - w : w - QF_WEIGHT[i];
        if (d < bd || (d == bd && QF_WEIGHT[i] > QF_WEIGHT[best]))
            bd = d, best = i;
    }
    return best;
}

static uint32_t utf8_next(const char **ps)
{
    const unsigned char *s = (const unsigned char *)*ps;
    uint32_t c = s[0];
    int n = 0, i;
    if (c < 0x80u) {
        *ps += 1;
        return c;
    }
    if ((c & 0xE0u) == 0xC0u)
        c &= 0x1Fu, n = 1;
    else if ((c & 0xF0u) == 0xE0u)
        c &= 0x0Fu, n = 2;
    else if ((c & 0xF8u) == 0xF0u)
        c &= 0x07u, n = 3;
    else {
        *ps += 1;
        return 0xFFFDu;
    }
    for (i = 1; i <= n; i++) {
        if ((s[i] & 0xC0u) != 0x80u) {
            *ps += i;
            return 0xFFFDu;
        }
        c = c << 6 | (s[i] & 0x3Fu);
    }
    *ps += n + 1;
    return c;
}
static int glyph_of(uint32_t cp)
{
    int i;
    if (cp >= 0x20u && cp < 0x7Fu)
        return 1 + (int)(cp - 0x20u);
    for (i = 0; i < (int)(sizeof QF_EXTRA / sizeof QF_EXTRA[0]); i++)
        if (QF_EXTRA[i] == cp)
            return 96 + i;
    return 0;
}

/* the width as plutovg measures it: each glyph's advance (font units x size / units per em) plus the spacing */
static float text_w(const char *s, float size, int wi, float ls)
{
    const float sc = size / (float)QF_UPEM[wi];
    float w = 0.f;
    while (*s) {
        const int g = glyph_of(utf8_next(&s));
        w += (float)QF_ADV[wi][g] * sc + ls;
    }
    return w;
}
float qd_text_width(const char *utf8, const qd_font *f)
{
    if (!utf8 || !f)
        return 0.f;
    return text_w(utf8, f->size, weight_index(f->weight), f->ls);
}

static float tmin = 1e9f;
static char tmin_s[32];
void qd_text_stats_reset(void)
{
    tmin = 1e9f;
    tmin_s[0] = 0;
}
float qd_text_stats_min(const char **text)
{
    if (text)
        *text = tmin_s;
    return tmin;
}

/* a glyph's outline into the path: m (font units to the screen) */
static const uint8_t *vz(const uint8_t *p, int *v)
{
    uint32_t u = 0;
    int sh_ = 0;
    do {
        u |= (uint32_t)(*p & 127u) << sh_;
        sh_ += 7;
    } while (*p++ & 128u);
    *v = (u & 1u) ? -(int)((u + 1u) >> 1) : (int)(u >> 1);
    return p;
}
static void glyph_path(int wi, int g, const qf_m *m)
{
    const uint8_t *p = QF_OUTLINE[wi] + QF_OOFF[wi][g], *ty;
    int n, k, x = 0, y = 0;
    {
        int nn;
        uint32_t u = 0;
        int s_ = 0;
        do {
            u |= (uint32_t)(*p & 127u) << s_;
            s_ += 7;
        } while (*p++ & 128u);
        nn = (int)u;
        n = nn;
    }
    ty = p;
    p += (n + 3) / 4;
    for (k = 0; k < n; k++) {
        const int t = (ty[k >> 2] >> (2 * (k & 3))) & 3;
        int dx, dy;
        float X, Y;
        if (t == 2) {
            float CX, CY, X0, Y0;
            int cx, cy;
            p = vz(p, &dx), p = vz(p, &dy);
            cx = x + dx, cy = y + dy;
            p = vz(p, &dx), p = vz(p, &dy);
            x = cx + dx, y = cy + dy;
            map(m, (float)cx, (float)cy, &CX, &CY);
            map(m, (float)x, (float)y, &X, &Y);
            if (cur_dev(&X0, &Y0))
                quad_dev(X0, Y0, CX, CY, X, Y);
            else
                move_dev(X, Y);
        } else {
            p = vz(p, &dx), p = vz(p, &dy);
            x += dx, y += dy;
            map(m, (float)x, (float)y, &X, &Y);
            if (t == 0)
                move_dev(X, Y);
            else
                line_dev(X, Y);
        }
    }
}

/* an atlas glyph with its pen at (gx, gy) on the screen: the glyph was made with the pen on a whole pixel, so a
 * fraction of a pixel is the 4-bit image moved by it (bilinear, in 1/16 px) */
static uint8_t g_ra[258], g_rb[258];                    /* (a glyph's two source rows, one pixel of margin) */
static void glyph_row(const qf_ag *e, const uint8_t *b, int j, uint8_t *to)
{
    int i, k;
    to[0] = 0, to[e->w + 1] = 0;
    if (j < 0 || j >= e->h) {
        memset(to, 0, (size_t)e->w + 2u);
        return;
    }
    k = j * e->w;
    for (i = 0; i < e->w; i++, k++)
        to[i + 1] = (uint8_t)((b[k >> 1] >> ((k & 1) << 2)) & 15u);
}
static void glyph_atlas(const qf_atlas *at, int g, float gx, float gy)
{
    const qf_ag *e = &at->g[g];
    const uint8_t *b = at->bits + e->off;
    int ox = qf_floor(gx), oy = qf_floor(gy);
    int fx = (int)((gx - (float)ox) * 16.f + 0.5f), fy = (int)((gy - (float)oy) * 16.f + 0.5f);
    int y, x, gx0, gy0, r0, r1, c0, c1, W, H;
    if (!e->w)
        return;
    if (fx >= 16)
        ox++, fx = 0;
    if (fy >= 16)
        oy++, fy = 0;
    W = e->w + (fx ? 1 : 0), H = e->h + (fy ? 1 : 0);
    gx0 = ox + e->x, gy0 = oy + e->y;
    r0 = gy0 > p_r0 ? gy0 : p_r0, r1 = gy0 + H < p_r1 ? gy0 + H : p_r1;
    c0 = gx0 > p_c0 ? gx0 : p_c0, c1 = gx0 + W < p_c1 ? gx0 + W : p_c1;
    if (r1 <= r0 || c1 <= c0)
        return;
    for (y = r0; y < r1; y++) {
        const int j = y - gy0;
        int any = 0;
        glyph_row(e, b, j, g_ra);                       /* (source row j: g_ra[i + 1] is column i) */
        if (fy)
            glyph_row(e, b, j - 1, g_rb);
        for (x = c0; x < c1; x++) {
            const int i = x - gx0;                      /* (output column i: source i, and i - 1 moved right) */
            unsigned v = (unsigned)g_ra[i + 1] * (unsigned)(16 - fx) + (unsigned)g_ra[i] * (unsigned)fx;
            if (fy)
                v = v * (unsigned)(16 - fy) +
                    ((unsigned)g_rb[i + 1] * (unsigned)(16 - fx) + (unsigned)g_rb[i] * (unsigned)fx) * (unsigned)fy;
            else
                v <<= 4;
            /* (v: 0 .. 15 x 256) */
            v = (v * 17u + 128u) >> 8;
            row8[x] = (uint8_t)v;
            any |= (int)v;
        }
        if (any)
            put_row(y, c0, c1 - 1, row8);
        else
            memset(row8 + c0, 0, (size_t)(c1 - c0));
    }
}

static const qf_atlas *atlas_for(float px, int wi)
{
    int i;
    for (i = 0; i < QF_NATLAS; i++)
        if (QF_ATLAS[i].wi == wi && qf_abs((float)QF_ATLAS[i].px - px) < 0.01f)
            return &QF_ATLAS[i];
    return 0;
}

/* the glyphs drawn from pen (user units) along the baseline y */
static void text_draw(const char *s, float pen, float y, float size, int wi, float ls, int vector_only)
{
    const qf_m *m = &S.m;
    const float sc = size / (float)QF_UPEM[wi];
    const qf_atlas *at = 0;
    if (QF_USE_ATLAS && !vector_only && m->b == 0.f && m->c == 0.f && m->a == m->d && m->a > 0.f)
        at = atlas_for(size * m->a, wi);
    if (at) {
        const float gy = m->d * y + m->f;
        while (*s) {
            const int g = glyph_of(utf8_next(&s));
            glyph_atlas(at, g, m->a * pen + m->e, gy);
            pen += (float)QF_ADV[wi][g] * sc + ls;
        }
        return;
    }
    path_reset();
    while (*s) {
        const int g = glyph_of(utf8_next(&s));
        qf_m gm;
        const qf_m t = {sc, 0.f, 0.f, -sc, pen, y};   /* (font units, y up, to the user's) */
        if (npt > QF_MAXPT - 256 || nct > QF_MAXCT - 8) {   /* (a long text: filled a part at a time) */
            fill_path();
            path_reset();
        }
        mul(&gm, &t, m);
        glyph_path(wi, g, &gm);
        pen += (float)QF_ADV[wi][g] * sc + ls;
    }
    fill_path();
    path_reset();
}

float qd_text(const char *utf8, float x, float y, const qd_font *f)
{
    int wi;
    float w, pen, X0, Y0, X1, Y1;
    if (!utf8 || !f || f->size <= 0.f)
        return 0.f;
    wi = weight_index(f->weight);
    if (utf8[0]) {                                      /* the clarity rules' guard (qd_gfx.h) */
        const float s2 = S.m.c * S.m.c + S.m.d * S.m.d;    /* (unscaled: the size itself; qf_sqrt(1) is 1 - 5e-6) */
        const float sz = s2 == 1.f ? f->size : f->size * qf_sqrt(s2);
        if (sz < tmin) {
            int i;
            tmin = sz;
            for (i = 0; utf8[i] && i < 31; i++)
                tmin_s[i] = utf8[i];
            tmin_s[i] = 0;
        }
    }
    w = text_w(utf8, f->size, wi, f->ls);
    if (hashing) {
        const float hv[4] = {x, y, f->size, f->ls};
        hb(utf8, strlen(utf8));
        hu((uint32_t)wi), hu((uint32_t)f->align);
        hcall(0x54455854u, hv, 4, f->col, 0);
        return w;
    }
    pen = f->align == QD_CENTER ? x - w / 2.f : f->align == QD_RIGHT ? x - w : x;
    if (f->col == QD_NONE || mk)
        return w;
    box_dev(&S.m, pen - 0.3f * f->size, y - 1.1f * f->size, (w > 0.f ? w : 0.f) + 0.6f * f->size, 1.45f * f->size,
            &X0, &Y0, &X1, &Y1);
    if (!dev_visible(X0, Y0, X1, Y1))
        return w;
    paint_setup(f->col, Y0, Y1);
    if (ga)
        text_draw(utf8, pen, y, f->size, wi, f->ls, 0);
    return w;
}

/* the text's coverage rows [row0, row0 + rows) into mask (w x rows): its own target, the identity, no clip */
int qd_text_mask_rows(const char *utf8, float x, float y, const qd_font *f, uint8_t *mask, int w, int row0, int rows)
{
    const qf_st keep = S;
    const int kr0 = t_r0, kr1 = t_r1, kc0 = t_c0, kc1 = t_c1;
    int wi;
    float tw, pen;
    if (!utf8 || !f || !mask || w <= 0 || rows <= 0)
        return 0;
    qd_init();
    memset(mask, 0, (size_t)w * (size_t)rows);
    S.m.a = 1.f, S.m.b = 0.f, S.m.c = 0.f, S.m.d = 1.f, S.m.e = 0.f, S.m.f = 0.f;
    S.alpha = 1.f;
    S.cx0 = 0.f, S.cy0 = (float)row0, S.cx1 = (float)(w < QD_W ? w : QD_W), S.cy1 = (float)(row0 + rows);
    mk = mask, mk_w = w, mk_r0 = row0, mk_rows = rows;
    t_r0 = row0, t_r1 = row0 + rows, t_c0 = 0, t_c1 = w < QD_W ? w : QD_W;
    wi = weight_index(f->weight);
    tw = text_w(utf8, f->size, wi, f->ls);
    pen = f->align == QD_CENTER ? x - tw / 2.f : f->align == QD_RIGHT ? x - tw : x;
    paint_setup(0xFFFFFFu, (float)row0, (float)(row0 + rows));
    text_draw(utf8, pen, y, f->size, wi, f->ls, 1);
    mk = 0;
    S = keep;
    t_r0 = kr0, t_r1 = kr1, t_c0 = kc0, t_c1 = kc1;
    path_reset();
    return 1;
}
int qd_text_mask(const char *utf8, float x, float y, const qd_font *f, uint8_t *mask, int w, int h)
{
    int r;
    for (r = 0; r < h; r += 8)
        if (!qd_text_mask_rows(utf8, x, y, f, mask + (size_t)r * (size_t)w, w, r, h - r < 8 ? h - r : 8))
            return 0;
    return h > 0;
}

/* P2.6: the public calls, and the frame recorded once, every strip drawn from it */
#include "qd_gfx_rec.c"
