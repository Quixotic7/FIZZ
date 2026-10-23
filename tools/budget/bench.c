/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P2.0 (tools/budget, from the Phase 2 audit's bench): the core built at the device's sizes (QD_DEVICE) for this host,
 * driven as the glue drives it (a tick a ms, 128-frame audio blocks as they fall due, a frame every 15 ms) through the
 * screens that cost the most heap: the idle screen, demo A01 playing in the step view, a held step and a p-lock,
 * knob takeovers, every button's screen held and tapped, the sound editor's pages, the TAP ape, a project written to
 * a RAM flash; and the sequencer's queues' high water over the three demos. The heap is counted by call site
 * (heapwrap.h, force-included into the core; P2.1: plutovg's in the drawing's arena, its peak reported). Prints "BUDGET <name> <value>" lines for budget.py. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "persist.h"
#include "qd_arena.h"
#include "seq.h"

void hw_report(FILE *o, const char *title);
void hw_sites(FILE *o, const char *prefix);
extern long hw_live, hw_peak;

static qd_app_t A;
static uint16_t lcd[240 * 240];
static int16_t abuf[256];
static uint32_t now, audio_frames, last_frame;
static int ev_peak, rec_peak;

static void peaks(void)
{
    int r, u = 0;
    for (r = 0; r < QD_SEQ_NREC; r++)
        u += A.seq.rec[r].used != 0;
    ev_peak = A.seq.nev > ev_peak ? A.seq.nev : ev_peak;
    rec_peak = u > rec_peak ? u : rec_peak;
}
static void step_ms(uint32_t ms, int frames)
{
    const uint32_t end = now + ms;
    while (now < end) {
        now++;
        qd_app_tick(&A, now);
        while ((uint64_t)audio_frames * 1000u < (uint64_t)now * 44100u) {
            qd_app_audio(&A, abuf, 128);
            audio_frames += 128;
            peaks();
        }
        if (frames && now - last_frame >= 15u) {
            last_frame = now;
            qd_app_frame(&A, lcd);
        }
    }
}
static void btn(int b, int down) { qd_app_button(&A, b, down); }
static void tapb(int b) { btn(b, 1), step_ms(30, 1), btn(b, 0), step_ms(30, 1); }
static void key(int k, int down) { qd_app_key(&A, k, down, down ? 100 : 0); }

static uint8_t nor[1u << 20];
static int fl_read(void *c, uint32_t off, void *dst, uint32_t n) { (void)c; memcpy(dst, nor + off, n); return 0; }
static int fl_erase(void *c, uint32_t off) { (void)c; memset(nor + (off & ~4095u), 0xFF, 4096); return 0; }
static int fl_prog(void *c, uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    (void)c;
    for (i = 0; i < n; i++)
        nor[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}

int main(void)
{
    static const uint8_t W[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
    static const uint8_t BK[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
    static const qd_flash_t fl = {fl_read, fl_erase, fl_prog, 0};
    int i, d;
    memset(nor, 0xFF, sizeof nor);
    qd_app_init(&A);
    qd_persist_attach(&A, &fl);
    step_ms(2000, 1);                                   /* the idle screen */
    btn(QD_B_MIX, 1), key(W[0], 1), key(W[0], 0), btn(QD_B_MIX, 0);   /* (SEQ + W1: A01) */
    step_ms(100, 1);
    tapb(QD_B_PLAY);
    step_ms(3000, 1);                                   /* A01 playing, the step view */
    key(W[4], 1);
    step_ms(800, 1);
    for (i = 0; i < 40; i++)                            /* a held step, KNOB 1: a p-lock */
        qd_app_knob(&A, QD_K_1, 1), step_ms(30, 1);
    key(W[4], 0);
    step_ms(300, 1);
    for (i = 0; i < 60; i++)                            /* the knobs' takeovers */
        qd_app_knob(&A, QD_K_2 + i / 20, i & 1 ? 1 : -1), step_ms(30, 1);
    {
        static const int B[] = {QD_B_TRACKS, QD_B_PATTERN, QD_B_PAGE, QD_B_MUTE, QD_B_SOUND, QD_B_SAVE, QD_B_KEYS,
                                QD_B_MIX};
        for (i = 0; i < (int)(sizeof B / sizeof B[0]); i++) {   /* every button's screen, held, then tapped */
            btn(B[i], 1), step_ms(1200, 1), btn(B[i], 0), step_ms(200, 1);
            tapb(B[i]), step_ms(1000, 1);
            tapb(QD_B_HOME), step_ms(300, 1);
        }
    }
    for (i = 0; i < 8; i++) {                           /* the sound editor's pages */
        tapb(QD_B_SOUND);
        key(BK[i], 1), step_ms(40, 1), key(BK[i], 0);
        qd_app_knob(&A, QD_K_1, 3);
        step_ms(700, 1);
    }
    tapb(QD_B_HOME);
    btn(QD_B_FUNC, 1);                                  /* SHIFT + B6: the TAP ape, ten taps and its finale */
    step_ms(300, 1);
    for (i = 0; i < 10; i++)
        key(13, 1), step_ms(40, 1), key(13, 0), step_ms(460, 1);
    btn(QD_B_FUNC, 0);
    step_ms(2500, 1);
    tapb(QD_B_PLAY);
    qd_persist_flush(&A);                               /* the project written */
    step_ms(500, 1);
    for (d = 0; d < 3; d++) {                           /* the queues over the three demos, 20 s each */
        btn(QD_B_MIX, 1), key(W[d], 1), key(W[d], 0), btn(QD_B_MIX, 0);
        step_ms(100, 0);
        tapb(QD_B_PLAY);
        step_ms(20000, 0);
        tapb(QD_B_PLAY);
    }
    printf("BUDGET profile %s\n", QD_PROFILE_NAME);
    printf("BUDGET sizeof_app_host %lu\n", (unsigned long)sizeof(qd_app_t));
    printf("BUDGET heap_peak %ld\n", hw_peak);
    printf("BUDGET heap_live %ld\n", hw_live);
    printf("BUDGET arena %lu %lu %u\n", (unsigned long)qd_arena_peak(), (unsigned long)qd_arena_size(), qd_arena_fails());
    printf("BUDGET seq_ev_peak %d %d\n", ev_peak, QD_SEQ_NEV);
    printf("BUDGET seq_rec_peak %d %d\n", rec_peak, QD_SEQ_NREC);
    printf("BUDGET seq_dropped %u %u\n", (unsigned)A.seq.dropped_ev, (unsigned)A.seq.dropped_rec);
    hw_sites(stdout, "BUDGET heap_site");
    return 0;
}
