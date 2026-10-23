/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.5u part: typing names on the keybed (SAVE PRESET's name selected, a key typed, SHIFT's signs, PRESETS'
 * ideas, ALGORITHM's cursor, F1's delete, a full name, a project's name), from real states on a RAM flash, in the order
 * of mockups/fizz-names.html's eight screens; into OUT/p15u-*.png and OUT/p15u-sheet.png, each beside the mock-up's
 * screen (MOCKS/fizz-names-NN.png, mockups/render_png.py nm-1 .. nm-8) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ctl.h"
#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"
#include "sounds.h"
#include "ui.h"

#define NSHOT 20
static qd_app_t A;
static uint32_t now;
static uint64_t frames_done;
static uint16_t lcd[QD_W * QD_H];
static const char *outdir, *mockdir;
static struct {
    char file[64], caption[112];
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
static void shot(const char *name, const char *caption)
{
    char path[1024];
    qd_app_frame(&A, lcd);
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p15u-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p15u: cannot write %s\n", path);
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

/* ---------------------------------------------------------------- the sheet: the mock-up's screen and FIZZ's, side by
 * side, two pairs a row, at 1.5x */
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
    const float sc = 1.5f;
    const int cell = (int)(240.f * sc), cell_h = cell + 34, pad = 18, cols = 4;
    const int rows = (nshot + 1) / 2, W = pad + cols * (cell + pad), H = 50 + rows * (cell_h + pad);
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
    plutovg_canvas_fill_text(c, "FIZZ P1.5u: typing names on the keybed: the mock-up (left) and FIZZ (right), 1.5x",
                             -1, PLUTOVG_TEXT_ENCODING_UTF8, (float)pad, 32.f);
    for (i = 0; i < nshot; i++) {
        const float x = (float)(pad + (i % 2) * 2 * (cell + pad)), y = (float)(50 + (i / 2) * (cell_h + pad));
        plutovg_surface_t *m, *o;
        char cap[160];
        snprintf(path, sizeof path, "%s/fizz-names-%02d.png", mockdir, i + 1);
        m = plutovg_surface_load_from_image_file(path);
        snprintf(path, sizeof path, "%s/%s", outdir, shots[i].file);
        o = plutovg_surface_load_from_image_file(path);
        snprintf(cap, sizeof cap, "%d  %s", i + 1, shots[i].caption);
        plutovg_canvas_set_font_size(c, 13.f);
        plutovg_canvas_set_rgb(c, 0.85f, 0.85f, 0.85f);
        plutovg_canvas_fill_text(c, cap, -1, PLUTOVG_TEXT_ENCODING_UTF8, x, y + 20.f);
        if (m) {
            draw_surface(c, m, x, y + 30.f, sc);
            plutovg_surface_destroy(m);
        }
        if (o) {
            draw_surface(c, o, x + (float)(cell + pad), y + 30.f, sc);
            plutovg_surface_destroy(o);
        }
    }
    snprintf(path, sizeof path, "%s/p15u-sheet.png", outdir);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens beside the mock-up's)\n", path, nshot);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
}

/* ---------------------------------------------------------------- the script */
static void key(int k, int down) { qd_app_key(&A, k, down, 100); }
static void tap(int k)
{
    key(k, 1);
    run(20);
    key(k, 0);
    run(10);
}
static void type(const char *s)                         /* typed on the keys: A..K black, L..Z and SPACE white */
{
    static const char B[] = "ABCDEFGHIJK", W[] = "LMNOPQRSTUVWXYZ ";
    for (; *s; s++) {
        const char *b = strchr(B, *s), *w = strchr(W, *s);
        if (b)
            tap(BK((int)(b - B)));
        else if (w)
            tap(WK((int)(w - W)));
    }
}
static void turn(int k, int d)
{
    A.ui.knob_ms[k] = 0;
    qd_app_knob(&A, k, d);
    run(5);
}

int render_p15u(const char *out, const char *mocks)
{
    outdir = out;
    mockdir = mocks;
    nshot = 0;
    failed = 0;

    memset(FL, 0xFF, sizeof FL);
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    qd_persist_attach(&A, &FLASH);
    qd_proj_new(&A, 0);
    run(30);
    btap(QD_B_HOME);
    qd_ctl_select_track(&A, 3);                         /* track 4: a PLUCK, brightened: BRIGHT PLUCK */
    qd_track_load_preset(&A.model.t[3], 4);
    A.model.t[3].edited = 1;
    run(100);

    btap(QD_B_SAVE);                                    /* 1 the name ready, selected */
    run(300);
    shot("start", "SAVE: a name ready, selected");
    tap(BK(3));                                         /* 2 D, then U (W10) */
    run(400);
    key(WK(9), 1);
    run(165);
    shot("type", "Typing: press a key, its letter goes in");
    key(WK(9), 0);
    run(1300);
    type("ST");                                         /* 3 SHIFT held */
    run(1300);
    bdown(QD_B_FUNC);
    run(200);
    shot("shift", "SHIFT held: digits and signs");
    bup(QD_B_FUNC);
    run(20);
    turn(QD_K_PRESETS, 3);                              /* 4 PRESETS: ideas */
    run(200);
    shot("ideas", "PRESETS: name ideas");
    run(QD_TY_IDEA_MS + 100);
    type("DUST PAD");                                   /* 5 ALGORITHM: the cursor */
    turn(QD_K_ALGO, -4);
    run(1300);
    shot("cursor", "ALGORITHM: the cursor");
    turn(QD_K_ALGO, 4);                                 /* 6 F1: delete */
    run(1300);
    bdown(QD_B_OCTDN);
    run(200);
    shot("delete", "F1: delete");
    bup(QD_B_OCTDN);
    run(600);
    bdown(QD_B_FUNC);                                   /* 7 twelve characters: full (SHIFT + F1 cleared first) */
    run(10);
    btap(QD_B_OCTDN);
    bup(QD_B_FUNC);
    run(20);
    type("DUSTY PADLAN");
    run(1300);
    key(BK(4), 1);
    run(50);
    shot("full", "Twelve characters: full");
    key(BK(4), 0);
    run(700);
    btap(QD_B_HOME);                                    /* 8 a project's name: the same keyboard */
    run(200);
    btap(QD_B_MIX);
    run(200);
    tap(BK(9));
    run(100);
    type("NIGHT BUS");
    run(1300);
    shot("project", "The same keyboard everywhere");
    btap(QD_B_HOME);
    run(500);

    if (!failed)
        sheet();
    return failed;
}
