/* SPDX-License-Identifier: GPL-3.0-only */
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

#define QD_QUAD_NP 74                   /* the sound's values: FM TONE's patch (eng_quad.c QP_NP, 73) and DRIVE (P1.4a) */
#define QD_QUAD_FMNP 73                 /* .. FM TONE's own (the engine reads these) */
#define QD_QUAD_BLOB 96                 /* a packed patch (QUAD_BLOB: 'Q', version 5, the values) */
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
    QD_QP_DRIVE                         /* P1.4a: ChoralRoot's DIST (fx.c track_dist) as the sound's own value, 0..127, per
                                         * voice (qd_quad_dist; the mix runs it); a preset's is its FX() drive */
};
enum { QD_QL_SPEED, QD_QL_MULT, QD_QL_FADE, QD_QL_DEST, QD_QL_WAVE, QD_QL_PHASE, QD_QL_TRIG, QD_QL_DEPTH };
#define QD_QP_LFO(k, f) (QD_QP_LFO0 + (k) * 8 + (f))
enum { QD_QF_OFF, QD_QF_LP12, QD_QF_HP12, QD_QF_LP24 };   /* FTYPE */

typedef struct qd_quad_voice qd_quad_voice_t;

/* A track as FM TONE's owner. Field names are eng_quad.c's (it reads this as its track_t) */
typedef struct qd_quad_owner {
    int16_t p[1];                       /* ChoralRoot's track parameters FM TONE reads: P_VOICE (0: V_POLY) */
    struct { uint32_t ph[3], rnd[3]; } quad_lfo;   /* the FREE / HOLD LFOs' phases and random values */
    int8_t quad_pan_off;                /* the LFOs' PAN offset, from the latest note's voice (-64..63) */
    int8_t quad_fx_off[4];              /* .. DRIVE, REVERB, DELAY, CHORUS offsets (-127..127) */
    const qd_quad_voice_t *m_v;         /* the latest note's voice */
    const int16_t *quad_patch;          /* the track's sound: the FREE LFOs run at its rates (none: they stand) */
    int16_t bpm;                        /* the tempo: synced LFOs, the envelope DELAYs */
} qd_quad_owner_t;

/* A voice. The first fields are ChoralRoot's voice_t's that FM TONE and its driver use (eng_quad.c reads this as its
 * voice_t); the patch is the voice's own copy */
struct qd_quad_voice {
    uint8_t note, vel, gate, active;    /* gate: the key is down; active: the voice sounds (or fades) */
    uint8_t mvel;                       /* the note's velocity (mod.c's; the same as vel here) */
    uint8_t kill;                       /* the fade steal: the next block ramps the voice to 0, then it ends */
    int32_t env, env_out;               /* the driver's: Q24, the last block's amplitude Q15 (0, 0: silent) */
    int32_t pitch16, pitch_cur;         /* 1/16 semitone */
    int16_t patch[QD_QUAD_NP];          /* the voice's own patch (PLAN.md §5) */
    qd_quad_owner_t *owner;
    uint64_t qstate[QD_QUAD_STATE_WORDS];   /* eng_quad.c quad_voice_t */
    uint8_t dip;                        /* the dip (PLAN §5): this block ramps the voice to 0 as kill's does, but it
                                         * goes on (the next block ramps back from 0); set and cleared by the pool */
};

/* ---- the owner (one per track) */
void qd_quad_owner_init(qd_quad_owner_t *o);
void qd_quad_owner_block(qd_quad_owner_t *o);   /* a control block: the FREE LFOs' phases advance */

/* ---- a voice */
void qd_quad_voice_init(qd_quad_voice_t *v, uint32_t idx);   /* silent; idx: its index in the pool (an LFO seed) */
/* a note on: a silent voice starts fresh; a sounding one (gate or release) is struck again in place, its phases and
 * filters running on (eng_quad.c quad_note_on's retrigger: PHRT restarts only the operators no one hears) */
void qd_quad_voice_start(qd_quad_voice_t *v, const int16_t *patch, qd_quad_owner_t *o, int note, int vel);
void qd_quad_voice_release(qd_quad_voice_t *v);   /* the key up: the envelopes' release */
void qd_quad_voice_kill(qd_quad_voice_t *v);      /* the fade steal: its next block ramps it to 0, and it ends */
/* MONO legato (ChoralRoot's voice.c mono_play, then eng_quad.c quad_legato): the sounding voice moves to note
 * without a new attack; the gate is held, the amplitude and filter envelopes and the phases run on, the operator
 * envelopes restart only where the patch's A / B TRIG is on (the Digitone's "every note"); the amplitude follows vel
 * (mvel; vel rises to it if higher, as ChoralRoot's). A voice that is not sounding: nothing */
void qd_quad_voice_legato(qd_quad_voice_t *v, int note, int vel);
/* one control block added to out[0..31]; 1: rendered, 0: the voice is silent (ended now or before) */
int qd_quad_voice_render(qd_quad_voice_t *v, int32_t *out);
int32_t qd_quad_voice_level(const qd_quad_voice_t *v);   /* its last block's amplitude, Q15 (the quietest is stolen) */
int qd_quad_voice_endless(const qd_quad_voice_t *v);    /* it would sound on after its release (AMP REL INF) */

/* ---- DRIVE (P1.4a): ChoralRoot's DIST (v0.25.1 fx.c track_dist, line for line), on one voice's block; the state is the
 * voice's (track_t dist_* there). drive 0: b untouched (the state kept: turning it up does not click) */
typedef struct { int32_t hp, lp1, lp2; } qd_dist_t;
void qd_quad_dist(qd_dist_t *st, int drive, int32_t *b, int n);

/* ---- patches */
void qd_quad_preset_patch(int k, int16_t *p);    /* k 0: INIT, 1..16: the factory sounds (P1.4a: with their DRIVE) */
const char *qd_quad_preset_name(int k);         /* "INIT", "EP" .. "NOISE-ISH" */
void qd_quad_pack(const int16_t *p, uint8_t *blob);         /* QD_QUAD_BLOB bytes (P1.4a: DRIVE in byte 87, after
                                                             * version 5's values; 0 there: ChoralRoot's blob) */
int qd_quad_unpack(const uint8_t *blob, int16_t *p);        /* 1: taken; 0: not a blob (p = INIT) */
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
const char *qd_quad_dest_name(int dest);
int qd_quad_dest_step(int dest, int d);

#ifdef __cplusplus
}
#endif
