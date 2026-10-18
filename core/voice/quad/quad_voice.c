/* SPDX-License-Identifier: GPL-3.0-only */
/* FM TONE as a library (quad_voice.h): ChoralRoot's eng_quad.c (v0.25.1, copied with the changes its header lists) on
 * the parts of core.h it reads (cr_core.h), dsp.c and the generated tables (cr_tables.h), all in this one unit as
 * ChoralRoot's tests/cr_quad_test.c builds them; then the interface. The driver below is cr_quad_test.c's voice
 * driver (drv_on, drv_tick: voice.c's control tick for an ownenv engine, done then render) plus ChoralRoot's fade
 * steal (voice.c voice_kill / env_tick stage 4: a voice given up renders one more block with its amplitude ramped to
 * 0, m.amp1 = 0, then ends). */
#include <stdint.h>
#include <string.h>

#include "quad_voice.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpedantic"
#pragma clang diagnostic ignored "-Wc11-extensions"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-const-variable"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "cr_tables.h"
#include "cr_core.h"
typedef qd_quad_voice_t voice_t;        /* (eng_quad.c's voice_t and track_t: QUADRILLE's voice and owner) */
typedef qd_quad_owner_t track_t;
#include "dsp.c"
#include "eng_quad.c"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* (C99: an array of negative size fails the build) quad_voice.h's sizes are eng_quad.c's; quad_voice_t fits */
typedef char qd_quad_sizes_ok[QD_QUAD_NP == QP_NP && QD_QUAD_BLOB == QUAD_BLOB && QD_QUAD_CTL == CTL &&
                              QD_QUAD_NPRESET == 1 + QUAD_NPRESETS ? 1 : -1];
typedef char qd_quad_qp_ok[QD_QP_ALGO == QP_ALGO && QD_QP_PHRT == QP_PHRT && QD_QP_FREQ == QP_FREQ &&
                           QD_QP_FTYPE == QP_FTYPE && QD_QP_LEVEL == QP_LEVEL && QD_QP_LFO0 == QP_LFO0 &&
                           QD_QP_LFO(2, QD_QL_DEPTH) == QP_LFO(2, QL_DEPTH) && QD_QP_RB2 == QP_RB2 &&
                           QD_QP_B2KEY + 1 == QP_NP && QD_QP_EREL == QP_EREL && QD_QP_FDLY == QP_FDLY &&
                           QD_QF_LP24 == QF_LP24 && QD_QL_TRIG == QL_TRIG ? 1 : -1];
typedef char qd_quad_state_fits[sizeof(quad_voice_t) <= sizeof(((qd_quad_voice_t *)0)->qstate) ? 1 : -1];

/* ---------------------------------------------------------------- the owner --- */
void qd_quad_owner_init(qd_quad_owner_t *o)
{
    memset(o, 0, sizeof *o);
    o->p[P_VOICE] = V_POLY;
    o->bpm = 120;
}

void qd_quad_owner_block(qd_quad_owner_t *o)
{
    song.g[G_BPM] = o->bpm;
    quad_block(o);
}

/* ---------------------------------------------------------------- a voice --- */
void qd_quad_voice_init(qd_quad_voice_t *v, uint32_t idx)
{
    memset(v, 0, sizeof *v);
    quad_init_patch(v->patch);
    ((quad_voice_t *)(void *)v->qstate)->idx = idx;
}

/* cr_quad_test.c drv_on: the note, the velocity, the gate; a voice that is not sounding starts from silence */
void qd_quad_voice_start(qd_quad_voice_t *v, const int16_t *patch, qd_quad_owner_t *o, int note, int vel)
{
    int sounding = v->active && !v->kill;
    if (patch != v->patch)
        memcpy(v->patch, patch, sizeof v->patch);
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    vel = vel < 1 ? 1 : vel > 127 ? 127 : vel;
    v->owner = o;
    v->note = (uint8_t)note;
    v->vel = v->mvel = (uint8_t)vel;
    v->gate = v->active = 1;
    v->kill = 0;
    v->pitch16 = v->pitch_cur = (int32_t)note * 16;
    o->m_v = v;
    if (!sounding)
        v->env = v->env_out = 0;
    song.g[G_BPM] = o->bpm;
    quad_note_on(o, v);
}

void qd_quad_voice_release(qd_quad_voice_t *v) { v->gate = 0; }

/* (ChoralRoot's MONO legato, voice.c: the note and pitch move, then engine_t.legato = quad_legato) */
void qd_quad_voice_legato(qd_quad_voice_t *v, int note, int vel)
{
    if (!v->active || v->kill || !v->owner)
        return;
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    vel = vel < 1 ? 1 : vel > 127 ? 127 : vel;
    v->note = (uint8_t)note;
    v->pitch16 = v->pitch_cur = (int32_t)note * 16;   /* (no glide: QUADRILLE has none yet) */
    v->mvel = (uint8_t)vel;
    if (vel > v->vel)
        v->vel = (uint8_t)vel;
    v->gate = 1;
    song.g[G_BPM] = v->owner->bpm;
    quad_legato(v->owner, v);
}

void qd_quad_voice_kill(qd_quad_voice_t *v)
{
    if (v->active) {
        v->gate = 0;
        v->kill = 1;
    }
}

/* cr_quad_test.c drv_tick for one voice: done, else render a block at full drive (m.amp1 32767: the engine's own
 * envelopes shape it); a killed voice's block ramps to 0 (voice.c: env_tick stage 4, then m.amp1 = 0), then it ends;
 * a dipped one's too, and it goes on: its next block ramps up from 0 (the engine's amplitude ramp from s->amp) */
int qd_quad_voice_render(qd_quad_voice_t *v, int32_t *out)
{
    qd_quad_owner_t *t = v->owner;
    vmod_t m;
    if (!v->active || !t)
        return 0;
    song.g[G_BPM] = t->bpm;
    if (quad_done(t, v)) {
        v->active = v->gate = v->kill = 0;
        v->env = v->env_out = 0;
        return 0;
    }
    v->env = 1 << 24;
    memset(&m, 0, sizeof m);
    m.amp0 = v->env_out;
    m.amp1 = v->env_out = v->kill || v->dip ? 0 : 32767;   /* (QUADRILLE's dip: to 0 and on, PLAN §5) */
    m.pitch16 = v->pitch_cur;
    m.inc = pitch_inc((uint32_t)m.pitch16);
    m.shape = 64 << 8;
    quad_render(t, v, out, CTL, &m);
    if (v->kill) {                                      /* (faded to 0 over this block: gone) */
        v->active = v->gate = v->kill = 0;
        v->env = v->env_out = 0;
    }
    return 1;
}

int32_t qd_quad_voice_level(const qd_quad_voice_t *v)
{
    const quad_voice_t *s = (const quad_voice_t *)(const void *)v->qstate;
    return !v->active ? 0 : s->amp < 0 ? -s->amp : s->amp;
}

int qd_quad_voice_endless(const qd_quad_voice_t *v) { return v->active && quad_endless(v->owner, v); }

/* ---------------------------------------------------------------- patches --- */
void qd_quad_preset_patch(int k, int16_t *p)
{
    if (k <= 0 || k > (int)QUAD_NPRESETS)
        quad_init_patch(p);
    else
        quad_preset_patch((uint32_t)k - 1u, p);
}

const char *qd_quad_preset_name(int k)
{
    return k <= 0 || k > (int)QUAD_NPRESETS ? "INIT" : QUAD_PRESETS[k - 1].name;
}

void qd_quad_pack(const int16_t *p, uint8_t *blob) { quad_pack(p, blob); }
int qd_quad_unpack(const uint8_t *blob, int16_t *p) { return quad_unpack(blob, p); }

const char *qd_quad_value_name(int i)
{
    uint32_t pg, c;
    if (i == QP_RB2)
        return "RATIO B2";
    for (pg = 0; pg < QUAD_NPAGES; pg++)
        for (c = 0; c < 4u; c++)
            if (QUAD_MAP[pg][c] == (uint32_t)i && QUAD_PAGES[pg].col[c].label)
                return QUAD_PAGES[pg].col[c].label;
    return "?";
}

int qd_quad_value_range(int i, int16_t *min, int16_t *max, int16_t *def)
{
    quad_rng_t r;
    if (i < 0 || i >= QP_NP)
        return 0;
    r = quad_range((uint32_t)i);
    if (min)
        *min = r.min;
    if (max)
        *max = r.max;
    if (def)
        *def = r.def;
    return 1;
}
