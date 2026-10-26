/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.5a: saving, the app's side (core/app/persist.h) on a RAM NOR of 1 MiB (the store's own tests, power cuts among
 * them, are test_store.c):
 *   - a fresh flash: the first project with its demos, written;
 *   - the tracks' sounds are each pattern's (switching patterns switches them);
 *   - written by itself (1.5 s after the last edit), §7's counters while playing (no erase while sounding);
 *   - a full project's round trip (64 patterns, two of them full, 128 pool sounds, the mix, the settings) through a
 *     new app on the same flash: the same project, pattern and track (the restart check);
 *   - the pool: move (swap) renumbers every sound lock and track source in all 64 patterns; delete sends the locks back
 *     to the track's sound, INIT stays; copy;
 *   - the library: store, copy, swap, delete, kept across a restart;
 *   - the projects: new, copy, load, delete, name.
 *
 *     test_persist        exit 1 on any failure */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#ifdef QD_DEVICE
#include "qd_arena.h"
#endif
#include "qd_fx.h"
#include "sounds.h"
#include "names.h"
#include "ui.h"
#include "demo.h"
#include "snd_peek.h"                          /* (P2.1: LIB, POOL) */

static int checks, failed;
#define CHECK(c, ...) do { checks++; if (!(c)) { failed++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define FSIZE 0x100000u
static uint8_t FL[FSIZE];
static int f_read(void *c, uint32_t off, void *d, uint32_t n) { (void)c; memcpy(d, FL + off, n); return off + n <= FSIZE ? 0 : -8; }
static int f_erase(void *c, uint32_t off) { (void)c; memset(FL + off / 4096u * 4096u, 0xFF, 4096u); return 0; }
static int f_prog(void *c, uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    (void)c;
    if (off / 256u != (off + n - 1u) / 256u)
        return -8;
    for (i = 0; i < n; i++)
        FL[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
static const qd_flash_t FLASH = {f_read, f_erase, f_prog, 0};

static qd_app_t A, B;
static uint32_t now;

static void boot(qd_app_t *a)                           /* a power-on on the flash as it is */
{
    qd_app_init(a);
    qd_app_tick(a, 0);
    qd_persist_attach(a, &FLASH);
    now = 0;
}
static void run(qd_app_t *a, uint32_t n)
{
    while (n--)
        qd_app_tick(a, ++now);
}

static uint32_t rnd_s = 12345u;
static uint32_t rnd(void)
{
    rnd_s ^= rnd_s << 13, rnd_s ^= rnd_s >> 17, rnd_s ^= rnd_s << 5;
    return rnd_s;
}

static void fill_pattern(qd_pattern_t *P, int nsteps, int nlocks)
{
    int i;
    for (i = 0; i < nsteps; i++) {
        const int t = (int)(rnd() % (uint32_t)QD_PAT_NTRACK), s = nsteps >= 1024 ? i % 128 + (i / 128) * 0 : (int)(rnd() % 128u);
        const int tt = nsteps >= 1024 ? i / 128 : t;
        uint8_t n[1] = {(uint8_t)(36 + rnd() % 48u)};
        qd_step_on(P, tt, s);
        qd_step_set_notes(P, tt, s, n, 1);
        qd_step_set_vel(P, tt, s, (int)(1 + rnd() % 127u));
    }
    for (i = 0; i < nlocks; i++)
        qd_lock_set(P, (int)(rnd() % (uint32_t)QD_PAT_NTRACK), (int)(rnd() % 128u), (int)(rnd() % QD_QP_LEVEL), (int16_t)(rnd() % 50u));
    P->used = 1;
}


/* ---------------------------------------------------------------- P1.5a part 2: the pages, by the panel */
static qd_app_t *G;
static void p_run(uint32_t n) { run(G, n); }
static void bt(int b, int down) { qd_app_button(G, b, down); }
static void btap(int b) { bt(b, 1); p_run(20); bt(b, 0); p_run(20); }
static void prj_tap(void) { bt(QD_B_FUNC, 1); btap(QD_B_SAVE); bt(QD_B_FUNC, 0); p_run(20); }   /* T1b: SHIFT + SAVE */
static void wkey(int w, int down) { qd_app_key(G, QD_WHITE_KEY[w], down, 100); }
static void wtap(int w) { wkey(w, 1); p_run(30); wkey(w, 0); p_run(20); }
static void bkey(int b, int down) { qd_app_key(G, QD_BLACK_KEY[b], down, 100); }
static void black(int b) { bkey(b, 1); p_run(20); bkey(b, 0); p_run(20); }
static void knob(int k, int d) { qd_app_knob(G, k, d); p_run(5); }

static void pages_test(void)
{
    static const struct { const char *name; int kind; } NAMES[] = {
        {"EP", QD_SK_KEYS}, {"BELL", QD_SK_BELL}, {"BASS", QD_SK_BASS}, {"PLUCK", QD_SK_PLUCK}, {"BRASS", QD_SK_LEAD},
        {"GLASS PAD", QD_SK_PAD}, {"HOLLOW", QD_SK_LEAD}, {"SQUARE LEAD", QD_SK_LEAD}, {"METAL", QD_SK_BELL},
        {"WOBBLE", QD_SK_BASS}, {"CLAV", QD_SK_PLUCK}, {"STRINGS", QD_SK_PAD}, {"MARIMBA", QD_SK_PLUCK},
        {"DRONE", QD_SK_PAD}, {"FEEDBACK", QD_SK_LEAD}, {"NOISE-ISH", QD_SK_NOISE}, {"KICK", QD_SK_KICK},
        {"SNARE", QD_SK_SNARE}, {"CLAP", QD_SK_CLAP}, {"GHOST", QD_SK_SNARE}, {"HAT", QD_SK_HAT},
        {"OPEN HAT", QD_SK_HAT}};
    char nm[QD_SOUND_NAME], nm2[QD_SOUND_NAME];
    int k, ok = 0, okn = 0;
    G = &A;
    memset(FL, 0xFF, sizeof FL);
    boot(G);
    p_run(50);
    /* the names from the sound: the factory sounds' and the drums' kinds, twelve letters, the same each time */
    for (k = 0; k < (int)(sizeof NAMES / sizeof NAMES[0]); k++) {
        int j;
        for (j = 1; j <= G->proj.nsound && strcmp(POOL(&G->proj, j)->name, NAMES[k].name); j++)
            ;
        ok += qd_sound_kind(POOL(&G->proj, j)->patch) == NAMES[k].kind;
        qd_sound_autoname(POOL(&G->proj, j)->patch, 0, nm);
        qd_sound_autoname(POOL(&G->proj, j)->patch, 0, nm2);
        okn += !strcmp(nm, nm2) && strlen(nm) <= 12 && strstr(nm, NAMES[k].kind == QD_SK_HAT ? "HAT" : qd_sound_kind_name(NAMES[k].kind));
        qd_sound_autoname(POOL(&G->proj, j)->patch, 1, nm2);
        okn += strcmp(nm, nm2) != 0;
    }
    CHECK(ok == (int)(sizeof NAMES / sizeof NAMES[0]) && okn == 2 * (int)(sizeof NAMES / sizeof NAMES[0]),
          "the classifier: the 16 factory sounds' and the 6 drums' kinds (%d), names of 12 letters at most with the "
          "kind, the same each time, another with the dice (%d)", ok, okn);

    {   /* P1.5t: the seven kinds of the factory presets (the browser's filters) */
        static const int PK[QD_NFACTORY] = {QD_PK_LEAD, QD_PK_KEYS, QD_PK_KEYS, QD_PK_BASS, QD_PK_PLUCK, QD_PK_LEAD,
                                            QD_PK_PAD, QD_PK_LEAD, QD_PK_LEAD, QD_PK_FX, QD_PK_BASS, QD_PK_PLUCK,
                                            QD_PK_PAD, QD_PK_PLUCK, QD_PK_PAD, QD_PK_FX, QD_PK_FX, QD_PK_DRUMS,
                                            QD_PK_DRUMS, QD_PK_DRUMS, QD_PK_DRUMS, QD_PK_DRUMS, QD_PK_DRUMS};
        int seen[QD_NPK] = {0};
        for (ok = 0, k = 0; k < QD_NFACTORY; k++) {
            qd_sound_t f;
            qd_factory_preset(k, &f);
            ok += f.tags == QD_PKB(PK[k]) && f.preset == k + 1;
            seen[qd_tags_first(f.tags)]++;
        }
        for (k = 0; k < QD_NPK; k++)
            ok += seen[k] > 0;
        CHECK(ok == QD_NFACTORY + QD_NPK, "the factory presets' kinds (METAL, FEEDBACK, NOISE-ISH FX; the drums DRUMS; "
              "BELL KEYS ..), every one of the seven used (%d)", ok);
    }

    /* P1.5t: SAVE PRESET: the track's sound into MY presets */
    qd_ctl_select_track(G, 3);
    btap(QD_B_SAVE);
    {   /* (P1.5y: the suggestion: the namesizer pair of the name's seed) */
        uint32_t sd = G->ui.ty.seed;
        qd_name_rnd(&sd, nm);
    }
    CHECK(qd_view_screen(G) == QD_SCR_SAVE && G->ui.ty.spark && G->ui.ty.sel && !strcmp(G->ui.ty.name, nm) &&
              G->ui.sv.replace < 0, "SAVE: SAVE PRESET, a suggested name (%s), no REPLACE (a factory sound)", nm);
    btap(QD_B_SAVE);
    CHECK(!strcmp(LIB(G, 0)->name, nm) && !memcmp(LIB(G, 0)->patch, G->model.t[3].patch, sizeof LIB(G, 0)->patch) &&
              LIB(G, 0)->preset >= QD_PRESET_MINE0 && G->model.t[3].preset == LIB(G, 0)->preset &&
              !G->model.t[3].edited && !G->ui.latch && G->ui.tk == QD_TK_SAVED,
          "SAVE again: a NEW MY preset (place 1, its own id), the track plays it, not edited; SAVED");
    G->model.t[3].patch[QD_QP_HARM] = 900;              /* edited, then SAVE: REPLACE offered */
    G->model.t[3].edited = 1;
    p_run(20);
    btap(QD_B_SAVE);
    CHECK(G->ui.sv.replace == 0 && !strcmp(G->ui.ty.name, nm), "edited, SAVE: REPLACE offered (its "
          "name kept)");
    btap(QD_B_OCTUP);                                   /* (P1.5v: F2 chooses REPLACE, SAVE does it) */
    btap(QD_B_SAVE);
    CHECK(LIB(G, 0)->patch[QD_QP_HARM] == 900 && LIB(G, 0)->preset == G->model.t[3].preset &&
              !G->model.t[3].edited && !LIB(G, 1)->name[0], "F2: replaced (the same id), nothing new");
    /* the browser on MY presets: load it onto another track */
    qd_ctl_select_track(G, 1);
    p_run(QD_TK_MS + QD_TK_FADE_MS + 10);
    knob(QD_K_PRESETS, 1);
    CHECK(qd_view_screen(G) == QD_SCR_BROWSE, "PRESETS: the library browser");
    black(8);                                           /* B9: MY PRESETS */
    knob(QD_K_PRESETS, 1);
    btap(QD_B_OCTUP);
    CHECK(G->model.t[1].preset == LIB(G, 0)->preset && G->model.t[1].patch[QD_QP_HARM] == 900 &&
              qd_proj_pat(&G->proj, G->seq.cur)->snd[1].preset == LIB(G, 0)->preset, "B9, a turn, F2: MY preset loaded on track 2");

    /* the project pages */
    prj_tap();
    CHECK(qd_view_screen(G) == QD_SCR_PRJ && G->ui.pj.page == 0, "SHIFT + SAVE: the project page");
    prj_tap();
    CHECK(G->ui.pj.page == 1, "SHIFT + SAVE again: the projects");
    wtap(2);
    btap(QD_B_OCTUP);
    CHECK(G->per.proj == 2 && G->per.pexists[2] && G->ui.pj.page == 0, "W3, F2: a new project 3, its page");
    prj_tap();
    bt(QD_B_OCTDN, 1);
    p_run(20);
    wtap(2);
    wtap(5);
    bt(QD_B_OCTDN, 0);
    p_run(20);
    CHECK(G->per.pexists[5] && qd_view_screen(G) == QD_SCR_PRJ, "F1 + W3, W6: project 3 copied to 6");
    bkey(10, 1);
    p_run(20);
    wtap(5);
    bkey(10, 0);
    p_run(20);
    CHECK(!G->per.pexists[5], "B11 + W6: deleted");
    black(9);                                           /* B10: named on the keybed (P1.5u) */
    CHECK(qd_view_screen(G) == QD_SCR_NAME && G->ui.ty.sel && !strcmp(G->ui.ty.name, G->per.pname[2]),
          "B10: the project's name, selected");
    wtap(6), black(0), black(8), wtap(2);               /* R A I N */
    btap(QD_B_SAVE);
    CHECK(!strcmp(G->per.pname[2], "RAIN") && qd_view_screen(G) == QD_SCR_PRJ, "R, A, I, N, SAVE: the project named "
          "(%s), the project page", G->per.pname[2]);
    btap(QD_B_HOME);
    CHECK(!G->ui.latch, "HOME: back");
    CHECK(G->per.prj_st.c.erases_noisy == 0, "no erase while sounding");
}

/* P1.5t: a 0.6 flash: its library slots become MY presets (ids, kinds), its pool stays as it was (its factory copies
 * knowing their factory preset), its track sounds learn their preset from their pool source; its absent pool records
 * are still the old starting pool (its header has no pool byte) */
static void migration_test(void)
{
    static uint8_t b[QDS_MAXREC * 256];
    uint8_t blob[QD_QUAD_BLOB5];
    int16_t p[QD_QUAD_NP];
    int len, t, n;
    G = &A;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);                                           /* (an erased flash: the demo project, written) */
    run(&A, 20);
    qd_persist_flush(&A);
    /* 0.6's records: a library slot, a pool slot (EP's copy), the pattern's track sounds, the header */
    qd_quad_preset_patch(3, p);                         /* (BASS) */
    qd_quad_pack5(p, blob);
    b[0] = 'S', b[1] = 1;
    memset(b + 2, 0, QD_SOUND_NAME);
    strcpy((char *)b + 2, "OLD LIB");
    memcpy(b + 2 + QD_SOUND_NAME, blob, QD_QUAD_BLOB5);
    qd_store_put(&A.per.lib_st, QD_RKEY(QD_RK_LIB, 0, 7), b, 2 + QD_SOUND_NAME + QD_QUAD_BLOB5);
    qd_store_flush(&A.per.lib_st);
    qd_quad_preset_patch(1, p);                         /* (EP, as the 0.6 pool had it) */
    qd_quad_pack5(p, blob);
    memset(b + 2, 0, QD_SOUND_NAME);
    strcpy((char *)b + 2, "EP");
    memcpy(b + 2 + QD_SOUND_NAME, blob, QD_QUAD_BLOB5);
    qd_store_put(&A.per.prj_st, QD_RKEY(QD_RK_SND, 0, 40), b, 2 + QD_SOUND_NAME + QD_QUAD_BLOB5);
    qd_store_flush(&A.per.prj_st);
    qd_proj_pat(&A.proj, QD_PAT_START)->snd[3].sound = 40;         /* (track 4, SQUARE LEAD's place, from pool 40) */
    qd_proj_pat(&A.proj, QD_PAT_START)->snd[3].preset = 0;
    qd_proj_pat(&A.proj, QD_PAT_START)->used = 1;
    qd_proj_pat(&A.proj, QD_PAT_START)->trk[2].mfx = 3;            /* (P1.6c: cut off below: OFF when read) */
    qd_persist_write_v4 = 1;                            /* (P2.1: a 'P' 4 record, made older below) */
    qd_persist_flush(&A);
    qd_persist_write_v4 = 0;
    len = qd_store_get(&A.per.prj_st, QD_RKEY(QD_RK_PAT, 0, QD_PAT_START), b, sizeof b);
    {   /* 'P' 3 -> 'P' 1: each track sound a 96-byte blob (E1), without its two preset bytes; the mix's lock ids as
         * 0.6 had them (74..) */
        const uint32_t one3 = QD_QUAD_BLOB + 2 + QD_SOUND_NAME + 2u, one1 = QD_QUAD_BLOB5 + 2 + QD_SOUND_NAME;
        const uint32_t start = (uint32_t)len - (uint32_t)QD_PAT_NTRACK * one3;
        static uint8_t c[QDS_MAXREC * 256];
        memcpy(c, b, start);
        for (t = 0; t < QD_PAT_NTRACK; t++) {
            int16_t q[QD_QUAD_NP];
            qd_quad_unpack(b + start + (uint32_t)t * one3, QD_QUAD_BLOB, q);
            qd_quad_pack5(q, c + start + (uint32_t)t * one1);
            memcpy(c + start + (uint32_t)t * one1 + QD_QUAD_BLOB5, b + start + (uint32_t)t * one3 + QD_QUAD_BLOB,
                   2 + QD_SOUND_NAME);
        }
        c[1] = 1;
        {   /* (P1.6c: and its track settings 42 bytes, without the MIDI FX field) */
            const uint32_t hd = 12u, ts = (uint32_t)sizeof(qd_tset_t), cut = ts - QD_TSET_SIZE_OLD;
            uint32_t k, total = start + (uint32_t)QD_PAT_NTRACK * one1;
            for (k = 0; k < (uint32_t)QD_PAT_NTRACK; k++)
                memmove(c + hd + k * QD_TSET_SIZE_OLD, c + hd + k * ts, QD_TSET_SIZE_OLD);
            memmove(c + hd + (uint32_t)QD_PAT_NTRACK * QD_TSET_SIZE_OLD, c + hd + (uint32_t)QD_PAT_NTRACK * ts, total - (hd + (uint32_t)QD_PAT_NTRACK * ts));
            total -= (uint32_t)QD_PAT_NTRACK * cut;
            c[3] = QD_TSET_SIZE_OLD;
            qd_store_put(&A.per.prj_st, QD_RKEY(QD_RK_PAT, 0, QD_PAT_START), c, total);
        }
        qd_store_flush(&A.per.prj_st);
    }
    len = qd_store_get(&A.per.prj_st, QD_RKEY(QD_RK_HDR, 0, 0), b, sizeof b);
    qd_store_put(&A.per.prj_st, QD_RKEY(QD_RK_HDR, 0, 0), b, (uint32_t)len - 1u);   /* (no pool byte) */
    qd_store_flush(&A.per.prj_st);
    boot(&B);
    CHECK(!strcmp(LIB(&B, 7)->name, "OLD LIB") && LIB(&B, 7)->preset == QD_PRESET_MINE0 + 7 &&
              LIB(&B, 7)->tags == QD_PKB(QD_PK_BASS) && !LIB(&B, 7)->flags,
          "a 0.6 library slot: one of MY presets (its id %d, its kind BASS, saved)", LIB(&B, 7)->preset);
    CHECK(!strcmp(POOL(&B.proj, 40)->name, "EP") && POOL(&B.proj, 40)->preset == 2 && POOL(&B.proj, 40)->tags == QD_PKB(QD_PK_KEYS),
          "a 0.6 pool sound: kept, knowing its factory preset (EP: 2), its kind");
    for (n = 0, t = 1; t <= QD_NFACTORY; t++)
        n += qd_sound_used(&B.proj, t) && POOL(&B.proj, t)->preset == t;
    CHECK(n == QD_NFACTORY && !B.proj.pool_empty, "its absent pool records: the old starting pool, as before (%d)", n);
    CHECK(qd_proj_pat(&B.proj, QD_PAT_START)->trk[2].mfx == 0 && qd_proj_pat(&B.proj, QD_PAT_START)->trk[2].limit ==
              qd_proj_pat(&A.proj, QD_PAT_START)->trk[2].limit, "P1.6c: an older pattern record (42-byte track settings): its tracks' "
          "MIDI FX OFF, the rest as it was");
    CHECK(qd_proj_pat(&B.proj, QD_PAT_START)->snd[3].preset == 2 && qd_proj_pat(&B.proj, QD_PAT_START)->snd[0].preset > 0,
          "its track sounds: their presets from their pool source (4: EP) or their factory sound (%d)",
          qd_proj_pat(&B.proj, QD_PAT_START)->snd[0].preset);
    qd_persist_flush(&B);
    boot(&A);
    CHECK(LIB(&A, 7)->preset == QD_PRESET_MINE0 + 7 && qd_proj_pat(&A.proj, QD_PAT_START)->snd[3].preset == 2,
          "written back as 0.7's records: the same after another restart");
}

/* P1.6c: the MIDI FX groups and the routing kept across a restart; a project without a groups record: empty */
static void mfx_test(void)
{
    int ok;
    G = &A;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    run(&A, 20);
    CHECK(!qd_mfx_group_active(&A.proj.mfx[0]) && !qd_proj_pat(&A.proj, 0)->trk[0].mfx && !qd_proj_pat(&A.proj, 0)->step[0][0].mfx,
          "P1.6c: a new project: every group empty, the tracks OFF, the steps TRACK");
    qd_mfx_slot_init(&A.proj.mfx[2].slot[1], QD_MFX_ARP);
    A.proj.mfx[2].slot[1].v[QD_MA_PAT] = QD_AP_CONDIV, A.proj.mfx[2].slot[1].lane[0][3] = QD_AL_TIE;
    A.proj.mfx[2].slot[1].lane[1][15] = -48, A.proj.mfx[2].slot[1].bypass = 1;
    qd_mfx_slot_init(&A.proj.mfx[4].slot[7], QD_MFX_SELECTOR);
    qd_proj_pat(&A.proj, QD_PAT_START)->trk[5].mfx = 3;
    qd_proj_pat(&A.proj, QD_PAT_START)->step[5][9].mfx = QD_SMFX_G1 + 4;
    qd_proj_pat(&A.proj, QD_PAT_START)->used = 1;
    run(&A, 2000);                                      /* (written by itself) */
    qd_persist_flush(&A);
    boot(&B);
    ok = !memcmp(B.proj.mfx, A.proj.mfx, sizeof A.proj.mfx);
    CHECK(ok && B.proj.mfx[2].slot[1].bypass && qd_proj_pat(&B.proj, QD_PAT_START)->trk[5].mfx == 3 &&
              qd_proj_pat(&B.proj, QD_PAT_START)->step[5][9].mfx == QD_SMFX_G1 + 4,
          "P1.6c: the groups (an ARP with its lanes, bypassed: the bypass saved; a SELECTOR), a track's group, a step's: "
          "the same after a restart");
}

/* P1.5v: the tags: several on one of MY presets and on a pool sound, the factory's as the owner set them, kept across
 * a restart; an 'S' 2 record's kind becomes its one tag */
/* E1: an 'S' 5 record as an older one had it: its blob 96 bytes (version 5), the rest as it was; the new length */
static long snd_to_v5(uint8_t *b, long len)
{
    int16_t q[QD_QUAD_NP];
    const uint32_t o = 2 + QD_SOUND_NAME;
    qd_quad_unpack(b + o, QD_QUAD_BLOB, q);
    qd_quad_pack5(q, b + o);
    memmove(b + o + QD_QUAD_BLOB5, b + o + QD_QUAD_BLOB, (size_t)len - o - QD_QUAD_BLOB);
    return len - (QD_QUAD_BLOB - QD_QUAD_BLOB5);
}

static void tags_test(void)
{
    static uint8_t b[QDS_MAXREC];
    int len;
    qd_sound_t f;
    G = &A;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    run(&A, 20);
    {
        int16_t bp[QD_QUAD_NP];
        qd_quad_preset_patch(3, bp);                    /* (BASS) */
        qd_lib_store(&A, 0, "TWO TAGS", bp);
    }
    CHECK(qd_tags_set(&A, QD_BR_MINE_CODE + 0, QD_PKB(QD_PK_BASS) | QD_PKB(QD_PK_FX)) &&
              qd_tags_set(&A, 0, QD_PKB(QD_PK_PAD) | QD_PKB(QD_PK_LEAD)) &&
              qd_tags_set(&A, QD_BR_POOL_CODE + 2, QD_PKB(QD_PK_DRUMS) | QD_PKB(QD_PK_KEYS)),
          "tags set on one of MY presets, on INIT (factory), on pool sound 2");
    qd_factory_get(&A, 0, &f);
    CHECK(f.tags == (QD_PKB(QD_PK_PAD) | QD_PKB(QD_PK_LEAD)) && A.per.ftags[0] == (0x80 | f.tags),
          "INIT's tags the owner's (PAD, LEAD), its sound as it was");
    qd_persist_flush(&A);
    boot(&B);
    qd_factory_get(&B, 0, &f);
    CHECK(LIB(&B, 0)->tags == (QD_PKB(QD_PK_BASS) | QD_PKB(QD_PK_FX)) &&
              f.tags == (QD_PKB(QD_PK_PAD) | QD_PKB(QD_PK_LEAD)) &&
              POOL(&B.proj, 2)->tags == (QD_PKB(QD_PK_DRUMS) | QD_PKB(QD_PK_KEYS)),
          "a restart: the tags kept (MY preset, the factory's, the pool's)");
    qd_factory_get(&B, 1, &f);
    CHECK(f.tags == (QD_PKB(QD_PK_DRUMS) | QD_PKB(QD_PK_KEYS)) && POOL(&B.proj, 2)->preset == 2,
          "P1.5w: pool sound 2 is a reference to EP: its tags are EP's");
    qd_factory_get(&B, 2, &f);
    CHECK(!B.per.ftags[2] && f.tags == QD_PKB(qd_tags_first(f.tags)), "another factory preset untouched: the "
          "classifier's");
    len = qd_store_get(&B.per.lib_st, QD_RKEY(QD_RK_LIB, 0, 0), b, sizeof b);   /* 0.7.1's record: 'S' 2, a kind */
    CHECK(len > 8 && b[0] == 'S' && b[1] == 5, "written as 'S' 5 (P1.5x: with its mix; E1: a version-6 blob)");
    len = snd_to_v5(b, len) - 4;                        /* (no mix: an 'S' 2) */
    b[1] = 2;
    b[len - 2] = QD_PK_PAD;
    qd_store_put(&B.per.lib_st, QD_RKEY(QD_RK_LIB, 0, 0), b, (uint32_t)len);
    qd_store_flush(&B.per.lib_st);
    boot(&A);
    CHECK(LIB(&A, 0)->tags == QD_PKB(QD_PK_PAD) && !strcmp(LIB(&A, 0)->name, "TWO TAGS"),
          "an 'S' 2 record (0.7.0 .. 0.7.1): its kind (PAD) its one tag");
}

/* P1.5w: THE POOL HOLDS REFERENCES. REPLACE plays in every project's pool and its locks; a track's own sound stays
 * its copy; a used preset's delete (its references, its locks, one undo level; another project drops its own as it
 * loads); an unused one's; a restart */
static int count_lib(const qd_app_t *a)
{
    int k, n = 0;
    for (k = 0; k < QD_NLIB; k++)
        n += LIB(a, k)->name[0] != 0;
    return n;
}
static int lock_n(const qd_app_t *a, int k)
{
    int p, t, s, n = 0;
    for (p = 0; p < QD_NPAT; p++)
        for (t = 0; t < QD_PAT_NTRACK; t++)
            for (s = 0; s < QD_NSTEP; s++)
                n += qd_proj_cpat(&a->proj, p)->step[t][s].sound == k && k;
    return n;
}
static void refs_test(void)
{
    int16_t p[QD_QUAD_NP];
    int id, k0, k1, np, ns, harm0;
    G = &A;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    run(&A, 20);
    qd_quad_preset_patch(5, p);
    p[QD_QP_HARM] = 100;
    qd_lib_store(&A, 0, "LEAD X", p);
    id = LIB(&A, 0)->preset;
    k0 = qd_pool_add_sound(&A, LIB(&A, 0));          /* project 1: a reference, two locks (two patterns) */
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 1, 2);
    qd_step_set_sound(qd_proj_pat(&A.proj, A.seq.cur), 1, 2, k0);
    qd_step_on(qd_proj_pat(&A.proj, 40), 3, 7);
    qd_step_set_sound(qd_proj_pat(&A.proj, 40), 3, 7, k0);
    CHECK(k0 > 0 && POOL(&A.proj, k0)->preset == id && POOL(&A.proj, k0)->patch[QD_QP_HARM] == 100,
          "+ POOL: a reference to LEAD X (its id), its sound the preset's");
    {   /* track 3: loaded from it (its own copy, not edited) */
        qd_track_t *t = &A.model.t[2];
        qd_track_load_sound(t, 0, "LEAD X", LIB(&A, 0)->patch);
        t->preset = (uint16_t)id;
        t->edited = 0;
        run(&A, 5);
        harm0 = t->patch[QD_QP_HARM];
    }
    CHECK(qd_persist_flush(&A), "flushed");
    qd_proj_new(&A, 1);                                 /* project 2: a reference too, a lock */
    k1 = qd_pool_add_sound(&A, LIB(&A, 0));
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 0, 5);
    qd_step_set_sound(qd_proj_pat(&A.proj, A.seq.cur), 0, 5, k1);
    run(&A, 5);
    p[QD_QP_HARM] = 1234;                               /* REPLACE (in project 2) */
    qd_lib_replace(&A, 0, "LEAD X", p);
    CHECK(POOL(&A.proj, k1)->patch[QD_QP_HARM] == 1234 && qd_sound_patch(&A.proj, k1)[QD_QP_HARM] == 1234,
          "REPLACE: project 2's reference (and its locked step) plays the new LEAD X");
    CHECK(qd_persist_flush(&A) && qd_proj_load(&A, 0), "flushed; project 1 loaded");
    qd_seq_select(&A.seq, QD_PAT_START, 1);              /* (a load opens on A01: back to A02, where the edits are) */
    CHECK(POOL(&A.proj, k0)->preset == id && POOL(&A.proj, k0)->patch[QD_QP_HARM] == 1234 &&
              qd_proj_pat(&A.proj, A.seq.cur)->step[1][2].sound == k0,
          "project 1's reference plays the new LEAD X too, its lock as it was");
    run(&A, 5);
    CHECK(A.model.t[2].patch[QD_QP_HARM] == harm0 && A.model.t[2].preset == id && !A.model.t[2].edited,
          "a track's own sound (loaded from it, not edited): its copy as it was (%d)", A.model.t[2].patch[QD_QP_HARM]);
    qd_preset_usage(&A, id, &np, &ns);
    CHECK(np == 2 && ns == 3, "its use: 2 projects, 3 steps (%d, %d)", np, ns);
    CHECK(qd_persist_flush(&A), "flushed");
    boot(&B);                                           /* a restart */
    G = &B;
    CHECK(B.per.proj == 0 && POOL(&B.proj, k0)->preset == id && POOL(&B.proj, k0)->patch[QD_QP_HARM] == 1234 &&
              !strcmp(POOL(&B.proj, k0)->name, "LEAD X"), "a restart: the reference, resolved");
    {
        uint8_t b[8];
        const int len = qd_store_get(&B.per.prj_st, QD_RKEY(QD_RK_SND, 0, k0), b, sizeof b);
        CHECK(len == 4 && b[0] == 'R' && b[1] == 1, "its record: 'R' 1, an id (%d bytes)", len);
    }
    qd_undo_reset(&B);                                  /* delete, used: the references and the locks; undo; redo */
    qd_undo_input(&B, 0);
    CHECK(qd_preset_delete(&B, 0) && qd_lib_find(&B, id) < 0 && !qd_sound_used(&B.proj, k0) && !lock_n(&B, k0) &&
              !qd_proj_pat(&B.proj, 40)->step[3][7].sound, "delete (used): the preset, its reference, its locks (both patterns)");
    qd_undo_close(&B);
    qd_undo_step(&B, -1);
    CHECK(qd_lib_find(&B, id) == 0 && POOL(&B.proj, k0)->preset == id && lock_n(&B, k0) == 2 &&
              (qd_proj_pat(&B.proj, 40)->step[3][7].ovr & QD_OV(QD_PM_SOUND)) && POOL(&B.proj, k0)->patch[QD_QP_HARM] == 1234,
          "undo: the preset, its reference, both locks (the other pattern's too) back");
    qd_undo_step(&B, 1);
    CHECK(qd_lib_find(&B, id) < 0 && !qd_sound_used(&B.proj, k0) && !lock_n(&B, k0), "redo: deleted again");
    CHECK(qd_persist_flush(&B) && qd_proj_load(&B, 1), "flushed; project 2 loaded");
    CHECK(!qd_sound_used(&B.proj, k1) && !lock_n(&B, k1) && !(qd_proj_pat(&B.proj, B.seq.cur)->step[0][5].ovr & QD_OV(QD_PM_SOUND)),
          "project 2 as it loads: its reference to the deleted preset dropped, its lock back to TRACK");
    qd_lib_store(&B, 5, "UNUSED", p);
    id = LIB(&B, 5)->preset;
    qd_preset_usage(&B, id, &np, &ns);
    CHECK(np == 0 && ns == 0 && qd_preset_delete(&B, 5) && !LIB(&B, 5)->name[0], "an unused one: no use, deleted");
    {
        int id2;
        qd_lib_store(&B, 6, "NEW ONE", p);
        id2 = LIB(&B, 6)->preset;
        CHECK(id2 > id, "a new MY preset: an id never used before (%d > %d)", id2, id);
    }
    CHECK(qd_persist_flush(&B), "flushed");
}

/* P1.5w: a 0.7.2 flash's pools (copies, 'S' 3) become references as their project loads: a copy of a factory sound
 * references it, one of a MY preset's references that, another becomes a new MY preset (its name made unique, its
 * tags kept); again: nothing new (the same references) */
static void put_copy(qd_app_t *a, int p, int k, const char *name, const int16_t *patch, int preset, int tags)
{
    uint8_t b[2 + QD_SOUND_NAME + QD_QUAD_BLOB5 + 4];
    uint32_t o = 2 + QD_SOUND_NAME + QD_QUAD_BLOB5;
    uint16_t pr = (uint16_t)preset;
    b[0] = 'S', b[1] = 3;
    memset(b + 2, 0, QD_SOUND_NAME);
    strcpy((char *)b + 2, name);
    qd_quad_pack5(patch, b + 2 + QD_SOUND_NAME);
    memcpy(b + o, &pr, 2);
    b[o + 2] = (uint8_t)tags, b[o + 3] = 0;
    qd_store_put(&a->per.prj_st, QD_RKEY(QD_RK_SND, p, k), b, o + 4);
    qd_store_flush(&a->per.prj_st);
}
static void migrate72_test(void)
{
    int16_t p[QD_QUAD_NP];
    qd_sound_t f;
    int nlib, m;
    G = &A;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    run(&A, 20);
    qd_quad_preset_patch(6, p);
    qd_lib_store(&A, 0, "MINE A", p);                   /* one of MY presets */
    qd_persist_flush(&A);
    qd_factory_get(&A, 3, &f);
    put_copy(&A, 0, 30, "MY BASS", f.patch, 0, 0);      /* a factory sound's copy (another name) */
    put_copy(&A, 0, 31, "A COPY", LIB(&A, 0)->patch, LIB(&A, 0)->preset, QD_PKB(QD_PK_PAD));   /* MINE A's */
    p[QD_QP_HARM] = 777;
    put_copy(&A, 0, 32, "MINE A", p, LIB(&A, 0)->preset, QD_PKB(QD_PK_PAD) | QD_PKB(QD_PK_FX));   /* an edited copy */
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 0, 0);
    qd_step_set_sound(qd_proj_pat(&A.proj, A.seq.cur), 0, 0, 32);
    qd_persist_flush(&A);
    nlib = count_lib(&A);
    boot(&B);
    G = &B;
    m = qd_lib_find(&B, POOL(&B.proj, 32)->preset);
    CHECK(POOL(&B.proj, 30)->preset == 4 && !strcmp(POOL(&B.proj, 30)->name, f.name), "a factory sound's copy: a reference "
          "to it (%s)", POOL(&B.proj, 30)->name);
    CHECK(POOL(&B.proj, 31)->preset == LIB(&B, 0)->preset, "a MY preset's copy: a reference to it");
    CHECK(m > 0 && !strcmp(LIB(&B, m)->name, "MINE A 2") && LIB(&B, m)->patch[QD_QP_HARM] == 777 &&
              LIB(&B, m)->tags == (QD_PKB(QD_PK_PAD) | QD_PKB(QD_PK_FX)) && count_lib(&B) == nlib + 1 &&
              qd_proj_pat(&B.proj, B.seq.cur)->step[0][0].sound == 32,
          "another sound: a new MY preset (%s, its tags kept), referenced; its lock as it was", m > 0 ? LIB(&B, m)->name : "-");
    {
        int n, k;
        for (n = 0, k = 1; k <= QD_NFACTORY; k++)
            n += POOL(&B.proj, k)->preset == k;
        CHECK(n == QD_NFACTORY, "the demo's starting pool: references to the factory presets (%d)", n);
    }
    qd_persist_flush(&B);
    boot(&A);
    G = &A;
    CHECK(count_lib(&A) == nlib + 1 && POOL(&A.proj, 32)->preset == POOL(&B.proj, 32)->preset &&
              POOL(&A.proj, 30)->preset == 4, "again: nothing new, the same references");
    {
        uint8_t b[8];
        const int len = qd_store_get(&A.per.prj_st, QD_RKEY(QD_RK_SND, 0, 32), b, sizeof b);
        CHECK(len == 4 && b[0] == 'R', "written as references");
    }
}

/* P1.5x: a MY preset's mix kept across a restart; an older record ('S' 3): no sends, pan centre */
static void mix_test(void)
{
    static uint8_t b[QDS_MAXREC];
    int16_t p[QD_QUAD_NP];
    int len;
    G = &A;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    run(&A, 20);
    qd_quad_preset_patch(2, p);
    qd_lib_store(&A, 3, "MIXED", p);
    {
        qd_sound_t m = *LIB(&A, 3);
        m.pan = -25, m.chorus = 9, m.delay = 19, m.reverb = 29;
        qd_lib_write(&A, 3, &m);
    }
    qd_persist_flush(&A);
    boot(&B);
    CHECK(LIB(&B, 3)->pan == -25 && LIB(&B, 3)->chorus == 9 && LIB(&B, 3)->delay == 19 && LIB(&B, 3)->reverb == 29,
          "P1.5x: a restart: a MY preset's pan and sends kept");
    len = qd_store_get(&B.per.lib_st, QD_RKEY(QD_RK_LIB, 0, 3), b, sizeof b);
    len = snd_to_v5(b, len);
    b[1] = 3;
    qd_store_put(&B.per.lib_st, QD_RKEY(QD_RK_LIB, 0, 3), b, (uint32_t)len - 4u);   /* (0.7.3's: no mix) */
    qd_store_flush(&B.per.lib_st);
    boot(&A);
    CHECK(!strcmp(LIB(&A, 3)->name, "MIXED") && !LIB(&A, 3)->pan && !LIB(&A, 3)->chorus && !LIB(&A, 3)->delay &&
              !LIB(&A, 3)->reverb, "an 'S' 3 record (0.7.3 and older): no sends, pan centre");
}

/* P1.6d: the tracks' kinds and names, AUTOMATION, kept across a restart; a 0.9.0 header (none of them): the default
 * kit's names and kinds, AUTOMATION ON, the pool byte still read */
/* 2026-10-24, six tracks: a store an eight-track FIZZ left (its settings record 'G' 1) with MY PRESETS and a project
 * of its own. The six-track build erases the projects' store at the mount (a fresh start: the demo project) and keeps
 * the library's (MY PRESETS, their ids) as it was */
static void old_format_test(void)
{
    static uint8_t b[QDS_MAXREC];
    int16_t p[QD_QUAD_NP];
    int len, id, ok_lib;
    G = &A;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    run(&A, 20);
    qd_quad_preset_patch(3, p);
    p[QD_QP_HARM] = 321;
    qd_lib_store(&A, 7, "OLD MINE", p);
    id = LIB(&A, 7)->preset;
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 2, 9);  /* the project's own content */
    strcpy(A.per.pname[0], "OLD SONG");
    run(&A, 20);
    CHECK(qd_persist_flush(&A), "flushed");
    len = qd_store_get(&A.per.prj_st, QD_RKEY(QD_RK_SET, 0, 0), b, sizeof b);
    b[1] = 1;                                           /* (the settings as 0.11 wrote them: 'G' 1, eight tracks) */
    qd_store_put(&A.per.prj_st, QD_RKEY(QD_RK_SET, 0, 0), b, (uint32_t)len);
    qd_store_flush(&A.per.prj_st);
    boot(&B);
    ok_lib = !strcmp(LIB(&B, 7)->name, "OLD MINE") && LIB(&B, 7)->preset == id && LIB(&B, 7)->patch[QD_QP_HARM] == 321;
    CHECK(len > 2 && B.per.on && B.per.prj_wiped == 1 && ok_lib,
          "an eight-track store ('G' 1): the projects' store erased once at the mount, MY PRESETS kept (%s, id %d)",
          LIB(&B, 7)->name, LIB(&B, 7)->preset);
    CHECK(!qd_step_is_on(qd_step(qd_proj_cpat(&B.proj, B.seq.cur), 2, 9)) && strcmp(B.per.pname[0], "OLD SONG") &&
              B.per.pexists[0] && !B.per.pexists[1] && B.per.proj == 0,
          "its projects fresh: the first project, a blank flash's (%s)", B.per.pname[0]);
    run(&B, 20);
    qd_persist_flush(&B);
    boot(&A);
    CHECK(A.per.prj_wiped == 0 && !strcmp(LIB(&A, 7)->name, "OLD MINE"), "the restart after it: nothing erased again, "
          "MY PRESETS still there");
    printf("  old format: an eight-track store's projects erased at the mount, MY PRESETS kept\n");
}

static void names_test(void)
{
    static uint8_t b[QDS_MAXREC];
    const uint32_t base = 2u + QD_PROJ_NAME + 7u + 7u * QD_NTRACK;
    int len;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    run(&A, 20);
    CHECK(!strcmp(A.model.t[0].name, "DRUMS") && A.model.t[0].kind == QD_PK_DRUMS && !strcmp(A.model.t[4].name, "PLUCK") &&
              A.model.t[4].kind == QD_PK_PLUCK && A.model.t[5].kind == QD_PK_PAD && !A.model.t[4].named && A.proj.rec_auto,
          "the default kit: DRUMS .. PAD, their kinds (PLUCK PLUCK, PAD PAD), none typed; AUTOMATION ON");
    qd_track_set_kind(&A.model.t[4], QD_PK_PAD);
    CHECK(!strcmp(A.model.t[4].name, "PAD") && A.model.t[4].kind == QD_PK_PAD, "a kind chosen: the name its own (PAD)");
    qd_track_set_name(&A.model.t[2], "GLASSY KEYS!");
    qd_track_set_kind(&A.model.t[2], QD_PK_LEAD);
    CHECK(!strcmp(A.model.t[2].name, "GLASSY KEYS!") && A.model.t[2].named && A.model.t[2].kind == QD_PK_LEAD,
          "a name typed (twelve characters): a kind chosen keeps it");
    qd_track_set_name(&A.model.t[2], "LEAD");
    CHECK(!A.model.t[2].named, "the kind's own name typed: not typed (a kind chosen renames it again)");
    qd_track_set_name(&A.model.t[2], "GLASSY KEYS!");
    A.proj.rec_auto = 0;
    run(&A, 20);
    CHECK(qd_persist_flush(&A), "flushed");
    boot(&B);
    CHECK(!strcmp(B.model.t[2].name, "GLASSY KEYS!") && B.model.t[2].named && B.model.t[2].kind == QD_PK_LEAD &&
              !strcmp(B.model.t[4].name, "PAD") && B.model.t[4].kind == QD_PK_PAD && !B.model.t[4].named &&
              !strcmp(B.model.t[1].name, "BASS") && !B.proj.rec_auto,
          "a restart: the names, the kinds, typed or not, AUTOMATION OFF");
    len = qd_store_get(&B.per.prj_st, QD_RKEY(QD_RK_HDR, 0, 0), b, sizeof b);
    CHECK(len == (int)(base + 3u + QD_NTRACK * (QD_TRACK_NAME + 1)), "the header: %d bytes", len);
    qd_store_put(&B.per.prj_st, QD_RKEY(QD_RK_HDR, 0, 0), b, base + 2u);   /* (0.9.0's) */
    qd_store_flush(&B.per.prj_st);
    boot(&A);
    CHECK(!strcmp(A.model.t[2].name, "KEYS") && A.model.t[2].kind == QD_PK_KEYS && !A.model.t[2].named &&
              !strcmp(A.model.t[4].name, "PLUCK") && A.proj.rec_auto,
          "a 0.9.0 header: the names it had (the default kit's), their kinds; AUTOMATION ON");
    {   /* P1.6g: a new project: its tracks named from their sounds (the kind from the preset's tags), kept */
        static const char *const KIT[QD_NTRACK] = {"DRUMS", "BASS", "KEYS", "LEAD", "PLUCK", "PAD"};
        int t, ok = 1, kit = 0;
        run(&A, 300);
        qd_proj_new(&A, 2);
        run(&A, 20);
        for (t = 0; t < QD_NTRACK; t++) {
            qd_sound_t f;
            const int pr = qd_proj_pat(&A.proj, A.seq.cur)->snd[t].preset;
            if (pr >= 1 && pr <= QD_NFACTORY)
                qd_factory_preset(pr - 1, &f), ok &= A.model.t[t].kind == qd_tags_first(f.tags);
            ok &= A.model.t[t].name[0] != 0 && !A.model.t[t].named;
            kit += !strcmp(A.model.t[t].name, KIT[t]);
        }
        CHECK(ok && kit < QD_NTRACK, "a new project: each track's kind from its preset's tags, its name from its sound (%s, %s, "
              "%s ..)", A.model.t[0].name, A.model.t[1].name, A.model.t[2].name);
        qd_persist_flush(&A);
        {
            char n0[QD_TRACK_NAME];
            strcpy(n0, A.model.t[0].name);
            boot(&B);
            CHECK(B.per.proj == 2 && !strcmp(B.model.t[0].name, n0), "kept across a restart (%s)", B.model.t[0].name);
        }
    }
}

/* P2.1: the patterns paged (at the device's sizes QD_PAT_RAM of them in RAM, the rest in the store; the host: all in
 * RAM, the same answers): every one of the 64 given its own step and lock through qd_proj_pat (each evicting others,
 * written back), then read back in another order; after a restart from the flash the same; with no flash attached
 * (the host's RAM store) the same; a pattern change while playing: the sequencer never left without its patterns */
static int pat_sig_ok(qd_project_t *pr, int k)
{
    const qd_pattern_t *P = qd_proj_cpat(pr, k);
    int16_t v;
    return qd_step_is_on(qd_step(P, k % QD_PAT_NTRACK, k)) && qd_lock_get(P, k % QD_PAT_NTRACK, k, QD_QP_HARM, &v) && v == 100 + k &&
           P->used;
}
static void run_audio(qd_app_t *a, uint32_t ms)       /* (ticks and the sound: the sequencer runs on it) */
{
    static int16_t buf[2 * 128];
    uint32_t i;
    for (i = 0; i < ms; i++) {
        qd_app_tick(a, ++now);
        qd_app_audio(a, buf, i % 10 == 9 ? 45 : 44);
    }
}
static void paging_test(void)
{
    int k, ok, res;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    for (k = 0; k < QD_NPAT; k++) {
        qd_pattern_t *P = qd_proj_pat(&A.proj, k);
        qd_step_on(P, k % QD_PAT_NTRACK, k);
        qd_lock_set(P, k % QD_PAT_NTRACK, k, QD_QP_HARM, 100 + k);
    }
    for (ok = 0, k = QD_NPAT - 1; k >= 0; k -= 2)
        ok += pat_sig_ok(&A.proj, k);
    for (k = 0; k < QD_NPAT; k += 2)
        ok += pat_sig_ok(&A.proj, k);
    for (res = 0, k = 0; k < QD_NPAT; k++)
        res += qd_proj_resident(&A.proj, k);
    CHECK(ok == QD_NPAT && res == (QD_PAT_RAM < QD_NPAT ? QD_PAT_RAM : QD_NPAT),
          "P2.1: 64 patterns edited one after another, read back in another order: all there (%d), %d in RAM", ok, res);
    qd_persist_flush(&A);
    boot(&B);
    for (ok = 0, k = 0; k < QD_NPAT; k++)
        ok += pat_sig_ok(&B.proj, k);
    CHECK(ok == QD_NPAT, "P2.1: .. after a restart from the flash: all there (%d)", ok);
    qd_app_init(&B);                                    /* (no flash: the host's RAM store at the device's sizes) */
    for (k = 0; k < QD_NPAT; k++) {
        qd_pattern_t *P = qd_proj_pat(&B.proj, k);
        qd_step_on(P, k % QD_PAT_NTRACK, k);
        qd_lock_set(P, k % QD_PAT_NTRACK, k, QD_QP_HARM, 100 + k);
    }
    for (ok = 0, k = QD_NPAT - 1; k >= 0; k--)
        ok += pat_sig_ok(&B.proj, k);
    CHECK(ok == QD_NPAT && qd_proj_used(&B.proj, 1) && qd_step_is_on(qd_step(qd_proj_cpat(&B.proj, 0), 0, 0)),
          "P2.1: no flash attached: the same (%d); A01..A03 the demos still under the edits", ok);
    boot(&A);                                           /* a change while playing: A01 queued over A02, then A05 */
    qd_app_play(&A);
    run_audio(&A, 500);
    qd_seq_select(&A.seq, 0, 1);                        /* (at the next 16th) */
    run_audio(&A, 3000);
    qd_seq_select(&A.seq, 4, 1);
    run_audio(&A, 3000);
    CHECK(A.seq.cur == 4 && A.seq.playing && !A.proj.pat_forced, "P2.1: two changes while playing: A05 playing (%d), never every slot pinned "
          "(forced %u, loads %u, saves %u)", A.seq.cur, (unsigned)A.proj.pat_forced, (unsigned)A.proj.pat_loads,
          (unsigned)A.proj.pat_saves);
}

/* P2.1: the pool's and MY presets' sounds paged (at the device's sizes QD_SND_RAM of them in RAM, the rest in the
 * store): 200 presets stored, 100 pool slots referencing them, tags set, one replaced (its references follow), one
 * deleted with its references; read back in another order: each as it was; a restart from the flash: the same */
static void sound_paging_test(void)
{
    static qd_sound_t want_lib[QD_NLIB], want_pool[QD_NSOUND];
    int k, ok, n0;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    for (k = 0; k < 200; k++) {
        int16_t p[QD_QUAD_NP];
        char nm[QD_SOUND_NAME];
        qd_quad_preset_patch(k % 17, p);
        p[QD_QP_HARM] = (int16_t)(10 + k);
        snprintf(nm, sizeof nm, "PG %03d", k);
        qd_lib_store(&A, k, nm, p);
    }
    n0 = A.proj.nsound;
    for (k = 0; k < 100; k++) {
        const qd_sound_t s = *LIB(&A, 199 - k);
        qd_pool_add_sound(&A, &s);
    }
    for (k = 0; k < 200; k += 7)
        qd_tags_set(&A, QD_BR_MINE_CODE + k, QD_PKB(k % QD_NPK) | QD_PKB(QD_PK_FX));
    {
        int16_t p[QD_QUAD_NP];
        qd_quad_preset_patch(9, p);
        p[QD_QP_HARM] = 1234;
        qd_lib_replace(&A, 190, "PG REPL", p);          /* (pool slot n0 + 10 references it) */
    }
    qd_preset_delete(&A, 180);                          /* (.. and n0 + 20 this one: gone with it) */
    qd_undo_close(&A);
    for (k = 0; k < QD_NLIB; k++)
        want_lib[k] = *LIB(&A, k);
    for (k = 1; k <= QD_NSOUND; k++)
        want_pool[k - 1] = *POOL(&A.proj, k);
    for (ok = 0, k = QD_NSOUND; k >= 1; k--)
        ok += !memcmp(POOL(&A.proj, k), &want_pool[k - 1], sizeof want_pool[0]);
    for (k = QD_NLIB - 1; k >= 0; k--)
        ok += !memcmp(LIB(&A, k), &want_lib[k], sizeof want_lib[0]);
    CHECK(ok == QD_NSOUND + QD_NLIB && n0 > 0 && POOL(&A.proj, n0 + 10)->patch[QD_QP_HARM] == 1234 &&
              !strcmp(POOL(&A.proj, n0 + 10)->name, "PG REPL") && !qd_sound_used(&A.proj, n0 + 20) &&
              !qd_lib_used(&A, 180) && LIB(&A, 7)->tags == (QD_PKB(7 % QD_NPK) | QD_PKB(QD_PK_FX)),
          "P2.1: 200 presets, 100 pool references, tags, a REPLACE, a delete: read back in another order, each as it "
          "was (%d of %d)", ok, QD_NSOUND + QD_NLIB);
#if QD_SND_RAM
    CHECK(A.per.sc.loads > 0 && A.per.sc.saves > 0, "P2.1: %d of them in RAM: read in %u times, given up %u times",
          QD_SND_RAM, (unsigned)A.per.sc.loads, (unsigned)A.per.sc.saves);
#endif
    qd_persist_flush(&A);
    boot(&B);
    for (ok = 0, k = 1; k <= QD_NSOUND; k++)
        ok += !memcmp(POOL(&B.proj, k), &want_pool[k - 1], sizeof want_pool[0]);
    for (k = 0; k < QD_NLIB; k++)
        ok += !memcmp(LIB(&B, k), &want_lib[k], sizeof want_lib[0]);
    CHECK(ok == QD_NSOUND + QD_NLIB && B.proj.nsound == A.proj.nsound,
          "P2.1: a restart from the flash: every pool sound and MY preset the same (%d of %d)", ok, QD_NSOUND + QD_NLIB);
}

/* P2.4: THE AUDIO PATH NEVER PAGES, AND NOTHING IS WRITTEN BUT IN qd_persist_step. The unit's way, simulated: the
 * audio interrupt (qd_app_audio, 44.1 frames a ms) runs every ms; the main loop (qd_app_tick, then the port's own
 * qd_persist_step every 15 ms) stalls for 400 ms at the worst moments, as a slow frame does. The patterns' changes
 * (a queued one, a chain) need patterns that are not in RAM, the playing one is edited (its slot cannot be given up
 * until its record is written), and the next patterns lock pool sounds. Checked: no flash access at all from the
 * audio side, no write from a tick (the prefetch included), the changes held while their pattern is out (counted) and
 * taken once it is in, no miss, the edit kept across a restart */
static int io_audio, io_tick;
static uint32_t n_audio_io, n_tick_writes, n_step_writes;
static int c_read(void *c, uint32_t off, void *d, uint32_t n)
{
    n_audio_io += io_audio != 0;
    return f_read(c, off, d, n);
}
static int c_erase(void *c, uint32_t off)
{
    n_audio_io += io_audio != 0, n_tick_writes += io_tick != 0, n_step_writes += !io_audio && !io_tick;
    return f_erase(c, off);
}
static int c_prog(void *c, uint32_t off, const void *src, uint32_t n)
{
    n_audio_io += io_audio != 0, n_tick_writes += io_tick != 0, n_step_writes += !io_audio && !io_tick;
    return f_prog(c, off, src, n);
}
static const qd_flash_t FLASH_C = {c_read, c_erase, c_prog, 0};
static void isr_ms(qd_app_t *a, uint32_t ms, int main_loop)  /* ms of the unit: the interrupt's sound; the main loop's
                                                         * tick and step (main_loop 0: stalled) */
{
    static int16_t buf[2 * 64];
    static uint32_t frac, t_main;
    while (ms--) {
        frac += 441u;
        io_audio = 1;
        qd_app_audio(a, buf, frac / 10u);
        io_audio = 0;
        frac %= 10u;
        now++;
        if (!main_loop)
            continue;
        io_tick = 1;
        qd_app_tick(a, now);                            /* (a stall's ms caught up at once, as the unit's loop does) */
        io_tick = 0;
        if (now - t_main >= 15u) {
            t_main = now;
            qd_persist_step(a);
        }
    }
}
static void isr_paging_test(void)
{
    static const int chain[3] = {4, 5, 0};
    int k, edit_ok;
    uint32_t late0;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    for (k = 0; k < 8; k++) {                           /* A01..A08: content; A05, A06 lock pool sounds */
        qd_pattern_t *P = qd_proj_pat(&A.proj, k);
        int s;
        for (s = 0; s < 16; s += 2) {
            qd_step_on(P, s % 4, s);
            if (k == 4 || k == 5)
                qd_step_set_sound(P, s % 4, s, 17 + (s / 2 + k) % 8);
        }
        P->used = 1;
    }
    qd_persist_flush(&A);
    boot(&A);
    qd_app_init(&A);                                    /* (the counting flash) */
    qd_app_tick(&A, 0);
    qd_persist_attach(&A, &FLASH_C);
    qd_persist_own_step(&A, 1);
    now = 0;
    n_audio_io = n_tick_writes = n_step_writes = 0;
    qd_seq_select(&A.seq, 1, 1);
    qd_app_play(&A);
    isr_ms(&A, 600, 1);
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 5, 5);  /* the playing one edited (the UI: main loop) .. */
    qd_step_on(qd_proj_pat(&A.proj, 2), 4, 9);          /* .. and A03 (the other slot: neither can be given up yet) */
    late0 = A.seq.sw_late;
    qd_seq_select(&A.seq, 3, 1);                        /* A04 at the next 16th .. */
    isr_ms(&A, 400, 0);                                 /* .. the main loop stalled: no prefetch */
    CHECK(QD_PAT_RAM >= QD_NPAT ? A.seq.cur == 3 : A.seq.cur == 1 && A.seq.sw_late > late0,   /* (the host: all in
                                                         * RAM, taken at once) */
          "P2.4: a change whose pattern is not in RAM, the main loop "
          "stalled: held, the playing one goes on (playing A%02d, held %u times)", A.seq.cur + 1,
          (unsigned)(A.seq.sw_late - late0));
    isr_ms(&A, 3000, 1);
    CHECK(A.seq.cur == 3, "P2.4: .. the main loop back: its pattern read in, the change taken (A%02d)", A.seq.cur + 1);
    qd_seq_chain(&A.seq, chain, 3);                     /* A05 A06 A01: sound locks, the edited A02 to give up */
    for (k = 0; k < 6; k++) {
        isr_ms(&A, 1500, 1);
        isr_ms(&A, 400, 0);
    }
    qd_seq_select(&A.seq, 1, 1);                        /* back to the edited A02 (given up while the chain played) */
    isr_ms(&A, 3000, 1);
    CHECK(A.seq.cur == 1, "P2.4: the chain played, then A02 again (A%02d)", A.seq.cur + 1);
    CHECK(n_audio_io == 0, "P2.4: the audio side touched the flash %u times (patterns read in %u, misses %u, sound "
          "misses %u)", (unsigned)n_audio_io, (unsigned)A.proj.pat_loads, (unsigned)A.proj.pat_miss,
          (unsigned)A.proj.snd_miss);
    CHECK(n_tick_writes == 0 && n_step_writes > 0, "P2.4: no flash write from a tick or its prefetch (%u), all from "
          "qd_persist_step (%u)", (unsigned)n_tick_writes, (unsigned)n_step_writes);
    CHECK(A.proj.pat_miss == 0 && !A.proj.pat_forced, "P2.4: no pattern missed on the audio side (%u), no pinned one "
          "given up (%u)", (unsigned)A.proj.pat_miss, (unsigned)A.proj.pat_forced);
    printf("  P2.4 paging: %u changes held for their pattern, %u sound locks played with the track's sound, %u "
           "patterns read in, %u flash writes, all in qd_persist_step\n", (unsigned)A.seq.sw_late,
           (unsigned)A.proj.snd_miss, (unsigned)A.proj.pat_loads, (unsigned)n_step_writes);
    qd_app_stop(&A);
    qd_persist_flush(&A);
    boot(&B);
    edit_ok = qd_step_is_on(qd_step(qd_proj_cpat(&B.proj, 1), 5, 5)) && qd_step_is_on(qd_step(qd_proj_cpat(&B.proj,
                                                                                                         2), 4, 9));
    CHECK(edit_ok, "P2.4: the edits to the playing A02 and to A03 (given up for A04) kept across a restart");
}

/* P2.4: A FOREIGN DATA REGION. The unit's region as another firmware left it (ChoralRoot's, RemoteFM1's), or worse:
 * random bytes, all zeros, FIZZ's sector magic with a broken check or with garbage pages, a half-erased region, text.
 * The mount reads and erases nothing (a call that returns soon: the watchdog; attach erases one sector at most, for
 * the first project's header); nothing of it gets into the model (the
 * same project, patterns and pool as a blank flash's); the region is erased in the background, one sector a quiet
 * step (no call erases two); then saving works and a restart reads the same */
static uint32_t fr_erases;
static int fr_erase(void *c, uint32_t off) { fr_erases++; return f_erase(c, off); }
static const qd_flash_t FLASH_F = {f_read, fr_erase, f_prog, 0};
static uint64_t model_sig(qd_app_t *a)
{
    uint64_t h = 1469598103934665603ull;
    int k;
#define SIG(p, n) do { const uint8_t *b_ = (const uint8_t *)(p); size_t i_; for (i_ = 0; i_ < (size_t)(n); i_++) h = (h ^ b_[i_]) * 1099511628211ull; } while (0)
    SIG(a->per.pname[0], QD_PROJ_NAME);
    SIG(&a->seq.cur, sizeof a->seq.cur);
    SIG(&a->proj.nsound, sizeof a->proj.nsound);
    for (k = 0; k < 8; k++)
        SIG(qd_proj_cpat(&a->proj, k), sizeof(qd_pattern_t));
    for (k = 1; k <= QD_NSOUND; k++)
        SIG(POOL(&a->proj, k), sizeof(qd_sound_t));
    for (k = 0; k < QD_NLIB; k++)
        h = (h ^ (uint64_t)qd_lib_used(a, k)) * 1099511628211ull;
    for (k = 0; k < QD_NTRACK; k++)
        SIG(a->model.t[k].patch, sizeof a->model.t[k].patch);
#undef SIG
    return h;
}
static void foreign_test(void)
{
    static const char *const KIND[] = {"random bytes", "all zeros", "FIZZ's magic, a broken check",
                                       "FIZZ's magic, garbage pages", "half-erased", "another firmware's text"};
    const uint32_t span = QD_FLASH_PRJ + QD_FLASH_PRJ_SEC * 4096u;
    uint64_t want;
    int kind, ok_model = 0, ok_mount = 0, ok_steps = 0, ok_done = 0, ok_again = 0;
    uint32_t max_step = 0, i, r = 99u;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    run(&A, 100);
    want = model_sig(&A);
    for (kind = 0; kind < (int)(sizeof KIND / sizeof KIND[0]); kind++) {
        uint32_t e0, k, ms;
        memset(FL, 0xFF, sizeof FL);
        for (i = 0; i < span; i++) {
            r ^= r << 13, r ^= r >> 17, r ^= r << 5;
            switch (kind) {
            case 0: FL[i] = (uint8_t)r; break;
            case 1: FL[i] = 0; break;
            case 2: FL[i] = i % 4096u < 12u ? "Z2FS\x05\0\0\0\x5A\x5A\x5A\x5A"[i % 4096u] : (uint8_t)r; break;
            case 3: FL[i] = i % 4096u < 12u ? "Z2FS\x05\0\0\0\x5A\x5A\xA5\xA5"[i % 4096u] : (uint8_t)r; break;
            case 4: FL[i] = (r & 0xFFu) < 3u ? (uint8_t)(r >> 8) : 0xFF; break;
            default: FL[i] = (uint8_t)"{\"choralroot\":{\"presets\":[1,2,3],\"name\":\"PAD\"}}\n"[i % 49u]; break;
            }
        }
        qd_app_init(&A);
        qd_app_tick(&A, 0);
        fr_erases = 0;
        qd_persist_attach(&A, &FLASH_F);
        now = 0;
        ok_mount += fr_erases <= 1u && A.per.on;        /* (one: the first project's header needs a sector) */
        qd_persist_own_step(&A, 1);
        ok_model += model_sig(&A) == want;
        for (ms = 0; ms < 14000u; ms++) {               /* (quiet after 10 s untouched: one sector a step) */
            qd_app_tick(&A, ++now);
            if (now % 15u == 0) {
                e0 = fr_erases;
                qd_persist_step(&A);
                k = fr_erases - e0;
                max_step = k > max_step ? k : max_step;
            }
        }
        ok_steps += max_step <= 1u;
        ok_done += !qd_store_pending(&A.per.prj_st) && !qd_store_pending(&A.per.lib_st) && !qd_persist_dirty(&A);
        qd_persist_flush(&A);
        boot(&B);
        ok_again += model_sig(&B) == want;
        if (model_sig(&B) != want || model_sig(&A) != want)
            printf("  (%s: the model differs)\n", KIND[kind]);
    }
    CHECK(ok_mount == kind, "P2.4: a foreign region (%d kinds): attached with one erase at most, the store on (%d)", kind,
          ok_mount);
    CHECK(ok_model == kind, "P2.4: .. nothing of it in the model: a blank flash's project, patterns, pool (%d)",
          ok_model);
    CHECK(ok_steps == kind && ok_done == kind, "P2.4: .. erased in the background, never two sectors a call (at most "
          "%u), all of it within 14 s (quiet from 10 s), the project written (%d)", (unsigned)max_step, ok_done);
    CHECK(ok_again == kind, "P2.4: .. a restart reads the same (%d)", ok_again);
}

/* P2.1: 'P' 5, TRACK SOUNDS BY REFERENCE: a track sound that is its preset (not edited) is written as the preset's id;
 * read back the same, exactly; one edited stays a copy. A track keeps the sound it loaded: REPLACE, an undo and a
 * redo of it, a delete leave every pattern's track sound as it was, in this project and another, in RAM or not */
static uint16_t rec_ref(qd_app_t *a, int p, int k, int t)   /* (a pattern record's reference t; 0xFFFF: not 'P' 5) */
{
    uint8_t h[12 + 2 * QD_PAT_NTRACK];
    uint16_t r;
    const int len = qd_store_get(&a->per.prj_st, QD_RKEY(QD_RK_PAT, p, k), h, sizeof h);
    if (len < (int)sizeof h || h[0] != 'P' || h[1] != 5)
        return 0xFFFFu;
    memcpy(&r, h + 12 + 2 * t, 2);
    return r;
}
static void tsound_of(qd_tsound_t *ts, const qd_sound_t *s, int edited)
{
    memset(ts, 0, sizeof *ts);
    memcpy(ts->name, s->name, QD_SOUND_NAME);
    memcpy(ts->patch, s->patch, sizeof ts->patch);
    ts->preset = s->preset;
    ts->edited = (uint8_t)edited;
}
static void pref_test(void)
{
    static qd_tsound_t want[3];
    int16_t p[QD_QUAD_NP];
    qd_sound_t f;
    int id, id2, ok, t, len4, len5, nref;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    run(&A, 20);
    qd_quad_preset_patch(5, p);
    p[QD_QP_HARM] = 321;
    qd_lib_store(&A, 3, "REF ME", p);
    p[QD_QP_HARM] = 654;
    qd_lib_store(&A, 4, "REF TWO", p);
    id = LIB(&A, 3)->preset, id2 = LIB(&A, 4)->preset;
    qd_proj_new(&A, 1);                                 /* project 2, pattern 1: MY preset REF ME on track 1 */
    tsound_of(&qd_proj_pat(&A.proj, 0)->snd[0], LIB(&A, 3), 0);
    qd_proj_pat(&A.proj, 0)->used = 1;
    qd_proj_load(&A, 0);                                /* project 1, pattern 11: REF ME, EP, an edited one, REF TWO */
    tsound_of(&qd_proj_pat(&A.proj, 10)->snd[0], LIB(&A, 3), 0);
    qd_factory_preset(1, &f);
    tsound_of(&qd_proj_pat(&A.proj, 10)->snd[1], &f, 0);
    qd_factory_preset(2, &f);
    tsound_of(&qd_proj_pat(&A.proj, 10)->snd[2], &f, 1);
    tsound_of(&qd_proj_pat(&A.proj, 11)->snd[0], LIB(&A, 4), 0);
    qd_proj_pat(&A.proj, 10)->used = qd_proj_pat(&A.proj, 11)->used = 1;
    for (t = 0; t < 3; t++)
        want[t] = qd_proj_cpat(&A.proj, 10)->snd[t];
    qd_persist_write_v4 = 1;                            /* (the same pattern as 'P' 4: its length) */
    qd_persist_flush(&A);
    len4 = qd_store_get(&A.per.prj_st, QD_RKEY(QD_RK_PAT, 0, 10), 0, 0);
    qd_persist_write_v4 = 0;
    qd_proj_pat(&A.proj, 10)->swing = 3;                /* (a change: written again, as 'P' 5) */
    qd_persist_flush(&A);
    len5 = qd_store_get(&A.per.prj_st, QD_RKEY(QD_RK_PAT, 0, 10), 0, 0);
    for (nref = 0, t = 0; t < QD_PAT_NTRACK; t++)
        nref += rec_ref(&A, 0, 10, t) != 0;
    CHECK(rec_ref(&A, 0, 10, 0) == id && rec_ref(&A, 0, 10, 1) == 2 && !rec_ref(&A, 0, 10, 2) &&
              rec_ref(&A, 0, 11, 0) == id2 && rec_ref(&A, 1, 0, 0) == id && len5 == len4 + 2 * QD_PAT_NTRACK - nref * 143,
          "P2.1: 'P' 5: MY preset and EP by reference (and the default kit's: %d of 6), the edited one a copy; in the "
          "other project too; %d bytes, not %d", nref, len5, len4);
    boot(&B);
    for (ok = 0, t = 0; t < 3; t++)
        ok += !memcmp(&qd_proj_cpat(&B.proj, 10)->snd[t], &want[t], sizeof want[t]);
    CHECK(ok == 3, "P2.1: a restart: the three track sounds exactly as they were (%d)", ok);
    boot(&A);                                           /* (A again: B may have written) */
    p[QD_QP_HARM] = 999;
    qd_lib_replace(&A, 3, "REF ME", p);                /* REPLACE: the references to it copies first */
    qd_undo_close(&A);
    CHECK(rec_ref(&A, 0, 10, 0) == 0 && rec_ref(&A, 1, 0, 0) == 0 && rec_ref(&A, 0, 10, 1) == 2,
          "P2.1: REPLACE: the records referring to it written again as copies, in both projects");
    qd_undo_step(&A, -1);                               /* UNDO the REPLACE, a write, REDO */
    qd_proj_pat(&A.proj, 10)->swing = 4;                /* (pattern 11 written again: by reference again) */
    qd_persist_flush(&A);
    CHECK(LIB(&A, 3)->patch[QD_QP_HARM] == 321 && rec_ref(&A, 0, 10, 0) == id,
          "P2.1: UNDO: the preset as it was, the track a reference to it again");
    qd_undo_step(&A, 1);
    qd_persist_flush(&A);
    boot(&B);
    CHECK(LIB(&B, 3)->patch[QD_QP_HARM] == 999 && qd_proj_cpat(&B.proj, 10)->snd[0].patch[QD_QP_HARM] == 321 &&
              !memcmp(&qd_proj_cpat(&B.proj, 10)->snd[0], &want[0], sizeof want[0]),
          "P2.1: REDO, a restart: the preset the new one (999), the track keeps the sound it loaded (321)");
    qd_proj_load(&B, 1);
    CHECK(qd_proj_cpat(&B.proj, 0)->snd[0].patch[QD_QP_HARM] == 321, "P2.1: .. in the other project too");
    boot(&A);
    qd_preset_delete(&A, 4);                            /* a delete: its references copies first */
    qd_undo_close(&A);
    qd_persist_flush(&A);
    boot(&B);
    CHECK(!qd_lib_used(&B, 4) && qd_proj_cpat(&B.proj, 11)->snd[0].patch[QD_QP_HARM] == 654 &&
              !strcmp(qd_proj_cpat(&B.proj, 11)->snd[0].name, "REF TWO") &&
              qd_proj_cpat(&B.proj, 11)->snd[0].preset == id2,
          "P2.1: a delete: the track keeps its sound, its name and where it came from");
}

/* the owner's rule (2026-10-25): a pattern's track holds its own version of a preset. P on track 1 of A01 and A02; in
 * A02 edited and saved over P (REPLACE): A01's track 1 keeps the old values, in RAM and after a restart; the pool's
 * reference to P plays the new one */
static void replace_rule_test(void)
{
    int16_t p[QD_QUAD_NP];
    int k, slot = 7;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    run(&A, 20);
    qd_quad_preset_patch(5, p);
    p[QD_QP_HARM] = 111;
    qd_lib_store(&A, slot, "KICK", p);
    k = qd_pool_add_sound(&A, LIB(&A, slot));
    tsound_of(&qd_proj_pat(&A.proj, 0)->snd[0], LIB(&A, slot), 0);
    tsound_of(&qd_proj_pat(&A.proj, 1)->snd[0], LIB(&A, slot), 0);
    qd_proj_pat(&A.proj, 0)->used = qd_proj_pat(&A.proj, 1)->used = 1;
    qd_persist_flush(&A);
    qd_proj_pat(&A.proj, 1)->snd[0].patch[QD_QP_HARM] = 222;   /* A02: edited, REPLACE */
    qd_proj_pat(&A.proj, 1)->snd[0].edited = 1;
    qd_lib_replace(&A, slot, "KICK", qd_proj_cpat(&A.proj, 1)->snd[0].patch);
    qd_undo_close(&A);
    CHECK(qd_proj_cpat(&A.proj, 0)->snd[0].patch[QD_QP_HARM] == 111 && k > 0 &&
              qd_sound_patch(&A.proj, k)[QD_QP_HARM] == 222,
          "REPLACE: A01's track keeps the old version (in RAM), the pool's reference plays the new one");
    qd_persist_flush(&A);
    boot(&B);
    CHECK(qd_proj_cpat(&B.proj, 0)->snd[0].patch[QD_QP_HARM] == 111 &&
              qd_proj_cpat(&B.proj, 1)->snd[0].patch[QD_QP_HARM] == 222 &&
              qd_sound_patch(&B.proj, k)[QD_QP_HARM] == 222 && LIB(&B, slot)->patch[QD_QP_HARM] == 222,
          "REPLACE, a restart: A01 the old version, A02 and the pool the new one");
}

/* P2.1: a flash of 'P' 4 pattern records (as P1.6h wrote them), two projects, MY presets and factory sounds on their
 * tracks: read by this core, every pattern the same; qd_persist_migrate: every record 'P' 5, read again after a
 * restart, every pattern still the same, the records smaller (the owner's own flash: tools/flash_check.py) */
static void p5_migration_test(void)
{
    static qd_pattern_t want[2][4];
    static const int PK[4] = {0, 1, 7, 40};
    int16_t p[QD_QUAD_NP];
    qd_sound_t f;
    int j, k, ok, n, v4 = 0, v5 = 0, len4 = 0, len5 = 0;
    memset(FL, 0xFF, sizeof FL);
    qd_persist_write_v4 = 1;
    boot(&A);
    run(&A, 20);
    qd_quad_preset_patch(9, p);
    p[QD_QP_HARM] = 444;
    qd_lib_store(&A, 0, "MIGRATE ME", p);
    for (j = 0; j < 2; j++) {
        if (j)
            qd_proj_new(&A, 1);
        for (k = 0; k < 4; k++) {
            qd_pattern_t *P = qd_proj_pat(&A.proj, PK[k]);
            qd_step_on(P, k, 3 + j);
            tsound_of(&P->snd[0], LIB(&A, 0), 0);
            qd_factory_preset(5 + k, &f);
            tsound_of(&P->snd[1], &f, k == 2);
            P->used = 1;
        }
        qd_persist_flush(&A);
        for (k = 0; k < 4; k++)
            want[j][k] = *qd_proj_cpat(&A.proj, PK[k]);
    }
    for (j = 0; j < 2; j++)
        for (k = 0; k < 4; k++) {
            v4 += rec_ref(&A, j, PK[k], 0) == 0xFFFFu;
            len4 += qd_store_get(&A.per.prj_st, QD_RKEY(QD_RK_PAT, j, PK[k]), 0, 0);
        }
    qd_persist_write_v4 = 0;
    boot(&B);                                           /* (project 2 the one worked on: its settings) */
    for (ok = 0, j = 1; j >= 0; j--) {
        qd_proj_load(&B, j);
        for (k = 0; k < 4; k++)
            ok += !memcmp(qd_proj_cpat(&B.proj, PK[k]), &want[j][k], sizeof want[j][k]);
    }
    CHECK(v4 == 8 && ok == 8, "P2.1: a flash of 'P' 4 records (%d): read by this core, the 8 patterns the same (%d)", v4,
          ok);
    n = qd_persist_migrate(&B);
    qd_persist_flush(&B);
    boot(&A);
    for (ok = 0, j = 1; j >= 0; j--) {
        qd_proj_load(&A, j);
        for (k = 0; k < 4; k++)
            ok += !memcmp(qd_proj_cpat(&A.proj, PK[k]), &want[j][k], sizeof want[j][k]);
    }
    for (j = 0; j < 2; j++)
        for (k = 0; k < 4; k++) {
            v5 += rec_ref(&A, j, PK[k], 0) != 0xFFFFu;
            len5 += qd_store_get(&A.per.prj_st, QD_RKEY(QD_RK_PAT, j, PK[k]), 0, 0);
        }
    CHECK(n >= 8 && v5 == 8 && ok == 8 && len5 < len4 && !qd_persist_migrate(&A),
          "P2.1: migrated (%d records): every one 'P' 5 (%d), a restart: the 8 patterns the same (%d); %d bytes, not %d; "
          "again: nothing", n, v5, ok, len5, len4);
}

/* the browser's POOL at the device's sizes (the owner's freeze, 2026-10-24): the view's users of a pool sound read
 * once, not every frame, and none of it pages a pattern in; - POOL of a sound other patterns lock pages nothing in
 * and writes no pattern then and there (their locks go as the background reaches them; undo brings them back) */
static int locks_to(qd_app_t *a, int k)
{
    int p, t, s, n = 0;
    for (p = 0; p < QD_NPAT; p++)
        for (t = 0; t < QD_PAT_NTRACK; t++)
            for (s = 0; s < QD_NSTEP; s++)
                n += qd_proj_cpat(&a->proj, p)->step[t][s].sound == k;
    return n;
}
static void demo_drums_test(void)                      /* demo.h's table: what the demos lock, as they are made */
{
    static qd_pattern_t P;
    int d, t, s, bad = 0;
    for (d = 0; d < QD_DEMO_N; d++) {
        qd_demo_pattern(&P, d);
        for (t = 0; t < QD_PAT_NTRACK; t++) {
            bad += P.snd[t].sound != 0;
            for (s = 0; s < QD_NSTEP; s++) {
                const qd_step_t *x = &P.step[t][s];
                const int want = t == 0 && s < 16 ? QD_DEMO_DRUMS[d][s] : 0;
                bad += x->sound != want || (want && (!(x->flags & QD_SF_ON) || !(x->ovr & QD_OV(QD_PM_SOUND))));
            }
        }
    }
    CHECK(!bad, "demo.h QD_DEMO_DRUMS: the demos' sound locks, as made (%d differ)", bad);
}
#if QD_SND_RAM
#define SC_LOADS(a) ((a)->per.sc.loads)
#else
#define SC_LOADS(a) 0u
#endif
static void pool_browse_test(void)
{
    static const int PATS[] = {10, 20, 30, 40, 50};
    uint32_t l0, s0, g0, l1;
    ui_browse_t v;
    int i, f, snare = 19, n0;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    G = &A;
    for (i = 0; i < 5; i++) {
        qd_pattern_t *P = qd_proj_pat(&A.proj, PATS[i]);
        qd_step_on(P, 1, 3 + i);
        qd_step_set_sound(P, 1, 3 + i, snare);
        P->used = 1;
    }
    qd_persist_flush(&A);
    run(&A, 50);
    n0 = locks_to(&A, snare);                           /* (the demos' own too) */
    knob(QD_K_PRESETS, 1);
    black(8), black(8);                                 /* B9 twice: the POOL */
    knob(QD_K_PRESETS, -200);
    knob(QD_K_PRESETS, snare - 1);
    CHECK(A.ui.br.src == QD_BR_POOL && qd_br_pool_k(&A) == snare, "(the POOL, SNARE chosen: %d)", qd_br_pool_k(&A));
    qd_view_browse(&A, &v);
    l0 = A.proj.pat_loads;
    for (f = 0; f < 20; f++)
        qd_view_browse(&A, &v);
    CHECK(v.usteps == n0 && A.proj.pat_loads == l0, "the POOL's view: SNARE's users, 20 frames, no pattern read in "
          "(%d users, %u loads)", v.usteps, (unsigned)(A.proj.pat_loads - l0));
    l0 = A.proj.pat_loads, s0 = A.proj.pat_saves, g0 = A.per.prj_st.c.progs;
    bkey(9, 1), bkey(9, 0);                             /* B10: OUT? */
    bkey(9, 1), bkey(9, 0);                             /* B10: out */
    l1 = A.proj.pat_loads;
    CHECK(!qd_sound_used(&A.proj, snare) && l1 == l0 && A.proj.pat_saves == s0,
          "- POOL of a sound 5 other patterns lock: no pattern read in or written then and there (%u loads, %u saves, "
          "%u pages)", (unsigned)(l1 - l0), (unsigned)(A.proj.pat_saves - s0), (unsigned)(A.per.prj_st.c.progs - g0));
#if QD_PAT_RAM < QD_NPAT
    CHECK(qd_pool_unref_pending(&A) != 0, "(its patterns' locks pending)");
    CHECK(qd_pool_free_slot(&A.proj) != snare, "its slot not given out while some records lock it (%d)",
          qd_pool_free_slot(&A.proj));
#endif
    run(&A, 3000);
#if QD_PAT_RAM < QD_NPAT
    CHECK(!qd_pool_unref_pending(&A), "a few seconds on: the background has been through them (%llx)",
          (unsigned long long)qd_pool_unref_pending(&A));
#endif
    CHECK(locks_to(&A, snare) == 0, "every lock of it gone (%d left)", locks_to(&A, snare));
    qd_undo_step(&A, -1);
    CHECK(qd_sound_used(&A.proj, snare) && locks_to(&A, snare) == n0, "undo: SNARE back, its %d locks too (%d)",
          n0, locks_to(&A, snare));
    qd_undo_step(&A, 1);
    CHECK(!qd_sound_used(&A.proj, snare) && locks_to(&A, snare) == 0, "redo: out again (%d)", locks_to(&A, snare));
    qd_undo_step(&A, -1);
    bkey(9, 1), bkey(9, 0), bkey(9, 1), bkey(9, 0);    /* out again, undone before the background got to them */
    qd_undo_step(&A, -1);
    CHECK(qd_sound_used(&A.proj, snare) && locks_to(&A, snare) == n0, "out and undone at once: the locks (%d)",
          locks_to(&A, snare));
    run(&A, 3000);
    {   /* + POOL from MY PRESETS / ALL: nothing written then and there either */
        uint32_t pp, pe, lp, le, sl;
        black(8);                                       /* B9: ALL */
        knob(QD_K_PRESETS, -200);
        for (i = 0; i < 30 && qd_br_pool_k(&A); i++)
            knob(QD_K_PRESETS, 1);
        pp = A.per.prj_st.c.progs, pe = A.per.prj_st.c.erases, lp = A.per.lib_st.c.progs, le = A.per.lib_st.c.erases;
        l0 = A.proj.pat_loads, sl = SC_LOADS(&A);
        bkey(9, 1), bkey(9, 0);
        CHECK(qd_br_pool_k(&A) && A.proj.pat_loads == l0 && A.per.prj_st.c.progs == pp && A.per.prj_st.c.erases == pe &&
                  A.per.lib_st.c.progs == lp && A.per.lib_st.c.erases == le,
              "+ POOL: in, no pattern read in, nothing written then and there (%u loads, %u + %u pages, %u + %u erases, "
              "%u sounds read)", (unsigned)(A.proj.pat_loads - l0), (unsigned)(A.per.prj_st.c.progs - pp),
              (unsigned)(A.per.lib_st.c.progs - lp), (unsigned)(A.per.prj_st.c.erases - pe),
              (unsigned)(A.per.lib_st.c.erases - le), (unsigned)(SC_LOADS(&A) - sl));
    }
}

int main(void)
{
    static qd_project_t keep;
    static qd_pattern_t keep_pat[QD_NPAT];             /* (P2.1: the 64 patterns, out of RAM or not) */
    static qd_sound_t keep_snd[QD_NSOUND];             /* (P2.1: the pool's 128 sounds, the same) */
    int k, ok, t;

    /* a fresh flash: the first project, its demos, written */
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    CHECK(A.per.on && A.per.proj == 0 && A.per.pexists[0] && !strcmp(A.per.pname[0], "PROJECT 1") &&
              qd_proj_pat(&A.proj, 0)->used && A.seq.cur == QD_PAT_START,
          "a fresh flash: PROJECT 1 with the demos, A02");

    /* the tracks' sounds are the pattern's */
    run(&A, 5);
    A.model.t[0].patch[QD_QP_HARM] = 777;
    A.model.t[0].edited = 1;
    run(&A, 2);
    CHECK(qd_proj_pat(&A.proj, QD_PAT_START)->snd[0].patch[QD_QP_HARM] == 777, "a track's sound edited: its pattern's copy");
    qd_seq_select(&A.seq, 7, 1);
    run(&A, 2);
    CHECK(A.model.t[0].patch[QD_QP_HARM] != 777, "another pattern: its own track sounds");
    qd_seq_select(&A.seq, QD_PAT_START, 1);
    run(&A, 2);
    CHECK(A.model.t[0].patch[QD_QP_HARM] == 777 && A.model.t[0].edited, "back: the edited one");

    /* written by itself; §7 while playing */
    qd_app_play(&A);
    run(&A, 200);
    qd_step_on(qd_proj_pat(&A.proj, QD_PAT_START), 5, 3);
    run(&A, 1000);
    CHECK(qd_persist_dirty(&A), "an edit: not written in the first second");
    run(&A, 4000);
    CHECK(!qd_persist_dirty(&A) && A.per.prj_st.c.saves_playing > 0 && A.per.prj_st.c.erases_noisy == 0,
          "1.5 s after: written, while playing (%u records, %u pages playing), no erase while sounding",
          (unsigned)A.per.prj_st.c.saves_playing, (unsigned)A.per.prj_st.c.pages_playing);
    qd_app_stop(&A);
    run(&A, 100);

    /* a full project: 64 patterns (two full: 1024 steps, 1024 locks), 128 pool sounds, the mix, the settings. P2.4: at
     * the device's sizes the projects' store is the unit's 51 sectors (615 pages for records): one full pattern, the
     * others lighter, so the copies below find room too */
#ifdef QD_DEVICE
    for (k = 0; k < QD_NPAT; k++)
        fill_pattern(qd_proj_pat(&A.proj, k), k == 9 ? 1024 : (int)(4 + rnd() % 12u), k == 9 ? 1000 : (int)(rnd() % 8u));
#else
    for (k = 0; k < QD_NPAT; k++)
        fill_pattern(qd_proj_pat(&A.proj, k), k == 9 || k == 40 ? 1024 : (int)(10 + rnd() % 60u), k == 9 || k == 40 ? 1000 : (int)(rnd() % 40u));
#endif
    for (k = 1; k <= QD_NSOUND; k++) {
        int16_t p[QD_QUAD_NP];
        char nm[12];
        qd_quad_preset_patch((int)(rnd() % 17u), p);
        p[QD_QP_DRIVE] = (int16_t)(k % 30);
        snprintf(nm, sizeof nm, "S%03d", k);
        qd_lib_store(&A, 100 + k, nm, p);               /* (P1.5w: the pool references presets: MY ones here) */
        {
            qd_sound_t s;
            memset(&s, 0, sizeof s);
            strcpy(s.name, nm);
            s.preset = LIB(&A, 100 + k)->preset;
            qd_sound_write(&A.proj, k, &s);
        }
    }
    qd_pool_resolve(&A);
    A.model.t[3].mix.level = 77;
    A.model.t[3].mix.pan = -20;
    A.proj.rec_quant = 35;
    A.proj.rec_dub = QD_TAKE_REPLACE;                   /* (2026-10-25: the TAKE's third value) */
    qd_fx_set(QD_FXP_RSIZE, 33);
    qd_fx_set(QD_FXP_CAMT_C, 41);                       /* the routing's chain amount, the latch mode, the editor's keys */
    A.ui.lmode = QD_LMODE_VALUES;                       /* (not the default: it must come back) */
    A.proj.rec_ekeys = 1;
    qd_seq_select(&A.seq, 12, 1);
    qd_ctl_select_track(&A, 5);
    run(&A, 20);
    CHECK(qd_persist_flush(&A), "flushed");
    keep = A.proj;
    for (k = 0; k < QD_NPAT; k++)
        keep_pat[k] = *qd_proj_cpat(&A.proj, k);
    for (k = 1; k <= QD_NSOUND; k++)
        qd_sound_read(&A.proj, k, &keep_snd[k - 1]);
    {
        uint32_t live, total;
        qd_persist_use(&A, &live, &total);
        printf("  the full project: %u of %u pages (%.0f KB of records)\n", (unsigned)live, (unsigned)total,
               live * 240.0 / 1024.0);
    }
    qd_fx_set(QD_FXP_RSIZE, 88);                        /* (the effects are the app's: B's boot reads them back) */
    qd_fx_set(QD_FXP_CAMT_C, 64);
    boot(&B);
    for (k = 0, ok = 0; k < QD_NPAT; k++)
        ok += !memcmp(&keep_pat[k], qd_proj_cpat(&B.proj, k), sizeof keep_pat[k]);
    CHECK(ok == QD_NPAT, "a restart: the 64 patterns as they were (%d same)", ok);
    for (k = 1, ok = 0; k <= QD_NSOUND; k++)
        ok += !memcmp(&keep_snd[k - 1], POOL(&B.proj, k), sizeof keep_snd[k - 1]);
    CHECK(ok == QD_NSOUND && B.proj.nsound == keep.nsound,
          "the pool's 128 sounds (DRIVE among them)");
    CHECK(B.model.t[3].mix.level == 77 && B.model.t[3].mix.pan == -20 && B.proj.rec_quant == 35 &&
              B.proj.rec_dub == QD_TAKE_REPLACE && qd_fx_get(QD_FXP_RSIZE) == 33,
          "the mix, the record settings, the effects");
    CHECK(qd_fx_get(QD_FXP_CAMT_C) == 41 && B.ui.lmode == QD_LMODE_VALUES && B.proj.rec_ekeys == 1,
          "P1.5b: the routing, the latch mode, EDIT KEYS");
    CHECK(B.per.proj == 0 && B.seq.cur == 12 && B.sel == 5, "the restart check: the project, its pattern (A13) and "
          "the selected track (6)");
    {   /* P1.5b: a flash of 0.4 (the settings with 23 effects' values and no latch mode, the header with no EDIT KEYS)
         * still boots: what it has, the rest at the defaults */
        static uint8_t b[QDS_MAXREC * 256];
        int len = qd_store_get(&B.per.prj_st, QD_RKEY(QD_RK_SET, 0, 0), b, sizeof b);
        b[8] = 23;
        qd_store_put(&B.per.prj_st, QD_RKEY(QD_RK_SET, 0, 0), b, 9u + 46u);
        qd_store_flush(&B.per.prj_st);
        len = qd_store_get(&B.per.prj_st, QD_RKEY(QD_RK_HDR, 0, 0), b, sizeof b);
        (void)len;
        qd_store_put(&B.per.prj_st, QD_RKEY(QD_RK_HDR, 0, 0), b, 2u + QD_PROJ_NAME + 7u + 7u * QD_NTRACK);   /* (no
                                                         * EDIT KEYS, no P1.5t pool byte, no P1.6d bytes) */
        qd_store_flush(&B.per.prj_st);
        qd_fx_set(QD_FXP_RSIZE, 88);
        boot(&A);
        CHECK(qd_fx_get(QD_FXP_RSIZE) == 33 && qd_fx_get(QD_FXP_CAMT_C) == 64 && A.ui.lmode == QD_LMODE_MODES &&
                  !A.proj.rec_ekeys && A.model.t[3].mix.level == 77 && A.seq.cur == 12,
              "a 0.4 flash boots: its effects and mix, the routing / latch mode / EDIT KEYS at their defaults (%d %d %d %d %d %d)",
              qd_fx_get(QD_FXP_RSIZE), qd_fx_get(QD_FXP_CAMT_C), A.ui.lmode, A.proj.rec_ekeys, A.model.t[3].mix.level,
              A.seq.cur);
        boot(&B);
    }

    /* the pool: swap renumbers, delete sends locks back, INIT stays, copy */
    {
        int locks_a = 0, locks_b = 0, src = 0;
        qd_step_on(qd_proj_pat(&B.proj, 3), 2, 4);
        qd_step_set_sound(qd_proj_pat(&B.proj, 3), 2, 4, 30);
        qd_step_on(qd_proj_pat(&B.proj, 50), 5, 100);
        qd_step_set_sound(qd_proj_pat(&B.proj, 50), 5, 100, 31);
        qd_proj_pat(&B.proj, 20)->snd[1].sound = 30;
        qd_pool_swap(&B, 30, 31);
        for (k = 0; k < QD_NPAT; k++)
            for (t = 0; t < QD_PAT_NTRACK; t++) {
                int s;
                for (s = 0; s < QD_NSTEP; s++) {
                    locks_a += qd_proj_pat(&B.proj, k)->step[t][s].sound == 31 && k == 3;
                    locks_b += qd_proj_pat(&B.proj, k)->step[t][s].sound == 30 && k == 50;
                }
                src += qd_proj_pat(&B.proj, k)->snd[t].sound == 31 && k == 20;
            }
        CHECK(locks_a == 1 && locks_b == 1 && src == 1 && !strcmp(POOL(&B.proj, 31)->name, "S030"),
              "pool move (30 <-> 31): the locks in every pattern and the track source follow");
        CHECK(qd_pool_delete(&B, 1) && !POOL(&B.proj, 1)->name[0], "P1.5t: slot 1 as any other: deleted");
        CHECK(qd_pool_delete(&B, 31) && qd_proj_pat(&B.proj, 3)->step[2][4].sound == 0 && !POOL(&B.proj, 31)->name[0] &&
                  qd_proj_pat(&B.proj, 20)->snd[1].sound == 0,
              "pool delete: the locks back to the track's sound, the track keeps its copy");
        CHECK(qd_pool_copy(&B, 5, 31) && !strcmp(POOL(&B.proj, 31)->name, POOL(&B.proj, 5)->name), "pool copy 5 -> 31");
    }
    /* MY presets: store, replace, delete, kept (256 places) */
    {
        int16_t p[QD_QUAD_NP];
        int id9;
        qd_quad_preset_patch(10, p);
        for (k = 0; k < QD_NLIB; k++) {
            qd_sound_t z;
            memset(&z, 0, sizeof z);
            qd_lib_write(&B, k, &z);
        }
        CHECK(qd_lib_free_slot(&B) == 0 && qd_lib_store(&B, 0, "WOB BASS", p), "MY presets: the first free place (1)");
        qd_lib_store(&B, 9, "WOB TWO", p);
        id9 = LIB(&B, 9)->preset;
        p[QD_QP_HARM] = 500;
        qd_lib_replace(&B, 9, "WOB REPL", p);
        qd_lib_store(&B, 2, "GONE", p);
        qd_lib_delete(&B, 2);
        qd_lib_store(&B, 255, "THE LAST", p);
        CHECK(qd_persist_flush(&B), "flushed");
        boot(&A);
        CHECK(!strcmp(LIB(&A, 0)->name, "WOB BASS") && !strcmp(LIB(&A, 9)->name, "WOB REPL") &&
                  LIB(&A, 9)->preset == id9 && LIB(&A, 9)->patch[QD_QP_HARM] == 500 && !LIB(&A, 2)->name[0] &&
                  !strcmp(LIB(&A, 255)->name, "THE LAST") && LIB(&A, 0)->patch[QD_QP_DRIVE] == 10 &&
                  LIB(&A, 0)->tags == QD_PKB(QD_PK_BASS) && LIB(&A, 0)->preset >= QD_PRESET_MINE0 &&
                  LIB(&A, 0)->preset != LIB(&A, 9)->preset && !strcmp(POOL(&A.proj, 31)->name, POOL(&A.proj, 5)->name),
              "a restart: MY presets (their ids, kinds; replace kept the id; place 256) and the pool's changes kept");
    }
    /* the projects */
    qd_seq_select(&A.seq, 1, 1);
    A.ui.view = QD_VIEW_ROLL;
    CHECK(qd_proj_new(&A, 2) && A.seq.cur == 0 && A.sel == 0, "a new project on A01, track 1 (not the last one's A02)");
    qd_persist_flush(&A);
    boot(&A);
    CHECK(A.per.proj == 2 && A.seq.cur == 0, "a restart: the new project on A01 still");
    qd_proj_load(&A, 0);
    qd_seq_select(&A.seq, 1, 1);
    A.ui.view = QD_VIEW_ROLL;
    CHECK(qd_proj_new(&A, 2) && A.ui.view == QD_VIEW_EDIT && A.per.proj == 2 && A.per.pexists[2] && !qd_proj_pat(&A.proj, 0)->used &&
              !strcmp(A.per.pname[2], "PROJECT 3") && A.proj.nsound == 0 && A.proj.pool_empty,
          "new project 3: blank, its pool empty (P1.5t)");
    qd_step_on(qd_proj_pat(&A.proj, 0), 0, 0);
    qd_proj_pat(&A.proj, 0)->used = 1;
    qd_proj_name(&A, 2, "MY SONG");
    CHECK(qd_proj_copy(&A, 2, 4) && A.per.pexists[4] && !strcmp(A.per.pname[4], "MY SONG"), "copy 3 -> 5");
    qd_undo_input(&A, 0);                               /* (P1.5c: an undo level, then a load forgets it) */
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur), 0, 5);
    qd_undo_close(&A);
    CHECK(qd_undo_count(&A, -1) == 1, "an edit: one undo level");
    CHECK(qd_proj_load(&A, 4) && A.per.proj == 4 && qd_step_is_on(qd_step(qd_proj_pat(&A.proj, 0), 0, 0)) && A.proj.nsound == 0,
          "load 5: the copy has the edit (its pool empty still: a copy of a new project)");
    CHECK(qd_undo_count(&A, -1) == 0 && qd_undo_count(&A, 1) == 0, "a project load: undo forgets (P1.5c)");
    CHECK(qd_proj_delete(&A, 2) && !A.per.pexists[2], "delete 3");
    CHECK(qd_proj_load(&A, 0) && A.per.proj == 0 && !memcmp(&keep_pat[9], qd_proj_cpat(&A.proj, 9), sizeof keep_pat[9]),
          "load 1 again: as it was");
    {   /* P1.5r: a sound imported over SysEx into LIBRARY 100: autosaved like any other */
        uint8_t f[160], blob[QD_QUAD_BLOB5];
        int16_t p[QD_QUAD_NP];
        uint32_t i, n = 0, sum = QD_SX_STORE;
        qd_quad_preset_patch(3, p);
        qd_quad_pack5(p, blob);
        f[n++] = 0x7D, f[n++] = 0x46, f[n++] = 0x5A, f[n++] = 0x01, f[n++] = QD_SX_STORE;
        f[n++] = 0, f[n++] = 0, f[n++] = 100;
        for (i = 0; i < QD_SX_NAME; i++)
            f[n++] = (uint8_t)("SX KEEP     "[i]);
        f[n++] = 0, f[n++] = 0, f[n++] = 64, f[n++] = 0, f[n++] = 0, f[n++] = 0, f[n++] = 0;
        n += qd_sx_pack7(blob, QD_QUAD_BLOB5, f + n);
        for (i = 4; i < n; i++)
            sum += i > 4 ? f[i] : 0;
        f[n++] = (uint8_t)((0x80u - (sum & 0x7Fu)) & 0x7Fu);
        qd_app_sysex(&A, f, n);
        CHECK(!strcmp(LIB(&A, 99)->name, "SX KEEP"), "a SysEx import into LIBRARY 100");
    }
    A.ui.view = QD_VIEW_STEPS;                          /* (P1.5s: the user's view kept) */
    qd_persist_flush(&A);
    boot(&B);
    CHECK(B.ui.view == QD_VIEW_STEPS, "a restart: the view kept (STEPS; P1.5s)");
    CHECK(B.per.proj == 0 && B.per.pexists[4] && !B.per.pexists[2] && !strcmp(B.per.pname[4], "MY SONG"),
          "a restart: project 1 worked on, 5 kept, 3 gone");
    CHECK(!strcmp(LIB(&B, 99)->name, "SX KEEP"), "a restart: the SysEx import kept (P1.5r)");
    CHECK(A.per.prj_st.c.erases_noisy == 0 && B.per.prj_st.c.erases_noisy == 0, "no erase while sounding, ever");
    demo_drums_test();
    pool_browse_test();
    pages_test();
    paging_test();
    sound_paging_test();
    isr_paging_test();
    foreign_test();
    pref_test();
    replace_rule_test();
    p5_migration_test();
    migration_test();
    mfx_test();
    tags_test();
    refs_test();
    migrate72_test();
    mix_test();
    names_test();
    old_format_test();
#ifdef QD_DEVICE
    CHECK(qd_arena_fails() == 0, "P2.1b: the drawing's arena never short (it lends its room between frames: %u "
          "refused)", qd_arena_fails());
#endif
    printf("test_persist: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
