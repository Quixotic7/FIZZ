/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* T1 (docs/TRACK-TOOLS-PLAN.md §3, §4): the track tools' operations on a pattern, pure functions (built through
 * pattern.c). Every op walks a POSITION LIST, pos[i] -> the physical step (0..127) of track t, n of them, whatever the
 * scope:
 *   SELECTED the step latch's selection (the caller makes that list: ctl_tools.c)
 *   PAGE     the page's start..end
 *   TRACK    the track's looped steps in play order (each looped page's start..end; steps outside the loop untouched)
 *   PATTERN  every track on its own loop (the caller walks the tracks)
 * ROTATE and REVERSE permute the steps in place (cycle-following, one step's temporary) and renumber the track's locks
 * through the same map, then re-sort its lock range (insertion sort): the pool's count is kept, so QD_EFULL cannot
 * happen. REVERSE also mirrors the micro-timing (an overridden micro becomes -micro). EUCLID's hit test is the
 * Bresenham form, ((i + rot) * hits) % len < hits (Bjorklund's spacing up to rotation): generated steps are qd_step_on's
 * (no overrides), removed steps go with their locks. LENGTH doubles (the looped pages copied after themselves: SHIFT +
 * B9's double) or halves (the later half of the looped pages leaves the loop; one page: its end halved; nothing is
 * deleted), PAGE scope the page's end. */
#pragma once
#include <stdint.h>

#include "pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { QD_TL_ROTATE, QD_TL_REVERSE, QD_TL_RANDOM, QD_TL_EUCLID, QD_TL_GRIDS, QD_TL_HUMANIZE, QD_TL_LENGTH, QD_TL_N };
enum { QD_TS_SEL, QD_TS_PAGE, QD_TS_TRACK, QD_TS_PATTERN, QD_TS_N };   /* (SEL: the selected steps, the caller's list) */
enum { QD_EU_NEW, QD_EU_ADD, QD_EU_MASK, QD_EU_N };    /* EUCLID's landing: replace, OR, AND */
#define QD_TL_BUILT(tool) ((tool) == QD_TL_ROTATE || (tool) == QD_TL_REVERSE || (tool) == QD_TL_EUCLID || \
                           (tool) == QD_TL_LENGTH)       /* (T1's; RANDOM, GRIDS, HUMANIZE are T2's) */

/* the scope's positions of track t (PAGE: page `page`'s start..end; TRACK / PATTERN: the loop): n (1..128); not SEL */
int qd_tl_positions(const qd_pattern_t *p, int t, int scope, int page, uint8_t *pos);
/* the position of step s in a list (-1: not in it) */
int qd_tl_pos_of(const uint8_t *pos, int n, int s);
/* step pos[i] takes the step that was at pos[(i - k) mod n] (k: any sign), its locks with it */
void qd_tl_rotate(qd_pattern_t *p, int t, const uint8_t *pos, int n, int k);
/* step pos[i] takes pos[n - 1 - i]'s; an overridden micro is negated */
void qd_tl_reverse(qd_pattern_t *p, int t, const uint8_t *pos, int n);
int qd_tl_euclid_hit(int i, int hits, int len, int rot);   /* position i a hit (the pattern repeating after len) */
void qd_tl_euclid(qd_pattern_t *p, int t, const uint8_t *pos, int n, int hits, int len, int rot, int mode);
/* the track doubled (SHIFT + B9's): 0 no room (8 pages); only != 0: its settings only, into *only (nothing copied) */
int qd_tl_double(qd_pattern_t *p, int t, qd_tset_t *only);
/* LENGTH: pw doublings (> 0) or halvings (< 0) of track t (page -1) or of page `page`'s end; only: as qd_tl_double */
void qd_tl_length(qd_pattern_t *p, int t, int page, int pw, qd_tset_t *only);

#ifdef __cplusplus
}
#endif
