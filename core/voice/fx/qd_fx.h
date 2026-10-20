/* SPDX-License-Identifier: GPL-3.0-only */
/* ChoralRoot's effects and output chain as a library (qd_fx.c over the copies of fx.c, cr_fx.c, master.c from
 * v0.25.1): the stereo chorus (xc_block), the delay at half rate tempo-synced and the reverb (xf_block), the master
 * filter (xq_block: Off, as ChoralRoot's default), then Options > Output Clean (out_master: the trim, the DC blocker,
 * the look-ahead limiter, the knee, MASTER). DIST / DRIVE is not in P1.1.
 *
 * One instance (ChoralRoot's state is file-static: one per loaded core). One control block (32 frames) a call, in the
 * order ChoralRoot's fx.c mix_block runs them. Integer arithmetic: two runs give the same samples. Nothing calls the
 * OS. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QD_FX_CTL 32

/* power-on: ChoralRoot's defaults (the delay's and the reverb's xf_defaults, the chorus's rate 40 and depth 60
 * (params.c), Output Clean, the limiter at rest, 120 BPM, MASTER at pot 724: Felucca's 2048 of 4096) */
void qd_fx_init(void);
void qd_fx_tempo(int bpm);              /* the tempo the synced delay follows (cr_fx.c xf_bpm, 20..300) */
void qd_fx_master_pot(int pot);         /* MASTER, the pot 0..1023 (master.c's law: pot^2 / 256, Q12) */
uint32_t qd_fx_master_q12(void);
/* P1.4b: the effects' own parameters (global: no sound or pattern load touches them), ChoralRoot's xfp[] (cr_fx.c XP_*,
 * in its order: the delay's nine, the reverb's seven, the chorus's width, the master filter's four) and the chorus's
 * rate and depth (song.g G_CRATE, G_CDEPTH). A change takes effect at the next block (cr_fx.c's own hashes) */
enum { QD_FXP_DTIME, QD_FXP_DDIV, QD_FXP_DSPREAD, QD_FXP_DRATIO, QD_FXP_DFDBK, QD_FXP_DSYNC, QD_FXP_DMODE,
       QD_FXP_DTONE, QD_FXP_DWOW, QD_FXP_RSIZE, QD_FXP_RDAMP, QD_FXP_RDECAY, QD_FXP_RPRE, QD_FXP_RMOD, QD_FXP_RLOW,
       QD_FXP_RWIDTH, QD_FXP_CWIDTH, QD_FXP_FLOW, QD_FXP_FHIGH, QD_FXP_FSLOPE, QD_FXP_FON,
       QD_FXP_CRATE, QD_FXP_CDEPTH, QD_NFXP };
int qd_fx_get(int id);
void qd_fx_set(int id, int v);          /* clamped to its range */
void qd_fx_range(int id, int *min, int *max, int *def);
/* its value as ChoralRoot's FX pages read it ("250 ms", "1/8.", "x0.75", "3:4", "PING" (ping-pong), "2.4 s", "130 Hz",
 * "0.80 Hz", "OFF", a number); buf 16 bytes */
const char *qd_fx_text(int id, int v, char *buf);
int qd_fx_delay_ms(void);               /* the delay's left time now (free or synced at the tempo) */
int qd_fx_chorus_hz100(void);           /* the chorus's rate in hundredths of a Hz */
int qd_fx_reverb_ms(void);              /* the reverb's RT60 (-1: INF) */

/* a block: sc, sd, sr the chorus, delay and reverb sends (mono), l, r the dry mix (ChoralRoot's mix_l / mix_r scale);
 * l and r are used as the block's scratch; out: QD_FX_CTL stereo frames, Q15 (|x| <= 32767), interleaved */
void qd_fx_block(const int32_t *sc, const int32_t *sd, const int32_t *sr, int32_t *l, int32_t *r, int32_t *out);

#ifdef __cplusplus
}
#endif
