/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* bench_gfx: a frame's cost by screen at the device's sizes (QD_DEVICE: the screen drawn in QD_STRIP_H-row strips,
 * the whole view's code run once a strip, as the unit does), linked with one drawing (tools/gfx_fast.mk gfx-bench):
 * plutovg (qd_gfx_plutovg.c), the fast one (qd_gfx_fast.c), or none (qd_gfx_null.c: the screens' own logic alone).
 *
 *   bench_gfx NAME [REPEAT]      one line a scene: frames, mean / max us a frame (each frame the fastest of REPEAT
 *                                draws, default 3), on this machine
 *
 * The scenes: the idle screen (stopped, the operators dancing), the step view while A02 plays, the sound editor while
 * it plays, the MIDI FX screen (A05, four slots), the TAP ape (ten taps at 120 BPM, a frame every 15 ms), the library
 * browser. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "app.h"
#include "ctl.h"
#include "qd_gfx.h"
#include "mfx.h"
#include "pattern.h"
#include "ui.h"
#ifdef __APPLE__
#include <libproc.h>
#include <unistd.h>
#endif

/* the instructions this process has retired (macOS: the kernel's count; elsewhere 0): ChoralRoot's calibration of the
 * unit is "device us = host instructions / 258" (its docs/EDITOR.md, docs/DUALCORE.md) */
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri) == 0)
        return ri.ri_instructions;
#endif
    return 0;
}
static uint64_t last_instr;

static qd_app_t A;
static uint32_t now;
static uint64_t frames_done;
static uint16_t lcd[QD_W * QD_H];
static int repeat = 3;

static void run(uint32_t n)
{
    while (n--) {
        now++;
        qd_app_tick(&A, now);
        while (frames_done + 128u <= (uint64_t)now * QD_FS / 1000u) {
            int16_t blk[256];
            qd_app_audio(&A, blk, 128);
            frames_done += 128u;
        }
    }
}
static void fresh(void)
{
    qd_app_init(&A);
    A.ui.view = QD_VIEW_ROLL;
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    qd_app_frame(&A, lcd);
}
static void btap(int b)
{
    qd_app_button(&A, b, 1);
    run(20);
    qd_app_button(&A, b, 0);
    run(20);
}
static void turn(int k, int d)
{
    A.ui.knob_ms[k] = 0;
    qd_app_knob(&A, k, d);
    run(5);
}
static double us_since(const struct timespec *t0)
{
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (double)(t1.tv_sec - t0->tv_sec) * 1e6 + (double)(t1.tv_nsec - t0->tv_nsec) / 1e3;
}
static double frame_us(void)
{
    double best = 1e30;
    int r;
    last_instr = ~(uint64_t)0;
    for (r = 0; r < repeat; r++) {
        struct timespec t0;
        double us;
        const uint64_t i0 = instr_now();
        uint64_t di;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        qd_app_frame(&A, lcd);
        us = us_since(&t0);
        di = instr_now() - i0;
        best = us < best ? us : best;
        last_instr = di < last_instr ? di : last_instr;
    }
    return best;
}

static const char *bname;
static double isum, imax;
static void report(const char *scene, int n, double sum, double mx)
{
    printf("bench %-8s %-18s %4d frames  mean %7.1f us  max %7.1f us   instructions a frame: mean %6.2f M  max %6.2f M"
           "  (/258: %6.1f ms, max %6.1f ms)\n", bname, scene, n, sum / n, mx, isum / n / 1e6, imax / 1e6,
           isum / n / 258e3, imax / 258e3);
    isum = imax = 0;
}
static void count_instr(void)
{
    const double i = (double)last_instr;
    isum += i, imax = i > imax ? i : imax;
}
static void frames(const char *scene, int n, uint32_t every)
{
    double sum = 0, mx = 0;
    int i;
    for (i = 0; i < n; i++) {
        double us;
        run(every);
        us = frame_us();
        count_instr();
        sum += us;
        mx = us > mx ? us : mx;
    }
    report(scene, n, sum, mx);
}

/* the TAP ape (render_tapape.c's model: the takeover as view.c fills it from the taps' times) */
static float tp_hash(float n)
{
    const double s = sin((double)n * 127.1 + 311.7) * 43758.5453;
    return (float)(s - floor(s));
}
static void tap_model(ui_turn_t *v, const uint32_t *tap, int n, uint32_t t)
{
    int i;
    memset(v, 0, sizeof *v);
    v->kind = UI_TK_TAP;
    v->name = "TAP";
    v->ntap = n < 8 ? n : 8;
    for (i = 0; i < v->ntap; i++)
        v->tap_age[i] = (float)(t - tap[n - 1 - i]) / 1000.f;
    v->since = n ? v->tap_age[0] : 0.f;
    v->ntotal = n;
    for (i = 0; i < 2; i++) {
        const int m = n - 1 - i, j0 = m - 3 > 0 ? m - 3 : 0;
        const uint32_t sum = m > 0 ? tap[m] - tap[j0] : 0u;
        v->tap_bpm[i] = sum ? (int)(60000u * (uint32_t)(m - j0) / sum) : 0;
    }
    v->value = v->tap_bpm[0];
    v->t = (float)t / 1000.f;
}
static void tap_ape(void)
{
    uint32_t tap[10], t;
    double sum = 0, mx = 0;
    int i, n = 0;
    ui_turn_t v;
    for (i = 0; i < 10; i++)
        tap[i] = (uint32_t)lroundf(1000.f * ((float)i * 0.5f + (i ? (tp_hash((float)i) - 0.5f) * 0.012f : 0.f)));
    ui_ape_forget();
    for (t = 15; t < 7000u; t += 15u) {
        int nt = 0, r, y;
        double best = 1e30;
        last_instr = ~(uint64_t)0;
        while (nt < 10 && tap[nt] <= t)
            nt++;
        if (!nt)
            continue;
        tap_model(&v, tap, nt, t);
        for (r = 0; r < repeat; r++) {
            struct timespec t0;
            double us;
            const uint64_t i0 = instr_now();
            clock_gettime(CLOCK_MONOTONIC, &t0);
            for (y = 0; y < QD_H; y += qd_strip_rows()) {
                qd_begin_strip(y);
                ui_turn_draw(&v);
                qd_end_strip(lcd);
            }
            us = us_since(&t0);
            best = us < best ? us : best;
            last_instr = instr_now() - i0 < last_instr ? instr_now() - i0 : last_instr;
        }
        count_instr();
        sum += best, mx = best > mx ? best : mx, n++;
    }
    report("TAP ape", n, sum, mx);
}

static const char *only;
static int want(const char *scene) { return !only || strstr(scene, only) != 0; }

int main(int argc, char **argv)
{
    bname = argc > 1 ? argv[1] : "?";
    repeat = argc > 2 ? atoi(argv[2]) : 3;
    repeat = repeat < 1 ? 1 : repeat;
    only = argc > 3 ? argv[3] : 0;                      /* (one scene: its name's part, for a profiler) */
    if (!qd_init())
        return 1;

    if (want("idle screen")) {                          /* the idle screen: stopped, the dance */
        fresh();
        run(300);
        frames("idle screen", 60, 66);
    }
    if (want("step view playing")) {                    /* the step view while A02 plays */
        fresh();
        btap(QD_B_PLAY);
        run(500);
        frames("step view playing", 120, 15);
    }
    if (want("sound editor")) {                         /* the sound editor, playing */
        fresh();
        btap(QD_B_PLAY);
        btap(QD_B_SOUND);
        run(300);
        frames("sound editor", 80, 15);
    }
    if (want("MIDI FX")) {                              /* the MIDI FX screen (render_p16e.c's set-up) */
        qd_pattern_t *P;
        qd_mfx_group_t *G;
        int st;
        fresh();
        btap(QD_B_HOME);
        run(300);
        qd_seq_select(&A.seq, 4, 1);
        run(50);
        P = qd_proj_pat(&A.proj, A.seq.cur);
        for (st = 0; st < 16; st += 4) {
            uint8_t nn[1];
            nn[0] = (uint8_t)(48 + st);
            qd_step_on(P, 0, st), qd_step_set_notes(P, 0, st, nn, 1);
        }
        P->trk[0].mfx = 1;
        G = &A.proj.mfx[0];
        qd_mfx_slot_init(&G->slot[0], QD_MFX_ARP);
        qd_mfx_slot_init(&G->slot[1], QD_MFX_CHANCE);
        qd_mfx_slot_init(&G->slot[2], QD_MFX_HARM);
        qd_mfx_slot_init(&G->slot[3], QD_MFX_TRANSPOSE);
        btap(QD_B_HOME);
        btap(QD_B_KEYS);
        btap(QD_B_PLAY);
        run(300);
        frames("MIDI FX", 80, 15);
    }

    if (want("TAP ape"))
        tap_ape();
    if (want("browser")) {                              /* the library browser */
        double sum = 0, mx = 0;
        int i;
        fresh();
        if (A.ui.latch != QD_LATCH_BROWSE)
            turn(QD_K_PRESETS, 1);
        run(300);
        for (i = 0; i < 60; i++) {
            double us;
            if (i % 10 == 5)
                turn(QD_K_PRESETS, 1);              /* (a turn now and then: the list moves) */
            run(15);
            us = frame_us();
            count_instr();
            sum += us, mx = us > mx ? us : mx;
        }
        report("browser", 60, sum, mx);
    }
    qd_shutdown();
    return 0;
}
