/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.3e part: the screens of PLAN.md §8b version 2.2's later bullets (editing a track's default, six
 * notes, the quiet KNOB 4, HOME's STEPS view), from a real state reached by panel input only, each from a fresh unit,
 * into OUT/p13e-*.png; then OUT/p13e-sheet.png, every shot at 2x with its caption. */
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13e-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13e: cannot write %s\n", path);
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
    plutovg_canvas_fill_text(c, "FIZZ P1.3e: the screens of PLAN §8b version 2.2's later bullets, each from the core (2x)", -1,
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
    snprintf(path, sizeof path, "%s/p13e-sheet.png", outdir);
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

static void key(int b, int down)
{
    qd_app_key(&A, BK(b), down, 100);
    run(10);
}

int render_p13e(const char *out)
{
    outdir = out;
    nshot = 0;
    failed = 0;

    /* a mode key held: the TRACK DEFAULT picture; NOTE's chord by the knobs, then B11 */
    at(A02, T3, UI_F_VEL);
    key(UI_F_NOTE, 1);
    turn(QD_K_1, 4);
    turn(QD_K_2, 6);
    turn(QD_K_3, 9);
    run(300);
    shot("mkey-note", "NOTE key held: the track's default chord (KNOB 1..3: C E G B), B11 RESET lit");
    qd_app_key(&A, BK(10), 1, 100);
    run(20);
    qd_app_key(&A, BK(10), 0, 0);
    run(250);
    shot("mkey-note-reset", "NOTE key held + B11: back to the single C4 (RESET dim)");
    key(UI_F_NOTE, 0);
    at(A02, T2, UI_F_NOTE);
    key(UI_F_VEL, 1);
    run(300);
    turn(QD_K_1, -22);
    run(250);
    shot("mkey-vel", "VEL key held, KNOB 1: the track's velocity (the mallet)");
    key(UI_F_VEL, 0);
    at(A02, T2, UI_F_NOTE);
    key(UI_F_LEN, 1);
    run(300);
    turn(QD_K_1, 2);
    run(250);
    shot("mkey-len", "LEN key held, KNOB 1: the track's length (2 steps)");
    key(UI_F_LEN, 0);

    /* nothing held: ALGORITHM in NOTE, the full picture */
    at(A02, T3, UI_F_NOTE);
    turn(QD_K_1, 3);
    turn(QD_K_2, 7);
    run(QD_TK_MS + QD_TK_FADE_MS + 20);
    turn(QD_K_ALGO, 2);
    run(300);
    shot("default-algo-note", "NOTE, nothing held, ALGORITHM: the default chord moved, its full picture");

    /* six notes on a held step */
    at(A02, T3, UI_F_NOTE);
    whold(0);
    turn(QD_K_SELECT, -3);
    turn(QD_K_1, 4);
    turn(QD_K_2, 6);
    turn(QD_K_3, 9);
    turn(QD_K_4, 11);
    turn(QD_K_ALGO, 17);
    run(QD_TK_MS + QD_TK_FADE_MS + 20);
    shot("note-six", "NOTE, step 1 held: six notes (ALGORITHM = note 6, pink)");
    wrelease(0);

    /* the STEPS view */
    at(A02, T1, UI_F_NOTE);
    btap(QD_B_HOME);
    run(200);
    shot("steps-stopped", "HOME: the STEPS view, A02 DRUMS, stopped");
    btap(QD_B_PLAY);
    run(1310);
    shot("steps-playing", "STEPS, playing: the playhead's cell, the trig flashing");
    btap(QD_B_PLAY);
    at(A01, T1, UI_F_VEL);
    btap(QD_B_HOME);
    black(UI_F_TRIG);
    whold(2);
    black(1);                                           /* step 3: a LOCK step */
    wrelease(2);
    black(UI_F_COND);
    whold(6);
    black(2);
    black(6);                                           /* step 7: 1:2 */
    wrelease(6);
    black(UI_F_CHNC);
    whold(10);
    black(4);                                           /* step 11: 50 % */
    wrelease(10);
    run(200);
    shot("steps-locks", "STEPS: A01's p-locks and sound locks (marks), a LOCK step (ring), a condition, a chance");
    at(A02, T2, UI_F_LEN);
    btap(QD_B_HOME);
    qd_app_button(&A, QD_B_OCTDN, 1);
    qd_app_button(&A, QD_B_OCTUP, 1);
    run(20);
    qd_app_button(&A, QD_B_OCTUP, 0);
    qd_app_button(&A, QD_B_OCTDN, 0);
    run(20);
    turn(QD_K_3, 2);
    turn(QD_K_4, -5);
    btap(QD_B_OCTDN);
    btap(QD_B_PLAY);
    run(900);
    shot("steps-range", "STEPS: a short page range (3..11), playing");
    btap(QD_B_PLAY);

    if (!failed)
        sheet();
    return failed;
}
