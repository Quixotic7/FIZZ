/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* Latched menus and confirmations (ctl.h; PLAN §8b version 2.2). A latched menu stays on screen once it is opened,
 * until it is left; a confirmation (later: "clear the track?") stays until it is answered. One rule for every one of
 * them, so the hands learn it once:
 *
 *   OCT-   back / exit / cancel      (a latch's on_back; none: the latch closes)
 *   OCT+   yes / confirm / enter     (a latch's on_confirm; none: nothing, the key is reserved)
 *   HOME   closes any latch
 *
 * P1.6h: the main pages (QD_LATCH_MAIN: the mix, the effects, the sound editor, the MIDI FX screen) are no menus: F1 /
 * F2 there are the step view's layers (copy, pick-up / drop on the steps), and they close with HOME or their own key.
 *
 * The OCT key that answered a latch is eaten (qd_ui_t.oct_eaten) until it is let go: it is no F1 / F2 layer meanwhile,
 * so nothing it does after leaves the latch leaks into a layer. A new latch is a row of LATCHES (the sound editor's
 * pages and the dialogs to come).
 *
 *   F3     (OCT- and OCT+ pressed together, in the step view with nothing held) the rate, the page's range, the tempo:
 *          everything F3 did while held works while it is open; OCT- exits, OCT+ does nothing yet
 *   REC    (P1.3y: SHIFT + REC) the record settings: KNOB 1 MODE (LIVE / STEP), KNOB 2 QUANTIZE (0..100 %), KNOB 3
 *          OVERDUB / REPLACE; the project's
 *   SETTINGS (P1.3y: SHIFT + SEL) the global settings: KNOB 1 PAGE FOLLOW */
#include "app.h"
#include "ui.h"

static void f3_close(qd_app_t *a) { a->ui.f3_anchor = -1; }
static void br_closed(qd_app_t *a);                     /* (ctl_presets.c) */
static void snd_close(qd_app_t *a) { a->ui.ekeys = 0, a->ui.snd_lk = 0; }   /* (P1.5b: the editor's keyboard ends with it) */
static void mf_closed(qd_app_t *a);                     /* (ctl_mfx.c) */
static void tl_revert(qd_app_t *a);                     /* (ctl_tools.c) */
static void tl_confirm(qd_app_t *a);
static void tl_closed(qd_app_t *a);

static const qd_latch_t LATCHES[QD_NLATCH] = {
    [QD_LATCH_F3] = {"F3", "EXIT", 0, 0, 0, f3_close},
    [QD_LATCH_REC] = {"REC", "BACK", 0, 0, 0, 0},      /* P1.3y: the record settings */
    [QD_LATCH_SET] = {"SETTINGS", "BACK", 0, 0, 0, 0}, /* P1.3y: the global settings */
    [QD_LATCH_SND] = {"SOUND", "BACK", 0, 0, 0, snd_close},   /* P1.4a: the sound editor (ctl_snd.c) */
    [QD_LATCH_TRK] = {"TRACK", "BACK", 0, 0, 0, 0},    /* P1.4b: the track settings (ctl_mix.c) */
    [QD_LATCH_MIX] = {"MIX", "BACK", 0, 0, 0, 0},      /* P1.4b: the mix */
    [QD_LATCH_FX] = {"FX", "BACK", 0, 0, 0, 0},        /* P1.4b: the effects */
    [QD_LATCH_BROWSE] = {"PRESETS", "CANCEL", "LOAD", 0, 0, br_closed},   /* P1.5t: the library browser */
    [QD_LATCH_NAME] = {"PROJECT NAME", "BACK", 0, 0, 0, 0},   /* P1.5a: the name editor (P1.5u: the typewriter) */
    [QD_LATCH_PRJ] = {"PROJECT", "BACK", 0, 0, 0, 0}, /* P1.5a: the project pages */
    [QD_LATCH_SAVE] = {"SAVE PRESET", "CANCEL", "REPLACE", 0, 0, 0},   /* P1.5t: SAVE PRESET */
    [QD_LATCH_MFX] = {"MIDI FX", "BACK", 0, 0, 0, mf_closed},   /* P1.6c: the MIDI FX screen (ctl_mfx.c) */
    [QD_LATCH_TOOLS] = {"TOOLS", "REVERT", "KEEP", tl_revert, tl_confirm, tl_closed},   /* T1: the track tools */
};

const qd_latch_t *qd_latch_cur(const qd_app_t *a)
{
    const int k = a->ui.latch;
    return k > QD_LATCH_NONE && k < QD_NLATCH ? &LATCHES[k] : 0;
}

void qd_latch_open(qd_app_t *a, int kind)
{
    if (kind <= QD_LATCH_NONE || kind >= QD_NLATCH)
        return;
    if (a->ui.latch && a->ui.latch != kind)
        qd_latch_close(a);
    a->ui.seq_used &= ~2;                               /* (2026-10-25: the latched patterns map closed by any page) */
    a->ui.latch = kind;
}

void qd_latch_close(qd_app_t *a)
{
    const qd_latch_t *l = qd_latch_cur(a);
    if (!l)
        return;
    if (l->on_close)
        l->on_close(a);
    a->ui.latch = QD_LATCH_NONE;
}

int qd_latch_oct(qd_app_t *a, int d)
{
    const qd_latch_t *l = qd_latch_cur(a);
    if (!l || QD_LATCH_MAIN(a->ui.latch))               /* (P1.6h: a main page: F1 / F2 no back, no confirm) */
        return 0;
    a->ui.oct_eaten |= (uint8_t)(d < 0 ? 1u : 2u);
    if (d < 0) {
        if (l->on_back)
            l->on_back(a);
        else
            qd_latch_close(a);
    } else if (l->on_confirm) {
        l->on_confirm(a);
    }
    return 1;
}

/* ---------------------------------------------------------------- P1.4a: the sound editor */
#include <string.h>
#include "../seq/scale.h"
#include "ctl_snd.c"
#include "../voice/fx/qd_fx.h"
#include "ctl_mix.c"
#include "../seq/sounds.h"
#include "../seq/names.h"
#include <stdio.h>
#include <string.h>
#include "ctl_presets.c"
#include "ctl_lib.c"
#include "ctl_type.c"
#include "ctl_shift.c"
#include "ctl_mfx.c"                                    /* P1.6c: the MIDI FX screen */
#include "ctl_tools.c"                                  /* T1: the track tools */
