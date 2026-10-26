/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The mix (mix.h). A voice's block goes in through fz_voice.h fz_mix_voice (L1): the level from LEVEL_Q12, the
 * voice's sample pre-shifted and saturated, the pan's two gains, the three sends each ramped from the gain the last
 * block left to this block's over the 32 samples (ChoralRoot's arithmetic for a part, to the bit). */
#include <string.h>

#include "mix.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "quad/fz_voice.h"             /* (L1: LEVEL_Q12, fz_mix_voice, the gains) */
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

void qd_mix_init(qd_mix_t *m) { memset(m, 0, sizeof *m); }

void qd_mix_begin(qd_mix_t *m, qd_pool_t *pool, qd_quad_owner_t *const *owners, int ntracks)
{
    int i, t;
    (void)m;
    qd_pool_block_begin(pool);                          /* (the notes that waited for a fade start: ChoralRoot's events
                                                         * come before the parts' block) */
    for (t = 0; t < ntracks; t++)
        if (owners[t])
            qd_quad_owner_block(owners[t]);             /* (engine_t.block, before the voices) */
    for (i = 0; i < pool->nvoice; i++)                  /* (P2.6: the comb lines, in the voices' order) */
        if (pool->v[i].q.active && pool->v[i].track < ntracks)
            qd_quad_voice_prepare(&pool->v[i].q);
}

int qd_mix_voices(qd_mix_t *m, qd_pool_t *pool, const qd_mix_track_t *tracks, int ntracks, int v0, int v1, int32_t *l,
                  int32_t *r, int32_t *sc, int32_t *sd, int32_t *sr)
{
    int32_t vbuf[QD_MIX_CTL];                           /* (P2.6: the voice's block on this call's stack) */
    int i, nr = 0;
    for (i = 0; i < QD_MIX_CTL; i++)
        l[i] = r[i] = sc[i] = sd[i] = sr[i] = 0;
    v0 = v0 < 0 ? 0 : v0;
    v1 = v1 > pool->nvoice ? pool->nvoice : v1;
    for (i = v0; i < v1; i++) {
        qd_pool_voice_t *pv = &pool->v[i];
        const qd_mix_track_t *tk;
        const qd_quad_owner_t *o;
        int32_t lvl, pan, gl, gr, c, d, rv, xmax, *sg = m->sg[i];
        int32_t po;
        const int8_t *fo;
        if (!pv->q.active || pv->track >= ntracks)
            continue;
        memset(vbuf, 0, sizeof vbuf);
        if (!qd_pool_render(pool, i, vbuf))
            continue;
        nr++;
        tk = &tracks[pv->track];
        o = pv->q.owner;
        po = o ? o->quad_pan_off : 0;
        fo = o ? o->quad_fx_off : 0;
        if (o && o->m_v == &pv->q && pv->q.nx_ok)       /* (P2.6: the latest voice's own offsets: the same values the
                                                         * owner holds without defer_off) */
            po = pv->q.nx_pan, fo = pv->q.nx_fx;
        {   /* the track's mix, the voice's mix locks over it (PLAN §5: P1.2) */
            const uint8_t mm = pv->at.mixm;
            const int32_t tl = mm & (1u << QD_VMIX_LEVEL) ? pv->at.mixv[QD_VMIX_LEVEL] : tk->level;
            const int32_t tc = mm & (1u << QD_VMIX_CHORUS) ? pv->at.mixv[QD_VMIX_CHORUS] : tk->chorus;
            const int32_t td = mm & (1u << QD_VMIX_DELAY) ? pv->at.mixv[QD_VMIX_DELAY] : tk->delay;
            const int32_t tr = mm & (1u << QD_VMIX_REVERB) ? pv->at.mixv[QD_VMIX_REVERB] : tk->reverb;
            lvl = LEVEL_Q12[tl & 127];
            pan = mm & (1u << QD_VMIX_PAN) ? pv->at.mixv[QD_VMIX_PAN] : tk->pan;
            if (po)                                     /* eng_quad.c quad_pan: the LFOs' PAN */
                pan = clampi(pan + po, -64, 63);
            fz_pan_gains(pan, &gl, &gr);
            c = fz_send_gain(tc), d = fz_send_gain(td), rv = fz_send_gain(tr);
            if (fo && (fo[1] | fo[2] | fo[3])) {        /* eng_quad.c quad_sends */
                rv = fz_send_gain(clampi(tr + fo[1], 0, 127));
                d = fz_send_gain(clampi(td + fo[2], 0, 127));
                c = fz_send_gain(clampi(tc + fo[3], 0, 127));
            }
            gl = gl * tk->g_lvl >> 12, gr = gr * tk->g_lvl >> 12;   /* 2026-10-26: the track's DRY / WET on the dry
                                                         * alone (the pan gains: the sends are after the level) and, in
                                                         * CHAIN, its sends as serial shares (app.c, Q12; 4096: as before) */
            c = c * tk->g_send[0] >> 12, d = d * tk->g_send[1] >> 12, rv = rv * tk->g_send[2] >> 12;
        }
        if (m->serial[i] != pv->serial) {               /* a fresh note in this slot: its sends from where they are */
            m->serial[i] = pv->serial;
            sg[0] = c, sg[1] = d, sg[2] = rv;
            memset(&m->dist[i], 0, sizeof m->dist[i]);  /* (P1.4a: its DRIVE from rest) */
        }
        {   /* P1.4a: DRIVE (qd_quad_dist, with eng_quad.c quad_drive's LFO offset), the voice's own */
            int32_t dr = pv->q.patch[QD_QP_DRIVE];
            if (fo && fo[0])
                dr = clampi(dr + fo[0], 0, 127);
            if (!pv->q.patch[QD_QP_DRPRE])              /* (E1: DRIVE PRE ran in the voice, before its filter) */
                qd_quad_dist(&m->dist[i], dr, vbuf, QD_MIX_CTL);
        }
        {   /* (L1: fz_voice.h: the sends ramp from the gains the last block left to this block's) */
            const int32_t now[3] = {c, d, rv};
            xmax = fz_send_xmax(now, sg);
            fz_mix_voice(vbuf, lvl, gl, gr, xmax, sg, now, l, r, sc, sd, sr);
        }
        sg[0] = c, sg[1] = d, sg[2] = rv;
    }
    return nr;
}

void qd_mix_end(qd_mix_t *m, qd_quad_owner_t *const *owners, int ntracks, int rendered)
{
    int t;
    for (t = 0; t < ntracks; t++)
        if (owners[t])
            qd_quad_owner_publish(owners[t]);
    m->rendered = rendered;
}

int qd_mix_block(qd_mix_t *m, qd_pool_t *pool, qd_quad_owner_t *const *owners, const qd_mix_track_t *tracks,
                 int ntracks, int32_t *l, int32_t *r, int32_t *sc, int32_t *sd, int32_t *sr)
{
    int nr;
    qd_mix_begin(m, pool, owners, ntracks);
    nr = qd_mix_voices(m, pool, tracks, ntracks, 0, pool->nvoice, l, r, sc, sd, sr);
    qd_mix_end(m, owners, ntracks, nr);
    return nr;
}
