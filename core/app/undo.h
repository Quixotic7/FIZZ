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
 * difference too big for the pool). P1.6c: the project's MIDI FX groups are a region too (2.3 KB).
 *
 * COST: the copies are held once (the gesture's "before": a pattern 26 KB, the pool 21 KB, the library 21 KB; RAM
 * that Phase 2 will have to find, or narrow to the pattern), the levels' differences share a QD_UNDO_POOL byte pool;
 * when it is full the oldest levels go. A project load or a new project forgets them all.
 *
 * P2.1, AT THE DEVICE'S SIZES (QD_UNDO_SLOTS): a 16 KB log, no whole copies. The gesture's pattern as it was is kept
 * compressed (its bytes, zero runs squeezed: exact) at the log's top, under the levels; the pool and library slots a
 * gesture changes are copied by the code that changes them, before it does (qd_undo_snd: at most QD_UNDO_NSLOT a
 * gesture) and stored in the level as their XOR with what they became; the MIDI FX groups are copied as before. A
 * gesture the log cannot hold (its pattern too big to keep with the levels, more slots) is no level (as a difference
 * too big always was). The snapshot and the pattern clipboard are kept compressed (P2.1c: in the log; a pattern that
 * does not fit is not taken: its confirm says so). The host keeps Phase 1's whole copies: the same levels.
 *
 * P2.1c, THE ONE LOG (the device's): the 16 KB holds everything, from the top down: the snapshot and the pattern
 * clipboard (compressed: keep bytes, the clipboard's last), the open gesture's copies (its pattern compressed, the
 * MIDI FX groups, then its slots' and other patterns' locks' records as they come: the region pool[glo .. gend)),
 * and from the bottom up the levels. Whatever needs room takes it from the oldest levels; what still does not fit
 * is not taken (a snapshot, a copy: "too big"; a gesture: no level). */
#pragma once
#include <stdint.h>

#include "../seq/pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_UNDO_LEVELS 8
#define QD_UNDO_QUIET_MS 500u
#define QD_NLIB_UNDO 256               /* (persist.h's QD_NLIB) */
/* (QD_UNDO_POOL, QD_UNDO_XLOCKS: core/qd_config.h, P2.0) */
#if defined(QD_UNDO_NSLOT)
#define QD_UNDO_SLOTS 1                /* P2.1: the device's log (above) */
#endif
enum { QD_UNDO_RPOOL = 1, QD_UNDO_RLIB = 2 };   /* (qd_undo_snd's regions) */

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
#ifdef QD_UNDO_SLOTS
    uint32_t keep;                     /* P2.1c: the snapshot's and the clipboard's bytes: pool[POOL - keep .. POOL) */
    uint32_t glo, gpz;                 /* .. the gesture's region pool[glo .. POOL - keep) (none: glo = POOL - keep);
                                        * its pattern as it was, compressed, gpz bytes at its top */
    int b_ok;                          /* .. kept (else the gesture is no level) */
    int nslot;                         /* .. the slots changed: region, slot (their copies before: in the region) */
    uint8_t slot_r[QD_UNDO_NSLOT], slot_k[QD_UNDO_NSLOT];
#else
    qd_pattern_t pat0;
    qd_sound_t snd0[QD_NSOUND];
    qd_sound_t lib0[QD_NLIB_UNDO];
#endif
    uint8_t nsound0;
#ifndef QD_UNDO_SLOTS
    qd_mfx_group_t mfx0[QD_MFX_NGROUP];   /* P1.6c: the MIDI FX groups (a fourth region) */
    uint8_t xl[QD_UNDO_XLOCKS * 5];    /* P1.5w: the other patterns' locks cleared (pattern, track, step, number, ovr) */
#endif
    uint32_t xl_n;
    int xl_over;
    /* the levels: lvl[0 .. n), pos of them applied (undo: pos - 1; redo: pos) */
    qd_undo_lvl_t lvl[QD_UNDO_LEVELS];
    int n, pos;
    uint8_t pool[QD_UNDO_POOL];
    uint32_t used;
    /* the snapshot of the active pattern (SHIFT + B1 / B3) and the pattern clipboard (SHIFT + B4 / B5); RAM only */
#ifdef QD_UNDO_SLOTS
    uint32_t snap_len, pclip_len;      /* P2.1c: in the log's top (keep): the snapshot's bytes, then the clipboard's */
#else
    qd_pattern_t snap, pclip;
#endif
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
/* P2.1: a pool slot (QD_UNDO_RPOOL, 1..128) or a library slot (QD_UNDO_RLIB, 0..255) about to change: kept as it was
 * for the open gesture (the device's log; the host: nothing, its whole copies) */
void qd_undo_snd(struct qd_app *a, int region, int slot);
/* P2.1: the snapshot (0) or the pattern clipboard (1): the active pattern stored (0: no room, the device's), put back
 * onto the active pattern (0: none) */
int qd_hist_store(struct qd_app *a, int which);
int qd_hist_recall(struct qd_app *a, int which);

#ifdef __cplusplus
}
#endif
