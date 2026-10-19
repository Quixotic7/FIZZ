/* SPDX-License-Identifier: GPL-3.0-only */
/* Chord shapes and names (chord.h). */
#include <string.h>

#include "chord.h"

const qd_shape_t QD_SHAPE[QD_NSHAPE] = {
    {"", 0, {0, 0, 0, 0, 0}},               /* note 1 alone */
    {"", 2, {4, 7, 0, 0, 0}},               /* major */
    {"m", 2, {3, 7, 0, 0, 0}},
    {"7", 3, {4, 7, 10, 0, 0}},
    {"m7", 3, {3, 7, 10, 0, 0}},
    {"maj7", 3, {4, 7, 11, 0, 0}},
    {"sus2", 2, {2, 7, 0, 0, 0}},
    {"sus4", 2, {5, 7, 0, 0, 0}},
    {"dim", 2, {3, 6, 0, 0, 0}},
    {"aug", 2, {4, 8, 0, 0, 0}},
    {"6", 3, {4, 7, 9, 0, 0}},
    {"add9", 3, {4, 7, 14, 0, 0}},
    {"madd9", 3, {3, 7, 14, 0, 0}},
    {"7sus4", 3, {5, 7, 10, 0, 0}},
    {"5", 1, {7, 0, 0, 0, 0}},              /* power */
    {"oct", 1, {12, 0, 0, 0, 0}},
    {"m6", 3, {3, 7, 9, 0, 0}},             /* P1.3c: more four-note chords .. */
    {"dim7", 3, {3, 6, 9, 0, 0}},
    {"m7b5", 3, {3, 6, 10, 0, 0}},
    {"9", 4, {4, 7, 10, 14, 0}},            /* .. and the five-note ones */
    {"maj9", 4, {4, 7, 11, 14, 0}},
    {"m9", 4, {3, 7, 10, 14, 0}},
    {"6/9", 4, {4, 7, 9, 14, 0}},
    {"9sus4", 4, {5, 7, 10, 14, 0}},
    {"7b9", 4, {4, 7, 10, 13, 0}},
    {"7#9", 4, {4, 7, 10, 15, 0}},
    {"m11", 4, {3, 7, 10, 17, 0}},
    {"13", 4, {4, 7, 10, 21, 0}},
    {"11", 5, {4, 7, 10, 14, 17}},          /* P1.3e: the six-note ones */
    {"m11", 5, {3, 7, 10, 14, 17}},
    {"9#11", 5, {4, 7, 10, 14, 18}},
    {"maj9#11", 5, {4, 7, 11, 14, 18}},
    {"13", 5, {4, 7, 10, 14, 21}},
    {"maj13", 5, {4, 7, 11, 14, 21}},
    {"m13", 5, {3, 7, 10, 14, 21}},
};

static const char *const PC[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

const char *qd_pc_name(int note) { return PC[((note % 12) + 12) % 12]; }

const char *qd_note_name(int note, char *buf)
{
    const char *p = qd_pc_name(note);
    const int oct = note / 12 - 1;
    int k = 0;
    while (*p)
        buf[k++] = *p++;
    if (oct < 0)
        buf[k++] = '-', buf[k++] = (char)('0' - oct);
    else
        buf[k++] = (char)('0' + oct % 10);
    buf[k] = 0;
    return buf;
}

int qd_shape_of(const uint8_t *off5)
{
    uint8_t v[5];
    int i, j, n = 0, k;
    for (i = 0; i < 5; i++)                             /* the offsets present, ascending */
        if (off5[i]) {
            for (j = n; j > 0 && v[j - 1] > off5[i]; j--)
                v[j] = v[j - 1];
            v[j] = off5[i];
            n++;
        }
    for (k = 0; k < QD_NSHAPE; k++)
        if (QD_SHAPE[k].n == n && !memcmp(QD_SHAPE[k].off, v, (size_t)n))
            return k;
    return -1;
}

/* the pitch-class set of notes as intervals above root (bit i: an interval of i semitones mod 12) */
static unsigned pcs_above(const uint8_t *notes, int n, int root)
{
    unsigned m = 0;
    int i;
    for (i = 0; i < n; i++)
        m |= 1u << (((notes[i] - root) % 12 + 12) % 12);
    return m;
}

int qd_chord_name(const uint8_t *notes, int n, char *root, char *qual)
{
    int i, k, lo = 0;
    root[0] = qual[0] = 0;
    if (n <= 0)
        return 0;
    for (i = 1; i < n; i++)
        if (notes[i] < notes[lo])
            lo = i;
    if (n == 1) {
        strcpy(root, qd_pc_name(notes[0]));
        return 1;
    }
    for (i = 0; i < n; i++) {                           /* the lowest note first as the root, then the others */
        const int r = notes[(lo + i) % n];
        const unsigned m = pcs_above(notes, n, r);
        for (k = 1; k < QD_NSHAPE; k++) {
            unsigned s = 1u;
            int j;
            for (j = 0; j < QD_SHAPE[k].n; j++)
                s |= 1u << (QD_SHAPE[k].off[j] % 12);
            if (s == m && strcmp(QD_SHAPE[k].name, "oct") != 0) {
                strcpy(root, qd_pc_name(r));
                strcpy(qual, QD_SHAPE[k].name);
                return 1;
            }
        }
    }
    if (pcs_above(notes, n, notes[lo]) == 1u) {         /* one pitch class in octaves */
        strcpy(root, qd_pc_name(notes[lo]));
        strcpy(qual, "oct");
        return 1;
    }
    return 0;
}
