/* SPDX-License-Identifier: GPL-3.0-only */
/* The sound picker's takeover (PLAN.md §9a rule 5: a value with steps is a picker, the value big, its neighbours
 * small; §8b: the project's pool of up to 128 sounds): while PRESETS turns, the whole screen. mockups/line.html s10's
 * cassette, simpler: SOUND and the track at the top (a step's sound lock: "STEP 5 · SOUND" and a padlock), the sound as
 * a cassette with its pool number (three digits) and its name big and its reels turning, the sounds before and after
 * it small and grey above and below; a detent rolls the list (line.html's roll: 22 px, eased). k 0 is the lock off:
 * the cassette grey, "TRACK SOUND" in its window. At the foot, where the sound is in the pool: thirteen banks of ten
 * as dashes, the banks that hold sounds grey, the sound's bank white with a tick at its place in it, the bank's
 * numbers under it. */
#include <math.h>

#include "ui.h"

static void three(char *s, int k)
{
    s[0] = (char)('0' + (k / 100) % 10);
    s[1] = (char)('0' + (k / 10) % 10);
    s[2] = (char)('0' + k % 10);
    s[3] = 0;
}

/* a neighbour: "004  PLUCK" in a slot's outline (k 0: the track's sound, no number) */
static void neighbour(int k, const char *name, float y, float h, int text_above)
{
    char s[48];
    int i = 0;
    if (k > 0) {
        three(s, k);
        s[3] = s[4] = ' ';
        i = 5;
    }
    while (*name && i < 46)
        s[i++] = *name++;
    s[i] = 0;
    ui_RR(52.f, y, 136.f, h, 5.f, UI_G3, 1.2f, QD_NONE);
    ui_T(s, 120.f, text_above ? y - 5.f : y + h - 8.f, 12.f, 500, 0.5f, UI_G2, QD_CENTER);
}

void ui_picker_draw(const ui_picker_t *v)
{
    const float dy = ui_ease(v->roll) * 22.f * (float)(v->dir < 0 ? -1 : 1);
    const float x = 24.f, y = 64.f + dy, w = 192.f, h = 94.f;
    const int off = v->k <= 0;
    const qd_rgb cc = off ? UI_G1 : UI_K3;
    float nw;
    char s[40], n[12];
    int j;

    /* the top line: SOUND (a lock: STEP n · SOUND and a padlock), the track at the right */
    s[0] = 0;
    if (v->lock) {
        const char *a = "STEP ";
        int i = 0, k = 0;
        while (a[i])
            s[k++] = a[i++];
        ui_itoa(n, v->step);
        for (i = 0; n[i]; i++)
            s[k++] = n[i];
        a = " \xc2\xb7 SOUND";
        for (i = 0; a[i]; i++)
            s[k++] = a[i];
        s[k] = 0;
    } else {
        s[0] = 'S', s[1] = 'O', s[2] = 'U', s[3] = 'N', s[4] = 'D', s[5] = 0;
    }
    ui_T(s, 8.f, 18.f, 12.f, 600, 0.8f, UI_K3, QD_LEFT);
    if (v->lock)
        ui_lock(8.f + ui_TW(s, 12.f, 600, 0.8f) + 4.f, 7.f, off ? UI_G2 : UI_K3, 1.f);
    (void)nw;                                           /* (P1.3r: a pop-up: no track box or name in its head) */

    qd_save();
    qd_clip_rect(0.f, 26.f, 240.f, 172.f);
    if (v->prev)
        neighbour(v->k - 1, v->prev, 44.f + dy, 14.f, 1);
    if (v->next)
        neighbour(v->k + 1, v->next, 166.f + dy, 26.f, 0);
    /* the cassette */
    ui_RR(x, y, w, h, 6.f, cc, 1.5f, QD_NONE);
    ui_RR(x + 10.f, y + 8.f, w - 20.f, 40.f, 3.f, UI_G2, 1.f, QD_NONE);
    if (off) {
        ui_T("TRACK", x + 16.f, y + 22.f, 12.f, 600, 0.5f, UI_G1, QD_LEFT);   /* (P1.3v: 12 px; no NO LOCK) */
    } else {
        three(n, v->k);
        ui_T(n, x + 16.f, y + 22.f, 12.f, 600, 0.5f, UI_K3, QD_LEFT);
    }
    if (v->cur) {                                       /* the name, as big as fits */
        const float tw = ui_TW(v->cur, 22.f, 400, 0.5f);
        const float sz = tw > 164.f ? 22.f * 164.f / tw : 22.f;
        ui_T(v->cur, x + w / 2.f, y + 42.f, sz < 12.f ? 12.f : sz, 400, 0.5f, cc, QD_CENTER);
    }
    for (j = 0; j < 2; j++) {                           /* the reels */
        const float rx = x + w * (j ? 0.68f : 0.32f), ry = y + h - 22.f;
        int k;
        ui_O(rx, ry, 11.f, UI_G1, 1.3f, QD_NONE);
        for (k = 0; k < 3; k++) {
            const float an = v->reel + (float)k * UI_TAU / 3.f + (float)j;
            qd_line(rx + cosf(an) * 3.5f, ry + sinf(an) * 3.5f, rx + cosf(an) * 8.5f, ry + sinf(an) * 8.5f, UI_G1,
                    1.2f);
        }
    }
    qd_line(x + w * 0.32f + 11.f, y + h - 11.f, x + w * 0.68f - 11.f, y + h - 11.f, UI_G2, 1.f);
    qd_restore();

    {   /* where it is in the pool: thirteen banks of ten */
        const int nb = (v->n + 9) / 10 < 1 ? 1 : (v->n + 9) / 10 > 13 ? 13 : (v->n + 9) / 10;
        const int cb = off ? -1 : (v->k - 1) / 10;
        const float bw = 14.f, gap = 2.5f, x0 = 120.f - (13.f * bw + 12.f * gap) / 2.f;
        for (j = 0; j < 13; j++) {
            const float bx = x0 + (float)j * (bw + gap);
            qd_line(bx, 212.f, bx + bw, 212.f, j == cb ? UI_K3 : j < nb ? UI_G2 : UI_G3, j == cb ? 2.f : 1.5f);
        }
        if (cb >= 0 && cb < 13) {
            const float bx = x0 + (float)cb * (bw + gap), tx = bx + ((float)((v->k - 1) % 10) + 0.5f) / 10.f * bw;
            qd_line(tx, 207.f, tx, 212.f, UI_K3, 1.5f);
            s[0] = 0;
            ui_itoa(s, cb * 10 + 1);
            for (j = 0; s[j]; j++)
                ;
            s[j++] = (char)0xe2, s[j++] = (char)0x80, s[j++] = (char)0x93;   /* "–" */
            ui_itoa(s + j, cb * 10 + 10 > v->n && v->n > cb * 10 ? v->n : cb * 10 + 10);
            ui_lab(s, bx + bw / 2.f, 228.f, UI_G1, QD_CENTER);
        }
    }
}
