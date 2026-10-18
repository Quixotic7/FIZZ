/* SPDX-License-Identifier: GPL-3.0-only */
/* The mix (PLAN.md §2: "voices are mixed per voice into the stereo bus and the three sends"): each voice of the pool
 * at its track's level, pan and three sends (chorus, delay, reverb), with ChoralRoot's arithmetic for a part
 * (fx.c mix_part_out / mix_part_loop: LEVEL_Q12, the pan law, the sends ramped over the block from the gains the last
 * block left; eng_quad.c's LFO offsets on the pan and the sends, quad_pan / quad_sends), applied per voice. Then the
 * buses go to qd_fx_block (core/voice/fx). DRIVE (ChoralRoot's DIST, a part insert) is not in P1.1. */
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
} qd_mix_track_t;

typedef struct {
    int32_t sg[QD_POOL_MAX][3];         /* each voice's chorus, delay, reverb gains as the last block left them */
    uint32_t serial[QD_POOL_MAX];       /* the start they belong to (a fresh start: no ramp from another note's) */
    int32_t vbuf[QD_MIX_CTL];
    int rendered;                       /* the last block's voices */
} qd_mix_t;

void qd_mix_init(qd_mix_t *m);
/* one control block: the pool's waiting notes start, every owner's block runs (the FREE LFOs), every voice renders and
 * goes into l, r (the dry stereo mix) and sc, sd, sr (the chorus, delay, reverb sends), all cleared here first.
 * owners[t] and tracks[t] for t < ntracks (a voice of another track is not mixed). Returns the voices rendered */
int qd_mix_block(qd_mix_t *m, qd_pool_t *pool, qd_quad_owner_t *const *owners, const qd_mix_track_t *tracks,
                 int ntracks, int32_t *l, int32_t *r, int32_t *sc, int32_t *sd, int32_t *sr);

#ifdef __cplusplus
}
#endif
