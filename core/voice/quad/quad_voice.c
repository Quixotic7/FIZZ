/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FM TONE as a library (quad_voice.h): the owner's eng_quad.c (ChoralRoot v0.25.1 with the changes its header lists)
 * on fz_core.h, fz_voice.h and fz_tables.h (L1: the declarations, building blocks and tables written anew), all in
 * this one unit; then the interface and FIZZ's own driver of the engine (a block at a time; a voice given up renders
 * one more block with its amplitude ramped to 0, then ends: the fade steal). */
#include <stdint.h>
#include <stdio.h>
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
#define FZ_TABLES_DEFINE                /* (2026-10-26: this unit holds the tables; the others see externs) */
#include "fz_tables.h"                 /* (L1: the tables, tools/gen_fz_tables.py) */
#include "fz_core.h"                   /* (L1: the declarations eng_quad.c reads) */
typedef qd_quad_voice_t voice_t;        /* (eng_quad.c's voice_t and track_t: QUADRILLE's voice and owner) */
typedef qd_quad_owner_t track_t;
#include "../../seq/qd_div.h"                  /* (P2.4: 64-bit by 32-bit division without the library routine) */
#include "fz_voice.h"                   /* (L1: the building blocks, fz_dsp.h, and the envelope, glide, drive) */
#include "eng_quad.c"
#include "quad_ext.c"                   /* QUADRILLE (e): FIZZ's extensions (research/FMTONE-DN2-PLAN.md) */
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* (C99: an array of negative size fails the build) quad_voice.h's sizes are eng_quad.c's; quad_voice_t fits */
typedef char qd_quad_sizes_ok[QD_QUAD_FMNP == QP_NP && QD_QUAD_NP5 == QP_NP + 1 && QD_QP_DRIVE == QP_NP &&
                              QUAD_V5_LEN < QUAD_BLOB && QD_QUAD_BLOB5 == QUAD_BLOB && QD_QUAD_CTL == CTL &&
                              QD_QUAD_NPRESET == 1 + QUAD_NPRESETS && QD_QP_MOD(QD_NMOD - 1, QD_QM_DEPTH) + 1 == QD_QP_VMODE &&
                              QD_QP_VMODE == QD_QUAD_NP_E4 && QD_QP_VPRIO + 1 == QD_QUAD_NP &&
                              QUAD_V5_LEN + 1 + (QD_QUAD_NP_E4 - QD_QUAD_NP5) + 4 == QD_QUAD_BLOB ? 1 : -1];
typedef char qd_quad_dpre_ok[sizeof(((qd_quad_voice_t *)0)->dpre) == sizeof(qd_dist_t) ? 1 : -1];
typedef char qd_quad_qp_ok[QD_QP_ALGO == QP_ALGO && QD_QP_PHRT == QP_PHRT && QD_QP_FREQ == QP_FREQ &&
                           QD_QP_FTYPE == QP_FTYPE && QD_QP_LEVEL == QP_LEVEL && QD_QP_LFO0 == QP_LFO0 &&
                           QD_QP_LFO(2, QD_QL_DEPTH) == QP_LFO(2, QL_DEPTH) && QD_QP_RB2 == QP_RB2 &&
                           QD_QP_B2KEY + 1 == QP_NP && QD_QP_EREL == QP_EREL && QD_QP_FDLY == QP_FDLY &&
                           QD_QF_LP24 == QF_LP24 && QD_QL_TRIG == QL_TRIG ? 1 : -1];
typedef char qd_quad_state_fits[sizeof(quad_voice_t) <= sizeof(((qd_quad_voice_t *)0)->qstate) ? 1 : -1];

/* ---------------------------------------------------------------- E1: the values 74.. --- */
/* each one's range and default (today's sound), its label and its names (research/FMTONE-DN2-PLAN.md §3) */
typedef struct { int16_t min, max, def; const char *name; const char *const *names; } qe_val_t;
static const char *const QE_AMOD[2] = {"ADSR", "AHD"};
static const char *const QE_PORT[3] = {"OFF", "ON", "LEGATO"};
static const char *const QE_PSLOPE[2] = {"TIME", "RATE"};
static const char *const QE_PREPOST[2] = {"POST", "PRE"};
static const char *const QE_BEFAFT[2] = {"BEFORE", "AFTER"};
static const char *const QE_MSRC[QD_MS_N] = {"OFF", "VEL", "KEY", "MW", "AT", "PB", "BR", "RAND", "F ENV"};
static const char *const QE_VMODE[2] = {"POLY", "MONO"};
static const char *const QE_PRIO[3] = {"LOW", "MID", "HIGH"};
static qe_val_t qe_val(int i)
{
    static const qe_val_t T[QD_QP_MOD0 - QD_QP_AMOD] = {
        {0, 1, 0, "MODE", QE_AMOD}, {0, 127, 127, "HOLD", 0}, {0, 2, 0, "PORT", QE_PORT}, {0, 127, 32, "TIME", 0},
        {0, 1, 0, "SLOPE", QE_PSLOPE}, {0, 1, 0, "ROUTE", QE_PREPOST}, {0, 127, 0, "SHAPE", 0},
        {-64, 63, 0, "GAIN", 0}, {0, 1, 0, "ROUTE", QE_BEFAFT}, {0, 127, 0, "BITS", 0}, {0, 127, 0, "SRR", 0},
        {0, 1, 1, "ROUTE", QE_BEFAFT}};
    static const qe_val_t M[3] = {{0, 8, 0, "SOURCE", QE_MSRC}, {0, (int16_t)(QD_N - 1), 0, "DEST", 0},
                                  {-127, 127, 0, "DEPTH", 0}};
    static const qe_val_t V[5] = {{0, 1, 0, "VOICE", QE_VMODE}, {1, 8, 1, "UNISON", 0}, {0, 127, 20, "DETUNE", 0},
                                  {0, 127, 64, "SPREAD", 0}, {0, 2, 1, "PRIORITY", QE_PRIO}};   /* (the VOICE page) */
    static const qe_val_t Z = {0, 0, 0, "?", 0};
    if (i >= QD_QP_AMOD && i < QD_QP_MOD0)
        return T[i - QD_QP_AMOD];
    if (i >= QD_QP_MOD0 && i < QD_QP_VMODE)
        return M[(i - QD_QP_MOD0) % 3];
    if (i >= QD_QP_VMODE && i < QD_QUAD_NP)
        return V[i - QD_QP_VMODE];
    return Z;
}
static void qe_defaults(int16_t *p)
{
    int i;
    for (i = QD_QP_AMOD; i < QD_QUAD_NP; i++)
        p[i] = qe_val(i).def;
}
int qd_quad_ext_used(const int16_t *p)
{
    int i;
    for (i = QD_QP_AMOD; i < QD_QUAD_NP; i++)
        if (p[i] != qe_val(i).def)
            return 1;
    return 0;
}

uint32_t qd_quad_srr_inc(int srr) { return QE_SRR_INC[srr < 0 ? 0 : srr > 127 ? 127 : srr]; }
int qd_quad_crush_bits(int bits) { return qe_crush_bits(bits); }

/* ---------------------------------------------------------------- the owner --- */
void qd_quad_owner_init(qd_quad_owner_t *o)
{
    memset(o, 0, sizeof *o);
    o->p[P_VOICE] = V_POLY;
    o->bpm = 120;
    o->last_pitch = -1;                                 /* (E1: no note yet: no glide) */
}

void qd_quad_owner_block(qd_quad_owner_t *o)
{
    song.g[G_BPM] = o->bpm;
    quad_block(o);
}

void qd_quad_owner_publish(qd_quad_owner_t *o)
{
    const qd_quad_voice_t *v = o->m_v;
    int k;
    if (!o->defer_off || !v || !v->nx_ok)
        return;
    o->quad_pan_off = v->nx_pan;
    for (k = 0; k < 4; k++)
        o->quad_fx_off[k] = v->nx_fx[k];
}

/* P2.6: a voice of factory preset k (0 INIT .. 16), note held, rendered nblocks control blocks into a scratch block
 * of its own; the samples' sum (so nothing is left out). For the port to time on the unit, with nothing else
 * running: the engine's own cost there. Its own owner and voice: the app's are not touched. P2.6c: in parts, so the
 * timed span can be ChoralRoot's: qd_quad_bench_start (the voice struck), then qd_quad_bench_block(what) a block:
 * what 0 the owner's block and qd_quad_voice_render (the voice's wrapper: its end test, glide, pitch, ramps) and
 * quad_render; what 1 quad_render alone (the engine's block, as ChoralRoot's track_render_x calls it a voice, the
 * pitch and amplitudes made once at the start) */
static qd_quad_owner_t qb_o;
static qd_quad_voice_t qb_v;
static int16_t qb_p[QD_QUAD_NP];
static vmod_t qb_m;
static int32_t qb_out[QD_QUAD_CTL];
void qd_quad_bench_start(int k, int note)
{
    qd_quad_preset_patch(k, qb_p);
    qd_quad_owner_init(&qb_o);
    qb_o.quad_patch = qb_p;
    qd_quad_voice_init(&qb_v, 0);
    qd_quad_voice_start(&qb_v, qb_p, &qb_o, note, 100);
    memset(&qb_m, 0, sizeof qb_m);
    qb_m.pitch16 = qb_v.pitch_cur;
    qb_m.inc = pitch_inc((uint32_t)qb_v.pitch_cur);
    qb_m.shape = 64 << 8;
    qb_m.amp0 = qb_m.amp1 = 32767;
    qb_v.env_out = 32767, qb_v.env = 1 << 24;
}
int32_t qd_quad_bench_block(int what)
{
    int32_t sum = 0;
    int i;
    memset(qb_out, 0, sizeof qb_out);
    if (what == 1) {
        quad_render(&qb_o, &qb_v, qb_out, CTL, &qb_m);
    } else {
        qd_quad_owner_block(&qb_o);
        qd_quad_voice_render(&qb_v, qb_out);
    }
    for (i = 0; i < QD_QUAD_CTL; i++)
        sum += qb_out[i];
    return sum;
}
int32_t qd_quad_bench(int k, int note, int nblocks)
{
    int32_t sum = 0;
    int b;
    qd_quad_bench_start(k, note);
    for (b = 0; b < nblocks; b++)
        sum += qd_quad_bench_block(0);
    return sum;
}

/* P2.6 (tests): a hash of what the voices share outside themselves (the song stub, who holds each comb line) */
uint32_t qd_quad_shared_sig(void)
{
    const uint8_t *b = (const uint8_t *)(const void *)&song;
    uint32_t h = 2166136261u, i;
    for (i = 0; i < sizeof song; i++)
        h = (h ^ b[i]) * 16777619u;
    b = (const uint8_t *)(const void *)qe_comb_by;
    for (i = 0; i < sizeof qe_comb_by; i++)
        h = (h ^ b[i]) * 16777619u;
    return h;
}

void qd_quad_voice_prepare(qd_quad_voice_t *v) { qe_comb_prepare(v, (int)((quad_voice_t *)(void *)v->qstate)->idx); }

/* ---------------------------------------------------------------- a voice --- */
void qd_quad_voice_init(qd_quad_voice_t *v, uint32_t idx)
{
    memset(v, 0, sizeof *v);
    quad_init_patch(v->patch);
    qe_defaults(v->patch);
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
    v->nx_ok = 0;                                       /* (P2.6: no offsets of its own until its first block) */
    v->fq_sm = v->patch[QD_QP_FREQ], v->rs_sm = v->patch[QD_QP_RESO];   /* (P2.6b: from the note's own: no glide) */
    v->pitch16 = v->pitch_cur = (int32_t)note * 16;
    v->gstep = 0;                                       /* (E1: no glide unless qd_quad_voice_glide says so) */
    {   /* E4: RANDOM: a value a note, from the voice's place and the track's count of notes (deterministic) */
        uint32_t r = ((quad_voice_t *)(void *)v->qstate)->idx * 0x9E3779B9u + ++o->nserial * 0x85EBCA6Bu;
        r ^= r >> 15, r *= 0x2C1B3C6Du, r ^= r >> 12;
        v->mrnd = (int16_t)(r >> 16);
    }
    o->m_v = v;
    if (!sounding) {
        v->env = v->env_out = 0;
        memset(&v->dpre, 0, sizeof v->dpre);            /* (E1: DRIVE PRE from rest) */
        v->comb.fresh = 1;                              /* (E2: the machines from rest) */
        memset(&v->crush, 0, sizeof v->crush);          /* (E3: CRUSH from rest) */
    }
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
    {   /* E1: PORT on (a TIE is held: LEGATO too): the glide from where the voice is; off: at once (as before) */
        const int from = v->pitch_cur;
        v->pitch16 = v->pitch_cur = (int32_t)note * 16;
        v->gstep = 0;
        if (v->patch[QD_QP_PORT])
            qd_quad_voice_glide(v, from);
    }
    v->mvel = (uint8_t)vel;
    if (vel > v->vel)
        v->vel = (uint8_t)vel;
    v->gate = 1;
    song.g[G_BPM] = v->owner->bpm;
    quad_legato(v->owner, v);
}

/* E1: the glide from pitch from to the note: TIME (PSLOPE 0) 1 + PTIM^2 / 8 blocks whatever the interval, RATE (1) a
 * fixed speed (QE_GLRATE); ChoralRoot's voice.c glide_set / its control tick */
void qd_quad_voice_glide(qd_quad_voice_t *v, int from)
{
    v->gstep = 0;
    if (from < 0 || !v->patch[QD_QP_PORT] || from == v->pitch16) {
        v->pitch_cur = v->pitch16;
        return;
    }
    v->pitch_cur = clamp(from, 0, 127 * 16);
    if (!v->patch[QD_QP_PSLOPE])                        /* (E4: TIME with its modulation, the last block's) */
        v->gstep = fz_glide_time_step(v->pitch_cur, v->pitch16, clamp(v->patch[QD_QP_PTIM] + v->ptim_mod, 0, 127));
}

void qd_quad_voice_kill(qd_quad_voice_t *v)
{
    if (v->active) {
        v->gate = 0;
        v->kill = 1;
    }
}

/* one control block of a voice (L1: FIZZ's own driver of the engine's interface): the engine says its amp envelope has
 * ended -> the voice is free; else the block is rendered at the pitch the glide has reached, its amplitude a ramp
 * from where the last block ended to full (the engine's envelopes shape it) or to 0 for a voice given up (it ends
 * after this block: the fade steal) or dipped (PLAN §5: down to 0 and on, its next block ramps up again) */
int qd_quad_voice_render(qd_quad_voice_t *v, int32_t *out)
{
    qd_quad_owner_t *t = v->owner;
    const int32_t full = 32767;
    vmod_t m;
    if (!v->active || !t)
        return 0;
    if (quad_done(t, v)) {                              /* (P2.6: the owner's tempo read from it: no global written) */
        v->env = v->env_out = 0;
        v->active = v->gate = v->kill = 0;
        return 0;
    }
    if (v->pitch_cur != v->pitch16)                     /* E1: the glide, a step a block */
        v->pitch_cur = fz_glide_move(v->pitch_cur, v->pitch16,
                                     v->gstep ? v->gstep : (int32_t)QE_GLRATE[clamp(v->patch[QD_QP_PTIM] + v->ptim_mod, 0, 127)]);
    memset(&m, 0, sizeof m);
    m.pitch16 = v->pitch_cur;
    m.inc = pitch_inc((uint32_t)v->pitch_cur);
    if (v->udet)                                        /* (a unison voice: its detune, a block at a time) */
        m.inc = (uint32_t)(((uint64_t)m.inc * v->udet) >> 16);
    m.shape = 64 << 8;                                  /* (the engine's middle: FM TONE does not read it) */
    m.amp0 = v->env_out;
    v->env_out = v->kill || v->dip ? 0 : full;
    m.amp1 = v->env_out;
    v->env = 1 << 24;                                   /* (the voice's own envelope: the engine's are used) */
    quad_render(t, v, out, CTL, &m);
    if (v->kill) {                                      /* (faded to 0 over this block: gone) */
        v->env = v->env_out = 0;
        v->active = v->gate = v->kill = 0;
    }
    return 1;
}

int32_t qd_quad_voice_level(const qd_quad_voice_t *v)
{
    const quad_voice_t *s = (const quad_voice_t *)(const void *)v->qstate;
    return !v->active ? 0 : s->amp < 0 ? -s->amp : s->amp;
}

int qd_quad_voice_endless(const qd_quad_voice_t *v) { return v->active && quad_endless(v->owner, v); }

/* ---------------------------------------------------------------- DRIVE (P1.4a) --- */
/* the voice's DRIVE (fz_voice.h fz_drive, L1): drive 0..127 (quad_drive's LFO offset added by the caller), 0 leaves the
 * state as it is (switching on does not click) */
void qd_quad_dist(qd_dist_t *st, int drive, int32_t *b, int n)
{
    fz_drive_t d;
    d.hp = st->hp, d.lp1 = st->lp1, d.lp2 = st->lp2;
    fz_drive(&d, drive, b, n);
    st->hp = d.hp, st->lp1 = d.lp1, st->lp2 = d.lp2;
}

/* ---------------------------------------------------------------- patches --- */
void qd_quad_preset_patch(int k, int16_t *p)
{
    qe_defaults(p);                                     /* (E1: the values 74..: today's sound) */
    if (k <= 0 || k > (int)QUAD_NPRESETS) {
        quad_init_patch(p);
        p[QD_QP_DRIVE] = 0;
    } else {
        quad_preset_patch((uint32_t)k - 1u, p);
        p[QD_QP_DRIVE] = (int16_t)(QUAD_PRESETS[k - 1].fx[0] ? QUAD_PRESETS[k - 1].fx[0] - 1 : 0);   /* (FX(): + 1, 0
                                                         * unset) cr_ui.c: FM TONE's DRIVE is its preset's */
    }
}

const char *qd_quad_preset_name(int k)
{
    return k <= 0 || k > (int)QUAD_NPRESETS ? "INIT" : QUAD_PRESETS[k - 1].name;
}

void qd_quad_pack5(const int16_t *p, uint8_t *blob)
{
    quad_pack(p, blob);
    blob[QUAD_V5_LEN] = (uint8_t)clamp(p[QD_QP_DRIVE], 0, 127);   /* P1.4a: DRIVE after version 5's values */
}
/* 2026-10-26: the VOICE page's values in the blob's last four bytes (124..127, zeros in every older blob), each a
 * byte that is 0 at its default: 124 VOICE (bit 0) and PRIORITY ((value - 1) & 3, bits 1..2), 125 UNISON - 1, 126
 * DETUNE - 20, 127 SPREAD - 64 (mod 256) */
#define QV_O (QD_QUAD_BLOB - 4)
static void qv_pack(const int16_t *p, uint8_t *b)
{
    b[0] = (uint8_t)((clamp(p[QD_QP_VMODE], 0, 1) & 1) | (((clamp(p[QD_QP_VPRIO], 0, 2) - 1) & 3) << 1));
    b[1] = (uint8_t)(clamp(p[QD_QP_UNI], 1, 8) - 1);
    b[2] = (uint8_t)((clamp(p[QD_QP_UDET], 0, 127) - 20) & 255);
    b[3] = (uint8_t)((clamp(p[QD_QP_USPR], 0, 127) - 64) & 255);
}
static int qv_unpack(const uint8_t *b, int16_t *p)
{
    const int pr = ((b[0] >> 1) + 1) & 3, det = (b[2] + 20) & 255, spr = (b[3] + 64) & 255;
    if ((b[0] & 0xF8u) || pr > 2 || b[1] > 7 || det > 127 || spr > 127)
        return 0;
    p[QD_QP_VMODE] = (int16_t)(b[0] & 1), p[QD_QP_VPRIO] = (int16_t)pr, p[QD_QP_UNI] = (int16_t)(b[1] + 1);
    p[QD_QP_UDET] = (int16_t)det, p[QD_QP_USPR] = (int16_t)spr;
    return 1;
}
int qd_quad_lockable(int id) { return id >= 0 && id < QD_QUAD_NP && (id < QD_QP_VMODE || id > QD_QP_VPRIO); }

/* E1: version 6: version 5's values, DRIVE at 87, then the values 74..109 a byte each (value - min), then the VOICE
 * page's four bytes (qv_pack) */
void qd_quad_pack(const int16_t *p, uint8_t *blob)
{
    int i, o = (int)QUAD_V5_LEN + 1;
    qd_quad_pack5(p, blob);
    blob[1] = 6;
    for (i = QD_QP_AMOD; i < QD_QP_VMODE; i++) {
        const qe_val_t r = qe_val(i);
        blob[o++] = (uint8_t)(clamp(p[i], r.min, r.max) - r.min);
    }
    qv_pack(p, blob + QV_O);
}
int qd_quad_unpack(const uint8_t *blob, int n, int16_t *p)
{
    uint8_t b[QUAD_BLOB];
    int ok, i, o;
    if (n < (int)QUAD_BLOB || !blob) {
        quad_init_patch(p);
        qe_defaults(p);
        p[QD_QP_DRIVE] = 0;
        return 0;
    }
    memcpy(b, blob, sizeof b);
    if (b[0] == QUAD_MAGIC && b[1] == 6u) {             /* E1: version 6 */
        if (n < QD_QUAD_BLOB)
            b[0] = 0;                                   /* (cut short: not a blob) */
        b[1] = QUAD_VER;
        for (i = (int)QUAD_V5_LEN; i < (int)QUAD_BLOB; i++)
            b[i] = 0;
    } else if (b[0] == QUAD_MAGIC && b[1] == QUAD_VER) {
        b[QUAD_V5_LEN] = 0;                             /* (DRIVE: not FM TONE's; the rest must be zeros) */
    }
    ok = quad_unpack(b, p);
    qe_defaults(p);
    p[QD_QP_DRIVE] = (int16_t)(ok && (blob[1] == QUAD_VER || blob[1] == 6u) && blob[QUAD_V5_LEN] <= 127
                                   ? blob[QUAD_V5_LEN] : 0);
    if (ok && blob[1] == 6u) {
        o = (int)QUAD_V5_LEN + 1;
        for (i = QD_QP_AMOD; i < QD_QP_VMODE; i++, o++) {
            const qe_val_t r = qe_val(i);
            if (blob[o] > (uint32_t)(r.max - r.min))
                ok = 0;
            else
                p[i] = (int16_t)(blob[o] + r.min);
        }
        if (!qv_unpack(blob + QV_O, p))                 /* (the VOICE page: 124..127) */
            ok = 0;
        if (!ok) {
            quad_init_patch(p);
            qe_defaults(p);
            p[QD_QP_DRIVE] = 0;
        }
    }
    return ok;
}

void qd_quad_algo(int a, uint8_t *mod4, int *fb, int *outx, int *outy)
{
    const quad_algo_t *q = &QUAD_ALGO[(a < 1 ? 1 : a > 8 ? 8 : a) - 1];
    memcpy(mod4, q->mod, 4);
    *fb = q->fb;
    *outx = q->x;
    *outy = q->y;
}

const char *qd_quad_value_text(int i, int v, char *buf)
{
    const param_desc_t *d = 0;
    uint32_t pg, c;
    int32_t a;
    buf[0] = 0;
    if (i >= QD_QP_AMOD && i < QD_QUAD_NP) {             /* E1: the values 74.. */
        const qe_val_t r = qe_val(i);
        if (r.names && v >= r.min && v <= r.max)
            return strcpy(buf, r.names[v - r.min]);
        if (i == QD_QP_UNI && v <= 1)                   /* (the VOICE page: UNISON 1 is OFF) */
            return strcpy(buf, "OFF");
        if (i == QD_QP_HOLD && v >= 127)
            return strcpy(buf, "NOTE");
        if ((i == QD_QP_BITS || i == QD_QP_SRR) && v <= 0)   /* (E3: CRUSH: OFF; BITS its bits, SRR its divider) */
            return strcpy(buf, "OFF");
        if (i == QD_QP_BITS) {
            snprintf(buf, 12, "%d", qe_crush_bits(v));
            return buf;
        }
        if (i == QD_QP_SRR) {
            /* "/%.1f" of 2^32 / inc, in integers (P2.4: no double): tenths rounded half to even, as printf */
            const uint32_t inc = QE_SRR_INC[v & 127];
            uint32_t r;
            uint64_t q = qd_udivmod64((uint64_t)10 << 32, inc, &r);
            if ((uint64_t)r * 2u > inc || ((uint64_t)r * 2u == inc && (q & 1u)))
                q++;
            {
                uint32_t d;
                const uint64_t w = qd_udivmod64(q, 10u, &d);
                snprintf(buf, 12, "/%u.%u", (unsigned)w, (unsigned)d);
            }
            return buf;
        }
        if (i >= QD_QP_MOD0 && i < QD_QP_VMODE && (i - QD_QP_MOD0) % 3 == QD_QM_DST)
            return strcpy(buf, v >= 0 && v < (int)QD_N ? N_QUAD_DEST[v] : "?");
        snprintf(buf, 12, r.min < 0 && v > 0 ? "+%d" : "%d", v);
        return buf;
    }
    if (i == QD_QP_DRIVE || i == QP_RB1 || i == QP_RB2) {
        if (i != QD_QP_DRIVE && v >= 0 && v < (int)QUAD_NRCB)
            return strcpy(buf, N_QUAD_RCB[v]);
        snprintf(buf, 12, "%d", v);
        return buf;
    }
    for (pg = 0; pg < QUAD_NPAGES && !d; pg++)
        for (c = 0; c < 4u; c++)
            if (QUAD_MAP[pg][c] == (uint32_t)i) {
                d = &QUAD_PAGES[pg].col[c];
                break;
            }
    if (!d) {
        snprintf(buf, 12, "%d", v);
        return buf;
    }
    switch (d->fmt) {
    case F_FIX2:
        a = v < 0 ? -v : v;
        if (d->min < 0)
            snprintf(buf, 12, "%c%d.%02d", v < 0 ? '-' : '+', (int)(a / 100), (int)(a % 100));
        else
            snprintf(buf, 12, "%d.%02d", (int)(a / 100), (int)(a % 100));
        break;
    case F_ENUM:
        if (d->names && v >= d->min && v <= d->max)
            snprintf(buf, 12, "%s", d->names[v - d->min]);
        break;
    case F_ONOFF:
        strcpy(buf, v ? "ON" : "OFF");
        break;
    case F_INTI:
        if (v >= d->max)
            strcpy(buf, "INF");
        else
            snprintf(buf, 12, "%d", v);
        break;
    case F_OFS:
        snprintf(buf, 12, v > 0 ? "+%d" : "%d", v);
        break;
    default:
        if (d->names && v >= d->min && v <= d->max)
            snprintf(buf, 12, "%s", d->names[v - d->min]);
        else
            snprintf(buf, 12, "%d", v);
        break;
    }
    return buf;
}

const char *qd_quad_value_name(int i)
{
    uint32_t pg, c;
    if (i == QD_QP_DRIVE)
        return "DRIVE";
    if (i >= QD_QP_AMOD && i < QD_QUAD_NP)              /* E1 */
        return qe_val(i).name;
    if (i == QP_RB1)
        return "RATIO B1";
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
    if (i >= QD_QP_AMOD && i < QD_QUAD_NP) {             /* E1: the values 74.. */
        const qe_val_t e = qe_val(i);
        if (min)
            *min = e.min;
        if (max)
            *max = e.max;
        if (def)
            *def = e.def;
        return 1;
    }
    if (i == QD_QP_DRIVE) {                             /* P1.4a: 0..127, 0 */
        if (min)
            *min = 0;
        if (max)
            *max = 127;
        if (def)
            *def = 0;
        return 1;
    }
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

/* ---------------------------------------------------------------- P1.5k: the LFOs' destinations (eng_quad.c QD_*) */
static int quad_dest_id(int d)
{
    switch (d) {
    case QD_HOLD: return QD_QP_HOLD;                    /* (E4: FIZZ's own) */
    case QD_PTIM: return QD_QP_PTIM;
    case QD_FSHAPE: return QD_QP_FSHAPE;
    case QD_FGAIN: return QD_QP_FGAIN;
    case QD_BITS: return QD_QP_BITS;
    case QD_SRR: return QD_QP_SRR;
    case QD_ALGO: return QP_ALGO;
    case QD_RC: return QP_RC;
    case QD_RA: return QP_RA;
    case QD_RB: return QP_RB1;                          /* (RATIO B: the pair) */
    case QD_OFSC: return QP_OFSC;
    case QD_OFSA: return QP_OFSA;
    case QD_OFSB1: return QP_OFSB1;
    case QD_OFSB2: return QP_OFSB2;
    case QD_HARM: return QP_HARM;
    case QD_DTUN: return QP_DTUN;
    case QD_FDBK: return QP_FDBK;
    case QD_MIX: return QP_MIX;
    case QD_ALEV: return QP_ALEV;
    case QD_BLEV: return QP_BLEV;
    case QD_AATK: return QP_AATK;
    case QD_ADEC: return QP_ADEC;
    case QD_AEND: return QP_AEND;
    case QD_BATK: return QP_BATK;
    case QD_BDEC: return QP_BDEC;
    case QD_BEND: return QP_BEND;
    case QD_ADLY: return QP_ADLY;
    case QD_BDLY: return QP_BDLY;
    case QD_FREQ: return QP_FREQ;
    case QD_RESO: return QP_RESO;
    case QD_FENV: return QP_FDEPTH;
    case QD_BASE: return QP_BASE;
    case QD_WIDTH: return QP_WIDTH;
    case QD_FATK: return QP_FATK;
    case QD_FDEC: return QP_FDEC;
    case QD_FSUS: return QP_FSUS;
    case QD_FREL: return QP_FREL;
    case QD_EATK: return QP_EATK;
    case QD_EDEC: return QP_EDEC;
    case QD_ESUS: return QP_ESUS;
    case QD_EREL: return QP_EREL;
    case QD_LEVEL: return QP_LEVEL;
    case QD_L1SPD: return QP_LFO(0, QL_SPEED);
    case QD_L1MUL: return QP_LFO(0, QL_MULT);
    case QD_L1FAD: return QP_LFO(0, QL_FADE);
    case QD_L1WAV: return QP_LFO(0, QL_WAVE);
    case QD_L1SPH: return QP_LFO(0, QL_PHASE);
    case QD_L1MOD: return QP_LFO(0, QL_TRIG);
    case QD_L1DEP: return QP_LFO(0, QL_DEPTH);
    case QD_DRIVE: return QD_QP_DRIVE;
    case QD_FDLY: return QP_FDLY;
    default: return -1;                                 /* (PITCH, P AB2, PAN, R ALL, the AB macros, the sends: no one
                                                         * value of a page) */
    }
}
int qd_quad_dest_id(int dest) { return dest < 0 || dest >= (int)QD_N ? -1 : quad_dest_id(dest); }
int qd_quad_dest_of(int id, int lfo)
{
    int d;
    if (id == QP_RB2)
        id = QP_RB1;
    for (d = 1; d < (int)QD_N; d++)
        if (quad_dest_id(d) == id) {
            if (lfo == 0 && d >= QD_L1SPD && d <= QD_L1DEP)
                return -1;                              /* (LFO 1 on itself: none, ChoralRoot's rule) */
            return d;
        }
    return -1;
}
int qd_quad_ndest(void) { return (int)QD_N; }
int qd_quad_owner_lfos(const qd_quad_owner_t *o, int16_t *w, uint8_t *dest)
{
    const qd_quad_voice_t *v = o ? o->m_v : 0;
    int k;
    if (!v || !v->active || v->owner != o || !v->nx_ok)
        return 0;
    for (k = 0; k < 3; k++) {
        w[k] = v->nx_lw[k];
        dest[k] = (uint8_t)v->patch[QP_LFO(k, QL_DEST)];
    }
    return 1;
}
int qd_quad_lfo_value(int id, int v, const int16_t *w, const uint8_t *dest)
{
    int16_t lo, hi;
    int32_t sum = 0, sc;
    int k;
    if (id == QP_RB1 || id == QP_RB2 || id == QP_LFO(0, QL_MULT) || !qd_quad_value_range(id, &lo, &hi, 0))
        return v;
    for (k = 0; k < 3; k++)
        if (w[k] && dest[k] && qd_quad_dest_of(id, k) == dest[k])
            sum += w[k];
    if (!sum)
        return v;
    sc = lo < 0 && lo == -hi ? hi : hi - lo;            /* (the engine's scales: HARM, OFS +-; the rest the range) */
    v += (int)((sum * sc) >> 15);
    return v < lo ? lo : v > hi ? hi : v;
}
const char *qd_quad_dest_name(int dest) { return dest >= 0 && dest < (int)QD_N ? N_QUAD_DEST[dest] : "?"; }
int qd_quad_dest_step(int dest, int d)                   /* (ChoralRoot's editor order, the Digitone's) */
{
    int k, at = 0;
    for (k = 0; k < (int)QD_N; k++)
        if (QUAD_DEST_ORDER[k] == dest)
            at = k;
    at += d;
    at = at < 0 ? 0 : at >= (int)QD_N ? (int)QD_N - 1 : at;
    return QUAD_DEST_ORDER[at];
}
int qd_quad_dest_pos(int dest)                          /* (its place in that order; 0: none) */
{
    int k;
    for (k = 0; k < (int)QD_N; k++)
        if (QUAD_DEST_ORDER[k] == dest)
            return k;
    return 0;
}
