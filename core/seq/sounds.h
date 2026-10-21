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

/* P1.5a: a sound's kind and a name from the sound (its amp envelope, its filter, its operators: small rules, the same
 * answer every time). Kinds: BASS KEYS PLUCK BELL PAD LEAD KICK SNARE CLAP HAT NOISE; a name is an adjective (DARK,
 * BRIGHT, SOFT, WIDE, HARD, SHORT, LONG, HOLLOW, METAL ...) and the kind, twelve letters at most. variant 0: the best
 * name; 1, 2 ..: others (the name editor's dice) */
enum { QD_SK_BASS, QD_SK_KEYS, QD_SK_PLUCK, QD_SK_BELL, QD_SK_PAD, QD_SK_LEAD, QD_SK_KICK, QD_SK_SNARE, QD_SK_CLAP,
       QD_SK_HAT, QD_SK_NOISE, QD_NSK };
int qd_sound_kind(const int16_t *patch);
const char *qd_sound_kind_name(int kind);
void qd_sound_autoname(const int16_t *patch, int variant, char *out);   /* out: QD_SOUND_NAME bytes */

#ifdef __cplusplus
}
#endif
