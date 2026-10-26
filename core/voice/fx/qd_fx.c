/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* ChoralRoot's effects and output chain as a library (qd_fx.h): fz_out.c (Output Clean, the chorus line) and the
 * owner's cr_fx.c (v0.25.1: delay, reverb, master filter, stereo chorus; one core; no profiling) built in this one
 * unit on core/voice/quad's fz_core.h, fz_voice.h and fz_tables.h (L1). Then the block in ChoralRoot's order:
 * xc_block, xf_block, xq_block, out_master (Output Clean). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "qd_fx.h"
#include "../../seq/qd_div.h"                  /* (P2.4) */

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpedantic"
#pragma clang diagnostic ignored "-Wc11-extensions"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-const-variable"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-macros"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#define __attribute__(x)                /* (ChoralRoot's .pool sections, its inlining hints: the host's choice, as
                                         * ChoralRoot's hostsim.c builds them) */
#define CR_FX 1                         /* ChoralRoot's delay, reverb and stereo chorus (cr_fx.c) */
#define CR_CPU1 0                       /* one core */
#define CR_PROFX(k) ((void)0)           /* (cr_cpu1.h's profiling: none) */
#define CR_PROFX_START() ((void)0)
#include "fz_tables.h"
#include "fz_core.h"
#include "cr_ram.h"                     /* (the CR_HOT_* markers: nothing on the host) */
#include "fz_voice.h"                   /* (fz_dsp.h, fz_chorus_block) */
#include "fz_out.c"
#include "cr_fx.c"
#undef __attribute__
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define QD_FX_POT_DEFAULT 724           /* fz_master_of_pot(724) = 2047: the power-on MASTER, half way */

typedef char qd_fx_ctl_ok[QD_FX_CTL == CTL ? 1 : -1];

/* THE ROUTING (2026-10-26, after ChoralRoot 0.27's, GitHub issue 21 there): qd_rt[] = the mode, the order, the three
 * chain amounts (QD_FXP_RMODE ..). PARALLEL (0, the default) is the block as it always was, bit for bit: the chorus's,
 * the delay's and the reverb's buses fed by the tracks' sends, their returns into the mix, the master filter after
 * (or, the FILTER first in the order, before: on the dry mix, so the effects hear the cut signal). CHAIN (1, the owner's
 * rule of 2026-10-26, the evening's): the effects in series, each track's sends its wet / dry at each effect (mix.c
 * chain_gains: send k's share of what the track feeds the chain goes into effect k, the rest passes it by; what passes
 * them all by is heard dry in l / r, with the track's DRY part). Here: the wet W starts empty; effect k takes its bus
 * and its share qd_cg[k] of W (the tracks' sends, level-weighted: app.c), leaves the rest of W and adds its wet; the
 * FILTER filters W at its place, or, last, everything (l / r folded in first: the master filter). Sequential within
 * the block (no previous-block snapshot: one core). The delay's half-rate wet has its own up-sampler (qd_ud) so the
 * reverb's (xf.up) keeps its history */
static int16_t qd_rt[5];
static int32_t qd_cg[3] = {4096, 4096, 4096};           /* (CHAIN: each effect's share of the wet before it) */
void qd_fx_chain_g(const int32_t g[3]) { qd_cg[0] = g[0], qd_cg[1] = g[1], qd_cg[2] = g[2]; }
static const uint8_t QD_RT_ORD[24][4] = {              /* QD_FXP_ORDER -> the block at position 0..3, lexicographic */
    {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 1, 2}, {0, 3, 2, 1},
    {1, 0, 2, 3}, {1, 0, 3, 2}, {1, 2, 0, 3}, {1, 2, 3, 0}, {1, 3, 0, 2}, {1, 3, 2, 0},
    {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 1, 0, 3}, {2, 1, 3, 0}, {2, 3, 0, 1}, {2, 3, 1, 0},
    {3, 0, 1, 2}, {3, 0, 2, 1}, {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}};
static xf_hb_t qd_ud[2];                                /* CHAIN: the delay's own up-sampler, left and right */
static uint32_t qd_ud_live;
static const uint8_t *qd_rt_ord(void) { return QD_RT_ORD[qd_rt[1] >= 0 && qd_rt[1] < 24 ? qd_rt[1] : 0]; }

void qd_fx_init(void)
{
    /* everything at rest (a second init, as a test's, starts from where power-on would) */
    memset(&xf, 0, sizeof xf);
    memset(&xq, 0, sizeof xq);
    memset(&xc, 0, sizeof xc);
    memset(&fx, 0, sizeof fx);
    memset(xd_buf, 0, sizeof xd_buf);
    memset(xr_buf, 0, sizeof xr_buf);
    memset(xr_pbuf, 0, sizeof xr_pbuf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(fx_dc, 0, sizeof fx_dc);
    fx_off = 0;
    /* ChoralRoot's defaults: the delay's and the reverb's (cr_fx.c XFP), the chorus's rate and depth (params.c GP:
     * G_CRATE 40, G_CDEPTH 60), the tempo */
    xf_defaults();
    xf_bpm = 120;
    song.g[G_BPM] = 120;
    song.g[G_CRATE] = 40;
    song.g[G_CDEPTH] = 60;
    fx_master_now(fz_master_of_pot(QD_FX_POT_DEFAULT));
    qd_rt[0] = 0, qd_rt[1] = 0, qd_rt[2] = 127, qd_rt[3] = qd_rt[4] = 64;   /* PARALLEL, C D R F, DRY full */
    xf_hb_clear(&qd_ud[0]);
    xf_hb_clear(&qd_ud[1]);
    qd_ud_live = 0;
    fx_output_set(1u, 1);               /* Options > Output Clean (cr_ui.c: ChoralRoot's default), before the first
                                         * block: no fade */
}

void qd_fx_tempo(int bpm)
{
    bpm = bpm < 20 ? 20 : bpm > 300 ? 300 : bpm;
    xf_bpm = (uint16_t)bpm;
    song.g[G_BPM] = (int16_t)bpm;
}

void qd_fx_tempo_fx(int bpm)            /* P2.6g: the effects' tempo alone (the voices' song.g untouched) */
{
    bpm = bpm < 20 ? 20 : bpm > 300 ? 300 : bpm;
    xf_bpm = (uint16_t)bpm;
}

void qd_fx_master_pot(int pot)
{
    pot = pot < 0 ? 0 : pot > 1023 ? 1023 : pot;
    song.master_q12 = fz_master_of_pot((uint32_t)pot);  /* (the chain eases it over the block) */
}

uint32_t qd_fx_master_q12(void) { return song.master_q12; }

typedef char qd_rt_half_ok[XF_HALF && CR_FX_DELAY_DS == 2 && CR_FX_REVERB_DS == 2 ? 1 : -1];   /* (the delay's wet
                                                         * is the half-rate xf.t[2..3], up-sampled here) */

/* CHAIN: the delay alone, from `in`, its wet added into l / r (its own up-sampler) */
static void qd_chain_delay(const int32_t *in, int32_t *l, int32_t *r)
{
    static const int32_t none[CTL];
    const uint32_t f = xf_prep(in, none, CTL), nh = CTL >> 1;
    uint32_t i;
    int32_t any = 0;
    if (!(f & 1u) && !qd_ud_live)
        return;
    for (i = 0; i < nh; i++)
        xf.t[2][i] = xf.t[3][i] = 0;
    if (f & 1u)
        xf_delay(in, l, r, CTL);
    for (i = 0; i < nh; i++)
        any |= xf.t[2][i] | xf.t[3][i];
    if (any)
        qd_ud_live = 256u;
    else if (qd_ud_live)
        qd_ud_live = qd_ud_live > nh ? qd_ud_live - nh : 0u;
    if (any || qd_ud_live) {
        XF_CALL(xf_up)(&qd_ud[0], xf.t[2], l, nh);
        XF_CALL(xf_up)(&qd_ud[1], xf.t[3], r, nh);
    } else {
        xf_hb_clear(&qd_ud[0]);
        xf_hb_clear(&qd_ud[1]);
    }
}
/* CHAIN: the reverb alone, from `in`, its wet added into l / r (xf.up, as in PARALLEL) */
static void qd_chain_reverb(const int32_t *in, int32_t *l, int32_t *r)
{
    static const int32_t none[CTL];
    const uint32_t f = xf_prep(none, in, CTL);
    uint32_t i;
    if (!(f & 6u))
        return;
    for (i = 0; i < (CTL >> 1); i++)
        xf.t[2][i] = xf.t[3][i] = 0;
    xf_reverb(in, l, r, CTL, (int)(f >> 1 & 1u), 0, 0, xf.t[2], xf.t[3], 1);
}

/* ChoralRoot's mix_block stages after the parts: PARALLEL the chorus's two taps into the mix, the delay and the reverb
 * into the mix, the master filter (first in the order: before them), the output chain; CHAIN the four in the order */
void qd_fx_block(const int32_t *sc, const int32_t *sd, const int32_t *sr, int32_t *l, int32_t *r, int32_t *out)
{
    const uint8_t *o = qd_rt_ord();
    int k, i;
    if (qd_rt[0] != 1) {                                /* PARALLEL */
        if (o[0] == QD_RT_FILTER)
            xq_block(l, r, CTL);
        xc_block(sc, l, r, CTL);
        xf_block(sd, sr, l, r, CTL);
        if (o[0] != QD_RT_FILTER)
            xq_block(l, r, CTL);
        out_master(out, l, r, (const int32_t *)0, CTL);
        return;
    }
    {                                                   /* CHAIN */
        int32_t wl[CTL], wr[CTL], in[CTL];
        int folded = 0;
        for (i = 0; i < CTL; i++)
            wl[i] = wr[i] = 0;
        for (k = 0; k < 4; k++) {
            const int b = o[k];
            const int32_t *bus = b == QD_RT_CHORUS ? sc : b == QD_RT_DELAY ? sd : sr;
            const int32_t g = b >= 0 && b < 3 ? qd_cg[b] : 0;
            if (b == QD_RT_FILTER) {
                if (k == 3) {                           /* last: the master filter on everything */
                    for (i = 0; i < CTL; i++)
                        l[i] += wl[i], r[i] += wr[i];
                    folded = 1;
                    xq_block(l, r, CTL);
                } else {
                    xq_block(wl, wr, CTL);
                }
                continue;
            }
            for (i = 0; i < CTL; i++) {                 /* its bus and its share of the wet so far; the rest stays
                                                         * (the gains Q12 on samples >> 12 first: 32-bit) */
                in[i] = bus[i] + ((wl[i] + wr[i]) / 2 >> 12) * g;
                wl[i] = (wl[i] >> 12) * (4096 - g);
                wr[i] = (wr[i] >> 12) * (4096 - g);
            }
            if (b == QD_RT_CHORUS)
                xc_block(in, wl, wr, CTL);
            else if (b == QD_RT_DELAY)
                qd_chain_delay(in, wl, wr);
            else
                qd_chain_reverb(in, wl, wr);
        }
        if (!folded)
            for (i = 0; i < CTL; i++)
                l[i] += wl[i], r[i] += wr[i];
    }
    out_master(out, l, r, (const int32_t *)0, CTL);
}

int qd_fx_route_block(int pos) { return qd_rt_ord()[pos & 3]; }
int qd_fx_route_move(int block, int dir)
{
    uint8_t o[4];
    int i, j, p = 0, q;
    for (i = 0; i < 4; i++) {
        o[i] = qd_rt_ord()[i];
        if (o[i] == block)
            p = i;
    }
    if (qd_rt[0] != 1 && block != QD_RT_FILTER)         /* (PARALLEL: the effects' order is not heard) */
        return 0;
    q = qd_rt[0] != 1 ? (dir < 0 ? 0 : p ? p : 3) : p + (dir < 0 ? -1 : 1);   /* (PARALLEL: already after stays) */
    q = q < 0 ? 0 : q > 3 ? 3 : q;
    if (q == p)
        return 0;
    for (; p < q; p++)
        o[p] = o[p + 1];
    for (; p > q; p--)
        o[p] = o[p - 1];
    o[q] = (uint8_t)block;
    for (i = 0; i < 24; i++) {
        for (j = 0; j < 4 && QD_RT_ORD[i][j] == o[j]; j++)
            ;
        if (j == 4) {
            qd_rt[1] = (int16_t)i;
            return 1;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- P1.4b: the parameters */
typedef char qd_fxp_ok[QD_FXP_DTIME == XP_DTIME && QD_FXP_RSIZE == XP_RSIZE && QD_FXP_CWIDTH == XP_CWIDTH &&
                       QD_FXP_FON == XP_FON && QD_FXP_CRATE == XP_N ? 1 : -1];

int qd_fx_get(int id)
{
    if (id >= 0 && id < XP_N)
        return xfp[id];
    if (id >= QD_FXP_RMODE && id <= QD_FXP_RSPARE2)
        return qd_rt[id - QD_FXP_RMODE];
    return id == QD_FXP_CRATE ? song.g[G_CRATE] : id == QD_FXP_CDEPTH ? song.g[G_CDEPTH] : 0;
}

void qd_fx_range(int id, int *min, int *max, int *def)
{
    int lo = 0, hi = 127, d = 0;
    if (id >= 0 && id < XP_N)
        lo = XFP[id].min, hi = XFP[id].max, d = XFP[id].def;
    else if (id >= QD_FXP_RMODE && id <= QD_FXP_RSPARE2)   /* the routing: PARALLEL / CHAIN, the 24 orders, DRY */
        hi = id == QD_FXP_RMODE ? 1 : id == QD_FXP_ORDER ? 23 : 127, d = id == QD_FXP_RDRY ? 127 : id > QD_FXP_RDRY ? 64 : 0;
    else
        d = id == QD_FXP_CRATE ? 40 : 60;               /* (params.c GP) */
    if (min)
        *min = lo;
    if (max)
        *max = hi;
    if (def)
        *def = d;
}

void qd_fx_set(int id, int v)
{
    int lo, hi;
    if (id < 0 || id >= QD_NFXP)
        return;
    qd_fx_range(id, &lo, &hi, 0);
    v = v < lo ? lo : v > hi ? hi : v;
    if (id < XP_N)
        xfp[id] = (int16_t)v;
    else if (id >= QD_FXP_RMODE)
        qd_rt[id - QD_FXP_RMODE] = (int16_t)v;
    else
        song.g[id == QD_FXP_CRATE ? G_CRATE : G_CDEPTH] = (int16_t)v;
}

static const char *fx_hz(char *buf, uint32_t hz)
{
    if (hz < 1000u)
        snprintf(buf, 16, "%u Hz", (unsigned)hz);
    else
        snprintf(buf, 16, "%u.%u kHz", (unsigned)(hz / 1000u), (unsigned)(hz % 1000u / 100u));
    return buf;
}

int qd_fx_chorus_hz100(void)
{
    return (int)(((uint64_t)LFO_INC[song.g[G_CRATE] & 127] * 275625u) >> 33);   /* (a block's step, 44100 / 32 blocks a
                                                         * second; x 100: 137812.5) */
}

const char *qd_fx_text(int id, int v, char *buf)
{
    static const char *const DIV[XF_NDIV] = {"1/32", "1/16T", "1/32.", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.",
                                             "1/4", "1/2T", "1/4.", "1/2"};   /* (cr_ui.c CU_XDIV) */
    static const char *const RATIO[4] = {"1:1", "3:4", "2:3", "1:2"};
    buf[0] = 0;
    switch (id) {
    case QD_FXP_DTIME: snprintf(buf, 16, "%u ms", (unsigned)xf_ms((uint32_t)v)); break;
    case QD_FXP_DDIV: snprintf(buf, 16, "%s", DIV[(unsigned)v % XF_NDIV]); break;
    case QD_FXP_DSPREAD: {
        const int x = ((254 - v) * 100 + 127) / 254;
        snprintf(buf, 16, "x%d.%02d", x / 100, x % 100);
        break;
    }
    case QD_FXP_DRATIO: snprintf(buf, 16, "%s", RATIO[v & 3]); break;
    case QD_FXP_DSYNC: strcpy(buf, v ? "TEMPO" : "FREE"); break;
    case QD_FXP_DMODE: strcpy(buf, v ? "PING" : "STEREO"); break;   /* (ChoralRoot's "Ping-pong": a cell's width) */
    case QD_FXP_RDECAY:
        if (v >= 127) {
            strcpy(buf, "INF");
        } else {
            const uint32_t ms = xf_rt_ms((uint32_t)v);
            if (ms < 10000u)
                snprintf(buf, 16, "%u.%u s", (unsigned)(ms / 1000u), (unsigned)(ms % 1000u / 100u));
            else
                snprintf(buf, 16, "%u s", (unsigned)(ms / 1000u));
        }
        break;
    case QD_FXP_RPRE: snprintf(buf, 16, "%u ms", (unsigned)xf_pre_ms((uint32_t)v)); break;
    case QD_FXP_RLOW: fx_hz(buf, xf_lowcut_hz((uint32_t)v)); break;
    case QD_FXP_FLOW:
        if (v <= 0)
            strcpy(buf, "OFF");
        else
            fx_hz(buf, xq_low_hz((uint32_t)v));
        break;
    case QD_FXP_FHIGH:
        if (v >= 127)
            strcpy(buf, "OFF");
        else
            fx_hz(buf, xq_high_hz((uint32_t)v));
        break;
    case QD_FXP_FSLOPE: strcpy(buf, v ? "12 dB" : "6 dB"); break;
    case QD_FXP_FON: strcpy(buf, v ? "ON" : "OFF"); break;
    case QD_FXP_CRATE: {
        const uint32_t h = (uint32_t)(((uint64_t)LFO_INC[v & 127] * 275625u) >> 33);
        snprintf(buf, 16, "%u.%02u Hz", (unsigned)(h / 100u), (unsigned)(h % 100u));
        break;
    }
    case QD_FXP_CDEPTH:
    case QD_FXP_RDRY:
    case QD_FXP_RSPARE1:
    case QD_FXP_RSPARE2: snprintf(buf, 16, "%d%%", (v * 100 + 63) / 127); break;
    case QD_FXP_RMODE: strcpy(buf, v ? "CHAIN" : "PARALLEL"); break;
    case QD_FXP_ORDER: {                                /* the chain's letters, C D R F */
        int k;
        for (k = 0; k < 4; k++)
            buf[2 * k] = "CDRF"[QD_RT_ORD[v >= 0 && v < 24 ? v : 0][k]], buf[2 * k + 1] = ' ';
        buf[7] = 0;
        break;
    }
    default: snprintf(buf, 16, "%d", v); break;
    }
    return buf;
}

int qd_fx_delay_ms(void)
{
    if (xfp[XP_DSYNC]) {
        const uint32_t bpm = xf_bpm ? xf_bpm : 120u;
        return (int)(60000u * XF_DIV[(uint32_t)xfp[XP_DDIV] % XF_NDIV] / (48u * bpm));
    }
    return (int)xf_ms((uint32_t)xfp[XP_DTIME]);
}

int qd_fx_reverb_ms(void) { return xfp[XP_RDECAY] >= 127 ? -1 : (int)xf_rt_ms((uint32_t)xfp[XP_RDECAY]); }
