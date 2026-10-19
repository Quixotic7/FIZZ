/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' STEPS part (P1.3j, P1.3p, P1.3q; was render_p13j.c): the STEPS view (the page edited unfolded at half
 * height, its notes in its cells; the others folded rows), the steps' kinds in colour (P1.3q: own notes, the track's
 * notes, ghosts), into OUT/p13q-steps-*.png and OUT/p13q-steps-sheet.png. The ghosts and the VEL overrides are set
 * through the pattern API (qd_ctl_pat); the rest by panel input. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ctl.h"
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
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
}
static void shot(const char *name, const char *caption)
{
    char path[1024];
    qd_app_frame(&A, lcd);
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13q-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13q: cannot write %s\n", path);
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
    plutovg_canvas_fill_text(c, "FIZZ P1.3q: STEPS: own notes white, the track's notes cyan, ghosts violet; bars only outside NOTE (2x)", -1,
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
    snprintf(path, sizeof path, "%s/p13q-steps-sheet.png", outdir);
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

/* A02 KEYS (track 3): page 2 joins the loop (F1: hold B1, press B2) and gets chords; STEPS on */
static void keys_two_pages(void)
{
    static const int CH[4][3] = {{0, 4, 7}, {3, 6, 9}, {1, 4, 8}, {2, 5, 9}};
    int k, j;
    at(A02, T3, UI_F_NOTE);
    qd_app_button(&A, QD_B_OCTDN, 1);
    run(80);
    qd_app_key(&A, BK(0), 1, 100);
    run(30);
    qd_app_key(&A, BK(1), 1, 100);
    run(30);
    qd_app_key(&A, BK(1), 0, 0);
    qd_app_key(&A, BK(0), 0, 0);
    run(20);
    black(1);                                           /* edit page 2 */
    qd_app_button(&A, QD_B_OCTDN, 0);
    run(20);
    for (k = 0; k < 4; k++) {                           /* steps 1, 5, 9, 13: four chords by the keys */
        whold(k * 4);
        for (j = 0; j < 3; j++)
            qd_app_key(&A, BK(CH[k][j]), 1, 100);
        run(30);
        for (j = 0; j < 3; j++)
            qd_app_key(&A, BK(CH[k][j]), 0, 0);
        run(20);
        wrelease(k * 4);
    }
    wtap(7);
    wtap(11);
    btap(QD_B_HOME);                                    /* STEPS */
}

int render_p13q(const char *out)
{
    outdir = out;
    nshot = 0;
    failed = 0;

    keys_two_pages();
    qd_step_set_trig(qd_ctl_pat(&A), T3, 16 + 2, QD_TRIG_LOCK);   /* ghosts: two on page 2, one on page 1 */
    qd_step_set_trig(qd_ctl_pat(&A), T3, 16 + 14, QD_TRIG_LOCK);
    qd_step_set_trig(qd_ctl_pat(&A), T3, 5, QD_TRIG_LOCK);
    qd_step_on(qd_ctl_pat(&A), T3, 9);                  /* a trig with the track's notes on page 1 */
    qd_step_set_vel(qd_ctl_pat(&A), T3, 16 + 0, 127);   /* velocities: the fills' heights */
    qd_step_set_vel(qd_ctl_pat(&A), T3, 16 + 4, 90);
    qd_step_set_vel(qd_ctl_pat(&A), T3, 16 + 8, 55);
    qd_step_set_vel(qd_ctl_pat(&A), T3, 16 + 12, 25);
    qd_step_set_vel(qd_ctl_pat(&A), T3, 16 + 11, 60);
    qd_step_set_vel(qd_ctl_pat(&A), T3, 16 + 14, 40);
    qd_step_set_len(qd_ctl_pat(&A), T3, 16 + 4, 8);    /* step 5: 4 steps long, a little late */
    qd_step_set_micro(qd_ctl_pat(&A), T3, 16 + 4, 6);
    qd_step_set_len(qd_ctl_pat(&A), T3, 16 + 12, QD_LEN_INF);   /* step 13: INF */
    run(400);
    shot("steps-stopped", "stopped: own notes white, the track's cyan, ghosts violet; fills = velocity");
    turn(QD_K_PRESETS, 1);                              /* PRESETS: the cursor (the latch on, at the last step) */
    turn(QD_K_PRESETS, -7);
    run(300);
    shot("steps-cursor-len", "PRESETS, the cursor: step 5, its length (4 steps, late)");
    turn(QD_K_PRESETS, 8);
    run(300);
    shot("steps-cursor-inf", "the cursor on step 13: INF (to the end, an arrow)");
    turn(QD_K_PRESETS, 5);
    run(110);
    shot("steps-cursor-wrap", "on past page 2's end: page 1's step 2, the rows moving");
    run(300);
    btap(QD_B_HOME);                                    /* the latch off */
    turn(QD_K_SELECT, 1);
    run(400);
    btap(QD_B_PLAY);
    while (qd_seq_track_step(&A.seq, 2) != 20)
        run(1);
    run(20);
    shot("steps-playing", "playing: the marker hops, step 21 pops, its notes red");
    while (qd_seq_track_step(&A.seq, 2) != 3)
        run(1);
    run(30);
    shot("steps-playing-p1", "playing on page 1 (followed): its row unfolded");
    btap(QD_B_PLAY);
    qd_step_set_vel(qd_ctl_pat(&A), T3, 4, 70);         /* VEL: three steps override it: its bars */
    qd_step_set_vel(qd_ctl_pat(&A), T3, 8, 120);
    qd_step_set_vel(qd_ctl_pat(&A), T3, 9, 60);
    black(UI_F_VEL);
    run(400);
    shot("steps-vel", "VEL: green bars under the steps overriding VEL");
    black(UI_F_NOTE);
    run(50);
    turn(QD_K_SELECT, 1);
    run(100);
    shot("steps-unfold", "SELECT to page 2, mid-way (folding / unfolding)");
    run(300);
    black(UI_F_PLOCK);                                  /* the latch in P-LOCK: two steps selected, the view stays */
    btap(QD_B_MUTE);
    qd_app_key(&A, WK(4), 1, 100);
    run(20);
    qd_app_key(&A, WK(8), 1, 100);
    run(20);
    qd_app_key(&A, WK(4), 0, 0);
    qd_app_key(&A, WK(8), 0, 0);
    run(300);
    shot("steps-latch", "the latch in P-LOCK: steps 21, 25 selected, their lengths");

    if (!failed)
        sheet();
    return failed;
}
