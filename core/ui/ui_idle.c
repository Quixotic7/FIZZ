/* SPDX-License-Identifier: GPL-3.0-only */
/* Screen 1, idle (mockups/line.html s1): the four operators (C, A, B1, B2) in their knob colours dance in a square,
 * a bob on each beat and a quarter turn in the last beat of each bar (the app's turn and phase); the pattern and the
 * tempo at the foot, the play mark between them. Positions, sizes, colours and widths as s1. */
#include <math.h>

#include "ui.h"

void ui_idle_draw(const ui_idle_t *v)
{
    static const char *const NAMES[4] = {"C", "A", "B1", "B2"};
    const float cx = 120.f, cy = 112.f, h = 44.f;
    char bpm[12];
    int i;

    ui_T("QUADRILLE", 120.f, 22.f, 10.f, 500, 4.f, UI_G1, QD_CENTER);
    {   /* the square, dotted */
        const float sq[8] = {cx - h, cy - h, cx + h, cy - h, cx + h, cy + h, cx - h, cy + h};
        static const float dots[2] = {1.f, 4.f};
        qd_save();
        qd_dash(dots, 2);
        ui_P(sq, 4, UI_G3, 1.f, 1, QD_NONE);
        qd_restore();
    }
    ui_O(cx, cy, 3.f, UI_G2, 1.f, QD_NONE);

    for (i = 0; i < 4; i++) {
        const float a = ((float)i + v->turn) * UI_TAU / 4.f - UI_PI * 3.f / 4.f;
        const float x = cx + cosf(a) * h * 1.41f, y = cy + sinf(a) * h * 1.41f;
        const float ph = v->phase + (float)i * 1.4f;
        const float bob = fabsf(sinf(ph / 2.f)) * 3.f * v->amp;
        const float sw = sinf(ph) * 0.9f * v->amp;
        const float hy = y - 14.f - bob;
        const qd_rgb col = UI_K[i];
        const float body[4] = {x, hy + 5.f, x, y + 4.f - bob};
        const float arms[6] = {x - 9.f, y - 9.f - bob + sw * 5.f, x, y - 5.f - bob, x + 9.f, y - 9.f - bob - sw * 5.f};
        const float legs[6] = {x - 6.f, y + 14.f, x, y + 4.f - bob, x + 6.f + sw * 2.f,
                               y + 14.f - (sw > 0.f ? sw : 0.f) * 4.f};
        ui_O(x, hy, 4.5f, col, 1.5f, QD_NONE);      /* head */
        ui_P(body, 2, col, 1.5f, 0, QD_NONE);        /* body */
        ui_P(arms, 3, col, 1.5f, 0, QD_NONE);        /* arms */
        ui_P(legs, 3, col, 1.5f, 0, QD_NONE);        /* legs */
        ui_T(NAMES[i], x, y + 26.f, 8.f, 600, 1.f, UI_G1, QD_CENTER);
    }

    ui_T(v->pattern, 10.f, 222.f, 22.f, 300, 0.f, UI_K3, QD_LEFT);
    ui_itoa(bpm, v->bpm);
    ui_T(bpm, 232.f, 222.f, 22.f, 300, 0.f, UI_K1, QD_RIGHT);
    ui_lab("BPM", 232.f, 196.f, UI_G2, QD_RIGHT);
    ui_lab("PATTERN", 10.f, 196.f, UI_G2, QD_LEFT);
    {   /* the play mark: outlined when stopped, filled when playing */
        const float tri[6] = {114.f, 206.f, 114.f, 220.f, 126.f, 213.f};
        ui_P(tri, 3, UI_G1, 1.3f, 1, v->playing ? UI_G1 : QD_NONE);
    }
}
