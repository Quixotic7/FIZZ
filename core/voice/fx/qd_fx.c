/* SPDX-License-Identifier: GPL-3.0-only */
/* ChoralRoot's effects and output chain as a library (qd_fx.h): the copies of fx.c (its parts), cr_fx.c and master.c
 * (v0.25.1) built in this one unit as ChoralRoot's firmware builds them (CR_FX 1: ChoralRoot's delay, reverb and
 * stereo chorus; one core: CR_CPU1 0; no profiling), on the tables, core.h parts and dsp.c of core/voice/quad. Then
 * the block in mix_block's order: xc_block, xf_block, xq_block, out_master (Output Clean). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "qd_fx.h"

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
#define NPART 4                         /* (fx.c's fx_sg: ChoralRoot's parts' send ramps; QUADRILLE ramps a voice's,
                                         * core/voice/mix.c) */
#define CR_PROFX(k) ((void)0)           /* (cr_cpu1.h's profiling: none) */
#define CR_PROFX_START() ((void)0)
#include "cr_tables.h"
#include "cr_core.h"
#include "cr_ram.h"                     /* (ChoralRoot's core.h includes it) */
#include "dsp.c"
#include "fx.c"
#include "master.c"
#include "cr_fx.c"
#undef __attribute__
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define QD_FX_POT_DEFAULT 724           /* master_of_pot(724) = 2047: Felucca's power-on MASTER (master.c's note) */

typedef char qd_fx_ctl_ok[QD_FX_CTL == CTL ? 1 : -1];

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
    xc_live = 0;
    lim_env = LIM_T;
    lim_g = 32768;
    dc_l = dc_r = dce_l = dce_r = 0;
    fx_off = 0;
    fx_smooth = 1;                      /* (cr_out.c: ChoralRoot's; Classic's limiter only) */
    fx_lowcut = 0;                      /* (cr_settings.c: ChoralRoot holds LOWCUT / BASS+ off) */
    fx_usb_fixed = 0;
    /* ChoralRoot's defaults: the delay's and the reverb's (cr_fx.c XFP), the chorus's rate and depth (params.c GP:
     * G_CRATE 40, G_CDEPTH 60), the tempo */
    xf_defaults();
    xf_bpm = 120;
    song.g[G_BPM] = 120;
    song.g[G_CRATE] = 40;
    song.g[G_CDEPTH] = 60;
    fx_master_now(master_of_pot(QD_FX_POT_DEFAULT));
    fx_output_set(1u, 1);               /* Options > Output Clean (cr_ui.c: ChoralRoot's default), before the first
                                         * block: no fade */
}

void qd_fx_tempo(int bpm)
{
    bpm = bpm < 20 ? 20 : bpm > 300 ? 300 : bpm;
    xf_bpm = (uint16_t)bpm;
    song.g[G_BPM] = (int16_t)bpm;
}

void qd_fx_master_pot(int pot)
{
    pot = pot < 0 ? 0 : pot > 1023 ? 1023 : pot;
    song.master_q12 = master_of_pot((uint32_t)pot);     /* (master.c master_poll: Clean eases it over the block) */
}

uint32_t qd_fx_master_q12(void) { return song.master_q12; }

/* fx.c mix_block's stages after the parts: the chorus's two taps into the mix, the delay and the reverb into the mix,
 * the master filter, the output chain */
void qd_fx_block(const int32_t *sc, const int32_t *sd, const int32_t *sr, int32_t *l, int32_t *r, int32_t *out)
{
    xc_block(sc, l, r, CTL);
    xf_block(sd, sr, l, r, CTL);
    xq_block(l, r, CTL);
    out_master(out, l, r, (const int32_t *)0, CTL);
}

/* ---------------------------------------------------------------- P1.4b: the parameters */
typedef char qd_fxp_ok[QD_FXP_DTIME == XP_DTIME && QD_FXP_RSIZE == XP_RSIZE && QD_FXP_CWIDTH == XP_CWIDTH &&
                       QD_FXP_FON == XP_FON && QD_FXP_CRATE == XP_N ? 1 : -1];

int qd_fx_get(int id)
{
    if (id >= 0 && id < XP_N)
        return xfp[id];
    return id == QD_FXP_CRATE ? song.g[G_CRATE] : id == QD_FXP_CDEPTH ? song.g[G_CDEPTH] : 0;
}

void qd_fx_range(int id, int *min, int *max, int *def)
{
    int lo = 0, hi = 127, d = 0;
    if (id >= 0 && id < XP_N)
        lo = XFP[id].min, hi = XFP[id].max, d = XFP[id].def;
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
    case QD_FXP_CDEPTH: snprintf(buf, 16, "%d%%", (v * 100 + 63) / 127); break;
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
