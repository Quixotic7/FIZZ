/* SPDX-License-Identifier: GPL-3.0-only */
/* Screen 2, the step view (mockups/line.html s2): the page as a little piano roll inside the sixteen white keys'
 * outlines. The top line: the track's box and name, the tempo grey, the pattern. The eight page dashes under it (the
 * track's pages grey, the shown one white, beyond the length dark; the playing page, when another, a short red mark).
 * In each column the step's notes as bars on the roll (lo at the foot, lo + span at the top): a chord a stack, the
 * length a bar, micro-timing off the column, a ratchet dashed into its hits; a LOCK step a small ring. Under a column
 * its markers (a white dot p-locks, a blue ring a condition, a green half ring a chance, an orange square a sound
 * lock). The held column and its notes outlined white, the notes the playhead is in filled white, the red playhead.
 * A page change slides the roll in from the side; page follow off: a small FOLLOW OFF left of the tempo. Positions,
 * sizes, colours and widths as s2. */
#include <math.h>

#include "ui.h"

static float col_x(int i) { return 9.f + (float)i * 13.5f + (float)(i / 4) * 2.f; }

static void roll(const ui_step_t *v)
{
    const int span = v->span > 0 ? v->span : 16;
    const float k = 150.f / (float)span;
    const int ph = v->pos >= 0.f ? (int)v->pos : -1;
    char num[12];
    int i, j, r;

    for (i = 0; i < 16; i++) {                          /* the keys' outlines */
        const ui_col_t *c = &v->col[i];
        const qd_rgb oc = c->beyond ? 0x141418u : c->held ? UI_K3 : i == ph ? UI_G2 : UI_G3;
        ui_RR(col_x(i), 36.f, 12.f, 174.f, 3.f, oc, 1.f, QD_NONE);
    }
    for (i = 0; i < 16; i++) {                          /* the notes */
        const ui_col_t *c = &v->col[i];
        const float mx = col_x(i) + 6.f;
        if (c->beyond)
            continue;
        if (c->type == 2) {                             /* LOCK: no notes, a small ring */
            ui_O(mx, 123.f, 3.f, c->held ? UI_K3 : UI_G1, 1.2f, QD_NONE);
        } else if (c->type) {
            const float x0 = col_x(i) + 1.f + (float)c->micro / 24.f * 13.5f;
            float w = c->len * 13.5f - 2.f;
            const int nr = c->ratch > 1 ? c->ratch : 1;
            const qd_rgb fill = c->lit ? UI_K3 : c->held ? QD_NONE : UI_G1;
            const qd_rgb line = c->held && !c->lit ? UI_K3 : QD_NONE;
            if (x0 + w > 238.f)                         /* INF and long notes: to the page's end */
                w = 238.f - x0;
            if (w < 3.f)
                w = 3.f;
            for (j = 0; j < c->nnote && j < 4; j++) {
                const float y = 200.f - (float)(c->note[j] - v->lo) * k;
                if (nr > 1) {
                    for (r = 0; r < nr; r++) {
                        const float pw = w / (float)nr - 1.5f;
                        ui_RR(x0 + (float)r * w / (float)nr, y - 2.f, pw < 1.5f ? 1.5f : pw, 4.f, 2.f, line, 1.f, fill);
                    }
                } else {
                    ui_RR(x0, y - 2.f, w, 4.f, 2.f, line, 1.f, fill);
                }
            }
        }
        /* the markers under the column: lock, condition, chance, sound */
        if (c->lock)
            ui_O(mx, 219.f, 1.8f, QD_NONE, 1.f, UI_K3);
        if (c->cond)
            ui_O(mx, 219.f, 2.2f, UI_K1, 1.2f, QD_NONE);
        if (c->chance)
            qd_arc(mx, 219.f, 2.2f, 0.f, UI_PI * 1.1f, UI_K2, 1.2f);
        if (c->sound)
            ui_RR(mx - 2.5f, 216.5f, 5.f, 5.f, 1.f, QD_NONE, 1.f, UI_K4);
    }
    if (ph >= 0 && ph < 16) {                           /* the playhead */
        const float px = col_x(ph) + (v->pos - (float)ph) * 13.5f;
        const float p[4] = {px, 33.f, px, 213.f};
        ui_P(p, 2, UI_RED, 1.f, 0, QD_NONE);
    }
    for (i = 0; i < 16; i += 4)                         /* the page's step numbers */
        ui_T(ui_itoa(num, v->page * 16 + i + 1), col_x(i) + 6.f, 235.f, 8.f, 500, 0.f, UI_G2, QD_CENTER);
}

void ui_step_draw(const ui_step_t *v)
{
    char bpm[12];
    int p;

    ui_head(v->name, v->pattern, v->track, UI_K3, UI_G1);
    ui_T(ui_itoa(bpm, v->bpm), 196.f, 17.f, 11.f, 400, 0.f, UI_G2, QD_RIGHT);
    for (p = 0; p < 8; p++) {                           /* the pages */
        const float x = 10.f + (float)p * 28.f;
        const float d[4] = {x, 27.f, x + 22.f, 27.f};
        ui_P(d, 2, p == v->page ? UI_K3 : p < v->npages ? UI_G2 : UI_G3, p == v->page ? 2.f : 1.5f, 0, QD_NONE);
        if (p == v->play_page && p != v->page) {
            const float m[4] = {x + 7.f, 27.f, x + 15.f, 27.f};
            ui_P(m, 2, UI_RED, 2.f, 0, QD_NONE);
        }
    }

    if (v->slide != 0.f) {                              /* a page change: the roll slides in */
        const float a = 1.f - fabsf(v->slide);
        qd_save();
        qd_alpha(a < 0.f ? 0.f : a);
        qd_translate(v->slide * 40.f, 0.f);
        roll(v);
        qd_restore();
    } else {
        roll(v);
    }
    if (!v->follow)                                     /* in the top line's gap, left of the tempo */
        ui_lab("FOLLOW OFF", 196.f - ui_TW(bpm, 11.f, 400, 0.f) - 8.f, 16.f, UI_G2, QD_RIGHT);
}
