/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's demo song "CARNIVAL": minimal techno house with Latin percussion and Boards of Canada's harmony. A kick on
 * every beat, a short bass off it, a chord stab with no third in it over bass roots that move under it, a lead of
 * long notes that slide into each other, hats that never play the same bar twice, and never more than a few things
 * at once. The percussion and the effects are sound locks; the last bar of every phrase is a fill (make -C tools tour
 * SONG=song_carnival.c; `song` in a tour script, a new project first).
 *
 * 124 BPM, straight sixteenths, A sharp (B flat) Ionian, with one bar that is not. The phrases are uneven: 6 + 5 + 5
 * bars, a pattern each. The chords are fourths, fifths and suspensions (B flat sus2, stacked fifths on C, a tritone
 * with a fourth on it at the turn); the bass goes B flat, G, E flat, F under them, and in the peak C and A flat.
 *   T1 DRUMS   the 808 kick; the fills' toms, snare roll and stops
 *   T2 BASS    FM BASS: off the kick (the off-beats and two ghosts), or rolling on every sixteenth but the kick's
 *   T3 KEYS    JUNO STAB: the chord, off the beat, into the echo; and TAPE PAD (a sound lock on each bar's one): a
 *              fifth and its octave, drifting, in the break, the peak and the end
 *   T4 LEAD    JUNO LEAD (mono, portamento): quarter, half and whole notes, each held into the next: it slides
 *   T5 HATS    the 808 hat: five steps on a loop of TWELVE (three beats against the bar's four), through MIDI FX
 *              group 1 (RANDOM velocities, CHANCE 72 %), a lot of the stereo echo: generative
 *   T6 PERC    the clap, the LATIN kit (conga, clave, bongo, agogo, timbales), and THE EFFECTS: single steps locking
 *              a blip, a laser tom, a radio, a gamelan, a hiss, each into the echo or the room
 * A01 KICK (6 bars)  A02 GROOVE (5)  A03 LEAD (5)  A04 BREAK (6)  A05 DROP (5)  A06 PERCUSSION (5)  A07 PEAK (6)
 * A08 OUT (5)  A09 END (1) */
enum { T_DR, T_BA, T_KEYS, T_MEL, T_HAT, T_PERC };
/* the pool: the LATIN kit 1..10, the 808 kit 11..20 (each in the pack's order), then the effects, then the rest */
enum { K_KICK = 1, K_SNARE, K_TIMBAL, K_CLAVE, K_HAT, K_OPEN, K_CONGA, K_BONGO, K_CABASA, K_AGOGO };
enum { E_KICK = 11, E_SNARE, E_CLAP, E_RIM, E_HAT, E_OPEN, E_TOM, E_CONGA, E_COWBELL, E_CYMBAL };
enum { X_BLIP = 21, X_NOISE, X_LASER, X_ZCRASH, X_RADIO, X_HISS, X_GAMELAN, X_DBELL, X_VINYL, X_TICK };
enum { P_BASS = 31, P_STAB, P_LEAD, P_PAD };
enum { F_TIMBAL, F_TOMS, F_ROLL, F_STOP };              /* the last bar's fill */
enum { L24 = 13 };                                      /* (a length past the common header's: 24 steps) */

#include "song_common.h"

/* the pattern being written: its bars (5 or 6), its steps, the fill's half bar */
static int NBAR, LEN, HALF;
static void bars(int n) { NBAR = n, LEN = n * 16, HALF = LEN - 8; }

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

/* THE KICK on every beat (0: none, the break), and the fill in the last bar's second half; the kick leaves its last
 * beat to it */
static void drums(qd_pattern_t *p, int kick, int fill)
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
    for (s = 0; kick && s <= HALF; s += 4)
        hit(p, T_DR, s, E_KICK, s & 15 ? 118 : 124);
}

/* THE HATS: generative. Five steps on a loop of twelve, so the figure starts a beat earlier every bar; MIDI FX group
 * 1 drops about a quarter of them and moves their velocities, never the same twice; the echo does the rest */
static void hats(qd_pattern_t *p)
{
    static const struct { uint8_t s, vel; } H[] = {{2, 96}, {5, 60}, {6, 84}, {10, 92}, {11, 56}};
    int i;
    qd_track_set_length(p, T_HAT, 12);
    p->trk[T_HAT].limit = 2;
    p->trk[T_HAT].mfx = 1;
    for (i = 0; i < (int)(sizeof H / sizeof H[0]); i++)
        note(p, T_HAT, H[i].s, 60, H[i].vel, L2);
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
 * the pool's effects each, its own note (B flat's scale), into the echo or the room */
static void sprinkle(qd_pattern_t *p, int set)
{
    static const struct { uint8_t s, k, n, vel, dly, rev; } FX[3][4] = {
        {{1, X_BLIP, 72, 92, 110, 30}, {24, X_GAMELAN, 77, 88, 90, 70}, {33, X_LASER, 65, 96, 40, 50},
         {56, X_DBELL, 70, 80, 100, 50}},
        {{8, X_TICK, 60, 96, 127, 20}, {17, X_BLIP, 82, 84, 120, 30}, {40, X_RADIO, 60, 70, 60, 40},
         {65, X_NOISE, 60, 84, 60, 80}},
        {{8, X_GAMELAN, 70, 90, 100, 80}, {17, X_LASER, 58, 100, 50, 60}, {49, X_BLIP, 77, 88, 120, 30},
         {65, X_HISS, 60, 92, 30, 70}}};
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

/* ---- THE HARMONY. A bar's chord: the bass's root, the stab's four notes, the pad's fifth. The notes above hardly
 * move; the root under them does:
 *   BB  B flat sus2 (C F B flat C)            GG  the same over G: its flat third below, a sixth chord's colour
 *   EB  F sus2 over E flat (C F G C)          FF  the turn: E flat and A, a tritone, a fourth and another on it
 *   CC  fifths stacked on C (C G D G)         AB  B flat sus2 over A FLAT: the one bar outside the scale */
enum { BB, GG, EB, FF, CC, AB };
static const struct { uint8_t root, v[4], d[3]; } CH[] = {
    {34, {60, 65, 70, 72}, {58, 65, 70}}, {31, {60, 65, 70, 72}, {62, 69, 74}}, {39, {60, 65, 67, 72}, {58, 65, 70}},
    {41, {63, 69, 74, 79}, {60, 67, 72}}, {36, {60, 67, 74, 79}, {55, 62, 67}}, {32, {58, 65, 70, 72}, {63, 70, 75}}};
static const uint8_t H_SIX[6] = {BB, BB, GG, EB, EB, FF}, H_FIVE[5] = {BB, GG, EB, EB, FF};
static const uint8_t H_ALT[5] = {BB, GG, CC, EB, FF}, H_PEAK[6] = {GG, EB, CC, AB, EB, FF};

/* THE BASS on each bar's root, never on the kick. roll 0: the off-beats and two ghost octaves; 1: every sixteenth
 * between the kicks, the off-beats accented. from: its first step; cut: its last + 1 (the stop fill) */
static void bass(qd_pattern_t *p, const uint8_t *h, int roll, int from, int cut)
{
    int s;
    qd_track_set_length(p, T_BA, LEN);
    p->trk[T_BA].limit = 1;
    for (s = from; s < cut; s++) {
        const int q = s & 15, r = CH[h[s >> 4]].root;
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

/* THE STAB: each bar's chord. busy 0: twice a bar (the "a" of one, the "e" of three); 1: four times */
static void stab(qd_pattern_t *p, const uint8_t *h, int busy, int cut)
{
    static const uint8_t AT[4] = {3, 11, 6, 14}, VEL[4] = {104, 96, 84, 78};
    int b, i;
    qd_track_set_length(p, T_KEYS, LEN);
    p->trk[T_KEYS].limit = 7;
    for (b = 0; b < NBAR; b++)
        for (i = 0; i < (busy ? 4 : 2); i++)
            if (b * 16 + AT[i] < cut)
                chord(p, T_KEYS, b * 16 + AT[i], CH[h[b]].v, 4, VEL[i], i ? L1 : LS2, 0);
}

/* THE PAD, on the stab's track: a sound lock on each bar's one, a fifth and its octave held for the bar, out of the
 * echo and deep in the room */
static void pad(qd_pattern_t *p, const uint8_t *h)
{
    int b;
    qd_track_set_length(p, T_KEYS, LEN);
    p->trk[T_KEYS].limit = 7;
    for (b = 0; b < NBAR; b++) {
        chord(p, T_KEYS, b * 16, CH[h[b]].d, 3, 76, LS16, 2);
        qd_step_set_sound(p, T_KEYS, b * 16, P_PAD);
        qd_lock_set(p, T_KEYS, b * 16, QD_LK_DELAY, 0);
        qd_lock_set(p, T_KEYS, b * 16, QD_LK_REVERB, 90);
    }
}

/* THE LEAD: step, note; 255 ends it. Every note is held past the next one's start (a quarter for six steps, a half
 * for twelve, a whole for twenty-four), so the mono voice slides into it; the last one for `last` */
typedef struct { uint8_t s, n, vel; } mel_t;
static void lead(qd_pattern_t *p, const mel_t *m, int last)
{
    qd_track_set_length(p, T_MEL, LEN);
    p->trk[T_MEL].limit = 2;
    for (; m->s != 255; m++) {
        const int gap = m[1].s == 255 ? 0 : m[1].s - m->s;
        const int len = gap == 4 ? LS6 : gap == 8 ? LS12 : gap == 12 ? LS16 : gap == 16 ? L24 : last;
        if (m->s < LEN)
            note(p, T_MEL, m->s, m->n, m->vel, len);
    }
}
/* five bars: F up a fifth to C, the long B flat; G, C, up to F; the long C; the turn's A and E flat (the tritone) */
static const mel_t M_FIVE[] = {{0, 65, 100}, {8, 72, 104}, {16, 70, 100}, {32, 67, 96}, {36, 72, 100}, {40, 77, 108},
    {48, 72, 100}, {64, 69, 98}, {72, 75, 104}, {255, 0, 0}};
/* its answer, lower: C down a fourth to G, the long F; B flat, C, E flat; the long F; C, A */
static const mel_t M_ANSWER[] = {{0, 72, 100}, {8, 67, 98}, {16, 65, 100}, {32, 70, 100}, {40, 72, 100}, {44, 75, 104},
    {48, 77, 108}, {64, 72, 100}, {72, 69, 100}, {255, 0, 0}};
/* six bars: the long B flat, F and C; the long G; B flat, C, F; the long G on top; E flat, A */
static const mel_t M_SIX[] = {{0, 70, 100}, {16, 77, 106}, {24, 72, 100}, {32, 67, 98}, {48, 70, 100}, {52, 72, 100},
    {56, 77, 106}, {64, 79, 110}, {80, 75, 102}, {88, 69, 100}, {255, 0, 0}};

static void sounds(qd_app_t *a, qd_pattern_t *p, int nbar)
{
    bars(nbar);
    tsound(a, p, T_DR, "808 KICK", E_KICK);
    tsound(a, p, T_BA, "FM BASS", P_BASS);
    tsound(a, p, T_KEYS, "JUNO STAB", P_STAB);
    tsound(a, p, T_MEL, "JUNO LEAD", P_LEAD);
    tsound(a, p, T_HAT, "808 HAT", E_HAT);
    tsound(a, p, T_PERC, "LATIN CONGA", K_CONGA);
    p->bpm = 124;
    p->swing = 50;
    p->used = 1;
}

int fizz_tour_song(qd_app_t *a, int argc, char **argv)
{
    static const char *const FXS[] = {"ZAP BLIP", "ZAP NOISE", "ZAP TOM", "ZAP CRASH", "RADIO", "TAPE HISS", "GAMELAN",
                                      "DUST BELL", "VINYL", "ZAP TICK"};
    static const char *const POOL[] = {"FM BASS", "JUNO STAB", "JUNO LEAD", "TAPE PAD"};
    /* level, chorus, delay, reverb, pan */
    static const struct { uint8_t level, chorus, delay, reverb; int8_t pan; } MIX[QD_NTRACK] = {
        {108, 0, 0, 10, 0}, {106, 0, 0, 0, 0}, {90, 60, 44, 34, -12}, {98, 50, 40, 44, 8}, {112, 0, 84, 24, 14},
        {102, 0, 10, 28, 0}};
    static const char *const TNAME[QD_NTRACK] = {"KICK", "BASS", "KEYS", "LEAD", "HATS", "PERC FX"};
    static const uint8_t TKIND[QD_NTRACK] = {QD_PK_DRUMS, QD_PK_BASS, QD_PK_KEYS, QD_PK_LEAD, QD_PK_DRUMS, QD_PK_FX};
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
    /* the effects: a big wide room (its input cut low, so the kick and the bass stay out of it), and a stereo echo:
     * dotted eighths on the left, eighths on the right (2:3) */
    qd_fx_set(QD_FXP_RSIZE, 120), qd_fx_set(QD_FXP_RDECAY, 74), qd_fx_set(QD_FXP_RDAMP, 40), qd_fx_set(QD_FXP_RMOD, 56);
    qd_fx_set(QD_FXP_RPRE, 34), qd_fx_set(QD_FXP_RLOW, 104), qd_fx_set(QD_FXP_RWIDTH, 127);
    qd_fx_set(QD_FXP_DSYNC, 1), qd_fx_set(QD_FXP_DDIV, 8), qd_fx_set(QD_FXP_DRATIO, 2), qd_fx_set(QD_FXP_DMODE, 0);
    qd_fx_set(QD_FXP_DFDBK, 56), qd_fx_set(QD_FXP_DTONE, 56), qd_fx_set(QD_FXP_DWOW, 16);
    /* MIDI FX group 1, the hats': velocities moved, about a quarter of the notes dropped */
    {
        qd_mfx_slot_t *m = a->proj.mfx[0].slot;
        qd_mfx_slot_init(&m[0], QD_MFX_RANDOM);
        m[0].v[QD_MD_VEL] = 40, m[0].v[QD_MD_CHANCE] = 60;
        qd_mfx_slot_init(&m[1], QD_MFX_CHANCE);
        m[1].v[QD_MC_CHANCE] = 72;
    }

    /* A01 KICK (6 bars): the kick, the conga, the hats; the bass from the fourth bar; the timbales */
    p = qd_proj_pat(&a->proj, 0);
    sounds(a, p, 6);
    drums(p, 1, F_TIMBAL), hats(p), perc(p, 0, 0, F_TIMBAL, 0), bass(p, H_SIX, 0, 48, LEN);
    sprinkle(p, 0);

    /* A02 GROOVE (5): the clap, the clave and the bongo, the stab twice a bar; the toms */
    p = qd_proj_pat(&a->proj, 1);
    sounds(a, p, 5);
    drums(p, 1, F_TOMS), hats(p), perc(p, 1, 1, F_TOMS, 1), bass(p, H_FIVE, 0, 0, LEN), stab(p, H_FIVE, 0, LEN);

    /* A03 LEAD (5): the lead's five bars; the stop */
    p = qd_proj_pat(&a->proj, 2);
    sounds(a, p, 5);
    drums(p, 1, F_STOP), hats(p), perc(p, 1, 1, F_STOP, 0), bass(p, H_FIVE, 0, 0, HALF), stab(p, H_FIVE, 0, HALF);
    lead(p, M_FIVE, LS8);
    sprinkle(p, 1);

    /* A04 BREAK (6): no kick, no bass: the pad, the lead's six bars, the stab four times a bar, the effects; the
     * snare roll */
    p = qd_proj_pat(&a->proj, 3);
    sounds(a, p, 6);
    drums(p, 0, F_ROLL), hats(p), perc(p, 2, 0, F_ROLL, 0), stab(p, H_SIX, 1, LEN), pad(p, H_SIX);
    lead(p, M_SIX, LS8);
    sprinkle(p, 0), sprinkle(p, 2);

    /* A05 DROP (5): the bass rolling, the stab four times a bar, the lead's answer; C in the third bar; the toms */
    p = qd_proj_pat(&a->proj, 4);
    sounds(a, p, 5);
    drums(p, 1, F_TOMS), hats(p), perc(p, 1, 1, F_TOMS, 1), bass(p, H_ALT, 1, 0, LEN), stab(p, H_ALT, 1, LEN);
    lead(p, M_ANSWER, LS8);

    /* A06 PERCUSSION (5): the kick, the rolling bass and all the percussion, no chords, the effects; the timbales */
    p = qd_proj_pat(&a->proj, 5);
    sounds(a, p, 5);
    drums(p, 1, F_TIMBAL), hats(p), perc(p, 2, 1, F_TIMBAL, 0), bass(p, H_FIVE, 1, 0, LEN);
    sprinkle(p, 1), sprinkle(p, 2);

    /* A07 PEAK (6): G, E flat, C, A flat, E flat, F under the same fourths and fifths: the pad, the stab, the lead's
     * six bars; the stop */
    p = qd_proj_pat(&a->proj, 6);
    sounds(a, p, 6);
    drums(p, 1, F_STOP), hats(p), perc(p, 2, 1, F_STOP, 1), bass(p, H_PEAK, 1, 0, HALF), stab(p, H_PEAK, 1, HALF);
    pad(p, H_PEAK);
    lead(p, M_SIX, LS4);

    /* A08 OUT (5): home, taken apart: the kick, the bass off the beat, the conga and the clave, the stab, the pad,
     * the lead's first five bars again; the snare roll into the last chord */
    p = qd_proj_pat(&a->proj, 7);
    sounds(a, p, 5);
    drums(p, 1, F_ROLL), hats(p), perc(p, 1, 0, F_ROLL, 1), bass(p, H_FIVE, 0, 0, LEN), stab(p, H_FIVE, 0, LEN);
    pad(p, H_FIVE);
    lead(p, M_FIVE, LS4);
    sprinkle(p, 0);

    /* A09 END (one bar): the kick, the cymbal, B flat sus2 into the echo, the pad's fifth, the lead's F */
    p = qd_proj_pat(&a->proj, 8);
    sounds(a, p, 1);
    for (t = 0; t < QD_NTRACK; t++)
        qd_track_set_length(p, t, 16);
    hit(p, T_DR, 0, E_KICK, 127);
    hit(p, T_PERC, 0, E_CYMBAL, 104);
    note(p, T_BA, 2, 34, 120, LS4);
    pad(p, H_FIVE);
    chord(p, T_KEYS, 3, CH[BB].v, 4, 108, LS2, 0);
    qd_lock_set(p, T_KEYS, 3, QD_LK_DELAY, 110);
    note(p, T_MEL, 0, 65, 100, LS12);
    p->master = 16;
    return 0;
}
