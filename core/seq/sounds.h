/* SPDX-License-Identifier: GPL-3.0-only */
/* The project's sound pool (PLAN §8b, decided 2026-10-18): up to QD_NSOUND sounds, each an FM TONE patch with a name.
 * A step's sound lock is a pool number, 1..QD_NSOUND (0: the track's own sound). The pool starts with
 *   1        INIT
 *   2..17    the 16 factory sounds (ChoralRoot's: EP .. NOISE-ISH), as quad_voice.h's presets 1..16
 *   18..23   the demo drums (P1.2's kick, snare, clap, ghost snare, closed and open hat): what were sound locks with
 *            p-locks on A01's steps are pool sounds now, made the same way (a factory sound with those values set)
 * The sound editor (P1.4) adds to it. Pure data and functions, nothing of the OS. */
#pragma once
#include <stdint.h>

#include "pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { QD_SND_INIT = 1, QD_SND_NOISE = 17, QD_SND_KICK = 18, QD_SND_SNARE, QD_SND_CLAP, QD_SND_GHOST, QD_SND_HAT,
       QD_SND_OHAT, QD_SND_FIRST_FREE };

void qd_sounds_init(qd_project_t *pr);  /* the pool's first sounds (above) */
const char *qd_sound_name(const qd_project_t *pr, int k);   /* k 1..nsound; else "" */
const int16_t *qd_sound_patch(const qd_project_t *pr, int k);   /* k 1..nsound; else INIT's */
int qd_sound_add(qd_project_t *pr, const char *name, const int16_t *patch);   /* its number (the first free slot), 0:
                                                                               * the pool is full */
/* P1.5a: the pool has holes (a sound deleted, moved): a slot is used when it has a name; nsound is the last used */
int qd_sound_used(const qd_project_t *pr, int k);
void qd_sound_recount(qd_project_t *pr);
int qd_sound_step(const qd_project_t *pr, int k, int d, int lo);   /* k moved d used slots (lo 0: 0 the track's sound
                                                                     * counts, 1: from 1), clamped */

#ifdef __cplusplus
}
#endif
