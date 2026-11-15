/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's demo song "ATRIUM": slow, worn, sunlit: the music of an empty shopping mall on an old tape. A dusty hip hop
 * beat at 71 BPM under pads that drift out of tune, a sub that follows the kick, a wobbling lead with a child's tune,
 * a bell figure that never lines up with the bar, birds and tape hiss. It opens and closes without the drums, and it
 * closes in another key (make -C tools tour SONG=song_atrium.c; `song` in a tour script, a new project first).
 *
 * 71 BPM, swing 58 %, C Lydian (C major with F sharp), four bars a pattern. The chords hold C, F and G over a bass
 * that moves under them: C, C, A, F; the second strain F, G, A, and C with its F sharp and D on top. The coda is in A.
 *   T1 DRUMS   the DUST kit: kick on the one, the "a" of two and the "and" of three; snare on two and four; a rim ghost
 *   T2 BASS    SUB, with the kick
 *   T3 PAD     TAPE PAD (SUNFADE in the second strain and the coda): the bar's chord, strummed slowly
 *   T4 LEAD    WOW LEAD: the tune
 *   T5 BELLS   HAZY BELL: five notes on a loop of TWELVE steps (three beats against four); MUSIC BOX alone in the
 *              opening and the coda
 *   T6 HATS    DUST HAT in eighths through MIDI FX group 1 (RANDOM velocities), the shaker later; and THE AIR as sound
 *              locks: TAPE HISS, BIRDS, RADIO, WIND
 * A01 OPENING  A02 BEAT  A03 BELLS  A04 TUNE  A05 SECOND STRAIN  A06 BARE  A07 TUNE AGAIN  A08 SECOND STRAIN, FULL
 * A09 CODA  A10 END (one bar) */
enum { T_DR, T_BA, T_PAD, T_MEL, T_BELL, T_HAT };
/* the pool, in this order */
enum { K_KICK = 1, K_SNARE, K_RIM, K_HAT, K_OPEN, K_SHAKER, K_SNAP, K_THAT, X_HISS, X_BIRDS, X_RADIO, X_WIND,
       P_SUB, P_PAD, P_SUN, P_LEAD, P_BELL, P_BOX };
enum { LEN = 64 };                                      /* four bars */

#include "song_common.h"

static void hit(qd_pattern_t *p, int t, int s, int k, int vel)
{
    if (qd_step_type(p, t, s) != QD_TRIG_NONE)
        return;
    note(p, t, s, 60, vel, L2);
    qd_step_set_sound(p, t, s, k);
}

/* THE BEAT. fill 1: the last bar ends on three snares; 2: it stops on three, a snap on the last sixteenth */
static void drums(qd_pattern_t *p, int fill)
{
    int b;
    qd_track_set_length(p, T_DR, LEN);
    p->trk[T_DR].limit = 3;
    p->trk[T_DR].swing = 1;
    for (b = 0; b < 4; b++) {
        const int o = b * 16, last = b == 3;
        if (last && fill == 1) {
            hit(p, T_DR, o + 13, K_SNARE, 70), hit(p, T_DR, o + 14, K_SNARE, 88), hit(p, T_DR, o + 15, K_SNARE, 104);
            qd_step_set_ratchet(p, T_DR, o + 15, 2, QD_RAMP_UP);
        }
        hit(p, T_DR, o, K_KICK, 122);
        hit(p, T_DR, o + 4, K_SNARE, 110);
        hit(p, T_DR, o + 7, K_KICK, 96);
        if (last && fill == 2) {
            hit(p, T_DR, o + 8, K_KICK, 118);
            hit(p, T_DR, o + 15, K_SNAP, 100);
            continue;
        }
        hit(p, T_DR, o + 10, K_KICK, 112);
        hit(p, T_DR, o + 12, K_SNARE, 106);
        hit(p, T_DR, o + (b & 1 ? 9 : 15), K_RIM, 58);
        qd_lock_set(p, T_DR, o + 4, QD_LK_REVERB, 40);
    }
}

/* THE HATS: eighths, the off ones softer, two ghosts; the open one before the fourth bar's three. shaker: the odd
 * sixteenths too. MIDI FX group 1 moves their velocities */
static void hats(qd_pattern_t *p, int shaker)
{
    int s;
    qd_track_set_length(p, T_HAT, LEN);
    p->trk[T_HAT].limit = 4;
    p->trk[T_HAT].swing = 1;
    p->trk[T_HAT].mfx = 1;
    for (s = 0; s < LEN; s++) {
        const int q = s & 15;
        if (s == 54)
            hit(p, T_HAT, s, K_OPEN, 84);
        else if (!(s & 1))
            hit(p, T_HAT, s, s & 2 ? K_HAT : K_THAT, s & 2 ? 88 : 62);
        else if (shaker)
            hit(p, T_HAT, s, K_SHAKER, q == 3 || q == 11 ? 70 : 46);
        else if (q == 11 || (q == 15 && (s >> 4) & 1))
            hit(p, T_HAT, s, K_HAT, 44);
    }
}

/* THE AIR, on the hats' track, written before them: sound locks on odd steps. hiss: the tape, each bar; birds: twice
 * a pattern; radio: once; wind: into the last bar */
static void air(qd_pattern_t *p, int hiss, int birds, int radio, int wind)
{
    static const struct { uint8_t on, s, k, n, vel, len, dly, rev; int8_t pan; } A[] = {
        {1, 1, X_HISS, 60, 70, LS16, 0, 30, 0},    {1, 17, X_HISS, 60, 64, LS16, 0, 30, 0},
        {1, 33, X_HISS, 60, 70, LS16, 0, 30, 0},   {1, 49, X_HISS, 60, 64, LS16, 0, 30, 0},
        {2, 5, X_BIRDS, 96, 80, LS8, 70, 80, -40}, {2, 37, X_BIRDS, 101, 72, LS8, 90, 80, 40},
        {4, 21, X_RADIO, 60, 64, LS8, 60, 60, 30}, {8, 45, X_WIND, 67, 78, LS16, 40, 90, -20}};
    const int on = (hiss ? 1 : 0) | (birds ? 2 : 0) | (radio ? 4 : 0) | (wind ? 8 : 0);
    int i;
    qd_track_set_length(p, T_HAT, LEN);
    p->trk[T_HAT].limit = 4;
    for (i = 0; i < (int)(sizeof A / sizeof A[0]); i++) {
        if (!(A[i].on & on))
            continue;
        note(p, T_HAT, A[i].s, A[i].n, A[i].vel, A[i].len);
        qd_step_set_sound(p, T_HAT, A[i].s, A[i].k);
        qd_lock_set(p, T_HAT, A[i].s, QD_LK_DELAY, A[i].dly);
        qd_lock_set(p, T_HAT, A[i].s, QD_LK_REVERB, A[i].rev);
        qd_lock_set(p, T_HAT, A[i].s, QD_LK_PAN, A[i].pan);
    }
}

/* ---- THE HARMONY: a bar's bass root and its pad chord (four notes, low to high)
 *   CS  C G C F (C, its fifth and its fourth: no third)   AM  A E C G (A minor seventh: the same C and G over A)
 *   FA  F A C G (F with G on top)                         GS  G C D F (G with its fourth and seventh)
 *   LY  C G D F sharp (C Lydian: the bright, strange one) AA  A E B C sharp (the coda: A major with B on it)
 *   FS  F sharp C sharp A E (the coda's F sharp minor seventh) */
enum { CS, AM, FA, GS, LY, AA, FS };
static const struct { uint8_t root, v[4]; } CH[] = {
    {36, {48, 55, 60, 65}}, {33, {45, 52, 60, 67}}, {29, {53, 57, 60, 67}}, {31, {55, 60, 62, 65}},
    {36, {48, 55, 62, 66}}, {33, {45, 52, 59, 61}}, {30, {42, 49, 57, 64}}};
static const uint8_t H_A[4] = {CS, CS, AM, FA}, H_B[4] = {FA, GS, AM, LY}, H_CODA[4] = {AA, FS, AA, AA};

/* THE PAD: each bar's chord for the bar, strummed slowly; sound k (0: the track's own) */
static void pad(qd_pattern_t *p, const uint8_t *h, int k)
{
    int b;
    qd_track_set_length(p, T_PAD, LEN);
    p->trk[T_PAD].limit = 8;
    for (b = 0; b < 4; b++) {
        chord(p, T_PAD, b * 16, CH[h[b]].v, 4, 84, LS16, 4);
        if (k)
            qd_step_set_sound(p, T_PAD, b * 16, k);
    }
}

/* THE BASS, with the kick: the root on the one (held), on the "a" of two (short) and the "and" of three; the fourth
 * bar walks up to the next pattern's root. stop: the last bar ends on three */
static void bass(qd_pattern_t *p, const uint8_t *h, int stop)
{
    int b;
    qd_track_set_length(p, T_BA, LEN);
    p->trk[T_BA].limit = 1;
    p->trk[T_BA].swing = 1;
    for (b = 0; b < 4; b++) {
        const int o = b * 16, r = CH[h[b]].root;
        note(p, T_BA, o, r, 118, LS6);
        note(p, T_BA, o + 7, r, 92, LS2);
        if (b == 3 && stop) {
            note(p, T_BA, o + 8, r, 112, LS4);
            continue;
        }
        note(p, T_BA, o + 10, r, 108, LS4);
        if (b == 3)
            note(p, T_BA, o + 14, r + 2, 96, LS2);
    }
}

/* THE BELLS: five notes on twelve steps, so the figure starts a beat earlier each bar (C Lydian: C, G, F sharp, G, D) */
static void bells(qd_pattern_t *p)
{
    static const struct { uint8_t s, n, vel; } B[] = {{0, 84, 84}, {3, 79, 66}, {6, 78, 72}, {8, 79, 60}, {10, 86, 76}};
    int i;
    qd_track_set_length(p, T_BELL, 12);
    p->trk[T_BELL].limit = 3;
    for (i = 0; i < (int)(sizeof B / sizeof B[0]); i++)
        note(p, T_BELL, B[i].s, B[i].n, B[i].vel, LS2);
}

/* a line of single notes on track t: step, note, velocity, length; s 255 ends it */
typedef struct { uint8_t s, n, vel, len; } mel_t;
static void line(qd_pattern_t *p, int t, const mel_t *m, int limit, int k)
{
    qd_track_set_length(p, t, LEN);
    p->trk[t].limit = (uint8_t)limit;
    for (; m->s != 255; m++) {
        note(p, t, m->s, m->n, m->vel, m->len);
        if (k)
            qd_step_set_sound(p, t, m->s, k);
    }
}
/* the tune: G up to C, the long E; D, C, back to G; over A: A, C, the long F sharp; over F: E, C, D left open */
static const mel_t M_A[] = {{0, 67, 96, LS6}, {6, 72, 92, LS2}, {8, 76, 104, LS8}, {16, 74, 96, LS4}, {20, 72, 92, LS4},
    {24, 67, 96, LS8}, {32, 69, 96, LS6}, {38, 72, 92, LS2}, {40, 78, 106, LS8}, {48, 76, 98, LS4}, {52, 72, 92, LS4},
    {56, 74, 100, LS8}, {255, 0, 0, 0}};
/* the second strain: A, C, the long F; D, B, the long G; the long E, D; C, and F sharp falling home to G */
static const mel_t M_B[] = {{0, 69, 96, LS4}, {4, 72, 96, LS4}, {8, 77, 106, LS8}, {16, 74, 98, LS4}, {20, 71, 94, LS4},
    {24, 67, 98, LS8}, {32, 76, 104, LS12}, {44, 74, 94, LS4}, {48, 72, 100, LS8}, {56, 66, 92, LS4}, {60, 67, 98, LS4},
    {255, 0, 0, 0}};
/* the music box, alone: pieces of the tune, high and far apart */
static const mel_t M_BOX[] = {{8, 79, 80, LS4}, {14, 84, 72, LS4}, {24, 88, 84, LS8}, {40, 86, 76, LS4}, {46, 84, 70, LS4},
    {56, 78, 80, LS8}, {255, 0, 0, 0}};
/* .. in the coda, in A: E, A, the long C sharp; B, A, F sharp */
static const mel_t M_BOXA[] = {{8, 76, 80, LS4}, {14, 81, 72, LS4}, {24, 85, 84, LS8}, {40, 83, 76, LS4}, {46, 81, 70, LS4},
    {56, 78, 76, LS8}, {255, 0, 0, 0}};

static void sounds(qd_app_t *a, qd_pattern_t *p)
{
    tsound(a, p, T_DR, "DUST KICK", K_KICK);
    tsound(a, p, T_BA, "SUB", P_SUB);
    tsound(a, p, T_PAD, "TAPE PAD", P_PAD);
    tsound(a, p, T_MEL, "WOW LEAD", P_LEAD);
    tsound(a, p, T_BELL, "HAZY BELL", P_BELL);
    tsound(a, p, T_HAT, "DUST HAT", K_HAT);
    p->bpm = 71;
    p->swing = 58;
    p->used = 1;
}

int fizz_tour_song(qd_app_t *a, int argc, char **argv)
{
    static const char *const POOL[] = {"DUST KICK", "DUST SNARE", "DUST RIM", "DUST HAT", "DUST OPEN", "DUST SHAKER",
        "TAPE SNAP", "TAPE HAT", "TAPE HISS", "BIRDS", "RADIO", "WIND", "SUB", "TAPE PAD", "SUNFADE", "WOW LEAD",
        "HAZY BELL", "MUSIC BOX"};
    /* level, chorus, delay, reverb, pan */
    static const struct { uint8_t level, chorus, delay, reverb; int8_t pan; } MIX[QD_NTRACK] = {
        {110, 0, 0, 14, 0}, {100, 0, 0, 0, 0}, {88, 60, 20, 60, 0}, {100, 30, 50, 50, 10}, {104, 20, 70, 60, -16},
        {104, 0, 20, 20, 12}};
    static const char *const TNAME[QD_NTRACK] = {"DRUMS", "SUB", "PAD", "TUNE", "BELLS", "HATS AIR"};
    static const uint8_t TKIND[QD_NTRACK] = {QD_PK_DRUMS, QD_PK_BASS, QD_PK_PAD, QD_PK_LEAD, QD_PK_KEYS, QD_PK_DRUMS};
    qd_pattern_t *p;
    int i, n, t;
    (void)argc, (void)argv;
    n = pack_store(a);
    if (n < 0) {
        fprintf(stderr, "song: MY PRESETS is full\n");
        return 1;
    }
    printf("song: %d presets stored (%d in the pack)\n", n, FIZZ_PACK_LIB);
    for (i = 0; i < (int)(sizeof POOL / sizeof POOL[0]); i++)
        if (pool_add(a, pack_find(POOL[i])) != 1 + i) {
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
    /* the effects: a long dark room, and a dull stereo echo that wobbles like a tape (dotted eighths, 2:3) */
    qd_fx_set(QD_FXP_RSIZE, 124), qd_fx_set(QD_FXP_RDECAY, 96), qd_fx_set(QD_FXP_RDAMP, 60), qd_fx_set(QD_FXP_RMOD, 70);
    qd_fx_set(QD_FXP_RPRE, 30), qd_fx_set(QD_FXP_RLOW, 90), qd_fx_set(QD_FXP_RWIDTH, 127);
    qd_fx_set(QD_FXP_DSYNC, 1), qd_fx_set(QD_FXP_DDIV, 8), qd_fx_set(QD_FXP_DRATIO, 2), qd_fx_set(QD_FXP_DMODE, 0);
    qd_fx_set(QD_FXP_DFDBK, 60), qd_fx_set(QD_FXP_DTONE, 44), qd_fx_set(QD_FXP_DWOW, 60);
    /* MIDI FX group 1, the hats': their velocities moved */
    {
        qd_mfx_slot_t *m = a->proj.mfx[0].slot;
        qd_mfx_slot_init(&m[0], QD_MFX_RANDOM);
        m[0].v[QD_MD_VEL] = 36, m[0].v[QD_MD_CHANCE] = 70;
    }

    /* A01 OPENING: the pad, the hiss, the birds, the music box. No drums */
    p = qd_proj_pat(&a->proj, 0);
    sounds(a, p);
    air(p, 1, 1, 0, 1), pad(p, H_A, 0), line(p, T_BELL, M_BOX, 3, P_BOX);

    /* A02 BEAT: the drums, the sub, the hats under the pad */
    p = qd_proj_pat(&a->proj, 1);
    sounds(a, p);
    drums(p, 0), air(p, 1, 0, 0, 0), hats(p, 0), bass(p, H_A, 0), pad(p, H_A, 0);

    /* A03 BELLS: the bell figure over it; the snares into the tune */
    p = qd_proj_pat(&a->proj, 2);
    sounds(a, p);
    drums(p, 1), air(p, 1, 0, 1, 0), hats(p, 0), bass(p, H_A, 0), pad(p, H_A, 0), bells(p);

    /* A04 TUNE */
    p = qd_proj_pat(&a->proj, 3);
    sounds(a, p);
    drums(p, 0), air(p, 1, 0, 0, 0), hats(p, 0), bass(p, H_A, 0), pad(p, H_A, 0), bells(p), line(p, T_MEL, M_A, 2, 0);

    /* A05 SECOND STRAIN: F, G, A, and C Lydian; the other pad; it stops on the last bar's three */
    p = qd_proj_pat(&a->proj, 4);
    sounds(a, p);
    drums(p, 2), air(p, 1, 0, 0, 1), hats(p, 0), bass(p, H_B, 1), pad(p, H_B, P_SUN), line(p, T_MEL, M_B, 2, 0);

    /* A06 BARE: the drums, the sub, the bells, the radio and the birds. No pad, no tune */
    p = qd_proj_pat(&a->proj, 5);
    sounds(a, p);
    drums(p, 1), air(p, 1, 1, 1, 0), hats(p, 0), bass(p, H_A, 0), bells(p);

    /* A07 TUNE AGAIN: everything, the shaker with it */
    p = qd_proj_pat(&a->proj, 6);
    sounds(a, p);
    drums(p, 0), air(p, 1, 0, 0, 0), hats(p, 1), bass(p, H_A, 0), pad(p, H_A, 0), bells(p), line(p, T_MEL, M_A, 2, 0);

    /* A08 SECOND STRAIN, FULL: with the bells; the stop */
    p = qd_proj_pat(&a->proj, 7);
    sounds(a, p);
    drums(p, 2), air(p, 1, 0, 0, 1), hats(p, 1), bass(p, H_B, 1), pad(p, H_B, P_SUN), bells(p);
    line(p, T_MEL, M_B, 2, 0);

    /* A09 CODA: in A, no drums: the other pad, the music box, the birds, the hiss */
    p = qd_proj_pat(&a->proj, 8);
    sounds(a, p);
    air(p, 1, 1, 0, 0), pad(p, H_CODA, P_SUN), line(p, T_BELL, M_BOXA, 3, P_BOX);

    /* A10 END (one bar): A, held */
    p = qd_proj_pat(&a->proj, 9);
    sounds(a, p);
    for (t = 0; t < QD_NTRACK; t++)
        qd_track_set_length(p, t, 16);
    chord(p, T_PAD, 0, CH[AA].v, 4, 80, LS16, 4);
    qd_step_set_sound(p, T_PAD, 0, P_SUN);
    note(p, T_BELL, 0, 85, 72, LS8);
    qd_step_set_sound(p, T_BELL, 0, P_BOX);
    note(p, T_HAT, 1, 60, 60, LS16);
    qd_step_set_sound(p, T_HAT, 1, X_HISS);
    p->master = 16;
    return 0;
}
