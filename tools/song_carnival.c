/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's demo song "CARNIVAL": minimal techno house with Latin percussion. A kick on every beat, a short bass off it,
 * one chord stab into the delay, a pluck's small figure that keeps changing, and never more than a few things at
 * once: each pattern adds or takes away one. The percussion (congas, a clave, bongos, an agogo, timbales) and the
 * effects are sound locks, and the fourth bar of every pattern is a fill (make -C tools tour SONG=song_carnival.c;
 * `song` in a tour script, a new project first).
 *
 * 124 BPM, straight sixteenths, A sharp (B flat) Ionian. Four bars a pattern (64 steps).
 *   T1 DRUMS   the 808 kit's kick and open hat, the LATIN cabasa between; the fills' toms and snare roll
 *   T2 BASS    FM BASS: off the kick (the off-beats and two ghosts), or rolling on every sixteenth but the kick's
 *   T3 STAB    JUNO STAB: one four-note chord (a saw, the filter closing, the chorus), off the beat, into the delay
 *   T4 LEAD    JUNO PLUCK: a two-bar figure, and its answer
 *   T5 PAD     TAPE PAD: three notes, in the break and the peak only
 *   T6 PERC    the clap, the LATIN kit (conga, clave, bongo, agogo, timbales), and THE EFFECTS: single steps locking
 *              a blip, a laser tom, a radio, a gamelan, a hiss, each into the delay or the reverb
 * A01 KICK  A02 GROOVE  A03 STAB  A04 FIGURE  A05 BREAK  A06 DROP  A07 PERCUSSION  A08 PEAK  A09 OUT  A10 END */
enum { T_DR, T_BA, T_KEYS, T_MEL, T_PAD, T_PERC };
/* the pool: the LATIN kit 1..10, the 808 kit 11..20 (each in the pack's order), then the effects, then the rest */
enum { K_KICK = 1, K_SNARE, K_TIMBAL, K_CLAVE, K_HAT, K_OPEN, K_CONGA, K_BONGO, K_CABASA, K_AGOGO };
enum { E_KICK = 11, E_SNARE, E_CLAP, E_RIM, E_HAT, E_OPEN, E_TOM, E_CONGA, E_COWBELL, E_CYMBAL };
enum { X_BLIP = 21, X_NOISE, X_LASER, X_ZCRASH, X_RADIO, X_HISS, X_GAMELAN, X_DBELL, X_VINYL, X_TICK };
enum { NBAR = 4, LEN = NBAR * 16, FILL = LEN - 16, HALF = FILL + 8 };
enum { F_TIMBAL, F_TOMS, F_ROLL, F_STOP };              /* the fourth bar's fill */

#include "song_common.h"

/* a drum: pool sound k on step s of track t at note n (60: as it is), if the step is free (a fill is written before
 * the groove, and keeps its steps: one sound a step) */
static void hitn(qd_pattern_t *p, int t, int s, int k, int vel, int n)
{
    if (qd_step_type(p, t, s) != QD_TRIG_NONE)
        return;
    note(p, t, s, n, vel, L2);
    qd_step_set_sound(p, t, s, k);
    if (k == E_CLAP || k == E_SNARE || k == E_TOM || k == K_TIMBAL)
        qd_lock_set(p, t, s, QD_LK_REVERB, 46);
    if (k == E_CYMBAL)
        qd_step_set_len(p, t, s, LS8);
}
static void hit(qd_pattern_t *p, int t, int s, int k, int vel) { hitn(p, t, s, k, vel, 60); }

/* THE KIT over four bars. kick: on every beat (0: none, the break). hats: 0 none, 1 the open hat off the beat, 2 the
 * cabasa between as well. The fourth bar's second half is the fill; the kick leaves its last beat to it */
static void drums(qd_pattern_t *p, int kick, int hats, int fill)
{
    static const uint8_t TOMS[6] = {67, 67, 64, 64, 60, 57};
    int s;
    qd_track_set_length(p, T_DR, LEN);
    p->trk[T_DR].limit = 4;
    switch (fill) {
    case F_TOMS:                                        /* the toms, high to low */
        for (s = 0; s < 6; s++)
            hitn(p, T_DR, LEN - 6 + s, E_TOM, 100 + (s & 1 ? 0 : 14), TOMS[s]);
        qd_step_set_ratchet(p, T_DR, LEN - 3, 2, QD_RAMP_UP);
        break;
    case F_ROLL:                                        /* a snare roll, rising, the last two doubled */
        for (s = 0; s < 8; s++)
            hit(p, T_DR, HALF + s, E_SNARE, 62 + s * 8);
        qd_step_set_ratchet(p, T_DR, LEN - 2, 2, QD_RAMP_UP);
        qd_step_set_ratchet(p, T_DR, LEN - 1, 3, QD_RAMP_UP);
        break;
    case F_STOP:                                        /* everything stops: 3 + 3 + 2 on the clap, a roll in */
        hit(p, T_DR, HALF + 3, E_CLAP, 112);
        hit(p, T_DR, HALF + 6, E_CLAP, 118);
        hit(p, T_DR, HALF + 7, E_SNARE, 120);
        qd_step_set_ratchet(p, T_DR, HALF + 7, 3, QD_RAMP_UP);
        break;
    default:                                            /* the timbales' (the percussion track's) */
        break;
    }
    for (s = 0; s < LEN; s++) {
        if (kick && !(s & 3) && s <= HALF)
            hit(p, T_DR, s, E_KICK, s & 15 ? 118 : 124);
        if (fill == F_STOP && s >= HALF)
            continue;
        if (hats && (s & 3) == 2)
            hit(p, T_DR, s, E_OPEN, 84);
        if (hats > 1 && (s & 1))
            hit(p, T_DR, s, K_CABASA, (s & 2) ? 64 : 52);
    }
}

/* THE PERCUSSION. lvl 0: the conga alone; 1: the clave and the bongo with it; 2: the agogo's two bells too. clap: on
 * two and four. Timbales at the end of the second bar, and all of the fill when it is theirs */
static void perc(qd_pattern_t *p, int lvl, int clap, int fill, int crash)
{
    static const uint8_t TIMB[8] = {67, 67, 64, 67, 64, 60, 64, 60};
    const int cut = fill == F_TOMS ? LEN : HALF;
    int s;
    qd_track_set_length(p, T_PERC, LEN);
    p->trk[T_PERC].limit = 4;
    if (crash)
        hit(p, T_PERC, 0, E_CYMBAL, 96);
    if (fill == F_TIMBAL) {
        for (s = 0; s < 8; s++)
            hitn(p, T_PERC, HALF + s, K_TIMBAL, 92 + (s & 1) * 12 + s * 2, TIMB[s]);
        qd_step_set_ratchet(p, T_PERC, LEN - 2, 2, QD_RAMP_UP);
    }
    if (lvl) {                                          /* the second bar's small answer */
        hitn(p, T_PERC, 30, K_TIMBAL, 100, 64);
        hitn(p, T_PERC, 31, K_TIMBAL, 110, 60);
    }
    for (s = 0; s < cut; s++) {
        const int q = s & 15;
        if (clap && (q == 4 || q == 12))
            hit(p, T_PERC, s, E_CLAP, 108);
        else if (q == 7 || q == 11 || q == 14 || q == 15)
            hitn(p, T_PERC, s, K_CONGA, q == 14 ? 96 : 84, q == 11 ? 57 : 60);
        else if (lvl && (q == 0 || q == 3 || q == 6 || q == 10))
            hit(p, T_PERC, s, K_CLAVE, q ? 80 : 90);
        else if (lvl && (q == 9 || q == 13))
            hit(p, T_PERC, s, K_BONGO, 82);
        else if (lvl > 1 && (q == 2 || q == 5))
            hitn(p, T_PERC, s, K_AGOGO, 78, q == 2 ? 65 : 60);
    }
}

/* THE EFFECTS, sprinkled on the percussion's free steps (the sixteenth after the one, the three): a sound lock of
 * the pool's effects each, its own note (B flat's scale), into the delay or the reverb */
static void sprinkle(qd_pattern_t *p, int set)
{
    static const struct { uint8_t s, k, n, vel, dly, rev; } FX[3][4] = {
        {{1, X_BLIP, 74, 92, 110, 30}, {24, X_GAMELAN, 77, 88, 90, 70}, {33, X_LASER, 65, 96, 40, 50},
         {40, X_DBELL, 70, 80, 100, 50}},
        {{8, X_TICK, 60, 96, 127, 20}, {17, X_BLIP, 82, 84, 120, 30}, {24, X_RADIO, 60, 70, 60, 40},
         {49, X_NOISE, 60, 84, 60, 80}},
        {{8, X_GAMELAN, 70, 90, 100, 80}, {17, X_LASER, 58, 100, 50, 60}, {40, X_BLIP, 77, 88, 120, 30},
         {49, X_HISS, 60, 92, 30, 70}}};
    int i;
    for (i = 0; i < 4; i++) {
        const int s = FX[set][i].s;
        if (qd_step_type(p, T_PERC, s) != QD_TRIG_NONE)
            continue;
        hitn(p, T_PERC, s, FX[set][i].k, FX[set][i].vel, FX[set][i].n);
        qd_step_set_len(p, T_PERC, s, FX[set][i].k == X_HISS || FX[set][i].k == X_RADIO ? LS4 : L1);
        qd_lock_set(p, T_PERC, s, QD_LK_DELAY, FX[set][i].dly);
        qd_lock_set(p, T_PERC, s, QD_LK_REVERB, FX[set][i].rev);
        qd_lock_set(p, T_PERC, s, QD_LK_PAN, (i & 1) ? 36 : -36);
    }
}

/* THE BASS on each bar's root, never on the kick. roll 0: the off-beats and two ghost octaves; 1: every sixteenth
 * between the kicks, the off-beats accented. from: its first step; cut: its last + 1 (the stop fill) */
static void bass(qd_pattern_t *p, const uint8_t *root, int roll, int from, int cut)
{
    int s;
    qd_track_set_length(p, T_BA, LEN);
    p->trk[T_BA].limit = 1;
    for (s = from; s < cut; s++) {
        const int q = s & 15, r = root[s >> 4];
        if (!(s & 3))
            continue;
        if ((s & 3) == 2)
            note(p, T_BA, s, r, 114, L1);
        else if (roll)
            note(p, T_BA, s, r + (q == 7 || q == 15 ? 12 : 0), 82, L2);
        else if (q == 3 || q == 11)
            note(p, T_BA, s, r + 12, 78, L2);
        else if (q == 15)
            note(p, T_BA, s, r + 7, 84, L2);
    }
}

/* THE STAB: each bar's chord, short. busy 0: twice a bar (the "a" of one, the "e" of three); 1: four times */
static void stab(qd_pattern_t *p, const uint8_t (*v)[4], int busy, int cut)
{
    static const uint8_t AT[4] = {3, 11, 6, 14}, VEL[4] = {104, 96, 84, 78};
    int b, i;
    qd_track_set_length(p, T_KEYS, LEN);
    p->trk[T_KEYS].limit = 4;
    for (b = 0; b < NBAR; b++)
        for (i = 0; i < (busy ? 4 : 2); i++)
            if (b * 16 + AT[i] < cut)
                chord(p, T_KEYS, b * 16 + AT[i], v[b], 4, VEL[i], i ? L1 : LS2, 0);
}

/* the pad: three notes a bar, held */
static void pad(qd_pattern_t *p, const uint8_t (*v)[3])
{
    int b;
    qd_track_set_length(p, T_PAD, LEN);
    p->trk[T_PAD].limit = 3;
    for (b = 0; b < NBAR; b++)
        chord(p, T_PAD, b * 16, v[b], 3, 84, LS16, 2);
}

/* a line: step, note, length, velocity; 255 ends it. On the lead from step `at`, the steps before `cut` */
typedef struct { uint8_t s, n, len, vel; } mel_t;
static void line(qd_pattern_t *p, const mel_t *m, int at, int cut)
{
    qd_track_set_length(p, T_MEL, LEN);
    p->trk[T_MEL].limit = 2;
    for (; m->s != 255; m++)
        if (at + m->s < cut)
            note(p, T_MEL, at + m->s, m->n, m->vel, m->len);
}
/* THE FIGURE (two bars): down from the third to the fifth below, up again to the major seventh (Ionian's own) */
static const mel_t M_A[] = {{2, 74, L1, 104}, {5, 72, L1, 92}, {8, 70, LS2, 100}, {11, 65, L1, 88}, {14, 67, LS2, 96},
    {18, 74, L1, 102}, {21, 77, L1, 96}, {24, 74, L1, 92}, {27, 72, L1, 90}, {30, 69, LS2, 100}, {255, 0, 0, 0}};
/* ITS ANSWER (two bars): from the fifth, over the sixth, home by the seventh */
static const mel_t M_B[] = {{0, 77, L1, 104}, {3, 74, L1, 92}, {6, 79, LS2, 102}, {9, 77, L1, 90}, {12, 72, L1, 94},
    {15, 70, LS2, 98}, {19, 69, L1, 96}, {22, 70, L1, 92}, {25, 74, LS2, 100}, {28, 72, L1, 90}, {31, 65, L1, 92},
    {255, 0, 0, 0}};

/* ---- the harmony, a bar each. HOME: B flat, B flat, E flat, F. PEAK: G minor, E flat, B flat, F */
static const uint8_t R_HOME[NBAR] = {34, 34, 39, 41}, R_PEAK[NBAR] = {31, 39, 34, 41};
static const uint8_t V_HOME[NBAR][4] = {{62, 65, 69, 72}, {62, 65, 69, 72}, {62, 65, 67, 70}, {60, 63, 65, 69}};
static const uint8_t V_PEAK[NBAR][4] = {{58, 62, 65, 69}, {58, 62, 65, 67}, {57, 60, 62, 65}, {57, 60, 63, 65}};
static const uint8_t D_HOME[NBAR][3] = {{65, 70, 74}, {65, 70, 74}, {67, 70, 74}, {65, 69, 72}};
static const uint8_t D_PEAK[NBAR][3] = {{67, 70, 74}, {67, 70, 75}, {65, 70, 74}, {65, 69, 72}};

enum { P_BASS = 31, P_STAB, P_PLUCK, P_PAD };
static void sounds(qd_app_t *a, qd_pattern_t *p)
{
    tsound(a, p, T_DR, "808 KICK", E_KICK);
    tsound(a, p, T_BA, "FM BASS", P_BASS);
    tsound(a, p, T_KEYS, "JUNO STAB", P_STAB);
    tsound(a, p, T_MEL, "JUNO PLUCK", P_PLUCK);
    tsound(a, p, T_PAD, "TAPE PAD", P_PAD);
    tsound(a, p, T_PERC, "LATIN CONGA", K_CONGA);
    p->bpm = 124;
    p->swing = 50;
    p->used = 1;
}

int fizz_tour_song(qd_app_t *a, int argc, char **argv)
{
    static const char *const FXS[] = {"ZAP BLIP", "ZAP NOISE", "ZAP TOM", "ZAP CRASH", "RADIO", "TAPE HISS", "GAMELAN",
                                      "DUST BELL", "VINYL", "ZAP TICK"};
    static const char *const POOL[] = {"FM BASS", "JUNO STAB", "JUNO PLUCK", "TAPE PAD"};
    /* level, chorus, delay, reverb, pan */
    static const struct { uint8_t level, chorus, delay, reverb; int8_t pan; } MIX[QD_NTRACK] = {
        {108, 0, 0, 4, 0}, {106, 0, 0, 0, 0}, {98, 60, 34, 16, -12}, {108, 30, 38, 20, 12}, {88, 30, 0, 40, 0},
        {102, 0, 6, 12, 0}};
    static const char *const TNAME[QD_NTRACK] = {"DRUMS", "BASS", "STAB", "LEAD", "PAD", "PERC FX"};
    static const uint8_t TKIND[QD_NTRACK] = {QD_PK_DRUMS, QD_PK_BASS, QD_PK_KEYS, QD_PK_LEAD, QD_PK_PAD, QD_PK_FX};
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
        pool_add(a, pack_find("808 KICK") + i);
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
    /* the effects: a small dark room, short, and a synced dotted-eighth ping-pong echo for the stab and the figure */
    qd_fx_set(QD_FXP_RSIZE, 70), qd_fx_set(QD_FXP_RDECAY, 34), qd_fx_set(QD_FXP_RDAMP, 60), qd_fx_set(QD_FXP_RMOD, 30);
    qd_fx_set(QD_FXP_RPRE, 20), qd_fx_set(QD_FXP_RLOW, 80);
    qd_fx_set(QD_FXP_DSYNC, 1), qd_fx_set(QD_FXP_DDIV, 8), qd_fx_set(QD_FXP_DMODE, 1), qd_fx_set(QD_FXP_DFDBK, 48);
    qd_fx_set(QD_FXP_DTONE, 50), qd_fx_set(QD_FXP_DWOW, 10);

    /* A01 KICK: the kick and the conga; the bass from the third bar; the timbales */
    p = qd_proj_pat(&a->proj, 0);
    sounds(a, p);
    drums(p, 1, 0, F_TIMBAL), perc(p, 0, 0, F_TIMBAL, 0), bass(p, R_HOME, 0, 32, LEN);
    sprinkle(p, 0);

    /* A02 GROOVE: the open hat, the clap, the clave and the bongo; the toms */
    p = qd_proj_pat(&a->proj, 1);
    sounds(a, p);
    drums(p, 1, 1, F_TOMS), perc(p, 1, 1, F_TOMS, 1), bass(p, R_HOME, 0, 0, LEN);

    /* A03 STAB: the chord, twice a bar; the stop */
    p = qd_proj_pat(&a->proj, 2);
    sounds(a, p);
    drums(p, 1, 1, F_STOP), perc(p, 1, 1, F_STOP, 0), bass(p, R_HOME, 0, 0, HALF), stab(p, V_HOME, 0, HALF);
    sprinkle(p, 1);

    /* A04 FIGURE: the pluck's two bars, twice; the cabasa; the timbales */
    p = qd_proj_pat(&a->proj, 3);
    sounds(a, p);
    drums(p, 1, 2, F_TIMBAL), perc(p, 1, 1, F_TIMBAL, 1), bass(p, R_HOME, 0, 0, LEN), stab(p, V_HOME, 0, LEN);
    line(p, M_A, 0, LEN), line(p, M_A, 32, LEN);

    /* A05 BREAK: no kick, no bass: the pad, the stab four times a bar, the figure once, the effects; the snare roll */
    p = qd_proj_pat(&a->proj, 4);
    sounds(a, p);
    drums(p, 0, 1, F_ROLL), perc(p, 2, 0, F_ROLL, 0), stab(p, V_HOME, 1, LEN), pad(p, D_HOME);
    line(p, M_A, 0, LEN);
    sprinkle(p, 0), sprinkle(p, 2);

    /* A06 DROP: everything but the pad, the bass rolling, the figure and its answer; the toms */
    p = qd_proj_pat(&a->proj, 5);
    sounds(a, p);
    drums(p, 1, 2, F_TOMS), perc(p, 1, 1, F_TOMS, 1), bass(p, R_HOME, 1, 0, LEN), stab(p, V_HOME, 1, LEN);
    line(p, M_A, 0, LEN), line(p, M_B, 32, LEN);

    /* A07 PERCUSSION: the kick, the rolling bass and all the percussion, no chords, the effects; the timbales */
    p = qd_proj_pat(&a->proj, 6);
    sounds(a, p);
    drums(p, 1, 2, F_TIMBAL), perc(p, 2, 1, F_TIMBAL, 0), bass(p, R_HOME, 1, 0, LEN);
    sprinkle(p, 1), sprinkle(p, 2);

    /* A08 PEAK: G minor, E flat, B flat, F: the pad, the stab, the answer then the figure; the stop */
    p = qd_proj_pat(&a->proj, 7);
    sounds(a, p);
    drums(p, 1, 2, F_STOP), perc(p, 2, 1, F_STOP, 1), bass(p, R_PEAK, 1, 0, HALF), stab(p, V_PEAK, 1, HALF);
    pad(p, D_PEAK);
    line(p, M_B, 0, LEN), line(p, M_A, 32, HALF);

    /* A09 OUT: home again, taken apart: the kick, the bass off the beat, the conga and the clave, the stab; the
     * figure's first two bars; the snare roll into the last chord */
    p = qd_proj_pat(&a->proj, 8);
    sounds(a, p);
    drums(p, 1, 1, F_ROLL), perc(p, 1, 0, F_ROLL, 1), bass(p, R_HOME, 0, 0, LEN), stab(p, V_HOME, 0, LEN);
    line(p, M_A, 0, LEN);
    sprinkle(p, 0);

    /* A10 END (one bar): the kick, the cymbal, B flat major nine into the delay */
    p = qd_proj_pat(&a->proj, 9);
    sounds(a, p);
    for (t = 0; t < QD_NTRACK; t++)
        qd_track_set_length(p, t, 16);
    hit(p, T_DR, 0, E_KICK, 127);
    hit(p, T_PERC, 0, E_CYMBAL, 104);
    note(p, T_BA, 0, 34, 120, LS4);
    chord(p, T_KEYS, 0, V_HOME[0], 4, 108, LS2, 0);
    qd_lock_set(p, T_KEYS, 0, QD_LK_DELAY, 110);
    note(p, T_MEL, 3, 74, 100, LS2);
    qd_lock_set(p, T_MEL, 3, QD_LK_DELAY, 120);
    p->master = 16;
    return 0;
}
