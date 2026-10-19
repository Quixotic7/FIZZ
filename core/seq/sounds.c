/* SPDX-License-Identifier: GPL-3.0-only */
/* The sound pool (sounds.h).
 *
 * THE DRUMS (P1.2's demo kit, moved here from demo.c's sound locks and p-locks; the mix values, a step's PAN, sends
 * and LEVEL, stay p-locks on the demos' steps). Designed by rendering one voice of each and measuring it (the attack,
 * the decay to -20 and -40 dB, the spectral centroid over 0..30 and 30..100 ms, the pitch from zero crossings; the
 * numbers are render_audio's demo.txt):
 *   KICK      INIT (a sine on operator C), AMP DEC 32 SUS 0 REL 20; LFO 1 on PITCH, EXP wave, HALF trig (one fall,
 *             then it holds), SPEED 40.00 x F64 (a 100 ms cycle at any tempo), DEPTH 63.99 (one octave): played at A1
 *             the pitch falls from 100 to 62 Hz in 0.1 s; A LEV 60 with A DEC 4, RATIO A 2.00: a click of FM on the
 *             attack; LEVEL 127. -20 dB at 74 ms, -40 dB at 180 ms; centroid 118 Hz on the attack, 68 Hz after
 *   SNARE     NOISE-ISH (operator A at full feedback), its high-pass swapped for LP12 at 95.00, RESO 10, FDBK 110,
 *             AMP DEC 24 SUS 0 REL 18, LEVEL 120: centroid 2.3 kHz, -20 dB at 68 ms, -40 dB at 172 ms (at G3)
 *   CLAP      the same noise, LP12 at 105.00 RESO 30, FDBK 120, AMP DEC 14 SUS 0 REL 12 (played as a strummed
 *             three-note chord: three bursts about 10 ms apart); centroid 2.9 kHz
 *   GHOST     the snare quieter (LEVEL 70) and darker (LP12 at 80.00, RESO 0), AMP DEC 16 REL 12: centroid 1.1 kHz
 *   HAT       NOISE-ISH's own HP12 raised to 95.00, RESO 10, AMP DEC 10 SUS 0 REL 8 (-40 dB at 36 ms; at C7)
 *   OPEN HAT  the same at 90.00, AMP DEC 32 REL 26 (-20 dB at 255 ms) */
#include <string.h>

#include "sounds.h"

static int16_t clampv(int id, int v)
{
    int16_t lo = 0, hi = 0;
    qd_lock_range(id, &lo, &hi);
    return (int16_t)(v < lo ? lo : v > hi ? hi : v);
}

typedef struct { int id, v; } kv_t;

static void make(qd_project_t *pr, const char *name, int preset, const kv_t *kv, int n)
{
    int16_t p[QD_QUAD_NP];
    int i;
    qd_quad_preset_patch(preset, p);
    for (i = 0; i < n; i++)
        p[kv[i].id] = clampv(kv[i].id, kv[i].v);
    qd_sound_add(pr, name, p);
}

#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

void qd_sounds_init(qd_project_t *pr)
{
    static const kv_t KICK[] = {
        {QD_QP_EDEC, 32}, {QD_QP_ESUS, 0}, {QD_QP_EREL, 20}, {QD_QP_LFO(0, QD_QL_SPEED), 4000},
        {QD_QP_LFO(0, QD_QL_MULT), 18},   /* F64: x 64 at a fixed 120 BPM */
        {QD_QP_LFO(0, QD_QL_DEST), 1},    /* PITCH */
        {QD_QP_LFO(0, QD_QL_WAVE), 5},    /* EXP */
        {QD_QP_LFO(0, QD_QL_TRIG), 4},    /* HALF */
        {QD_QP_LFO(0, QD_QL_DEPTH), 6399}, {QD_QP_ALEV, 60}, {QD_QP_ADEC, 4}, {QD_QP_AEND, 0},
        {QD_QP_RA, 8},                    /* RATIO A 2.00 */
        {QD_QP_LEVEL, 127},
    };
#define NOISE_BODY(freq, reso, fdbk, dec, rel)                                                                       \
    {QD_QP_FTYPE, QD_QF_LP12}, {QD_QP_FREQ, freq}, {QD_QP_RESO, reso}, {QD_QP_FDBK, fdbk}, {QD_QP_EDEC, dec},      \
        {QD_QP_ESUS, 0}, {QD_QP_EREL, rel}
    static const kv_t SNARE[] = {NOISE_BODY(9500, 1000, 110, 24, 18), {QD_QP_LEVEL, 120}};
    static const kv_t CLAP[] = {NOISE_BODY(10500, 3000, 120, 14, 12)};
    static const kv_t GHOST[] = {NOISE_BODY(8000, 0, 110, 16, 12), {QD_QP_LEVEL, 70}};
#undef NOISE_BODY
    static const kv_t HAT[] = {{QD_QP_FREQ, 9500}, {QD_QP_RESO, 1000}, {QD_QP_EDEC, 10}, {QD_QP_ESUS, 0},
                               {QD_QP_EREL, 8}};
    static const kv_t OHAT[] = {{QD_QP_FREQ, 9000}, {QD_QP_RESO, 1000}, {QD_QP_EDEC, 32}, {QD_QP_ESUS, 0},
                                {QD_QP_EREL, 26}};
    int k;
    memset(pr->sound, 0, sizeof pr->sound);
    pr->nsound = 0;
    for (k = 0; k < QD_QUAD_NPRESET; k++)
        make(pr, qd_quad_preset_name(k), k, 0, 0);
    make(pr, "KICK", 0, KICK, N(KICK));
    make(pr, "SNARE", QD_SND_NOISE - 1, SNARE, N(SNARE));
    make(pr, "CLAP", QD_SND_NOISE - 1, CLAP, N(CLAP));
    make(pr, "GHOST", QD_SND_NOISE - 1, GHOST, N(GHOST));
    make(pr, "HAT", QD_SND_NOISE - 1, HAT, N(HAT));
    make(pr, "OPEN HAT", QD_SND_NOISE - 1, OHAT, N(OHAT));
}

const char *qd_sound_name(const qd_project_t *pr, int k)
{
    return k >= 1 && k <= pr->nsound ? pr->sound[k - 1].name : "";
}

const int16_t *qd_sound_patch(const qd_project_t *pr, int k)
{
    return pr->sound[k >= 1 && k <= pr->nsound ? k - 1 : 0].patch;
}

int qd_sound_add(qd_project_t *pr, const char *name, const int16_t *patch)
{
    qd_sound_t *s;
    if (pr->nsound >= QD_NSOUND)
        return 0;
    s = &pr->sound[pr->nsound++];
    memset(s->name, 0, sizeof s->name);
    strncpy(s->name, name ? name : "", QD_SOUND_NAME - 1);
    memcpy(s->patch, patch, sizeof s->patch);
    return pr->nsound;
}
