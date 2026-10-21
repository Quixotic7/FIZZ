/* SPDX-License-Identifier: GPL-3.0-only */
/* Saving (P1.5a; PLAN.md "P1.5a, saving, the library and the pool" and §7): the project written by itself into the
 * flash, eight projects, the user LIBRARY of 128 sounds, the project's POOL of 128. persist.c (built through app.c).
 *
 * THE FLASH (1 MiB, the core's image: FM1VST keeps it between launches): two floating page stores (core/store):
 *   0x000000 .. 0x01FFFF   the LIBRARY's (32 sectors): a record a sound
 *   0x020000 .. 0x0FFFFF   the projects' (224 sectors): the global settings (the current project, its pattern, the
 *                          selected track, the knob layout, the effects' parameters ..), each project's header (its
 *                          name, its mix, its record settings, the global key), its patterns (sparse, each with its
 *                          eight track sounds) and its pool's sounds
 * A record absent is its default: a pattern as a new project has it (blank, or the demos for the first project), a
 * pool slot as the starting pool (INIT, the factory sounds, the drums); a deleted pool slot is a record that says so.
 *
 * WRITTEN BY ITSELF: every 50 ms the app looks at what may have changed (the settings, the project's header, the
 * pattern being edited and one more of the 64 in turn, a few pool and library slots in turn): a record whose bytes
 * changed, unchanged for QD_SAVE_DEBOUNCE_MS since, and not as it was last written, is queued; the store writes it
 * by §7's rules (one page a UI frame while playing, no erase unless quiet). qd_persist_flush writes all of it now.
 * Without a flash attached (the tests, the renders) nothing is written. */
#pragma once
#include <stdint.h>

#include "../store/store.h"
#include "../seq/pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qd_app;

#define QD_NPROJ 8
#define QD_NLIB 128
#define QD_PROJ_NAME 12
#define QD_SAVE_DEBOUNCE_MS 1500u      /* §7: written a moment after the last edit */
#define QD_SAVE_LOOK_MS 50u            /* how often the app looks for changes */
#define QD_QUIET_MS 10000u             /* §7 rule 1: erases (GC, blank sectors ahead) after this long untouched, stopped,
                                        * nothing sounding (ChoralRoot's ST_AHEAD_IDLE_MS) */
#define QD_FLASH_LIB 0x000000u
#define QD_FLASH_LIB_SEC 32
#define QD_FLASH_PRJ 0x020000u
#define QD_FLASH_PRJ_SEC 224
/* the keys: kind << 24 | project << 16 | index */
enum { QD_RK_SET = 1, QD_RK_HDR, QD_RK_PAT, QD_RK_SND, QD_RK_LIB };
#define QD_RKEY(kind, p, i) ((uint32_t)(kind) << 24 | (uint32_t)(p) << 16 | (uint32_t)(i))

/* the things the app writes, each with a hash of its record as last written and as last seen, and when it changed */
enum { QD_PI_SET, QD_PI_HDR, QD_PI_PAT, QD_PI_SND = QD_PI_PAT + QD_NPAT, QD_PI_LIB = QD_PI_SND + QD_NSOUND,
       QD_PI_N = QD_PI_LIB + QD_NLIB };

typedef struct {
    int on;                            /* a flash attached */
    qd_store_t lib_st, prj_st;
    qd_flash_t fl;
    qd_sound_t lib[QD_NLIB];           /* the LIBRARY (name[0] 0: an empty slot) */
    char pname[QD_NPROJ][QD_PROJ_NAME];   /* the projects' names */
    uint8_t pexists[QD_NPROJ];
    int proj;                          /* the project worked on, 0..7 */
    uint8_t pdemo;                     /* .. it started with the demo patterns (its patterns' defaults) */
    uint32_t saved[QD_PI_N], seen[QD_PI_N], changed[QD_PI_N];
    int scan;                          /* the round robin's place */
    int job;                           /* the item being written (-1 none) .. */
    uint32_t job_hash, job_drops;      /* .. its hash, the store's drops before it */
    uint32_t written;                  /* records committed (the project page's bubbles) */
    uint32_t written_ms[QD_NPAT];      /* when each pattern was last written (its bubbles) */
} qd_persist_t;

/* the flash given (the glue's NOR, a test's RAM): mounted, the library, the settings, the project worked on (its
 * pattern and track selected); an erased flash: the first project, the demos (written by itself from then on) */
void qd_persist_attach(struct qd_app *a, const qd_flash_t *fl);
void qd_persist_ms(struct qd_app *a);  /* each ms: the looks, the writes (by §7's rules) */
int qd_persist_flush(struct qd_app *a);   /* everything changed written now (a shutdown, a load); 1 done */
int qd_persist_dirty(const struct qd_app *a);   /* something is not written yet */
void qd_persist_use(const struct qd_app *a, uint32_t *live_pages, uint32_t *total_pages);   /* the flash meter */

/* the projects: load (the one worked on written first), new (an empty slot: a blank project), copy, delete (the one
 * worked on: a blank project in its place), name. 1 done */
int qd_proj_load(struct qd_app *a, int p);
int qd_proj_new(struct qd_app *a, int p);
int qd_proj_copy(struct qd_app *a, int from, int to);
int qd_proj_delete(struct qd_app *a, int p);
void qd_proj_name(struct qd_app *a, int p, const char *name);

/* the POOL (the project's, slots 1..128; INIT, slot 1, stays): the first free slot (0 none); add (into the first free:
 * its number, 0 full); move / swap a and b (every step's sound lock and every track's source in all 64 patterns
 * follow); copy a onto b; delete (the steps locking it go back to their track's sound; the tracks keep their copy) */
int qd_pool_free_slot(const qd_project_t *pr);
int qd_pool_add(struct qd_app *a, const char *name, const int16_t *patch);
int qd_pool_swap(struct qd_app *a, int x, int y);
int qd_pool_copy(struct qd_app *a, int from, int to);
int qd_pool_delete(struct qd_app *a, int k);
int qd_pool_users(const struct qd_app *a, int k, uint8_t *tracks, int *steps);   /* tracks: bit t; 1 used */

/* the LIBRARY (slots 0..127): store a sound (its name), copy, swap, delete; the first free slot (-1 none) */
int qd_lib_free_slot(const struct qd_app *a);
int qd_lib_store(struct qd_app *a, int slot, const char *name, const int16_t *patch);
int qd_lib_copy(struct qd_app *a, int from, int to);
int qd_lib_swap(struct qd_app *a, int x, int y);
int qd_lib_delete(struct qd_app *a, int slot);

#ifdef __cplusplus
}
#endif
