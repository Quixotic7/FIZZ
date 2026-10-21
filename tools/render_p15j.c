/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.5j part: the sound editor's uniform foot (four fixed columns, one chip size, 4 px clear of the
 * frame), the shorter RATIO picture with the mix's own row and the operators fading with the mix: the owner's screen
 * (algorithm 5, C and B locked, MIX -40), algorithms 1 and 7 at MIX -64 / 0 / +63, three other pages with locks; and
 * every page locked (p15j-pages.png); into
 * OUT/p15j-*.png and OUT/p15j-sheet.png */
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

#define NSHOT 20
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p15j-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p15j: cannot write %s\n", path);
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
    const int cell = (int)(240.f * sc), leds_h = 60, cell_h = cell + 34 + leds_h, pad = 22, cols = 3;
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
static void set(int id, int v) { qd_player_live_edit(&A.player, A.sel, id, v); }
static void lock(int s, int id, int v) { qd_lock_set(&A.proj.pat[A.seq.cur], A.sel, s, id, v); }
int render_p15j(const char *out)
{
    static const int ALG[2] = {1, 7}, MIX[3] = {-64, 0, 63};
    char nm[32], cap[96];
    int i, j, pg;
    outdir = out;
    nshot = 0;
    failed = 0;
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    btap(QD_B_HOME);
    run(100);
    bdown(QD_B_MIX);                                    /* (an empty pattern, A05: the track's own sound on its steps) */
    run(20);
    qd_app_key(&A, WK(4), 1, 100);
    run(30);
    qd_app_key(&A, WK(4), 0, 0);
    bup(QD_B_MIX);
    run(100);
    qd_step_on(&A.proj.pat[A.seq.cur], 0, 0);
    btap(QD_B_SOUND);
    /* the owner's screen: algorithm 5, C 11.00 and B 4.00 over 12.00 locked on the step, MIX -40 */
    A.ui.snd_page = qd_snd_page_of("RATIO");
    set(QD_QP_ALGO, 5), set(QD_QP_MIX, -40);
    lock(0, QD_QP_RC, 13);                              /* (11.00) */
    lock(0, QD_QP_RB1, 14), lock(0, QD_QP_RB2, 6);      /* (B1 12.00, B2 4.00) */
    qd_app_key(&A, WK(0), 1, 100);
    run(400);
    shot("owner", "The owner's screen: algorithm 5, C and B locked, MIX -40 (a step held)", 0);
    qd_app_key(&A, WK(0), 0, 0);
    run(300);
    shot("owner-track", "The same, nothing held: C and B locked on a step (outlined)", 0);
    for (i = 0; i < 2; i++)
        for (j = 0; j < 3; j++) {
            set(QD_QP_ALGO, ALG[i]), set(QD_QP_MIX, MIX[j]);
            A.ui.snd_knob = 3;
            run(300);
            snprintf(nm, sizeof nm, "alg%d-%d", ALG[i], j);
            snprintf(cap, sizeof cap, "Algorithm %d, MIX %+d", ALG[i], MIX[j]);
            shot(nm, cap, 0);
        }
    A.ui.snd_knob = -1;
    lock(0, QD_QP_FREQ, 6000), lock(0, QD_QP_FTYPE, 2);
    lock(0, QD_QP_AATK, 30), lock(0, QD_QP_AEND, 60);
    lock(0, QD_QP_LFO(0, QD_QL_SPEED), 4000), lock(0, QD_QP_LFO(0, QD_QL_DEST), 3);
    qd_app_key(&A, WK(0), 1, 100);
    run(300);
    A.ui.snd_page = qd_snd_page_of("FILTER");
    run(200);
    shot("filter-held", "FILTER, a step held: FREQ and TYPE locked (filled)", 0);
    A.ui.snd_page = qd_snd_page_of("ENV A");
    run(200);
    shot("enva-held", "ENV A, a step held: ATK and END locked", 0);
    qd_app_key(&A, WK(0), 0, 0);
    run(300);
    A.ui.snd_page = qd_snd_page_of("LFO 1");
    run(200);
    shot("lfo1-track", "LFO 1, nothing held: SPEED and DEST locked somewhere (outlined)", 0);
    if (!failed)
        sheet_as("p15j-sheet.png", "FIZZ P1.5j: the editor's uniform foot, the RATIO picture and the mix (1.5x)");
    /* every page, a step held with every value locked (the margins) */
    nshot = 0;
    for (pg = 0; pg < QD_SND_NPAGE && pg < NSHOT; pg++) {
        int k;
        A.ui.snd_page = pg;
        qd_app_key(&A, WK(0), 1, 100);
        run(300);
        for (k = QD_K_1; k <= QD_K_4; k++) {
            A.ui.knob_ms[k] = 0;
            qd_app_knob(&A, k, 1);
            run(5);
        }
        A.ui.knob_ms[QD_K_ALGO] = 0;
        qd_app_knob(&A, QD_K_ALGO, 1);
        run(100);
        snprintf(nm, sizeof nm, "page-%d", pg);
        snprintf(cap, sizeof cap, "%s, every value locked", QD_SND_PAGES[pg].name);
        shot(nm, cap, 0);
        qd_app_key(&A, WK(0), 0, 0);
        run(30);
    }
    btap(QD_B_HOME);
    if (!failed)
        sheet_as("p15j-pages.png", "FIZZ P1.5j: every editor page, a step held, every value locked (1.5x)");
    return failed;
}
