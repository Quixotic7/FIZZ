/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.5d part (PLAN version 2.5): VEL as the caped stunt rider (VEL 10, 40, 70, 100, 127, the kick as
 * the step plays, EDIT's), and the frames the other way round (a held step's screen framed, a track default's not;
 * the sound editor dark orange for the track's sound, bright orange for a step's locks); into
 * OUT/p15d-*.png and OUT/p15d-sheet.png */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ctl.h"
#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"
#include "qd_fx.h"
#include "scale.h"
#include "sounds.h"
#include "ui.h"

#define NSHOT 16
static qd_app_t A;
static uint32_t now;
static uint64_t frames_done;
static uint16_t lcd[QD_W * QD_H];
static const char *outdir;
static struct {
    char file[64], caption[112];
    int has_leds;
    qd_leds_t leds;
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
static void shot(const char *name, const char *caption, int with_leds)
{
    char path[1024];
    qd_app_frame(&A, lcd);
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p15d-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p15d: cannot write %s\n", path);
        failed = 1;
    }
    printf("%s (%s)\n", path, caption);
    if (nshot < NSHOT - 1)
        nshot++;
}

/* the panel */
#define WK(w) QD_WHITE_KEY[w]
#define BK(b) QD_BLACK_KEY[b]
static void bdown(int b) { qd_app_button(&A, b, 1); }
static void bup(int b) { qd_app_button(&A, b, 0); }
static void btap(int b)
{
    bdown(b);
    run(20);
    bup(b);
    run(20);
}
static void black(int b)
{
    qd_app_key(&A, BK(b), 1, 100);
    run(20);
    qd_app_key(&A, BK(b), 0, 0);
    run(10);
}
static void wtap(int w)
{
    qd_app_key(&A, WK(w), 1, 100);
    run(30);
    qd_app_key(&A, WK(w), 0, 0);
    run(20);
}
/* ---------------------------------------------------------------- the sheet: three a row at 1.5x, the keys' LEDs under
 * the screens that light the scale (the panel's 27 keys: the whites a row, the blacks over their gaps) */
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

static void draw_leds(plutovg_canvas_t *c, const qd_leds_t *l, float x, float y, float w)
{
    const float ww = w / 16.f;
    int k, wi = 0;
    for (k = 0; k < QD_NKEY; k++) {
        const int blk = qd_ctl_black(k) >= 0;
        const float v = (float)l->key[k] / 255.f, lx = blk ? x + (float)wi * ww - ww * 0.32f : x + (float)wi * ww + 2.f;
        const float ly = blk ? y : y + 26.f, lw = blk ? ww * 0.64f : ww - 4.f, lh = blk ? 22.f : 26.f;
        plutovg_canvas_set_rgb(c, 0.20f, 0.20f, 0.22f);
        plutovg_canvas_round_rect(c, lx, ly, lw, lh, 3.f, 3.f);
        plutovg_canvas_fill(c);
        if (v > 0.f) {
            plutovg_canvas_set_rgba(c, 1.f, 0.62f, 0.28f, v);   /* the unit's warm LED */
            plutovg_canvas_round_rect(c, lx + 2.f, ly + 2.f, lw - 4.f, lh - 4.f, 2.f, 2.f);
            plutovg_canvas_fill(c);
        }
        if (!blk)
            wi++;
    }
}

static void sheet(void)
{
    const float sc = 1.5f;
    const int cell = (int)(240.f * sc), leds_h = 60, cell_h = cell + 34 + leds_h, pad = 22, cols = 3;
    const int rows = (nshot + cols - 1) / cols, W = pad + cols * (cell + pad), H = 50 + rows * (cell_h + pad);
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
    plutovg_canvas_fill_text(c, "FIZZ P1.5d: VEL as the caped stunt rider; the frames the other way round (1.5x)",
                             -1, PLUTOVG_TEXT_ENCODING_UTF8, (float)pad, 32.f);
    for (i = 0; i < nshot; i++) {
        const float x = (float)(pad + (i % cols) * (cell + pad)), y = (float)(50 + (i / cols) * (cell_h + pad));
        plutovg_surface_t *o;
        char cap[160];
        snprintf(path, sizeof path, "%s/%s", outdir, shots[i].file);
        o = plutovg_surface_load_from_image_file(path);
        snprintf(cap, sizeof cap, "%d  %s", i + 1, shots[i].caption);
        plutovg_canvas_set_font_size(c, 13.f);
        plutovg_canvas_set_rgb(c, 0.85f, 0.85f, 0.85f);
        plutovg_canvas_fill_text(c, cap, -1, PLUTOVG_TEXT_ENCODING_UTF8, x, y + 20.f);
        if (o) {
            draw_surface(c, o, x, y + 30.f, sc);
            plutovg_surface_destroy(o);
        }
        if (shots[i].has_leds)
            draw_leds(c, &shots[i].leds, x, y + 30.f + (float)cell + 6.f, (float)cell);
    }
    snprintf(path, sizeof path, "%s/p15d-sheet.png", outdir);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens)\n", path, nshot);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
}

/* ---------------------------------------------------------------- the script */
int render_p15d(const char *out)
{
    int k;
    outdir = out;
    nshot = 0;
    failed = 0;
    qd_app_init(&A);
    A.ui.view = QD_VIEW_ROLL;   /* (P1.5s: EDIT is the power-on view; made on the PIANO ROLL) */
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    btap(QD_B_HOME);
    run(100);
    for (k = 0; k < 16; k += 4)
        qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 0, k);
    black(1);                                           /* VEL */
    {   /* the rider at VEL 10, 40, 70, 100, 127 (a held step) */
        static const int V[5] = {10, 40, 70, 100, 127};
        char nm[32], cap[64];
        for (k = 0; k < 5; k++) {
            qd_step_set_vel(qd_proj_pat(&A.proj, A.seq.cur), 0, 4, V[k]);
            qd_app_key(&A, WK(4), 1, 100);
            run(400);
            snprintf(nm, sizeof nm, "vel-%d", V[k]);
            snprintf(cap, sizeof cap, "VEL %d: the rider (a step held: the frame)", V[k]);
            shot(nm, cap, 0);
            qd_app_key(&A, WK(4), 0, 0);
            run(30);
        }
    }
    btap(QD_B_PLAY);                                    /* the kick as the step plays */
    qd_app_key(&A, WK(4), 1, 100);
    run(300);
    for (k = 0; k < 4000 && !(A.ui.held_played && A.ms - A.ui.held_play_ms == 90u); k++)
        run(1);
    shot("vel-kick", "The step plays: the kick and a puff (VEL 127)", 0);
    qd_app_key(&A, WK(4), 0, 0);
    run(30);
    btap(QD_B_PLAY);
    run(100);
    qd_step_set_vel(qd_proj_pat(&A.proj, A.seq.cur), 0, 4, 100);
    qd_app_key(&A, WK(4), 1, 100);                      /* a step held: framed */
    run(400);
    shot("vel-held", "A step held (VEL): the frame", 0);
    qd_app_key(&A, WK(4), 0, 0);
    run(30);
    qd_app_key(&A, BK(1), 1, 100);                      /* the VEL key held: the track's default, no frame */
    run(400);
    shot("vel-default", "The VEL key held: the track default, no frame", 0);
    qd_app_key(&A, BK(1), 0, 0);
    run(30);
    bdown(QD_B_FUNC);                                   /* EDIT, the latch's selection: framed */
    btap(QD_B_HOME);
    btap(QD_B_HOME);
    bup(QD_B_FUNC);
    run(20);
    wtap(8);
    btap(QD_B_MUTE);
    run(300);
    shot("edit-latch", "EDIT, the latch's selection (VEL): the frame", 0);
    btap(QD_B_MUTE);
    run(300);
    shot("edit-default", "EDIT, nothing held: the default, no frame", 0);
    btap(QD_B_HOME);
    btap(QD_B_SOUND);                                   /* the sound editor */
    black(3);
    run(300);
    shot("snd-track", "The editor, the track's sound: dark orange", 0);
    qd_app_key(&A, WK(4), 1, 100);
    run(300);
    qd_app_knob(&A, QD_K_1, -8);
    run(300);
    shot("snd-step", "The editor, a step held (its locks): bright orange", 0);
    qd_app_key(&A, WK(4), 0, 0);
    run(30);
    btap(QD_B_HOME);
    bdown(QD_B_FUNC);
    btap(QD_B_HOME);
    bup(QD_B_FUNC);
    run(20);

    if (!failed)
        sheet();
    return failed;
}
