/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
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
 * P1.6c, MIDI FX (mfx.h): a trig whose step plays through a group (qd_step_mfx: the step's, else the track's; a group
 * with no effect playing: straight on, as before) goes into the group, its OFF after it (to the group its ON went to),
 * and the group's notes come back here to the pool: each with its trig's sound and locks (resolved again, qd_seq_step_rec,
 * when the group plays it later: an arp's), on its own track (that track's limit). A TIE into a group: the track's
 * notes held before it end, then it strikes (a retrigger: no legato through a group). The keys (the keybed, MIDI in)
 * play through the selected track's group likewise. The groups run on the player's clock: the transport's ticks while
 * playing, the tempo's while stopped (the arp plays the keys at any time). STOP ends every group's notes; a track
 * that falls silent (a mute, a solo) has its sequencer notes taken out of the groups.
 *
 * FIZZ MIDI (c), MIDI OUT: every note the player gives the pool, or takes back, is also told to out (the app's: a track
 * with MIDI OUT sends it), as the pool sees it: the sequencer's (after its groups: an arp's notes), the keys' and the
 * preview's (live 1), a LEGATO / TIE the new note's ON then the old one's OFF, note -1 every note of track t (live -1:
 * all; 0: the sequencer's only): the track's flush (MIDI's ALL NOTES OFF), a track that falls silent, STOP. (A MIDI
 * track, next: its notes go out and skip the pool here.) edit is told every change of a track's sound value by
 * qd_player_live_edit (its old value): the app sends the panel's knob moves as CCs from it.
 *
 * Everything that happens can be logged (qd_player_t.log): the timelines of the tests. */
#pragma once
#include <stdint.h>

#include "mfx.h"
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
typedef void (*qd_pout_fn)(void *ctx, int t, int note, int vel, int live);   /* (c): a note on (vel 1..127) / off (0) */
typedef void (*qd_pedit_fn)(void *ctx, int t, int id, int old, int value);    /* (c): a sound value changed */

typedef struct {
    qd_seq_t *seq;
    qd_pool_t *pool;
    qd_model_t *model;
    uint32_t last_on[QD_PAT_NTRACK][128];   /* the strike that owns (track, note): a sequencer serial, 0 a key */
    qd_plog_fn log;
    void *log_ctx;
    qd_pout_fn out;                     /* FIZZ MIDI (c): the notes as the pool gets them (0: none) .. */
    qd_pedit_fn edit;                   /* .. the sound's live edits */
    void *out_ctx;
    uint32_t ons, offs, offs_kept, locks, legatos, dips;
    uint32_t previews;                  /* P1.3d: steps struck by qd_player_preview */
    /* P1.6c: the MIDI FX groups' run-time, the group each note went into (0 none), the clock */
    qd_mfx_t mfx;
    uint8_t on_grp[QD_PAT_NTRACK][128]; /* the sequencer's notes .. */
    uint8_t key_grp[QD_PAT_NTRACK][128];   /* .. the keys' */
    int64_t mfx_sub;                    /* the groups' clock (subticks, never back) */
    int64_t mfx_tk;                     /* P2.4: (mfx_sub + QD_SUB - 1) / QD_SUB, kept with it; .. */
    int64_t mfx_q;                      /* .. mfx_sub / QD_SUB and its remainder (C's) */
    int32_t mfx_r;
    int64_t mfx_base;                   /* .. the tick of the transport's 0 */
    int64_t mfx_pos;                    /* .. the transport's position at the last block */
    int mfx_play;                       /* .. it was playing */
    uint8_t mfx_audible;                /* the tracks heard at the last block (bit t) */
    uint32_t mfx_nstart;                /* 2026-10-26: the pattern starts seen (a change: the groups' sequencer notes flushed) */
    qd_trec_t mrec;                     /* (a trig resolved again for a group's note) */
    uint32_t mfx_ons, mfx_offs;
    /* 2026-10-26, SLIDE (the 303's: the slide is on the note that slides INTO the next): a step's PORT lock arms its
     * track's next note (arm, from the arming step's tick); the first note of that next step takes it (cur, for every
     * note of its chord: ON a fresh voice gliding from the track's last note, LEGATO the note before moved without a
     * new attack). LEGATO's notes are kept in gate until then (hold: their serials and notes: their OFFs are kept
     * back). TIME / SLOPE: the step's locks, -1 the sound's own */
    struct { int8_t on, port, time, slope; int64_t tick; } slide_arm[QD_PAT_NTRACK], slide_cur[QD_PAT_NTRACK];
    uint32_t slide_hold[QD_PAT_NTRACK][QD_NNOTE];
    uint8_t slide_hold_note[QD_PAT_NTRACK][QD_NNOTE];
} qd_player_t;

void qd_player_init(qd_player_t *pl, qd_seq_t *s, qd_pool_t *pool, qd_model_t *m);
void qd_player_event(void *ctx, const qd_seq_ev_t *e);   /* the sequencer's qd_seq_out_fn (ctx: the player) */
void qd_player_block(qd_player_t *pl);  /* qd_seq_block into the pool */
void qd_player_stop(qd_player_t *pl);   /* the transport stops: the sequencer's notes released (tails ring) */
/* the keybed and MIDI notes on a track (P1.1's path; the sequencer's OFFs leave a key's note alone) */
void qd_player_key_on(qd_player_t *pl, int t, int note, int vel);
void qd_player_key_off(qd_player_t *pl, int t, int note);
/* P1.6c: the groups' tick now; track t's notes released (MIDI's ALL NOTES OFF: the pool's and the groups') */
int64_t qd_player_mfx_tick(const qd_player_t *pl);
void qd_player_pattern_ms(qd_player_t *pl);   /* 2026-10-26: a pattern change: the groups' sequencer notes end */
void qd_player_flush_track(qd_player_t *pl, int t);
/* P1.3d, the stopped preview (PLAN §8b 2.2): step s of pattern pat on track t struck as the sequencer strikes it (its
 * notes at its velocity, its sound lock or the track's sound, its p-locks and mix locks, the track's limit and
 * reserve), owned as a key's notes (released with qd_player_key_off; the sequencer's OFFs leave them). The sequencer's
 * state and the pattern are not changed. An empty step, or a LOCK step (it plays no note): nothing. Returns the notes
 * struck (notes: room for QD_NNOTE), ascending */
int qd_player_preview(qd_player_t *pl, int pat, int t, int s, uint8_t *notes);
/* P1.6e: the preview over (every held step let go): track t's keys' notes taken out of the groups (a HOLD arp's too) */
void qd_player_preview_end(qd_player_t *pl, int t);
/* a live edit of track t's sound: patch value id (0..73, DRIVE 73) or a mix value (QD_LK_PAN .. QD_LK_LEVEL) */
void qd_player_live_edit(qd_player_t *pl, int t, int id, int value);
/* P1.6d, automation recording: value id (a patch value or a mix value) into track t's sounding voices as their own, as
 * a LOCK trig writes it (heard now; the track's sound unchanged) */
void qd_player_voice_lock(qd_player_t *pl, int t, int id, int value);

uint32_t qd_patch_hash(const int16_t *patch);   /* FNV-1a over the 74 values, little-endian (DRIVE 0 skipped) */
/* voice i's patch as it will be after its dip (the values waiting applied) */
void qd_player_voice_patch(const qd_pool_t *pool, int i, int16_t *out);

#ifdef __cplusplus
}
#endif
