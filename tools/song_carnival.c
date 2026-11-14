/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's demo song "CARNIVAL": an original tune for the first big thing you hear walking into a neon-lit shopping
 * mall in 1985: part baroque rock, part stadium guitars, part jazz-funk. It starts on a bass riff and a lead guitar
 * solo; the guitar comes back in thirds, a harpsichord races under it, a clavinet chops the chords, and the kit fills
 * into every fifth bar (make -C tools tour SONG=song_carnival.c; `song` in a tour script, a new project first).
 *
 * 116 BPM, straight sixteenths, E minor. FOUR bars a pattern (64 steps): three of groove, the fourth a fill.
 *   T1 DRUMS   the MALL kit's kick, gated snare, toms and crash, the LATIN kit's hats and cabasa: sound locks
 *   T2 BASS    SYNTH BASS: the riff on each chord's root (octaves, the fifth, the seventh, pushed)
 *   T3 KEYS    FUNK CLAV's chops, HARPSICHORD's sixteenths, or a pad (CHAPEL, GLASS AIR) holding the chords
 *   T4 LEAD    LEAD GUITAR: the solo and the anthem, alone or in thirds; SOFT SAW / SQUARE BOY: the synth's funk
 *   T5 BRASS   FM BRASS: stabs (minor ninths, B7 sharp 9), answers, the fills' accents
 *   T6 PERC    tambourine, cowbell, a conga, timbales in the fills: sound locks again
 * A01 RIFF  A02 TWO GUITARS  A03 FUNK  A04 ANTHEM  A05 ANTHEM IN THIRDS  A06 BREAK  A07 LIFT (a tone up)  A08 HOME
 * A09 END */
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
#define SHARP9(r) {r, 4, 10, 15}

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

/* the percussion: a tambourine on the off-beats, a cowbell on the beat when it is busy, a conga's funk under it,
 * timbales in the fourth bar */
static void perc(qd_pattern_t *p, int fill, int busy)
{
    static const uint8_t TIMB[8] = {67, 67, 64, 67, 64, 60, 64, 60};
    int s;
    qd_track_set_length(p, T_PERC, 64);
    p->trk[T_PERC].limit = 3;
    for (s = 0; s < (fill == 3 ? 56 : 60); s++) {
        if ((s & 3) == 2)
            hit(p, T_PERC, s, M_TAMB, 84);
        else if (busy && (s & 3) == 0)
            hit(p, T_PERC, s, M_BELL, (s & 15) ? 76 : 92);
        else if ((s & 15) == 7 || (s & 15) == 11)
            hit(p, T_PERC, s, K_CONGA, 88);
        else if ((s & 15) == 13)
            hit(p, T_PERC, s, K_BONGO, 80);
    }
    if (fill == 3)                                      /* the timbales' own fill */
        for (s = 56; s < 64; s++)
            hitn(p, T_PERC, s, K_TIMBAL, 96 + (s & 1) * 14, TIMB[s - 56]);
    else {
        hitn(p, T_PERC, 61, K_TIMBAL, 104, 64);
        hitn(p, T_PERC, 63, K_TIMBAL, 116, 60);
    }
}

/* THE BASS LINE: a riff of two half-bar cells on each chord's root: octaves, the fifth and the seventh, pushed */
static void bass(qd_pattern_t *p, const chord_t *c, int tr, int fill)
{
    int k;
    qd_track_set_length(p, T_BA, 64);
    p->trk[T_BA].limit = 1;
    for (k = 0; k < 8; k++) {
        const int r = 28 + (c[k].root + tr + 8 + 120) % 12, b = k * 8;   /* E1 .. D#2 */
        if (fill == 2 && k == 7)
            break;
        if (!(k & 1)) {
            note(p, T_BA, b, r, 124, L1);
            note(p, T_BA, b + 2, r + 12, 96, L2);
            note(p, T_BA, b + 3, r, 110, L1);
            note(p, T_BA, b + 5, r + 7, 92, L2);
            note(p, T_BA, b + 6, r + c[k].seventh, 104, L1);
            note(p, T_BA, b + 7, r + 12, 98, L2);
        } else {
            note(p, T_BA, b, r, 120, L1);
            note(p, T_BA, b + 1, r, 76, L2);
            note(p, T_BA, b + 3, r + 12, 104, L2);
            note(p, T_BA, b + 4, r + c[k].seventh, 108, LS2);
            note(p, T_BA, b + 6, r + 7, 96, L1);
            note(p, T_BA, b + 7, r + 12, 100, L2);
        }
    }
    if (fill == 2) {                                    /* the stops: the bass plays them with the kit */
        const int r = 28 + (c[7].root + tr + 8 + 120) % 12;
        note(p, T_BA, 56, r, 124, LS2);
        note(p, T_BA, 59, r, 124, LS2);
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

/* the clavinet: each chord chopped on the sixteenths, funk's places */
static void clav(qd_pattern_t *p, const chord_t *c, int tr)
{
    int k;
    qd_track_set_length(p, T_KEYS, 64);
    p->trk[T_KEYS].limit = 4;
    for (k = 0; k < 8; k++) {
        uint8_t x[4];
        voicing(&c[k], tr, 52, x);
        chord(p, T_KEYS, k * 8 + 2, x, 4, 100, L2, 0);
        chord(p, T_KEYS, k * 8 + 3, x, 4, 76, L2, 0);
        chord(p, T_KEYS, k * 8 + 6, x, 4, 96, L1, 0);
        if (k & 1)
            chord(p, T_KEYS, k * 8 + 5, x, 4, 70, L2, 0);
    }
}
/* the harpsichord: sixteenths through each chord, the baroque engine under the rock */
static void harpsi(qd_pattern_t *p, const chord_t *c, int tr)
{
    static const uint8_t O[8] = {0, 2, 1, 2, 3, 2, 1, 2};
    int k, s;
    qd_track_set_length(p, T_KEYS, 64);
    p->trk[T_KEYS].limit = 4;
    for (k = 0; k < 8; k++) {
        const int r = 64 + (c[k].root + tr + 8 + 120) % 12;   /* E4 .. D#5 */
        const int tone[4] = {r, r + c[k].third, r + 7, r + 12};
        for (s = 0; s < 8; s++)
            if (k * 8 + s < 60)
                note(p, T_KEYS, k * 8 + s, tone[O[s]], s & 3 ? 78 : 100, L1);
    }
}
/* .. or held: a pad under the anthem */
static void pad(qd_pattern_t *p, const chord_t *c, int tr)
{
    int k;
    qd_track_set_length(p, T_KEYS, 64);
    p->trk[T_KEYS].limit = 4;
    for (k = 0; k < 8; k++) {
        uint8_t x[4];
        voicing(&c[k], tr, 55, x);
        chord(p, T_KEYS, k * 8, x, 4, 88, LS8, 2);
    }
}

/* the brass: a stab on each phrase's one, a funk answer at the end of the second bar, the fill's accents */
static void brass(qd_pattern_t *p, const chord_t *c, int tr, int fill)
{
    uint8_t x[4];
    qd_track_set_length(p, T_BRASS, 64);
    p->trk[T_BRASS].limit = 4;
    voicing(&c[0], tr, 62, x);
    chord(p, T_BRASS, 0, x, 4, 114, L2, 0);
    chord(p, T_BRASS, 11, x, 4, 96, L2, 0);
    voicing(&c[3], tr, 62, x);
    chord(p, T_BRASS, 27, x, 4, 100, L2, 0);
    chord(p, T_BRASS, 28, x, 4, 108, L1, 0);
    chord(p, T_BRASS, 30, x, 4, 118, LS2, 0);
    voicing(&c[4], tr, 62, x);
    chord(p, T_BRASS, 32, x, 4, 110, L2, 0);
    chord(p, T_BRASS, 43, x, 4, 96, L2, 0);
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

/* a line: step, note, length (LS? index), velocity; 255 ends it. Placed on track t from step `at`, `tr` semitones up,
 * the steps before `cut`; harm: a second voice a third under it, in E minor (the two guitars) */
typedef struct { uint8_t s, n, len, vel; } mel_t;
static int third_under(int n)
{
    static const uint8_t E_MINOR[7] = {4, 6, 7, 9, 11, 0, 2};
    int d, k;
    for (d = 3; d <= 4; d++)
        for (k = 0; k < 7; k++)
            if ((n - d) % 12 == E_MINOR[k])
                return n - d;
    return n - 3;
}
static void line(qd_pattern_t *p, int t, const mel_t *m, int at, int tr, int cut, int harm)
{
    qd_track_set_length(p, t, 64);
    p->trk[t].limit = 2;
    for (; m->s != 255; m++)
        if (at + m->s < cut) {
            uint8_t x[2];
            x[0] = (uint8_t)(m->n + tr), x[1] = (uint8_t)(third_under(m->n) + tr);
            chord(p, t, at + m->s, x, harm ? 2 : 1, m->vel, m->len, 0);
        }
}

/* THE SOLO (four bars, E minor): a bent-up opening, the held top, a run up to the scream, down to the fifth of B */
static const mel_t M_SOLO[] = {{0, 71, LS4, 116}, {4, 74, LS2, 108}, {6, 76, LS6, 122}, {12, 79, LS2, 112},
    {14, 76, LS2, 104}, {16, 79, LS4, 118}, {20, 78, LS2, 104}, {22, 76, LS2, 104}, {24, 78, LS4, 114},
    {28, 81, LS2, 112}, {30, 78, L1, 100}, {31, 74, L1, 98}, {32, 76, L1, 108}, {33, 79, L1, 104}, {34, 81, L1, 108},
    {35, 83, L1, 112}, {36, 86, LS4, 126}, {40, 83, LS2, 110}, {42, 81, L1, 102}, {43, 79, L1, 100},
    {44, 76, LS2, 108}, {46, 79, LS2, 106}, {48, 78, LS4, 116}, {52, 75, LS2, 106}, {54, 78, LS2, 110},
    {56, 83, LS8, 124}, {255, 0, 0, 0}};
/* THE ANTHEM (four bars over Am D G C, Am, B): long notes, climbing */
static const mel_t M_THEME[] = {{0, 72, LS4, 112}, {4, 76, LS2, 104}, {6, 81, LS6, 120}, {12, 79, LS2, 108},
    {14, 78, LS2, 104}, {16, 79, LS4, 114}, {20, 83, LS2, 108}, {22, 86, LS4, 122}, {26, 84, LS2, 108},
    {28, 83, LS2, 106}, {30, 79, LS2, 104}, {32, 81, LS4, 114}, {36, 84, LS2, 108}, {38, 88, LS6, 126},
    {44, 86, LS2, 110}, {46, 84, LS2, 106}, {48, 83, LS8, 120}, {56, 78, LS8, 116}, {255, 0, 0, 0}};
/* THE SYNTH'S FUNK (two bars): the pentatonic and its blue note, off the beat */
static const mel_t M_SYNTH[] = {{0, 76, L1, 110}, {1, 79, L1, 98}, {3, 81, L1, 104}, {4, 82, L2, 100}, {5, 81, L1, 102},
    {7, 79, L1, 96}, {8, 76, LS2, 108}, {11, 74, L1, 96}, {12, 76, L1, 104}, {14, 79, L1, 98}, {15, 81, L1, 104},
    {16, 83, LS2, 112}, {19, 81, L1, 98}, {20, 79, L1, 100}, {22, 76, L1, 98}, {23, 74, L1, 96}, {24, 76, LS3, 110},
    {27, 79, L1, 98}, {28, 81, L1, 102}, {29, 82, L1, 100}, {30, 83, L1, 106}, {31, 86, L1, 114}, {255, 0, 0, 0}};

/* ---- the chords, half a bar each (E minor; the LIFT plays HOME a whole tone up) */
static const chord_t C_HOME[8] = {MIN7(4), MIN7(4), MAJ7(0), DOM9(2), MIN7(4), MIN7(4), SUS7(11), SHARP9(11)};
static const chord_t C_ANTHEM[8] = {MIN7(9), DOM9(2), MAJ7(7), MAJ7(0), MIN7(9), MIN7(9), SUS7(11), SHARP9(11)};

enum { P_BASS = 21, P_CLAV, P_GUITAR, P_SAW, P_HARPSI, P_BRASS, P_AIR, P_CHAPEL, P_SQUARE, P_ROUND };
static void sounds(qd_app_t *a, qd_pattern_t *p, const char *mel, int mk, const char *key, int kk)
{
    tsound(a, p, T_DR, "MALL KICK", M_KICK);
    tsound(a, p, T_BA, "SYNTH BASS", P_BASS);
    tsound(a, p, T_KEYS, key, kk);
    tsound(a, p, T_MEL, mel, mk);
    tsound(a, p, T_BRASS, "FM BRASS", P_BRASS);
    tsound(a, p, T_PERC, "MALL TAMB", M_TAMB);
    p->bpm = 116;
    p->swing = 50;
    p->used = 1;
}

int fizz_tour_song(qd_app_t *a, int argc, char **argv)
{
    static const char *const POOL[] = {"SYNTH BASS", "FUNK CLAV", "LEAD GUITAR", "SOFT SAW", "HARPSICHORD", "FM BRASS",
                                       "GLASS AIR", "CHAPEL", "SQUARE BOY", "ROUND BASS"};
    /* level, chorus, delay, reverb, pan */
    static const struct { uint8_t level, chorus, delay, reverb; int8_t pan; } MIX[QD_NTRACK] = {
        {108, 0, 0, 20, 0}, {111, 0, 0, 2, 0}, {104, 30, 8, 36, -14}, {103, 24, 40, 56, 6}, {98, 30, 12, 46, 16},
        {102, 0, 8, 26, 0}};
    static const char *const TNAME[QD_NTRACK] = {"DRUMS", "BASS", "KEYS", "LEAD", "BRASS", "PERC"};
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
    /* the effects: the mall's atrium (a big bright hall), a synced echo behind the lead */
    qd_fx_set(QD_FXP_RSIZE, 104), qd_fx_set(QD_FXP_RDECAY, 50), qd_fx_set(QD_FXP_RDAMP, 34), qd_fx_set(QD_FXP_RMOD, 50);
    qd_fx_set(QD_FXP_RPRE, 36), qd_fx_set(QD_FXP_RLOW, 80);
    qd_fx_set(QD_FXP_DSYNC, 1), qd_fx_set(QD_FXP_DDIV, 8), qd_fx_set(QD_FXP_DMODE, 1), qd_fx_set(QD_FXP_DFDBK, 48);

    /* A01 RIFF: the bass line and the guitar's solo from the first beat, the kit, the brass's stabs; the toms */
    p = qd_proj_pat(&a->proj, 0);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "FUNK CLAV", P_CLAV);
    drums(p, 0, 1), perc(p, 1, 0), bass(p, C_HOME, 0, 1), brass(p, C_HOME, 0, 1);
    line(p, T_MEL, M_SOLO, 0, 0, 64, 0);

    /* A02 TWO GUITARS: the solo again in thirds, the harpsichord's sixteenths under it; the stops */
    p = qd_proj_pat(&a->proj, 1);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "HARPSICHORD", P_HARPSI);
    drums(p, 1, 2), perc(p, 2, 1), bass(p, C_HOME, 0, 2), harpsi(p, C_HOME, 0), brass(p, C_HOME, 0, 2);
    line(p, T_MEL, M_SOLO, 0, 0, 64, 1);

    /* A03 FUNK: the clavinet's chops, the synth's line (and again an octave up); the snare roll */
    p = qd_proj_pat(&a->proj, 2);
    sounds(a, p, "SOFT SAW", P_SAW, "FUNK CLAV", P_CLAV);
    drums(p, 1, 0), perc(p, 0, 1), bass(p, C_HOME, 0, 0), clav(p, C_HOME, 0), brass(p, C_HOME, 0, 0);
    line(p, T_MEL, M_SYNTH, 0, 0, 64, 0), line(p, T_MEL, M_SYNTH, 32, 0, 56, 0);
    p->trk[T_MEL].limit = 1;

    /* A04 ANTHEM: Am D G C, Am, B: the guitar's long theme over the pad; the toms */
    p = qd_proj_pat(&a->proj, 3);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "CHAPEL", P_CHAPEL);
    drums(p, 0, 1), perc(p, 1, 0), bass(p, C_ANTHEM, 0, 1), pad(p, C_ANTHEM, 0), brass(p, C_ANTHEM, 0, 1);
    line(p, T_MEL, M_THEME, 0, 0, 64, 0);

    /* A05 ANTHEM, TWO GUITARS: the theme in thirds, the harpsichord racing under it; the timbales */
    p = qd_proj_pat(&a->proj, 4);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "HARPSICHORD", P_HARPSI);
    drums(p, 1, 3), perc(p, 3, 1), bass(p, C_ANTHEM, 0, 3), harpsi(p, C_ANTHEM, 0), brass(p, C_ANTHEM, 0, 3);
    line(p, T_MEL, M_THEME, 0, 0, 64, 1);

    /* A06 BREAK: the kit, the bass and the clavinet alone; the square lead answers; the stops */
    p = qd_proj_pat(&a->proj, 5);
    sounds(a, p, "SQUARE BOY", P_SQUARE, "FUNK CLAV", P_CLAV);
    tsound(a, p, T_BA, "ROUND BASS", P_ROUND);
    drums(p, 1, 2), perc(p, 2, 1), bass(p, C_HOME, 0, 2), clav(p, C_HOME, 0), brass(p, C_HOME, 0, 2);
    line(p, T_MEL, M_SYNTH, 32, 0, 56, 0);
    p->trk[T_MEL].limit = 1;

    /* A07 LIFT: a whole tone up (F#m): the solo in thirds over the pad and the whole band; the toms */
    p = qd_proj_pat(&a->proj, 6);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "GLASS AIR", P_AIR);
    drums(p, 1, 1), perc(p, 1, 1), bass(p, C_HOME, 2, 1), pad(p, C_HOME, 2), brass(p, C_HOME, 2, 1);
    line(p, T_MEL, M_SOLO, 0, 2, 64, 1);

    /* A08 HOME: E minor again: the harpsichord, the synth's line twice; the snare roll into the last chord */
    p = qd_proj_pat(&a->proj, 7);
    sounds(a, p, "SOFT SAW", P_SAW, "HARPSICHORD", P_HARPSI);
    drums(p, 1, 0), perc(p, 0, 1), bass(p, C_HOME, 0, 0), harpsi(p, C_HOME, 0), brass(p, C_HOME, 0, 0);
    line(p, T_MEL, M_SYNTH, 0, 0, 64, 0), line(p, T_MEL, M_SYNTH, 32, 12, 56, 0);
    p->trk[T_MEL].limit = 1;

    /* A09 END (one bar): the last chord, held: E minor with its ninth on everything, the crash */
    p = qd_proj_pat(&a->proj, 8);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "GLASS AIR", P_AIR);
    {
        static const uint8_t TOP[4] = {64, 67, 71, 78}, PADS[4] = {52, 59, 62, 66}, GTR[2] = {76, 83};
        qd_track_set_length(p, T_BRASS, 16), qd_track_set_length(p, T_KEYS, 16), qd_track_set_length(p, T_BA, 16);
        qd_track_set_length(p, T_MEL, 16), qd_track_set_length(p, T_DR, 16);
        chord(p, T_BRASS, 0, TOP, 4, 124, LS12, 0);
        chord(p, T_KEYS, 0, PADS, 4, 100, LS12, 2);
        note(p, T_BA, 0, 28, 124, LS12);
        chord(p, T_MEL, 0, GTR, 2, 120, LS12, 0);
        hit(p, T_DR, 0, M_KICK, 127);
        hit(p, T_DR, 1, M_CRASH, 120);
    }
    p->master = 16;
    return 0;
}
