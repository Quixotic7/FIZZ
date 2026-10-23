/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.3s part (was render_p13r.c): the EDIT view (HOME's third view): the hold mode's drawing at full
 * size over the page's row, the value in the head; a step selected by the PRESETS cursor in every mode, the track's
 * default with nothing held, a mode key held, playing; into OUT/p13s-edit-*.png and OUT/p13s-edit-sheet.png, with
 * P1.3r's NOTE and VEL shots beside them when OUT has them (saved as p13s-before-*.png). The velocities, lengths and ghosts are set through the pattern API
 * (qd_ctl_pat); the rest by panel input. */
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
    A.ui.view = QD_VIEW_ROLL;   /* (P1.5s: EDIT is the power-on view; made on the PIANO ROLL) */
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
}
static void shot(const char *name, const char *caption)
{
    char path[1024];
    qd_app_frame(&A, lcd);
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p13s-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p13s: cannot write %s\n", path);
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
static void vhome(void)                                 /* (P1.3y: SHIFT + HOME, the next view; HOME alone never cycles) */
{
    qd_app_button(&A, QD_B_FUNC, 1);
    run(10);
    btap(QD_B_HOME);
    qd_app_button(&A, QD_B_FUNC, 0);
    run(10);
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
    plutovg_font_face_t *f = plutovg_font_face_load_from_data(qd_fonts_gen[QD_FONTS_GEN_500].data, qd_fonts_gen[QD_FONTS_GEN_500].len, 0, NULL, NULL);
    char path[1024];
    int i;
    plutovg_canvas_set_rgb(c, 0.11f, 0.11f, 0.12f);
    plutovg_canvas_rect(c, 0, 0, (float)W, (float)H);
    plutovg_canvas_fill(c);
    plutovg_canvas_set_font(c, f, 18.f);
    plutovg_canvas_set_rgb(c, 0.9f, 0.9f, 0.9f);
    plutovg_canvas_fill_text(c, "FIZZ P1.3s: EDIT: the drawings full size, the value in the head, no strip (2x); last: P1.3r before", -1,
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
    snprintf(path, sizeof path, "%s/p13s-edit-sheet.png", outdir);
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
    vhome();                                    /* STEPS */
}

/* the cursor on step 21 (page 2's step 5) in hold mode m (the latch off first, then PRESETS) */
static void edit_mode(int m)
{
    if (A.ui.slatch)
        btap(QD_B_MUTE);                                /* LFO: the latch off */
    black(m);
    run(20);
    turn(QD_K_ALGO, 1);                              /* the latch on at the last step .. */
    while (A.ui.nheld && A.ui.held[0] != 20)
        turn(QD_K_ALGO, A.ui.held[0] < 20 ? 1 : -1);  /* .. moved to step 21 */
    run(400);
}

int render_p13s(const char *out)
{
    static const struct {
        int m;
        const char *name, *caption;
    } MODES[] = {
        {UI_F_NOTE, "edit-note", "NOTE: the keybed full size, the chord in the head"},
        {UI_F_VEL, "edit-vel", "VEL: the mallet and drum full size"},
        {UI_F_LEN, "edit-len", "LEN: 4 steps, the ruler; the length line"},
        {UI_F_MICRO, "edit-micro", "MICRO: +6/24, the tape"},
        {UI_F_RATCH, "edit-ratch", "RATCH: the bounces"},
        {UI_F_CHNC, "edit-chnc", "CHNC: the chance, its dots"},
        {UI_F_COND, "edit-cond", "COND: the condition"},
        {UI_F_TRIG, "edit-trig", "TRIG: NOTE / LOCK / TIE"},
        {UI_F_SOUND, "edit-sound", "SOUND: the cassette"},
        {UI_F_PLOCK, "edit-plock", "P-LOCK: the step's locks"},
    };
    size_t i;
    outdir = out;
    nshot = 0;
    failed = 0;

    keys_two_pages();                                   /* (STEPS) */
    qd_step_set_trig(qd_ctl_pat(&A), T3, 16 + 2, QD_TRIG_LOCK);
    qd_step_set_trig(qd_ctl_pat(&A), T3, 16 + 14, QD_TRIG_LOCK);
    qd_step_set_vel(qd_ctl_pat(&A), T3, 16 + 0, 127);
    qd_step_set_vel(qd_ctl_pat(&A), T3, 16 + 4, 90);
    qd_step_set_vel(qd_ctl_pat(&A), T3, 16 + 8, 55);
    qd_step_set_vel(qd_ctl_pat(&A), T3, 16 + 12, 25);
    qd_step_set_len(qd_ctl_pat(&A), T3, 16 + 4, 8);
    qd_step_set_micro(qd_ctl_pat(&A), T3, 16 + 4, 6);
    qd_step_set_ratchet(qd_ctl_pat(&A), T3, 16 + 4, 3, 1);
    qd_step_set_chance(qd_ctl_pat(&A), T3, 16 + 4, 60);
    vhome();                                    /* EDIT */
    black(UI_F_NOTE);
    run(400);
    shot("edit-default-note", "nothing held: NOTE's track default");
    black(UI_F_VEL);
    run(400);
    shot("edit-default", "nothing held: VEL's track default");
    for (i = 0; i < sizeof MODES / sizeof MODES[0]; i++) {
        edit_mode(MODES[i].m);
        shot(MODES[i].name, MODES[i].caption);
    }
    edit_mode(UI_F_VEL);                                /* a value: the mallet and the cell's fill */
    black(1);                                           /* B2 */
    run(60);
    shot("edit-vel-b2", "VEL B2 on the cursor's step: the value and its fill fall together");
    btap(QD_B_MUTE);                                    /* the latch off */
    qd_app_key(&A, BK(UI_F_LEN), 1, 100);               /* a mode key held: its default */
    run(700);
    shot("edit-mkey", "LEN's key held: the track's default (B11 RESET)");
    qd_app_key(&A, BK(UI_F_LEN), 0, 0);
    run(50);
    edit_mode(UI_F_VEL);
    btap(QD_B_PLAY);                                    /* playing: the marker, the pop, the mallet striking */
    while (qd_seq_track_step(&A.seq, 2) != 20)
        run(1);
    run(30);
    shot("edit-playing", "playing: step 21 strikes, the marker over the row");
    btap(QD_B_PLAY);

    {   /* P1.3r's shots beside them (copies kept in OUT as p13s-before-*.png) */
        static const char *const B[3][2] = {{"p13s-before-default.png", "before (P1.3r): VEL's default"},
                                            {"p13s-before-note.png", "before (P1.3r): NOTE"},
                                            {"p13s-before-vel.png", "before (P1.3r): VEL"}};
        char path[1024];
        int k;
        for (k = 0; k < 3 && nshot < NSHOT - 1; k++) {
            FILE *f;
            snprintf(path, sizeof path, "%s/%s", outdir, B[k][0]);
            f = fopen(path, "rb");
            if (!f)
                continue;
            fclose(f);
            snprintf(shots[nshot].file, sizeof shots[nshot].file, "%s", B[k][0]);
            snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", B[k][1]);
            nshot++;
        }
    }
    if (!failed)
        sheet();
    return failed;
}
