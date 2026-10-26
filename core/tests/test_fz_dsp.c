/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
 * not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Self-test of fz_tables.h, fz_dsp.h, fz_core.h and fz_voice.h against independent references (double-precision
 * formulas, brute force, int64 shadow arithmetic) and against the behaviours the voice code relies on.
 *
 *   cc -std=c99 -Wall -Wextra -Wpedantic -O1 -g -fsanitize=undefined,signed-integer-overflow \
 *      -fno-sanitize-recover=all -I core/voice/quad core/tests/test_fz_dsp.c -lm -o test_fz_dsp
 *   ./test_fz_dsp          (add --full for an exhaustive fz_isqrt over all 2^32 inputs) */

#define FZ_TABLES_DEFINE                /* (this test links no quad_voice.c: the tables defined here) */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fz_core.h"
#include "fz_voice.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures;
static int checks;

#define CHECK(cond, ...)                                                                                              \
    do {                                                                                                              \
        checks++;                                                                                                     \
        if (!(cond)) {                                                                                                \
            failures++;                                                                                               \
            if (failures <= 40) {                                                                                     \
                printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                           \
                printf(__VA_ARGS__);                                                                                  \
                printf("\n");                                                                                         \
            }                                                                                                         \
        }                                                                                                             \
    } while (0)

static uint32_t rng_state = 0x12345678u;
static uint32_t rnd(void)
{
    /* xorshift32 */
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

/* ---- tables: recompute each from its formula in C doubles ---- */
static void test_tables(void)
{
    int i;
    for (i = 0; i <= 1024; i++) {
        long want = lround(32767.0 * sin(2.0 * M_PI * i / 1024.0));
        CHECK(SINE[i] == want, "SINE[%d] = %d, want %ld", i, SINE[i], want);
    }
    for (i = 0; i < 128; i++) {
        double h = 0.05 * pow(800.0, i / 127.0);
        uint32_t want = (uint32_t)(h * CTL / FS * 4294967296.0);
        CHECK(LFO_INC[i] == want, "LFO_INC[%d] = %u, want %u", i, LFO_INC[i], want);
    }
    for (i = 0; i < 128; i++) {
        double fc = 30.0 * pow(16000.0 / 30.0, i / 127.0);
        int want = (int)(4096.0 * tan(M_PI * fc / FS));
        CHECK(SVF_G[i] == want, "SVF_G[%d] = %d, want %d", i, SVF_G[i], want);
    }
    CHECK(LEVEL_Q12[0] == 0, "LEVEL_Q12[0]");
    CHECK(LEVEL_Q12[112] == 4096, "LEVEL_Q12[112] = %d", LEVEL_Q12[112]);
    for (i = 1; i < 128; i++) {
        long want = lround(4096.0 * pow(10.0, ((i - 112) / 2.0) / 20.0));
        CHECK(LEVEL_Q12[i] == want, "LEVEL_Q12[%d] = %d, want %ld", i, LEVEL_Q12[i], want);
    }
    for (i = 0; i <= 256; i++) {
        int want = (int)(32767.0 * tanh(i / 128.0));
        CHECK(TANH_Q15[i] == want, "TANH_Q15[%d] = %d, want %d", i, TANH_Q15[i], want);
    }
    CHECK(QE_GLRATE[0] == 743, "QE_GLRATE[0] = %d", QE_GLRATE[0]);
    for (i = 0; i < 128; i++) {
        double ms = pow(10000.0, i / 127.0);
        long big = (long)(16777216.0 * 32.0 / (ms * 44.1));
        long want;
        if (big < 1)
            big = 1;
        want = big >> 14;
        if (want < 1)
            want = 1;
        CHECK(QE_GLRATE[i] == want, "QE_GLRATE[%d] = %d, want %ld", i, QE_GLRATE[i], want);
    }
    CHECK(sizeof(FZ_PITCH_OCT) <= 1024, "pitch table is %u bytes", (unsigned)sizeof(FZ_PITCH_OCT));
}

static void test_pitch(void)
{
    uint32_t p;
    for (p = 0; p < 2048; p++) {
        double f = 440.0 * pow(2.0, (p / 16.0 - 69.0) / 12.0);
        double want = f / FS * 4294967296.0;
        uint32_t w = (uint32_t)want;
        CHECK(pitch_inc(p) == w, "pitch_inc(%u) = %u, want %u", p, pitch_inc(p), w);
        CHECK(pitch_inc(p) <= 0x7FFFFFFFu, "pitch_inc(%u) above 0x7FFFFFFF", p);
    }
    CHECK(pitch_inc(69 * 16) == (uint32_t)(440.0 / FS * 4294967296.0), "A4");
    CHECK(pitch_inc(2048) == pitch_inc(2047), "clamp 2048");
    CHECK(pitch_inc(0xFFFFFFFFu) == pitch_inc(2047), "clamp max");
}

/* ---- primitives ---- */
static void test_basic(void)
{
    int32_t a, b;
    int i;
    CHECK(clamp(-5, 0, 10) == 0 && clamp(15, 0, 10) == 10 && clamp(7, 0, 10) == 7, "clamp");
    CHECK(mulq15(32768, 1234) == 1234 && mulq15(-16384, 3) == -2, "mulq15");
    for (i = 0; i < 100000; i++) {
        a = (int32_t)(rnd() % 65536u) - 32768;
        b = (int32_t)(rnd() % 65536u) - 32768;
        CHECK(mulq15(a, b) == (int32_t)floor((double)a * b / 32768.0), "mulq15(%d, %d)", a, b);
    }
    CHECK(fz_master_of_pot(0) == 0 && fz_master_of_pot(16) == 1 && fz_master_of_pot(1023) == 4088, "master pot");
    CHECK(VOICE_FS == 24000 && CTL == 32 && (1 << CTL_LOG2) == CTL, "constants");
}

static void test_osc(void)
{
    uint32_t ph;
    int32_t maxerr = 0;
    long n;
    for (n = 0; n < 2000000; n++) {
        int32_t s, t;
        double ref, tri;
        ph = (n < 1000000) ? (uint32_t)n * 4295u : rnd();
        s = sine_i(ph);
        CHECK(s == osc_sine(ph), "osc_sine != sine_i");
        ref = 32767.0 * sin(2.0 * M_PI * ph / 4294967296.0);
        if (fabs(s - ref) > maxerr)
            maxerr = (int32_t)ceil(fabs(s - ref));
        /* triangle: rises -32768..32767 over the first half, falls back over the second */
        t = osc_tri(ph);
        tri = (ph < 0x80000000u) ? (double)(ph >> 15) - 32768.0 : 98303.0 - (double)(ph >> 15);
        CHECK(t == (int32_t)tri && t >= -32768 && t <= 32767, "osc_tri(%u) = %d", ph, t);
    }
    CHECK(maxerr <= 3, "sine_i max error %d", maxerr);
    CHECK(sine_i(0) == 0 && sine_i(0x40000000u) == 32767 && sine_i(0xC0000000u) == -32767, "sine points");
    CHECK(osc_tri(0) == -32768 && osc_tri(0x7FFFFFFFu) == 32767 && osc_tri(0x80000000u) == 32767, "tri points");
}

static void test_softclip(void)
{
    int32_t x, prev = softclip(-70000);
    int32_t maxerr = 0;
    for (x = -70000; x <= 70000; x++) {
        int32_t y = softclip(x);
        double ref = 32767.0 * tanh(x / 32768.0);
        if (x > -65536 && x < 65536 && fabs(y - ref) > maxerr)
            maxerr = (int32_t)ceil(fabs(y - ref));
        CHECK(y == -softclip(-x), "softclip odd at %d", x);
        CHECK(y >= prev, "softclip monotonic at %d", x);
        prev = y;
    }
    CHECK(maxerr <= 4, "softclip max error vs tanh %d", maxerr);
    CHECK(softclip(INT32_MAX) == TANH_Q15[256] && softclip(INT32_MIN) == -TANH_Q15[256], "softclip extremes");
    for (x = -60000; x <= 60000; x += 7) {
        CHECK(soft_knee(x, 20000) == (abs(x) <= 20000 ? x : (x > 0 ? 1 : -1) * (20000 + (softclip((abs(x) - 20000) * 2) >> 1))),
              "soft_knee(%d)", x);
        CHECK(soft_knee(x, 20000) == -soft_knee(-x, 20000), "soft_knee odd");
        CHECK(abs(soft_knee(x, 20000)) <= 20000 + (TANH_Q15[256] >> 1), "soft_knee bound");
    }
}

/* ---- SVF ---- */
static void test_svf_coefs(void)
{
    int32_t cut, k;
    int ki;
    const int32_t ks[] = {0, 256, 1024, 2048, 4096, 8192};
    for (ki = 0; ki < (int)NELEM(ks); ki++) {
        k = ks[ki];
        for (cut = -1000; cut <= 33000; cut++) {
            tsvf_t c;
            int32_t a1, a2, a3, f2, cc = cut < 0 ? 0 : (cut > 32512 ? 32512 : cut);
            double g = SVF_G[cc >> 8], gf;
            double fa1, fa2;
            if ((cc >> 8) < 127)
                g += floor((SVF_G[(cc >> 8) + 1] - (double)SVF_G[cc >> 8]) * (cc & 255) / 256.0);
            gf = (SVF_G[cc >> 8] * 256.0 + ((cc >> 8) < 127 ? (SVF_G[(cc >> 8) + 1] - (double)SVF_G[cc >> 8]) * (cc & 255) : 0)) / 256.0;
            tsvf_coef_k(&c, cut, k);
            fz_svf_coef_fine(cut, k, &a1, &a2, &a3, &f2);
            /* reference in doubles: a1 = 8192 / (1 + g (g + k)), a2 = a1 g, a3 = a2 g (g, k in Q12) */
            fa1 = 8192.0 / (1.0 + (g / 4096.0) * ((g + k) / 4096.0));
            CHECK(fabs(c.a1 - fa1) <= 3.0, "a1 cut %d k %d: %d vs %.2f", cut, k, c.a1, fa1);
            CHECK(fabs(c.a2 - fa1 * g / 4096.0) <= 2.0 + 3.0 * g / 4096.0, "a2 cut %d k %d", cut, k);
            CHECK(fabs(c.a3 - fa1 * g * g / 16777216.0) <= 3.0 + 3.0 * g * g / 16777216.0 + 2.0 * g / 4096.0, "a3 cut %d k %d", cut, k);
            fa1 = 8192.0 / (1.0 + (gf / 4096.0) * ((gf + k) / 4096.0));
            fa2 = fa1 * gf / 4096.0;
            CHECK(fabs(a1 - fa1) <= 3.0, "fine a1 cut %d k %d: %d vs %.2f", cut, k, a1, fa1);
            CHECK(f2 >= 0 && f2 < 32, "f2 range");
            CHECK(fabs(a2 + f2 / 32.0 - fa2) <= 1.0 + 3.0 * gf / 4096.0, "fine a2 cut %d k %d: %d+%d/32 vs %.3f", cut, k, a2, f2, fa2);
            CHECK(fabs(a3 - fa2 * gf / 4096.0) <= 2.0 + 3.0 * gf * gf / 16777216.0, "fine a3 cut %d k %d", cut, k);
        }
    }
}

/* the tick written again in int64; returns the largest intermediate magnitude seen */
static int64_t svf_tick_ref(int32_t a1, int32_t a2, int32_t a3, int32_t f2, int32_t *s1, int32_t *s2, int32_t x,
                            int32_t *v1, int32_t *v2)
{
    int64_t v3 = (int64_t)x - *s2;
    int64_t p[8], t1, t2, b, l, n1, n2, m = 0;
    int j;
    /* every partial sum and product of the C expression, in its evaluation order */
    p[0] = (int64_t)a1 * *s1;
    p[1] = (int64_t)a2 * v3;
    p[2] = p[0] + p[1];
    p[3] = p[2] + ((f2 * v3) >> 5);
    t1 = p[3] + 4096;
    p[4] = (int64_t)a2 * *s1;
    p[5] = (int64_t)a3 * v3;
    p[6] = p[4] + p[5];
    p[7] = p[6] + ((f2 * (int64_t)*s1) >> 5);
    t2 = p[7] + 4096;
    b = t1 >> 13;
    l = *s2 + (t2 >> 13);
    n1 = 2 * b - *s1;
    n2 = 2 * l - *s2;
    for (j = 0; j < 8; j++)
        if (llabs(p[j]) > m)
            m = llabs(p[j]);
    if (llabs(t1) > m)
        m = llabs(t1);
    if (llabs(t2) > m)
        m = llabs(t2);
    if (llabs(2 * b) > m)
        m = llabs(2 * b);
    if (llabs(2 * l) > m)
        m = llabs(2 * l);
    *s1 = (int32_t)(n1 < -150000 ? -150000 : n1 > 150000 ? 150000 : n1);
    *s2 = (int32_t)(n2 < -150000 ? -150000 : n2 > 150000 ? 150000 : n2);
    *v1 = (int32_t)b;
    *v2 = (int32_t)l;
    return m;
}

static void test_svf_run(void)
{
    int32_t a1, a2, a3, f2, s1 = 0, s2 = 0, v1 = 0, v2 = 0, r1 = 0, r2 = 0, rv1, rv2;
    int i, amp, ki, cut;
    tsvf_t c;
    int64_t worst;
    static const int32_t ks[] = {0, 512, 4096};
    static const int32_t amps[] = {24000, 65536, 98304, 131072, 262144};

    /* DC through the low-pass settles at the input */
    tsvf_coef_k(&c, 64 << 8, 4096);
    for (i = 0; i < 20000; i++)
        fz_svf_tick(c.a1, c.a2, c.a3, 0, &s1, &s2, 20000, &v1, &v2);
    /* the rounding (+4096 >> 13) leaves a dead zone of 4096 / a3 around the input */
    CHECK(abs(v2 - 20000) <= 4096 / c.a3 + 1 && abs(v1) <= 16, "svf DC: low %d band %d (a3 %d)", v2, v1, c.a3);

    /* a tone far above the cut-off is attenuated by the low-pass */
    {
        int32_t peak = 0;
        fz_svf_coef_fine(20 << 8, 4096, &a1, &a2, &a3, &f2);
        s1 = s2 = 0;
        for (i = 0; i < 20000; i++) {
            fz_svf_tick(a1, a2, a3, f2, &s1, &s2, sine_i((uint32_t)i * pitch_inc(100 * 16)) , &v1, &v2);
            if (i > 10000 && abs(v2) > peak)
                peak = abs(v2);
        }
        CHECK(peak < 300, "svf low-pass of a high tone peaks at %d", peak);
    }

    /* the int32 tick equals the int64 one while nothing exceeds int32; find where full-scale squares overflow */
    for (ki = 0; ki < (int)NELEM(ks); ki++) {
        for (amp = 0; amp < (int)NELEM(amps); amp++) {
            worst = 0;
            for (cut = 0; cut <= 32512; cut += 512) {
                int32_t x;
                fz_svf_coef_fine(cut, ks[ki], &a1, &a2, &a3, &f2);
                s1 = s2 = r1 = r2 = 0;
                for (i = 0; i < 4000; i++) {
                    int64_t m;
                    /* a square at a cut-dependent period, the worst case for a resonant filter */
                    x = ((i / (2 + cut / 1024)) & 1) ? amps[amp] : -amps[amp];
                    m = svf_tick_ref(a1, a2, a3, f2, &r1, &r2, x, &rv1, &rv2);
                    if (m > worst)
                        worst = m;
                    if (m <= INT32_MAX - 1) {
                        fz_svf_tick(a1, a2, a3, f2, &s1, &s2, x, &v1, &v2);
                        CHECK(v1 == rv1 && v2 == rv2 && s1 == r1 && s2 == r2, "svf tick vs int64");
                    } else {
                        s1 = r1;
                        s2 = r2;
                    }
                }
            }
            printf("  svf: k %5d square +-%6d: largest intermediate %lld%s\n", ks[ki], amps[amp], (long long)worst,
                   worst > INT32_MAX ? "  (would overflow int32)" : "");
            if (amps[amp] <= 24000)
                CHECK(worst <= INT32_MAX, "svf overflows at a voice's full scale");
        }
    }
}

static void test_dc(void)
{
    fz_dc_t s = {0, 0};
    int32_t y = 0;
    int i;
    double sum = 0;
    for (i = 0; i < 200000; i++)
        y = fz_dc_step(&s, 10000);
    CHECK(y == 0 && s.acc == 640000, "dc settles: y %d acc %d", y, s.acc);
    /* a sine passes nearly unchanged and its mean is removed */
    s.acc = s.rem = 0;
    for (i = 0; i < 400000; i++) {
        int32_t x = 5000 + (sine_i((uint32_t)i * pitch_inc(69 * 16)) >> 1);
        y = fz_dc_step(&s, x);
        if (i >= 200000)
            sum += y - (x - 5000);
    }
    CHECK(fabs(sum / 200000.0) < 1.0, "dc: output minus the input's AC part averages %.3f", sum / 200000.0);
}

static void test_isqrt(int full)
{
    uint32_t r;
    int i;
    for (r = 0; r < 65536u; r++) {
        uint32_t sq = r * r;
        CHECK(fz_isqrt(sq) == r, "isqrt(%u^2)", r);
        if (r > 0)
            CHECK(fz_isqrt(sq - 1u) == r - 1u, "isqrt(%u^2 - 1)", r);
        CHECK(fz_isqrt(sq + 2u * r) == r, "isqrt(%u^2 + 2r)", r);
    }
    CHECK(fz_isqrt(0xFFFFFFFFu) == 65535u, "isqrt max");
    for (i = 0; i < 2000000; i++) {
        uint32_t v = rnd();
        uint32_t want = (uint32_t)floor(sqrt((double)v));
        CHECK(fz_isqrt(v) == want, "isqrt(%u)", v);
    }
    if (full) {
        /* every input: walk the squares and check each interval [r^2, (r+1)^2) maps to r */
        uint64_t v = 0;
        uint32_t want = 0;
        long bad = 0;
        for (v = 0; v <= 0xFFFFFFFFull; v++) {
            if ((uint64_t)(want + 1u) * (want + 1u) == v)
                want++;
            if (fz_isqrt((uint32_t)v) != want)
                bad++;
        }
        CHECK(bad == 0, "isqrt exhaustive: %ld wrong", bad);
        printf("  isqrt: exhaustive over 2^32 inputs done\n");
    }
}

/* ---- envelopes ---- */
static uint32_t TATK[128], TDEC[128];

static void make_env_tables(void)
{
    int i;
    for (i = 0; i < 128; i++) {
        TATK[i] = (uint32_t)((1u << 24) / (uint32_t)(1 + 4 * i));
        TDEC[i] = (uint32_t)((1u << 24) / (uint32_t)(2 + 8 * i));
    }
}

static void test_env(void)
{
    int32_t lvl = 0, out = 0;
    uint8_t st = 1;
    uint32_t prog = 0;
    int ticks;

    /* delay of 2 units at a tempo of 120 with dly_k = 1 << 20: 2 << 24 / (120 << 20) = 0.27 -> ticks */
    for (ticks = 0; st == 1 && ticks < 1000; ticks++)
        out = fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 10, 20, 1 << 23, 30, 2, 1, 1, 1, 120, 0, 1u << 20);
    CHECK(st == 2 && ticks == (int)(((2u << 24) + (120u << 20) - 1u) / (120u << 20)) && lvl == 0, "delay ticks %d",
          ticks);
    /* tempo clamps to 20..300 */
    st = 1;
    prog = 0;
    fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 10, 20, 1 << 23, 30, 2, 1, 1, 1, 5000, 0, 1u);
    CHECK(prog == 300u, "bpm clamp high %u", prog);
    st = 1;
    prog = 0;
    fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 10, 20, 1 << 23, 30, 2, 1, 1, 1, 1, 0, 1u);
    CHECK(prog == 20u, "bpm clamp low %u", prog);

    /* linear attack: 41 steps of TATK[10] reach full scale, then decay toward the sustain */
    st = 2;
    lvl = 0;
    for (ticks = 0; st == 2; ticks++)
        out = fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 10, 20, 1 << 23, 30, 0, 1, 1, 1, 120, 0, 0);
    CHECK(ticks == (int)(((1u << 24) + TATK[10] - 1u) / TATK[10]) && lvl == 1 << 24 && out == 32768, "attack %d", ticks);
    for (ticks = 0; ticks < 5000; ticks++)
        out = fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 10, 20, 1 << 23, 30, 0, 1, 1, 1, 120, 0, 0);
    CHECK(st == 3 && abs(lvl - (1 << 23)) < 200, "sustain %d", lvl);
    /* amp decay 127 holds */
    {
        int32_t keep = lvl;
        fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 10, 127, 0, 30, 0, 1, 1, 1, 120, 0, 0);
        CHECK(lvl == keep, "amp decay 127 holds");
        fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 10, 127, 0, 30, 0, 1, 1, 0, 120, 0, 0);
        CHECK(lvl < keep, "non-amp decay 127 moves");
    }
    /* gate off with adsr: release to silence */
    for (ticks = 0; st != 0 && ticks < 100000; ticks++)
        out = fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 10, 20, 1 << 23, 30, 0, 0, 1, 1, 120, 0, 0);
    CHECK(st == 0 && lvl == 0 && out == 0, "release ends");
    /* without adsr the gate does not end the decay */
    st = 3;
    lvl = 1 << 24;
    fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 10, 20, 1 << 23, 30, 0, 0, 0, 1, 120, 0, 0);
    CHECK(st == 3, "no adsr keeps stage");
    /* release 127 holds in stage 5 */
    st = 4;
    lvl = 5000000;
    fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 10, 20, 0, 127, 0, 0, 1, 1, 120, 0, 0);
    CHECK(st == 5 && lvl == 5000000, "release hold");
    fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 10, 20, 0, 40, 0, 0, 1, 1, 120, 0, 0);
    CHECK(st == 4 && lvl == 5000000 - (int32_t)(((int64_t)5000000 * TDEC[40] + 0xFFFFFF) >> 24), "release step");
    /* operator attack: multiplicative from the floor */
    st = 2;
    lvl = 0;
    fz_env_step(&lvl, &st, &prog, 1, TATK, TDEC, 3, 20, 0, 30, 0, 1, 1, 0, 120, 1 << 16, 0);
    CHECK(lvl == (1 << 16) + (int32_t)(((int64_t)(1 << 16) * TATK[3]) >> 24), "op attack %d", lvl);
    /* stage 0 and junk stages force silence */
    st = 0;
    lvl = 777;
    CHECK(fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 0, 0, 0, 0, 0, 1, 1, 1, 120, 0, 0) == 0 && lvl == 0, "off");
    st = 9;
    lvl = 777;
    fz_env_step(&lvl, &st, &prog, 0, TATK, TDEC, 0, 0, 0, 0, 0, 1, 1, 1, 120, 0, 0);
    CHECK(lvl == 0, "junk stage");
}

static void test_ahd(void)
{
    int32_t lvl = 0;
    uint8_t st = 1;
    uint32_t prog = 99;
    int ticks;
    fz_ahd_step(&lvl, &st, &prog, TATK, TDEC, 20, 30, 40, 1);
    CHECK(st == 2 && lvl == (int32_t)TATK[20], "ahd delay -> attack (%d)", st);
    while (st == 2)
        fz_ahd_step(&lvl, &st, &prog, TATK, TDEC, 20, 30, 40, 1);
    CHECK(st == 6 && lvl == 1 << 24 && prog == 0, "ahd attack -> hold");
    for (ticks = 0; st == 6; ticks++)
        fz_ahd_step(&lvl, &st, &prog, TATK, TDEC, 20, 30, 40, 1);
    CHECK(ticks == (int)(((1u << 24) + TATK[30] - 1u) / TATK[30]) && lvl == 1 << 24, "hold ticks %d", ticks);
    while (st == 7)
        fz_ahd_step(&lvl, &st, &prog, TATK, TDEC, 20, 30, 40, 1);
    CHECK(st == 0 && lvl == 0, "ahd decay ends");
    /* hold 127 waits for the gate */
    st = 6;
    lvl = 1 << 24;
    fz_ahd_step(&lvl, &st, &prog, TATK, TDEC, 20, 127, 40, 1);
    CHECK(st == 6, "infinite hold with gate");
    fz_ahd_step(&lvl, &st, &prog, TATK, TDEC, 20, 127, 40, 0);
    CHECK(st == 7, "infinite hold released");
    /* decay 127 holds; stage mapping of 3 and 4/5 */
    fz_ahd_step(&lvl, &st, &prog, TATK, TDEC, 20, 127, 127, 0);
    CHECK(st == 7 && lvl == 1 << 24, "decay 127 holds");
    st = 3;
    prog = 5;
    fz_ahd_step(&lvl, &st, &prog, TATK, TDEC, 20, 30, 40, 1);
    CHECK(st == 6 && prog == TATK[30], "3 -> hold");
    st = 5;
    fz_ahd_step(&lvl, &st, &prog, TATK, TDEC, 20, 30, 40, 1);
    CHECK(st == 7 && lvl < 1 << 24, "5 -> decay");
}

static void test_glide(void)
{
    int32_t t, d;
    for (t = 0; t < 128; t++)
        for (d = -3000; d <= 3000; d += 37) {
            int32_t s = fz_glide_time_step(1000, 1000 + d, t);
            int32_t want = abs(d) / (1 + t * t / 8);
            CHECK(s == (want > 0 ? want : 1), "glide step");
        }
    CHECK(fz_glide_move(100, 200, 30) == 130 && fz_glide_move(100, 120, 30) == 120, "glide up");
    CHECK(fz_glide_move(200, 100, 30) == 170 && fz_glide_move(110, 100, 30) == 100, "glide down");
    CHECK(fz_glide_move(100, 200, 0) == 101 && fz_glide_move(100, 200, -5) == 101, "glide min step");
    CHECK(fz_glide_move(5, 5, 9) == 5, "glide at target");
    CHECK(fz_glide_move(INT32_MIN, INT32_MAX, INT32_MAX) == -1 && fz_glide_move(0, INT32_MAX, INT32_MAX) == INT32_MAX,
          "glide extremes");
}

static void test_drive(void)
{
    fz_drive_t s = {0, 0, 0}, z = {0, 0, 0};
    int32_t buf[CTL], ref[CTL];
    int i, blk, drive;
    for (i = 0; i < CTL; i++)
        buf[i] = ref[i] = (int32_t)(rnd() % 100000u) - 50000;
    s.hp = 3;
    fz_drive(&s, 0, buf, CTL);
    CHECK(memcmp(buf, ref, sizeof buf) == 0 && s.hp == 3, "drive 0 bypass");
    /* silence stays silence; full-range noise stays bounded and never overflows (UBSan) */
    for (drive = 1; drive < 128; drive++) {
        s = z;
        for (blk = 0; blk < 20; blk++) {
            for (i = 0; i < CTL; i++)
                buf[i] = 0;
            fz_drive(&s, drive, buf, CTL);
        }
        CHECK(buf[CTL - 1] == 0, "drive %d silence -> %d", drive, buf[CTL - 1]);
        for (blk = 0; blk < 200; blk++) {
            for (i = 0; i < CTL; i++)
                buf[i] = (blk & 1) ? 884000 : -884000;
            if (blk % 7 == 0)
                for (i = 0; i < CTL; i++)
                    buf[i] = (int32_t)(rnd() % 1768001u) - 884000;
            fz_drive(&s, drive, buf, CTL);
            for (i = 0; i < CTL; i++)
                CHECK(abs(buf[i]) <= 32768, "drive output bounded");
        }
    }
}

static void test_mix(void)
{
    int32_t gl, gr, pan, v;
    int32_t b[CTL], l[CTL], r[CTL], s[3][CTL];
    int64_t rl[CTL], rr[CTL], rs[3][CTL];
    int trial, i, k;

    for (pan = -64; pan <= 63; pan++) {
        fz_pan_gains(pan, &gl, &gr);
        CHECK(gl == (pan > 0 ? 4096 - 64 * pan : 4096) && gr == (pan < 0 ? 4096 + 64 * pan : 4096), "pan %d", pan);
    }
    for (v = 0; v < 128; v++)
        CHECK(fz_send_gain(v) == v * 258, "send gain");

    for (trial = 0; trial < 20000; trial++) {
        int32_t was[3], now[3], m = 0, xmax, lvl;
        for (k = 0; k < 3; k++) {
            was[k] = (trial % 5 == k) ? 0 : fz_send_gain((int32_t)(rnd() % 128u));
            now[k] = (trial % 7 == k) ? 0 : fz_send_gain((int32_t)(rnd() % 128u));
            if (was[k] > m)
                m = was[k];
            if (now[k] > m)
                m = now[k];
        }
        /* fz_send_xmax follows its specified comparison sequence, which can miss was[1] or was[2] (see
         * test_send_xmax); the caller here passes the bound of the true maximum, so no product overflows */
        xmax = 0x7FFFFFFF / (m | 1);
        lvl = LEVEL_Q12[rnd() % 128u];
        pan = (int32_t)(rnd() % 128u) - 64;
        fz_pan_gains(pan, &gl, &gr);
        for (i = 0; i < CTL; i++) {
            b[i] = (trial & 1) ? ((rnd() & 1) ? INT32_MAX : INT32_MIN) : (int32_t)(rnd() % 2000001u) - 1000000;
            l[i] = r[i] = (int32_t)(rnd() % 1000u);
            rl[i] = l[i];
            rr[i] = r[i];
            for (k = 0; k < 3; k++)
                rs[k][i] = s[k][i] = (int32_t)(rnd() % 1000u);
        }
        fz_mix_voice(b, lvl, gl, gr, xmax, was, now, l, r, s[0], s[1], s[2]);
        /* reference in int64 */
        {
            int64_t g[3], d[3];
            for (k = 0; k < 3; k++) {
                g[k] = was[k];
                d[k] = ((int64_t)now[k] - was[k]) >> 5;
            }
            for (i = 0; i < CTL; i++) {
                int64_t bb = b[i] < -884000 ? -884000 : b[i] > 884000 ? 884000 : b[i];
                int64_t x = ((bb >> 2) * lvl) >> 10, xs;
                x = x < -524287 ? -524287 : x > 524287 ? 524287 : x;
                xs = x < -xmax ? -xmax : x > xmax ? xmax : x;
                for (k = 0; k < 3; k++) {
                    g[k] += d[k];
                    if (g[k] != 0)
                        rs[k][i] += (xs * g[k]) >> 15;
                }
                rl[i] += (x * gl) >> 12;
                rr[i] += (x * gr) >> 12;
            }
            for (i = 0; i < CTL; i++) {
                CHECK(l[i] == rl[i] && r[i] == rr[i], "mix l/r");
                for (k = 0; k < 3; k++)
                    CHECK(s[k][i] == rs[k][i], "mix send %d", k);
            }
        }
    }
    /* a send whose gain stays 0 is never touched (may be NULL) */
    {
        int32_t was[3] = {0, 1000, 0}, now[3] = {0, 1000, 0};
        for (i = 0; i < CTL; i++)
            b[i] = 1000, l[i] = r[i] = s[1][i] = 0;
        fz_mix_voice(b, 4096, 4096, 4096, fz_send_xmax(now, was), was, now, l, r, NULL, s[1], NULL);
        CHECK(l[0] == 1000 && r[0] == 1000 && s[1][0] == (1000 * 1000) >> 15, "mix with NULL sends");
    }
}

/* fz_send_xmax: the specified sequence of comparisons, case by case */
static void test_send_xmax(void)
{
    static const int32_t cases[][7] = {
        /* now[3], was[3], the m the sequence picks */
        {100, 200, 300, 0, 0, 0, 300},
        {0, 0, 0, 400, 0, 0, 400},
        {0, 0, 0, 0, 500, 0, 500},
        {0, 0, 0, 0, 0, 600, 600},
        {10, 20, 30, 40, 50, 60, 60},
        {0, 100, 0, 0, 29863, 0, 100},   /* now[1] beats m, so was[1] is never looked at */
        {0, 0, 100, 0, 0, 29863, 100},   /* the same for the third send */
        {0, 0, 0, 0, 0, 0, 0},
    };
    int i;
    for (i = 0; i < (int)NELEM(cases); i++) {
        int32_t now[3], was[3];
        now[0] = cases[i][0];
        now[1] = cases[i][1];
        now[2] = cases[i][2];
        was[0] = cases[i][3];
        was[1] = cases[i][4];
        was[2] = cases[i][5];
        CHECK(fz_send_xmax(now, was) == 0x7FFFFFFF / (cases[i][6] | 1), "send xmax case %d", i);
    }
}

/* ---- chorus ---- */
static void test_chorus(void)
{
    static int16_t line[2048];
    fz_chorus_t c;
    uint32_t w = 77, ph = 12345, n;
    int32_t in[64], ml[64], mr[64];
    int blk, i;
    long t, hit = -1;

    memset(&c, 0, sizeof c);
    memset(line, 0x55, sizeof line);
    for (i = 0; i < 32; i++)
        in[i] = ml[i] = mr[i] = 0;
    /* idle and silent: only the counters move */
    fz_chorus_block(&c, line, &w, &ph, in, ml, mr, 32, 64, 50, 64);
    CHECK(w == 109 && ph == 12345u + (LFO_INC[50] / 32u) * 32u && c.live == 0 && line[100] == 0x5555 && ml[0] == 0,
          "chorus idle");

    /* depth 0, width 0: both taps at exactly 400 samples; an impulse comes back as (x >> 1) * 2 */
    memset(line, 0, sizeof line);
    w = 0;
    t = 0;
    for (blk = 0; blk < 40; blk++) {
        for (i = 0; i < 32; i++) {
            in[i] = (t + i == 5) ? 30001 : 0;
            ml[i] = mr[i] = 0;
        }
        fz_chorus_block(&c, line, &w, &ph, in, ml, mr, 32, 0, 30, 0);
        for (i = 0; i < 32; i++) {
            if (ml[i] != 0 || mr[i] != 0) {
                CHECK(hit < 0 && ml[i] == 30000 && mr[i] == 30000, "impulse value %d %d", ml[i], mr[i]);
                hit = t + i;
            }
        }
        t += 32;
    }
    CHECK(hit == 405, "impulse delay %ld", hit);
    CHECK(c.live == 2048u - (40u * 32u - 32u), "live countdown %u", c.live);

    /* a constant input reads back as the constant through both swept taps, with odd block sizes too */
    memset(&c, 0, sizeof c);
    memset(line, 0, sizeof line);
    for (blk = 0; blk < 300; blk++) {
        n = (uint32_t)(blk % 3 == 0 ? 32 : 1 + (blk % 31));
        for (i = 0; i < (int)n; i++) {
            in[i] = -12346;
            ml[i] = mr[i] = 0;
        }
        fz_chorus_block(&c, line, &w, &ph, in, ml, mr, n, 127, 127, 127);
        if (blk > 200)
            for (i = 0; i < (int)n; i++)
                CHECK(ml[i] == -12346 && mr[i] == -12346, "chorus constant %d %d", ml[i], mr[i]);
    }
    /* the depth and offset glide toward a new target */
    {
        int32_t dc0 = c.dc;
        for (i = 0; i < 32; i++)
            in[i] = 1000;
        fz_chorus_block(&c, line, &w, &ph, in, ml, mr, 32, 0, 10, 0);
        CHECK(c.dc == dc0 + ((0 - dc0) >> 6) && c.dc > 0, "depth glide");
        for (blk = 0; blk < 2000; blk++)
            fz_chorus_block(&c, line, &w, &ph, in, ml, mr, 32, 0, 10, 0);
        CHECK(c.dc == 0 && c.oc == 0, "glide arrives dc %d oc %d", c.dc, c.oc);
        for (blk = 0; blk < 2000; blk++)
            fz_chorus_block(&c, line, &w, &ph, in, ml, mr, 32, 127, 10, 127);
        CHECK(c.dc == 127 * 96 && c.oc == 127 * 8454660, "glide up arrives dc %d oc %d", c.dc, c.oc);
    }
    /* the tail drains to idle */
    for (i = 0; i < 32; i++)
        in[i] = 0;
    for (blk = 0; blk < 70; blk++)
        fz_chorus_block(&c, line, &w, &ph, in, ml, mr, 32, 127, 10, 127);
    CHECK(c.live == 0, "chorus drains");
    /* n = 0 is harmless */
    fz_chorus_block(&c, line, &w, &ph, in, ml, mr, 0, 127, 10, 127);
}

static void test_core(void)
{
    static const char *const names[] = {"A", "B"};
    static const param_desc_t pd = {"LBL", F_ENUM, 0, 1, 0, names, ""};
    static const preset_t pr = {"P", {1, 2, 3, 4, 5, 6, 7, 8}, {9, 10, 11, 12}, -3, 1, FX(0, 1, 2, 3), PAT(4)};
    static const preset_t pr2 = {"Q", {0}, {0}, 0, 0, {0}, 0};
    static const eng_page_t pg = {"T", {{"a", F_INT, 0, 127, 64, NULL, NULL}}};
    vmod_t vm = {0, 0, 0, 0, 0};
    CHECK(F_INT == 0 && F_OFS == 14 && F_TIMEI == 17 && F_FIX2 == 18 && F_INTI == 19, "formats");
    CHECK(P_COUNT == 1 && G_COUNT == 3 && V_UNISON == 3, "enums");
    CHECK(pd.fmt == F_ENUM && pd.max == 1 && pd.names == names, "param_desc_t");
    CHECK(pr.fx[0] == 1 && pr.fx[3] == 4 && pr.pat == 4 && pr.fenv == -3 && pr.mono == 1, "preset_t helpers");
    CHECK(pr2.fx[1] == 0 && pr2.pat == 0, "preset defaults");
    CHECK(NELEM(pg.col) == 4 && pg.col[0].def == 64 && ENG_BLOB_MAX == 160u, "eng_page_t");
    song.g[G_BPM] = 120;
    song.master_q12 = 4096;
    CHECK(song.g[G_BPM] == 120 && vm.inc == 0, "song");
}

int main(int argc, char **argv)
{
    int full = argc > 1 && strcmp(argv[1], "--full") == 0;
    make_env_tables();
    test_tables();
    test_pitch();
    test_basic();
    test_osc();
    test_softclip();
    test_svf_coefs();
    test_svf_run();
    test_dc();
    test_isqrt(full);
    test_env();
    test_ahd();
    test_glide();
    test_drive();
    test_mix();
    test_send_xmax();
    test_chorus();
    test_core();
    printf("test_fz_dsp: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
