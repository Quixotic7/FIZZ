/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.3c part: the screens of PLAN.md §8b version 2.1, rendered from a real state reached by panel input
 * only (the demo patterns A01..A03 and edits made with the gestures of core/app/ctl.h), each from a fresh unit, into
 * OUT/p13c-*.png; then OUT/p13c-sheet.png, every shot at 2x with its caption.
 *
 * The shots: SEL's track map (a mute, a solo); SEQ's pattern map (a pattern queued to the next BAR); the F1 map with a
 * step copied, and after B10 with a page copied; the F2 map with a step in the hand; the F3 map with a dotted rate and
 * a page range; NOTE with five notes (the five knobs' colours); the octave takeover at three moments (the change, mid
 * slide, settled); the keyboard's octave; SOUND's bank takeover; the P-LOCK lane, a step held in P-LOCK, and its lock's
 * takeover. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"
#include "ui.h"

#define NSHOT 32
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
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
}
static void shot(const char *name, const char *caption)
{
    char path[1024];
    qd_app_frame(&A, lcd);
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13c-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13c: cannot write %s\n", path);
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
static void settle(void) { run(QD_TK_MS + QD_TK_FADE_MS + 20); }
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
    plutovg_font_face_t *f = plutovg_font_face_load_from_data(qd_fonts_gen[3].data, qd_fonts_gen[3].len, 0, NULL, NULL);
    char path[1024];
    int i;
    plutovg_canvas_set_rgb(c, 0.11f, 0.11f, 0.12f);
    plutovg_canvas_rect(c, 0, 0, (float)W, (float)H);
    plutovg_canvas_fill(c);
    plutovg_canvas_set_font(c, f, 18.f);
    plutovg_canvas_set_rgb(c, 0.9f, 0.9f, 0.9f);
    plutovg_canvas_fill_text(c, "FIZZ P1.3c: the screens of PLAN §8b version 2.1, each from the core (2x)", -1,
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
    snprintf(path, sizeof path, "%s/p13c-sheet.png", outdir);
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

int render_p13c(const char *out)
{
    int k;
    outdir = out;
    nshot = 0;
    failed = 0;

    /* SEL: the track map, A03 playing, track 2 muted, track 5 soloed */
    at(A03, T3, UI_F_NOTE);
    btap(QD_B_PLAY);
    run(800);
    bdown(QD_B_PATTERN);
    wtap(1);
    qd_app_key(&A, WK(4), 1, 100);
    run(450);
    qd_app_key(&A, WK(4), 0, 0);
    run(200);
    shot("sel", "SEL held: the tracks (W2 tapped: muted; W5 held: soloed)");
    bup(QD_B_PATTERN);

    /* SEQ: A02 playing, ALGORITHM = BAR, W1 tapped: A01 queued to the next bar */
    at(A02, T1, UI_F_NOTE);
    btap(QD_B_PLAY);
    run(600);
    bdown(QD_B_MIX);
    turn(QD_K_ALGO, 1);
    wtap(0);
    run(700);
    shot("seq", "SEQ held: A01 queued to the next BAR (the launch mode)");
    bup(QD_B_MIX);

    /* F1: a step copied (W1), then a page copied (B10) */
    at(A02, T3, UI_F_NOTE);
    bdown(QD_B_OCTDN);
    run(100);
    wtap(0);
    run(50);
    shot("f1-step", "F1 held, W1 copied: the next W pastes it");
    black(9);
    run(60);
    shot("f1-page", "F1 + B10: page 1 copied, B1..B8 paste it");
    bup(QD_B_OCTDN);

    /* F2: A02 CHORDS, W1 (a chord) cut into the hand */
    at(A02, T3, UI_F_NOTE);
    bdown(QD_B_OCTUP);
    run(100);
    wtap(0);
    run(60);
    shot("f2-hand", "F2 held, W1 cut: the chord in the hand");
    wtap(5);
    run(60);
    shot("f2-dropped", "F2: W6 empty: pasted, empty-handed");
    bup(QD_B_OCTUP);

    /* F3: dotted 1/8, page 1 at 3..12 */
    at(A02, T2, UI_F_NOTE);
    bdown(QD_B_OCTDN);
    bdown(QD_B_OCTUP);
    run(60);
    black(3);                                           /* 1/8 */
    black(8);                                           /* dotted */
    turn(QD_K_3, 2);
    turn(QD_K_4, -4);
    run(60);
    shot("f3", "F3: dotted 1/8 (B4, B9), the page's start and end on KNOB 3 / 4");
    black(9);                                           /* triplet */
    run(60);
    shot("f3-triplet", "F3: triplet 1/8 (B10)");
    bup(QD_B_OCTUP);
    bup(QD_B_OCTDN);

    /* NOTE with five notes: A02 KEYS step 1 held; KNOB 1..4 give notes 2..5 */
    at(A02, T3, UI_F_NOTE);
    whold(0);
    turn(QD_K_SELECT, -3);
    turn(QD_K_4, 14);
    turn(QD_K_1, 3);
    run(400);
    shot("note-five", "NOTE: five notes, each in its knob's colour (SELECT yellow, KNOB 1..4)");
    wrelease(0);
    at(A02, T3, UI_F_NOTE);
    bdown(QD_B_FUNC);
    turn(QD_K_ALGO, 3);                                 /* (D dorian: SHIFT + ALGORITHM, PRESETS) */
    turn(QD_K_PRESETS, 2);
    bup(QD_B_FUNC);
    settle();
    whold(4);
    qd_app_key(&A, BK(0), 1, 100), qd_app_key(&A, BK(2), 1, 100), qd_app_key(&A, BK(4), 1, 100);
    qd_app_key(&A, BK(6), 1, 100), qd_app_key(&A, BK(8), 1, 100);
    run(40);
    for (k = 0; k < 10; k += 2)
        qd_app_key(&A, BK(k), 0, 0);
    run(400);
    shot("note-five-keys", "NOTE: five notes by the keys (B1 B3 B5 B7 B9, D dorian)");

    /* the octave takeover: OCT+ with a step held in NOTE, three moments */
    bdown(QD_B_OCTUP);
    run(15);
    shot("oct-1", "OCT+ (NOTE, a step held): the octave, 15 ms");
    run(135);
    shot("oct-2", "the octave, 150 ms: the number rolls, the keyboard slides");
    bup(QD_B_OCTUP);
    run(450);
    shot("oct-3", "the octave, 600 ms: settled (OCT+ blinks slowly)");
    wrelease(4);
    at(A02, T4, UI_F_NOTE);
    btap(QD_B_REC);
    btap(QD_B_OCTDN);
    btap(QD_B_OCTDN);
    run(300);
    shot("oct-keys", "the keyboard: OCT- twice (-2)");
    at(A01, T1, UI_F_SOUND);
    whold(8);
    btap(QD_B_OCTUP);
    run(250);
    shot("bank", "SOUND, a step held: OCT+ the next bank of ten");
    wrelease(8);

    /* P-LOCK: the lane, a step held (the screen stays), a lock's takeover */
    at(A01, T1, 10);
    btap(QD_B_PLAY);
    run(700);
    shot("plock-lane", "B11: P-LOCK, the lane (the roll, each step's p-locks)");
    btap(QD_B_PLAY);
    whold(4);
    run(100);
    shot("plock-held", "P-LOCK, step 5 held: the screen stays");
    turn(QD_K_3, 4);
    run(60);
    shot("plock-turn", "P-LOCK, step 5 held, KNOB 3: a HARM lock");
    wrelease(4);

    if (!failed)
        sheet();
    return failed;
}
