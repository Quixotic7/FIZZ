/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* render_frames' P1.3b part: the screens of the sequencer controls v2 (PLAN.md §8b) rendered from a real state reached
 * by panel input only (presses, holds, turns; the demo patterns A01..A03 as the unit has them, and edits made with the
 * gestures of core/app/ctl.h), each from a fresh unit, into OUT/p13b-*.png; then OUT/p13b-sheet.png: each screen at
 * 2x beside line.html's mock of it (mockups/png/line-*.png) where there is one, with captions.
 *
 * The shots: the step view's ten lanes (NOTE .. LOCKS) on the demos, with steps overriding the mode and a track
 * default changed; a page with its own start..end in a loop of two pages; every hold mode with a step held and its
 * value strip; ALGORITHM's default takeover; SELECT's page picker; PRESETS' pool picker (the track's sound, a step's
 * lock); SHIFT + ALGORITHM (the scale) and SHIFT + PRESETS (the root); the F1, F2 and F3 maps; the SHIFT map; the
 * keyboard, step recording and live recording. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"
#include "ui.h"

#define NSHOT 48
static qd_app_t A;
static uint32_t now;
static uint64_t frames_done;
static uint16_t lcd[QD_W * QD_H];
static const char *outdir, *mockdir;
static struct {
    char file[64], caption[96], mock[64];
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
static void shot(const char *name, const char *caption, const char *mock)
{
    char path[1024];
    qd_app_frame(&A, lcd);
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13b-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(shots[nshot].mock, sizeof shots[nshot].mock, "%s", mock ? mock : "");
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13b: cannot write %s\n", path);
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
static void black(int b)                                /* B1..B11 tapped (b 0..10) */
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
static void whold(int w)                                /* a step held: its key down 300 ms */
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
static void track(int t)                                /* TRACKS held + white key t: select (t 0..7) */
{
    bdown(QD_B_TRACKS);
    run(10);
    wtap(t);
    bup(QD_B_TRACKS);
    run(20);
}
/* a fresh unit on pattern k (0 A01 ..), the step view, track t (0..7), hold mode m */
static void at(int k, int t, int m)
{
    fresh();
    btap(QD_B_HOME);
    bdown(QD_B_MIX);                                    /* (P1.3c: printed SEQ held + W: the pattern) */
    qd_app_key(&A, WK(k), 1, 100);
    run(20);
    qd_app_key(&A, WK(k), 0, 0);
    bup(QD_B_MIX);
    run(20);
    track(t);
    black(m);
    run(20);
}
static void play(uint32_t ms)
{
    btap(QD_B_PLAY);
    run(ms);
}
/* track t's step s (0-based, on its page) held in mode m: the page by SELECT first */
static void held(int k, int t, int s, int m, uint32_t play_ms)
{
    at(k, t, m);
    if (play_ms)
        play(play_ms);
    if (s >= 16) {
        turn(QD_K_SELECT, s / 16);
        run(QD_TK_MS + QD_TK_FADE_MS + 20);
    }
    whold(s % 16);
    run(300);
}
static void settle(void) { run(QD_TK_MS + QD_TK_FADE_MS + 20); }   /* a takeover gone */

/* ---------------------------------------------------------------- the sheet */
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

/* the shots with a mock: ours at 2x beside the mock at 2x; the others two a row-cell at 2x */
static void sheet(void)
{
    const int cell_w = 480 * 2 + 12, cell_h = 480 + 34, cols = 2, pad = 16;
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
    plutovg_canvas_fill_text(c, "FIZZ P1.3b: each screen from the core (left) beside its line.html mock (right)",
                             -1, PLUTOVG_TEXT_ENCODING_UTF8, (float)pad, 32.f);
    for (i = 0; i < nshot; i++) {
        const float x = (float)(pad + (i % cols) * (cell_w + pad)), y = (float)(50 + (i / cols) * (cell_h + pad));
        plutovg_surface_t *ours, *mock = 0;
        snprintf(path, sizeof path, "%s/%s", outdir, shots[i].file);
        ours = plutovg_surface_load_from_image_file(path);
        if (shots[i].mock[0]) {
            snprintf(path, sizeof path, "%s/%s", mockdir, shots[i].mock);
            mock = plutovg_surface_load_from_image_file(path);
        }
        plutovg_canvas_set_font_size(c, 14.f);
        plutovg_canvas_set_rgb(c, 0.85f, 0.85f, 0.85f);
        plutovg_canvas_fill_text(c, shots[i].caption, -1, PLUTOVG_TEXT_ENCODING_UTF8, x, y + 20.f);
        if (ours) {
            draw_surface(c, ours, x, y + 30.f, 2.f);
            plutovg_surface_destroy(ours);
        }
        if (mock) {
            draw_surface(c, mock, x + 480.f + 12.f, y + 30.f, 2.f);
            plutovg_surface_destroy(mock);
        } else {
            plutovg_canvas_set_rgb(c, 0.4f, 0.4f, 0.42f);
            plutovg_canvas_fill_text(c, "(no mock: a P1.3b screen line.html does not have)", -1,
                                     PLUTOVG_TEXT_ENCODING_UTF8, x + 492.f + 20.f, y + 270.f);
        }
    }
    snprintf(path, sizeof path, "%s/p13b-sheet.png", outdir);
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

int render_p13b(const char *out, const char *mocks)
{
    int k;
    outdir = out;
    mockdir = mocks;
    nshot = 0;
    failed = 0;

    /* ---- the lanes, playing */
    at(A02, T3, UI_F_NOTE);
    play(1450);
    shot("02-lane-note", "2 NOTE lane: A02 CHORDS, playing", "line-02-step-view.png");

    at(A01, T1, UI_F_VEL);                              /* two steps overridden, the default lowered */
    whold(2);
    black(9);                                           /* step 3: 127 */
    wrelease(2);
    whold(6);
    black(2);                                           /* step 7: 38 */
    wrelease(6);
    qd_app_key(&A, BK(UI_F_VEL), 1, 100);               /* (P1.3y: the VEL key held: ALGORITHM the default) */
    run(20);
    turn(QD_K_ALGO, -3);
    qd_app_key(&A, BK(UI_F_VEL), 0, 0);
    run(20);
    settle();
    play(900);
    shot("02-lane-vel", "VEL lane: A01 DRUMS, steps 3 and 7 overridden, the default lowered", 0);

    at(A02, T2, UI_F_LEN);
    whold(4);
    black(6);                                           /* step 5: 4 steps */
    wrelease(4);
    play(700);
    shot("02-lane-len", "LEN lane: A02 BASS, step 5 set to 4 steps", 0);

    at(A02, T3, UI_F_MICRO);
    whold(8);
    black(1);                                           /* step 9: -15 */
    wrelease(8);
    play(700);
    shot("02-lane-micro", "MICRO lane: A02 CHORDS (a stab micro-timed, step 9 at -15)", 0);

    at(A02, T4, UI_F_RATCH);
    play(700);
    shot("02-lane-ratch", "RATCH lane: A02 LEAD (ratchets)", 0);

    at(A02, T4, UI_F_CHNC);
    play(700);
    shot("02-lane-chnc", "CHNC lane: A02 LEAD (chances)", 0);

    at(A02, T3, UI_F_COND);
    play(700);
    shot("02-lane-cond", "COND lane: A02 CHORDS (a 1:2 and a NOT 1ST)", 0);

    at(A02, T2, UI_F_TRIG);
    play(700);
    shot("02-lane-trig", "TRIG lane: A02 BASS (two TIEs)", 0);

    at(A01, T1, UI_F_SOUND);
    play(700);
    shot("02-lane-sound", "SOUND lane: A01 DRUMS (a sound lock on every step)", 0);

    at(A01, T1, UI_F_SOUND);                            /* (P1.3d: LOCKS is dropped; P-LOCK lists the locks) */
    play(700);
    shot("02-lane-locks", "P-LOCK lane: A01 DRUMS (the kit's mix as p-locks)", 0);

    /* ---- a page with its own range in a loop of two pages: A02 CHORDS, F1 B1 held + B2 (loop 1..2), then F3 + W5
     * held + W12 (page 1 at 5..12), page follow off, playing */
    at(A02, T3, UI_F_NOTE);
    btap(QD_B_PAGE);                                    /* page follow off: page 1 stays on screen */
    run(QD_CONFIRM_MS + 20);
    bdown(QD_B_OCTDN);
    run(100);
    qd_app_key(&A, BK(0), 1, 100);
    run(40);
    black(1);
    qd_app_key(&A, BK(0), 0, 0);
    run(40);
    shot("f1-loop", "F1 held, B1 held + B2: pages 1..2 looped", 0);
    bup(QD_B_OCTDN);
    run(40);
    bdown(QD_B_OCTDN);
    bdown(QD_B_OCTUP);
    run(100);
    qd_app_key(&A, WK(4), 1, 100);                      /* W5 held, W12 pressed: start 5, end 12 */
    run(60);
    qd_app_key(&A, WK(11), 1, 100);
    run(60);
    shot("f3-range", "F3 held, W5 + W12: page 1's range 5..12", 0);
    qd_app_key(&A, WK(11), 0, 0);
    qd_app_key(&A, WK(4), 0, 0);
    run(30);
    bup(QD_B_OCTUP);
    bup(QD_B_OCTDN);
    run(QD_CONFIRM_MS + 20);
    play(1900);
    shot("02-lane-range", "NOTE lane: page 1 at 5..12, pages 1 and 2 looped, follow off", 0);

    /* ---- the held steps, each mode with its value strip */
    held(A02, T3, 0, UI_F_NOTE, 0);
    shot("04-held-note", "4 NOTE: A02 CHORDS step 1", "line-04-held-note.png");

    held(A01, T1, 4, UI_F_VEL, 400);                    /* the snare, playing: the mallet strikes */
    for (k = 0; k < 4000 && !(A.ui.held_played && A.ms - A.ui.held_play_ms == 30u); k++)
        run(1);
    shot("05-held-vel", "5 VEL: A01 step 5 as it plays", "line-05-held-vel.png");

    held(A02, T3, 4, UI_F_LEN, 0);
    shot("06-held-len", "6 LEN: A02 CHORDS step 5", "line-06-held-len.png");

    held(A02, T3, 0, UI_F_MICRO, 0);
    black(7);                                           /* +10 */
    run(1200);
    shot("07-held-micro", "7 MICRO: step 1 set to +10 (B8)", "line-07-held-micro.png");

    held(A02, T4, 4, UI_F_RATCH, 0);
    run(500);
    shot("08-held-ratch", "8 RATCH: A02 LEAD step 5", "line-08-held-ratchet.png");

    held(A01, T1, 14, UI_F_COND, 900);
    shot("09-held-cond", "9 COND: A01 step 15", "line-09-held-cond-chance.png");
    held(A02, T4, 3, UI_F_CHNC, 0);
    shot("09b-held-chnc", "9 CHNC: A02 LEAD step 4", 0);

    held(A01, T1, 8, UI_F_SOUND, 900);
    shot("10-held-sound", "10 SOUND: A01 step 9 (a hat), its bank on the keys", "line-10-held-sound.png");

    held(A02, T2, 4, UI_F_TRIG, 0);
    shot("19-held-trig", "19 TRIG: A02 BASS step 5", "line-19-held-trig.png");

    held(A01, T1, 0, UI_F_SOUND, 0);                    /* (P1.3d: LOCKS is dropped: P-LOCK keeps the lane) */
    run(300);
    shot("20-held-locks", "20 P-LOCK: A01 step 1 held (LOCKS is gone)", "line-20-held-locks.png");

    held(A02, T3, 1, UI_F_VEL, 0);                      /* an empty step */
    shot("held-empty", "VEL: an empty step held", 0);

    /* ---- the takeovers */
    at(A02, T3, UI_F_VEL);
    turn(QD_K_1, -2);
    run(100);
    shot("tk-default", "KNOB 1, nothing held: the track's default (VEL)", 0);

    at(A02, T3, UI_F_NOTE);
    play(600);
    turn(QD_K_SELECT, 1);
    run(100);
    shot("tk-page", "SELECT: the page picker", 0);

    at(A01, T1, UI_F_NOTE);
    turn(QD_K_PRESETS, 1);
    run(100);
    shot("tk-pool", "PRESETS: the track's sound from the pool", 0);
    at(A01, T1, UI_F_VEL);
    whold(4);
    turn(QD_K_PRESETS, 2);
    run(100);
    shot("tk-pool-lock", "PRESETS with step 5 held: its sound lock", 0);

    at(A02, T3, UI_F_NOTE);
    bdown(QD_B_FUNC);
    run(20);
    turn(QD_K_ALGO, 2);
    run(100);
    shot("tk-scale", "SHIFT + ALGORITHM: the track's scale", 0);
    turn(QD_K_PRESETS, 2);
    run(100);
    shot("tk-root", "SHIFT + PRESETS: the scale's root", 0);
    bup(QD_B_FUNC);

    /* ---- the layer maps, SHIFT */
    at(A02, T3, UI_F_NOTE);
    play(900);
    bdown(QD_B_OCTDN);
    run(100);
    shot("f1", "F1 (OCT- held): the pages", 0);
    bup(QD_B_OCTDN);
    run(30);
    bdown(QD_B_OCTUP);
    run(100);
    wtap(1);                                            /* (P1.3c: Form's pick-up: W2 cut into the hand) */
    run(100);
    shot("f2", "F2 (OCT+ held): the pick-up / drop, W2 cut into the hand", 0);
    bup(QD_B_OCTUP);
    run(30);
    bdown(QD_B_OCTDN);
    bdown(QD_B_OCTUP);
    run(100);
    shot("f3", "F3 (both held): the page's range and the rate", 0);
    bup(QD_B_OCTUP);
    bup(QD_B_OCTDN);
    run(30);
    btap(QD_B_OCTDN);                                   /* (F3 is latched since P1.3d: OCT- exits it) */
    run(30);
    bdown(QD_B_FUNC);
    run(200);
    shot("shift", "SHIFT (GLO held): the map", 0);
    bup(QD_B_FUNC);

    /* ---- the keyboard */
    at(A02, T4, UI_F_NOTE);
    btap(QD_B_REC);
    qd_app_key(&A, 7, 1, 100), qd_app_key(&A, 11, 1, 100);
    run(100);
    shot("keys", "REC: the keyboard (track 4, two keys held)", 0);
    qd_app_key(&A, 7, 0, 0), qd_app_key(&A, 11, 0, 0);
    run(20);
    bdown(QD_B_FUNC);
    btap(QD_B_REC);                                     /* SHIFT + REC, stopped: step recording */
    bup(QD_B_FUNC);
    qd_app_key(&A, 7, 1, 100);
    run(50);
    qd_app_key(&A, 7, 0, 0);
    run(20);
    qd_app_key(&A, 14, 1, 100);
    run(QD_CONFIRM_MS + 20);
    shot("keys-step-rec", "SHIFT + REC stopped: step recording", 0);
    qd_app_key(&A, 14, 0, 0);

    at(A02, T4, UI_F_NOTE);                             /* the keyboard, PLAY, REC held: live recording (P1.6g) */
    btap(QD_B_REC);
    btap(QD_B_PLAY);
    bdown(QD_B_REC);
    run(QD_REC_HOLD_MS + 40);
    bup(QD_B_REC);
    {   /* (P2.x: F3, the keybed's first key, then up to D4: the drawn keybed starts on F as the FM-1's) */
        static const uint8_t KEYS[8] = {0, 7, 14, 11, 16, 14, 12, 9};
        for (k = 0; k < 8; k++) {
            run(k % 3 ? 140 : 90);
            qd_app_key(&A, KEYS[k], 1, 100);
            run(k == 7 ? 600 : 110 + 40 * (k % 2));
            if (k < 7)
                qd_app_key(&A, KEYS[k], 0, 0);
        }
    }
    shot("18-keys-rec", "18 REC + PLAY: live recording (D4 held)", "line-18-keys-rec.png");
    qd_app_key(&A, 9, 0, 0);

    if (!failed)
        sheet();
    return failed;
}
