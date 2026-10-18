/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.3 part: every P1.3 screen rendered from a real state reached by panel input only (presses, holds,
 * turns, MIDI, the demo patterns A01..A03 as the unit has them), each from a fresh unit, into OUT/p13-*.png; then
 * OUT/p13-sheet.png: each screen at 2x beside line.html's mock of it (mockups/png/line-*.png) where there is one,
 * with captions. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"

#define NSHOT 40
static qd_app_t A;
static uint32_t now;
static uint64_t frames_done;
static uint16_t lcd[QD_W * QD_H];
static const char *outdir, *mockdir;
static struct {
    char file[64], caption[96], mock[64];
} shots[NSHOT];
static int nshot, failed;

int p13_write_lcd_png(const char *path, const uint16_t *lcd_be);   /* render_frames.c's */

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
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
}
static void shot(const char *name, const char *caption, const char *mock)
{
    char path[1024];
    qd_app_frame(&A, lcd);
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(shots[nshot].mock, sizeof shots[nshot].mock, "%s", mock ? mock : "");
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13: cannot write %s\n", path);
        failed = 1;
    }
    printf("%s (%s)\n", path, caption);
    if (nshot < NSHOT - 1)
        nshot++;
}

/* the panel */
#define WK(w) QD_WHITE_KEY[w]
#define BK(b) QD_BLACK_KEY[b]
static void btap(int b)
{
    qd_app_button(&A, b, 1);
    run(20);
    qd_app_button(&A, b, 0);
    run(20);
}
static void black(int b)
{
    qd_app_key(&A, BK(b), 1, 100);
    run(20);
    qd_app_key(&A, BK(b), 0, 0);
    run(10);
}
static void whold(int w)
{
    qd_app_key(&A, WK(w), 1, 100);
    run(300);
}
static void turn(int k, int d)
{
    qd_app_knob(&A, k, d);
    run(5);
}
/* a fresh unit on pattern k, the step view, track t */
static void at(int k, int t)
{
    fresh();
    btap(QD_B_HOME);
    qd_app_button(&A, QD_B_PATTERN, 1);
    qd_app_key(&A, WK(k), 1, 100);
    run(20);
    qd_app_key(&A, WK(k), 0, 0);
    qd_app_button(&A, QD_B_PATTERN, 0);
    run(20);
    if (t)
        black(t);
}
/* hold track t's step s (its page first), open field f */
static void held(int k, int t, int s, int f, int play)
{
    at(k, t);
    if (play) {
        btap(QD_B_PLAY);
        run(900);
    }
    if (s >= 16) {
        btap(QD_B_PAGE);
        run(200);
    }
    whold(s % 16);
    black(f);
    run(400);
}

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
    const int cell_w = 480 * 2 + 12, cell_h = 480 + 34, cols = 2, pad = 16;
    const int rows = (nshot + cols - 1) / cols;
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
    plutovg_canvas_fill_text(c, "QUADRILLE P1.3: each screen from the core (left) beside its line.html mock (right), 2x",
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
            plutovg_canvas_fill_text(c, "(no mock: a screen line.html leaves open)", -1, PLUTOVG_TEXT_ENCODING_UTF8,
                                     x + 492.f + 20.f, y + 270.f);
        }
    }
    snprintf(path, sizeof path, "%s/p13-sheet.png", outdir);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens)\n", path, nshot);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
}

enum { T1, T2, T3, T4 };
enum { F_NOTE, F_VEL, F_LEN, F_MICRO, F_RATCH, F_COND, F_CHNC, F_TRIG, F_SOUND, F_LOCKS };

int render_p13(const char *out, const char *mocks)
{
    int k;
    outdir = out;
    mockdir = mocks;
    nshot = 0;
    failed = 0;

    fresh();
    run(1700);
    shot("01-idle", "1 idle (power-on, A02 stopped)", "line-01-idle.png");

    at(1, T3);                                          /* A02's chords, playing */
    btap(QD_B_PLAY);
    run(1450);
    shot("02-step-view", "2 the step view: A02 track 3, playing", "line-02-step-view.png");

    at(2, T1);                                          /* A03's drums, stopped */
    shot("02b-step-view-drums", "2 the step view: A03 drums, stopped", 0);

    held(1, T3, 0, F_NOTE, 0);
    shot("04-held-note", "4 NOTE: track 3 step 1 (Am9 as A C E B)", "line-04-held-note.png");
    held(1, T3, 4, F_NOTE, 0);
    turn(QD_K_SELECT, -1);                              /* the chord shape before Fmaj7's: m7 .. a new name squeezes */
    run(60);
    shot("04b-held-note-squeeze", "4 NOTE: SELECT, a new chord squeezing in", 0);

    held(0, T1, 4, F_VEL, 1);                           /* A01's snare, playing: the mallet */
    for (k = 0; k < 4000 && !(A.ui.held_played && A.ms - A.ui.held_play_ms == 30u); k++)
        run(1);
    shot("05-held-vel", "5 VEL: A01 step 5 (snare 116) as it plays", "line-05-held-vel.png");

    held(1, T3, 4, F_LEN, 0);
    shot("06-held-len", "6 LEN: track 3 step 5 (1.5 steps at 1/2x)", "line-06-held-len.png");
    turn(QD_K_2, 1);
    run(100);
    shot("06b-held-len-tie", "6 LEN: KNOB 2, TIE on", 0);

    held(1, T3, 0, F_MICRO, 0);
    turn(QD_K_1, 7);
    run(1200);
    shot("07-held-micro", "7 MICRO: step 1, KNOB 1 +7, strum 3", "line-07-held-micro.png");

    held(1, T4, 20, F_RATCH, 0);
    run(500);
    shot("08-held-ratch", "8 RATCH: track 4 step 21 (x4, accent)", "line-08-held-ratchet.png");

    held(0, T1, 14, F_COND, 1);
    turn(QD_K_2, -25);
    shot("09-held-cond", "9 COND: A01 step 15 (2:2), chance 75 by KNOB 2", "line-09-held-cond-chance.png");
    held(1, T4, 3, F_CHNC, 0);
    shot("09b-held-chnc", "9 CHNC: track 4 step 4 (75 %, KNOB 1 the chance)", 0);
    held(0, T1, 4, F_COND, 0);
    turn(QD_K_1, 1);
    shot("09c-held-cond-fill", "9 COND: FILL (not an A:B)", 0);

    held(0, T1, 8, F_SOUND, 1);
    turn(QD_K_1, -1);
    run(60);
    shot("10-held-sound", "10 SOUND: A01 step 9 (a hat, sound 17 -> 16)", "line-10-held-sound.png");

    held(1, T2, 4, F_TRIG, 0);
    shot("19-held-trig", "19 TRIG: track 2 step 5 (a TIE)", "line-19-held-trig.png");

    held(0, T1, 0, F_LOCKS, 0);
    turn(QD_K_2, 3);
    run(300);
    shot("20-held-locks", "20 LOCKS: A01 step 1 (the kick's locks)", "line-20-held-locks.png");

    at(1, T3);
    btap(QD_B_PLAY);
    run(500);
    turn(QD_K_1, -45);
    run(300);
    shot("21-turn-freq", "21 KNOB 1 on the step view: FREQ", "line-21-turn-freq.png");
    run(QD_TK_MS + 60 - 300);
    shot("21b-turn-freq-fade", "21 .. fading back (1.06 s after the last detent)", 0);
    turn(QD_K_2, 20);
    run(300);
    shot("22-turn-decay", "22 KNOB 2: DECAY", "line-22-turn-decay.png");
    turn(QD_K_3, 4);
    run(300);
    shot("23-turn-harm", "23 KNOB 3: HARM", "line-23-turn-harm.png");
    turn(QD_K_4, 14);
    run(300);
    shot("24-turn-fdbk", "24 KNOB 4: FDBK", "line-24-turn-feedback.png");
    turn(QD_K_ALGO, 4);
    run(300);
    shot("12-turn-algo", "ALGORITHM: the track's ALGO (5)", "line-12-engine-syn1.png");
    turn(QD_K_SELECT, 22);
    run(300);
    shot("16-turn-tempo", "16 SELECT: tempo", "line-16-tempo.png");
    qd_app_button(&A, QD_B_FUNC, 1);
    turn(QD_K_SELECT, 2);
    qd_app_button(&A, QD_B_FUNC, 0);
    run(300);
    shot("16b-turn-swing", "16 FUNC + SELECT: swing", 0);

    at(4, T1);                                          /* an empty step (A05) held + KNOB 1: a lock */
    whold(1);
    turn(QD_K_1, -30);
    run(200);
    shot("21c-turn-freq-lock", "a p-lock: an empty step held + KNOB 1 (LOCK step)", 0);

    at(1, T3);
    btap(QD_B_TRACKS);
    btap(QD_B_PLAY);
    run(600);
    turn(QD_K_2, -1);
    run(300);
    shot("25-turn-rate", "25 the tracks screen, KNOB 2: RATE", "line-25-turn-rate.png");
    turn(QD_K_1, 8);
    run(300);
    shot("25b-turn-len", "the tracks screen, KNOB 1: LEN", 0);
    turn(QD_K_3, -2);
    run(300);
    shot("25c-turn-limit", "the tracks screen, KNOB 3: LIMIT", 0);
    turn(QD_K_4, 1);
    run(300);
    shot("25d-turn-tswing", "the tracks screen, KNOB 4: SWING on", 0);
    run(1400);
    shot("03-tracks", "3 the tracks screen", "line-03-tracks.png");

    held(0, T1, 4, F_VEL, 0);                           /* CLEAR */
    black(10);
    run(200);
    shot("clear", "CLEAR (F#5): the confirmation", 0);

    at(1, T4);                                          /* KEYS live recording */
    btap(QD_B_KEYS);
    btap(QD_B_PLAY);
    btap(QD_B_REC);
    {
        static const uint8_t KEYS[8] = {7, 11, 14, 11, 16, 14, 12, 9};   /* C4 E4 G4 E4 A4 G4 F4 D4 */
        for (k = 0; k < 8; k++) {
            run(k % 3 ? 140 : 90);
            qd_app_key(&A, KEYS[k], 1, 100);
            run(k == 7 ? 600 : 110 + 40 * (k % 2));
            if (k < 7)
                qd_app_key(&A, KEYS[k], 0, 0);
        }
    }
    shot("18-keys-rec", "18 KEYS, REC while playing: the roll (D4 held)", "line-18-keys-rec.png");
    qd_app_key(&A, 9, 0, 0);

    at(1, T4);                                          /* KEYS step recording */
    btap(QD_B_KEYS);
    btap(QD_B_REC);
    qd_app_key(&A, 7, 1, 100), qd_app_key(&A, 11, 1, 100);
    run(50);
    qd_app_key(&A, 7, 0, 0), qd_app_key(&A, 11, 0, 0);
    run(20);
    btap(QD_B_OCTUP);
    qd_app_key(&A, 14, 1, 100);
    run(100);
    shot("18b-keys-step-rec", "KEYS, REC stopped: step recording (G4 held at step 3)", 0);

    if (!failed)
        sheet();
    return failed;
}
