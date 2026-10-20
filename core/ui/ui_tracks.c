/* SPDX-License-Identifier: GPL-3.0-only */
/* Screen 3, TRACKS (mockups/line.html s3): eight little clocks, each at its own length and rate. Each track is a ring
 * with its trigs as dots round it (a LOCK step a hollow one), its own hand at its own place in its own loop (from the
 * sequencer: polymeter at a glance), the dot that just played flashing white, the voices it sounds as dots inside, its
 * number above them, its name and "length · rate" under it. A muted track's ring is dashed, its number big, no hand.
 * The selected track is white with a red hand, its parts in its knobs' colours (PLAN §9a rule 1: trigs blue = LEN,
 * length and rate green = RATE, voices white = LIMIT, an orange arc outside = SWING, when its swing is on). The pool's
 * sixteen voices top right, the pattern's name grey after the title. Positions, sizes, colours and widths as s3. */
#include <math.h>

#include "ui.h"

void ui_tracks_draw(const ui_tracks_t *v)
{
    static const float dash[2] = {2.f, 3.f};
    int i, k;
    ui_head("TRACKS", 0, 0, UI_K3, UI_G1);
    if (v->pattern)                                     /* the pattern playing, grey after the title (the pool's dots
                                                         * hold the right) */
        ui_T(v->pattern, 8.f + ui_TW("TRACKS", 12.f, 600, 0.8f) + 8.f, 18.f, 12.f, 500, 0.8f, UI_G1, QD_LEFT);
    for (k = 0; k < 16; k++)
        ui_O(186.f + (float)(k % 8) * 6.f, 8.f + (float)(k / 8) * 6.f, 1.6f, QD_NONE, 1.f,
             k < v->pool_used ? UI_K3 : UI_G3);
    for (i = 0; i < 8; i++) {
        const ui_track_clock_t *tr = &v->tr[i];
        const float cx = 32.f + (float)(i % 4) * 59.f, cy = 72.f + (float)(i / 4) * 96.f, r = 23.f;
        const int sel = i == v->sel, len = tr->len < 1 ? 1 : tr->len;
        char num[12], lr[32];
        int s;
        if (tr->mute) {
            qd_save();
            qd_dash(dash, 2);
            ui_O(cx, cy, r, UI_G3, 1.2f, QD_NONE);
            qd_restore();
        } else {
            ui_O(cx, cy, r, sel ? UI_K3 : UI_G2, sel ? 1.5f : 1.2f, QD_NONE);
        }
        if (sel && tr->swing && v->swing_amt > 50)      /* SWING, orange: the arc grows with the amount */
            qd_arc(cx, cy, r + 5.f, -UI_PI / 2.f, -UI_PI / 2.f + UI_TAU * 0.16f * (float)(v->swing_amt - 50) / 15.f,
                   UI_K4, 1.2f);
        for (s = 0; s < len && s < 128; s++) {
            const float a = -UI_PI / 2.f + (float)s / (float)len * UI_TAU;
            const float x = cx + cosf(a) * r, y = cy + sinf(a) * r;
            const int hit = !tr->mute && s == tr->flash_step && tr->flash_age >= 0.f && tr->flash_age < 1.2f;
            const qd_rgb col = tr->mute ? UI_G3 : hit ? UI_K3 : sel ? UI_K1 : UI_G2;
            if (!tr->trig[s])
                continue;
            if (tr->trig[s] == 2 && !hit)               /* a LOCK step: hollow */
                ui_O(x, y, 1.5f, col, 1.f, UI_BG);
            else
                ui_O(x, y, hit ? 2.6f : 1.5f, QD_NONE, 1.f, col);
        }
        if (!tr->mute) {
            const float a = -UI_PI / 2.f + tr->pos / (float)len * UI_TAU;
            const float hand[4] = {cx, cy, cx + cosf(a) * (r - 5.f), cy + sinf(a) * (r - 5.f)};
            ui_P(hand, 2, sel ? UI_RED : UI_G1, 1.2f, 0, QD_NONE);
            ui_O(cx, cy, 1.5f, QD_NONE, 1.f, sel ? UI_RED : UI_G1);
        }
        ui_T(ui_itoa(num, i + 1), cx, cy + (tr->mute ? 4.5f : -4.f), 12.f, 600, 0.f, UI_G2, QD_CENTER);
        {   /* the name, 12 px, squeezed to the clock's width */
            const float tw = ui_TW(tr->name, 12.f, 600, 0.5f);
            qd_save();
            qd_translate(cx, cy + r + 17.f);
            if (tw > 56.f)
                qd_scale(56.f / tw < 0.7f ? 0.7f : 56.f / tw, 1.f);
            ui_T(tr->name, 0.f, 0.f, 12.f, 600, 0.5f, tr->mute ? UI_G2 : sel ? UI_K3 : UI_G1, QD_CENTER);
            qd_restore();
        }
        ui_itoa(lr, len);
        {   /* "16 · 1x" */
            int n = 0;
            while (lr[n])
                n++;
            lr[n++] = ' ', lr[n++] = (char)0xC2, lr[n++] = (char)0xB7, lr[n++] = ' ';
            for (k = 0; tr->rate && tr->rate[k] && n < 30; k++)
                lr[n++] = tr->rate[k];
            lr[n] = 0;
        }
        {   /* its length and rate, 12 px, squeezed to the clock's width */
            const float tw = ui_TW(lr, 12.f, 500, 0.f);
            qd_save();
            qd_translate(cx, cy + r + 31.f);
            if (tw > 56.f)
                qd_scale(56.f / tw < 0.7f ? 0.7f : 56.f / tw, 1.f);
            ui_T(lr, 0.f, 0.f, 12.f, 500, 0.f, sel ? UI_K2 : UI_G2, QD_CENTER);
            qd_restore();
        }
        for (k = 0; k < tr->voices && k < 8; k++)
            ui_O(cx - (float)(tr->voices - 1 < 7 ? tr->voices - 1 : 7) * 2.5f + (float)k * 5.f, cy + 8.f, 1.2f,
                 QD_NONE, 1.f, sel ? UI_K3 : UI_G1);
    }
}
