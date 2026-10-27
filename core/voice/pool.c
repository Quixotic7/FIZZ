/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The voice pool (pool.h; PLAN.md §2). */
#include <string.h>

#include "pool.h"

static int sounding(const qd_pool_voice_t *v) { return v->q.active && !v->q.kill; }
static int is_free(const qd_pool_voice_t *v) { return !v->q.active && !v->pend; }

void qd_pool_init(qd_pool_t *p, int nvoice)
{
    int i;
    memset(p, 0, sizeof *p);
    p->nvoice = nvoice < 1 ? 1 : nvoice > QD_POOL_MAX ? QD_POOL_MAX : nvoice;
    for (i = 0; i < QD_POOL_MAX; i++) {
        qd_quad_voice_init(&p->v[i].q, (uint32_t)i);
        p->v[i].pend_gfrom = -1;
    }
    for (i = 0; i < QD_POOL_NTRACK; i++)
        p->limit[i] = QD_POOL_LIMIT_MAX;
    p->dip_on = 1;
}

void qd_pool_set_track(qd_pool_t *p, int track, int limit, int reserve)
{
    if (track < 0 || track >= QD_POOL_NTRACK)
        return;
    p->limit[track] = (uint8_t)(limit < 1 ? 1 : limit > QD_POOL_LIMIT_MAX ? QD_POOL_LIMIT_MAX : limit);
    p->reserve[track] = (uint8_t)(reserve < 0 ? 0 : reserve > QD_POOL_LIMIT_MAX ? QD_POOL_LIMIT_MAX : reserve);
}

void qd_pool_set_pan(qd_pool_t *p, int track, int pan)
{
    if (track >= 0 && track < QD_POOL_NTRACK)
        p->pan[track] = (int8_t)(pan < -64 ? -64 : pan > 63 ? 63 : pan);
}

int qd_pool_count(const qd_pool_t *p, int track)
{
    int i, n = 0;
    for (i = 0; i < p->nvoice; i++) {
        const qd_pool_voice_t *v = &p->v[i];
        n += (sounding(v) && v->track == track) || (v->pend && v->pend_track == track);
    }
    return n;
}

int qd_pool_sounding(const qd_pool_t *p, int i) { return i >= 0 && i < p->nvoice && sounding(&p->v[i]); }
int qd_pool_used(const qd_pool_t *p, int i) { return i >= 0 && i < p->nvoice && !is_free(&p->v[i]); }

int qd_pool_in_use(const qd_pool_t *p)
{
    int i, n = 0;
    for (i = 0; i < p->nvoice; i++)
        n += !is_free(&p->v[i]);
    return n;
}

static void set_at(qd_vattr_t *d, const qd_vattr_t *at)
{
    if (at)
        *d = *at;
    else
        memset(d, 0, sizeof *d);
}

/* a note into slot i now (a free voice, or one that has not sounded: from silence) */
static void start(qd_pool_t *p, int i, int track, qd_quad_owner_t *o, int note, int vel, const int16_t *patch,
                  const qd_vattr_t *at, const qd_uni_t *u)
{
    qd_pool_voice_t *v = &p->v[i];
    v->q.active = 0;                                    /* (a fresh start: nothing of the old note) */
    v->q.dip = 0;
    v->dip_n = 0;                                       /* (routing changes waiting for a dip: the new patch wins) */
    if (at != &v->at)
        set_at(&v->at, at);
    v->track = (uint8_t)track;
    qd_quad_voice_start(&v->q, patch, o, note, vel);
    v->u = *u;                                          /* (2026-10-26: its unison place) */
    v->q.udet = u->det;
    if (u->panon) {
        v->at.mixm |= (uint8_t)(1u << QD_VMIX_PAN);
        v->at.mixv[QD_VMIX_PAN] = u->pan;
    }
    if (v->pend_gfrom >= 0) {                           /* (E1: the glide settled while it waited) */
        qd_quad_voice_glide(&v->q, v->pend_gfrom);
        v->pend_gfrom = -1;
    }
    v->age = ++p->clock;
    v->serial++;
    p->starts++;
}

/* the note takes slot i, which sounds: faded over the next block, the note waits for the one after (pend); a voice
 * that has not rendered a block since its start is silent: restarted in place at once */
static int take(qd_pool_t *p, int i, int track, qd_quad_owner_t *o, int note, int vel, const int16_t *patch,
                const qd_vattr_t *at)
{
    qd_pool_voice_t *v = &p->v[i];
    if (!v->q.env_out) {
        start(p, i, track, o, note, vel, patch, at, &p->nu);
        return i;
    }
    qd_quad_voice_kill(&v->q);
    v->pend = 1;
    v->pend_track = (uint8_t)track;
    v->pend_owner = o;
    v->pend_note = (uint8_t)note;
    v->pend_vel = (uint8_t)vel;
    v->pend_off = 0;
    v->pend_gfrom = -1;
    memcpy(v->pend_patch, patch, sizeof v->pend_patch);
    set_at(&v->pend_at, at);
    v->pend_u = p->nu;
    return i;
}

/* track's own voice to take: a released one, the quietest; else the oldest held */
static int victim(const qd_pool_t *p, int track)
{
    int i, rel = -1, held = -1;
    int32_t rl = 0;
    for (i = 0; i < p->nvoice; i++) {
        const qd_pool_voice_t *v = &p->v[i];
        if (!sounding(v) || v->track != track)
            continue;
        if (!v->q.gate) {
            int32_t l = qd_quad_voice_level(&v->q);
            if (rel < 0 || l < rl) {
                rel = i;
                rl = l;
            }
        } else if (held < 0 || v->age < p->v[held].age) {
            held = i;
        }
    }
    return rel >= 0 ? rel : held;
}

int qd_pool_note_on(qd_pool_t *p, int track, qd_quad_owner_t *o, int note, int vel, const int16_t *patch)
{
    return qd_pool_note_on_at(p, track, o, note, vel, patch, 0);
}

/* a voice's PRIORITY (the VOICE page's: 0 LOW .. 2 HIGH) */
static int vprio(const qd_pool_voice_t *v) { return v->q.patch[QD_QP_VPRIO]; }

/* one voice for the note (its unison place p->nu), the track's own at lim */
static int alloc1(qd_pool_t *p, int track, qd_quad_owner_t *o, int note, int vel, const int16_t *patch,
                  const qd_vattr_t *at, int lim)
{
    int i, best = -1, t;
    /* 0. the same note again on this track (a unison note: its voices were released, fresh ones below) */
    for (i = 0; i < p->nvoice && !p->nu.grp; i++) {
        qd_pool_voice_t *v = &p->v[i];
        if (v->pend && v->pend_track == track && v->pend_note == note) {   /* still waiting: the newer strike */
            v->pend_vel = (uint8_t)vel;
            v->pend_owner = o;
            v->pend_off = 0;
            memcpy(v->pend_patch, patch, sizeof v->pend_patch);
            set_at(&v->pend_at, at);
            return i;
        }
        if (sounding(v) && v->track == track && v->q.note == note && (best < 0 || v->age > p->v[best].age))
            best = i;
    }
    if (best >= 0) {
        qd_pool_voice_t *v = &p->v[best];
        if (!memcmp(v->q.patch, patch, sizeof v->q.patch) && !v->dip_n) {   /* the same sound: struck again in
                                                         * place (a voice mid-dip has another sound coming) */
            qd_quad_voice_start(&v->q, patch, o, note, vel);
            set_at(&v->at, at);
            v->u = p->nu, v->q.udet = 0;
            v->age = ++p->clock;
            p->restrikes++;
            return best;
        }
        qd_quad_voice_release(&v->q);                    /* another sound: this one releases, a fresh voice below */
    }
    /* 1. the track at its limit: its own voice */
    if (qd_pool_count(p, track) >= lim) {
        best = victim(p, track);
        if (best >= 0) {
            p->steals_own++;
            return take(p, best, track, o, note, vel, patch, at);
        }
        for (i = 0; i < p->nvoice; i++)                 /* (its limit is all waiting notes: the newest takes the place
                                                         * of the first) */
            if (p->v[i].pend && p->v[i].pend_track == track) {
                qd_pool_voice_t *v = &p->v[i];
                v->pend_owner = o;
                v->pend_note = (uint8_t)note;
                v->pend_vel = (uint8_t)vel;
                v->pend_off = 0;
                memcpy(v->pend_patch, patch, sizeof v->pend_patch);
                set_at(&v->pend_at, at);
                v->pend_u = p->nu;
                p->steals_own++;
                return i;
            }
    }
    /* 2. a free voice (P2.6, halves: in the half with fewer voices in use first; a tie: the first half) */
    {
        int h0 = 0, h1 = p->nvoice, k;
        if (p->halves && p->nvoice >= 2) {
            const int m = p->nvoice / 2;
            int n0 = 0, n1 = 0;
            for (i = 0; i < p->nvoice; i++)
                if (!is_free(&p->v[i]))
                    *(i < m ? &n0 : &n1) += 1;
            h0 = n1 < n0 ? m : 0;
        }
        for (k = 0; k < p->nvoice; k++) {
            i = (h0 + k) % h1;
            if (is_free(&p->v[i])) {
                start(p, i, track, o, note, vel, patch, at, &p->nu);
                p->frees++;
                return i;
            }
        }
    }
    /* 3. across the tracks: a released voice (the quietest), else the oldest held note of the track furthest over
     * its reserve; a track at or under its reserve is not robbed. 2026-10-26: only the lowest PRIORITY among those
     * is robbed; when every one is higher than the note's own, the note is dropped */
    {
        int over[QD_POOL_NTRACK], top = 0, bt = -1, minp = 3, maxp = -1, np = patch[QD_QP_VPRIO];
        for (t = 0; t < QD_POOL_NTRACK; t++) {
            over[t] = qd_pool_count(p, t) - p->reserve[t];
            if (over[t] > top) {
                top = over[t];
                bt = t;
            }
        }
        for (i = 0; i < p->nvoice; i++)
            if (sounding(&p->v[i]) && over[p->v[i].track] > 0) {
                minp = vprio(&p->v[i]) < minp ? vprio(&p->v[i]) : minp;
                maxp = vprio(&p->v[i]) > maxp ? vprio(&p->v[i]) : maxp;
            }
        if (minp < 3 && minp > np) {
            p->drops++, p->prio_drops++;
            return -1;
        }
        best = -1;
        for (i = 0; i < p->nvoice; i++) {               /* a released voice of any track over its reserve */
            const qd_pool_voice_t *v = &p->v[i];
            if (sounding(v) && !v->q.gate && over[v->track] > 0 && vprio(v) == minp &&
                (best < 0 || qd_quad_voice_level(&v->q) < qd_quad_voice_level(&p->v[best].q)))
                best = i;
        }
        if (best < 0 && bt >= 0) {                      /* the furthest over (a tie: the one whose held note is oldest) */
            uint32_t oldest = 0xFFFFFFFFu;
            for (t = 0; t < QD_POOL_NTRACK; t++) {
                if (over[t] != top)
                    continue;
                for (i = 0; i < p->nvoice; i++) {
                    const qd_pool_voice_t *v = &p->v[i];
                    if (sounding(v) && v->q.gate && v->track == t && vprio(v) == minp && v->age < oldest) {
                        oldest = v->age;
                        best = i;
                    }
                }
            }
        }
        if (best < 0 && minp < maxp) {                  /* (PRIORITIES mixed, the furthest over has none of the lowest:
                                                         * the oldest held one of any track over its reserve; all the
                                                         * same: as before, dropped) */
            uint32_t oldest = 0xFFFFFFFFu;
            for (i = 0; i < p->nvoice; i++) {
                const qd_pool_voice_t *v = &p->v[i];
                if (sounding(v) && v->q.gate && over[v->track] > 0 && vprio(v) == minp && v->age < oldest) {
                    oldest = v->age;
                    best = i;
                }
            }
        }
        if (best < 0) {
            p->drops++;
            return -1;
        }
        p->steals_cross++;
        if (p->v[best].q.gate && p->v[best].track < QD_POOL_NTRACK)
            p->stolen[p->v[best].track]++;              /* P2.11: a held note taken (a released one recycled: not) */
        return take(p, best, track, o, note, vel, patch, at);
    }
}

/* 2026-10-26, the VOICE page. A note: MONO's LEGATO with a key of the track down moves its voices (no new attack);
 * else n = UNISON voices (MONO: the track's voices are n, else its LIMIT caps n), each at its place s = -1 .. 1 across
 * the unison (the middle ones first: a LIMIT under UNISON drops the outer ones), detuned by s x DETUNE (+-50 cents at
 * 127) and panned by s x SPREAD around the track's pan (or the trig's pan lock) */
int qd_pool_note_on_at(qd_pool_t *p, int track, qd_quad_owner_t *o, int note, int vel, const int16_t *patch,
                       const qd_vattr_t *at)
{
    int i, j, k, n, lead = -1, nn, ord[8];
    const int mono = patch ? patch[QD_QP_VMODE] == 1 : 0;
    if (track < 0 || track >= QD_POOL_NTRACK || !o || !patch)
        return -1;
    nn = patch[QD_QP_UNI] < 1 ? 1 : patch[QD_QP_UNI] > 8 ? 8 : patch[QD_QP_UNI];
    if (mono && patch[QD_QP_PORT] == QD_PORT_LEGATO && qd_pool_gated(p, track)) {   /* MONO's LEGATO: moved */
        for (i = 0; i < p->nvoice; i++)
            if (sounding(&p->v[i]) && p->v[i].track == track && p->v[i].q.gate) {
                qd_quad_voice_legato(&p->v[i].q, note, vel);
                p->v[i].age = ++p->clock;
                p->legatos++;
                if (lead < 0)
                    lead = i;
            }
        if (lead >= 0)
            return lead;
    }
    memset(&p->nu, 0, sizeof p->nu);
    if (nn == 1)
        return alloc1(p, track, o, note, vel, patch, at, mono ? 1 : p->limit[track]);
    n = mono ? nn : (nn < p->limit[track] ? nn : p->limit[track]);
    for (i = 0; i < p->nvoice; i++)                     /* (the same note again: its voices released, fresh ones) */
        if (sounding(&p->v[i]) && p->v[i].track == track && p->v[i].q.note == note && p->v[i].q.gate)
            qd_quad_voice_release(&p->v[i].q);
    for (j = 0; j < nn; j++)                            /* the places, the middle first */
        ord[j] = j;
    for (j = 1; j < nn; j++)
        for (k = j; k > 0; k--) {
            const int a = 2 * ord[k] - (nn - 1), b = 2 * ord[k - 1] - (nn - 1);
            if ((a < 0 ? -a : a) >= (b < 0 ? -b : b))
                break;
            i = ord[k], ord[k] = ord[k - 1], ord[k - 1] = i;
        }
    if (!++p->grpc)
        p->grpc = 1;
    p->unis++;
    {
        const int det = patch[QD_QP_UDET] < 0 ? 0 : patch[QD_QP_UDET] > 127 ? 127 : patch[QD_QP_UDET];
        const int spr = patch[QD_QP_USPR] < 0 ? 0 : patch[QD_QP_USPR] > 127 ? 127 : patch[QD_QP_USPR];
        const int base = at && (at->mixm & (1u << QD_VMIX_PAN)) ? at->mixv[QD_VMIX_PAN] : p->pan[track];
        for (j = 0; j < n; j++) {
            const int32_t sn = 2 * ord[j] - (nn - 1), sd = nn - 1;   /* s = sn / sd */
            int32_t pan = base + (sn * spr * 64) / (sd * 127);
            p->nu.grp = p->grpc;
            /* 2^(cents / 1200) as 1 + cents x ln 2 / 1200 (within 0.7 cent at 50): Q16 37.856 a cent */
            p->nu.det = (uint32_t)(65536 + (sn * det * 50 * 37856) / (sd * 127 * 1000));
            p->nu.pan = (int8_t)(pan < -64 ? -64 : pan > 63 ? 63 : pan);
            p->nu.panon = spr != 0 || (at && (at->mixm & (1u << QD_VMIX_PAN)));
            i = alloc1(p, track, o, note, vel, patch, at, n);
            if (lead < 0)
                lead = i;
        }
    }
    memset(&p->nu, 0, sizeof p->nu);
    return lead;
}

/* the other voices of voice i's unison note (its group: the same track and grp; none: -1 at once): k from -1 */
static int sib_next(const qd_pool_t *p, int i, int k)
{
    const qd_pool_voice_t *v = &p->v[i];
    const uint16_t g = v->pend ? v->pend_u.grp : v->u.grp;
    if (!g)
        return -1;
    for (k++; k < p->nvoice; k++) {
        const qd_pool_voice_t *w = &p->v[k];
        if (k != i && ((w->q.active && w->track == v->track && w->u.grp == g) ||
                       (w->pend && w->pend_track == v->track && w->pend_u.grp == g)))
            return k;
    }
    return -1;
}

void qd_pool_note_off(qd_pool_t *p, int track, int note)
{
    int i;
    for (i = 0; i < p->nvoice; i++) {
        qd_pool_voice_t *v = &p->v[i];
        if (sounding(v) && v->track == track && v->q.note == note && v->q.gate)
            qd_quad_voice_release(&v->q);
        if (v->pend && v->pend_track == track && v->pend_note == note)
            v->pend_off = 1;                            /* (it still starts, and is released at once: a short tap
                                                         * sounds) */
    }
}

void qd_pool_release_all(qd_pool_t *p, int track)
{
    int i;
    for (i = 0; i < p->nvoice; i++) {
        qd_pool_voice_t *v = &p->v[i];
        if (sounding(v) && (track < 0 || v->track == track))
            qd_quad_voice_release(&v->q);
        if (v->pend && (track < 0 || v->pend_track == track))
            v->pend_off = 1;
    }
}

void qd_pool_kill_all(qd_pool_t *p)
{
    int i;
    for (i = 0; i < p->nvoice; i++) {
        p->v[i].pend = 0;
        qd_quad_voice_kill(&p->v[i].q);
    }
}

static void glide_settle(qd_pool_t *p, qd_quad_owner_t *o);
void qd_pool_block_begin(qd_pool_t *p)
{
    int i;
    for (i = 0; i < p->nvoice; i++) {                   /* (E1: the chords started since: their glides first) */
        qd_pool_voice_t *v = &p->v[i];
        if (v->pend && v->pend_owner && v->pend_owner->cc_n)
            glide_settle(p, v->pend_owner);
        if (v->q.active && v->q.owner && v->q.owner->cc_n)
            glide_settle(p, v->q.owner);
    }
    for (i = 0; i < p->nvoice; i++) {
        qd_pool_voice_t *v = &p->v[i];
        if (v->pend && !v->q.active) {                  /* its fade has been rendered: the note starts */
            v->pend = 0;
            start(p, i, v->pend_track, v->pend_owner, v->pend_note, v->pend_vel, v->pend_patch, &v->pend_at, &v->pend_u);
            if (v->pend_off)
                qd_quad_voice_release(&v->q);
        }
    }
}

int qd_pool_render(qd_pool_t *p, int i, int32_t *out)
{
    qd_pool_voice_t *v;
    int r, k;
    if (i < 0 || i >= p->nvoice)
        return 0;
    v = &p->v[i];
    if (!v->dip_n)
        return qd_quad_voice_render(&v->q, out);
    v->q.dip = 1;                                       /* the dip: this block to 0 with the old routing .. */
    r = qd_quad_voice_render(&v->q, out);
    v->q.dip = 0;
    for (k = 0; k < v->dip_n; k++)                      /* .. the new one lands; the next block ramps up from 0 */
        v->q.patch[v->dip_id[k]] = v->dip_v[k];
    v->dip_n = 0;
    return r;
}

/* ---------------------------------------------------------------- a voice's values (P1.2) --- */
int qd_pool_is_dip_id(int id)
{
    int k;
    if (id == QD_QP_ALGO || id == QD_QP_FTYPE || id == QD_QP_PHRT)
        return 1;
    if (id == QD_QP_AMOD || id == QD_QP_DRPRE || id == QD_QP_BWPOST || id == QD_QP_CRPOST)
        return 1;                                       /* (E1..E3: a routing change) */
    for (k = 0; k < 3; k++)
        if (id == QD_QP_LFO(k, QD_QL_WAVE) || id == QD_QP_LFO(k, QD_QL_TRIG))
            return 1;
    return 0;
}

int qd_pool_owns(const qd_pool_t *p, int i, int id)
{
    const qd_pool_voice_t *v;
    if (i < 0 || i >= p->nvoice || id < 0 || id >= QD_QUAD_NP)
        return 0;
    v = &p->v[i];
    return (int)((v->at.own[id >> 5] >> (id & 31)) & 1u);
}

static int write1(qd_pool_t *p, int i, int id, int value, int own);
int qd_pool_write(qd_pool_t *p, int i, int id, int value, int own)
{
    int r, k;
    if (i < 0 || i >= p->nvoice)
        return 0;
    r = write1(p, i, id, value, own);
    for (k = sib_next(p, i, -1); k >= 0; k = sib_next(p, i, k))   /* (a unison note's other voices: the same) */
        write1(p, k, id, value, own);
    return r;
}
static int write1(qd_pool_t *p, int i, int id, int value, int own)
{
    qd_pool_voice_t *v;
    int16_t lo, hi, x;
    int k, r = 0;
    if (i < 0 || i >= p->nvoice || !qd_quad_value_range(id, &lo, &hi, 0))
        return 0;
    v = &p->v[i];
    x = (int16_t)(value < lo ? lo : value > hi ? hi : value);
    if (v->pend) {                                      /* the note waiting for this voice's fade */
        if (own)
            v->pend_at.own[id >> 5] |= 1u << (id & 31);
        if (v->pend_patch[id] != x) {
            v->pend_patch[id] = x;
            r = 1;
        }
    }
    if (own)
        v->at.own[id >> 5] |= 1u << (id & 31);
    for (k = 0; k < v->dip_n; k++)                      /* already waiting for the dip: the newer value */
        if (v->dip_id[k] == id) {
            v->dip_v[k] = x;
            return 2;
        }
    if (v->q.patch[id] == x)
        return r;
    p->writes++;
    if (p->dip_on && qd_pool_is_dip_id(id) && qd_pool_sounding(p, i) && v->q.env_out && v->dip_n < QD_DIP_MAX) {
        if (!v->dip_n)
            p->dips++;
        v->dip_id[v->dip_n] = (uint8_t)id;
        v->dip_v[v->dip_n] = x;
        v->dip_n++;
        return 2;
    }
    v->q.patch[id] = x;
    return 1;
}

static void set_mix1(qd_pool_t *p, int i, int k, int value);
void qd_pool_set_mix(qd_pool_t *p, int i, int k, int value)
{
    int s;
    if (i < 0 || i >= p->nvoice || k < 0 || k >= QD_VMIX_N)
        return;
    for (s = sib_next(p, i, -1); s >= 0; s = sib_next(p, i, s))   /* (a unison note's others: a pan lock keeps their
                                                         * places around it) */
        set_mix1(p, s, k, k == QD_VMIX_PAN && p->v[s].u.panon ? value + p->v[s].u.pan - p->v[i].u.pan : value);
    set_mix1(p, i, k, value);
}
static void set_mix1(qd_pool_t *p, int i, int k, int value)
{
    qd_pool_voice_t *v = &p->v[i];
    value = k == QD_VMIX_PAN ? (value < -64 ? -64 : value > 63 ? 63 : value) : (value < 0 ? 0 : value > 127 ? 127 : value);
    v->at.mixm |= (uint8_t)(1u << k);
    v->at.mixv[k] = (int16_t)value;
    if (v->pend) {
        v->pend_at.mixm |= (uint8_t)(1u << k);
        v->pend_at.mixv[k] = (int16_t)value;
    }
}

int qd_pool_legato(qd_pool_t *p, int i, int note, int vel)
{
    qd_pool_voice_t *v;
    if (i < 0 || i >= p->nvoice || !qd_pool_sounding(p, i))
        return 0;
    v = &p->v[i];
    qd_quad_voice_legato(&v->q, note, vel);
    v->age = ++p->clock;
    p->legatos++;
    {
        int k;
        for (k = sib_next(p, i, -1); k >= 0; k = sib_next(p, i, k))   /* (a unison note's others move with it) */
            if (sounding(&p->v[k]))
                qd_quad_voice_legato(&p->v[k].q, note, vel);
    }
    return 1;
}

/* ---------------------------------------------------------------- E1: portamento's starts --- */
int qd_pool_gated(const qd_pool_t *p, int track)
{
    int i;
    for (i = 0; i < p->nvoice; i++)
        if (sounding(&p->v[i]) && p->v[i].track == track && p->v[i].q.gate)
            return 1;
    return 0;
}

/* owner o's chord (o->cc) settled: each note's start by rank against the last chord (o->lc), a lone note from the track's
 * last note; then it is the last chord */
static void glide_settle(qd_pool_t *p, qd_quad_owner_t *o)
{
    int n = o->cc_n, k, j, ord[8];
    if (!n)
        return;
    for (k = 0; k < n; k++)                             /* the chord's notes low to high (ties: as they came) */
        ord[k] = k;
    for (k = 1; k < n; k++)
        for (j = k; j > 0 && o->cc[ord[j]] < o->cc[ord[j - 1]]; j--) {
            const int x = ord[j];
            ord[j] = ord[j - 1];
            ord[j - 1] = x;
        }
    for (k = 0; k < n; k++) {
        const int e = ord[k], i = o->cc_slot[e], note = o->cc[e];
        qd_pool_voice_t *v = &p->v[i];
        const int16_t *pt = v->pend ? v->pend_patch : v->q.patch;
        int from = -1;
        if (i >= p->nvoice)
            continue;
        if (n == 1 || !o->lc_n) {
            from = o->last_pitch;
        } else if (k < o->lc_n) {
            from = o->lc[k];
        } else {                                        /* (one more than the last chord had: the nearest) */
            int best = 0, b;
            for (b = 1; b < o->lc_n; b++)
                if ((o->lc[b] - note * 16 < 0 ? note * 16 - o->lc[b] : o->lc[b] - note * 16) <
                    (o->lc[best] - note * 16 < 0 ? note * 16 - o->lc[best] : o->lc[best] - note * 16))
                    best = b;
            from = o->lc[best];
        }
        if (!pt[QD_QP_PORT] || (pt[QD_QP_PORT] == QD_PORT_LEGATO && !o->cc_held))
            from = -1;
        if (v->pend && v->pend_owner == o && v->pend_note == note)
            v->pend_gfrom = (int16_t)from;
        else if (v->q.active && v->q.owner == o && v->q.note == note)
            qd_quad_voice_glide(&v->q, from);
        {   /* (a unison note's other voices glide with it) */
            int s;
            for (s = sib_next(p, i, -1); s >= 0; s = sib_next(p, i, s)) {
                qd_pool_voice_t *w = &p->v[s];
                if (w->pend && w->pend_owner == o && w->pend_note == note)
                    w->pend_gfrom = (int16_t)from;
                else if (w->q.active && w->q.owner == o && w->q.note == note)
                    qd_quad_voice_glide(&w->q, from);
            }
        }
    }
    for (k = 0; k < n; k++)
        o->lc[k] = (int16_t)(o->cc[ord[k]] * 16);
    o->lc_n = (uint8_t)n;
    o->last_pitch = o->lc[n - 1];                       /* (a lone note next glides from the chord's top) */
    o->cc_n = 0;
}

void qd_pool_glide_note(qd_pool_t *p, int i, qd_quad_owner_t *o, const void *key, int64_t tick, int held)
{
    const qd_pool_voice_t *v;
    if (!o || i < 0 || i >= p->nvoice)
        return;
    v = &p->v[i];
    if (o->cc_n && (!key || key != o->cc_key || tick != o->cc_tick || o->cc_n >= 8))
        glide_settle(p, o);                             /* (another chord: the one before settles first) */
    if (!o->cc_n) {
        o->cc_key = key;
        o->cc_tick = tick;
        o->cc_held = (uint8_t)(held != 0);
    }
    o->cc[o->cc_n] = (uint8_t)(v->pend ? v->pend_note : v->q.note);
    o->cc_slot[o->cc_n++] = (uint8_t)i;
}

void qd_pool_glide_tie(qd_pool_t *p, qd_quad_owner_t *o, int note)
{
    if (!o)
        return;
    glide_settle(p, o);
    o->lc[0] = o->last_pitch = (int16_t)((note & 127) * 16);
    o->lc_n = 1;
}
