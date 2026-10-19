/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.3k part: FIZZ's idle screen (bubbles), a few moments stopped and playing, into OUT/p13k-fizz-*.png
 * and OUT/p13k-sheet.png; and 4 s of it (a stopped second, then playing) as frames OUT/fizz/fizz-NNN.png at 30 fps,
 * which `make -C tools frames` turns into OUT/fizz-idle.gif with ffmpeg (2x). */
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
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
}
static void shot(const char *name, const char *caption)
{
    char path[1024];
    qd_app_frame(&A, lcd);
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13k-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13k: cannot write %s\n", path);
        failed = 1;
    }
    printf("%s (%s)\n", path, caption);
    if (nshot < NSHOT - 1)
        nshot++;
}

/* the panel */
static void bdown(int b) { qd_app_button(&A, b, 1); }
static void bup(int b) { qd_app_button(&A, b, 0); }
static void btap(int b)
{
    bdown(b);
    run(20);
    bup(b);
    run(20);
}

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
    const int cell_w = 480, cell_h = 480 + 34, cols = 3, pad = 18;
    const int rows = (nshot + cols - 1) / cols;
    const int W = pad + cols * (cell_w + pad), H = 50 + rows * (cell_h + pad);
    plutovg_surface_t *s = plutovg_surface_create(W, H);
    plutovg_canvas_t *c = plutovg_canvas_create(s);
    plutovg_font_face_t *f = plutovg_font_face_load_from_data(qd_fonts_gen[3].data, qd_fonts_gen[3].len, 0, NULL, NULL);
    char path[1024];
    int i;
    plutovg_canvas_set_rgb(c, 0.11f, 0.11f, 0.12f);
    plutovg_canvas_rect(c, 0, 0, (float)W, (float)H);
    plutovg_canvas_fill(c);
    plutovg_canvas_set_font(c, f, 18.f);
    plutovg_canvas_set_rgb(c, 0.9f, 0.9f, 0.9f);
    plutovg_canvas_fill_text(c, "FIZZ P1.3k: the idle screen, bubbles, each from the core (2x)", -1,
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
    snprintf(path, sizeof path, "%s/p13k-sheet.png", outdir);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens)\n", path, nshot);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
}

/* ---------------------------------------------------------------- the script */
int render_p13k(const char *out)
{
    char path[1024];
    int k;
    outdir = out;
    nshot = 0;
    failed = 0;

    fresh();                                            /* power-on: the idle screen */
    run(2000);
    shot("fizz-stopped-1", "FIZZ, stopped: a lazy stream (2 s)");
    run(3500);
    shot("fizz-stopped-2", "FIZZ, stopped (5.5 s): one caught on a letter?");
    btap(QD_B_PLAY);                                    /* (as a DAW's START would: playing on the idle screen) */
    A.ui.mode = QD_MODE_IDLE;
    run(1500);
    shot("fizz-playing-1", "FIZZ, playing (A02, 100 BPM): the bursts on the beats");
    while ((long)(A.beats) % 4 != 0)
        run(1);
    run(120);
    shot("fizz-downbeat", "FIZZ, playing: the bar's downbeat, a bigger fizz");
    run(380);
    shot("fizz-downbeat-2", "FIZZ, playing: the downbeat's bubbles rising, the beat after");
    btap(QD_B_PLAY);
    A.ui.mode = QD_MODE_IDLE;
    run(600);
    shot("fizz-stopped-after", "FIZZ, just stopped: the last bubbles still rising");

    /* the GIF's frames: 4 s at 30 fps, a fresh unit, 1 s stopped, PLAY (it plays on the idle screen), 3 s playing */
    snprintf(path, sizeof path, "%s/fizz", out);
    mkdir(path, 0755);
    fresh();
    run(4000);
    for (k = 0; k < 120; k++) {
        if (k == 30) {                                  /* PLAY, the idle screen kept (it goes back after a press) */
            qd_app_button(&A, QD_B_PLAY, 1);
            run(10);
            qd_app_button(&A, QD_B_PLAY, 0);
            A.ui.mode = QD_MODE_IDLE;
        }
        qd_app_frame(&A, lcd);
        snprintf(path, sizeof path, "%s/fizz/fizz-%03d.png", out, k);
        if (!p13b_write_lcd_png(path, lcd))
            failed = 1;
        run(k == 30 ? 23 : 33);
    }
    printf("%s/fizz/fizz-000..119.png (4 s at 30 fps)\n", out);

    if (!failed)
        sheet();
    return failed;
}
