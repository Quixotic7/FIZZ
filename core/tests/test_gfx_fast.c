/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* test_gfx_fast: the fast drawing (core/gfx/qd_gfx_fast.c) on its own, at the device's sizes (tools/gfx_fast.mk
 * test-gfx-fast, ASan + UBSan). Its likeness to plutovg is render_frames' (gfx-compare); here:
 *   1. every call of qd_gfx.h, in a scene that uses them all, drawn whole and again in strips of 23, 7 and 1 rows: the
 *      same pixels (each row is drawn from the row alone);
 *   2. the geometry: a 1 px line on a pixel row's centre fills that row; a rectangle's edges' coverage; a disc's
 *      area; round caps; a dash's on / off; the clip; the global alpha; a transform's scale of the width;
 *   3. text: the width is the advances' float sum (as plutovg's), an atlas size and an outline size draw the same ink
 *      in the same place, the clarity guard's smallest size, the text mask;
 *   4. the frame hash: the same calls the same hash, a changed colour another; nothing drawn while hashing;
 *   5. no point of the screens' paths lost (qd_fast_overflows). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qd_gfx.h"

unsigned qd_fast_overflows(void);
void qd_fast_peak(int *points, int *contours);

static int fails, checks;
#define CHECK(c, ...)                                                                                                  \
    do {                                                                                                               \
        checks++;                                                                                                      \
        if (!(c)) {                                                                                                    \
            fails++;                                                                                                   \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                               \
            printf(__VA_ARGS__);                                                                                       \
            printf("\n");                                                                                              \
        }                                                                                                              \
    } while (0)

static uint16_t A[QD_W * QD_H], B[QD_W * QD_H];

static void px(const uint16_t *lcd, int x, int y, int *r, int *g, int *b)   /* a pixel as 8-bit channels */
{
    const uint16_t be = lcd[y * QD_W + x], c = (uint16_t)((be >> 8) | (be << 8));
    *r = ((c >> 11) & 31) * 255 / 31, *g = ((c >> 5) & 63) * 255 / 63, *b = (c & 31) * 255 / 31;
}
static int lum(const uint16_t *lcd, int x, int y)
{
    int r, g, b;
    px(lcd, x, y, &r, &g, &b);
    return (r + g + b) / 3;
}

/* ---- the scene: every call */
static void scene(void)
{
    static const float poly[] = {20.f, 200.f, 60.f, 170.f, 90.f, 215.f, 130.f, 160.f};
    static const float dash[] = {3.f, 2.f};
    qd_font f;
    float tw;
    qd_fill_rect(0.f, 0.f, 240.f, 240.f, 0x101418u);
    qd_line(10.5f, 10.5f, 230.5f, 30.25f, 0xff6a2cu, 1.f);
    qd_line(10.f, 40.f, 10.f, 120.f, 0x3f7dffu, 2.5f);
    qd_line(20.f, 40.f, 120.f, 40.f, 0x1fd47eu, 0.6f);
    qd_polyline(poly, 4, 0, 0xffd23fu, 3.f, QD_NONE);
    qd_polyline(poly, 4, 1, 0xa879ffu, 1.2f, 0x202060u);
    qd_circle(60.f, 80.f, 18.f, 0xff52c0u, 1.3f, 0x301020u);
    qd_circle(60.f, 80.f, 0.4f, QD_NONE, 1.f, 0xffffffu);
    qd_arc(120.f, 80.f, 20.f, -1.f, 2.f, 0x36d6e7u, 4.f);
    qd_arc(120.f, 80.f, 10.f, 2.f, -1.f, 0xf1f0eau, 1.f);
    qd_ellipse(180.f, 80.f, 30.f, 12.f, 0x9fe6ffu, 1.f, 0x102030u);
    qd_ellipse_arc(180.f, 110.f, 30.f, 8.f, 0.3f, 2.5f, 0xb5e853u, 2.f);
    qd_rrect(150.f, 130.f, 70.f, 40.f, 8.f, 0xd8c39au, 1.2f, 0x202020u);
    qd_rrect(150.5f, 180.25f, 70.f, 30.f, 0.f, 0x3f7dffu, 1.f, QD_NONE);
    qd_save();
    qd_alpha(0.35f);
    qd_line(20.f, 230.f, 220.f, 140.f, 0xffffffu, 9.f);   /* (a translucent glow: blended once, joints and all) */
    qd_polyline(poly, 4, 0, 0x28e1ffu, 7.f, QD_NONE);
    qd_restore();
    qd_save();
    qd_dash(dash, 2);
    qd_line(20.f, 130.f, 140.f, 130.f, 0xffffffu, 1.f);
    qd_circle(100.f, 190.f, 14.f, 0xff6a2cu, 1.f, QD_NONE);
    qd_restore();
    qd_save();
    qd_translate(200.f, 200.f);
    qd_rotate(0.6f);
    qd_scale(1.5f, 1.5f);
    qd_rrect(-10.f, -6.f, 20.f, 12.f, 3.f, 0xffffffu, 1.f, 0x803020u);
    qd_circle(0.f, 0.f, 3.f, 0x00ff00u, 1.f, QD_NONE);
    qd_restore();
    qd_save();
    qd_clip_rect(30.3f, 140.6f, 60.4f, 20.2f);
    qd_fill_rect(0.f, 0.f, 240.f, 240.f, 0x3060a0u);
    qd_restore();
    qd_path_begin();
    qd_move_to(100.f, 230.f);
    qd_quad_to(130.f, 180.f, 160.f, 230.f);
    qd_cubic_to(170.f, 200.f, 200.f, 240.f, 230.f, 220.f);
    qd_path_arc(200.f, 200.f, 15.f, 0.5f, 2.5f);
    qd_line_width(2.f);
    qd_path_stroke_keep(0xff3ca0u, 6.f);
    qd_path_stroke(0xffffffu, 0.f);
    qd_path_begin();
    qd_move_to(160.f, 20.f);
    qd_line_to(200.f, 60.f);
    qd_line_to(150.f, 60.f);
    qd_path_close();
    qd_path_paint(0xffffffu, 1.f, 0x406080u);
    qd_path_begin();
    qd_move_to(205.f, 20.f);
    qd_line_to(230.f, 50.f);
    qd_line_to(205.f, 50.f);
    qd_path_fill(0x80ff80u);
    f.size = 12.f, f.weight = 700, f.col = 0xf1f0eau, f.align = QD_LEFT, f.ls = 0.8f;
    tw = qd_text("STEP 5 \xc2\xb7 NOTE", 12.3f, 150.f, &f);
    f.size = 13.f, f.weight = 500, f.align = QD_RIGHT, f.ls = 0.f;
    qd_text("A02 imported", 232.f, 150.f, &f);
    f.size = 44.f, f.weight = 300, f.align = QD_CENTER, f.col = 0xffd23fu;
    qd_text("122", 120.f, 120.f, &f);
    qd_save();
    qd_translate(60.f, 225.f);
    qd_scale(0.7f, 1.f);                                /* (a squeezed text: from its outlines) */
    f.size = 16.51f, f.weight = 500, f.align = QD_LEFT;
    qd_text("SQUEEZED", 0.f, 0.f, &f);
    qd_restore();
    (void)tw;
    CHECK(qd_layer_begin(0, 10, 10) == 0, "no layers at the device's sizes");
    qd_layer_draw(0, 0.f, 0.f, 10.f, 10.f);
    qd_layer_end();
}

static void draw_strips(uint16_t *lcd, int rows)
{
    int y;
    qd_set_strip_rows(rows);
    for (y = 0; y < QD_H; y += qd_strip_rows()) {
        qd_begin_strip(y);
        scene();
        qd_end_strip(lcd);
    }
    qd_set_strip_rows(240);
}

static void one(uint16_t *lcd, void (*fn)(void))       /* a frame of fn in the device's strips */
{
    int y;
    for (y = 0; y < QD_H; y += qd_strip_rows()) {
        qd_begin_strip(y);
        fn();
        qd_end_strip(lcd);
    }
}

static float g_y;
static void hline(void) { qd_line(20.f, g_y, 220.f, g_y, 0xffffffu, 1.f); }
static void rect_frac(void) { qd_fill_rect(10.25f, 10.5f, 20.5f, 10.f, 0xffffffu); }
static void disc(void) { qd_circle(120.f, 120.f, 30.f, QD_NONE, 1.f, 0xffffffu); }
static void cap(void) { qd_line(100.f, 100.f, 140.f, 100.f, 0xffffffu, 10.f); }
static void dashes(void)
{
    static const float d[] = {4.f, 4.f};
    qd_dash(d, 2);
    qd_line(0.f, 50.5f, 240.f, 50.5f, 0xffffffu, 1.f);
}
static void clipped(void)
{
    qd_clip_rect(50.f, 50.f, 20.f, 20.f);
    qd_fill_rect(0.f, 0.f, 240.f, 240.f, 0xffffffu);
}
static void half(void)
{
    qd_alpha(0.5f);
    qd_fill_rect(0.f, 0.f, 240.f, 240.f, 0xffffffu);
}
static void scaled(void)
{
    qd_scale(3.f, 3.f);
    qd_line(10.f, 20.f, 70.f, 20.f, 0xffffffu, 1.f);   /* (3 px wide on the screen) */
}
static qd_font tf;
static float t_x, t_y;
static void text1(void) { qd_text("FIZZ 0123", t_x, t_y, &tf); }

int main(void)
{
    int i, x, y;
    CHECK(qd_init(), "qd_init");

    /* 1. the scene in strips */
    draw_strips(A, 40);
    {
        static const int R[3] = {23, 7, 1};
        int k;
        for (k = 0; k < 3; k++) {
            int n = 0;
            draw_strips(B, R[k]);
            for (i = 0; i < QD_W * QD_H; i++)
                n += A[i] != B[i];
            CHECK(n == 0, "the scene in %d-row strips: %d pixels differ from 40-row strips", R[k], n);
        }
    }
    {   /* (something was drawn everywhere it should) */
        int lit = 0;
        for (i = 0; i < QD_W * QD_H; i++)
            lit += A[i] != 0;
        CHECK(lit > QD_W * QD_H * 9 / 10, "the scene drew (%d pixels not black)", lit);
    }

    /* 2. geometry */
    g_y = 10.5f, one(A, hline);
    CHECK(lum(A, 100, 10) == 255 && lum(A, 100, 9) == 0 && lum(A, 100, 11) == 0, "a 1 px line at y 10.5 fills row 10 "
          "(%d %d %d)", lum(A, 100, 9), lum(A, 100, 10), lum(A, 100, 11));
    g_y = 11.f, one(A, hline);
    CHECK(abs(lum(A, 100, 10) - 128) <= 6 && abs(lum(A, 100, 11) - 128) <= 6, "a 1 px line at y 11: half of rows 10, "
          "11 (%d %d)", lum(A, 100, 10), lum(A, 100, 11));
    one(A, rect_frac);                                  /* (10.25 .. 30.75 x 10.5 .. 20.5) */
    CHECK(lum(A, 10, 15) > 180 && lum(A, 10, 15) < 200 && lum(A, 20, 15) == 255 && lum(A, 30, 15) > 180 &&
          lum(A, 30, 15) < 200 && abs(lum(A, 20, 10) - 128) <= 6 && abs(lum(A, 20, 20) - 128) <= 6 &&
          lum(A, 20, 21) == 0 && lum(A, 31, 15) == 0, "a rectangle's fractional edges (%d %d %d %d %d)", lum(A, 10, 15),
          lum(A, 20, 15), lum(A, 30, 15), lum(A, 20, 10), lum(A, 20, 20));
    one(A, disc);
    {
        double s = 0;
        for (i = 0; i < QD_W * QD_H; i++)
            s += lum(A, i % QD_W, i / QD_W) / 255.0;
        CHECK(fabs(s - 3.14159265 * 900.0) < 6.0, "a disc of r 30: its area %.1f (pi r^2 %.1f)", s, 3.14159265 * 900.0);
    }
    one(A, cap);
    CHECK(lum(A, 96, 100) == 255 && lum(A, 143, 100) == 255 && lum(A, 94, 94) == 0 && lum(A, 120, 104) == 255 &&
          lum(A, 120, 106) == 0, "a 10 px line's round caps and body (%d %d %d %d %d)", lum(A, 96, 100),
          lum(A, 143, 100), lum(A, 94, 94), lum(A, 120, 104), lum(A, 120, 106));
    one(A, dashes);
    {
        int ok = 1;
        double on = 0;
        for (x = 8; x < 232; x++)
            on += lum(A, x, 50) / 255.0;
        /* (4 on, 4 off, round caps: each dash 5 px long) */
        CHECK(fabs(on - 224.0 * 5.0 / 8.0) < 4.0, "a 4 / 4 dash with round caps covers 5 / 8 of the row (%.1f of 224)", on);
        for (x = 0; x < 3; x++)
            ok &= lum(A, x, 50) > 200;                  /* (the pattern from the line's start: on) */
        CHECK(ok, "the dash starts on");
    }
    one(A, clipped);
    CHECK(lum(A, 50, 50) == 255 && lum(A, 69, 69) == 255 && lum(A, 49, 60) == 0 && lum(A, 70, 60) == 0 &&
          lum(A, 60, 49) == 0 && lum(A, 60, 70) == 0, "the clip");
    one(A, half);
    CHECK(abs(lum(A, 5, 5) - 128) <= 6, "alpha 0.5 over black (%d)", lum(A, 5, 5));
    one(A, scaled);                                     /* (y 60, 3 px: rows 58.5 .. 61.5) */
    CHECK(lum(A, 100, 59) == 255 && lum(A, 100, 60) == 255 && abs(lum(A, 100, 58) - 128) <= 6 &&
          abs(lum(A, 100, 61) - 128) <= 6 && lum(A, 100, 57) == 0 && lum(A, 100, 62) == 0,
          "the transform scales the width (rows 57..62: %d %d %d %d %d %d)", lum(A, 100, 57), lum(A, 100, 58),
          lum(A, 100, 59), lum(A, 100, 60), lum(A, 100, 61), lum(A, 100, 62));

    /* 3. text */
    {
        qd_font f;
        f.size = 12.f, f.weight = 700, f.col = 0xffffffu, f.align = QD_LEFT, f.ls = 0.5f;
        {   /* the width: the advances' sum plus the spacing after each (a three-glyph text is three times one) */
            const float w1 = qd_text_width("8", &f), w3 = qd_text_width("888", &f);
            CHECK(w1 > 6.f && w1 < 9.f && fabsf(w3 - 3.f * w1) < 1e-4f, "text widths (%f %f)", w1, w3);
        }
        f.weight = 650;                                 /* (the nearest weight, a tie to the heavier) */
        CHECK(qd_text_width("ABC", &f) == (f.weight = 700, qd_text_width("ABC", &f)), "weight 650 is 700");
        f.weight = 400;
        CHECK(qd_text_width("ABC", &f) == (f.weight = 500, qd_text_width("ABC", &f)), "weight 400 is 500");
        f.weight = 200;
        CHECK(qd_text_width("ABC", &f) == (f.weight = 300, qd_text_width("ABC", &f)), "weight 200 is 300");
    }
    {   /* an atlas size (13 px 500) and the same text from the outlines (13.0001 px): the same ink, same place */
        long sa = 0, sb = 0, cxa = 0, cxb = 0, diff = 0;
        tf.size = 13.f, tf.weight = 500, tf.col = 0xffffffu, tf.align = QD_CENTER, tf.ls = 0.3f;
        t_x = 120.3f, t_y = 100.f;
        one(A, text1);
        tf.size = 13.0001f;
        one(B, text1);
        for (y = 0; y < QD_H; y++)
            for (x = 0; x < QD_W; x++) {
                const int a = lum(A, x, y), b = lum(B, x, y);
                sa += a, sb += b, cxa += (long)a * x, cxb += (long)b * x, diff += abs(a - b);
            }
        CHECK(sa > 0 && labs(sa - sb) * 100 < sa * 4, "atlas and outline ink: %ld %ld", sa, sb);
        CHECK(sa > 0 && sb > 0 && fabs((double)cxa / sa - (double)cxb / sb) < 0.1, "atlas and outline: the same place "
              "(%.2f %.2f)", (double)cxa / sa, (double)cxb / sb);
        CHECK(diff * 100 < sa * 30, "atlas and outline: pixel by pixel within 30 %% of the ink (%ld of %ld)", diff, sa);
    }
    {   /* the clarity guard: the smallest text drawn, as on screen (the transform's vertical scale) */
        qd_font f;
        const char *s = 0;
        qd_text_stats_reset();
        f.size = 14.f, f.weight = 500, f.col = 0xffffffu, f.align = QD_LEFT, f.ls = 0.f;
        qd_begin_strip(0);
        qd_text("BIG", 10.f, 30.f, &f);
        qd_scale(1.f, 0.5f);
        qd_text("small", 10.f, 60.f, &f);
        qd_end_strip(A);
        CHECK(fabsf(qd_text_stats_min(&s) - 7.f) < 1e-3f && s && !strcmp(s, "small"), "the smallest text: %f %s",
              qd_text_stats_min(0), s ? s : "-");
    }
    {   /* the text mask: rows of it as the whole of it */
        static uint8_t m1[240 * 40], m2[240 * 40];
        qd_font f;
        int n = 0, ink = 0;
        f.size = 30.f, f.weight = 300, f.col = 0xffffffu, f.align = QD_LEFT, f.ls = 0.f;
        CHECK(qd_text_mask("FIZZ", 10.f, 30.f, &f, m1, 240, 40), "qd_text_mask");
        for (y = 0; y < 40; y++)
            CHECK(qd_text_mask_rows("FIZZ", 10.f, 30.f, &f, m2 + y * 240, 240, y, 1), "qd_text_mask_rows");
        for (i = 0; i < 240 * 40; i++)
            n += m1[i] != m2[i], ink += m1[i];
        CHECK(n == 0 && ink > 255 * 200, "the mask by rows is the mask (%d differ; ink %d)", n, ink);
    }

    /* 4. the frame hash */
    {
        uint32_t h1, h2, h3;
        int lit = 0;
        draw_strips(B, 40);
        qd_hash_begin();
        scene();
        h1 = qd_hash_end();
        qd_end_strip(A);
        for (i = 0; i < QD_W * 40; i++)
            lit += A[i] != 0;
        qd_hash_begin();
        scene();
        h2 = qd_hash_end();
        qd_hash_begin();
        scene();
        qd_line(0.f, 0.f, 1.f, 1.f, 0x010101u, 1.f);
        h3 = qd_hash_end();
        CHECK(h1 == h2 && h1 != h3, "the hash: the same calls the same (%08x %08x), more calls another (%08x)", h1, h2,
              h3);
        CHECK(lit == 0, "nothing drawn while hashing (%d pixels)", lit);
    }

    /* 5. nothing lost: the screens' paths (render_frames' peak: 288 points), the charset at 120 px in each weight */
    {
        static const int W[3] = {300, 500, 700};
        int k, pts = 0, cts = 0;
        char all[128];
        qd_font f;
        for (i = 0; i < 95; i++)
            all[i] = (char)(0x20 + i);
        all[95] = 0;
        f.size = 120.f, f.col = 0xffffffu, f.align = QD_LEFT, f.ls = 0.f;
        for (k = 0; k < 3; k++) {
            f.weight = W[k];
            qd_begin_strip(0);
            qd_text(all, -2000.f, 100.f, &f);
            qd_text("\xc2\xb0\xc2\xb1\xc2\xb7\xc3\x97\xe2\x80\x93\xe2\x80\x94\xe2\x80\xa6\xe2\x86\x92\xe2\x88\x92", 0.f,
                    100.f, &f);
            qd_end_strip(A);
        }
        qd_fast_peak(&pts, &cts);
        printf("test_gfx_fast: the largest path %d points, %d contours\n", pts, cts);
    }
    CHECK(qd_fast_overflows() == 0, "path points lost: %u", qd_fast_overflows());

    qd_shutdown();
    printf("test_gfx_fast: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
