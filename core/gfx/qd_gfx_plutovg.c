/* SPDX-License-Identifier: GPL-3.0-only */
/* qd_gfx on plutovg (third_party/plutovg, MIT; Phase 1). One 240 x 240 ARGB surface and one canvas, the fonts
 * embedded (fonts_gen.h: Inter Tight's static instances, made by tools/make_fonts.py). What plutovg has no call for
 * is built here on top of it, without touching it:
 *   - weights: one face per static instance, the nearest one chosen;
 *   - letter spacing and alignment: the text is laid out glyph by glyph (advance + spacing), as the canvas does;
 *   - the ellipse's arc: a unit arc path, transformed;
 *   - RGB565 big-endian out of the premultiplied ARGB surface.
 * Nothing here calls the OS: the surface and the paths are malloc'ed once by qd_init. */
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"

static plutovg_surface_t *qd_surf;
static plutovg_canvas_t *qd_cv;
static plutovg_surface_t *qd_lsurf[QD_NLAYER];
static plutovg_canvas_t *qd_lcv[QD_NLAYER];
static plutovg_path_t *qd_tmp;                 /* the ellipse arc's path */
static plutovg_font_face_t *qd_face[QD_FONTS_GEN_N];
static float qd_lw = 1.f;                      /* the state's default width (qd_path_stroke): saved with the state */
static float qd_lw_stack[64];
static int qd_depth;
static const float qd_nodash[1] = {0.f};     /* (an empty dash array: solid; never a NULL to memcpy) */

int qd_init(void)
{
    int i;
    if (qd_cv)
        return 1;
    qd_surf = plutovg_surface_create(QD_W, QD_H);
    if (!qd_surf)
        return 0;
    qd_cv = plutovg_canvas_create(qd_surf);
    qd_tmp = plutovg_path_create();
    for (i = 0; i < QD_FONTS_GEN_N; i++)
        qd_face[i] = plutovg_font_face_load_from_data(qd_fonts_gen[i].data, qd_fonts_gen[i].len, 0, NULL, NULL);
    for (i = 0; i < QD_FONTS_GEN_N; i++)
        if (!qd_face[i])
            return 0;
    return qd_cv && qd_tmp;
}

void qd_shutdown(void)
{
    int i;
    for (i = 0; i < QD_FONTS_GEN_N; i++) {
        if (qd_face[i])
            plutovg_font_face_destroy(qd_face[i]);
        qd_face[i] = NULL;
    }
    for (i = 0; i < QD_NLAYER; i++) {                   /* (0.9.2: the layers) */
        if (qd_lcv[i])
            plutovg_canvas_destroy(qd_lcv[i]);
        if (qd_lsurf[i])
            plutovg_surface_destroy(qd_lsurf[i]);
        qd_lcv[i] = NULL, qd_lsurf[i] = NULL;
    }
    if (qd_tmp)
        plutovg_path_destroy(qd_tmp);
    if (qd_cv)
        plutovg_canvas_destroy(qd_cv);
    if (qd_surf)
        plutovg_surface_destroy(qd_surf);
    qd_tmp = NULL;
    qd_cv = NULL;
    qd_surf = NULL;
}

static void qd_color(qd_rgb c)
{
    plutovg_canvas_set_rgb(qd_cv, (float)((c >> 16) & 255u) / 255.f, (float)((c >> 8) & 255u) / 255.f,
                           (float)(c & 255u) / 255.f);
}

void qd_begin(void)
{
    static const plutovg_color_t black = {0.f, 0.f, 0.f, 1.f};
    if (!qd_cv && !qd_init())
        return;
    while (qd_depth > 0)                       /* (a screen that forgot a restore: the stack starts empty) */
        qd_restore();
    plutovg_canvas_reset_matrix(qd_cv);
    plutovg_canvas_new_path(qd_cv);
    plutovg_surface_clear(qd_surf, &black);
    plutovg_canvas_set_opacity(qd_cv, 1.f);
    plutovg_canvas_set_line_width(qd_cv, 1.f);
    plutovg_canvas_set_line_cap(qd_cv, PLUTOVG_LINE_CAP_ROUND);
    plutovg_canvas_set_line_join(qd_cv, PLUTOVG_LINE_JOIN_ROUND);
    plutovg_canvas_set_dash_array(qd_cv, qd_nodash, 0);
    plutovg_canvas_set_fill_rule(qd_cv, PLUTOVG_FILL_RULE_NON_ZERO);
    plutovg_canvas_set_operator(qd_cv, PLUTOVG_OPERATOR_SRC_OVER);
    qd_lw = 1.f;
}

void qd_end(uint16_t *lcd_be)
{
    const unsigned char *d;
    int stride, x, y;
    if (!qd_cv || !lcd_be)
        return;
    d = plutovg_surface_get_data(qd_surf);
    stride = plutovg_surface_get_stride(qd_surf);
    for (y = 0; y < QD_H; y++) {
        const uint32_t *row = (const uint32_t *)(const void *)(d + (size_t)y * (size_t)stride);
        for (x = 0; x < QD_W; x++) {
            /* premultiplied ARGB over an opaque black clear: alpha is 255 everywhere, the channels are the colour */
            const uint32_t p = row[x];
            const uint32_t r = (p >> 16) & 255u, g = (p >> 8) & 255u, b = p & 255u;
            const uint16_t c = (uint16_t)((((r * 31u + 127u) / 255u) << 11) | (((g * 63u + 127u) / 255u) << 5) |
                                          ((b * 31u + 127u) / 255u));
            lcd_be[y * QD_W + x] = (uint16_t)((c >> 8) | (c << 8));
        }
    }
}

/* ---------------------------------------------------------------- the state --- */
void qd_save(void)
{
    if (!qd_cv)
        return;
    plutovg_canvas_save(qd_cv);
    if (qd_depth < (int)(sizeof qd_lw_stack / sizeof qd_lw_stack[0]))
        qd_lw_stack[qd_depth] = qd_lw;
    qd_depth++;
}
void qd_restore(void)
{
    if (!qd_cv || qd_depth <= 0)
        return;
    plutovg_canvas_restore(qd_cv);
    qd_depth--;
    if (qd_depth < (int)(sizeof qd_lw_stack / sizeof qd_lw_stack[0]))
        qd_lw = qd_lw_stack[qd_depth];
}
void qd_translate(float x, float y) { plutovg_canvas_translate(qd_cv, x, y); }
void qd_scale(float sx, float sy) { plutovg_canvas_scale(qd_cv, sx, sy); }
void qd_rotate(float a) { plutovg_canvas_rotate(qd_cv, a); }
void qd_alpha(float a) { plutovg_canvas_set_opacity(qd_cv, a); }
float qd_get_alpha(void) { return qd_cv ? plutovg_canvas_get_opacity(qd_cv) : 1.f; }
void qd_clip_rect(float x, float y, float w, float h) { plutovg_canvas_clip_rect(qd_cv, x, y, w, h); }
void qd_line_width(float w) { qd_lw = w; }
void qd_line_cap(int cap)
{
    plutovg_canvas_set_line_cap(qd_cv, cap == QD_CAP_BUTT     ? PLUTOVG_LINE_CAP_BUTT
                                       : cap == QD_CAP_SQUARE ? PLUTOVG_LINE_CAP_SQUARE
                                                              : PLUTOVG_LINE_CAP_ROUND);
}
void qd_line_join(int join)
{
    plutovg_canvas_set_line_join(qd_cv, join == QD_JOIN_MITER   ? PLUTOVG_LINE_JOIN_MITER
                                        : join == QD_JOIN_BEVEL ? PLUTOVG_LINE_JOIN_BEVEL
                                                                : PLUTOVG_LINE_JOIN_ROUND);
}
void qd_dash(const float *on_off, int n) { plutovg_canvas_set_dash(qd_cv, 0.f, n > 0 && on_off ? on_off : qd_nodash, n > 0 && on_off ? n : 0); }

/* the current path: filled (if fill), then stroked (if stroke), then cleared */
static void qd_paint(qd_rgb stroke, float lw, qd_rgb fill)
{
    if (fill != QD_NONE) {
        qd_color(fill);
        plutovg_canvas_fill_preserve(qd_cv);
    }
    if (stroke != QD_NONE && lw > 0.f) {
        qd_color(stroke);
        plutovg_canvas_set_line_width(qd_cv, lw);
        plutovg_canvas_stroke_preserve(qd_cv);
    }
    plutovg_canvas_new_path(qd_cv);
}

/* ---------------------------------------------------------------- shapes --- */
void qd_line(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    if (!qd_cv)
        return;
    plutovg_canvas_new_path(qd_cv);
    plutovg_canvas_move_to(qd_cv, x0, y0);
    plutovg_canvas_line_to(qd_cv, x1, y1);
    qd_paint(col, lw, QD_NONE);
}

void qd_polyline(const float *xy, int npts, int closed, qd_rgb stroke, float lw, qd_rgb fill)
{
    int i;
    if (!qd_cv || !xy || npts < 1)
        return;
    plutovg_canvas_new_path(qd_cv);
    plutovg_canvas_move_to(qd_cv, xy[0], xy[1]);
    for (i = 1; i < npts; i++)
        plutovg_canvas_line_to(qd_cv, xy[2 * i], xy[2 * i + 1]);
    if (closed)
        plutovg_canvas_close_path(qd_cv);
    qd_paint(stroke, lw, fill);
}

void qd_circle(float cx, float cy, float r, qd_rgb stroke, float lw, qd_rgb fill)
{
    if (!qd_cv || r <= 0.f)
        return;
    plutovg_canvas_new_path(qd_cv);
    plutovg_canvas_arc(qd_cv, cx, cy, r, 0.f, 6.28318531f, false);
    plutovg_canvas_close_path(qd_cv);
    qd_paint(stroke, lw, fill);
}

void qd_arc(float cx, float cy, float r, float a0, float a1, qd_rgb stroke, float lw)
{
    if (!qd_cv || r <= 0.f)
        return;
    plutovg_canvas_new_path(qd_cv);
    plutovg_canvas_arc(qd_cv, cx, cy, r, a0, a1, false);
    qd_paint(stroke, lw, QD_NONE);
}

void qd_ellipse(float cx, float cy, float rx, float ry, qd_rgb stroke, float lw, qd_rgb fill)
{
    if (!qd_cv || rx <= 0.f || ry <= 0.f)
        return;
    plutovg_canvas_new_path(qd_cv);
    plutovg_canvas_ellipse(qd_cv, cx, cy, rx, ry);
    qd_paint(stroke, lw, fill);
}

void qd_ellipse_arc(float cx, float cy, float rx, float ry, float a0, float a1, qd_rgb stroke, float lw)
{
    plutovg_matrix_t m;
    if (!qd_cv || rx <= 0.f || ry <= 0.f)
        return;
    /* the unit circle's arc, scaled to the radii and moved to the centre (the stroke keeps the canvas's width) */
    plutovg_path_reset(qd_tmp);
    plutovg_path_add_arc(qd_tmp, 0.f, 0.f, 1.f, a0, a1, false);
    plutovg_matrix_init(&m, rx, 0.f, 0.f, ry, cx, cy);
    plutovg_path_transform(qd_tmp, &m);
    plutovg_canvas_new_path(qd_cv);
    plutovg_canvas_add_path(qd_cv, qd_tmp);
    qd_paint(stroke, lw, QD_NONE);
}

void qd_rrect(float x, float y, float w, float h, float r, qd_rgb stroke, float lw, qd_rgb fill)
{
    if (!qd_cv || w <= 0.f || h <= 0.f)
        return;
    if (r > w / 2.f)
        r = w / 2.f;
    if (r > h / 2.f)
        r = h / 2.f;
    if (r < 0.f)
        r = 0.f;
    plutovg_canvas_new_path(qd_cv);
    plutovg_canvas_round_rect(qd_cv, x, y, w, h, r, r);
    qd_paint(stroke, lw, fill);
}

void qd_fill_rect(float x, float y, float w, float h, qd_rgb col)
{
    if (!qd_cv || w <= 0.f || h <= 0.f)
        return;
    plutovg_canvas_new_path(qd_cv);
    plutovg_canvas_rect(qd_cv, x, y, w, h);
    qd_paint(QD_NONE, 0.f, col);
}

/* ---------------------------------------------------------------- the path --- */
void qd_path_begin(void) { plutovg_canvas_new_path(qd_cv); }
void qd_move_to(float x, float y) { plutovg_canvas_move_to(qd_cv, x, y); }
void qd_line_to(float x, float y) { plutovg_canvas_line_to(qd_cv, x, y); }
void qd_quad_to(float cx, float cy, float x, float y) { plutovg_canvas_quad_to(qd_cv, cx, cy, x, y); }
void qd_cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y)
{
    plutovg_canvas_cubic_to(qd_cv, c1x, c1y, c2x, c2y, x, y);
}
void qd_path_arc(float cx, float cy, float r, float a0, float a1) { plutovg_canvas_arc(qd_cv, cx, cy, r, a0, a1, false); }
void qd_path_close(void) { plutovg_canvas_close_path(qd_cv); }
void qd_path_fill(qd_rgb col) { qd_paint(QD_NONE, 0.f, col); }
void qd_path_stroke(qd_rgb col, float lw) { qd_paint(col, lw > 0.f ? lw : qd_lw, QD_NONE); }
void qd_path_stroke_keep(qd_rgb col, float lw)
{
    if (col == QD_NONE)
        return;
    qd_color(col);
    plutovg_canvas_set_line_width(qd_cv, lw > 0.f ? lw : qd_lw);
    plutovg_canvas_stroke_preserve(qd_cv);
}
void qd_path_paint(qd_rgb stroke, float lw, qd_rgb fill) { qd_paint(stroke, lw > 0.f ? lw : qd_lw, fill); }
void qd_path_clip(void) { plutovg_canvas_clip(qd_cv); }

/* ---------------------------------------------------------------- 0.9.2: layers --- */
static plutovg_canvas_t *qd_main_cv;                  /* (the screen's, while a layer is drawn) */
static int qd_lw_main, qd_lw_h[QD_NLAYER], qd_luse[QD_NLAYER][2];   /* (its surface's height; the part in use) */
static float qd_lw_saved;

int qd_layer_begin(int slot, int w, int h)
{
    static const plutovg_color_t clear = {0.f, 0.f, 0.f, 0.f};
    const int uw = w, uh = h;
    if (!qd_cv || qd_main_cv || slot < 0 || slot >= QD_NLAYER || w < 1 || h < 1)
        return 0;
    if (!qd_lsurf[slot] || plutovg_surface_get_width(qd_lsurf[slot]) < w || qd_lw_h[slot] < h) {
        if (qd_lsurf[slot]) {                           /* (grown in both: the largest asked for, made once) */
            const int ow = plutovg_surface_get_width(qd_lsurf[slot]);
            w = w > ow ? w : ow, h = h > qd_lw_h[slot] ? h : qd_lw_h[slot];
        }
        if (qd_lcv[slot])
            plutovg_canvas_destroy(qd_lcv[slot]);
        if (qd_lsurf[slot])
            plutovg_surface_destroy(qd_lsurf[slot]);
        qd_lcv[slot] = NULL;
        qd_lsurf[slot] = plutovg_surface_create(w, h);
        if (!qd_lsurf[slot])
            return 0;
        qd_lcv[slot] = plutovg_canvas_create(qd_lsurf[slot]);
        if (!qd_lcv[slot])
            return 0;
        qd_lw_h[slot] = h;
    }
    plutovg_surface_clear(qd_lsurf[slot], &clear);
    qd_luse[slot][0] = uw, qd_luse[slot][1] = uh;
    return qd_layer_resume(slot);
}

int qd_layer_resume(int slot)
{
    if (!qd_cv || qd_main_cv || slot < 0 || slot >= QD_NLAYER || !qd_lcv[slot])
        return 0;
    qd_main_cv = qd_cv;
    qd_cv = qd_lcv[slot];
    qd_lw_main = qd_depth;
    qd_lw_saved = qd_lw;
    plutovg_canvas_reset_matrix(qd_cv);
    plutovg_canvas_new_path(qd_cv);
    plutovg_canvas_set_opacity(qd_cv, 1.f);
    plutovg_canvas_set_line_cap(qd_cv, PLUTOVG_LINE_CAP_ROUND);
    plutovg_canvas_set_line_join(qd_cv, PLUTOVG_LINE_JOIN_ROUND);
    plutovg_canvas_set_dash_array(qd_cv, qd_nodash, 0);
    plutovg_canvas_set_operator(qd_cv, PLUTOVG_OPERATOR_SRC_OVER);
    return 1;
}

void qd_layer_end(void)
{
    if (!qd_main_cv)
        return;
    while (qd_depth > qd_lw_main)                       /* (the layer's own saves undone) */
        qd_restore();
    qd_cv = qd_main_cv;
    qd_main_cv = NULL;
    qd_lw = qd_lw_saved;
}

void qd_layer_draw(int slot, float x, float y, float w, float h)
{
    plutovg_matrix_t m, ctm;
    if (!qd_cv || slot < 0 || slot >= QD_NLAYER || !qd_lsurf[slot])
        return;
    plutovg_canvas_get_matrix(qd_cv, &ctm);
    if (w == (float)qd_luse[slot][0] && h == (float)qd_luse[slot][1] && ctm.a == 1.f && ctm.b == 0.f && ctm.c == 0.f &&
        ctm.d == 1.f) {                                 /* (unscaled, unturned: placed as it is, a pixel a pixel) */
        plutovg_matrix_init_translate(&m, x, y);
        plutovg_canvas_set_texture(qd_cv, qd_lsurf[slot], PLUTOVG_TEXTURE_TYPE_PLAIN, 1.f, &m);
        plutovg_canvas_new_path(qd_cv);
        plutovg_canvas_rect(qd_cv, x, y, w, h);
        plutovg_canvas_fill(qd_cv);
        return;
    }
    /* (plutovg samples a texture bilinearly only when tiled and turned: a turn of a millionth of a degree and the
     * layer's margin left clear, so the tiling never shows; else a scaled layer is blocky) */
    plutovg_matrix_init_translate(&m, x, y);
    plutovg_matrix_rotate(&m, 2e-5f);
    plutovg_matrix_scale(&m, w / (float)qd_luse[slot][0], h / (float)qd_luse[slot][1]);
    plutovg_canvas_set_texture(qd_cv, qd_lsurf[slot], PLUTOVG_TEXTURE_TYPE_TILED, 1.f, &m);
    plutovg_canvas_new_path(qd_cv);
    plutovg_canvas_rect(qd_cv, x, y, w, h);
    plutovg_canvas_fill(qd_cv);
}

/* ---------------------------------------------------------------- text --- */
static plutovg_font_face_t *qd_face_for(int weight)
{
    int i, best = 0, bd = 1 << 30;
    for (i = 0; i < QD_FONTS_GEN_N; i++) {
        const int d = qd_fonts_gen[i].weight > weight ? qd_fonts_gen[i].weight - weight : weight - qd_fonts_gen[i].weight;
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return qd_face[best];
}

/* the width as the canvas measures it: every glyph's advance plus the letter spacing after each one */
float qd_text_width(const char *utf8, const qd_font *f)
{
    plutovg_text_iterator_t it;
    plutovg_font_face_t *face;
    float w = 0.f;
    if (!qd_cv || !utf8 || !f)
        return 0.f;
    face = qd_face_for(f->weight);
    plutovg_text_iterator_init(&it, utf8, -1, PLUTOVG_TEXT_ENCODING_UTF8);
    while (plutovg_text_iterator_has_next(&it)) {
        float adv = 0.f;
        plutovg_font_face_get_glyph_metrics(face, f->size, plutovg_text_iterator_next(&it), &adv, NULL, NULL);
        w += adv + f->ls;
    }
    return w;
}

int qd_text_mask(const char *utf8, float x, float y, const qd_font *f, uint8_t *mask, int w, int h)
{
    plutovg_text_iterator_t it;
    plutovg_surface_t *s;
    plutovg_canvas_t *c;
    const unsigned char *d;
    float pen;
    int i, j, stride;
    if (!qd_cv || !utf8 || !f || !mask || w <= 0 || h <= 0)
        return 0;
    s = plutovg_surface_create(w, h);
    c = s ? plutovg_canvas_create(s) : NULL;
    if (!c) {
        if (s)
            plutovg_surface_destroy(s);
        return 0;
    }
    pen = qd_text_width(utf8, f);
    pen = f->align == QD_CENTER ? x - pen / 2.f : f->align == QD_RIGHT ? x - pen : x;
    plutovg_canvas_set_font(c, qd_face_for(f->weight), f->size);
    plutovg_text_iterator_init(&it, utf8, -1, PLUTOVG_TEXT_ENCODING_UTF8);
    while (plutovg_text_iterator_has_next(&it))
        pen += plutovg_canvas_add_glyph(c, plutovg_text_iterator_next(&it), pen, y) + f->ls;
    plutovg_canvas_set_rgba(c, 1.f, 1.f, 1.f, 1.f);
    plutovg_canvas_fill(c);
    d = plutovg_surface_get_data(s);
    stride = plutovg_surface_get_stride(s);
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            uint32_t px;
            memcpy(&px, d + (size_t)j * (size_t)stride + (size_t)i * 4u, 4);
            mask[j * w + i] = (uint8_t)(px >> 24);      /* premultiplied ARGB: the alpha */
        }
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
    return 1;
}

static float qd_tmin = 1e9f;
static char qd_tmin_s[32];
void qd_text_stats_reset(void)
{
    qd_tmin = 1e9f;
    qd_tmin_s[0] = 0;
}
float qd_text_stats_min(const char **text)
{
    if (text)
        *text = qd_tmin_s;
    return qd_tmin;
}
static void qd_text_stat(const char *utf8, float size)
{
    plutovg_matrix_t m;
    float sz;
    int i;
    if (!utf8[0])
        return;
    plutovg_canvas_get_matrix(qd_cv, &m);
    sz = size * sqrtf(m.c * m.c + m.d * m.d);
    if (sz >= qd_tmin)
        return;
    qd_tmin = sz;
    for (i = 0; utf8[i] && i < 31; i++)
        qd_tmin_s[i] = utf8[i];
    qd_tmin_s[i] = 0;
}

float qd_text(const char *utf8, float x, float y, const qd_font *f)
{
    plutovg_text_iterator_t it;
    plutovg_font_face_t *face;
    float w, pen;
    if (!qd_cv || !utf8 || !f || f->size <= 0.f)
        return 0.f;
    face = qd_face_for(f->weight);
    qd_text_stat(utf8, f->size);
    w = qd_text_width(utf8, f);
    pen = f->align == QD_CENTER ? x - w / 2.f : f->align == QD_RIGHT ? x - w : x;
    plutovg_canvas_new_path(qd_cv);
    plutovg_canvas_set_font(qd_cv, face, f->size);
    plutovg_text_iterator_init(&it, utf8, -1, PLUTOVG_TEXT_ENCODING_UTF8);
    while (plutovg_text_iterator_has_next(&it))
        pen += plutovg_canvas_add_glyph(qd_cv, plutovg_text_iterator_next(&it), pen, y) + f->ls;
    qd_paint(QD_NONE, 0.f, f->col);
    return w;
}
