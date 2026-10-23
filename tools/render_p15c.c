/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.5c part: randomise's dice and champagne bottle at a few moments, UNDO / REDO's takeovers with
 * their glyphs, SHIFT's map with its key strip and LEDs, the snapshot, tap tempo, the click, double and clear; into
 * OUT/p15c-*.png and OUT/p15c-sheet.png */
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

#define NSHOT 24
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p15c-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p15c: cannot write %s\n", path);
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

static void sheet_as(const char *file, const char *title)
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
    plutovg_canvas_fill_text(c, title,
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
    snprintf(path, sizeof path, "%s/%s", outdir, file);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens)\n", path, nshot);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
}

/* ---------------------------------------------------------------- the script */
static void kdown(int k) { qd_app_key(&A, k, 1, 100); }
static void kup(int k) { qd_app_key(&A, k, 0, 0); }
static void black(int b)
{
    kdown(BK(b));
    run(20);
    kup(BK(b));
    run(10);
}
static void wtap(int w)
{
    kdown(WK(w));
    run(30);
    kup(WK(w));
    run(20);
}
static void shift_down(void)
{
    bdown(QD_B_FUNC);
    run(20);
}
static void shift_up(void)
{
    bup(QD_B_FUNC);
    run(600);
}
static void undo_redo(int dir)
{
    shift_down();
    bdown(dir < 0 ? QD_B_OCTDN : QD_B_OCTUP);
    run(20);
    bup(dir < 0 ? QD_B_OCTDN : QD_B_OCTUP);
    run(20);
    bup(QD_B_FUNC);
    run(120);
}
int render_p15c(const char *out)
{
    int i;
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
    bdown(QD_B_MIX);                                    /* (an empty pattern, A05) */
    run(20);
    kdown(WK(4));
    run(30);
    kup(WK(4));
    bup(QD_B_MIX);
    run(100);
    for (i = 0; i < 16; i += 3)
        wtap(i);
    run(600);
    btap(QD_B_SOUND);                                   /* the editor: the dice */
    A.ui.snd_page = qd_snd_page_of("FILTER");
    run(600);
    kdown(BK(8));                                       /* (P1.6d: held a second, the dice shaking, then they roll) */
    run(700);
    shot("dice-1", "B9 held: the dice shaking (0.7 s)", 0);
    run(400);
    shot("dice-2", "a second: they roll, the page new", 0);
    kup(BK(8));
    run(250);
    shot("dice-3", "settling", 0);
    run(300);
    shot("dice-page", "FILTER's values new", 0);
    kdown(BK(9));                                       /* the bottle */
    run(300);
    shot("bottle-1", "B10 held: the bottle filling (0.3 s)", 0);
    run(500);
    shot("bottle-2", "bubbles rising (0.8 s)", 0);
    run(600);
    shot("bottle-3", "nearly full, the cork pushed (1.4 s)", 0);
    run(160);
    shot("burst-1", "1.5 s: the cork pops", 0);
    run(200);
    shot("burst-2", "the burst (the whole sound new)", 0);
    kup(BK(9));
    run(800);
    shot("sound", "the new sound", 0);
    undo_redo(-1);
    shot("undo-random", "SHIFT + F1: UNDO (a randomise: the dice)", 0);
    run(1200);
    undo_redo(1);
    shot("redo-random", "SHIFT + F2: REDO", 0);
    run(1200);
    btap(QD_B_HOME);
    run(300);
    shift_down();
    black(0);
    run(60);
    shot("snap", "SHIFT + B1: the snapshot stored", 0);
    shift_up();
    run(1500);
    shift_down();                                       /* SHIFT's map: the snapshot stored, nothing copied */
    run(1300);
    shot("shift-map", "SHIFT held: its map, only the shortcuts lit (B5 dim: nothing copied)", 1);
    shift_up();
    wtap(1), wtap(2);
    run(600);
    shift_down();
    black(2);
    run(60);
    shot("recall", "SHIFT + B3: recalled at once", 0);
    shift_up();
    undo_redo(-1);
    shot("undo-snap", "UNDO a recall (the snapshot's frame)", 0);
    run(1200);
    shift_down();
    for (i = 0; i < 4; i++) {
        black(5);
        run(470);
    }
    shot("tap", "SHIFT + B6 tapped 500 ms apart: 120 BPM", 0);
    kdown(BK(6));
    run(30);
    A.ui.knob_ms[0] = 0;
    qd_app_knob(&A, QD_K_1, -3);
    run(60);
    shot("click", "SHIFT + B7 held + a knob: the click's level", 0);
    kup(BK(6));
    run(30);
    black(8);
    run(60);
    shot("double", "SHIFT + B9: the track doubled", 0);
    black(10);
    run(60);
    shot("clear-armed", "SHIFT + B11 once: armed (B11 blinking)", 1);
    black(10);
    run(60);
    shot("cleared", "again: the track cleared", 0);
    shift_up();
    undo_redo(-1);
    shot("undo-clear", "UNDO a clear", 0);
    run(1200);
    for (i = 0; i < 9; i++)
        undo_redo(-1);
    shot("undo-none", "nothing left to undo: the dash", 0);
    run(1200);
    if (!failed)
        sheet_as("p15c-sheet.png", "FIZZ P1.5c: randomise (dice, champagne), undo / redo, SHIFT's keys (1.5x)");
    return failed;
}
