/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* render_audio: QUADRILLE's sound headless, for listening and for the log. Runs the app (core/app) on the core's clock
 * (a tick a ms, the audio in 128-frame blocks as they fall due, as the glue and FM1VST drive it) with notes played as
 * MIDI on channels 1..8 (tracks 1..8), and writes 16-bit stereo WAVs at 44.1 kHz:
 *
 *   presets.wav       every sound in turn on track 3 (KEYS): INIT, then the 16 factory sounds, each a 1 s chord (C4 E4
 *                     G4 C5) and 0.5 s of its release, with the track's default sends (reverb 20)
 *   tracks.wav        the 8 default tracks, each playing a short phrase (two bars at 122 BPM) alone, then all 8 together
 *                     (two bars), then the tails
 *   pool-stress.wav   32 notes at once across the 8 tracks (4 each) on the 16-voice pool, held 2 s, released
 *   demo-A01.wav ..   the built-in demo patterns (seq/demo.c), each played from PLAY for 16 bars at its tempo, with
 *   demo-A03.wav      the effects, then 2.5 s of tails; and demo.txt: the drums of A01 measured one by one (each step's
 *                     sound lock and p-locks rendered alone on one voice: the attack, the decay to -20 and -40 dB, the
 *                     spectral centroid over 0..30 and 30..100 ms, the pitch from zero crossings), the patterns'
 *                     counters, and the host's CPU of the sequencer per block (its own work, then with the player
 *                     starting the voices)
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
#include "sounds.h"

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

/* ---------------------------------------------------------------- the demos --- */
static void press_key(int key)
{
    qd_app_key(&app, key, 1, 100);
    qd_app_key(&app, key, 0, 0);
}
static const uint8_t WHITE_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};

/* demo k: SEQ held + white key k + 1 (stopped: selected at once), PLAY, 16 bars, PLAY, the tails */
static uint32_t play_demo(int k)
{
    uint32_t ms;
    fresh();
    run(50);
    qd_app_button(&app, QD_B_MIX, 1);                  /* (P1.3c: printed SEQ held + W: the pattern) */
    press_key(WHITE_KEY[k]);
    qd_app_button(&app, QD_B_MIX, 0);
    run(50);
    ms = (uint32_t)(16.0 * 4.0 * 60000.0 / app.bpm + 0.5);
    qd_app_button(&app, QD_B_PLAY, 1);
    qd_app_button(&app, QD_B_PLAY, 0);
    run(ms);
    qd_app_button(&app, QD_B_PLAY, 1);
    qd_app_button(&app, QD_B_PLAY, 0);
    run(2500);
    return ms;
}

/* one drum: step s of A01's track 1, its sound lock and p-locks, alone on one voice (velocity of the step, a gate of
 * the step's length at A01's tempo); measured as the drum lab did */
static void drum_metrics(FILE *log, const qd_pattern_t *P, int s, const char *what)
{
    static qd_quad_voice_t V;
    static qd_quad_owner_t O;
    static int32_t x[44100 * 2];
    qd_rstep_t rs;
    const qd_rstep_t *st = &rs;
    int16_t p[QD_QUAD_NP];
    const qd_lock_t *lk;
    int nl, k, b, n = 0, i, gate;
    uint8_t notes[QD_NNOTE];
    qd_step_resolve(P, 0, s, &rs);
    if (st->sound)
        memcpy(p, qd_sound_patch(&app.proj, st->sound), sizeof p);
    else
        qd_quad_preset_patch(13, p);
    lk = qd_locks(P, 0, s, &nl);
    for (k = 0; k < nl; k++)
        if (lk[k].id < QD_QUAD_NP)
            p[lk[k].id] = lk[k].value;
    memcpy(notes, st->notes, sizeof notes);
    gate = (int)(qd_len_eighths(st->len) * 15.0 / P->bpm / 8.0 * 44100.0);
    qd_quad_owner_init(&O);
    O.quad_patch = p;
    O.bpm = (int16_t)P->bpm;
    qd_quad_voice_init(&V, 0);
    qd_quad_voice_start(&V, p, &O, notes[0], st->vel);
    memset(x, 0, sizeof x);
    for (b = 0; b < 44100 * 2 / 32; b++) {
        if (b * 32 >= gate)
            qd_quad_voice_release(&V);
        qd_quad_owner_block(&O);
        if (!qd_quad_voice_render(&V, x + n) && b > 4)
            break;
        n += 32;
    }
    {
        double peak = 0, env[2000], cen[2], zc[3];
        int nw = n / 44, tp = 0, t20 = -1, t40 = -1, w;
        static const int za[3] = {88, 1323, 3528}, zl[3] = {441, 882, 1764};
        for (i = 0; i < nw && i < 2000; i++) {
            double m = 0;
            for (k = 0; k < 44; k++)
                m = fabs((double)x[i * 44 + k]) > m ? fabs((double)x[i * 44 + k]) : m;
            env[i] = m;
            if (m > peak)
                peak = m, tp = i;
        }
        for (i = tp; i < nw && i < 2000; i++) {
            if (t20 < 0 && env[i] < peak * 0.1)
                t20 = i;
            if (t40 < 0 && env[i] < peak * 0.01)
                t40 = i;
        }
        for (w = 0; w < 2; w++) {                       /* centroid: a DFT on a log-ish grid of bins */
            const int a = w ? 1323 : 0, len = w ? 3087 : 1323;
            double num = 0, den = 0;
            int f;
            for (f = 20; f < 16000; f += f < 400 ? 20 : f < 2000 ? 100 : 400) {
                double re = 0, im = 0;
                for (k = 0; k < len && a + k < n; k++) {
                    const double win = 0.5 - 0.5 * cos(2 * 3.141592653589793 * k / len);
                    re += x[a + k] * win * cos(2 * 3.141592653589793 * f * k / 44100.0);
                    im += x[a + k] * win * sin(2 * 3.141592653589793 * f * k / 44100.0);
                }
                {
                    const double mag = sqrt(re * re + im * im) * (f < 400 ? 20 : f < 2000 ? 100 : 400);
                    num += f * mag, den += mag;
                }
            }
            cen[w] = den > 0 ? num / den : 0;
        }
        for (w = 0; w < 3; w++) {                       /* the pitch from zero crossings at 2..12, 30..50, 80..120 ms */
            int z = 0;
            for (k = za[w] + 1; k < za[w] + zl[w] && k < n; k++)
                z += (x[k - 1] < 0) != (x[k] < 0);
            zc[w] = z * 0.5 * 44100.0 / zl[w];
        }
        fprintf(log, "  step %2d %-7s %-9s n%-3d v%-3d %2d locks: peak %5.0f at %2d ms, -20 dB %4d ms, -40 dB %4d ms; "
                     "centroid %5.0f Hz (0-30 ms) %5.0f (30-100); pitch %5.0f %5.0f %5.0f Hz (2-12, 30-50, 80-120 ms)\n",
                s + 1, what, st->sound ? qd_sound_name(&app.proj, st->sound) : "MARIMBA", notes[0], st->vel, nl, peak, tp,
                t20 - tp, t40 - tp, cen[0], cen[1], zc[0], zc[1], zc[2]);
    }
}

static void null_out(void *ctx, const qd_seq_ev_t *e)
{
    (void)ctx;
    (void)e;
}

/* the sequencer's own CPU over n blocks of pattern k: alone (a sink that does nothing), and with the player starting
 * the voices in a pool (no rendering). The mean is the whole run's time over its blocks (the host clock's microsecond
 * steps are too coarse for one block); the largest is one block's, timed alone */
static void seq_cpu(FILE *log, int k, uint32_t blocks)
{
    static qd_seq_t S;
    static qd_pool_t P;
    static qd_player_t PL;
    int pass, timed;
    for (pass = 0; pass < 2; pass++) {
        double mean = 0, worst = 0;
        for (timed = 0; timed < 2; timed++) {
            uint32_t b;
            double t0 = now_us();
            fresh();
            qd_seq_init(&S, &app.proj, &app.model);
            qd_seq_select(&S, k, 0);
            qd_seq_play(&S);
            qd_pool_init(&P, 16);
            qd_player_init(&PL, &S, &P, &app.model);
            t0 = now_us();
            for (b = 0; b < blocks; b++) {
                const double b0 = timed ? now_us() : 0;
                if (pass)
                    qd_player_block(&PL);
                else
                    qd_seq_block(&S, null_out, 0);
                if (timed) {
                    const double d = now_us() - b0;
                    worst = d > worst ? d : worst;
                }
                if (pass) {                             /* (the voices end as if their release had run) */
                    int i;
                    for (i = 0; i < P.nvoice; i++)
                        if (P.v[i].q.active && !P.v[i].q.gate)
                            P.v[i].q.active = 0, P.v[i].pend = 0;
                    qd_pool_block_begin(&P);
                }
            }
            if (!timed)
                mean = (now_us() - t0) / blocks;
        }
        fprintf(log, "  the sequencer, %s, %s: %.3f us a 32-sample block on average (the largest block %.0f us), %u "
                     "steps resolved, %u notes\n", k == 0 ? "A01" : k == 1 ? "A02" : "A03",
                pass ? "with the player starting the voices" : "alone", mean, worst, S.resolved, S.played);
        printf("  sequencer %s %-38s %.3f us a 32-sample block on average (largest %.0f)\n",
               k == 0 ? "A01" : k == 1 ? "A02" : "A03", pass ? "with the player starting the voices" : "alone", mean,
               worst);
    }
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
    fprintf(log, "FIZZ P1.1, render_audio: %d voices, MASTER at its default (pot %d), 122 BPM\n", QD_VOICES,
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

    /* the demos */
    {
        FILE *dl;
        snprintf(path, sizeof path, "%s/demo.txt", argv[1]);
        dl = fopen(path, "w");
        if (!dl)
            return 1;
        fprintf(dl, "FIZZ P1.2, the demo patterns (seq/demo.c)\n\nA01's drums, each alone on one voice:\n");
        {
            static const char *const WHAT[16] = {"kick", "c-hat", "c-hat", "ghost", "snare", "c-hat", "kick", "o-hat",
                                                 "c-hat", "ghost", "kick", "c-hat", "clap", "c-hat", "ghost", "o-hat"};
            fresh();
            for (k = 0; k < 16; k++)
                if (qd_step_type(qd_proj_pat(&app.proj, 0), 0, k))
                    drum_metrics(dl, qd_proj_pat(&app.proj, 0), k, WHAT[k]);
        }
        fprintf(dl, "\nthe renders (16 bars from PLAY, the effects on, then 2.5 s of tails):\n");
        for (k = 0; k < 3; k++) {
            char name[32];
            const uint32_t ms = play_demo(k);
            snprintf(name, sizeof name, "demo-A0%d.wav", k + 1);
            if (!wav(argv[1], name))
                return 1;
            fprintf(dl, "  %s: %d BPM, %.1f s; %u steps resolved, %u notes played, the lock pool %d of 1024; pool: %u "
                        "starts, %u struck again in place, steals %u own / %u across, %u dropped, %u legato; peak %d\n",
                    name, app.bpm, ms / 1000.0, app.seq.resolved, app.seq.played, qd_proj_pat(&app.proj, k)->nlock,
                    app.pool.starts, app.pool.restrikes, app.pool.steals_own, app.pool.steals_cross, app.pool.drops,
                    app.pool.legatos, (int)peak);
        }
        fprintf(dl, "\nthe host's CPU of the sequencer (this Mac), 30 s of each pattern:\n");
        for (k = 0; k < 3; k++)
            seq_cpu(dl, k, 44100 * 30 / 32);
        fclose(dl);
    }

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
