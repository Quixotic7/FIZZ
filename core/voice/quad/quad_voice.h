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

#define QD_QUAD_NP 73                   /* the patch's values (eng_quad.c QP_NP) */
#define QD_QUAD_BLOB 96                 /* a packed patch (QUAD_BLOB: 'Q', version 5, the values) */
#define QD_QUAD_CTL 32                  /* samples a control block */
#define QD_QUAD_NPRESET 17              /* 0 INIT, 1..16 ChoralRoot's factory sounds (EP .. NOISE-ISH) */
#define QD_QUAD_STATE_WORDS 32          /* quad_voice_t's room (eng_quad.c; quad_voice.c checks it fits) */

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
/* one control block added to out[0..31]; 1: rendered, 0: the voice is silent (ended now or before) */
int qd_quad_voice_render(qd_quad_voice_t *v, int32_t *out);
int32_t qd_quad_voice_level(const qd_quad_voice_t *v);   /* its last block's amplitude, Q15 (the quietest is stolen) */
int qd_quad_voice_endless(const qd_quad_voice_t *v);    /* it would sound on after its release (AMP REL INF) */

/* ---- patches */
void qd_quad_preset_patch(int k, int16_t *p);    /* k 0: INIT, 1..16: the factory sounds */
const char *qd_quad_preset_name(int k);         /* "INIT", "EP" .. "NOISE-ISH" */
void qd_quad_pack(const int16_t *p, uint8_t *blob);         /* QD_QUAD_BLOB bytes */
int qd_quad_unpack(const uint8_t *blob, int16_t *p);        /* 1: taken; 0: not a blob (p = INIT) */
int qd_quad_value_range(int i, int16_t *min, int16_t *max, int16_t *def);   /* patch value i's range; 0: no such */

#ifdef __cplusplus
}
#endif
