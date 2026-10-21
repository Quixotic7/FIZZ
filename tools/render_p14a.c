/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.4a part: the sound editor on EDIT: every page of the track's sound (the TRACK frame), a step held
 * with p-locks (its values, the track's left behind), DRIVE, the eight algorithms; into OUT/p14a-*.png,
 * OUT/p14a-sheet.png and OUT/p14a-algos.png */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ctl.h"
#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"
#include "ui.h"

#define NSHOT 64
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p14a-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p14a: cannot write %s\n", path);
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

/* ---------------------------------------------------------------- the sheets */
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

/* shots i0 .. i1 - 1, cols a row, at scale; captions over each */
static void sheet(const char *file, const char *title, int i0, int i1, int cols, float scale)
{
    const int cell_w = (int)(240.f * scale), cell_h = (int)(240.f * scale) + 34, pad = 18;
    const int n = i1 - i0, rows = (n + cols - 1) / cols;
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
    plutovg_canvas_fill_text(c, title, -1, PLUTOVG_TEXT_ENCODING_UTF8, (float)pad, 32.f);
    for (i = 0; i < n; i++) {
        const float x = (float)(pad + (i % cols) * (cell_w + pad)), y = (float)(50 + (i / cols) * (cell_h + pad));
        plutovg_surface_t *ours;
        snprintf(path, sizeof path, "%s/%s", outdir, shots[i0 + i].file);
        ours = plutovg_surface_load_from_image_file(path);
        plutovg_canvas_set_font_size(c, 13.f);
        plutovg_canvas_set_rgb(c, 0.85f, 0.85f, 0.85f);
        plutovg_canvas_fill_text(c, shots[i0 + i].caption, -1, PLUTOVG_TEXT_ENCODING_UTF8, x, y + 20.f);
        if (ours) {
            draw_surface(c, ours, x, y + 30.f, scale);
            plutovg_surface_destroy(ours);
        }
    }
    snprintf(path, sizeof path, "%s/%s", outdir, file);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens)\n", path, n);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
}

/* ---------------------------------------------------------------- the script */
enum { A01, A02, A03 };
enum { T1, T2, T3, T4 };
static void whold(int w)
{
    qd_app_key(&A, WK(w), 1, 100);
    run(300);
}
static void wrel(int w)
{
    qd_app_key(&A, WK(w), 0, 0);
    run(20);
}

int render_p14a(const char *out)
{
    int p, k, split;
    char nm[32], cap[112];
    outdir = out;
    nshot = 0;
    failed = 0;

    at(A02, T3, UI_F_NOTE);                             /* A02 KEYS: its sound, every page */
    btap(QD_B_SOUND);
    for (p = 0; p < QD_SND_NPAGE; p++) {
        A.ui.snd_page = p;
        run(240);
        snprintf(nm, sizeof nm, "page-%02d", p);
        snprintf(cap, sizeof cap, "%s (B%d): %s", QD_SND_GROUP_NAME[QD_SND_PAGES[p].group], QD_SND_PAGES[p].group + 1,
                 QD_SND_PAGES[p].name);
        shot(nm, cap);
    }
    A.ui.snd_page = qd_snd_page_of("ENV A");            /* ENV A, step 1 held: two locks */
    whold(0);
    turn(QD_K_2, -12);
    turn(QD_K_3, -40);
    run(100);
    shot("lock-enva", "step 1 held, KNOB 2 / 3: A DEC and A END locked (the track's dashed)");
    A.ui.snd_page = qd_snd_page_of("LEVEL");
    turn(QD_K_2, 70);
    run(100);
    shot("lock-drive", "LEVEL page, step 1 held: DRIVE 70 locked on it");
    wrel(0);
    turn(QD_K_2, 25);
    run(100);
    shot("drive-track", "nothing held: the track's DRIVE 25");
    turn(QD_K_2, 100);
    run(100);
    shot("drive-max", "the track's DRIVE 125: the sine squared off");
    btap(QD_B_SOUND);

    split = nshot;
    at(A02, T3, UI_F_NOTE);                             /* the eight algorithms */
    btap(QD_B_SOUND);
    for (k = 1; k <= 8; k++) {
        qd_player_live_edit(&A.player, A.sel, QD_QP_ALGO, k);
        run(200);
        snprintf(nm, sizeof nm, "algo-%d", k);
        snprintf(cap, sizeof cap, "RATIO, ALGORITHM %d", k);
        shot(nm, cap);
    }
    btap(QD_B_SOUND);

    if (!failed) {
        sheet("p14a-sheet.png", "FIZZ P1.4a: the sound editor on EDIT (2x)", 0, split, 3, 2.f);
        sheet("p14a-algos.png", "FIZZ P1.4a: the eight algorithms (1.5x)", split, nshot, 4, 1.5f);
    }
    return failed;
}
