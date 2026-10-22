/* SPDX-License-Identifier: GPL-3.0-only */
/* FM TONE as a library (quad_voice.h): ChoralRoot's eng_quad.c (v0.25.1, copied with the changes its header lists) on
 * the parts of core.h it reads (cr_core.h), dsp.c and the generated tables (cr_tables.h), all in this one unit as
 * ChoralRoot's tests/cr_quad_test.c builds them; then the interface. The driver below is cr_quad_test.c's voice
 * driver (drv_on, drv_tick: voice.c's control tick for an ownenv engine, done then render) plus ChoralRoot's fade
 * steal (voice.c voice_kill / env_tick stage 4: a voice given up renders one more block with its amplitude ramped to
 * 0, m.amp1 = 0, then ends). */
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
#include "cr_tables.h"
#include "cr_core.h"
typedef qd_quad_voice_t voice_t;        /* (eng_quad.c's voice_t and track_t: QUADRILLE's voice and owner) */
typedef qd_quad_owner_t track_t;
#include "dsp.c"
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
                              QD_QUAD_NPRESET == 1 + QUAD_NPRESETS && QD_QP_MOD(QD_NMOD - 1, QD_QM_DEPTH) + 1 == QD_QUAD_NP &&
                              QUAD_V5_LEN + 1 + (QD_QUAD_NP - QD_QUAD_NP5) <= QD_QUAD_BLOB ? 1 : -1];
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
static qe_val_t qe_val(int i)
{
    static const qe_val_t T[QD_QP_MOD0 - QD_QP_AMOD] = {
        {0, 1, 0, "MODE", QE_AMOD}, {0, 127, 127, "HOLD", 0}, {0, 2, 0, "PORT", QE_PORT}, {0, 127, 32, "TIME", 0},
        {0, 1, 0, "SLOPE", QE_PSLOPE}, {0, 1, 0, "ROUTE", QE_PREPOST}, {0, 127, 0, "SHAPE", 0},
        {-64, 63, 0, "GAIN", 0}, {0, 1, 0, "ROUTE", QE_BEFAFT}, {0, 127, 0, "BITS", 0}, {0, 127, 0, "SRR", 0},
        {0, 1, 1, "ROUTE", QE_BEFAFT}};
    static const qe_val_t M[3] = {{0, 8, 0, "SOURCE", 0}, {0, (int16_t)(QD_N - 1), 0, "DEST", 0},
                                  {-127, 127, 0, "DEPTH", 0}};
    static const qe_val_t Z = {0, 0, 0, "?", 0};
    if (i >= QD_QP_AMOD && i < QD_QP_MOD0)
        return T[i - QD_QP_AMOD];
    if (i >= QD_QP_MOD0 && i < QD_QUAD_NP)
        return M[(i - QD_QP_MOD0) % 3];
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
    v->pitch16 = v->pitch_cur = (int32_t)note * 16;
    v->gstep = 0;                                       /* (E1: no glide unless qd_quad_voice_glide says so) */
    o->m_v = v;
    if (!sounding) {
        v->env = v->env_out = 0;
        memset(&v->dpre, 0, sizeof v->dpre);            /* (E1: DRIVE PRE from rest) */
        v->comb.fresh = 1;                              /* (E2: the machines from rest) */
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
    int32_t d, ticks;
    v->gstep = 0;
    if (from < 0 || !v->patch[QD_QP_PORT] || from == v->pitch16) {
        v->pitch_cur = v->pitch16;
        return;
    }
    v->pitch_cur = clamp(from, 0, 127 * 16);
    if (!v->patch[QD_QP_PSLOPE]) {
        d = v->pitch16 - v->pitch_cur;
        d = d < 0 ? -d : d;
        ticks = 1 + v->patch[QD_QP_PTIM] * v->patch[QD_QP_PTIM] / 8;
        v->gstep = d / ticks > 0 ? d / ticks : 1;
    }
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
    if (v->pitch_cur != v->pitch16) {                   /* E1: the glide, a step a block (voice.c's control tick) */
        const int32_t d = v->pitch16 - v->pitch_cur;
        int32_t st = v->gstep ? v->gstep : (int32_t)QE_GLRATE[v->patch[QD_QP_PTIM] & 127];
        st = st < 1 ? 1 : st;
        v->pitch_cur += d > 0 ? (d < st ? d : st) : (-d < st ? d : -st);
    }
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

/* ---------------------------------------------------------------- DRIVE (P1.4a) --- */
/* ChoralRoot v0.25.1 fx.c track_dist, line for line: t->p[P_DIST] is drive (quad_drive's LFO offset added by the
 * caller), t->dist_* the voice's st */
void qd_quad_dist(qd_dist_t *st, int drive, int32_t *b, int n)
{
    int32_t d = drive, i, g, k, mk, bias = 2400, b0;
    if (!d)
        return;                                         /* states kept: switching on does not click */
    g = 4096 + d * d * 2;                                /* Q12: 1x .. ~9x, gentle at first */
    k = 32000 - d * 95;                                  /* tone: transparent at low drive .. ~3 kHz, Q15 */
    mk = 30000 - d * 120;                                /* make-up */
    b0 = softclip(bias);
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = b[i], y;
        st->hp += (x - st->hp + 64) >> 7;                /* ~55 Hz low cut: keep the bass out of the clipper */
        x = clamp(x - st->hp, -230000, 230000);          /* (x >> 2) * g fits 32 bits; the clip is flat out there */
        y = softclip((((x >> 2) * g) >> 10) + bias) - b0;   /* >> 2 first: no overflow for loud poly */
        st->lp1 += mulq15(y - st->lp1, k);               /* two poles: tames the fizz */
        st->lp2 += mulq15(st->lp1 - st->lp2, k);
        b[i] = mulq15(st->lp2, mk);
    }
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
/* E1: version 6: version 5's values, DRIVE at 87, then the values 74.. a byte each (value - min), zeros to 128 */
void qd_quad_pack(const int16_t *p, uint8_t *blob)
{
    int i, o = (int)QUAD_V5_LEN + 1;
    qd_quad_pack5(p, blob);
    blob[1] = 6;
    for (i = QD_QP_AMOD; i < QD_QUAD_NP; i++) {
        const qe_val_t r = qe_val(i);
        blob[o++] = (uint8_t)(clamp(p[i], r.min, r.max) - r.min);
    }
    while (o < QD_QUAD_BLOB)
        blob[o++] = 0;
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
        for (i = QD_QP_AMOD; i < QD_QUAD_NP; i++, o++) {
            const qe_val_t r = qe_val(i);
            if (blob[o] > (uint32_t)(r.max - r.min))
                ok = 0;
            else
                p[i] = (int16_t)(blob[o] + r.min);
        }
        for (; o < QD_QUAD_BLOB; o++)
            if (blob[o])
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
        if (i == QD_QP_HOLD && v >= 127)
            return strcpy(buf, "NOTE");
        if (i >= QD_QP_MOD0 && (i - QD_QP_MOD0) % 3 == QD_QM_DST)
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
