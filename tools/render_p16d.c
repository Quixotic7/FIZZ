/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* render_frames' P1.6d part: every screen P1.6d made or changed, by the gestures that show them (the step latch's
 * default keys, the dice's hold, TONE's MIX cell, the FILTER machines' popup, the sends' locks on the mix and the
 * effects, automation recording's cue, the record settings' page 2, the track settings' page 3 and the track's name
 * typed, the kinds' icons where the tracks' names show, ARP held, the groups' glide, a slot moved, F1 / F2 on the
 * slots, SHIFT's map). Into OUT/p16d-*.png and OUT/p16d-sheet.png */
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

#define NSHOT 30
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p16d-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p16d: cannot write %s\n", path);
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
static void wtap(int w)
{
    kdown(WK(w));
    run(30);
    kup(WK(w));
    run(30);
}
static void home(void)
{
    btap(QD_B_HOME);
    run(1300);
}

int render_p16d(const char *out)
{
    qd_pattern_t *P;
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
    P = qd_proj_pat(&A.proj, A.seq.cur);
    for (st = 0; st < 16; st += 4) {
        uint8_t n[1] = {(uint8_t)(48 + st)};
        qd_step_on(P, 0, st), qd_step_set_notes(P, 0, st, n, 1);
    }
    home();
    /* 1: the step latch, VALUES, nothing selected: a black key the track's default */
    btap(QD_B_MUTE);                                    /* (LFO: the latch, step 1 selected) */
    run(50);
    wtap(0);                                            /* (tapped again: nothing selected) */
    run(300);
    bdown(QD_B_MUTE), run(30), black(1), bup(QD_B_MUTE), run(30);   /* (LFO + B2: VEL) */
    black(6);
    run(250);
    shot("latchdef", "The latch, nothing selected: B7 = the track's default VEL", 1);
    btap(QD_B_MUTE);
    home();
    /* 2, 3: the dice held: shaking, rolled at 1 s */
    btap(QD_B_SOUND);
    run(200);
    kdown(BK(8));
    run(700);
    shot("diceshake", "B9 held: the dice shake harder (0.7 s)", 1);
    run(420);
    shot("dicerolled", "B9 held 1 s: rolled, the page randomised", 1);
    kup(BK(8));
    run(1500);
    /* 4: TONE, its MIX in the lower right */
    A.ui.snd_page = qd_snd_page_of("TONE");
    A.ui.snd_knob = -1;
    run(300);
    shot("tone", "TONE: MIX in its knob's cell (lower right)", 1);
    /* 5: the FILTER key held: the machines */
    kdown(BK(3));
    run(QD_FPOP_MS + 50);
    turn(QD_K_ALGO, 1);
    run(200);
    shot("fpop", "FILTER held: the six machines (ALGORITHM / a knob / B1..B6)", 1);
    kup(BK(3));
    run(100);
    home();
    /* 6, 7: the mix, a step held: its locks */
    btap(QD_B_PAGE);
    run(100);
    kdown(WK(4));
    run(300);
    turn(QD_K_1, -24);
    turn(QD_K_4, 30);
    run(200);
    shot("mixlock", "The mix, W5 held: LEVEL and REVERB locked (the frame, the chips)", 1);
    kup(WK(4));
    run(300);
    shot("mixany", "Let go: the track's values, the chips outlined (some step locks)", 1);
    home();
    /* 8, 9: the effects, the latch's selection: OVERVIEW KNOB 1, DELAY's SEND */
    btap(QD_B_MUTE);
    run(50);
    wtap(8);
    run(300);
    btap(QD_B_TRACKS);
    run(100);
    turn(QD_K_1, 40);
    run(200);
    shot("fxover", "The effects, the latch's selection (step 9): OVERVIEW's CHORUS locked", 1);
    black(1);
    turn(QD_K_4, 25);
    run(200);
    shot("fxsend", "DELAY: SEND locked on the step", 1);
    btap(QD_B_HOME);
    run(50);
    home();
    /* 10, 11: automation recording: playing, REC held, a knob */
    btap(QD_B_PLAY);
    run(600);
    bdown(QD_B_REC);
    run(50);
    turn(QD_K_1, -6);
    run(80);
    shot("autostep", "Playing, REC held + KNOB 1: recorded into the steps (the red dot, the ring)", 1);
    run(600);
    turn(QD_K_1, -6);
    run(150);
    bup(QD_B_REC);
    run(1500);                                          /* (P1.6d.1: the steps view's KNOB 1 takeover gone first) */
    btap(QD_B_SOUND);
    run(200);
    bdown(QD_B_REC);
    run(50);
    turn(QD_K_3, 5);
    run(100);
    shot("autosnd", "The sound editor, REC held + KNOB 3: automation", 1);
    bup(QD_B_REC);
    run(50);
    btap(QD_B_PLAY);
    run(50);
    home();
    /* 12, 13: the record settings' page 2 */
    bdown(QD_B_FUNC), run(20), btap(QD_B_REC), btap(QD_B_REC), bup(QD_B_FUNC), run(20);
    run(500);
    shot("recauto", "SHIFT + REC twice: page 2, AUTOMATION ON", 1);
    turn(QD_K_1, -1);
    run(300);
    shot("recautooff", "AUTOMATION OFF: armed recording records notes only", 1);
    turn(QD_K_1, 1);
    home();
    /* 14, 15: the track settings' page 3; the name typed */
    qd_ctl_select_track(&A, 6);
    btap(QD_B_PATTERN), btap(QD_B_PATTERN), btap(QD_B_PATTERN);
    turn(QD_K_1, 2);
    run(300);
    shot("trkkind", "SEL three times: KIND (PAD: the name follows), NAME", 1);
    turn(QD_K_2, 1);
    run(100);
    black(6), wtap(4), black(0), black(3), black(4);    /* G P A D E .. */
    run(300);
    shot("trkname", "KNOB 2: TRACK NAME, typed", 1);
    btap(QD_B_SAVE);
    run(300);
    shot("trknamed", "SAVE: the name kept (typed: a kind keeps it)", 1);
    home();
    /* 16..19: the kinds' icons where the names show */
    bdown(QD_B_PATTERN);
    run(QD_TAP_MS + 300);
    shot("picker", "SEL held: the track picker, each track's kind", 1);
    bup(QD_B_PATTERN);
    run(100);
    home();
    btap(QD_B_PAGE);
    run(300);
    shot("mixicons", "The mix: the kinds over the faders", 1);
    home();
    qd_ctl_select_track(&A, 1);
    btap(QD_B_REC);
    run(300);
    shot("header", "The keyboard's head: the track's kind", 1);
    btap(QD_B_REC);
    bdown(QD_B_TRACKS);
    run(20);
    turn(QD_K_ALGO, 1);
    run(200);
    shot("trackbig", "FX held + ALGORITHM: the track, its kind", 1);
    bup(QD_B_TRACKS);
    run(100);
    home();
    /* 20: ARP held: the arp ON */
    qd_ctl_select_track(&A, 0);
    bdown(QD_B_KEYS);
    run(QD_ARP_HOLD_MS + 250);
    bup(QD_B_KEYS);
    shot("arphold", "ARP held: the arp ON (big); a tap opens the MIDI FX screen", 1);
    home();
    /* 21..24: the MIDI FX screen */
    qd_mfx_slot_init(&A.proj.mfx[0].slot[1], QD_MFX_CHANCE);
    qd_mfx_slot_init(&A.proj.mfx[0].slot[2], QD_MFX_HARM);
    btap(QD_B_KEYS);
    run(200);
    black(2);
    run(300);
    turn(QD_K_ALGO, 1);
    run(90);
    turn(QD_K_ALGO, 1);
    run(110);
    shot("glide", "ALGORITHM, two detents: the rails stay up, the square glides on (slot 3 kept)", 1);
    turn(QD_K_ALGO, -2);
    run(1400);
    kdown(BK(0));
    run(30);
    turn(QD_K_1, 2);
    run(150);
    shot("slotmove", "B1 held + KNOB 1 +2: the ARP moved to slot 3 (the lift)", 1);
    kup(BK(0));
    run(900);
    bdown(QD_B_OCTDN);
    run(20);
    black(0), black(4), black(5);
    run(1400);
    shot("f1", "F1 + B1, B5, B6: copied, then pasted", 1);
    bup(QD_B_OCTDN);
    run(900);
    bdown(QD_B_OCTUP);
    run(20);
    black(1), black(6);
    run(160);
    shot("f2", "F2 + B2, B7: picked up, dropped (Form's)", 1);
    bup(QD_B_OCTUP);
    run(900);
    btap(QD_B_KEYS);
    run(200);
    /* 25: SHIFT's map: ARP = FILL */
    bdown(QD_B_FUNC);
    run(400);
    shot("shift", "SHIFT held: ARP's glyph FILL (SHIFT + ARP held)", 1);
    bup(QD_B_FUNC);
    run(200);
    if (!failed)
        sheet_as("p16d-sheet.png", "FIZZ P1.6d (1.5x, the keys under each)");
    return failed;
}
