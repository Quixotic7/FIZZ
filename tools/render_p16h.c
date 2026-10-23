/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.6g part: the ARP picture's seventeen PATTERNs (on C E G over two octaves: the real walk), the arp
 * playing a held chord, its lanes' rests and transposes, a PATTERN turned (the morph); the FILTER page's ALGORITHM
 * popup; recording in the sound editor (its cue). Into OUT/p16h-*.png, OUT/p16h-arp-sheet.png and OUT/p16h-sheet.png */
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p16h-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p16h: cannot write %s\n", path);
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
    const int cell = (int)(240.f * sc), leds_h = 60, cell_h = cell + 34 + leds_h, pad = 22, cols = 4;
    const int rows = (nshot + cols - 1) / cols, W = pad + cols * (cell + pad), H = 50 + rows * (cell_h + pad);
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
static void turn(int knob, int d)
{
    A.ui.knob_ms[knob] = 0;
    qd_app_knob(&A, knob, d);
    run(5);
}
static void home(void)
{
    btap(QD_B_HOME);
    run(1300);
}

static void wtap(int w)
{
    kdown(WK(w));
    run(30);
    kup(WK(w));
    run(10);
}

int render_p16h(const char *out)
{
    qd_pattern_t *P;
    outdir = out;
    nshot = 0;
    failed = 0;
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    btap(QD_B_HOME);
    run(300);
    qd_seq_select(&A.seq, 4, 1);
    run(50);
    home();
    wtap(0), wtap(2), wtap(5), wtap(9);                 /* a few steps */
    P = qd_ctl_pat(&A);
    qd_lock_set(P, 0, 0, QD_LK_LEVEL, 70);
    qd_lock_set(P, 0, 0, QD_LK_DELAY, 90);
    btap(QD_B_PAGE);
    run(400);
    shot("mix", "The mix: no back key (F1 is no back here)", 1);
    btap(QD_B_MUTE);                                    /* the latch (P1.6h: on the mix, empty), its selection made here */
    kdown(WK(1)), kdown(WK(2)), kdown(WK(3)), run(30);
    kup(WK(1)), kup(WK(2)), kup(WK(3)), run(10);
    run(300);
    shot("mix-sel", "LFO, W2..W4 together: three selected (the frame, the head's 3)", 1);
    turn(QD_K_1, 6);
    run(300);
    shot("mix-lock", "KNOB 1: LEVEL locked on them: the chip on the fader and in the head", 1);
    btap(QD_B_PAGE);
    turn(QD_K_1, 9);
    run(300);
    shot("sends-lock", "SENDS, KNOB 1: CHORUS locked: the chip on its bar", 1);
    btap(QD_B_MUTE);                                    /* the latch off */
    btap(QD_B_PAGE);
    run(200);
    bdown(QD_B_OCTDN);
    run(200);
    shot("mix-f1", "The mix, F1 held: F1's picture (the pages, W copies)", 1);
    wtap(0);
    run(200);
    shot("mix-f1-copied", "F1 + W1: copied (ringed on page 1), the next W pastes", 1);
    wtap(4);
    bup(QD_B_OCTDN);
    run(300);
    btap(QD_B_OCTUP);
    bdown(QD_B_OCTUP);
    run(200);
    wtap(4);
    run(200);
    shot("mix-f2", "F2 + W5: picked up, in the hand (its LEVEL and DELAY locks with it)", 1);
    wtap(12);
    bup(QD_B_OCTUP);
    run(300);
    shot("mix-after", "Dropped on W13; F2 let go: the mix", 1);
    btap(QD_B_TRACKS);                                  /* the effects' overview */
    btap(QD_B_MUTE);
    kdown(WK(12)), run(30), kup(WK(12)), run(10);
    turn(QD_K_2, -20);
    run(300);
    shot("fx-lock", "The effects, step 13 selected, KNOB 2: DELAY's send locked (the chip on its bar)", 1);
    btap(QD_B_MUTE);
    bdown(QD_B_OCTDN);
    run(200);
    shot("fx-f1", "The effects, F1 held: F1's picture", 1);
    bup(QD_B_OCTDN);
    run(100);
    btap(QD_B_SOUND);
    run(300);
    bdown(QD_B_OCTUP);
    run(200);
    wtap(2);
    run(200);
    shot("snd-f2", "The sound editor, F2 + W3: the hand", 1);
    bup(QD_B_OCTUP);
    run(100);
    btap(QD_B_HOME);
    run(300);
    btap(QD_B_KEYS);                                    /* MIDI FX, STEPS */
    run(400);
    bdown(QD_B_OCTDN);
    run(300);
    wtap(0);
    run(200);
    shot("mfx-f1", "MIDI FX, STEPS, F1 + W1: the step copied (a card), PASTE next", 1);
    bup(QD_B_OCTDN);
    run(100);
    btap(QD_B_OCTUP);
    bdown(QD_B_OCTUP);
    run(300);
    shot("mfx-f2-empty", "F2 held, the hand empty (no group: the steps'): PICK UP", 1);
    wtap(5);
    run(200);
    shot("mfx-f2", "F2 + W6: the step in the hand", 1);
    bup(QD_B_OCTUP);
    run(200);
    btap(QD_B_HOME);
    run(300);
    if (!failed)
        sheet_as("p16h-sheet.png", "FIZZ P1.6h (1.5x, the keys under each)");
    return failed;
}
