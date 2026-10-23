/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* Copy, paste and undo (PLAN §8b): a step clipboard and a page clipboard (copies may go between tracks), and one level
 * of undo per track covering clears, pastes and cuts.
 *
 *   step     copy: the step with its locks; paste: onto any step of any track (each paste again: the same step);
 *            cut: copy, then clear the step
 *   page     copy: the page's 16 steps, their locks, its start and end; paste: onto any page of any track, the start
 *            and end with them, and the page joins the loop (Form's TOOLS paste does the same)
 *   undo     before a clear, a paste or a cut the track is kept (its steps, locks and pages: their ranges and the
 *            loop, with the pattern it belongs to); undo swaps them with the track as it is, so undo again is a redo
 *            (Form's); the track's defaults, rate, mute and key are not undone. An undo for another pattern than the
 *            one edited does nothing. A paste that does not fit (QD_EFULL) leaves the earlier undo level.
 *   group    (P1.3c) between qd_clip_group(c, 1) and qd_clip_group(c, 0) the edits are ONE undo level: the first edit
 *            of a track keeps it, the later ones of the same track and pattern do not (the controller groups an F1 or
 *            F2 hold: a cut and its paste, a page pasted twice, undo as one). An undo ends the group's level (the
 *            next edit keeps a new one).
 * Plain C over pattern.h's raw step access; nothing of the OS. */
#pragma once
#include <stdint.h>

#include "pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int valid;
    qd_step_t step;
    qd_lock_t lk[QD_LK_N];
    int nlk;
} qd_clip_step_t;

typedef struct {
    int valid;
    qd_step_t step[QD_PAGE];
    qd_lock_t lk[QD_NLOCK];
    int nlk;
    uint8_t start, end;
} qd_clip_page_t;

typedef struct {
    int valid;                          /* a level is kept */
    int pat;                            /* the pattern it belongs to */
    qd_step_t step[QD_NSTEP];
    qd_tset_t ts;
    qd_lock_t lk[QD_NLOCK];
    int nlk;
} qd_undo_t;

/* P2.1: at the device's sizes (QD_CLIP_ZBYTES) the tracks' levels are kept compressed (seq/zr.h), one after another
 * in QD_CLIP_ZBYTES; a level that finds no room takes it from the oldest of the other tracks' (counted: zdropped) */
typedef struct {
    qd_clip_step_t st;
    qd_clip_page_t pg;
#ifdef QD_CLIP_ZBYTES
    uint8_t z[QD_CLIP_ZBYTES];
    uint16_t zoff[QD_PAT_NTRACK], zlen[QD_PAT_NTRACK];   /* (zlen 0: no level) */
    int16_t zpat[QD_PAT_NTRACK];
    uint32_t zage[QD_PAT_NTRACK], zclock, zdropped;
#else
    qd_undo_t undo[QD_PAT_NTRACK];
#endif
    int group;                          /* grouping on (qd_clip_group) */
    int group_t, group_k;               /* .. the track and pattern whose level it kept (group_t -1: none yet) */
} qd_clip_t;

void qd_clip_init(qd_clip_t *c);
/* the undo level of track t of pattern k (pattern p) kept as it is now */
void qd_clip_keep(qd_clip_t *c, const qd_pattern_t *p, int k, int t);
/* undo / redo track t of pattern k: 1 done, 0 nothing to undo (QD_EFULL: the locks did not fit, nothing changed) */
int qd_clip_undo(qd_clip_t *c, qd_pattern_t *p, int k, int t);
int qd_clip_can_undo(const qd_clip_t *c, int k, int t);
/* the edits until qd_clip_group(c, 0) are one undo level */
void qd_clip_group(qd_clip_t *c, int on);

int qd_clip_copy_step(qd_clip_t *c, const qd_pattern_t *p, int t, int s);
/* each keeps the undo level first (pattern index k); QD_EARG: the clipboard is empty; QD_EFULL: no room for locks */
int qd_clip_paste_step(qd_clip_t *c, qd_pattern_t *p, int k, int t, int s);
int qd_clip_cut_step(qd_clip_t *c, qd_pattern_t *p, int k, int t, int s);
int qd_clip_copy_page(qd_clip_t *c, const qd_pattern_t *p, int t, int page);
int qd_clip_paste_page(qd_clip_t *c, qd_pattern_t *p, int k, int t, int page);
int qd_clip_clear_page(qd_clip_t *c, qd_pattern_t *p, int k, int t, int page);
int qd_clip_clear_track(qd_clip_t *c, qd_pattern_t *p, int k, int t);
int qd_clip_clear_steps(qd_clip_t *c, qd_pattern_t *p, int k, int t, const int *steps, int n);

#ifdef __cplusplus
}
#endif
