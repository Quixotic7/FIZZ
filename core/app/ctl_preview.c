/* SPDX-License-Identifier: GPL-3.0-only */
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
 * Nothing here writes the pattern or the sequencer: only the pool (through the player) and this record. */
#include <string.h>

#include "app.h"

int qd_ctl_previewing(const qd_app_t *a)
{
    return a->ui.mode == QD_MODE_STEP && !a->seq.playing && a->ui.nheld > 0;
}

static uint32_t fnv(uint32_t h, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--)
        h = (h ^ *b++) * 16777619u;
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
    }
    u->pvs_n--;
}

static int is_held(const qd_ui_t *u, int s)
{
    int i;
    for (i = 0; i < u->nheld; i++)
        if (u->held[i] == s)
            return 1;
    return 0;
}

void qd_ctl_preview_sync(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    const int on = qd_ctl_previewing(a);
    int i, j;
    for (i = u->pvs_n - 1; i >= 0; i--) {               /* gone, or changed: released */
        const int keep = on && u->pvs_track[i] == a->sel && is_held(u, u->pvs_step[i]) &&
                         sig_of(a, a->sel, u->pvs_step[i]) == u->pvs_sig[i];
        if (!keep) {
            release(a, i);
            drop(u, i);
        }
    }
    if (!on)
        return;
    for (i = 0; i < u->nheld && u->pvs_n < 16; i++) {   /* held and not sounding: struck */
        const int s = u->held[i];
        int found = 0;
        for (j = 0; j < u->pvs_n; j++)
            found |= u->pvs_step[j] == s;
        if (found)
            continue;
        j = u->pvs_n++;
        u->pvs_step[j] = (int16_t)s;
        u->pvs_track[j] = (uint8_t)a->sel;
        u->pvs_sig[j] = sig_of(a, a->sel, s);
        u->pvs_nn[j] = (uint8_t)qd_player_preview(&a->player, a->seq.cur, a->sel, s, u->pvs_note[j]);
    }
}
