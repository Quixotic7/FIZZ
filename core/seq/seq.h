/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The sequencer (PLAN §4 .. §6): the transport, the tracks' clocks, the trigs resolved ahead, conditions and chance,
 * pattern change and chains, MIDI clock in. It turns the project's patterns into timed events; the player (player.h)
 * gives them to the voice pool. Plain C, no OS, deterministic: the same calls give the same events.
 *
 * TIME. Musical time is in ticks, QD_TPS (100800 since P1.3c) a 16th (pattern.h). The transport's position is in
 * subticks, QD_SUB (52500) a tick, and moves 8 x the tempo in milli-BPM subticks a sample: at 44.1 kHz that is exact
 * (a 16th at B BPM is 44100 x 15 / B samples = 100800 x 52500 / (8 x 1000 B) subticks a sample), so a run at a fixed tempo has no
 * drift and the same ticks map to the same samples every time. The sequencer runs a block of 32 samples (one FM TONE
 * control block) a call, qd_seq_block: an event at tick E lands at sample S0 + ceil((E x QD_SUB - P0) / adv) of the
 * block that holds it (P0 the position at the block's first sample S0, adv the subticks a sample), and reaches the
 * pool at that block (the engine's 32-sample quantisation).
 *
 * A TRACK'S CLOCK. Each track has a cursor: its next step's grid tick g (the step's length L from its rate), its index
 * in the track's loop, the loop's pass, the steps since the pattern started. The loop is the track's looped pages in
 * ascending order, each from its start to its end (PLAN §8b; pattern.h qd_track_loop_step): position idx plays that
 * page's step. A step is resolved with qd_step_resolve: the track's defaults where it does not override them, read
 * when it is resolved (a default changed reaches every step not yet resolved). A sound lock is a pool sound. Steps sit at g = origin + n L; tracks of
 * other lengths and rates run on their own (polymeter). Swing delays the odd steps of the loop (the 2nd, 4th, ..) by
 * (swing - 50) / 50 of a step on tracks with swing on; micro-timing moves the step by micro / 24 of a step; a ratchet
 * of n hits strikes at L / n apart; a strum spreads the chord's notes strum / 24 of a step apart, at every hit.
 *
 * AHEAD. A step is resolved (its condition, its chance, its patch, its events) when the transport reaches two of its
 * own steps before its grid tick (PLAN §5: so a micro-timing of -23/24 on step 1 fires in the last step of the pass
 * before). The events wait in a queue ordered by tick (then OFF, LOCK, TIE, ON; then track; then the order they were
 * made) and leave it at their block. Conditions are evaluated in grid order across the tracks: before a step of track t
 * is resolved, track t - 1 is resolved up to its grid tick, so NEI reads the neighbour's state as of that tick.
 *
 * CONDITIONS AND CHANCE. A:B counts the passes of the track's own loop since its pattern started; 1ST is pass 0; FILL
 * follows qd_seq_fill. PRE is the last conditional step of the same track: it played (condition true and chance won);
 * a conditional step is one with FILL / NOT FILL / 1ST / NOT 1ST / A:B, or a chance under 100 % (PRE and NEI steps
 * read the state, they do not set it, as Elektron's). NEI is the same state of the track to the left (track 1 has
 * none: NEI false). Chance: one roll per step per pass, roll = qd_seq_roll(seed, track, n) < chance, n the step's
 * count since its pattern started; the seed is qd_seq_seed's, mixed with how many patterns have started since PLAY,
 * so a run is deterministic and a chain repeated rolls anew.
 *
 * PATTERN CHANGE. qd_seq_queue: at the end of the master length (the next multiple of it from where the playing pattern
 * started) or, immediate, at the next 16th. P1.3c (§8b 2.1, the launch mode): qd_seq_select_at also changes at the
 * next BAR (16 sixteenths) or BEAT (4) counted from where the playing pattern started (a change point the transport is
 * exactly on: there), or NOW (the next 16th, as immediate); END keeps a change already queued at its point, the others
 * reckon it again from now. Tracks that resolved steps of the old pattern beyond that tick are wound
 * back to it (their unplayed events dropped). Chains (qd_seq_chain): up to 16 patterns, each played for its master
 * length, looped. A pattern with master INF never moves on.
 *
 * STEP FUNCTIONS (P1.3w; pattern.h QD_FN_*, Form's FUNCTION). A step's function runs when the step fires (after its
 * condition and chance), as it is resolved, so in resolution order (the look-ahead's): it sets where the track's
 * cursor goes next. Each track has a DIRECTION (forward; back to forward at PLAY, kept through a pattern change):
 * reverse walks the loop's positions backwards (each page from its end to its start, the pages in descending order),
 * a pass counted at each wrap. RESTART: the next position is the loop's start in the playing direction (the first
 * position forward, the last in reverse) and the passes start again from 0 (A:B, 1ST). REVERSE / FORWARD set the
 * direction, FLIP turns it, before the step after is reckoned. RANDOM JUMP: a position of the loop at random; RANDOM
 * FUNCTION: none / restart / reverse / forward / flip at random; both rolled with qd_seq_fn_roll (splitmix64 of the
 * pattern's seed, the track, the step's count since its pattern started and a salt: deterministic). JUMP n: the
 * position of the track's step n (a step outside the loop: the loop's start, position 0). A jump does not count a
 * pass. P1.3y: LEAP n: n positions on from the step's in the playing direction (back for a negative n), round the
 * loop, a pass at each wrap. Micro-timing in reverse is mirrored (as Form's nudges): a step plays at -micro. A pattern change in reverse
 * starts the new pattern at its loop's last position. A rewind (a pattern change reckoned again) undoes the direction
 * changes of the steps it drops.
 *
 * MIDI CLOCK IN (qd_seq_midi_*): 24 pulses a quarter, 8400 ticks each. START begins at tick 0 on the first pulse;
 * between pulses the transport runs at the tempo of the last 24 intervals (smoothed), never past the next pulse's
 * tick (it waits there), and a pulse that arrives while it is behind brings it to its tick at once: the 16ths land on
 * their pulses. STOP stops (the notes released), CONTINUE goes on from there at the next pulse. P1.3i: STOP also ends
 * following the clock (the tempo is the pattern's again), and a clock that sends no pulse for half a second (a START
 * with no clock after it, a DAW gone quiet) stops the transport and ends it the same way. */
#pragma once
#include <stdint.h>

#include "model.h"
#include "pattern.h"
#include "../voice/pool.h"
#include "../qd_config.h"                 /* (P2.0: QD_SEQ_NEV, QD_SEQ_NREC) */

#ifdef __cplusplus
extern "C" {
#endif

#define QD_SUB 52500                    /* subticks a tick (P1.3c: halved as QD_TPS doubled: a 16th unchanged) */
#define QD_SEQ_BLOCK 32                 /* samples a block (FM TONE's control block) */
#define QD_SEQ_FS 44100
#define QD_SEQ_NHIST 128                /* the conditional steps kept per track (NEI) */
#define QD_SEQ_NCHAIN 16
#define QD_CLOCK_TICKS (QD_TPS / 6)     /* a MIDI clock pulse: 6 a 16th */
#define QD_CLOCK_TIMEOUT (QD_SEQ_FS / 2)   /* P1.3i: following the clock, no pulse for this long (samples): it is gone */

enum { QD_EV_OFF, QD_EV_LOCK, QD_EV_TIE, QD_EV_ON };   /* (their order at the same tick) */

/* a resolved trig (PLAN §5 "the pending record"): the patch its voices start with, their own mask and mix locks, and
 * the step's locks as they are (a LOCK step writes them, a TIE applies them) */
typedef struct {
    uint8_t used, track, type, sound;
    uint16_t refs;                      /* events that point at it */
    int16_t step;                       /* the step (0..127) and pattern it came from */
    int16_t pat;
    int16_t patch[QD_QUAD_NP];
    qd_vattr_t at;
    uint8_t nlk;
    uint32_t lkm[(QD_LK_N + 31) / 32];  /* P2.1b: bit id: the step locks it; its value is patch[id] (a sound value: a
                                         * live edit passes an own value by) or at.mixv (a mix value). Was the locks'
                                         * copies (345 B a record) */
} qd_trec_t;

/* an event waiting */
typedef struct {
    int64_t tick;
    int32_t gate;                       /* ON / TIE: ticks, -1 INF (P2.1b: 32 bits; the longest, 128 steps of a dotted
                                         * 1/1, is 3.1e8 ticks) */
    int32_t dg;                         /* the grid tick of its step, less tick (P2.1b: within a gate and a step of it;
                                         * the pattern's origin it kept is gone: nothing read it) */
    uint32_t seq;                       /* the order it was made in */
    uint32_t serial;                    /* ON / TIE: its own; OFF: the one it ends */
    int16_t rec;                        /* ON / TIE / LOCK: its trig, else -1 */
    uint8_t kind, track, note, vel, step, hit;
} qd_sev_t;

/* an event leaving, to the player */
typedef struct {
    uint64_t sample;                    /* the sample it falls on (qd_seq_t.now's clock) */
    int64_t tick;
    int kind, track, note, vel;
    int64_t gate;                       /* ticks, -1 INF */
    uint32_t serial;
    const qd_trec_t *rec;               /* ON / TIE / LOCK */
    int step, pat, hit;
} qd_seq_ev_t;
typedef void (*qd_seq_out_fn)(void *ctx, const qd_seq_ev_t *e);

typedef struct {
    int pat;                            /* the pattern of the cursor's segment */
    int64_t origin;                     /* .. where it started */
    int64_t g;                          /* the next step's grid tick */
    int64_t n;                          /* steps since origin */
    int idx;                            /* in the track's loop */
    int64_t pass;
    int pre;                            /* PRE */
    int dir;                            /* P1.3w: -1 reverse, else forward */
    int dn;                             /* .. its changes by steps resolved (undone by a rewind): grid tick, old dir */
    int64_t dg[8];
    int8_t dold[8];
    int rn, rw;                         /* the last positions resolved (the playhead): grid tick, loop position */
    int64_t rg[4];
    int16_t ridx[4];
    int hn, hw;                         /* the history of conditional steps (NEI): count, next write */
    int32_t hg[QD_SEQ_NHIST];           /* P2.1c: their grid ticks less hbase (32 bits; rebased as time goes on, an
                                         * entry left more than 2^31 behind kept as INT32_MIN, the newest such one's
                                         * tick exactly in hsat) */
    int64_t hbase, hsat;
    uint8_t hr[QD_SEQ_NHIST];
} qd_tcur_t;

typedef struct {
    qd_project_t *proj;
    qd_model_t *model;
    /* the transport */
    int playing;
    uint64_t now;                       /* samples run (qd_seq_block's clock: 32 a block, playing or not) */
    int64_t pos;                        /* subticks since PLAY */
    int64_t tick;                       /* P2.4: pos / QD_SUB, kept with it (no 64-bit division a call) .. */
    int32_t tick_rem;                   /* .. and its remainder */
    int32_t mbpm;                       /* the tempo, milli-BPM (internal clock) */
    int fill;
    uint8_t mute;                       /* bit t: track t muted (the app's; a pattern's own mute flags too) */
    uint8_t solo;                       /* bit t: track t soloed (any solo: only the soloed tracks sound; a muted one
                                         * stays silent) */
    uint64_t seed, seed_play;           /* qd_seq_seed's; the run's (seed and the patterns started) */
    uint32_t nstart;                    /* patterns started since PLAY */
    /* patterns */
    int cur;                            /* the pattern playing (or to play at PLAY) */
    int64_t origin;                     /* where it started (ticks) */
    int sw_pat;                         /* the pattern queued (-1: none) */
    int64_t sw_x;                       /* .. the tick it takes over */
    int sw_cpos;                        /* .. its place in the chain (-1: not the chain's) */
    int sw_held;                        /* P2.4: a cursor went on past sw_x in the playing pattern (its change not in
                                         * RAM yet, pattern.h nopage): it enters with a rewind */
    uint32_t sw_late;                   /* P2.4: changes held for their pattern (each: to the next boundary, or 16th) */
    int chain[QD_SEQ_NCHAIN], nchain, cpos;
    /* the tracks */
    qd_tcur_t cur_t[QD_PAT_NTRACK];
    int64_t inf_g[QD_PAT_NTRACK];       /* the step (its grid tick) holding INF notes on the track */
    uint8_t inf_note[QD_PAT_NTRACK][QD_NNOTE * 8];
    uint32_t inf_serial[QD_PAT_NTRACK][QD_NNOTE * 8];
    int inf_n[QD_PAT_NTRACK];
    int tie_pending[QD_PAT_NTRACK];     /* TIEs waiting in the queue */
    uint8_t skip_note[QD_PAT_NTRACK][8];   /* the OFFs a MONO track holds back because a TIE follows */
    uint32_t skip_serial[QD_PAT_NTRACK][8];
    int skip_n[QD_PAT_NTRACK];
    /* the UI's view: the last step played per track */
    int flash_step[QD_PAT_NTRACK];
    int64_t flash_tick[QD_PAT_NTRACK];
    uint64_t flash_sample[QD_PAT_NTRACK];
    /* P1.5s: the last step that came to play per track, audible or not (a muted or not soloed track too: EDIT follows
     * it), its step's grid tick (a chord's notes, a ratchet's hits: one step) and when (its first note's sample) */
    int heard_step[QD_PAT_NTRACK];
    int64_t heard_g[QD_PAT_NTRACK];
    uint64_t heard_sample[QD_PAT_NTRACK];
    /* P1.5t: a track's sound heard instead of its pattern's (the library browser's preview; 0: none) */
    const int16_t *pv_patch[QD_PAT_NTRACK];
    /* the queue */
    qd_sev_t ev[QD_SEQ_NEV];
    int nev;
    uint32_t seqno, serial;
    qd_trec_t rec[QD_SEQ_NREC];
    /* MIDI clock */
    int ext;                            /* following MIDI clock */
    int ext_wait;                       /* START / CONTINUE seen: the next pulse runs */
    int64_t clk_target;                 /* the last pulse's position (subticks); the cap is one pulse on */
    int64_t clk_adv;                    /* subticks a sample between pulses */
    uint64_t clk_last;                  /* the last pulse's sample */
    uint32_t clk_iv[24];                /* the last intervals, samples */
    int clk_niv, clk_wiv;
    int64_t clk_pulses;
    /* counters (tests, the budget meter) */
    uint32_t resolved, played, dropped_ev, dropped_rec, rewinds;
} qd_seq_t;

void qd_seq_init(qd_seq_t *s, qd_project_t *proj, qd_model_t *model);
void qd_seq_seed(qd_seq_t *s, uint64_t seed);
/* P1.3w: a step function's roll: splitmix64 of seed ^ track << 56 ^ n ^ salt << 40, mod m (salt 1: RANDOM JUMP's
 * position, 2: RANDOM FUNCTION's choice) */
uint32_t qd_seq_fn_roll(uint64_t seed, int track, int64_t n, int salt, uint32_t m);
uint32_t qd_seq_roll(uint64_t seed, int track, int64_t n);   /* 0..99 */
void qd_seq_tempo_mbpm(qd_seq_t *s, int32_t mbpm);  /* 30000..300000 */
int qd_seq_bpm(const qd_seq_t *s);      /* the tempo now, rounded (the MIDI clock's when following it) */
void qd_seq_fill(qd_seq_t *s, int on);

/* the transport */
void qd_seq_play(qd_seq_t *s);          /* from the top of the current pattern (or the chain's first) */
void qd_seq_stop(qd_seq_t *s, qd_seq_out_fn out, void *ctx);   /* every note released (OFFs out now), counters reset */
/* one block: the steps due resolved, the events of this block out (each with its sample), the clock on 32 samples */
void qd_seq_block(qd_seq_t *s, qd_seq_out_fn out, void *ctx);

/* patterns: stopped, the choice is the pattern PLAY starts; playing, it is queued (to the end of the master length,
 * or at the next 16th when immediate). A queue replaces the chain with that pattern alone */
void qd_seq_select(qd_seq_t *s, int pat, int immediate);   /* immediate: QD_LAUNCH_NOW, else QD_LAUNCH_END */
enum { QD_LAUNCH_END, QD_LAUNCH_BAR, QD_LAUNCH_BEAT, QD_LAUNCH_NOW, QD_NLAUNCH };
void qd_seq_select_at(qd_seq_t *s, int pat, int launch);
const char *qd_launch_name(int launch);   /* "END", "BAR", "BEAT", "NOW" */
void qd_seq_chain(qd_seq_t *s, const int *pats, int n);
int qd_seq_queued(const qd_seq_t *s);   /* the queued pattern, -1 none */
/* the playing pattern's master length and the ticks to the change (0: none queued) */
int64_t qd_seq_to_change(const qd_seq_t *s);

/* MIDI clock in (between blocks: the sample is qd_seq_t.now) */
void qd_seq_midi_clock(qd_seq_t *s);
void qd_seq_midi_start(qd_seq_t *s);
void qd_seq_midi_continue(qd_seq_t *s);
void qd_seq_midi_stop(qd_seq_t *s, qd_seq_out_fn out, void *ctx);
/* P1.3i: following a clock that runs (START or CONTINUE, then pulses): the tempo is the clock's. STOP ends following
 * it (CONTINUE follows again), and so does a clock gone quiet: no pulse for QD_CLOCK_TIMEOUT stops the transport */
int qd_seq_ext_running(const qd_seq_t *s);

/* the view (the screens): the transport in ticks, a track's play head as a step position in its loop (0..length,
 * fractional), whether the track's step s flashed within the last ticks */
int64_t qd_seq_tick(const qd_seq_t *s);
void qd_seq_track_pos_q(const qd_seq_t *s, int t, int64_t *num, int64_t *den);   /* P2.4: num / den steps (0 / 1
                                                         * stopped): 0 <= num < the loop's length x den */
float qd_seq_track_pos_f(const qd_seq_t *s, int t);   /* .. as a float (the nearest) */
int qd_seq_track_pos_i(const qd_seq_t *s, int t);     /* .. its whole steps */
int qd_seq_track_step(const qd_seq_t *s, int t);   /* the step (0..127) under the track's play head; -1 stopped */
int qd_seq_audible(const qd_seq_t *s, const qd_pattern_t *P, int t);   /* not muted, and soloed if any is */
const qd_pattern_t *qd_seq_track_pat(const qd_seq_t *s, int t);   /* the pattern the track plays now */

/* track t's loop from `steps` of its steps after its start (the cursor rewound there); skip: that step not played
 * (a capture's end and start: ctl_rec.c) */
void qd_seq_track_from(qd_seq_t *s, int t, int steps, int skip);

/* a live edit of track t's sound reaches the trigs resolved and waiting too (their values not their own) */
void qd_seq_live_patch(qd_seq_t *s, int t, int id, int value);
/* P1.5t: track t's trigs (those with no sound lock) play patch instead of the pattern's track sound (0: back to it);
 * the trigs already resolved follow at once (their locks kept) */
void qd_seq_preview(qd_seq_t *s, int t, const int16_t *patch);

/* P1.3d: the record step `step` of pattern `pat` on track t would have if it were resolved now (its sound lock or the
 * track's sound, its p-locks over it, the own mask, the mix locks): the stopped preview's (player.h). Reads only: the
 * sequencer's state is not changed */
void qd_seq_step_rec(const qd_seq_t *s, int pat, int t, int step, qd_trec_t *r);

#ifdef __cplusplus
}
#endif
