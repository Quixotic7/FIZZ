/* QUADRILLE: copied from ChoralRoot v0.25.1 (15ca8d0), firmware/src/fx.c, GPL-3.0-only: the parts its effects and
 * output chain need, each verbatim and in its order there: the header comment; the chorus's line (cho_buf) and the
 * fx state its chorus reads (fx.cho_w, fx.cho_ph); the master (the Classic limiter and soft clip, master_block) and
 * Options > Output's Clean chain with its look-ahead limiter (clean_block, clean_post17), out_master, fx_output_set,
 * fx_master_now. Left out: Felucca's own delay and reverb (CR_FX 0), DIST (track_dist: not in P1.1), usb_fixed_dac and
 * the USB staging, the parts' render and mix (mix_part*: core/voice/mix.c does it per voice with mix_part_loop's
 * arithmetic), the dual-core split, perform.c (ChoralRoot builds it out: FELUCCA_PERFORM 0), mix_block (qd_fx.c runs
 * the same stages in its order). Built only through qd_fx.c (CR_FX 1). Lines between the parts: "(QUADRILLE: ..)". */
/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Effects: per-track DIST insert, then sends into three
 * shared buses (chorus, tempo delay, reverb). Mono buses, stereo dry mix.
 * ChoralRoot 0.19: the chorus returns in stereo in ChoralRoot's unit (cr_fx.c xc_block; this file's one tap is
 * Felucca's, for its builds and goldens). MASTER is read before the audio starts (fx_master_now, master.c: Felucca
 * 1.1.5's fix). Options > Output's Clean chain (clean_block). Its limiter's look-ahead, one audio block with the
 * gain a straight line across the block, is the design of SLOOP 2.6's master (github.com/isod89/sloop-fm1,
 * firmware/src/fx.c lim_block; GPL-3.0-only, a fork of Felucca): the design, written anew, no code copied
 * (LICENSING.md, docs/AUDIO-QUALITY.md "The look-ahead"). */

#define CHO_LEN 2048u
static int16_t cho_buf[CHO_LEN] __attribute__((section(".pool")));
/* (QUADRILLE: CR_CHORUS_MONO and Felucca's delay and reverb buffers left out) */
static struct {
    uint32_t dly_w, cho_w, cho_ph;
    int32_t dly_lp;
    uint16_t comb_i[4], ap_i[2];
    int32_t comb_lp[4];
    uint8_t rtype;                       /* the reverb model running (G_RTYPE: 0 ROOM, 1 SPRING) */
    uint16_t sp_w;                       /* SPRING: the loop's write index (SP_MASK) */
    int32_t sp_lp, sp_hp, sp_he, sp_size;   /* .. its loop low-pass, low cut (and its remainder), the loop
                                             * length (Q8, glides) */
    uint32_t sp_ph;                      /* .. the output tap's wobble */
    uint16_t dly_dl, dly_old, dly_xf;    /* DELAY: the tap in use, the one it fades from, samples left of the fade */
} fx;
/* (QUADRILLE: DLY_XF, SPRING, DIST left out) */

/* master: peak limiter in front of the soft clipper. Fast attack (~0.1 ms),
 * ~150 ms release, threshold where tanh is still nearly linear, so chords
 * get quieter instead of crushed. */
#define LIM_T 18000
static int32_t lim_env = LIM_T;
/* ChoralRoot (cr_out.c sets it; 0 in Felucca, its golden renders and reverb_test's bit-exact buses): the delay's
 * new times crossfade (DLY_XF), and the limiter is smoothed. A 6-note chord with the bass keeps the limiter
 * working (pre-limiter peaks ~2x LIM_T on ANALOG / WHEEL sounds, 60 % of the samples limited), and its 4-sample
 * attack re-attacking on every new peak of the beating chord modulated the gain at audio rate: -49 dB of
 * sidebands above 1 kHz, a crackle on held chords, louder with the FX sends. Smoothed (lim_g): -57 dB, the same
 * loudness; the peaks the slower gain lets through (to ~25000) go into the soft clipper's near-linear part */
static uint8_t fx_smooth;
#if CR_FX
static volatile uint8_t fx_off;       /* ChoralRoot's FX tap at off (cr_ui.c cu_fx_apply): no part sends into the buses */
static int32_t fx_sg[NPART][3];       /* each part's send gains as the last block left them (Q15: chorus, delay, reverb):
                                       * a new send, FX on / off, a CC is reached as a ramp over the block, not as a step
                                       * in what the delay and the reverb are fed (a click at every detent of Send) */
#endif
static int32_t lim_g = 32768;
/* Options > USB Level = Fixed (Felucca 1.0.5's MENU > USB LEVEL FIXED, #42: record over USB with the speaker turned
 * down): the mix goes to master_out at the full MASTER level, USB audio's master pair takes that (choralroot.c's
 * mix_block shim, usb_audio_stream.c), and only then does MASTER scale what the DAC gets (usb_fixed_dac, audio.c).
 * 0 (Master, the default): MASTER before master_out, as always (USB follows the knob) */
static volatile uint8_t fx_usb_fixed;
#define MASTER_FULL 4096                /* main.c: the MASTER knob's top, Q12 */
static uint8_t fx_out_blk;              /* Options > Output: 1, this block went through the Clean chain (below) */
static void clean_post(int32_t *out, uint32_t n);
/* (QUADRILLE: usb_fixed_dac left out) */
static volatile uint8_t fx_lowcut;     /* settings: 1 LOWCUT 12 dB/oct ~110 Hz, 2 BASS+ (the small speaker):
                                        * 12 dB/oct ~220 Hz plus the harmonics of the bass (spk_bass) */
static int32_t lc_l1, lc_l2, lc_r1, lc_r2, dc_l, dc_r, dce_l, dce_r;

/* DC blocker (~2 Hz), always on: a leaky integrator of the input (Q6 state) subtracted from it.
 * The >> 12 step keeps its remainder (error feedback, 0..4095) and adds it to the next one, so no
 * part of the step is lost: the state follows the input exactly, down to 0 after the sound stops.
 * (A rounded step of (x - dc) / 4096 would stop moving at |x - dc| < 2048 and leave an offset of up
 * to +-31 at the output after silence.) */
static inline int32_t dc_block(int32_t x, int32_t *dc, int32_t *err)
{
    int32_t e = (x << 6) - *dc + *err, d = e >> 12;
    *err = e - (d << 12);
    *dc += d;
    return x - ((*dc + 32) >> 6);
}

static int32_t lce[4];
static inline int32_t lowcut1(int32_t x, int32_t *lc, int32_t *err, uint32_t sh)   /* x minus its one-pole low-pass */
{
    int32_t e = x - *lc + *err, d = e >> sh;
    *err = e - (d << sh);
    *lc += d;
    return x - *lc;
}

/* BASS+: what the speaker cannot play, heard through its harmonics. The bass below ~150 Hz is clipped at its
 * own envelope (a level-following trapezoid: odd harmonics), then band-passed ~220 Hz..1 kHz and added.
 * The low-pass has 4 poles (Felucca 1.0.5, #42: with 2, the trapezoid rebuilt the 300 .. 600 Hz of the mix itself,
 * late, and cancelled up to 6 dB of it; now under 0.5 dB) */
static int32_t sb_lp1, sb_lp2, sb_lp3, sb_lp4, sb_env, sb_h1, sb_h2, sb_hl;
enum { SB_LP1, SB_LP2, SB_LP3, SB_LP4, SB_ENV, SB_H1, SB_H2, SB_HL, SB_N };   /* (the states above, in a block's copy) */
static inline int32_t spk_bass(int32_t *sb, int32_t m)
{
    int32_t a, t, u;
    sb[SB_LP1] += ((m - sb[SB_LP1]) * 692) >> 15;
    sb[SB_LP2] += ((sb[SB_LP1] - sb[SB_LP2]) * 692) >> 15;
    sb[SB_LP3] += ((sb[SB_LP2] - sb[SB_LP3]) * 692) >> 15;
    sb[SB_LP4] += ((sb[SB_LP3] - sb[SB_LP4]) * 692) >> 15;
    a = sb[SB_LP4] < 0 ? -sb[SB_LP4] : sb[SB_LP4];
    if (a > sb[SB_ENV])
        sb[SB_ENV] += (a - sb[SB_ENV]) >> 2;
    else if (sb[SB_ENV] > 0)
        sb[SB_ENV] -= (sb[SB_ENV] >> 11) + 1;
    t = clamp(sb[SB_LP4] * 8, -sb[SB_ENV], sb[SB_ENV]);
    sb[SB_H1] += (t - sb[SB_H1]) >> 5;
    u = t - sb[SB_H1];
    sb[SB_H2] += (u - sb[SB_H2]) >> 5;
    u -= sb[SB_H2];
    sb[SB_HL] += (u - sb[SB_HL]) >> 3;
    return sb[SB_HL] * 3;
}

/* The master over a block, in place on its frames (o: left, right, ...): the DC blocker, LOWCUT / BASS+, the limiter,
 * the soft clip. The states are the globals above, held in locals over the block (a call a sample loaded and stored
 * them every sample); lowcut and smooth are constants where a loop is built for them (master_block), so a sample has
 * no test of the settings. */
static inline __attribute__((always_inline)) void master_loop(int32_t *o, uint32_t n, uint32_t lowcut, int smooth)
{
    int32_t dl = dc_l, el = dce_l, dr = dc_r, er = dce_r, env = lim_env, lg = lim_g;
    int32_t l1 = lc_l1, l2 = lc_l2, r1 = lc_r1, r2 = lc_r2, e0 = lce[0], e1 = lce[1], e2 = lce[2], e3 = lce[3];
    int32_t sb[SB_N] = {sb_lp1, sb_lp2, sb_lp3, sb_lp4, sb_env, sb_h1, sb_h2, sb_hl};
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t l = o[2u * i], r = o[2u * i + 1u], al, ar, a;
        l = dc_block(l, &dl, &el);
        r = dc_block(r, &dr, &er);
        if (lowcut) {                 /* two one-pole high-passes, error feedback as dc_block (the */
            uint32_t sh = lowcut == 2u ? 5u : 6u;       /* rounded step stopped at |x - lc| < 32: an offset) */
            int32_t b = lowcut == 2u ? spk_bass(sb, (l + r) >> 1) : 0;
            l = lowcut1(l, &l1, &e0, sh);
            l = lowcut1(l, &l2, &e1, sh) + b;
            r = lowcut1(r, &r1, &e2, sh);
            r = lowcut1(r, &r2, &e3, sh) + b;
        }
        al = l < 0 ? -l : l;
        ar = r < 0 ? -r : r;
        a = al > ar ? al : ar;
        if (smooth) {                /* ChoralRoot: the peak held at once, the gain eased down over ~1.5 ms */
            int32_t gt;
            if (a > env)
                env = a;
            else if (env > LIM_T)
                env -= ((env - LIM_T) >> 12) + 1;
            gt = env > LIM_T ? (int32_t)(((uint32_t)LIM_T << 15) / (uint32_t)env) : 32768;
            if (gt < lg)
                lg += (gt - lg) >> 6;
            else
                lg = gt;
            if (lg < 32768) {
                l = ((l >> 4) * lg) >> 11;
                r = ((r >> 4) * lg) >> 11;
            }
        } else {
            if (a > env)
                env += (a - env) >> 2;
            else if (env > LIM_T)
                env -= ((env - LIM_T) >> 12) + 1;
            if (env > LIM_T) {
                int32_t g = (int32_t)(((uint32_t)LIM_T << 15) / (uint32_t)env);   /* < 32768 */
                l = ((l >> 4) * g) >> 11;                    /* >> 4 first: |l| may be far above Q15 */
                r = ((r >> 4) * g) >> 11;
            }
        }
        o[2u * i] = softclip(l);
        o[2u * i + 1u] = softclip(r);
    }
    dc_l = dl;
    dce_l = el;
    dc_r = dr;
    dce_r = er;
    lim_env = env;
    lim_g = lg;
    if (lowcut) {
        lc_l1 = l1;
        lc_l2 = l2;
        lc_r1 = r1;
        lc_r2 = r2;
        lce[0] = e0;
        lce[1] = e1;
        lce[2] = e2;
        lce[3] = e3;
    }
    if (lowcut == 2u) {
        sb_lp1 = sb[SB_LP1];
        sb_lp2 = sb[SB_LP2];
        sb_lp3 = sb[SB_LP3];
        sb_lp4 = sb[SB_LP4];
        sb_env = sb[SB_ENV];
        sb_h1 = sb[SB_H1];
        sb_h2 = sb[SB_H2];
        sb_hl = sb[SB_HL];
    }
}
CR_HOT_MASTER static __attribute__((noinline)) void master_block(int32_t *o, uint32_t n)
{
    uint32_t lowcut = fx_lowcut;                        /* (the main loop's: it does not run under this block) */
    if (fx_smooth) {
        if (!lowcut)
            master_loop(o, n, 0u, 1);
        else if (lowcut == 2u)
            master_loop(o, n, 2u, 1);
        else
            master_loop(o, n, 1u, 1);
    } else {
        if (!lowcut)
            master_loop(o, n, 0u, 0);
        else if (lowcut == 2u)
            master_loop(o, n, 2u, 0);
        else
            master_loop(o, n, 1u, 0);
    }
}

/* ------------------------------------------------------------------------------------------------------------
 * Options > Output: Classic / Clean (ChoralRoot 0.19, docs/AUDIO-QUALITY.md; GitHub issue 3).
 * Classic is the master above, untouched: 0.18's sound bit for bit. Clean is another chain on the same mix:
 *   the mix at a FIXED gain (CLN_TRIM: MASTER is not in front of anything any more), with the two bits the Classic
 *   line shifts out kept (the chain's samples are Q17: a quarter of a Q15 unit) -> the same DC blocker -> a limiter
 *   that is a safety, not a part of the sound (threshold CLN_T instead of LIM_T: the knee below leaves it the room;
 *   the peak HELD for CLN_HOLD samples and released slowly, so a bass note's own period no longer moves the gain;
 *   the gain applied to all the sample's bits) -> a knee that is a straight line up to CLN_K and a tanh only above it
 *   (to full scale) -> MASTER, last, into the DAC's 24-bit word (out_frac: the 7 bits under the Q15 sample, which
 *   Classic leaves at 0): the knob turns the level down and nothing else, a quiet setting keeps its resolution, and
 *   the limiter's work no longer depends on it.
 * No LOWCUT / BASS+ in it: ChoralRoot has no control for them and holds them off (cr_settings.c cr_settings_boot).
 * fx_out_want is what Options asks for (the main loop writes it), fx_out_mode what runs: a change is taken at a
 * block's start, the mix faded to nothing over OUT_FADE_BLK blocks, the chain changed at that zero with its limiter
 * at rest, and faded in again: no step in the sound. Felucca's builds never ask: Classic. */
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
#define OUT_FADE_BLK 16u                /* 512 samples, 11.6 ms, each way */
#define OUT_FADE_FLUSH ((CLN_LA + CTL - 1u) / CTL)     /* silent blocks between: the delay runs out */
_Static_assert(OUT_FADE_BLK * CTL == 512u, "the fade's step is 4096 / 512");
_Static_assert(!(CLN_TRIM & 15) && CLN_T <= CLN_K && CLN_K <= 30000 && CLN_MAKEUP <= 8192, "the Clean chain's ranges");
_Static_assert(CLN_LA_K >= 1u && CLN_LA_K <= 8u && CLN_LA <= 128u && CTL % CLN_LA_B == 0u, "the look-ahead's blocks");
static volatile uint8_t fx_out_want;
static uint8_t fx_out_mode, fx_out_fade;
static int32_t cln_dl[2u * CLN_DLEN];   /* the delay: the chain's samples (Q17, after the DC blocker), L R. In RAM
                                         * (.bss, 256 bytes): the ISR reads and writes it every sample; the pool is
                                         * the tight resource (docs/FX.md "The cost on the unit") */
static uint32_t cln_w, cln_hold, cln_hlen = CLN_HOLD, cln_top;
static int32_t cln_pk[CLN_LA_K];        /* the peaks of the blocks in the delay, the newest first */
static int32_t cln_wh[CLN_LA_K];        /* their gains (Q15), a ring; cln_gs is their sum */
static uint32_t cln_wi;
static int32_t cln_env = CLN_T4, cln_wg = 32768, cln_gs = CLN_G1, cln_g = 32768, cln_mq;
static uint8_t out_frac[2u * CTL];      /* Clean: the bits under each of the block's Q15 samples: the DAC word's 7
                                         * (audio.c), or between clean_block and a later clean_post the chain's 2 */
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
    int32_t dl = dc_l, el = dce_l, dr = dc_r, er = dce_r;
    uint32_t w = cln_w;
#ifdef CLN_GAIN_TAP
    uint32_t tap = 0;
#endif
    while (n) {
        uint32_t m = n < CLN_LA_B ? n : CLN_LA_B, i;
        int32_t p = 0, wm, env = cln_env, g0 = cln_gs, g1;
        /* in: through the DC blocker into the delay, the block's peak; out: what went in CLN_LA samples ago */
        for (i = 0; i < m; i++) {
            int32_t l = dc_block(o[2u * i], &dl, &el), r = dc_block(o[2u * i + 1u], &dr, &er);
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
    dc_l = dl;
    dce_l = el;
    dc_r = dr;
    dce_r = er;
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
CR_HOT_MASTER static __attribute__((noinline)) void clean_post(int32_t *out, uint32_t n) { clean_post_loop(out, n, 0); }
CR_HOT_MASTER static __attribute__((noinline)) void clean_post17(int32_t *out, uint32_t n) { clean_post_loop(out, n, 1); }
CR_HOT_MASTER static __attribute__((noinline)) void clean_split(int32_t *out, uint32_t n)   /* Q17 -> Q15 and the two bits under */
{
    uint32_t i;
    for (i = 0; i < 2u * n; i++) {
        out_frac[i & (2u * CTL - 1u)] = (uint8_t)(out[i] & 3);
        out[i] >>= 2;
    }
}
/* Options changed Output: this block of the chain's input (o, n = CTL) faded, and the chain changed at the zero: the
 * mix down to nothing over OUT_FADE_BLK blocks, then OUT_FADE_FLUSH silent blocks through the old chain (Clean's
 * delay plays its last samples out: nothing of the fade's end is cut off), the other chain from rest with an empty
 * delay, and up again */
static __attribute__((noinline)) void out_fade(int32_t *o, uint32_t n)
{
    uint32_t i, k;
    if (!fx_out_fade)
        fx_out_fade = 1;
    k = fx_out_fade - 1u;
    for (i = 0; i < n; i++) {
        int32_t g = k < OUT_FADE_BLK ? 4096 - 8 * (int32_t)(k * CTL + i + 1u)
                  : k < OUT_FADE_BLK + OUT_FADE_FLUSH ? 0
                  : 8 * (int32_t)((k - OUT_FADE_BLK - OUT_FADE_FLUSH) * CTL + i + 1u);
        g = clamp(g, 0, 4096);
        o[2u * i] = ((clamp(o[2u * i], -0x7FFFFF, 0x7FFFFF) >> 4) * g) >> 8;
        o[2u * i + 1u] = ((clamp(o[2u * i + 1u], -0x7FFFFF, 0x7FFFFF) >> 4) * g) >> 8;
    }
    if (fx_out_fade == OUT_FADE_BLK + OUT_FADE_FLUSH)   /* silent now: the other chain from the next block on */
        fx_out_mode = !fx_out_mode;                     /* (out_master puts both at rest after this one) */
    fx_out_fade = fx_out_fade == 2u * OUT_FADE_BLK + OUT_FADE_FLUSH ? 0u : (uint8_t)(fx_out_fade + 1u);
}
/* the mix (l, r, and Felucca's mono wet w: its chorus, delay and reverb; 0 where the mix already holds every return,
 * ChoralRoot's unit: mix_block) through its chain into out: fx_out_mode's, read once for the block */
static inline __attribute__((always_inline)) void out_master(int32_t *out, const int32_t *l, const int32_t *r,
                                                             const int32_t *w, uint32_t n)
{
    uint32_t i, mode = fx_out_mode;
    if (mode) {
        for (i = 0; i < n; i++) {                       /* (x 3 for the default trim: nothing shifted out) */
            int32_t ww = w ? w[i] : 0;
            out[2u * i] = ((l[i] + ww) * (CLN_TRIM >> 4)) >> 6;
            out[2u * i + 1u] = ((r[i] + ww) * (CLN_TRIM >> 4)) >> 6;
        }
    } else {
        int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)song.master_q12;   /* (USB Level Fixed: MASTER after) */
        for (i = 0; i < n; i++) {
            int32_t ww = w ? w[i] : 0;
            out[2u * i] = (((l[i] + ww) >> 2) * mg) >> 10;
            out[2u * i + 1u] = (((r[i] + ww) >> 2) * mg) >> 10;
        }
    }
    if (fx_out_fade || fx_out_want != mode)
        out_fade(out, n);
    fx_out_blk = (uint8_t)mode;
    if (!mode) {
        master_block(out, n);
    } else {
        clean_block(out, n);
        if (fx_usb_fixed)
            clean_split(out, n);
        else
            clean_post17(out, n);
    }
    if (fx_out_mode != mode) {                          /* the fade's zero: this was the old chain's last block. Both
                                                         * limiters at rest and Clean's delay empty for the new one */
        lim_env = LIM_T;
        lim_g = 32768;
        clean_rest();
        cln_mq = ((int32_t)song.master_q12 * CLN_MAKEUP >> 12) << 8;
    }
}
/* Options > Output (cr_ui.c) and the settings at boot (now: before the first block, no fade) */
static uint8_t fx_out_pin;              /* the emulator's EMU_OUTPUT=classic (the proof that Classic is 0.18's, bit for
                                         * bit, on scripts whose settings say Clean): Classic whatever is asked */
static void fx_output_set(uint32_t clean, int now)
{
    if (fx_out_pin)
        clean = 0;
    fx_out_want = (uint8_t)(clean != 0u);
    if (now) {
        fx_out_mode = fx_out_want;
        fx_out_fade = 0;
        clean_rest();
        cln_mq = ((int32_t)song.master_q12 * CLN_MAKEUP >> 12) << 8;
    }
}
/* MASTER at once, no ramp (master.c master_boot: the pot as it stands before the audio starts, 0.19; Felucca 1.1.5's
 * fix for its #137): song.master_q12, and Output Clean's eased copy of it, which would glide from where it was over
 * the first block. master_pot_q12: the pot's level + 1 as master.c last read it (0: never read: a host test, a unit
 * whose ADC did not answer); cr_out.c cr_out_init goes back to it after the UI's power-on defaults */
static uint32_t master_pot_q12;
static void fx_master_now(uint32_t q12)
{
    song.master_q12 = q12;
    master_pot_q12 = q12 + 1u;
    cln_mq = ((int32_t)q12 * CLN_MAKEUP >> 12) << 8;
}

/* one frame through the master (tests/speaker_test.c: its response sample by sample) */
static inline void master_out(int32_t *l, int32_t *r)
{
    int32_t o[2];
    o[0] = *l;
    o[1] = *r;
    master_block(o, 1u);
    *l = o[0];
    *r = o[1];
}
