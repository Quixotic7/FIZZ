/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.5t part: the sound system (the library browser, the kinds, MY presets, LOAD, CANCEL, + POOL, the
 * POOL and - POOL, a step's pool picker with TRACK, an empty pool, SAVE PRESET, naming, REPLACE, SAVED), from real
 * states on a RAM flash, in the order of mockups/fizz-presets.html's fifteen screens; into OUT/p15t-*.png and
 * OUT/p15t-sheet.png, each beside the mock-up's screen (MOCKS/fizz-presets-NN.png, mockups/render_png.py pr-1 ..
 * pr-15) */
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
#include "snd_peek.h"                          /* (P2.1: LIB, POOL) */

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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p15t-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p15t: cannot write %s\n", path);
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
    plutovg_canvas_fill_text(c, "FIZZ P1.5t: the sound system: the mock-up (left) and FIZZ (right), 1.5x",
                             -1, PLUTOVG_TEXT_ENCODING_UTF8, (float)pad, 32.f);
    for (i = 0; i < nshot; i++) {
        const float x = (float)(pad + (i % 2) * 2 * (cell + pad)), y = (float)(50 + (i / 2) * (cell_h + pad));
        plutovg_surface_t *m, *o;
        char cap[160];
        snprintf(path, sizeof path, "%s/fizz-presets-%02d.png", mockdir, i + 1);
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
    snprintf(path, sizeof path, "%s/p15t-sheet.png", outdir);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens beside the mock-up's)\n", path, nshot);
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
static void browse_to(int src, int kind, const char *name)   /* the browser on that entry */
{
    int list[QD_BR_LIST], n, i;
    qd_sound_t s;
    if (A.ui.latch != QD_LATCH_BROWSE)
        turn(QD_K_PRESETS, 1);
    while (A.ui.br.src != src)
        black(8);
    if (A.ui.br.kind != kind)
        black(kind);
    n = qd_br_list(&A, src, kind, list);
    for (i = 0; i < n; i++)
        if (qd_br_entry(&A, list[i], &s) && !strcmp(s.name, name))
            break;
    if (i < n && i != A.ui.br.pos)
        turn(QD_K_PRESETS, i - A.ui.br.pos);
}
static int fact(const char *name)                       /* a factory preset's index */
{
    int k;
    for (k = 0; k < QD_NFACTORY; k++) {
        qd_sound_t f;
        qd_factory_preset(k, &f);
        if (!strcmp(f.name, name))
            return k;
    }
    return 0;
}
static void mine(const char *name, const char *from, int harm, int dtun, int imported)
{
    qd_sound_t f;
    qd_factory_preset(fact(from), &f);
    if (harm)
        f.patch[QD_QP_HARM] = (int16_t)harm;
    if (dtun)
        f.patch[QD_QP_DTUN] = (int16_t)dtun;
    qd_lib_store_ex(&A, qd_lib_free_slot(&A), name, f.patch, imported ? QD_SNDF_IMPORTED : 0);
}
static void pool_put(const char *name)                 /* a preset (factory or MY) into the pool */
{
    int k;
    for (k = 0; k < QD_NFACTORY; k++) {
        qd_sound_t f;
        qd_factory_preset(k, &f);
        if (!strcmp(f.name, name)) {
            qd_pool_add_sound(&A, &f);
            return;
        }
    }
    for (k = 0; k < QD_NLIB; k++)
        if (!strcmp(LIB(&A, k)->name, name)) {
            qd_pool_add_sound(&A, LIB(&A, k));
            return;
        }
}
static int pool_num(const char *name)
{
    int k;
    for (k = 1; k <= QD_NSOUND; k++)
        if (qd_sound_used(&A.proj, k) && !strcmp(POOL(&A.proj, k)->name, name))
            return k;
    return 1;
}

int render_p15t(const char *out, const char *mocks)
{
    static const char *const POOL[8] = {"KICK", "SNARE", "HAT", "OPEN HAT", "CLAP", "DARK BASS", "SOFT KEYS", "DIGIT-ONE"};
    int k;
    outdir = out;
    mockdir = mocks;
    nshot = 0;
    failed = 0;

    memset(FL, 0xFF, sizeof FL);                        /* MY presets, a new project's pool of eight */
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    qd_persist_attach(&A, &FLASH);
    qd_proj_new(&A, 1);
    mine("BRIGHT PLUCK", "PLUCK", 600, 0, 0);
    mine("DARK BASS", "BASS", 0, 0, 0);
    mine("SOFT KEYS", "EP", -400, 0, 0);
    mine("DIGIT-ONE", "SQUARE LEAD", 0, 0, 1);
    mine("CHAPPET DL", "EP", 300, 0, 1);
    mine("SQB TK", "WOBBLE", 0, 0, 1);
    mine("WIDE LEAD", "SQUARE LEAD", 0, 3000, 0);
    mine("VOX PAD", "GLASS PAD", 0, 6000, 0);
    for (k = 0; k < 8; k++)
        pool_put(POOL[k]);
    for (k = 0; k < 6; k++) {                           /* six steps lock SOFT KEYS */
        qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 3, k * 2);
        qd_step_set_sound(qd_proj_pat(&A.proj, A.seq.cur), 3, k * 2, pool_num("SOFT KEYS"));
    }
    run(30);
    btap(QD_B_HOME);
    qd_ctl_select_track(&A, 3);                         /* track 4: its own LEAD 4, edited */
    {
        qd_track_t *t = &A.model.t[3];
        qd_track_load_preset(t, fact("SQUARE LEAD"));
        strcpy(t->sound_name, "LEAD 4");
        t->patch[QD_QP_HARM] = 300;
        t->edited = 1;
    }
    run(100);

    browse_to(QD_BR_ALL, 0, "GLASS PAD");               /* 1 */
    run(150);
    shot("browse", "PRESETS turned: the Preset Library");
    browse_to(QD_BR_ALL, 1 + QD_PK_BASS, "WOBBLE");     /* 2 */
    run(400);
    shot("bass", "B1..B8: by kind");
    browse_to(QD_BR_MINE, 0, "DIGIT-ONE");              /* 3 */
    run(400);
    shot("mine", "B9: MY PRESETS / the POOL");
    browse_to(QD_BR_ALL, 0, "GLASS PAD");               /* 4 F2: LOAD */
    btap(QD_B_OCTUP);
    run(700);
    shot("load", "F2: LOAD");
    run(1000);
    {                                                   /* (the own LEAD 4 * again for the cancel) */
        qd_track_t *t = &A.model.t[3];
        qd_track_load_preset(t, fact("SQUARE LEAD"));
        strcpy(t->sound_name, "LEAD 4");
        t->patch[QD_QP_HARM] = 300;
        t->edited = 1;
        run(20);
    }
    browse_to(QD_BR_ALL, 0, "GLASS PAD");               /* 5 F1: CANCEL */
    btap(QD_B_OCTDN);
    run(700);
    shot("cancel", "F1 / HOME: CANCEL");
    run(1000);
    browse_to(QD_BR_ALL, 0, "METAL");                   /* 6 B10: + POOL */
    black(9);
    run(380);
    shot("add", "B10: + POOL");
    run(600);
    browse_to(QD_BR_POOL, 0, "SOFT KEYS");              /* 7 the POOL */
    A.model.t[3].sound = (uint8_t)pool_num("SOFT KEYS");
    run(400);
    shot("pool", "B9 -> POOL: the project's sounds");
    black(9);                                           /* 8 - POOL: armed */
    run(200);
    shot("out", "B10 in the POOL: take it out");
    btap(QD_B_HOME);
    run(1200);
    qd_ctl_set_mode(&A, QD_MODE_STEP);                  /* 9 a step held + PRESETS */
    key(WK(4), 1);
    run(300);
    turn(QD_K_PRESETS, -20);                            /* (step 5 locks SOFT KEYS: from TRACK) */
    run(200);
    turn(QD_K_PRESETS, pool_num("SNARE"));
    run(300);
    shot("step", "A step held + PRESETS: the pool");
    turn(QD_K_PRESETS, -20);                            /* 10 .. down to TRACK */
    run(300);
    shot("track", ".. down to TRACK: no lock");
    key(WK(4), 0);
    run(1200);
    qd_sounds_clear(&A.proj);                           /* 11 a new project: the pool empty */
    key(WK(4), 1);
    run(300);
    turn(QD_K_PRESETS, 1);
    run(300);
    shot("empty", "A new project: the pool empty");
    key(WK(4), 0);
    run(1200);
    btap(QD_B_HOME);
    {
        qd_track_t *t = &A.model.t[3];                  /* 12 SAVE: a PLUCK, brightened: BRIGHT PLUCK */
        qd_track_load_preset(t, fact("PLUCK"));
        t->edited = 1;
        run(20);
    }
    btap(QD_B_SAVE);
    run(300);
    shot("save", "SAVE: save the track's sound as a preset");
    turn(QD_K_ALGO, 7);                                 /* 13 naming */
    run(300);
    shot("name", ".. naming it yourself");
    btap(QD_B_OCTDN);
    run(200);
    {                                                   /* 14 from DARK BASS (one of MY presets), edited: REPLACE */
        qd_track_t *t = &A.model.t[3];
        const int slot = qd_lib_find(&A, LIB(&A, 1)->preset);
        memcpy(t->patch, LIB(&A, slot)->patch, sizeof t->patch);
        strcpy(t->sound_name, "DARK BASS");
        t->preset = LIB(&A, slot)->preset;
        t->patch[QD_QP_FREQ] = 6000;
        t->edited = 1;
        run(20);
    }
    btap(QD_B_SAVE);
    run(300);
    shot("replace", ".. or REPLACE one of MY presets");
    btap(QD_B_OCTDN);
    run(200);
    {
        qd_track_t *t = &A.model.t[3];                  /* 15 saved */
        qd_track_load_preset(t, fact("PLUCK"));
        t->edited = 1;
        run(20);
    }
    btap(QD_B_SAVE);
    run(100);
    btap(QD_B_SAVE);
    run(850);
    shot("saved", "Saved");
    run(1000);

    if (!failed)
        sheet();
    return failed;
}
