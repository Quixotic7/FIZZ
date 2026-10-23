/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* qd_gfx drawing nothing (tools/bench_gfx.c's floor: what the screens' own code costs a frame, the strips' repeats
 * included). The text's width is a fixed 0.55 em a character, so the layouts run as they would. */
#include <stddef.h>

#include "qd_gfx.h"
#include "../core/qd_config.h"
#ifdef QD_DEVICE
#include "../core/gfx/qd_arena.c"
#endif

static int y0s, sh = QD_STRIP_H;
static float al = 1.f, alst[64];
static int dep;
int qd_init(void) { return 1; }
void qd_shutdown(void) {}
static uint32_t hsh;
void qd_hash_begin(void) { hsh++; }
uint32_t qd_hash_end(void) { return hsh; }
void qd_begin(void) { qd_begin_strip(0); }
void qd_end(uint16_t *lcd_be) { (void)lcd_be; }
void qd_begin_strip(int y0)
{
    y0s = y0;
    al = 1.f, dep = 0;
}
void qd_end_strip(uint16_t *lcd_be) { (void)lcd_be; }
const uint16_t *qd_end_strip_rows(uint16_t *rows_be) { return rows_be; }
int qd_strip_rows(void) { return sh; }
void qd_set_strip_rows(int h) { sh = h < 1 ? 1 : h > QD_STRIP_H ? QD_STRIP_H : h; }
int qd_box_visible(float x, float y, float w, float h, float lw)
{
    (void)x, (void)w, (void)lw;
    return y < (float)(y0s + sh) && y + h > (float)y0s;
}
void qd_save(void)
{
    if (dep < 64)
        alst[dep] = al;
    dep++;
}
void qd_restore(void)
{
    if (dep > 0 && --dep < 64)
        al = alst[dep];
}
void qd_translate(float x, float y) { (void)x, (void)y; }
void qd_scale(float sx, float sy) { (void)sx, (void)sy; }
void qd_rotate(float a) { (void)a; }
void qd_alpha(float a) { al = a; }
float qd_get_alpha(void) { return al; }
void qd_clip_rect(float x, float y, float w, float h) { (void)x, (void)y, (void)w, (void)h; }
void qd_line_width(float w) { (void)w; }
void qd_line_cap(int cap) { (void)cap; }
void qd_line_join(int join) { (void)join; }
void qd_dash(const float *on_off, int n) { (void)on_off, (void)n; }
void qd_line(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    (void)x0, (void)y0, (void)x1, (void)y1, (void)col, (void)lw;
}
void qd_polyline(const float *xy, int npts, int closed, qd_rgb stroke, float lw, qd_rgb fill)
{
    (void)xy, (void)npts, (void)closed, (void)stroke, (void)lw, (void)fill;
}
void qd_circle(float cx, float cy, float r, qd_rgb stroke, float lw, qd_rgb fill)
{
    (void)cx, (void)cy, (void)r, (void)stroke, (void)lw, (void)fill;
}
void qd_arc(float cx, float cy, float r, float a0, float a1, qd_rgb stroke, float lw)
{
    (void)cx, (void)cy, (void)r, (void)a0, (void)a1, (void)stroke, (void)lw;
}
void qd_ellipse(float cx, float cy, float rx, float ry, qd_rgb stroke, float lw, qd_rgb fill)
{
    (void)cx, (void)cy, (void)rx, (void)ry, (void)stroke, (void)lw, (void)fill;
}
void qd_ellipse_arc(float cx, float cy, float rx, float ry, float a0, float a1, qd_rgb stroke, float lw)
{
    (void)cx, (void)cy, (void)rx, (void)ry, (void)a0, (void)a1, (void)stroke, (void)lw;
}
void qd_rrect(float x, float y, float w, float h, float r, qd_rgb stroke, float lw, qd_rgb fill)
{
    (void)x, (void)y, (void)w, (void)h, (void)r, (void)stroke, (void)lw, (void)fill;
}
void qd_fill_rect(float x, float y, float w, float h, qd_rgb col) { (void)x, (void)y, (void)w, (void)h, (void)col; }
void qd_path_begin(void) {}
void qd_move_to(float x, float y) { (void)x, (void)y; }
void qd_line_to(float x, float y) { (void)x, (void)y; }
void qd_quad_to(float cx, float cy, float x, float y) { (void)cx, (void)cy, (void)x, (void)y; }
void qd_cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y)
{
    (void)c1x, (void)c1y, (void)c2x, (void)c2y, (void)x, (void)y;
}
void qd_path_arc(float cx, float cy, float r, float a0, float a1) { (void)cx, (void)cy, (void)r, (void)a0, (void)a1; }
void qd_path_close(void) {}
void qd_path_fill(qd_rgb col) { (void)col; }
void qd_path_stroke(qd_rgb col, float lw) { (void)col, (void)lw; }
void qd_path_stroke_keep(qd_rgb col, float lw) { (void)col, (void)lw; }
void qd_path_paint(qd_rgb stroke, float lw, qd_rgb fill) { (void)stroke, (void)lw, (void)fill; }
void qd_path_clip(void) {}
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
float qd_text_width(const char *utf8, const qd_font *f)
{
    float w = 0.f;
    if (!utf8 || !f)
        return 0.f;
    for (; *utf8; utf8++)
        if ((*utf8 & 0xC0) != 0x80)
            w += 0.55f * f->size + f->ls;
    return w;
}
float qd_text(const char *utf8, float x, float y, const qd_font *f)
{
    (void)x, (void)y;
    return qd_text_width(utf8, f);
}
void qd_text_stats_reset(void) {}
float qd_text_stats_min(const char **text)
{
    if (text)
        *text = "";
    return 1e9f;
}
int qd_text_mask(const char *utf8, float x, float y, const qd_font *f, uint8_t *mask, int w, int h)
{
    (void)utf8, (void)x, (void)y, (void)f, (void)mask, (void)w, (void)h;
    return 0;
}
int qd_text_mask_rows(const char *utf8, float x, float y, const qd_font *f, uint8_t *mask, int w, int row0, int rows)
{
    (void)utf8, (void)x, (void)y, (void)f, (void)mask, (void)w, (void)row0, (void)rows;
    return 0;
}
