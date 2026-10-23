/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* render_frames' P1.5v part: the owner's fixes to the presets and the pool: DELETE? (a red X, the head red) and the
 * cassette breaking up; B10 from ALL: + POOL (an empty tray, +) and - POOL (a full tray), OUT? with its users going to
 * TRACK; tags (SHIFT + B2..B8: a chip, the cassette's dots, the filter); SAVE PRESET's NEW / REPLACE choice; a step's
 * pool picker up while held. Into OUT/p15v-*.png and OUT/p15v-sheet.png, the keys' LEDs under each */
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
#include "snd_peek.h"                          /* (P2.1: LIB, POOL) */

#define NSHOT 12
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p15v-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p15v: cannot write %s\n", path);
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
static void lit_b(int b)                                /* (on a blinking key's lit phase) */
{
    int n = 0;
    qd_leds_t l;
    do {
        run(1);
        qd_app_leds(&A, &l);
    } while (l.key[BK(b)] != QD_LED_LIT && ++n < 400);
}
static void to_code(int code)                           /* the browser on that entry */
{
    static int list[QD_BR_LIST];
    const int n = qd_br_list(&A, A.ui.br.src, A.ui.br.kind, list);
    int i;
    for (i = 0; i < n && list[i] != code; i++)
        ;
    if (i < n && i != A.ui.br.pos)
        turn(QD_K_PRESETS, i - A.ui.br.pos);
}

int render_p15v(const char *out)
{
    int16_t p[QD_QUAD_NP];
    int k;
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
    qd_quad_preset_patch(3, p);                         /* MY presets */
    qd_lib_store(&A, 0, "DARK BASS", p);
    qd_quad_preset_patch(6, p);
    qd_lib_store(&A, 1, "VOX PAD", p);
    qd_quad_preset_patch(4, p);
    qd_lib_store(&A, 2, "GONE SOON", p);
    qd_tags_set(&A, QD_BR_MINE_CODE + 1, QD_PKB(QD_PK_PAD) | QD_PKB(QD_PK_LEAD) | QD_PKB(QD_PK_FX));
    run(100);

    turn(QD_K_PRESETS, 1);                              /* 1 DELETE? */
    black(8);
    to_code(QD_BR_MINE_CODE + 2);
    run(700);
    black(10);
    run(260);
    lit_b(10);
    shot("delete", "B11 on one of MY presets: DELETE? (a red X; B11 blinks)", 1);
    black(10);                                          /* 2 it breaks up */
    run(230);
    shot("gone", "B11 again: it breaks up and fizzes away", 1);
    run(1200);
    black(8), black(8);                                 /* 3 ALL: not in the pool: + */
    qd_sounds_clear(&A.proj);
    to_code(4);
    run(900);
    shot("add", "ALL, not in the pool: an empty tray, + (B10 lit)", 1);
    black(9);                                           /* 4 in: a full tray */
    run(1100);
    shot("in", "B10: in the pool (a full tray; B10 breathing: it takes it out)", 1);
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 3, 2);           /* 5 OUT?: steps use it */
    qd_step_set_sound(qd_proj_pat(&A.proj, A.seq.cur), 3, 2, qd_br_pool_k(&A));
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 3, 6);
    qd_step_set_sound(qd_proj_pat(&A.proj, A.seq.cur), 3, 6, qd_br_pool_k(&A));
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 1, 4);
    qd_step_set_sound(qd_proj_pat(&A.proj, A.seq.cur), 1, 4, qd_br_pool_k(&A));
    run(50);
    black(9);
    run(300);
    lit_b(9);
    shot("out", "B10 again, steps use it: OUT? (its users go back to TRACK)", 1);
    black(9);
    run(1000);
    black(8);                                           /* 6 tags: SHIFT + B5 */
    to_code(QD_BR_MINE_CODE + 1);
    run(700);
    btap(QD_B_FUNC);
    qd_app_button(&A, QD_B_FUNC, 1);
    run(10);
    black(1);
    run(120);
    shot("tag", "SHIFT + B2 on VOX PAD: BASS on (its dots; SHIFT: its tags lit)", 1);
    qd_app_button(&A, QD_B_FUNC, 0);
    run(1000);
    black(8), black(8);                                 /* 7 B5: PAD lists every one tagged PAD */
    black(4);
    to_code(QD_BR_MINE_CODE + 1);
    run(900);
    shot("filter", "B5 (PAD): every preset tagged PAD", 1);
    btap(QD_B_HOME);
    run(300);
    {                                                   /* 8, 9 SAVE PRESET: NEW / REPLACE */
        qd_track_t *t = &A.model.t[3];
        memcpy(t->patch, LIB(&A, 0)->patch, sizeof t->patch);
        strcpy(t->sound_name, "DARK BASS");
        t->preset = LIB(&A, 0)->preset;
        t->patch[QD_QP_FREQ] = 6000;
        t->edited = 1;
        run(20);
    }
    btap(QD_B_SAVE);
    run(1300);
    shot("new", "SAVE, from one of MY presets: NEW chosen (F2 lit)", 0);
    btap(QD_B_OCTUP);
    run(300);
    shot("replace", "F2: REPLACE chosen; SAVE does the chosen", 0);
    btap(QD_B_HOME);
    run(300);
    for (k = 0; k < 2; k++)                             /* 10 a step held + PRESETS: up while held */
        qd_pool_add_sound(&A, LIB(&A, k));
    {
        qd_sound_t f;
        for (k = 17; k < 21; k++) {
            qd_factory_get(&A, k, &f);
            qd_pool_add_sound(&A, &f);
        }
    }
    key(WK(5), 1);
    run(300);
    turn(QD_K_PRESETS, 3);
    run(4000);
    shot("picker", "A step held + PRESETS: the pool picker, still up 4 s later", 0);
    key(WK(5), 0);
    run(100);
    shot("released", "Let go: closed, the sound kept", 0);
    run(500);

    if (!failed)
        sheet_as("p15v-sheet.png", "FIZZ P1.5v: delete, the pool from any source, tags, NEW / REPLACE, the picker held (1.5x)");
    return failed;
}
