/* SPDX-License-Identifier: GPL-3.0-only */
/* The pattern model (PLAN.md §4, §5, §6, §8b): steps, the lock pool, the tracks' settings and defaults, pages, a
 * pattern, a project of 64 with its sound pool and global key; and the editing API the UI and the tests call. Plain C
 * structs, no allocation, nothing of the OS: a pattern can be stored as it is (P1.5).
 *
 * P1.3b (PLAN §8b, "track defaults and step overrides"): a track has DEFAULTS for every step parameter (the notes, up
 * to six since P1.3e (§8b version 2.2; five in P1.3c, four before); velocity, length, micro, ratchet and ramp, chance, condition, trig type, strum; the sound default is always
 * the track's own sound). A step stores a value AND AN OVERRIDE BIT per parameter (qd_step_t.ovr, bit QD_PM_k); a
 * parameter without its bit takes the track's default WHEN THE STEP IS RESOLVED (qd_step_resolve: the sequencer two
 * steps ahead, the screens at each frame), so a default change moves every step that does not override it, at once.
 * A tapped step (qd_step_on) has no overrides. Every setter sets its value and its bit; qd_step_reset clears a bit (the
 * held step's B11). An empty step set a value becomes a trig (Form's rule: setting a value turns the step on).
 *
 * PAGES (§8b): a track has 8 pages of 16 steps, each with a start and an end (0..15 here, start <= end; 1..16 on the
 * screen; default the whole page) and a loop mask (bit p: page p in the loop; default page 1 only; never empty: page 1
 * then). Playback walks the looped pages in ascending order, each from its start to its end (qd_track_len,
 * qd_track_loop_step): this replaces §6's track length. qd_track_set_length is Form's flat rebuild (pages 1..k looped,
 * full but the last).
 *
 * THE SOUND POOL (§8b): a project holds up to QD_NSOUND sounds (sounds.h): INIT, the 16 factory sounds, the demo drums.
 * A step's sound lock is one byte, a pool number (1..QD_NSOUND; 0 = the track's own sound); its p-locks apply on top.
 *
 * SCALES (scale.h): a track has a scale and a root (chromatic, C by default) or follows the project's global key.
 *
 * A pattern's p-locks are one pool of QD_NLOCK lock_t {step, id, value}, sorted by track, then step, then id (a
 * track's range: lock_start[t], lock_n[t]); a step's QD_SF_LOCKS says it has any. The editing API keeps all of that
 * consistent: a full pool returns QD_EFULL and nothing is changed.
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
#define QD_NPAGE (QD_NSTEP / QD_PAGE)
#define QD_NNOTE 6                      /* notes a step (P1.3e: six; P1.3c five, P1.3b four) */
#define QD_NLOCK 1024                   /* the lock pool a pattern */
#define QD_NBANK 4                      /* A..D */
#define QD_NPAT (QD_NBANK * 16)         /* patterns a project */
#define QD_NSOUND 128                   /* the sound pool a project */
#define QD_SOUND_NAME 12
/* the sequencer's musical time: ticks, QD_TPS a 16th (a step at rate 1). 100800 = 2^6 3^2 5^2 7 (P1.3c: twice
 * P1.2's 50400, for the 1/64 steps; QD_SUB halved with it, so a 16th is the same 5,292,000,000 subticks and every
 * event the same sample): a step at every rate (1/1 .. 1/64, dotted, triplet) divides by 24 (micro, strum), 1..8
 * (ratchets), 8 (the shortest length) and 50 (swing 50..80 %) in whole ticks, so every time the sequencer computes is
 * exact */
#define QD_TPS 100800

/* ---- the step parameters (PLAN §8b): each has a track default and an override bit on a step */
enum { QD_PM_NOTE, QD_PM_VEL, QD_PM_LEN, QD_PM_MICRO, QD_PM_RATCH, QD_PM_CHANCE, QD_PM_COND, QD_PM_TRIG, QD_PM_SOUND,
       QD_PM_STRUM, QD_NPM };
#define QD_OV(pm) ((uint16_t)(1u << (pm)))

/* ---- a step (20 bytes since P1.3w: the step function and a pad; 18 since P1.3e, six notes; P1.3c's five notes and a
 * pad, P1.3b's four in 16) */
enum { QD_TRIG_NONE, QD_TRIG_NOTE, QD_TRIG_LOCK, QD_TRIG_TIE };
enum { QD_RAMP_FLAT, QD_RAMP_UP, QD_RAMP_DOWN, QD_RAMP_ACCENT };
#define QD_SF_ON 0x01u                  /* flags: the step has a trig */
#define QD_SF_LOCKS 0x80u               /* .. it has locks in the pool */
#define QD_RT_COUNT 0x07u               /* ratch: bits 0-2 the count - 1 (x1..x8) */
#define QD_RT_RAMP_SH 4                 /* .. bits 4-5 the ramp */
#define QD_RT_RAMP 0x30u
typedef struct {
    uint16_t ovr;                       /* bit QD_PM_k: the step overrides parameter k (else the track's default) */
    uint8_t flags;                      /* QD_SF_ON, QD_SF_LOCKS */
    uint8_t type;                       /* QD_TRIG_NOTE / LOCK / TIE (with QD_PM_TRIG's bit) */
    uint8_t note[QD_NNOTE];             /* P1.3f: note k's pitch in slot k (1..127, 0 none; no two the same) */
    uint8_t vel;                        /* 1..127 */
    uint8_t len;                        /* an index into the length table (qd_len_eighths), QD_LEN_INF the last */
    int8_t micro;                       /* -23..+23 24ths of a step */
    uint8_t ratch;                      /* QD_RT_*: the count and the ramp */
    uint8_t chance;                     /* 0..100 % */
    uint8_t cond;                       /* QD_COND_*, then A:B (qd_cond_ab) */
    uint8_t sound;                      /* the sound lock: 0 none (the track's sound), 1..QD_NSOUND a pool number */
    uint8_t strum;                      /* 0..23 24ths between a chord's notes */
    uint8_t func;                       /* P1.3w: the step function (QD_FN_*; no override bit, no track default: 0
                                         * none) */
    uint8_t pad;
} qd_step_t;

/* P1.3w (PLAN §8b "FUNC on B9", Form's FUNCTION): what a step does to its track's playing when it fires (after its
 * condition and chance): RESTART (the next step is the loop's start, the passes from 0), REVERSE / FORWARD (the
 * track's direction), FLIP it, RANDOM JUMP (a random step of the loop), RANDOM FUNCTION (one of none / restart /
 * reverse / forward / flip, rolled each time), JUMP n (the next step is the track's step n: QD_FN_JUMP + n, n 0..127;
 * a step outside the loop: the loop's start). P1.3y: LEAP n (n -15..-1, 1..15: the next position is n on from the
 * step's in the playing direction, round the loop; QD_FN_LEAP + 15 + n) */
enum { QD_FN_NONE, QD_FN_RESTART, QD_FN_REVERSE, QD_FN_FORWARD, QD_FN_FLIP, QD_FN_RJUMP, QD_FN_RFUNC, QD_FN_JUMP };
#define QD_FN_LEAP (QD_FN_JUMP + QD_NSTEP)
#define QD_NFN (QD_FN_LEAP + 31)
#define QD_FN_IS_JUMP(f) ((f) == QD_FN_RJUMP || ((f) >= QD_FN_JUMP && (f) < QD_FN_LEAP))
#define QD_FN_IS_LEAP(f) ((f) >= QD_FN_LEAP && (f) < QD_NFN && (f) != QD_FN_LEAP + 15)
#define QD_FN_LEAP_N(f) ((f) - QD_FN_LEAP - 15)   /* a LEAP's n */
#define QD_FN_MAKE_LEAP(n) (QD_FN_LEAP + 15 + (n))
#define QD_FN_MAKE_JUMP(s) (QD_FN_JUMP + (s))
const char *qd_fn_name(int f, char *out);   /* "NONE", "RESTART", .., "JUMP 17" (out: 12 bytes) */

/* a track's step defaults (the sound's default is the track's own sound: no field) */
typedef struct {
    uint8_t note[QD_NNOTE];             /* as a step's */
    uint8_t vel, len;
    int8_t micro;
    uint8_t ratch, chance, cond, type, strum;
} qd_sdef_t;

/* a step as it plays: its values, each the step's own or the track's default */
typedef struct {
    int on;                             /* a trig */
    int type;                           /* QD_TRIG_* (NONE when not on) */
    int nn;                             /* notes */
    uint8_t notes[QD_NNOTE];            /* absolute, ascending (above 127 clamped) */
    uint8_t note[QD_NNOTE];             /* as stored: the slots' pitches (note k in slot k, 0 none) */
    int vel, len, micro, ratch, ramp, chance, cond, sound, strum;
    int func;                           /* P1.3w: the step function (the step's own: no default) */
    uint16_t ovr;
} qd_rstep_t;

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

/* ---- a track's settings in a pattern (PLAN §4 "track defaults", §6, §8b) */
/* P1.3c (§8b 2.1, F3's knobs and keys): a track's rate is TWO settings, kept apart (qd_tset_t.rbase, .rtype): a BASE
 * note value, the step's length (0 1/1, 1 1/2, 2 1/4, 3 1/8, 4 1/16 = P1.3b's rate 1, 5 1/32, 6 1/64), and a TYPE
 * (0 normal; 1 dotted: the step 3/2 as long, the rate x 2/3; 2 triplet: the step 2/3 as long, the rate x 3/2).
 * Choosing a base keeps the type and the other way round. A RATE CODE (QD_RATE_*) is base x 3 + type: 21 of them, each
 * step a whole number of ticks divisible by 24 (micro, strum), 1..8 (ratchets), 8 (the shortest length) and 50 (swing)
 * since QD_TPS is 100800 (the shortest, triplet 1/64, is 16800; dotted 1/64 is 37800). P1.3b's nine rates are codes
 * with their old names: 1/8x = 1/2, 1/4x = 1/4, 1/2x = 1/8, 1x = 1/16, 2x = 1/32, 2/3x = dotted 1/16, 4/3x = dotted
 * 1/32, 3/2x = triplet 1/16, 3/4x = triplet 1/8 */
#define QD_NVAL 7                       /* the bases */
#define QD_NRTYPE 3
enum { QD_RTYPE_NORMAL, QD_RTYPE_DOTTED, QD_RTYPE_TRIPLET };
#define QD_RATE_CODE(base, type) ((base) * 3 + (type))
#define QD_NRATE (QD_NVAL * QD_NRTYPE)
enum { QD_RATE_1_8 = QD_RATE_CODE(1, 0), QD_RATE_1_4 = QD_RATE_CODE(2, 0), QD_RATE_1_2 = QD_RATE_CODE(3, 0),
       QD_RATE_1 = QD_RATE_CODE(4, 0), QD_RATE_2 = QD_RATE_CODE(5, 0), QD_RATE_2_3 = QD_RATE_CODE(4, 1),
       QD_RATE_4_3 = QD_RATE_CODE(5, 1), QD_RATE_3_2 = QD_RATE_CODE(4, 2), QD_RATE_3_4 = QD_RATE_CODE(3, 2) };
const char *qd_rate_name(int rate);     /* "1/16", "1/8." (dotted), "1/16T" (triplet) */
int qd_rate_make(int base, int type);   /* the code of a base and a type (clamped) */
int qd_rate_val(int rate);              /* a code's base .. */
int qd_rate_type(int rate);             /* .. and type */
const char *qd_rate_val_name(int base); /* "1/1" .. "1/64" */
typedef struct {
    qd_sdef_t def;                      /* the step defaults (§8b) */
    uint8_t chan;                       /* MIDI channel 0..15 */
    uint8_t limit, reserve;             /* the voices (PLAN §2): 1..16 (1 mono), 0..16 */
    uint8_t rbase;                      /* the rate's base note value (0 1/1 .. 6 1/64; 4 1/16 the default) .. */
    uint8_t swing;                      /* swing on (1) / off */
    int8_t transpose;                   /* -48..+48 */
    uint8_t mute;
    uint8_t loop;                       /* bit p: page p in the loop (never 0 when read: page 1 then) */
    uint8_t pstart[QD_NPAGE], pend[QD_NPAGE];   /* each page's first and last step, 0..15, start <= end */
    uint8_t scale, root;                /* the track's key (scale.h: QD_SCALE_*, root 0..11 = C..B) */
    uint8_t key_global;                 /* 1: the track follows the project's global key */
    uint8_t rtype;                      /* .. and its type (QD_RTYPE_*), kept apart (P1.3c; was padding) */
} qd_tset_t;
int qd_tset_rate(const qd_tset_t *ts);  /* the track's rate code */
void qd_tset_set_rate(qd_tset_t *ts, int rate);   /* base and type from a code */

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

/* a sound of the pool: an FM TONE patch and its name */
typedef struct {
    char name[QD_SOUND_NAME];
    int16_t patch[QD_QUAD_NP];
} qd_sound_t;

typedef struct {
    qd_pattern_t pat[QD_NPAT];
    qd_sound_t sound[QD_NSOUND];        /* the pool: sound number k is sound[k - 1] */
    uint8_t nsound;                     /* sounds in it (1..QD_NSOUND) */
    uint8_t key_scale, key_root;        /* the global key (the tracks that follow it) */
    uint8_t launch;                     /* P1.3c: the pattern launch mode (seq.h QD_LAUNCH_*: END at power-on) */
    /* P1.3y: the record settings (SHIFT + REC), the project's */
    uint8_t rec_mode;                   /* what holding REC arms: 0 LIVE, 1 STEP */
    uint8_t rec_quant;                  /* live recording's quantize, 0..100 % (100: on the step, the micro-timing gone) */
    uint8_t rec_dub;                    /* 0 REPLACE (a step recorded into is cleared first), 1 OVERDUB (the notes join) */
} qd_project_t;

/* ---- errors */
#define QD_OK 0
#define QD_EARG (-1)                    /* a track, step, id or value out of range */
#define QD_EFULL (-2)                   /* the lock pool is full: nothing changed */
#define QD_ENOTE (-3)                   /* a pitch another note of the step already has */

/* ---- patterns */
void qd_pat_init(qd_pattern_t *p);      /* empty: no trigs, no locks, track defaults, 120 BPM, swing 50, master auto */
void qd_tset_init(qd_tset_t *s, int track);
void qd_sdef_init(qd_sdef_t *d);        /* C4, velocity 100, a step, micro 0, x1, 100 %, ALWAYS, NOTE, strum 0 */
void qd_project_init(qd_project_t *pr); /* empty patterns, the pool's first sounds (sounds.h), the key C chromatic */
const char *qd_pat_name(int k, char *buf);   /* 0 -> "A01" .. 63 -> "D16" (buf: 4 bytes) */
/* the step length of a track in ticks of the sequencer (QD_TPS ticks a 16th, seq.h); a track's loop and the master
 * length likewise */
int64_t qd_rate_ticks(int rate);
int64_t qd_pat_master_ticks(const qd_pattern_t *p);   /* QD_MASTER_INF: 0 */
int qd_pat_check(const qd_pattern_t *p);   /* the pool sorted and counted, the flags right: 1; else 0 (tests) */

/* ---- pages and the loop (§8b) */
int qd_track_loop(const qd_tset_t *ts);  /* the loop mask, never 0 */
int qd_track_len(const qd_tset_t *ts);   /* the loop's steps: the looped pages' ranges summed (1..128) */
int qd_track_loop_step(const qd_tset_t *ts, int pos);   /* loop position pos (wrapped) -> its step 0..127 */
int qd_track_step_pos(const qd_tset_t *ts, int s);      /* step s -> its loop position, -1: not in the loop */
int qd_page_in_range(const qd_tset_t *ts, int s);       /* step s within its page's start..end */
int qd_page_set_range(qd_pattern_t *p, int t, int page, int start, int end);   /* 0..15 each, start <= end */
int qd_page_set_loop(qd_pattern_t *p, int t, int mask);  /* (0: page 1) */
int qd_track_set_length(qd_pattern_t *p, int t, int n);   /* Form's flat rebuild: pages 1..k, full but the last */

/* ---- steps (t 0..7, s 0..127). Each returns QD_OK or an error */
const qd_step_t *qd_step(const qd_pattern_t *p, int t, int s);
int qd_step_is_on(const qd_step_t *st);
int qd_step_overrides(const qd_step_t *st, int pm);
/* the step as it plays (its own values where it overrides, else the track's defaults) */
void qd_step_resolve(const qd_pattern_t *p, int t, int s, qd_rstep_t *r);
int qd_step_type(const qd_pattern_t *p, int t, int s);   /* resolved: QD_TRIG_NONE when off */
int qd_step_notes(const qd_pattern_t *p, int t, int s, uint8_t *notes);   /* resolved: the absolute notes (n) */
int qd_notes_abs(const uint8_t *note6, uint8_t *notes);   /* the slots' pitches -> ascending (n) */
/* a trig with no overrides (the tap); NOTE / LOCK / TIE: a trig with that type overridden; NONE: the step and its
 * locks cleared */
int qd_step_on(qd_pattern_t *p, int t, int s);
int qd_step_set_trig(qd_pattern_t *p, int t, int s, int type);
/* notes[0..n): note 1, note 2 .. in that order (P1.3f: the slots keep it; repeats dropped) */
int qd_step_set_notes(qd_pattern_t *p, int t, int s, const uint8_t *notes, int n);
/* P1.3f: note k (slot 0..5) set to a pitch (0: none; a pitch another slot has: QD_ENOTE; the last note emptied: QD_EARG),
 * the others where they are. Without the NOTE override, the track's default notes are its start */
int qd_step_set_note(qd_pattern_t *p, int t, int s, int k, int pitch);
/* .. or all six slots as they are (pitches, 0 none; repeats: QD_ENOTE; none at all: QD_EARG) */
int qd_step_set_slots(qd_pattern_t *p, int t, int s, const uint8_t *slots);
int qd_step_set_vel(qd_pattern_t *p, int t, int s, int vel);
int qd_step_set_len(qd_pattern_t *p, int t, int s, int len);
int qd_step_set_micro(qd_pattern_t *p, int t, int s, int micro);
int qd_step_set_ratchet(qd_pattern_t *p, int t, int s, int count, int ramp);
int qd_step_set_cond(qd_pattern_t *p, int t, int s, int cond);
int qd_step_set_chance(qd_pattern_t *p, int t, int s, int chance);
int qd_step_set_sound(qd_pattern_t *p, int t, int s, int sound);   /* 0 (the track's: the override cleared) .. 128 */
int qd_step_set_strum(qd_pattern_t *p, int t, int s, int strum);
int qd_step_set_func(qd_pattern_t *p, int t, int s, int func);   /* P1.3w: 0..QD_NFN-1 (an empty step: a trig made) */
int qd_step_reset(qd_pattern_t *p, int t, int s, int pm);   /* parameter pm back to the track's (the bit cleared) */
/* the resolved ratchet of a step's stored byte / of a default */
int qd_ratch_count(int ratch);
int qd_ratch_ramp(int ratch);
int qd_ratch_make(int count, int ramp);

/* ---- the track's defaults (§8b: ALGORITHM with nothing held). A default change reaches every step that does not
 * override it, at once */
qd_sdef_t *qd_tdef(qd_pattern_t *p, int t);
int qd_tdef_set_notes(qd_pattern_t *p, int t, const uint8_t *notes, int n);
int qd_tdef_set_note(qd_pattern_t *p, int t, int k, int pitch);   /* P1.3f: one slot, as qd_step_set_note */
int qd_tdef_set_slots(qd_pattern_t *p, int t, const uint8_t *slots);   /* .. all six, as qd_step_set_slots */
int qd_tdef_set(qd_pattern_t *p, int t, int pm, int value);   /* VEL .. STRUM (RATCH: qd_ratch_make; NOTE: note 1) */
int qd_tdef_get(const qd_pattern_t *p, int t, int pm);

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
/* raw: steps [s, s + n) of track t out (their locks with steps relative to s: lk[] room for QD_NLOCK), and back in
 * (QD_EFULL: nothing changed). clip.c's clipboards and undo are built on these */
int qd_steps_get(const qd_pattern_t *p, int t, int s, int n, qd_step_t *steps, qd_lock_t *lk, int *nlk);
int qd_steps_put(qd_pattern_t *p, int t, int s, int n, const qd_step_t *steps, const qd_lock_t *lk, int nlk);

#ifdef __cplusplus
}
#endif
