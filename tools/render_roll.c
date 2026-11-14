/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* render_frames' ROLL part (2026-11-13, the piano roll review): the PIANO ROLL in each of its eleven hold modes over one
 * page with varied values (conditions, chances, functions, trig types, sound locks, slides), playing (the playhead mid
 * page) with one step selected by the ALGORITHM cursor. Into OUT/roll-<mode>.png, OUT/roll-sheet.png, and the page's
 * values as OUT/roll-data.js (mockups/fizz-roll-grid.html draws its proposals from the same data) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ctl.h"
#include "plutovg.h"
#include "qd_gfx.h"
#include "fonts_gen.h"
#include "quad_voice.h"
#include "sounds.h"
#include "ui.h"

#define NSHOT UI_NFIELD
static qd_app_t A;
static uint32_t now;
static uint64_t frames_done;
static uint16_t lcd[QD_W * QD_H];
static const char *outdir;
static char files[NSHOT][32], caps[NSHOT][48];
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
static void btap(int b)
{
    qd_app_button(&A, b, 1);
    run(20);
    qd_app_button(&A, b, 0);
    run(20);
}

static void sheet(void)
{
    const float sc = 1.5f;
    const int cell = (int)(240.f * sc), cell_h = cell + 34, pad = 22, cols = 4;
    const int rows = (nshot + cols - 1) / cols, W = pad + cols * (cell + pad), H = 50 + rows * (cell_h + pad);
    plutovg_surface_t *s = plutovg_surface_create(W, H);
    plutovg_canvas_t *c = plutovg_canvas_create(s);
    plutovg_font_face_t *f = plutovg_font_face_load_from_data(qd_fonts_gen[QD_FONTS_GEN_500].data,
                                                              qd_fonts_gen[QD_FONTS_GEN_500].len, 0, NULL, NULL);
    char path[1024];
    int i;
    plutovg_canvas_set_rgb(c, 0.11f, 0.11f, 0.12f);
    plutovg_canvas_rect(c, 0, 0, (float)W, (float)H);
    plutovg_canvas_fill(c);
    plutovg_canvas_set_font(c, f, 18.f);
    plutovg_canvas_set_rgb(c, 0.9f, 0.9f, 0.9f);
    plutovg_canvas_fill_text(c, "FIZZ: the PIANO ROLL, every hold mode (now, 1.5x)", -1, PLUTOVG_TEXT_ENCODING_UTF8,
                             (float)pad, 32.f);
    for (i = 0; i < nshot; i++) {
        const float x = (float)(pad + (i % cols) * (cell + pad)), y = (float)(50 + (i / cols) * (cell_h + pad));
        plutovg_surface_t *o;
        plutovg_matrix_t m;
        snprintf(path, sizeof path, "%s/%s", outdir, files[i]);
        o = plutovg_surface_load_from_image_file(path);
        plutovg_canvas_set_font_size(c, 13.f);
        plutovg_canvas_set_rgb(c, 0.85f, 0.85f, 0.85f);
        plutovg_canvas_fill_text(c, caps[i], -1, PLUTOVG_TEXT_ENCODING_UTF8, x, y + 20.f);
        if (o) {
            plutovg_matrix_init_translate(&m, x, y + 30.f);
            plutovg_matrix_scale(&m, sc, sc);
            plutovg_canvas_set_texture(c, o, PLUTOVG_TEXTURE_TYPE_PLAIN, 1.f, &m);
            plutovg_canvas_rect(c, x, y + 30.f, (float)cell, (float)cell);
            plutovg_canvas_fill(c);
            plutovg_surface_destroy(o);
        }
    }
    snprintf(path, sizeof path, "%s/roll-sheet.png", outdir);
    if (!plutovg_surface_write_to_png(s, path))
        failed = 1;
    printf("%s (%d screens)\n", path, nshot);
    plutovg_font_face_destroy(f);
    plutovg_canvas_destroy(c);
    plutovg_surface_destroy(s);
}

/* the page's values for the mock-ups, as the lane reads them */
static void dump(const qd_pattern_t *p, int t, int sel)
{
    char path[1024], b[8];
    FILE *f;
    int s;
    snprintf(path, sizeof path, "%s/roll-data.js", outdir);
    f = fopen(path, "w");
    if (!f) {
        failed = 1;
        return;
    }
    fprintf(f, "/* written by tools/render_frames --only roll (render_roll.c): the page the real renders show */\n");
    fprintf(f, "window.ROLL = {track: %d, name: \"%s\", pattern: \"%s\", sel: %d, play: %d, steps: [\n", t + 1,
            A.model.t[t].name, qd_pat_name(A.seq.cur, b), sel, A.seq.playing ? qd_seq_track_step(&A.seq, t) % 16 : -1);
    for (s = 0; s < 16; s++) {
        qd_rstep_t r;
        const qd_step_t *st = qd_step(p, t, s);
        int tm, port;
        char cn[8];
        qd_step_resolve(p, t, s, &r);
        port = qd_step_slide(p, t, s, &tm, 0);
        qd_cond_name(r.cond, cn);
        fprintf(f,
                "  {on: %d, type: %d, cond: \"%s\", condOvr: %d, chance: %d, chanceOvr: %d, func: %d, sound: %d, "
                "soundName: \"%s\", soundTag: %d, port: %d, ptime: %d, vel: %d},\n",
                r.on, r.on ? r.type : 0, r.cond == QD_COND_ALWAYS ? "" : cn, qd_step_overrides(st, QD_PM_COND), r.chance,
                qd_step_overrides(st, QD_PM_CHANCE), r.func, r.sound, r.sound ? qd_sound_name(&A.proj, r.sound) : "",
                r.sound ? qd_tags_first(qd_sound_tags(&A.proj, r.sound)) : -1, port, tm >= 0 ? tm : A.model.t[t].patch[QD_QP_PTIM], r.vel);
    }
    fprintf(f, "], trackSound: \"%s\", trackSoundNo: %d, trackTag: %d};\n", qd_track_sound_label(&A.model.t[t]),
            A.model.t[t].sound, A.model.t[t].sound ? qd_tags_first(qd_sound_tags(&A.proj, A.model.t[t].sound)) : -1);
    fclose(f);
    printf("%s\n", path);
}

int render_roll(const char *out)
{
    static const uint8_t NOTE[16] = {48, 0, 55, 51, 60, 0, 58, 58, 48, 0, 63, 55, 53, 0, 51, 60};
    qd_pattern_t *p;
    int s, t, m;
    outdir = out;
    nshot = failed = 0;
    qd_app_init(&A);
    qd_app_seed(&A, 7);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    btap(QD_B_HOME);
    run(300);
    qd_app_show(&A, QD_MODE_STEP, QD_VIEW_ROLL);
    run(100);
    p = qd_ctl_pat(&A);
    t = A.sel;
    for (s = 0; s < 16; s++) {
        qd_step_clear(p, t, s);
        if (!NOTE[s])
            continue;
        qd_step_on(p, t, s);
        qd_step_set_note(p, t, s, 0, NOTE[s]);
        qd_step_set_vel(p, t, s, 50 + (s * 37) % 77);
    }
    /* TRIG: LOCKs and TIEs */
    qd_step_set_trig(p, t, 3, QD_TRIG_LOCK);
    qd_step_set_trig(p, t, 7, QD_TRIG_TIE);
    qd_step_set_trig(p, t, 11, QD_TRIG_LOCK);
    qd_step_set_trig(p, t, 15, QD_TRIG_TIE);
    /* LEN, MICRO, RATCH: a few */
    qd_step_set_len(p, t, 0, 6);
    qd_step_set_len(p, t, 8, 9);
    qd_step_set_micro(p, t, 6, 8);
    qd_step_set_micro(p, t, 12, -6);
    qd_step_set_ratchet(p, t, 4, 3, 1);
    qd_step_set_ratchet(p, t, 14, 4, 2);
    /* COND / CHANCE */
    qd_step_set_cond(p, t, 2, qd_cond_ab(1, 2));
    qd_step_set_cond(p, t, 4, QD_COND_FILL);
    qd_step_set_cond(p, t, 6, qd_cond_ab(3, 4));
    qd_step_set_cond(p, t, 8, QD_COND_N1ST);
    qd_step_set_cond(p, t, 10, QD_COND_PRE);
    qd_step_set_cond(p, t, 14, qd_cond_ab(2, 3));
    qd_step_set_chance(p, t, 0, 75);
    qd_step_set_chance(p, t, 10, 30);
    qd_step_set_chance(p, t, 12, 50);
    /* FUNC */
    qd_step_set_func(p, t, 4, QD_FN_REVERSE);
    qd_step_set_func(p, t, 6, QD_FN_RESTART);
    qd_step_set_func(p, t, 8, QD_FN_MAKE_JUMP(0));
    qd_step_set_func(p, t, 10, QD_FN_RJUMP);
    qd_step_set_func(p, t, 12, QD_FN_FLIP);
    qd_step_set_func(p, t, 15, QD_FN_MAKE_LEAP(2));
    /* SOUND: pool locks */
    qd_step_set_sound(p, t, 2, 2);
    qd_step_set_sound(p, t, 8, 5);
    qd_step_set_sound(p, t, 10, 12);
    qd_step_set_sound(p, t, 14, 2);
    /* SLIDE */
    qd_lock_set(p, t, 2, QD_QP_PORT, QD_PORT_ON);
    qd_lock_set(p, t, 2, QD_QP_PTIM, 40);
    qd_lock_set(p, t, 6, QD_QP_PORT, QD_PORT_LEGATO);
    qd_lock_set(p, t, 6, QD_QP_PTIM, 110);
    qd_lock_set(p, t, 10, QD_QP_PORT, QD_PORT_ON);
    qd_lock_set(p, t, 10, QD_QP_PTIM, 90);
    qd_lock_set(p, t, 12, QD_QP_PORT, QD_PORT_OFF);
    run(100);
    /* the cursor on step 11 (ALGORITHM), then PLAY until the playhead is mid page */
    for (s = 0; s < 11; s++) {
        qd_app_knob(&A, QD_K_ALGO, 1);
        run(30);
    }
    btap(QD_B_PLAY);
    for (s = 0; s < 20000 && !(A.seq.playing && qd_seq_track_step(&A.seq, t) == 6); s++)
        run(1);
    run(40);
    for (m = 0; m < UI_NFIELD; m++) {
        char path[1024];
        A.ui.hmode = m;                                 /* (the mode set directly: a black key would edit the cursor's
                                                         * step; the frame alone, the playhead where it is) */
        qd_app_frame(&A, lcd);
        snprintf(files[nshot], sizeof files[nshot], "roll-%s.png", UI_FIELD_NAME[m]);
        snprintf(caps[nshot], sizeof caps[nshot], "%d  %s", m + 1, UI_FIELD_NAME[m]);
        snprintf(path, sizeof path, "%s/%s", outdir, files[nshot]);
        if (!p13b_write_lcd_png(path, lcd))
            failed = 1;
        printf("%s\n", path);
        nshot++;
    }
    dump(p, t, 10);
    if (!failed)
        sheet();
    return failed;
}
