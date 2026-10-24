/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
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
#include "pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_NTRACK QD_TRACKS
#define QD_TRACK_NAME 13               /* (P1.6d: twelve characters, as every name typed) */
#define QD_TKIND_NONE 255               /* P1.6d: a track of no kind */

typedef struct {
    char name[QD_TRACK_NAME];           /* its role: "DRUMS", "BASS", .. (P1.6d: the user's, or its kind's) */
    uint8_t kind;                       /* P1.6d: its kind (sounds.h QD_PK_*: BASS .. FX; its icon), QD_TKIND_NONE */
    uint8_t named;                      /* .. 1: the name typed by the user (a kind chosen keeps it) */
    /* the sound */
    int16_t patch[QD_QUAD_NP];
    uint8_t sound;                      /* the pool sound it was loaded from (0: not from the pool) */
    uint8_t edited;                     /* the patch differs from that sound's (shown as a * after its name) */
    char sound_name[QD_SOUND_NAME];     /* .. its name */
    uint16_t preset;                    /* P1.5t: the preset it came from (sounds.h: 1..23 factory, MINE ids; 0 none) */
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
/* P1.6d: track i's name and kind as the default kit has them (DRUMS .. NOISE; the kinds DRUMS, BASS, KEYS, LEAD,
 * PLUCK, PAD, KEYS, FX), not typed; a kind chosen (named 0: the name becomes the kind's); a name typed (the kind's own
 * name: named 0 again); the kinds' names */
void qd_track_name_default(qd_track_t *t, int i);
void qd_track_set_kind(qd_track_t *t, int kind);
void qd_track_set_name(qd_track_t *t, const char *name);
const char *qd_track_kind_name(int kind);
/* the track's sound: FM TONE's factory sound k (0 INIT, 1..16: preset id k + 1), edited 0; the voices sounding keep
 * theirs */
void qd_track_load_preset(qd_track_t *t, int k);
/* P1.3e: track i's starting sound (the default kit's: its preset id) and the track back on it (a SOUND default's
 * reset) */
int qd_track_start_sound(int i);
void qd_track_reset_sound(qd_track_t *t, int i);
/* the track's sound: pool sound k (0: none; its name, its patch), edited 0 (P1.5t: its preset id apart) */
void qd_track_load_sound(qd_track_t *t, int k, const char *name, const int16_t *patch);
const char *qd_track_sound_name(const qd_track_t *t);   /* the sound's name */
const char *qd_track_sound_label(const qd_track_t *t);  /* P1.5t: .. with " *" when edited (a static buffer) */
/* P1.5a: the track's sound to and from its pattern's copy (pattern.h qd_tsound_t) */
void qd_track_to_tsound(const qd_track_t *t, qd_tsound_t *s);
void qd_track_from_tsound(qd_track_t *t, const qd_tsound_t *s);

#define QD_MIX_LEVEL_DEFAULT 104        /* ChoralRoot's part LEVEL default (params.c TP[P_LEVEL]) */
#define QD_MIX_REVERB_DEFAULT 20        /* a modest reverb send (ChoralRoot's FX amounts: 35 the chord part, 10 the bass) */

#ifdef __cplusplus
}
#endif
