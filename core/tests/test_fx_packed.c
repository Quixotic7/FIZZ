/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ (2026-10-27): the reverb's lines packed 12-bit (cr_fx.c QD_FX_REVERB_PACKED, the Bluetooth package's) against
 * ChoralRoot's 16-bit lines, in one program. This file is compiled twice: as itself (the 16-bit effects, and main)
 * and with -DFXP_PACKED_TU (the packed effects, their public names pk_*); cr_fx.c keeps its state file-static, so the
 * two never meet. Whatever QD_PKG_BLE the build's profile gives, each half sets the option itself.
 * Through qd_fx_block (the chorus's and the delay's sends 0, MASTER at the pot's top), a reverb-send impulse and a
 * 100 ms noise burst at the defaults and at SIZE / DECAY / DAMP / PRE / MOD's ends (and a SIZE + PRE move mid-burst):
 *   (1) close: the difference's RMS over the first second, in dBFS (FXP_ABS_DBFS) and under a loud response's RMS
 *       (FXP_REL_DB);
 *   (2) the tails end: after the input the packed reverb switches itself off (r_live 0), no later than the 16-bit
 *       one, and the output is exactly 0 from then on. DECAY Inf holds a loud tail as the 16-bit one does (its 5 s
 *       windows never climbing, within FXP_INF_DB of the 16-bit one's over a minute); a quiet one, down at the stored
 *       unit, ends as every tail does at its last units (the 16-bit one's at its own, 24 dB lower: later);
 *   (3) two runs give the same samples.
 * And the packing itself: every start and length of a read, across the ring's end, against the single read.
 *     test_fx_packed        exit 1 on a failure */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef QD_FX_REVERB_PACKED
#ifdef FXP_PACKED_TU
#define QD_FX_REVERB_PACKED 1
#define qd_fx_init pk_qd_fx_init
#define qd_fx_tempo_fx pk_qd_fx_tempo_fx
#define qd_fx_tempo pk_qd_fx_tempo
#define qd_fx_master_pot pk_qd_fx_master_pot
#define qd_fx_master_q12 pk_qd_fx_master_q12
#define qd_fx_route_block pk_qd_fx_route_block
#define qd_fx_chain_g pk_qd_fx_chain_g
#define qd_fx_route_move pk_qd_fx_route_move
#define qd_fx_get pk_qd_fx_get
#define qd_fx_set pk_qd_fx_set
#define qd_fx_range pk_qd_fx_range
#define qd_fx_text pk_qd_fx_text
#define qd_fx_delay_ms pk_qd_fx_delay_ms
#define qd_fx_chorus_hz100 pk_qd_fx_chorus_hz100
#define qd_fx_reverb_ms pk_qd_fx_reverb_ms
#define qd_fx_block pk_qd_fx_block
#else
#define QD_FX_REVERB_PACKED 0
#endif
#include "qd_fx.c"

#ifdef FXP_PACKED_TU
uint32_t pk_r_live(void);
int pk_pack_check(void);
uint32_t pk_r_live(void) { return xf.r_live; }
typedef char pk_rings_ok[sizeof xr_buf == 24576u && sizeof xr_pbuf == 6144u ? 1 : -1];   /* (12 bits a sample) */
/* the packing: a ring of 64 written a block at a time, read back from every start, every length 1..40 (across its
 * end, one by one and into every eighth), against the single read and the values written (the 12-bit range's ends
 * among them) */
int pk_pack_check(void)
{
    uint8_t b[96];
    int32_t s[64], d[48], d8[8u * 40u];
    uint32_t i, p, n, k, x = 12345u, bad = 0;
    for (i = 0; i < 64u; i++) {
        x = x * 1103515245u + 12345u;
        s[i] = (int32_t)((x >> 16) & 4095u) - 2048;
    }
    s[0] = 2047, s[1] = -2048, s[2] = -1, s[3] = 0, s[4] = 1, s[5] = -2047;
    for (i = 0; i < 64u; i += 16u)
        xr_pack(XR_PB(b, i), s + i, 16u);
    for (i = 0; i < 64u; i++)
        bad += xr_get(b, i) != s[i] * 16;
    for (p = 0; p < 64u; p++)
        for (n = 1; n <= 40u; n++) {
            xr_unpack(d, 1u, b, p, 63u, n);
            xr_unpack(d8, 8u, b, p, 63u, n);        /* (the still lines' form: into every eighth) */
            for (k = 0; k < n; k++)
                bad += (d[k] != s[(p + k) & 63u] * 16) + (d8[8u * k] != s[(p + k) & 63u] * 16);
        }
    return (int)bad;
}
#else
#include "quad/quad_voice.h"

void pk_qd_fx_init(void);
void pk_qd_fx_set(int id, int v);
void pk_qd_fx_master_pot(int pot);
void pk_qd_fx_block(const int32_t *sc, const int32_t *sd, const int32_t *sr, int32_t *l, int32_t *r, int32_t *out);
uint32_t pk_r_live(void);
int pk_pack_check(void);

/* the bounds (2026-10-27's runs, printed: the difference -71 .. -65 dBFS, 33.6 .. 40.7 dB under a loud response; Inf
 * held within 0.4 dB), with margin. The packed lines' error is a floor, not a share: 12 bits keep -72 dB of the
 * line's range on every pass, about 12 units RMS at the output (MASTER full) whatever the level, so a quiet response
 * (an impulse: -52 dBFS) is only 12 .. 16 dB over it; a loud one (the burst: -31 dBFS) 34 .. 41 */
#define FXP_ABS_DBFS (-62.0)           /* (1) the difference's RMS over the first second, dBFS, every case */
#define FXP_REL_DB (-30.0)             /* .. under the response's RMS, the loud bursts that decay */
#define FXP_INF_DB 1.5                 /* (2) DECAY Inf: the packed tail's 5 s windows against the 16-bit one's */

typedef struct {
    const char *name;
    void (*init)(void);
    void (*set)(int, int);
    void (*pot)(int);
    void (*block)(const int32_t *, const int32_t *, const int32_t *, int32_t *, int32_t *, int32_t *);
    uint32_t (*live)(void);
} fxp_api_t;
static uint32_t r_live16(void) { return xf.r_live; }
static const fxp_api_t A16 = {"16-bit", qd_fx_init, qd_fx_set, qd_fx_master_pot, qd_fx_block, r_live16};
static const fxp_api_t APK = {"packed", pk_qd_fx_init, pk_qd_fx_set, pk_qd_fx_master_pot, pk_qd_fx_block, pk_r_live};

typedef struct {
    const char *name;
    int id, v;                         /* one parameter away from the defaults (id < 0: none) */
    int burst;                         /* 0 an impulse (32767), else a 100 ms noise burst of +-this */
    int move;                          /* SIZE and PRE moved mid-burst (the gliding reads) */
    uint32_t cap_s;                    /* the longest run, seconds */
} fxp_case_t;

#define FS_ 44100u
#define MAXW 64u                       /* 5-second windows' energy */
typedef struct {
    int32_t *first;                    /* the first second, stereo */
    uint32_t n;                        /* samples run */
    uint32_t live0;                    /* the sample where r_live reached 0 after the input (0: never) */
    uint32_t last_nz;                  /* the last non-zero output sample (+1; 0: none) */
    double win[MAXW];
    uint32_t nwin;
    uint64_t hash;
} fxp_res_t;

static void fxp_run(const fxp_api_t *a, const fxp_case_t *c, fxp_res_t *res)
{
    static int32_t sc[CTL], sd[CTL], sr[CTL], l[CTL], r[CTL], out[2 * CTL];
    const uint32_t n_in = c->burst ? FS_ / 10u : 1u, cap = c->cap_s * FS_;
    uint32_t t, k, x = 1u;
    a->init();
    a->pot(1023);
    if (c->id >= 0)
        a->set(c->id, c->v);
    memset(res->win, 0, sizeof res->win);
    res->live0 = res->last_nz = res->nwin = 0;
    res->hash = 1469598103934665603ull;
    for (t = 0; t < cap; t += CTL) {
        if (c->move && t == FS_ / 20u / CTL * CTL) {
            a->set(QD_FXP_RSIZE, 20);
            a->set(QD_FXP_RPRE, 60);
        }
        for (k = 0; k < CTL; k++) {
            sc[k] = sd[k] = l[k] = r[k] = 0;
            x = x * 1664525u + 1013904223u;
            sr[k] = t + k >= n_in ? 0 : c->burst ? (int32_t)(x >> 16) % c->burst : 32767;
        }
        a->block(sc, sd, sr, l, r, out);
        for (k = 0; k < 2u * CTL; k++) {
            const int32_t v = out[k];
            const uint32_t i = t + k / 2u;
            if (i < FS_)
                res->first[2u * i + (k & 1u)] = v;
            if (i / (5u * FS_) < MAXW)
                res->win[i / (5u * FS_)] += (double)v * v;
            res->hash = (res->hash ^ (uint32_t)v) * 1099511628211ull;
            if (v)
                res->last_nz = i + 1u;
        }
        if (t + CTL >= n_in && !res->live0 && !a->live())
            res->live0 = t + CTL;
        if (res->live0 && t + CTL >= res->live0 + FS_ && t + CTL >= FS_)   /* a second past the end, and the first */
            break;
    }
    res->n = t < cap ? t + CTL : cap;
    res->nwin = (res->n + 5u * FS_ - 1u) / (5u * FS_);
}

static double fxp_db(double a, double b) { return 10.0 * log10((a + 1e-30) / (b + 1e-30)); }

int main(void)
{
    enum { BURST = 20000, END = 0, HOLD = 1 };
    static const struct { fxp_case_t c; int kind; } C[] = {
        {{"defaults, impulse", -1, 0, 0, 0, 120}, END}, {{"defaults, burst", -1, 0, BURST, 0, 120}, END},
        {{"SIZE 0", QD_FXP_RSIZE, 0, BURST, 0, 120}, END}, {{"SIZE 127", QD_FXP_RSIZE, 127, BURST, 0, 120}, END},
        {{"DECAY 0", QD_FXP_RDECAY, 0, BURST, 0, 60}, END}, {{"DECAY 64", QD_FXP_RDECAY, 64, BURST, 0, 120}, END},
        {{"DECAY 126", QD_FXP_RDECAY, 126, BURST, 0, 400}, END},
        {{"DECAY 126, impulse", QD_FXP_RDECAY, 126, 0, 0, 400}, END},
        {{"DAMP 0", QD_FXP_RDAMP, 0, BURST, 0, 120}, END}, {{"DAMP 127", QD_FXP_RDAMP, 127, BURST, 0, 120}, END},
        {{"PRE 0", QD_FXP_RPRE, 0, BURST, 0, 120}, END}, {{"PRE 127", QD_FXP_RPRE, 127, BURST, 0, 120}, END},
        {{"MOD 0", QD_FXP_RMOD, 0, BURST, 0, 120}, END}, {{"MOD 127", QD_FXP_RMOD, 127, BURST, 0, 120}, END},
        {{"SIZE + PRE moved", -1, 0, BURST, 1, 120}, END},
        {{"Inf", QD_FXP_RDECAY, 127, BURST, 0, 60}, HOLD},
        {{"Inf, -20 dB", QD_FXP_RDECAY, 127, BURST / 10, 0, 300}, END},
        {{"Inf, -40 dB", QD_FXP_RDECAY, 127, BURST / 100, 0, 300}, END},
        {{"Inf, impulse", QD_FXP_RDECAY, 127, 0, 0, 300}, END}};
    static int32_t f16[2u * FS_], fpk[2u * FS_], fpk2[2u * FS_];
    const double fs2 = 32768.0 * 32768.0;
    fxp_res_t a, b, b2;
    uint32_t ci, i, w, fails = 0;
    double worst_abs = -1e9, worst_rel = -1e9;
    int bad = pk_pack_check();
    printf("test_fx_packed: the packing, every start and length across the ring's end: %s\n", bad ? "FAIL" : "ok");
    fails += bad != 0;
    a.first = f16, b.first = fpk, b2.first = fpk2;
    for (ci = 0; ci < sizeof C / sizeof C[0]; ci++) {
        const fxp_case_t *c = &C[ci].c;
        double e16 = 0, ed = 0, adb, rdb;
        int ok = 1;
        fxp_run(&A16, c, &a);
        fxp_run(&APK, c, &b);
        for (i = 0; i < 2u * FS_; i++) {
            const double d = (double)fpk[i] - f16[i];
            e16 += (double)f16[i] * f16[i];
            ed += d * d;
        }
        adb = fxp_db(ed / (2.0 * FS_), fs2);
        rdb = fxp_db(ed, e16);
        printf("  %-18s 1 s: RMS %6.1f, difference %5.2f (%5.1f dBFS, %5.1f dB)", c->name, sqrt(e16 / (2.0 * FS_)),
               sqrt(ed / (2.0 * FS_)), adb, rdb);
        /* (1) close: the difference under FXP_ABS_DBFS everywhere; under the loud response by FXP_REL_DB */
        worst_abs = adb > worst_abs ? adb : worst_abs;
        if (adb > FXP_ABS_DBFS)
            ok = 0, printf(" [over %.0f dBFS]", FXP_ABS_DBFS);
        if (c->burst == BURST && C[ci].kind == END) {
            worst_rel = rdb > worst_rel ? rdb : worst_rel;
            if (rdb > FXP_REL_DB)
                ok = 0, printf(" [over %.0f dB]", FXP_REL_DB);
        }
        if (C[ci].kind == END) {                        /* (2) the end: r_live 0, nothing after (a second more run) */
            printf("; ends 16-bit %.2f s, packed %.2f s (its last sample %.2f s)", a.live0 / (double)FS_,
                   b.live0 / (double)FS_, b.last_nz / (double)FS_);
            if (!b.live0 || b.last_nz > b.live0 || b.n < b.live0 + FS_)
                ok = 0, printf(" [the packed tail never ends]");
            else if (!a.live0 || b.live0 > a.live0 + FS_ / 4u)
                ok = 0, printf(" [the packed tail outlasts the 16-bit one]");
        } else {                                        /* .. Inf: held as the 16-bit one holds it */
            double up = -1e9, off = 0;
            for (w = 1; w + 1u < b.nwin; w++) {
                const double s = fxp_db(b.win[w + 1u], b.win[w]), o = fabs(fxp_db(b.win[w], a.win[w]));
                up = s > up ? s : up;
                off = o > off ? o : off;
            }
            printf("; held %u s: 16-bit %.1f dB, packed %.1f dB from 5 s on (a window's step at most %+.2f dB, %.2f dB"
                   " off the 16-bit)", b.n / FS_, fxp_db(a.win[a.nwin - 1u], a.win[1]),
                   fxp_db(b.win[b.nwin - 1u], b.win[1]), up, off);
            if (b.live0 || up > 0.0 || off > FXP_INF_DB)
                ok = 0, printf(" [Inf not held as the 16-bit one]");
        }
        if (ci == 0u || ci == 1u || ci == 14u) {        /* (3) two runs, the same samples */
            fxp_run(&APK, c, &b2);
            if (b2.hash != b.hash || memcmp(fpk, fpk2, sizeof fpk) || b2.live0 != b.live0 || b2.n != b.n)
                ok = 0, printf(" [not deterministic]");
            else
                printf("; twice the same");
        }
        printf("%s\n", ok ? "" : "  FAIL");
        fails += !ok;
    }
    printf("test_fx_packed: the difference at most %.1f dBFS (bound %.0f), %.1f dB under a loud response (bound %.0f)\n",
           worst_abs, FXP_ABS_DBFS, -worst_rel, -FXP_REL_DB);
    if (fails) {
        printf("test_fx_packed: %u FAILED\n", fails);
        return 1;
    }
    printf("test_fx_packed: all passed\n");
    return 0;
}
#endif
