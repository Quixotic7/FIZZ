/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FM TONE (ChoralRoot's QUAD, eng_quad.c v0.25.1) as a library: one voice at a time, a patch per voice (PLAN.md §5),
 * an owner per track (PLAN.md §2: the track's FREE / HOLD LFO phases, its tempo). quad_voice.c is the only unit that
 * includes ChoralRoot's code; this header includes none of it.
 *
 * The time: a voice renders one control block (QD_QUAD_CTL = 32 samples at 44.1 kHz, ChoralRoot's CTL) a call. The
 * caller runs, for each block: qd_quad_owner_block for every owner, then qd_quad_voice_render for every voice (as
 * ChoralRoot's voice.c runs engine_t.block, then each voice's done and render). Starts, releases and kills go
 * between blocks. Single-threaded; nothing here calls the OS; integer DSP: two runs give the same samples.
 *
 * Levels: a voice adds ChoralRoot's engine output to out[] (a full-scale voice near VOICE_FS = 24000, its peaks up to
 * 65535 x amplitude; the mix, core/voice/mix.c, takes it from there as ChoralRoot's fx.c takes a part). */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QD_QUAD_NP 115                  /* the sound's values: FM TONE's patch (eng_quad.c QP_NP, 73), DRIVE (P1.4a),
                                         * E1 (research/FMTONE-DN2-PLAN.md): the values 74..109 (QD_QP_AMOD ..), and the
                                         * VOICE page's 110..114 (2026-10-26: QD_QP_VMODE ..) */
#define QD_QUAD_NP_E4 110               /* .. the count before the VOICE page ('P' 1..5 records' mix lock ids from here) */
#define QD_QUAD_FMNP 73                 /* .. FM TONE's own (the engine reads these) */
#define QD_QUAD_NP5 74                  /* .. those of a version-5 blob (FM TONE's and DRIVE) */
#define QD_QUAD_BLOB 128                /* E1: FIZZ's packed patch, blob version 6 ('Q', 6, version 5's values, DRIVE at 87,
                                         * values 74..109 a byte each (value - min) at 88..123, zeros) */
#define QD_QUAD_BLOB5 96                /* ChoralRoot's (QUAD_BLOB: 'Q', version 1..5; FIZZ's DRIVE in byte 87) */
#define QD_QUAD_CTL 32                  /* samples a control block */
#define QD_QUAD_NPRESET 17              /* 0 INIT, 1..16 ChoralRoot's factory sounds (EP .. NOISE-ISH) */
#define QD_QUAD_STATE_WORDS 32          /* quad_voice_t's room (eng_quad.c; quad_voice.c checks it fits) */

/* the patch's values by index (eng_quad.c's QP_* enum, checked equal in quad_voice.c), then DRIVE: the p-lock ids
 * 0..73 */
enum {
    QD_QP_ALGO, QD_QP_RC, QD_QP_RA, QD_QP_RB1, QD_QP_HARM, QD_QP_DTUN, QD_QP_FDBK, QD_QP_MIX,
    QD_QP_OFSC, QD_QP_OFSA, QD_QP_OFSB1, QD_QP_OFSB2,
    QD_QP_AATK, QD_QP_ADEC, QD_QP_AEND, QD_QP_ALEV, QD_QP_BATK, QD_QP_BDEC, QD_QP_BEND, QD_QP_BLEV,
    QD_QP_ADLY, QD_QP_ATRIG, QD_QP_ARST, QD_QP_PHRT, QD_QP_BDLY, QD_QP_BTRIG, QD_QP_BRST, QD_QP_VEL,
    QD_QP_AKEY, QD_QP_B1KEY,
    QD_QP_FATK, QD_QP_FDEC, QD_QP_FSUS, QD_QP_FREL, QD_QP_FREQ, QD_QP_RESO, QD_QP_FTYPE, QD_QP_FDEPTH,
    QD_QP_FDLY, QD_QP_FKTRK, QD_QP_BASE, QD_QP_WIDTH,
    QD_QP_EATK, QD_QP_EDEC, QD_QP_ESUS, QD_QP_EREL, QD_QP_LEVEL,
    QD_QP_LFO0,
    QD_QP_RB2 = QD_QP_LFO0 + 3 * 8, QD_QP_B2KEY,
    QD_QP_DRIVE,                        /* P1.4a: ChoralRoot's DIST (qd_quad_dist) as the sound's own value, 0..127, per
                                         * voice (qd_quad_dist; the mix runs it); a preset's is its FX() drive */
    /* E1..E4 (research/FMTONE-DN2-PLAN.md, FIZZ only: quad_ext.c): each one's default is today's sound */
    QD_QP_AMOD,                         /* E1: the amp envelope's mode: 0 ADSR, 1 AHD */
    QD_QP_HOLD,                         /* E1: AHD's HOLD 0..126 (a time, the amp attack's table), 127 NOTE (while held) */
    QD_QP_PORT,                         /* E1: portamento 0 OFF, 1 ON, 2 LEGATO (only into a note still held) */
    QD_QP_PTIM,                         /* E1: its time 0..127 */
    QD_QP_PSLOPE,                       /* E1: 0 TIME (the same time any interval), 1 RATE (a fixed speed) */
    QD_QP_DRPRE,                        /* E1: DRIVE 0 after the voice (POST, as before), 1 before the filter (PRE) */
    QD_QP_FSHAPE,                       /* E2: the filter machine's own control (MULTI's morph, the combs' LPF, EQ's Q) */
    QD_QP_FGAIN,                        /* E2: EQ's gain -64..63 */
    QD_QP_BWPOST,                       /* E2: base-width 0 before the filter, 1 after */
    QD_QP_BITS,                         /* E3: CRUSH: bit reduction 0 OFF, 1..127 */
    QD_QP_SRR,                          /* E3: .. sample-rate reduction 0 OFF, 1..127 */
    QD_QP_CRPOST,                       /* E3: .. 0 before the filter, 1 after */
    QD_QP_MOD0                          /* E4: eight modulation slots: QD_QP_MOD(k, QD_QM_SRC / DST / DEPTH) */
};
/* 2026-10-26, the VOICE page (mockups/fizz-unison-voice.html, the owner's: unison with real voices as the Digitone 1):
 * how the sound takes voices. Not lockable on a step (qd_quad_lockable); the voice pool reads them at a note's start */
enum {
    QD_QP_VMODE = QD_QP_MOD0 + 3 * 8,   /* VOICE: 0 POLY, 1 MONO (one note at a time on the track) */
    QD_QP_UNI,                          /* UNISON: 1 OFF, 2..8 voices a note (the track's LIMIT caps them) */
    QD_QP_UDET,                         /* DETUNE 0..127: the unison voices' spread in pitch (the outer ones +-50 cents) */
    QD_QP_USPR,                         /* SPREAD 0..127: .. across the stereo */
    QD_QP_VPRIO                         /* PRIORITY: 0 LOW, 1 MID, 2 HIGH (the pool robs the lowest first) */
};
int qd_quad_lockable(int id);           /* a step may lock value id (the VOICE page's may not) */
/* FIZZ MIDI (e, research/FIZZ-MIDI-PLAN.md §5): A MIDI TRACK'S SOUND lives in the same patch (a track is a MIDI track
 * when its sound is one: ENGINE on track settings page 3; a preset, a pool sound, a pattern's track sound carry it).
 * Its 28 slots (7 pages of 4): slot k's value 0..127 in FM TONE's value QD_MSND_ID[k] (28 values whose own range is
 * 0..127: a slot's p-lock is that value's lock, clamped to 0..127 as it is); its CC number and touched flag in value
 * QD_MSND_CC(k) (cc | QD_MSND_TOUCHED: FM TONE's values 74..101); the mark QD_MSND_MARK in VOICE's PRIORITY (0..2 in an
 * FM TONE sound). The rest is INIT's (the FREE LFOs stand). No voice ever takes it (pool.c refuses it). Packed
 * (qd_quad_pack, the same 128 bytes): 'Q' 6, bytes 2..29 the values (| 128 touched), 30..57 the CC numbers, zeros,
 * 124..127 "MIDI" (VOICE's four bytes, which no FM TONE blob can hold: a FIZZ before this one reads it as no blob, INIT) */
#define QD_MSND_N 28
#define QD_MSND_PAGE 4
#define QD_MSND_NPAGE 7
#define QD_MSND_MARK 0x4D
#define QD_MSND_TOUCHED 128
#define QD_MSND_CC(k) (QD_QP_AMOD + (k))
extern const uint8_t QD_MSND_ID[QD_MSND_N];
static inline int qd_msnd_is(const int16_t *p) { return p && p[QD_QP_VPRIO] == QD_MSND_MARK; }
int qd_msnd_slot(int id);               /* the slot whose value id is (-1: none) */
void qd_msnd_init(int16_t *p);          /* a new MIDI sound: every slot untouched (0), slot k CC 16 + k */
enum { QD_QM_SRC, QD_QM_DST, QD_QM_DEPTH };
/* E4: the modulation slots' sources (SOURCE 0 OFF) */
enum { QD_MS_OFF, QD_MS_VEL, QD_MS_KEY, QD_MS_MW, QD_MS_AT, QD_MS_PB, QD_MS_BR, QD_MS_RAND, QD_MS_FENV, QD_MS_N };
#define QD_QP_MOD(k, f) (QD_QP_MOD0 + (k) * 3 + (f))
#define QD_NMOD 8
enum { QD_PORT_OFF, QD_PORT_ON, QD_PORT_LEGATO };
/* E2: the filter machines: QD_QP_FTYPE 0 OFF, 1 LP12, 2 HP12 (LEGACY), 3 LP24 (LOWPASS 4), then: */
enum { QD_QF_MULTI = 4, QD_QF_COMBP = 5, QD_QF_COMBN = 6, QD_QF_EQ = 7, QD_QF_N = 8 };
enum { QD_QL_SPEED, QD_QL_MULT, QD_QL_FADE, QD_QL_DEST, QD_QL_WAVE, QD_QL_PHASE, QD_QL_TRIG, QD_QL_DEPTH };
#define QD_QP_LFO(k, f) (QD_QP_LFO0 + (k) * 8 + (f))
enum { QD_QF_OFF, QD_QF_LP12, QD_QF_HP12, QD_QF_LP24 };   /* FTYPE */

typedef struct qd_quad_voice qd_quad_voice_t;
/* P2.6c: an LFO's phase step as last worked out, and what it was worked out from (SPEED, MULT, the tempo) */
typedef struct {
    int16_t sp, bpm;
    uint8_t mu, ok;
    int32_t inc;
} qd_lfo_cache_t;

/* A track as FM TONE's owner. Field names are eng_quad.c's (it reads this as its track_t) */
typedef struct qd_quad_owner {
    int16_t p[1];                       /* ChoralRoot's track parameters FM TONE reads: P_VOICE (0: V_POLY) */
    struct { uint32_t ph[3], rnd[3]; } quad_lfo;   /* the FREE / HOLD LFOs' phases and random values */
    int8_t quad_pan_off;                /* the LFOs' PAN offset, from the latest note's voice (-64..63) */
    int8_t quad_fx_off[4];              /* .. DRIVE, REVERB, DELAY, CHORUS offsets (-127..127) */
    const qd_quad_voice_t *m_v;         /* the latest note's voice */
    const int16_t *quad_patch;          /* the track's sound: the FREE LFOs run at its rates (none: they stand) */
    int16_t bpm;                        /* the tempo: synced LFOs, the envelope DELAYs */
    int16_t last_pitch;                 /* E1: the track's last note (1/16 semitone; -1 none): a glide's start */
    uint8_t lc_n;                       /* E1: the last chord (its pitches, low to high) .. */
    int16_t lc[8];
    uint8_t cc_n;                       /* E1: .. the chord starting now: its notes as they come, their pool slots */
    uint8_t cc[8], cc_slot[8];
    const void *cc_key;                 /* .. what makes it one chord (the trig's record and tick) */
    int64_t cc_tick;
    uint8_t cc_held;                    /* .. a voice of the track was in gate when it started (PORT LEGATO glides) */
    uint8_t mw, at, br;                 /* E4: the track's MIDI state: CC 1, channel pressure, CC 2 (0..127) .. */
    int16_t pb;                         /* .. the pitch bend (-8192..8191) */
    uint32_t nserial;                   /* .. the notes struck (the RANDOM source's seed) */
    qd_lfo_cache_t lfc[3];              /* P2.6c: the FREE LFOs' steps (quad_block), worked out again on a change */
    uint8_t defer_off;                  /* P2.6: the split block (app.h qd_app_block_voices): a voice's render writes
                                         * nothing of the owner's: the latest note's voice keeps the LFOs' offsets in
                                         * itself (nx_*), qd_quad_owner_publish puts them here after the block, and a
                                         * comb line is claimed before it (qd_quad_voice_prepare). 0: as ever (the
                                         * engine's tests, ChoralRoot's order) */
} qd_quad_owner_t;

/* A voice. The first fields are ChoralRoot's voice_t's that FM TONE and its driver use (eng_quad.c reads this as its
 * voice_t); the patch is the voice's own copy */
struct qd_quad_voice {
    uint8_t note, vel, gate, active;    /* gate: the key is down; active: the voice sounds (or fades) */
    uint8_t mvel;                       /* the note's velocity (mod.c's; the same as vel here) */
    uint8_t kill;                       /* the fade steal: the next block ramps the voice to 0, then it ends */
    int32_t env, env_out;               /* the driver's: Q24, the last block's amplitude Q15 (0, 0: silent) */
    int32_t pitch16, pitch_cur;         /* 1/16 semitone */
    uint32_t udet;                      /* 2026-10-26: a unison voice's detune, its pitch x udet / 65536 (0: none) */
    int16_t patch[QD_QUAD_NP];          /* the voice's own patch (PLAN.md §5) */
    qd_quad_owner_t *owner;
    uint64_t qstate[QD_QUAD_STATE_WORDS];   /* eng_quad.c quad_voice_t */
    uint8_t dip;                        /* the dip (PLAN §5): this block ramps the voice to 0 as kill's does, but it
                                         * goes on (the next block ramps back from 0); set and cleared by the pool */
    int32_t gstep;                      /* E1: the glide's step a block (1/16 semitone; 0: by its RATE) */
    struct { int32_t hp, lp1, lp2; } dpre;   /* E1: DRIVE before the filter: its state (quad_ext.c) */
    struct { int32_t lp, len; uint16_t w; uint8_t mach, fresh, line; } comb;   /* (P2.1c: line, its comb line + 1; 0 none) */
    struct { uint32_t ph; int32_t held; } crush;   /* E3: CRUSH's sample and hold: its phase, the value held */
    int16_t ptim_mod;                   /* E4: PTIM's modulation now (the glide's RATE) */
    int16_t mrnd;                       /* E4: the RANDOM source's value for this note (Q15, bipolar) */
    int8_t nx_pan, nx_fx[4];            /* P2.6: as the latest note's voice, the offsets its last block made (the
                                         * owner's quad_pan_off, quad_fx_off) .. */
    uint8_t nx_ok;                      /* .. made since its start */
    int16_t nx_lw[3];                   /* as the latest note's voice: each LFO's last output (Q15, its DEPTH and FADE
                                         * in; 0 none): the sound editor shows the values as heard */
    qd_lfo_cache_t lfc[4];              /* P2.6c: its LFOs' steps (and LFO 1's as LFOs 2, 3 move it), on a change */
    int16_t fq_sm, rs_sm;               /* P2.6b: FREQ and RESO as the filter runs them: they glide to a live change (a
                                         * knob, a LOCK on a sounding voice) over a few blocks; a note starts at its own */   /* E2: the filter machines' state (quad_ext.c:
                                         * the comb's damping and length (Q8), its write place; the machine last run; a
                                         * fresh start: its line from silence) */
};

/* ---- the owner (one per track) */
void qd_quad_owner_init(qd_quad_owner_t *o);
void qd_quad_owner_block(qd_quad_owner_t *o);   /* a control block: the FREE LFOs' phases advance */
/* P2.6 (defer_off): after the block's voices, the latest note's voice's offsets become the owner's */
void qd_quad_owner_publish(qd_quad_owner_t *o);
/* P2.6 (defer_off): before the block's voices, what a voice's render would claim of the shared (a comb line) */
void qd_quad_voice_prepare(qd_quad_voice_t *v);
int32_t qd_quad_bench(int k, int note, int nblocks);   /* P2.6: a preset's voice alone, n blocks: the unit's timing */
void qd_quad_bench_start(int k, int note);             /* P2.6c: .. in parts: the voice struck, then a block a call: */
int32_t qd_quad_bench_block(int what);                 /* what 0 the owner and qd_quad_voice_render, 1 quad_render alone */
uint32_t qd_quad_shared_sig(void);              /* P2.6 (tests): what the voices share outside themselves, hashed */

/* ---- a voice */
void qd_quad_voice_init(qd_quad_voice_t *v, uint32_t idx);   /* silent; idx: its index in the pool (an LFO seed) */
/* a note on: a silent voice starts fresh; a sounding one (gate or release) is struck again in place, its phases and
 * filters running on (eng_quad.c quad_note_on's retrigger: PHRT restarts only the operators no one hears) */
void qd_quad_voice_start(qd_quad_voice_t *v, const int16_t *patch, qd_quad_owner_t *o, int note, int vel);
void qd_quad_voice_release(qd_quad_voice_t *v);   /* the key up: the envelopes' release */
void qd_quad_voice_kill(qd_quad_voice_t *v);      /* the fade steal: its next block ramps it to 0, and it ends */
/* E1: the voice's glide starts from pitch from (1/16 semitone; -1 or PORT OFF: none, the note at once): before its
 * first block (a fresh start), or a legato move */
void qd_quad_voice_glide(qd_quad_voice_t *v, int from);
/* MONO legato (ChoralRoot's voice.c mono_play, then eng_quad.c quad_legato): the sounding voice moves to note
 * without a new attack; the gate is held, the amplitude and filter envelopes and the phases run on, the operator
 * envelopes restart only where the patch's A / B TRIG is on (the Digitone's "every note"); the amplitude follows vel
 * (mvel; vel rises to it if higher, as ChoralRoot's). A voice that is not sounding: nothing */
void qd_quad_voice_legato(qd_quad_voice_t *v, int note, int vel);
/* one control block added to out[0..31]; 1: rendered, 0: the voice is silent (ended now or before) */
int qd_quad_voice_render(qd_quad_voice_t *v, int32_t *out);
int32_t qd_quad_voice_level(const qd_quad_voice_t *v);   /* its last block's amplitude, Q15 (the quietest is stolen) */
int qd_quad_voice_endless(const qd_quad_voice_t *v);    /* it would sound on after its release (AMP REL INF) */

/* ---- DRIVE (P1.4a): ChoralRoot's DIST (fz_voice.h fz_drive, L1: written anew, the same samples), on one voice's
 * block; the state is the voice's. drive 0: b untouched (the state kept: turning it up does not click) */
typedef struct { int32_t hp, lp1, lp2; } qd_dist_t;
void qd_quad_dist(qd_dist_t *st, int drive, int32_t *b, int n);

/* ---- patches */
void qd_quad_preset_patch(int k, int16_t *p);    /* k 0: INIT, 1..16: the factory sounds (P1.4a: with their DRIVE) */
const char *qd_quad_preset_name(int k);         /* "INIT", "EP" .. "NOISE-ISH" */
void qd_quad_pack(const int16_t *p, uint8_t *blob);         /* QD_QUAD_BLOB bytes, version 6 (E1) */
void qd_quad_pack5(const int16_t *p, uint8_t *blob);        /* QD_QUAD_BLOB5 bytes, version 5 (P1.4a: DRIVE in byte 87,
                                                             * after version 5's values; 0 there: ChoralRoot's blob);
                                                             * values 74.. are lost */
/* n bytes there (QD_QUAD_BLOB5 or QD_QUAD_BLOB): versions 1..5 (values 74.. their defaults) and 6. 1: taken; 0: not a
 * blob (p = INIT) */
int qd_quad_unpack(const uint8_t *blob, int n, int16_t *p);
int qd_quad_ext_used(const int16_t *p);
uint32_t qd_quad_srr_inc(int srr);       /* E3: SRR's phase step a sample (2^32 / its divider) */
int qd_quad_crush_bits(int bits);        /* E3: BITS' bits (16 .. 1) */ /* E1: a value 74.. away from its default (the sound needs a version-6 blob) */
/* P1.4a: algorithm a's routing (a 1..8, the patch's ALGO; eng_quad.c QUAD_ALGO): each operator's modulators (C A B1 B2;
 * bit 0 C .. 3 B2), the feedback operator, the X and Y outputs (bit sets) */
void qd_quad_algo(int a, uint8_t *mod4, int *fb, int *outx, int *outy);
/* patch value i's text as FM TONE's pages show it ("0.50", "42.01", "LP12", "INF", "ON", "+12"); buf: 12 bytes */
const char *qd_quad_value_text(int i, int v, char *buf);
int qd_quad_value_range(int i, int16_t *min, int16_t *max, int16_t *def);   /* patch value i's range; 0: no such */
const char *qd_quad_value_name(int i);  /* its label on FM TONE's pages ("FREQ", "A DEC", .. the LFOs' "SPEED") */
/* P1.5k: the LFOs' destinations (ChoralRoot's list, eng_quad.c QD_*: N_QUAD_DEST names, QUAD_DEST_ORDER the editor's
 * order): the patch value a destination moves (-1: none of the pages' values: PITCH, PAN, the sends ..), the
 * destination that moves patch value id for LFO lfo (0..2; -1: none, LFO 1 on its own values neither), the names, a
 * step through the order */
int qd_quad_dest_id(int dest);
int qd_quad_dest_of(int id, int lfo);
int qd_quad_ndest(void);
/* the sound editor's values as heard: owner o's latest voice's LFOs (sounding: 1, w their outputs Q15, dest their
 * destinations), and value id's v moved by them (as the engine moves it; RATIO B and LFO 1's MULT: as stored) */
int qd_quad_owner_lfos(const qd_quad_owner_t *o, int16_t *w, uint8_t *dest);
int qd_quad_lfo_value(int id, int v, const int16_t *w, const uint8_t *dest);
const char *qd_quad_dest_name(int dest);
int qd_quad_dest_step(int dest, int d);
int qd_quad_dest_pos(int dest);         /* FIZZ MIDI (a): dest's place in that order (0 .. qd_quad_ndest() - 1) */

#ifdef __cplusplus
}
#endif
