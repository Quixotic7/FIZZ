/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The mix (PLAN.md §2: "voices are mixed per voice into the stereo bus and the three sends"): each voice of the pool
 * at its track's level, pan and three sends (chorus, delay, reverb), with ChoralRoot's arithmetic for a part
 * (fz_voice.h, L1: LEVEL_Q12, the pan law, the sends ramped over the block from the gains the last block left;
 * eng_quad.c's LFO offsets on the pan and the sends, quad_pan / quad_sends), applied per voice. Then the
 * buses go to qd_fx_block (core/voice/fx). P1.4a: DRIVE, ChoralRoot's DIST (a part insert there), runs per voice before
 * the level: the voice's patch's DRIVE plus its owner's LFO offset (eng_quad.c quad_drive), its own state (a fresh note
 * starts it from 0). */
#pragma once
#include <stdint.h>

#include "pool.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_MIX_CTL 32

typedef struct {                        /* a track's mix (the model's; a voice could carry its own later: p-locks) */
    uint8_t level;                      /* 0..127, ChoralRoot's part LEVEL (LEVEL_Q12; 104 its default) */
    int8_t pan;                         /* -64..63 */
    uint8_t chorus, delay, reverb;      /* the sends, 0..127 */
    uint8_t dry;                        /* 2026-10-26 (the owner): the track's DRY / WET, 0..127: 127 the dry alone, 0
                                         * the effects alone (PARALLEL: the returns; CHAIN: the chain's output), 64
                                         * (QD_MIX_DRY_DEFAULT) both full in PARALLEL (as every unit sounded) and half
                                         * and half in CHAIN (unity with the sends at 0) */
    int32_t g_lvl, g_send[3];           /* .. as gains (Q12) on the level and the chorus, delay, reverb sends, made at
                                         * the control rate (app.c mix_gains: the audio path's RAM code has no room):
                                         * PARALLEL the dry / wet law, CHAIN the serial shares; 4096: as before */
} qd_mix_track_t;
#define QD_MIX_DRY_DEFAULT 64

typedef struct {
    int32_t sg[QD_POOL_MAX][3];         /* each voice's chorus, delay, reverb gains as the last block left them */
    uint32_t serial[QD_POOL_MAX];       /* the start they belong to (a fresh start: no ramp from another note's) */
    qd_dist_t dist[QD_POOL_MAX];        /* P1.4a: each voice's DRIVE state */
    int rendered;                       /* the last block's voices */
    int32_t (*vtap)[QD_MIX_CTL];        /* 2026-10-27: while the Geek Out TRACKS page shows: each voice's block (DRIVE in,
                                         * before its level, pan and sends) copied to vtap[i] (0: none) */
} qd_mix_t;

void qd_mix_init(qd_mix_t *m);
/* one control block: the pool's waiting notes start, every owner's block runs (the FREE LFOs), every voice renders and
 * goes into l, r (the dry stereo mix) and sc, sd, sr (the chorus, delay, reverb sends), all cleared here first.
 * owners[t] and tracks[t] for t < ntracks (a voice of another track is not mixed). Returns the voices rendered */
int qd_mix_block(qd_mix_t *m, qd_pool_t *pool, qd_quad_owner_t *const *owners, const qd_mix_track_t *tracks,
                 int ntracks, int32_t *l, int32_t *r, int32_t *sc, int32_t *sd, int32_t *sr);
/* P2.6: the same block in three (the two cores, app.h qd_app_block_*): qd_mix_begin (the pool's waiting notes start,
 * every owner's block, what the voices will claim of the shared: their comb lines), qd_mix_voices (voices v0 .. v1 - 1
 * rendered and mixed into l, r, sc, sd, sr, cleared first; it writes nothing but those voices' own state (the pool's
 * and the mix's slot of each) and these buses: two ranges may run at once), qd_mix_end (the latest voices' offsets
 * to their owners, the count of voices rendered). The buses of two ranges summed (in any order: integer sums) are
 * qd_mix_block's */
void qd_mix_begin(qd_mix_t *m, qd_pool_t *pool, qd_quad_owner_t *const *owners, int ntracks);
int qd_mix_voices(qd_mix_t *m, qd_pool_t *pool, const qd_mix_track_t *tracks, int ntracks, int v0, int v1, int32_t *l,
                  int32_t *r, int32_t *sc, int32_t *sd, int32_t *sr);
void qd_mix_end(qd_mix_t *m, qd_quad_owner_t *const *owners, int ntracks, int rendered);

#ifdef __cplusplus
}
#endif
