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
/* a block: sc, sd, sr the chorus, delay and reverb sends (mono), l, r the dry mix (ChoralRoot's mix_l / mix_r scale);
 * l and r are used as the block's scratch; out: QD_FX_CTL stereo frames, Q15 (|x| <= 32767), interleaved */
void qd_fx_block(const int32_t *sc, const int32_t *sd, const int32_t *sr, int32_t *l, int32_t *r, int32_t *out);

#ifdef __cplusplus
}
#endif
