/* SPDX-License-Identifier: GPL-3.0-only */
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
#include "qd_fx.h"
#include "sounds.h"

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
        const int t = (int)(rnd() % 8u), s = nsteps >= 1024 ? i % 128 + (i / 128) * 0 : (int)(rnd() % 128u);
        const int tt = nsteps >= 1024 ? i / 128 : t;
        uint8_t n[1] = {(uint8_t)(36 + rnd() % 48u)};
        qd_step_on(P, tt, s);
        qd_step_set_notes(P, tt, s, n, 1);
        qd_step_set_vel(P, tt, s, (int)(1 + rnd() % 127u));
    }
    for (i = 0; i < nlocks; i++)
        qd_lock_set(P, (int)(rnd() % 8u), (int)(rnd() % 128u), (int)(rnd() % QD_QP_LEVEL), (int16_t)(rnd() % 50u));
    P->used = 1;
}


/* ---------------------------------------------------------------- P1.5a part 2: the pages, by the panel */
static qd_app_t *G;
static void p_run(uint32_t n) { run(G, n); }
static void bt(int b, int down) { qd_app_button(G, b, down); }
static void btap(int b) { bt(b, 1); p_run(20); bt(b, 0); p_run(20); }
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
        for (j = 1; j <= G->proj.nsound && strcmp(G->proj.sound[j - 1].name, NAMES[k].name); j++)
            ;
        ok += qd_sound_kind(G->proj.sound[j - 1].patch) == NAMES[k].kind;
        qd_sound_autoname(G->proj.sound[j - 1].patch, 0, nm);
        qd_sound_autoname(G->proj.sound[j - 1].patch, 0, nm2);
        okn += !strcmp(nm, nm2) && strlen(nm) <= 12 && strstr(nm, NAMES[k].kind == QD_SK_HAT ? "HAT" : qd_sound_kind_name(NAMES[k].kind));
        qd_sound_autoname(G->proj.sound[j - 1].patch, 1, nm2);
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
            ok += f.kind == PK[k] && f.preset == k + 1;
            seen[f.kind]++;
        }
        for (k = 0; k < QD_NPK; k++)
            ok += seen[k] > 0;
        CHECK(ok == QD_NFACTORY + QD_NPK, "the factory presets' kinds (METAL, FEEDBACK, NOISE-ISH FX; the drums DRUMS; "
              "BELL KEYS ..), every one of the seven used (%d)", ok);
    }

    /* P1.5t: SAVE PRESET: the track's sound into MY presets */
    qd_ctl_select_track(G, 3);
    btap(QD_B_SAVE);
    qd_sound_autoname(G->model.t[3].patch, 0, nm);
    CHECK(qd_view_screen(G) == QD_SCR_SAVE && G->ui.sv.spark && !strncmp(G->ui.sv.name, nm, strlen(nm)) &&
              G->ui.sv.replace < 0, "SAVE: SAVE PRESET, a name from the sound (%s), no REPLACE (a factory sound)", nm);
    btap(QD_B_SAVE);
    CHECK(!strcmp(G->per.lib[0].name, nm) && !memcmp(G->per.lib[0].patch, G->model.t[3].patch, sizeof G->per.lib[0].patch) &&
              G->per.lib[0].preset >= QD_PRESET_MINE0 && G->model.t[3].preset == G->per.lib[0].preset &&
              !G->model.t[3].edited && !G->ui.latch && G->ui.tk == QD_TK_SAVED,
          "SAVE again: a NEW MY preset (place 1, its own id), the track plays it, not edited; SAVED");
    G->model.t[3].patch[QD_QP_HARM] = 900;              /* edited, then SAVE: REPLACE offered */
    G->model.t[3].edited = 1;
    p_run(20);
    btap(QD_B_SAVE);
    CHECK(G->ui.sv.replace == 0 && !strncmp(G->ui.sv.name, nm, strlen(nm)), "edited, SAVE: REPLACE offered (its "
          "name kept)");
    btap(QD_B_OCTUP);
    CHECK(G->per.lib[0].patch[QD_QP_HARM] == 900 && G->per.lib[0].preset == G->model.t[3].preset &&
              !G->model.t[3].edited && !G->per.lib[1].name[0], "F2: replaced (the same id), nothing new");
    /* the browser on MY presets: load it onto another track */
    qd_ctl_select_track(G, 1);
    p_run(QD_TK_MS + QD_TK_FADE_MS + 10);
    knob(QD_K_PRESETS, 1);
    CHECK(qd_view_screen(G) == QD_SCR_BROWSE, "PRESETS: the library browser");
    black(8);                                           /* B9: MY PRESETS */
    knob(QD_K_PRESETS, 1);
    btap(QD_B_OCTUP);
    CHECK(G->model.t[1].preset == G->per.lib[0].preset && G->model.t[1].patch[QD_QP_HARM] == 900 &&
              G->proj.pat[G->seq.cur].snd[1].preset == G->per.lib[0].preset, "B9, a turn, F2: MY preset loaded on track 2");

    /* the project pages */
    btap(QD_B_MIX);
    CHECK(qd_view_screen(G) == QD_SCR_PRJ && G->ui.pj.page == 0, "SEQ tapped: the project page");
    btap(QD_B_MIX);
    CHECK(G->ui.pj.page == 1, "SEQ again: the projects");
    wtap(2);
    btap(QD_B_OCTUP);
    CHECK(G->per.proj == 2 && G->per.pexists[2] && G->ui.pj.page == 0, "W3, F2: a new project 3, its page");
    btap(QD_B_MIX);
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
    black(9);
    knob(QD_K_SELECT, 2);
    btap(QD_B_OCTUP);
    CHECK(G->per.pname[2][0] == 'R', "B10: the project named (%s)", G->per.pname[2]);
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
    uint8_t blob[QD_QUAD_BLOB];
    int16_t p[QD_QUAD_NP];
    int len, t, n;
    G = &A;
    memset(FL, 0xFF, sizeof FL);
    boot(&A);                                           /* (an erased flash: the demo project, written) */
    run(&A, 20);
    qd_persist_flush(&A);
    /* 0.6's records: a library slot, a pool slot (EP's copy), the pattern's track sounds, the header */
    qd_quad_preset_patch(3, p);                         /* (BASS) */
    qd_quad_pack(p, blob);
    b[0] = 'S', b[1] = 1;
    memset(b + 2, 0, QD_SOUND_NAME);
    strcpy((char *)b + 2, "OLD LIB");
    memcpy(b + 2 + QD_SOUND_NAME, blob, QD_QUAD_BLOB);
    qd_store_put(&A.per.lib_st, QD_RKEY(QD_RK_LIB, 0, 7), b, 2 + QD_SOUND_NAME + QD_QUAD_BLOB);
    qd_store_flush(&A.per.lib_st);
    qd_quad_preset_patch(1, p);                         /* (EP, as the 0.6 pool had it) */
    qd_quad_pack(p, blob);
    memset(b + 2, 0, QD_SOUND_NAME);
    strcpy((char *)b + 2, "EP");
    memcpy(b + 2 + QD_SOUND_NAME, blob, QD_QUAD_BLOB);
    qd_store_put(&A.per.prj_st, QD_RKEY(QD_RK_SND, 0, 40), b, 2 + QD_SOUND_NAME + QD_QUAD_BLOB);
    qd_store_flush(&A.per.prj_st);
    A.proj.pat[QD_PAT_START].snd[3].sound = 40;         /* (track 4, SQUARE LEAD's place, from pool 40) */
    A.proj.pat[QD_PAT_START].snd[3].preset = 0;
    A.proj.pat[QD_PAT_START].used = 1;
    qd_persist_flush(&A);
    len = qd_store_get(&A.per.prj_st, QD_RKEY(QD_RK_PAT, 0, QD_PAT_START), b, sizeof b);
    {   /* 'P' 2 -> 'P' 1: each track sound without its two preset bytes */
        const uint32_t one = QD_QUAD_BLOB + 2 + QD_SOUND_NAME, start = (uint32_t)len - 8u * (one + 2u);
        static uint8_t c[QDS_MAXREC * 256];
        memcpy(c, b, start);
        for (t = 0; t < 8; t++)
            memcpy(c + start + (uint32_t)t * one, b + start + (uint32_t)t * (one + 2u), one);
        c[1] = 1;
        qd_store_put(&A.per.prj_st, QD_RKEY(QD_RK_PAT, 0, QD_PAT_START), c, start + 8u * one);
        qd_store_flush(&A.per.prj_st);
    }
    len = qd_store_get(&A.per.prj_st, QD_RKEY(QD_RK_HDR, 0, 0), b, sizeof b);
    qd_store_put(&A.per.prj_st, QD_RKEY(QD_RK_HDR, 0, 0), b, (uint32_t)len - 1u);   /* (no pool byte) */
    qd_store_flush(&A.per.prj_st);
    boot(&B);
    CHECK(!strcmp(B.per.lib[7].name, "OLD LIB") && B.per.lib[7].preset == QD_PRESET_MINE0 + 7 &&
              B.per.lib[7].kind == QD_PK_BASS && !B.per.lib[7].flags,
          "a 0.6 library slot: one of MY presets (its id %d, its kind BASS, saved)", B.per.lib[7].preset);
    CHECK(!strcmp(B.proj.sound[39].name, "EP") && B.proj.sound[39].preset == 2 && B.proj.sound[39].kind == QD_PK_KEYS,
          "a 0.6 pool sound: kept, knowing its factory preset (EP: 2), its kind");
    for (n = 0, t = 1; t <= QD_NFACTORY; t++)
        n += qd_sound_used(&B.proj, t) && B.proj.sound[t - 1].preset == t;
    CHECK(n == QD_NFACTORY && !B.proj.pool_empty, "its absent pool records: the old starting pool, as before (%d)", n);
    CHECK(B.proj.pat[QD_PAT_START].snd[3].preset == 2 && B.proj.pat[QD_PAT_START].snd[0].preset > 0,
          "its track sounds: their presets from their pool source (4: EP) or their factory sound (%d)",
          B.proj.pat[QD_PAT_START].snd[0].preset);
    qd_persist_flush(&B);
    boot(&A);
    CHECK(A.per.lib[7].preset == QD_PRESET_MINE0 + 7 && A.proj.pat[QD_PAT_START].snd[3].preset == 2,
          "written back as 0.7's records: the same after another restart");
}

int main(void)
{
    static qd_project_t keep;
    int k, ok, t;

    /* a fresh flash: the first project, its demos, written */
    memset(FL, 0xFF, sizeof FL);
    boot(&A);
    CHECK(A.per.on && A.per.proj == 0 && A.per.pexists[0] && !strcmp(A.per.pname[0], "PROJECT 1") &&
              A.proj.pat[0].used && A.seq.cur == QD_PAT_START,
          "a fresh flash: PROJECT 1 with the demos, A02");

    /* the tracks' sounds are the pattern's */
    run(&A, 5);
    A.model.t[0].patch[QD_QP_HARM] = 777;
    A.model.t[0].edited = 1;
    run(&A, 2);
    CHECK(A.proj.pat[QD_PAT_START].snd[0].patch[QD_QP_HARM] == 777, "a track's sound edited: its pattern's copy");
    qd_seq_select(&A.seq, 7, 1);
    run(&A, 2);
    CHECK(A.model.t[0].patch[QD_QP_HARM] != 777, "another pattern: its own track sounds");
    qd_seq_select(&A.seq, QD_PAT_START, 1);
    run(&A, 2);
    CHECK(A.model.t[0].patch[QD_QP_HARM] == 777 && A.model.t[0].edited, "back: the edited one");

    /* written by itself; §7 while playing */
    qd_app_play(&A);
    run(&A, 200);
    qd_step_on(&A.proj.pat[QD_PAT_START], 5, 3);
    run(&A, 1000);
    CHECK(qd_persist_dirty(&A), "an edit: not written in the first second");
    run(&A, 4000);
    CHECK(!qd_persist_dirty(&A) && A.per.prj_st.c.saves_playing > 0 && A.per.prj_st.c.erases_noisy == 0,
          "1.5 s after: written, while playing (%u records, %u pages playing), no erase while sounding",
          (unsigned)A.per.prj_st.c.saves_playing, (unsigned)A.per.prj_st.c.pages_playing);
    qd_app_stop(&A);
    run(&A, 100);

    /* a full project: 64 patterns (two full: 1024 steps, 1024 locks), 128 pool sounds, the mix, the settings */
    for (k = 0; k < QD_NPAT; k++)
        fill_pattern(&A.proj.pat[k], k == 9 || k == 40 ? 1024 : (int)(10 + rnd() % 60u), k == 9 || k == 40 ? 1000 : (int)(rnd() % 40u));
    for (k = 1; k <= QD_NSOUND; k++) {
        int16_t p[QD_QUAD_NP];
        char nm[12];
        qd_quad_preset_patch((int)(rnd() % 17u), p);
        p[QD_QP_DRIVE] = (int16_t)(k % 30);
        snprintf(nm, sizeof nm, "S%03d", k);
        memset(&A.proj.sound[k - 1], 0, sizeof A.proj.sound[k - 1]);
        strcpy(A.proj.sound[k - 1].name, nm);
        memcpy(A.proj.sound[k - 1].patch, p, sizeof p);
    }
    qd_sound_recount(&A.proj);
    A.model.t[3].mix.level = 77;
    A.model.t[3].mix.pan = -20;
    A.proj.rec_quant = 35;
    qd_fx_set(QD_FXP_RSIZE, 33);
    qd_fx_set(QD_FXP_XCR, 41);                          /* P1.5b: a route, the latch mode, the editor's keys */
    A.ui.lmode = QD_LMODE_MODES;
    A.proj.rec_ekeys = 1;
    A.ui.knobs = QD_KNOBS_C;
    qd_seq_select(&A.seq, 12, 1);
    qd_ctl_select_track(&A, 5);
    run(&A, 20);
    CHECK(qd_persist_flush(&A), "flushed");
    keep = A.proj;
    {
        uint32_t live, total;
        qd_persist_use(&A, &live, &total);
        printf("  the full project: %u of %u pages (%.0f KB of records)\n", (unsigned)live, (unsigned)total,
               live * 240.0 / 1024.0);
    }
    qd_fx_set(QD_FXP_RSIZE, 88);                        /* (the effects are the app's: B's boot reads them back) */
    qd_fx_set(QD_FXP_XCR, 0);
    boot(&B);
    for (k = 0, ok = 0; k < QD_NPAT; k++)
        ok += !memcmp(&keep.pat[k], &B.proj.pat[k], sizeof keep.pat[k]);
    CHECK(ok == QD_NPAT, "a restart: the 64 patterns as they were (%d same)", ok);
    CHECK(!memcmp(keep.sound, B.proj.sound, sizeof keep.sound) && B.proj.nsound == keep.nsound,
          "the pool's 128 sounds (DRIVE among them)");
    CHECK(B.model.t[3].mix.level == 77 && B.model.t[3].mix.pan == -20 && B.proj.rec_quant == 35 &&
              qd_fx_get(QD_FXP_RSIZE) == 33 && B.ui.knobs == QD_KNOBS_C,
          "the mix, the record settings, the effects, the knob layout");
    CHECK(qd_fx_get(QD_FXP_XCR) == 41 && B.ui.lmode == QD_LMODE_MODES && B.proj.rec_ekeys == 1,
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
        qd_store_put(&B.per.prj_st, QD_RKEY(QD_RK_HDR, 0, 0), b, (uint32_t)len - 2u);   /* (no EDIT KEYS, no P1.5t
                                                         * pool byte) */
        qd_store_flush(&B.per.prj_st);
        qd_fx_set(QD_FXP_RSIZE, 88);
        boot(&A);
        CHECK(qd_fx_get(QD_FXP_RSIZE) == 33 && qd_fx_get(QD_FXP_XCR) == 0 && A.ui.lmode == QD_LMODE_VALUES &&
                  !A.proj.rec_ekeys && A.model.t[3].mix.level == 77 && A.seq.cur == 12,
              "a 0.4 flash boots: its effects and mix, the routing / latch mode / EDIT KEYS at their defaults (%d %d %d %d %d %d)",
              qd_fx_get(QD_FXP_RSIZE), qd_fx_get(QD_FXP_XCR), A.ui.lmode, A.proj.rec_ekeys, A.model.t[3].mix.level,
              A.seq.cur);
        boot(&B);
    }

    /* the pool: swap renumbers, delete sends locks back, INIT stays, copy */
    {
        int locks_a = 0, locks_b = 0, src = 0;
        qd_step_on(&B.proj.pat[3], 2, 4);
        qd_step_set_sound(&B.proj.pat[3], 2, 4, 30);
        qd_step_on(&B.proj.pat[50], 7, 100);
        qd_step_set_sound(&B.proj.pat[50], 7, 100, 31);
        B.proj.pat[20].snd[1].sound = 30;
        qd_pool_swap(&B, 30, 31);
        for (k = 0; k < QD_NPAT; k++)
            for (t = 0; t < 8; t++) {
                int s;
                for (s = 0; s < QD_NSTEP; s++) {
                    locks_a += B.proj.pat[k].step[t][s].sound == 31 && k == 3;
                    locks_b += B.proj.pat[k].step[t][s].sound == 30 && k == 50;
                }
                src += B.proj.pat[k].snd[t].sound == 31 && k == 20;
            }
        CHECK(locks_a == 1 && locks_b == 1 && src == 1 && !strcmp(B.proj.sound[30].name, "S030"),
              "pool move (30 <-> 31): the locks in every pattern and the track source follow");
        CHECK(qd_pool_delete(&B, 1) && !B.proj.sound[0].name[0], "P1.5t: slot 1 as any other: deleted");
        CHECK(qd_pool_delete(&B, 31) && B.proj.pat[3].step[2][4].sound == 0 && !B.proj.sound[30].name[0] &&
                  B.proj.pat[20].snd[1].sound == 0,
              "pool delete: the locks back to the track's sound, the track keeps its copy");
        CHECK(qd_pool_copy(&B, 5, 31) && !strcmp(B.proj.sound[30].name, B.proj.sound[4].name), "pool copy 5 -> 31");
    }
    /* MY presets: store, replace, delete, kept (256 places) */
    {
        int16_t p[QD_QUAD_NP];
        int id9;
        qd_quad_preset_patch(10, p);
        for (k = 0; k < QD_NLIB; k++)
            memset(&B.per.lib[k], 0, sizeof B.per.lib[k]);
        CHECK(qd_lib_free_slot(&B) == 0 && qd_lib_store(&B, 0, "WOB BASS", p), "MY presets: the first free place (1)");
        qd_lib_store(&B, 9, "WOB TWO", p);
        id9 = B.per.lib[9].preset;
        p[QD_QP_HARM] = 500;
        qd_lib_replace(&B, 9, "WOB REPL", p);
        qd_lib_store(&B, 2, "GONE", p);
        qd_lib_delete(&B, 2);
        qd_lib_store(&B, 255, "THE LAST", p);
        CHECK(qd_persist_flush(&B), "flushed");
        boot(&A);
        CHECK(!strcmp(A.per.lib[0].name, "WOB BASS") && !strcmp(A.per.lib[9].name, "WOB REPL") &&
                  A.per.lib[9].preset == id9 && A.per.lib[9].patch[QD_QP_HARM] == 500 && !A.per.lib[2].name[0] &&
                  !strcmp(A.per.lib[255].name, "THE LAST") && A.per.lib[0].patch[QD_QP_DRIVE] == 10 &&
                  A.per.lib[0].kind == QD_PK_BASS && A.per.lib[0].preset >= QD_PRESET_MINE0 &&
                  A.per.lib[0].preset != A.per.lib[9].preset && !strcmp(A.proj.sound[30].name, A.proj.sound[4].name),
              "a restart: MY presets (their ids, kinds; replace kept the id; place 256) and the pool's changes kept");
    }
    /* the projects */
    A.ui.view = QD_VIEW_ROLL;
    CHECK(qd_proj_new(&A, 2) && A.ui.view == QD_VIEW_EDIT && A.per.proj == 2 && A.per.pexists[2] && !A.proj.pat[0].used &&
              !strcmp(A.per.pname[2], "PROJECT 3") && A.proj.nsound == 0 && A.proj.pool_empty,
          "new project 3: blank, its pool empty (P1.5t)");
    qd_step_on(&A.proj.pat[0], 0, 0);
    A.proj.pat[0].used = 1;
    qd_proj_name(&A, 2, "MY SONG");
    CHECK(qd_proj_copy(&A, 2, 4) && A.per.pexists[4] && !strcmp(A.per.pname[4], "MY SONG"), "copy 3 -> 5");
    qd_undo_input(&A, 0);                               /* (P1.5c: an undo level, then a load forgets it) */
    qd_step_on(&A.proj.pat[A.seq.cur], 0, 5);
    qd_undo_close(&A);
    CHECK(qd_undo_count(&A, -1) == 1, "an edit: one undo level");
    CHECK(qd_proj_load(&A, 4) && A.per.proj == 4 && qd_step_is_on(qd_step(&A.proj.pat[0], 0, 0)) && A.proj.nsound == 0,
          "load 5: the copy has the edit (its pool empty still: a copy of a new project)");
    CHECK(qd_undo_count(&A, -1) == 0 && qd_undo_count(&A, 1) == 0, "a project load: undo forgets (P1.5c)");
    CHECK(qd_proj_delete(&A, 2) && !A.per.pexists[2], "delete 3");
    CHECK(qd_proj_load(&A, 0) && A.per.proj == 0 && !memcmp(&keep.pat[9], &A.proj.pat[9], sizeof keep.pat[9]),
          "load 1 again: as it was");
    {   /* P1.5r: a sound imported over SysEx into LIBRARY 100: autosaved like any other */
        uint8_t f[160], blob[QD_QUAD_BLOB];
        int16_t p[QD_QUAD_NP];
        uint32_t i, n = 0, sum = QD_SX_STORE;
        qd_quad_preset_patch(3, p);
        qd_quad_pack(p, blob);
        f[n++] = 0x7D, f[n++] = 0x46, f[n++] = 0x5A, f[n++] = 0x01, f[n++] = QD_SX_STORE;
        f[n++] = 0, f[n++] = 0, f[n++] = 100;
        for (i = 0; i < QD_SX_NAME; i++)
            f[n++] = (uint8_t)("SX KEEP     "[i]);
        f[n++] = 0, f[n++] = 0, f[n++] = 64, f[n++] = 0, f[n++] = 0, f[n++] = 0, f[n++] = 0;
        n += qd_sx_pack7(blob, QD_QUAD_BLOB, f + n);
        for (i = 4; i < n; i++)
            sum += i > 4 ? f[i] : 0;
        f[n++] = (uint8_t)((0x80u - (sum & 0x7Fu)) & 0x7Fu);
        qd_app_sysex(&A, f, n);
        CHECK(!strcmp(A.per.lib[99].name, "SX KEEP"), "a SysEx import into LIBRARY 100");
    }
    A.ui.view = QD_VIEW_STEPS;                          /* (P1.5s: the user's view kept) */
    qd_persist_flush(&A);
    boot(&B);
    CHECK(B.ui.view == QD_VIEW_STEPS, "a restart: the view kept (STEPS; P1.5s)");
    CHECK(B.per.proj == 0 && B.per.pexists[4] && !B.per.pexists[2] && !strcmp(B.per.pname[4], "MY SONG"),
          "a restart: project 1 worked on, 5 kept, 3 gone");
    CHECK(!strcmp(B.per.lib[99].name, "SX KEEP"), "a restart: the SysEx import kept (P1.5r)");
    CHECK(A.per.prj_st.c.erases_noisy == 0 && B.per.prj_st.c.erases_noisy == 0, "no erase while sounding, ever");
    pages_test();
    migration_test();
    printf("test_persist: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
