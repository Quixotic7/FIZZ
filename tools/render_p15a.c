/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.5a part: the sounds page (SAVE: storing the track's sound, stored, into the pool, browsing, a sound
 * held, armed onto a sound, the pool, onto the track, the name editor, copy, pick up, delete) and the project pages
 * (SEQ tapped), from real states on a RAM flash, in the order of mockups/fizz-save-pool.html's fourteen screens; into
 * OUT/p15a-*.png and OUT/p15a-sheet.png, each beside the mock-up's screen (MOCKS/fizz-save-pool-NN.png, made by
 * mockups/render_png.py sp-1 .. sp-14) */
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p15a-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p15a: cannot write %s\n", path);
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
    plutovg_font_face_t *f = plutovg_font_face_load_from_data(qd_fonts_gen[3].data, qd_fonts_gen[3].len, 0, NULL, NULL);
    char path[1024];
    int i;
    plutovg_canvas_set_rgb(c, 0.11f, 0.11f, 0.12f);
    plutovg_canvas_rect(c, 0, 0, (float)W, (float)H);
    plutovg_canvas_fill(c);
    plutovg_canvas_set_font(c, f, 18.f);
    plutovg_canvas_set_rgb(c, 0.9f, 0.9f, 0.9f);
    plutovg_canvas_fill_text(c, "FIZZ P1.5a: the library, the pool, the project: the mock-up (left) and FIZZ (right), 1.5x",
                             -1, PLUTOVG_TEXT_ENCODING_UTF8, (float)pad, 32.f);
    for (i = 0; i < nshot; i++) {
        const float x = (float)(pad + (i % 2) * 2 * (cell + pad)), y = (float)(50 + (i / 2) * (cell_h + pad));
        plutovg_surface_t *m, *o;
        char cap[160];
        snprintf(path, sizeof path, "%s/fizz-save-pool-%02d.png", mockdir, i + 1);
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
    snprintf(path, sizeof path, "%s/p15a-sheet.png", outdir);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens beside the mock-up's)\n", path, nshot);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
}

/* ---------------------------------------------------------------- the script */
static int pool_of(const char *name)
{
    int k;
    for (k = 1; k <= A.proj.nsound; k++)
        if (!strcmp(A.proj.sound[k - 1].name, name))
            return k;
    return 1;
}

int render_p15a(const char *out, const char *mocks)
{
    static const struct { const char *name, *from; } LIB[10] = {
        {"DARK BASS", "WOBBLE"}, {"SOFT KEYS", "EP"}, {"GLASS PAD", "GLASS PAD"}, {"TIGHT KICK", "KICK"},
        {"TIN SNARE", "SNARE"}, {"WIDE LEAD", "SQUARE LEAD"}, {"BELL TONE", "BELL"}, {"HAT DUST", "HAT"},
        {"WARM PAD", "STRINGS"}, {"RUBBER BASS", "BASS"}};
    int k;
    outdir = out;
    mockdir = mocks;
    nshot = 0;
    failed = 0;

    memset(FL, 0xFF, sizeof FL);                        /* a unit with a library of ten, a pool with some of them */
    qd_app_init(&A);
    A.ui.view = QD_VIEW_ROLL;   /* (P1.5s: EDIT is the power-on view; made on the PIANO ROLL) */
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    qd_persist_attach(&A, &FLASH);
    qd_proj_name(&A, 0, "NIGHT BUS");
    for (k = 0; k < 10; k++)
        qd_lib_store(&A, k, LIB[k].name, A.proj.sound[pool_of(LIB[k].from) - 1].patch);
    qd_pool_add(&A, "SOFT KEYS", A.per.lib[1].patch);   /* 24 */
    qd_pool_add(&A, "DARK BASS", A.per.lib[0].patch);   /* 25 */
    qd_pool_add(&A, "TINE", A.proj.sound[pool_of("MARIMBA") - 1].patch);
    qd_pool_add(&A, "GLASS PAD", A.per.lib[2].patch);
    for (k = 0; k < 6; k++) {                           /* six steps lock SOFT KEYS */
        qd_step_on(&A.proj.pat[A.seq.cur], 3, k * 2);
        qd_step_set_sound(&A.proj.pat[A.seq.cur], 3, k * 2, 24);
    }
    run(30);
    btap(QD_B_HOME);
    qd_ctl_select_track(&A, 3);
    qd_quad_preset_patch(4, A.model.t[3].patch);        /* track 4: a PLUCK, brightened */
    A.model.t[3].patch[QD_QP_HARM] = 600;
    run(100);

    btap(QD_B_SAVE);                                    /* 1 SAVE: storing */
    run(1400);
    shot("store", "SAVE tapped: store the track's sound");
    btap(QD_B_SAVE);                                    /* 2 stored, F2 offered */
    run(300);
    shot("stored", "SAVE again: stored; F2 adds it to the pool");
    btap(QD_B_OCTUP);                                   /* 3 into the pool */
    run(250);
    shot("to-pool", "F2: into the project's pool");
    black(8);                                           /* 4 the library, W3 */
    black(0);
    wtap(2);
    run(100);
    shot("browse", "Browsing: W chooses and plays once");
    qd_app_key(&A, WK(2), 1, 100);                      /* 5 held: audition */
    run(700);
    shot("audition", "A white key held: audition");
    qd_app_key(&A, WK(2), 0, 0);
    run(30);
    btap(QD_B_SAVE);                                    /* 6 SAVE onto a sound: armed */
    wtap(1);
    btap(QD_B_SAVE);
    run(150);
    shot("armed", "SAVE onto a sound: it blinks, SAVE again replaces it");
    black(4);                                           /* (cancelled) */
    black(8);                                           /* 7 the pool, SOFT KEYS (24) with its users */
    black(1);
    wtap(7);
    A.model.t[3].sound = 24;
    run(700);
    shot("pool", "B9: the project's pool (SOFT KEYS: track 4, six steps)");
    wtap(9);                                            /* 8 F2: TINE onto track 4 */
    run(700);
    btap(QD_B_OCTUP);
    run(200);
    shot("to-track", "F2 in the pool: onto the track");
    run(800);
    black(9);                                           /* 9 the name editor */
    qd_app_knob(&A, QD_K_ALGO, 2);
    run(20);
    qd_app_knob(&A, QD_K_SELECT, 1);
    run(60);
    shot("name", "B10: naming");
    btap(QD_B_OCTDN);
    bdown(QD_B_OCTDN);                                  /* 10 F1 + W: copied (lifted) */
    run(20);
    wtap(4);
    run(100);
    wtap(12);                                           /* (pasted: GHOST 2) */
    run(120);
    shot("copy", "F1 + W: copy (the tile lifted), another W pastes");
    bup(QD_B_OCTDN);
    run(150);
    bdown(QD_B_OCTUP);                                  /* 11 F2 + W: picked up */
    run(20);
    wtap(9);
    run(100);
    shot("move", "F2 + W: pick up, another W drops (a sound: a swap)");
    wtap(2);
    bup(QD_B_OCTUP);
    run(300);
    qd_app_key(&A, BK(10), 1, 100);                     /* 12 B11 + W: delete (the users orange) */
    run(20);
    wtap(7);
    qd_app_key(&A, BK(10), 0, 0);
    run(60);
    shot("delete", "B11 + W: delete (its users orange, then it fizzes)");
    run(1000);
    btap(QD_B_HOME);
    for (k = 0; k < 12; k++)                            /* 13 the project: patterns in use, one just written */
        A.proj.pat[k == 9 ? 32 : k].used = 1;
    A.proj.pat[A.seq.cur].step[0][3].vel = 77;
    run(1700);
    btap(QD_B_MIX);
    run(250);
    shot("project", "SEQ tapped: the project");
    btap(QD_B_MIX);                                     /* 14 the projects */
    qd_persist_flush(&A);
    qd_proj_copy(&A, 0, 1);
    qd_proj_name(&A, 1, "TAPE CLUB");
    qd_proj_copy(&A, 0, 2);
    qd_proj_name(&A, 2, "DRUMS");
    qd_proj_copy(&A, 0, 4);
    qd_proj_name(&A, 4, "FM SKETCH");
    wtap(1);
    run(200);
    shot("projects", "The projects (W2 chosen: F2 loads it)");
    btap(QD_B_HOME);

    if (!failed)
        sheet();
    return failed;
}
