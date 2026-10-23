/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* render_frames' P1.3f part: NOTE's knobs as PLAN §8b 2.2's "superseded the same day" has them (each knob moves its
 * own note, ALGORITHM all of them), from a real state reached by panel input only, into OUT/p13f-*.png and
 * OUT/p13f-sheet.png. */
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
    A.ui.view = QD_VIEW_ROLL;   /* (P1.5s: EDIT is the power-on view; made on the PIANO ROLL) */
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
}
static void shot(const char *name, const char *caption)
{
    char path[1024];
    qd_app_frame(&A, lcd);
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13f-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13f: cannot write %s\n", path);
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
static void vhome(void)                                 /* (P1.3y: SHIFT + HOME, the next view; HOME alone never cycles) */
{
    qd_app_button(&A, QD_B_FUNC, 1);
    run(10);
    btap(QD_B_HOME);
    qd_app_button(&A, QD_B_FUNC, 0);
    run(10);
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
    A.ui.knob_ms[k] = 0;                                /* (one gesture a turn: no acceleration, P1.5b) */
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
    plutovg_font_face_t *f = plutovg_font_face_load_from_data(qd_fonts_gen[QD_FONTS_GEN_500].data, qd_fonts_gen[QD_FONTS_GEN_500].len, 0, NULL, NULL);
    char path[1024];
    int i;
    plutovg_canvas_set_rgb(c, 0.11f, 0.11f, 0.12f);
    plutovg_canvas_rect(c, 0, 0, (float)W, (float)H);
    plutovg_canvas_fill(c);
    plutovg_canvas_set_font(c, f, 18.f);
    plutovg_canvas_set_rgb(c, 0.9f, 0.9f, 0.9f);
    plutovg_canvas_fill_text(c, "FIZZ P1.3f: the screens of PLAN §8b 2.2: NOTE's knobs each move their own note (P1.3f), each from the core (2x)", -1,
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
    snprintf(path, sizeof path, "%s/p13f-sheet.png", outdir);
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

static void settle(void) { run(QD_TK_MS + QD_TK_FADE_MS + 20); }

int render_p13f(const char *out)
{
    int k;
    outdir = out;
    nshot = 0;
    failed = 0;

    at(A02, T3, UI_F_NOTE);                             /* KEYS, step 1 held: a four-note chord by the keys */
    whold(0);
    for (k = 0; k < 10; k += 2)
        if (k != 6)
            qd_app_key(&A, BK(k), 1, 100);
    run(30);
    for (k = 0; k < 10; k += 2)
        if (k != 6)
            qd_app_key(&A, BK(k), 0, 0);
    settle();
    shot("before", "NOTE, step 1 held: C D E G# (keys B1 B3 B5 B9)");
    turn(QD_K_SELECT, -3);
    run(250);
    shot("select-band", "SELECT -3: note 1 alone (C4 -> A3), its band");
    settle();
    shot("select-after", "after SELECT: note 1 moved alone, the others stayed");
    turn(QD_K_2, 3);
    settle();
    shot("knob2", "KNOB 2 +3: note 3 alone (E4 -> G4)");
    turn(QD_K_ALGO, 2);
    settle();
    shot("algo", "ALGORITHM +2: every note together (A = ALL)");
    wrelease(0);

    at(A02, T3, UI_F_VEL);                              /* the NOTE key held: the default chord, the same knobs */
    qd_app_key(&A, BK(0), 1, 100);
    run(20);
    turn(QD_K_1, 4);
    turn(QD_K_2, 6);
    turn(QD_K_SELECT, -2);
    run(300);
    shot("mkey", "NOTE key held: the default chord, SELECT moved note 1 alone (A#3)");
    qd_app_key(&A, BK(0), 0, 0);
    run(20);

    /* the STEPS overview: A02 DRUMS with a page 2 and a page 4 in the loop */
    at(A02, T1, UI_F_VEL);
    qd_app_button(&A, QD_B_OCTDN, 1);                   /* F1: hold B1, press B2: pages 1..2; hold B4: page 4 in */
    run(80);
    qd_app_key(&A, BK(0), 1, 100);
    run(30);
    qd_app_key(&A, BK(1), 1, 100);
    run(30);
    qd_app_key(&A, BK(1), 0, 0);
    qd_app_key(&A, BK(0), 0, 0);
    run(20);
    qd_app_key(&A, BK(3), 1, 100);
    run(450);
    qd_app_key(&A, BK(3), 0, 0);
    run(20);
    black(1);                                           /* edit page 2 */
    qd_app_button(&A, QD_B_OCTDN, 0);
    run(20);
    for (k = 0; k < 16; k += 3)
        wtap(k);
    whold(6);
    black(9);                                           /* (VEL 127: an override) */
    wrelease(6);
    vhome();
    run(200);
    shot("steps-stopped", "STEPS: the eight pages, page 2 edited, pages 1, 2, 4 in the loop (stopped)");
    btap(QD_B_PLAY);
    while (qd_seq_track_step(&A.seq, 0) != 16)
        run(1);
    run(40);
    shot("steps-playing", "STEPS, playing: the playhead (red) crossing into page 2, the trig flashing");
    btap(QD_B_PLAY);
    qd_app_button(&A, QD_B_OCTDN, 1);                   /* F3 latched: page 2's range 4..11 */
    qd_app_button(&A, QD_B_OCTUP, 1);
    run(20);
    qd_app_button(&A, QD_B_OCTUP, 0);
    qd_app_button(&A, QD_B_OCTDN, 0);
    run(20);
    turn(QD_K_3, 3);
    turn(QD_K_4, -4);
    btap(QD_B_OCTDN);
    run(200);
    shot("steps-range", "STEPS: page 2 at 4..12 (outside: dark)");
    black(UI_F_PLOCK);                                  /* P-LOCK (holding keeps the screen), then the latch: */
    btap(QD_B_MUTE);                                    /* W4 + W7 held together select steps 20 and 23 */
    qd_app_key(&A, WK(3), 1, 100);
    run(20);
    qd_app_key(&A, WK(6), 1, 100);
    run(20);
    qd_app_key(&A, WK(3), 0, 0);
    qd_app_key(&A, WK(6), 0, 0);
    run(300);
    shot("steps-latch", "STEPS, the latch (LFO) in P-LOCK: steps 20 and 23 selected (outlined), the overview stays");

    if (!failed)
        sheet();
    return failed;
}
