/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
 * not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* FIZZ voice building blocks run once per control block or per sample: envelopes, glide, the drive stage, mixing a
 * voice into the stereo bus and the effect sends, and a two-tap chorus. */

#ifndef FZ_VOICE_H
#define FZ_VOICE_H

#include <stdint.h>

#include "fz_dsp.h"

/* envelope stages (lvl is Q24, 1.0 = 1 << 24) */
enum {
    FZ_ST_OFF = 0,
    FZ_ST_DELAY = 1,
    FZ_ST_ATTACK = 2,
    FZ_ST_DECAY = 3,     /* decay toward sustain, then sustain */
    FZ_ST_RELEASE = 4,
    FZ_ST_REL_HOLD = 5,  /* release time infinite: hold the level */
    FZ_ST_HOLD = 6,      /* attack-hold-decay: hold */
    FZ_ST_AHD_DEC = 7    /* attack-hold-decay: decay */
};

#define FZ_ONE_Q24 (1 << 24)

/* ---- envelopes ---- */

/* One control tick of a delay-attack-decay-sustain-release envelope; returns the level in Q15.
 * op: an operator envelope (its attack multiplies, i.e. is linear in dB, from at least op_floor).
 * amp: the amplitude envelope (its decay at 127 holds the level). adsr: the gate ends delay/attack/decay.
 * The delay advances by the tempo (clamped 20..300) times dly_k per tick and lasts dly << 24. */
static inline int32_t fz_env_step(int32_t *lvl, uint8_t *stage, uint32_t *prog, int op, const uint32_t *tatk,
                                  const uint32_t *tdec, int32_t atk, int32_t dec, int32_t sus, int32_t rel, int32_t dly,
                                  int gate, int adsr, int amp, int32_t bpm, int32_t op_floor, uint32_t dly_k)
{
    int32_t v = *lvl;

    if (adsr && !gate && (*stage == FZ_ST_DELAY || *stage == FZ_ST_ATTACK || *stage == FZ_ST_DECAY))
        *stage = FZ_ST_RELEASE;

    switch (*stage) {
    case FZ_ST_DELAY:
        *prog += (uint32_t)clamp(bpm, 20, 300) * dly_k;
        if (*prog >= ((uint32_t)(dly & 127) << 24))
            *stage = FZ_ST_ATTACK;
        break;
    case FZ_ST_ATTACK:
        if (op) {
            if (v < op_floor)
                v = op_floor;
            v += (int32_t)(((int64_t)v * tatk[atk & 127]) >> 24);
        } else {
            v += (int32_t)tatk[atk & 127];
        }
        if (v >= FZ_ONE_Q24) {
            v = FZ_ONE_Q24;
            *stage = FZ_ST_DECAY;
        }
        break;
    case FZ_ST_DECAY:
        if (!(amp && dec >= 127))
            v += (int32_t)(((int64_t)(sus - v) * tdec[dec & 127]) >> 24);
        break;
    case FZ_ST_RELEASE:
    case FZ_ST_REL_HOLD:
        if (rel >= 127) {
            *stage = FZ_ST_REL_HOLD;
        } else {
            *stage = FZ_ST_RELEASE;
            /* rel is 0..126 here; the mask only keeps a bad caller inside the table */
            v -= (int32_t)(((int64_t)v * tdec[rel & 127] + 0xFFFFFF) >> 24);
            if (v < 4096) {
                v = 0;
                *stage = FZ_ST_OFF;
            }
        }
        break;
    default:
        v = 0;
        break;
    }

    *lvl = v;
    return v >> 9;
}

/* One control tick of an attack-hold-decay envelope (stages 2, 6, 7); a caller's decay stage enters the hold, a
 * delay stage restarts the attack and a release stage enters the decay. Returns the level in Q15. */
static inline int32_t fz_ahd_step(int32_t *lvl, uint8_t *stage, uint32_t *prog, const uint32_t *tatk,
                                  const uint32_t *tdec, int32_t atk, int32_t hold, int32_t dec, int gate)
{
    int32_t v = *lvl;

    if (*stage == FZ_ST_DECAY) {
        *stage = FZ_ST_HOLD;
        *prog = 0;
    } else if (*stage == FZ_ST_DELAY) {
        *stage = FZ_ST_ATTACK;
        *prog = 0;
    } else if (*stage == FZ_ST_RELEASE || *stage == FZ_ST_REL_HOLD) {
        *stage = FZ_ST_AHD_DEC;
    }

    switch (*stage) {
    case FZ_ST_ATTACK:
        v += (int32_t)tatk[atk & 127];
        if (v >= FZ_ONE_Q24) {
            v = FZ_ONE_Q24;
            *stage = FZ_ST_HOLD;
            *prog = 0;
        }
        break;
    case FZ_ST_HOLD:
        if (hold >= 127) {
            if (!gate)
                *stage = FZ_ST_AHD_DEC;
        } else {
            *prog += tatk[hold & 127];
            if (*prog >= (uint32_t)FZ_ONE_Q24)
                *stage = FZ_ST_AHD_DEC;
        }
        break;
    case FZ_ST_AHD_DEC:
        if (dec < 127) {
            v -= (int32_t)(((int64_t)v * tdec[dec & 127] + 0xFFFFFF) >> 24);
            if (v < 4096) {
                v = 0;
                *stage = FZ_ST_OFF;
            }
        }
        break;
    default:
        v = 0;
        break;
    }

    *lvl = v;
    return v >> 9;
}

/* ---- glide ---- */

/* per-tick step that covers from..to in about 1 + t*t/8 ticks (t 0..127), at least 1 */
static inline int32_t fz_glide_time_step(int32_t from, int32_t to, int32_t t)
{
    int32_t dist = to > from ? to - from : from - to;
    int32_t ticks = 1 + t * t / 8;
    int32_t step = dist / ticks;
    return step > 0 ? step : 1;
}

/* cur moved toward target by at most step (step below 1 counts as 1), never past it */
static inline int32_t fz_glide_move(int32_t cur, int32_t target, int32_t step)
{
    int64_t gap = (int64_t)target - cur;
    if (step < 1)
        step = 1;
    if (gap > step)
        return cur + step;
    if (gap < -(int64_t)step)
        return cur - step;
    return target;
}

/* ---- drive ---- */

typedef struct {
    int32_t hp, lp1, lp2;
} fz_drive_t;

/* Saturating drive of n samples in place (drive 0..127; 0 bypasses it): a DC-removing high-pass, gain into an
 * offset (asymmetric) soft clip, the offset's own level removed, two one-pole low-passes and a make-up gain that
 * falls as the drive rises. */
static inline void fz_drive(fz_drive_t *s, int drive, int32_t *b, int n)
{
    int32_t gain, kt, mk, base;
    const int32_t bias = 2400;
    int i;

    if (drive == 0)
        return;
    gain = 4096 + 2 * drive * drive;
    kt = 32000 - 95 * drive;
    mk = 30000 - 120 * drive;
    base = softclip(bias);

    for (i = 0; i < n; i++) {
        int32_t x = b[i], x2, y;
        s->hp += (x - s->hp + 64) >> 7;
        x2 = clamp(x - s->hp, -230000, 230000);
        y = softclip((((x2 >> 2) * gain) >> 10) + bias) - base;
        s->lp1 += mulq15(y - s->lp1, kt);
        s->lp2 += mulq15(s->lp1 - s->lp2, kt);
        b[i] = mulq15(s->lp2, mk);
    }
}

/* ---- mixing ---- */

/* stereo gains in Q12 for pan -64..63: the far side falls by 64 per step, the near side stays at unity */
static inline void fz_pan_gains(int32_t pan, int32_t *gl, int32_t *gr)
{
    *gl = 4096 - (pan > 0 ? 64 * pan : 0);
    *gr = 4096 + (pan < 0 ? 64 * pan : 0);
}

/* a send amount 0..127 as a Q15 gain */
static inline int32_t fz_send_gain(int32_t v)
{
    return v * 258;
}

/* the largest |sample| whose product with any send gain of this block (old or new) still fits in int32 */
static inline int32_t fz_send_xmax(const int32_t now[3], const int32_t was[3])
{
    int32_t m = now[0] > was[0] ? now[0] : was[0];
    m = now[1] > m ? now[1] : (was[1] > m ? was[1] : m);
    m = now[2] > m ? now[2] : (was[2] > m ? was[2] : m);
    return 0x7FFFFFFF / (m | 1);
}

/* Adds one voice's CTL samples b (scaled by lvl, Q10 after a >> 2) to l/r with the pan gains gl/gr (Q12), and to
 * the three sends s0..s2 with gains ramping from was[] to now[] (Q15) over the block. A send whose gain is 0 for a
 * sample is not touched for that sample. */
static inline void fz_mix_voice(const int32_t *b, int32_t lvl, int32_t gl, int32_t gr, int32_t xmax,
                                const int32_t was[3], const int32_t now[3], int32_t *l, int32_t *r, int32_t *s0,
                                int32_t *s1, int32_t *s2)
{
    int32_t *send[3];
    int32_t gk[3], dk[3];
    int i, k;

    send[0] = s0;
    send[1] = s1;
    send[2] = s2;
    for (k = 0; k < 3; k++) {
        gk[k] = was[k];
        dk[k] = (now[k] - was[k]) >> CTL_LOG2;
    }

    for (i = 0; i < CTL; i++) {
        int32_t x = clamp(((clamp(b[i], -884000, 884000) >> 2) * lvl) >> 10, -524287, 524287);
        int32_t xs = clamp(x, -xmax, xmax);
        for (k = 0; k < 3; k++) {
            gk[k] += dk[k];
            if (gk[k] != 0)
                send[k][i] += (xs * gk[k]) >> 15;
        }
        l[i] += (x * gl) >> 12;
        r[i] += (x * gr) >> 12;
    }
}

/* ---- chorus ---- */

typedef struct {
    int32_t dc;    /* depth now (gliding toward the target) */
    int32_t oc;    /* phase offset of the right tap now (gliding) */
    int32_t ra;    /* left tap delay at the end of the last block, Q8 samples */
    int32_t rb;    /* right tap delay at the end of the last block, Q8 samples */
    uint32_t live; /* samples left until the line has drained after the last nonzero input; 0 = idle */
} fz_chorus_t;

#define FZ_CH_LEN 2048u
#define FZ_CH_MASK (FZ_CH_LEN - 1u)

/* tap delay (Q8 samples) for LFO phase p and depth d: 400 samples plus up to 65535 * d / 4096 / 256 */
static inline int32_t fz_ch_pos(uint32_t p, int32_t d)
{
    return 102400 + (((sine_i(p) + 32768) * d) >> 12);
}

/* one sixty-fourth of the way from c to t, rounding so that a rising value still arrives */
static inline int32_t fz_ch_glide(int32_t c, int32_t t)
{
    return c + ((t - c + (t > c ? 63 : 0)) >> 6);
}

/* one linearly interpolated read of the line at delay r (Q8 samples) behind write position wi */
static inline int32_t fz_ch_tap(const int16_t *line, uint32_t wi, int32_t r)
{
    uint32_t k = wi - ((uint32_t)r >> 8);
    int32_t c0 = line[k & FZ_CH_MASK];
    int32_t c1 = line[(k - 1u) & FZ_CH_MASK];
    return (c0 + (((c1 - c0) * (r & 255)) >> 8)) * 2;
}

/* Runs n samples of in through a 2048-sample line with two LFO-swept taps added to ml (left) and mr (right).
 * depth, rate and width are 0..127; width sets the right tap's LFO phase offset. The tap delays move linearly over
 * the block from where the last block left them. Idle (live 0) with silent input costs nothing beyond advancing
 * the write counter and the LFO phase. */
static inline void fz_chorus_block(fz_chorus_t *c, int16_t *line, uint32_t *w, uint32_t *ph, const int32_t *in,
                                   int32_t *ml, int32_t *mr, uint32_t n, int32_t depth, int32_t rate, int32_t width)
{
    int32_t dt = depth * 96;
    int32_t ot = width * 8454660;
    uint32_t w0 = *w;
    uint32_t p0 = *ph;
    uint32_t p1 = p0 + (LFO_INC[rate & 127] / 32u) * n;
    int32_t ea, eb, da, db, ra, rb;
    int heard = 0;
    uint32_t i;

    *ph = p1;
    *w = w0 + n;

    if (c->live == 0) {
        int any = 0;
        for (i = 0; i < n; i++) {
            if (in[i] != 0) {
                any = 1;
                break;
            }
        }
        if (!any)
            return;
        c->dc = dt;
        c->oc = ot;
        c->ra = fz_ch_pos(p0, dt);
        c->rb = fz_ch_pos(p0 + (uint32_t)ot, dt);
    }

    if (c->dc != dt)
        c->dc = fz_ch_glide(c->dc, dt);
    if (c->oc != ot)
        c->oc = fz_ch_glide(c->oc, ot);

    ea = fz_ch_pos(p1, c->dc);
    eb = fz_ch_pos(p1 + (uint32_t)c->oc, c->dc);
    da = ea - c->ra;
    db = eb - c->rb;
    ra = c->ra << 5;
    rb = c->rb << 5;
    c->ra = ea;
    c->rb = eb;
    if (n != 32u && n != 0u) {
        /* signed division: n must not drag da/db into unsigned arithmetic */
        da = da * 32 / (int32_t)n;
        db = db * 32 / (int32_t)n;
    }

    for (i = 0; i < n; i++) {
        uint32_t wi = w0 + i;
        int32_t x = in[i];
        if (x != 0)
            heard = 1;
        line[wi & FZ_CH_MASK] = (int16_t)clamp(x >> 1, -32768, 32767);
        ra += da;
        ml[i] += fz_ch_tap(line, wi, ra >> 5);
        rb += db;
        mr[i] += fz_ch_tap(line, wi, rb >> 5);
    }

    if (heard)
        c->live = FZ_CH_LEN;
    else
        c->live = c->live > n ? c->live - n : 0u;
}

#endif /* FZ_VOICE_H */
