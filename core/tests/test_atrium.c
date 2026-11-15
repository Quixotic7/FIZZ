/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* 2026-11-15: the built-in ATRIUM (core/seq/demo.c, persist.c atrium_app) against the song it is made from,
 * tools/song_atrium.c (linked in: the tour's song hook), on RAM flashes of the host's layout:
 *   - the song's project: a new project, the song made in it (its MY presets stored from the pack, as the tour does);
 *   - a fresh flash's first project (qd_demo_first ATRIUM, as the firmware's): ATRIUM, then the same after a power-on;
 *   - compared: the 64 patterns byte for byte (each track sound's preset id mapped: the song's MY preset of a pack
 *     preset is factory id 24 + its place), the pool's 128 slots (the same mapping), the tracks' mix, names, kinds,
 *     limits, the MIDI FX groups, the effects, the project's settings; no MY preset stored; and the sound: each
 *     pattern played for a while from both, every sample the same;
 *   - an old unit: a project 1 of the old demo with A02 edited, the firmware's view (no old demo built in): A01 and A03
 *     blank, A02 the edit, its name, pool and mix kept, nothing written over.
 * At the host's sizes the whole comparison; at the device's (patterns and sounds paged) the fresh flash, the power-on
 * and the sound.
 *
 *     test_atrium        exit 1 on any failure */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#ifdef QD_DEVICE
#include "qd_arena.h"
#endif
#include "qd_fx.h"
#include "sounds.h"
#include "demo.h"
#include "../../tools/fizz_pack.h"

int fizz_tour_song(qd_app_t *a, int argc, char **argv);   /* tools/song_atrium.c */

static int checks, failed;
#define CHECK(c, ...) do { checks++; if (!(c)) { failed++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } else { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* two RAM flashes (the context: which) */
#define FSIZE 0x100000u
static uint8_t FL[2][FSIZE];
static int f_read(void *c, uint32_t off, void *d, uint32_t n)
{
    memcpy(d, FL[(intptr_t)c] + off, n);
    return off + n <= FSIZE ? 0 : -8;
}
static int f_erase(void *c, uint32_t off)
{
    memset(FL[(intptr_t)c] + off / 4096u * 4096u, 0xFF, 4096u);
    return 0;
}
static int f_prog(void *c, uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    if (off / 256u != (off + n - 1u) / 256u)
        return -8;
    for (i = 0; i < n; i++)
        FL[(intptr_t)c][off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
static const qd_flash_t FLASH0 = {f_read, f_erase, f_prog, (void *)0}, FLASH1 = {f_read, f_erase, f_prog, (void *)1};

static qd_app_t R, B;                                   /* the song's, the built-in's */
static uint32_t now;
static void boot(qd_app_t *a, const qd_flash_t *fl)
{
    qd_app_init(a);
    qd_app_tick(a, 0);
    qd_persist_attach(a, fl);
    now = 0;
}
static void run(qd_app_t *a, uint32_t n)
{
    while (n--)
        qd_app_tick(a, ++now);
}
static int flush(qd_app_t *a)
{
    int i;
    for (i = 0; i < 50; i++) {
        if (qd_persist_flush(a))
            return 1;
        run(a, 1);
    }
    return 0;
}

#if QD_PAT_RAM >= QD_NPAT
/* the song's MY preset id of a pack preset -> its factory id (the pack's place + 24); others as they are */
static int map_id(const qd_app_t *a, int id)
{
    qd_sound_t s;
    int j;
    if (id < QD_PRESET_MINE0 || !qd_preset_get(a, id, &s))
        return id;
    for (j = 0; j < FIZZ_PACK_LIB; j++)
        if (!strcmp(FIZZ_PACK[j].name, s.name))
            return QD_ATRIUM_ID0 + j;
    return id;
}

static int fx_ref[QD_NFXP];
static void compare(const char *when)
{
    static qd_pattern_t P;
    int k, t, i, pat_bad = 0, pool_bad = 0, trk_bad = 0, fx_bad = 0, lib = 0, first = -1;
    for (k = 0; k < QD_NPAT; k++) {
        P = *qd_proj_cpat(&R.proj, k);
        for (t = 0; t < QD_PAT_NTRACK; t++)
            P.snd[t].preset = (uint16_t)map_id(&R, P.snd[t].preset);
        if (memcmp(&P, qd_proj_cpat(&B.proj, k), sizeof P)) {
            pat_bad++;
            if (first < 0)
                first = k;
        }
    }
    CHECK(!pat_bad, "%s: the 64 patterns, byte for byte (the track sounds' preset ids mapped): %d differ (the first %d)",
          when, pat_bad, first);
    for (k = 1; k <= QD_NSOUND; k++) {
        qd_sound_t x, y;
        qd_sound_read(&R.proj, k, &x);
        qd_sound_read(&B.proj, k, &y);
        x.preset = (uint16_t)map_id(&R, x.preset);
        if (memcmp(&x, &y, sizeof x)) {
            if (!pool_bad)
                printf("    pool %d: %s %d (tags %d flags %d) / %s %d (tags %d flags %d)\n", k, x.name, x.preset, x.tags,
                       x.flags, y.name, y.preset, y.tags, y.flags);
            pool_bad++;
        }
    }
    CHECK(!pool_bad && B.proj.nsound == QD_ATRIUM_NPOOL && R.proj.nsound == QD_ATRIUM_NPOOL,
          "%s: the pool's 128 slots (17 used: references to factory presets; the song's MY presets mapped): %d differ",
          when, pool_bad);
    for (t = 0; t < QD_NTRACK; t++) {
        const qd_track_t *x = &R.model.t[t], *y = &B.model.t[t];
        trk_bad += memcmp(&x->mix, &y->mix, offsetof(qd_mix_track_t, g_lvl)) || strcmp(x->name, y->name) ||
                   x->kind != y->kind || x->named != y->named || x->limit != y->limit || x->reserve != y->reserve;
    }
    CHECK(!trk_bad, "%s: the tracks' mix (level, pan, sends, dry), names, kinds, voice limits and reserves: %d differ",
          when, trk_bad);
    for (i = 0; i < QD_NFXP; i++)
        fx_bad += qd_fx_get(i) != fx_ref[i];
    CHECK(!fx_bad, "%s: the effects' %d parameters: %d differ", when, QD_NFXP, fx_bad);
    CHECK(!memcmp(R.proj.mfx, B.proj.mfx, sizeof R.proj.mfx) && R.proj.key_scale == B.proj.key_scale &&
              R.proj.key_root == B.proj.key_root && R.proj.launch == B.proj.launch &&
              R.proj.rec_mode == B.proj.rec_mode && R.proj.rec_quant == B.proj.rec_quant &&
              R.proj.rec_dub == B.proj.rec_dub && R.proj.rec_auto == B.proj.rec_auto &&
              R.proj.pool_empty == B.proj.pool_empty,
          "%s: the MIDI FX groups, the key, launch and recording settings, the pool's start", when);
    for (k = 0; k < QD_NLIB; k++)
        lib += qd_lib_used(&B, k);
    CHECK(!lib && !strcmp(B.per.pname[B.per.proj], "ATRIUM") && B.per.pdemo == QD_DEMO_ATRIUM && B.per.proj == 0,
          "%s: project 1 ATRIUM (its demo byte %d), no MY preset stored (%d)", when, B.per.pdemo, lib);
}
#endif

/* pattern k played from a power-on for ms, its audio's hash (the effects: each run from their initial state) */
static int peak[QD_ATRIUM_N];
static uint32_t loads[2];                               /* (the device's sizes: the pool's sounds read into RAM, each) */
static uint32_t play_hash(qd_app_t *a, const qd_flash_t *fl, int k, uint32_t ms, const int *fx)
{
    static int16_t b[256];
    uint32_t h = 2166136261u, frames = 0;
    int i;
    boot(a, fl);
    qd_fx_init();
    for (i = 0; i < QD_NFXP; i++)
        qd_fx_set(i, fx[i]);
    run(a, 50);
    qd_seq_select(&a->seq, k, 1);
    a->snd_pat = -1;
    run(a, 10);
    qd_app_tempo(a, qd_seq_bpm(&a->seq));
    a->seed_fix = 77;
    qd_app_play(a);
    while (ms--) {
        run(a, 1);
        while (frames + 128u <= (uint64_t)(now - 60u) * QD_FS / 1000u) {
            int j;
            qd_app_audio(a, b, 128);
            for (j = 0; j < 256; j++) {
                h = (h ^ (uint16_t)b[j]) * 16777619u;
                peak[k] = abs(b[j]) > peak[k] ? abs(b[j]) : peak[k];
            }
            frames += 128u;
        }
    }
    qd_app_stop(a);
#if QD_SND_RAM
    loads[a == &B] = a->per.sc.loads;
#endif
    return h;
}

/* an old unit's project 1: the old demo's, A02 edited (written), its name typed */
static void old_unit_test(void)
{
#ifdef QD_DEMO_HOST
    static qd_app_t O;
    uint8_t hdr0[512];
    int len0, len1, a01, a02, a03, keep;
    memset(FL[0], 0xFF, FSIZE);
    qd_demo_first = QD_DEMO_OLD;
    qd_demo_old_on = 1;
    boot(&O, &FLASH0);
    run(&O, 100);
    qd_proj_fetch(&O.proj, 1);
    qd_step_on(qd_proj_pat(&O.proj, 1), 5, 9);          /* (A02: one more step, track 6) */
    O.edit_gen++;
    qd_proj_name(&O, 0, "MY SONG");
    O.model.t[2].mix.level = 33;
    O.per.maybe_dirty = 1;
    run(&O, 2000);
    flush(&O);
    len0 = qd_store_get(&O.per.prj_st, QD_RKEY(QD_RK_HDR, 0, 0), hdr0, sizeof hdr0);
    /* the update: the firmware's demos (no old one, ATRIUM first), the same flash */
    qd_demo_old_on = 0;
    qd_demo_first = QD_DEMO_ATRIUM;
    boot(&O, &FLASH0);
    run(&O, 3000);
    flush(&O);
    {
        uint8_t hdr1[512];
        len1 = qd_store_get(&O.per.prj_st, QD_RKEY(QD_RK_HDR, 0, 0), hdr1, sizeof hdr1);
        keep = len1 == len0 && !memcmp(hdr0, hdr1, (size_t)len0);
    }
    qd_proj_fetch(&O.proj, 0);
    a01 = qd_proj_cpat(&O.proj, 0)->used;
    qd_proj_fetch(&O.proj, 1);
    a02 = qd_proj_cpat(&O.proj, 1)->used && qd_step_type(qd_proj_cpat(&O.proj, 1), 5, 9) != QD_TRIG_NONE &&
          qd_step_type(qd_proj_cpat(&O.proj, 1), 0, 0) != QD_TRIG_NONE;
    qd_proj_fetch(&O.proj, 2);
    a03 = qd_proj_cpat(&O.proj, 2)->used;
    CHECK(keep && !a01 && a02 && !a03 && !strcmp(O.per.pname[0], "MY SONG") && O.per.pdemo == QD_DEMO_OLD &&
              O.model.t[2].mix.level == 33 && qd_sound_used(&O.proj, 1) && !O.per.pexists[1],
          "an old unit (project 1 the old demo, A02 edited, named MY SONG): updated, A01 and A03 blank, A02 its record, "
          "its header unchanged (name, mix, demo byte %d), its pool, no ATRIUM made", O.per.pdemo);
    qd_demo_old_on = 1;
#endif
}

int main(void)
{
    static int fx[QD_NFXP];
    int k, i, same = 0, thrash = 0;

    /* the song's project (tools/song_atrium.c, as `make -C tools tour SONG=song_atrium.c` and demos/atrium/song.txt
     * make it: a new project, then the song) */
    memset(FL[0], 0xFF, FSIZE);
    boot(&R, &FLASH0);
    run(&R, 100);
    CHECK(qd_proj_new(&R, 1), "the song's: a new project (project 2)");
    qd_proj_name(&R, 1, "ATRIUM");
    {
        char *av[1] = {"song"};
        CHECK(!fizz_tour_song(&R, 1, av), "the song's: tools/song_atrium.c made it");
    }
    R.snd_pat = -1;
    qd_seq_select(&R.seq, 0, 1);
    run(&R, 50);
    for (i = 0; i < QD_NFXP; i++)
        fx[i] = qd_fx_get(i);

    /* the built-in: a fresh flash, as the firmware's */
#ifdef QD_DEMO_HOST
    qd_demo_first = QD_DEMO_ATRIUM;
#endif
    memset(FL[1], 0xFF, FSIZE);
    boot(&B, &FLASH1);
    run(&B, 100);
    CHECK(B.per.pdemo == QD_DEMO_ATRIUM && !strcmp(B.per.pname[0], "ATRIUM") && qd_seq_bpm(&B.seq) == 71 &&
              B.seq.cur == 0,
          "a fresh flash: project 1 ATRIUM, A01 at 71 BPM");
#if QD_PAT_RAM >= QD_NPAT
    memcpy(fx_ref, fx, sizeof fx);
    compare("a fresh flash");
#endif
    {
        int n = 0;
        for (k = 1; k <= QD_NSOUND; k++) {
            uint8_t b[64];
            n += qd_store_get(&B.per.prj_st, QD_RKEY(QD_RK_SND, 0, k), b, sizeof b) == 4 && b[0] == 'R';
        }
        for (k = 0; k < QD_NPAT; k++)
            n += 100 * (qd_store_get(&B.per.prj_st, QD_RKEY(QD_RK_PAT, 0, k), 0, 0) >= 0);
        CHECK(n == QD_ATRIUM_NPOOL, "its records: the pool's 17 references ('R' 1), no pattern (the demo's code) (%d)", n);
    }
    flush(&B);
    boot(&B, &FLASH1);                                  /* a power-on */
    run(&B, 100);
#if QD_PAT_RAM >= QD_NPAT
    compare("after a power-on");
#else
    CHECK(B.per.pdemo == QD_DEMO_ATRIUM && !strcmp(B.per.pname[0], "ATRIUM") && B.proj.nsound == QD_ATRIUM_NPOOL,
          "after a power-on: ATRIUM, its 17 pool sounds");
#endif
    {
        int fx_bad = 0;
        for (i = 0; i < QD_NFXP; i++)
            fx_bad += qd_fx_get(i) != fx[i];
        CHECK(!fx_bad, "after a power-on: the effects the song's (%d differ)", fx_bad);
    }

    /* the sound: each pattern played 6 s from both, each from a power-on on its flash */
    flush(&R);
    for (k = 0; k < QD_ATRIUM_N; k++) {
        const uint32_t hr = play_hash(&R, &FLASH0, k, 6000, fx), hb = play_hash(&B, &FLASH1, k, 6000, fx);
        same += hr == hb;
        if (hr != hb && loads[0] > 4u * loads[1]) {     /* (the device's sizes: the song's pool reads its MY presets
                                                         * through the eight places the pool's sounds take, which a
                                                         * pattern locking eight drums overflows: its locks miss) */
            printf("    A%02d: the audio differs: the song's MY presets read %u times into the sound cache's 8 places, "
                   "the built-in's factory presets %u\n", k + 1, loads[0], loads[1]);
            thrash++;
        } else if (hr != hb)
            printf("    A%02d: the audio differs\n", k + 1);
    }
    {
        int quiet = 0;
        for (k = 0; k < QD_ATRIUM_N; k++)
            quiet += peak[k] < 2000;
        CHECK(same + (QD_SND_RAM ? thrash : 0) == QD_ATRIUM_N && !quiet,
              "the sound: each of A01..A10 played 6 s from the song's project and the built-in: %d of %d the same, sample "
              "for sample (%d differ by the song's MY presets' cache misses; none quiet: %d under 2000)", same,
              QD_ATRIUM_N, thrash, quiet);
    }

    old_unit_test();

    printf("test_atrium (%s): %d checks, %d failed\n", QD_PROFILE_NAME, checks, failed);
    return failed ? 1 : 0;
}
