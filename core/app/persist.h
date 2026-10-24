/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
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
#include "../seq/sounds.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qd_app;

#define QD_NPROJ 8
#define QD_NLIB 256                    /* P1.5t: MY presets (was the LIBRARY's 128) */
#define QD_PROJ_NAME 13
#define QD_SAVE_DEBOUNCE_MS 1500u      /* §7: written a moment after the last edit */
#define QD_SAVE_LOOK_MS 50u            /* how often the app looks for changes */
#define QD_QUIET_MS 10000u             /* §7 rule 1: erases (GC, blank sectors ahead) after this long untouched, stopped,
                                        * nothing sounding (ChoralRoot's ST_AHEAD_IDLE_MS) */
#ifdef QD_DEVICE
/* P2.4: the unit's data region (0x97000..0xDFFFF: 73 sectors), from its first byte: the LIBRARY's 22 sectors (256 MY
 * PRESETS, a page each, and the factory tags: 257 pages of 300, two sectors kept blank), then the projects' 51
 * (765 pages, 10 kept blank for a full pattern's record: 615 for the eight projects, which keep their sounds by
 * reference; the owner's projects today take 51 pages, a full one of 64 dense patterns ~835: the store refuses what
 * does not fit, counted, and says so) */
#define QD_FLASH_LIB 0x000000u
#define QD_FLASH_LIB_SEC 22
#define QD_FLASH_LIB_RESERVE 2
#define QD_FLASH_PRJ (22u * 4096u)
#define QD_FLASH_PRJ_SEC 51
#else
#define QD_FLASH_LIB 0x000000u
#define QD_FLASH_LIB_SEC 32            /* (256 presets of about 120 bytes: a quarter of it) */
#define QD_FLASH_LIB_RESERVE 6
#define QD_FLASH_PRJ 0x020000u
#define QD_FLASH_PRJ_SEC 224
#endif
#define QD_SWAP_SEC 256                /* (P2.1: the host's RAM store for the patterns and sounds at the device's sizes) */
#define QD_SWAP_KEYS 1024
/* the keys: kind << 24 | project << 16 | index */
enum { QD_RK_SET = 1, QD_RK_HDR, QD_RK_PAT, QD_RK_SND, QD_RK_LIB, QD_RK_FTAG, QD_RK_MFX };   /* (P1.6c: MIDI FX) */   /* (P1.5v: the factory tags) */
#define QD_RKEY(kind, p, i) ((uint32_t)(kind) << 24 | (uint32_t)(p) << 16 | (uint32_t)(i))
/* 2026-10-24: the projects' format, the settings record's version (persist.c prj_format_ok): 2 since six tracks (1:
 * eight); another one at the mount: the projects' store erased, a fresh start (MY PRESETS, in the library's store,
 * stay) */
#define QD_PRJ_FORMAT 2

/* the things the app writes, each with a hash of its record as last written and as last seen, and when it changed */
enum { QD_PI_SET, QD_PI_HDR, QD_PI_PAT, QD_PI_SND = QD_PI_PAT + QD_NPAT, QD_PI_LIB = QD_PI_SND + QD_NSOUND,
       QD_PI_FTAG = QD_PI_LIB + QD_NLIB, QD_PI_MFX, QD_PI_N };

typedef struct {
    int on;                            /* a flash attached */
    qd_store_t lib_st, prj_st;
    uint32_t prj_wiped;                 /* 2026-10-24: the projects' store erased for its format (the mounts) */
    qds_ent_t lib_ent[QD_LIB_KEYS], prj_ent[QD_PRJ_KEYS];   /* P2.1: their indexes, each sized to what it keeps */
    qd_flash_t fl;
#if QD_SND_RAM
    qd_sound_dir_t ldir[QD_NLIB];      /* P2.1: MY presets' directory (the sounds in the store: qd_lib_read) .. */
    qd_sndc_t sc;                      /* .. the places in RAM the pool's and the library's sounds are read into */
#else
    qd_sound_t lib[QD_NLIB];           /* MY presets, the library's (name[0] 0: an empty place; .preset its own id) */
#endif
    uint8_t ftags[QD_NFACTORY];        /* P1.5v: the factory presets' tags as the owner set them (0x80 | tags; 0: the
                                        * classifier's) */
    uint16_t id_next;                  /* P1.5w: the next MY preset id (never one used before: a deleted preset's
                                        * references in other projects stay dangling until they load) */
    char pname[QD_NPROJ][QD_PROJ_NAME];   /* the projects' names */
    uint8_t pexists[QD_NPROJ];
    int proj;                          /* the project worked on, 0..7 */
    uint8_t pdemo;                     /* .. it started with the demo patterns (its patterns' defaults) */
    uint32_t saved[QD_PI_N], seen[QD_PI_N], changed[QD_PI_N];
    int scan;                          /* the round robin's place */
    int job;                           /* the item being written (-1 none) .. */
    uint32_t job_hash, job_drops;      /* .. its hash, the store's drops before it */
    int job_it;                        /* P2.1: the item the store's source makes (the job's, while it is written) */
    uint32_t written;                  /* records committed (the project page's bubbles) */
    int own_step;                      /* P2.4: the port calls qd_persist_step itself */
    uint32_t pat_raw[QD_NPAT];         /* P2.4: each pattern's bytes' fast hash at its last look (0: none) */
    uint16_t pat_gen[QD_NPAT], pat_wr[QD_NPAT];   /* P2.6c: each pattern's last look: edit_gen, written (low bits) .. */
    uint8_t pat_skip[QD_NPAT];         /* .. and the looks skipped since (a real one at least every 11th) */
    uint32_t snd_pf_ms, snd_pf_key;    /* P2.6c: the sound prefetch's last scan, what it read .. */
    int snd_pf_left;                   /* .. a sound it could not fetch yet */
    uint16_t it_gen[4], it_wr[4];      /* P2.6c: SET, HDR, FTAG, MFX's last looks (as pat_gen) */
    uint8_t it_skip[4];
    int maybe_dirty;                   /* P2.6c: an item's seen or saved hash set since a scan found all clean */
    int urgent;                        /* P2.4: 1 + the item a prefetch waits on to leave RAM (0 none): written next,
                                        * with no debounce */
    uint32_t fetch_ms;                 /* P2.4: the prefetch's last wait (it tries again 8 ms on) */
    uint32_t written_ms[QD_NPAT];      /* when each pattern was last written (its bubbles) */
#if QD_PAT_RAM < QD_NPAT && !defined(QD_TARGET_FM1)
    /* P2.1: with no flash attached, at the device's sizes on the host (the tests, the tools), the patterns out of RAM
     * go to a RAM store (the device always has its flash: none of this is in its build) */
    qd_store_t swap_st;
    qds_ent_t swap_ent[QD_SWAP_KEYS];
    uint8_t swap_nor[QD_SWAP_SEC * QDS_SECTOR];
    int swap_on;
#endif
} qd_persist_t;

/* the flash given (the glue's NOR, a test's RAM): mounted, the library, the settings, the project worked on (its
 * pattern and track selected); an erased flash: the first project, the demos (written by itself from then on) */
void qd_persist_attach(struct qd_app *a, const qd_flash_t *fl);
void qd_persist_ms(struct qd_app *a);  /* each ms: the looks, the writes (by §7's rules) */
/* P2.4: the port's own write steps: on, qd_persist_ms leaves the flash alone and the port calls qd_persist_step (a
 * UI frame's step of the stores, every 15 ms or as it can: the erases and writes where it masks the interrupts) */
void qd_persist_own_step(struct qd_app *a, int on);
void qd_persist_step(struct qd_app *a);
int qd_persist_flush(struct qd_app *a);   /* everything changed written now (a shutdown, a load); 1 done */
int qd_persist_dirty(const struct qd_app *a);   /* something is not written yet */
void qd_persist_io(struct qd_app *a);  /* P2.1: the patterns' and sounds' backing (pattern.h qd_pat_io_t, qd_snd_io_t):
                                        * the store */
void qd_persist_prefetch(struct qd_app *a);   /* P2.1: the patterns the sequencer will want read in (each ms) */
void qd_persist_use(const struct qd_app *a, uint32_t *live_pages, uint32_t *total_pages);   /* the flash meter */
uint32_t qd_persist_proj_pages(const struct qd_app *a, int p);   /* .. project p's part */
void qd_persist_proj_used(const struct qd_app *a, int p, uint8_t *used);   /* its patterns written (64) */

/* the projects: load (the one worked on written first), new (an empty slot: a blank project), copy, delete (the one
 * worked on: a blank project in its place), name. 1 done */
int qd_proj_load(struct qd_app *a, int p);
int qd_proj_new(struct qd_app *a, int p);
int qd_proj_copy(struct qd_app *a, int from, int to);
int qd_proj_delete(struct qd_app *a, int p);
void qd_proj_name(struct qd_app *a, int p, const char *name);

/* the POOL (the project's, slots 1..128; P1.5t: a new project's starts empty, slot 1 is no different): the first free
 * slot (0 none); add (into the first free: its number, 0 full; add_sound keeps the preset it came from); move / swap a
 * and b (every step's sound lock and every track's source in all 64 patterns follow); copy a onto b; delete (the steps
 * locking it go back to their track's sound; the tracks keep their copy); its users (the selected pattern's tracks and
 * steps; P1.5t: users_all, every pattern's steps) */
int qd_pool_free_slot(const qd_project_t *pr);
int qd_pool_add(struct qd_app *a, const char *name, const int16_t *patch);
int qd_pool_add_sound(struct qd_app *a, const qd_sound_t *s);
int qd_pool_find(const struct qd_app *a, const qd_sound_t *s);   /* P1.5t: the pool sound that is s (its preset, or its
                                                                  * name and patch), 0 none */
uint64_t qd_pool_unref_pending(const struct qd_app *a);   /* (2026-10-24) the patterns whose records still lock a
                                         * sound taken out of the pool (bit p; 0 none) */
void qd_pool_sweep_all(struct qd_app *a);   /* (2026-10-24) every pattern still holding a removed sound's lock: read in
                                         * and cleared now (an undo, a flush) */
int qd_pool_users_all(const struct qd_app *a, int k, uint8_t *tracks);   /* steps locking it in all patterns (tracks:
                                                                         * the tracks playing it or locking it) */
int qd_pool_swap(struct qd_app *a, int x, int y);
int qd_pool_copy(struct qd_app *a, int from, int to);
int qd_pool_delete(struct qd_app *a, int k);
int qd_pool_users(const struct qd_app *a, int k, uint8_t *tracks, int *steps);   /* tracks: bit t; 1 used */

/* MY PRESETS (the library's, places 0..255; P1.5t): store a new preset (its name, a new id, its kind read; flags
 * QD_SNDF_*), replace one (its id kept), delete; the first free place (-1 none); a place by its preset id (-1 none); a
 * preset by its id (factory or mine: 1 found) */
int qd_lib_free_slot(const struct qd_app *a);
int qd_factory_tags(const struct qd_app *a, int k);   /* (P2.1b) its tags alone, as qd_factory_get's */
void qd_factory_get(const struct qd_app *a, int k, qd_sound_t *out);   /* P1.5v: factory preset k with its tags */
int qd_tags_set(struct qd_app *a, int code, int tags);   /* P1.5v: an entry's tags (the browser's code: factory k,
                                                           * QD_BR_MINE_CODE + place, QD_BR_POOL_CODE + number); 1 done */
int qd_lib_store(struct qd_app *a, int slot, const char *name, const int16_t *patch);
int qd_lib_store_ex(struct qd_app *a, int slot, const char *name, const int16_t *patch, int flags);
int qd_lib_replace(struct qd_app *a, int slot, const char *name, const int16_t *patch);
int qd_lib_delete(struct qd_app *a, int slot);
int qd_lib_find(const struct qd_app *a, int id);
/* P2.1: MY preset at place i read and written whole (every change through these: at the device's sizes the sounds
 * are in the store; an empty place reads as zeros); its name, id, tags, flags (the directory's, no reading) */
void qd_lib_read(const struct qd_app *a, int i, qd_sound_t *out);
void qd_lib_write(struct qd_app *a, int i, const qd_sound_t *s);
int qd_lib_used(const struct qd_app *a, int i);
const char *qd_lib_name(const struct qd_app *a, int i);
uint16_t qd_lib_preset(const struct qd_app *a, int i);
int qd_lib_tags(const struct qd_app *a, int i);
int qd_lib_flags(const struct qd_app *a, int i);
/* ---- P1.5w: THE POOL HOLDS REFERENCES (PLAN.md P1.5w): a pool slot is a preset id (factory 1..23, MINE 0x100..);
 * the slot (qd_sound_read) keeps the preset's name, sound and tags as a cache (what plays; resolved from the library),
 * so a REPLACE plays everywhere. A slot of a pool that could not become a reference (the library full) stays a copy
 * (preset 0) until it can. */
void qd_pool_resolve(struct qd_app *a);          /* the current pool's cache from the library (after a library change) */
void qd_preset_usage(const struct qd_app *a, int id, int *projects, int *steps);   /* the projects whose pools
                                                   * reference it, the steps locking it there (this one's from RAM,
                                                   * the others' from the flash) */
int qd_preset_delete(struct qd_app *a, int slot);   /* one of MY presets out, with its references in this project's
                                                   * pool (their steps back to their track's sound); one undo level.
                                                   * Other projects drop theirs when they load */
void qd_lib_unique_name(const struct qd_app *a, const char *name, char *out);   /* a name no MY preset has ("X 2") */
int qd_preset_get(const struct qd_app *a, int id, qd_sound_t *out);
/* P2.1 (the 'P' 5 records: track sounds by reference): before MY preset id changes or goes, every pattern record
 * referring to it (every project's) is written again with the track's own copy. persist.c calls it itself (REPLACE, a
 * store over one, a delete); undo.c before it changes MY presets back */
void qd_preset_will_change(struct qd_app *a, int id);
extern int qd_persist_write_v4;        /* (the tests: pattern records as 'P' 4, by copy) */
/* P2.1: every pattern record of an older format, of every project, written again as 'P' 5 (each project loaded in
 * turn; the one worked on again after); the records written. Not called by itself: older records read as they are */
int qd_persist_migrate(struct qd_app *a);

#ifdef __cplusplus
}
#endif
