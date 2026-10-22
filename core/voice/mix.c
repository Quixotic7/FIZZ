/* SPDX-License-Identifier: GPL-3.0-only */
/* The mix (mix.h). A voice's block goes in as ChoralRoot's fx.c took a part's (v0.25.1 mix_part_out, mix_part_loop
 * with sends and no stereo side): the level from LEVEL_Q12, the voice's sample pre-shifted and saturated, the pan's
 * two gains, the three sends each ramped from the gain the last block left to this block's over the 32 samples. */
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
#include "quad/cr_tables.h"            /* (LEVEL_Q12) */
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

void qd_mix_init(qd_mix_t *m) { memset(m, 0, sizeof *m); }

/* fx.c mix_part_loop (side 0, sends 1), on one voice's block b */
static void mix_loop(const int32_t *b, int32_t lvl, int32_t gl, int32_t gr, int32_t xmax, const int32_t *g0,
                     const int32_t *dg, int32_t *l, int32_t *r, int32_t *sc, int32_t *sd, int32_t *sr)
{
    int32_t s0 = g0[0], s1 = g0[1], s2 = g0[2], d0 = dg[0], d1 = dg[1], d2 = dg[2];
    int i;
    for (i = 0; i < QD_MIX_CTL; i++) {
        int32_t x = clampi(((clampi(b[i], -884000, 884000) >> 2) * lvl) >> 10, -524287, 524287), xs;
        xs = clampi(x, -xmax, xmax);                    /* sends: mulq15 would overflow */
        s0 += d0;
        if (s0)
            sc[i] += (xs * s0) >> 15;
        s1 += d1;
        if (s1)
            sd[i] += (xs * s1) >> 15;
        s2 += d2;
        if (s2)
            sr[i] += (xs * s2) >> 15;
        l[i] += (x * gl) >> 12;
        r[i] += (x * gr) >> 12;
    }
}

int qd_mix_block(qd_mix_t *m, qd_pool_t *pool, qd_quad_owner_t *const *owners, const qd_mix_track_t *tracks,
                 int ntracks, int32_t *l, int32_t *r, int32_t *sc, int32_t *sd, int32_t *sr)
{
    int i, t, nr = 0;
    for (i = 0; i < QD_MIX_CTL; i++)
        l[i] = r[i] = sc[i] = sd[i] = sr[i] = 0;
    qd_pool_block_begin(pool);                          /* (the notes that waited for a fade start: ChoralRoot's events
                                                         * come before the parts' block) */
    for (t = 0; t < ntracks; t++)
        if (owners[t])
            qd_quad_owner_block(owners[t]);             /* (engine_t.block, before the voices) */
    for (i = 0; i < pool->nvoice; i++) {
        const qd_pool_voice_t *pv = &pool->v[i];
        const qd_mix_track_t *tk;
        const qd_quad_owner_t *o;
        int32_t lvl, pan, gl, gr, c, d, rv, xmax, dg[3], *sg = m->sg[i];
        if (!pv->q.active || pv->track >= ntracks)
            continue;
        memset(m->vbuf, 0, sizeof m->vbuf);
        if (!qd_pool_render(pool, i, m->vbuf))
            continue;
        nr++;
        tk = &tracks[pv->track];
        o = pv->q.owner;
        {   /* the track's mix, the voice's mix locks over it (PLAN §5: P1.2) */
            const uint8_t mm = pv->at.mixm;
            const int32_t tl = mm & (1u << QD_VMIX_LEVEL) ? pv->at.mixv[QD_VMIX_LEVEL] : tk->level;
            const int32_t tc = mm & (1u << QD_VMIX_CHORUS) ? pv->at.mixv[QD_VMIX_CHORUS] : tk->chorus;
            const int32_t td = mm & (1u << QD_VMIX_DELAY) ? pv->at.mixv[QD_VMIX_DELAY] : tk->delay;
            const int32_t tr = mm & (1u << QD_VMIX_REVERB) ? pv->at.mixv[QD_VMIX_REVERB] : tk->reverb;
            lvl = LEVEL_Q12[tl & 127];
            pan = mm & (1u << QD_VMIX_PAN) ? pv->at.mixv[QD_VMIX_PAN] : tk->pan;
            if (o && o->quad_pan_off)                   /* eng_quad.c quad_pan: the LFOs' PAN */
                pan = clampi(pan + o->quad_pan_off, -64, 63);
            gl = 4096 - (pan > 0 ? pan * 64 : 0);
            gr = 4096 + (pan < 0 ? pan * 64 : 0);
            c = tc * 258, d = td * 258, rv = tr * 258;
            if (o && (o->quad_fx_off[1] | o->quad_fx_off[2] | o->quad_fx_off[3])) {   /* eng_quad.c quad_sends */
                rv = clampi(tr + o->quad_fx_off[1], 0, 127) * 258;
                d = clampi(td + o->quad_fx_off[2], 0, 127) * 258;
                c = clampi(tc + o->quad_fx_off[3], 0, 127) * 258;
            }
        }
        if (m->serial[i] != pv->serial) {               /* a fresh note in this slot: its sends from where they are */
            m->serial[i] = pv->serial;
            sg[0] = c, sg[1] = d, sg[2] = rv;
            memset(&m->dist[i], 0, sizeof m->dist[i]);  /* (P1.4a: its DRIVE from rest) */
        }
        {   /* P1.4a: DRIVE (fx.c track_dist with eng_quad.c quad_drive's LFO offset), the voice's own */
            int32_t dr = pv->q.patch[QD_QP_DRIVE];
            if (o && o->quad_fx_off[0])
                dr = clampi(dr + o->quad_fx_off[0], 0, 127);
            if (!pv->q.patch[QD_QP_DRPRE])              /* (E1: DRIVE PRE ran in the voice, before its filter) */
                qd_quad_dist(&m->dist[i], dr, m->vbuf, QD_MIX_CTL);
        }
        dg[0] = (c - sg[0]) >> 5;                       /* (fx.c: the ramp over the block, CTL_LOG2) */
        dg[1] = (d - sg[1]) >> 5;
        dg[2] = (rv - sg[2]) >> 5;
        xmax = c > sg[0] ? c : sg[0];
        xmax = d > xmax ? d : sg[1] > xmax ? sg[1] : xmax;
        xmax = rv > xmax ? rv : sg[2] > xmax ? sg[2] : xmax;
        xmax = 0x7FFFFFFF / (xmax | 1);
        mix_loop(m->vbuf, lvl, gl, gr, xmax, sg, dg, l, r, sc, sd, sr);
        sg[0] = c, sg[1] = d, sg[2] = rv;
    }
    m->rendered = nr;
    return nr;
}
