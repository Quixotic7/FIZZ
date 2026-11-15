/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's demo song "ATRIUM": slow, worn, sunlit: a television station's opening music on an old tape. A dusty hip hop
 * beat at 71 BPM under pads, a sub that follows the kick, and a FANFARE: a brassy lead that climbs in fourths and
 * fifths to long high notes, a bell doubling it an octave up, a tom roll and a cymbal into each strain. Everything
 * pitched is tuned a quarter of a semitone FLAT, as a tape running slow. It opens and closes without the drums, and
 * it closes in another key (make -C tools tour SONG=song_atrium.c; `song` in a tour script, a new project first).
 *
 * 71 BPM, swing 58 %, C Lydian (C major with F sharp), four bars a pattern. The first strain holds C, F and G over a
 * bass that moves under them: C, C, A, F. The second is the big cadence: A flat, B flat, C, C. The coda is in A.
 *   T1 DRUMS   the DUST kit: kick on the one, the "a" of two and the "and" of three; snare on two and four; a rim
 *              ghost; the toms' roll
 *   T2 BASS    SUB, with the kick
 *   T3 PAD     JUNO PAD (CHAPEL in the second strain and the coda): the bar's chord, strummed slowly
 *   T4 LEAD    JUNO LEAD (mono, it slides): the fanfare
 *   T5 BELLS   HAZY BELL: five notes on a loop of TWELVE steps (three beats against four); with the fanfare, its
 *              notes an octave up; alone, slowly, in the opening and the coda
 *   T6 HATS    DUST HAT in eighths through MIDI FX group 1 (RANDOM velocities), the shaker later; the cymbal; and
 *              THE AIR as sound locks: TAPE HISS, RADIO, WIND
 * A01 OPENING  A02 BEAT  A03 BELLS  A04 FANFARE  A05 CADENCE  A06 BARE  A07 FANFARE AGAIN  A08 CADENCE, FULL
 * A09 CODA  A10 END (one bar) */
enum { T_DR, T_BA, T_PAD, T_MEL, T_BELL, T_HAT };
/* the pool, in this order: the drums and the air from the pack, then the song's five flat sounds */
enum { K_KICK = 1, K_SNARE, K_RIM, K_HAT, K_OPEN, K_SHAKER, K_SNAP, K_THAT, K_TOM, K_CYMBAL, X_HISS, X_RADIO, X_WIND,
       P_SUB, P_PAD, P_CHAPEL, P_LEAD, P_BELL };
enum { LEN = 64 };                                      /* four bars */
enum { FLAT_DEPTH = -133 };                             /* PITCH depth 64.00 an octave: 1.33 = 25 cents */

#include "song_common.h"

/* ---- THE FLAT SOUNDS. FIZZ has no master tuning, so each pitched sound gets its own: its third LFO (free in all
 * five) as a square that starts at each note and takes hours to turn, on PITCH: a fixed 25 cents down. They are
 * MY presets of their own (FLAT ..), not the pack's */
static struct { const char *from, *name; int slot; int16_t patch[sizeof FIZZ_PACK[0].patch / sizeof(int16_t)]; }
    FL[] = {{"SUB", "FLAT SUB", 0, {0}}, {"JUNO PAD", "FLAT JUNO PD", 0, {0}}, {"CHAPEL", "FLAT CHAPEL", 0, {0}},
            {"JUNO LEAD", "FLAT JUNO LD", 0, {0}}, {"HAZY BELL", "FLAT BELL", 0, {0}}};
enum { NFLAT = sizeof FL / sizeof FL[0] };

static int flat_store(qd_app_t *a, int k)
{
    const fizz_pack_t *f = &FIZZ_PACK[pack_find(FL[k].from)];
    qd_sound_t s;
    int d, pitch = -1;
    for (d = 0; d < qd_quad_ndest(); d++)
        if (!strcmp(qd_quad_dest_name(d), "PITCH"))
            pitch = d;
    memcpy(FL[k].patch, f->patch, sizeof FL[k].patch);
    FL[k].patch[QD_QP_LFO(2, QD_QL_SPEED)] = 1, FL[k].patch[QD_QP_LFO(2, QD_QL_MULT)] = 12;     /* F1: 0.00004 Hz */
    FL[k].patch[QD_QP_LFO(2, QD_QL_WAVE)] = 2, FL[k].patch[QD_QP_LFO(2, QD_QL_TRIG)] = 1;      /* SQR, TRIG */
    FL[k].patch[QD_QP_LFO(2, QD_QL_PHASE)] = 0, FL[k].patch[QD_QP_LFO(2, QD_QL_FADE)] = 0;
    FL[k].patch[QD_QP_LFO(2, QD_QL_DEST)] = (int16_t)pitch, FL[k].patch[QD_QP_LFO(2, QD_QL_DEPTH)] = FLAT_DEPTH;
    FL[k].slot = qd_lib_free_slot(a);
    if (pitch < 0 || FL[k].slot < 0 || !qd_lib_store_ex(a, FL[k].slot, FL[k].name, FL[k].patch, 0))
        return -1;
    qd_lib_read(a, FL[k].slot, &s);
    s.tags = f->tag, s.pan = f->pan, s.chorus = f->chorus, s.delay = f->delay, s.reverb = f->reverb;
    qd_lib_write(a, FL[k].slot, &s);
    qd_lib_read(a, FL[k].slot, &s);
    return qd_pool_add_sound(a, &s);
}
/* track t's sound in pattern p: flat sound k (pool number P_SUB + k) */
static void fsound(qd_app_t *a, qd_pattern_t *p, int t, int k)
{
    memcpy(p->snd[t].patch, FL[k].patch, sizeof p->snd[t].patch);
    memset(p->snd[t].name, 0, sizeof p->snd[t].name);
    strncpy(p->snd[t].name, FL[k].name, sizeof p->snd[t].name - 1);
    p->snd[t].sound = (uint8_t)(P_SUB + k), p->snd[t].edited = 0, p->snd[t].preset = qd_lib_preset(a, FL[k].slot);
}

static void hitn(qd_pattern_t *p, int t, int s, int k, int vel, int n)
{
    if (qd_step_type(p, t, s) != QD_TRIG_NONE)
        return;
    note(p, t, s, n, vel, L2);
    qd_step_set_sound(p, t, s, k);
}
static void hit(qd_pattern_t *p, int t, int s, int k, int vel) { hitn(p, t, s, k, vel, 60); }

/* THE BEAT. fill 1: the last bar ends on three snares; 2: it stops on three, a snap on the last sixteenth; 3: the
 * toms roll down through its last two beats, into the fanfare */
static void drums(qd_pattern_t *p, int fill)
{
    static const uint8_t TOM[8] = {60, 60, 57, 57, 53, 53, 48, 48};
    int b, s;
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
        qd_lock_set(p, T_DR, o + 4, QD_LK_REVERB, 40);
        hit(p, T_DR, o + 7, K_KICK, 96);
        if (last && fill == 2) {
            hit(p, T_DR, o + 8, K_KICK, 118);
            hit(p, T_DR, o + 15, K_SNAP, 100);
            continue;
        }
        if (last && fill == 3) {
            for (s = 0; s < 8; s++) {
                hitn(p, T_DR, o + 8 + s, K_TOM, 76 + s * 6, TOM[s]);
                qd_lock_set(p, T_DR, o + 8 + s, QD_LK_REVERB, 60);
            }
            qd_step_set_ratchet(p, T_DR, o + 15, 2, QD_RAMP_UP);
            continue;
        }
        hit(p, T_DR, o + 10, K_KICK, 112);
        hit(p, T_DR, o + 12, K_SNARE, 106);
        hit(p, T_DR, o + (b & 1 ? 9 : 15), K_RIM, 58);
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

/* THE AIR, on the hats' track, written before them. hiss: the tape, each bar; radio: once; wind: into the last bar;
 * cymbal: on the pattern's one, long */
static void air(qd_pattern_t *p, int hiss, int radio, int wind, int cymbal)
{
    static const struct { uint8_t on, s, k, n, vel, len, dly, rev; int8_t pan; } A[] = {
        {1, 1, X_HISS, 60, 70, LS16, 0, 30, 0},    {1, 17, X_HISS, 60, 64, LS16, 0, 30, 0},
        {1, 33, X_HISS, 60, 70, LS16, 0, 30, 0},   {1, 49, X_HISS, 60, 64, LS16, 0, 30, 0},
        {2, 21, X_RADIO, 60, 64, LS8, 60, 60, 30}, {4, 45, X_WIND, 60, 70, LS16, 20, 80, -20},
        {8, 0, K_CYMBAL, 60, 92, LS16, 30, 70, 0}};
    const int on = (hiss ? 1 : 0) | (radio ? 2 : 0) | (wind ? 4 : 0) | (cymbal ? 8 : 0);
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
 *   FA  F A C G (F with G on top)                         LY  C G D F sharp (C Lydian: the bright, strange one)
 *   AB  A flat, E flat, C, E flat                         BB  B flat, F, D, F      CM  C G C E: C major, arrived
 *   AA  A E B C sharp (the coda: A major with B on it)    FS  F sharp C sharp A E (its F sharp minor seventh) */
enum { CS, AM, FA, LY, AB, BB, CM, AA, FS };
static const struct { uint8_t root, v[4]; } CH[] = {
    {36, {48, 55, 60, 65}}, {33, {45, 52, 60, 67}}, {29, {53, 57, 60, 67}}, {36, {48, 55, 62, 66}},
    {32, {44, 51, 60, 63}}, {34, {46, 53, 62, 65}}, {36, {48, 55, 60, 64}}, {33, {45, 52, 59, 61}},
    {30, {42, 49, 57, 64}}};
static const uint8_t H_A[4] = {CS, LY, AM, FA}, H_B[4] = {AB, BB, CM, LY}, H_CODA[4] = {AA, FS, AA, AA};

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
    static const struct { uint8_t s, n, vel; } B[] = {{0, 72, 84}, {3, 67, 66}, {6, 66, 72}, {8, 67, 60}, {10, 74, 76}};
    int i;
    qd_track_set_length(p, T_BELL, 12);
    p->trk[T_BELL].limit = 3;
    for (i = 0; i < (int)(sizeof B / sizeof B[0]); i++)
        note(p, T_BELL, B[i].s, B[i].n, B[i].vel, LS2);
}

/* a line of single notes on track t, up `up` semitones: step, note, velocity, length; s 255 ends it */
typedef struct { uint8_t s, n, vel, len; } mel_t;
static void line(qd_pattern_t *p, int t, const mel_t *m, int limit, int up, int dvel)
{
    qd_track_set_length(p, t, LEN);
    p->trk[t].limit = (uint8_t)limit;
    for (; m->s != 255; m++)
        note(p, t, m->s, m->n + up, m->vel + dvel, m->len);
}
/* THE FANFARE: G, C, D, up to the long high G; F sharp, E; over A the long E and up a fourth to A; over F: F, G, A,
 * climbing out */
static const mel_t M_A[] = {{0, 67, 100, LS4}, {4, 72, 104, LS4}, {8, 74, 108, LS4}, {12, 79, 116, LS12},
    {24, 78, 100, LS4}, {28, 76, 100, LS4}, {32, 76, 106, LS8}, {40, 81, 114, LS8}, {48, 77, 102, LS4},
    {52, 79, 106, LS4}, {56, 81, 112, LS8}, {255, 0, 0, 0}};
/* THE CADENCE: over A flat C, E flat, the long A flat; over B flat F, B flat, the long D; C major: the high C for
 * the bar; then D, C, and home to G */
static const mel_t M_B[] = {{0, 72, 104, LS4}, {4, 75, 106, LS4}, {8, 80, 114, LS8}, {16, 77, 106, LS4},
    {20, 82, 110, LS4}, {24, 86, 118, LS8}, {32, 84, 122, LS12}, {44, 79, 104, LS4}, {48, 86, 114, LS4},
    {52, 84, 108, LS4}, {56, 79, 110, LS8}, {255, 0, 0, 0}};
/* the bell alone in the opening: the fanfare's first notes, slowly */
static const mel_t M_OPEN[] = {{8, 67, 84, LS8}, {16, 72, 80, LS8}, {24, 74, 84, LS8}, {40, 79, 90, LS16},
    {56, 78, 76, LS8}, {255, 0, 0, 0}};
/* .. in the coda, in A: E, A, B, the long E */
static const mel_t M_CODA[] = {{8, 64, 84, LS8}, {16, 69, 80, LS8}, {24, 71, 84, LS8}, {40, 76, 90, LS16},
    {255, 0, 0, 0}};

static void sounds(qd_app_t *a, qd_pattern_t *p)
{
    tsound(a, p, T_DR, "DUST KICK", K_KICK);
    fsound(a, p, T_BA, P_SUB - P_SUB);
    fsound(a, p, T_PAD, P_PAD - P_SUB);
    fsound(a, p, T_MEL, P_LEAD - P_SUB);
    fsound(a, p, T_BELL, P_BELL - P_SUB);
    tsound(a, p, T_HAT, "DUST HAT", K_HAT);
    p->bpm = 71;
    p->swing = 58;
    p->used = 1;
}

int fizz_tour_song(qd_app_t *a, int argc, char **argv)
{
    static const char *const POOL[] = {"DUST KICK", "DUST SNARE", "DUST RIM", "DUST HAT", "DUST OPEN", "DUST SHAKER",
        "TAPE SNAP", "TAPE HAT", "DUST TOM", "808 CYMBAL", "TAPE HISS", "RADIO", "WIND"};
    /* level, chorus, delay, reverb, pan */
    static const struct { uint8_t level, chorus, delay, reverb; int8_t pan; } MIX[QD_NTRACK] = {
        {98, 0, 0, 14, 0}, {88, 0, 0, 0, 0}, {74, 60, 20, 60, 0}, {98, 40, 44, 56, 6}, {100, 20, 60, 60, -16},
        {92, 0, 20, 20, 12}};
    static const char *const TNAME[QD_NTRACK] = {"DRUMS", "SUB", "PAD", "FANFARE", "BELLS", "HATS AIR"};
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
    for (i = 0; i < NFLAT; i++)
        if (flat_store(a, i) != P_SUB + i) {
            fprintf(stderr, "song: no room for %s\n", FL[i].name);
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

    /* A01 OPENING: the pad, the hiss, the bell with the fanfare's first notes. No drums */
    p = qd_proj_pat(&a->proj, 0);
    sounds(a, p);
    air(p, 1, 0, 0, 0), pad(p, H_A, 0), line(p, T_BELL, M_OPEN, 3, 0, 0);

    /* A02 BEAT: the drums, the sub, the hats under the pad */
    p = qd_proj_pat(&a->proj, 1);
    sounds(a, p);
    drums(p, 0), air(p, 1, 0, 0, 0), hats(p, 0), bass(p, H_A, 0), pad(p, H_A, 0);

    /* A03 BELLS: the bell figure over it; the toms into the fanfare */
    p = qd_proj_pat(&a->proj, 2);
    sounds(a, p);
    drums(p, 3), air(p, 1, 1, 0, 0), hats(p, 0), bass(p, H_A, 0), pad(p, H_A, 0), bells(p);

    /* A04 FANFARE: the cymbal, the lead, the bell an octave over it */
    p = qd_proj_pat(&a->proj, 3);
    sounds(a, p);
    drums(p, 1), air(p, 1, 0, 0, 1), hats(p, 0), bass(p, H_A, 0), pad(p, H_A, 0);
    line(p, T_MEL, M_A, 2, 0, 0), line(p, T_BELL, M_A, 3, 12, -10);

    /* A05 CADENCE: A flat, B flat, C; the other pad; it stops on the last bar's three */
    p = qd_proj_pat(&a->proj, 4);
    sounds(a, p);
    drums(p, 2), air(p, 1, 0, 1, 1), hats(p, 0), bass(p, H_B, 1), pad(p, H_B, P_CHAPEL);
    line(p, T_MEL, M_B, 2, 0, 0), line(p, T_BELL, M_B, 3, 12, -14);

    /* A06 BARE: the drums, the sub, the bell figure, the radio. No pad, no lead; the toms */
    p = qd_proj_pat(&a->proj, 5);
    sounds(a, p);
    drums(p, 3), air(p, 1, 1, 0, 0), hats(p, 0), bass(p, H_A, 0), bells(p);

    /* A07 FANFARE AGAIN: everything, the shaker with it */
    p = qd_proj_pat(&a->proj, 6);
    sounds(a, p);
    drums(p, 1), air(p, 1, 0, 0, 1), hats(p, 1), bass(p, H_A, 0), pad(p, H_A, 0);
    line(p, T_MEL, M_A, 2, 0, 0), line(p, T_BELL, M_A, 3, 12, -10);

    /* A08 CADENCE, FULL; the stop */
    p = qd_proj_pat(&a->proj, 7);
    sounds(a, p);
    drums(p, 2), air(p, 1, 0, 1, 1), hats(p, 1), bass(p, H_B, 1), pad(p, H_B, P_CHAPEL);
    line(p, T_MEL, M_B, 2, 0, 0), line(p, T_BELL, M_B, 3, 12, -14);

    /* A09 CODA: in A, no drums: the other pad, the bell, the hiss */
    p = qd_proj_pat(&a->proj, 8);
    sounds(a, p);
    air(p, 1, 0, 0, 0), pad(p, H_CODA, P_CHAPEL), line(p, T_BELL, M_CODA, 3, 0, 0);

    /* A10 END (one bar): A, held */
    p = qd_proj_pat(&a->proj, 9);
    sounds(a, p);
    for (t = 0; t < QD_NTRACK; t++)
        qd_track_set_length(p, t, 16);
    chord(p, T_PAD, 0, CH[AA].v, 4, 80, LS16, 4);
    qd_step_set_sound(p, T_PAD, 0, P_CHAPEL);
    note(p, T_BELL, 0, 73, 72, LS8);
    note(p, T_HAT, 1, 60, 60, LS16);
    qd_step_set_sound(p, T_HAT, 1, X_HISS);
    p->master = 16;
    return 0;
}
