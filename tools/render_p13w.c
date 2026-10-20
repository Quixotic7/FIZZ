/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.3w part: FUNC on B9 (the step functions), SOUND on B10: the PIANO ROLL's FUNC lane, the held and
 * EDIT FUNC pictures (a direction, a jump with its target on its page's strip, a random one), STEPS with the functions'
 * dots; into OUT/p13w-*.png and OUT/p13w-func-sheet.png. The functions are set through the pattern API (qd_ctl_pat), the
 * jump's target by the gesture (a W pressed while the jump is held). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ctl.h"
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13w-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13w: cannot write %s\n", path);
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
static void whold(int w)
{
    qd_app_key(&A, WK(w), 1, 100);
    run(300);
}
static void wrelease(int w)
{
    qd_app_key(&A, WK(w), 0, 0);
    run(20);
}
static void turn(int k, int d)
{
    qd_app_knob(&A, k, d);
    run(5);
}
/* a fresh unit on pattern k (0 A01 ..), the step view, track t (SEL + B), hold mode m */
static void at(int k, int t, int m)
{
    fresh();
    btap(QD_B_HOME);
    bdown(QD_B_MIX);
    wtap(k);
    bup(QD_B_MIX);
    run(20);
    bdown(QD_B_PATTERN);
    black(t);
    bup(QD_B_PATTERN);
    run(20);
    black(m);
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
    plutovg_canvas_fill_text(c, "FIZZ P1.3w: FUNC on B9: the step functions (2x)", -1,
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
    snprintf(path, sizeof path, "%s/p13w-func-sheet.png", outdir);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens)\n", path, nshot);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
}

/* ---------------------------------------------------------------- the script */
enum { A01, A02, A03 };
enum { T1, T2, T3, T4 };

/* A02 KEYS (track 3): page 2 joins the loop (F1: hold B1, press B2) and gets chords; STEPS on */
static void keys_two_pages(void)
{
    static const int CH[4][3] = {{0, 4, 7}, {3, 6, 9}, {1, 4, 8}, {2, 5, 9}};
    int k, j;
    at(A02, T3, UI_F_NOTE);
    qd_app_button(&A, QD_B_OCTDN, 1);
    run(80);
    qd_app_key(&A, BK(0), 1, 100);
    run(30);
    qd_app_key(&A, BK(1), 1, 100);
    run(30);
    qd_app_key(&A, BK(1), 0, 0);
    qd_app_key(&A, BK(0), 0, 0);
    run(20);
    black(1);                                           /* edit page 2 */
    qd_app_button(&A, QD_B_OCTDN, 0);
    run(20);
    for (k = 0; k < 4; k++) {                           /* steps 1, 5, 9, 13: four chords by the keys */
        whold(k * 4);
        for (j = 0; j < 3; j++)
            qd_app_key(&A, BK(CH[k][j]), 1, 100);
        run(30);
        for (j = 0; j < 3; j++)
            qd_app_key(&A, BK(CH[k][j]), 0, 0);
        run(20);
        wrelease(k * 4);
    }
    wtap(7);
    wtap(11);
    btap(QD_B_HOME);                                    /* STEPS */
}

int render_p13w(const char *out)
{
    qd_pattern_t *P;
    outdir = out;
    nshot = 0;
    failed = 0;

    keys_two_pages();                                   /* A02 KEYS, page 2 edited (STEPS) */
    P = qd_ctl_pat(&A);
    qd_step_set_func(P, T3, 16 + 0, QD_FN_REVERSE);
    qd_step_set_func(P, T3, 16 + 4, QD_FN_FLIP);
    qd_step_set_func(P, T3, 16 + 12, QD_FN_RJUMP);
    qd_step_set_func(P, T3, 16 + 14, QD_FN_RFUNC);
    qd_step_set_func(P, T3, 9, QD_FN_FORWARD);
    qd_step_set_func(P, T3, 16 + 2, QD_FN_RESTART);
    run(300);
    shot("steps", "STEPS: the functions as dots (page 2 open, page 1 folded)");
    btap(QD_B_HOME);                                    /* EDIT */
    btap(QD_B_HOME);                                    /* the PIANO ROLL */
    black(UI_F_FUNC);
    run(300);
    shot("lane", "the PIANO ROLL's FUNC lane: reverse, restart, flip, random, random function");
    whold(0);
    run(300);
    shot("held-reverse", "held: REVERSE");
    wrelease(0);
    whold(8);                                           /* page 2 step 9: RANDOM, then W4 its target: JUMP 20 */
    black(5);
    run(50);
    qd_app_key(&A, WK(3), 1, 100);
    run(40);
    qd_app_key(&A, WK(3), 0, 0);
    run(300);
    shot("held-jump", "held: B6 RANDOM, then W4: JUMP 20, its page strip");
    wrelease(8);
    whold(12);
    run(300);
    shot("held-random", "held: RANDOM (a jump to a step at random)");
    wrelease(12);
    btap(QD_B_HOME);                                    /* STEPS, then EDIT with the cursor on step 9 */
    btap(QD_B_HOME);
    turn(QD_K_PRESETS, 1);
    while (A.ui.nheld && A.ui.held[0] != 16 + 8)
        turn(QD_K_PRESETS, A.ui.held[0] < 16 + 8 ? 1 : -1);
    run(400);
    shot("edit-jump", "EDIT: JUMP 20 in the head, its target on the strip, the row under it");
    btap(QD_B_MUTE);
    black(UI_F_SOUND);
    run(200);
    shot("edit-sound-b10", "B10 is SOUND now (EDIT, nothing held)");

    if (!failed)
        sheet();
    return failed;
}
