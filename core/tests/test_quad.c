/* SPDX-License-Identifier: GPL-3.0-only */
/* Acceptance B (P1.1): ChoralRoot's own host checks of FM TONE (tests/cr_quad_test.c at v0.25.1, GPL-3.0-only),
 * ported to QUADRILLE's copy of the engine (core/voice/quad). The checks are cr_quad_test.c's, their arithmetic and
 * limits unchanged; what changed is the driver underneath them: ChoralRoot's part (trk[0], quad_vs[0], quad_patch[0])
 * is here an owner, voices of quad_voice.h and a "part patch" every voice takes at its note-on and at every block (as
 * a live edit of the track's sound reaches its voices, PLAN.md §5), which is what quad_patch[0] was to them.
 *
 *   t_blob      2000 random patches through the blob and back; every value at its min, max and default
 *   t_golden    every algorithm and four presets against tests/quad_ref.py's renders (core/tests/quad_goldens, copied
 *               from ChoralRoot's tests/quad_goldens at the tag): SNR over the first 2048 samples > 36 dB and every
 *               10 ms block's RMS within 1.5 dB over 0.3 s
 *   t_env       the envelopes' times against quad_times.h (ATK, DEC to END, END held, DELAY at 120 and 80 BPM, the
 *               amp's sustain and release), LEV, TRIG, RESET, PHRT on a sounding / quiet voice, legato
 *   t_rel_inf   REL INF holds (and is endless), 126 decays, INF -> 30 ends it, a retrigger restarts it, a kill frees it
 *   t_render    a 6-note chord (a note for the mono ones) 1.4 s on every preset: no |sum| near the int32 wrap, peak
 *               under 0.9 FS at LEVEL 92, not silent, every voice ended 20 s after the release; the extremes (every
 *               algorithm at full feedback, levels, HARM, MIX, LFOs onto every destination, resonance, filter types,
 *               key tracks, notes 12..127): no wrap, the voices end
 *   t_ext       E1: AHD (ATK, HOLD, DEC against the tables; NOTE; INF; a switch mid-note both ways), DRIVE PRE, the
 *               glide (TIME, RATE, OFF, a legato)
 *   t_dc        every preset held 1 s, a plain feedback operator at FDBK 0..120: the mean of 0.3..1 s under 2 % / 1 %
 *               of the RMS
 * Built (tools/Makefile) with -fsanitize=signed-integer-overflow -fno-sanitize-recover=all, as ChoralRoot builds
 * cr_quad_test: an int32 wrap anywhere in the engine aborts.
 *
 *     test_quad [GOLDENS_DIR]       (default core/tests/quad_goldens; VERBOSE=1 prints the figures) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quad_voice.c"                 /* (the engine and its internals: quad_voice_t, QP_*, quad_env_tick, ..) */

static int fails, checks;
#define CHECK(c, ...)                                     \
    do {                                                  \
        checks++;                                         \
        if (!(c)) {                                       \
            fails++;                                      \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

static uint32_t rs = 12345u;
static uint32_t rnd(void)
{
    rs = rs * 1664525u + 1013904223u;
    return rs >> 8;
}
static double db(double a, double b) { return 20.0 * log10((a + 1e-9) / (b + 1e-9)); }
static double tms(uint32_t ticks) { return ticks * CTL * 1000.0 / FS; }
static const char *goldens = "core/tests/quad_goldens";

/* ---------------------------------------------------- the driver (QUADRILLE's for cr_quad_test.c's) --- */
#define NV 16
static qd_quad_owner_t own;
static qd_quad_voice_t vv[NV];
static int16_t part_patch[QD_QUAD_NP];       /* ChoralRoot's quad_patch[0]: every voice takes it (E1: the values 74..
                                              * zero: AHD, PORT, DRIVE PRE off) */
#define QS(i) (*(quad_voice_t *)(void *)vv[i].qstate)

static void drv_reset(void)
{
    uint32_t i;
    qd_quad_owner_init(&own);
    own.bpm = 120;
    own.quad_patch = part_patch;
    for (i = 0; i < NV; i++)
        qd_quad_voice_init(&vv[i], i);
    song.g[G_BPM] = 120;
}
/* cr_quad_test.c's patches are in version 3's units (whole numbers, FDBK 0..120), converted for the engine as an old
 * blob's are; quad_init3 / quad_preset3 make them, drv_patch converts, drv_patch4 loads a version-4/5 patch as it is */
static void quad_init3(int16_t *p)
{
    uint32_t i;
    for (i = 0; i < QP_NP; i++)
        p[i] = quad_range_v3(i).def;
    qe_defaults(p);                              /* (E1: DRIVE and the values 74..: today's sound) */
    p[QD_QP_DRIVE] = 0;
}
static void quad_preset3(uint32_t k, int16_t *p)
{
    const uint8_t *e;
    quad_init3(p);
    for (e = QUAD_PRESET_EDITS[k]; *e != 0xFFu; e += 2) {
        quad_rng_t r = quad_range_v3(e[0]);
        p[e[0]] = (int16_t)clamp((int8_t)e[1], r.min, r.max);
    }
}
static int32_t v5_of_v3(uint32_t i, int32_t v) { return quad_clampv(i, quad_v5_of_v4(i, quad_v4_of_v3(i, v))); }
static void drv_patch4(const int16_t *p) { memcpy(part_patch, p, sizeof part_patch); }
static void drv_patch(const int16_t *p)
{
    uint32_t i;
    for (i = 0; i < QP_NP; i++)
        part_patch[i] = (int16_t)v5_of_v3(i, p[i]);
    for (; i < QD_QUAD_NP; i++)                  /* (E1: DRIVE and the values 74.. as they are) */
        part_patch[i] = p[i];
}
static void drv_on(uint32_t i, uint32_t note, uint32_t vel)
{
    qd_quad_voice_start(&vv[i], part_patch, &own, (int)note, (int)vel);
}
/* one control tick: the owner's block, then each voice (done, then render) into out; returns the voices rendered */
static uint32_t drv_tick(int32_t *out)
{
    uint32_t i, nr = 0;
    memset(out, 0, CTL * sizeof *out);
    qd_quad_owner_block(&own);
    for (i = 0; i < NV; i++) {
        if (!vv[i].active)
            continue;
        memcpy(vv[i].patch, part_patch, sizeof part_patch);   /* (the part's edits reach its voices) */
        nr += (uint32_t)qd_quad_voice_render(&vv[i], out);
    }
    return nr;
}

/* ------------------------------------------------------------- the blob --- */
static void rnd_patch(int16_t *p)
{
    uint32_t i;
    for (i = 0; i < QP_NP; i++) {
        quad_rng_t r = quad_range(i);
        p[i] = (int16_t)(r.min + (int32_t)(rnd() % (uint32_t)(r.max - r.min + 1)));
    }
    p[QD_QP_DRIVE] = 0;                          /* (P1.4a: DRIVE 0: ChoralRoot's blob exactly) */
}
static void t_blob(void)
{
    uint8_t b[QD_QUAD_BLOB], b2[QD_QUAD_BLOB];
    int16_t p[QD_QUAD_NP], q[QD_QUAD_NP];
    uint32_t n, i, w;
    for (n = 0; n < 2000u; n++) {                /* version 5 (ChoralRoot's 96 bytes): FM TONE's values, DRIVE */
        rnd_patch(p);
        qd_quad_pack5(p, b);
        CHECK(quad_blob_ok(b), "random patch %u: its blob is not valid", n);
        CHECK(qd_quad_unpack(b, QUAD_BLOB, q) == 1 && !memcmp(p, q, QP_NP * sizeof(int16_t)), "random patch %u: round trip differs", n);
        qd_quad_pack5(q, b2);
        CHECK(!memcmp(b, b2, QUAD_BLOB), "random patch %u: blob round trip differs", n);
        CHECK(b[0] == 'Q' && b[1] == 5u, "magic / version");
        for (i = QUAD_V5_LEN; i < QUAD_BLOB; i++)
            CHECK(!b[i], "padding byte %u not 0", i);
    }
    for (n = 0; n < 2000u; n++) {                /* E1: version 6 (128 bytes): every value, 74.. too */
        int16_t lo, hi;
        rnd_patch(p);
        p[QD_QP_DRIVE] = (int16_t)(rnd() % 128u);
        for (i = QD_QP_AMOD; i < QD_QUAD_NP; i++) {
            qd_quad_value_range((int)i, &lo, &hi, 0);
            p[i] = (int16_t)(lo + (int32_t)(rnd() % (uint32_t)(hi - lo + 1)));
        }
        qd_quad_pack(p, b);
        CHECK(b[0] == 'Q' && b[1] == 6u && qd_quad_unpack(b, QD_QUAD_BLOB, q) == 1 && !memcmp(p, q, sizeof p),
              "version 6 %u: round trip", n);
        CHECK(qd_quad_unpack(b, QUAD_BLOB, q) == 0, "version 6 in 96 bytes: not taken");
        for (i = QUAD_V5_LEN + 1u + (QD_QUAD_NP - QD_QUAD_NP5); i < QD_QUAD_BLOB; i++)
            CHECK(!b[i], "version 6 padding byte %u not 0", i);
    }
    for (w = 0; w < 3u; w++) {                   /* every value at its min, its max, its default */
        int16_t lo, hi, df;
        quad_init_patch(p);
        for (i = 0; i < QD_QUAD_NP; i++) {
            qd_quad_value_range((int)i, &lo, &hi, &df);
            p[i] = w == 0u ? lo : w == 1u ? hi : df;
        }
        qd_quad_pack(p, b);
        CHECK(qd_quad_unpack(b, QD_QUAD_BLOB, q) == 1 && !memcmp(p, q, sizeof p), "the %s patch: round trip", w == 0u ? "min" : w == 1u ? "max" : "default");
    }
    rnd_patch(p);                                /* P1.4a: DRIVE in byte 87, back again; ChoralRoot's unpack refuses it */
    for (i = QD_QP_AMOD; i < QD_QUAD_NP; i++)
        qd_quad_value_range((int)i, 0, 0, &p[i]);
    p[QD_QP_DRIVE] = 10;
    qd_quad_pack5(p, b);
    CHECK(b[QUAD_V5_LEN] == 10 && !quad_blob_ok(b) && qd_quad_unpack(b, QUAD_BLOB, q) == 1 && !memcmp(p, q, sizeof p),
          "DRIVE 10: in byte %u, the round trip (the values 74.. their defaults)", (unsigned)QUAD_V5_LEN);
    memset(b, 0, sizeof b);
    CHECK(qd_quad_unpack(b, QD_QUAD_BLOB, q) == 0, "an empty blob taken");
    qd_quad_preset_patch(0, p);
    CHECK(!memcmp(p, q, sizeof p), "a bad blob: not the init patch");
}

/* ------------------------------------------------- against the model --- */
static char *slurp(const char *fn)
{
    FILE *f = fopen(fn, "rb");
    long n;
    char *s;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    s = malloc((size_t)n + 1u);
    if (s && fread(s, 1, (size_t)n, f) != (size_t)n) {
        free(s);
        s = 0;
    }
    if (s)
        s[n] = 0;
    fclose(f);
    return s;
}
/* the numbers of the JSON array "key": up to max into out, returns how many */
static uint32_t jarr(const char *s, const char *key, double *out, uint32_t max)
{
    char k[32];
    const char *p;
    uint32_t n = 0;
    snprintf(k, sizeof k, "\"%s\":[", key);
    if (!(p = strstr(s, k)))
        return 0;
    p += strlen(k);
    while (*p && *p != ']' && n < max) {
        char *e;
        double v = strtod(p, &e);
        if (e == p)
            break;
        out[n++] = v;
        p = *e == ',' ? e + 1 : e;
    }
    return n;
}

static void t_golden(void)
{
    static const char *const G[12] = {"algo1", "algo2", "algo3", "algo4", "algo5", "algo6", "algo7", "algo8", "ep",
                                      "bass", "glass_pad", "wobble"};
    static const char *const GP[4] = {"EP", "BASS", "GLASS PAD", "WOBBLE"};
    static double first[2048], rms[64], pv[QP_NP];
    static int32_t buf[13230 + CTL];
    uint32_t g, i, k;
    for (g = 0; g < 12u; g++) {
        char fn[512], *s;
        int16_t p[QD_QUAD_NP];
        uint32_t nf, nr, np, nt = (uint32_t)(0.3 * FS / CTL), rel = (uint32_t)(0.2 * FS / CTL);
        double se = 0, sr = 0, worst = 0, snr;
        snprintf(fn, sizeof fn, "%s/%s.json", goldens, G[g]);
        s = slurp(fn);
        CHECK(s != 0, "%s: missing", fn);
        if (!s)
            continue;
        np = jarr(s, "patch", pv, QP_NP);
        nf = jarr(s, "first", first, 2048);
        nr = jarr(s, "rms", rms, 64);
        free(s);
        CHECK(np == QP_NP && nf == 2048u && nr == 29u, "%s: patch %u, first %u, rms %u", fn, np, nf, nr);
        if (np != QP_NP || nf != 2048u || !nr)
            continue;
        qd_quad_preset_patch(0, p);             /* (E1: DRIVE and the values 74..: today's) */
        p[QD_QP_DRIVE] = 0;
        for (i = 0; i < QP_NP; i++)
            p[i] = (int16_t)pv[i];
        if (g >= 8u) {                           /* a preset: the model's copy is the firmware's */
            int16_t q[QD_QUAD_NP];
            for (k = 0; k < QUAD_NPRESETS && strcmp(QUAD_PRESETS[k].name, GP[g - 8u]); k++)
                ;
            qd_quad_preset_patch((int)k + 1, q);
            CHECK(k < QUAD_NPRESETS && !memcmp(p, q, QP_NP * sizeof(int16_t)), "%s: quad_ref.py's patch is not eng_quad.c's", GP[g - 8u]);
        }
        drv_reset();
        drv_patch4(p);                           /* (the goldens' patches: version 4's) */
        drv_on(0, 60, 100);
        drv_on(1, 67, 100);
        for (k = 0; k < nt; k++) {
            if (k == rel)
                vv[0].gate = vv[1].gate = 0;
            drv_tick(buf + k * CTL);
        }
        for (i = 0; i < 2048u; i++) {
            se += (buf[i] - first[i]) * (buf[i] - first[i]);
            sr += first[i] * first[i];
        }
        snr = 10.0 * log10(sr / (se + 1e-9));
        for (k = 0; k < nr; k++) {
            double a = 0;
            for (i = 0; i < 441u; i++)
                a += (double)buf[k * 441u + i] * buf[k * 441u + i];
            a = sqrt(a / 441.0);
            if (rms[k] > 30.0 && fabs(db(a, rms[k])) > worst)
                worst = fabs(db(a, rms[k]));
        }
        printf("  golden %-10s SNR %5.1f dB, block RMS within %.2f dB\n", G[g], snr, worst);
        CHECK(snr > 36.0, "%s: SNR %.1f dB against the model", G[g], snr);
        CHECK(worst < 1.5, "%s: a block's RMS %.2f dB off the model", G[g], worst);
    }
}

/* ------------------------------------------------------- envelopes --- */
/* the times of quad_times.h's entries (the Digitone's, docs/QUAD.md "Times"), ms: an attack's full rise, a decay's T60 */
static double atk_ms(const uint32_t *tab, uint32_t v) { return (double)(1u << 24) / tab[v] * CTL * 1000.0 / FS; }
static double t60_ms(const uint32_t *tab, uint32_t v) { return -3.0 * CTL * 1000.0 / FS / log10(1.0 - tab[v] / 16777216.0); }
static uint32_t ticks_of(double ms) { return (uint32_t)(ms * FS / 1000.0 / CTL + 0.5); }
/* an operator attack's (linear in dB from QUAD_OP_FLOOR to the top: the growth a tick) */
static double oatk_ms(uint32_t v)
{
    return CTL * 1000.0 / FS * log(16777216.0 / QUAD_OP_FLOOR) / log(1.0 + QUAD_T_OATK[v] / 16777216.0);
}

static void t_env(void)
{
    quad_voice_t s;
    uint32_t n, k;
    int32_t want;
    /* ATK 64: to the top in its table's time (the operators' linear in dB from QUAD_OP_FLOOR: QUAD_T_OATK; the
     * filter's and the amp's linear: QUAD_T_FATK, QUAD_T_AATK), within a tick */
    for (k = 0; k < 4u; k++) {
        static const uint32_t *const TA[4] = {QUAD_T_OATK, QUAD_T_OATK, QUAD_T_FATK, QUAD_T_AATK};
        memset(&s, 0, sizeof s);
        s.stage[k] = 2;
        for (n = 0; s.stage[k] == 2 && n < 1000000u; n++)
            quad_env_tick(&s, k, 64, 60, 20 << 17, 0, 0, 1, 0);
        CHECK(fabs(tms(n) - (k < 2u ? oatk_ms(64) : atk_ms(TA[k], 64))) < 1.5 + tms(n) / 1000.0,
              "env %u ATK 64: %.1f ms, want %.1f", k, tms(n), k < 2u ? oatk_ms(64) : atk_ms(TA[k], 64));
    }
    /* DEC 70 to END 20: 99 % of the way (-40 dB: 2/3 of the T60) */
    memset(&s, 0, sizeof s);
    s.stage[0] = 2;
    for (n = 0; s.stage[0] == 2 && n < 1000000u; n++)
        quad_env_tick(&s, 0, 64, 60, 20 << 17, 0, 0, 1, 0);
    want = 20 << 17;
    for (n = 0; abs(s.env[0] - want) > ((1 << 24) - want) / 100 && n < 1000000u; n++)
        quad_env_tick(&s, 0, 64, 70, want, 0, 0, 1, 0);
    CHECK(fabs(tms(n) / (t60_ms(QUAD_T_ODEC, 70) * 2.0 / 3.0) - 1.0) < 0.01, "A DEC 70: %.1f ms to 99 %%, want %.1f",
          tms(n), t60_ms(QUAD_T_ODEC, 70) * 2.0 / 3.0);
    for (n = 0; n < 40000u; n++)                 /* held at END, gate or not */
        quad_env_tick(&s, 0, 64, 70, want, 0, 0, 1, 0);
    CHECK(s.stage[0] == 3 && abs(s.env[0] - want) < (1 << 12), "END held: %d, want %d", s.env[0], want);
    /* DELAY 50: 50 / 32 beats (781.25 ms at 120 BPM, 1171.9 at 80), then the attack */
    for (k = 0; k < 2u; k++) {
        song.g[G_BPM] = (int16_t)(k ? 80 : 120);
        memset(&s, 0, sizeof s);
        s.stage[1] = 1;
        for (n = 0; s.stage[1] == 1 && n < 100000u; n++)
            quad_env_tick(&s, 1, 0, 60, 0, 0, 50, 1, 0);
        CHECK(fabs(tms(n) - 50.0 / 32.0 * 60000.0 / song.g[G_BPM]) < 0.8 && s.env[1] == 0, "B DLY 50 at %d BPM: %.1f ms, want %.1f",
              song.g[G_BPM], tms(n), 50.0 / 32.0 * 60000.0 / song.g[G_BPM]);
    }
    song.g[G_BPM] = 120;
    /* .. and through the owner: its tempo is the one a voice's DELAY counts in */
    {
        int16_t p[QD_QUAD_NP];
        int32_t out[CTL];
        quad_init3(p);
        p[QP_BDLY] = 50;
        drv_reset();
        own.bpm = 80;
        drv_patch(p);
        drv_on(0, 60, 100);
        for (n = 0; QS(0).stage[1] == 1 && n < 100000u; n++)
            drv_tick(out);
        CHECK(fabs(tms(n) - 50.0 / 32.0 * 60000.0 / 80.0) < 0.8, "the owner's 80 BPM: B DLY 50 %.1f ms, want %.1f",
              tms(n), 50.0 / 32.0 * 60000.0 / 80.0);
    }
    /* the amp: sustain, release (T60 QUAD_T_ADEC[70]: to 1/4096, -72 dB, in 1.2 of it), off */
    memset(&s, 0, sizeof s);
    s.stage[3] = 2;
    for (n = 0; n < 3u * FS / CTL; n++)
        quad_env_tick(&s, 3, 20, 30, 90 << 17, 70, 0, 1, 1);
    CHECK(s.stage[3] == 3 && abs(s.env[3] - (90 << 17)) < (1 << 16), "amp sustain %d", s.env[3]);
    for (n = 0; s.stage[3] && n < 100000u; n++)
        quad_env_tick(&s, 3, 20, 30, 90 << 17, 70, 0, 0, 1);
    want = (int32_t)(t60_ms(QUAD_T_ADEC, 70) * 20.0 * log10((90 << 17) / 4096.0) / 60.0);   /* (SUS 90 -> 2^12) */
    CHECK(!s.stage[3] && !s.env[3] && fabs(tms(n) / want - 1.0) < 0.03, "amp REL 70 ended after %.1f ms (to 1/4096), want %d",
          tms(n), want);
    /* in a voice: LEV reached after the attack (velocity 127), TRIG and RESET on a retrigger */
    {
        int32_t out[CTL], lvl, e0;
        int16_t p[QD_QUAD_NP];
        quad_init3(p);
        p[QP_AATK] = 30, p[QP_ADEC] = 100, p[QP_AEND] = 127, p[QP_ALEV] = 100;
        p[QP_BATK] = 30, p[QP_BDEC] = 90, p[QP_BEND] = 30, p[QP_BLEV] = 90;
        drv_reset();
        drv_patch(p);
        drv_on(0, 60, 127);
        for (n = 0; n < ticks_of(oatk_ms(30)) + 4u; n++)
            drv_tick(out);
        lvl = quad_lev(100 * 4096 / 127);
        CHECK(abs(QS(0).lv[0] - lvl) < lvl / 100, "A LEV 100: level %d, want %d", QS(0).lv[0], lvl);
        for (n = 0; n < 20u; n++)
            drv_tick(out);
        e0 = QS(0).env[1];
        part_patch[QP_BTRIG] = 0;                /* TRIG off: a retrigger holds B */
        drv_on(0, 62, 127);
        CHECK(QS(0).env[1] == e0 && QS(0).stage[1] == 3, "B TRIG off: restarted");
        part_patch[QP_BTRIG] = 1;
        part_patch[QP_BRST] = 0;                 /* TRIG on, RESET off: the attack from where it is */
        drv_on(0, 64, 127);
        CHECK(QS(0).env[1] == e0 && QS(0).stage[1] == 2, "B RESET off: env %d stage %u", QS(0).env[1], QS(0).stage[1]);
        part_patch[QP_BRST] = 1;                 /* RESET on: from 0 */
        drv_on(0, 65, 127);
        CHECK(QS(0).env[1] == 0 && QS(0).stage[1] == 2, "B RESET on: env %d", QS(0).env[1]);
        part_patch[QP_PHRT] = QR_OFF;            /* PHRT OFF: the phases run on */
        drv_tick(out);
        e0 = (int32_t)QS(0).ph[0];
        drv_on(0, 67, 127);
        CHECK((int32_t)QS(0).ph[0] == e0 && e0, "PHASE RESET off: phase reset");
        part_patch[QP_PHRT] = QR_ALL;            /* PHRT ALL on a voice that sounds: what is heard runs on */
        drv_tick(out);
        {
            uint32_t ph0[4];
            memcpy(ph0, QS(0).ph, sizeof ph0);
            drv_on(0, 69, 127);
            CHECK(!memcmp(QS(0).ph, ph0, sizeof ph0) && ph0[0], "PHASE RESET on a sounding voice: restarted");
            part_patch[QP_BLEV] = 0;             /* B2 silent (B LEV 0: B1 at 0 too, but B1 is algorithm 1's direct Y) */
            for (n = 0; n < 4u; n++)
                drv_tick(out);
            memcpy(ph0, QS(0).ph, sizeof ph0);
            drv_on(0, 71, 127);
            CHECK(QS(0).ph[0] == ph0[0] && QS(0).ph[1] == ph0[1] && QS(0).ph[2] == ph0[2] && !QS(0).ph[3] && ph0[3],
                  "PHASE RESET on, B2 silent: C %u A %u B1 %u B2 %u", QS(0).ph[0], QS(0).ph[1], QS(0).ph[2], QS(0).ph[3]);
            part_patch[QP_BLEV] = 90;
            vv[0].gate = 0;                      /* released down to under -40 dB (still sounding): all restart */
            for (n = 0; n < 20000u && (QS(0).amp >= 328 || !QS(0).amp); n++)
                drv_tick(out);
            CHECK(QS(0).amp > 0 && QS(0).amp < 328 && QS(0).stage[3], "the release: amp %d", QS(0).amp);
            drv_on(0, 72, 127);
            CHECK(!QS(0).ph[0] && !QS(0).ph[1] && !QS(0).ph[2] && !QS(0).ph[3], "PHASE RESET on a quiet voice: phases %u %u %u %u",
                  QS(0).ph[0], QS(0).ph[1], QS(0).ph[2], QS(0).ph[3]);
        }
        part_patch[QP_ATRIG] = 1;                /* legato with TRIG on: A restarts */
        for (n = 0; n < 200u; n++)
            drv_tick(out);
        quad_legato(&own, &vv[0]);
        CHECK(QS(0).stage[0] == 2 && !QS(0).env[0], "legato, A TRIG on: not restarted");
        part_patch[QP_BTRIG] = 0;
        for (n = 0; n < ticks_of(oatk_ms(30)) + 4u; n++)
            drv_tick(out);
        quad_legato(&own, &vv[0]);
        CHECK(QS(0).stage[1] == 3, "legato, B TRIG off: restarted");
    }
}

/* REL INF (127): the amp and filter envelopes hold their level after the note-off (stage 5) for 2 s within 1 LSB, the
 * voice sounds on and is endless; REL 126 decays; INF turned down ends it; a retrigger restarts it; a kill frees the
 * slot for a fresh note */
static void t_rel_inf(void)
{
    int32_t out[CTL];
    int16_t p[QD_QUAD_NP];
    uint32_t n, r;
    for (r = 126; r <= 127u; r++) {
        int32_t e0, f0, e1, f1;
        double sq = 0.0;
        quad_init3(p);
        p[QP_EATK] = 0, p[QP_EDEC] = 40, p[QP_ESUS] = 100, p[QP_EREL] = (int16_t)r;
        p[QP_FATK] = 0, p[QP_FDEC] = 40, p[QP_FSUS] = 90, p[QP_FREL] = (int16_t)r, p[QP_FDEPTH] = 30, p[QP_FREQ] = 60;
        drv_reset();
        drv_patch(p);
        drv_on(0, 60, 100);
        for (n = 0; n < FS / 2u / CTL; n++)      /* 0.5 s: at SUS */
            drv_tick(out);
        vv[0].gate = 0;                          /* the note-off */
        drv_tick(out);
        e0 = QS(0).env[3] >> 9, f0 = QS(0).env[2] >> 9;
        for (n = 0; n < 2u * FS / CTL; n++)      /* 2 s later */
            drv_tick(out);
        for (n = 0; n < CTL; n++)
            sq += (double)out[n] * out[n];
        e1 = QS(0).env[3] >> 9, f1 = QS(0).env[2] >> 9;
        if (r == 127u) {
            CHECK(abs(e1 - e0) <= 1 && e0 > 20000 && QS(0).stage[3] == 5u && vv[0].active && !quad_done(&own, &vv[0]),
                  "AMP REL INF: amp %d -> %d after 2 s (stage %u, active %u)", e0, e1, QS(0).stage[3], vv[0].active);
            CHECK(abs(f1 - f0) <= 1 && f0 > 20000 && QS(0).stage[2] == 5u, "F REL INF: filter env %d -> %d (stage %u)",
                  f0, f1, QS(0).stage[2]);
            CHECK(sqrt(sq / CTL) > 100.0, "REL INF: silent 2 s after the note-off (RMS %.1f)", sqrt(sq / CTL));
            CHECK(qd_quad_voice_endless(&vv[0]), "REL INF: not endless (the panic would not end it)");
            part_patch[QP_EREL] = 30;            /* REL turned down from INF: it decays and ends */
            for (n = 0; n < 4u * FS / CTL && drv_tick(out); n++)
                ;
            CHECK(!vv[0].active && !QS(0).live, "REL INF -> 30: the held voice did not end");
            part_patch[QP_EREL] = 127;           /* held again; a retrigger restarts it */
            drv_on(0, 60, 100);
            for (n = 0; n < FS / 4u / CTL; n++)
                drv_tick(out);
            vv[0].gate = 0;
            for (n = 0; n < FS / 4u / CTL; n++)
                drv_tick(out);
            CHECK(QS(0).stage[3] == 5u, "REL INF: not held (stage %u)", QS(0).stage[3]);
            drv_on(0, 64, 100);
            CHECK(QS(0).stage[3] == 2u && vv[0].gate, "REL INF: a retrigger did not restart the amp envelope");
            vv[0].gate = 0;
            for (n = 0; n < FS / 4u / CTL; n++)
                drv_tick(out);
            qd_quad_voice_kill(&vv[0]);          /* the fade steal: one block more, then free */
            CHECK(drv_tick(out) == 1 && !vv[0].active, "REL INF: the kill's block not rendered, or the voice still on");
            CHECK(!drv_tick(out), "REL INF: a killed voice still rendered");
            drv_on(0, 67, 100);                  /* .. its slot a fresh voice */
            CHECK(QS(0).stage[3] == 2u && !QS(0).env[3], "REL INF: the slot after a kill not fresh");
        } else {
            CHECK(e1 < e0 * 9 / 10 && f1 < f0 * 9 / 10 && QS(0).stage[3] == 4u && !qd_quad_voice_endless(&vv[0]),
                  "REL 126: amp %d -> %d, filter %d -> %d after 2 s (want a decay)", e0, e1, f0, f1);
        }
    }
}

/* ------------------------------------------------------------- renders --- */
/* secs of a chord (6 notes, or one for a mono preset) on patch p; the part's peak at LEVEL 92, the largest |sum| */
static void chord(const int16_t *p, uint32_t mono, double secs, const uint8_t *notes, int32_t *peak, int64_t *maxabs,
                  uint32_t *left)
{
    static const uint8_t CH[6] = {62, 66, 69, 71, 73, 76};
    int32_t out[CTL];
    uint32_t n, i, nn = mono ? 1u : 6u;
    if (!notes)
        notes = CH;
    drv_reset();
    drv_patch(p);
    *peak = 0;
    *maxabs = 0;
    for (i = 0; i < nn; i++)
        drv_on(i, mono ? 38u : notes[i], 100);
    for (n = 0; n < (uint32_t)(secs * FS / CTL); n++) {
        if (n == (uint32_t)(secs * 0.7 * FS / CTL))
            for (i = 0; i < nn; i++)
                vv[i].gate = 0;
        drv_tick(out);
        for (i = 0; i < CTL; i++) {
            int64_t a = llabs((int64_t)out[i]);
            int32_t x = ((out[i] >> 2) * LEVEL_Q12[92]) >> 10;
            x = x < 0 ? -x : x;
            *maxabs = a > *maxabs ? a : *maxabs;
            *peak = x > *peak ? x : *peak;
        }
    }
    *left = 0;
    for (n = 0; n < 20u * FS / CTL && drv_tick(out); n++)
        ;
    for (i = 0; i < NV; i++)
        *left += vv[i].active;
}

static void t_render(void)
{
    uint32_t k, left, nx = 0;
    int32_t peak;
    int64_t mx;
    for (k = 0; k < QUAD_NPRESETS; k++) {
        int16_t p[QD_QUAD_NP];
        quad_preset3(k, p);
        chord(p, QUAD_PRESETS[k].mono, 1.4, 0, &peak, &mx, &left);
        if (getenv("VERBOSE"))
            printf("  %-12s peak %5.1f %% FS (part at LEVEL 92)\n", QUAD_PRESETS[k].name, peak * 100.0 / 32768);
        CHECK(mx < (1 << 30), "%s: |sum| %lld near the int32 wrap", QUAD_PRESETS[k].name, (long long)mx);
        CHECK(peak < 0.9 * 32768, "%s: peak %.1f %% FS", QUAD_PRESETS[k].name, peak * 100.0 / 32768);
        CHECK(peak > 300, "%s: silent (peak %d)", QUAD_PRESETS[k].name, peak);
        CHECK(!left, "%s: %u voices still active 20 s after the release", QUAD_PRESETS[k].name, left);
    }
    /* the extremes: every algorithm at full feedback, full levels, HARM +-26, MIX at both ends and the middle, the
     * LFOs at full depth onto every destination, full resonance, every filter type, key tracks, top and bottom
     * notes: no int32 wrap (-fsanitize=signed-integer-overflow aborts on one), the voices end */
    {
        static const uint8_t NOTES[6] = {12, 36, 60, 96, 115, 127};
        uint32_t a, h, mi, f;
        for (a = 1; a <= 8u; a++)
            for (h = 0; h < 2u; h++)
                for (mi = 0; mi < 3u; mi++)
                    for (f = 0; f < QF_N; f++) {
                        int16_t p[QD_QUAD_NP];
                        quad_init3(p);
                        p[QP_ALGO] = (int16_t)a;
                        p[QP_RC] = 18, p[QP_RA] = (int16_t)(h ? 63 : 0), p[QP_RB1] = 18, p[QP_RB2] = (int16_t)(18 - h * 2);
                        p[QP_HARM] = (int16_t)(h ? 26 : -26), p[QP_FDBK] = 120, p[QP_DTUN] = 127;
                        p[QP_MIX] = (int16_t)(mi == 0 ? -64 : mi == 1 ? 63 : 0);
                        p[QP_OFSC] = 100, p[QP_OFSB2] = -100;
                        p[QP_ALEV] = p[QP_BLEV] = p[QP_AEND] = p[QP_BEND] = 127;
                        p[QP_AKEY] = p[QP_B1KEY] = p[QP_B2KEY] = (int16_t)(h ? 127 : 0), p[QP_VEL] = 0;
                        p[QP_PHRT] = (int16_t)((a + mi) % QR_N);
                        p[QP_FTYPE] = (int16_t)f, p[QP_RESO] = 127, p[QP_FREQ] = (int16_t)(f == QF_HP12 ? 0 : 90);
                        p[QP_FDEPTH] = (int16_t)(h ? 63 : -64), p[QP_FKTRK] = 127, p[QP_FSUS] = 127;
                        p[QP_BASE] = (int16_t)(f * 20), p[QP_WIDTH] = 60, p[QP_LEVEL] = 127, p[QP_ESUS] = 127;
                        for (k = 0; k < 3u; k++)    /* (every destination over the 192 patches) */
                            p[QP_LFO(k, QL_DEST)] = (int16_t)((a * 5u + k * 13u + h * 7u + mi * 3u + f) % QD_N);
                        p[QP_LFO(2, QL_FADE)] = (int16_t)(h ? 20 : -64);
                        for (k = 0; k < 3u; k++) {
                            p[QP_LFO(k, QL_DEPTH)] = (int16_t)(h ? 63 : -64);
                            p[QP_LFO(k, QL_WAVE)] = (int16_t)(k == 2u ? QW_RAND : QW_SQR);
                            p[QP_LFO(k, QL_PHASE)] = (int16_t)(k == 2u ? 127 : 0);
                            p[QP_LFO(k, QL_SPEED)] = (int16_t)(h ? 63 : -64), p[QP_LFO(k, QL_MULT)] = (int16_t)(11 + 12 * h);
                        }
                        chord(p, 0, 0.3, NOTES, &peak, &mx, &left);
                        CHECK(mx < (1 << 30), "extremes a%u h%u m%u f%u: |sum| %lld", a, h, mi, f, (long long)mx);
                        CHECK(!left, "extremes a%u h%u m%u f%u: voices left", a, h, mi, f);
                        nx++;
                    }
    }
    printf("  renders: 16 presets' chords, %u extreme patches x 6 notes: no wrap, every voice ended\n", nx);
}

/* the mean and the RMS (AC) of 0.3..1 s of one note (60, velocity 100) held 1 s on patch p */
static void held_dc(const int16_t *p, double *mean, double *rms)
{
    int32_t out[CTL];
    uint32_t n, i, cnt = 0, nt = FS / CTL;
    double s = 0, s2 = 0;
    drv_reset();
    drv_patch(p);
    drv_on(0, 60, 100);
    for (n = 0; n < nt; n++) {
        drv_tick(out);
        if (n >= nt * 3 / 10)
            for (i = 0; i < CTL; i++, cnt++)
                s += out[i], s2 += (double)out[i] * out[i];
    }
    *mean = s / cnt;
    *rms = sqrt(s2 / cnt - *mean * *mean);
}

static void t_dc(void)
{
    uint32_t k;
    double m, r, worst = 0;
    int16_t p[QD_QUAD_NP];
    for (k = 0; k < QUAD_NPRESETS; k++) {
        quad_preset3(k, p);
        held_dc(p, &m, &r);
        if (getenv("VERBOSE"))
            printf("  %-12s DC %7.1f RMS %8.1f (%.2f %%)\n", QUAD_PRESETS[k].name, m, r, 100.0 * m / (r + 1e-9));
        if (r >= 100 && fabs(m) / r > worst)
            worst = fabs(m) / r;
        CHECK(r < 100 || fabs(m) < 0.02 * r, "%s: DC %.1f, RMS %.1f", QUAD_PRESETS[k].name, m, r);
    }
    for (k = 0; k <= 120u; k += 8u) {            /* algorithm 8's Y: B1 (feedback) direct */
        quad_init3(p);
        p[QP_ALGO] = 8, p[QP_MIX] = 63, p[QP_ALEV] = 0, p[QP_BLEV] = p[QP_BEND] = 127, p[QP_FDBK] = (int16_t)k;
        held_dc(p, &m, &r);
        CHECK(fabs(m) < 0.01 * r, "plain feedback operator at FDBK %u: DC %.1f, RMS %.1f", k, m, r);
    }
    printf("  DC: the presets' worst mean %.2f %% of the RMS (limit 2 %%)\n", worst * 100.0);
}

/* ------------------------------------------------------------- E1: AHD, DRIVE PRE, the glide --- */
/* the ticks until the amp envelope's stage is st (at most max) */
static uint32_t until_stage(uint32_t st, uint32_t max)
{
    int32_t out[CTL];
    uint32_t n;
    for (n = 0; n < max && QS(0).stage[3] != st && vv[0].active; n++)
        drv_tick(out);
    return n;
}
static void t_ext(void)
{
    int32_t out[CTL], a[CTL], b[CTL];
    int16_t p[QD_QUAD_NP];
    uint32_t n, na, nh;
    double d;
    /* AHD: ATK 20, HOLD 40 (the attack's table), DEC 40; one-shot: the key up in HOLD changes nothing */
    quad_init3(p);
    drv_reset();
    drv_patch(p);
    part_patch[QD_QP_AMOD] = 1, part_patch[QP_EATK] = 20, part_patch[QD_QP_HOLD] = 40, part_patch[QP_EDEC] = 40;
    drv_on(0, 60, 100);
    na = until_stage(6, 20000);
    CHECK(fabs(tms(na) - atk_ms(QUAD_T_AATK, 20)) <= tms(1) + 0.5, "AHD ATK 20: %.1f ms (the table's %.1f)", tms(na),
          atk_ms(QUAD_T_AATK, 20));
    vv[0].gate = 0;                              /* (the key up in a timed HOLD: nothing) */
    nh = until_stage(7, 20000);
    CHECK(fabs(tms(nh) - atk_ms(QUAD_T_AATK, 40)) <= tms(1) + 0.5 && QS(0).env[3] == 1 << 24,
          "AHD HOLD 40: %.1f ms at the top (ATK 40's time %.1f)", tms(nh), atk_ms(QUAD_T_AATK, 40));
    for (n = 0; n < ticks_of(t60_ms(QUAD_T_ADEC, 40)) && vv[0].active; n++)
        drv_tick(out);
    d = 20.0 * log10((double)QS(0).env[3] / 16777216.0);
    CHECK(d < -50.0 && d > -70.0, "AHD DEC 40: %.1f dB after its T60 (want about -60)", d);
    for (n = 0; n < 4u * FS / CTL && drv_tick(out); n++)
        ;
    CHECK(!vv[0].active, "AHD: the voice ends after its decay");
    /* HOLD NOTE: at the top while the key is down, then the decay; DEC INF: held, endless */
    drv_reset();
    part_patch[QD_QP_HOLD] = 127;
    drv_on(0, 60, 100);
    for (n = 0; n < 2u * FS / CTL; n++)
        drv_tick(out);
    CHECK(QS(0).stage[3] == 6u && QS(0).env[3] == 1 << 24, "HOLD NOTE: at the top 2 s on (stage %u)", QS(0).stage[3]);
    vv[0].gate = 0;
    drv_tick(out);
    CHECK(QS(0).stage[3] == 7u && !qd_quad_voice_endless(&vv[0]), "HOLD NOTE: the key up: the decay (stage %u)",
          QS(0).stage[3]);
    for (n = 0; n < 4u * FS / CTL && drv_tick(out); n++)
        ;
    CHECK(!vv[0].active, "HOLD NOTE: the voice ends after the key");
    drv_reset();
    part_patch[QP_EDEC] = 127;
    drv_on(0, 60, 100);
    vv[0].gate = 0;
    for (n = 0; n < FS / CTL; n++)
        drv_tick(out);
    CHECK(vv[0].active && QS(0).stage[3] == 7u && QS(0).env[3] == 1 << 24 && qd_quad_voice_endless(&vv[0]),
          "AHD DEC INF: held at the top after the key, endless");
    /* a switch mid-note: ADSR at SUS -> AHD (the hold, NOTE) -> the key up: AHD's decay -> back to ADSR: its release */
    quad_init3(p);
    drv_reset();
    drv_patch(p);
    part_patch[QP_EATK] = 0, part_patch[QP_EDEC] = 30, part_patch[QP_ESUS] = 100, part_patch[QP_EREL] = 30;
    drv_on(0, 60, 100);
    for (n = 0; n < FS / 2u / CTL; n++)
        drv_tick(out);
    CHECK(QS(0).stage[3] == 3u, "ADSR: at SUS (stage %u)", QS(0).stage[3]);
    part_patch[QD_QP_AMOD] = 1;
    drv_tick(out);
    CHECK(QS(0).stage[3] == 6u, "ADSR -> AHD at SUS: the hold (stage %u)", QS(0).stage[3]);
    vv[0].gate = 0;
    drv_tick(out);
    CHECK(QS(0).stage[3] == 7u, "AHD, the key up: the decay (stage %u)", QS(0).stage[3]);
    part_patch[QD_QP_AMOD] = 0;
    drv_tick(out);
    CHECK(QS(0).stage[3] == 4u, "AHD -> ADSR in the decay: the release (stage %u)", QS(0).stage[3]);
    for (n = 0; n < 4u * FS / CTL && drv_tick(out); n++)
        ;
    CHECK(!vv[0].active, "the switched voice ends (stage %u, env %d, live %u)", QS(0).stage[3], QS(0).env[3], QS(0).live);
    /* DRIVE PRE: the voice's own signal driven (before its filter); POST: the voice untouched (the mix drives it) */
    {
        double ea = 0.0, eb = 0.0, diff = 0.0;
        uint32_t k;
        quad_init3(p);
        for (k = 0; k < 2; k++) {
            drv_reset();
            drv_patch(p);
            part_patch[QD_QP_DRIVE] = 110;
            part_patch[QD_QP_DRPRE] = (int16_t)k;
            drv_on(0, 48, 120);
            for (n = 0; n < FS / 4u / CTL; n++) {
                drv_tick(k ? b : a);
                if (k) {
                    uint32_t j;
                    for (j = 0; j < CTL; j++) {
                        eb += (double)b[j] * b[j];
                        diff += fabs((double)b[j]);
                    }
                }
            }
            if (!k) {                            /* (POST: the same as DRIVE 0's voice) */
                drv_reset();
                drv_patch(p);
                drv_on(0, 48, 120);
                for (n = 0; n < FS / 4u / CTL; n++) {
                    uint32_t j;
                    drv_tick(out);
                    if (n == FS / 4u / CTL - 1u)
                        for (j = 0; j < CTL; j++)
                            ea += (double)(out[j] - a[j]) * (out[j] - a[j]);
                }
            }
        }
        CHECK(ea == 0.0, "DRIVE POST: the voice untouched (the mix drives it)");
        CHECK(eb > 0.0 && diff > 0.0, "DRIVE PRE: the voice sounds");
        {   /* PRE against no drive at all: different */
            double e2 = 0.0;
            drv_reset();
            drv_patch(p);
            drv_on(0, 48, 120);
            for (n = 0; n < FS / 4u / CTL; n++) {
                uint32_t j;
                drv_tick(out);
                if (n == FS / 4u / CTL - 1u)
                    for (j = 0; j < CTL; j++)
                        e2 += (double)(out[j] - b[j]) * (out[j] - b[j]);
            }
            CHECK(e2 > 0.0, "DRIVE PRE 110: the voice changed by it");
        }
    }
    /* the glide: TIME (1 + PTIM^2 / 8 blocks, whatever the interval), RATE (QE_GLRATE a block), PORT OFF: none */
    quad_init3(p);
    drv_reset();
    drv_patch(p);
    part_patch[QD_QP_PORT] = 1, part_patch[QD_QP_PTIM] = 20, part_patch[QD_QP_PSLOPE] = 0;
    drv_on(0, 60, 100);
    qd_quad_voice_glide(&vv[0], 48 * 16);
    CHECK(vv[0].pitch_cur == 48 * 16 && vv[0].pitch16 == 60 * 16, "glide: from 48 to 60");
    for (n = 0; n < 400 && vv[0].pitch_cur != vv[0].pitch16; n++)
        drv_tick(out);
    CHECK(n == 64, "TIME 20: 12 semitones in %u blocks (51 blocks' step, 3 / 16 a block: 64)", n);
    drv_on(0, 72, 100);
    qd_quad_voice_glide(&vv[0], 48 * 16);
    for (n = 0; n < 400 && vv[0].pitch_cur != vv[0].pitch16; n++)
        drv_tick(out);
    CHECK(n == 55, "TIME 20: 24 semitones in %u blocks (about as long: 7 / 16 a block, 55)", n);
    part_patch[QD_QP_PSLOPE] = 1;
    drv_on(0, 60, 100);
    qd_quad_voice_glide(&vv[0], 48 * 16);
    for (n = 0; n < 400 && vv[0].pitch_cur != vv[0].pitch16; n++)
        drv_tick(out);
    CHECK(n == (192u + QE_GLRATE[20] - 1u) / QE_GLRATE[20], "RATE 20: 12 semitones in %u blocks (%u a block)", n,
          QE_GLRATE[20]);
    part_patch[QD_QP_PORT] = 0;
    drv_on(0, 60, 100);
    qd_quad_voice_glide(&vv[0], 48 * 16);
    CHECK(vv[0].pitch_cur == 60 * 16, "PORT OFF: no glide");
    part_patch[QD_QP_PORT] = 1, part_patch[QD_QP_PSLOPE] = 0;   /* a TIE (legato) glides from where the voice is */
    drv_on(0, 60, 100);
    drv_tick(out);
    qd_quad_voice_legato(&vv[0], 67, 100);
    CHECK(vv[0].pitch_cur == 60 * 16 && vv[0].pitch16 == 67 * 16 && vv[0].gstep > 0, "legato with PORT: glides 60 -> 67");
    part_patch[QD_QP_PORT] = 0;
    drv_tick(out);
    vv[0].patch[QD_QP_PORT] = 0;
    qd_quad_voice_legato(&vv[0], 70, 100);
    CHECK(vv[0].pitch_cur == 70 * 16, "legato with PORT OFF: at once (as before)");
}

int main(int argc, char **argv)
{
    if (argc > 1)
        goldens = argv[1];
    t_blob();
    t_golden();
    t_env();
    t_rel_inf();
    t_render();
    t_dc();
    t_ext();
    printf("test_quad: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
