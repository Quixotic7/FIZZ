/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' P1.6c part: the MIDI FX screen, mockups/fizz-midifx.html's 23 screens made by the gestures they show
 * (SHIFT + ARP, the slots, PRESETS' rolodex, each effect, the arp's pages and lanes, ALGORITHM's groups, a track OFF,
 * B10, B11 armed, F2 + B, ARP tapped, a held step's MIDI FX). Into OUT/p16c-*.png and OUT/p16c-sheet.png */
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
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "p16c-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = with_leds;
    qd_app_leds(&A, &shots[nshot].leds);
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_p16c: cannot write %s\n", path);
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
static void kdown(int k) { qd_app_key(&A, k, 1, 100); }
static void kup(int k) { qd_app_key(&A, k, 0, 0); }
static void turn(int knob, int d)
{
    A.ui.knob_ms[knob] = 0;
    qd_app_knob(&A, knob, d);
    run(5);
}
static void black(int b)
{
    kdown(BK(b));
    run(30);
    kup(BK(b));
    run(30);
}
static qd_mfx_slot_t *S(int g, int k, int type)
{
    qd_mfx_slot_init(&A.proj.mfx[g - 1].slot[k], type);
    return &A.proj.mfx[g - 1].slot[k];
}
static void route(int t, int g) { qd_proj_pat(&A.proj, A.seq.cur)->trk[t].mfx = (uint8_t)g; }
/* the example project (the mock-up's GR) */
static void groups(void)
{
    qd_mfx_slot_t *s;
    int k;
    memset(A.proj.mfx, 0, sizeof A.proj.mfx);
    for (k = 0; k < 8; k++)
        route(k, 0);
    s = S(1, 0, QD_MFX_ARP);
    s->v[QD_MA_RATE] = 9;
    S(1, 1, QD_MFX_CHANCE)->v[QD_MC_CHANCE] = 70;
    s = S(1, 2, QD_MFX_HARM);
    s->v[QD_MH_N2] = 4, s->v[QD_MH_N3] = 7, s->v[QD_MH_N4] = 12;
    S(2, 0, QD_MFX_SELECTOR);
    s = S(2, 1, QD_MFX_REPEAT);
    s->v[QD_MR_RATE] = 9, s->v[QD_MR_FADE] = -60;
    s = S(2, 2, QD_MFX_TRANSPOSE);
    s->v[QD_MT_SEMI] = 5, s->v[QD_MT_OCT] = -1;
    s = S(2, 3, QD_MFX_SCALER);
    s->v[QD_MZ_SCALE] = QD_SCALE_MINOR;
    S(2, 4, QD_MFX_MONO);
    s = S(3, 0, QD_MFX_CHORD);
    s->v[QD_MK_SHAPE] = 4, s->v[QD_MK_NOTES] = 4;
    s = S(3, 1, QD_MFX_RANDOM);
    s->v[QD_MD_PITCH] = 3, s->v[QD_MD_VEL] = 20, s->v[QD_MD_LEN] = 40, s->v[QD_MD_CHANCE] = 60;
    S(3, 2, QD_MFX_CHANCE);
    S(3, 3, QD_MFX_ARP);
    S(4, 0, QD_MFX_TRANSPOSE);
    S(4, 1, QD_MFX_HARM);
    route(0, 1), route(3, 1), route(1, 2), route(2, 3), route(6, 3), route(4, 4);
}
static void open_on(int t, int slot)
{
    qd_ctl_select_track(&A, t);
    run(20);
    if (!qd_mfx_open(&A))                               /* (P1.6d: ARP tapped opens it) */
        btap(QD_B_KEYS);
    if (slot >= 0 && A.ui.mfx_slot != slot)
        black(slot);
    A.ui.mfx_knob = -1;
    run(200);
}

int render_p16c(const char *out)
{
    static const int8_t MOD[16] = {QD_AL_PLAY, QD_AL_PLAY, QD_AL_REST, QD_AL_PLAY, QD_AL_REPEAT, QD_AL_TIE, QD_AL_LO,
                                   QD_AL_PLAY, QD_AL_CHORD, QD_AL_REST, QD_AL_HI, QD_AL_PWR, 0, 0, 0, 0};
    static const int8_t TR[16] = {0, 7, 0, 5, 0, 7, 12, 3};
    outdir = out;
    nshot = 0;
    failed = 0;
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    btap(QD_B_HOME);
    run(300);
    groups();
    /* 1: SHIFT + ARP on track 1: group 1, its ARP; W1, W3, W5 held (the keys through the group) */
    open_on(0, 0);
    kdown(WK(0)), kdown(WK(2)), kdown(WK(4));
    run(700);
    shot("arp", "ARP tapped: group 1 (tracks 1, 4), its ARP; W1 W3 W5 held", 1);
    kup(WK(0)), kup(WK(2)), kup(WK(4));
    run(300);
    /* 2: playing: the notes run along the rail (both tracks into one arp) */
    {
        uint8_t ch[2] = {60, 67}, n1 = 64;
        qd_pattern_t *P = qd_proj_pat(&A.proj, A.seq.cur);
        int st;
        for (st = 0; st < 16; st += 4) {
            qd_step_on(P, 0, st), qd_step_set_notes(P, 0, st, ch, 2), qd_step_set_len(P, 0, st, QD_LEN_1 + 4);
            qd_step_on(P, 3, st + 2), qd_step_set_notes(P, 3, st + 2, &n1, 1), qd_step_set_len(P, 3, st + 2, QD_LEN_1 + 3);
        }
    }
    btap(QD_B_PLAY);
    run(1400);
    shot("play", "Playing: the notes run along the rail", 1);
    btap(QD_B_PLAY);
    run(200);
    /* 3: an empty slot; 4: PRESETS, the rolodex */
    black(3);
    run(400);
    shot("empty", "B4: an empty slot", 1);
    turn(QD_K_PRESETS, 6);
    run(10);
    turn(QD_K_PRESETS, 2);
    run(230);
    shot("roll", "PRESETS: the effect, a rolodex", 1);
    qd_mfx_slot_init(&A.proj.mfx[0].slot[3], QD_MFX_NONE);
    run(1300);
    /* 5..13: each effect */
    black(1), run(300);
    shot("chance", "CHANCE", 1);
    open_on(1, 1);
    shot("repeat", "REPEAT (group 2: a SELECTOR's block)", 1);
    open_on(0, 2);
    run(500);
    shot("harm", "HARM", 1);
    open_on(1, 2);
    shot("transpose", "TRANSPOSE", 1);
    black(3), run(500);
    shot("scaler", "SCALER", 1);
    open_on(2, 1);
    shot("random", "RANDOM (group 3)", 1);
    black(0), run(300);
    shot("chord", "CHORD", 1);
    open_on(1, 4);
    run(300);
    shot("mono", "MONO", 1);
    black(0), run(300);
    shot("selector", "SELECTOR", 1);
    /* 14..16: the arp's pages */
    open_on(0, 0);
    black(0);
    turn(QD_K_1, 3);
    run(400);
    shot("arp2", "ARP page 2: MODE HOLD", 1);
    memcpy(A.proj.mfx[0].slot[0].lane[0], MOD, sizeof MOD);
    memcpy(A.proj.mfx[0].slot[0].lane[1], TR, sizeof TR);
    A.proj.mfx[0].slot[0].v[QD_MA_MLEN] = 12, A.proj.mfx[0].slot[0].v[QD_MA_TLEN] = 8;
    black(0);
    kdown(WK(4)), run(30), kup(WK(4));
    run(400);
    shot("modlane", "ARP page 3: the MOD lane (W5 its step)", 1);
    black(0);
    kdown(WK(1)), run(30), kup(WK(1));
    run(400);
    shot("trlane", "ARP page 4: the TRANSPOSE lane", 1);
    A.proj.mfx[0].slot[0].v[QD_MA_MODE] = QD_AM_ON;
    black(0);
    run(100);
    /* 17: ALGORITHM, the track's group; 18: a track OFF */
    turn(QD_K_ALGO, 1);
    run(10);
    turn(QD_K_ALGO, 1);
    run(330);
    shot("groups", "ALGORITHM: the track's group (1 to 3)", 1);
    turn(QD_K_ALGO, -1), turn(QD_K_ALGO, -1);
    run(1300);
    open_on(5, -1);
    run(300);
    shot("off", "A track routed OFF (track 6)", 1);
    /* 19: B10, bypass; 20: B11 armed; 21: F2 + B3, a move */
    open_on(0, 2);
    black(9);
    run(300);
    shot("bypass", "B10: the slot bypassed", 1);
    black(9);
    black(1);
    kdown(BK(10)), run(30), kup(BK(10));
    run(200);
    shot("arm", "B11: remove? (armed)", 1);
    black(1);
    bdown(QD_B_OCTUP);
    run(20);
    kdown(BK(1)), run(30), kup(BK(1));                  /* (P1.6d: F2 + B, Form's: B2 picked up, B4 dropped) */
    kdown(BK(3)), run(30), kup(BK(3));
    run(400);
    shot("move", "F2 + B2, B4: CHANCE carried to slot 4", 1);
    bup(QD_B_OCTUP);
    run(900);
    btap(QD_B_HOME);
    run(200);
    /* 22: ARP tapped (track 5, group 4: no arp yet) */
    qd_ctl_select_track(&A, 4);
    run(50);
    bdown(QD_B_KEYS);                                   /* (P1.6d: ARP held) */
    run(QD_ARP_HOLD_MS + 250);
    bup(QD_B_KEYS);
    shot("arpon", "ARP held, anywhere: ARP ON (added in slot 3)", 1);
    run(1400);
    /* 23: a held step in TRIG, KNOB 2: its MIDI FX */
    qd_ctl_select_track(&A, 0);
    btap(QD_B_HOME);
    run(100);
    black(7);                                           /* (TRIG) */
    kdown(WK(4));
    run(300);
    turn(QD_K_2, 4);                                    /* (TRACK -> OFF, 1, 2, 3) */
    run(200);
    shot("step", "A held step, TRIG: KNOB 2 = its MIDI FX (group 3)", 1);
    kup(WK(4));
    run(1500);
    btap(QD_B_HOME);
    run(300);
    if (!failed)
        sheet_as("p16c-sheet.png", "FIZZ P1.6c: the MIDI FX groups (1.5x, the keys under each)");
    return failed;
}
