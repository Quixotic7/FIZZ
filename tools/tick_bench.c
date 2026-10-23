/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P2.4: the core's per-millisecond tick and its frames, timed on the host at the device's sizes (make PROFILE=-DQD_DEVICE
 * B=build/dev build/dev/tick_bench): the idle screen, the step view stopped and playing; each: qd_app_tick's cost a
 * millisecond (60 s of ticks, the sound rendered between: the transport moves), a frame's (qd_app_frame_strips, the strips' callback empty), and how many of the frames
 * qd_app_frame_changed says to draw. On a RAM flash (the store on, as the unit's). Times are the host's: the unit's
 * are some 20 to 40 times them (research/PHASE2-BUDGET.md §3.1). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app.h"

#define FSIZE 0x100000u
static uint8_t FL[FSIZE];
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

static qd_app_t A;
static uint32_t now;

static double secs(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}
static void strip_out(void *ctx, int y0, int rows, const uint16_t *px) { (void)ctx, (void)y0, (void)rows, (void)px; }
/* a millisecond of the device: its sound (44.1 frames, the audio's own: not timed) and the core's tick (timed) */
static double t_ticks;
static void ms_step(void)
{
    static int16_t buf[2 * 64];
    static uint32_t frac;
    double t0;
    frac += 441u;                                       /* (44.1 frames a ms) */
    qd_app_audio(&A, buf, frac / 10u);
    frac %= 10u;
    t0 = secs();
    qd_app_tick(&A, ++now);
    t_ticks += secs() - t0;
}

static void bench(const char *name)
{
    const uint32_t ms = 60000u;
    uint32_t i, frames = 0, changed = 0;
    double t0, t_tick, t_frame = 0.0;
    t_ticks = 0.0;
    for (i = 0; i < ms; i++)
        ms_step();
    t_tick = t_ticks;
    for (i = 0; i < 300u; i++) {                        /* 300 frames, 33 ms apart (the ticks between not timed) */
        uint32_t k;
        for (k = 0; k < 33u; k++)
            ms_step();
        if (qd_app_frame_changed(&A))
            changed++;
        t0 = secs();
        qd_app_frame_strips(&A, 0, strip_out, 0);
        t_frame += secs() - t0;
        frames++;
    }
    printf("tick_bench %-22s a ms's tick %6.2f us   a frame %7.1f us   frames that changed %u of %u (mode %d view %d; "
           "the same frame asked again: %s)\n", name, t_tick / ms * 1e6, t_frame / frames * 1e6, changed, frames,
           A.ui.mode, A.ui.view, qd_app_frame_changed(&A) ? "changed (wrong)" : "unchanged");
}

int main(void)
{
    memset(FL, 0xFF, sizeof FL);
    qd_app_init(&A);
    qd_app_tick(&A, 0);
    qd_persist_attach(&A, &FLASH);
    qd_app_seed(&A, 1u);
    while (now < 2000u)                                 /* (settled: the first writes done) */
        ms_step();
    qd_app_show(&A, QD_MODE_IDLE, -1);
    bench("idle");
    qd_app_show(&A, QD_MODE_STEP, QD_VIEW_EDIT);
    bench("step view, stopped");
    qd_app_play(&A);
    bench("step view, playing");
    qd_app_show(&A, QD_MODE_IDLE, -1);
    bench("idle, playing");
    qd_app_stop(&A);
    return 0;
}
