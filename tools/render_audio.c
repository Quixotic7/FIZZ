/* SPDX-License-Identifier: GPL-3.0-only */
/* render_audio: QUADRILLE's sound headless, for listening and for the log. Runs the app (core/app) on the core's clock
 * (a tick a ms, the audio in 128-frame blocks as they fall due, as the glue and FM1VST drive it) with notes played as
 * MIDI on channels 1..8 (tracks 1..8), and writes 16-bit stereo WAVs at 44.1 kHz:
 *
 *   presets.wav       every sound in turn on track 3 (KEYS): INIT, then the 16 factory sounds, each a 1 s chord (C4 E4
 *                     G4 C5) and 0.5 s of its release, with the track's default sends (reverb 20)
 *   tracks.wav        the 8 default tracks, each playing a short phrase (two bars at 122 BPM) alone, then all 8 together
 *                     (two bars), then the tails
 *   pool-stress.wav   32 notes at once across the 8 tracks (4 each) on the 16-voice pool, held 2 s, released
 *
 * and audio.txt: the pool's counters for each, the peaks, and the host's CPU for a 128-frame block (2902 us of
 * audio): 16 voices with the effects on (8 tracks x 2 notes, every send at 40), the same with WOBBLE (ChoralRoot's
 * heaviest), the effects alone (noise into all three sends and the mix: everything running), silence. The CPU is the
 * host's (Apple silicon here), not the unit's. The peaks: MASTER at its default (pot 724: 2047 of 4096) puts Output
 * Clean's ceiling at 26000 x 2047 / 4096 = 12994.
 *
 *     render_audio OUT_DIR */
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "qd_fx.h"
#include "qd_gfx.h"

static qd_app_t app;
static uint32_t now, frames_done;
static int16_t *buf;
static uint32_t nbuf, cap;
static int32_t peak;

static void fresh(void)
{
    qd_app_init(&app);
    now = frames_done = 0;
    qd_app_tick(&app, 0);
    nbuf = 0;
    peak = 0;
}
/* n ms of the core's clock, the audio appended to buf */
static void run(uint32_t n)
{
    while (n--) {
        now++;
        qd_app_tick(&app, now);
        while (frames_done + 128u <= (uint64_t)now * QD_FS / 1000u) {
            int k;
            if (nbuf + 128u > cap) {
                cap = cap ? cap * 2u : 1u << 20;
                buf = realloc(buf, (size_t)cap * 2u * sizeof *buf);
                if (!buf) {
                    fprintf(stderr, "render_audio: out of memory\n");
                    exit(1);
                }
            }
            qd_app_audio(&app, buf + 2u * nbuf, 128);
            for (k = 0; k < 256; k++)
                peak = abs(buf[2u * nbuf + (uint32_t)k]) > peak ? abs(buf[2u * nbuf + (uint32_t)k]) : peak;
            nbuf += 128u;
            frames_done += 128u;
        }
    }
}
static void midi(int status, int d1, int d2) { qd_app_midi(&app, status, d1, d2); }

static void put32(FILE *f, uint32_t v) { fputc((int)(v & 255u), f), fputc((int)(v >> 8 & 255u), f), fputc((int)(v >> 16 & 255u), f), fputc((int)(v >> 24), f); }
static void put16(FILE *f, uint32_t v) { fputc((int)(v & 255u), f), fputc((int)(v >> 8 & 255u), f); }
static int wav(const char *dir, const char *name)
{
    char path[1024];
    FILE *f;
    uint32_t i;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "wb");
    if (!f)
        return 0;
    fwrite("RIFF", 1, 4, f), put32(f, 36u + nbuf * 4u), fwrite("WAVEfmt ", 1, 8, f), put32(f, 16), put16(f, 1),
        put16(f, 2), put32(f, QD_FS), put32(f, QD_FS * 4u), put16(f, 4), put16(f, 16), fwrite("data", 1, 4, f),
        put32(f, nbuf * 4u);
    for (i = 0; i < 2u * nbuf; i++)
        put16(f, (uint16_t)buf[i]);
    fclose(f);
    printf("%s (%.1f s, peak %d)\n", path, nbuf / (double)QD_FS, (int)peak);
    return 1;
}

/* ---------------------------------------------------------------- the phrases --- */
/* a note of a phrase: its 16th, its note, its length in 16ths, its velocity */
typedef struct { uint8_t at, note, len, vel; } ev_t;
#define STEP_MS (60000.0 / QD_BPM_DEFAULT / 4.0)
static const ev_t PH_DRUMS[] = {{0, 38, 1, 120}, {4, 74, 1, 90}, {6, 38, 1, 100}, {8, 38, 1, 120}, {10, 86, 1, 70},
    {12, 74, 1, 100}, {14, 86, 1, 70}, {15, 86, 1, 60}, {16, 38, 1, 120}, {20, 74, 1, 90}, {22, 38, 1, 100},
    {24, 38, 1, 120}, {26, 86, 1, 70}, {28, 74, 1, 110}, {30, 86, 1, 80}, {31, 74, 1, 70}, {255, 0, 0, 0}};
static const ev_t PH_BASS[] = {{0, 36, 2, 110}, {3, 36, 1, 90}, {4, 43, 2, 100}, {6, 46, 1, 90}, {8, 36, 2, 110},
    {11, 48, 1, 90}, {12, 46, 2, 100}, {14, 43, 2, 90}, {16, 41, 2, 110}, {19, 41, 1, 90}, {20, 48, 2, 100},
    {22, 51, 1, 90}, {24, 41, 2, 110}, {27, 53, 1, 90}, {28, 51, 2, 100}, {30, 48, 2, 90}, {255, 0, 0, 0}};
static const ev_t PH_KEYS[] = {{0, 60, 7, 90}, {0, 64, 7, 80}, {0, 67, 7, 80}, {0, 71, 7, 75}, {8, 57, 7, 90},
    {8, 60, 7, 80}, {8, 64, 7, 80}, {8, 67, 7, 75}, {16, 53, 7, 90}, {16, 57, 7, 80}, {16, 60, 7, 80}, {16, 64, 7, 75},
    {24, 55, 7, 90}, {24, 59, 7, 80}, {24, 62, 7, 80}, {24, 65, 7, 75}, {255, 0, 0, 0}};
static const ev_t PH_LEAD[] = {{0, 72, 3, 100}, {3, 74, 1, 90}, {4, 76, 4, 100}, {8, 79, 2, 100}, {10, 76, 2, 90},
    {12, 74, 4, 100}, {16, 72, 2, 100}, {18, 69, 2, 90}, {20, 67, 4, 100}, {24, 69, 2, 100}, {26, 72, 2, 90},
    {28, 71, 4, 100}, {255, 0, 0, 0}};
static const ev_t PH_PLUCK[] = {{0, 60, 1, 100}, {1, 67, 1, 80}, {2, 72, 1, 90}, {3, 76, 1, 80}, {4, 79, 1, 100},
    {5, 76, 1, 80}, {6, 72, 1, 90}, {7, 67, 1, 80}, {8, 57, 1, 100}, {9, 64, 1, 80}, {10, 69, 1, 90}, {11, 72, 1, 80},
    {12, 76, 1, 100}, {13, 72, 1, 80}, {14, 69, 1, 90}, {15, 64, 1, 80}, {16, 53, 1, 100}, {17, 60, 1, 80},
    {18, 65, 1, 90}, {19, 69, 1, 80}, {20, 72, 1, 100}, {21, 69, 1, 80}, {22, 65, 1, 90}, {23, 60, 1, 80},
    {24, 55, 1, 100}, {25, 62, 1, 80}, {26, 67, 1, 90}, {27, 71, 1, 80}, {28, 74, 1, 100}, {29, 71, 1, 80},
    {30, 67, 1, 90}, {31, 62, 1, 80}, {255, 0, 0, 0}};
static const ev_t PH_PAD[] = {{0, 48, 15, 80}, {0, 55, 15, 70}, {0, 64, 15, 70}, {0, 71, 15, 70}, {16, 53, 15, 80},
    {16, 60, 15, 70}, {16, 64, 15, 70}, {16, 69, 15, 70}, {255, 0, 0, 0}};
static const ev_t PH_BELL[] = {{0, 84, 2, 90}, {6, 91, 2, 80}, {12, 88, 2, 90}, {16, 84, 2, 90}, {22, 79, 2, 80},
    {28, 83, 2, 90}, {255, 0, 0, 0}};
static const ev_t PH_NOISE[] = {{4, 60, 1, 100}, {12, 67, 1, 110}, {20, 60, 1, 100}, {28, 72, 2, 120}, {30, 64, 1, 70},
    {255, 0, 0, 0}};
static const ev_t *const PHRASE[QD_NTRACK] = {PH_DRUMS, PH_BASS, PH_KEYS, PH_LEAD, PH_PLUCK, PH_PAD, PH_BELL, PH_NOISE};

/* the phrases of the tracks in mask, two bars from now */
static void play(uint32_t mask)
{
    const uint32_t t0 = now, n = (uint32_t)(32.0 * STEP_MS) + 1u;
    uint32_t ms;
    for (ms = 0; ms < n; ms++) {
        int t;
        for (t = 0; t < QD_NTRACK; t++) {
            const ev_t *e;
            if (!((mask >> t) & 1u))
                continue;
            for (e = PHRASE[t]; e->at != 255; e++) {
                const uint32_t on = (uint32_t)(e->at * STEP_MS), off = (uint32_t)((e->at + e->len) * STEP_MS) - 15u;
                if (ms == on)
                    midi(0x90 | t, e->note, e->vel);
                if (ms == off)
                    midi(0x80 | t, e->note, 0);
            }
        }
        run(1);
    }
    (void)t0;
}

/* the host's time of n 128-frame blocks, us a block: mean and the largest */
static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}
static void cpu(FILE *log, const char *what, uint32_t blocks)
{
    static int16_t b[256];
    double t0, worst = 0, sum = 0;
    uint32_t i;
    for (i = 0; i < blocks; i++) {
        double t1;
        now += 3u;                                      /* (the clock kept roughly in step: 2.9 ms a block) */
        qd_app_tick(&app, now);
        t0 = now_us();
        qd_app_audio(&app, b, 128);
        t1 = now_us() - t0;
        sum += t1;
        worst = t1 > worst ? t1 : worst;
    }
    fprintf(log, "  %-58s %6.1f us a block (largest %6.1f), %4.1f %% of the block's 2902 us; voices %d\n", what,
            sum / blocks, worst, 100.0 * sum / blocks / 2902.0, qd_pool_in_use(&app.pool));
    printf("  %-58s %6.1f us a block (largest %6.1f), %4.1f %% of 2902 us\n", what, sum / blocks, worst,
           100.0 * sum / blocks / 2902.0);
}

static void counters(FILE *log, const char *what)
{
    const qd_pool_t *p = &app.pool;
    fprintf(log, "%s: %u notes started (%u on a free voice), %u struck again in place, steals %u own / %u across, "
            "%u dropped; peak %d\n", what, p->starts, p->frees, p->restrikes, p->steals_own, p->steals_cross, p->drops,
            (int)peak);
}

int main(int argc, char **argv)
{
    char path[1024];
    FILE *log;
    int k, t;
    if (argc < 2) {
        fprintf(stderr, "usage: render_audio OUT_DIR\n");
        return 2;
    }
    snprintf(path, sizeof path, "%s/audio.txt", argv[1]);
    log = fopen(path, "w");
    if (!log) {
        fprintf(stderr, "render_audio: cannot write %s (does the folder exist?)\n", path);
        return 1;
    }
    fprintf(log, "QUADRILLE P1.1, render_audio: %d voices, MASTER at its default (pot %d), 122 BPM\n", QD_VOICES,
            QD_MASTER_POT_DEFAULT);

    /* presets.wav: track 3 (KEYS), every sound */
    fresh();
    run(100);
    for (k = 0; k < QD_QUAD_NPRESET; k++) {
        static const int CH[4] = {60, 64, 67, 72};
        int i;
        qd_track_load_preset(&app.model.t[2], k);
        for (i = 0; i < 4; i++)
            midi(0x92, CH[i], 100);
        run(1000);
        for (i = 0; i < 4; i++)
            midi(0x82, CH[i], 0);
        run(500);
    }
    run(1500);
    if (!wav(argv[1], "presets.wav"))
        return 1;
    counters(log, "presets.wav (INIT and the 16 on track 3, a 1 s chord and 0.5 s of release each)");

    /* tracks.wav */
    fresh();
    run(100);
    for (t = 0; t < QD_NTRACK; t++) {
        play(1u << t);
        run(400);
    }
    play(0xFFu);
    run(3000);
    if (!wav(argv[1], "tracks.wav"))
        return 1;
    counters(log, "tracks.wav (each track's phrase alone, then all 8 together)");
    for (t = 0; t < QD_NTRACK; t++)
        fprintf(log, "  track %d %-6s %-12s limit %d, level %d, pan %d, sends chorus %d delay %d reverb %d\n", t + 1,
                app.model.t[t].name, qd_track_sound_name(&app.model.t[t]), app.model.t[t].limit, app.model.t[t].mix.level,
                app.model.t[t].mix.pan, app.model.t[t].mix.chorus, app.model.t[t].mix.delay, app.model.t[t].mix.reverb);

    /* pool-stress.wav: 32 notes at once */
    fresh();
    run(100);
    for (t = 0; t < QD_NTRACK; t++)
        for (k = 0; k < 4; k++)
            midi(0x90 | t, 48 + t * 3 + k * 7, 100);
    run(2000);
    fprintf(log, "pool-stress: %d of %d voices in use after 32 notes on 8 tracks (limits 4 1 6 6 6 6 6 6)\n",
            qd_pool_in_use(&app.pool), app.pool.nvoice);
    for (t = 0; t < QD_NTRACK; t++)
        fprintf(log, "  track %d: %d voices\n", t + 1, qd_pool_count(&app.pool, t));
    for (t = 0; t < QD_NTRACK; t++)
        midi(0xB0 | t, 123, 0);
    run(3000);
    if (!wav(argv[1], "pool-stress.wav"))
        return 1;
    counters(log, "pool-stress.wav (32 notes at once, 4 a track)");

    /* the CPU */
    fprintf(log, "the host's CPU for a 128-frame block (2902 us of audio at 44.1 kHz):\n");
    printf("the host's CPU for a 128-frame block:\n");
    for (k = 0; k < 2; k++) {
        fresh();
        for (t = 0; t < QD_NTRACK; t++) {
            if (k)
                qd_track_load_preset(&app.model.t[t], 10);   /* WOBBLE */
            app.model.t[t].mix.chorus = app.model.t[t].mix.delay = app.model.t[t].mix.reverb = 40;
            qd_pool_set_track(&app.pool, t, 16, 0);
        }
        for (t = 0; t < QD_NTRACK; t++) {
            midi(0x90 | t, 48 + t * 2, 100);
            midi(0x90 | t, 55 + t * 2, 100);
        }
        run(500);
        cpu(log, k ? "16 voices of WOBBLE, the effects on" : "16 voices (the default sounds), the effects on", 4000);
        if (!k) {
            for (t = 0; t < QD_NTRACK; t++)
                midi(0xB0 | t, 123, 0);
            run(12000);
            cpu(log, "silence (no voice, the effects off)", 1000);
        }
    }
    {   /* the effects alone, every one running: noise into the three sends and the mix */
        static int32_t sc[QD_MIX_CTL], sd[QD_MIX_CTL], sr[QD_MIX_CTL], l[QD_MIX_CTL], r[QD_MIX_CTL], o[2 * QD_MIX_CTL];
        uint32_t x = 0x1234567u, b, i;
        double sum = 0, worst = 0;
        qd_fx_init();
        for (b = 0; b < 4000u; b++) {
            double t0, t1;
            for (i = 0; i < QD_MIX_CTL * 4u; i++) {
                x ^= x << 13, x ^= x >> 17, x ^= x << 5;
                if (i < QD_MIX_CTL)
                    sc[i] = sd[i] = sr[i] = (int32_t)(x >> 18) - 8192;
            }
            t0 = now_us();
            for (i = 0; i < 4u; i++) {                  /* (128 frames: four control blocks) */
                memset(l, 0, sizeof l), memset(r, 0, sizeof r);
                qd_fx_block(sc, sd, sr, l, r, o);
            }
            t1 = now_us() - t0;
            sum += t1;
            worst = t1 > worst ? t1 : worst;
        }
        fprintf(log, "  %-58s %6.1f us a block (largest %6.1f), %4.1f %% of the block's 2902 us\n",
                "the effects alone (chorus, delay, reverb, Output Clean)", sum / 4000.0, worst, 100.0 * sum / 4000.0 / 2902.0);
        printf("  %-58s %6.1f us a block (largest %6.1f)\n", "the effects alone", sum / 4000.0, worst);
    }
    fclose(log);
    printf("%s\n", path);
    free(buf);
    return 0;
}
