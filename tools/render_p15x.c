/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.5x part: presets carry their mix (the cassette's three send bars) and the black keys show the
 * chosen preset's tags (the filter's key solid, its tags breathing; SHIFT + a key toggles one). Into OUT/p15x-*.png and
 * OUT/p15x-sheet.png, the keys' LEDs under each */
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

#define NSHOT 6
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p15x-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p15x: cannot write %s\n", path);
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
static void key(int k, int down) { qd_app_key(&A, k, down, 100); }
static void black(int b)
{
    key(BK(b), 1);
    run(20);
    key(BK(b), 0);
    run(10);
}
static void turn(int k, int d)
{
    A.ui.knob_ms[k] = 0;
    qd_app_knob(&A, k, d);
    run(5);
}
static void to_code(int code)
{
    static int list[QD_BR_LIST];
    const int n = qd_br_list(&A, A.ui.br.src, A.ui.br.kind, list);
    int i;
    for (i = 0; i < n && list[i] != code; i++)
        ;
    if (i < n && i != A.ui.br.pos)
        turn(QD_K_PRESETS, i - A.ui.br.pos);
}
static void peak(void)                                  /* (on a breath's peak: the tags' LEDs bright in the sheet) */
{
    run((uint32_t)(QD_LED_BREATH_MS - now % QD_LED_BREATH_MS));
}

int render_p15x(const char *out)
{
    int16_t p[QD_QUAD_NP];
    outdir = out;
    nshot = 0;
    failed = 0;
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    btap(QD_B_HOME);
    run(300);
    qd_ctl_select_track(&A, 3);
    qd_quad_preset_patch(6, p);                         /* VOX PAD: two tags, its own sends */
    qd_lib_store(&A, 0, "VOX PAD", p);
    qd_tags_set(&A, QD_BR_MINE_CODE, QD_PKB(QD_PK_PAD) | QD_PKB(QD_PK_LEAD));
    A.per.lib[0].chorus = 90, A.per.lib[0].delay = 10, A.per.lib[0].reverb = 120, A.per.lib[0].pan = -20;
    run(100);

    turn(QD_K_PRESETS, 1);                              /* 1 EP: ChoralRoot's sends, KEYS breathing */
    to_code(1);
    run(700);
    peak();
    shot("ep", "EP: sends 30 / 20 / 40; B1 solid, B3 (KEYS) breathing", 1);
    to_code(6);                                         /* 2 GLASS PAD */
    run(700);
    peak();
    shot("glass", "GLASS PAD: sends 50 / 25 / 70; B5 (PAD) breathing", 1);
    black(8);                                           /* 3 MY PRESETS: VOX PAD, two tags; the filter on one */
    black(4);
    run(700);
    peak();
    shot("filter", "VOX PAD, filter PAD: B5 solid, B6 (LEAD) breathing", 1);
    qd_app_button(&A, QD_B_FUNC, 1);                    /* 4 SHIFT + B8: FX on */
    run(10);
    black(7);
    run(120);
    shot("toggle", "SHIFT + B8: FX on (its chip; B8 starts breathing)", 1);
    qd_app_button(&A, QD_B_FUNC, 0);
    run(900);
    btap(QD_B_HOME);
    run(500);

    if (!failed)
        sheet_as("p15x-sheet.png", "FIZZ P1.5x: presets carry their mix; the black keys show the tags (1.5x)");
    return failed;
}
