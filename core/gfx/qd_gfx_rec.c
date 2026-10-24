/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P2.6: ONE INSTANT A FRAME. The device draws the screen in strips (QD_STRIP_H rows), and between two strips its
 * platform runs FIZZ's clock and its input: drawn live, each strip would show another moment (the owner's "each
 * segment lagging behind"). So a frame is recorded once (qd_dl_begin .. qd_dl_end: the screen's drawing calls, at
 * the frame's instant, into a display list) and every strip is drawn from that list (qd_dl_play). The list also gives
 * the frame's hash and one hash a strip (the calls that touch it: qd_dl_strip_hashes), so that a strip that would
 * draw the same pixels need not be drawn.
 *
 * Included at the end of a backend (qd_gfx_fast.c), after its qdb_* (qd_gfx_be.h): the public qd_* calls here go
 * straight to the backend, or, while recording, into the list. A recorded frame asks nothing of the strip:
 * qd_box_visible says 1, qd_get_alpha and qd_text's advance are the list's own. A layer (the backend draws into it at
 * once, the list keeps its placing). The list lives in the drawing's arena (the fast drawing leaves it free while it
 * draws; between frames the borrowers' (pattern.h qd_tmp_borrow) take it back: a kept list is then dropped). No room,
 * or a list that outgrows its block: nothing recorded, the frame is drawn live strip by strip, as before. */
#undef qd_save
#undef qd_restore
#undef qd_translate
#undef qd_scale
#undef qd_rotate
#undef qd_alpha
#undef qd_get_alpha
#undef qd_clip_rect
#undef qd_line_width
#undef qd_line_cap
#undef qd_line_join
#undef qd_dash
#undef qd_line
#undef qd_polyline
#undef qd_circle
#undef qd_arc
#undef qd_ellipse
#undef qd_ellipse_arc
#undef qd_rrect
#undef qd_fill_rect
#undef qd_path_begin
#undef qd_move_to
#undef qd_line_to
#undef qd_quad_to
#undef qd_cubic_to
#undef qd_path_arc
#undef qd_path_close
#undef qd_path_fill
#undef qd_path_stroke
#undef qd_path_stroke_keep
#undef qd_path_paint
#undef qd_path_clip
#undef qd_text
#undef qd_box_visible
#undef qd_layer_begin
#undef qd_layer_resume
#undef qd_layer_end
#undef qd_layer_draw

enum {
    RO_SAVE = 1, RO_RESTORE, RO_TRANSLATE, RO_SCALE, RO_ROTATE, RO_ALPHA, RO_CLIP, RO_LW, RO_CAP, RO_JOIN, RO_DASH,
    RO_LINE, RO_POLY, RO_CIRCLE, RO_ARC, RO_ELL, RO_ELLARC, RO_RRECT, RO_FRECT, RO_PBEGIN, RO_MOVE, RO_LINETO, RO_QUAD,
    RO_CUBIC, RO_PARC, RO_PCLOSE, RO_PFILL, RO_PSTROKE, RO_PSTROKEK, RO_PPAINT, RO_PCLIP, RO_TEXT, RO_LDRAW
};

static uint8_t *rl_buf;                 /* the list (in the arena), its room, what it holds */
static uint32_t rl_cap, rl_len;
static int rl_on, rl_fail, rl_bypass;   /* recording; it outgrew its block; a layer being drawn (straight through) */
static uint32_t rl_nrec, rl_nlive, rl_maxb;            /* (qd_dl_stats) */
static float rl_alpha, rl_ast[32];      /* (qd_get_alpha while recording: the list's own state) */
static int rl_adepth;

static void rl_put(const void *p, uint32_t n)
{
    if (rl_fail || rl_len + n > rl_cap) {
        rl_fail = 1;
        return;
    }
    memcpy(rl_buf + rl_len, p, n);
    rl_len += n;
}
static void rl_op(int op) { const uint32_t w = (uint32_t)op; rl_put(&w, 4); }   /* (a word: the floats aligned) */
static void rl_f(float f) { rl_put(&f, 4); }
static void rl_u(uint32_t u) { rl_put(&u, 4); }
static int rec(void) { return rl_on && !rl_bypass; }

/* ---------------------------------------------------------------- the public calls */
void qd_save(void)
{
    if (!rec()) {
        qdb_save();
        return;
    }
    if (rl_adepth < 32)
        rl_ast[rl_adepth] = rl_alpha;
    rl_adepth++;
    rl_op(RO_SAVE);
}
void qd_restore(void)
{
    if (!rec()) {
        qdb_restore();
        return;
    }
    if (rl_adepth > 0 && --rl_adepth < 32)
        rl_alpha = rl_ast[rl_adepth];
    rl_op(RO_RESTORE);
}
void qd_translate(float x, float y)
{
    if (!rec()) {
        qdb_translate(x, y);
        return;
    }
    rl_op(RO_TRANSLATE), rl_f(x), rl_f(y);
}
void qd_scale(float sx, float sy)
{
    if (!rec()) {
        qdb_scale(sx, sy);
        return;
    }
    rl_op(RO_SCALE), rl_f(sx), rl_f(sy);
}
void qd_rotate(float a)
{
    if (!rec()) {
        qdb_rotate(a);
        return;
    }
    rl_op(RO_ROTATE), rl_f(a);
}
void qd_alpha(float a)
{
    if (!rec()) {
        qdb_alpha(a);
        return;
    }
    rl_alpha = a;
    rl_op(RO_ALPHA), rl_f(a);
}
float qd_get_alpha(void) { return rec() ? rl_alpha : qdb_get_alpha(); }
void qd_clip_rect(float x, float y, float w, float h)
{
    if (!rec()) {
        qdb_clip_rect(x, y, w, h);
        return;
    }
    rl_op(RO_CLIP), rl_f(x), rl_f(y), rl_f(w), rl_f(h);
}
void qd_line_width(float w)
{
    if (!rec()) {
        qdb_line_width(w);
        return;
    }
    rl_op(RO_LW), rl_f(w);
}
void qd_line_cap(int cap)
{
    if (!rec()) {
        qdb_line_cap(cap);
        return;
    }
    rl_op(RO_CAP), rl_u((uint32_t)cap);
}
void qd_line_join(int join)
{
    if (!rec()) {
        qdb_line_join(join);
        return;
    }
    rl_op(RO_JOIN), rl_u((uint32_t)join);
}
void qd_dash(const float *on_off, int n)
{
    int i;
    if (!rec()) {
        qdb_dash(on_off, n);
        return;
    }
    n = n < 0 ? 0 : n > 16 ? 16 : n;
    rl_op(RO_DASH), rl_u((uint32_t)n);
    for (i = 0; i < n; i++)
        rl_f(on_off[i]);
}
void qd_line(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    if (!rec()) {
        qdb_line(x0, y0, x1, y1, col, lw);
        return;
    }
    rl_op(RO_LINE), rl_f(x0), rl_f(y0), rl_f(x1), rl_f(y1), rl_u(col), rl_f(lw);
}
void qd_polyline(const float *xy, int npts, int closed, qd_rgb stroke, float lw, qd_rgb fill)
{
    if (!rec()) {
        qdb_polyline(xy, npts, closed, stroke, lw, fill);
        return;
    }
    npts = npts < 0 ? 0 : npts;
    rl_op(RO_POLY), rl_u((uint32_t)npts), rl_u((uint32_t)closed), rl_u(stroke), rl_f(lw), rl_u(fill);
    rl_put(xy, (uint32_t)npts * 8u);
}
void qd_circle(float cx, float cy, float r, qd_rgb stroke, float lw, qd_rgb fill)
{
    if (!rec()) {
        qdb_circle(cx, cy, r, stroke, lw, fill);
        return;
    }
    rl_op(RO_CIRCLE), rl_f(cx), rl_f(cy), rl_f(r), rl_u(stroke), rl_f(lw), rl_u(fill);
}
void qd_arc(float cx, float cy, float r, float a0, float a1, qd_rgb stroke, float lw)
{
    if (!rec()) {
        qdb_arc(cx, cy, r, a0, a1, stroke, lw);
        return;
    }
    rl_op(RO_ARC), rl_f(cx), rl_f(cy), rl_f(r), rl_f(a0), rl_f(a1), rl_u(stroke), rl_f(lw);
}
void qd_ellipse(float cx, float cy, float rx, float ry, qd_rgb stroke, float lw, qd_rgb fill)
{
    if (!rec()) {
        qdb_ellipse(cx, cy, rx, ry, stroke, lw, fill);
        return;
    }
    rl_op(RO_ELL), rl_f(cx), rl_f(cy), rl_f(rx), rl_f(ry), rl_u(stroke), rl_f(lw), rl_u(fill);
}
void qd_ellipse_arc(float cx, float cy, float rx, float ry, float a0, float a1, qd_rgb stroke, float lw)
{
    if (!rec()) {
        qdb_ellipse_arc(cx, cy, rx, ry, a0, a1, stroke, lw);
        return;
    }
    rl_op(RO_ELLARC), rl_f(cx), rl_f(cy), rl_f(rx), rl_f(ry), rl_f(a0), rl_f(a1), rl_u(stroke), rl_f(lw);
}
void qd_rrect(float x, float y, float w, float h, float r, qd_rgb stroke, float lw, qd_rgb fill)
{
    if (!rec()) {
        qdb_rrect(x, y, w, h, r, stroke, lw, fill);
        return;
    }
    rl_op(RO_RRECT), rl_f(x), rl_f(y), rl_f(w), rl_f(h), rl_f(r), rl_u(stroke), rl_f(lw), rl_u(fill);
}
void qd_fill_rect(float x, float y, float w, float h, qd_rgb col)
{
    if (!rec()) {
        qdb_fill_rect(x, y, w, h, col);
        return;
    }
    rl_op(RO_FRECT), rl_f(x), rl_f(y), rl_f(w), rl_f(h), rl_u(col);
}
void qd_path_begin(void)
{
    if (!rec()) {
        qdb_path_begin();
        return;
    }
    rl_op(RO_PBEGIN), rl_f(-1e30f), rl_f(1e30f);       /* (P2.6e: the rows its paths paint, rl_index's) */
}
void qd_move_to(float x, float y)
{
    if (!rec()) {
        qdb_move_to(x, y);
        return;
    }
    rl_op(RO_MOVE), rl_f(x), rl_f(y);
}
void qd_line_to(float x, float y)
{
    if (!rec()) {
        qdb_line_to(x, y);
        return;
    }
    rl_op(RO_LINETO), rl_f(x), rl_f(y);
}
void qd_quad_to(float cx, float cy, float x, float y)
{
    if (!rec()) {
        qdb_quad_to(cx, cy, x, y);
        return;
    }
    rl_op(RO_QUAD), rl_f(cx), rl_f(cy), rl_f(x), rl_f(y);
}
void qd_cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y)
{
    if (!rec()) {
        qdb_cubic_to(c1x, c1y, c2x, c2y, x, y);
        return;
    }
    rl_op(RO_CUBIC), rl_f(c1x), rl_f(c1y), rl_f(c2x), rl_f(c2y), rl_f(x), rl_f(y);
}
void qd_path_arc(float cx, float cy, float r, float a0, float a1)
{
    if (!rec()) {
        qdb_path_arc(cx, cy, r, a0, a1);
        return;
    }
    rl_op(RO_PARC), rl_f(cx), rl_f(cy), rl_f(r), rl_f(a0), rl_f(a1);
}
void qd_path_close(void)
{
    if (!rec()) {
        qdb_path_close();
        return;
    }
    rl_op(RO_PCLOSE);
}
void qd_path_fill(qd_rgb col)
{
    if (!rec()) {
        qdb_path_fill(col);
        return;
    }
    rl_op(RO_PFILL), rl_u(col);
}
void qd_path_stroke(qd_rgb col, float lw)
{
    if (!rec()) {
        qdb_path_stroke(col, lw);
        return;
    }
    rl_op(RO_PSTROKE), rl_u(col), rl_f(lw);
}
void qd_path_stroke_keep(qd_rgb col, float lw)
{
    if (!rec()) {
        qdb_path_stroke_keep(col, lw);
        return;
    }
    rl_op(RO_PSTROKEK), rl_u(col), rl_f(lw);
}
void qd_path_paint(qd_rgb stroke, float lw, qd_rgb fill)
{
    if (!rec()) {
        qdb_path_paint(stroke, lw, fill);
        return;
    }
    rl_op(RO_PPAINT), rl_u(stroke), rl_f(lw), rl_u(fill);
}
void qd_path_clip(void)
{
    if (!rec()) {
        qdb_path_clip();
        return;
    }
    rl_op(RO_PCLIP);
}
float qd_text(const char *utf8, float x, float y, const qd_font *f)
{
    uint32_t n;
    uint16_t n16;
    if (!rec())
        return qdb_text(utf8, x, y, f);
    if (!utf8 || !f)
        return 0.f;
    n = (uint32_t)strlen(utf8);
    n = n > 255u ? 255u : n;
    n16 = (uint16_t)n;
    rl_op(RO_TEXT), rl_f(x), rl_f(y), rl_f(f->size), rl_u((uint32_t)f->weight), rl_u(f->col), rl_u((uint32_t)f->align),
        rl_f(f->ls);
    rl_put(&n16, 2);
    rl_put(utf8, n);
    {
        static const uint8_t z[4] = {0, 0, 0, 0};
        rl_put(z, (4u - ((2u + n) & 3u)) & 3u);         /* (to a word) */
    }
    return qd_text_width(utf8, f);                      /* (qd_text's advance: the same sums) */
}
int qd_box_visible(float x, float y, float w, float h, float lw)
{
    return rec() ? 1 : qdb_box_visible(x, y, w, h, lw);   /* (a recorded frame: every strip's) */
}
int qd_layer_begin(int slot, int w, int h)
{
    const int r = qdb_layer_begin(slot, w, h);          /* (drawn at once, into the layer: straight through) */
    if (r && rl_on)
        rl_bypass++;
    return r;
}
int qd_layer_resume(int slot)
{
    const int r = qdb_layer_resume(slot);
    if (r && rl_on)
        rl_bypass++;
    return r;
}
void qd_layer_end(void)
{
    if (rl_on && rl_bypass)
        rl_bypass--;
    qdb_layer_end();
}
void qd_layer_draw(int slot, float x, float y, float w, float h)
{
    if (!rec()) {
        qdb_layer_draw(slot, x, y, w, h);
        return;
    }
    rl_op(RO_LDRAW), rl_u((uint32_t)slot), rl_f(x), rl_f(y), rl_f(w), rl_f(h);
}

/* ---------------------------------------------------------------- the list */
#if defined(QD_DEVICE) && defined(QD_GFX_FAST)        /* (the list's room: the arena, the fast drawing leaves it free) */
#define RL_ON 1
#else
#define RL_ON 0                                         /* (no recording: the host draws the whole screen at once;
                                                         * plutovg's drawing needs its arena) */
#endif
#if RL_ON
static void rl_release(void)                           /* (the arena wants its room: the kept list goes) */
{
    if (rl_buf && !rl_on) {
        qd_arena_free(rl_buf);
        rl_buf = 0;
        rl_len = rl_cap = 0;
    }
}
#endif
int qd_dl_begin(void)
{
#if RL_ON
    static const uint32_t SZ[5] = {22528u, 20480u, 16384u, 12288u, 8192u};
    const size_t room = qd_arena_size() - QD_GFX_MARGIN - qd_arena_live();   /* (the margin kept: a list is not the
                                                         * drawing's to spend it) */
    int k;
#endif
    qd_dl_drop();
#if !RL_ON
    return 0;
#else
    for (k = 0; k < 5 && !rl_buf; k++) {
        if (SZ[k] + 64u > room)
            continue;
        const int was = qd_arena_use(1);                /* (the host at the device's sizes: the arena, as the unit) */
        rl_buf = (uint8_t *)qd_arena_try(SZ[k]);
        qd_arena_use(was);
        rl_cap = rl_buf ? SZ[k] : 0u;
    }
    if (!rl_buf) {
        rl_nlive++;
        return 0;
    }
    qd_arena_set_release(rl_release);
    rl_len = 0;
    rl_on = 1, rl_fail = 0, rl_bypass = 0;
    rl_alpha = 1.f, rl_adepth = 0;
    return 1;
#endif
}
int qd_dl_end(void)
{
    rl_on = 0;
    rl_bypass = 0;
    if (rl_fail) {
        rl_nlive++;
        qd_dl_drop();
    } else {
        rl_nrec++;
        rl_maxb = rl_len > rl_maxb ? rl_len : rl_maxb;
#if RL_ON
        {   /* P2.6c: the list cut to what it holds (+ 64): the rest of its block back to the arena (the port's own
             * borrowers too, while the frame is drawn) */
            const int was = qd_arena_use(1);
            rl_buf = (uint8_t *)qd_arena_shrink(rl_buf, rl_len + 64u);
            rl_cap = rl_len + 64u;
            qd_arena_use(was);
        }
#endif
        qd_dl_strip_hashes(0, 0, 0);                    /* (P2.6e: each path group's rows, for the strips' play) */
    }
    return rl_buf != 0;
}
void qd_dl_stats(uint32_t *recorded, uint32_t *outgrown, uint32_t *max_bytes)
{
    *recorded = rl_nrec, *outgrown = rl_nlive, *max_bytes = rl_maxb;
}
int qd_dl_kept(void) { return rl_buf != 0 && !rl_on; }
uint32_t qd_dl_bytes(void) { return rl_len; }
void qd_dl_drop(void)
{
#if RL_ON
    if (rl_buf)
        qd_arena_free(rl_buf);
#endif
    rl_buf = 0;
    rl_len = rl_cap = 0;
    rl_on = rl_fail = 0;
}

/* the reader */
static uint32_t rl_at;
static float rl_gf(void)
{
    float f;
    memcpy(&f, rl_buf + rl_at, 4);
    rl_at += 4;
    return f;
}
static uint32_t rl_gu(void)
{
    uint32_t u;
    memcpy(&u, rl_buf + rl_at, 4);
    rl_at += 4;
    return u;
}

/* the list into the backend: the strip begun (qd_begin_strip). P2.6e: a path group (from a path's begin to the next)
 * whose rows miss the strip's: its path calls and paints left out (the state calls between played) */
void qd_dl_play(void)
{
    float v[8];
    uint32_t u0, u1, u2;
    int i, skip = 0;
    rl_at = 0;
    while (rl_buf && rl_at < rl_len) {
        const int op = (int)rl_gu();
        if (skip && op >= RO_MOVE && op <= RO_PPAINT) {
            static const uint8_t W[RO_PPAINT - RO_MOVE + 1] = {8, 8, 16, 24, 20, 0, 4, 8, 8, 12};   /* (their bytes) */
            rl_at += W[op - RO_MOVE];
            continue;
        }
        switch (op) {
        case RO_SAVE: qdb_save(); break;
        case RO_RESTORE: qdb_restore(); break;
        case RO_TRANSLATE: v[0] = rl_gf(), v[1] = rl_gf(); qdb_translate(v[0], v[1]); break;
        case RO_SCALE: v[0] = rl_gf(), v[1] = rl_gf(); qdb_scale(v[0], v[1]); break;
        case RO_ROTATE: qdb_rotate(rl_gf()); break;
        case RO_ALPHA: qdb_alpha(rl_gf()); break;
        case RO_CLIP:
            for (i = 0; i < 4; i++)
                v[i] = rl_gf();
            qdb_clip_rect(v[0], v[1], v[2], v[3]);
            break;
        case RO_LW: qdb_line_width(rl_gf()); break;
        case RO_CAP: qdb_line_cap((int)rl_gu()); break;
        case RO_JOIN: qdb_line_join((int)rl_gu()); break;
        case RO_DASH: {
            float d[16];
            const int n = (int)rl_gu();
            for (i = 0; i < n; i++)
                d[i] = rl_gf();
            qdb_dash(d, n);
            break;
        }
        case RO_LINE:
            for (i = 0; i < 4; i++)
                v[i] = rl_gf();
            u0 = rl_gu(), v[4] = rl_gf();
            qdb_line(v[0], v[1], v[2], v[3], u0, v[4]);
            break;
        case RO_POLY: {
            const int n = (int)rl_gu(), cl = (int)rl_gu();
            u0 = rl_gu(), v[0] = rl_gf(), u1 = rl_gu();
            qdb_polyline((const float *)(const void *)(rl_buf + rl_at), n, cl, u0, v[0], u1);   /* (aligned: in place) */
            rl_at += (uint32_t)n * 8u;
            break;
        }
        case RO_CIRCLE:
            for (i = 0; i < 3; i++)
                v[i] = rl_gf();
            u0 = rl_gu(), v[3] = rl_gf(), u1 = rl_gu();
            qdb_circle(v[0], v[1], v[2], u0, v[3], u1);
            break;
        case RO_ARC:
            for (i = 0; i < 5; i++)
                v[i] = rl_gf();
            u0 = rl_gu(), v[5] = rl_gf();
            qdb_arc(v[0], v[1], v[2], v[3], v[4], u0, v[5]);
            break;
        case RO_ELL:
            for (i = 0; i < 4; i++)
                v[i] = rl_gf();
            u0 = rl_gu(), v[4] = rl_gf(), u1 = rl_gu();
            qdb_ellipse(v[0], v[1], v[2], v[3], u0, v[4], u1);
            break;
        case RO_ELLARC:
            for (i = 0; i < 6; i++)
                v[i] = rl_gf();
            u0 = rl_gu(), v[6] = rl_gf();
            qdb_ellipse_arc(v[0], v[1], v[2], v[3], v[4], v[5], u0, v[6]);
            break;
        case RO_RRECT:
            for (i = 0; i < 5; i++)
                v[i] = rl_gf();
            u0 = rl_gu(), v[5] = rl_gf(), u1 = rl_gu();
            qdb_rrect(v[0], v[1], v[2], v[3], v[4], u0, v[5], u1);
            break;
        case RO_FRECT:
            for (i = 0; i < 4; i++)
                v[i] = rl_gf();
            qdb_fill_rect(v[0], v[1], v[2], v[3], rl_gu());
            break;
        case RO_PBEGIN:
            v[0] = rl_gf(), v[1] = rl_gf();
#ifdef QDB_ROW0
            skip = v[1] < (float)QDB_ROW0 || v[0] >= (float)QDB_ROW1;   /* (the backend's strip rows) */
#endif
            qdb_path_begin();
            break;
        case RO_MOVE: v[0] = rl_gf(), v[1] = rl_gf(); qdb_move_to(v[0], v[1]); break;
        case RO_LINETO: v[0] = rl_gf(), v[1] = rl_gf(); qdb_line_to(v[0], v[1]); break;
        case RO_QUAD:
            for (i = 0; i < 4; i++)
                v[i] = rl_gf();
            qdb_quad_to(v[0], v[1], v[2], v[3]);
            break;
        case RO_CUBIC:
            for (i = 0; i < 6; i++)
                v[i] = rl_gf();
            qdb_cubic_to(v[0], v[1], v[2], v[3], v[4], v[5]);
            break;
        case RO_PARC:
            for (i = 0; i < 5; i++)
                v[i] = rl_gf();
            qdb_path_arc(v[0], v[1], v[2], v[3], v[4]);
            break;
        case RO_PCLOSE: qdb_path_close(); break;
        case RO_PFILL: qdb_path_fill(rl_gu()); break;
        case RO_PSTROKE: u0 = rl_gu(); qdb_path_stroke(u0, rl_gf()); break;
        case RO_PSTROKEK: u0 = rl_gu(); qdb_path_stroke_keep(u0, rl_gf()); break;
        case RO_PPAINT: u0 = rl_gu(), v[0] = rl_gf(), u1 = rl_gu(); qdb_path_paint(u0, v[0], u1); break;
        case RO_PCLIP: qdb_path_clip(); break;
        case RO_TEXT: {
            qd_font f;
            uint16_t n;
            char s[256];
            v[0] = rl_gf(), v[1] = rl_gf();
            f.size = rl_gf(), f.weight = (int)rl_gu(), f.col = rl_gu(), f.align = (int)rl_gu(), f.ls = rl_gf();
            memcpy(&n, rl_buf + rl_at, 2);
            rl_at += 2;
            memcpy(s, rl_buf + rl_at, n);
            s[n] = 0;
            rl_at += (n + 2u + 3u) / 4u * 4u - 2u;      /* (the string and its pad) */
            qdb_text(s, v[0], v[1], &f);
            break;
        }
        case RO_LDRAW:
            u2 = rl_gu();
            for (i = 0; i < 4; i++)
                v[i] = rl_gf();
            qdb_layer_draw((int)u2, v[0], v[1], v[2], v[3]);
            break;
        default:
            return;                                     /* (never: a list of its own making) */
        }
    }
}

/* ---------------------------------------------------------------- the strips' hashes */
/* Each call that draws: its bytes and the state then (the transform, the alpha, the width, caps and joins, the dash,
 * the clip, the path it paints) hashed into the hash of every strip its box touches on the screen. The box is
 * generous (the transform's corners, the stroke's half width and 2 px more; a text's box at its advance either way
 * and its size above and below; a path's every point, control points too; a clip by a path or an unknown kind: every
 * strip). A strip whose hash is the last frame's draws the same pixels. */
typedef struct {
    float m[6];                         /* x' = m0 x + m2 y + m4, y' = m1 x + m3 y + m5 */
    uint32_t sh;                        /* the alpha, width, caps, joins, dash and clip, hashed */
} rl_st_t;
static uint32_t rl_mix(uint32_t h, const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i;
    for (i = 0; i < n; i++)
        h = (h ^ b[i]) * 0x01000193u;
    return h;
}
static void rl_box(const rl_st_t *st, float x0, float y0, float x1, float y1, float lw, float *ya, float *yb)
{
    const float *m = st->m;
    const float s = (m[0] < 0 ? -m[0] : m[0]) + (m[1] < 0 ? -m[1] : m[1]) + (m[2] < 0 ? -m[2] : m[2]) +
                    (m[3] < 0 ? -m[3] : m[3]);
    const float pad = lw * 0.5f * s + 2.f;
    const float yy[4] = {m[1] * x0 + m[3] * y0 + m[5], m[1] * x1 + m[3] * y0 + m[5], m[1] * x0 + m[3] * y1 + m[5],
                         m[1] * x1 + m[3] * y1 + m[5]};
    int k;
    float a = yy[0], b = yy[0];
    for (k = 1; k < 4; k++) {
        a = yy[k] < a ? yy[k] : a;
        b = yy[k] > b ? yy[k] : b;
    }
    *ya = a - pad, *yb = b + pad;
}
static void rl_into(uint32_t *h, int n, int rows, float ya, float yb, uint32_t v)
{
    if (!h)
        return;                                         /* (rl_index: no hashes) */
    {
    int s0 = ya <= 0.f ? 0 : (int)(ya / (float)rows), s1 = yb <= 0.f ? -1 : (int)(yb / (float)rows), s;
    s0 = s0 < 0 ? 0 : s0;
    s1 = s1 >= n ? n - 1 : s1;
    for (s = s0; s <= s1; s++)
        h[s] = rl_mix(h[s], &v, 4) * 0x9E3779B1u + 1u;
    }
}
/* (h 0: rl_index: each path group's rows (the union of its paints' boxes) written into its begin; a group that clips
 * by its path: every row) */
static uint32_t g_at;
static float g_ya, g_yb;
static int g_ok, g_open;
static void g_close(void)
{
    if (g_open) {
        const float a = g_ok ? g_ya : -1e30f, b = g_ok ? g_yb : 1e30f;
        memcpy(rl_buf + g_at, &a, 4);
        memcpy(rl_buf + g_at + 4, &b, 4);
    }
    g_open = 0;
}
void qd_dl_strip_hashes(uint32_t *h, int n, int rows)
{
    rl_st_t st, stk[32];
    int depth = 0, i, path_any = 0;
    float pya = 1e9f, pyb = -1e9f, ya, yb, v[8], lw = 1.f;
    uint32_t ph = 2166136261u, o0;
    for (i = 0; h && i < n; i++)
        h[i] = 2166136261u;
    g_open = 0;
    memset(&st, 0, sizeof st);
    st.m[0] = st.m[3] = 1.f;
    st.sh = 2166136261u;
    rl_at = 0;
    while (rl_buf && rl_at < rl_len) {
        const uint32_t at0 = rl_at;
        const int op = (int)rl_gu();
        uint32_t ch;                                    /* (this call's bytes and the state) */
#define CALLH() (ch = rl_mix(rl_mix(st.sh, st.m, sizeof st.m), rl_buf + at0, rl_at - at0))
#define PT(x, y)                                                                                                       \
    do {                                                                                                               \
        rl_box(&st, (x), (y), (x), (y), 0.f, &ya, &yb);                                                                \
        pya = ya < pya ? ya : pya, pyb = yb > pyb ? yb : pyb, path_any = 1;                                            \
    } while (0)
        switch (op) {
        case RO_SAVE:
            if (depth < 32)
                stk[depth] = st;
            depth++;
            break;
        case RO_RESTORE:
            if (depth > 0 && --depth < 32)
                st = stk[depth];
            break;
        case RO_TRANSLATE: {
            const float x = rl_gf(), y = rl_gf();
            st.m[4] += st.m[0] * x + st.m[2] * y, st.m[5] += st.m[1] * x + st.m[3] * y;
            break;
        }
        case RO_SCALE: {
            const float sx = rl_gf(), sy = rl_gf();
            st.m[0] *= sx, st.m[1] *= sx, st.m[2] *= sy, st.m[3] *= sy;
            break;
        }
        case RO_ROTATE: {
            const float a = rl_gf(), c = cosf(a), s = sinf(a), m0 = st.m[0], m1 = st.m[1];
            st.m[0] = m0 * c + st.m[2] * s, st.m[1] = m1 * c + st.m[3] * s;
            st.m[2] = st.m[2] * c - m0 * s, st.m[3] = st.m[3] * c - m1 * s;
            break;
        }
        case RO_LW:
            lw = rl_gf();
            st.sh = rl_mix(st.sh, rl_buf + at0, rl_at - at0);
            break;
        case RO_ALPHA: case RO_CAP: case RO_JOIN:
            rl_at += 4;
            st.sh = rl_mix(st.sh, rl_buf + at0, rl_at - at0);
            break;
        case RO_CLIP:
            rl_at += 16;
            st.sh = rl_mix(rl_mix(st.sh, st.m, sizeof st.m), rl_buf + at0, rl_at - at0);
            break;
        case RO_DASH:
            rl_at += 4u + 4u * rl_gu();
            st.sh = rl_mix(st.sh, rl_buf + at0, rl_at - at0);
            break;
        case RO_LINE:
            for (i = 0; i < 4; i++)
                v[i] = rl_gf();
            o0 = rl_gu(), v[4] = rl_gf();
            (void)o0;
            CALLH();
            rl_box(&st, v[0] < v[2] ? v[0] : v[2], v[1] < v[3] ? v[1] : v[3], v[0] > v[2] ? v[0] : v[2],
                   v[1] > v[3] ? v[1] : v[3], v[4] > 0.f ? v[4] : lw, &ya, &yb);
            rl_into(h, n, rows, ya, yb, ch);
            break;
        case RO_POLY: {
            const uint32_t np = rl_gu();
            float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f, w;
            uint32_t k;
            rl_at += 8u;
            w = rl_gf();
            rl_at += 4u;
            for (k = 0; k < np; k++) {
                const float x = rl_gf(), y = rl_gf();
                x0 = x < x0 ? x : x0, x1 = x > x1 ? x : x1, y0 = y < y0 ? y : y0, y1 = y > y1 ? y : y1;
            }
            CALLH();
            if (np) {
                rl_box(&st, x0, y0, x1, y1, w > 0.f ? w : lw, &ya, &yb);
                rl_into(h, n, rows, ya, yb, ch);
            }
            break;
        }
        case RO_CIRCLE: case RO_ARC: {
            const float cx = rl_gf(), cy = rl_gf(), r = rl_gf();
            float w;
            if (op == RO_ARC)
                rl_at += 8u;
            rl_at += 4u;
            w = rl_gf();
            if (op == RO_CIRCLE)
                rl_at += 4u;
            CALLH();
            rl_box(&st, cx - r, cy - r, cx + r, cy + r, w, &ya, &yb);
            rl_into(h, n, rows, ya, yb, ch);
            break;
        }
        case RO_ELL: case RO_ELLARC: {
            const float cx = rl_gf(), cy = rl_gf(), rx = rl_gf(), ry = rl_gf();
            float w;
            if (op == RO_ELLARC)
                rl_at += 8u;
            rl_at += 4u;
            w = rl_gf();
            if (op == RO_ELL)
                rl_at += 4u;
            CALLH();
            rl_box(&st, cx - rx, cy - ry, cx + rx, cy + ry, w, &ya, &yb);
            rl_into(h, n, rows, ya, yb, ch);
            break;
        }
        case RO_RRECT: case RO_FRECT: {
            const float x = rl_gf(), y = rl_gf(), w = rl_gf(), hh = rl_gf();
            float sw = 0.f;
            if (op == RO_RRECT) {
                rl_at += 8u;
                sw = rl_gf();
                rl_at += 4u;
            } else {
                rl_at += 4u;
            }
            CALLH();
            rl_box(&st, x, y, x + w, y + hh, sw, &ya, &yb);
            rl_into(h, n, rows, ya, yb, ch);
            break;
        }
        case RO_PBEGIN:
            if (!h) {
                g_close();
                g_open = 1, g_ok = 1, g_at = rl_at, g_ya = 1e30f, g_yb = -1e30f;
            }
            rl_at += 8u;
            ph = 2166136261u, path_any = 0, pya = 1e9f, pyb = -1e9f;
            break;
        case RO_MOVE: case RO_LINETO: case RO_QUAD: case RO_CUBIC: {
            const int np = op == RO_CUBIC ? 3 : op == RO_QUAD ? 2 : 1;
            for (i = 0; i < np; i++) {
                const float x = rl_gf(), y = rl_gf();
                PT(x, y);
            }
            ph = rl_mix(rl_mix(ph, st.m, sizeof st.m), rl_buf + at0, rl_at - at0);
            break;
        }
        case RO_PARC: {
            const float cx = rl_gf(), cy = rl_gf(), r = rl_gf();
            rl_at += 8u;
            PT(cx - r, cy - r);
            PT(cx + r, cy + r);
            PT(cx - r, cy + r);
            PT(cx + r, cy - r);
            ph = rl_mix(rl_mix(ph, st.m, sizeof st.m), rl_buf + at0, rl_at - at0);
            break;
        }
        case RO_PCLOSE:
            ph = rl_mix(ph, rl_buf + at0, 4);
            break;
        case RO_PFILL: case RO_PSTROKE: case RO_PSTROKEK: case RO_PPAINT: {
            float w = 0.f, s;
            const float *m = st.m;
            if (op == RO_PFILL)
                rl_at += 4u;
            else if (op == RO_PPAINT)
                rl_at += 4u, w = rl_gf(), rl_at += 4u;
            else
                rl_at += 4u, w = rl_gf();
            w = op != RO_PFILL && w <= 0.f ? lw : w;
            s = (m[0] < 0 ? -m[0] : m[0]) + (m[1] < 0 ? -m[1] : m[1]) + (m[2] < 0 ? -m[2] : m[2]) +
                (m[3] < 0 ? -m[3] : m[3]);
            ch = rl_mix(rl_mix(st.sh, &ph, 4), rl_buf + at0, rl_at - at0);
            if (path_any)
                rl_into(h, n, rows, pya - w * 0.5f * s - 2.f, pyb + w * 0.5f * s + 2.f, ch);
            if (path_any && !h) {
                g_ya = pya - w * 0.5f * s - 2.f < g_ya ? pya - w * 0.5f * s - 2.f : g_ya;
                g_yb = pyb + w * 0.5f * s + 2.f > g_yb ? pyb + w * 0.5f * s + 2.f : g_yb;
            }
            if (op != RO_PSTROKEK)
                ph = 2166136261u, path_any = 0, pya = 1e9f, pyb = -1e9f;
            break;
        }
        case RO_PCLIP:
            g_ok = 0;
            st.sh = rl_mix(st.sh, &ph, 4);              /* (the clip's path: in the state of what follows) */
            ph = 2166136261u, path_any = 0, pya = 1e9f, pyb = -1e9f;
            break;
        case RO_TEXT: {
            qd_font f;
            uint16_t nn;
            char s[256];
            float w;
            v[0] = rl_gf(), v[1] = rl_gf();
            f.size = rl_gf(), f.weight = (int)rl_gu(), f.col = rl_gu(), f.align = (int)rl_gu(), f.ls = rl_gf();
            memcpy(&nn, rl_buf + rl_at, 2);
            memcpy(s, rl_buf + rl_at + 2, nn);
            s[nn] = 0;
            rl_at += (2u + nn + 3u) / 4u * 4u;
            w = qd_text_width(s, &f) + f.size;
            CALLH();
            rl_box(&st, v[0] - w, v[1] - 1.5f * f.size, v[0] + w, v[1] + 0.75f * f.size, 0.f, &ya, &yb);
            rl_into(h, n, rows, ya, yb, ch);
            break;
        }
        case RO_LDRAW: {
            float x, y, w, hh;
            rl_at += 4u;
            x = rl_gf(), y = rl_gf(), w = rl_gf(), hh = rl_gf();
            CALLH();
            rl_box(&st, x, y, x + w, y + hh, 0.f, &ya, &yb);
            rl_into(h, n, rows, ya, yb, ch);
            break;
        }
        default:
            for (i = 0; h && i < n; i++)                /* (never: unknown: every strip) */
                h[i] ^= 0xFFFFFFFFu;
            g_ok = 0, g_close();
            return;
        }
#undef CALLH
#undef PT
    }
    if (!h)
        g_close();
}
