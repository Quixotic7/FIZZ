/* SPDX-License-Identifier: GPL-3.0-only */
/* The voice pool (PLAN.md §2): FM TONE voices shared by the tracks. A voice belongs to the track that struck it while it
 * sounds and carries its own patch (PLAN.md §5). Each track has a LIMIT (1..16; 1 is a mono choke) and a RESERVE (0 by
 * default: the voices no other track may take from it).
 *
 * A note on track t (qd_pool_note_on):
 *   0. the same note again on t, with a voice still sounding it: if the sound is the same (the patch, value for
 *      value), that voice is struck again in place; else it is released (its own envelope) and a fresh voice takes
 *      the note below. (A note still waiting for its voice's fade: it takes the new velocity and sound.)
 *   1. t at its limit (its sounding voices and the notes waiting for one): t's own voice: the quietest released one,
 *      else the oldest held one;
 *   2. else a free voice;
 *   3. else a steal across the tracks: a released voice (the quietest of all), else the oldest held note of the track
 *      furthest over its reserve. A track at or under its reserve is not robbed (its released voices neither). No
 *      voice to take: the note is dropped (counted).
 * A voice taken while it sounds is not cut: it fades over one control block (32 samples, ChoralRoot's fade steal) and
 * the new note starts in that slot at the next block (qd_pool_block_begin). One that has not sounded yet (no block
 * rendered since its start) is restarted in place at once.
 *
 * The time is the control block: the caller runs qd_pool_block_begin, the owners' blocks, then qd_pool_render for every
 * voice, once each 32 samples; notes go in between blocks. Plain C, no OS, deterministic. */
#pragma once
#include <stdint.h>

#include "quad/quad_voice.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_POOL_MAX 64                  /* voices at most (a setting: 16 by default, more for experiments) */
#define QD_POOL_DEFAULT 16
#define QD_POOL_NTRACK 8                /* tracks the limits and reserves are kept for */
#define QD_POOL_LIMIT_MAX 16

typedef struct {
    qd_quad_voice_t q;                  /* the FM TONE voice: its patch, its note, its state */
    uint8_t track;                      /* the track of the note sounding (or fading out) */
    uint8_t pend;                       /* a note waits for this voice's fade; it starts at the next block */
    uint8_t pend_track;                 /* .. its track */
    uint8_t pend_note, pend_vel, pend_off;   /* .. its note and velocity; pend_off: its key went up meanwhile */
    qd_quad_owner_t *pend_owner;
    int16_t pend_patch[QD_QUAD_NP];
    uint32_t age;                       /* when its note was struck (the pool's clock: oldest = smallest) */
    uint32_t serial;                    /* bumped at every fresh start in this slot (the mix's ramps start over) */
} qd_pool_voice_t;

typedef struct {
    int nvoice;                         /* 1..QD_POOL_MAX */
    uint8_t limit[QD_POOL_NTRACK];      /* 1..16 */
    uint8_t reserve[QD_POOL_NTRACK];    /* 0..16 */
    uint32_t clock;
    /* counters (tests, the budget meter) */
    uint32_t starts, restrikes, frees, steals_own, steals_cross, drops;   /* frees: notes that took a free voice */
    qd_pool_voice_t v[QD_POOL_MAX];
} qd_pool_t;

void qd_pool_init(qd_pool_t *p, int nvoice);   /* every voice free; limits 16, reserves 0 */
void qd_pool_set_track(qd_pool_t *p, int track, int limit, int reserve);
/* a note on track (0..QD_POOL_NTRACK-1) with the sound patch (copied); returns the slot it took or will take, -1:
 * dropped */
int qd_pool_note_on(qd_pool_t *p, int track, qd_quad_owner_t *o, int note, int vel, const int16_t *patch);
void qd_pool_note_off(qd_pool_t *p, int track, int note);   /* every voice of track holding note: released */
void qd_pool_release_all(qd_pool_t *p, int track);         /* track < 0: every track */
void qd_pool_kill_all(qd_pool_t *p);                        /* every voice faded out (a panic), no note waits */
/* a control block begins: the notes whose voice's fade has been rendered start now */
void qd_pool_block_begin(qd_pool_t *p);
/* voice i's block added to out[0..31]: 1 rendered, 0 silent */
int qd_pool_render(qd_pool_t *p, int i, int32_t *out);

/* the state, for the screens and the tests */
int qd_pool_sounding(const qd_pool_t *p, int i);           /* the voice sounds (not fading out for another note) */
int qd_pool_used(const qd_pool_t *p, int i);               /* sounds, fades or waits: not free */
int qd_pool_count(const qd_pool_t *p, int track);          /* the track's voices against its limit */
int qd_pool_in_use(const qd_pool_t *p);                    /* voices not free */

#ifdef __cplusplus
}
#endif
