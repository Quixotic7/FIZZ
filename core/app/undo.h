/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.5c (PLAN version 2.6): undo / redo (SHIFT + F1 / SHIFT + F2), eight levels; the snapshot of the active pattern;
 * the pattern clipboard (SHIFT + B4 / B5).
 *
 * ONE GESTURE ONE LEVEL. Any input opens a gesture (if none is open): the regions an edit can touch are copied as they
 * were (the active pattern, with its track sounds and settings; the project's pool; the library). The gesture closes
 * when nothing is held and no input came for QD_UNDO_QUIET_MS (a knob turn, a W held and edited, a recorded take
 * all end that way), or at once before an undo / redo. Closing compares the regions with their copies: no change, no
 * level; a change is stored as its XOR difference, run-length coded (a byte offset, a length, the bytes), usually a few
 * dozen to a few hundred bytes. Applying a difference turns the state into the other one, both ways: undo and redo
 * are the same operation on the same bytes, in opposite orders through the list. An operation (a paste, a clear, a
 * snapshot recall, a randomise, a pool or library change, double) closes the gesture before it: a level of its own,
 * even with SHIFT still held for the next one.
 *
 * P1.5w: a pool slot emptied (- POOL, a MY preset deleted) clears its locks in every pattern: those outside the
 * gesture's pattern are kept as a list (pattern, track, step, the pool number, the override bit), up to QD_UNDO_XLOCKS,
 * stored as a fourth region of the level and XORed back the same way (a bigger clear: the level is forgotten, as a
 * difference too big for the pool).
 *
 * COST: the copies are held once (the gesture's "before": a pattern 26 KB, the pool 21 KB, the library 21 KB; RAM
 * that Phase 2 will have to find, or narrow to the pattern), the levels' differences share a QD_UNDO_POOL byte pool;
 * when it is full the oldest levels go. A project load or a new project forgets them all. */
#pragma once
#include <stdint.h>

#include "../seq/pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_UNDO_LEVELS 8
#define QD_UNDO_POOL 65536u
#define QD_UNDO_QUIET_MS 500u
#define QD_NLIB_UNDO 256               /* (persist.h's QD_NLIB) */
#define QD_UNDO_XLOCKS 2048            /* P1.5w: the locks of the other patterns a pool slot's emptying cleared (10 KB) */

/* what a level was (its glyph on the takeover) */
enum { QD_UK_EDIT, QD_UK_KNOB, QD_UK_PASTE, QD_UK_CLEAR, QD_UK_REC, QD_UK_SOUNDS, QD_UK_RANDOM, QD_UK_SNAP,
       QD_UK_DOUBLE, QD_NUK };

typedef struct {
    uint8_t kind;
    uint8_t pat;                       /* the pattern it changed */
    uint32_t off, len;                 /* its difference in the pool */
} qd_undo_lvl_t;

typedef struct {
    /* the gesture open */
    int open, hint, knob;
    int join;                          /* P1.5r: an operation of the open gesture's kind joins it (an import batch) */
    uint32_t last_ms;
    int pat;
    qd_pattern_t pat0;
    qd_sound_t snd0[QD_NSOUND];
    uint8_t nsound0;
    qd_sound_t lib0[QD_NLIB_UNDO];
    uint8_t xl[QD_UNDO_XLOCKS * 5];    /* P1.5w: the other patterns' locks cleared (pattern, track, step, number, ovr) */
    uint32_t xl_n;
    int xl_over;
    /* the levels: lvl[0 .. n), pos of them applied (undo: pos - 1; redo: pos) */
    qd_undo_lvl_t lvl[QD_UNDO_LEVELS];
    int n, pos;
    uint8_t pool[QD_UNDO_POOL];
    uint32_t used;
    /* the snapshot of the active pattern (SHIFT + B1 / B3) and the pattern clipboard (SHIFT + B4 / B5); RAM only */
    qd_pattern_t snap, pclip;
    int snap_ok, pclip_ok;
} qd_hist_t;

struct qd_app;
void qd_undo_init(struct qd_app *a);
void qd_undo_reset(struct qd_app *a);          /* (a project loaded: the levels forgotten) */
void qd_undo_xlock(struct qd_app *a, int pat, int t, int s, int k, int ovr);   /* P1.5w: a lock cleared outside the
                                                   * gesture's pattern (kept for its undo) */
void qd_undo_input(struct qd_app *a, int knob);   /* any input (knob: it was a knob) */
void qd_undo_ms(struct qd_app *a);             /* the gesture closes when quiet */
void qd_undo_close(struct qd_app *a);
void qd_undo_hint(struct qd_app *a, int kind);   /* what the open gesture is (the first hint wins) */
int qd_undo_step(struct qd_app *a, int dir);   /* -1 undo, +1 redo: the level's kind, -1 none */
int qd_undo_count(const struct qd_app *a, int dir);   /* levels there to undo (-1) / redo (+1) */

#ifdef __cplusplus
}
#endif
