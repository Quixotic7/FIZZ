/* SPDX-License-Identifier: GPL-3.0-only */
/* ChoralRoot's effects and output chain as a library (qd_fx.h): the copies of fx.c (its parts), cr_fx.c and master.c
 * (v0.25.1) built in this one unit as ChoralRoot's firmware builds them (CR_FX 1: ChoralRoot's delay, reverb and
 * stereo chorus; one core: CR_CPU1 0; no profiling), on the tables, core.h parts and dsp.c of core/voice/quad. Then
 * the block in mix_block's order: xc_block, xf_block, xq_block, out_master (Output Clean). */
#include <stdint.h>
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
