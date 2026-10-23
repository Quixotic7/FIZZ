/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* render_frames' P1.3p part: FIZZ's logo screen, champagne bubbles in the app's colours and the word from particles, at eight moments of its loop
 * (OUT/p13p-fizz-*.png, OUT/p13p-sheet.png) and one whole loop as frames OUT/fizz-logo/f-NNN.png at 15 fps, which
 * `make -C tools frames` turns into OUT/fizz-logo.gif with ffmpeg (2x). */
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"
#include "ui.h"

#define NSHOT 40
/* the owner's reference frames (Champagne01), relative to tools/ */
#define REF_DIR "../../ReferenceImages/FizzyBubbles/Champagne01/Videos Samples-a close up of champagne bubbles slightly " \
                "artistic and abstract"
static qd_app_t A;
static uint32_t now;
static uint64_t frames_done;
static uint16_t lcd[QD_W * QD_H];
static const char *outdir;
static struct {
    char file[64], caption[112];
} shots[NSHOT];
static int nshot, failed;

int p13b_write_lcd_png(const char *path, const uint16_t *lcd_be);   /* render_frames.c's */

static void run(uint32_t n)
{
    while (n--) {
        now++;
        qd_app_tick(&A, now);
        if (now % 15u == 0u)
            qd_app_frame(&A, lcd);
        while (frames_done + 128u <= (uint64_t)now * QD_FS / 1000u) {
            int16_t blk[256];
            qd_app_audio(&A, blk, 128);
            frames_done += 128u;
        }
    }
}
static void fresh(void)
{
    qd_app_init(&A);
    A.ui.view = QD_VIEW_ROLL;   /* (P1.5s: EDIT is the power-on view; made on the PIANO ROLL) */
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
}
static void shot(const char *name, const char *caption)
{
    char path[1024];
    qd_app_frame(&A, lcd);
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13p-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13p: cannot write %s\n", path);
        failed = 1;
    }
    printf("%s (%s)\n", path, caption);
    if (nshot < NSHOT - 1)
        nshot++;
}

/* the panel */

/* ---------------------------------------------------------------- the sheet: three a row, at 2x */
static void draw_surface(plutovg_canvas_t *c, plutovg_surface_t *s, float x, float y, float scale)
{
    plutovg_matrix_t m;
    plutovg_matrix_init_translate(&m, x, y);
    plutovg_matrix_scale(&m, scale, scale);
    plutovg_canvas_set_texture(c, s, PLUTOVG_TEXTURE_TYPE_PLAIN, 1.f, &m);
    plutovg_canvas_rect(c, x, y, (float)plutovg_surface_get_width(s) * scale,
                        (float)plutovg_surface_get_height(s) * scale);
    plutovg_canvas_fill(c);
}

static void sheet(void)
{
    const int cell_w = 480, cell_h = 480 + 34, cols = 3, pad = 18, ref_w = 270;
    const int rows = (nshot + cols - 1) / cols;
    const int W = pad + cols * (cell_w + pad) + ref_w + pad, H = 50 + rows * (cell_h + pad);
    plutovg_surface_t *s = plutovg_surface_create(W, H);
    plutovg_canvas_t *c = plutovg_canvas_create(s);
    plutovg_font_face_t *f = plutovg_font_face_load_from_data(qd_fonts_gen[QD_FONTS_GEN_500].data, qd_fonts_gen[QD_FONTS_GEN_500].len, 0, NULL, NULL);
    char path[1024];
    int i;
    plutovg_canvas_set_rgb(c, 0.11f, 0.11f, 0.12f);
    plutovg_canvas_rect(c, 0, 0, (float)W, (float)H);
    plutovg_canvas_fill(c);
    plutovg_canvas_set_font(c, f, 18.f);
    plutovg_canvas_set_rgb(c, 0.9f, 0.9f, 0.9f);
    plutovg_canvas_fill_text(c, "FIZZ P1.3p: the logo screen, the word gathering, solid, fizzing apart, each from the core (2x); right: the reference", -1,
                             PLUTOVG_TEXT_ENCODING_UTF8, (float)pad, 32.f);
    for (i = 0; i < nshot; i++) {
        const float x = (float)(pad + (i % cols) * (cell_w + pad)), y = (float)(50 + (i / cols) * (cell_h + pad));
        plutovg_surface_t *ours;
        snprintf(path, sizeof path, "%s/%s", outdir, shots[i].file);
        ours = plutovg_surface_load_from_image_file(path);
        plutovg_canvas_set_font_size(c, 13.f);
        plutovg_canvas_set_rgb(c, 0.85f, 0.85f, 0.85f);
        plutovg_canvas_fill_text(c, shots[i].caption, -1, PLUTOVG_TEXT_ENCODING_UTF8, x, y + 20.f);
        if (ours) {
            draw_surface(c, ours, x, y + 30.f, 2.f);
            plutovg_surface_destroy(ours);
        }
    }
    for (i = 0; i < 2; i++) {                          /* the owner's reference, two of its frames */
        static const char *const ref[2] = {REF_DIR "-0001.png", REF_DIR "-0007.png"};
        const float x = (float)(pad + cols * (cell_w + pad)), y = (float)(50 + i * (cell_h + pad));
        plutovg_surface_t *r = plutovg_surface_load_from_image_file(ref[i]);
        plutovg_canvas_set_font_size(c, 13.f);
        plutovg_canvas_set_rgb(c, 0.85f, 0.85f, 0.85f);
        plutovg_canvas_fill_text(c, i ? "reference, frame 7" : "reference, frame 1", -1, PLUTOVG_TEXT_ENCODING_UTF8,
                                 x, y + 20.f);
        if (r) {
            draw_surface(c, r, x, y + 30.f, 480.f / (float)plutovg_surface_get_height(r));
            plutovg_surface_destroy(r);
        }
    }
    snprintf(path, sizeof path, "%s/p13p-sheet.png", outdir);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens)\n", path, nshot);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
}

/* ---------------------------------------------------------------- the script */
#define LOOP_MS 11000u

int render_p13p(const char *out)
{
    static const struct {
        uint32_t at;
        const char *name, *caption;
    } M[] = {
        {1000, "fizz-1-calm", "1.0 s: calm: the bubbles in the app's colours, the big ones turning"},
        {4200, "fizz-2-gather", "4.2 s: gathering: the particles swirl up into the letters"},
        {4580, "fizz-3-handover", "4.58 s: the handover: the solid word in, the particles fading under it"},
        {6200, "fizz-4-solid", "6.2 s: solid: FIZZ in the letters' colours, bubbles behind and in front"},
        {8950, "fizz-5-break", "8.95 s: the break: the word back to particles, the first leaving"},
        {9400, "fizz-6-dissolve", "9.4 s: dissolving: the particles fizz apart upward"},
        {9900, "fizz-7-fade", "9.9 s: the last of them fading"},
        {10700, "fizz-8-calm", "10.7 s: calm again (the loop's end)"},
    };
    char path[1024];
    size_t i;
    int k;
    outdir = out;
    nshot = 0;
    failed = 0;

    for (i = 0; i < sizeof M / sizeof M[0]; i++) {     /* power-on at loop time 0: run to each moment */
        fresh();
        run(M[i].at);
        shot(M[i].name, M[i].caption);
    }
    fresh();                                            /* playing (a DAW's START: the screen stays idle) */
    run(5200);
    qd_app_button(&A, QD_B_PLAY, 1);
    run(10);
    qd_app_button(&A, QD_B_PLAY, 0);
    A.ui.mode = QD_MODE_IDLE;
    while ((long)A.beats % 4 != 0 || A.beats - (double)(long)A.beats > 0.1)
        run(1);
    shot("fizz-9-playing", "playing: a surge from the bed on each beat, more on the downbeat");

    /* the GIF's frames: one whole loop at 15 fps */
    snprintf(path, sizeof path, "%s/fizz-logo", out);
    mkdir(path, 0755);
    fresh();
    for (k = 0; k < (int)(LOOP_MS * 15u / 1000u); k++) {   /* exactly one loop */
        qd_app_frame(&A, lcd);
        snprintf(path, sizeof path, "%s/fizz-logo/f-%03d.png", out, k);
        if (!p13b_write_lcd_png(path, lcd))
            failed = 1;
        run(66 + (k % 3 != 2));                       /* 67, 67, 66: 15 fps exactly */
    }
    printf("%s/fizz-logo/f-*.png (one loop at 15 fps)\n", out);

    if (!failed)
        sheet();
    return failed;
}
