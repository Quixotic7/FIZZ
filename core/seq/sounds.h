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

void qd_sounds_init(qd_project_t *pr);  /* the pool's first sounds (above): the demo's (and a 0.6 project's) pool */
void qd_sounds_clear(qd_project_t *pr); /* P1.5t: an empty pool (a new project's) */
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
/* P1.5u: up to max name ideas (the first: autoname's best; then adjectives with the kind's words: WARM PLUCK, GLASS
 * TINE, SOFT PING ..); their count */
int qd_sound_ideas(const int16_t *patch, char (*out)[QD_SOUND_NAME], int max);

/* ---- P1.5t (PLAN.md "The sound system, redesigned"): THE PRESETS. A preset is a sound with an id:
 *   FACTORY   ids 1..QD_NFACTORY, compiled in, read only: INIT, the 16 FM TONE sounds (quad_voice.h's presets 1..16),
 *             the six drums (above); id k = the pool's old number k
 *   MINE      ids QD_PRESET_MINE0.. (each MY preset keeps its own: persist.h's library holds them, 256 places)
 * A pool sound and a track's sound remember the preset they came from (0: none known).
 * THE KINDS (the browser's filters B2..B8): the auto-names' classifier, in seven: BASS KEYS PLUCK PAD LEAD DRUMS FX
 * (a BELL is KEYS; KICK SNARE CLAP HAT are DRUMS; NOISE, a strongly detuned or fed-back sound is FX) */
#define QD_NFACTORY 23
#define QD_PRESET_MINE0 0x100
enum { QD_PK_BASS, QD_PK_KEYS, QD_PK_PLUCK, QD_PK_PAD, QD_PK_LEAD, QD_PK_DRUMS, QD_PK_FX, QD_NPK };
int qd_preset_kind(const int16_t *patch);
const char *qd_preset_kind_name(int pk);   /* "BASS" .. "FX" */
void qd_factory_preset(int i, qd_sound_t *out);   /* i 0..QD_NFACTORY-1: id i + 1, its kind */
int qd_factory_of(const char *name, const int16_t *patch);   /* the factory preset this is (its id), 0 none */

#ifdef __cplusplus
}
#endif
