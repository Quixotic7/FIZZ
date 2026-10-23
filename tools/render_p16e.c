/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.6e part: the MIDI FX screen's new states (a slot key held: lifted with its arrows; a knob: the move
 * previewed, the others making room; let go: dropped; F1 held: COPY, its source marked; F2 held: the hand, empty and
 * holding; REC: STEPS, its steps held + ALGORITHM: their MIDI FX lock, the frame; KEYS again). Into OUT/p16e-*.png and
 * OUT/p16e-sheet.png */
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p16e-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p16e: cannot write %s\n", path);
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
static void black(int b)
{
    kdown(BK(b));
    run(30);
    kup(BK(b));
    run(30);
}
static void home(void)
{
    btap(QD_B_HOME);
    run(1300);
}

int render_p16e(const char *out)
{
    qd_pattern_t *P;
    qd_mfx_group_t *G;
    int st;
    outdir = out;
    nshot = 0;
    failed = 0;
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    btap(QD_B_HOME);
    run(300);
    qd_seq_select(&A.seq, 4, 1);                        /* (A05) */
    run(50);
    P = &A.proj.pat[A.seq.cur];
    for (st = 0; st < 16; st += 4) {
        uint8_t n[1] = {(uint8_t)(48 + st)};
        qd_step_on(P, 0, st), qd_step_set_notes(P, 0, st, n, 1);
    }
    P->trk[0].mfx = 1;
    G = &A.proj.mfx[0];
    qd_mfx_slot_init(&G->slot[0], QD_MFX_ARP);
    qd_mfx_slot_init(&G->slot[1], QD_MFX_CHANCE);
    qd_mfx_slot_init(&G->slot[2], QD_MFX_HARM);
    qd_mfx_slot_init(&G->slot[3], QD_MFX_TRANSPOSE);
    home();
    btap(QD_B_KEYS);                                    /* the MIDI FX screen */
    run(300);
    /* 1..3: a slot held, moved, dropped */
    kdown(BK(1));
    run(QD_TAP_MS + 250);
    shot("hold", "B2 held: the CHANCE lifts, arrows either side (it can move)", 1);
    turn(QD_K_1, 2);
    run(80);
    shot("preview", "+ KNOB 1 +2: previewed in slot 4, the others making room (nothing moved yet)", 1);
    run(400);
    shot("previewed", "Still held: the preview stays; the chain plays as it was", 1);
    kup(BK(1));
    run(60);
    shot("drop", "Let go: dropped into slot 4 (one undo level)", 1);
    run(800);
    /* 4..7: F1, F2 held */
    bdown(QD_B_OCTDN);
    run(QD_TAP_MS + 100);
    shot("f1", "F1 held: COPY", 1);
    black(0);
    run(200);
    shot("f1-src", "F1 + B1: the ARP copied (its slot marked), the next B gets it", 1);
    bup(QD_B_OCTDN);
    run(300);
    bdown(QD_B_OCTUP);
    run(200);
    shot("f2-copy", "F2 held: the hand holds F1's copy (the next B: dropped)", 1);
    black(5);
    run(200);
    shot("f2-empty", "F2 + B6: dropped; the hand empty (the next B: picked up)", 1);
    black(2);
    run(200);
    shot("f2-hand", "F2 + B3: picked up, in the hand", 1);
    black(2);
    bup(QD_B_OCTUP);
    run(900);
    /* 8..11: STEPS (opened from the step view: P1.6f), KEYS */
    run(300);
    shot("steps", "STEPS (opened from the step view): the white keys the steps (REC dark)", 1);
    kdown(WK(4)), kdown(WK(8));
    run(100);
    turn(QD_K_ALGO, 3);
    run(250);
    shot("steps-lock", "W5, W9 held + ALGORITHM: their MIDI FX locked (group 2), the step's frame", 1);
    kup(WK(4)), kup(WK(8));
    run(200);
    btap(QD_B_REC);
    run(300);
    shot("keys", "REC: KEYS (REC lit): the keys play through the group", 1);
    btap(QD_B_REC);
    btap(QD_B_HOME);
    run(300);
    if (!failed)
        sheet_as("p16e-sheet.png", "FIZZ P1.6e: the MIDI FX screen (1.5x, the keys under each)");
    return failed;
}
