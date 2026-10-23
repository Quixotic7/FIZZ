/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The stopped preview (ctl.h; PLAN §8b version 2.2): with the transport stopped, a held step sounds. Its notes at its
 * velocity, with its sound (the pool sound it locks, else the track's), its p-locks and mix locks, the track's limit:
 * the record the sequencer would make for it (qd_seq_step_rec), struck through the player like a key's notes
 * (qd_player_preview). Several held steps sound together; the step latch's selection (LFO) counts as held.
 *
 * qd_ctl_preview_sync runs after every input and every millisecond and makes the sound match the held steps:
 *   a step held now and not sounding           struck
 *   a step sounding whose step changed         released and struck again (an edit re-strikes it: its resolved values
 *                                              or its locks, a signature of both)
 *   a step sounding that is no longer held     released (a note another sounding step also plays stays)
 *   playing, or not the step view              everything released, nothing struck
 * Nothing here writes the pattern or the sequencer: only the pool (through the player) and this record.
 *
 * P1.6e (the owner): a step with a MIDI FX group (its lock, else the track's) previews through it: the arp, the chord ..
 * heard; when the last held step is let go the group's notes of it end (a HOLD arp's too: nothing stuck).
 *
 * P1.5s (the owner: "When you have Lock on, it is continuously playing the currently selected step. It should only play
 * the sound of the step while holding the step or if you turn the step selection knob"): only a FINGER sounds as
 * above (a W down on the step; with the latch off every held step is one). The step latch's selection alone is silent:
 * it sounds once, QD_PREVIEW_MS, when the step cursor (ALGORITHM) moves it to a step (qd_ctl_preview_once), and never
 * again while it stands or is edited. */
#include <string.h>

#include "app.h"

int qd_ctl_previewing(const qd_app_t *a)
{
    return a->ui.mode == QD_MODE_STEP && !a->seq.playing && a->ui.nheld > 0;
}

static uint32_t fnv(uint32_t h, const void *p, size_t n)   /* FNV-1a over n bytes (the published 32-bit prime) */
{
    size_t i;
    for (i = 0; i < n; i++) {
        h ^= ((const uint8_t *)p)[i];
        h *= 0x01000193u;
    }
    return h;
}

/* what the step sounds like: its resolved values and its locks */
static uint32_t sig_of(const qd_app_t *a, int t, int s)
{
    qd_rstep_t r;
    int n;
    const qd_lock_t *l = qd_locks(qd_ctl_cpat(a), t, s, &n);
    uint32_t h = 2166136261u;
    qd_step_resolve(qd_ctl_cpat(a), t, s, &r);           /* (it clears the record first: no stray padding) */
    h = fnv(h, &r, sizeof r);
    h = fnv(h, &n, sizeof n);
    if (n > 0)
        h = fnv(h, l, (size_t)n * sizeof *l);
    return h;
}

/* entry i's notes released, but those another entry on its track still plays */
static void release(qd_app_t *a, int i)
{
    qd_ui_t *u = &a->ui;
    int k, j, m;
    for (k = 0; k < u->pvs_nn[i]; k++) {
        int shared = 0;
        for (j = 0; j < u->pvs_n && !shared; j++)
            if (j != i && u->pvs_track[j] == u->pvs_track[i])
                for (m = 0; m < u->pvs_nn[j]; m++)
                    shared |= u->pvs_note[j][m] == u->pvs_note[i][k];
        if (!shared)
            qd_player_key_off(&a->player, u->pvs_track[i], u->pvs_note[i][k]);
    }
}

static void drop(qd_ui_t *u, int i)
{
    for (; i + 1 < u->pvs_n; i++) {
        u->pvs_step[i] = u->pvs_step[i + 1];
        u->pvs_track[i] = u->pvs_track[i + 1];
        u->pvs_nn[i] = u->pvs_nn[i + 1];
        memcpy(u->pvs_note[i], u->pvs_note[i + 1], sizeof u->pvs_note[i]);
        u->pvs_sig[i] = u->pvs_sig[i + 1];
        u->pvs_until[i] = u->pvs_until[i + 1];
    }
    u->pvs_n--;
}

/* a finger on step s: the latch off, any held step; on, a W down on it */
static int fingered(const qd_app_t *a, int s)
{
    int w;
    if (!a->ui.slatch)
        return 1;
    for (w = 0; w < 16; w++)
        if (((a->keys_held >> QD_WHITE_KEY[w]) & 1u) && a->ui.page[a->sel] * QD_PAGE + w == s)
            return 1;
    return 0;
}

static int is_held(const qd_ui_t *u, int s)
{
    int i;
    for (i = 0; i < u->nheld; i++)
        if (u->held[i] == s)
            return 1;
    return 0;
}

static void mark_grp(qd_app_t *a, int s)                /* (P1.6e: through a group: its notes end with the preview) */
{
    const int g = qd_step_mfx(qd_ctl_cpat(a), a->sel, s);
    if (g && qd_mfx_group_active(&a->proj.mfx[g - 1]))
        a->ui.pvs_grp = a->sel + 1;
}

void qd_ctl_preview_sync(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    const int on = qd_ctl_previewing(a);
    int i, j;
    for (i = u->pvs_n - 1; i >= 0; i--) {               /* gone, or changed: released */
        int keep = on && u->pvs_track[i] == a->sel && is_held(u, u->pvs_step[i]);
        if (keep && u->pvs_until[i] && fingered(a, u->pvs_step[i]))
            u->pvs_until[i] = 0;                        /* (a once-preview a finger now holds: the finger's) */
        if (keep)
            keep = u->pvs_until[i] ? (int32_t)(u->pvs_until[i] - a->ms) > 0
                                   : fingered(a, u->pvs_step[i]) && sig_of(a, a->sel, u->pvs_step[i]) == u->pvs_sig[i];
        if (!keep) {
            release(a, i);
            drop(u, i);
        }
    }
    if (!u->pvs_n && u->pvs_grp) {                      /* P1.6e: the preview over: its group's notes end */
        qd_player_preview_end(&a->player, u->pvs_grp - 1);
        u->pvs_grp = 0;
    }
    if (!on)
        return;
    for (i = 0; i < u->nheld && u->pvs_n < 16; i++) {   /* held and not sounding: struck */
        const int s = u->held[i];
        int found = 0;
        for (j = 0; j < u->pvs_n; j++)
            found |= u->pvs_step[j] == s;
        if (found || !fingered(a, s))
            continue;                                   /* (P1.5s: the latch's selection alone: silent) */
        j = u->pvs_n++;
        u->pvs_until[j] = 0;
        u->pvs_step[j] = (int16_t)s;
        u->pvs_track[j] = (uint8_t)a->sel;
        u->pvs_sig[j] = sig_of(a, a->sel, s);
        u->pvs_nn[j] = (uint8_t)qd_player_preview(&a->player, a->seq.cur, a->sel, s, u->pvs_note[j]);
        mark_grp(a, s);
    }
}

/* P1.5s: the step cursor moved the latch's selection onto step s: it sounds once (QD_PREVIEW_MS); a once-preview of
 * another step ends */
void qd_ctl_preview_once(qd_app_t *a, int s)
{
    qd_ui_t *u = &a->ui;
    int i, j;
    if (!qd_ctl_previewing(a))
        return;
    for (i = u->pvs_n - 1; i >= 0; i--)
        if (u->pvs_until[i] && u->pvs_step[i] != s) {
            release(a, i);
            drop(u, i);
        }
    for (i = 0; i < u->pvs_n; i++)
        if (u->pvs_step[i] == s)
            return;
    if (u->pvs_n >= 16)
        return;
    j = u->pvs_n++;
    u->pvs_step[j] = (int16_t)s;
    u->pvs_track[j] = (uint8_t)a->sel;
    u->pvs_sig[j] = sig_of(a, a->sel, s);
    u->pvs_until[j] = (a->ms + QD_PREVIEW_MS) | 1u;
    u->pvs_nn[j] = (uint8_t)qd_player_preview(&a->player, a->seq.cur, a->sel, s, u->pvs_note[j]);
    mark_grp(a, s);
}
