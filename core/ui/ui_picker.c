/* SPDX-License-Identifier: GPL-3.0-only */
/* The sound picker's takeover (PLAN.md §9a rule 5: a value with steps is a picker, the value big, its neighbours
 * small): while PRESETS turns, the whole screen. mockups/line.html s10's cassette, simpler: SOUND and the track at the
 * top, the sound on the track as a cassette with its number and its name big and its reels turning, the sounds before
 * and after it small and grey above and below; a detent rolls the list (line.html's roll: 22 px, eased). */
#include <math.h>

#include "ui.h"

static void two(char *s, int k)
{
    s[0] = (char)('0' + (k / 10) % 10);
    s[1] = (char)('0' + k % 10);
    s[2] = 0;
}

/* a neighbour: "04  PLUCK" in a slot's outline */
static void neighbour(int k, const char *name, float y, float h, int text_above)
{
    char s[48];
    int i = 0;
    two(s, k);
    s[2] = s[3] = ' ';
    i = 4;
    while (*name && i < 46)
        s[i++] = *name++;
    s[i] = 0;
    ui_RR(52.f, y, 136.f, h, 5.f, UI_G3, 1.2f, QD_NONE);
    ui_T(s, 120.f, text_above ? y - 6.f : y + h - 9.f, 10.f, 500, 1.f, UI_G2, QD_CENTER);
}

void ui_picker_draw(const ui_picker_t *v)
{
    const float dy = ui_ease(v->roll) * 22.f * (float)(v->dir < 0 ? -1 : 1);
    const float x = 24.f, y = 64.f + dy, w = 192.f, h = 94.f;
    float nw;
    char s[12];
    int j;

    /* the top line: SOUND, the track at the right */
    ui_T("SOUND", 8.f, 17.f, 11.f, 500, 1.f, UI_K3, QD_LEFT);
    nw = ui_TW(v->name, 11.f, 500, 1.f);
    ui_T(v->name, 232.f, 17.f, 11.f, 500, 1.f, UI_G1, QD_RIGHT);
    ui_box(232.f - nw - 21.f, 5.f, v->track, UI_G1);

    qd_save();
    qd_clip_rect(0.f, 26.f, 240.f, 172.f);
    if (v->prev)
        neighbour(v->k - 1, v->prev, 44.f + dy, 14.f, 1);
    if (v->next)
        neighbour(v->k + 1, v->next, 166.f + dy, 26.f, 0);
    /* the cassette */
    ui_RR(x, y, w, h, 6.f, UI_K3, 1.5f, QD_NONE);
    ui_RR(x + 10.f, y + 8.f, w - 20.f, 40.f, 3.f, UI_G2, 1.f, QD_NONE);
    two(s, v->k);
    ui_T(s, x + 16.f, y + 21.f, 9.f, 600, 1.f, UI_K3, QD_LEFT);
    ui_T(v->cur, x + w / 2.f, y + 42.f, 22.f, 300, 0.5f, UI_K3, QD_CENTER);
    for (j = 0; j < 2; j++) {                           /* the reels */
        const float rx = x + w * (j ? 0.68f : 0.32f), ry = y + h - 22.f;
        int k;
        ui_O(rx, ry, 11.f, UI_G1, 1.3f, QD_NONE);
        for (k = 0; k < 3; k++) {
            const float an = v->reel + (float)k * UI_TAU / 3.f + (float)j;
            qd_line(rx + cosf(an) * 3.5f, ry + sinf(an) * 3.5f, rx + cosf(an) * 8.5f, ry + sinf(an) * 8.5f, UI_G1, 1.2f);
        }
    }
    qd_line(x + w * 0.32f + 11.f, y + h - 11.f, x + w * 0.68f - 11.f, y + h - 11.f, UI_G2, 1.f);
    qd_restore();

    {   /* where it is in the list: a dot a sound, this one lit */
        const float x0 = 120.f - (float)(v->n - 1) * 3.5f;
        for (j = 0; j < v->n; j++)
            ui_O(x0 + (float)j * 7.f, 214.f, j == v->k ? 2.2f : 1.4f, QD_NONE, 1.f, j == v->k ? UI_K3 : UI_G2);
    }
    ui_lab("FM TONE", 120.f, 232.f, UI_G2, QD_CENTER);
}
