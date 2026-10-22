/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.4b part: the track settings (SEL tapped), the mix and its sends (ENV), the effects' six pages (FX
 * tapped), playing A02; into OUT/p14b-*.png and OUT/p14b-sheet.png */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ctl.h"
#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"
#include "ui.h"
#include "qd_fx.h"

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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p14b-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p14b: cannot write %s\n", path);
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
    plutovg_font_face_t *f = plutovg_font_face_load_from_data(qd_fonts_gen[3].data, qd_fonts_gen[3].len, 0, NULL, NULL);
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

int render_p14b(const char *out)
{
    int p;
    char nm[32], cap[112];
    outdir = out;
    nshot = 0;
    failed = 0;

    at(A02, T3, UI_F_NOTE);                             /* A02, track 3 KEYS, playing */
    btap(QD_B_PLAY);
    run(500);
    btap(QD_B_PATTERN);
    run(150);
    shot("trk-1", "SEL tapped: track 3's settings (SCALE, ROOT, FOLLOW KEY, MIDI CH)");
    turn(QD_K_1, 3);
    turn(QD_K_2, 2);
    run(150);
    shot("trk-1b", "KNOB 1 / 2: D DORIAN");
    btap(QD_B_PATTERN);
    run(150);
    shot("trk-2", "SEL again: VOICES (the limit, the ones sounding white), SWING, PAGE FOLLOW");
    btap(QD_B_HOME);
    btap(QD_B_PAGE);
    run(300);
    shot("mix", "ENV: the mix, track 3 selected (its parts in the knobs' colours), playing");
    bdown(QD_B_PATTERN);                                /* SEL held + W7: track 7 muted */
    run(300);
    qd_app_key(&A, WK(6), 1, 100);
    run(30);
    qd_app_key(&A, WK(6), 0, 0);
    bup(QD_B_PATTERN);
    run(20);
    turn(QD_K_2, -20);
    turn(QD_K_4, 30);
    run(200);
    shot("mix-2", "track 7 muted (SEL + W7); track 3: PAN -20, REVERB +30");
    btap(QD_B_PAGE);
    run(200);
    shot("sends", "ENV again: the sends, every track's chorus, delay, reverb");
    btap(QD_B_HOME);
    btap(QD_B_TRACKS);
    for (p = 0; p < QD_FX_NPAGE; p++) {                 /* (P1.5b: OVERVIEW first, ROUTING last) */
        run(p ? 400 : 300);
        snprintf(nm, sizeof nm, "fx-%d", p);
        snprintf(cap, sizeof cap, "FX: %s", QD_FX_PAGES[p].name);
        shot(nm, cap);
        if (p < QD_FX_NPAGE - 1)
            btap(QD_B_TRACKS);
    }
    black(4);                                           /* B5 MASTER: a low cut and on */
    turn(QD_K_1, 30);
    turn(QD_K_2, -40);
    turn(QD_K_4, 1);
    run(200);
    shot("fx-master-on", "MASTER: LOW CUT, HIGH CUT, ON");
    turn(QD_K_4, -1);
    turn(QD_K_1, -30);
    turn(QD_K_2, 40);
    black(1);                                           /* B2 DELAY */
    turn(QD_K_3, 50);
    btap(QD_B_TRACKS);
    turn(QD_K_2, 1);
    for (p = 0; p < QD_FX_NPAGE - 1; p++)               /* round to DELAY */
        btap(QD_B_TRACKS);
    run(400);
    shot("fx-delay-2", "DELAY: FEEDBACK +50 (FX round again)");
    btap(QD_B_HOME);
    btap(QD_B_PLAY);

    if (!failed)
        sheet("p14b-sheet.png", "FIZZ P1.4b: track settings, the mix, the effects (2x)", 0, nshot, 3, 2.f);
    return failed;
}
