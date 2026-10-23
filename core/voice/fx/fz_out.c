/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* L1: FIZZ's output chain: ChoralRoot's Options > Output "Clean" (the owner's: ChoralRoot 0.19, docs/AUDIO-QUALITY.md
 * there), the only chain FIZZ runs, and the chorus line cr_fx.c's two taps read. Built only through qd_fx.c.
 *   the mix at a fixed gain (CLN_TRIM), its samples Q17 -> a DC blocker (fz_dsp.h fz_dc_step) -> a look-ahead limiter
 *   that is a safety (threshold CLN_T, the peak held, released slowly, the gain on all the sample's bits) -> a knee,
 *   a straight line up to CLN_K and a tanh above -> MASTER, last, into the DAC's 24-bit word (out_frac: the 7 bits
 *   under the Q15 sample).
 * Its limiter's look-ahead, one block with the gain a straight line across it, is the design of SLOOP 2.6's master
 * (github.com/isod89/sloop-fm1 firmware/src/fx.c lim_block): the design, written anew, no code copied (LICENSING.md).
 * (L1: ChoralRoot's other chain, Classic, its LOWCUT / BASS+, the USB staging and the output fade between the chains
 * are not here: FIZZ never ran them.) */

/* the chorus's line (cr_fx.c xc_block: fz_voice.h fz_chorus_block) and its write counter and LFO phase */
#define CHO_LEN 2048u
static int16_t cho_buf[CHO_LEN] __attribute__((section(".pool")));
static struct {
    uint32_t cho_w, cho_ph;
} fx;
static volatile uint8_t fx_off;         /* the effects off (cr_fx.c reads it: FIZZ's is always 0) */
static fz_dc_t fx_dc[2];                /* the DC blockers, L R */

#ifndef CLN_TRIM
#define CLN_TRIM 3072                   /* Q12, a multiple of 16: the mix into the Clean chain, -2.5 dB (x 0.75) */
#endif
#ifndef CLN_T
#define CLN_T 26000                     /* the limiter's threshold in Q15 units (LIM_T + 3.2 dB) */
#endif
#define CLN_K 26000                     /* the knee: a straight line up to here (-2.0 dBFS: the limiter's threshold, so
                                         * nothing the limiter lets through meets the curve), then
                                         * K + R tanh((a - K) / R), R = 32767 - K: to full scale. A safety net only */
/* The hold (0.19's third round, docs/AUDIO-QUALITY.md "Round 3"). What Clean had left on a loud chord was the envelope
 * letting go between the chord's beats and stepping the level 0.1 to 2 dB at the next one. So the envelope is held
 * CLN_HOLD (200 ms) and held again by every block's peak within 1.2 dB of it (CLN_REHOLD): a chord, a wet tail, an
 * arp keep one gain. A HIT, a peak more than 3.5 dB above the envelope (CLN_HIT), gets the short hold of the first
 * rounds (CLN_HOLD_HIT, 40 ms: a period of 25 Hz) until the envelope has begun to let go: a kick or a slammed chord
 * is released as it was (12 dB down: back within half a dB after 0.47 s), not held down for the long one. */
#ifndef CLN_HOLD
#define CLN_HOLD 8820u                  /* 200 ms */
#endif
#ifndef CLN_HOLD_HIT
#define CLN_HOLD_HIT 1764u              /* 40 ms: after a hit */
#endif
#ifndef CLN_HIT
#define CLN_HIT 1                       /* a hit: a peak more than env >> CLN_HIT (3.5 dB) above the envelope */
#endif
#ifndef CLN_REHOLD
#define CLN_REHOLD 3                    /* a block's peak within env >> CLN_REHOLD (1.2 dB) of the envelope holds it again */
#endif
#ifndef CLN_CREEP
#define CLN_CREEP 10                    /* .. and, once CLN_HOLD samples of such blocks have gone by without the
                                         * top itself (a peak within 0.03 dB of the envelope), draws the envelope a
                                         * 1024th of the way down to itself: an envelope held up to 1.2 dB above a
                                         * steady sound (a release that stopped in the window) comes down to it
                                         * within a second or two, too slowly to be a step; a sound whose top comes
                                         * back (a bass note's period, a chord's beat) keeps its gain exactly */
#endif
#ifndef CLN_REL
#define CLN_REL 13                     /* the release: (env - T) >> CLN_REL a sample, 186 ms */
#endif
/* The look-ahead (0.19, docs/AUDIO-QUALITY.md "The look-ahead"). The chain's output is its input CLN_LA samples late,
 * and the limiter's gain is a straight line across each block of CLN_LA_B samples, from what the last block ended on
 * to the mean of the last CLN_LA_K blocks' gains, each of them the gain that fits the loudest sample of its own block
 * and of the CLN_LA_K before it under the threshold. So the gain has arrived before the peak comes out of the delay:
 * no sample leaves the limiter above CLN_T (cr_output_test.c proves it), where the gain used to ease down over 1.5 ms
 * AFTER the peak and the first samples went into the knee. One block of look-ahead and the linear glide between
 * blocks are Sloop 2.6's design (sloop-fm1 firmware/src/fx.c lim_block, GPL-3.0: LICENSING.md); the hold, the
 * release, the mean over CLN_LA_K blocks, the block maximum kept from block to block instead of a scan of the delay,
 * and the arithmetic are ChoralRoot's. CLN_LA_B divides CTL (the audio ISR's block: the block's peak is known before
 * its first sample goes out); a call with fewer samples (tests) takes a slower, safe way. */
#ifndef CLN_LA_B
#define CLN_LA_B 32u                    /* a block of the look-ahead, samples */
#endif
#ifndef CLN_LA_K
#define CLN_LA_K 1u                     /* blocks of look-ahead */
#endif
#define CLN_LA (CLN_LA_B * CLN_LA_K)    /* the delay: 32 samples, 0.73 ms */
#define CLN_DLEN (CLN_LA <= 16u ? 16u : CLN_LA <= 32u ? 32u : CLN_LA <= 64u ? 64u : 128u)
#ifndef CLN_MAKEUP
#define CLN_MAKEUP 4096                 /* Q12 after the knee, with MASTER: 4096 = none, the DAC word stays under the
                                         * Q15 sample's full scale as in every release. The word has 6 dB above that
                                         * (audio.c OUT_SHIFT): a build with 5461 gives the trim back there. NOT the
                                         * default: nobody has heard this unit's output stage up there
                                         * (docs/AUDIO-QUALITY.md "Proposed") */
#endif
#define CLN_T4 (4 * CLN_T)              /* (Q17) */
#define CLN_K4 (4 * CLN_K)
#define CLN_R4 (4 * (32767 - CLN_K))    /* the knee's range above K (Q17) */
#define CLN_KS (268435456 / CLN_R4)     /* 32768 / R4 in Q13: the tanh table's own scale */
#define CLN_G1 ((int32_t)(CLN_LA_K << 15))             /* the sum of CLN_LA_K block gains at rest */
_Static_assert(!(CLN_TRIM & 15) && CLN_T <= CLN_K && CLN_K <= 30000 && CLN_MAKEUP <= 8192, "the Clean chain's ranges");
_Static_assert(CLN_LA_K >= 1u && CLN_LA_K <= 8u && CLN_LA <= 128u && CTL % CLN_LA_B == 0u, "the look-ahead's blocks");
static int32_t cln_dl[2u * CLN_DLEN];   /* the delay: the chain's samples (Q17, after the DC blocker), L R. In RAM
                                         * (.bss, 256 bytes): the ISR reads and writes it every sample; the pool is
                                         * the tight resource (docs/FX.md "The cost on the unit") */
static uint32_t cln_w, cln_hold, cln_hlen = CLN_HOLD, cln_top;
static int32_t cln_pk[CLN_LA_K];        /* the peaks of the blocks in the delay, the newest first */
static int32_t cln_wh[CLN_LA_K];        /* their gains (Q15), a ring; cln_gs is their sum */
static uint32_t cln_wi;
static int32_t cln_env = CLN_T4, cln_wg = 32768, cln_gs = CLN_G1, cln_g = 32768, cln_mq;
static uint8_t out_frac[2u * CTL];      /* the bits under each of the block's Q15 samples: the DAC word's 7 (audio.c) */
#ifdef CLN_GAIN_TAP                     /* (tests/master_measure.c: the gain of each sample of the last call) */
static int32_t cln_gtap[CTL];
#endif

/* the limiter at rest and its delay empty (power-on; Options > Output changed, at the fade's zero) */
static void clean_rest(void)
{
    uint32_t i;
    for (i = 0; i < 2u * CLN_DLEN; i++)
        cln_dl[i] = 0;
    for (i = 0; i < CLN_LA_K; i++) {
        cln_pk[i] = 0;
        cln_wh[i] = 32768;
    }
    cln_w = cln_wi = cln_hold = cln_top = 0;
    cln_hlen = CLN_HOLD;
    cln_env = CLN_T4;
    cln_wg = cln_g = 32768;
    cln_gs = CLN_G1;
}

/* (Q17) */
static inline int32_t clean_knee(int32_t y)
{
    int32_t a = y < 0 ? -y : y;
    if (a <= CLN_K4)
        return y;
    a -= CLN_K4;
    if (a > 2 * CLN_R4)                 /* (the table ends at tanh 2) */
        a = 2 * CLN_R4;
    a = CLN_K4 + ((softclip((a * CLN_KS) >> 13) * CLN_R4) >> 15);
    return y < 0 ? -a : a;
}
/* x * g / 32768 with every bit of x (|x| < 2^23, g Q15, 32768 the identity): the limiter's gain (Classic's
 * (x >> 4) * g >> 11 drops four bits and, rounding down from them, shifts the sample by up to 16 while it limits) */
static inline int32_t clean_gain(int32_t x, int32_t g)
{
    return (((x >> 8) * g) >> 7) + (((x & 255) * g) >> 15);
}

/* the slow way's part (a call that is not whole blocks: tests): every peak the limiter knows of and the delay's own */
static __attribute__((noinline)) int32_t clean_short(int32_t p)
{
    uint32_t i;
    for (i = 0; i < CLN_LA_K; i++)
        if (cln_pk[i] > p)
            p = cln_pk[i];
    for (i = 0; i < 2u * CLN_DLEN; i++) {
        int32_t a = cln_dl[i] < 0 ? -cln_dl[i] : cln_dl[i];
        if (a > p)
            p = a;
    }
    for (i = 0; i < CLN_LA_K; i++)
        cln_pk[i] = p;
    return p;
}

CR_HOT_MASTER static __attribute__((noinline)) void clean_block(int32_t *o, uint32_t n)
{
    uint32_t w = cln_w;
#ifdef CLN_GAIN_TAP
    uint32_t tap = 0;
#endif
    while (n) {
        uint32_t m = n < CLN_LA_B ? n : CLN_LA_B, i;
        int32_t p = 0, wm, env = cln_env, g0 = cln_gs, g1;
        /* in: through the DC blocker into the delay, the block's peak; out: what went in CLN_LA samples ago */
        for (i = 0; i < m; i++) {
            int32_t l = fz_dc_step(&fx_dc[0], o[2u * i]), r = fz_dc_step(&fx_dc[1], o[2u * i + 1u]);
            int32_t al = l < 0 ? -l : l, ar = r < 0 ? -r : r;
            uint32_t rd = (w + CLN_DLEN - CLN_LA) & (CLN_DLEN - 1u);
            if (al > p)
                p = al;
            if (ar > p)
                p = ar;
            o[2u * i] = cln_dl[2u * rd];
            o[2u * i + 1u] = cln_dl[2u * rd + 1u];
            cln_dl[2u * w] = l;
            cln_dl[2u * w + 1u] = r;
            w = (w + 1u) & (CLN_DLEN - 1u);
        }
        if (p > 0x7FFFFF) {             /* (clean_gain's range; 36 dB over the threshold: no sound gets there) */
            for (i = 1; i <= m; i++) {
                uint32_t j = (w - i) & (CLN_DLEN - 1u);
                cln_dl[2u * j] = clamp(cln_dl[2u * j], -0x7FFFFF, 0x7FFFFF);
                cln_dl[2u * j + 1u] = clamp(cln_dl[2u * j + 1u], -0x7FFFFF, 0x7FFFFF);
            }
            p = 0x7FFFFF;
        }
        /* the loudest of this block and of those still in the delay */
        if (m == CLN_LA_B) {
            wm = p;
            for (i = CLN_LA_K; i-- > 0u;) {
                if (cln_pk[i] > wm)
                    wm = cln_pk[i];
                cln_pk[i] = i ? cln_pk[i - 1u] : p;
            }
        } else {
            wm = p = clean_short(p);
        }
        /* the envelope: a new peak at once, held (CLN_HOLD; after a hit CLN_HOLD_HIT), held again by every peak
         * within 1.2 dB of it, then released */
        if (wm > env) {
            if (wm - env > (env >> CLN_HIT))            /* a hit: the short hold, until the envelope has let go */
                cln_hlen = CLN_HOLD_HIT;
            env = wm;
            cln_hold = cln_hlen;
            cln_top = 0;
        } else if (p > env - (env >> CLN_REHOLD)) {
            if (cln_hold < cln_hlen)
                cln_hold = cln_hlen;
            if (p >= env - (env >> 8))                  /* the top itself again: nothing to creep to */
                cln_top = 0;
            else if (cln_top < CLN_HOLD)
                cln_top += m;
            else                                        /* (never under the delay's own peak) */
                env -= (env - (wm > CLN_T4 ? wm : CLN_T4)) >> CLN_CREEP;
        } else if (cln_hold) {
            cln_hold = cln_hold > m ? cln_hold - m : 0u;
        } else if (env > CLN_T4) {
            cln_hlen = CLN_HOLD;                        /* (letting go: from here on the long hold again) */
            env -= (((env - CLN_T4) >> CLN_REL) + 4) * (int32_t)m;
            if (env < wm)
                env = wm;
            if (env < CLN_T4)
                env = CLN_T4;
        }
        if (env != cln_env) {           /* (one division a block, and only while the envelope moves) */
            cln_env = env;
            cln_wg = env > CLN_T4 ? (int32_t)(((uint32_t)CLN_T4 << 15) / (uint32_t)env) : 32768;
        }
        g1 = g0 - cln_wh[cln_wi] + cln_wg;              /* the mean of the last CLN_LA_K gains, as their sum */
        cln_wh[cln_wi] = cln_wg;
        cln_wi = cln_wi + 1u < CLN_LA_K ? cln_wi + 1u : 0u;
        if (m != CLN_LA_B) {            /* (the slow way: no older gain above this one) */
            for (i = 0, g1 = 0; i < CLN_LA_K; i++) {
                if (cln_wh[i] > cln_wg)
                    cln_wh[i] = cln_wg;
                g1 += cln_wh[i];
            }
        }
        cln_gs = g1;
        if (g0 != CLN_G1 || g1 != CLN_G1) {             /* limiting: the gain glides from g0 to g1 over the block */
            int32_t acc = g0 * (int32_t)m, d = g1 - g0;
            for (i = 0; i < m; i++) {
                int32_t g;
                acc += d;
                g = m == CLN_LA_B ? (int32_t)((uint32_t)acc / (CLN_LA_K * CLN_LA_B))
                                  : (int32_t)((uint32_t)acc / (CLN_LA_K * m));
                o[2u * i] = clean_knee(clean_gain(o[2u * i], g));
                o[2u * i + 1u] = clean_knee(clean_gain(o[2u * i + 1u], g));
#ifdef CLN_GAIN_TAP
                cln_gtap[(tap + i) & (CTL - 1u)] = g;
#endif
            }
        }
#ifdef CLN_GAIN_TAP
        else
            for (i = 0; i < m; i++)
                cln_gtap[(tap + i) & (CTL - 1u)] = 32768;
        tap += m;
#endif
        /* (at rest nothing in the delay is over the threshold: the samples go out as they are, under the knee) */
        o += 2u * m;
        n -= m;
    }
    cln_w = w;
    cln_g = cln_gs / (int32_t)CLN_LA_K;
}
/* Clean's last stage: MASTER (eased over the block: a turned knob is a ramp, not 32-sample steps) on the block, kept
 * as the DAC's 24-bit word: out = its upper 17 bits (the Q15 sample: what USB, the scope and the click see), out_frac
 * the 7 under them (audio.c adds them to the word). q17: the block is the chain's own (Q17); else it is Q15 samples
 * with the chain's two bits in out_frac (clean_split: USB Level = Fixed, where USB's tap and the click come between) */
static inline __attribute__((always_inline)) void clean_post_loop(int32_t *out, uint32_t n, int q17)
{
    int32_t m = ((int32_t)song.master_q12 * CLN_MAKEUP >> 12) << 8, mq = cln_mq, d = (m - mq) >> CTL_LOG2;
    uint32_t i;
    for (i = 0; i < n; i++) {
        uint32_t j = (2u * i) & (2u * CTL - 1u);
        int32_t g, l = out[2u * i], r = out[2u * i + 1u];
        if (!q17) {
            l = l * 4 + out_frac[j];
            r = r * 4 + out_frac[j + 1u];
        }
        mq += d;
        g = mq >> 8;
        l = (l * g) >> 7;                               /* Q17 x Q12 -> Q22 (|l| < 2^18) */
        r = (r * g) >> 7;
        out[2u * i] = l >> 7;
        out[2u * i + 1u] = r >> 7;
        out_frac[j] = (uint8_t)(l & 127);
        out_frac[j + 1u] = (uint8_t)(r & 127);
    }
    cln_mq = m;
}
CR_HOT_MASTER static __attribute__((noinline)) void clean_post17(int32_t *out, uint32_t n) { clean_post_loop(out, n, 1); }

/* P2.4: the chain at rest and the block silent: its output is zero and its state as it was (but the delay's write
 * place, moved on). At rest: both DC blockers at 0 (their fixed point), the delay all zero, the limiter's peaks 0, its
 * envelope at the threshold with no hold, its gains whole, MASTER's ramp arrived, the DAC word's low bits 0. Then the
 * block need not run: the same samples, bit for bit (the whole chain on silence costs a look at it) */
static int out_rest(const int32_t *l, const int32_t *r, const int32_t *w, uint32_t n)
{
    uint32_t i;
    int32_t z = 0;
#ifdef CLN_GAIN_TAP
    return 0;
#endif
    if (n != CTL || w || fx_dc[0].acc | fx_dc[0].rem | fx_dc[1].acc | fx_dc[1].rem || cln_env != CLN_T4 || cln_hold ||
        cln_gs != CLN_G1 || cln_wg != 32768 || cln_mq != ((int32_t)song.master_q12 * CLN_MAKEUP >> 12) << 8)
        return 0;
    for (i = 0; i < n; i++)
        z |= l[i] | r[i];
    for (i = 0; i < 2u * CLN_DLEN; i++)
        z |= cln_dl[i];
    for (i = 0; i < CLN_LA_K; i++)
        z |= cln_pk[i] | (cln_wh[i] ^ 32768);
    for (i = 0; i < 2u * CTL; i++)
        z |= out_frac[i];
    return !z;
}

/* the mix (l, r and an optional mono return w) through the chain into out */
static inline __attribute__((always_inline)) void out_master(int32_t *out, const int32_t *l, const int32_t *r,
                                                             const int32_t *w, uint32_t n)
{
    uint32_t i;
    if (out_rest(l, r, w, n)) {                         /* (P2.4: silence at rest: zeros, nothing else moves) */
        for (i = 0; i < 2u * n; i++)
            out[i] = 0;
        cln_w = (cln_w + n) & (CLN_DLEN - 1u);
        cln_wi = (cln_wi + n / CLN_LA_B) % CLN_LA_K;
        cln_g = cln_gs / (int32_t)CLN_LA_K;
        return;
    }
    for (i = 0; i < n; i++) {                           /* (x 3 for the default trim: nothing shifted out) */
        const int32_t ww = w ? w[i] : 0;
        out[2u * i] = ((l[i] + ww) * (CLN_TRIM >> 4)) >> 6;
        out[2u * i + 1u] = ((r[i] + ww) * (CLN_TRIM >> 4)) >> 6;
    }
    clean_block(out, n);
    clean_post17(out, n);
}
/* the chain at rest, before the first block (power-on; a test's second init). (clean: kept for the callers: the one
 * chain) */
static void fx_output_set(uint32_t clean, int now)
{
    (void)clean;
    if (now) {
        clean_rest();
        cln_mq = ((int32_t)song.master_q12 * CLN_MAKEUP >> 12) << 8;
    }
}
/* MASTER at once, no ramp (before the audio starts): song.master_q12 and the chain's eased copy of it */
static uint32_t master_pot_q12;
static void fx_master_now(uint32_t q12)
{
    song.master_q12 = q12;
    master_pot_q12 = q12 + 1u;
    cln_mq = ((int32_t)q12 * CLN_MAKEUP >> 12) << 8;
}
