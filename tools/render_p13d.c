/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.3d part: the screens of PLAN.md §8b version 2.2, rendered from a real state reached by panel input
 * only (the demo patterns A01..A03 and edits made with the gestures of core/app/ctl.h), each from a fresh unit, into
 * OUT/p13d-*.png; then OUT/p13d-sheet.png, every shot at 2x with its caption.
 *
 * The shots: a default turned with nothing held in every mode (the lane, its default lit, the knob band "TRACK
 * DEFAULT"); a held step's extra knobs (LEN's TIE, MICRO's strum, CHNC's condition) and its main value on KNOB 1; the
 * P-LOCK track chooser (ALGORITHM); the SEQ map on bank B; F3 latched ("OCT- EXIT"); the step latch (LFO) with two
 * steps selected, on VEL's screen and on the P-LOCK lane. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13d-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13d: cannot write %s\n", path);
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
    plutovg_canvas_fill_text(c, "FIZZ P1.3d: the screens of PLAN §8b version 2.2, each from the core (2x)", -1,
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
    snprintf(path, sizeof path, "%s/p13d-sheet.png", outdir);
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
enum { K_SEL = QD_K_SELECT, K_ALG = QD_K_ALGO, K_PRE = QD_K_PRESETS, K1 = QD_K_1, K2, K3, K4 };

int render_p13d(const char *out)
{
    static const struct {
        int pat, track, mode, knob, d;
        const char *name, *caption;
    } DEF[] = {
        {A02, T3, UI_F_NOTE, K2, 7, "def-note", "NOTE, nothing held, KNOB 2: the default chord's note 3"},
        {A02, T2, UI_F_VEL, K1, -24, "def-vel", "VEL, nothing held, KNOB 1: the track's velocity"},
        {A02, T2, UI_F_LEN, K2, 1, "def-len-tie", "LEN, nothing held, KNOB 2: the default TIE on"},
        {A02, T3, UI_F_MICRO, K2, 3, "def-micro-strum", "MICRO, nothing held, KNOB 2: the default strum"},
        {A01, T1, UI_F_RATCH, K2, 1, "def-ratch-ramp", "RATCH, nothing held, KNOB 2: the default ramp UP"},
        {A01, T1, UI_F_CHNC, K1, -25, "def-chnc", "CHNC, nothing held, KNOB 1: the default chance 75 %"},
        {A01, T1, UI_F_COND, K1, 9, "def-cond", "COND, nothing held, KNOB 1: the default condition"},
        {A02, T2, UI_F_TRIG, K1, 2, "def-trig", "TRIG, nothing held, KNOB 1: the default TIE"},
        {A02, T4, UI_F_SOUND, K1, 2, "def-sound", "SOUND, nothing held, KNOB 1: the track's own sound"},
    };
    size_t i;
    outdir = out;
    nshot = 0;
    failed = 0;

    for (i = 0; i < sizeof DEF / sizeof DEF[0]; i++) {  /* a default per mode: the lane, the band */
        at(DEF[i].pat, DEF[i].track, DEF[i].mode);
        turn(DEF[i].knob, DEF[i].d);
        run(250);
        shot(DEF[i].name, DEF[i].caption);
    }

    /* a step held: the extras on KNOB 2, the main value on KNOB 1 */
    at(A02, T2, UI_F_LEN);
    whold(4);
    turn(K2, 1);
    run(250);
    shot("held-len-tie", "LEN, step 5 held, KNOB 2: TIE on (the band, the slur)");
    wrelease(4);
    at(A02, T3, UI_F_MICRO);
    whold(0);
    turn(K2, 4);
    run(250);
    shot("held-micro-strum", "MICRO, step 1 held (a chord), KNOB 2: the strum 4/24");
    wrelease(0);
    at(A01, T1, UI_F_CHNC);
    whold(0);
    turn(K2, 12);
    run(250);
    shot("held-chnc-cond", "CHNC, step 1 held, KNOB 2: the condition (1:3)");
    wrelease(0);
    at(A01, T1, UI_F_VEL);
    whold(4);
    turn(K1, -30);
    run(250);
    shot("held-vel-k1", "VEL, step 5 held, KNOB 1: the step's velocity");
    wrelease(4);

    /* P-LOCK, nothing held: ALGORITHM chooses the track */
    at(A03, T1, UI_F_PLOCK);
    btap(QD_B_PLAY);
    run(600);
    turn(K_ALG, 3);
    run(250);
    shot("plock-track", "P-LOCK, nothing held, ALGORITHM: the track (4), the eight tracks");
    btap(QD_B_PLAY);

    /* SEQ: bank B shown, B03 chosen; A02 playing */
    at(A02, T1, UI_F_NOTE);
    bdown(QD_B_MIX);
    black(1);
    wtap(2);
    run(100);
    shot("seq-bank", "SEQ held + B2: bank B; W3: B03 (stopped: at once)");
    bup(QD_B_MIX);

    /* F3 latched */
    at(A02, T2, UI_F_NOTE);
    bdown(QD_B_OCTDN);
    bdown(QD_B_OCTUP);
    run(30);
    bup(QD_B_OCTUP);
    bup(QD_B_OCTDN);
    run(30);
    black(3);
    black(8);
    run(300);
    shot("f3-latched", "F3 latched (both pressed, let go): it stays; OCT- exits");

    /* the step latch: two steps selected */
    at(A01, T1, UI_F_VEL);
    btap(QD_B_MUTE);
    qd_app_key(&A, WK(4), 1, 100);
    run(20);
    qd_app_key(&A, WK(12), 1, 100);
    run(20);
    qd_app_key(&A, WK(4), 0, 0);
    qd_app_key(&A, WK(12), 0, 0);
    run(300);
    shot("latch-vel", "LFO: the step latch, steps 5 and 13 selected (VEL)");
    btap(QD_B_MUTE);
    at(A01, T1, UI_F_PLOCK);
    btap(QD_B_MUTE);
    qd_app_key(&A, WK(0), 1, 100);
    run(20);
    qd_app_key(&A, WK(8), 1, 100);
    run(20);
    qd_app_key(&A, WK(0), 0, 0);
    qd_app_key(&A, WK(8), 0, 0);
    run(300);
    shot("latch-plock", "LFO in P-LOCK: steps 1 and 9 selected, the lane stays (LATCH)");

    if (!failed)
        sheet();
    return failed;
}
