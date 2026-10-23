/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.5w part: the pool holds references: a pool before and after a REPLACE (its reference and the
 * step locking it play the new sound), SAVE PRESET's REPLACE, DELETE? with where the preset is used (the projects,
 * the steps going to TRACK), the pool after; on a RAM flash with two projects. Into OUT/p15w-*.png and
 * OUT/p15w-sheet.png */
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

#define NSHOT 8
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

/* the RAM flash (1 MiB) */
static uint8_t FL[0x100000];
static int f_read(void *c, uint32_t off, void *d, uint32_t n) { (void)c; memcpy(d, FL + off, n); return 0; }
static int f_erase(void *c, uint32_t off) { (void)c; memset(FL + off / 4096u * 4096u, 0xFF, 4096u); return 0; }
static int f_prog(void *c, uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    (void)c;
    for (i = 0; i < n; i++)
        FL[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
static const qd_flash_t FLASH = {f_read, f_erase, f_prog, 0};

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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p15w-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p15w: cannot write %s\n", path);
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

int render_p15w(const char *out)
{
    int16_t p[QD_QUAD_NP];
    int k, kp;
    outdir = out;
    nshot = 0;
    failed = 0;
    memset(FL, 0xFF, sizeof FL);
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    qd_persist_attach(&A, &FLASH);
    qd_quad_preset_patch(14, p);                        /* LEAD X: one of MY presets, in two projects' pools */
    qd_lib_store(&A, 0, "LEAD X", p);
    qd_proj_new(&A, 1);                                 /* (project 2 first: a reference, a locked step) */
    kp = qd_pool_add_sound(&A, &A.per.lib[0]);
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 2, 4);
    qd_step_set_sound(qd_proj_pat(&A.proj, A.seq.cur), 2, 4, kp);
    qd_persist_flush(&A);
    qd_proj_new(&A, 0);                                 /* project 1: its pool, LEAD X locked on two steps */
    for (k = 17; k < 20; k++) {
        qd_sound_t f;
        qd_factory_get(&A, k, &f);
        qd_pool_add_sound(&A, &f);
    }
    kp = qd_pool_add_sound(&A, &A.per.lib[0]);
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 3, 0);
    qd_step_set_sound(qd_proj_pat(&A.proj, A.seq.cur), 3, 0, kp);
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 3, 8);
    qd_step_set_sound(qd_proj_pat(&A.proj, A.seq.cur), 3, 8, kp);
    btap(QD_B_HOME);
    run(300);
    qd_ctl_select_track(&A, 3);
    run(100);

    turn(QD_K_PRESETS, 1);                              /* 1 the POOL: LEAD X (a reference) */
    black(8), black(8);
    to_code(QD_BR_POOL_CODE + kp);
    run(900);
    shot("before", "The POOL: LEAD X, a reference to one of MY presets", 1);
    btap(QD_B_HOME);
    run(300);
    {                                                   /* 2 track 4 from LEAD X, edited: SAVE, F2: REPLACE */
        qd_track_t *t = &A.model.t[3];
        memcpy(t->patch, A.per.lib[0].patch, sizeof t->patch);
        strcpy(t->sound_name, "LEAD X");
        t->preset = A.per.lib[0].preset;
        t->patch[QD_QP_EATK] = 0;                       /* (a pluck now: its envelope shows it) */
        t->patch[QD_QP_EDEC] = 24;
        t->patch[QD_QP_ESUS] = 0;
        t->patch[QD_QP_EREL] = 10;
        t->edited = 1;
        run(20);
    }
    btap(QD_B_SAVE);
    btap(QD_B_OCTUP);
    run(400);
    shot("replace", "SAVE, F2: REPLACE LEAD X", 0);
    btap(QD_B_SAVE);
    run(1500);
    turn(QD_K_PRESETS, 1);                              /* 3 the POOL after: the new LEAD X */
    black(8), black(8);
    to_code(QD_BR_POOL_CODE + kp);
    run(900);
    shot("after", "The POOL after REPLACE: the new LEAD X (project 2's too)", 1);
    btap(QD_B_HOME);
    run(300);
    key(WK(0), 1);                                      /* 4 the step locking it */
    run(300);
    turn(QD_K_PRESETS, -40);                            /* (TRACK, then the pool's four: LEAD X the fourth) */
    turn(QD_K_PRESETS, kp);
    run(400);
    shot("step", "A step locking it: the new sound", 0);
    key(WK(0), 0);
    run(300);
    turn(QD_K_PRESETS, 1);                              /* 5 DELETE?: where it is used */
    black(8);
    to_code(QD_BR_MINE_CODE + 0);
    run(700);
    black(10);
    run(260);
    shot("delete", "B11 on LEAD X: DELETE? (2 projects, its steps going to TRACK)", 1);
    black(10);
    run(230);
    shot("gone", "B11 again: deleted, its references with it", 1);
    run(1200);
    black(8);                                           /* 6 the POOL after */
    run(900);
    shot("pool", "The POOL: LEAD X gone (project 2's goes as it loads)", 1);
    btap(QD_B_HOME);
    run(500);

    if (!failed)
        sheet_as("p15w-sheet.png", "FIZZ P1.5w: the pool holds references: REPLACE plays everywhere, DELETE? shows its use (1.5x)");
    return failed;
}
