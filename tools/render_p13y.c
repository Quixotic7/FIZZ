/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* render_frames' P1.3y part: the control fixes (ALGORITHM the cursor in every view, LEAP, FX + ALGORITHM the track
 * big, PRESETS' sound picture in EDIT, the TRACK frame on a default) and the owner's additions (the record settings,
 * the global settings' knob layouts, SHIFT's map); into OUT/p13y-*.png, OUT/p13y-sheet.png and the track-vs-step sheet
 * OUT/p13y-track-vs-step.png (each mode: its track default, framed, beside a step held) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ctl.h"
#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"
#include "ui.h"

#define NSHOT 64
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13y-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13y: cannot write %s\n", path);
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

/* ---------------------------------------------------------------- the sheets */
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

/* shots i0 .. i1 - 1, cols a row, at scale; captions over each */
static void sheet(const char *file, const char *title, int i0, int i1, int cols, float scale)
{
    const int cell_w = (int)(240.f * scale), cell_h = (int)(240.f * scale) + 34, pad = 18;
    const int n = i1 - i0, rows = (n + cols - 1) / cols;
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
    plutovg_canvas_fill_text(c, title, -1, PLUTOVG_TEXT_ENCODING_UTF8, (float)pad, 32.f);
    for (i = 0; i < n; i++) {
        const float x = (float)(pad + (i % cols) * (cell_w + pad)), y = (float)(50 + (i / cols) * (cell_h + pad));
        plutovg_surface_t *ours;
        snprintf(path, sizeof path, "%s/%s", outdir, shots[i0 + i].file);
        ours = plutovg_surface_load_from_image_file(path);
        plutovg_canvas_set_font_size(c, 13.f);
        plutovg_canvas_set_rgb(c, 0.85f, 0.85f, 0.85f);
        plutovg_canvas_fill_text(c, shots[i0 + i].caption, -1, PLUTOVG_TEXT_ENCODING_UTF8, x, y + 20.f);
        if (ours) {
            draw_surface(c, ours, x, y + 30.f, scale);
            plutovg_surface_destroy(ours);
        }
    }
    snprintf(path, sizeof path, "%s/%s", outdir, file);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens)\n", path, n);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
}

/* ---------------------------------------------------------------- the script */
enum { A01, A02, A03 };
enum { T1, T2, T3, T4 };
static void whold(int w)
{
    qd_app_key(&A, WK(w), 1, 100);
    run(300);
}
static void wrel(int w)
{
    qd_app_key(&A, WK(w), 0, 0);
    run(20);
}
static void shome(void)                                 /* SHIFT + HOME: the next view */
{
    bdown(QD_B_FUNC);
    run(10);
    btap(QD_B_HOME);
    bup(QD_B_FUNC);
    run(10);
}
static void menu(int button)                            /* SHIFT + REC / SEL: a settings menu */
{
    bdown(QD_B_FUNC);
    run(10);
    bdown(button);
    run(20);
    bup(button);
    bup(QD_B_FUNC);
    run(60);
}

int render_p13y(const char *out)
{
    static const char *const MODE[10] = {"NOTE", "VEL", "LEN", "MICRO", "RATCH", "CHNC", "COND", "TRIG", "FUNC",
                                         "SOUND"};
    int m, split;
    char nm[32], cap[112];
    outdir = out;
    nshot = 0;
    failed = 0;

    /* ALGORITHM the cursor in the PIANO ROLL */
    at(A02, T3, UI_F_VEL);
    turn(QD_K_ALGO, 1);
    turn(QD_K_ALGO, 4);
    run(100);
    shot("roll-cursor", "PIANO ROLL, ALGORITHM: the cursor (the latch on), its step outlined");
    btap(QD_B_HOME);
    run(100);
    shot("home", "HOME: the latch ended, the PIANO ROLL still (HOME never cycles)");
    /* FUNC: LEAP */
    black(UI_F_FUNC);
    whold(2);
    black(7);                                           /* B8: LEAP +2 */
    run(100);
    shot("leap-held", "FUNC, a step held, B8: LEAP +2");
    turn(QD_K_2, 3);
    run(100);
    shot("leap-knob2", "KNOB 2 +3: LEAP +5 (its band)");
    wrel(2);
    whold(6);
    qd_app_key(&A, BK(8), 1, 100);                      /* B9 held + W12: JUMP to step 12 */
    run(30);
    qd_app_key(&A, WK(11), 1, 100);
    run(30);
    qd_app_key(&A, WK(11), 0, 0);
    run(20);
    qd_app_key(&A, BK(8), 0, 0);
    run(100);
    shot("jump-b9", "B9 held + W12: JUMP to step 12 (step 7 still held)");
    wrel(6);
    shome();
    run(400);
    shot("steps-leap", "STEPS, FUNC: LEAP +5 on step 3, JUMP 12 on step 7");
    shome();
    black(UI_F_VEL);
    run(400);
    shot("edit-default", "EDIT, VEL, nothing held: the track's default, the TRACK frame");
    whold(0);
    turn(QD_K_PRESETS, 1);
    run(100);
    shot("edit-presets", "EDIT, a step held, PRESETS: the sound picture in the top");
    wrel(0);
    run(1300);
    shome();
    run(300);
    /* FX held + ALGORITHM: the track big */
    bdown(QD_B_TRACKS);
    run(20);
    turn(QD_K_ALGO, 1);
    run(100);
    shot("fx-track", "FX held + ALGORITHM: track 4 big, in its colour");
    turn(QD_K_ALGO, 2);
    run(100);
    shot("fx-track6", ".. + 2: track 6, the last");
    bup(QD_B_TRACKS);
    run(1300);
    /* the record settings */
    menu(QD_B_REC);
    shot("rec-menu", "SHIFT + REC: the record settings (LIVE, 0 %, REPLACE)");
    turn(QD_K_1, 1);
    turn(QD_K_2, 10);
    turn(QD_K_3, 1);
    run(60);
    shot("rec-menu2", "KNOB 1 STEP, KNOB 2 50 %, KNOB 3 OVERDUB");
    btap(QD_B_OCTDN);
    /* the global settings */
    menu(QD_B_PATTERN);
    shot("set-a", "SHIFT + SEL: the settings, KNOBS A (P1.3y)");
    turn(QD_K_1, 1);
    run(60);
    shot("set-b", "KNOBS B (P1.3q): the PIANO ROLL / STEPS, EDIT");
    turn(QD_K_1, 1);
    run(60);
    shot("set-c", "KNOBS C: SELECT the cursor, ALGORITHM the page");
    turn(QD_K_1, 1);
    turn(QD_K_2, -1);
    run(60);
    shot("set-d", "KNOBS D, PAGE FOLLOW off");
    turn(QD_K_1, -3);
    turn(QD_K_2, 1);
#if QD_PKG_BLE
    turn(QD_K_1, 1);                                    /* (2026-10-27, the Bluetooth package: SETTINGS' page 2) */
    turn(QD_K_2, -1);
    run(60);
    shot("set-p1", "the Bluetooth package: SETTINGS page 1 (its page dots)");
    turn(QD_K_SELECT, 1);
    qd_app_ble_state(&A, 1);
    run(60);
    shot("set-ble-wait", "SELECT: page 2, BLUETOOTH ON, WAITING");
    qd_app_ble_state(&A, 2);
    run(60);
    shot("set-ble-conn", "CONNECTED");
    turn(QD_K_1, -1);
    qd_app_ble_state(&A, 0);
    run(60);
    shot("set-ble-off", "KNOB 1: BLUETOOTH OFF, the radio OFF");
    turn(QD_K_1, 1);
    turn(QD_K_SELECT, -1);
#endif
    btap(QD_B_HOME);
    /* SHIFT's map */
    bdown(QD_B_FUNC);
    run(150);
    shot("shift", "SHIFT held, the step view: HOME, REC and SEL now there");
    bup(QD_B_FUNC);
    run(50);

    split = nshot;
    /* each mode: the track's default (the mode key held: framed) beside a step held */
    for (m = 0; m < 10; m++) {
        at(A02, T3, m);
        qd_app_key(&A, BK(m), 1, 100);
        run(320);
        snprintf(nm, sizeof nm, "tvs-%d-track", m);
        snprintf(cap, sizeof cap, "%s: the TRACK default (key held)", MODE[m]);
        shot(nm, cap);
        qd_app_key(&A, BK(m), 0, 0);
        run(20);
        whold(0);
        run(100);
        snprintf(nm, sizeof nm, "tvs-%d-step", m);
        snprintf(cap, sizeof cap, "%s: step 1 held", MODE[m]);
        shot(nm, cap);
        wrel(0);
    }

    if (!failed) {
        sheet("p13y-sheet.png", "FIZZ P1.3y: the control fixes, the record and global settings (2x)", 0, split, 3, 2.f);
        sheet("p13y-track-vs-step.png", "FIZZ P1.3y: TRACK (the default, framed) vs STEP, each mode (1.5x)", split,
              nshot, 4, 1.5f);
    }
    return failed;
}
