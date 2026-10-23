/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.3u part (was render_p13t.c; P1.3u: EDIT's chord name big and centred over the notes, VEL's value
 * big beside the mallet): the NOTE drawing redone (the keybed low, the notes as one line over it in their knobs'
 * colours, the scale's name only, no SELECTED, no "A = ALL"), in EDIT and on the held screen: one note, a chord the
 * namer knows, the owner's six-note cluster (C E F F# G G#), D DORIAN, the track's default; into OUT/p13u-note-*.png
 * and OUT/p13u-note-sheet.png. The notes and the scale are set through the pattern API (qd_ctl_pat). */
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
    A.ui.view = QD_VIEW_ROLL;   /* (P1.5s: EDIT is the power-on view; made on the PIANO ROLL) */
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
}
static void shot(const char *name, const char *caption)
{
    char path[1024];
    qd_app_frame(&A, lcd);
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13u-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13u: cannot write %s\n", path);
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
    plutovg_canvas_fill_text(c, "FIZZ P1.3u: EDIT: the chord's name big over the notes, VEL's value big; and the held NOTE screens (2x)", -1,
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
    snprintf(path, sizeof path, "%s/p13u-edit-sheet.png", outdir);
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
    vhome();                                    /* STEPS */
}

/* EDIT (the cursor on step s, NOTE) and the held screen (the PIANO ROLL, step s's key held) */
static void both(int s, const char *name, const char *caption)
{
    char nm[64], cap[112];
    if (A.ui.slatch)
        btap(QD_B_MUTE);
    while (A.ui.view != QD_VIEW_EDIT)
        vhome();
    black(UI_F_NOTE);
    turn(QD_K_ALGO, 1);
    while (A.ui.nheld && A.ui.held[0] != s)
        turn(QD_K_ALGO, A.ui.held[0] < s ? 1 : -1);
    run(400);
    snprintf(nm, sizeof nm, "note-%s-edit", name);
    snprintf(cap, sizeof cap, "EDIT: %s", caption);
    shot(nm, cap);
    btap(QD_B_MUTE);                                    /* the latch off */
    while (A.ui.view != QD_VIEW_ROLL)
        vhome();
    qd_ctl_set_page(&A, s / 16);
    run(300);
    whold(s % 16);
    run(300);
    snprintf(nm, sizeof nm, "note-%s-held", name);
    snprintf(cap, sizeof cap, "held: %s", caption);
    shot(nm, cap);
    wrelease(s % 16);
    run(100);
}

int render_p13u(const char *out)
{
    static const uint8_t ONE[1] = {64}, CHORD[3] = {62, 65, 69}, OWNER[6] = {60, 64, 65, 66, 67, 68};
    qd_pattern_t *P;
    outdir = out;
    nshot = 0;
    failed = 0;

    keys_two_pages();                                   /* A02 KEYS, page 2 (STEPS) */
    P = qd_ctl_pat(&A);
    qd_step_set_notes(P, T3, 16 + 0, ONE, 1);
    qd_step_set_notes(P, T3, 16 + 4, CHORD, 3);
    qd_step_set_notes(P, T3, 16 + 8, OWNER, 6);
    both(16 + 0, "one", "one note (E4)");
    both(16 + 4, "chord", "a chord the namer knows (Dm)");
    both(16 + 8, "six", "the owner's six-note cluster C E F F# G G#");
    P->trk[T3].scale = 3;                               /* D DORIAN */
    P->trk[T3].root = 2;
    P->trk[T3].key_global = 0;
    both(16 + 4, "dorian", "the chord in D DORIAN");
    while (A.ui.view != QD_VIEW_EDIT)
        vhome();
    run(300);
    shot("note-default-edit", "EDIT, nothing held: the track's default");
    if (A.ui.slatch)
        btap(QD_B_MUTE);
    black(UI_F_VEL);                                    /* VEL: the value big beside the mallet */
    turn(QD_K_ALGO, 1);
    while (A.ui.nheld && A.ui.held[0] != 20)
        turn(QD_K_ALGO, A.ui.held[0] < 20 ? 1 : -1);
    run(400);
    shot("edit-vel", "EDIT: VEL, the value big beside the mallet");

    if (!failed)
        sheet();
    return failed;
}
