/* SPDX-License-Identifier: GPL-3.0-only */
/* Scales (PLAN §8b question 39: per track, chromatic by default, with a global key the tracks can follow). A scale is
 * a set of pitch classes above its root; the held step's NOTE keys B1..B10 are ten degrees of it from the root at the
 * entry octave (a chromatic track: ten semitones), ALGORITHM moves notes a scale step, and the keyboard snaps its keys
 * down onto the scale. Pure functions, nothing of the OS. */
#pragma once
#include <stdint.h>

#include "pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { QD_SCALE_CHROMATIC, QD_SCALE_MAJOR, QD_SCALE_MINOR, QD_SCALE_DORIAN, QD_SCALE_PHRYGIAN, QD_SCALE_LYDIAN,
       QD_SCALE_MIXOLYDIAN, QD_SCALE_LOCRIAN, QD_SCALE_HARM_MINOR, QD_SCALE_MEL_MINOR, QD_SCALE_MAJ_PENTA,
       QD_SCALE_MIN_PENTA, QD_SCALE_BLUES, QD_SCALE_WHOLE_TONE, QD_NSCALE };

const char *qd_scale_name(int scale);   /* "CHROMATIC", "MAJOR", .. */
const char *qd_scale_short(int scale);  /* "CHR", "MAJ", .. */
int qd_scale_size(int scale);           /* notes an octave: 12, 7, 5, 6 */
int qd_scale_has(int scale, int root, int note);   /* the note is in the scale */
/* degree d (0.. ; negative below) of the scale from root at octave base (the root's note: 12 x (octave + 1) + root,
 * MIDI 60 = C4): clamped 0..127 */
int qd_scale_note(int scale, int root, int octave, int degree);
/* the next note of the scale above (dir > 0) or below note (a note not in the scale: the nearest that way) */
int qd_scale_step(int scale, int root, int note, int dir);
int qd_scale_snap(int scale, int root, int note);   /* the scale's note at or below note */

/* the key a track plays in: its own, or the project's global key when it follows it */
void qd_track_key(const qd_project_t *pr, const qd_pattern_t *p, int t, int *scale, int *root);

#ifdef __cplusplus
}
#endif
