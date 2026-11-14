/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's demo song "CARNIVAL": an original tune in the manner of a 1988 home keyboard's demo. Latin FM percussion,
 * a bass that never stops arpeggiating, jazzy chord stabs, and a melody that changes its line and its sound every
 * four bars (make -C tools tour SONG=song_carnival.c; `song` in a tour script, a new project first).
 *
 * 124 BPM, straight sixteenths. Two bars a pattern, each played twice: something new every four bars.
 *   T1 DRUMS   the LATIN kit's kick, snare, hats and cabasa: sound locks (the pool's 1..10)
 *   T2 BASS    SLAP BASS: sixteenths up and down each chord (root, octave, fifth, seventh)
 *   T3 KEYS    DX EP (PERC ORGAN, WURLY): rootless seventh and ninth chords on the off-beats
 *   T4 MELODY  a new line and a new sound each section: STEEL PAN, SQUARE BOY, WHISTLE, KALIMBA, FM REED
 *   T5 BRASS   FM BRASS: hits on the one, answers at the end of the phrase
 *   T6 PERC    clave (3-2), congas, bongo, agogo, timbale fills: sound locks again
 * A01 GROOVE  A02 TUNE  A03 RUNS  A04 BRIDGE  A05 TURN  A06 DRUMS  A07 LIFT (up a minor third)  A08 HOME */
enum { T_DR, T_BA, T_KEYS, T_MEL, T_BRASS, T_PERC };
/* the LATIN kit's ten, in the pack's order (the pool's 1..10) */
enum { K_KICK = 1, K_SNARE, K_TIMBAL, K_CLAVE, K_HAT, K_OPEN, K_CONGA, K_BONGO, K_CABASA, K_AGOGO };

#include "song_common.h"

/* a chord: its root (a pitch class), its third, its seventh and one more colour, in semitones from the root */
typedef struct { int8_t root, third, seventh, colour; } chord_t;
#define MAJ7(r) {r, 4, 11, 14}
#define MIN7(r) {r, 3, 10, 14}
#define DOM9(r) {r, 4, 10, 14}
#define DOM13(r) {r, 4, 10, 21}
#define SUS7(r) {r, 5, 10, 14}

static void hit(qd_pattern_t *p, int t, int s, int k, int vel)
{
    note(p, t, s, 60, vel, L2);
    qd_step_set_sound(p, t, s, k);
}

/* the kit: kick on the one and its push, snare on two and four, hats and the cabasa between */
static void drums(qd_pattern_t *p, int busy)
{
    int b, s;
    qd_track_set_length(p, T_DR, 32);
    p->trk[T_DR].limit = 4;
    for (b = 0; b < 32; b += 16) {
        hit(p, T_DR, b, K_KICK, 120);
        hit(p, T_DR, b + 4, K_SNARE, 104);
        hit(p, T_DR, b + 7, K_KICK, 92);
        hit(p, T_DR, b + 8, K_KICK, 114);
        hit(p, T_DR, b + 12, K_SNARE, 108);
        hit(p, T_DR, b + 15, K_KICK, 86);
        hit(p, T_DR, b + 14, K_OPEN, 74);
        for (s = 2; s < 14; s += 4)
            hit(p, T_DR, b + s, K_HAT, 78);
        hit(p, T_DR, b + 6, K_HAT, 70);
        for (s = 1; s < 16; s += 2)
            if (s != 7 && s != 15)
                hit(p, T_DR, b + s, K_CABASA, busy ? 62 + (s & 2) * 6 : 50);
    }
}

/* the hand percussion: a 3-2 clave, the congas' tumbao round it, the agogo, a timbale fill into the next phrase */
static void perc(qd_pattern_t *p, int fill)
{
    static const uint8_t P[32] = {
        K_CLAVE, 0, K_BONGO, K_AGOGO, K_CONGA, 0, K_CLAVE, K_CONGA, K_AGOGO, 0, K_BONGO, K_AGOGO, K_CLAVE, 0, K_CONGA,
        K_CONGA, K_AGOGO, 0, K_BONGO, 0, K_CLAVE, 0, K_CONGA, K_CONGA, K_CLAVE, 0, K_BONGO, K_AGOGO, K_CONGA, 0,
        K_CONGA, K_CONGA};
    static const uint8_t V[8] = {110, 0, 84, 70, 96, 0, 104, 78};
    int s;
    qd_track_set_length(p, T_PERC, 32);
    p->trk[T_PERC].limit = 4;
    for (s = 0; s < 32; s++)
        if (P[s])
            hit(p, T_PERC, s, P[s], P[s] == K_CLAVE ? 108 : V[s & 7] ? V[s & 7] : 80);
    if (fill) {
        hit(p, T_PERC, 28, K_TIMBAL, 104);
        qd_step_set_notes(p, T_PERC, 28, (const uint8_t[]){64}, 1);
        hit(p, T_PERC, 29, K_TIMBAL, 92);
        qd_step_set_notes(p, T_PERC, 29, (const uint8_t[]){64}, 1);
        hit(p, T_PERC, 30, K_TIMBAL, 110);
        hit(p, T_PERC, 31, K_TIMBAL, 118);
        qd_step_set_ratchet(p, T_PERC, 31, 2, QD_RAMP_UP);
    }
}

/* the bass: sixteenths over each chord's half bar: root, octave, fifth, octave, root, octave, seventh, octave */
static void bass(qd_pattern_t *p, const chord_t *c, int tr)
{
    static const uint8_t V[8] = {116, 84, 98, 84, 106, 84, 98, 90};
    int k, s;
    qd_track_set_length(p, T_BA, 32);
    p->trk[T_BA].limit = 1;
    for (k = 0; k < 4; k++) {
        const int r = 36 + (c[k].root + tr + 120) % 12;
        const int o[8] = {0, 12, 7, 12, 0, 12, c[k].seventh, 12};
        for (s = 0; s < 8; s++)
            note(p, T_BA, k * 8 + s, r + o[s], V[s], L2);
    }
}

/* a chord without its root, its notes folded into lo .. lo + 11 */
static int voicing(const chord_t *c, int tr, int lo, uint8_t *x)
{
    const int iv[4] = {c->third, 7, c->seventh, c->colour};
    int i;
    for (i = 0; i < 4; i++)
        x[i] = (uint8_t)(lo + (c->root + tr + iv[i] - lo % 12 + 240) % 12);
    return 4;
}

/* the keys: each chord stabbed on its off-beats (the montuno's places) */
static void keys(qd_pattern_t *p, const chord_t *c, int tr, int more)
{
    int k;
    qd_track_set_length(p, T_KEYS, 32);
    p->trk[T_KEYS].limit = 4;
    for (k = 0; k < 4; k++) {
        uint8_t x[4];
        voicing(&c[k], tr, 55, x);
        chord(p, T_KEYS, k * 8 + 3, x, 4, 92, L1, 1);
        chord(p, T_KEYS, k * 8 + 6, x, 4, 80, LS2, 1);
        if (more)
            chord(p, T_KEYS, k * 8 + 1, x, 4, 62, L2, 0);
    }
}

/* the brass: a hit on the first chord, an answer on the last */
static void brass(qd_pattern_t *p, const chord_t *c, int tr, int answer)
{
    uint8_t x[4];
    qd_track_set_length(p, T_BRASS, 32);
    p->trk[T_BRASS].limit = 4;
    voicing(&c[0], tr, 62, x);
    chord(p, T_BRASS, 0, x, 4, 112, L2, 0);
    if (answer) {
        voicing(&c[3], tr, 62, x);
        chord(p, T_BRASS, 26, x, 4, 96, L2, 0);
        chord(p, T_BRASS, 28, x, 4, 104, L1, 0);
        chord(p, T_BRASS, 30, x, 4, 116, LS2, 0);
    }
}

/* a line: step, note, length (LS? index), velocity; 255 ends it */
typedef struct { uint8_t s, n, len, vel; } mel_t;
static void melody(qd_pattern_t *p, const mel_t *m, int tr)
{
    qd_track_set_length(p, T_MEL, 32);
    p->trk[T_MEL].limit = 2;
    for (; m->s != 255; m++)
        note(p, T_MEL, m->s, m->n + tr, m->vel, m->len);
}

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

/* ---- the sections' chords (C major; the LIFT plays two of them a minor third up) */
static const chord_t C_HOME[4] = {MAJ7(0), MIN7(9), MIN7(2), DOM13(7)};      /* Cmaj7 Am7 | Dm7 G13 */
static const chord_t C_BRIDGE[4] = {MAJ7(5), MIN7(4), MIN7(2), SUS7(7)};     /* Fmaj7 Em7 | Dm7 G7sus */
static const chord_t C_TURN[4] = {MIN7(9), DOM9(2), MIN7(2), DOM13(7)};      /* Am7 D9 | Dm7 G13 */

enum { P_BASS = 11, P_EP, P_PAN, P_SQUARE, P_WHISTLE, P_KALIMBA, P_REED, P_BRASS, P_ORGAN, P_WURLY, P_PLUCK };
static void sounds(qd_app_t *a, qd_pattern_t *p, const char *mel, int mk, const char *key, int kk)
{
    tsound(a, p, T_DR, "LATIN KICK", K_KICK);
    tsound(a, p, T_BA, "SLAP BASS", P_BASS);
    tsound(a, p, T_KEYS, key, kk);
    tsound(a, p, T_MEL, mel, mk);
    tsound(a, p, T_BRASS, "FM BRASS", P_BRASS);
    tsound(a, p, T_PERC, "LATIN CONGA", K_CONGA);
    p->bpm = 124;
    p->swing = 50;
    p->used = 1;
}

int fizz_tour_song(qd_app_t *a, int argc, char **argv)
{
    static const char *const POOL[] = {"SLAP BASS", "DX EP", "STEEL PAN", "SQUARE BOY", "WHISTLE", "KALIMBA",
                                       "FM REED", "FM BRASS", "PERC ORGAN", "WURLY", "PLUCK BASS"};
    /* level, chorus, delay, reverb, pan */
    static const struct { uint8_t level, chorus, delay, reverb; int8_t pan; } MIX[QD_NTRACK] = {
        {110, 0, 0, 14, 0}, {106, 0, 0, 2, 0}, {100, 45, 6, 30, -14}, {101, 20, 26, 40, 6}, {97, 16, 8, 34, 16},
        {105, 0, 6, 24, 0}};
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
    /* the effects: a bright room, a short synced echo */
    qd_fx_set(QD_FXP_RSIZE, 64), qd_fx_set(QD_FXP_RDECAY, 34), qd_fx_set(QD_FXP_RDAMP, 40), qd_fx_set(QD_FXP_RMOD, 40);
    qd_fx_set(QD_FXP_RPRE, 12), qd_fx_set(QD_FXP_RLOW, 70);
    qd_fx_set(QD_FXP_DSYNC, 1), qd_fx_set(QD_FXP_DDIV, 8), qd_fx_set(QD_FXP_DMODE, 1), qd_fx_set(QD_FXP_DFDBK, 38);

    /* A01 GROOVE: the percussion, the bass, the keys */
    p = qd_proj_pat(&a->proj, 0);
    sounds(a, p, "STEEL PAN", P_PAN, "DX EP", P_EP);
    drums(p, 0), perc(p, 1), bass(p, C_HOME, 0), keys(p, C_HOME, 0, 0);
    /* A02 TUNE: the steel pan's tune, the brass on the one */
    p = qd_proj_pat(&a->proj, 1);
    sounds(a, p, "STEEL PAN", P_PAN, "DX EP", P_EP);
    drums(p, 0), perc(p, 1), bass(p, C_HOME, 0), keys(p, C_HOME, 0, 0), brass(p, C_HOME, 0, 0);
    melody(p, M_TUNE, 0);
    /* A03 RUNS: the same chords, the square lead's busier line, the brass answering */
    p = qd_proj_pat(&a->proj, 2);
    sounds(a, p, "SQUARE BOY", P_SQUARE, "DX EP", P_EP);
    drums(p, 1), perc(p, 1), bass(p, C_HOME, 0), keys(p, C_HOME, 0, 1), brass(p, C_HOME, 0, 1);
    melody(p, M_RUNS, 0);
    /* A04 BRIDGE: Fmaj7 Em7 Dm7 G7sus, the whistle, the organ */
    p = qd_proj_pat(&a->proj, 3);
    sounds(a, p, "WHISTLE", P_WHISTLE, "PERC ORGAN", P_ORGAN);
    drums(p, 0), perc(p, 1), bass(p, C_BRIDGE, 0), keys(p, C_BRIDGE, 0, 0), brass(p, C_BRIDGE, 0, 0);
    melody(p, M_BRIDGE, 0);
    /* A05 TURN: Am7 D9 Dm7 G13, the kalimba high up, everything busy */
    p = qd_proj_pat(&a->proj, 4);
    sounds(a, p, "KALIMBA", P_KALIMBA, "DX EP", P_EP);
    drums(p, 1), perc(p, 1), bass(p, C_TURN, 0), keys(p, C_TURN, 0, 1), brass(p, C_TURN, 0, 1);
    melody(p, M_TURN, 0);
    /* A06 DRUMS: the percussion alone with the bass, the brass's hits */
    p = qd_proj_pat(&a->proj, 5);
    sounds(a, p, "STEEL PAN", P_PAN, "DX EP", P_EP);
    tsound(a, p, T_BA, "PLUCK BASS", P_PLUCK);
    drums(p, 1), perc(p, 1), bass(p, C_HOME, 0), brass(p, C_HOME, 0, 1);
    for (i = 3; i < 28; i += 8) {
        hit(p, T_PERC, i, K_TIMBAL, 100);
        hit(p, T_PERC, i + 2, K_TIMBAL, 108);
        qd_step_set_notes(p, T_PERC, i, (const uint8_t[]){65}, 1);
    }
    /* A07 LIFT: up a minor third (Ebmaj7 Cm7 | Fm7 Bb13): the tune on the reed, the Wurly */
    p = qd_proj_pat(&a->proj, 6);
    sounds(a, p, "FM REED", P_REED, "WURLY", P_WURLY);
    drums(p, 1), perc(p, 1), bass(p, C_HOME, 3), keys(p, C_HOME, 3, 1), brass(p, C_HOME, 3, 1);
    melody(p, M_TUNE, 3);
    /* A08 HOME: back in C, the runs on the steel pan, the last hit */
    p = qd_proj_pat(&a->proj, 7);
    sounds(a, p, "STEEL PAN", P_PAN, "DX EP", P_EP);
    drums(p, 1), perc(p, 1), bass(p, C_HOME, 0), keys(p, C_HOME, 0, 1), brass(p, C_HOME, 0, 1);
    melody(p, M_RUNS, 0);
    return 0;
}
