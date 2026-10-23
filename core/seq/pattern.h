/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
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
 * Lock ids (PLAN §5): 0..72 the FM TONE patch's values (eng_quad.c QP_*), 73 its DRIVE (P1.4a: the sound's, QD_QP_DRIVE;
 * QD_QUAD_NP of them), then the voice's mix: QD_LK_PAN (-64..63), QD_LK_DELAY, QD_LK_REVERB, QD_LK_CHORUS (the sends,
 * 0..127), QD_LK_LEVEL (the mix level, 0..127, ChoralRoot's LEVEL_Q12 index). */
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "../voice/quad/quad_voice.h"
#include "mfx.h"
#include "../qd_config.h"                 /* (P2.1: QD_PAT_RAM) */

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
#define QD_SOUND_NAME 13                /* (P1.5a: twelve letters and the end: "BRIGHT PLUCK") */
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
#define QD_SF_LOCKS 0x02u               /* .. it has locks in the pool (P2.1c: 0x02 in RAM; the records' 0x80) */
#define QD_RT_COUNT 0x07u               /* ratch: bits 0-2 the count - 1 (x1..x8) */
#define QD_RT_RAMP_SH 4                 /* .. bits 4-5 the ramp */
#define QD_RT_RAMP 0x30u
/* P2.1c: 16 bytes in RAM (the small fields as bit-fields; each holds its whole range), the records' 20-byte layout
 * kept (QD_STEP_WIRE: qd_step_wire / qd_step_unwire, the old struct's bytes) */
typedef struct {
    uint8_t note[QD_NNOTE];             /* P1.3f: note k's pitch in slot k (1..127, 0 none; no two the same) */
    uint8_t vel;                        /* 1..127 */
    uint8_t chance;                     /* 0..100 % */
    uint8_t sound;                      /* the sound lock: 0 none (the track's sound), 1..QD_NSOUND a pool number */
    uint8_t func;                       /* P1.3w: the step function (QD_FN_*; no override bit, no track default: 0
                                         * none) */
    uint16_t ovr : 10;                  /* bit QD_PM_k: the step overrides parameter k (else the track's default) */
    uint16_t type : 2;                  /* QD_TRIG_NOTE / LOCK / TIE (with QD_PM_TRIG's bit) */
    uint16_t flags : 2;                 /* QD_SF_ON, QD_SF_LOCKS */
    uint16_t pad0 : 2;
    uint16_t len : 5;                   /* an index into the length table (qd_len_eighths), QD_LEN_INF the last */
    uint16_t cond : 6;                  /* QD_COND_*, then A:B (qd_cond_ab) */
    uint16_t strum : 5;                 /* 0..23 24ths between a chord's notes */
    int16_t micro : 6;                  /* -23..+23 24ths of a step */
    uint16_t ratch : 6;                 /* QD_RT_*: the count and the ramp */
    uint16_t mfx : 3;                   /* P1.6c: its MIDI FX (no override bit: 0 TRACK, the track's; 1 OFF; 2..6 group
                                         * 1..5; was padding, so 0 in older steps) */
    uint16_t pad1 : 1;
} qd_step_t;
typedef char qd_step_size_ok[sizeof(qd_step_t) == 16 ? 1 : -1];
#define QD_STEP_WIRE 20                 /* a step in a record (the 20-byte layout of P1.3w .. P2.1b) */
void qd_step_wire(const qd_step_t *st, uint8_t *w);     /* into QD_STEP_WIRE bytes */
void qd_step_unwire(qd_step_t *st, const uint8_t *w);   /* .. and back (the fields each to its range) */
enum { QD_SMFX_TRACK, QD_SMFX_OFF, QD_SMFX_G1 };

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
enum { QD_LK_PAN = QD_QUAD_NP, QD_LK_DELAY, QD_LK_REVERB, QD_LK_CHORUS, QD_LK_LEVEL, QD_LK_N };   /* (P1.4a: DRIVE
                                                         * moved into the sound, id 73) */
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
    uint8_t mfx;                        /* P1.6c: its MIDI FX group, 0 OFF, 1..5 (appended: a record of an older layout
                                         * reads 0) */
} qd_tset_t;
#define QD_TSET_SIZE_OLD 42             /* (the record's track settings before P1.6c) */
int qd_tset_rate(const qd_tset_t *ts);  /* the track's rate code */
void qd_tset_set_rate(qd_tset_t *ts, int rate);   /* base and type from a code */

/* ---- P1.5a: a track's sound in a pattern (each pattern keeps its eight tracks' sounds as edited: switching patterns
 * switches them; the player plays a trig with its pattern's track sound) */
typedef struct {
    int16_t patch[QD_QUAD_NP];
    uint8_t sound;                      /* the pool number it came from (0: none, a copy) */
    uint8_t edited;                     /* changed since */
    char name[QD_SOUND_NAME];
    uint16_t preset;                    /* P1.5t: the preset it came from (sounds.h QD_PRESET_*; 0 none) */
} qd_tsound_t;
void qd_tsound_default(int t, qd_tsound_t *s);   /* track t's sound in a new pattern: the default kit's (model.c) */

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
    qd_tsound_t snd[QD_PAT_NTRACK];     /* P1.5a: the tracks' sounds */
} qd_pattern_t;

/* a sound of the pool (or a preset of the library's MINE): an FM TONE patch and its name; P1.5t: the preset it came
 * from (a pool sound; a MY preset: its own id), its flags (QD_SNDF_*); P1.5v: its tags, several kinds (bit QD_PK_*:
 * sounds.h QD_PKB) */
#define QD_SNDF_IMPORTED 0x01           /* a MY preset that came in (SysEx, a dropped .dnsnd), not saved here */
#define QD_SNDF_MISSING 0x02            /* P1.5w: a pool reference whose preset is not there (INIT plays; greyed) */
typedef struct {
    char name[QD_SOUND_NAME];
    int16_t patch[QD_QUAD_NP];
    uint16_t preset;
    uint8_t tags, flags;
    int8_t pan;                         /* P1.5x: its mix, as a track takes it on LOAD: pan -64..63, .. */
    uint8_t chorus, delay, reverb;      /* .. the sends 0..127 (a step's sound lock leaves the track's alone) */
} qd_sound_t;

/* P2.1 (Phase 2: the pool and MY presets in the store): at the device's sizes (QD_SND_RAM) a sound's name, id, tags
 * and flags stay in RAM (what the lists show: qd_sound_dir_t), the sound itself is its record in the store, read into
 * one of QD_SND_RAM places when asked for (sounds.h qd_sound_read); the least recently used place given up, written
 * back first if it changed (io.save). Keys: pool number k (1..QD_NSOUND); MY preset place i: QD_SK_LIB + i. On the
 * host every sound is in RAM, as in Phase 1 (proj.sound, persist.h's lib) */
#define QD_SK_LIB 256
typedef struct {
    char name[QD_SOUND_NAME];
    uint16_t preset;
    uint8_t tags, flags;
} qd_sound_dir_t;
typedef struct {
    void (*load)(void *ctx, int key, qd_sound_t *dst);   /* the sound from its record (none: its default) */
    void (*save)(void *ctx, int key, const qd_sound_t *src);   /* written to its record, if it is not what that holds */
    int (*clean)(void *ctx, int key, const qd_sound_t *s);    /* P2.4: its record holds s as it is (0: none) */
    void (*save_soon)(void *ctx, int key);  /* P2.4: its record wanted written (outside this call) */
    void *ctx;
} qd_snd_io_t;
#if QD_SND_RAM
/* P2.4: reading a sound (qd_sndc_get) never writes: a place whose sound its record does not hold yet is not given up
 * (it is asked to be written soon); every place so: the sound is read into a scratch place, not kept */
typedef struct qd_sndc {
    qd_sound_t s[QD_SND_RAM];
    int16_t key[QD_SND_RAM];            /* (-1: a free place) */
    uint32_t lru[QD_SND_RAM], clock;
    qd_snd_io_t io;
    uint32_t loads, saves;              /* (the budget's counters) */
    qd_sound_t scratch;                 /* P2.4: a sound read with every place waiting to be written */
    uint32_t uncached;                  /* (.. how often) */
} qd_sndc_t;
#endif

/* P2.1 (Phase 2: patterns paged): a project's patterns are read through qd_proj_pat. On the host all 64 are in RAM
 * (QD_PAT_RAM == QD_NPAT: pattern k is pat[k], as in Phase 1); at the device's sizes QD_PAT_RAM slots hold the
 * patterns in use (the playing one, the queued one, one more), the rest are their records in the store: a pattern not
 * in a slot is read in when asked for (its record, else its default: blank, or a demo), the slot it takes given up by
 * the least recently used pattern not pinned (the owner's io.pinned: the sequencer's, the undo's ..), written back
 * first if it changed (io.save). A pointer from qd_proj_pat stays good until another pattern is asked for.
 * P2.4: THE AUDIO PATH NEVER PAGES. While nopage is set (the app's audio side: qd_app_audio, qd_app_midi_play) a
 * pattern not in RAM is not read in: qd_proj_pat gives the playing one's slot instead (pat_fb, else a blank), counted
 * in pat_miss, and the sequencer holds a change until its pattern is in (seq.c). qd_proj_fetch is the main loop's way
 * in: it never writes; a slot whose pattern the store does not hold yet (io.clean) is not given up but asked to be
 * written soon (io.save_soon: the next record qd_persist_step writes), and taken once it is */
typedef struct {
    int (*load)(void *ctx, int k, qd_pattern_t *dst);   /* pattern k from the backing into dst: 1; none there: 0 */
    void (*save)(void *ctx, int k, const qd_pattern_t *src);   /* written back (if it changed) */
    int (*pinned)(void *ctx, int k);    /* k must stay in RAM now */
    int (*clean)(void *ctx, int k, const qd_pattern_t *p);    /* P2.4: the backing holds p as it is (0: none) */
    void (*save_soon)(void *ctx, int k);    /* P2.4: k's record wanted written (outside this call) */
    void *ctx;
} qd_pat_io_t;

typedef struct {
    qd_pattern_t pat[QD_PAT_RAM];       /* (the host: every pattern, pat[k]; the device: the slots) */
#if QD_PAT_RAM < QD_NPAT
    int8_t slot_of[QD_NPAT];            /* the slot holding pattern k (-1 none) */
    int8_t pat_in[QD_PAT_RAM];          /* the pattern in each slot (-1 none) */
    uint32_t lru[QD_PAT_RAM], lru_clock;
    uint8_t pat_used[QD_NPAT];          /* each pattern's used flag while it is out of RAM (the overviews) */
#endif
    uint8_t demo;                       /* P2.1: the patterns' defaults: the demos (A01..A03), else blank */
    qd_pat_io_t io;
    uint32_t pat_loads, pat_saves, pat_forced;   /* (the budget's counters: forced: every slot pinned) */
    uint8_t nopage;                     /* P2.4: the audio side is running: nothing is read in or written (a count) */
    int8_t pat_fb;                      /* P2.4: the pattern a miss plays instead (the sequencer's playing one) */
    uint32_t pat_miss, snd_miss;        /* P2.4: what the audio side asked for and did not find in RAM */
#if QD_SND_RAM
    qd_sound_dir_t sdir[QD_NSOUND];     /* P2.1: the pool's directory (the sounds: sounds.h qd_sound_read) .. */
    struct qd_sndc *sc;                 /* .. the RAM they are read into (the app's; 0: a test's bare project) */
#else
    qd_sound_t sound[QD_NSOUND];        /* the pool: sound number k is sound[k - 1] (sounds.h qd_sound_read) */
#endif
    uint8_t nsound;                     /* sounds in it (1..QD_NSOUND) */
    uint8_t key_scale, key_root;        /* the global key (the tracks that follow it) */
    uint8_t launch;                     /* P1.3c: the pattern launch mode (seq.h QD_LAUNCH_*: END at power-on) */
    /* P1.3y: the record settings (SHIFT + REC), the project's */
    uint8_t rec_mode;                   /* what holding REC arms: 0 LIVE, 1 STEP */
    uint8_t rec_quant;                  /* live recording's quantize, 0..100 % (100: on the step, the micro-timing gone) */
    uint8_t rec_dub;                    /* 0 REPLACE (a step recorded into is cleared first), 1 OVERDUB (the notes join) */
    uint8_t pool_empty;                 /* P1.5t: its pool started empty (a new project's; 0.6's and the demo's: the old
                                         * starting pool) */
    uint8_t rec_ekeys;                  /* P1.5b: the keyboard in the sound editor: 0 SCALE (W the scale, B the pages), 1
                                         * FULL (every key as the keyboard; the pages on SELECT) */
    qd_mfx_group_t mfx[QD_MFX_NGROUP];  /* P1.6c: the MIDI FX groups (mfx.h) */
    uint8_t rec_auto;                   /* P1.6d: AUTOMATION: armed live recording records knob moves too (1 ON) */
} qd_project_t;
/* the MIDI FX group a step of track t plays through: 0 none, 1..5 (the step's, else the track's) */
int qd_step_mfx(const qd_pattern_t *p, int t, int s);

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
/* P2.1: pattern k (0..63), in RAM (read in if it was not); its used flag (without reading it in); in RAM now; every
 * slot forgotten (a project read: the backing holds them) or written back; a pattern's default (blank, or a demo) */
qd_pattern_t *qd_proj_pat(qd_project_t *pr, int k);
const qd_pattern_t *qd_proj_cpat(const qd_project_t *pr, int k);
int qd_proj_used(const qd_project_t *pr, int k);
int qd_proj_resident(const qd_project_t *pr, int k);
int qd_proj_fetch(qd_project_t *pr, int k);   /* P2.4: pattern k into RAM without writing anything: 1 in, 0 not yet */
void qd_proj_drop(qd_project_t *pr);
void qd_proj_save_all(qd_project_t *pr);
void qd_pat_default(const qd_project_t *pr, int k, qd_pattern_t *dst);
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
/* P2.1: a step resolved against a track's defaults (no pattern needed: the clipboard's step, the defaults' picture) */
void qd_step_resolve_def(const qd_sdef_t *d, const qd_step_t *st, qd_rstep_t *r);
/* P2.1: the one scratch pattern (the device has room for one, not seven): for a computation that needs a whole
 * pattern for a moment; never held across calls, never by two at once. P2.1b: borrowed and given back; at the device's
 * sizes it is the drawing's arena's room between frames (no RAM of its own), 0 when the arena has none (never in a
 * drawing). qd_tmp_borrow: the same for any size */
qd_pattern_t *qd_pat_borrow(void);
void qd_pat_return(qd_pattern_t *p);
void *qd_tmp_borrow(size_t n);
void qd_tmp_return(void *p);
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
int qd_step_set_mfx(qd_pattern_t *p, int t, int s, int m);   /* P1.6c: QD_SMFX_* (TRACK, OFF, group 1..5; not TRACK on an
                                                              * empty step: a trig made) */
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
