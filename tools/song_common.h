/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* What FIZZ's demo songs share (tools/song_*.c, each tools/tour.c's song hook): the preset pack into MY PRESETS and
 * the pool, a track's sound, notes, chords, drums as sound locks. A song defines T_DR (its drum track) before this. */
#pragma once
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "fizz_pack.h"
#include "qd_fx.h"

enum { L8 = 0, L4, L2, L34, L1, L15, LS2, LS3, LS4, LS6, LS8, LS12, LS16 };
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
