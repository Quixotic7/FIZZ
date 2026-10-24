/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
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
 * voice, once each 32 samples; notes go in between blocks. Plain C, no OS, deterministic.
 *
 * P1.2 (PLAN §5): a voice carries its own attributes from the trig (qd_vattr_t): the "own" mask (the patch values a
 * sound lock or a p-lock made its own: a live edit of the track's sound passes them by) and its mix locks (pan, the
 * sends, the level). qd_pool_write changes one value of a sounding voice (a LOCK step, a live edit); a change of the
 * routing (ALGO, the filter TYPE, PHRT, an LFO's WAVE or TRIG) on a voice that sounds is THE DIP: the voice ramps to 0
 * over its next block with the old value, the value lands, and the block after ramps back from 0 (1.5 ms; PLAN §5
 * "Click risk", question 26). qd_pool_legato moves a voice to a new note (TIE on a MONO track). */
#pragma once
#include <stdint.h>

#include "quad/quad_voice.h"
#include "../qd_config.h"                 /* (P2.0: QD_POOL_MAX, QD_POOL_DEFAULT) */

#ifdef __cplusplus
extern "C" {
#endif

#define QD_POOL_NTRACK QD_TRACKS               /* tracks the limits and reserves are kept for */
#define QD_POOL_LIMIT_MAX (QD_POOL_DEFAULT < 16 ? QD_POOL_DEFAULT : 16)   /* (P2.6: a track's limit and reserve: up to
                                                         * the unit's voices) */

/* a voice's mix values that a lock can set (the lock ids QD_LK_PAN.. in seq/pattern.h, in this order) */
enum { QD_VMIX_PAN, QD_VMIX_DELAY, QD_VMIX_REVERB, QD_VMIX_CHORUS, QD_VMIX_LEVEL, QD_VMIX_N };   /* (P1.4a: DRIVE is
                                                         * the sound's: QD_QP_DRIVE) */
typedef struct {
    uint32_t own[(QD_QUAD_NP + 31) / 32];   /* bit i of own[i >> 5]: patch value i is the voice's own */
    uint8_t mixm;                       /* bit k: mix value k is locked (QD_VMIX_*) */
    int16_t mixv[QD_VMIX_N];            /* pan -64..63, the sends, level 0..127 */
} qd_vattr_t;
#define QD_DIP_MAX 16                   /* routing changes that can wait for one dip */

typedef struct {
    qd_quad_voice_t q;                  /* the FM TONE voice: its patch, its note, its state */
    qd_vattr_t at;                      /* its own values and mix locks (PLAN §5) */
    qd_vattr_t pend_at;                 /* .. those of the note waiting for this voice's fade */
    uint8_t dip_n;                      /* routing changes waiting for this voice's dip (rendered next block) */
    uint8_t dip_id[QD_DIP_MAX];
    int16_t dip_v[QD_DIP_MAX];
    uint8_t track;                      /* the track of the note sounding (or fading out) */
    uint8_t pend;                       /* a note waits for this voice's fade; it starts at the next block */
    uint8_t pend_track;                 /* .. its track */
    uint8_t pend_note, pend_vel, pend_off;   /* .. its note and velocity; pend_off: its key went up meanwhile */
    qd_quad_owner_t *pend_owner;
    int16_t pend_patch[QD_QUAD_NP];
    int16_t pend_gfrom;                 /* E1: .. its glide's start (1/16 semitone; -1 none) */
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
    uint32_t stolen[QD_POOL_NTRACK];   /* P2.11: each track's held notes taken by another track */
    uint32_t dips, writes, legatos;
    int dip_on;                         /* 1 (the default): routing changes dip; 0: they land at once (tests) */
    int halves;                         /* P2.6: a free voice from the half (0 .. n/2 - 1, n/2 .. n - 1) with fewer
                                         * voices in use (the two cores' share); 0 (the default): the first free */
    qd_pool_voice_t v[QD_POOL_MAX];
} qd_pool_t;

void qd_pool_init(qd_pool_t *p, int nvoice);   /* every voice free; limits 16, reserves 0 */
void qd_pool_set_track(qd_pool_t *p, int track, int limit, int reserve);
/* a note on track (0..QD_POOL_NTRACK-1) with the sound patch (copied); returns the slot it took or will take, -1:
 * dropped */
int qd_pool_note_on(qd_pool_t *p, int track, qd_quad_owner_t *o, int note, int vel, const int16_t *patch);
/* .. with the voice's own attributes (at 0: none) */
int qd_pool_note_on_at(qd_pool_t *p, int track, qd_quad_owner_t *o, int note, int vel, const int16_t *patch,
                       const qd_vattr_t *at);
/* patch value id (0..QD_QUAD_NP - 1) of voice i set to value (own: the bit is set as the voice's own). Returns 0
 * nothing changed, 1 written, 2 a routing value of a sounding voice: it lands after the dip (next block). A voice
 * waiting for its fade: its waiting note's patch */
int qd_pool_write(qd_pool_t *p, int i, int id, int value, int own);
int qd_pool_is_dip_id(int id);          /* ALGO, the filter TYPE, PHRT, an LFO's WAVE or TRIG */
int qd_pool_owns(const qd_pool_t *p, int i, int id);   /* patch value id is voice i's own */
void qd_pool_set_mix(qd_pool_t *p, int i, int k, int value);   /* mix value k (QD_VMIX_*) of voice i locked */
/* voice i (sounding, its key down) moves to note legato (quad_voice.h qd_quad_voice_legato): 1 done, 0 not sounding */
int qd_pool_legato(qd_pool_t *p, int i, int note, int vel);
/* E1, portamento: the note that slot i took (qd_pool_note_on*) on owner o's track: a glide's start. key: its chord
 * (the trig's record; 0: a played note, alone) at tick; held: a voice of the track was in gate before the chord. A
 * chord's notes pair by rank with the last chord's (low with low; one more than it had: the nearest), a lone note
 * glides from the track's last note; PORT LEGATO only when held. Settled at the next block (or the next chord) */
void qd_pool_glide_note(qd_pool_t *p, int i, qd_quad_owner_t *o, const void *key, int64_t tick, int held);
int qd_pool_gated(const qd_pool_t *p, int track);
void qd_pool_glide_tie(qd_pool_t *p, qd_quad_owner_t *o, int note);   /* E1: a TIE moved owner o's track to note */   /* E1: a voice of track in gate (its key down) */
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
