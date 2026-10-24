/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The built-in demo patterns (P1.2), made through the editing API so they are what P1.3's UI would make:
 *
 *   A01  a beat on track 1 (DRUMS) alone: every drum is a sound lock on its step, a sound of the pool (sounds.h: KICK,
 *        SNARE, CLAP, GHOST, HAT, OPEN HAT; PLAN §5: a whole kit on one track), with its mix as p-locks. 16 steps at
 *        100 BPM, swing 56 %
 *   A02  A01's beat with BASS (mono, two TIEs), CHORDS on the KEYS track (EP, strummed, a micro-timed stab, a 1:2
 *        and a NOT 1ST stab) and a LEAD (ratchets, chance), at mixed rates and lengths (16, 32 = pages 1 and 2, 16 at
 *        1/2x, 24 = page 1 and page 2's 1..8)
 *   A03  polymeter on all eight tracks: 16, 12, 7 at 1/2x, 10 at 3/2x, 9 at 2x, 5 at 1/4x, 11 at 4/3x, 6 at 2/3x (each
 *        page 1 from 1 to its length)
 *
 * The drum sounds were designed by rendering and measuring (no ears yet; the owner tunes them): see sounds.c. */
#pragma once
#include "pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_DEMO_N 3                     /* A01..A03 */
void qd_demo_load(qd_project_t *pr);   /* patterns 0..2 written (the rest untouched) */
void qd_demo_pattern(qd_pattern_t *p, int k);   /* demo k (0..2) into p */
/* (2026-10-24) demo k's pool sound locks without making it: track 1's steps 1..16, each its sound (0 none; every one
 * on, its sound overridden; no other track locks one, no track sound has a source: test_persist checks) */
extern const uint8_t QD_DEMO_DRUMS[QD_DEMO_N][16];

#ifdef __cplusplus
}
#endif
