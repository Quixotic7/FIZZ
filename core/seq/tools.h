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

/* (2026-10-25, the owner's layout: B1..B8 the eight tools below, STRETCH and CLIP after them on PRESETS only) */
enum { QD_TL_ROTATE, QD_TL_REVERSE, QD_TL_RANDOM, QD_TL_EUCLID, QD_TL_GRIDS, QD_TL_HUMANIZE, QD_TL_QUANTIZE, QD_TL_LENGTH,
       QD_TL_STRETCH, QD_TL_CLIP, QD_TL_N };
#define QD_TL_NKEY 8                    /* the tools on B1..B8 */
enum { QD_CL_COPY, QD_CL_PASTE, QD_CL_CLEAR, QD_CL_N };   /* CLIP's actions (KNOB 1 picks, OCT+ does it) */
enum { QD_QG_1, QD_QG_2, QD_QG_3, QD_QG_4, QD_QG_N };  /* QUANTIZE's grid: a step, 1/2, 1/3, 1/4 of one */
enum { QD_TS_SEL, QD_TS_PAGE, QD_TS_TRACK, QD_TS_PATTERN, QD_TS_N };   /* (SEL: the selected steps, the caller's list) */
enum { QD_EU_NEW, QD_EU_ADD, QD_EU_MASK, QD_EU_N };    /* EUCLID's landing: replace, OR, AND */
#define QD_TL_BUILT(tool) ((tool) >= 0 && (tool) < QD_TL_N)   /* (T2: every one) */

/* the scope's positions of track t (PAGE: page `page`'s start..end; TRACK / PATTERN: the loop): n (1..128); not SEL */
int qd_tl_positions(const qd_pattern_t *p, int t, int scope, int page, uint8_t *pos);
/* the position of step s in a list (-1: not in it) */
int qd_tl_pos_of(const uint8_t *pos, int n, int s);
/* step pos[i] takes the step that was at pos[(i - k) mod n] (k: any sign), its locks with it */
void qd_tl_rotate(qd_pattern_t *p, int t, const uint8_t *pos, int n, int k);
/* step pos[i] takes pos[n - 1 - i]'s; an overridden micro is negated */
void qd_tl_reverse(qd_pattern_t *p, int t, const uint8_t *pos, int n);
/* ROTATE over SELECTED STEPS (the owner, 2026-10-24: a step selected and rotated moves): the selected steps sel[0..ns)
 * go k places on round the list pos (the track's loop), the others fill the places left in their order, their locks
 * with them. qd_tl_move_map: where each position's step comes from (from[0..n)), 0 if none of sel is in the list */
int qd_tl_move_map(const uint8_t *pos, int n, const uint8_t *sel, int ns, int k, uint8_t *from);
void qd_tl_move(qd_pattern_t *p, int t, const uint8_t *pos, int n, const uint8_t *sel, int ns, int k);
int qd_tl_euclid_hit(int i, int hits, int len, int rot);   /* position i a hit (the pattern repeating after len) */
void qd_tl_euclid(qd_pattern_t *p, int t, const uint8_t *pos, int n, int hits, int len, int rot, int mode);
/* T2 (§4.3, §4.5, §4.6): the generators, pure functions of a seed: h = qd_tl_hash(seed, track, position) */
uint32_t qd_tl_hash(uint32_t seed, int t, int i);
/* RANDOM: position i a trig when (h & 0xFF) < dens % of 256 (dens 0..100); the trigs' note 1 the track's default moved
 * by -notes..notes degrees of (scale, root) (0: kept), the velocity the default's +- vel (0: kept), the chance 100 -
 * chance x (h >> 24) / 255 % (0: kept) */
int qd_tl_random_on(uint32_t seed, int t, int i, int dens);
void qd_tl_random(qd_pattern_t *p, int t, const uint8_t *pos, int n, uint32_t seed, int dens, int notes, int vel,
                  int chance, int scale, int root);
/* HUMANIZE: every trig's micro +- micro 24ths and velocity +- vel (0: kept), from what they are */
void qd_tl_humanize(qd_pattern_t *p, int t, const uint8_t *pos, int n, uint32_t seed, int micro, int vel);
/* .. one trig's (h: qd_tl_hash(seed, t, i); *m, *v its micro and velocity now, in and out) */
void qd_tl_humanize_one(uint32_t h, int micro, int vel, int *m, int *v);
/* QUANTIZE (HUMANIZE's opposite): every trig's own micro (an override; the track's default micro is the track's nudge,
 * left alone) amt % of the way to the nearest line of the grid (QD_QG_*: 24ths 24, 12, 8, 6; the step's own lines
 * only, |line| <= 23, so a note never changes step; a tie to the step's), and its own velocity vamt % of the way to the
 * track's default; a value that lands on the track's default drops its override. Pure: qd_tl_quant_to is the micro's */
int qd_tl_quant_to(int m, int grid, int amt);
void qd_tl_quantize_one(const qd_step_t *st, const qd_sdef_t *d, int grid, int amt, int vamt, int *m, int *v);
void qd_tl_quantize(qd_pattern_t *p, int t, const uint8_t *pos, int n, int grid, int amt, int vamt);
/* GRIDS: FIZZ's own map (grids_map.h, tools/grids_map.py): step i's level 0..255 at x, y (0..255) for part 0 LOW, 1
 * MID, 2 HIGH, the bilinear blend of the four nodes round it; plus chaos (0..255) x a seeded offset; a trig when the
 * level > 255 - dens, its velocity 40 + level x 87 / 255 */
int qd_tl_grids_level(int i, int x, int y, int part);
int qd_tl_grids_at(uint32_t seed, int t, int i, int x, int y, int part, int chaos);
void qd_tl_grids(qd_pattern_t *p, int t, const uint8_t *pos, int n, int x, int y, int dens, int part, int chaos,
                 uint32_t seed);

/* STRETCH (2026-10-25): the trigs of the n positions pos[] (play order) laid onto the m steps base.. (physical, in order),
 * the time scaled by num / den: a trig at i + micro / 24 steps (the track's nudge, its default micro, taken off) lands on
 * the nearest new step, the rest its micro (|micro| <= 12 but at the ends); two on one step: the nearer to its line
 * kept; a step's own length scaled too (the nearest of the table); the steps of pos and base.. that take no trig
 * cleared, locks and all; the steps outside them untouched. qd_tl_stretch_to: where trig i (micro mic) goes */
void qd_tl_stretch_to(int i, int mic, int num, int den, int m, int *j, int *mo);
void qd_tl_stretch(qd_pattern_t *p, int t, const uint8_t *pos, int n, int base, int m, int num, int den);
/* .. a track's loop (page -1; ts: its settings, into which the new loop goes: pages from its first looped one, whole
 * but the last) or page `page`'s start..end (its end moved) by 2^*pw (*pw -3..3; out: as far as it fits) or, fit 1..8,
 * onto fit pages (a page: fit x 2 steps); only: the settings alone (nothing written); returns the steps after */
int qd_tl_stretch_track(qd_pattern_t *p, int t, int page, int *pw, int fit, qd_tset_t *only);

/* the track doubled (SHIFT + B9's): 0 no room (8 pages); only != 0: its settings only, into *only (nothing copied) */
int qd_tl_double(qd_pattern_t *p, int t, qd_tset_t *only);
/* LENGTH: pw doublings (> 0) or halvings (< 0) of track t (page -1) or of page `page`'s end; only: as qd_tl_double */
void qd_tl_length(qd_pattern_t *p, int t, int page, int pw, qd_tset_t *only);

#ifdef __cplusplus
}
#endif
