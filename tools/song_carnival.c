/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's demo song "CARNIVAL": an original tune in the manner of a mid-80s keyboard's demo, the first big thing you
 * hear walking into a neon-lit shopping mall. A brass fanfare, Latin FM percussion, a bass that never stops
 * arpeggiating, jazzy chord stabs, a melody that changes its line and its sound every four bars, and a drum fill
 * into every fifth bar (make -C tools tour SONG=song_carnival.c; `song` in a tour script, a new project first).
 *
 * 120 BPM, straight sixteenths. FOUR bars a pattern (64 steps): three of groove, the fourth a fill.
 *   T1 DRUMS   the MALL kit's kick, gated snare, toms and crash, the LATIN kit's hats and cabasa: sound locks
 *   T2 BASS    SLAP BASS: sixteenths up and down each chord (root, octave, fifth, seventh)
 *   T3 KEYS    DX EP's off-beat stabs (rootless sevenths and ninths), or GLASS AIR / CHAPEL holding the chords
 *   T4 MELODY  a new line and a new sound each section: SOFT SAW, GLOCKEN, STEEL PAN, WHISTLE, KALIMBA
 *   T5 BRASS   FM BRASS: the fanfare, hits on the one, answers, the fills' accents
 *   T6 PERC    clave (3-2), congas, bongo, agogo, timbales: sound locks again
 * A01 FANFARE  A02 GROOVE  A03 TUNE  A04 BRIDGE  A05 TURN  A06 DRUMS  A07 LIFT (the fanfare, up a minor third)
 * A08 HOME  A09 END */
enum { T_DR, T_BA, T_KEYS, T_MEL, T_BRASS, T_PERC };
/* the pool: the LATIN kit 1..10, the MALL kit 11..20 (each in the pack's order) */
enum { K_KICK = 1, K_SNARE, K_TIMBAL, K_CLAVE, K_HAT, K_OPEN, K_CONGA, K_BONGO, K_CABASA, K_AGOGO };
enum { M_KICK = 11, M_SNARE, M_CLAP, M_RIM, M_HAT, M_OPEN, M_TOM, M_BELL, M_TAMB, M_CRASH };

#include "song_common.h"

/* a chord: its root (a pitch class), its third, its seventh and one more colour, in semitones from the root */
typedef struct { int8_t root, third, seventh, colour; } chord_t;
#define MAJ7(r) {r, 4, 11, 14}
#define MIN7(r) {r, 3, 10, 14}
#define DOM9(r) {r, 4, 10, 14}
#define DOM13(r) {r, 4, 10, 21}
#define SUS7(r) {r, 5, 10, 14}

/* a drum: pool sound k on step s, played at note n (60: as it is; the toms and timbales tuned by it) */
static void hitn(qd_pattern_t *p, int t, int s, int k, int vel, int n)
{
    note(p, t, s, n, vel, L2);
    qd_step_set_sound(p, t, s, k);
    if (k == M_SNARE || k == M_TOM || k == M_CLAP)
        qd_lock_set(p, t, s, QD_LK_REVERB, 70);
    if (k == M_CRASH)
        qd_step_set_len(p, t, s, LS8);
}
static void hit(qd_pattern_t *p, int t, int s, int k, int vel) { hitn(p, t, s, k, vel, 60); }

/* a bar of the groove from step b: kick on the one and its pushes, the gated snare on two and four, hats, cabasa */
static void groove_bar(qd_pattern_t *p, int b, int busy, int last)
{
    int s;
    hit(p, T_DR, b, M_KICK, 122);
    hit(p, T_DR, b + 4, M_SNARE, 112);
    hit(p, T_DR, b + 7, M_KICK, 94);
    hit(p, T_DR, b + 8, M_KICK, 116);
    hit(p, T_DR, b + 12, M_SNARE, 116);
    hit(p, T_DR, b + 14, K_OPEN, 78);
    hit(p, T_DR, b + 15, M_KICK, last ? 108 : 86);
    for (s = 2; s < 14; s += 4)
        hit(p, T_DR, b + s, K_HAT, 80);
    hit(p, T_DR, b + 6, K_HAT, 72);
    for (s = 1; s < 14; s += 2)
        if (s != 7)
            hit(p, T_DR, b + s, K_CABASA, busy ? 62 + (s & 2) * 6 : 50);
    if (last)                                           /* the third bar leans into the fill */
        hit(p, T_DR, b + 13, M_SNARE, 84);
}

/* THE FOURTH BAR (steps 48..63): half a bar of groove, then the fill; its accents are the brass's too */
static void fill_bar(qd_pattern_t *p, int kind)
{
    static const uint8_t TOMS[8] = {72, 72, 69, 69, 65, 65, 62, 60};
    int s;
    hit(p, T_DR, 48, M_KICK, 122);
    hit(p, T_DR, 50, K_HAT, 80);
    hit(p, T_DR, 52, M_SNARE, 114);
    hit(p, T_DR, 54, K_HAT, 76);
    hit(p, T_DR, 55, M_KICK, 100);
    switch (kind) {
    case 0:                                             /* a snare roll, rising, the last two doubled */
        for (s = 56; s < 64; s++)
            hit(p, T_DR, s, M_SNARE, 70 + (s - 56) * 7);
        qd_step_set_ratchet(p, T_DR, 62, 2, QD_RAMP_UP);
        qd_step_set_ratchet(p, T_DR, 63, 2, QD_RAMP_UP);
        break;
    case 1:                                             /* the toms, high to low, the kick under the last */
        for (s = 56; s < 64; s++)
            hitn(p, T_DR, s, M_TOM, 96 + (s & 1 ? 0 : 16), TOMS[s - 56]);
        qd_step_set_ratchet(p, T_DR, 59, 2, QD_RAMP_UP);
        break;
    case 2:                                             /* stops: kick and snare together with the brass, 3 + 3 + 2 */
        hit(p, T_DR, 56, M_KICK, 124);
        hit(p, T_DR, 57, M_SNARE, 96);
        hit(p, T_DR, 59, M_KICK, 124);
        hit(p, T_DR, 60, M_SNARE, 100);
        hit(p, T_DR, 62, M_SNARE, 118);
        hit(p, T_DR, 63, M_SNARE, 124);
        qd_step_set_ratchet(p, T_DR, 63, 3, QD_RAMP_UP);
        break;
    default:                                            /* the kit drops out for the timbales (perc), then two flams */
        hit(p, T_DR, 56, M_CLAP, 110);
        hitn(p, T_DR, 60, M_TOM, 112, 67);
        hitn(p, T_DR, 61, M_TOM, 104, 64);
        hitn(p, T_DR, 62, M_TOM, 118, 60);
        hit(p, T_DR, 63, M_SNARE, 124);
        qd_step_set_ratchet(p, T_DR, 63, 2, QD_RAMP_UP);
        break;
    }
}

/* the kit over four bars: a crash on the one (the fill before it lands there), three bars, the fill */
static void drums(qd_pattern_t *p, int busy, int fill)
{
    qd_track_set_length(p, T_DR, 64);
    p->trk[T_DR].limit = 4;
    groove_bar(p, 0, busy, 0);
    groove_bar(p, 16, busy, 0);
    groove_bar(p, 32, busy, 1);
    fill_bar(p, fill);
    hit(p, T_DR, 1, M_CRASH, 100);                      /* (beside the kick: one sound a step) */
    hit(p, T_DR, 30, M_OPEN, 92);                       /* the accents: the end of the second bar .. */
    hit(p, T_DR, 31, M_SNARE, 90);
    hit(p, T_DR, 46, M_KICK, 110);                      /* .. and the third's double kick */
}

/* the hand percussion, two bars of it twice: a 3-2 clave, the congas' tumbao, the agogo; timbales in the fourth bar */
static void perc(qd_pattern_t *p, int fill)
{
    static const uint8_t P[32] = {
        K_CLAVE, 0, K_BONGO, K_AGOGO, K_CONGA, 0, K_CLAVE, K_CONGA, K_AGOGO, 0, K_BONGO, K_AGOGO, K_CLAVE, 0, K_CONGA,
        K_CONGA, K_AGOGO, 0, K_BONGO, 0, K_CLAVE, 0, K_CONGA, K_CONGA, K_CLAVE, 0, K_BONGO, K_AGOGO, K_CONGA, 0,
        K_CONGA, K_CONGA};
    static const uint8_t V[8] = {110, 0, 84, 70, 96, 0, 104, 78};
    static const uint8_t TIMB[8] = {67, 67, 64, 67, 64, 60, 64, 60};
    int s;
    qd_track_set_length(p, T_PERC, 64);
    p->trk[T_PERC].limit = 3;
    for (s = 0; s < 64; s++)
        if (P[s & 31] && !(fill == 3 && s >= 56))
            hit(p, T_PERC, s, P[s & 31], P[s & 31] == K_CLAVE ? 108 : V[s & 7] ? V[s & 7] : 80);
    hit(p, T_PERC, 29, M_BELL, 84);                     /* a cowbell's push into the third bar */
    hit(p, T_PERC, 45, M_TAMB, 90);
    if (fill == 3)                                      /* the timbales' own fill */
        for (s = 56; s < 64; s++)
            hitn(p, T_PERC, s, K_TIMBAL, 96 + (s & 1) * 14, TIMB[s - 56]);
    else {
        hitn(p, T_PERC, 61, K_TIMBAL, 104, 64);
        hitn(p, T_PERC, 63, K_TIMBAL, 116, 60);
    }
}

/* the bass: sixteenths over each chord's half bar: root, octave, fifth, octave, root, octave, seventh, octave */
static void bass(qd_pattern_t *p, const chord_t *c, int tr, int fill)
{
    static const uint8_t V[8] = {116, 84, 98, 84, 106, 84, 98, 90};
    int k, s;
    qd_track_set_length(p, T_BA, 64);
    p->trk[T_BA].limit = 1;
    for (k = 0; k < 8; k++) {
        const int r = 36 + (c[k].root + tr + 120) % 12;
        const int o[8] = {0, 12, 7, 12, 0, 12, c[k].seventh, 12};
        for (s = 0; s < 8; s++)
            if (fill != 2 || k * 8 + s < 56)             /* (the stops: below) */
                note(p, T_BA, k * 8 + s, r + o[s], V[s], L2);
    }
    if (fill == 2) {                                    /* the stops: the bass plays them with the kit */
        const int r = 36 + (c[7].root + tr + 120) % 12;
        note(p, T_BA, 56, r, 120, LS2);
        note(p, T_BA, 59, r, 120, LS2);
        note(p, T_BA, 62, r + 12, 112, L1);
        note(p, T_BA, 63, r + 7, 116, L1);
    }
}

/* a chord without its root, its notes folded into lo .. lo + 11 */
static void voicing(const chord_t *c, int tr, int lo, uint8_t *x)
{
    const int iv[4] = {c->third, 7, c->seventh, c->colour};
    int i;
    for (i = 0; i < 4; i++)
        x[i] = (uint8_t)(lo + (c->root + tr + iv[i] - lo % 12 + 240) % 12);
}

/* the keys: each chord stabbed on its off-beats (the montuno's places) */
static void keys(qd_pattern_t *p, const chord_t *c, int tr, int more)
{
    int k;
    qd_track_set_length(p, T_KEYS, 64);
    p->trk[T_KEYS].limit = 4;
    for (k = 0; k < 8; k++) {
        uint8_t x[4];
        voicing(&c[k], tr, 55, x);
        chord(p, T_KEYS, k * 8 + 3, x, 4, 92, L1, 1);
        chord(p, T_KEYS, k * 8 + 6, x, 4, 80, LS2, 1);
        if (more)
            chord(p, T_KEYS, k * 8 + 1, x, 4, 62, L2, 0);
    }
}
/* .. or held: a pad under the big sections */
static void pad(qd_pattern_t *p, const chord_t *c, int tr)
{
    int k;
    qd_track_set_length(p, T_KEYS, 64);
    p->trk[T_KEYS].limit = 4;
    for (k = 0; k < 8; k++) {
        uint8_t x[4];
        voicing(&c[k], tr, 57, x);
        chord(p, T_KEYS, k * 8, x, 4, 88, LS8, 2);
    }
}

/* the brass: a hit on each phrase's one, an answer at the end of the second bar, the fill's accents */
static void brass(qd_pattern_t *p, const chord_t *c, int tr, int fill)
{
    uint8_t x[4];
    qd_track_set_length(p, T_BRASS, 64);
    p->trk[T_BRASS].limit = 4;
    voicing(&c[0], tr, 62, x);
    chord(p, T_BRASS, 0, x, 4, 114, L2, 0);
    voicing(&c[3], tr, 62, x);
    chord(p, T_BRASS, 26, x, 4, 96, L2, 0);
    chord(p, T_BRASS, 28, x, 4, 104, L1, 0);
    chord(p, T_BRASS, 30, x, 4, 116, LS2, 0);
    voicing(&c[4], tr, 62, x);
    chord(p, T_BRASS, 32, x, 4, 108, L2, 0);
    voicing(&c[7], tr, 62, x);
    if (fill == 2) {                                    /* the stops, with the kit: 3 + 3 + 2 */
        chord(p, T_BRASS, 56, x, 4, 118, LS2, 0);
        chord(p, T_BRASS, 59, x, 4, 118, LS2, 0);
        chord(p, T_BRASS, 62, x, 4, 124, LS2, 0);
    } else {
        chord(p, T_BRASS, 60, x, 4, 104, L1, 0);
        chord(p, T_BRASS, 62, x, 4, 120, LS2, 0);
    }
}

/* a line: step, note, length (LS? index), velocity; 255 ends it. Placed from step `at`, `tr` semitones up */
typedef struct { uint8_t s, n, len, vel; } mel_t;
static void line(qd_pattern_t *p, int t, const mel_t *m, int at, int tr, int cut)
{
    qd_track_set_length(p, t, 64);
    for (; m->s != 255; m++)
        if (at + m->s < cut)
            note(p, t, at + m->s, m->n + tr, m->vel, m->len);
}

/* THE FANFARE (four bars): up through the chord, a held top, the answer; its last bar is the fill's */
static const mel_t M_FAN[] = {{0, 67, LS2, 112}, {2, 72, LS2, 108}, {4, 74, LS2, 110}, {6, 76, LS6, 118},
    {12, 79, LS2, 112}, {14, 76, LS2, 104}, {16, 74, LS6, 114}, {22, 72, LS2, 100}, {24, 74, LS4, 110},
    {28, 67, LS4, 104}, {32, 69, LS2, 112}, {34, 72, LS2, 108}, {36, 76, LS2, 112}, {38, 81, LS6, 122},
    {44, 79, LS2, 112}, {46, 76, LS2, 106}, {48, 79, LS8, 120}, {255, 0, 0, 0}};
/* the two-bar lines */
static const mel_t M_TUNE[] = {{0, 76, L1, 108}, {2, 79, L1, 96}, {3, 76, L1, 90}, {5, 72, L1, 92}, {6, 74, L1, 96},
    {8, 76, LS2, 106}, {11, 72, L1, 90}, {13, 69, L1, 88}, {14, 72, LS2, 100}, {16, 74, LS2, 106}, {18, 77, L1, 94},
    {19, 81, L1, 104}, {21, 77, L1, 90}, {22, 74, L1, 92}, {24, 71, LS2, 100}, {27, 74, L1, 92}, {28, 79, LS2, 108},
    {30, 77, L1, 92}, {31, 74, L1, 88}, {255, 0, 0, 0}};
static const mel_t M_RUNS[] = {{0, 79, LS2, 110}, {3, 81, L1, 96}, {4, 79, L1, 92}, {6, 76, L1, 90}, {7, 79, L1, 96},
    {8, 81, LS2, 108}, {10, 79, L1, 92}, {11, 76, L1, 90}, {12, 72, LS2, 100}, {14, 76, L1, 92}, {15, 79, L1, 98},
    {16, 81, LS2, 110}, {19, 77, L1, 92}, {20, 74, L1, 90}, {22, 77, L1, 94}, {23, 81, L1, 100}, {24, 79, LS3, 108},
    {27, 77, L1, 92}, {28, 74, L1, 92}, {29, 71, L1, 88}, {30, 74, L1, 94}, {31, 79, L1, 104}, {255, 0, 0, 0}};
static const mel_t M_BRIDGE[] = {{0, 81, LS3, 108}, {3, 79, L1, 92}, {4, 77, L1, 94}, {6, 72, LS2, 100},
    {8, 79, LS3, 106}, {11, 76, L1, 92}, {12, 74, L1, 94}, {14, 71, LS2, 98}, {16, 77, LS2, 106}, {18, 76, L1, 90},
    {19, 74, L1, 92}, {20, 69, LS2, 98}, {22, 74, L1, 92}, {23, 77, L1, 96}, {24, 79, LS2, 108}, {26, 81, L1, 98},
    {27, 79, L1, 92}, {28, 77, L1, 94}, {30, 74, LS2, 102}, {255, 0, 0, 0}};
static const mel_t M_TURN[] = {{0, 84, LS2, 112}, {2, 83, L1, 94}, {3, 81, L1, 96}, {4, 79, L1, 94}, {5, 76, L1, 90},
    {6, 79, LS2, 100}, {8, 78, LS2, 106}, {10, 81, L1, 94}, {11, 84, L1, 102}, {12, 81, LS2, 100}, {14, 78, LS2, 96},
    {16, 77, LS2, 106}, {18, 81, L1, 94}, {19, 84, L1, 100}, {20, 86, LS2, 112}, {22, 84, L1, 96}, {23, 81, L1, 94},
    {24, 83, LS2, 108}, {26, 79, L1, 92}, {27, 76, L1, 90}, {28, 77, L1, 94}, {29, 74, L1, 90}, {30, 71, L1, 92},
    {31, 74, L1, 98}, {255, 0, 0, 0}};

/* a bell's arpeggio: eighths up and down each chord, high */
static void arp(qd_pattern_t *p, const chord_t *c, int tr)
{
    int k, s;
    qd_track_set_length(p, T_MEL, 64);
    for (k = 0; k < 8; k++) {
        uint8_t x[4];
        static const uint8_t O[4] = {0, 2, 3, 1};
        voicing(&c[k], tr, 76, x);
        for (s = 0; s < 4; s++)
            if (k * 8 + s * 2 < 56)
                note(p, T_MEL, k * 8 + s * 2, x[O[s]], s ? 84 : 102, LS2);
    }
}

/* ---- the chords, half a bar each (C major; the LIFT plays HOME a minor third up) */
static const chord_t C_HOME[8] = {MAJ7(0), MIN7(9), MIN7(2), DOM13(7), MAJ7(0), MIN7(9), MIN7(2), DOM13(7)};
static const chord_t C_BRIDGE[8] = {MAJ7(5), MIN7(4), MIN7(2), SUS7(7), MIN7(9), DOM9(2), MIN7(2), DOM13(7)};

enum { P_BASS = 21, P_EP, P_PAN, P_SAW, P_WHISTLE, P_KALIMBA, P_GLOCK, P_BRASS, P_AIR, P_CHAPEL, P_PLUCK, P_SQUARE };
static void sounds(qd_app_t *a, qd_pattern_t *p, const char *mel, int mk, const char *key, int kk)
{
    tsound(a, p, T_DR, "MALL KICK", M_KICK);
    tsound(a, p, T_BA, "SLAP BASS", P_BASS);
    tsound(a, p, T_KEYS, key, kk);
    tsound(a, p, T_MEL, mel, mk);
    tsound(a, p, T_BRASS, "FM BRASS", P_BRASS);
    tsound(a, p, T_PERC, "LATIN CONGA", K_CONGA);
    p->trk[T_MEL].limit = 2;
    p->bpm = 120;
    p->swing = 50;
    p->used = 1;
}

int fizz_tour_song(qd_app_t *a, int argc, char **argv)
{
    static const char *const POOL[] = {"SLAP BASS", "DX EP", "STEEL PAN", "SOFT SAW", "WHISTLE", "KALIMBA", "GLOCKEN",
                                       "FM BRASS", "GLASS AIR", "CHAPEL", "PLUCK BASS", "SQUARE BOY"};
    /* level, chorus, delay, reverb, pan */
    static const struct { uint8_t level, chorus, delay, reverb; int8_t pan; } MIX[QD_NTRACK] = {
        {108, 0, 0, 20, 0}, {106, 0, 0, 2, 0}, {99, 55, 8, 46, -14}, {103, 30, 30, 56, 6}, {103, 30, 12, 50, 16},
        {104, 0, 8, 30, 0}};
    static const char *const TNAME[QD_NTRACK] = {"DRUMS", "BASS", "KEYS", "MELODY", "BRASS", "PERC"};
    static const uint8_t TKIND[QD_NTRACK] = {QD_PK_DRUMS, QD_PK_BASS, QD_PK_KEYS, QD_PK_LEAD, QD_PK_LEAD, QD_PK_DRUMS};
    qd_pattern_t *p;
    int i, n, t;
    (void)argc, (void)argv;
    n = pack_store(a);
    if (n < 0) {
        fprintf(stderr, "song: MY PRESETS is full\n");
        return 1;
    }
    printf("song: %d presets stored (%d in the pack)\n", n, FIZZ_PACK_N);
    for (i = 0; i < 10; i++)
        pool_add(a, pack_find("LATIN KICK") + i);
    for (i = 0; i < 10; i++)
        pool_add(a, pack_find("MALL KICK") + i);
    for (i = 0; i < (int)(sizeof POOL / sizeof POOL[0]); i++)
        if (pool_add(a, pack_find(POOL[i])) != P_BASS + i) {
            fprintf(stderr, "song: the pool is not empty (a new project first)\n");
            return 1;
        }
    for (t = 0; t < QD_NTRACK; t++) {
        qd_mix_track_t *m = &a->model.t[t].mix;
        m->level = MIX[t].level, m->chorus = MIX[t].chorus, m->delay = MIX[t].delay, m->reverb = MIX[t].reverb;
        m->pan = MIX[t].pan;
        memset(a->model.t[t].name, 0, sizeof a->model.t[t].name);
        strncpy(a->model.t[t].name, TNAME[t], sizeof a->model.t[t].name - 1);
        a->model.t[t].kind = TKIND[t], a->model.t[t].named = 1;
    }
    /* the effects: the mall's atrium (a big bright hall), a synced echo on the melody */
    qd_fx_set(QD_FXP_RSIZE, 104), qd_fx_set(QD_FXP_RDECAY, 50), qd_fx_set(QD_FXP_RDAMP, 34), qd_fx_set(QD_FXP_RMOD, 50);
    qd_fx_set(QD_FXP_RPRE, 36), qd_fx_set(QD_FXP_RLOW, 80);
    qd_fx_set(QD_FXP_DSYNC, 1), qd_fx_set(QD_FXP_DDIV, 8), qd_fx_set(QD_FXP_DMODE, 1), qd_fx_set(QD_FXP_DFDBK, 44);

    /* A01 FANFARE: the brass's theme with the saw an octave over it, the pad, the kit's hits and the toms */
    p = qd_proj_pat(&a->proj, 0);
    sounds(a, p, "SOFT SAW", P_SAW, "GLASS AIR", P_AIR);
    pad(p, C_HOME, 0);
    line(p, T_BRASS, M_FAN, 0, 0, 64);
    p->trk[T_BRASS].limit = 3;
    line(p, T_MEL, M_FAN, 0, 12, 64);
    bass(p, C_HOME, 0, 2);
    qd_track_set_length(p, T_DR, 64);
    p->trk[T_DR].limit = 4;
    for (i = 0; i < 48; i += 16) {
        hit(p, T_DR, i, M_KICK, 124);
        hit(p, T_DR, i + 1, M_CRASH, i ? 80 : 108);
        hit(p, T_DR, i + 6, M_KICK, 110);
        hit(p, T_DR, i + 8, M_SNARE, 118);
        hitn(p, T_DR, i + 12, M_TOM, 104, 65);
        hitn(p, T_DR, i + 14, M_TOM, 110, 60);
    }
    fill_bar(p, 1);
    perc(p, 0);

    /* A02 GROOVE: the percussion, the bass, the stabs, a glockenspiel up and down the chords; a snare roll */
    p = qd_proj_pat(&a->proj, 1);
    sounds(a, p, "GLOCKEN", P_GLOCK, "DX EP", P_EP);
    drums(p, 0, 0), perc(p, 0), bass(p, C_HOME, 0, 0), keys(p, C_HOME, 0, 0), brass(p, C_HOME, 0, 0);
    arp(p, C_HOME, 0);

    /* A03 TUNE: the steel pan's tune, then its runs; the stops */
    p = qd_proj_pat(&a->proj, 2);
    sounds(a, p, "STEEL PAN", P_PAN, "DX EP", P_EP);
    drums(p, 0, 2), perc(p, 2), bass(p, C_HOME, 0, 2), keys(p, C_HOME, 0, 0), brass(p, C_HOME, 0, 2);
    line(p, T_MEL, M_TUNE, 0, 0, 64), line(p, T_MEL, M_RUNS, 32, 0, 56);

    /* A04 BRIDGE: Fmaj7 Em7 Dm7 G7sus, Am7 D9 Dm7 G13: the whistle over the pad; the toms */
    p = qd_proj_pat(&a->proj, 3);
    sounds(a, p, "WHISTLE", P_WHISTLE, "CHAPEL", P_CHAPEL);
    drums(p, 0, 1), perc(p, 1), bass(p, C_BRIDGE, 0, 1), pad(p, C_BRIDGE, 0), brass(p, C_BRIDGE, 0, 1);
    line(p, T_MEL, M_BRIDGE, 0, 0, 64), line(p, T_MEL, M_TURN, 32, 0, 56);

    /* A05 TURN: the bridge again, busy: the kalimba an octave up, the stabs; the timbales' fill */
    p = qd_proj_pat(&a->proj, 4);
    sounds(a, p, "KALIMBA", P_KALIMBA, "DX EP", P_EP);
    drums(p, 1, 3), perc(p, 3), bass(p, C_BRIDGE, 0, 3), keys(p, C_BRIDGE, 0, 1), brass(p, C_BRIDGE, 0, 3);
    line(p, T_MEL, M_TURN, 0, 0, 64), line(p, T_MEL, M_BRIDGE, 32, 12, 56);

    /* A06 DRUMS: the percussion and the bass alone, the brass's hits, timbales all through; the stops */
    p = qd_proj_pat(&a->proj, 5);
    sounds(a, p, "STEEL PAN", P_PAN, "DX EP", P_EP);
    tsound(a, p, T_BA, "PLUCK BASS", P_PLUCK);
    drums(p, 1, 2), perc(p, 2), bass(p, C_HOME, 0, 2), brass(p, C_HOME, 0, 2);
    for (i = 3; i < 56; i += 8) {
        hitn(p, T_PERC, i, K_TIMBAL, 100, 65);
        hitn(p, T_PERC, i + 2, K_TIMBAL, 110, 60);
    }

    /* A07 LIFT: up a minor third (Ebmaj7 Cm7 Fm7 Bb13): the fanfare again over the whole band; the toms */
    p = qd_proj_pat(&a->proj, 6);
    sounds(a, p, "SOFT SAW", P_SAW, "GLASS AIR", P_AIR);
    drums(p, 1, 1), perc(p, 1), bass(p, C_HOME, 3, 1), pad(p, C_HOME, 3);
    line(p, T_BRASS, M_FAN, 0, 3, 64);
    p->trk[T_BRASS].limit = 3;
    line(p, T_MEL, M_FAN, 0, 15, 64);

    /* A08 HOME: back in C: the runs, then the tune, on the square lead; the snare roll into the last chord */
    p = qd_proj_pat(&a->proj, 7);
    sounds(a, p, "SQUARE BOY", P_SQUARE, "DX EP", P_EP);
    drums(p, 1, 0), perc(p, 0), bass(p, C_HOME, 0, 0), keys(p, C_HOME, 0, 1), brass(p, C_HOME, 0, 0);
    line(p, T_MEL, M_RUNS, 0, 0, 64), line(p, T_MEL, M_TUNE, 32, 0, 56);

    /* A09 END (one bar): the last chord, held: C6/9 on everything, the crash */
    p = qd_proj_pat(&a->proj, 8);
    sounds(a, p, "SOFT SAW", P_SAW, "GLASS AIR", P_AIR);
    {
        static const uint8_t TOP[4] = {64, 67, 69, 74}, PADS[4] = {55, 60, 64, 69};
        qd_track_set_length(p, T_BRASS, 16), qd_track_set_length(p, T_KEYS, 16), qd_track_set_length(p, T_BA, 16);
        qd_track_set_length(p, T_MEL, 16), qd_track_set_length(p, T_DR, 16);
        chord(p, T_BRASS, 0, TOP, 4, 124, LS12, 0);
        chord(p, T_KEYS, 0, PADS, 4, 100, LS12, 2);
        note(p, T_BA, 0, 36, 124, LS12);
        note(p, T_MEL, 0, 84, 116, LS12);
        hit(p, T_DR, 0, M_KICK, 127);
        hit(p, T_DR, 1, M_CRASH, 120);
    }
    p->master = 16;
    return 0;
}
