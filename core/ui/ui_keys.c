/* SPDX-License-Identifier: GPL-3.0-only */
/* KEYS (PLAN.md §8 "KEYS mode"; mockups/line.html s18 without the recording roll): the keybed plays the selected track.
 * Top left the track's box and name, top right the pool's sixteen voices as dots (lit: in use; white this track's,
 * grey another's); the sound's name in the middle, medium; the line-drawn keybed of two octaves with the keys
 * sounding filled white; OCT at the foot. The keybed shows the two octaves from the C the keys' range starts in
 * (C4 at OCT 0), one lower when the notes sounding are all below it. */
#include "ui.h"

void ui_keys_draw(const ui_keys_t *v)
{
    static qd_rgb fill[128];
    char num[12];
    int i, lo = 60 + 12 * v->octave, below = 0, above = 0;
    float x;

    ui_head(v->name, 0, v->track, UI_K3, UI_G1);
    for (i = 0; i < v->nvoice && i < UI_NVOICE_DOTS; i++)
        ui_O(186.f + (float)(i % 8) * 6.f, 8.f + (float)(i / 8) * 6.f, 1.6f, QD_NONE, 1.f,
             v->voice[i] == 2 ? UI_K3 : v->voice[i] ? UI_G1 : UI_G3);

    /* the sound: its number small, its name medium */
    ui_lab("SOUND", 120.f, 52.f, UI_G2, QD_CENTER);
    num[0] = (char)('0' + v->preset / 10);
    num[1] = (char)('0' + v->preset % 10);
    num[2] = 0;
    x = 120.f - ui_TW(v->sound, 26.f, 300, 0.5f) / 2.f;
    ui_T(num, x - 6.f, 80.f, 9.f, 600, 1.f, UI_G1, QD_RIGHT);
    ui_T(v->sound, 120.f, 92.f, 26.f, 300, 0.5f, UI_K3, QD_CENTER);
    if (v->edited)
        ui_O(x + ui_TW(v->sound, 26.f, 300, 0.5f) + 6.f, 74.f, 2.f, QD_NONE, 1.f, UI_K4);

    /* the keybed */
    for (i = 0; i < 128; i++) {
        fill[i] = v->sounding[i] ? UI_K3 : QD_NONE;
        below |= v->sounding[i] && i < lo;
        above |= v->sounding[i] && i >= lo;
    }
    if (below && !above && lo >= 12)
        lo -= 12;
    ui_keyboard(14.f, 118.f, 212.f, 78.f, lo, fill);

    {
        static const char *const OCT[5] = {"OCT -2", "OCT -1", "OCT 0", "OCT +1", "OCT +2"};
        const int k = v->octave < -2 ? 0 : v->octave > 2 ? 4 : v->octave + 2;
        ui_lab(OCT[k], 120.f, 222.f, UI_G2, QD_CENTER);
    }
}
