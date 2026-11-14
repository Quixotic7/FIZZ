/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's demo song, "SUNFADE", and the preset pack (tools/fizz_pack.h), made through the editing API as the panel
 * would make them (tools/tour.c's song hook: `song` in a tour script; `song pack` stores the presets only).
 *
 * 84 BPM, swing 58 %. Two bars a pattern (the intro one), four chords: Fmaj7, G6, Em7, Am9 (half a bar each).
 *   T1 DRUMS   kick, snare, rim, tom: sound locks of the pool's kit (1..10 DUST, 11..20 TAPE, 21..30 808)
 *   T2 BASS    DUST BASS (mono, glides)
 *   T3 PAD     TAPE PAD: the chords
 *   T4 LEAD    WOW LEAD's melody, or TAPE PIANO's off-beat stabs (the track's sound is the pattern's)
 *   T5 BELL    HAZY BELL: six notes of E minor pentatonic over twelve steps, turning against the bar
 *   T6 HATS    hats, shaker, clap, hiss: the kit again
 * A01 INTRO  A02 BEAT  A03 KEYS  A04 LEAD  A05 FULL  A06 BREAK  A07 OUTRO  A08 808 */
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "fizz_pack.h"

enum { L8 = 0, L4, L2, L34, L1, L15, LS2, LS3, LS4, LS6, LS8, LS12, LS16 };
enum { T_DR, T_BA, T_PAD, T_LEAD, T_BELL, T_HAT };
/* a kit's ten, in the pack's order */
enum { KICK, SNARE, CLAP, RIM, HAT, OPEN, TOM, PERC, SHAKER, CRASH };
enum { KIT_DUST = 1, KIT_TAPE = 11, KIT_808 = 21 };

static int LIB[FIZZ_PACK_N];            /* each pack preset's MY PRESETS place */

static int pack_find(const char *name)
{
    int i;
    for (i = 0; i < FIZZ_PACK_N; i++)
        if (!strcmp(FIZZ_PACK[i].name, name))
            return i;
    fprintf(stderr, "song: no preset %s\n", name);
    return 0;
}

/* the pack into MY PRESETS (a preset of that name already there: kept) */
static int pack_store(qd_app_t *a)
{
    int i, n = 0;
    for (i = 0; i < FIZZ_PACK_N; i++) {
        const fizz_pack_t *k = &FIZZ_PACK[i];
        qd_sound_t s;
        int slot = -1, j;
        for (j = 0; j < QD_NLIB && slot < 0; j++)
            if (qd_lib_used(a, j) && !strcmp(qd_lib_name(a, j), k->name))
                slot = j;
        if (slot < 0) {
            slot = qd_lib_free_slot(a);
            if (slot < 0 || !qd_lib_store_ex(a, slot, k->name, k->patch, 0))
                return -1;
            n++;
        }
        qd_lib_read(a, slot, &s);
        s.tags = k->tag, s.pan = k->pan, s.chorus = k->chorus, s.delay = k->delay, s.reverb = k->reverb;
        qd_lib_write(a, slot, &s);
        LIB[i] = slot;
    }
    return n;
}

static int pool_add(qd_app_t *a, int i)
{
    qd_sound_t s;
    qd_lib_read(a, LIB[i], &s);
    return qd_pool_add_sound(a, &s);
}

/* track t's sound in pattern p: pack preset `name` (through the pool: number k) */
static void tsound(qd_app_t *a, qd_pattern_t *p, int t, const char *name, int k)
{
    const int i = pack_find(name);
    memcpy(p->snd[t].patch, FIZZ_PACK[i].patch, sizeof p->snd[t].patch);
    memset(p->snd[t].name, 0, sizeof p->snd[t].name);
    strncpy(p->snd[t].name, name, sizeof p->snd[t].name - 1);
    p->snd[t].sound = (uint8_t)k, p->snd[t].edited = 0, p->snd[t].preset = qd_lib_preset(a, LIB[i]);
}

static void note(qd_pattern_t *p, int t, int s, int n, int vel, int len)
{
    const uint8_t x = (uint8_t)n;
    qd_step_on(p, t, s);
    qd_step_set_notes(p, t, s, &x, 1);
    qd_step_set_vel(p, t, s, vel);
    qd_step_set_len(p, t, s, len);
}
static void chord(qd_pattern_t *p, int t, int s, const uint8_t *x, int n, int vel, int len, int strum)
{
    qd_step_on(p, t, s);
    qd_step_set_notes(p, t, s, x, n);
    qd_step_set_vel(p, t, s, vel);
    qd_step_set_len(p, t, s, len);
    if (strum)
        qd_step_set_strum(p, t, s, strum);
}
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

/* ---- the parts (two bars: steps 0..31) */
static const uint8_t CH[4][4] = {{53, 57, 60, 64}, {55, 59, 62, 64}, {52, 55, 59, 62}, {57, 60, 64, 71}};

static void pad(qd_pattern_t *p, int vel)
{
    int c;
    qd_track_set_length(p, T_PAD, 32);
    p->trk[T_PAD].limit = 4;
    for (c = 0; c < 4; c++)
        chord(p, T_PAD, c * 8, CH[c], 4, vel, LS8, 2);
}

static void bass(qd_pattern_t *p)
{
    qd_track_set_length(p, T_BA, 32);
    p->trk[T_BA].limit = 1;
    p->trk[T_BA].swing = 1;
    note(p, T_BA, 0, 29, 114, LS4);                     /* F */
    note(p, T_BA, 6, 41, 88, L1);
    note(p, T_BA, 7, 29, 102, L1);
    note(p, T_BA, 8, 31, 110, LS3);                     /* G */
    note(p, T_BA, 11, 31, 84, L1);
    note(p, T_BA, 14, 43, 90, L15);
    note(p, T_BA, 16, 28, 114, LS4);                    /* E */
    note(p, T_BA, 22, 40, 86, L1);
    note(p, T_BA, 23, 28, 100, L1);
    note(p, T_BA, 24, 33, 110, LS3);                    /* A */
    note(p, T_BA, 27, 33, 86, L1);
    note(p, T_BA, 30, 31, 96, LS2);                     /* .. down through G to the F */
    qd_step_set_trig(p, T_BA, 30, QD_TRIG_TIE);
}

static void bell(qd_pattern_t *p, int oct, int vel)
{
    static const uint8_t N[6] = {76, 71, 79, 74, 69, 71};
    static const uint8_t V[6] = {96, 70, 88, 74, 82, 64};
    int k;
    qd_track_set_length(p, T_BELL, 12);                 /* three beats against four */
    p->trk[T_BELL].limit = 3;
    for (k = 0; k < 6; k++)
        note(p, T_BELL, k * 2, N[k] + oct, V[k] * vel / 100, LS2);
}

static void lead(qd_pattern_t *p)
{
    qd_track_set_length(p, T_LEAD, 32);
    p->trk[T_LEAD].limit = 1;
    note(p, T_LEAD, 0, 76, 104, LS3);
    note(p, T_LEAD, 4, 74, 92, LS2);
    note(p, T_LEAD, 6, 72, 96, LS2);
    note(p, T_LEAD, 8, 71, 104, LS4);
    note(p, T_LEAD, 12, 67, 90, LS2);
    note(p, T_LEAD, 14, 69, 96, LS2);
    note(p, T_LEAD, 16, 71, 104, LS6);
    note(p, T_LEAD, 22, 67, 88, LS2);
    note(p, T_LEAD, 24, 69, 102, LS6);
    note(p, T_LEAD, 30, 72, 92, LS2);
    qd_step_set_cond(p, T_LEAD, 30, qd_cond_ab(2, 2));  /* the pick-up every other pass */
}

static void piano(qd_pattern_t *p)
{
    static const uint8_t ST[4][3] = {{69, 72, 76}, {71, 74, 76}, {67, 71, 74}, {72, 76, 79}};
    int c;
    qd_track_set_length(p, T_LEAD, 32);
    p->trk[T_LEAD].limit = 3;
    p->trk[T_LEAD].swing = 1;
    for (c = 0; c < 4; c++) {
        chord(p, T_LEAD, c * 8 + 3, ST[c], 3, 66, LS2, 1);
        if (c & 1) {
            chord(p, T_LEAD, c * 8 + 6, ST[c], 3, 50, L1, 0);
            qd_step_set_cond(p, T_LEAD, c * 8 + 6, qd_cond_ab(1, 2));
        }
    }
}

static void beat(qd_pattern_t *p, int kit, int full)
{
    qd_track_set_length(p, T_DR, 32);
    p->trk[T_DR].limit = 4;
    p->trk[T_DR].swing = 1;
    drum(p, T_DR, 0, kit, KICK, 122, L2);
    drum(p, T_DR, 4, kit, SNARE, 116, L2);
    drum(p, T_DR, 7, kit, KICK, 96, L2);
    drum(p, T_DR, 10, kit, KICK, 112, L2);
    drum(p, T_DR, 12, kit, SNARE, 118, L2);
    drum(p, T_DR, 15, kit, RIM, 54, L4);
    drum(p, T_DR, 16, kit, KICK, 122, L2);
    drum(p, T_DR, 19, kit, KICK, 88, L2);
    drum(p, T_DR, 20, kit, SNARE, 116, L2);
    drum(p, T_DR, 23, kit, RIM, 46, L4);
    qd_step_set_cond(p, T_DR, 23, qd_cond_ab(2, 2));
    drum(p, T_DR, 26, kit, KICK, 110, L2);
    drum(p, T_DR, 28, kit, SNARE, 120, L2);
    if (full) {
        drum(p, T_DR, 30, kit, TOM, 84, L2);
        drum(p, T_DR, 31, kit, SNARE, 70, L4);
        qd_step_set_ratchet(p, T_DR, 31, 2, QD_RAMP_UP);
    }
}

static void hats(qd_pattern_t *p, int kit, int full)
{
    static const uint8_t V[8] = {88, 60, 78, 58, 86, 62, 76, 66};
    int s;
    qd_track_set_length(p, T_HAT, 32);
    p->trk[T_HAT].limit = 3;
    p->trk[T_HAT].swing = 1;
    for (s = 0; s < 32; s += 2)
        drum(p, T_HAT, s, kit, HAT, V[(s / 2) & 7], L4);
    drum(p, T_HAT, 3, kit, HAT, 44, L4);                /* the swung ghosts */
    drum(p, T_HAT, 11, kit, HAT, 48, L4);
    drum(p, T_HAT, 19, kit, HAT, 44, L4);
    drum(p, T_HAT, 14, kit, OPEN, 84, LS2);
    drum(p, T_HAT, 30, kit, OPEN, 88, LS2);
    drum(p, T_HAT, 12, kit, CLAP, 96, L2);              /* under the snare */
    drum(p, T_HAT, 28, kit, CLAP, 104, L2);
    if (full) {
        drum(p, T_HAT, 5, kit, SHAKER, 60, L2);
        drum(p, T_HAT, 13, kit, SHAKER, 64, L2);
        drum(p, T_HAT, 21, kit, SHAKER, 60, L2);
        drum(p, T_HAT, 27, kit, HAT, 70, L4);
        qd_step_set_ratchet(p, T_HAT, 27, 3, QD_RAMP_UP);
        drum(p, T_HAT, 0, kit, CRASH, 80, LS8);
        qd_step_set_cond(p, T_HAT, 0, QD_COND_1ST);
    }
}

/* every pattern's tracks: the song's sounds (pool numbers 31..) */
enum { P_BASS = 31, P_PAD, P_BELL, P_LEAD, P_PIANO, P_HAZE, P_BOX, P_VOX, P_SUB, P_KALIMBA };
static void sounds(qd_app_t *a, qd_pattern_t *p, int lead_is_piano)
{
    tsound(a, p, T_DR, "DUST KICK", KIT_DUST + KICK);
    tsound(a, p, T_BA, "DUST BASS", P_BASS);
    tsound(a, p, T_PAD, "TAPE PAD", P_PAD);
    tsound(a, p, T_LEAD, lead_is_piano ? "TAPE PIANO" : "WOW LEAD", lead_is_piano ? P_PIANO : P_LEAD);
    tsound(a, p, T_BELL, "HAZY BELL", P_BELL);
    tsound(a, p, T_HAT, "DUST HAT", KIT_DUST + HAT);
    p->bpm = 84;
    p->swing = 58;
    p->used = 1;
}

int fizz_tour_song(qd_app_t *a, int argc, char **argv)
{
    static const char *const POOL[] = {"DUST BASS", "TAPE PAD", "HAZY BELL", "WOW LEAD", "TAPE PIANO", "HAZE",
                                       "MUSIC BOX", "VOX HUM", "SUB", "KALIMBA"};
    static const struct { uint8_t level, chorus, delay, reverb; int8_t pan; } MIX[QD_NTRACK] = {
        {110, 0, 0, 14, 0}, {108, 0, 0, 4, 0}, {96, 55, 20, 75, 0}, {102, 30, 42, 55, 8}, {107, 45, 44, 70, -10},
        {110, 0, 6, 10, 0}};
    static const char *const TNAME[QD_NTRACK] = {"DRUMS", "BASS", "PAD", "LEAD", "BELL", "HATS"};
    static const uint8_t TKIND[QD_NTRACK] = {QD_PK_DRUMS, QD_PK_BASS, QD_PK_PAD, QD_PK_LEAD, QD_PK_KEYS, QD_PK_DRUMS};
    qd_pattern_t *p;
    int i, n, t;
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

    /* A01 INTRO (one bar): the first two chords and the bell; the tape kit's hats, a hiss and a fill into the beat */
    p = qd_proj_pat(&a->proj, 0);
    sounds(a, p, 1);
    qd_track_set_length(p, T_PAD, 16);
    p->trk[T_PAD].limit = 4;
    chord(p, T_PAD, 0, CH[0], 4, 82, LS8, 3);
    chord(p, T_PAD, 8, CH[1], 4, 82, LS8, 3);
    bell(p, 0, 96);
    qd_track_set_length(p, T_HAT, 16);
    p->trk[T_HAT].swing = 1;
    for (i = 4; i < 16; i += 2)
        drum(p, T_HAT, i, KIT_TAPE, HAT, 50 + i * 2, L4);
    drum(p, T_HAT, 9, KIT_TAPE, CRASH, 96, LS6);        /* (the tape kit's tenth: TAPE HISS, a swell) */
    qd_track_set_length(p, T_DR, 16);
    drum(p, T_DR, 12, KIT_DUST, TOM, 90, L2);
    drum(p, T_DR, 14, KIT_DUST, SNARE, 84, L2);
    drum(p, T_DR, 15, KIT_DUST, SNARE, 100, L4);
    qd_step_set_ratchet(p, T_DR, 15, 2, QD_RAMP_UP);
    p->master = 16;

    /* A02 BEAT: the kit, the bass, the chords */
    p = qd_proj_pat(&a->proj, 1);
    sounds(a, p, 1);
    beat(p, KIT_DUST, 0);
    hats(p, KIT_DUST, 0);
    bass(p);
    pad(p, 84);

    /* A03 KEYS: .. the bell and the piano's stabs */
    p = qd_proj_pat(&a->proj, 2);
    sounds(a, p, 1);
    beat(p, KIT_DUST, 0);
    hats(p, KIT_DUST, 0);
    bass(p);
    pad(p, 84);
    bell(p, 0, 100);
    piano(p);

    /* A04 LEAD: .. the melody */
    p = qd_proj_pat(&a->proj, 3);
    sounds(a, p, 0);
    beat(p, KIT_DUST, 0);
    hats(p, KIT_DUST, 0);
    bass(p);
    pad(p, 84);
    bell(p, 0, 92);
    lead(p);

    /* A05 FULL: everything, the fill, the shaker, the crash on the first pass, the bell an octave up */
    p = qd_proj_pat(&a->proj, 4);
    sounds(a, p, 0);
    beat(p, KIT_DUST, 1);
    hats(p, KIT_DUST, 1);
    bass(p);
    pad(p, 90);
    bell(p, 12, 84);
    lead(p);

    /* A06 BREAK: the tape kit, no bass; HAZE under the melody */
    p = qd_proj_pat(&a->proj, 5);
    sounds(a, p, 0);
    tsound(a, p, T_PAD, "HAZE", P_HAZE);
    tsound(a, p, T_DR, "TAPE KICK", KIT_TAPE + KICK);
    tsound(a, p, T_HAT, "TAPE HAT", KIT_TAPE + HAT);
    pad(p, 86);
    lead(p);
    bell(p, 0, 80);
    qd_track_set_length(p, T_DR, 32);
    p->trk[T_DR].swing = 1;
    drum(p, T_DR, 0, KIT_TAPE, KICK, 110, L2);
    drum(p, T_DR, 10, KIT_TAPE, KICK, 96, L2);
    drum(p, T_DR, 12, KIT_TAPE, RIM, 90, L2);
    drum(p, T_DR, 16, KIT_TAPE, KICK, 110, L2);
    drum(p, T_DR, 28, KIT_TAPE, RIM, 96, L2);
    drum(p, T_DR, 30, KIT_TAPE, PERC, 70, L2);
    qd_track_set_length(p, T_HAT, 32);
    p->trk[T_HAT].swing = 1;
    for (i = 2; i < 32; i += 4)
        drum(p, T_HAT, i, KIT_TAPE, i % 8 == 6 ? SHAKER : HAT, 62, L4);

    /* A07 OUTRO: the chords, the bell, one kick */
    p = qd_proj_pat(&a->proj, 6);
    sounds(a, p, 1);
    tsound(a, p, T_BELL, "MUSIC BOX", P_BOX);
    pad(p, 76);
    bell(p, 12, 80);
    qd_track_set_length(p, T_DR, 32);
    drum(p, T_DR, 0, KIT_TAPE, KICK, 108, L2);

    /* A08 808: the same song on the 808 kit and the SUB */
    p = qd_proj_pat(&a->proj, 7);
    sounds(a, p, 1);
    tsound(a, p, T_BA, "SUB", P_SUB);
    tsound(a, p, T_BELL, "KALIMBA", P_KALIMBA);
    tsound(a, p, T_DR, "808 KICK", KIT_808 + KICK);
    tsound(a, p, T_HAT, "808 HAT", KIT_808 + HAT);
    beat(p, KIT_808, 1);
    hats(p, KIT_808, 1);
    bass(p);
    pad(p, 84);
    bell(p, 0, 96);
    piano(p);
    return 0;
}
