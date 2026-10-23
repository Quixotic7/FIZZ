/* SPDX-License-Identifier: GPL-3.0-only */
/* render_frames' TAP ape part (0.9.2): mockups/fizz-tap-ape.html's fifteen moments drawn by FIZZ's own renderer
 * (ui_ape_draw, the takeover's model filled as view.c fills it from the taps' times), into OUT/tap-ape-*.png and
 * OUT/tap-ape-sheet.png; and the drawing's cost a frame (the busiest moments, on this machine). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "app.h"
#include "ctl.h"
#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"
#include "qd_fx.h"
#include "scale.h"
#include "sounds.h"
#include "ui.h"

#define NSHOT 16
static uint16_t lcd[QD_W * QD_H];
static const char *outdir;
static struct {
    char file[64], caption[112];
    int has_leds;
    qd_leds_t leds;
} shots[NSHOT];
static int nshot, failed;

int p13b_write_lcd_png(const char *path, const uint16_t *lcd_be);   /* render_frames.c's */

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
    const float sc = 2.f;
    const int cell = (int)(240.f * sc), leds_h = 0, cell_h = cell + 34 + leds_h, pad = 22, cols = 4;
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

static void shot_lcd(const char *name, const char *caption)
{
    char path[1024];
    snprintf(shots[nshot].file, sizeof shots[nshot].file, "tap-ape-%s.png", name);
    snprintf(shots[nshot].caption, sizeof shots[nshot].caption, "%s", caption);
    shots[nshot].has_leds = 0;
    snprintf(path, sizeof path, "%s/%s", outdir, shots[nshot].file);
    if (!p13b_write_lcd_png(path, lcd)) {
        fprintf(stderr, "render_tapape: cannot write %s\n", path);
        failed = 1;
    }
    printf("%s (%s)\n", path, caption);
    if (nshot < NSHOT - 1)
        nshot++;
}

/* the mock-up's taps: n at b s apart, a little human jitter (its hash), in ms */
static float tp_hash(float n)
{
    const double s = sin((double)n * 127.1 + 311.7) * 43758.5453;
    return (float)(s - floor(s));
}
static int taps_run(uint32_t *ms, int n, float b, float j)
{
    int i;
    for (i = 0; i < n; i++)
        ms[i] = (uint32_t)lroundf(1000.f * ((float)i * b + (i ? (tp_hash((float)i) - 0.5f) * j : 0.f)));
    return n;
}
/* the takeover's model at now (ms) after the taps, as view.c fills it (the firmware's BPM: the last four taps) */
static void model(ui_turn_t *v, const uint32_t *tap, int n, uint32_t now, int ext, int bpm_ext)
{
    int i;
    memset(v, 0, sizeof *v);
    v->kind = UI_TK_TAP;
    v->name = "TAP";
    v->ntap = n < 8 ? n : 8;
    for (i = 0; i < v->ntap; i++)
        v->tap_age[i] = (float)(now - tap[n - 1 - i]) / 1000.f;
    v->since = n ? v->tap_age[0] : 0.f;
    v->ntotal = n;
    for (i = 0; i < 2; i++) {
        const int m = n - 1 - i, j0 = m - 3 > 0 ? m - 3 : 0;
        const uint32_t sum = m > 0 ? tap[m] - tap[j0] : 0u;
        v->tap_bpm[i] = sum ? (int)(60000u * (uint32_t)(m - j0) / sum) : 0;
    }
    v->value = ext ? bpm_ext : v->tap_bpm[0];
    v->ext = ext;
    v->t = (float)now / 1000.f;
}
static void draw(const ui_turn_t *v)
{
    qd_begin();
    ui_turn_draw(v);
    qd_end(lcd);
}

int render_tapape(const char *out)
{
    static const struct { const char *name, *cap; int n; float b, t; int ext; } S[16] = {
        {"enter", "The first tap: the ape bounces in", 1, 0.5f, 0.12f, 0},
        {"point", "Point-and-squat: one tap, no tempo yet", 1, 0.5f, 0.7f, 0},
        {"tap2", "Tap 2: the jump to the step", 2, 0.5f, 0.5f + 0.05f, 0},
        {"groove", "Between taps: the groove (side lean)", 3, 0.5f, 1.f + 0.26f, 0},
        {"tap4", "Tap 4: knuckle crouch", 4, 0.5f, 1.5f + 0.3f, 0},
        {"tap5", "Tap 5: side kick", 5, 0.5f, 2.f + 0.3f, 0},
        {"tap6", "Tap 6: handstand", 6, 0.5f, 2.5f + 0.3f, 0},
        {"tap7", "Tap 7: one-arm air freeze", 7, 0.5f, 3.f + 0.3f, 0},
        {"tap8", "Tap 8: spin", 8, 0.5f, 3.5f + 0.3f, 0},
        {"tap9", "Tap 9: lying freeze, on the boombox", 9, 0.5f, 4.f + 0.3f, 0},
        {"tap10", "Tap 10: headstand, turning", 10, 0.5f, 4.5f + 0.36f, 0},
        {"tap11", "Tap 11: back freeze, feet up", 11, 0.5f, 5.f + 0.3f, 0},
        {"finale", "2 s on: the finale, kneel, fist up", 8, 0.5f, 3.5f + 2.f + 0.2f, 0},
        {"ext", "An external clock: lounging on the boombox", 0, 0.5f, 0.6f, 1},
        {"flurry", "Easter egg: past 180 BPM, the flurry", 5, 0.3f, 4.f * 0.3f + 0.3f, 0},
        {"moonwalk", "P1.6g: 70 BPM and under: the slow-motion moonwalk, its trails", 5, 1.0f, 4.f + 0.85f, 0},
    };
    static uint32_t tap[16];
    ui_turn_t v;
    int i;
    outdir = out;
    nshot = 0;
    failed = 0;
    for (i = 0; i < 16; i++) {
        const int n = taps_run(tap, S[i].n, S[i].b, 0.012f);
        model(&v, tap, n, (uint32_t)lroundf(S[i].t * 1000.f), S[i].ext, 124);
        draw(&v);
        shot_lcd(S[i].name, S[i].cap);
    }
    {   /* the cost a frame, as the unit and the site draw it: a frame every 15 ms through ten taps at 120 BPM, the
         * finale and the close; then six at 200 (the flurry); then the external clock. Each frame timed, sorted by what
         * it shows: the first frame after a tap, the ghost / flash frames (the tap's first 160 ms), the steady grooving
         * ones, the finale's, the flurry's, the external clock's */
        enum { F_TAP, F_GHOST, F_STEADY, F_TADA, F_FLURRY, F_EXT, F_WARM, F_N };
        static const char *const FN[F_N] = {"tap (the first frame)", "ghost / flash (160 ms)", "steady (grooving)",
                                             "finale", "flurry", "external clock", "SHIFT held (prewarm)"};
        double sum[F_N] = {0}, mx[F_N] = {0};
        int cnt[F_N] = {0}, pass;
        {   /* cold, no SHIFT before it (no prewarm): the first tap's frame makes its pose at once */
            struct timespec t0, t1;
            ui_ape_forget();
            taps_run(tap, 1, 0.5f, 0.f);
            model(&v, tap, 1, 15u, 0, 124);
            clock_gettime(CLOCK_MONOTONIC, &t0);
            draw(&v);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            printf("render_tapape: %-24s %5.2f ms (once; this machine)\n", "cold first tap",
                   (double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6);
            ui_ape_forget();
        }
        for (pass = 0; pass < 3; pass++) {
            const int nt = pass == 0 ? 10 : pass == 1 ? 6 : 0;
            const float b = pass == 1 ? 0.3f : 0.5f;
            const uint32_t end = pass == 2 ? 2000u : (uint32_t)(1000.f * ((float)(nt - 1) * b + 2.9f));
            uint32_t now2;
            int prev_n = 0;
            taps_run(tap, nt, b, 0.012f);
            for (now2 = 0; now2 < 300u; now2 += 15u) {     /* (SHIFT held before B6: the view prewarms) */
                struct timespec t0, t1;
                double ms;
                clock_gettime(CLOCK_MONOTONIC, &t0);
                ui_ape_prewarm(pass == 2);
                clock_gettime(CLOCK_MONOTONIC, &t1);
                ms = (double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;
                sum[F_WARM] += ms, cnt[F_WARM]++;
                mx[F_WARM] = ms > mx[F_WARM] ? ms : mx[F_WARM];
            }
            for (now2 = 0; now2 < end; now2 += 15u) {
                int n = 0, f;
                struct timespec t0, t1;
                double ms;
                while (n < nt && tap[n] <= now2)
                    n++;
                if (pass < 2 && !n)
                    continue;
                model(&v, tap, n, now2, pass == 2, 124);
                clock_gettime(CLOCK_MONOTONIC, &t0);
                draw(&v);
                clock_gettime(CLOCK_MONOTONIC, &t1);
                ms = (double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;
                f = pass == 2 ? F_EXT : n != prev_n ? F_TAP : pass == 1 && n >= 2 && v.since < 2.f ? F_FLURRY
                  : v.since >= 2.f ? F_TADA : v.since < 0.16f ? F_GHOST : F_STEADY;
                if (ms > 1.2 && getenv("TAPAPE_DEBUG"))
                    printf("  (pass %d, %u ms, tap %d, since %.3f: %.2f ms)\n", pass, now2, n, v.since, ms);
                prev_n = n;
                sum[f] += ms, cnt[f]++;
                mx[f] = ms > mx[f] ? ms : mx[f];
            }
        }
        for (i = 0; i < F_N; i++)
            printf("render_tapape: %-24s %5.2f ms a frame (worst %5.2f, %d frames; this machine)\n", FN[i],
                   cnt[i] ? sum[i] / cnt[i] : 0.0, mx[i], cnt[i]);
    }
    {   /* for scale: the EDIT view (the VEL rider, neon too), the same way */
        static qd_app_t A;
        uint32_t now = 0;
        clock_t c0;
        int f;
        qd_app_init(&A);
        qd_app_tick(&A, 0);
        qd_app_button(&A, QD_B_HOME, 1), qd_app_tick(&A, ++now), qd_app_button(&A, QD_B_HOME, 0);
        for (f = 0; f < 300; f++)
            qd_app_tick(&A, ++now);
        qd_app_key(&A, QD_BLACK_KEY[1], 1, 100), qd_app_tick(&A, ++now), qd_app_key(&A, QD_BLACK_KEY[1], 0, 0);
        for (f = 0; f < 300; f++)
            qd_app_tick(&A, ++now);
        c0 = clock();
        for (f = 0; f < 200; f++) {
            qd_app_tick(&A, ++now);
            qd_app_frame(&A, lcd);
        }
        printf("render_tapape: for scale, the EDIT view (VEL) %.2f ms a frame\n",
               (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC / 200.0);
    }
    if (!failed)
        sheet_as("tap-ape-sheet.png", "FIZZ 0.9.2: the TAP ape, SHIFT + B6 (2x, FIZZ's renderer)");
    return failed;
}
