/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.6g part: the ARP picture's seventeen PATTERNs (on C E G over two octaves: the real walk), the arp
 * playing a held chord, its lanes' rests and transposes, a PATTERN turned (the morph); the FILTER page's ALGORITHM
 * popup; recording in the sound editor (its cue). Into OUT/p16g-*.png, OUT/p16g-arp-sheet.png and OUT/p16g-sheet.png */
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p16g-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p16g: cannot write %s\n", path);
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
static void kdown(int k) { qd_app_key(&A, k, 1, 100); }
static void kup(int k) { qd_app_key(&A, k, 0, 0); }
static void turn(int knob, int d)
{
    A.ui.knob_ms[knob] = 0;
    qd_app_knob(&A, knob, d);
    run(5);
}
static void home(void)
{
    btap(QD_B_HOME);
    run(1300);
}

int render_p16g(const char *out)
{
    static const char *const PAT[QD_NAP] = {"UP", "DOWN", "UP DN", "DN UP", "U+D", "D+U", "CONV", "DIV", "CON DIV",
                                            "HI UP", "HI UPDN", "LO UP", "LO UPDN", "RAND", "RAND OTH", "SHUFFLE",
                                            "AS PLAYED"};
    qd_mfx_group_t *G;
    qd_mfx_slot_t *s;
    int k;
    char cap[64];
    outdir = out;
    nshot = 0;
    failed = 0;
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    btap(QD_B_HOME);
    run(300);
    qd_seq_select(&A.seq, 4, 1);
    run(50);
    qd_proj_pat(&A.proj, A.seq.cur)->trk[0].mfx = 1;
    G = &A.proj.mfx[0];
    s = &G->slot[0];
    qd_mfx_slot_init(s, QD_MFX_ARP);
    s->v[QD_MA_RANGE] = 2;
    home();
    btap(QD_B_KEYS);                                    /* the MIDI FX screen, the ARP */
    run(300);
    for (k = 0; k < QD_NAP; k++) {                      /* the seventeen PATTERNs on C E G */
        s->v[QD_MA_PAT] = (int16_t)k;
        run(400);
        snprintf(cap, sizeof cap, "%d %s (C E G, RANGE 2)", k + 1, PAT[k]);
        shot(PAT[k], cap, 0);
    }
    if (!failed)
        sheet_as("p16g-arp-sheet.png", "FIZZ P1.6g: the ARP's seventeen PATTERNs, their real walks (1.5x)");
    nshot = 0;
    s->v[QD_MA_PAT] = QD_AP_UPDN;
    btap(QD_B_REC);                                     /* (KEYS: the keys through the group) */
    kdown(WK(0)), kdown(WK(2)), kdown(WK(4)), kdown(WK(7));
    run(900);
    shot("arp-held", "A chord held: UP DN on it, its keys lit on the strip, the dot running", 1);
    turn(QD_K_1, 4);                                    /* (PATTERN: CONV) */
    run(60);
    shot("arp-morph", "KNOB 1: PATTERN changed, the walk morphing (180 ms)", 1);
    run(400);
    kup(WK(0)), kup(WK(2)), kup(WK(4)), kup(WK(7));
    run(300);
    s->v[QD_MA_MLEN] = 8, s->v[QD_MA_TLEN] = 4;
    {
        static const int8_t MOD[8] = {QD_AL_PLAY, QD_AL_PLAY, QD_AL_REST, QD_AL_PLAY, QD_AL_PLAY, QD_AL_REST, QD_AL_PLAY,
                                      QD_AL_PLAY};
        static const int8_t TR[4] = {0, 0, 7, 12};
        memcpy(s->lane[0], MOD, sizeof MOD);
        memcpy(s->lane[1], TR, sizeof TR);
    }
    s->v[QD_MA_PAT] = QD_AP_UP;
    run(400);
    shot("arp-lanes", "The lanes in it: the MOD lane's rests as gaps, the TRANSPOSE lane's +7, +12", 1);
    s->v[QD_MA_MLEN] = s->v[QD_MA_TLEN] = 0;
    btap(QD_B_REC);
    btap(QD_B_HOME);
    run(300);
    /* the FILTER page: ALGORITHM shows the machines */
    btap(QD_B_SOUND);
    A.ui.snd_page = qd_snd_page_of("FILTER");
    run(200);
    turn(QD_K_ALGO, 1);
    run(200);
    shot("filter-algo", "FILTER, ALGORITHM turned: the machines' popup, a second", 1);
    run(1200);
    shot("filter-after", ".. a second on: gone", 1);
    /* recording in the editor */
    btap(QD_B_REC);
    bdown(QD_B_REC);
    run(1100);
    bup(QD_B_REC);
    run(1300);
    shot("rec-armed", "The editor: REC held: armed (REC blinking), the editor stays", 1);
    btap(QD_B_PLAY);
    run(300);
    kdown(WK(2));
    run(120);
    kup(WK(2));
    run(100);
    shot("rec-on", "PLAY: recording (REC lit), the red frame; its keys recorded", 1);
    btap(QD_B_PLAY);
    btap(QD_B_HOME);
    run(300);
    if (!failed)
        sheet_as("p16g-sheet.png", "FIZZ P1.6g (1.5x, the keys under each)");
    return failed;
}
