/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.6c: MIDI FX groups (mfx.h). Built through player.c (it includes this file: the core's source lists stay as they
 * are). */
#include <stdio.h>
#include <string.h>

#include "chord.h"
#include "mfx.h"
#include "scale.h"

/* ---------------------------------------------------------------- the values */
static const int16_t RATES[QD_MFX_NRATE] = {1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64};
int qd_mfx_rate_of(int idx) { return RATES[idx < 0 ? 0 : idx >= QD_MFX_NRATE ? QD_MFX_NRATE - 1 : idx]; }

static const char *const TYPE_NAME[QD_MFX_NTYPE] = {"NONE", "ARP", "CHANCE", "REPEAT", "HARM", "TRANSPOSE", "SCALER",
                                                    "RANDOM", "CHORD", "MONO", "SELECTOR"};
const char *qd_mfx_type_name(int type) { return type >= 0 && type < QD_MFX_NTYPE ? TYPE_NAME[type] : "?"; }

#define P_(n, lo, hi, def, k, st) {n, lo, hi, def, QD_MFX_##k, st}
#define CHC P_("CHANCE", 0, 100, 100, KPCT, 5)
#define NOP {0, 0, 0, 0, 0, 0}
static const qd_mfx_par_t PAR[QD_MFX_NTYPE][QD_MFX_NV] = {
    {NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP},
    {P_("PATTERN", 0, QD_NAP - 1, QD_AP_UP, KENUM, 1), P_("RATE", 0, 15, 6, KRATE, 1), P_("RANGE", 1, 8, 2, KNUM, 1), CHC,
     P_("MODE", 0, QD_NAM - 1, QD_AM_ON, KENUM, 1), P_("GATE", 2, 200, 90, KPCT, 5), P_("OCT DIST", -24, 24, 12, KSIGN, 1),
     P_("QUANT", 0, 16, 10, KQUANT, 1), P_("RESET", 0, QD_NAR - 1, QD_AR_NORM, KENUM, 1), P_("LENGTH", 1, 16, 16, KNUM, 1),
     P_("LENGTH", 1, 16, 16, KNUM, 1), P_("", 1, QD_NAM - 1, QD_AM_ON, KENUM, 1)},
    {P_("CHANCE", 0, 100, 100, KPCT, 5), NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP},
    {P_("RATE", 0, 15, 6, KRATE, 1), P_("HITS", 1, 64, 4, KNUM, 1), P_("FADE", -100, 100, 0, KPM, 5), CHC,
     P_("MODE", 0, QD_NAM - 1, QD_AM_ON, KENUM, 1), P_("GATE", 2, 200, 90, KPCT, 5), P_("QUANT", 0, 16, 10, KQUANT, 1),
     NOP, NOP, NOP, NOP, NOP},
    {P_("NOTE 2", -48, 48, 7, KSIGN, 1), P_("NOTE 3", -48, 48, 12, KSIGN, 1), P_("NOTE 4", -48, 48, 0, KSIGN, 1), CHC,
     P_("NOTE 5", -48, 48, 0, KSIGN, 1), P_("NOTE 6", -48, 48, 0, KSIGN, 1), P_("NOTE 7", -48, 48, 0, KSIGN, 1),
     P_("ORIG", 0, 1, 1, KONOFF, 1), NOP, NOP, NOP, NOP},
    {P_("SEMI", -24, 24, 0, KSIGN, 1), P_("OCT", -6, 6, 1, KSIGN, 1), NOP, CHC, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP},
    {P_("SCALE", 1, QD_NSCALE - 1, QD_SCALE_MAJOR, KSCALE, 1), P_("ROOT", 0, 11, 0, KROOT, 1), P_("KEY", 0, 1, 0, KENUM, 1),
     CHC, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP},
    {P_("PITCH", 0, 12, 2, KPM, 1), P_("VEL", 0, 127, 20, KPM, 5), P_("LENGTH", 0, 100, 0, KPCT, 5), CHC,
     P_("OCTAVE", 0, 4, 0, KPM, 1), P_("DELAY", 0, 16, 0, KDELAY, 1), NOP, NOP, NOP, NOP, NOP, NOP},
    {P_("CHORD", 1, QD_NSHAPE - 1, 1, KSHAPE, 1), P_("NOTES", 1, 6, 3, KNUM, 1), P_("INVERT", 0, 5, 0, KNUM, 1), CHC,
     P_("SPREAD", -2, 2, 0, KSIGN, 1), P_("VOICING", 0, QD_NCV - 1, QD_CV_CLOSE, KENUM, 1), NOP, NOP, NOP, NOP, NOP, NOP},
    {NOP, NOP, NOP, CHC, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP},
    {P_("MODE", 0, 2, QD_SM_RND, KENUM, 1), P_("LENGTH", 0, 7, 2, KNUM, 1), NOP, CHC, NOP, NOP, NOP, NOP, NOP, NOP, NOP,
     NOP},
};
/* the pages: KNOB 1..4's values (an index of v, QD_MFX_ID_LANE, -1) */
#define L_ QD_MFX_ID_LANE
static const int8_t PAGES[QD_MFX_NTYPE][4][4] = {
    {{-1, -1, -1, -1}},
    {{QD_MA_PAT, QD_MA_RATE, QD_MA_RANGE, QD_MA_CHANCE}, {QD_MA_MODE, QD_MA_GATE, QD_MA_ODIST, QD_MA_QUANT},
     {L_, QD_MA_RESET, -1, QD_MA_MLEN}, {L_, -1, -1, QD_MA_TLEN}},
    {{QD_MC_CHANCE, -1, -1, -1}},
    {{QD_MR_RATE, QD_MR_HITS, QD_MR_FADE, QD_MR_CHANCE}, {QD_MR_MODE, QD_MR_GATE, QD_MR_QUANT, -1}},
    {{QD_MH_N2, QD_MH_N3, QD_MH_N4, QD_MH_CHANCE}, {QD_MH_N5, QD_MH_N6, QD_MH_N7, QD_MH_ORIG}},
    {{QD_MT_SEMI, QD_MT_OCT, -1, QD_MT_CHANCE}},
    {{QD_MZ_SCALE, QD_MZ_ROOT, QD_MZ_KEY, QD_MZ_CHANCE}},
    {{QD_MD_PITCH, QD_MD_VEL, QD_MD_LEN, QD_MD_CHANCE}, {QD_MD_OCT, QD_MD_DELAY, -1, -1}},
    {{QD_MK_SHAPE, QD_MK_NOTES, QD_MK_INV, QD_MK_CHANCE}, {QD_MK_SPREAD, QD_MK_VOICING, -1, -1}},
    {{-1, -1, -1, QD_MM_CHANCE}},
    {{QD_ML_MODE, QD_ML_LEN, -1, QD_ML_CHANCE}},
};
static const uint8_t NPAGES[QD_MFX_NTYPE] = {0, 4, 1, 2, 2, 1, 1, 2, 2, 1, 1};

int qd_mfx_npages(int type) { return type > 0 && type < QD_MFX_NTYPE ? NPAGES[type] : 0; }
int qd_mfx_page_id(int type, int page, int k)
{
    if (type <= 0 || type >= QD_MFX_NTYPE || page < 0 || page >= NPAGES[type] || k < 0 || k > 3)
        return -1;
    return PAGES[type][page][k];
}
const qd_mfx_par_t *qd_mfx_par(int type, int id)
{
    if (type <= 0 || type >= QD_MFX_NTYPE || id < 0 || id >= QD_MFX_NV || !PAR[type][id].name)
        return 0;
    return &PAR[type][id];
}
int qd_mfx_chance_id(int type) { return type == QD_MFX_CHANCE ? QD_MC_CHANCE : 3; }

void qd_mfx_slot_init(qd_mfx_slot_t *s, int type)
{
    int i;
    memset(s, 0, sizeof *s);
    if (type <= 0 || type >= QD_MFX_NTYPE)
        return;
    s->type = (uint8_t)type;
    for (i = 0; i < QD_MFX_NV; i++)
        s->v[i] = PAR[type][i].name ? PAR[type][i].def : 0;
}

static const char *const N_PAT[QD_NAP] = {"UP", "DOWN", "UP DN", "DN UP", "U+D", "D+U", "CONV", "DIV", "CON DIV",
                                          "HI UP", "HI UPDN", "LO UP", "LO UPDN", "RAND", "RAND OTH", "SHUFFLE",
                                          "AS PLAYED"};
static const char *const N_MODE[QD_NAM] = {"OFF", "ON", "1-ST", "ONCE", "HOLD"};
static const char *const N_RESET[QD_NAR] = {"NORM", "NOTE", "MPAT", "TPAT"};
static const char *const N_KEY[2] = {"OWN", "TRACK"};
static const char *const N_SEL[3] = {"RND", "UP", "DOWN"};
static const char *const N_VOI[QD_NCV] = {"CLOSE", "DROP 2", "DROP 3", "OPEN"};
static const char *enum_name(int type, int id, int v)
{
    const char *const *t = 0;
    int n = 0;
    if (type == QD_MFX_ARP && id == QD_MA_PAT)
        t = N_PAT, n = QD_NAP;
    else if ((type == QD_MFX_ARP && (id == QD_MA_MODE || id == QD_MA_LAST)) || (type == QD_MFX_REPEAT && id == QD_MR_MODE))
        t = N_MODE, n = QD_NAM;
    else if (type == QD_MFX_ARP && id == QD_MA_RESET)
        t = N_RESET, n = QD_NAR;
    else if (type == QD_MFX_SCALER && id == QD_MZ_KEY)
        t = N_KEY, n = 2;
    else if (type == QD_MFX_SELECTOR && id == QD_ML_MODE)
        t = N_SEL, n = 3;
    else if (type == QD_MFX_CHORD && id == QD_MK_VOICING)
        t = N_VOI, n = QD_NCV;
    return t && v >= 0 && v < n ? t[v] : "?";
}

const char *qd_mfx_lane_text(int lane, int v, char *buf)
{
    static const char *const M[QD_AL_N1] = {"x", ".", "-", "R", "<", ">", "\"", "#"};
    if (lane == 0) {
        if (v >= 0 && v < QD_AL_N1)
            snprintf(buf, 12, "%s", M[v]);
        else
            snprintf(buf, 12, "%d", v - QD_AL_N1 + 1);
    } else {
        snprintf(buf, 12, v > 0 ? "+%d" : "%d", v);
    }
    return buf;
}

const char *qd_mfx_value_text(const qd_mfx_slot_t *s, int id, char *buf)
{
    const qd_mfx_par_t *p = qd_mfx_par(s->type, id);
    int v;
    if (!p) {
        buf[0] = 0;
        return buf;
    }
    v = s->v[id];
    switch (p->kind) {
    case QD_MFX_KSIGN: snprintf(buf, 12, v > 0 ? "+%d" : "%d", v); break;
    case QD_MFX_KPCT: snprintf(buf, 12, "%d%%", v); break;
    case QD_MFX_KPM:
        if (s->type == QD_MFX_REPEAT)
            snprintf(buf, 12, v > 0 ? "+%d%%" : "%d%%", v);
        else
            snprintf(buf, 12, v ? "\xC2\xB1%d" : "0", v);   /* (+- n) */
        break;
    case QD_MFX_KRATE: snprintf(buf, 12, "1/%d", qd_mfx_rate_of(v)); break;
    case QD_MFX_KQUANT:
        if (!v)
            snprintf(buf, 12, "OFF");
        else
            snprintf(buf, 12, "1/%d", qd_mfx_rate_of(v - 1));
        break;
    case QD_MFX_KDELAY:
        if (!v)
            snprintf(buf, 12, "OFF");
        else
            snprintf(buf, 12, "1/%d", qd_mfx_rate_of(QD_MFX_NRATE - v));
        break;
    case QD_MFX_KENUM: snprintf(buf, 12, "%s", enum_name(s->type, id, v)); break;
    case QD_MFX_KROOT: snprintf(buf, 12, "%s", qd_pc_name(v)); break;
    case QD_MFX_KSCALE: snprintf(buf, 12, "%s", qd_scale_short(v)); break;
    case QD_MFX_KSHAPE: snprintf(buf, 12, "%s", v == 1 ? "MAJ" : QD_SHAPE[v].name); break;
    case QD_MFX_KONOFF: snprintf(buf, 12, "%s", v ? "ON" : "OFF"); break;
    default: snprintf(buf, 12, "%d", v); break;
    }
    return buf;
}

static int sactive(const qd_mfx_slot_t *s) { return s->type != QD_MFX_NONE && !s->bypass; }

int qd_mfx_group_active(const qd_mfx_group_t *g)
{
    int k;
    for (k = 0; k < QD_MFX_NSLOT; k++)
        if (sactive(&g->slot[k]))
            return 1;
    return 0;
}

int qd_mfx_selector_of(const qd_mfx_group_t *g, int k)
{
    int s;
    for (s = k - 1; s >= 0; s--) {
        const qd_mfx_slot_t *x = &g->slot[s];
        if (sactive(x) && x->type == QD_MFX_SELECTOR && x->v[QD_ML_LEN] > 0 && s + x->v[QD_ML_LEN] >= k)
            return s;
    }
    return -1;
}

int qd_mfx_arp_slot(const qd_mfx_group_t *g)
{
    int k;
    for (k = 0; k < QD_MFX_NSLOT; k++)
        if (g->slot[k].type == QD_MFX_ARP)
            return k;
    return -1;
}

/* ---------------------------------------------------------------- the engine */
static uint32_t rnd(qd_mfx_t *m, uint32_t n)           /* 0 .. n - 1 (xorshift32) */
{
    uint32_t x = m->rng ? m->rng : 0x9E3779B9u;
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    m->rng = x;
    return n ? x % n : 0;
}
static int rpm(qd_mfx_t *m, int r) { return r > 0 ? (int)rnd(m, (uint32_t)(2 * r + 1)) - r : 0; }   /* -r .. r */
static int hit(qd_mfx_t *m, int chance) { return chance >= 100 ? 1 : chance <= 0 ? 0 : (int)rnd(m, 100) < chance; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void qd_mfx_init(qd_mfx_t *m, qd_mfx_group_t *cfg, const qd_mfx_io_t *io)
{
    memset(m, 0, sizeof *m);
    m->cfg = cfg;
    if (io)
        m->io = *io;
    m->rng = 0x4D464658u;
}
void qd_mfx_seed(qd_mfx_t *m, uint32_t seed) { m->rng = seed ? seed : 0x4D464658u; }

static int64_t quant(const qd_mfx_t *m, int q)          /* the grid's next tick at or after now (q 0: now) */
{
    int64_t Q, d;
    if (q <= 0)
        return m->now;
    Q = QD_MFX_BAR / qd_mfx_rate_of(q - 1);
    d = m->now - m->qref;
    d = d < 0 ? -((-d) / Q) * Q : ((d + Q - 1) / Q) * Q;
    return m->qref + d;
}

static void deliver(qd_mfx_t *m, int g, int k, int on, const qd_mfx_note_t *n);
static void fx_in(qd_mfx_t *m, int g, int k, int on, const qd_mfx_note_t *n);

/* slot k's output: past its SELECTOR's block if it is in one, else the next slot */
static void emit(qd_mfx_t *m, int g, int k, int on, const qd_mfx_note_t *n)
{
    const qd_mfx_group_t *G = &m->cfg[g];
    const int s = qd_mfx_selector_of(G, k);
    deliver(m, g, s >= 0 ? s + G->slot[s].v[QD_ML_LEN] + 1 : k + 1, on, n);
}

/* the group's output: the note sent (and kept: held, by (track, prev); timed, to its end), or its OFF: every note its
 * input made released */
static void out_note(qd_mfx_t *m, int g, int on, const qd_mfx_note_t *n)
{
    qd_mfx_grt_t *R = &m->g[g];
    int i;
    if (on) {
        if (n->len > 0) {
            for (i = 0; i < QD_MFX_NPEND && R->pend[i].used; i++)
                ;
            if (i >= QD_MFX_NPEND) {
                R->dropped++;
                return;
            }
            R->pend[i].used = 1, R->pend[i].tick = m->now + n->len;
            R->pend[i].track = n->track, R->pend[i].prev = n->prev, R->pend[i].pitch = n->pitch, R->pend[i].live = n->live;
            R->pend[i].gen = n->gen;
        } else {
            for (i = 0; i < QD_MFX_NOUT && R->out[i].used; i++)
                ;
            if (i >= QD_MFX_NOUT) {
                R->dropped++;
                return;
            }
            R->out[i].used = 1;
            R->out[i].track = n->track, R->out[i].prev = n->prev, R->out[i].pitch = n->pitch, R->out[i].live = n->live;
            R->out[i].gen = n->gen;
        }
        R->sent++;
        if (m->io.on)
            m->io.on(m->io.ctx, g, n);
        return;
    }
    for (i = 0; i < QD_MFX_NOUT; i++)
        if (R->out[i].used && R->out[i].track == n->track && R->out[i].prev == n->prev && R->out[i].gen == n->gen) {
            R->out[i].used = 0;
            if (m->io.off)
                m->io.off(m->io.ctx, g, R->out[i].track, R->out[i].pitch);
        }
    for (i = 0; i < QD_MFX_NPEND; i++)
        if (R->pend[i].used && R->pend[i].track == n->track && R->pend[i].prev == n->prev && R->pend[i].gen == n->gen) {
            R->pend[i].used = 0;
            if (m->io.off)
                m->io.off(m->io.ctx, g, R->pend[i].track, R->pend[i].pitch);
        }
}

static void deliver(qd_mfx_t *m, int g, int k, int on, const qd_mfx_note_t *n)
{
    const qd_mfx_group_t *G = &m->cfg[g];
    while (k < QD_MFX_NSLOT && !sactive(&G->slot[k]))
        k++;
    if (k >= QD_MFX_NSLOT)
        out_note(m, g, on, n);
    else
        fx_in(m, g, k, on, n);
}

/* ---- the state a slot's effect keeps: set up for its type (a change, a bypass, ARP / REPEAT turned OFF: anew; a
 * RANDOM's delayed ends go out first, so nothing it held back hangs) */
static void srt_clear(qd_mfx_t *m, int g, int k)
{
    qd_mfx_srt_t *r = &m->g[g].s[k];
    int i;
    if ((r->type & 0x7F) == QD_MFX_RANDOM)
        for (i = 0; i < QD_MFX_NHOLD; i++)
            if (r->h[i].used && !r->h[i].on)
                out_note(m, g, 0, &r->h[i].n);
    memset(r, 0, sizeof *r);
}
static int srt_mode(const qd_mfx_slot_t *s)
{
    return s->type == QD_MFX_ARP ? s->v[QD_MA_MODE] != QD_AM_OFF : s->type == QD_MFX_REPEAT ? s->v[QD_MR_MODE] != QD_AM_OFF : 1;
}
static void srt_sync(qd_mfx_t *m, int g, int k)
{
    const qd_mfx_slot_t *s = &m->cfg[g].slot[k];
    qd_mfx_srt_t *r = &m->g[g].s[k];
    const uint8_t want = (uint8_t)(s->type | (srt_mode(s) ? 0x80u : 0u));
    if (r->type != want || r->bypass != s->bypass) {
        srt_clear(m, g, k);
        r->type = want;
        r->bypass = s->bypass;
    }
}
static void sync_all(qd_mfx_t *m)
{
    int g, k;
    for (g = 0; g < QD_MFX_NGROUP; g++)
        for (k = 0; k < QD_MFX_NSLOT; k++)
            srt_sync(m, g, k);
}

/* ---- held notes (ARP, REPEAT): h[] compact, in the order they came */
static int held_n(const qd_mfx_srt_t *r)
{
    int i = 0;
    while (i < QD_MFX_NHOLD && r->h[i].used)
        i++;
    return i;
}
static void held_del(qd_mfx_srt_t *r, int i)
{
    const int n = held_n(r);
    if (i < 0 || i >= n)
        return;
    memmove(&r->h[i], &r->h[i + 1], (size_t)(n - i - 1) * sizeof r->h[0]);
    memset(&r->h[n - 1], 0, sizeof r->h[0]);
}
static int held_add(qd_mfx_t *m, qd_mfx_srt_t *r, const qd_mfx_note_t *n)
{
    const int i = held_n(r);
    if (i >= QD_MFX_NHOLD)
        return -1;
    memset(&r->h[i], 0, sizeof r->h[i]);
    r->h[i].used = 1;
    r->h[i].n = *n;
    r->h[i].n.rec = 0;
    r->h[i].until = n->len > 0 ? m->now + n->len : 0;
    return i;
}

/* ---- ARP: the walk of a pattern over the held notes, by pitch */
static void arp_build(qd_mfx_t *m, qd_mfx_srt_t *r, int pat)
{
    const int n = held_n(r);
    int i, j, k = 0;
    for (i = 0; i < n; i++)                             /* by pitch (then as they came) */
        r->order[i] = (uint8_t)i;
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && r->h[r->order[j]].n.pitch < r->h[r->order[j - 1]].n.pitch; j--) {
            const uint8_t x = r->order[j];
            r->order[j] = r->order[j - 1], r->order[j - 1] = x;
        }
#define PUT(x) (r->seq[k++] = (uint8_t)(x))
    switch (pat) {
    case QD_AP_UP: case QD_AP_RAND: case QD_AP_ROTH: case QD_AP_ASP:
        for (i = 0; i < n; i++) PUT(i);
        break;
    case QD_AP_DN:
        for (i = n - 1; i >= 0; i--) PUT(i);
        break;
    case QD_AP_UPDN:
        for (i = 0; i < n; i++) PUT(i);
        for (i = n - 2; i >= 1; i--) PUT(i);
        break;
    case QD_AP_DNUP:
        for (i = n - 1; i >= 0; i--) PUT(i);
        for (i = 1; i <= n - 2; i++) PUT(i);
        break;
    case QD_AP_UANDD:
        for (i = 0; i < n; i++) PUT(i);
        for (i = n - 1; i >= 0; i--) PUT(i);
        break;
    case QD_AP_DANDU:
        for (i = n - 1; i >= 0; i--) PUT(i);
        for (i = 0; i < n; i++) PUT(i);
        break;
    case QD_AP_CON: case QD_AP_DIV: case QD_AP_CONDIV: {
        uint8_t c[QD_MFX_NHOLD];
        int lo = 0, hi = n - 1, nc = 0;
        while (lo <= hi) {
            c[nc++] = (uint8_t)lo++;
            if (lo <= hi)
                c[nc++] = (uint8_t)hi--;
        }
        if (pat == QD_AP_DIV)
            for (i = nc - 1; i >= 0; i--) PUT(c[i]);
        else
            for (i = 0; i < nc; i++) PUT(c[i]);
        if (pat == QD_AP_CONDIV)
            for (i = nc - 2; i >= 1; i--) PUT(c[i]);
        break;
    }
    case QD_AP_HI1: case QD_AP_HI2:
        if (n == 1) {
            PUT(0);
            break;
        }
        for (i = n - 2; i >= 0; i--) PUT(n - 1), PUT(i);
        if (pat == QD_AP_HI2)                           /* (back up, the ends not again) */
            for (i = 1; i <= n - 3; i++) PUT(n - 1), PUT(i);
        break;
    case QD_AP_LO1: case QD_AP_LO2:
        if (n == 1) {
            PUT(0);
            break;
        }
        for (i = 1; i < n; i++) PUT(0), PUT(i);
        if (pat == QD_AP_LO2)
            for (i = n - 2; i >= 2; i--) PUT(0), PUT(i);
        break;
    case QD_AP_RONC:
        for (i = 0; i < n; i++) PUT(i);
        for (i = n - 1; i > 0; i--) {                   /* (a shuffle: every note once) */
            const int x = (int)rnd(m, (uint32_t)(i + 1));
            const uint8_t t = r->seq[i];
            r->seq[i] = r->seq[x], r->seq[x] = t;
        }
        break;
    default:
        break;
    }
#undef PUT
    r->nseq = k;
}

static void arp_restart(qd_mfx_srt_t *r)
{
    r->pos = r->oct = r->mpos = r->tpos = r->passes = r->steps = 0;
    r->last_idx = -1;
}

static void arp_note_on(qd_mfx_t *m, int g, int k, const qd_mfx_note_t *n)
{
    const qd_mfx_slot_t *s = &m->cfg[g].slot[k];
    qd_mfx_srt_t *r = &m->g[g].s[k];
    const int mode = s->v[QD_MA_MODE];
    if ((mode == QD_AM_HOLD || mode == QD_AM_1ST) && r->down <= 0 && held_n(r)) {   /* (a latched set: the next
                                                         * note after every key is up starts a new one) */
        memset(r->h, 0, sizeof r->h);
        r->run = 0;
        r->down = 0;
    }
    if (held_add(m, r, n) < 0)
        return;
    r->down++;
    arp_build(m, r, s->v[QD_MA_PAT]);
    if (s->v[QD_MA_RESET] == QD_AR_NOTE)
        arp_restart(r);
    if (!r->run) {
        r->run = 1;
        arp_restart(r);
        r->next = quant(m, s->v[QD_MA_QUANT]);
    }
}

/* a held note of (track, prev, gen) released: 1 the ARP / REPEAT had it */
static int held_release(qd_mfx_t *m, int g, int k, int track, int prev, int gen)
{
    const qd_mfx_slot_t *s = &m->cfg[g].slot[k];
    qd_mfx_srt_t *r = &m->g[g].s[k];
    const int mode = s->type == QD_MFX_ARP ? s->v[QD_MA_MODE] : s->v[QD_MR_MODE];
    int i, any = 0;
    for (i = held_n(r) - 1; i >= 0; i--) {
        qd_mfx_held_t *h = &r->h[i];
        if (h->n.track != track || h->n.prev != prev || h->n.gen != gen || h->released)
            continue;
        any = 1;
        h->released = 1;
        r->down--;
        if (mode == QD_AM_ON || mode == QD_AM_ONCE || (s->type == QD_MFX_ARP && mode == QD_AM_OFF))
            held_del(r, i);
    }
    if (r->down < 0)
        r->down = 0;
    if (any && s->type == QD_MFX_ARP) {
        arp_build(m, r, s->v[QD_MA_PAT]);
        if (!held_n(r))
            r->run = 0;
    }
    return any;
}

/* one ARP step at m->now */
static void arp_step(qd_mfx_t *m, int g, int k)
{
    const qd_mfx_slot_t *s = &m->cfg[g].slot[k];
    qd_mfx_srt_t *r = &m->g[g].s[k];
    const int n = held_n(r), mode = s->v[QD_MA_MODE], mlen = clampi(s->v[QD_MA_MLEN], 1, 16),
              tlen = clampi(s->v[QD_MA_TLEN], 1, 16), range = clampi(s->v[QD_MA_RANGE], 1, 8);
    const int64_t steplen = QD_MFX_BAR / qd_mfx_rate_of(s->v[QD_MA_RATE]);
    int mod, tr, ties = 0, idx, i, np = 0, pitch[QD_MFX_NHOLD], pi[QD_MFX_NHOLD];
    if (!n || !r->nseq) {
        r->run = 0;
        return;
    }
    if ((mode == QD_AM_ONCE || mode == QD_AM_1ST) && r->steps >= r->nseq * range) {   /* (once through the pattern
                                                         * over the range: done; counted in steps, so a RESET does not
                                                         * make it endless) */
        r->run = 0;
        return;
    }
    r->steps++;
    if (r->pos >= r->nseq) {                            /* a pass ends: the next octave */
        r->pos = 0;
        if (++r->oct >= range) {
            r->oct = 0;
            r->passes++;
        }
        if (s->v[QD_MA_PAT] == QD_AP_RONC)
            arp_build(m, r, QD_AP_RONC);
    }
    mod = s->lane[0][r->mpos % mlen];
    tr = s->lane[1][r->tpos % tlen];
    for (i = 1; i < mlen && s->lane[0][(r->mpos + i) % mlen] == QD_AL_TIE; i++)
        ties++;
    switch (s->v[QD_MA_PAT]) {
    case QD_AP_RAND: idx = (int)rnd(m, (uint32_t)n); break;
    case QD_AP_ROTH:
        idx = (int)rnd(m, (uint32_t)n);
        if (n > 1 && idx == r->last_idx)
            idx = (idx + 1 + (int)rnd(m, (uint32_t)(n - 1))) % n;
        break;
    case QD_AP_ASP: idx = -1 - (r->pos % n); break;   /* (arrival order: below) */
    default: idx = r->seq[r->pos % r->nseq]; break;
    }
    r->last_idx = idx;
    {
        const qd_mfx_held_t *lo = &r->h[r->order[0]], *hi = &r->h[r->order[n - 1]];
        const qd_mfx_held_t *e = idx < 0 ? &r->h[-1 - idx] : &r->h[r->order[idx]];
        const int on = r->oct * s->v[QD_MA_ODIST];
        const int hp = n == 1 ? hi->n.pitch + 12 : hi->n.pitch;
        switch (mod) {
        case QD_AL_PLAY: pitch[np] = e->n.pitch + on + tr, pi[np++] = (int)(e - r->h); break;
        case QD_AL_REST: case QD_AL_TIE: break;
        case QD_AL_REPEAT:
            for (i = 0; i < r->last_n; i++) {
                int j;
                for (j = 0; j < n && !(r->h[j].n.track == r->last_t[i]); j++)
                    ;
                pitch[np] = r->last_p[i], pi[np++] = j < n ? j : 0;
            }
            break;
        case QD_AL_LO: pitch[np] = lo->n.pitch - 12 + tr, pi[np++] = (int)(lo - r->h); break;
        case QD_AL_HI: pitch[np] = hp + 12 + tr, pi[np++] = (int)(hi - r->h); break;
        case QD_AL_PWR:
            pitch[np] = lo->n.pitch + 12 * r->oct + tr, pi[np++] = (int)(lo - r->h);
            pitch[np] = hp + 12 * r->oct + tr, pi[np++] = (int)(hi - r->h);
            break;
        case QD_AL_CHORD:
            for (i = 0; i < n; i++)
                pitch[np] = r->h[r->order[i]].n.pitch + 12 * r->oct + tr, pi[np++] = r->order[i];
            break;
        default:                                        /* NOTE 1..6: as played */
            if (mod - QD_AL_N1 < n)
                pitch[np] = r->h[mod - QD_AL_N1].n.pitch + on + tr, pi[np++] = mod - QD_AL_N1;
            break;
        }
    }
    {
        const int64_t len = steplen * s->v[QD_MA_GATE] / 100 * (1 + ties);
        if (mod != QD_AL_REPEAT && np)
            r->last_n = 0;
        for (i = 0; i < np; i++) {
            qd_mfx_note_t o;
            if (pitch[i] < 0 || pitch[i] > 127)
                continue;
            o = r->h[pi[i]].n;
            o.pitch = o.prev = (uint8_t)pitch[i];
            o.vel = r->h[0].n.vel;                      /* (every note at the first held note's velocity) */
            o.len = (int32_t)(len < 1 ? 1 : len);
            o.glen = 0;
            o.gen = 1;
            o.rec = 0;
            if (mod != QD_AL_REPEAT && r->last_n < QD_MFX_NHOLD)
                r->last_p[r->last_n] = o.pitch, r->last_t[r->last_n++] = o.track;
            emit(m, g, k, 1, &o);
        }
    }
    r->pos++;
    r->mpos = (r->mpos + 1) % mlen;
    r->tpos = (r->tpos + 1) % tlen;
    if ((s->v[QD_MA_RESET] == QD_AR_MPAT && r->mpos == 0) || (s->v[QD_MA_RESET] == QD_AR_TPAT && r->tpos == 0))
        r->pos = 0, r->oct = 0;
    r->next += steplen;
}

/* ---- REPEAT */
static void rep_hit(qd_mfx_t *m, int g, int k, int i)
{
    const qd_mfx_slot_t *s = &m->cfg[g].slot[k];
    qd_mfx_srt_t *r = &m->g[g].s[k];
    qd_mfx_held_t *h = &r->h[i];
    const int64_t iv = QD_MFX_BAR / qd_mfx_rate_of(s->v[QD_MR_RATE]);
    const int cnt = clampi(s->v[QD_MR_HITS], 1, 64), f = s->v[QD_MR_FADE], mode = s->v[QD_MR_MODE];
    const int kk = h->hits < cnt - 1 ? h->hits : cnt - 1;
    const int a = f < 0 ? 100 : 100 - f, b = f < 0 ? 100 + f : 100;
    const int pct = cnt > 1 ? a + (b - a) * kk / (cnt - 1) : b;
    qd_mfx_note_t o = h->n;
    int64_t len = iv * s->v[QD_MR_GATE] / 100;
    o.prev = o.pitch;
    o.vel = (uint8_t)clampi(h->n.vel * pct / 100, 1, 127);
    o.len = (int32_t)(len < 1 ? 1 : len);
    o.glen = 0;
    o.gen = 1;
    h->hits++;
    h->next += iv;
    if ((mode == QD_AM_1ST || mode == QD_AM_ONCE) && h->hits >= cnt)
        held_del(r, i);
    emit(m, g, k, 1, &o);
}

/* ---- each effect's note in */
static void fx_in(qd_mfx_t *m, int g, int k, int on, const qd_mfx_note_t *nin)
{
    const qd_mfx_slot_t *s = &m->cfg[g].slot[k];
    qd_mfx_srt_t *r = &m->g[g].s[k];
    qd_mfx_note_t n = *nin;
    const int chance = s->v[qd_mfx_chance_id(s->type)];
    int i;
    srt_sync(m, g, k);
    if (on)
        m->g[g].passed[k]++;
    switch (s->type) {
    case QD_MFX_CHANCE:
        if (on && !hit(m, chance))
            return;
        emit(m, g, k, on, &n);
        return;
    case QD_MFX_TRANSPOSE:
        if (on && hit(m, chance)) {
            const int p = n.pitch + s->v[QD_MT_SEMI] + 12 * s->v[QD_MT_OCT];
            if (p < 0 || p > 127)
                return;
            n.pitch = (uint8_t)p;
        }
        emit(m, g, k, on, &n);
        return;
    case QD_MFX_SCALER:
        if (on && hit(m, chance)) {
            int sc = s->v[QD_MZ_SCALE], root = s->v[QD_MZ_ROOT];
            if (s->v[QD_MZ_KEY] && m->io.key)
                m->io.key(m->io.ctx, n.track, &sc, &root);
            if (sc != QD_SCALE_CHROMATIC) {
                const int p = qd_scale_snap(sc, root, n.pitch);
                if (p < 0 || p > 127 || !qd_scale_has(sc, root, p))
                    return;
                n.pitch = (uint8_t)p;
            }
        }
        emit(m, g, k, on, &n);
        return;
    case QD_MFX_HARM:
        if (!on || !hit(m, chance)) {
            emit(m, g, k, on, &n);
            return;
        }
        {
            static const uint8_t ID[6] = {QD_MH_N2, QD_MH_N3, QD_MH_N4, QD_MH_N5, QD_MH_N6, QD_MH_N7};
            int done[6], nd = 0, j;
            const int base = n.pitch;
            if (s->v[QD_MH_ORIG])
                emit(m, g, k, 1, &n);
            for (i = 0; i < 6; i++) {
                const int off = s->v[ID[i]];
                int p, dup = 0;
                if (!off)
                    continue;
                p = clampi(base + off, 0, 127);
                for (j = 0; j < nd; j++)
                    dup |= done[j] == p;
                if (dup)
                    continue;
                done[nd++] = p;
                n.pitch = (uint8_t)p;
                emit(m, g, k, 1, &n);
            }
        }
        return;
    case QD_MFX_CHORD:
        if (!on || !hit(m, chance)) {
            emit(m, g, k, on, &n);
            return;
        }
        {
            const qd_shape_t *sh = &QD_SHAPE[clampi(s->v[QD_MK_SHAPE], 0, QD_NSHAPE - 1)];
            const int per = sh->n + 1, cnt = clampi(s->v[QD_MK_NOTES], 1, 6);
            int p[6], j, q, sp = s->v[QD_MK_SPREAD], v = s->v[QD_MK_VOICING];
            for (i = 0; i < cnt; i++)
                p[i] = n.pitch + (i % per ? sh->off[i % per - 1] : 0) + 12 * (i / per);
#define SORT6() for (i = 1; i < cnt; i++) for (j = i; j > 0 && p[j] < p[j - 1]; j--) { const int x = p[j]; p[j] = p[j - 1], p[j - 1] = x; }
            SORT6();
            for (q = 0; q < clampi(s->v[QD_MK_INV], 0, 5); q++) {   /* INVERT: the lowest up an octave, each */
                p[0] += 12;
                SORT6();
            }
            if (cnt >= 2 && (v == QD_CV_DROP2 || v == QD_CV_OPEN))
                p[cnt - 2] -= 12;
            if (cnt >= 3 && v == QD_CV_DROP3)
                p[cnt - 3] -= 12;
            if (cnt >= 4 && v == QD_CV_OPEN)
                p[cnt - 4] -= 12;
            SORT6();
            if (sp)                                     /* SPREAD: every second note an octave (or two) away */
                for (i = 1; i < cnt; i += 2)
                    p[i] += 12 * sp;
#undef SORT6
            for (i = 0; i < cnt; i++) {
                int dup = 0;
                for (j = 0; j < i; j++)
                    dup |= p[j] == p[i];
                if (dup || p[i] < 0 || p[i] > 127)
                    continue;
                n.pitch = (uint8_t)p[i];
                emit(m, g, k, 1, &n);
            }
        }
        return;
    case QD_MFX_MONO: {
        const int t = n.track & 7;
        if (on && hit(m, chance)) {
            if (r->mono_on[t] && (r->mono_prev[t] != n.prev || r->mono_gen[t] != n.gen)) {
                qd_mfx_note_t o = n;
                o.prev = o.pitch = r->mono_prev[t];
                o.gen = r->mono_gen[t];
                emit(m, g, k, 0, &o);
            }
            r->mono_on[t] = 1, r->mono_prev[t] = n.prev, r->mono_gen[t] = n.gen;
        } else if (!on && r->mono_on[t] && r->mono_prev[t] == n.prev && r->mono_gen[t] == n.gen) {
            r->mono_on[t] = 0;
        }
        emit(m, g, k, on, &n);
        return;
    }
    case QD_MFX_SELECTOR: {
        const int len = clampi(s->v[QD_ML_LEN], 0, 7);
        const qd_mfx_group_t *G = &m->cfg[g];
        if (!on) {                                      /* the OFF to every slot of the block, and past it */
            for (i = k + 1; i <= k + len && i < QD_MFX_NSLOT; i++)
                if (sactive(&G->slot[i]))
                    fx_in(m, g, i, 0, &n);
            deliver(m, g, k + len + 1, 0, &n);
            return;
        }
        if (!len) {
            deliver(m, g, k + 1, 1, &n);
            return;
        }
        if (!hit(m, chance)) {
            deliver(m, g, k + len + 1, 1, &n);
            return;
        }
        if (s->v[QD_ML_MODE] == QD_SM_UP)               /* (a ring walked: up from the first, down from the last) */
            r->cursor %= len, i = k + 1 + r->cursor, r->cursor = (r->cursor + 1) % len;
        else if (s->v[QD_ML_MODE] == QD_SM_DOWN)
            r->cursor = (r->cursor % len + len - 1) % len, i = k + 1 + r->cursor;
        else
            i = k + 1 + (int)rnd(m, (uint32_t)len);
        if (i >= QD_MFX_NSLOT)
            out_note(m, g, 1, &n);
        else if (!sactive(&G->slot[i]))
            deliver(m, g, k + len + 1, 1, &n);
        else
            fx_in(m, g, i, 1, &n);
        return;
    }
    case QD_MFX_RANDOM:
        if (!on) {
            int32_t e = 0;
            for (i = 0; i < QD_MFX_NHOLD; i++)          /* still waiting to start: they never will */
                if (r->h[i].used && r->h[i].on && r->h[i].n.track == n.track && r->h[i].n.prev == n.prev &&
                    r->h[i].n.gen == n.gen)
                    r->h[i].used = 0;
            for (i = 0; i < QD_MFX_NHOLD; i++)          /* LENGTH: its end later (the latest of its notes') */
                if (r->ext[i] && r->ext_t[i] == n.track && r->ext_p[i] == n.prev) {
                    e = r->ext[i] > e ? r->ext[i] : e;
                    r->ext[i] = 0;
                }
            if (e > 0) {
                for (i = 0; i < QD_MFX_NHOLD && r->h[i].used; i++)
                    ;
                if (i < QD_MFX_NHOLD) {
                    memset(&r->h[i], 0, sizeof r->h[i]);
                    r->h[i].used = 1, r->h[i].on = 0, r->h[i].n = n, r->h[i].n.rec = 0, r->h[i].next = m->now + e;
                    return;
                }
            }
            emit(m, g, k, 0, &n);
            return;
        }
        if (!hit(m, chance)) {
            emit(m, g, k, 1, &n);
            return;
        }
        n.pitch = (uint8_t)clampi(n.pitch + rpm(m, s->v[QD_MD_PITCH]) + 12 * rpm(m, s->v[QD_MD_OCT]), 0, 127);
        n.vel = (uint8_t)clampi(n.vel + rpm(m, s->v[QD_MD_VEL]), 1, 127);
        if (s->v[QD_MD_LEN] > 0) {
            const int32_t add = (int32_t)rnd(m, (uint32_t)s->v[QD_MD_LEN] + 1u);
            if (n.len > 0) {
                n.len += (int32_t)((int64_t)n.len * add / 100);
            } else if (n.glen > 0 && !n.live) {
                const int32_t e = (int32_t)((int64_t)n.glen * add / 100);
                for (i = 0; e > 0 && i < QD_MFX_NHOLD; i++)
                    if (!r->ext[i]) {
                        r->ext[i] = e, r->ext_t[i] = n.track, r->ext_p[i] = n.prev;
                        break;
                    }
            }
        }
        if (s->v[QD_MD_DELAY] > 0) {
            const int64_t span = QD_MFX_BAR / qd_mfx_rate_of(QD_MFX_NRATE - s->v[QD_MD_DELAY]);
            const int64_t d = span * (int64_t)rnd(m, 1000) / 1000;
            for (i = 0; d > 0 && i < QD_MFX_NHOLD && r->h[i].used; i++)
                ;
            if (d > 0 && i < QD_MFX_NHOLD) {
                memset(&r->h[i], 0, sizeof r->h[i]);
                r->h[i].used = 1, r->h[i].on = 1, r->h[i].n = n, r->h[i].n.rec = 0, r->h[i].next = m->now + d;
                return;
            }
        }
        emit(m, g, k, 1, &n);
        return;
    case QD_MFX_ARP:
        if (s->v[QD_MA_MODE] == QD_AM_OFF) {
            emit(m, g, k, on, &n);
            return;
        }
        if (on) {
            if (!hit(m, chance))
                emit(m, g, k, 1, &n);
            else
                arp_note_on(m, g, k, &n);
        } else {                                        /* (released here; on too: a sibling let by may sound) */
            held_release(m, g, k, n.track, n.prev, n.gen);
            emit(m, g, k, 0, &n);
        }
        return;
    case QD_MFX_REPEAT:
        if (s->v[QD_MR_MODE] == QD_AM_OFF) {
            emit(m, g, k, on, &n);
            return;
        }
        if (!on) {
            held_release(m, g, k, n.track, n.prev, n.gen);
            emit(m, g, k, 0, &n);
            return;
        }
        if (!hit(m, chance)) {
            emit(m, g, k, 1, &n);
            return;
        }
        if (s->v[QD_MR_MODE] == QD_AM_HOLD && r->down <= 0) {   /* (the latched set replaced) */
            memset(r->h, 0, sizeof r->h);
            r->down = 0;
        }
        if (s->v[QD_MR_MODE] == QD_AM_1ST)
            for (i = held_n(r) - 1; i >= 0; i--)        /* (the same note again: it starts over) */
                if (r->h[i].n.track == n.track && r->h[i].n.prev == n.prev && r->h[i].n.pitch == n.pitch &&
                    r->h[i].n.gen == n.gen) {
                    if (!r->h[i].released && r->down > 0)
                        r->down--;
                    held_del(r, i);
                }
        i = held_add(m, r, &n);
        if (i < 0)
            return;
        r->h[i].next = quant(m, s->v[QD_MR_QUANT]);
        r->down++;
        return;
    default:
        emit(m, g, k, on, &n);
        return;
    }
}

void qd_mfx_note(qd_mfx_t *m, int g, int on, const qd_mfx_note_t *n, int64_t tick)
{
    if (g < 1 || g > QD_MFX_NGROUP || !m->cfg)
        return;
    sync_all(m);
    m->now = tick;
    m->g[g - 1].idle = 0;
    deliver(m, g - 1, 0, on, n);
}

/* ---- time: everything due before tick, in time order (at a tick: the timed notes' ends, the timed inputs' ends,
 * then each slot's steps, hits and waits in slot order) */
static void mark_idle(qd_mfx_t *m)                     /* the groups with nothing timed: skipped from now */
{
    int g, k, i;
    for (g = 0; g < QD_MFX_NGROUP; g++) {
        qd_mfx_grt_t *R = &m->g[g];
        int busy = 0;
        if (R->idle)
            continue;
        for (i = 0; i < QD_MFX_NPEND && !busy; i++)
            busy = R->pend[i].used;
        for (k = 0; k < QD_MFX_NSLOT && !busy; k++)
            for (i = 0; i < QD_MFX_NHOLD && !busy; i++)
                busy = R->s[k].h[i].used || R->s[k].run;
        R->idle = (uint8_t)!busy;
    }
}

static int64_t next_due(const qd_mfx_t *m)
{
    int64_t t = INT64_MAX;
    int g, k, i;
    for (g = 0; g < QD_MFX_NGROUP; g++) {
        const qd_mfx_grt_t *R = &m->g[g];
        if (R->idle)
            continue;
        for (i = 0; i < QD_MFX_NPEND; i++)
            if (R->pend[i].used && R->pend[i].tick < t)
                t = R->pend[i].tick;
        for (k = 0; k < QD_MFX_NSLOT; k++) {
            const qd_mfx_srt_t *r = &R->s[k];
            const int ty = r->type & 0x7F;
            if (ty == QD_MFX_ARP && r->run && r->next < t)
                t = r->next;
            if (ty == QD_MFX_ARP || ty == QD_MFX_REPEAT || ty == QD_MFX_RANDOM)
                for (i = 0; i < QD_MFX_NHOLD && (ty == QD_MFX_RANDOM || r->h[i].used); i++) {
                    if (!r->h[i].used)
                        continue;
                    if (r->h[i].until && !r->h[i].released && r->h[i].until < t)
                        t = r->h[i].until;
                    if ((ty == QD_MFX_REPEAT || ty == QD_MFX_RANDOM) && r->h[i].next < t)
                        t = r->h[i].next;
                }
        }
    }
    return t;
}

void qd_mfx_advance(qd_mfx_t *m, int64_t tick)
{
    int guard = 0, g0, k0;
    if (!m->cfg)
        return;
    for (g0 = 0; g0 < QD_MFX_NGROUP; g0++)              /* (an idle group's state is set up when a note comes) */
        if (!m->g[g0].idle)
            for (k0 = 0; k0 < QD_MFX_NSLOT; k0++)
                srt_sync(m, g0, k0);
    while (guard++ < 4096) {
        const int64_t t = next_due(m);
        int g, k, i;
        if (t >= tick)
            break;
        m->now = t;
        for (g = 0; g < QD_MFX_NGROUP; g++) {           /* the timed notes' ends */
            qd_mfx_grt_t *R = &m->g[g];
            for (i = 0; i < QD_MFX_NPEND; i++)
                if (R->pend[i].used && R->pend[i].tick <= t) {
                    R->pend[i].used = 0;
                    if (m->io.off)
                        m->io.off(m->io.ctx, g, R->pend[i].track, R->pend[i].pitch);
                }
        }
        for (g = 0; g < QD_MFX_NGROUP; g++)
            for (k = 0; k < QD_MFX_NSLOT; k++) {
                qd_mfx_srt_t *r = &m->g[g].s[k];
                const int ty = r->type & 0x7F;
                if (ty == QD_MFX_ARP || ty == QD_MFX_REPEAT)   /* the timed inputs' ends: as released */
                    for (i = held_n(r) - 1; i >= 0; i--)
                        if (r->h[i].until && !r->h[i].released && r->h[i].until <= t)
                            held_release(m, g, k, r->h[i].n.track, r->h[i].n.prev, r->h[i].n.gen);
                if (ty == QD_MFX_ARP && r->run && r->next <= t)
                    arp_step(m, g, k);
                if (ty == QD_MFX_REPEAT)
                    for (i = 0; i < held_n(r); i++)
                        if (r->h[i].next <= t) {
                            const int n0 = held_n(r);
                            rep_hit(m, g, k, i);
                            if (held_n(r) < n0)
                                i--;
                        }
                if (ty == QD_MFX_RANDOM)
                    for (i = 0; i < QD_MFX_NHOLD; i++)
                        if (r->h[i].used && r->h[i].next <= t) {
                            qd_mfx_note_t n = r->h[i].n;
                            const int on = r->h[i].on;
                            r->h[i].used = 0;
                            emit(m, g, k, on, &n);
                        }
            }
    }
    m->now = tick;
    mark_idle(m);
}

/* ---- flushes */
static void group_release(qd_mfx_t *m, int g, int t, int live)
{
    qd_mfx_grt_t *R = &m->g[g];
    int i;
    for (i = 0; i < QD_MFX_NOUT; i++)
        if (R->out[i].used && (t < 0 || R->out[i].track == t) && (live < 0 || R->out[i].live == live)) {
            R->out[i].used = 0;
            if (m->io.off)
                m->io.off(m->io.ctx, g, R->out[i].track, R->out[i].pitch);
        }
    for (i = 0; i < QD_MFX_NPEND; i++)
        if (R->pend[i].used && (t < 0 || R->pend[i].track == t) && (live < 0 || R->pend[i].live == live)) {
            R->pend[i].used = 0;
            if (m->io.off)
                m->io.off(m->io.ctx, g, R->pend[i].track, R->pend[i].pitch);
        }
}

void qd_mfx_flush_track(qd_mfx_t *m, int t, int live)
{
    int g, k, i;
    if (!m->cfg)
        return;
    for (g = 0; g < QD_MFX_NGROUP; g++) {
        for (k = 0; k < QD_MFX_NSLOT; k++) {
            qd_mfx_srt_t *r = &m->g[g].s[k];
            const int ty = r->type & 0x7F;
            if (ty == QD_MFX_RANDOM) {
                for (i = 0; i < QD_MFX_NHOLD; i++)
                    if (r->h[i].used && r->h[i].n.track == t && (live < 0 || r->h[i].n.live == live))
                        r->h[i].used = 0;
            } else if (ty == QD_MFX_ARP || ty == QD_MFX_REPEAT) {
                for (i = held_n(r) - 1; i >= 0; i--)
                    if (r->h[i].n.track == t && (live < 0 || r->h[i].n.live == live)) {
                        if (!r->h[i].released && r->down > 0)
                            r->down--;
                        held_del(r, i);
                    }
                if (ty == QD_MFX_ARP) {
                    arp_build(m, r, m->cfg[g].slot[k].v[QD_MA_PAT]);
                    if (!held_n(r))
                        r->run = 0;
                }
            }
            if (t >= 0 && t < 8)
                r->mono_on[t] = 0;
        }
        group_release(m, g, t, live);
    }
}

void qd_mfx_reset(qd_mfx_t *m)
{
    int g, k;
    if (!m->cfg)
        return;
    for (g = 0; g < QD_MFX_NGROUP; g++) {
        for (k = 0; k < QD_MFX_NSLOT; k++) {
            qd_mfx_srt_t *r = &m->g[g].s[k];
            const uint8_t ty = r->type, by = r->bypass;
            memset(r, 0, sizeof *r);
            r->type = ty, r->bypass = by;
        }
        group_release(m, g, -1, -1);
    }
}

int qd_mfx_busy(const qd_mfx_t *m, int g)
{
    const qd_mfx_grt_t *R;
    int i, k;
    if (g < 1 || g > QD_MFX_NGROUP)
        return 0;
    R = &m->g[g - 1];
    for (i = 0; i < QD_MFX_NOUT; i++)
        if (R->out[i].used)
            return 1;
    for (i = 0; i < QD_MFX_NPEND; i++)
        if (R->pend[i].used)
            return 1;
    for (k = 0; k < QD_MFX_NSLOT; k++)
        for (i = 0; i < QD_MFX_NHOLD; i++)
            if (R->s[k].h[i].used)
                return 1;
    return 0;
}

int qd_mfx_arp_toggle(qd_mfx_t *m, int g, int *on)
{
    qd_mfx_group_t *G;
    int k;
    if (g < 1 || g > QD_MFX_NGROUP || !m->cfg)
        return -1;
    G = &m->cfg[g - 1];
    k = qd_mfx_arp_slot(G);
    if (k < 0) {                                        /* none: one in the first empty slot, on */
        for (k = 0; k < QD_MFX_NSLOT && G->slot[k].type != QD_MFX_NONE; k++)
            ;
        if (k >= QD_MFX_NSLOT)
            return -1;
        qd_mfx_slot_init(&G->slot[k], QD_MFX_ARP);
        *on = 1;
        return k;
    }
    {
        qd_mfx_slot_t *s = &G->slot[k];
        if (s->v[QD_MA_MODE] != QD_AM_OFF && !s->bypass) {
            s->v[QD_MA_LAST] = s->v[QD_MA_MODE];
            s->v[QD_MA_MODE] = QD_AM_OFF;
            *on = 0;
        } else {
            if (s->v[QD_MA_MODE] == QD_AM_OFF)
                s->v[QD_MA_MODE] = s->v[QD_MA_LAST] >= QD_AM_ON && s->v[QD_MA_LAST] < QD_NAM ? s->v[QD_MA_LAST] : QD_AM_ON;
            s->bypass = 0;
            *on = 1;
        }
    }
    return k;
}

int qd_mfx_arp_playing(const qd_mfx_t *m, int g)
{
    int k;
    if (g < 1 || g > QD_MFX_NGROUP || !m->cfg)
        return 0;
    for (k = 0; k < QD_MFX_NSLOT; k++)
        if ((m->g[g - 1].s[k].type & 0x7F) == QD_MFX_ARP && m->g[g - 1].s[k].run)
            return 1;
    return 0;
}
