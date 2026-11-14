/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's demo song "CARNIVAL": an original tune for the first big thing you hear walking into a neon-lit shopping
 * mall in 1985: part baroque rock, part stadium guitars, part jazz-funk, with a little of Boards of Canada's harmony.
 * A distorted bass pulses sixteenths under all of it; it starts on that and a lead guitar solo; the guitar comes back
 * in thirds, a harpsichord races under it, a clavinet chops the chords, effects are sprinkled in as sound locks, and
 * the kit fills the fifth bar (make -C tools tour SONG=song_carnival.c; `song` in a tour script, a new project first).
 *
 * 116 BPM, straight sixteenths, E minor. FIVE bars a pattern (80 steps): four of groove, the fifth a fill. The
 * chords are suspended (sus2, sus4: no thirds), a bar each, pushed a sixteenth or two ahead of the bar.
 *   T1 DRUMS   the MALL kit's kick, gated snare, toms and crash, the LATIN kit's hats and cabasa: sound locks
 *   T2 BASS    PULSE BASS: driven sixteenths on each chord's root, accented 3 + 3 + 2, octaves popping out
 *   T3 KEYS    FUNK CLAV's chops, HARPSICHORD's sixteenths, or a pad (OLD FILM, TAPE PAD) holding the chords
 *   T4 LEAD    LEAD GUITAR: the solo and the anthem, alone or in thirds; ACID FM: the synth's funk
 *   T5 BRASS   FM BRASS: stabs and answers, the fills' accents
 *   T6 PERC    tambourine, cowbell, a conga, timbales in the fills, and THE EFFECTS: single steps locking a zap, a
 *              noise sweep, a radio, a gamelan, a laser tom, a hiss, each into the delay or the hall
 * A01 RIFF  A02 TWO GUITARS  A03 FUNK  A04 ANTHEM  A05 ANTHEM IN THIRDS  A06 BREAK  A07 LIFT (a tone up)  A08 HOME
 * A09 END */
enum { T_DR, T_BA, T_KEYS, T_MEL, T_BRASS, T_PERC };
/* the pool: the LATIN kit 1..10, the MALL kit 11..20 (each in the pack's order), then the effects, then the rest */
enum { K_KICK = 1, K_SNARE, K_TIMBAL, K_CLAVE, K_HAT, K_OPEN, K_CONGA, K_BONGO, K_CABASA, K_AGOGO };
enum { M_KICK = 11, M_SNARE, M_CLAP, M_RIM, M_HAT, M_OPEN, M_TOM, M_BELL, M_TAMB, M_CRASH };
enum { X_BLIP = 21, X_NOISE, X_LASER, X_ZCRASH, X_RADIO, X_HISS, X_GAMELAN, X_DBELL, X_VINYL, X_TICK };
enum { NBAR = 5, LEN = NBAR * 16, FILL = LEN - 16 };

#include "song_common.h"

/* a chord: its root (a pitch class) and three more notes, in semitones from the root (no thirds: suspended) */
typedef struct { int8_t root, a, b, c; } chord_t;
#define SUS2(r) {r, 2, 7, 12}
#define SUS4(r) {r, 5, 7, 12}
#define SUS2_7(r) {r, 2, 7, 10}                         /* sus2 with its seventh */
#define SUS4_7(r) {r, 5, 7, 10}
#define SHARP9(r) {r, 4, 10, 15}                        /* the one chord with a third: B7 sharp 9, at the turn */

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

/* a bar of the groove from step b: the kick syncopated (the one, the "a" of one, the "and" of three, the push), the
 * gated snare on two and four, hats, the cabasa between */
static void groove_bar(qd_pattern_t *p, int b, int busy, int last)
{
    static const uint8_t HATS[] = {2, 6, 8, 14};
    int i, s;
    hit(p, T_DR, b, M_KICK, 122);
    hit(p, T_DR, b + 3, M_KICK, 98);
    hit(p, T_DR, b + 4, M_SNARE, 112);
    hit(p, T_DR, b + 10, M_KICK, 116);
    hit(p, T_DR, b + 12, M_SNARE, 116);
    hit(p, T_DR, b + 15, M_KICK, last ? 110 : 90);
    for (i = 0; i < 4; i++)
        hit(p, T_DR, b + HATS[i], i == 3 ? K_OPEN : K_HAT, 78);
    for (s = 1; s < 14; s += 2)
        if (s != 3)
            hit(p, T_DR, b + s, K_CABASA, busy ? 62 + (s & 2) * 6 : 50);
    if (last)                                           /* the fourth bar leans into the fill */
        hit(p, T_DR, b + 13, M_SNARE, 84);
}

/* THE FIFTH BAR: half a bar of groove, then the fill; its accents are the brass's and the bass's too */
static void fill_bar(qd_pattern_t *p, int kind)
{
    static const uint8_t TOMS[8] = {72, 72, 69, 69, 65, 65, 62, 60};
    const int f = FILL;
    int s;
    hit(p, T_DR, f, M_KICK, 122);
    hit(p, T_DR, f + 2, K_HAT, 80);
    hit(p, T_DR, f + 3, M_KICK, 100);
    hit(p, T_DR, f + 4, M_SNARE, 114);
    hit(p, T_DR, f + 6, K_HAT, 76);
    hit(p, T_DR, f + 7, M_KICK, 100);
    switch (kind) {
    case 0:                                             /* a snare roll, rising, the last two doubled */
        for (s = 8; s < 16; s++)
            hit(p, T_DR, f + s, M_SNARE, 70 + (s - 8) * 7);
        qd_step_set_ratchet(p, T_DR, f + 14, 2, QD_RAMP_UP);
        qd_step_set_ratchet(p, T_DR, f + 15, 2, QD_RAMP_UP);
        break;
    case 1:                                             /* the toms, high to low */
        for (s = 8; s < 16; s++)
            hitn(p, T_DR, f + s, M_TOM, 96 + (s & 1 ? 0 : 16), TOMS[s - 8]);
        qd_step_set_ratchet(p, T_DR, f + 11, 2, QD_RAMP_UP);
        break;
    case 2:                                             /* stops: kick and snare with the brass and the bass, 3 + 3 + 2 */
        hit(p, T_DR, f + 8, M_KICK, 124);
        hit(p, T_DR, f + 9, M_SNARE, 96);
        hit(p, T_DR, f + 11, M_KICK, 124);
        hit(p, T_DR, f + 12, M_SNARE, 100);
        hit(p, T_DR, f + 14, M_SNARE, 118);
        hit(p, T_DR, f + 15, M_SNARE, 124);
        qd_step_set_ratchet(p, T_DR, f + 15, 3, QD_RAMP_UP);
        break;
    default:                                            /* the kit drops out for the timbales (perc), then the toms */
        hit(p, T_DR, f + 8, M_CLAP, 110);
        hitn(p, T_DR, f + 12, M_TOM, 112, 67);
        hitn(p, T_DR, f + 13, M_TOM, 104, 64);
        hitn(p, T_DR, f + 14, M_TOM, 118, 60);
        hit(p, T_DR, f + 15, M_SNARE, 124);
        qd_step_set_ratchet(p, T_DR, f + 15, 2, QD_RAMP_UP);
        break;
    }
}

/* the kit over five bars: a crash after the one (the fill lands there), four bars, the fill */
static void drums(qd_pattern_t *p, int busy, int fill)
{
    int b;
    qd_track_set_length(p, T_DR, LEN);
    p->trk[T_DR].limit = 4;
    for (b = 0; b < NBAR - 1; b++)
        groove_bar(p, b * 16, busy, b == NBAR - 2);
    fill_bar(p, fill);
    hit(p, T_DR, 1, M_CRASH, 100);                      /* (beside the kick: one sound a step) */
    hit(p, T_DR, 30, M_OPEN, 92);                       /* the accents: the end of the second bar .. */
    hit(p, T_DR, 31, M_SNARE, 90);
    hit(p, T_DR, 46, M_KICK, 108);                      /* .. and the third's */
}

/* the percussion: a tambourine on the off-beats, a cowbell on the beat when it is busy, a conga's funk under it,
 * timbales in the fifth bar */
static void perc(qd_pattern_t *p, int fill, int busy)
{
    static const uint8_t TIMB[8] = {67, 67, 64, 67, 64, 60, 64, 60};
    int s;
    qd_track_set_length(p, T_PERC, LEN);
    p->trk[T_PERC].limit = 4;
    for (s = 0; s < (fill == 3 ? FILL + 8 : LEN - 4); s++) {
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
        for (s = 8; s < 16; s++)
            hitn(p, T_PERC, FILL + s, K_TIMBAL, 96 + (s & 1) * 14, TIMB[s - 8]);
    else {
        hitn(p, T_PERC, LEN - 3, K_TIMBAL, 104, 64);
        hitn(p, T_PERC, LEN - 1, K_TIMBAL, 116, 60);
    }
}

/* THE EFFECTS, sprinkled: one step each on the percussion's free sixteenths, a sound lock of the pool's effects, its
 * own note, into the delay or the hall. set: which sprinkle of the three (a pattern's own) */
static void sprinkle(qd_pattern_t *p, int set)
{
    static const struct { uint8_t s, k, n, vel, dly, rev; } FX[3][7] = {
        {{5, X_BLIP, 72, 96, 110, 40}, {19, X_GAMELAN, 76, 90, 90, 80}, {25, X_RADIO, 60, 70, 60, 50},
         {37, X_LASER, 67, 100, 40, 60}, {49, X_BLIP, 79, 88, 120, 40}, {57, X_NOISE, 60, 84, 70, 90},
         {69, X_DBELL, 64, 80, 100, 60}},
        {{9, X_TICK, 60, 100, 127, 30}, {17, X_LASER, 72, 96, 60, 60}, {33, X_BLIP, 84, 84, 120, 50},
         {41, X_GAMELAN, 71, 92, 80, 90}, {53, X_VINYL, 60, 60, 0, 30}, {61, X_HISS, 60, 100, 30, 80},
         {73, X_BLIP, 76, 90, 110, 40}},
        {{1, X_ZCRASH, 60, 76, 40, 90}, {21, X_RADIO, 67, 72, 80, 50}, {35, X_DBELL, 71, 84, 110, 60},
         {45, X_LASER, 60, 104, 50, 70}, {51, X_TICK, 72, 96, 127, 30}, {65, X_GAMELAN, 83, 86, 100, 90},
         {77, X_NOISE, 60, 90, 60, 100}}};
    int i;
    for (i = 0; i < 7; i++) {
        const int s = FX[set][i].s;
        if (qd_step_type(p, T_PERC, s) != QD_TRIG_NONE)
            continue;                                   /* (a fill's own step: left) */
        hitn(p, T_PERC, s, FX[set][i].k, FX[set][i].vel, FX[set][i].n);
        qd_step_set_len(p, T_PERC, s, FX[set][i].k == X_HISS || FX[set][i].k == X_RADIO ? LS4 : L1);
        qd_lock_set(p, T_PERC, s, QD_LK_DELAY, FX[set][i].dly);
        qd_lock_set(p, T_PERC, s, QD_LK_REVERB, FX[set][i].rev);
        qd_lock_set(p, T_PERC, s, QD_LK_PAN, (i & 1) ? 34 : -34);
    }
}

/* THE BASS: driven sixteenths on each bar's root, all the way through: accents in 3 + 3 + 2, the octave popping
 * out on them, the chord's top note to lead into the next bar */
static void bass(qd_pattern_t *p, const chord_t *c, int tr, int fill)
{
    static const uint8_t ACC[16] = {1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0};
    int b, s;
    qd_track_set_length(p, T_BA, LEN);
    p->trk[T_BA].limit = 1;
    for (b = 0; b < NBAR; b++) {
        const int r = 28 + (c[b].root + tr + 8 + 120) % 12;   /* E1 .. D#2 */
        for (s = 0; s < 16; s++) {
            const int at = b * 16 + s;
            if (fill == 2 && at >= FILL + 8)
                break;
            note(p, T_BA, at, r + (ACC[s] && s ? 12 : 0) + (s == 15 ? c[b].c - 12 : 0), ACC[s] ? 120 : 84,
                 ACC[s] ? L1 : L2);
        }
    }
    if (fill == 2) {                                    /* the stops: the bass plays them with the kit */
        const int r = 28 + (c[NBAR - 1].root + tr + 8 + 120) % 12;
        note(p, T_BA, FILL + 8, r, 124, LS2);
        note(p, T_BA, FILL + 11, r, 124, LS2);
        note(p, T_BA, FILL + 14, r + 12, 112, L1);
        note(p, T_BA, FILL + 15, r + 7, 116, L1);
    }
}

/* a chord's notes over its root, folded into lo .. lo + 11 (two the same: one an octave up) */
static void voicing(const chord_t *c, int tr, int lo, uint8_t *x)
{
    const int iv[4] = {0, c->a, c->b, c->c};
    int i, j;
    for (i = 0; i < 4; i++) {
        x[i] = (uint8_t)(lo + (c->root + tr + iv[i] - lo % 12 + 240) % 12);
        for (j = 0; j < i; j++)
            if (x[j] == x[i])
                x[i] = (uint8_t)(x[i] + 12);
    }
}

/* the clavinet: each bar's chord chopped off the beat (3 + 3 + 2 again, a sixteenth late), funk's places */
static void clav(qd_pattern_t *p, const chord_t *c, int tr)
{
    static const uint8_t AT[] = {1, 4, 7, 9, 12, 15};
    int b, i;
    qd_track_set_length(p, T_KEYS, LEN);
    p->trk[T_KEYS].limit = 4;
    for (b = 0; b < NBAR; b++) {
        uint8_t x[4];
        voicing(&c[b], tr, 52, x);
        for (i = 0; i < 6; i++)
            if (b * 16 + AT[i] < LEN - 4)
                chord(p, T_KEYS, b * 16 + AT[i], x, 4, i & 1 ? 78 : 102, i == 2 || i == 5 ? L1 : L2, 0);
    }
}
/* the harpsichord: sixteenths through each chord, the baroque engine under the rock */
static void harpsi(qd_pattern_t *p, const chord_t *c, int tr)
{
    static const uint8_t O[8] = {0, 2, 1, 2, 3, 2, 1, 2};
    int b, s;
    qd_track_set_length(p, T_KEYS, LEN);
    p->trk[T_KEYS].limit = 4;
    for (b = 0; b < NBAR; b++) {
        const int r = 52 + (c[b].root + tr + 8 + 120) % 12;   /* E3 .. D#4 */
        const int tone[4] = {r, r + c[b].a, r + c[b].b, r + 12};
        for (s = 0; s < 16; s++)
            if (b * 16 + s < LEN - 4)
                note(p, T_KEYS, b * 16 + s, tone[O[s & 7]], s & 3 ? 78 : 100, L1);
    }
}
/* .. or held: a pad, each chord pushed two sixteenths ahead of its bar (the first on the one) */
static void pad(qd_pattern_t *p, const chord_t *c, int tr)
{
    int b;
    qd_track_set_length(p, T_KEYS, LEN);
    p->trk[T_KEYS].limit = 4;
    for (b = 0; b < NBAR; b++) {
        uint8_t x[4];
        voicing(&c[b], tr, 50, x);
        chord(p, T_KEYS, b ? b * 16 - 2 : 0, x, 4, 88, b ? LS16 : LS12, 3);
    }
}

/* the brass: a stab on the one, pushed answers through the phrase, the fill's accents */
static void brass(qd_pattern_t *p, const chord_t *c, int tr, int fill)
{
    uint8_t x[4];
    qd_track_set_length(p, T_BRASS, LEN);
    p->trk[T_BRASS].limit = 4;
    voicing(&c[0], tr, 57, x);
    chord(p, T_BRASS, 0, x, 4, 114, L2, 0);
    chord(p, T_BRASS, 11, x, 4, 96, L2, 0);
    voicing(&c[1], tr, 57, x);
    chord(p, T_BRASS, 27, x, 4, 100, L2, 0);
    chord(p, T_BRASS, 30, x, 4, 116, LS2, 0);
    voicing(&c[2], tr, 57, x);
    chord(p, T_BRASS, 43, x, 4, 98, L2, 0);
    voicing(&c[3], tr, 57, x);
    chord(p, T_BRASS, 54, x, 4, 104, L1, 0);
    chord(p, T_BRASS, 59, x, 4, 96, L2, 0);
    voicing(&c[4], tr, 57, x);
    chord(p, T_BRASS, FILL, x, 4, 112, L2, 0);
    if (fill == 2) {                                    /* the stops, with the kit: 3 + 3 + 2 */
        chord(p, T_BRASS, FILL + 8, x, 4, 118, LS2, 0);
        chord(p, T_BRASS, FILL + 11, x, 4, 118, LS2, 0);
        chord(p, T_BRASS, FILL + 14, x, 4, 124, LS2, 0);
    } else {
        chord(p, T_BRASS, FILL + 12, x, 4, 104, L1, 0);
        chord(p, T_BRASS, FILL + 14, x, 4, 120, LS2, 0);
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
    qd_track_set_length(p, t, LEN);
    p->trk[t].limit = 2;
    for (; m->s != 255; m++)
        if (at + m->s < cut) {
            uint8_t x[2];
            x[0] = (uint8_t)(m->n + tr), x[1] = (uint8_t)(third_under(m->n) + tr);
            chord(p, t, at + m->s, x, harm ? 2 : 1, m->vel, m->len, 0);
        }
}

/* THE SOLO (five bars, E minor, the guitar's middle): a bent-up opening off the beat, the held top, a run up to the
 * scream, down to the fifth of B, and a last bar that hangs on it over the fill */
static const mel_t M_SOLO[] = {{2, 59, LS4, 116}, {6, 62, LS2, 108}, {8, 64, LS6, 122}, {14, 67, LS2, 112},
    {17, 64, LS2, 104}, {19, 67, LS4, 118}, {23, 66, LS2, 104}, {25, 64, LS2, 104}, {27, 66, LS4, 114},
    {31, 69, LS2, 112}, {33, 66, L1, 100}, {34, 62, L1, 98}, {35, 64, L1, 108}, {36, 67, L1, 104}, {37, 69, L1, 108},
    {38, 71, L1, 112}, {39, 74, LS4, 126}, {43, 71, LS2, 110}, {45, 69, L1, 102}, {46, 67, L1, 100},
    {47, 64, LS2, 108}, {50, 67, LS2, 106}, {52, 66, LS4, 116}, {56, 63, LS2, 106}, {59, 66, LS2, 110},
    {62, 71, LS8, 124}, {70, 69, LS2, 108}, {73, 71, LS6, 122}, {255, 0, 0, 0}};
/* THE ANTHEM (five bars over A, D, G, C, B): long notes, climbing, each a little early */
static const mel_t M_THEME[] = {{0, 64, LS4, 112}, {4, 69, LS2, 104}, {6, 71, LS8, 120}, {14, 69, LS2, 108},
    {16, 69, LS4, 114}, {20, 74, LS2, 108}, {22, 76, LS8, 122}, {30, 74, LS2, 108}, {32, 74, LS4, 114},
    {36, 71, LS2, 106}, {38, 69, LS6, 112}, {44, 71, LS2, 108}, {46, 79, LS12, 126}, {58, 76, LS2, 108},
    {60, 74, LS2, 106}, {62, 71, LS12, 120}, {74, 66, LS6, 116}, {255, 0, 0, 0}};
/* THE SYNTH'S FUNK (two bars, low): the pentatonic and its blue note, off the beat */
static const mel_t M_SYNTH[] = {{0, 52, L1, 110}, {1, 55, L1, 98}, {3, 57, L1, 104}, {4, 58, L2, 100}, {5, 57, L1, 102},
    {7, 55, L1, 96}, {8, 52, LS2, 108}, {11, 50, L1, 96}, {12, 52, L1, 104}, {14, 55, L1, 98}, {15, 57, L1, 104},
    {16, 59, LS2, 112}, {19, 57, L1, 98}, {20, 55, L1, 100}, {22, 52, L1, 98}, {23, 50, L1, 96}, {24, 52, LS3, 110},
    {27, 55, L1, 98}, {28, 57, L1, 102}, {29, 58, L1, 100}, {30, 59, L1, 106}, {31, 62, L1, 114}, {255, 0, 0, 0}};

/* ---- the chords, a bar each (E minor, suspended; the LIFT plays HOME a whole tone up) */
static const chord_t C_HOME[NBAR] = {SUS2_7(4), SUS2(0), SUS4_7(2), SUS2_7(4), SHARP9(11)};   /* E C D E | B7#9 */
static const chord_t C_ANTHEM[NBAR] = {SUS2_7(9), SUS4_7(2), SUS2(7), SUS2(0), SUS4_7(11)};   /* A D G C | Bsus4 */

enum { P_BASS = 31, P_CLAV, P_GUITAR, P_ACID, P_HARPSI, P_BRASS, P_FILM, P_TAPE, P_ROUND };
static void sounds(qd_app_t *a, qd_pattern_t *p, const char *mel, int mk, const char *key, int kk)
{
    tsound(a, p, T_DR, "MALL KICK", M_KICK);
    tsound(a, p, T_BA, "PULSE BASS", P_BASS);
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
    static const char *const FXS[] = {"ZAP BLIP", "ZAP NOISE", "ZAP TOM", "ZAP CRASH", "RADIO", "TAPE HISS", "GAMELAN",
                                      "DUST BELL", "VINYL", "ZAP TICK"};
    static const char *const POOL[] = {"PULSE BASS", "FUNK CLAV", "LEAD GUITAR", "ACID FM", "HARPSICHORD", "FM BRASS",
                                       "OLD FILM", "TAPE PAD", "ROUND BASS"};
    /* level, chorus, delay, reverb, pan */
    static const struct { uint8_t level, chorus, delay, reverb; int8_t pan; } MIX[QD_NTRACK] = {
        {108, 0, 0, 20, 0}, {110, 0, 0, 0, 0}, {104, 30, 8, 36, -14}, {106, 24, 40, 56, 6}, {96, 30, 12, 46, 16},
        {102, 0, 8, 26, 0}};
    static const char *const TNAME[QD_NTRACK] = {"DRUMS", "BASS", "KEYS", "LEAD", "BRASS", "PERC FX"};
    static const uint8_t TKIND[QD_NTRACK] = {QD_PK_DRUMS, QD_PK_BASS, QD_PK_KEYS, QD_PK_LEAD, QD_PK_LEAD, QD_PK_FX};
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
    for (i = 0; i < 10; i++)
        pool_add(a, pack_find(FXS[i]));
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
    /* the effects: the mall's atrium (a big bright hall), a synced ping-pong echo for the lead and the sprinkles */
    qd_fx_set(QD_FXP_RSIZE, 104), qd_fx_set(QD_FXP_RDECAY, 50), qd_fx_set(QD_FXP_RDAMP, 34), qd_fx_set(QD_FXP_RMOD, 50);
    qd_fx_set(QD_FXP_RPRE, 36), qd_fx_set(QD_FXP_RLOW, 80);
    qd_fx_set(QD_FXP_DSYNC, 1), qd_fx_set(QD_FXP_DDIV, 8), qd_fx_set(QD_FXP_DMODE, 1), qd_fx_set(QD_FXP_DFDBK, 56);
    qd_fx_set(QD_FXP_DWOW, 30);

    /* A01 RIFF: the bass's pulse and the guitar's solo from the first beat, the kit, the brass's stabs; the toms */
    p = qd_proj_pat(&a->proj, 0);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "FUNK CLAV", P_CLAV);
    drums(p, 0, 1), perc(p, 1, 0), bass(p, C_HOME, 0, 1), brass(p, C_HOME, 0, 1);
    line(p, T_MEL, M_SOLO, 0, 0, LEN, 0);
    sprinkle(p, 0);

    /* A02 TWO GUITARS: the solo again in thirds, the harpsichord's sixteenths under it; the stops */
    p = qd_proj_pat(&a->proj, 1);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "HARPSICHORD", P_HARPSI);
    drums(p, 1, 2), perc(p, 2, 1), bass(p, C_HOME, 0, 2), harpsi(p, C_HOME, 0), brass(p, C_HOME, 0, 2);
    line(p, T_MEL, M_SOLO, 0, 0, LEN, 1);
    sprinkle(p, 1);

    /* A03 FUNK: the clavinet's chops, the acid synth's low line, twice and a half; the snare roll */
    p = qd_proj_pat(&a->proj, 2);
    sounds(a, p, "ACID FM", P_ACID, "FUNK CLAV", P_CLAV);
    drums(p, 1, 0), perc(p, 0, 1), bass(p, C_HOME, 0, 0), clav(p, C_HOME, 0), brass(p, C_HOME, 0, 0);
    line(p, T_MEL, M_SYNTH, 0, 0, LEN, 0), line(p, T_MEL, M_SYNTH, 32, 0, LEN, 0), line(p, T_MEL, M_SYNTH, 64, 0, 72, 0);
    p->trk[T_MEL].limit = 1;
    sprinkle(p, 2);

    /* A04 ANTHEM: A D G C, B, suspended: the guitar's long theme over the worn pad; the toms */
    p = qd_proj_pat(&a->proj, 3);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "OLD FILM", P_FILM);
    drums(p, 0, 1), perc(p, 1, 0), bass(p, C_ANTHEM, 0, 1), pad(p, C_ANTHEM, 0), brass(p, C_ANTHEM, 0, 1);
    line(p, T_MEL, M_THEME, 0, 0, LEN, 0);
    sprinkle(p, 0);

    /* A05 ANTHEM, TWO GUITARS: the theme in thirds, the harpsichord racing under it; the timbales */
    p = qd_proj_pat(&a->proj, 4);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "HARPSICHORD", P_HARPSI);
    drums(p, 1, 3), perc(p, 3, 1), bass(p, C_ANTHEM, 0, 3), harpsi(p, C_ANTHEM, 0), brass(p, C_ANTHEM, 0, 3);
    line(p, T_MEL, M_THEME, 0, 0, LEN, 1);
    sprinkle(p, 1);

    /* A06 BREAK: the kit, the bass and the clavinet alone, the effects thick; the acid synth answers; the stops */
    p = qd_proj_pat(&a->proj, 5);
    sounds(a, p, "ACID FM", P_ACID, "FUNK CLAV", P_CLAV);
    drums(p, 1, 2), perc(p, 2, 1), bass(p, C_HOME, 0, 2), clav(p, C_HOME, 0), brass(p, C_HOME, 0, 2);
    line(p, T_MEL, M_SYNTH, 32, 0, LEN, 0);
    p->trk[T_MEL].limit = 1;
    sprinkle(p, 0), sprinkle(p, 1), sprinkle(p, 2);

    /* A07 LIFT: a whole tone up (F sharp): the solo in thirds over the tape pad and the whole band; the toms */
    p = qd_proj_pat(&a->proj, 6);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "TAPE PAD", P_TAPE);
    drums(p, 1, 1), perc(p, 1, 1), bass(p, C_HOME, 2, 1), pad(p, C_HOME, 2), brass(p, C_HOME, 2, 1);
    line(p, T_MEL, M_SOLO, 0, 2, LEN, 1);
    sprinkle(p, 2);

    /* A08 HOME: E again: the harpsichord, the solo once more, alone; the snare roll into the last chord */
    p = qd_proj_pat(&a->proj, 7);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "HARPSICHORD", P_HARPSI);
    drums(p, 1, 0), perc(p, 0, 1), bass(p, C_HOME, 0, 0), harpsi(p, C_HOME, 0), brass(p, C_HOME, 0, 0);
    line(p, T_MEL, M_SOLO, 0, 0, LEN, 0);
    sprinkle(p, 1);

    /* A09 END (one bar): the last chord, held: E sus2 on everything, the crash, a blip into the delay */
    p = qd_proj_pat(&a->proj, 8);
    sounds(a, p, "LEAD GUITAR", P_GUITAR, "TAPE PAD", P_TAPE);
    {
        static const uint8_t TOP[4] = {59, 64, 66, 71}, PADS[4] = {52, 59, 62, 66}, GTR[2] = {64, 71};
        qd_track_set_length(p, T_BRASS, 16), qd_track_set_length(p, T_KEYS, 16), qd_track_set_length(p, T_BA, 16);
        qd_track_set_length(p, T_MEL, 16), qd_track_set_length(p, T_DR, 16), qd_track_set_length(p, T_PERC, 16);
        chord(p, T_BRASS, 0, TOP, 4, 124, LS12, 0);
        chord(p, T_KEYS, 0, PADS, 4, 100, LS12, 2);
        note(p, T_BA, 0, 28, 124, LS12);
        chord(p, T_MEL, 0, GTR, 2, 120, LS12, 0);
        hit(p, T_DR, 0, M_KICK, 127);
        hit(p, T_DR, 1, M_CRASH, 120);
        hitn(p, T_PERC, 3, X_BLIP, 100, 76);
        qd_lock_set(p, T_PERC, 3, QD_LK_DELAY, 127);
    }
    p->master = 16;
    return 0;
}
