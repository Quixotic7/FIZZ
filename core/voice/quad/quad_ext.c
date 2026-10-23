/* SPDX-License-Identifier: GPL-3.0-only */
/* QUADRILLE (e): FM TONE's extensions beside ChoralRoot's code (research/FMTONE-DN2-PLAN.md), FIZZ only. Built only
 * through quad_voice.c, after eng_quad.c (it reads quad_voice_t, the time tables); eng_quad.c calls these at its
 * marked hook points ("QUADRILLE (e)"), each behind its feature's test: with every new value at its default the voice
 * runs exactly ChoralRoot's code (test_quad_bitexact). Integer, as the voice is.
 *
 *   E1  AHD: the amp envelope's attack, a HOLD (a time on the amp attack's table, or NOTE: while the key is down), a
 *       decay to 0 (then the voice ends); SUS and REL unread, the gate unread in a timed HOLD (one-shot)
 *       DRIVE before the filter (QD_QP_DRPRE): ChoralRoot's DIST (qd_quad_dist) on the voice before its multimode
 *       filter, its state the voice's (qd_quad_voice_t.dpre); the mix's after the voice then skips it
 *       portamento: the glide's RATE table (quad_voice.c glides pitch_cur toward pitch16 once a block)
 *   E2  the filter machines (QD_QP_FTYPE 4..7, quad_ext_machine): MULTI (the SVF's LP, BP, HP morphed by FSHAPE),
 *       COMB+ / COMB- (a 1024-sample line a voice, FREQ its pitch, RESO its feedback, FSHAPE its damping), EQ (a
 *       peaking bell from the SVF: FREQ its centre, FSHAPE its Q, FGAIN -18 .. +18 dB); BWPOST: the base-width
 *       filter after the machine (quad_ext_bw_post)
 *   E3  CRUSH (quad_ext_crush): SRR a sample and hold (its rate 1 / N of the sample rate, N 1..64 on a log table), BITS
 *       the sample quantized to b bits of +-32768 (b 16 .. 1: 2^b steps across, 0 kept); CRPOST 0 before the filter, 1
 *       after; both OFF: none
 *   E4  the modulation slots (quad_ext_mods): eight SOURCE -> DEST x DEPTH into the LFOs' dst[] (their destinations,
 *       FIZZ's new ones too: HOLD, PTIM, SHAPE, GAIN, BITS, SRR) */

/* E1: RATE's step a block (1/16 semitone) by PTIM: ChoralRoot's (voice.c, its slide: ENV_LIN[v] >> 14, at least 1;
 * tools/gen_tables.py ENV_LIN = 2^24 CTL / (ms(v) FS / 1000), ms(v) = 10000^(v / 127)) */
static const uint16_t QE_GLRATE[128] = {
    743, 691, 642, 597, 555, 517, 480, 447, 415, 386, 359, 334, 311, 289, 269, 250, 232, 216, 201, 187, 174, 162, 150,
    140, 130, 121, 112, 104, 97, 90, 84, 78, 72, 67, 63, 58, 54, 50, 47, 43, 40, 37, 35, 32, 30, 28, 26, 24, 22, 21, 19,
    18, 17, 15, 14, 13, 12, 11, 11, 10, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1};

/* E1, hook 1: the amp envelope in AHD (stage 2 the attack, 6 the hold, 7 the decay; ADSR's stages met at a switch
 * mid-note: its decay / sustain is the hold, its release the decay). The hold's progress is dly[3] (the amp has no
 * DELAY: quad_note_on starts it with none). Q15, as quad_env_tick */
static int32_t quad_ext_ahd(quad_voice_t *s, int32_t atk, int32_t hold, int32_t dec, int gate)
{
    int32_t x = s->env[3];
    if (s->stage[3] == 3u || s->stage[3] == 1u) {
        s->stage[3] = s->stage[3] == 1u ? 2u : 6u;
        s->dly[3] = 0;
    } else if (s->stage[3] == 4u || s->stage[3] == 5u) {
        s->stage[3] = 7u;
    }
    switch (s->stage[3]) {
    case 2:                                     /* ATK: linear to 1 on the amp attack's table, as ADSR's */
        x += (int32_t)QUAD_T_AATK[atk & 127];
        if (x >= (1 << 24)) {
            x = 1 << 24;
            s->stage[3] = 6;
            s->dly[3] = 0;
        }
        break;
    case 6:                                     /* HOLD: NOTE until the key is up; else its time, the gate unread */
        if (hold >= 127) {
            if (!gate)
                s->stage[3] = 7;
        } else {
            s->dly[3] += QUAD_T_AATK[hold & 127];
            if (s->dly[3] >= (1u << 24))
                s->stage[3] = 7;
        }
        break;
    case 7:                                     /* DEC: exponential to 0 on the amp decay's table (127 INF: held) */
        if (dec >= 127)
            break;
        x -= (int32_t)(((int64_t)x * QUAD_T_ADEC[dec & 127] + 0xFFFFFF) >> 24);
        if (x < (1 << 12)) {
            x = 0;
            s->stage[3] = 0;
        }
        break;
    default:
        x = 0;
        break;
    }
    s->env[3] = x;
    return x >> 9;
}

/* E1: back to ADSR from an AHD stage (a switch mid-note): the hold is ADSR's decay / sustain, the decay its release */
static void quad_ext_adsr_back(quad_voice_t *s)
{
    if (s->stage[3] == 6u)
        s->stage[3] = 3;
    else if (s->stage[3] == 7u)
        s->stage[3] = 4;
}

/* E1, hook 2: DRIVE before the filter: ChoralRoot's DIST (the mix's, with the LFOs' DRIVE offset) on the voice's
 * signal before the multimode filter, its state the voice's */
static void quad_ext_drive_pre(track_t *t, voice_t *v, int32_t *b, uint32_t n)
{
    int32_t dr = v->patch[QD_QP_DRIVE];
    if (t->quad_fx_off[0])
        dr = clamp(dr + t->quad_fx_off[0], 0, 127);
    qd_quad_dist((qd_dist_t *)(void *)&v->dpre, dr, b, (int)n);
}

/* ================================================================ E2: the filter machines */
/* the comb's length (Q8 samples) at FREQ's cut (cut >> 8: ChoralRoot's cutoff law, 30 Hz .. 16 kHz), at most 1022:
 * the lowest pitch 43 Hz; the EQ's damping k = 1 / Q (Q12, Q 0.5 .. 10 by FSHAPE), its gain 10^(dB / 20) - 1 (Q12,
 * FGAIN + 64: -18 .. +17.7 dB) (tools: research/FMTONE-DN2-PLAN.md §4.3) */
static const uint32_t QE_COMB_LEN[128] = {
    261632, 261632, 261632, 261632, 261632, 261632, 261632, 261632, 253383, 241160, 229527, 218454,
    207916, 197886, 188340, 179255, 170608, 162378, 154544, 147089, 139994, 133240, 126813, 120696,
    114873, 109332, 104058, 99038, 94260, 89713, 85386, 81267, 77346, 73615, 70064, 66684,
    63467, 60406, 57492, 54718, 52079, 49566, 47175, 44900, 42734, 40672, 38710, 36843,
    35066, 33374, 31764, 30232, 28773, 27385, 26064, 24807, 23610, 22471, 21387, 20356,
    19374, 18439, 17550, 16703, 15897, 15130, 14400, 13706, 13045, 12415, 11816, 11246,
    10704, 10188, 9696, 9228, 8783, 8359, 7956, 7572, 7207, 6859, 6529, 6214,
    5914, 5629, 5357, 5099, 4853, 4619, 4396, 4184, 3982, 3790, 3607, 3433,
    3267, 3110, 2960, 2817, 2681, 2552, 2429, 2312, 2200, 2094, 1993, 1897,
    1805, 1718, 1635, 1556, 1481, 1410, 1342, 1277, 1216, 1157, 1101, 1048,
    997, 949, 903, 860, 818, 779, 741, 706,
};
static const uint16_t QE_EQ_K[128] = {
    8192, 8001, 7815, 7632, 7454, 7281, 7111, 6945, 6783, 6625, 6471, 6320,
    6172, 6029, 5888, 5751, 5617, 5486, 5358, 5233, 5111, 4992, 4875, 4762,
    4651, 4542, 4436, 4333, 4232, 4133, 4037, 3943, 3851, 3761, 3674, 3588,
    3504, 3423, 3343, 3265, 3189, 3114, 3042, 2971, 2902, 2834, 2768, 2703,
    2640, 2579, 2519, 2460, 2403, 2347, 2292, 2238, 2186, 2135, 2086, 2037,
    1989, 1943, 1898, 1854, 1810, 1768, 1727, 1687, 1647, 1609, 1571, 1535,
    1499, 1464, 1430, 1397, 1364, 1332, 1301, 1271, 1241, 1212, 1184, 1156,
    1129, 1103, 1077, 1052, 1028, 1004, 980, 958, 935, 913, 892, 871,
    851, 831, 812, 793, 774, 756, 739, 721, 705, 688, 672, 657,
    641, 626, 612, 597, 583, 570, 557, 544, 531, 519, 506, 495,
    483, 472, 461, 450, 440, 429, 419, 410,
};
static const int16_t QE_EQ_G[128] = {
    -3580, -3563, -3546, -3528, -3509, -3490, -3470, -3449, -3428, -3406, -3383, -3360,
    -3335, -3310, -3285, -3258, -3230, -3202, -3172, -3142, -3111, -3078, -3045, -3010,
    -2974, -2937, -2899, -2860, -2819, -2777, -2734, -2689, -2643, -2595, -2545, -2494,
    -2442, -2387, -2331, -2273, -2213, -2151, -2087, -2021, -1953, -1882, -1809, -1734,
    -1656, -1576, -1493, -1407, -1319, -1227, -1133, -1035, -935, -831, -723, -612,
    -498, -379, -257, -131, 0, 135, 274, 418, 566, 720, 878, 1042,
    1211, 1386, 1566, 1753, 1945, 2144, 2349, 2561, 2780, 3007, 3240, 3482,
    3731, 3989, 4255, 4530, 4814, 5107, 5410, 5723, 6046, 6379, 6724, 7080,
    7448, 7828, 8220, 8626, 9044, 9477, 9924, 10385, 10862, 11354, 11862, 12387,
    12930, 13490, 14069, 14667, 15284, 15922, 16581, 17261, 17964, 18690, 19440, 20215,
    21015, 21841, 22695, 23576, 24487, 25428, 26399, 27403,
};
#include "../../qd_config.h"             /* (P2.0: QE_NCOMB, the comb arena: a line for every voice) */
#define QE_COMB_N 1024
static int16_t qe_comb[QE_NCOMB][QE_COMB_N];
/* P2.1c: the lines are lent: a voice starting a comb (fresh, or switched to one) takes its own place's line (idx %
 * QE_NCOMB) if it is free, else the first free one; it keeps it while it runs a comb and sounds. None free (the
 * device's 8 for its 16 voices): that voice runs MULTI until its next start (the owner's rule). The host's 64: every
 * voice its own place's, as before */
static voice_t *qe_comb_by[QE_NCOMB];
static int qe_is_comb(int mach) { return mach == QD_QF_COMBP || mach == QD_QF_COMBN; }
static int qe_comb_taken(int k)
{
    const voice_t *o = qe_comb_by[k];
    return o && o->active && o->comb.line == k + 1 && qe_is_comb(o->comb.mach);
}
static void qe_comb_claim(voice_t *v, int idx)
{
    int k;
    if (v->comb.line && qe_comb_by[v->comb.line - 1] == v)
        return;                                         /* (its own still) */
    v->comb.line = 0;
    for (k = 0; k < QE_NCOMB; k++) {
        const int at = (idx + k) % QE_NCOMB;
        if (!qe_comb_taken(at)) {
            qe_comb_by[at] = v;
            v->comb.line = (uint8_t)(at + 1);
            return;
        }
    }
}
static void qe_comb_free(voice_t *v)
{
    if (v->comb.line && qe_comb_by[v->comb.line - 1] == v)
        qe_comb_by[v->comb.line - 1] = 0;
    v->comb.line = 0;
}

static int32_t qe_comb_len(int32_t cut)
{
    int32_t i;
    cut = clamp(cut, 0, 127 << 8);
    i = cut >> 8;
    if (i >= 127)
        return (int32_t)QE_COMB_LEN[127];
    return (int32_t)QE_COMB_LEN[i] + (((int32_t)QE_COMB_LEN[i + 1] - (int32_t)QE_COMB_LEN[i]) * (cut & 255)) / 256;
}

/* hook 3: machines 4..7 in the place of the multimode SVF (its coefficients c by FREQ and RESO's damping kd), its
 * states ic1 / ic2 (MULTI, EQ), the soft knee after it as the SVF's */
static void quad_ext_machine(voice_t *v, quad_voice_t *s, const int16_t *p, int32_t *acc, uint32_t n,
                             const quad_svf_t *c, int32_t kd, int32_t cut, int32_t reso, int32_t shape, int32_t gain,
                             int32_t *ic1, int32_t *ic2)
{
    const int mach = p[QD_QP_FTYPE];
    int run;
    uint32_t i;
    if (v->comb.fresh || v->comb.mach != mach) {        /* (a fresh voice, or a machine switched: from rest; the dip
                                                         * makes it silent meanwhile) */
        if (qe_is_comb(mach)) {
            qe_comb_claim(v, (int)s->idx);              /* (P2.1c: a line lent; none: MULTI) */
            if (v->comb.line)
                memset(qe_comb[v->comb.line - 1], 0, sizeof qe_comb[0]);
        } else {
            qe_comb_free(v);
        }
        v->comb.w = 0, v->comb.lp = 0, v->comb.len = qe_comb_len(cut);
        if (v->comb.mach != mach && !v->comb.fresh)
            *ic1 = 0, *ic2 = acc[0];
        v->comb.mach = (uint8_t)mach, v->comb.fresh = 0;
    }
    run = qe_is_comb(mach) && !v->comb.line ? QD_QF_MULTI : mach;   /* (P2.1c: no line: MULTI) */
    if (run == QD_QF_MULTI || run == QD_QF_EQ) {
        quad_svf_t ce;
        const quad_svf_t *cc = c;
        int32_t a = *ic1, b = *ic2, k = kd, g = 0, wm = 0;
        if (run == QD_QF_EQ) {                         /* the bell: its own damping (FSHAPE: Q), FGAIN its height */
            k = QE_EQ_K[shape];
            quad_svf_coef(&ce, cut, k);
            cc = &ce;
            g = QE_EQ_G[clamp(gain + 64, 0, 127)];
        } else {
            wm = shape <= 64 ? shape * 512 : (shape - 64) * 32768 / 63;   /* (Q15: LP -> BP, then BP -> HP) */
        }
        for (i = 0; i < n; i++) {
            int32_t v1, v2, v3, x = acc[i], y;
            v3 = x - b;
            v1 = (cc->a1 * a + cc->a2 * v3 + ((cc->f2 * v3) >> 5) + 4096) >> 13;
            v2 = b + ((cc->a2 * a + cc->a3 * v3 + ((cc->f2 * a) >> 5) + 4096) >> 13);
            a = clamp(2 * v1 - a, -150000, 150000);
            b = clamp(2 * v2 - b, -150000, 150000);
            if (run == QD_QF_EQ) {
                y = x + (int32_t)(((int64_t)g * ((k * v1) >> 12)) >> 12);
            } else {
                const int32_t bp = (kd * v1) >> 12, hp = x - bp - v2;
                y = shape == 0 ? v2 : shape == 127 ? hp
                  : shape <= 64 ? v2 + (int32_t)(((int64_t)(bp - v2) * wm) >> 15)
                                : bp + (int32_t)(((int64_t)(hp - bp) * wm) >> 15);
            }
            acc[i] = soft_knee(clamp(y, -200000, 200000), 16000);
        }
        *ic1 = a, *ic2 = b;
        return;
    }
    {   /* COMB+ / COMB-: y = x +- g lp(d[n - L]) into the line; out y (1 - g / 2) */
        int16_t *line = qe_comb[v->comb.line - 1];
        const int32_t g = (int32_t)(((int64_t)clamp(reso, 0, 12700) * 32276) / 12700);   /* (0 .. 0.985, Q15) */
        const int32_t ka = 1024 + shape * 31744 / 127, l0 = v->comb.len, l1 = qe_comb_len(cut);
        const int32_t outk = 32768 - g / 2;
        const int neg = mach == QD_QF_COMBN;
        int32_t lp = v->comb.lp;
        uint32_t w = v->comb.w;
        for (i = 0; i < n; i++) {
            const int32_t len = l0 + (int32_t)(((int64_t)(l1 - l0) * (int32_t)i) / (int32_t)n);
            const int32_t di = len >> 8, fr = len & 255;
            const int32_t a0 = line[(w - (uint32_t)di) & (QE_COMB_N - 1)];
            const int32_t a1 = line[(w - (uint32_t)di - 1u) & (QE_COMB_N - 1)];
            const int32_t d = a0 + (((a1 - a0) * fr) >> 8);
            int32_t y;
            lp += (int32_t)(((int64_t)(d - lp) * ka) >> 15);
            y = acc[i] + (neg ? -1 : 1) * (int32_t)(((int64_t)g * lp) >> 15);
            line[w & (QE_COMB_N - 1)] = (int16_t)clamp(y, -32768, 32767);
            w++;
            acc[i] = soft_knee(clamp((int32_t)(((int64_t)y * outk) >> 15), -200000, 200000), 16000);
        }
        v->comb.lp = lp, v->comb.len = l1, v->comb.w = (uint16_t)(w & (QE_COMB_N - 1));
    }
}

/* hook 4: BWPOST: the base-width filter (BASE: a one-pole high-pass hpk, BASE + WIDTH: a one-pole low-pass lpk, its
 * states the voice's bh / bl) after the machine, as eng_quad.c runs it before (its switches by the dip) */
static void quad_ext_bw_post(quad_voice_t *s, int32_t *acc, uint32_t n, int32_t hpk, int32_t lpk)
{
    int32_t bh = s->bh, bl = s->bl;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t y = acc[i];
        if (hpk) {
            bh += (int32_t)(((int64_t)((y << 8) - bh) * hpk) >> 16);
            y -= bh >> 8;
        }
        if (lpk) {
            bl += (int32_t)(((int64_t)((y << 8) - bl) * lpk) >> 16);
            y = bl >> 8;
        }
        acc[i] = y;
    }
    s->bh = bh, s->bl = bl;
}

/* ================================================================ E3: CRUSH */
/* SRR's phase step a sample (2^32 / N, N = 64^(SRR / 127)); BITS b = 16 .. 1 over 1..127 (0: OFF) */
static const uint32_t QE_SRR_INC[128] = {
    0u, 4156597496u, 4022685519u, 3893087748u, 3767665193u, 3646283343u, 3528812018u, 3415125236u,
    3305101070u, 3198621523u, 3095572399u, 2995843180u, 2899326911u, 2805920080u, 2715522512u, 2628037257u,
    2543370492u, 2461431413u, 2382132143u, 2305387636u, 2231115586u, 2159236339u, 2089672806u, 2022350382u,
    1957196867u, 1894142385u, 1833119312u, 1774062203u, 1716907721u, 1661594569u, 1608063426u, 1556256882u,
    1506119375u, 1457597134u, 1410638122u, 1365191975u, 1321209954u, 1278644891u, 1237451134u, 1197584506u,
    1159002250u, 1121662988u, 1085526674u, 1050554555u, 1016709123u, 983954080u, 952254298u, 921575779u,
    891885621u, 863151983u, 835344050u, 808431996u, 782386961u, 757181012u, 732787116u, 709179112u,
    686331680u, 664220318u, 642821312u, 622111712u, 602069307u, 582672603u, 563900797u, 545733758u,
    528152001u, 511136671u, 494669519u, 478732885u, 463309677u, 448383354u, 433937909u, 419957849u,
    406428181u, 393334394u, 380662446u, 368398747u, 356530144u, 345043909u, 333927723u, 323169664u,
    312758194u, 302682148u, 292930719u, 283493449u, 274360217u, 265521228u, 256967003u, 248688366u,
    240676441u, 232922633u, 225418627u, 218156376u, 211128091u, 204326235u, 197743512u, 191372863u,
    185207455u, 179240676u, 173466128u, 167877617u, 162469149u, 157234924u, 152169329u, 147266931u,
    142522472u, 137930864u, 133487183u, 129186662u, 125024691u, 120996804u, 117098683u, 113326146u,
    109675148u, 106141774u, 102722233u, 99412859u, 96210101u, 93110527u, 90110810u, 87207734u,
    84398186u, 81679153u, 79047718u, 76501059u, 74036445u, 71651233u, 69342864u, 67108864u,
};
static int qe_crush_bits(int v) { return v <= 0 ? 16 : 16 - (v - 1) * 15 / 126; }

/* hook 5: CRUSH on the voice's signal (before the filter with CRPOST 0, after it with 1) */
static void quad_ext_crush(voice_t *v, int bits, int srr, int32_t *acc, uint32_t n)
{
    const uint32_t inc = srr ? QE_SRR_INC[srr] : 0u;
    const int sh = bits ? 16 - qe_crush_bits(bits) : 0;
    uint32_t ph = v->crush.ph, i;
    int32_t held = v->crush.held;
    for (i = 0; i < n; i++) {
        int32_t y = acc[i];
        if (inc) {                                      /* the sample and hold: a new value when the phase wraps */
            const uint32_t np = ph + inc;
            if (np <= ph)                               /* (fresh: the phase at 0, the first value N samples on) */
                held = y;
            ph = np;
            y = held;
        }
        if (sh) {                                       /* b bits of the voice's +-32768: a step 2^(16 - b), rounded
                                                         * (0 stays 0: a silent voice stays silent; 1 bit: -, 0, +) */
            y = clamp(y, -32768, 32767);
            y = ((y + (1 << (sh - 1))) >> sh) << sh;
        }
        acc[i] = y;
    }
    v->crush.ph = ph, v->crush.held = held;
}

/* ================================================================ E4: the modulation slots */
/* hook 6, a block: each slot with a SOURCE adds source x DEPTH / 127 (Q15) to its destination, after the LFOs */
static void quad_ext_mods(track_t *t, voice_t *v, quad_voice_t *s, const int16_t *p, int32_t *dst)
{
    int k;
    for (k = 0; k < QD_NMOD; k++) {
        const int src = p[QD_QP_MOD(k, QD_QM_SRC)], d = p[QD_QP_MOD(k, QD_QM_DST)], dep = p[QD_QP_MOD(k, QD_QM_DEPTH)];
        int32_t x;
        if (!src || d <= 0 || d >= (int)QD_N || !dep)
            continue;
        switch (src) {
        case QD_MS_VEL: x = (int32_t)(v->mvel ? v->mvel : v->vel) * 32767 / 127; break;
        case QD_MS_KEY: x = clamp((v->pitch16 - 60 * 16) * 32767 / (64 * 16), -32767, 32767); break;
        case QD_MS_MW: x = (int32_t)t->mw * 32767 / 127; break;
        case QD_MS_AT: x = (int32_t)t->at * 32767 / 127; break;
        case QD_MS_PB: x = clamp((int32_t)t->pb * 4, -32767, 32767); break;
        case QD_MS_BR: x = (int32_t)t->br * 32767 / 127; break;
        case QD_MS_RAND: x = v->mrnd; break;
        case QD_MS_FENV: x = s->env[2] >> 9; break;  /* (the filter envelope's level, the last block's: Q15) */
        default: x = 0; break;
        }
        dst[d] += x * dep / 127;
    }
}
