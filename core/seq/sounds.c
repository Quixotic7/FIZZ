/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
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

static void make_patch(int preset, const kv_t *kv, int n, int16_t *p)
{
    int i;
    qd_quad_preset_patch(preset, p);
    for (i = 0; i < n; i++)
        p[kv[i].id] = clampv(kv[i].id, kv[i].v);
}

#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* P1.5t: factory preset i (0..QD_NFACTORY-1): INIT, the 16 FM TONE sounds, the six drums */
/* P1.5x: the FM TONE presets' sends, chorus / delay / reverb, as ChoralRoot's FX() has them (firmware/src/eng_quad.c
 * QUAD_PRESETS: EP .. NOISE-ISH, the order of quad_voice.h's presets 1..16) */
static const uint8_t FX_SENDS[16][3] = {
    {30, 20, 40}, {20, 35, 60}, {0, 10, 10}, {20, 30, 40}, {25, 20, 40}, {50, 25, 70}, {35, 25, 50}, {25, 40, 40},
    {20, 30, 50}, {0, 15, 10}, {15, 20, 25}, {55, 15, 65}, {15, 25, 40}, {50, 30, 80}, {25, 30, 40}, {20, 30, 50}};

#ifdef QD_DEVICE
static uint8_t tagc[QD_NFACTORY];                       /* (P2.1b: only the classifier's tags kept, made once; the rest
                                                         * made when asked: no 5.5 KB cache) */
#endif
void qd_factory_preset(int i, qd_sound_t *out)
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
    static const struct {
        const char *name;
        const kv_t *kv;
        int n;
    } DRUM[6] = {{"KICK", KICK, N(KICK)}, {"SNARE", SNARE, N(SNARE)}, {"CLAP", CLAP, N(CLAP)},
                 {"GHOST", GHOST, N(GHOST)}, {"HAT", HAT, N(HAT)}, {"OPEN HAT", OHAT, N(OHAT)}};
    const int want = i < 0 ? 0 : i >= QD_NFACTORY ? QD_NFACTORY - 1 : i;
    static int made;
    int k;
#ifdef QD_DEVICE
    qd_sound_t one, *s = &one;
    for (k = made ? want : 0; k < (made ? want + 1 : QD_NFACTORY); k++) {
#else
    static qd_sound_t cache[QD_NFACTORY];
    qd_sound_t *s;
    for (k = 0; !made && k < QD_NFACTORY; k++) {      /* (made once: the classifier run once for the set) */
        s = &cache[k];
#endif
        memset(s, 0, sizeof *s);
        if (k < QD_QUAD_NPRESET) {
            qd_quad_preset_patch(k, s->patch);
            strncpy(s->name, qd_quad_preset_name(k), QD_SOUND_NAME - 1);
        } else {
            const int d = k - QD_QUAD_NPRESET;
            make_patch(d ? QD_SND_NOISE - 1 : 0, DRUM[d].kv, DRUM[d].n, s->patch);
            strncpy(s->name, DRUM[d].name, QD_SOUND_NAME - 1);
        }
        s->preset = (uint16_t)(k + 1);
#ifdef QD_DEVICE
        if (!made)
            tagc[k] = (uint8_t)qd_preset_tags(s->patch);
        s->tags = tagc[k];
#else
        s->tags = (uint8_t)qd_preset_tags(s->patch);
#endif
        if (k >= 1 && k <= 16) {                    /* P1.5x: ChoralRoot's FX(drive, chorus, delay, reverb) of its
                                                     * QUAD preset (eng_quad.c QUAD_PRESETS); pan centre */
            s->chorus = FX_SENDS[k - 1][0];
            s->delay = FX_SENDS[k - 1][1];
            s->reverb = FX_SENDS[k - 1][2];
        } else {
            s->reverb = 20;                         /* (INIT, the drums: the default kit's, QD_MIX_REVERB_DEFAULT) */
        }
#ifdef QD_DEVICE
        if (k == want)
            *out = *s;
#endif
    }
    made = 1;
#ifndef QD_DEVICE
    *out = cache[want];
#endif
}

int qd_factory_tag(int i)
{
    qd_sound_t s;
#ifdef QD_DEVICE
    static int once;
    if (!once)
        qd_factory_preset(0, &s), once = 1;             /* (the first: all the tags made) */
    return tagc[i < 0 ? 0 : i >= QD_NFACTORY ? QD_NFACTORY - 1 : i];
#else
    qd_factory_preset(i, &s);
    return s.tags;
#endif
}

int qd_factory_of(const char *name, const int16_t *patch)
{
    int k;
    for (k = 0; k < QD_NFACTORY; k++) {
        qd_sound_t f;
        qd_factory_preset(k, &f);
        if (!strcmp(f.name, name) && !memcmp(f.patch, patch, sizeof f.patch))
            return k + 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- the pool's sounds (P2.1: read and written here) */
#if QD_SND_RAM
/* (P2.1: the device's sizes) the places in RAM */
static int sc_find(const qd_sndc_t *c, int key)
{
    int i;
    for (i = 0; i < QD_SND_RAM; i++)
        if (c->key[i] == key)
            return i;
    return -1;
}
static int sc_clean(const qd_sndc_t *c, int i)
{
    return !c->io.clean || c->io.clean(c->io.ctx, c->key[i], &c->s[i]);
}
static int sc_kept(int key, const int *keep, int nkeep)
{
    int i;
    for (i = 0; i < nkeep; i++)
        if (keep[i] == key)
            return 1;
    return 0;
}
/* a free place, else the least recently used whose sound its record holds (P2.4); none (all waiting to be written):
 * write: the least recently used, written now; else -1, the oldest asked to be written soon */
static int sc_place_ex(qd_sndc_t *c, int write, const int *keep, int nkeep)
{
    int i, sl = -1, old = -1;
    for (i = 0; i < QD_SND_RAM; i++)
        if (c->key[i] < 0)
            return i;
    for (i = 0; i < QD_SND_RAM; i++) {
        if (sc_kept(c->key[i], keep, nkeep))
            continue;
        if (old < 0 || c->lru[i] < c->lru[old])
            old = i;
        if ((sl < 0 || c->lru[i] < c->lru[sl]) && sc_clean(c, i))
            sl = i;
    }
    if (sl < 0 && old >= 0 && !write) {
        if (c->io.save_soon)
            c->io.save_soon(c->io.ctx, c->key[old]);
        return -1;
    }
    if (sl < 0)
        sl = old;
    if (sl < 0)
        return -1;
    if (c->io.save) {
        c->io.save(c->io.ctx, c->key[sl], &c->s[sl]);   /* (clean: nothing written) */
        c->saves++;
    }
    c->key[sl] = -1;
    return sl;
}
static int sc_place(qd_sndc_t *c) { return sc_place_ex(c, 1, 0, 0); }
void qd_sndc_init(qd_sndc_t *c)
{
    int i;
    memset(c->s, 0, sizeof c->s);
    for (i = 0; i < QD_SND_RAM; i++)
        c->key[i] = -1, c->lru[i] = 0;
    c->clock = 0;
}
const qd_sound_t *qd_sndc_get(qd_sndc_t *c, int key)
{
    int sl = sc_find(c, key);
    if (sl < 0) {
        qd_sound_t t;
        memset(&t, 0, sizeof t);
        if (c->io.load)                                 /* (read first: a pool reference reads its preset, which may
                                                         * move the places) */
            c->io.load(c->io.ctx, key, &t);
        c->loads++;
        sl = sc_place_ex(c, 0, 0, 0);                   /* (P2.4: never a write) */
        if (sl < 0) {
            c->uncached++;
            memcpy(&c->scratch, &t, sizeof t);
            return &c->scratch;
        }
        memcpy(&c->s[sl], &t, sizeof t);
        c->key[sl] = (int16_t)key;
    }
    c->lru[sl] = ++c->clock;
    return &c->s[sl];
}
int qd_sndc_fetch(qd_sndc_t *c, int key, const int *keep, int nkeep)
{
    qd_sound_t t;
    int sl;
    if (sc_find(c, key) >= 0)
        return 1;
    for (sl = 0; sl < QD_SND_RAM; sl++)                 /* (a place it could take: else nothing read) */
        if (c->key[sl] < 0 || (!sc_kept(c->key[sl], keep, nkeep) && sc_clean(c, sl)))
            break;
    if (sl == QD_SND_RAM) {
        sc_place_ex(c, 0, keep, nkeep);                 /* (the oldest asked to be written) */
        return 0;
    }
    memset(&t, 0, sizeof t);
    if (c->io.load)
        c->io.load(c->io.ctx, key, &t);
    c->loads++;
    if (sc_find(c, key) >= 0)                           /* (its reading read it: a reference to itself) */
        return 1;
    sl = sc_place_ex(c, 0, keep, nkeep);
    if (sl < 0)
        return 0;
    memcpy(&c->s[sl], &t, sizeof t);
    c->key[sl] = (int16_t)key;
    c->lru[sl] = ++c->clock;
    return 1;
}
const qd_sound_t *qd_sndc_peek(const qd_sndc_t *c, int key)
{
    const int sl = sc_find(c, key);
    return sl >= 0 ? &c->s[sl] : 0;
}
void qd_sndc_put(qd_sndc_t *c, int key, const qd_sound_t *s)
{
    int sl = sc_find(c, key);
    if (sl < 0) {
        sl = sc_place(c);
        c->key[sl] = (int16_t)key;
    }
    memcpy(&c->s[sl], s, sizeof *s);
    c->lru[sl] = ++c->clock;
}
void qd_sndc_drop(qd_sndc_t *c, int lo, int hi)
{
    int i;
    for (i = 0; i < QD_SND_RAM; i++)
        if (c->key[i] >= lo && c->key[i] <= hi)
            c->key[i] = -1;
}
void qd_sndc_save_all(qd_sndc_t *c)
{
    int i;
    for (i = 0; i < QD_SND_RAM; i++)
        if (c->key[i] >= 0 && c->io.save)
            c->io.save(c->io.ctx, c->key[i], &c->s[i]);
}

void qd_sound_dir(qd_sound_dir_t *d, const qd_sound_t *s)
{
    memcpy(d->name, s->name, sizeof d->name);
    d->preset = s->preset, d->tags = s->tags, d->flags = s->flags;
}

#ifndef QD_TARGET_FM1
static qd_sound_t sc_bare[QD_NSOUND];                   /* (the host's tests at the device's sizes: a project with no
                                                         * app (pr->sc 0) keeps its pool here; the device has none) */
#define SC_BARE(pr) (!(pr)->sc)
#else
#define SC_BARE(pr) 0
#endif
#endif

void qd_sound_read(const qd_project_t *pr, int k, qd_sound_t *out)
{
    if (k < 1 || k > QD_NSOUND) {
        memset(out, 0, sizeof *out);
        return;
    }
#if QD_SND_RAM
#ifndef QD_TARGET_FM1
    if (SC_BARE(pr)) {
        memcpy(out, &sc_bare[k - 1], sizeof *out);
        return;
    }
#endif
    if (!pr->sc || !pr->sdir[k - 1].name[0]) {          /* (an empty slot: nothing to read) */
        memset(out, 0, sizeof *out);
        return;
    }
    memcpy(out, qd_sndc_get(pr->sc, k), sizeof *out);
#else
    memcpy(out, &pr->sound[k - 1], sizeof *out);
#endif
}

void qd_sound_write(qd_project_t *pr, int k, const qd_sound_t *s)
{
    if (k < 1 || k > QD_NSOUND)
        return;
#if QD_SND_RAM
    qd_sound_dir(&pr->sdir[k - 1], s);
#ifndef QD_TARGET_FM1
    if (SC_BARE(pr)) {
        memcpy(&sc_bare[k - 1], s, sizeof *s);
        return;
    }
#endif
    if (pr->sc)
        qd_sndc_put(pr->sc, k, s);
#else
    memcpy(&pr->sound[k - 1], s, sizeof *s);
#endif
}

void qd_sound_clear(qd_project_t *pr, int k)
{
    qd_sound_t z;
    memset(&z, 0, sizeof z);
    qd_sound_write(pr, k, &z);
}

#if QD_SND_RAM
#define POOL_DIR(pr, k) (&(pr)->sdir[(k) - 1])
#else
#define POOL_DIR(pr, k) (&(pr)->sound[(k) - 1])
#endif
uint16_t qd_sound_preset(const qd_project_t *pr, int k) { return k >= 1 && k <= QD_NSOUND ? POOL_DIR(pr, k)->preset : 0; }
int qd_sound_tags(const qd_project_t *pr, int k) { return k >= 1 && k <= QD_NSOUND ? POOL_DIR(pr, k)->tags : 0; }
int qd_sound_flags(const qd_project_t *pr, int k) { return k >= 1 && k <= QD_NSOUND ? POOL_DIR(pr, k)->flags : 0; }
int qd_sound_ready(const qd_project_t *pr, int k)
{
#if QD_SND_RAM
    return !qd_sound_used(pr, k) || !pr->sc || qd_sndc_peek(pr->sc, k) != 0;
#else
    (void)pr, (void)k;
    return 1;
#endif
}
int qd_sound_resident(const qd_project_t *pr, int k)
{
#if QD_SND_RAM
    return pr->sc && qd_sndc_peek(pr->sc, k) != 0;
#else
    (void)pr, (void)k;
    return 1;
#endif
}

/* the default pool: the factory's sounds (a new project's: none). (P2.1, the device: their names, ids and tags; the
 * sounds are what a slot with no record reads, persist.c) */
void qd_sounds_init(qd_project_t *pr)
{
    int k;
    qd_sounds_clear(pr);
    for (k = 0; k < QD_NFACTORY; k++) {
        qd_sound_t f;
        qd_factory_preset(k, &f);
#if QD_SND_RAM
        qd_sound_dir(&pr->sdir[k], &f);
#ifndef QD_TARGET_FM1
        if (SC_BARE(pr))
            sc_bare[k] = f;
#endif
#else
        pr->sound[k] = f;
#endif
    }
    qd_sound_recount(pr);
}

void qd_sounds_clear(qd_project_t *pr)
{
#if QD_SND_RAM
    memset(pr->sdir, 0, sizeof pr->sdir);
    if (pr->sc)                                         /* (a project starting: its pool's places out, unwritten: the
                                                         * one before was written first, persist.c) */
        qd_sndc_drop(pr->sc, 1, QD_NSOUND);
#ifndef QD_TARGET_FM1
    else
        memset(sc_bare, 0, sizeof sc_bare);
#endif
#else
    memset(pr->sound, 0, sizeof pr->sound);
#endif
    pr->nsound = 0;
}

const char *qd_sound_name(const qd_project_t *pr, int k)
{
    return k >= 1 && k <= pr->nsound ? POOL_DIR(pr, k)->name : "";
}

int qd_sound_used(const qd_project_t *pr, int k) { return k >= 1 && k <= QD_NSOUND && POOL_DIR(pr, k)->name[0]; }

/* (P2.1: on the device the patch is in one of the places: good until the next sound is read) */
const int16_t *qd_sound_patch(const qd_project_t *pr, int k)
{
    static int16_t init[QD_QUAD_NP];
    static int made;
    if (qd_sound_used(pr, k)) {
#if QD_SND_RAM
#ifndef QD_TARGET_FM1
        if (SC_BARE(pr))
            return sc_bare[k - 1].patch;
#endif
        if (pr->sc)
            return qd_sndc_get(pr->sc, k)->patch;
#else
        return pr->sound[k - 1].patch;
#endif
    }
    if (!made)
        qd_quad_preset_patch(0, init), made = 1;
    return init;                                        /* (P1.5t: no sound there: INIT's patch) */
}

#if QD_PAT_RAM < QD_NPAT
uint16_t qd_users_gen;                                  /* (2026-10-24: sounds.h) */
#endif
void qd_sound_recount(qd_project_t *pr)
{
    int k;
    for (k = QD_NSOUND; k > 0 && !POOL_DIR(pr, k)->name[0]; k--)
        ;
    pr->nsound = (uint8_t)k;                            /* (P1.5t: 0: the pool empty) */
#if QD_PAT_RAM < QD_NPAT
    qd_users_gen++;                                     /* (persist.c's count of a slot's users: read again) */
#endif
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
    qd_sound_t s;
    int k;
    for (k = 1; k <= QD_NSOUND && POOL_DIR(pr, k)->name[0]; k++)
        ;
    if (k > QD_NSOUND)
        return 0;
    memset(&s, 0, sizeof s);
    strncpy(s.name, name && name[0] ? name : "SOUND", QD_SOUND_NAME - 1);
    memcpy(s.patch, patch, sizeof s.patch);
    s.tags = (uint8_t)qd_preset_tags(patch);
    qd_sound_write(pr, k, &s);
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

/* the adjectives that hold for the sound, the strongest first (a drum: of its body only), then the general ones; the
 * kind's name in kind; their count */
static int sound_adjectives(const int16_t *p, const char **cand, const char **kind)
{
    static const char *const MORE[] = {"WARM", "DEEP", "CLEAR", "ROUND", "THIN", "BIG", "TINY", "DUST", "GLOW", "VELVET",
                                       "DRY", "RAW"};
    const int k = qd_sound_kind(p);
    int n = 0, i;
    const int lp = p[QD_QP_FTYPE] == QD_QF_LP12 || p[QD_QP_FTYPE] == QD_QF_LP24;
    const int drum = k == QD_SK_KICK || k == QD_SK_SNARE || k == QD_SK_CLAP || k == QD_SK_HAT;
    *kind = k == QD_SK_HAT && p[QD_QP_EDEC] >= 24 ? "OPEN HAT" : qd_sound_kind_name(k);
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
    {   /* each once */
        int m = 0, j;
        for (i = 0; i < n; i++) {
            int dup = 0;
            for (j = 0; j < m; j++)
                dup |= !strcmp(cand[j], cand[i]);
            if (!dup)
                cand[m++] = cand[i];
        }
        n = m;
    }
    return n;
}

void qd_sound_autoname(const int16_t *p, int variant, char *out)
{
    const char *cand[24], *fit[24], *kind;
    const int n = sound_adjectives(p, cand, &kind);
    int m = 0, i;
    memset(out, 0, QD_SOUND_NAME);
    for (i = 0; i < n; i++)                             /* variant v: the v-th of those that fit with the kind */
        if (strlen(cand[i]) + 1 + strlen(kind) <= QD_SOUND_NAME - 1)
            fit[m++] = cand[i];
    if (m) {
        snprintf(out, QD_SOUND_NAME, "%s %s", fit[(variant < 0 ? 0 : variant) % m], kind);
        return;
    }
    snprintf(out, QD_SOUND_NAME, "%s", kind);
}

/* P1.5u: name ideas (PRESETS in SAVE PRESET): the best name first (qd_sound_autoname's), then the adjectives that
 * hold paired with the kind's other words (a PLUCK: PING, TINE, HARP ..), each once, twelve letters at most */
int qd_sound_ideas(const int16_t *p, char (*out)[QD_SOUND_NAME], int max)
{
    static const char *const NOUN[QD_NSK][4] = {
        {"BASS", "SUB", "LOW", "BOTTOM"},  {"KEYS", "PIANO", "ORGAN", "KEY"},  {"PLUCK", "PING", "TINE", "HARP"},
        {"BELL", "CHIME", "TINE", "DROP"}, {"PAD", "DRONE", "CLOUD", "HAZE"}, {"LEAD", "LINE", "SYNTH", "VOICE"},
        {"KICK", "THUMP", "BOOM", "DRUM"}, {"SNARE", "SNAP", "CRACK", "RIM"}, {"CLAP", "SLAP", "HANDS", "SNAP"},
        {"HAT", "TICK", "SHAKER", "SIZZLE"}, {"NOISE", "HISS", "WIND", "STATIC"}};
    const char *cand[24], *kind;
    const int na = sound_adjectives(p, cand, &kind), k = qd_sound_kind(p);
    int n = 0, i, j;
    if (max <= 0)
        return 0;
    qd_sound_autoname(p, 0, out[n++]);
    for (i = 0; i < na * 4 && n < max; i++) {
        const int w = (i + i / na) % 4;                 /* (the adjectives in turn, each with the next word) */
        const char *adj = cand[i % na], *noun = w ? NOUN[k][w] : kind;   /* (an open hat stays OPEN HAT) */
        char nm[32];
        int dup = 0;
        if (strlen(adj) + 1 + strlen(noun) > QD_SOUND_NAME - 1)
            continue;
        snprintf(nm, sizeof nm, "%s %s", adj, noun);
        for (j = 0; j < n; j++)
            dup |= !strcmp(out[j], nm);
        if (!dup)
            memcpy(out[n++], nm, QD_SOUND_NAME);
    }
    return n;
}

/* ---------------------------------------------------------------- P1.5t: the seven kinds */
int qd_preset_kind(const int16_t *p)
{
    const int k = qd_sound_kind(p);
    if (k == QD_SK_KICK || k == QD_SK_SNARE || k == QD_SK_CLAP || k == QD_SK_HAT)
        return QD_PK_DRUMS;
    if (k == QD_SK_NOISE)
        return QD_PK_FX;
    if (k == QD_SK_PAD)
        return QD_PK_PAD;
    if (p[QD_QP_DTUN] >= 8000 || p[QD_QP_FDBK] >= 40)  /* (METAL, FEEDBACK: strongly detuned or fed back) */
        return QD_PK_FX;
    switch (k) {
    case QD_SK_BASS: return QD_PK_BASS;
    case QD_SK_KEYS:
    case QD_SK_BELL: return QD_PK_KEYS;
    case QD_SK_PLUCK: return QD_PK_PLUCK;
    default: return QD_PK_LEAD;
    }
}

int qd_preset_tags(const int16_t *patch) { return QD_PKB(qd_preset_kind(patch)); }

int qd_tags_first(int tags)
{
    int k;
    for (k = 0; k < QD_NPK; k++)
        if ((tags >> k) & 1)
            return k;
    return -1;
}

const char *qd_preset_kind_name(int pk)
{
    static const char *const N[QD_NPK] = {"BASS", "KEYS", "PLUCK", "PAD", "LEAD", "DRUMS", "FX"};
    return pk >= 0 && pk < QD_NPK ? N[pk] : "";
}

#include "names.c"                     /* P1.5y: the owner's namesizer (names.h; built here: the source lists stay) */
