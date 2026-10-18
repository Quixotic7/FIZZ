/* SPDX-License-Identifier: GPL-3.0-only */
/* Chords for the held step's NOTE field (PLAN §8: "SELECT picks a chord shape"; line.html s4: the chord name big).
 *
 * The shapes SELECT steps through, built on note 1 (offsets in semitones, as a step's notes 2..4 are stored):
 *   NOTE (note 1 alone), MAJ, m, 7, m7, maj7, sus2, sus4, dim, aug, 6, add9, madd9, 7sus4, 5 (power), OCT
 * The name of any set of notes: the root and the quality ("C", "Cm7", "F#sus4"), trying each note as the root (so an
 * inversion is named by its root); a set no shape matches: no name (the screen shows the note names).
 * Pure functions, nothing of the OS. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QD_NSHAPE 16
typedef struct {
    const char *name;                   /* the shape's name on the screen ("m7"; "" major) */
    uint8_t n;                          /* the offsets above note 1 (0..3 of them) */
    uint8_t off[3];
} qd_shape_t;
extern const qd_shape_t QD_SHAPE[QD_NSHAPE];

/* the shape a step's offsets (notes 2..4, 0 none) are, in QD_SHAPE; -1: none of them */
int qd_shape_of(const uint8_t *off3);
/* the name of notes[0..n) (absolute, any order): root ("C#") and quality ("m7"); 0 when no shape matches (n 1: the
 * note's name as root, quality "") */
int qd_chord_name(const uint8_t *notes, int n, char *root, char *qual);
const char *qd_note_name(int note, char *buf);   /* "C#4" (MIDI 60 = C4; buf 6 bytes) */
const char *qd_pc_name(int note);                /* "C#" */

#ifdef __cplusplus
}
#endif
