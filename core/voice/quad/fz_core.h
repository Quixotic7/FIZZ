/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
 * not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* FIZZ engine declarations shared by the voice code: how parameters are described and shown, the per-track and
 * global value indices the voice reads, the voice modes, one block's modulation of a voice, presets and the pages
 * of an engine's parameters. */

#ifndef FZ_CORE_H
#define FZ_CORE_H

#include <stdint.h>

/* number of elements of a true array (not a pointer) */
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

/* how a parameter's value is displayed */
enum {
    F_INT = 0,     /* plain number */
    F_PCT = 1,     /* percent of the range */
    F_BIPCT = 2,   /* signed percent around the centre */
    F_TIME = 3,    /* a time */
    F_LFOHZ = 4,   /* an LFO rate in Hz */
    F_CUTOFF = 5,  /* a filter frequency */
    F_DB = 6,      /* a level in dB */
    F_SEMI = 7,    /* semitones */
    F_ENUM = 8,    /* one of the names[] */
    F_BPM = 9,     /* a tempo */
    F_NOTE = 10,   /* a note name */
    F_ONOFF = 11,  /* off / on */
    F_OCT = 12,    /* octaves */
    F_STEPS = 13,  /* a step count */
    F_OFS = 14,    /* the value relative to the middle of its range */
    /* 15 and 16 are reserved */
    F_TIMEI = 17,  /* a time whose 127 shows "INF" */
    F_FIX2 = 18,   /* hundredths (4201 is "42.01"), signed when the range goes below 0; names[0], if present, is the
                      word shown for the top whole number */
    F_INTI = 19    /* a number whose maximum shows "INF" */
};

/* one parameter: its label, display format, range and default, value names (F_ENUM etc.) and unit text */
typedef struct {
    const char *label;
    uint8_t fmt;
    int16_t min, max, def;
    const char *const *names;
    const char *unit;
} param_desc_t;

/* per-track parameters the voice reads */
enum { P_VOICE, P_COUNT };

/* global values the voice and the effects read (indices into song_t.g): tempo, chorus rate, chorus depth */
enum { G_BPM, G_CRATE, G_CDEPTH, G_COUNT };

/* voice modes (the value of P_VOICE) */
enum { V_POLY, V_MONO, V_LEGATO, V_UNISON };

/* one control block's modulation of a voice */
typedef struct {
    uint32_t inc;     /* phase increment of the base pitch */
    int32_t pitch16;  /* the pitch in 1/16 semitones */
    int32_t amp0;     /* amplitude (Q15) at the block's start ... */
    int32_t amp1;     /* ... ramping to this at its end */
    int32_t shape;    /* a shape value, 0..127 << 8 */
} vmod_t;

/* a preset: name, eight engine values, envelope, filter envelope amount, mono flag, effect sends and a pattern.
 * fx holds each send value plus 1, so 0 means "not set by the preset". */
typedef struct {
    const char *name;
    int8_t e[8];
    uint8_t env[4];
    int8_t fenv;
    uint8_t mono;
    uint8_t fx[4];
    uint8_t pat;
} preset_t;

/* initialiser helpers for preset_t: the four effect sends and the pattern number */
#define FX(d, c, dl, r) .fx = {(d) + 1, (c) + 1, (dl) + 1, (r) + 1}
#define PAT(n) .pat = (n)

/* one page of an engine's parameter screen: a title over four columns */
typedef struct {
    const char *title;
    param_desc_t col[4];
} eng_page_t;

/* upper bound on the size of an engine's saved state */
#define ENG_BLOB_MAX 160u

/* the song-wide values: the globals above and the master level in Q12 */
typedef struct {
    int16_t g[G_COUNT];
    uint32_t master_q12;
} song_t;

/* one instance per translation unit that includes this header, filled in by that unit */
static song_t song;

#endif /* FZ_CORE_H */
