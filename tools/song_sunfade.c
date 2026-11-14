/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's demo song, "SUNFADE", and the preset pack (tools/fizz_pack.h), made through the editing API as the panel
 * would make them (tools/tour.c's song hook: `song` in a tour script; `song pack` stores the presets only).
 *
 * 88 BPM, swing 57 %. D phrygian, two bars a pattern (the intro one): Dm a bar, Eb a bar.
 *   T1 DRUMS   the beat on one track: every kick, snare and hat a sound lock of the pool's kit (1..10 DUST,
 *              11..20 TAPE, 21..30 808), DRIVE locked on the kicks and snares, the hats' rolls ratchets
 *   T2 808     808 BASS (mono, slides)
 *   T3 PAD     OLD FILM: a chord a bar, its filter locked step by step (the sweeps)
 *   T4 ARP     TOY PIANO: a held chord through MIDI FX group 1 (a random-walk arp, lanes of 7 and 5, RANDOM, CHANCE)
 *   T5 BELLS   HAZY BELL: three notes through group 2 (a slow random arp with rests)
 *   T6 PERC    the clap under the snare, a rim into the delay, the hiss, the crash
 * The effects: a ping-pong delay on dotted eighths, and the reverb as a black hole (SIZE 127, a very long tail).
 * A01 INTRO  A02 BEAT  A03 STABS  A04 SWEEP  A05 DROP  A06 BREAK  A07 OUTRO  A08 808 */

enum { T_DR, T_BA, T_PAD, T_LEAD, T_BELL, T_HAT };
/* a kit's ten, in the pack's order */
enum { KICK, SNARE, CLAP, RIM, HAT, OPEN, TOM, PERC, SHAKER, CRASH };
enum { KIT_DUST = 1, KIT_TAPE = 11, KIT_808 = 21 };

#include "song_common.h"

/* a drum: sound k of the kit on track t's step s (every drum of the pack is tuned for C4) */
static void drum(qd_pattern_t *p, int t, int s, int kit, int k, int vel, int len)
{
    note(p, t, s, 60, vel, len);
    qd_step_set_sound(p, t, s, kit + k);
    if (k == KICK)
        qd_lock_set(p, t, s, QD_LK_REVERB, 2);
    if (k == CLAP || k == CRASH)
        qd_lock_set(p, t, s, QD_LK_REVERB, 44);
}

/* ---- the parts (two bars: steps 0..31). D phrygian: Dm a bar, Eb over it a bar */
enum { LS24 = 13, LS32 = 14 };
static const uint8_t CH[2][4] = {{38, 50, 57, 65}, {39, 51, 58, 67}};

static void lock_trig(qd_pattern_t *p, int t, int s, int id, int v)
{
    if (qd_step_type(p, t, s) == QD_TRIG_NONE) {
        qd_step_on(p, t, s);
        qd_step_set_trig(p, t, s, QD_TRIG_LOCK);
    }
    qd_lock_set(p, t, s, id, v);
}

/* the pad: a chord a bar, held; its filter locked from f0 to f1 over the bars (every second step: a sweep) */
static void pad(qd_pattern_t *p, int vel, int f0, int f1, int bars)
{
    int c, s;
    qd_track_set_length(p, T_PAD, bars * 16);
    p->trk[T_PAD].limit = 5;
    for (c = 0; c < bars; c++)
        chord(p, T_PAD, c * 16, CH[c & 1], 4, vel, LS16, 4);
    for (s = 0; s < bars * 16; s += 2)
        lock_trig(p, T_PAD, s, QD_QP_FREQ, f0 + (f1 - f0) * s / (bars * 16 - 2));
}

/* the bells: three notes held two bars, for the slow arp (MIDI FX group 2) */
static void drone(qd_pattern_t *p, int vel)
{
    static const uint8_t N[3] = {74, 77, 81};
    qd_track_set_length(p, T_BELL, 32);
    p->trk[T_BELL].limit = 3;
    p->trk[T_BELL].mfx = 2;
    chord(p, T_BELL, 0, N, 3, vel, LS32, 0);
}

static void bass(qd_pattern_t *p, int drop)
{
    qd_track_set_length(p, T_BA, 32);
    p->trk[T_BA].limit = 1;
    p->trk[T_BA].swing = 1;
    note(p, T_BA, 0, 38, 118, LS6);                     /* D */
    note(p, T_BA, 9, 38, 104, L1);
    note(p, T_BA, 10, 38, 110, LS3);
    note(p, T_BA, 14, 41, 98, LS2);                     /* .. up to F, a slide */
    qd_step_set_trig(p, T_BA, 14, QD_TRIG_TIE);
    if (drop)
        return;                                         /* (the build: the second bar without it) */
    note(p, T_BA, 16, 39, 118, LS6);                    /* Eb */
    note(p, T_BA, 23, 39, 100, L1);
    note(p, T_BA, 25, 39, 110, LS3);
    note(p, T_BA, 30, 36, 98, LS2);                     /* .. C, back to the D */
    qd_step_set_trig(p, T_BA, 30, QD_TRIG_TIE);
}

/* the arp: a chord held a bar, Dm7 then Ebmaj7; MIDI FX group 1 plays it (a random walk, its lanes of seven and
 * five steps turning against the bar, octaves and velocities thrown, one note in eight dropped) */
static void stabs(qd_pattern_t *p, int vel)
{
    static const uint8_t A[2][4] = {{62, 65, 69, 72}, {63, 67, 70, 74}};
    qd_track_set_length(p, T_LEAD, 32);
    p->trk[T_LEAD].limit = 4;
    p->trk[T_LEAD].swing = 1;
    p->trk[T_LEAD].mfx = 1;
    chord(p, T_LEAD, 0, A[0], 4, vel, LS16, 0);
    chord(p, T_LEAD, 16, A[1], 4, vel, LS16, 0);
}

/* THE BEAT, all on one track: every kick, snare and hat a sound lock of the kit */
static void kick(qd_pattern_t *p, int s, int kit, int vel)
{
    drum(p, T_DR, s, kit, KICK, vel, L2);
    qd_lock_set(p, T_DR, s, QD_LK_LEVEL, 124);
}
static void snare(qd_pattern_t *p, int s, int kit, int vel)
{
    drum(p, T_DR, s, kit, SNARE, vel, L2);
    qd_lock_set(p, T_DR, s, QD_LK_REVERB, 34);
}
static void hat(qd_pattern_t *p, int s, int kit, int vel, int roll)
{
    drum(p, T_DR, s, kit, HAT, vel, L4);
    qd_lock_set(p, T_DR, s, QD_LK_PAN, (s & 2) ? 16 : -10);
    if (roll)
        qd_step_set_ratchet(p, T_DR, s, roll, QD_RAMP_UP);
}
static void beat(qd_pattern_t *p, int kit, int full)
{
    static const uint8_t SOFT[] = {1, 5, 11, 13, 17, 19, 21, 27};
    int i;
    qd_track_set_length(p, T_DR, 32);
    p->trk[T_DR].limit = 4;
    p->trk[T_DR].swing = 1;
    kick(p, 0, kit, 124);
    hat(p, 2, kit, 84, 0);
    kick(p, 3, kit, 98);
    snare(p, 4, kit, 120);
    hat(p, 6, kit, 78, 0);
    hat(p, 7, kit, 58, 0);
    hat(p, 8, kit, 86, 0);
    kick(p, 9, kit, 110);
    kick(p, 10, kit, 104);
    snare(p, 12, kit, 122);
    hat(p, 14, kit, 82, 0);
    drum(p, T_DR, 15, kit, OPEN, 86, LS2);
    kick(p, 16, kit, 124);
    hat(p, 18, kit, 84, 0);
    snare(p, 20, kit, 120);
    hat(p, 22, kit, 78, 0);
    kick(p, 23, kit, 100);
    hat(p, 24, kit, 70, 2);
    kick(p, 25, kit, 112);
    hat(p, 26, kit, 82, 0);
    snare(p, 28, kit, 124);
    hat(p, 29, kit, 74, 3);
    hat(p, 30, kit, 86, full ? 4 : 0);
    snare(p, 31, kit, 66);
    if (full)
        qd_step_set_ratchet(p, T_DR, 31, 2, QD_RAMP_UP);
    for (i = 0; i < (int)sizeof SOFT; i++)
        hat(p, SOFT[i], kit, 46 + (i & 1) * 6, 0);
}

/* the sixth track: the clap under the snare, a rim into the delay, a hiss into the bar, the crash */
static void perc(qd_pattern_t *p, int kit, int full)
{
    int s;
    qd_track_set_length(p, T_HAT, 32);
    p->trk[T_HAT].limit = 3;
    p->trk[T_HAT].swing = 1;
    for (s = 4; s < 32; s += 8)
        drum(p, T_HAT, s, kit, CLAP, s == 28 ? 108 : 96, L2);
    drum(p, T_HAT, 10, kit, RIM, 84, L4);
    qd_lock_set(p, T_HAT, 10, QD_LK_DELAY, 120);
    qd_step_set_cond(p, T_HAT, 10, qd_cond_ab(1, 2));
    drum(p, T_HAT, 22, kit, RIM, 70, L4);
    qd_lock_set(p, T_HAT, 22, QD_LK_DELAY, 127);
    drum(p, T_HAT, 1, KIT_TAPE, CRASH, 70, LS12);       /* (the tape kit's tenth: TAPE HISS: the tape's breath) */
    drum(p, T_HAT, 17, KIT_TAPE, CRASH, 74, LS12);
    if (full) {
        drum(p, T_HAT, 0, kit, CRASH, 84, LS8);
        qd_step_set_cond(p, T_HAT, 0, QD_COND_1ST);
        drum(p, T_HAT, 13, kit, SHAKER, 64, L2);
        drum(p, T_HAT, 17, kit, SHAKER, 60, L2);
    }
}

/* every pattern's tracks: the song's sounds (pool numbers 31..) */
enum { P_BASS = 31, P_PAD, P_BELL, P_DRONE, P_HAZE, P_SUB, P_BOX, P_WIND, P_TAPEPAD, P_GAMELAN };
static void sounds(qd_app_t *a, qd_pattern_t *p)
{
    tsound(a, p, T_DR, "DUST KICK", KIT_DUST + KICK);
    tsound(a, p, T_BA, "808 BASS", P_BASS);
    tsound(a, p, T_PAD, "OLD FILM", P_PAD);
    tsound(a, p, T_LEAD, "TOY PIANO", P_BELL);
    tsound(a, p, T_BELL, "HAZY BELL", P_DRONE);
    tsound(a, p, T_HAT, "DUST CLAP", KIT_DUST + CLAP);
    p->bpm = 88;
    p->swing = 57;
    p->used = 1;
}

int fizz_tour_song(qd_app_t *a, int argc, char **argv)
{
    static const char *const POOL[] = {"808 BASS", "OLD FILM", "TOY PIANO", "HAZY BELL", "HAZE", "SUB", "MUSIC BOX",
                                       "WIND", "TAPE PAD", "GAMELAN"};
    /* level, chorus, delay, reverb, pan */
    static const struct { uint8_t level, chorus, delay, reverb; int8_t pan; } MIX[QD_NTRACK] = {
        {106, 0, 4, 12, 0}, {105, 0, 0, 0, 0}, {100, 50, 30, 96, 0}, {103, 30, 96, 84, 10}, {104, 30, 60, 110, -12},
        {110, 0, 30, 40, 0}};
    enum { DDIV = 8 };                                  /* the delay's note value: dotted eighths */
    static const char *const TNAME[QD_NTRACK] = {"DRUMS", "808", "PAD", "ARP", "BELLS", "PERC"};
    static const uint8_t TKIND[QD_NTRACK] = {QD_PK_DRUMS, QD_PK_BASS, QD_PK_PAD, QD_PK_PLUCK, QD_PK_KEYS, QD_PK_DRUMS};
    qd_pattern_t *p;
    int i, n, t;
    if (argc > 1 && !strcmp(argv[1], "fx")) {           /* (the effects' values, for the log) */
        static const char *const FN[] = {"DTIME", "DDIV", "DSPREAD", "DRATIO", "DFDBK", "DSYNC", "DMODE", "DTONE", "DWOW",
            "RSIZE", "RDAMP", "RDECAY", "RPRE", "RMOD", "RLOW", "RWIDTH", "CWIDTH", "FLOW", "FHIGH", "FSLOPE", "FON",
            "CRATE", "CDEPTH", "RMODE", "ORDER", "RDRY"};
        for (i = 0; i <= 12; i++) {
            char b[16];
            printf("ddiv %d = %s\n", i, qd_fx_text(QD_FXP_DDIV, i, b));
        }
        for (i = 0; i < (int)(sizeof FN / sizeof FN[0]); i++) {
            int lo, hi, d;
            char b[16], b1[16], b2[16];
            qd_fx_range(i, &lo, &hi, &d);
            printf("fx %-8s %d (%s)  range %d (%s) .. %d (%s)\n", FN[i], qd_fx_get(i), qd_fx_text(i, qd_fx_get(i), b), lo,
                   qd_fx_text(i, lo, b1), hi, qd_fx_text(i, hi, b2));
        }
        return 0;
    }
    n = pack_store(a);
    if (n < 0) {
        fprintf(stderr, "song: MY PRESETS is full\n");
        return 1;
    }
    printf("song: %d presets stored (%d in the pack)\n", n, FIZZ_PACK_N);
    if (argc > 1 && !strcmp(argv[1], "pack"))
        return 0;
    /* the pool: three kits of ten, then the song's sounds */
    for (i = 0; i < 10; i++)
        pool_add(a, pack_find("DUST KICK") + i);
    for (i = 0; i < 10; i++)
        pool_add(a, pack_find("TAPE KICK") + i);
    for (i = 0; i < 10; i++)
        pool_add(a, pack_find("808 KICK") + i);
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
    /* the effects: a ping-pong delay on dotted eighths that keeps going, and the black hole (the reverb at its
     * largest, a very long tail, the delay's echoes falling into it) */
    qd_fx_set(QD_FXP_DSYNC, 1), qd_fx_set(QD_FXP_DDIV, DDIV), qd_fx_set(QD_FXP_DMODE, 1), qd_fx_set(QD_FXP_DFDBK, 84);
    qd_fx_set(QD_FXP_DTONE, 58), qd_fx_set(QD_FXP_DWOW, 40);
    qd_fx_set(QD_FXP_RSIZE, 127), qd_fx_set(QD_FXP_RDECAY, 112), qd_fx_set(QD_FXP_RDAMP, 44), qd_fx_set(QD_FXP_RMOD, 84);
    qd_fx_set(QD_FXP_RPRE, 30), qd_fx_set(QD_FXP_RLOW, 84);

    {   /* the generative arps: MIDI FX groups 1 (the ARP track) and 2 (the BELLS) */
        static const int8_t MOD1[7] = {QD_AL_PLAY, QD_AL_PLAY, QD_AL_REST, QD_AL_PLAY, QD_AL_HI, QD_AL_PLAY, QD_AL_TIE};
        static const int8_t TR1[5] = {0, 0, 12, 0, -12};
        static const int8_t MOD2[5] = {QD_AL_PLAY, QD_AL_REST, QD_AL_PLAY, QD_AL_LO, QD_AL_REST};
        qd_mfx_slot_t *m = a->proj.mfx[0].slot;
        qd_mfx_slot_init(&m[0], QD_MFX_ARP);
        m[0].v[QD_MA_PAT] = QD_AP_ROTH, m[0].v[QD_MA_RATE] = 9, m[0].v[QD_MA_RANGE] = 2, m[0].v[QD_MA_MODE] = QD_AM_ON;
        m[0].v[QD_MA_MLEN] = 7, m[0].v[QD_MA_TLEN] = 5;
        memcpy(m[0].lane[0], MOD1, sizeof MOD1), memcpy(m[0].lane[1], TR1, sizeof TR1);
        qd_mfx_slot_init(&m[1], QD_MFX_RANDOM);
        m[1].v[QD_MD_VEL] = 36, m[1].v[QD_MD_OCT] = 1, m[1].v[QD_MD_CHANCE] = 30;
        qd_mfx_slot_init(&m[2], QD_MFX_CHANCE);
        m[2].v[QD_MC_CHANCE] = 88;
        m = a->proj.mfx[1].slot;
        qd_mfx_slot_init(&m[0], QD_MFX_ARP);
        m[0].v[QD_MA_PAT] = QD_AP_RAND, m[0].v[QD_MA_RATE] = 6, m[0].v[QD_MA_RANGE] = 2, m[0].v[QD_MA_MODE] = QD_AM_ON;
        m[0].v[QD_MA_MLEN] = 5;
        memcpy(m[0].lane[0], MOD2, sizeof MOD2);
        qd_mfx_slot_init(&m[1], QD_MFX_CHANCE);
        m[1].v[QD_MC_CHANCE] = 80;
    }

    /* A01 INTRO (one bar): the drone, the pad opening, a hiss and a fill into the beat */
    p = qd_proj_pat(&a->proj, 0);
    sounds(a, p);
    pad(p, 84, 3800, 7600, 1);
    drone(p, 78);
    qd_track_set_length(p, T_BELL, 16);
    stabs(p, 64);
    qd_track_set_length(p, T_LEAD, 16);
    qd_track_set_length(p, T_HAT, 16);
    drum(p, T_HAT, 7, KIT_TAPE, CRASH, 100, LS8);
    qd_track_set_length(p, T_DR, 16);
    p->trk[T_DR].swing = 1;
    hat(p, 8, KIT_DUST, 60, 0);
    hat(p, 10, KIT_DUST, 70, 0);
    kick(p, 11, KIT_DUST, 96);
    hat(p, 12, KIT_DUST, 78, 2);
    snare(p, 13, KIT_DUST, 90);
    snare(p, 14, KIT_DUST, 104);
    snare(p, 15, KIT_DUST, 116);
    qd_step_set_ratchet(p, T_DR, 15, 3, QD_RAMP_UP);
    p->master = 16;

    /* A02 BEAT: the kit, the 808, the pad half shut */
    p = qd_proj_pat(&a->proj, 1);
    sounds(a, p);
    beat(p, KIT_DUST, 0);
    perc(p, KIT_DUST, 0);
    bass(p, 0);
    pad(p, 84, 4600, 6200, 2);
    drone(p, 70);

    /* A03 STABS: .. the bell thrown into the delay, the pad a little wider */
    p = qd_proj_pat(&a->proj, 2);
    sounds(a, p);
    beat(p, KIT_DUST, 0);
    perc(p, KIT_DUST, 0);
    bass(p, 0);
    pad(p, 86, 5200, 7400, 2);
    drone(p, 70);
    stabs(p, 100);

    /* A04 SWEEP: the build. The pad's filter all the way up, the bass out of the second bar, the rolls */
    p = qd_proj_pat(&a->proj, 3);
    sounds(a, p);
    beat(p, KIT_DUST, 1);
    perc(p, KIT_DUST, 0);
    bass(p, 1);
    pad(p, 90, 4200, 11200, 2);
    for (i = 0; i < 32; i += 2)
        qd_lock_set(p, T_PAD, i, QD_QP_RESO, 4400 + i * 90);
    drone(p, 74);
    stabs(p, 92);
    drum(p, T_HAT, 17, KIT_TAPE, CRASH, 110, LS12);     /* the long hiss up to the drop */

    /* A05 DROP: everything, the pad open, the crash, the shaker */
    p = qd_proj_pat(&a->proj, 4);
    sounds(a, p);
    beat(p, KIT_DUST, 1);
    perc(p, KIT_DUST, 1);
    bass(p, 0);
    pad(p, 92, 9800, 6800, 2);
    drone(p, 74);
    stabs(p, 104);

    /* A06 BREAK: the tape kit at half time, no bass; HAZE and the stabs */
    p = qd_proj_pat(&a->proj, 5);
    sounds(a, p);
    tsound(a, p, T_PAD, "HAZE", P_HAZE);
    tsound(a, p, T_DR, "TAPE KICK", KIT_TAPE + KICK);
    pad(p, 86, 6000, 8800, 2);
    drone(p, 80);
    stabs(p, 96);
    qd_track_set_length(p, T_DR, 32);
    p->trk[T_DR].swing = 1;
    kick(p, 0, KIT_TAPE, 112);
    drum(p, T_DR, 8, KIT_TAPE, RIM, 92, L2);
    qd_lock_set(p, T_DR, 8, QD_LK_DELAY, 120);
    kick(p, 10, KIT_TAPE, 96);
    kick(p, 16, KIT_TAPE, 112);
    drum(p, T_DR, 24, KIT_TAPE, SNARE, 100, L2);
    qd_lock_set(p, T_DR, 24, QD_LK_REVERB, 110);
    for (i = 2; i < 32; i += 4)
        hat(p, i, KIT_TAPE, 60, i == 30 ? 3 : 0);

    /* A07 OUTRO: the drone, the pad closing, one kick into the black hole */
    p = qd_proj_pat(&a->proj, 6);
    sounds(a, p);
    pad(p, 80, 7600, 2600, 2);
    drone(p, 80);
    qd_track_set_length(p, T_DR, 32);
    kick(p, 0, KIT_DUST, 116);
    qd_lock_set(p, T_DR, 0, QD_LK_REVERB, 120);
    stabs(p, 70);

    /* A08 808: the same beat on the 808 kit and the SUB, TAPE PAD's chords */
    p = qd_proj_pat(&a->proj, 7);
    sounds(a, p);
    tsound(a, p, T_BA, "SUB", P_SUB);
    tsound(a, p, T_PAD, "TAPE PAD", P_TAPEPAD);
    tsound(a, p, T_LEAD, "GAMELAN", P_GAMELAN);
    tsound(a, p, T_DR, "808 KICK", KIT_808 + KICK);
    tsound(a, p, T_HAT, "808 CLAP", KIT_808 + CLAP);
    beat(p, KIT_808, 1);
    perc(p, KIT_808, 1);
    bass(p, 0);
    pad(p, 84, 6000, 8200, 2);
    drone(p, 66);
    stabs(p, 96);
    return 0;
}
