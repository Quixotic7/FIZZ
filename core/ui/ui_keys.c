/* SPDX-License-Identifier: GPL-3.0-only */
/* KEYS (PLAN.md §8 "KEYS mode"; mockups/line.html s18 without the recording roll): the keybed plays the selected track.
 * Top left the track's box and name, top right the pool's sixteen voices as dots (lit: in use; white this track's,
 * grey another's); the sound's name in the middle, medium; the line-drawn keybed of two octaves with the keys
 * sounding filled white; OCT at the foot. The keybed shows the two octaves from the C the keys' range starts in
 * (C4 at OCT 0), one lower when the notes sounding are all below it.
 * Recording (P1.3): rec 1, STEP recording, adds the cursor's page as sixteen little squares under the sound (trigs
 * filled grey, the cursor's square outlined red in the blink's lit half), STEP REC in red and the cursor's step at the
 * foot. rec 2, LIVE recording, is line.html s18: the RECORDING badge blinking (a Q by it with quantize on), the notes
 * scrolling in from the red line (11 px a step, the grid's beats darker), the keybed small at the foot with the keys
 * held white, OCT and the step under the playhead. */
#include <math.h>

#include "ui.h"

static const char *const OCT[5] = {"OCT -2", "OCT -1", "OCT 0", "OCT +1", "OCT +2"};
static const char *oct_name(int o) { return OCT[o < -2 ? 0 : o > 2 ? 4 : o + 2]; }

static char *cat(char *d, const char *s)
{
    char *e = d;
    while (*e)
        e++;
    while (*s)
        *e++ = *s++;
    *e = 0;
    return d;
}

/* rec 2: LIVE recording (s18) */
static void live(const ui_keys_t *v)
{
    static qd_rgb fill[128];
    const float now = 186.f, pxs = 11.f;
    char s[40], n[12];
    int i, lo = 60 + 12 * v->octave, mn = 128, rl;
    float st;

    ui_head(v->name, 0, v->track, UI_K3, UI_G1);
    ui_RR(150.f, 4.f, 82.f, 17.f, 3.f, QD_NONE, 1.f, v->blink ? UI_RED : 0xa01c2au);
    ui_T("RECORDING", 191.f, 16.f, 9.f, 700, 1.f, 0xffffffu, QD_CENTER);
    if (v->quantize)
        ui_T("Q", 143.f, 16.f, 9.f, 700, 1.f, UI_G1, QD_RIGHT);

    /* the keybed from the octave's C, one lower when the roll has notes below it; the roll two semitones under it */
    for (i = 0; i < 128; i++)
        fill[i] = v->sounding[i] ? UI_K3 : QD_NONE;
    for (i = 0; i < v->nroll && i < 32; i++) {
        if (v->roll[i].note < mn)
            mn = v->roll[i].note;
        if (v->roll[i].live)
            fill[v->roll[i].note & 127] = UI_K3;
    }
    if (mn < lo && lo >= 12)
        lo -= 12;
    rl = lo - 2;

    for (st = ceilf(v->pos - 16.f); st <= v->pos; st += 1.f) {   /* the grid, scrolling with the playhead */
        const float x = now - (v->pos - st) * pxs;
        const float g[4] = {x, 40.f, x, 156.f};
        ui_P(g, 2, (int)st % 4 == 0 ? UI_G3 : 0x121216u, 1.f, 0, QD_NONE);
    }
    {
        const float r[4] = {now, 34.f, now, 160.f};
        ui_P(r, 2, UI_RED, 1.f, 0, QD_NONE);
    }
    qd_save();
    qd_clip_rect(0.f, 30.f, 240.f, 132.f);
    for (i = 0; i < v->nroll && i < 32; i++) {          /* the notes played, scrolling left from the red line */
        const float x0 = now - v->roll[i].start * pxs, x1 = now - v->roll[i].end * pxs;
        const float xa = x0 > 8.f ? x0 : 8.f, w = x1 - xa;
        const float y = 152.f - (float)(v->roll[i].note - rl) * 6.5f;
        if (x1 < 8.f)
            continue;
        ui_RR(xa, y - 2.5f, w > 3.f ? w : 3.f, 5.f, 2.5f, QD_NONE, 1.f, v->roll[i].live ? UI_K3 : UI_G1);
    }
    qd_restore();
    ui_keyboard(22.f, 170.f, 196.f, 48.f, lo, fill);
    s[0] = 0;
    cat(cat(cat(s, oct_name(v->octave)), " \xc2\xb7 STEP "), ui_itoa(n, v->step));
    ui_lab(s, 120.f, 234.f, UI_G2, QD_CENTER);
}

void ui_keys_draw(const ui_keys_t *v)
{
    static qd_rgb fill[128];
    char num[24], step[12];
    int i, lo = 60 + 12 * v->octave, below = 0, above = 0;
    float x;

    if (v->rec == 2) {
        live(v);
        return;
    }

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

    ui_lab(oct_name(v->octave), 120.f, 222.f, UI_G2, QD_CENTER);

    if (v->rec == 1) {                                  /* STEP recording: the cursor's page, the cursor's step */
        const int c = (v->cursor - 1) % 16;
        for (i = 0; i < 16; i++) {
            const float sx = 14.f + (float)i * 13.5f + (float)(i / 4) * 2.f;
            const int cur = i == c && v->blink;
            ui_RR(sx, 99.f, 9.f, 9.f, 2.f, cur ? UI_RED : UI_G3, 1.f, v->page_trig[i] ? UI_G2 : QD_NONE);
        }
        ui_lab("STEP REC", 14.f, 222.f, UI_RED, QD_LEFT);
        num[0] = 0;
        ui_lab(cat(cat(num, "STEP "), ui_itoa(step, v->cursor)), 226.f, 222.f, UI_G1, QD_RIGHT);
    }
}
