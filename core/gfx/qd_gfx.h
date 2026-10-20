/* SPDX-License-Identifier: GPL-3.0-only */
/* qd_gfx: the drawing seam (PLAN.md §13). Every screen draws through this and nothing else, so Phase 2 replaces only
 * its implementation (the device's own anti-aliased primitives) and keeps every screen.
 *
 * What it covers is what mockups/line.html's helpers use: lines and polylines (open or closed, an optional fill), a
 * small path (for curves), circles, arcs, ellipses and their arcs, rounded rectangles, filled rectangles; the line
 * width, round (or butt / square) caps and joins, a dash pattern; global alpha, a clip rectangle (or path), save and
 * restore, translate and scale; text at a baseline in Inter Tight at a size in px and a weight 200..700, aligned
 * left / center / right, with letter spacing, and its width.
 *
 * The model is the HTML canvas's, simplified:
 *   - One target: the 240 x 240 panel. qd_begin() clears it to black and resets the state; the drawing calls follow;
 *     qd_end(lcd) writes the frame as RGB565 big-endian (the panel's own byte order, fm1panel_t.lcd).
 *   - Coordinates are floats in pixels, (0, 0) the top left pixel's corner (a 1 px line at y = 10.5 covers row 10).
 *     Angles are radians, 0 along +x, growing clockwise on the screen (the canvas's).
 *   - Colours are 0xRRGGBB. QD_NONE means "not drawn" where a call takes a stroke and a fill.
 *   - The state (transform, alpha, clip, line width, caps, joins, dash) is saved and restored as a stack; each call
 *     that strokes takes its own line width, the state's width is only the default for qd_path_stroke.
 *   - Strokes are centred on the geometry. A stroke and a fill in one call: the fill first, then the stroke.
 * Single-threaded, one context per process (per loaded core): the caller's clock thread. No allocation per frame
 * beyond the implementation's own path; nothing here calls the OS. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QD_W 240
#define QD_H 240
#define QD_NONE 0xFFFFFFFFu     /* no stroke / no fill */

typedef uint32_t qd_rgb;        /* 0xRRGGBB */

enum { QD_CAP_ROUND, QD_CAP_BUTT, QD_CAP_SQUARE };
enum { QD_JOIN_ROUND, QD_JOIN_MITER, QD_JOIN_BEVEL };
enum { QD_LEFT, QD_CENTER, QD_RIGHT };

/* ---- the frame */
int qd_init(void);              /* once (idempotent): the surface, the fonts; 0 on failure */
void qd_shutdown(void);
void qd_begin(void);            /* clear to black, reset the state (identity, alpha 1, no clip, width 1, round, no dash) */
void qd_end(uint16_t *lcd_be);  /* the frame out: QD_W * QD_H RGB565, big-endian */

/* ---- the state */
void qd_save(void);
void qd_restore(void);
void qd_translate(float x, float y);
void qd_scale(float sx, float sy);
void qd_alpha(float a);         /* global alpha 0..1 (absolute, as the canvas's globalAlpha) */
void qd_clip_rect(float x, float y, float w, float h);   /* intersects the clip */
void qd_line_width(float w);    /* the default width for qd_path_stroke */
void qd_line_cap(int cap);      /* QD_CAP_*; default round */
void qd_line_join(int join);    /* QD_JOIN_*; default round */
void qd_dash(const float *on_off, int n);   /* the dash pattern (lengths in px, on, off, ..); n = 0: solid */

/* ---- shapes (stroke / fill QD_NONE: not drawn) */
void qd_line(float x0, float y0, float x1, float y1, qd_rgb col, float lw);
void qd_polyline(const float *xy, int npts, int closed, qd_rgb stroke, float lw, qd_rgb fill);
void qd_circle(float cx, float cy, float r, qd_rgb stroke, float lw, qd_rgb fill);
void qd_arc(float cx, float cy, float r, float a0, float a1, qd_rgb stroke, float lw);   /* a0 -> a1 clockwise */
void qd_ellipse(float cx, float cy, float rx, float ry, qd_rgb stroke, float lw, qd_rgb fill);
void qd_ellipse_arc(float cx, float cy, float rx, float ry, float a0, float a1, qd_rgb stroke, float lw);
void qd_rrect(float x, float y, float w, float h, float r, qd_rgb stroke, float lw, qd_rgb fill);
void qd_fill_rect(float x, float y, float w, float h, qd_rgb col);

/* ---- a path, for what the shapes do not cover (curves, a clip shape) */
void qd_path_begin(void);
void qd_move_to(float x, float y);
void qd_line_to(float x, float y);
void qd_quad_to(float cx, float cy, float x, float y);
void qd_cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y);
void qd_path_arc(float cx, float cy, float r, float a0, float a1);   /* joined to the path's current point */
void qd_path_close(void);
void qd_path_fill(qd_rgb col);
void qd_path_stroke(qd_rgb col, float lw);   /* lw <= 0: the state's width */
void qd_path_clip(void);        /* intersects the clip with the path (and ends it) */

/* ---- text: Inter Tight, at a baseline */
typedef struct {
    float size;                 /* px (the canvas font size) */
    int weight;                 /* 200 .. 700 (the nearest instance: 200, 300, 400, 500, 600, 700) */
    qd_rgb col;
    int align;                  /* QD_LEFT, QD_CENTER, QD_RIGHT */
    float ls;                   /* letter spacing, px (after every character, as the canvas's letterSpacing) */
} qd_font;
float qd_text(const char *utf8, float x, float y, const qd_font *f);   /* the advance drawn */
/* P1.3v (§9a's clarity rules): the smallest text drawn since the reset, its size on the screen (the size times the
 * transform's vertical scale; 1e9 when none) and its first characters, for the tests' guard */
void qd_text_stats_reset(void);
float qd_text_stats_min(const char **text);
float qd_text_width(const char *utf8, const qd_font *f);              /* the width as the canvas measures it */
/* P1.3p: the text's coverage as qd_text would draw it at (x, y), rasterised into mask (w x h, a byte a pixel, 0..255;
 * f->col ignored), for shapes made of the letters (the FIZZ logo's particles). Its own surface: the screen and its
 * state are untouched. 1 drawn, 0 not (no fonts, no memory). */
int qd_text_mask(const char *utf8, float x, float y, const qd_font *f, uint8_t *mask, int w, int h);

#ifdef __cplusplus
}
#endif
