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
    boot(&B);
    for (k = 0, ok = 0; k < QD_NPAT; k++)
        ok += !memcmp(&keep.pat[k], &B.proj.pat[k], sizeof keep.pat[k]);
    CHECK(ok == QD_NPAT, "a restart: the 64 patterns as they were (%d same)", ok);
    CHECK(!memcmp(keep.sound, B.proj.sound, sizeof keep.sound) && B.proj.nsound == keep.nsound,
          "the pool's 128 sounds (DRIVE among them)");
    CHECK(B.model.t[3].mix.level == 77 && B.model.t[3].mix.pan == -20 && B.proj.rec_quant == 35 &&
              qd_fx_get(QD_FXP_RSIZE) == 33 && B.ui.knobs == QD_KNOBS_C,
          "the mix, the record settings, the effects, the knob layout");
    CHECK(B.per.proj == 0 && B.seq.cur == 12 && B.sel == 5, "the restart check: the project, its pattern (A13) and "
          "the selected track (6)");

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
    printf("test_persist: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
