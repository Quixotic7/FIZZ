/* SPDX-License-Identifier: GPL-3.0-only */
/* Scales (scale.h). */
#include "scale.h"

static const struct {
    const char *name, *shrt;
    uint16_t mask;                      /* bit k: k semitones above the root */
} SC[QD_NSCALE] = {
    {"CHROMATIC", "CHR", 0xFFF},
    {"MAJOR", "MAJ", (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11)},
    {"MINOR", "MIN", (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10)},
    {"DORIAN", "DOR", (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10)},
    {"PHRYGIAN", "PHR", (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10)},
    {"LYDIAN", "LYD", (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 11)},
    {"MIXOLYDIAN", "MIX", (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10)},
    {"LOCRIAN", "LOC", (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 10)},
    {"HARM MINOR", "HMI", (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 11)},
    {"MEL MINOR", "MMI", (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11)},
    {"MAJ PENTA", "MPN", (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9)},
    {"MIN PENTA", "mPN", (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10)},
    {"BLUES", "BLU", (1 << 0) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 10)},
    {"WHOLE TONE", "WHL", (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8) | (1 << 10)},
};

static int sc(int s) { return s < 0 || s >= QD_NSCALE ? 0 : s; }
const char *qd_scale_name(int scale) { return SC[sc(scale)].name; }
const char *qd_scale_short(int scale) { return SC[sc(scale)].shrt; }

int qd_scale_size(int scale)
{
    int k, n = 0;
    for (k = 0; k < 12; k++)
        n += (SC[sc(scale)].mask >> k) & 1;
    return n;
}

int qd_scale_has(int scale, int root, int note)
{
    const int pc = ((note - root) % 12 + 12) % 12;
    return (SC[sc(scale)].mask >> pc) & 1;
}

int qd_scale_note(int scale, int root, int octave, int degree)
{
    const int n = qd_scale_size(scale);
    int base = 12 * (octave + 1) + (root % 12 + 12) % 12, oct = degree >= 0 ? degree / n : -((n - 1 - degree) / n);
    int k, d = degree - oct * n, x = -1;
    for (k = 0; k < 12; k++)
        if ((SC[sc(scale)].mask >> k) & 1) {
            if (d == 0) {
                x = k;
                break;
            }
            d--;
        }
    x = base + 12 * oct + (x < 0 ? 0 : x);
    return x < 0 ? 0 : x > 127 ? 127 : x;
}

int qd_scale_step(int scale, int root, int note, int dir)
{
    int x = note, k;
    for (k = 0; k < 12; k++) {
        x += dir > 0 ? 1 : -1;
        if (x < 0 || x > 127)
            return note;
        if (qd_scale_has(scale, root, x))
            return x;
    }
    return note;
}

int qd_scale_snap(int scale, int root, int note)
{
    int x = note;
    while (x > 0 && !qd_scale_has(scale, root, x))
        x--;
    return x;
}

void qd_track_key(const qd_project_t *pr, const qd_pattern_t *p, int t, int *scale, int *root)
{
    const qd_tset_t *ts = &p->trk[t < 0 || t >= QD_PAT_NTRACK ? 0 : t];
    if (ts->key_global && pr) {
        *scale = pr->key_scale;
        *root = pr->key_root;
    } else {
        *scale = ts->scale;
        *root = ts->root;
    }
    *scale = sc(*scale);
    *root = (*root % 12 + 12) % 12;
}
