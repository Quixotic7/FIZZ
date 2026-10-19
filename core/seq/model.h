/* SPDX-License-Identifier: GPL-3.0-only */
/* The model, its start (PLAN.md §2, §4): eight tracks, each with a name, its sound (an FM TONE patch, the factory
 * sound it came from, whether it has been edited since), its mix, its voice limit and reserve, and FM TONE's owner (its
 * FREE LFOs' phases, the tempo they run at). The sequencer's patterns are pattern.h (P1.2); a pattern's track settings
 * carry their own limit and reserve, which the player gives the pool at each of its notes (the limit here is the
 * keybed's until a pattern has played).
 *
 * Plain C structs with no allocation, so a track can be stored as it is (P1.5); the owner's two pointers are run-time
 * state and are set again by qd_model_bind after a load. Nothing here calls the OS. */
#pragma once
#include <stdint.h>

#include "../voice/mix.h"
#include "../voice/quad/quad_voice.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_NTRACK 8
#define QD_TRACK_NAME 12

typedef struct {
    char name[QD_TRACK_NAME];           /* its role: "DRUMS", "BASS", .. */
    /* the sound */
    int16_t patch[QD_QUAD_NP];
    uint8_t sound;                      /* the pool sound it was loaded from (1 INIT, 2..17 the factory sounds, ..) */
    uint8_t edited;                     /* the patch differs from that sound's */
    char sound_name[12];                /* .. its name */
    /* the mix */
    qd_mix_track_t mix;
    /* the voices (PLAN.md §2) */
    uint8_t limit;                      /* 1..16 (1: mono, a choke) */
    uint8_t reserve;                    /* 0..16 */
    /* FM TONE's owner: the FREE / HOLD LFOs' phases, the tempo, the latest voice (run-time) */
    qd_quad_owner_t owner;
} qd_track_t;

typedef struct {
    qd_track_t t[QD_NTRACK];
} qd_model_t;

/* the default kit: a factory sound per track (qd_model.c's table), level 104, pan centre, reverb 20 */
void qd_model_init(qd_model_t *m);
void qd_model_bind(qd_model_t *m);      /* the owners' pointers (after a load) */
void qd_model_tempo(qd_model_t *m, int bpm);
/* the track's sound: factory sound k (0 INIT, 1..16; pool sound k + 1), edited 0; the voices sounding keep theirs */
void qd_track_load_preset(qd_track_t *t, int k);
/* the track's sound: pool sound k (its name, its patch), edited 0 */
void qd_track_load_sound(qd_track_t *t, int k, const char *name, const int16_t *patch);
const char *qd_track_sound_name(const qd_track_t *t);   /* the sound's name */

#define QD_MIX_LEVEL_DEFAULT 104        /* ChoralRoot's part LEVEL default (params.c TP[P_LEVEL]) */
#define QD_MIX_REVERB_DEFAULT 20        /* a modest reverb send (ChoralRoot's FX amounts: 35 the chord part, 10 the bass) */

#ifdef __cplusplus
}
#endif
