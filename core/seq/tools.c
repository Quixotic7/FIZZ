/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* T1: the track tools' operations (tools.h). Built through pattern.c (it includes this file: the source lists stay as
 * they are; copy_steps and the pool's helpers are its own). Nothing here keeps state; the temporaries are on the stack
 * (a step, a 128-byte map), never a buffer. */
#include "tools.h"

int qd_tl_positions(const qd_pattern_t *p, int t, int scope, int page, uint8_t *pos)
{
    const qd_tset_t *ts = &p->trk[t];
    int i, n;
    if (scope == QD_TS_PAGE) {
        const int a = ts->pstart[page & 7] & 15, b = ts->pend[page & 7] & 15;
        n = b >= a ? b - a + 1 : 1;
        for (i = 0; i < n; i++)
            pos[i] = (uint8_t)((page & 7) * QD_PAGE + a + i);
        return n;
    }
    n = qd_track_len(ts);
    for (i = 0; i < n; i++)
        pos[i] = (uint8_t)qd_track_loop_step(ts, i);
    return n;
}

int qd_tl_pos_of(const uint8_t *pos, int n, int s)
{
    int i;
    for (i = 0; i < n; i++)
        if (pos[i] == s)
            return i;
    return -1;
}

/* the track's locks on the list's steps renumbered (position j -> map(j)), its range sorted again */
static void tl_locks(qd_pattern_t *p, int t, const uint8_t *pos, int n, int k, int rev)
{
    uint8_t inv[QD_NSTEP];
    qd_lock_t *L = &p->lock[p->lock_start[t]];
    const int m = p->lock_n[t];
    int i;
    memset(inv, 0xFF, sizeof inv);
    for (i = 0; i < n; i++)
        inv[pos[i]] = (uint8_t)i;
    for (i = 0; i < m; i++) {
        const int j = inv[L[i].step];
        if (j != 0xFF)
            L[i].step = pos[rev ? n - 1 - j : (j + k) % n];
    }
    for (i = 1; i < m; i++) {                           /* (insertion: few move far) */
        const qd_lock_t x = L[i];
        int j = i - 1;
        while (j >= 0 && (L[j].step > x.step || (L[j].step == x.step && L[j].id > x.id))) {
            L[j + 1] = L[j];
            j--;
        }
        L[j + 1] = x;
    }
}

static int tl_gcd(int a, int b)
{
    while (b) {
        const int r = a % b;
        a = b, b = r;
    }
    return a;
}

void qd_tl_rotate(qd_pattern_t *p, int t, const uint8_t *pos, int n, int k)
{
    qd_step_t *S = p->step[t];
    int c, g;
    if (n < 2)
        return;
    k %= n;
    if (k < 0)
        k += n;
    if (!k)
        return;
    g = tl_gcd(n, k);
    for (c = 0; c < g; c++) {                           /* (gcd(n, k) cycles, one step held) */
        const qd_step_t tmp = S[pos[c]];
        int j = c;
        for (;;) {
            const int src = (j - k + n) % n;
            if (src == c)
                break;
            S[pos[j]] = S[pos[src]];
            j = src;
        }
        S[pos[j]] = tmp;
    }
    tl_locks(p, t, pos, n, k, 0);
}

void qd_tl_reverse(qd_pattern_t *p, int t, const uint8_t *pos, int n)
{
    qd_step_t *S = p->step[t];
    int i;
    for (i = 0; i < n / 2; i++) {
        const qd_step_t tmp = S[pos[i]];
        S[pos[i]] = S[pos[n - 1 - i]];
        S[pos[n - 1 - i]] = tmp;
    }
    for (i = 0; i < n; i++)                             /* (the timing mirrors) */
        if (S[pos[i]].ovr & QD_OV(QD_PM_MICRO))
            S[pos[i]].micro = -S[pos[i]].micro;
    if (n > 1)
        tl_locks(p, t, pos, n, 0, 1);
}

int qd_tl_euclid_hit(int i, int hits, int len, int rot)
{
    if (len < 1)
        return 0;
    return (((i % len) + rot) * hits) % len < hits;
}

void qd_tl_euclid(qd_pattern_t *p, int t, const uint8_t *pos, int n, int hits, int len, int rot, int mode)
{
    int i;
    for (i = 0; i < n; i++) {
        const int hit = qd_tl_euclid_hit(i, hits, len, rot), on = (p->step[t][pos[i]].flags & QD_SF_ON) != 0;
        if (hit && !on && mode != QD_EU_MASK)
            qd_step_on(p, t, pos[i]);
        else if (!hit && on && mode != QD_EU_ADD)
            qd_step_clear(p, t, pos[i]);
    }
}

int qd_tl_double(qd_pattern_t *p, int t, qd_tset_t *only)
{
    qd_tset_t *ts = only ? only : &p->trk[t];
    const int loop = qd_track_loop(ts);
    int pages[QD_NPAGE], n = 0, last = -1, k, mask = loop;
    for (k = 0; k < QD_NPAGE; k++)
        if ((loop >> k) & 1)
            pages[n++] = k, last = k;
    if (last + n >= QD_NPAGE)
        return 0;
    for (k = 0; k < n; k++) {
        const int to = last + 1 + k;
        if (!only && qd_page_copy(p, t, to, p, t, pages[k]) != QD_OK)   /* (P2.1b: no statics of its own) */
            return 0;
        ts->pstart[to] = ts->pstart[pages[k]];
        ts->pend[to] = ts->pend[pages[k]];
        mask |= 1 << to;
    }
    ts->loop = (uint8_t)mask;
    return 1;
}

static void tl_halve_page(qd_tset_t *ts, int page)
{
    const int a = ts->pstart[page] & 15, b = ts->pend[page] & 15, m = b >= a ? b - a + 1 : 1;
    ts->pend[page] = (uint8_t)(a + (m >= 2 ? m / 2 : 1) - 1);
}

void qd_tl_length(qd_pattern_t *p, int t, int page, int pw, qd_tset_t *only)
{
    qd_tset_t *ts = only ? only : &p->trk[t];
    for (; pw > 0; pw--) {
        if (page >= 0) {                                /* PAGE: the page's end twice as far (its steps copied on) */
            const int a = ts->pstart[page] & 15, b = ts->pend[page] & 15, m = b - a + 1;
            const int e = a + 2 * m - 1 > 15 ? 15 : a + 2 * m - 1;
            if (b >= 15)
                break;
            if (!only && copy_steps(p, t, page * QD_PAGE + b + 1, p, t, page * QD_PAGE + a, e - b) != QD_OK)
                break;
            ts->pend[page] = (uint8_t)e;
        } else if (!qd_tl_double(p, t, only)) {
            break;
        }
    }
    for (; pw < 0; pw++) {
        if (page >= 0) {
            tl_halve_page(ts, page);
        } else {                                        /* the later half of the looped pages out of the loop */
            const int loop = qd_track_loop(ts);
            int k, n = 0, keep, mask = 0;
            for (k = 0; k < QD_NPAGE; k++)
                n += (loop >> k) & 1;
            if (n < 2) {
                for (k = 0; k < QD_NPAGE && !((loop >> k) & 1); k++)
                    ;
                tl_halve_page(ts, k & 7);
                continue;
            }
            keep = (n + 1) / 2;
            for (k = 0; k < QD_NPAGE && keep; k++)
                if ((loop >> k) & 1)
                    mask |= 1 << k, keep--;
            ts->loop = (uint8_t)mask;
        }
    }
}
