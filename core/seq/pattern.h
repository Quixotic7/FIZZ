/* SPDX-License-Identifier: GPL-3.0-only */
/* The pattern model (PLAN.md §4, §5, §6): steps, the lock pool, the tracks' settings, a pattern, a project of 64; and
 * the editing API the UI (P1.3) and the tests call. Plain C structs, no allocation, nothing of the OS: a pattern can be
 * stored as it is (P1.5).
 *
 * A step is PLAN §4's 12-byte layout, kept byte for byte (qd_step_t). A pattern's p-locks are one pool of
 * QD_NLOCK lock_t {step, id, value}, sorted by track, then step, then id (a track's range: lock_start[t],
 * lock_n[t]); a step's "has locks" flag (bit 7) says it has any. The editing API keeps all of that consistent: a
 * full pool returns QD_EFULL and nothing is changed.
 *
 * Lock ids (PLAN §5): 0..72 the FM TONE patch's values (eng_quad.c QP_*: QD_QUAD_NP of them), then the voice's mix:
 * QD_LK_PAN (-64..63), QD_LK_DELAY, QD_LK_REVERB, QD_LK_CHORUS (the sends, 0..127), QD_LK_DRIVE (0..127: stored, the
 * track insert DRIVE is not built yet), QD_LK_LEVEL (the mix level, 0..127, ChoralRoot's LEVEL_Q12 index). */
#pragma once
#include <stdint.h>

#include "../voice/quad/quad_voice.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_PAT_NTRACK 8
#define QD_NSTEP 128                    /* steps a track at most (8 pages of 16) */
#define QD_PAGE 16
#define QD_NNOTE 4                      /* notes a step */
#define QD_NLOCK 1024                   /* the lock pool a pattern */
#define QD_NBANK 4                      /* A..D */
#define QD_NPAT (QD_NBANK * 16)         /* patterns a project */
/* the sequencer's musical time: ticks, QD_TPS a 16th (a step at rate 1). 50400 = 2^5 3^2 5^2 7: a step at every rate
 * (8 .. 1/2 sixteenths, 3/2, 3/4, 4/3, 2/3) divides by 24 (micro, strum), 1..8 (ratchets), 8 (the shortest length)
 * and 50 (swing 50..80 %) in whole ticks, so every time the sequencer computes is exact */
#define QD_TPS 50400

/* ---- a step (PLAN §4: 12 bytes) */
enum { QD_TRIG_NONE, QD_TRIG_NOTE, QD_TRIG_LOCK, QD_TRIG_TIE };
enum { QD_RAMP_FLAT, QD_RAMP_UP, QD_RAMP_DOWN, QD_RAMP_ACCENT };
#define QD_SF_TYPE 0x03u                /* flags: bits 0-1 the trig type */
#define QD_SF_RATCH_SH 2                /* bits 2-4 ratchet - 1 */
#define QD_SF_RATCH 0x1Cu
#define QD_SF_RAMP_SH 5                 /* bits 5-6 the ramp */
#define QD_SF_RAMP 0x60u
#define QD_SF_LOCKS 0x80u               /* bit 7 the step has locks in the pool */
typedef struct {
    uint8_t flags;
    uint8_t note[QD_NNOTE];             /* note 1 absolute (1..127, 0 none); 2..4 offsets +1..+48 from note 1 (0 none) */
    uint8_t vel;                        /* 1..127 */
    uint8_t len;                        /* an index into the length table (qd_len_eighths), QD_LEN_INF the last */
    int8_t micro;                       /* -23..+23 24ths of a step */
    uint8_t cond;                       /* QD_COND_*, then A:B (qd_cond_ab) */
    uint8_t chance;                     /* 0..100 % */
    uint8_t sound;                      /* the sound lock: 0 none, 1 INIT, 2..17 the factory sounds (preset k = sound - 1) */
    uint8_t strum;                      /* 0..23 24ths between a chord's notes */
} qd_step_t;

/* the lengths (PLAN §4): 1/8, 1/4, 1/2, 3/4 of a step, 1, 1.5, 2, 3, 4, 6, 8 .. 128 steps (the ".." as 12, 16, 24,
 * 32, 48, 64, 96), INF: eighths of a step */
#define QD_NLEN 20
#define QD_LEN_INF (QD_NLEN - 1)
#define QD_LEN_1 4                      /* one step */
int qd_len_eighths(int len);            /* eighths of a step; -1 for INF */
const char *qd_len_name(int len);       /* "1/8" .. "128", "INF" */

/* conditions (PLAN §4: 44 values) */
enum { QD_COND_ALWAYS, QD_COND_FILL, QD_COND_NFILL, QD_COND_PRE, QD_COND_NPRE, QD_COND_NEI, QD_COND_NNEI,
       QD_COND_1ST, QD_COND_N1ST, QD_COND_AB };
#define QD_NCOND 44                     /* QD_COND_AB .. 43: 1:2, 2:2, 1:3, 2:3, 3:3, .. 8:8 */
int qd_cond_ab(int a, int b);           /* the condition A:B (1 <= a <= b, 2 <= b <= 8); -1 if none */
int qd_cond_ab_of(int cond, int *a, int *b);   /* 1: cond is an A:B (its a, b) */
const char *qd_cond_name(int cond, char *buf);   /* "ALWAYS" .. "3:4" (buf: 8 bytes) */

/* ---- a lock (PLAN §5: 4 bytes) */
typedef struct { uint8_t step, id; int16_t value; } qd_lock_t;
enum { QD_LK_PAN = QD_QUAD_NP, QD_LK_DELAY, QD_LK_REVERB, QD_LK_CHORUS, QD_LK_DRIVE, QD_LK_LEVEL, QD_LK_N };
int qd_lock_range(int id, int16_t *min, int16_t *max);   /* 0: no such id */

/* ---- a track's settings in a pattern (PLAN §4 "track defaults", §6) */
enum { QD_RATE_1_8, QD_RATE_1_4, QD_RATE_1_2, QD_RATE_3_4, QD_RATE_1, QD_RATE_3_2, QD_RATE_2, QD_RATE_2_3,
       QD_RATE_4_3, QD_NRATE };         /* the Digitone's seven and 2/3, 4/3 (PLAN question 14) */
const char *qd_rate_name(int rate);     /* "1/8x" .. "2x" */
typedef struct {
    uint8_t def_note, def_vel, def_len; /* what a new trig gets */
    uint8_t chan;                       /* MIDI channel 0..15 */
    uint8_t limit, reserve;             /* the voices (PLAN §2): 1..16 (1 mono), 0..16 */
    uint8_t rate;                       /* QD_RATE_* */
    uint8_t length;                     /* 1..128 steps */
    uint8_t swing;                      /* swing on (1) / off */
    int8_t transpose;                   /* -48..+48 */
    uint8_t mute;
    uint8_t pad[5];
} qd_tset_t;

/* ---- a pattern */
#define QD_MASTER_AUTO 0                /* master length: the longest track at its rate */
#define QD_MASTER_INF 0xFFFFu           /* .. never: a queued pattern waits for ever, a chain stands */
typedef struct {
    qd_step_t step[QD_PAT_NTRACK][QD_NSTEP];
    qd_tset_t trk[QD_PAT_NTRACK];
    uint16_t lock_start[QD_PAT_NTRACK], lock_n[QD_PAT_NTRACK];
    uint16_t nlock;
    uint16_t master;                    /* QD_MASTER_AUTO, 16..1024 steps of rate 1, QD_MASTER_INF */
    uint8_t swing;                      /* 50..80 % */
    uint8_t used;                       /* the pattern has content */
    uint16_t bpm;                       /* 30..300 */
    qd_lock_t lock[QD_NLOCK];
} qd_pattern_t;

typedef struct {
    qd_pattern_t pat[QD_NPAT];
} qd_project_t;

/* ---- errors */
#define QD_OK 0
#define QD_EARG (-1)                    /* a track, step, id or value out of range */
#define QD_EFULL (-2)                   /* the lock pool is full: nothing changed */
#define QD_ENOTE (-3)                   /* notes 2..4 not within +1..+48 of note 1 */

/* ---- patterns */
void qd_pat_init(qd_pattern_t *p);      /* empty: no trigs, no locks, track defaults, 120 BPM, swing 50, master auto */
void qd_tset_init(qd_tset_t *s, int track);
void qd_project_init(qd_project_t *pr);
const char *qd_pat_name(int k, char *buf);   /* 0 -> "A01" .. 63 -> "D16" (buf: 4 bytes) */
/* the step length of a track in ticks of the sequencer (QD_TPS ticks a 16th, seq.h); a track's loop and the master
 * length likewise */
int64_t qd_rate_ticks(int rate);
int64_t qd_pat_master_ticks(const qd_pattern_t *p);   /* QD_MASTER_INF: 0 */
int qd_pat_check(const qd_pattern_t *p);   /* the pool sorted and counted, the flags right: 1; else 0 (tests) */

/* ---- steps (t 0..7, s 0..127). Each returns QD_OK or an error */
const qd_step_t *qd_step(const qd_pattern_t *p, int t, int s);
int qd_step_type(const qd_step_t *st);
int qd_step_ratchet(const qd_step_t *st);   /* 1..8 */
int qd_step_ramp(const qd_step_t *st);
int qd_step_notes(const qd_step_t *st, uint8_t *notes);   /* the absolute notes (n of them) */
/* a trig: NOTE / LOCK / TIE set (an empty step takes the track's default note, velocity and length), NONE clears the
 * step and its locks */
int qd_step_set_trig(qd_pattern_t *p, int t, int s, int type);
int qd_step_set_notes(qd_pattern_t *p, int t, int s, const uint8_t *notes, int n);   /* absolute, any order */
int qd_step_set_vel(qd_pattern_t *p, int t, int s, int vel);
int qd_step_set_len(qd_pattern_t *p, int t, int s, int len);
int qd_step_set_micro(qd_pattern_t *p, int t, int s, int micro);
int qd_step_set_ratchet(qd_pattern_t *p, int t, int s, int count, int ramp);
int qd_step_set_cond(qd_pattern_t *p, int t, int s, int cond);
int qd_step_set_chance(qd_pattern_t *p, int t, int s, int chance);
int qd_step_set_sound(qd_pattern_t *p, int t, int s, int sound);   /* 0 none, 1 INIT, 2..17 */
int qd_step_set_strum(qd_pattern_t *p, int t, int s, int strum);

/* ---- locks */
int qd_lock_set(qd_pattern_t *p, int t, int s, int id, int value);   /* QD_EFULL: the pool is full */
int qd_lock_get(const qd_pattern_t *p, int t, int s, int id, int16_t *value);   /* 1: found */
int qd_lock_clear(qd_pattern_t *p, int t, int s, int id);   /* QD_OK (none there: also QD_OK) */
int qd_lock_count(const qd_pattern_t *p, int t, int s);
const qd_lock_t *qd_locks(const qd_pattern_t *p, int t, int s, int *n);   /* the step's locks (sorted by id) */

/* ---- clear and copy (a step, a page of 16, a track: its steps and their locks; a track copy also its settings).
 * A copy that would overfill the destination's pool returns QD_EFULL and changes nothing */
int qd_step_clear(qd_pattern_t *p, int t, int s);
int qd_step_copy(qd_pattern_t *dst, int dt, int ds, const qd_pattern_t *src, int st, int ss);
int qd_page_clear(qd_pattern_t *p, int t, int page);
int qd_page_copy(qd_pattern_t *dst, int dt, int dpage, const qd_pattern_t *src, int st, int spage);
int qd_track_clear(qd_pattern_t *p, int t);   /* steps and locks; the settings stay */
int qd_track_copy(qd_pattern_t *dst, int dt, const qd_pattern_t *src, int st);

#ifdef __cplusplus
}
#endif
