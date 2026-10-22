/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.6c: MIDI FX groups (PLAN.md "MIDI FX groups"; research/OMX-MIDIFX.md, the OMX-27 v1.15.4 mechanics).
 *
 * THE MODEL. Five groups of eight slots, kept with the project (qd_project_t.mfx). A slot holds one of ten effects or
 * nothing; a note entering a group passes its slots in order (an empty or bypassed slot is skipped), then leaves for
 * the voices. A track routes through one group or none (qd_tset_t.mfx: 0 OFF, 1..5), a step can override it
 * (qd_step_t.mfx: 0 TRACK, 1 OFF, 2..6 group 1..5); two tracks may share a group (their notes meet in one arp, each
 * sounding on its own track).
 *
 * THE NOTES. A note in the chain carries its track, its pitch, the pitch it ENTERED the group with (prev), its
 * velocity, its context (the pattern and step it came from: the voice's sound and locks; -1: a key, the track's
 * sound) and a length: 0 for a held note (its OFF comes as its own event: a key, a sequencer note) or the ticks a
 * note made by the chain lasts (ARP, REPEAT). As the OMX's: the group keeps a table of the notes it sent, by (track,
 * prev), and an OFF leaving the chain releases every note its input made, so no effect handles another's offs; the
 * timed notes end on their own. A held note's known length (the sequencer's gate) rides along as glen, for RANDOM's
 * LENGTH.
 *
 * THE EFFECTS (every one with a CHANCE: KNOB 4 on its first page; a miss lets the note pass untouched):
 *   ARP        the held notes as a pattern (17: UP .. AS PLAYED) over RANGE octaves OCT DIST apart, a step every RATE,
 *              GATE long, from the QUANT grid; MODE OFF / ON / 1-ST / ONCE / HOLD; the MOD lane (16 steps: as
 *              played, rest, tie, repeat, low -oct, high +oct, power chord, chord, note 1..6) and the TRANSPOSE lane
 *              (-48..+48), each its own length; RESET NORM / NOTE / MPAT / TPAT. Velocity: the first held note's;
 *              each note on its own track. A new arp starts ON (the OMX's starts OFF)
 *   CHANCE     lets a note through or drops it (100 % at first: the OMX's starts random)
 *   REPEAT     note repeat: RATE, HITS, FADE (the velocity over the hits), GATE, QUANT; MODE OFF / ON (while held) /
 *              1-ST (HITS, even after release) / ONCE (at most HITS) / HOLD (latched: the next note after every key is
 *              up replaces the set)
 *   HARM       the note (ORIG on / off) and up to six more at fixed intervals (duplicates among them skipped)
 *   TRANSPOSE  SEMI and OCT; a result outside 0..127 dropped
 *   SCALER     snapped down onto SCALE / ROOT (KEY OWN) or the track's key (KEY TRACK); outside 0..127 dropped
 *   RANDOM     PITCH +-, VEL +-, LENGTH + (a held note's OFF later by up to that much of its length; lengths only
 *              grow; a key unaffected), OCT +-, DELAY (a random wait up to 1/64 .. 1/1 of a bar; a released note
 *              cancels its own delayed start)
 *   CHORD      the note as the root of a shape (chord.h's), NOTES of it (more than the shape: doubled an octave up),
 *              INVERT, SPREAD, VOICING (CLOSE, DROP 2, DROP 3, OPEN); the played velocity kept (the OMX's is 100)
 *   MONO       last-note priority per track: a new note ends the track's one before
 *   SELECTOR   each note to ONE of the next LENGTH slots (RND / UP / DOWN), the others skipped for it; its OFF to all of
 *              them
 *
 * TIME. Ticks (pattern.h QD_TPS a 16th), the player's clock (the transport's while playing, the tempo's while
 * stopped): qd_mfx_note at a tick, qd_mfx_advance to a tick (the arps, repeats, delays and the timed notes' ends due
 * before it, in time order). A bar is 16 x QD_TPS; RATE is the OMX's table (1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24,
 * 32, 40, 48, 64 a bar).
 *
 * SIZE (the FM-1's: fixed tables, no allocation): a group's settings 8 x qd_mfx_slot_t (58 bytes); its run-time
 * state qd_mfx_grt_t (sizeof: the report's "RAM per group"). Plain C, nothing of the OS; deterministic (its own
 * xorshift, seeded). */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QD_MFX_NGROUP 5
#define QD_MFX_NSLOT 8
#define QD_MFX_NV 12                    /* a slot's values */
#define QD_MFX_LANE 16                  /* the ARP's lanes' steps */
#define QD_MFX_NOUT 32                  /* a group's held notes sent (the OMX's 32) */
#define QD_MFX_NPEND 64                 /* .. its timed notes sounding */
#define QD_MFX_NHOLD 8                  /* notes an ARP / REPEAT / RANDOM holds */
#define QD_MFX_NRATE 16
#define QD_MFX_BAR (16LL * 100800LL)    /* a bar in ticks (16 x QD_TPS) */

enum { QD_MFX_NONE, QD_MFX_ARP, QD_MFX_CHANCE, QD_MFX_REPEAT, QD_MFX_HARM, QD_MFX_TRANSPOSE, QD_MFX_SCALER,
       QD_MFX_RANDOM, QD_MFX_CHORD, QD_MFX_MONO, QD_MFX_SELECTOR, QD_MFX_NTYPE };

/* the values (qd_mfx_slot_t.v) by type */
enum { QD_MA_PAT, QD_MA_RATE, QD_MA_RANGE, QD_MA_CHANCE, QD_MA_MODE, QD_MA_GATE, QD_MA_ODIST, QD_MA_QUANT, QD_MA_RESET,
       QD_MA_MLEN, QD_MA_TLEN, QD_MA_LAST };                         /* ARP (LAST: the mode ARP's tap brings back) */
enum { QD_MC_CHANCE };                                                /* CHANCE */
enum { QD_MR_RATE, QD_MR_HITS, QD_MR_FADE, QD_MR_CHANCE, QD_MR_MODE, QD_MR_GATE, QD_MR_QUANT };   /* REPEAT */
enum { QD_MH_N2, QD_MH_N3, QD_MH_N4, QD_MH_CHANCE, QD_MH_N5, QD_MH_N6, QD_MH_N7, QD_MH_ORIG };    /* HARM */
enum { QD_MT_SEMI, QD_MT_OCT, QD_MT_CHANCE = 3 };                    /* TRANSPOSE */
enum { QD_MZ_SCALE, QD_MZ_ROOT, QD_MZ_KEY, QD_MZ_CHANCE };           /* SCALER (KEY 0 OWN, 1 TRACK) */
enum { QD_MD_PITCH, QD_MD_VEL, QD_MD_LEN, QD_MD_CHANCE, QD_MD_OCT, QD_MD_DELAY };   /* RANDOM */
enum { QD_MK_SHAPE, QD_MK_NOTES, QD_MK_INV, QD_MK_CHANCE, QD_MK_SPREAD, QD_MK_VOICING };   /* CHORD */
enum { QD_MM_CHANCE = 3 };                                            /* MONO */
enum { QD_ML_MODE, QD_ML_LEN, QD_ML_CHANCE = 3 };                    /* SELECTOR */
/* ARP / REPEAT modes; ARP patterns; RESET; the MOD lane's values; SELECTOR's modes; CHORD's voicings */
enum { QD_AM_OFF, QD_AM_ON, QD_AM_1ST, QD_AM_ONCE, QD_AM_HOLD, QD_NAM };
enum { QD_AP_UP, QD_AP_DN, QD_AP_UPDN, QD_AP_DNUP, QD_AP_UANDD, QD_AP_DANDU, QD_AP_CON, QD_AP_DIV, QD_AP_CONDIV,
       QD_AP_HI1, QD_AP_HI2, QD_AP_LO1, QD_AP_LO2, QD_AP_RAND, QD_AP_ROTH, QD_AP_RONC, QD_AP_ASP, QD_NAP };
enum { QD_AR_NORM, QD_AR_NOTE, QD_AR_MPAT, QD_AR_TPAT, QD_NAR };
enum { QD_AL_PLAY, QD_AL_REST, QD_AL_TIE, QD_AL_REPEAT, QD_AL_LO, QD_AL_HI, QD_AL_PWR, QD_AL_CHORD, QD_AL_N1,
       QD_NAL = QD_AL_N1 + 6 };
enum { QD_SM_RND, QD_SM_UP, QD_SM_DOWN };
enum { QD_CV_CLOSE, QD_CV_DROP2, QD_CV_DROP3, QD_CV_OPEN, QD_NCV };

/* a slot (58 bytes): its effect, bypassed or not, its values, the ARP's lanes */
typedef struct {
    uint8_t type;                       /* QD_MFX_* */
    uint8_t bypass;                     /* 1: the notes pass it by (saved) */
    int16_t v[QD_MFX_NV];
    int8_t lane[2][QD_MFX_LANE];        /* ARP: 0 the MOD lane (QD_AL_*), 1 the TRANSPOSE lane (-48..48) */
} qd_mfx_slot_t;
typedef struct {
    qd_mfx_slot_t slot[QD_MFX_NSLOT];
} qd_mfx_group_t;

/* ---- the values: names, ranges, pages */
enum { QD_MFX_KNUM, QD_MFX_KSIGN, QD_MFX_KPCT, QD_MFX_KRATE, QD_MFX_KQUANT, QD_MFX_KENUM, QD_MFX_KROOT, QD_MFX_KSCALE,
       QD_MFX_KSHAPE, QD_MFX_KONOFF, QD_MFX_KPM, QD_MFX_KDELAY };
typedef struct {
    const char *name;                   /* "PATTERN" */
    int16_t lo, hi, def;
    uint8_t kind;                       /* QD_MFX_K* (how it reads) */
    uint8_t step;                       /* a detent's (1, 5) */
} qd_mfx_par_t;
#define QD_MFX_ID_LANE 100              /* a page's knob: the ARP's lane step selected (page 3: MOD, 4: TRANSPOSE) */
const char *qd_mfx_type_name(int type); /* "NONE", "ARP" .. "SELECTOR" */
int qd_mfx_npages(int type);            /* 1..4 (NONE: 0) */
int qd_mfx_page_id(int type, int page, int k);   /* KNOB k+1's value on that page: an index of v, QD_MFX_ID_LANE, -1 */
const qd_mfx_par_t *qd_mfx_par(int type, int id);   /* 0: none */
int qd_mfx_chance_id(int type);         /* its CHANCE's index of v */
void qd_mfx_slot_init(qd_mfx_slot_t *s, int type);   /* the type at its defaults (NONE: empty) */
int qd_mfx_rate_of(int idx);            /* a RATE index -> notes a bar (1 .. 64) */
const char *qd_mfx_value_text(const qd_mfx_slot_t *s, int id, char *buf);   /* "UP", "1/16", "+12", "100%" (12 bytes) */
const char *qd_mfx_lane_text(int lane, int v, char *buf);   /* MOD: "x" "." "-" "R" "<" ">" "\"" "#" "1".."6"; TR: "+7" */
int qd_mfx_group_active(const qd_mfx_group_t *g);   /* a slot with an effect that is not bypassed */
int qd_mfx_selector_of(const qd_mfx_group_t *g, int k);   /* the SELECTOR whose block holds slot k (-1 none) */

/* ---- the notes */
typedef struct {
    uint8_t track, pitch, prev, vel;
    int16_t pat, step;                  /* the context: the trig's (step -1: a key, the track's sound) */
    int32_t len;                        /* ticks (a timed note), 0 held */
    int32_t glen;                       /* a held note's known length (the sequencer's gate), 0 unknown */
    uint8_t live;                       /* a key's (else the sequencer's) */
    uint8_t gen;                        /* made by an ARP / REPEAT (a lineage of its own: an OFF of an input never ends
                                         * it; (track, prev, gen) is a note's identity in the chain) */
    const void *rec;                    /* the trig's record while it is being played (the player's; else 0) */
} qd_mfx_note_t;

/* the output: a note on (n.pitch on n.track; n.len > 0: it will end by itself), a note off */
typedef struct {
    void (*on)(void *ctx, int g, const qd_mfx_note_t *n);
    void (*off)(void *ctx, int g, int track, int pitch);
    void (*key)(void *ctx, int track, int *scale, int *root);   /* SCALER's KEY TRACK */
    void *ctx;
} qd_mfx_io_t;

/* ---- the run-time state (not saved) */
typedef struct {
    qd_mfx_note_t n;
    int64_t until;                      /* a timed input's end (0: until its OFF) */
    int64_t next;                       /* REPEAT: the next hit; RANDOM: when it leaves */
    uint16_t hits;
    uint8_t used, released, on;         /* (RANDOM: on 1 a delayed start, 0 a delayed end) */
} qd_mfx_held_t;
typedef struct {
    uint8_t type, bypass;               /* what it was set up for (another: reset) */
    qd_mfx_held_t h[QD_MFX_NHOLD];
    uint8_t order[QD_MFX_NHOLD];        /* ARP: the held notes, by pitch */
    uint8_t seq[QD_MFX_NHOLD * 4];      /* .. the pattern's walk (indices into order) */
    int nseq;
    int run;                            /* ARP: running */
    int64_t next;                       /* .. the next step's tick */
    int pos, oct, mpos, tpos, passes, steps;
    int down;                           /* keys down (ARP / REPEAT latches) */
    int last_idx;                       /* ARP ROTH: the last note's place */
    uint8_t last_n;                     /* ARP's last notes played (REPEAT mod) */
    uint8_t last_p[QD_MFX_NHOLD], last_t[QD_MFX_NHOLD];
    int cursor;                         /* SELECTOR UP / DOWN */
    uint8_t mono_on[8], mono_prev[8], mono_gen[8];   /* MONO: each track's sounding note (its prev, gen) */
    int32_t ext[QD_MFX_NHOLD];          /* RANDOM: a held note's later end (by h[] place: track, prev) */
    uint8_t ext_t[QD_MFX_NHOLD], ext_p[QD_MFX_NHOLD];
} qd_mfx_srt_t;
typedef struct {
    uint8_t used, track, prev, pitch, live, gen;
} qd_mfx_out_t;
typedef struct {
    int64_t tick;
    uint8_t used, track, prev, pitch, live, gen;
} qd_mfx_pend_t;
typedef struct {
    qd_mfx_srt_t s[QD_MFX_NSLOT];
    qd_mfx_out_t out[QD_MFX_NOUT];
    qd_mfx_pend_t pend[QD_MFX_NPEND];
    uint32_t passed[QD_MFX_NSLOT];      /* notes each slot has seen (the screen's activity) */
    uint32_t sent, dropped;
    uint8_t idle;                       /* nothing timed in it (the clock skips it until a note comes) */
} qd_mfx_grt_t;

typedef struct {
    qd_mfx_group_t *cfg;                /* the project's five */
    qd_mfx_io_t io;
    qd_mfx_grt_t g[QD_MFX_NGROUP];
    int64_t now;                        /* the tick being worked at */
    int64_t qref;                       /* the quantize grid's origin (the transport's 0) */
    uint32_t rng;
} qd_mfx_t;

void qd_mfx_init(qd_mfx_t *m, qd_mfx_group_t *cfg, const qd_mfx_io_t *io);
void qd_mfx_seed(qd_mfx_t *m, uint32_t seed);
/* a note into group g (1..5) at tick: on 1 a note on, 0 its off (n.prev = n.pitch as it enters) */
void qd_mfx_note(qd_mfx_t *m, int g, int on, const qd_mfx_note_t *n, int64_t tick);
void qd_mfx_advance(qd_mfx_t *m, int64_t tick);   /* everything due before tick */
void qd_mfx_reset(qd_mfx_t *m);         /* every group: its notes released, its effects' state cleared (STOP) */
/* track t's notes out of every group (the sequencer's: live 0; a key's: 1; both: -1), their sounding notes released
 * (a mute, a flush) */
void qd_mfx_flush_track(qd_mfx_t *m, int t, int live);
int qd_mfx_busy(const qd_mfx_t *m, int g);   /* the group holds notes or has notes sounding */
/* ARP tapped: group g's arp on / off (one added in the first empty slot if it has none: -1 full); returns the slot */
int qd_mfx_arp_toggle(qd_mfx_t *m, int g, int *on);
int qd_mfx_arp_slot(const qd_mfx_group_t *g);   /* the group's first ARP (-1 none) */
int qd_mfx_arp_playing(const qd_mfx_t *m, int g);   /* its ARP is running */

#ifdef __cplusplus
}
#endif
