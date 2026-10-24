/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* render_frames' P2.11 part: the Geek Out pages (mockups/fizz-geekout.html's moments) on the demo playing, the
 * platform's figures as the unit's port gives them (set here: a load of 65 %, 36 fps in a dip, RAM 70 %, the store 23 %,
 * the ISR's longest half 1027 us, the longest lock 41 us), a steal or two; after a crash; stopped; and as the host shows
 * them (no platform figures: dim). Into OUT/p211-*.png and OUT/p211-sheet.png */
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p211-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p211: cannot write %s\n", path);
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
    const int cell = (int)(240.f * sc), leds_h = 60, cell_h = cell + 34 + leds_h, pad = 22, cols = 4;
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
static void shome(void)                                 /* SHIFT + HOME: the next view */
{
    bdown(QD_B_FUNC);
    run(10);
    btap(QD_B_HOME);
    bup(QD_B_FUNC);
    run(10);
}
static void unit_stats(int dip)                         /* the port's figures, as the unit gives them */
{
    qd_app_stats_t *st = qd_app_stats(&A);
    st->have = QD_ST_LOAD0 | QD_ST_FPS | QD_ST_ISR | QD_ST_LOCK | QD_ST_RAM | QD_ST_STORE;
    st->load_pm[0] = 650;
    st->fps10 = dip ? 360 : 600;
    st->frame_us = dip ? 20500u : 16700u;
    st->isr_us = 1027, st->isr_budget_us = 2902, st->late = 0, st->lock_us = 41;
    st->ram_used = 311000u, st->ram_size = 444125u, st->store_used = 230u, st->store_size = 1000u;
    st->boot = 0;
}

int render_p211(const char *out)
{
    int k;
    outdir = out;
    nshot = 0;
    failed = 0;
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    btap(QD_B_HOME);                                    /* the step view (EDIT) */
    run(300);
    unit_stats(0);
    qd_app_play(&A);                                    /* the demo playing */
    run(3000);
    shome();                                            /* EDIT -> METERS */
    unit_stats(1);
    run(1200);
    shot("meters", "METERS: core 0 65 %, core 1 off, a frame-rate dip (36), RAM, the store", 1);
    A.pool.stolen[5] += 1;                              /* (a held note of track 6 taken: the steal's flash) */
    shome();
    unit_stats(1);
    run(300);
    shot("numbers", "NUMBERS: the voices as dots, a steal flashing, ISR / LATE / LOCK, the frame, the BPM", 1);
    A.pool.stolen[2] += 3;
    shome();
    run(250);
    shot("voices", "VOICES: a column a track: its voices, its steals (one flashing), its share", 1);
    shome();
    run(400);
    shot("scope", "SCOPE: ~3 cycles of the loudest track, held on a rising zero crossing", 1);
    for (k = 0; k < 3; k++)
        shome();                                        /* (round to NUMBERS) */
    shome();
    shome();
    A.stats.boot = "WATCHDOG";
    A.stats.have |= QD_ST_BOOT;
    run(300);
    shot("numbers-crash", "NUMBERS after a crash: the boot's cause at the foot", 1);
    A.stats.have &= ~(uint32_t)QD_ST_BOOT;
    qd_app_stop(&A);
    for (k = 0; k < 6; k++)
        shome();                                        /* (round to METERS) */
    A.stats.load_pm[0] = 50, A.stats.fps10 = 600;
    run(2600);
    shot("meters-stopped", "METERS, stopped: the load at its idle few per cent, the peaks followed", 1);
    memset(&A.stats, 0, sizeof A.stats);
    run(300);
    shot("meters-host", "METERS on the host (no platform figures: dim)", 0);
    sheet_as("p211-sheet.png", "P2.11: the Geek Out pages");
    return failed;
}
