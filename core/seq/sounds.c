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
#include <stdio.h>
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

int qd_sound_used(const qd_project_t *pr, int k) { return k >= 1 && k <= QD_NSOUND && pr->sound[k - 1].name[0]; }

const int16_t *qd_sound_patch(const qd_project_t *pr, int k)
{
    return pr->sound[qd_sound_used(pr, k) ? k - 1 : 0].patch;
}

void qd_sound_recount(qd_project_t *pr)
{
    int k;
    for (k = QD_NSOUND; k > 1 && !pr->sound[k - 1].name[0]; k--)
        ;
    pr->nsound = (uint8_t)k;
}

int qd_sound_step(const qd_project_t *pr, int k, int d, int lo)
{
    const int dir = d < 0 ? -1 : 1;
    int n = d < 0 ? -d : d;
    while (n-- > 0) {
        int j = k + dir;
        while (j >= 1 && j <= pr->nsound && !qd_sound_used(pr, j))
            j += dir;
        if (j < lo || j > pr->nsound || (j >= 1 && !qd_sound_used(pr, j)))
            break;
        k = j;
    }
    return k;
}

int qd_sound_add(qd_project_t *pr, const char *name, const int16_t *patch)
{
    qd_sound_t *s;
    int k;
    for (k = 1; k <= QD_NSOUND && pr->sound[k - 1].name[0]; k++)
        ;
    if (k > QD_NSOUND)
        return 0;
    s = &pr->sound[k - 1];
    memset(s->name, 0, sizeof s->name);
    strncpy(s->name, name && name[0] ? name : "SOUND", QD_SOUND_NAME - 1);
    memcpy(s->patch, patch, sizeof s->patch);
    qd_sound_recount(pr);
    return k;
}

/* ---------------------------------------------------------------- P1.5a: a name from the sound */
int qd_sound_kind(const int16_t *p)
{
    const int noise = p[QD_QP_FDBK] >= 100, perc = p[QD_QP_ESUS] <= 10 && p[QD_QP_EDEC] <= 40;
    const int lp = p[QD_QP_FTYPE] == QD_QF_LP12 || p[QD_QP_FTYPE] == QD_QF_LP24;
    if (noise && perc) {
        if (p[QD_QP_FTYPE] == QD_QF_HP12)
            return QD_SK_HAT;
        if (p[QD_QP_EDEC] < 20 && p[QD_QP_RESO] >= 2000)
            return QD_SK_CLAP;
        return QD_SK_SNARE;
    }
    if (noise)
        return QD_SK_NOISE;
    if (perc && p[QD_QP_ADEC] <= 6 && p[QD_QP_ALEV] >= 40)
        return QD_SK_KICK;                              /* (a fast pitch-like drop of the modulator) */
    if (p[QD_QP_EATK] >= 30)
        return QD_SK_PAD;
    if (p[QD_QP_ESUS] <= 40 && p[QD_QP_EDEC] >= 40)
        return QD_SK_BELL;
    if (p[QD_QP_ESUS] <= 10)
        return QD_SK_PLUCK;
    if (p[QD_QP_ESUS] < 80)
        return QD_SK_KEYS;
    if (lp && p[QD_QP_FREQ] < 9500 && p[QD_QP_EATK] < 10)
        return QD_SK_BASS;
    return QD_SK_LEAD;
}

const char *qd_sound_kind_name(int k)
{
    static const char *const N[QD_NSK] = {"BASS", "KEYS", "PLUCK", "BELL", "PAD", "LEAD", "KICK", "SNARE", "CLAP",
                                          "HAT", "NOISE"};
    return k >= 0 && k < QD_NSK ? N[k] : "SOUND";
}

void qd_sound_autoname(const int16_t *p, int variant, char *out)
{
    static const char *const MORE[] = {"WARM", "DEEP", "CLEAR", "ROUND", "THIN", "BIG", "TINY", "DUST", "GLOW", "VELVET",
                                       "DRY", "RAW"};
    const char *cand[24], *kind;
    const int k = qd_sound_kind(p);
    int n = 0, i;
    const int lp = p[QD_QP_FTYPE] == QD_QF_LP12 || p[QD_QP_FTYPE] == QD_QF_LP24;
    const int drum = k == QD_SK_KICK || k == QD_SK_SNARE || k == QD_SK_CLAP || k == QD_SK_HAT;
    kind = k == QD_SK_HAT && p[QD_QP_EDEC] >= 24 ? "OPEN HAT" : qd_sound_kind_name(k);
    /* the adjectives that hold, the strongest first (a drum: of its body only) */
    if (k == QD_SK_KICK)
        cand[n++] = "DEEP";
    if (lp && p[QD_QP_FREQ] < 8000)
        cand[n++] = "DARK";
    if (p[QD_QP_LEVEL] < 80 || (p[QD_QP_EATK] >= 20 && k != QD_SK_PAD))
        cand[n++] = "SOFT";
    if (p[QD_QP_DTUN] >= 8000 && k != QD_SK_PAD && !drum)
        cand[n++] = "METAL";
    if (p[QD_QP_HARM] <= -1000 && !drum)
        cand[n++] = "HOLLOW";
    if (p[QD_QP_DTUN] >= 3000 && !drum)
        cand[n++] = "WIDE";
    if ((p[QD_QP_FDBK] >= 30 && p[QD_QP_FDBK] < 100) || p[QD_QP_DRIVE] >= 20)
        cand[n++] = "HARD";
    if (p[QD_QP_FREQ] >= 12000 && !drum && (p[QD_QP_HARM] > 0 || p[QD_QP_RA] >= 7 || p[QD_QP_RB1] >= 6))
        cand[n++] = "BRIGHT";
    if (p[QD_QP_ESUS] <= 10 && p[QD_QP_EDEC] <= 14)
        cand[n++] = k == QD_SK_KICK || k == QD_SK_HAT ? "TIGHT" : "SHORT";
    if (p[QD_QP_EREL] >= 50)
        cand[n++] = "LONG";
    if (p[QD_QP_RESO] >= 4000 && !drum)
        cand[n++] = "RUBBER";
    if (p[QD_QP_HARM] > 0 && !drum)
        cand[n++] = "GLASS";
    for (i = 0; i < (int)(sizeof MORE / sizeof MORE[0]); i++)
        cand[n++] = MORE[i];
    memset(out, 0, QD_SOUND_NAME);
    {   /* variant v: the v-th of those that fit with the kind */
        const char *fit[24];
        int m = 0;
        for (i = 0; i < n; i++) {
            int j, dup = 0;
            for (j = 0; j < m; j++)
                dup |= !strcmp(fit[j], cand[i]);
            if (!dup && strlen(cand[i]) + 1 + strlen(kind) <= QD_SOUND_NAME - 1)
                fit[m++] = cand[i];
        }
        if (m) {
            snprintf(out, QD_SOUND_NAME, "%s %s", fit[(variant < 0 ? 0 : variant) % m], kind);
            return;
        }
    }
    snprintf(out, QD_SOUND_NAME, "%s", kind);
}
