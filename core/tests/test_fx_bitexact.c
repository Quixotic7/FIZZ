/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* Acceptance C (P1.1), the effects against ChoralRoot: FIZZ's (core/voice/fx: fz_out.c, cr_fx.c, built by qd_fx.c;
 * L1: the Felucca-derived parts written anew) against ChoralRoot's own code compiled from the v0.25.1 tag
 * (ref/cr_fx_ref.c, through its hostsim.c: GPL-3.0, a test's oracle, not FIZZ), sample for sample, at its defaults:
 *   - an impulse into the chorus (2 s), the delay (3 s, at 120 and at 97 BPM: the synced time follows the tempo) and the
 *     reverb (5 s): the stereo wet;
 *   - Output Clean (the trim, the DC blocker, the look-ahead limiter, the knee, MASTER) on a loud chord-like signal with
 *     a hit in it, then silence, at MASTER 4096 (full) and 1600;
 *   - the whole chain (qd_fx_block: chorus, delay, reverb, the master filter, Output Clean) on noise bursts into every
 *     send and the dry mix, then 5 s of silence (the tails, and their end: exact zeros in the last second).
 * Also: the defaults the copy powers on with are ChoralRoot's (params.c's chorus rate and depth, cr_fx.c's XFP).
 * P1.4a: DRIVE, FIZZ's per-voice DIST (quad_voice.c qd_quad_dist) against ChoralRoot's fx.c track_dist, on a
 * loud FM-like signal with the drive changing block by block (0 between: the state kept), and its host cost.
 *
 *     test_fx_bitexact        exit 1 on any difference */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qd_fx.c"                      /* (the copies and their internals: xc_block, xf_block, out_master, ..) */
#include "ref/bitexact.h"
#include "quad/quad_voice.h"
#include <time.h>

#define NMAX (6u * 44100u)

static void qd_reset(int bpm, uint32_t master_q12)
{
    qd_fx_init();
    qd_fx_tempo(bpm);
    fx_master_now(master_q12);
    fx_output_set(1u, 1);
}

static void qd_run(int which, const int32_t *in, int32_t *out, uint32_t n, int bpm, uint32_t master_q12)
{
    static int32_t z[CTL], s[CTL], l[CTL], r[CTL];
    uint32_t i, k;
    qd_reset(bpm, master_q12);
    for (i = 0; i + CTL <= n; i += CTL) {
        for (k = 0; k < CTL; k++) {
            z[k] = 0;
            if (which == BE_FX_CLEAN) {
                l[k] = in[2u * (i + k)];
                r[k] = in[2u * (i + k) + 1u];
            } else {
                s[k] = in[i + k];
                l[k] = r[k] = 0;
            }
        }
        if (which == BE_FX_CHORUS)
            xc_block(s, l, r, CTL);
        else if (which == BE_FX_DELAY)
            xf_block(s, z, l, r, CTL);
        else if (which == BE_FX_REVERB)
            xf_block(z, s, l, r, CTL);
        if (which == BE_FX_CLEAN) {
            out_master(out + 2u * i, l, r, (const int32_t *)0, CTL);
        } else {
            for (k = 0; k < CTL; k++) {
                out[2u * (i + k)] = l[k];
                out[2u * (i + k) + 1u] = r[k];
            }
        }
    }
}

static void qd_chain(const int32_t *in, int32_t *out, uint32_t n, int bpm, uint32_t master_q12)
{
    static int32_t sc[CTL], sd[CTL], sr[CTL], l[CTL], r[CTL];
    uint32_t i, k;
    qd_reset(bpm, master_q12);
    for (i = 0; i + CTL <= n; i += CTL) {
        for (k = 0; k < CTL; k++) {
            const int32_t *f = in + 5u * (i + k);
            sc[k] = f[0], sd[k] = f[1], sr[k] = f[2], l[k] = f[3], r[k] = f[4];
        }
        qd_fx_block(sc, sd, sr, l, r, out + 2u * i);
    }
}

static int bad;
static void compare(const char *what, const int32_t *a, const int32_t *b, uint32_t frames)
{
    uint32_t i, diff = 0, first = 0, nz = 0;
    int32_t pk = 0, last = 0;
    for (i = 0; i < 2u * frames; i++) {
        if (a[i] != b[i]) {
            if (!diff)
                first = i / 2u;
            diff++;
        }
        if (a[i]) {
            nz++;
            last = (int32_t)(i / 2u);
        }
        pk = abs(a[i]) > pk ? abs(a[i]) : pk;
    }
    printf("  %s %-46s %6u frames, %6u non-zero samples, peak %6d, last non-zero %.2f s: %s",
           diff ? "FAIL" : "ok  ", what, (unsigned)frames, (unsigned)nz, (int)pk, last / 44100.0,
           diff ? "" : "identical\n");
    if (diff)
        printf("%u samples differ (the first at frame %u)\n", (unsigned)diff, (unsigned)first);
    if (!nz) {
        printf("       (silent: nothing was compared)\n");
        diff++;
    }
    bad += diff != 0;
}

static uint32_t ns = 0x2545F491u;              /* (L1: a linear congruential noise, both sides fed the same) */
static int32_t noise(int32_t amp)
{
    ns = ns * 1664525u + 1013904223u;
    return (int32_t)(((int64_t)(int32_t)ns * amp) >> 31);
}

int main(void)
{
    static int32_t in[5u * NMAX], a[2u * NMAX], b[2u * NMAX];
    static const struct { int which; const char *name; double secs; int bpm; } IMP[] = {
        {BE_FX_CHORUS, "impulse -> chorus (stereo, rate 40, depth 60)", 2.0, 120},
        {BE_FX_DELAY, "impulse -> delay (half rate, 1/8 synced, 120 BPM)", 3.0, 120},
        {BE_FX_DELAY, "impulse -> delay (97 BPM)", 3.0, 97},
        {BE_FX_REVERB, "impulse -> reverb (half rate)", 5.0, 120},
    };
    uint32_t k, i, n;
    /* the defaults */
    qd_fx_init();
    printf("       the defaults: chorus rate %d depth %d, delay time %d fdbk %d sync %d mode %d, reverb size %d decay %d,"
           " Output %s, MASTER %u\n", song.g[G_CRATE], song.g[G_CDEPTH], xfp[XP_DTIME], xfp[XP_DFDBK],
           xfp[XP_DSYNC], xfp[XP_DMODE], xfp[XP_RSIZE], xfp[XP_RDECAY], "Clean",
           (unsigned)song.master_q12);
    for (k = 0; k < sizeof IMP / sizeof IMP[0]; k++) {
        n = (uint32_t)(IMP[k].secs * 44100.0) / CTL * CTL;
        memset(in, 0, n * sizeof *in);
        in[0] = 30000;
        qd_run(IMP[k].which, in, a, n, IMP[k].bpm, 4096);
        cr_fx_ref_run(IMP[k].which, in, b, n, IMP[k].bpm, 4096);
        compare(IMP[k].name, a, b, n);
    }
    /* Output Clean: a loud chord-like sum (over the limiter's threshold) with a hit at 0.5 s, then silence */
    for (k = 0; k < 2u; k++) {
        uint32_t m = k ? 1600u : 4096u;
        char what[64];
        n = 3u * 44100u / CTL * CTL;
        for (i = 0; i < n; i++) {
            double t = i / 44100.0, v = 0;
            int j;
            for (j = 0; j < 6; j++)
                v += 14000.0 * sin(6.2831853 * (110.0 * (1 + j * 0.26)) * t + j);
            if (i >= n * 2u / 3u)
                v = 0;
            if (i >= 22050u && i < 22050u + 300u)
                v += 90000.0;
            in[2u * i] = (int32_t)v;
            in[2u * i + 1u] = (int32_t)(v * 0.8);
        }
        qd_run(BE_FX_CLEAN, in, a, n, 120, m);
        cr_fx_ref_run(BE_FX_CLEAN, in, b, n, 120, m);
        snprintf(what, sizeof what, "Output Clean, a loud chord and a hit, MASTER %u", (unsigned)m);
        compare(what, a, b, n);
    }
    /* the whole chain */
    n = 6u * 44100u / CTL * CTL;
    for (i = 0; i < n; i++) {
        int on = i < 44100u && (i / 4410u) % 2u == 0u;
        in[5u * i] = on ? noise(30000) : 0;
        in[5u * i + 1u] = on ? noise(30000) : 0;
        in[5u * i + 2u] = on ? noise(30000) : 0;
        in[5u * i + 3u] = on ? noise(20000) : 0;
        in[5u * i + 4u] = on ? noise(20000) : 0;
    }
    qd_chain(in, a, n, 120, 3000);
    cr_fx_ref_chain(in, b, n, 120, 3000);
    compare("the chain: bursts into every send and the mix", a, b, n);
    for (i = n - 44100u; i < n; i++)
        if (a[2u * i] || a[2u * i + 1u])
            break;
    printf("  %s the chain's tails end in exact zeros (the last 1 s)\n", i == n ? "ok  " : "FAIL");
    bad += i != n;
    {   /* P1.4a: DRIVE against track_dist: 4 s, the drive a step a quarter second (0, 1, 10 (WOBBLE), 40, 64, 100, 127,
         * 0 again, 77 ..), a chord of six partials with hits up to the clamp */
        static int dr[NMAX / CTL];
        static const int D[16] = {0, 1, 10, 40, 64, 100, 127, 0, 77, 5, 0, 120, 33, 127, 0, 90};
        qd_dist_t st;
        clock_t c0;
        double ns;
        uint32_t nb, reps = 2000u, r;
        n = 4u * 44100u / CTL * CTL;
        nb = n / CTL;
        for (i = 0; i < n; i++) {
            double t = i / 44100.0, v = 0;
            int j;
            for (j = 0; j < 6; j++)
                v += 9000.0 * sin(6.2831853 * (82.4 * (1 + j * 0.5)) * t + j) * (1.0 + (double)(i % 11025u) / 11025.0);
            if (i % 22050u < 200u)
                v += 250000.0;
            in[i] = (int32_t)v;
        }
        for (i = 0; i < nb; i++)
            dr[i] = D[(i * CTL / 11025u) % 16u];
        memset(&st, 0, sizeof st);
        for (i = 0; i < n; i += CTL) {
            memcpy(a + i, in + i, CTL * sizeof *a);
            qd_quad_dist(&st, dr[i / CTL], a + i, (int)CTL);
        }
        cr_fx_ref_dist(in, b, n, dr);
        for (i = 0; i < n && a[i] == b[i]; i++)
            ;
        printf("  %s DRIVE (per voice) against track_dist: %u samples, the drive changing every 0.25 s (0 .. 127): %s\n",
               i == n ? "ok  " : "FAIL", (unsigned)n, i == n ? "identical" : "DIFFERENT");
        bad += i != n;
        memset(&st, 0, sizeof st);              /* the host cost: a block at drive 64 */
        c0 = clock();
        for (r = 0; r < reps; r++)
            for (i = 0; i < 256u; i++) {
                memcpy(a, in + i * CTL, CTL * sizeof *a);
                qd_quad_dist(&st, 64, a, (int)CTL);
            }
        ns = (double)(clock() - c0) / CLOCKS_PER_SEC * 1e9 / (double)(reps * 256u);
        printf("       DRIVE's host cost: %.0f ns a 32-sample block (%.3f %% of one core a driven voice)\n", ns,
               ns / (32.0 / 44100.0 * 1e9) * 100.0);
    }
    printf("test_fx_bitexact: %s\n", bad ? "DIFFERENCES" : "all identical");
    return bad ? 1 : 0;
}
