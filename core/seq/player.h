/* SPDX-License-Identifier: GPL-3.0-only */
/* The player: the sequencer's events into the voice pool (PLAN §5 "How a step's sound, locks and live edits
 * combine"), and the live edits of a track's sound.
 *
 *   ON      a voice from the pool (qd_pool_note_on_at) with the trig's patch (the step's sound lock or the track's
 *           sound, its p-locks over it), its own mask (every value with a sound lock, else its locks') and its mix
 *           locks; the track's limit and reserve from the trig's pattern. Same-note re-strike, the limit and the
 *           steals are the pool's.
 *   OFF     the track's voice on that note released, unless a newer strike of the same note (the sequencer's or a
 *           key's) owns it now
 *   LOCK    a trigless step: its locks written into the track's sounding voices (and a note waiting for its fade),
 *           each value made the voice's own; a routing value dips (pool.h)
 *   TIE     a MONO track whose voice holds: the voice moves to the new note legato and the step's locks land on it;
 *           with no voice held, a NOTE
 *   live    qd_player_live_edit: the track's sound changes, and so does every sounding voice of the track whose value
 *           is not its own (routing values dip), and every trig resolved and waiting
 *
 * Everything that happens can be logged (qd_player_t.log): the timelines of the tests. */
#pragma once
#include <stdint.h>

#include "seq.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { QD_LOG_ON, QD_LOG_OFF, QD_LOG_OFF_KEPT, QD_LOG_LOCK, QD_LOG_LEGATO, QD_LOG_DIP, QD_LOG_LIVE };
typedef struct {
    uint64_t sample;
    int64_t tick;
    int kind, track, note, vel;
    int voice;                          /* the pool's slot (-1: none) */
    uint32_t hash;                      /* the voice's patch (FNV-1a, qd_patch_hash) as it is, or will be after a dip */
    qd_vattr_t at;                      /* its own mask and mix locks */
    int id, value;                      /* LOCK / DIP / LIVE: the value */
    int step, pat, hit;
    int64_t gate;
} qd_plog_t;
typedef void (*qd_plog_fn)(void *ctx, const qd_plog_t *l);

typedef struct {
    qd_seq_t *seq;
    qd_pool_t *pool;
    qd_model_t *model;
    uint32_t last_on[QD_PAT_NTRACK][128];   /* the strike that owns (track, note): a sequencer serial, 0 a key */
    qd_plog_fn log;
    void *log_ctx;
    uint32_t ons, offs, offs_kept, locks, legatos, dips;
} qd_player_t;

void qd_player_init(qd_player_t *pl, qd_seq_t *s, qd_pool_t *pool, qd_model_t *m);
void qd_player_event(void *ctx, const qd_seq_ev_t *e);   /* the sequencer's qd_seq_out_fn (ctx: the player) */
void qd_player_block(qd_player_t *pl);  /* qd_seq_block into the pool */
void qd_player_stop(qd_player_t *pl);   /* the transport stops: the sequencer's notes released (tails ring) */
/* the keybed and MIDI notes on a track (P1.1's path; the sequencer's OFFs leave a key's note alone) */
void qd_player_key_on(qd_player_t *pl, int t, int note, int vel);
void qd_player_key_off(qd_player_t *pl, int t, int note);
/* a live edit of track t's sound: patch value id (0..72) or a mix value (QD_LK_PAN .. QD_LK_LEVEL) */
void qd_player_live_edit(qd_player_t *pl, int t, int id, int value);

uint32_t qd_patch_hash(const int16_t *patch);   /* FNV-1a over the 73 values, little-endian */
/* voice i's patch as it will be after its dip (the values waiting applied) */
void qd_player_voice_patch(const qd_pool_t *pool, int i, int16_t *out);

#ifdef __cplusplus
}
#endif
