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
    int k, ok = 0, okn = 0, before;
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

    /* SAVE: storing the track's sound into the library */
    qd_ctl_select_track(G, 3);
    btap(QD_B_SAVE);
    qd_sound_autoname(G->model.t[3].patch, 0, nm);
    CHECK(qd_view_screen(G) == QD_SCR_LIB && G->ui.lb.storing && G->ui.lb.tab == 0 && G->ui.lb.chosen == 0 &&
              !strcmp(G->ui.lb.sname, nm), "SAVE: the sounds page on the LIBRARY, the first free slot, a name from the "
          "sound (%s)", nm);
    btap(QD_B_SAVE);
    CHECK(!strcmp(G->per.lib[0].name, nm) && !memcmp(G->per.lib[0].patch, G->model.t[3].patch, sizeof G->per.lib[0].patch) &&
              G->ui.lb.offer && !G->ui.lb.storing, "SAVE again: stored in slot 1; F2 offered");
    btap(QD_B_OCTUP);
    k = G->ui.lb.chosen + 1;
    CHECK(G->ui.lb.tab == 1 && k == 24 && !strcmp(G->proj.sound[23].name, nm), "F2: into the pool's first free slot "
          "(%d), the POOL shown", k);
    /* SAVE onto a sound: armed, anything else cancels, SAVE again replaces (the name stays) */
    btap(QD_B_SAVE);
    CHECK(G->ui.lb.storing && G->ui.lb.chosen == 1, "SAVE: storing again, the next free slot (2)");
    wtap(0);
    G->model.t[3].patch[QD_QP_HARM] = 900;
    btap(QD_B_SAVE);
    CHECK(G->ui.lb.armed && G->per.lib[0].patch[QD_QP_HARM] != 900, "W1 (a sound), SAVE: armed, not stored");
    black(0);
    CHECK(!G->ui.lb.armed, "another key: the arm cancelled");
    btap(QD_B_SAVE);
    btap(QD_B_SAVE);
    CHECK(G->per.lib[0].patch[QD_QP_HARM] == 900 && !strcmp(G->per.lib[0].name, nm), "SAVE, SAVE: replaced, the name "
          "kept");
    /* browsing: a W plays once; held, while held */
    black(8);                                           /* B9: the pool */
    black(1);                                           /* B2: sounds 17..32 */
    before = qd_pool_in_use(&G->pool);
    wkey(1, 1);                                         /* KICK (18) */
    p_run(30);
    CHECK(G->ui.lb.chosen == 17 && qd_pool_in_use(&G->pool) > before, "the POOL, B2, W2: KICK chosen and heard");
    wkey(1, 0);
    p_run(700);
    CHECK(!G->ui.lb.pv_on, ".. once (it stops)");
    wkey(1, 1);
    p_run(400);
    CHECK(G->ui.lb.aud && qd_view_screen(G) == QD_SCR_LIB, "W held: heard while held, on the screen");
    wkey(1, 0);
    p_run(20);
    CHECK(!G->ui.lb.aud && !G->ui.lb.pv_on, "let go: it stops");
    /* F2 in the pool: onto the selected track */
    btap(QD_B_OCTUP);
    CHECK(G->model.t[3].sound == 18 && !strcmp(G->model.t[3].sound_name, "KICK"), "F2 in the pool: KICK on track 4");
    /* F1 + W copy, then paste (a free slot: "KICK 2") */
    bt(QD_B_OCTDN, 1);
    p_run(20);
    wtap(1);
    black(9 - 9 + 2);                                   /* (B3: the bank of 33..48) */
    wtap(0);
    bt(QD_B_OCTDN, 0);
    p_run(20);
    CHECK(!strcmp(G->proj.sound[32].name, "KICK 2") && qd_view_screen(G) == QD_SCR_LIB, "F1 + W2, B3, W1: KICK copied "
          "into 33 (KICK 2), F1 let go: still the page");
    /* F2 + W: pick up, drop on a filled slot: a swap (the locks follow) */
    qd_step_on(&G->proj.pat[G->seq.cur], 0, 0);
    qd_step_set_sound(&G->proj.pat[G->seq.cur], 0, 0, 33);
    bt(QD_B_OCTUP, 1);
    p_run(20);
    wtap(0);                                            /* KICK 2 (33) picked up */
    black(1);
    wtap(2);                                            /* dropped on SNARE (19) */
    bt(QD_B_OCTUP, 0);
    p_run(20);
    CHECK(!strcmp(G->proj.sound[18].name, "KICK 2") && !strcmp(G->proj.sound[32].name, "SNARE") &&
              G->proj.pat[G->seq.cur].step[0][0].sound == 19 && G->model.t[3].sound == 18,
          "F2 + W1, B2, W3: KICK 2 and SNARE swapped, the step's lock follows (19), no F2 tap");
    /* B11 + W: delete; INIT stays */
    bkey(10, 1);
    p_run(20);
    wtap(2);                                            /* (19: KICK 2) */
    black(0);
    wtap(0);                                            /* (1: INIT) */
    bkey(10, 0);
    p_run(20);
    CHECK(!G->proj.sound[18].name[0] && G->proj.pat[G->seq.cur].step[0][0].sound == 0 && G->proj.sound[0].name[0],
          "B11 + W3: deleted, the lock back to the track's sound; B11 + W1: INIT stays");
    /* the name editor */
    black(1);
    wtap(1);                                            /* KICK (18) */
    black(9);                                           /* B10 */
    CHECK(qd_view_screen(G) == QD_SCR_NAME, "B10: the name editor");
    knob(QD_K_SELECT, 1);                               /* K -> L */
    knob(QD_K_ALGO, 1);
    black(10);                                          /* B11: the I deleted */
    btap(QD_B_OCTUP);
    CHECK(!strcmp(G->proj.sound[17].name, "LCK") && qd_view_screen(G) == QD_SCR_LIB, "SELECT +1, ALGORITHM, B11, F2: "
          "LCK kept (%s), back to the page", G->proj.sound[17].name);
    black(9);
    black(9);                                           /* the dice */
    btap(QD_B_OCTDN);
    CHECK(!strcmp(G->proj.sound[17].name, "LCK"), "B10, the dice, OCT-: the old name stays");
    btap(QD_B_OCTDN);
    CHECK(!G->ui.latch && qd_view_screen(G) != QD_SCR_LIB, "OCT- tapped: the page left");
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
        qd_store_put(&B.per.prj_st, QD_RKEY(QD_RK_HDR, 0, 0), b, (uint32_t)len - 1u);
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
        CHECK(!qd_pool_delete(&B, 1), "INIT cannot be deleted");
        CHECK(qd_pool_delete(&B, 31) && B.proj.pat[3].step[2][4].sound == 0 && !B.proj.sound[30].name[0] &&
                  B.proj.pat[20].snd[1].sound == 0,
              "pool delete: the locks back to the track's sound, the track keeps its copy");
        CHECK(qd_pool_copy(&B, 5, 31) && !strcmp(B.proj.sound[30].name, B.proj.sound[4].name), "pool copy 5 -> 31");
    }
    /* the library: store, copy, swap, delete, kept */
    {
        int16_t p[QD_QUAD_NP];
        qd_quad_preset_patch(10, p);
        CHECK(qd_lib_free_slot(&B) == 0 && qd_lib_store(&B, 0, "WOB BASS", p), "the library: the first free slot (1)");
        qd_lib_copy(&B, 0, 5);
        qd_lib_swap(&B, 5, 9);
        qd_lib_store(&B, 2, "GONE", p);
        qd_lib_delete(&B, 2);
        CHECK(qd_persist_flush(&B), "flushed");
        boot(&A);
        CHECK(!strcmp(A.per.lib[0].name, "WOB BASS") && !strcmp(A.per.lib[9].name, "WOB BASS") && !A.per.lib[5].name[0] &&
                  !A.per.lib[2].name[0] && A.per.lib[0].patch[QD_QP_DRIVE] == 10 && !strcmp(A.proj.sound[30].name,
                                                                                            A.proj.sound[4].name),
              "a restart: the library (copy, swap, delete) and the pool's changes kept");
    }
    /* the projects */
    CHECK(qd_proj_new(&A, 2) && A.per.proj == 2 && A.per.pexists[2] && !A.proj.pat[0].used &&
              !strcmp(A.per.pname[2], "PROJECT 3"), "new project 3: blank");
    qd_step_on(&A.proj.pat[0], 0, 0);
    A.proj.pat[0].used = 1;
    qd_proj_name(&A, 2, "MY SONG");
    CHECK(qd_proj_copy(&A, 2, 4) && A.per.pexists[4] && !strcmp(A.per.pname[4], "MY SONG"), "copy 3 -> 5");
    CHECK(qd_proj_load(&A, 4) && A.per.proj == 4 && qd_step_is_on(qd_step(&A.proj.pat[0], 0, 0)),
          "load 5: the copy has the edit");
    CHECK(qd_proj_delete(&A, 2) && !A.per.pexists[2], "delete 3");
    CHECK(qd_proj_load(&A, 0) && A.per.proj == 0 && !memcmp(&keep.pat[9], &A.proj.pat[9], sizeof keep.pat[9]),
          "load 1 again: as it was");
    qd_persist_flush(&A);
    boot(&B);
    CHECK(B.per.proj == 0 && B.per.pexists[4] && !B.per.pexists[2] && !strcmp(B.per.pname[4], "MY SONG"),
          "a restart: project 1 worked on, 5 kept, 3 gone");
    CHECK(A.per.prj_st.c.erases_noisy == 0 && B.per.prj_st.c.erases_noisy == 0, "no erase while sounding, ever");
    pages_test();
    printf("test_persist: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
