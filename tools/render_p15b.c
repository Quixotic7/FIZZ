/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.5b part (PLAN version 2.4, the owner after playing 0.4.1): the sound editor with the keyboard
 * (SCALE, FULL) and the keyboard's scale LEDs (drawn under the screen as the panel's keys), the lock chips (a step held:
 * filled; the track's view: outlined), the editor's head (the page dots first), the effects' OVERVIEW and ROUTING, the
 * latch mode's takeover, the record settings' EDIT KEYS, the pool's lock count; into OUT/p15b-*.png and
 * OUT/p15b-sheet.png */
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p15b-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p15b: cannot write %s\n", path);
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
    A.ui.knob_ms[k] = 0;
    qd_app_knob(&A, k, d);
    run(5);
}
static void set_key(int sc, int rt)
{
    qd_tset_t *ts = &qd_proj_pat(&A.proj, A.seq.cur)->trk[A.sel];
    ts->scale = (uint8_t)sc;
    ts->root = (uint8_t)rt;
    ts->key_global = 0;
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
    plutovg_canvas_fill_text(c, "FIZZ P1.5b: live play in the editor, the scale's LEDs, lock chips, FX overview and "
                                "routing, the latch modes (1.5x; the keys' LEDs under a screen)",
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
    snprintf(path, sizeof path, "%s/p15b-sheet.png", outdir);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens)\n", path, nshot);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
}

/* ---------------------------------------------------------------- the script */
int render_p15b(const char *out)
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

    /* 1 .. 3: the keyboard in the editor (SCALE, FULL) and the keyboard's scale */
    btap(QD_B_SOUND);
    btap(QD_B_REC);
    set_key(QD_SCALE_MAJOR, 0);
    A.ui.snd_page = qd_snd_page_of("TONE");             /* OPERATORS: TONE (P1.5l: B2) */
    qd_app_key(&A, WK(4), 1, 100);                      /* G3 playing */
    run(300);
    shot("keys-scale", "The editor + REC: SCALE keys (C major, G3 playing)", 1);
    qd_app_key(&A, WK(4), 0, 0);
    run(20);
    A.proj.rec_ekeys = 1;
    set_key(QD_SCALE_DORIAN, 2);
    black(3);                                           /* FILTERS (P1.5l: B4) */
    qd_app_key(&A, 9, 1, 100);                          /* D4 */
    run(300);
    shot("keys-full", "EDIT KEYS FULL: every key plays (D dorian; D4 playing)", 1);
    qd_app_key(&A, 9, 0, 0);
    run(20);
    btap(QD_B_SOUND);                                   /* back to the keyboard */
    set_key(QD_SCALE_MIN_PENTA, 9);
    run(200);
    shot("keyboard", "The keyboard: the scale's LEDs (A minor pentatonic: A 80 %, the rest of it 50 %)", 1);
    A.proj.rec_ekeys = 0;
    btap(QD_B_HOME);

    /* 4 .. 6: the lock chips and the head */
    btap(QD_B_SOUND);
    black(3);                                           /* FILTER: FREQ, RESO (B4) */
    qd_app_key(&A, WK(4), 1, 100);
    run(300);
    turn(QD_K_1, -12);
    turn(QD_K_2, 5);
    run(200);
    shot("chips-held", "A step held: its locks as filled chips (FREQ, RESO)", 0);
    qd_app_key(&A, WK(4), 0, 0);
    run(300);
    shot("chips-track", "Nothing held: values some step locks, outlined chips", 0);
    A.ui.snd_page = qd_snd_page_of("LFO 2");            /* LFO 2 (the dots first) */
    qd_app_key(&A, WK(0), 1, 100);
    run(300);
    turn(QD_K_ALGO, 2);
    turn(QD_K_3, 7);
    qd_app_key(&A, WK(0), 0, 0);
    run(300);
    shot("head", "The head: the group's dots first, then the page (ALGORITHM's lock: its chip)", 0);
    btap(QD_B_HOME);

    /* 7 .. 9: the effects' overview and routing */
    for (k = 0; k < QD_NTRACK; k++) {
        A.model.t[k].mix.chorus = (uint8_t)(10 + k * 13);
        A.model.t[k].mix.delay = (uint8_t)(100 - k * 11);
        A.model.t[k].mix.reverb = (uint8_t)(k * 17 % 120);
    }
    A.seq.mute = 1u << 6;
    btap(QD_B_TRACKS);
    wtap(2);
    turn(QD_K_1, 6);
    run(200);
    shot("fx-over", "FX: the OVERVIEW (W3: track 3 chosen)", 1);
    A.seq.mute = 0;
    black(5);
    run(200);
    shot("fx-route", "B6 ROUTING: ChoralRoot's chain (no feeds, the filter after)", 0);
    turn(QD_K_1, 90);
    turn(QD_K_2, 40);
    turn(QD_K_4, 1);
    run(200);
    shot("fx-route2", "ROUTING: DELAY > REVERB, CHORUS > DELAY, the filter BEFORE", 0);
    qd_fx_init();
    btap(QD_B_HOME);

    /* 10 .. 11: the latch modes */
    wtap(5);
    btap(QD_B_MUTE);
    bdown(QD_B_MUTE);                                   /* (P1.5h: the screen while held, SELECT turns it) */
    run(450);
    turn(QD_K_SELECT, 1);
    run(200);
    shot("lmode", "LFO held alone: the latch mode, SELECT: MODES", 1);
    bup(QD_B_MUTE);
    run(300);
    black(1);
    run(300);
    shot("lmode-vel", "MODES: B2 chose VEL, the knobs set the selection's", 1);
    bdown(QD_B_MUTE);
    run(450);
    turn(QD_K_SELECT, -1);
    bup(QD_B_MUTE);
    run(300);
    btap(QD_B_HOME);

    /* 12: the record settings */
    bdown(QD_B_FUNC);
    btap(QD_B_REC);
    bup(QD_B_FUNC);
    run(20);
    turn(QD_K_4, 1);
    run(200);
    shot("rec", "SHIFT + REC: KNOB 4 EDIT KEYS (FULL)", 0);
    turn(QD_K_4, -1);
    btap(QD_B_HOME);

    if (!failed)
        sheet();
    return failed;
}
