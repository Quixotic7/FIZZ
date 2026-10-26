/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The octave takeover (P1.3c; PLAN.md §8b version 2.1, "octave feedback"): a change of NOTE's entry octave (OCT- /
 * OCT+ with a step held in NOTE), of the keyboard's octave, or of SOUND's bank of ten takes the whole screen for about
 * a second (the takeover's QD_TK_MS, then its 120 ms fade). The LINE look:
 *   - the number big in the middle (the octave, the keyboard's shift signed, the bank), at once (2026-10-26, the
 *     owner: the roll and the slide were too slow for playing)
 *   - under it the whole 88-key piano, still, with the keys in use lit: the entry octave's twelve (B1's note dotted in
 *     KNOB 1's colour), or the keybed's 27 keys F..G; for a bank, the pool as ticks in tens (the sounds there bright,
 *     the step's sound lock tall in SOUND's colour) with the bank's ten bracketed, sliding to it
 *   - at the foot OCT- and OCT+ with the distance from the centre as dots on the side that blinks (the LEDs' story:
 *     one dot slow, two faster, three or more fastest), the line under the number in grey
 * Nothing here reads a clock: the view gives the slide's progress (k) and the seconds since the change (t). */
#include <math.h>
#include "fz_math.h"                   /* (the drawing's own sinf .. atan2f: the flash diet) */

#include "ui.h"

static const int WI[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
static const int ISB[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

static void line2(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, col, lw, 0, QD_NONE);
}

/* note n's white-key index from note 0 (a black key: the white key it sits after) */
static int wx(int n) { return n / 12 * 7 + WI[n % 12]; }

/* 2026-10-26 (the owner: the slide was slow): a full 88-key keyboard (A0 .. C8) across the screen, keys lo..hi (the
 * two octaves F .. G the keybed plays, or the entry octave) filled bright, the rest outlined grey; mark a note dotted in
 * mcol. Nothing moves */
static void piano88(float y, float h, int lo, int hi, int mark, qd_rgb mcol)
{
    const float x0 = 6.f, kw = 228.f / 52.f, bh = h * 0.6f, bw = kw * 0.62f, w0 = (float)wx(21);
    int n;
    for (n = 21; n <= 108; n++) {                       /* the white keys */
        const float x = x0 + ((float)wx(n) - w0) * kw;
        const int in = n >= lo && n <= hi;
        if (ISB[n % 12])
            continue;
        ui_RR(x, y, kw - 0.6f, h, 1.f, in ? QD_NONE : UI_G3, 1.f, in ? UI_K3 : QD_NONE);
        if (n % 12 == 0)                                /* a C: a grey tick at its foot */
            line2(x + kw / 2.f, y + h + 3.f, x + kw / 2.f, y + h + 6.f, UI_G2, 1.f);
    }
    for (n = 21; n <= 108; n++) {                       /* the black keys over them */
        const float x = x0 + ((float)(wx(n) + 1) - w0) * kw - bw / 2.f;
        const int in = n >= lo && n <= hi;
        if (!ISB[n % 12])
            continue;
        ui_RR(x, y - 1.f, bw, bh, 1.f, QD_NONE, 1.f, in ? UI_K3 : UI_G2);
    }
    if (mark >= 21 && mark <= 108) {
        const float x = ISB[mark % 12] ? x0 + ((float)(wx(mark) + 1) - w0) * kw : x0 + ((float)wx(mark) + 0.5f - w0) * kw;
        ui_O(x, ISB[mark % 12] ? y + bh - 6.f : y + h - 6.f, 2.4f, QD_NONE, 1.f, mcol);
    }
}

/* the pool as ticks in tens, bank b's ten bracketed, centred on bank position pos (eased) */
static void pool_strip(float pos, int bank, int nsound, int nbank, int sound, float y)
{
    const float bw = 50.f, tick = 4.6f, x0 = 120.f - (pos + 0.5f) * bw;
    int b, k;
    char n[12];
    qd_save();
    qd_clip_rect(6.f, y - 22.f, 228.f, 60.f);
    for (b = 0; b < nbank && b < 13; b++) {
        const float bx = x0 + (float)b * bw + 2.f;
        for (k = 0; k < 10; k++) {
            const int snd = b * 10 + k + 1;
            const float x = bx + 1.5f + (float)k * tick;
            const int have = snd <= nsound, lock = snd == sound;
            const qd_rgb col = lock ? UI_MODE_COL[UI_F_SOUND] : !have ? UI_G3 : b == bank ? UI_K3 : UI_G2;
            line2(x, lock ? y - 8.f : y, x, y + 16.f, col, lock ? 2.f : 1.3f);
        }
        (void)n;                                        /* (P1.3v: no 7 px numbers: the sub line names the bank) */
        if (b == bank) {                                /* the bank's bracket */
            line2(bx - 1.f, y - 12.f, bx + 10.f * tick + 1.f, y - 12.f, UI_K3, 1.5f);
            line2(bx - 1.f, y - 12.f, bx - 1.f, y - 8.f, UI_K3, 1.5f);
            line2(bx + 10.f * tick + 1.f, y - 12.f, bx + 10.f * tick + 1.f, y - 8.f, UI_K3, 1.5f);
        }
    }
    qd_restore();
}

/* the number's text: the octave, the keyboard's shift signed ("+1", "0", "-2" with a true minus), the bank 1-based */
static void num_text(const ui_octave_t *v, int val, char *s)
{
    char n[12];
    s[0] = 0;
    if (v->kind == UI_OCT_KEYS && val) {
        const char *sign = val > 0 ? "+" : "\xe2\x88\x92";
        int i = 0;
        while (sign[i])
            s[i] = sign[i], i++;
        ui_itoa(n, val < 0 ? -val : val);
        for (val = 0; n[val]; val++)
            s[i++] = n[val];
        s[i] = 0;
        return;
    }
    ui_itoa(s, v->kind == UI_OCT_BANK ? val + 1 : val);
}

void ui_octave_draw(const ui_octave_t *v)
{
    static const char *const TITLE[3] = {"ENTRY OCTAVE", "KEYBOARD OCTAVE", "SOUND BANK"};
    const int kind = v->kind < 0 ? 0 : v->kind > 2 ? 2 : v->kind;
    const float k = v->k < 0.f ? 0.f : v->k > 1.f ? 1.f : v->k;
    const int centre = 0;
    char s[16];
    int off, i;

    ui_T(TITLE[kind], 8.f, 18.f, 12.f, 600, 0.8f, UI_K3, QD_LEFT);   /* (P1.3r: a pop-up: no track in its head) */

    /* the number, at once (2026-10-26, the owner: no roll) */
    num_text(v, v->value, s);
    ui_T(s, 120.f, 112.f, 92.f, 200, 0.f, UI_K3, QD_CENTER);
    ui_lab(v->sub, 120.f, 132.f, UI_G1, QD_CENTER);

    /* the whole piano with the keys in use lit (NOTE: the entry octave, B1's note dotted; KEYS: the keybed's F .. G);
     * the pool still slides to its bank */
    if (kind == UI_OCT_NOTE) {                          /* octave o's C is MIDI 12 (o + 1) */
        const int c = 12 * (v->value + 1);
        piano88(150.f, 40.f, c, c + 11, v->lo_note, UI_K1);
    } else if (kind == UI_OCT_KEYS) {
        piano88(150.f, 40.f, v->lo_note, v->lo_note + 26, -1, UI_K3);
    } else {
        const float pos = (float)v->from + (float)(v->value - v->from) * k;
        pool_strip(pos, v->value, v->nsound, v->nbank, v->sound, 160.f);
    }

    /* OCT- / OCT+ and the distance from the centre: the dots on the side whose LED blinks */
    off = kind == UI_OCT_NOTE ? v->value - 4 : v->value - centre;
    for (i = 0; i < 3; i++) {                           /* (P1.3v: the dots alone, as the LEDs: no OCT- / OCT+ words) */
        const int lit_l = off < 0 && i < -off, lit_r = off > 0 && i < off;
        ui_O(52.f + (float)i * 9.f, 217.f, 2.4f, lit_l ? UI_K3 : UI_G3, 1.f, lit_l ? UI_K3 : QD_NONE);
        ui_O(188.f - (float)i * 9.f, 217.f, 2.4f, lit_r ? UI_K3 : UI_G3, 1.f, lit_r ? UI_K3 : QD_NONE);
    }
    (void)v->t;
}
