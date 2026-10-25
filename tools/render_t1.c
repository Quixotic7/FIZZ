/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* render_frames' T1 part: TOOLS, the track tools (mockups/fizz-tools.html's moments, docs/TRACK-TOOLS-PLAN.md): ROTATE,
 * REVERSE, EUCLID, LENGTH live on a bass line playing, OCT- back to the original, the name popped, the scope's takeover,
 * EUCLID over the PATTERN, OCT+ kept, the device's drawn-only fallback. Into OUT/t1-*.png and OUT/t1-sheet.png */
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

#define NSHOT 24
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "t1-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_t1: cannot write %s\n", path);
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

static void black(int b)
{
    qd_app_key(&A, BK(b), 1, 100);
    run(20);
    qd_app_key(&A, BK(b), 0, 0);
    run(20);
}
static void turn(int knob, int d)
{
    A.ui.knob_ms[knob] = 0;
    qd_app_knob(&A, knob, d);
    run(5);
}
static void white(int w)
{
    qd_app_key(&A, WK(w), 1, 100);
    run(20);
    qd_app_key(&A, WK(w), 0, 0);
    run(20);
}
static void tools_open(void)
{
    bdown(QD_B_KEYS);
    run(QD_ARP_HOLD_MS + 10);
    bup(QD_B_KEYS);
    run(QD_TOOL_POP_MS + 20);                           /* (the name gone) */
}
/* track t: a bass line, page 1 x..x..x.x.x..x.. with its notes, page 2 a few; the loop pages 1..2 */
static void bass(int t)
{
    static const char *const L = "x..x..x.x.x..x..";
    static const int8_t N[16] = {0, 0, 0, 3, 0, 0, 7, 0, 5, 0, 3, 0, 0, 10, 0, 0};
    qd_pattern_t *p = qd_ctl_pat(&A);
    int i;
    for (i = 0; i < 16; i++)
        if (L[i] == 'x')
            qd_step_set_note(p, t, i, 0, 36 + N[i]);
    qd_step_set_note(p, t, 16 + 2, 0, 48);
    qd_step_set_note(p, t, 16 + 9, 0, 43);
    qd_page_set_loop(p, t, 0x03);
}

int render_t1(const char *out)
{
    outdir = out;
    nshot = 0;
    failed = 0;
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    btap(QD_B_HOME);
    bdown(QD_B_MIX);                                    /* the empty A05 */
    white(4);
    bup(QD_B_MIX);
    run(10);
    bass(1);
    bass(0);
    bass(4);
    qd_ctl_select_track(&A, 1);
    A.ui.follow = 0;                                    /* (page 1 kept on screen while it plays) */
    qd_app_play(&A);
    run(500);
    tools_open();
    shot("rotate-open", "ARP held: TOOLS, ROTATE, TRACK (nothing pending); W: the page's steps", 1);
    turn(QD_K_1, 2);
    run(300);
    shot("rotate", "ROTATE +2 (KNOB 1): rewritten live, heard; the orange dot; the W LEDs as it will be", 1);
    btap(QD_B_OCTUP);
    run(150);
    shot("kept", "OCT+: kept, one undo level, KEPT popped", 1);
    run(QD_TOOL_POP_MS);
    black(1);
    black(10);
    black(10);
    black(10);                                          /* TRACK -> PATTERN -> SELECTED -> PAGE */
    run(QD_TK_MS + 200);
    black(1);                                           /* B2 again: flipped */
    run(250);
    shot("reverse", "REVERSE, PAGE: B2 again flips the page (heard); the mirror line", 1);
    btap(QD_B_OCTDN);                                   /* back */
    run(150);
    shot("reverted", "OCT-: REVERTED (TOOLS stays; HOME is the way out)", 1);
    run(QD_TOOL_POP_MS);
    turn(QD_K_PRESETS, 2);
    run(150);
    shot("popup", "PRESETS +2: EUCLID, its name pops (no frame; ~0.8 s)", 1);
    run(QD_TOOL_POP_MS);
    turn(QD_K_1, 1);
    run(300);
    shot("euclid", "EUCLID 5 of 16 (KNOB 1 hits, KNOB 2 the length): NEW", 1);
    turn(QD_K_1, -2);
    turn(QD_K_2, -8);
    turn(QD_K_3, 2);
    turn(QD_K_4, 1);
    run(300);
    shot("euclid-3-8-add", "EUCLID 3 of 8, rotated 2, KNOB 4: ADD (the bass's trigs stay, rings new)", 1);
    btap(QD_B_OCTDN);
    run(QD_TOOL_POP_MS);
    turn(QD_K_ALGO, 1);
    run(200);
    shot("scope", "ALGORITHM (or B11): the scope's takeover (TRACK of four)", 1);
    turn(QD_K_ALGO, 1);
    run(QD_TK_MS + 200);
    turn(QD_K_1, 2);
    turn(QD_K_2, 8);
    run(300);
    shot("euclid-pattern", "EUCLID over the PATTERN: every track at once (the box dashed)", 1);
    btap(QD_B_OCTUP);
    run(QD_TOOL_POP_MS);
    white(0);
    white(3);
    white(6);
    white(10);
    run(QD_TK_MS + 200);
    black(1);
    run(QD_TOOL_POP_MS);
    black(1);
    run(300);
    shot("selected", "W1, W4, W7, W11: SELECTED STEPS (marked, the W LEDs blinking); REVERSE on them alone", 1);
    btap(QD_B_OCTDN);
    run(QD_TOOL_POP_MS);
    turn(QD_K_ALGO, 2);                                    /* SELECTED -> TRACK */
    black(6);
    run(QD_TK_MS + 200);
    turn(QD_K_1, 1);
    run(300);
    shot("length", "LENGTH x2 (KNOB 1): pages 3, 4 new (outlined), 64 steps", 1);
    turn(QD_K_1, -2);
    run(300);
    shot("length-half", "LENGTH /2: page 2 leaves the loop (faded), 16 steps; nothing deleted", 1);
    btap(QD_B_OCTDN);
    black(0);
    run(QD_TOOL_POP_MS + 20);
    A.ui.tl.flags |= QD_TLF_PEND | QD_TLF_NOLIVE;       /* (as the device's log does it when the pattern is too big) */
    A.ui.tl.rot = 3;
    run(300);
    shot("drawn-only", "The device's fallback: drawn only (nothing written), the undo arrow crossed out", 1);
    A.ui.tl.flags = 0, A.ui.tl.rot = 0;
    /* T2 */
    black(2);                                           /* (TRACK) */
    run(QD_TK_MS + 200);
    turn(QD_K_1, 5);
    turn(QD_K_2, 3);
    turn(QD_K_3, 20);
    turn(QD_K_4, 40);
    run(300);
    shot("random", "RANDOM: the die (B3 again rolls), KNOB 1 density, 2 notes, 3 velocity, 4 chance", 1);
    btap(QD_B_OCTDN);
    black(5);
    run(QD_TOOL_POP_MS);
    turn(QD_K_1, 6);
    turn(QD_K_2, 25);
    run(300);
    shot("humanize", "HUMANIZE: KNOB 1 the timing jitter (the steps off their lines), KNOB 2 the velocity", 1);
    btap(QD_B_OCTDN);
    black(4);
    run(QD_TOOL_POP_MS);
    turn(QD_K_1, 5);
    turn(QD_K_2, 3);
    turn(QD_K_3, 6);
    turn(QD_K_4, 12);
    run(300);
    shot("grids", "GRIDS: X (KNOB 1), Y (KNOB 2), density (KNOB 3), chaos (KNOB 4, the cloud); B5 again: the part", 1);
    turn(QD_K_ALGO, 1);                                    /* PATTERN */
    run(QD_TK_MS + 200);
    turn(QD_K_3, 4);
    run(300);
    shot("grids-pattern", "GRIDS over the PATTERN: LOW, MID, HIGH to tracks 1, 2, 3 (DRUMS first)", 1);
    btap(QD_B_OCTDN);
    /* T3 */
    run(QD_TOOL_POP_MS);
    black(7);
    run(150);
    shot("copied", "T3: PATTERN, B8: the pattern copied (B9 pastes it, B10 twice clears it)", 1);
    run(QD_TOOL_POP_MS);
    black(9);
    run(150);
    shot("clearq", "B10 on PATTERN: CLEAR? (B10 blinking: again to clear)", 1);
    run(QD_TOOL_POP_MS);
    btap(QD_B_HOME);
    qd_app_stop(&A);
    sheet_as("t1-sheet.png", "TOOLS: T1 (ROTATE, REVERSE, EUCLID, LENGTH), T2 (RANDOM, HUMANIZE, GRIDS), T3 (COPY, PASTE, CLEAR)");
    return failed;
}
