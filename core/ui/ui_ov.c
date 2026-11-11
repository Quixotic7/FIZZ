/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* 2026-11-10: OVERVIEW (ui.h ui_ov_t), mockups/fizz-overview.html take A (LANES) as the owner chose it: six rows of the
 * page's 16 steps, a step's fill as tall as its velocity, the playing column marked. A function of its model only (no
 * clock): the frame's strips change only where a cell, the marker or a mute did (the playhead hops a step at a time).
 * Built through ui_layer.c (it includes this file; the Bluetooth package: in its second blob). */

#define OV_X0 34.f                                      /* the first cell's left */
#define OV_CW 12.4f                                     /* a cell's pitch (a 2 px gap every four) */
#define OV_W 10.5f                                      /* a cell's width */
#define OV_Y0 36.f                                      /* the first row's top .. */
#define OV_RP 33.f                                      /* .. a row's pitch .. */
#define OV_H 27.f                                       /* .. its cells' height */
static float ov_cx(int i) { return OV_X0 + (float)i * OV_CW + (float)(i / 4) * 2.f; }

void ui_ov_draw(const ui_ov_t *v)
{
    int t, i, mark = -1;
    ui_T(v->pattern, 10.f, 19.f, 12.f, 600, 0.8f, UI_OV_HEAD, QD_LEFT);
    for (i = 0; i < 8; i++) {                           /* the pages, the one shown lit; 2026-11-11 (owner): the selected
                                                         * track's loop: a page in it ringed grey, out of it dark; the
                                                         * page shown out of the loop: its ring only, crossed */
        const float x = 150.f + (float)i * 10.f;
        const int in = (v->loop_sel >> i) & 1;
        if (i == v->page) {
            ui_O(x, 14.f, 2.8f, in ? QD_NONE : UI_OV_HEAD, 1.2f, in ? UI_OV_HEAD : QD_NONE);
            if (!in)
                line2(x - 3.5f, 17.5f, x + 3.5f, 10.5f, UI_OV_HEAD, 1.2f);
        } else {
            ui_O(x, 14.f, 2.8f, in ? UI_G1 : UI_G3, 1.f, QD_NONE);
        }
    }
    for (i = 0; i < 16; i += 4)                         /* the beats */
        ui_O(ov_cx(i) + OV_W / 2.f, 29.f, 1.3f, QD_NONE, 1.f, i ? UI_G2 : UI_G1);
    if (v->sel >= 0 && v->sel < UI_NTRACK && v->play[v->sel] >= 0)
        mark = v->play[v->sel];                         /* the marker: the selected track's column .. */
    for (t = 0; t < UI_NTRACK && mark < 0; t++)         /* .. else the first track's on this page */
        mark = v->play[t];
    if (mark >= 0 && mark < 16) {
        const float x = ov_cx(mark) + OV_W / 2.f, tri[6] = {x - 4.f, 23.f, x + 4.f, 23.f, x, 30.f};
        ui_P(tri, 3, UI_RED, 1.f, 1, UI_RED);
    }
    for (t = 0; t < UI_NTRACK; t++) {
        const float y = OV_Y0 + (float)t * OV_RP;
        const int k = ui_track_kind(t + 1), solo = v->mute[t] == 2, pgoff = (v->pg_off >> t) & 1;
        const int mute = v->mute[t] == 1 || pgoff;          /* (2026-11-11: a page out of the loop is muted too) */
        const qd_rgb col = mute ? UI_G1 : k >= 0 ? UI_PK_COL[k] : UI_G1;   /* (muted: grey, at 55 %) */
        if (t == v->sel || solo)                        /* selected: boxed white; soloed: yellow (the mix's) */
            ui_RR(3.f, y - 1.f, 26.f, OV_H + 2.f, 6.f, solo ? UI_KSEL : UI_K3, 1.6f, QD_NONE);
        qd_save();
        if (mute)
            qd_alpha(0.55f);
        ui_kind_icon(k, 16.f, y + OV_H / 2.f, 1.4f, col);   /* (no kind: nothing) */
        for (i = 0; i < 16; i++) {
            const float x = ov_cx(i);
            const int vel = v->vel[t][i], lock = (v->lock[t] >> i) & 1, out = (v->out[t] >> i) & 1;
            const qd_rgb sc = out ? UI_G2 : col;        /* outside the page's start..end (2026-11-11, owner): the step
                                                         * still drawn, grey and darker; KNOB 3 / 4 move the range */
            if (out) {
                qd_save();
                qd_alpha(mute ? 0.3f : 0.6f);
            }
            ui_RR(x, y, OV_W, OV_H, 3.f, QD_NONE, 1.f, UI_G3);
            if (vel) {                                  /* a fill as tall as the velocity (a ghost: an outline) */
                const float h = lock ? OV_H : (float)vel / 127.f * OV_H < 4.f ? 4.f : (float)vel / 127.f * OV_H;
                const int hot = i == v->play[t] && !mute && !lock && !out;
                if (hot) {                              /* playing: white, glowing */
                    qd_save();
                    qd_alpha(0.28f);
                    ui_RR(x - 2.5f, y + OV_H - h - 2.5f, OV_W + 5.f, h + 5.f, 5.f, QD_NONE, 1.f, UI_K3);
                    qd_restore();
                }
                ui_RR(x, y + OV_H - h, OV_W, h, 3.f, lock ? sc : QD_NONE, 1.2f,
                      lock ? QD_NONE : hot ? 0xffffffu : sc);
            }
            if (out)
                qd_restore();
            if (t == v->sel && ((v->held >> i) & 1))    /* held / selected: outlined white */
                ui_RR(x - 2.f, y - 2.f, OV_W + 4.f, OV_H + 4.f, 4.5f, UI_K3, 1.3f, QD_NONE);
        }
        qd_restore();
        if (mute && !pgoff) {                           /* not heard: a line through the row */
            const float p[4] = {OV_X0, y + OV_H / 2.f, 236.f, y + OV_H / 2.f};
            ui_P(p, 2, UI_G1, 1.5f, 0, QD_NONE);
        } else if (pgoff) {                             /* the page out of its loop: dashed through (2026-11-11) */
            float xx;
            for (xx = OV_X0; xx < 232.f; xx += 9.f) {
                const float p[4] = {xx, y + OV_H / 2.f, xx + 5.f, y + OV_H / 2.f};
                ui_P(p, 2, UI_G1, 1.5f, 0, QD_NONE);
            }
        }
    }
}
