/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* T1: the track tools' operations (tools.h). Built through pattern.c (it includes this file: the source lists stay as
 * they are; copy_steps and the pool's helpers are its own). Nothing here keeps state; the temporaries are on the stack
 * (a step, a 128-byte map), never a buffer. */
#include "tools.h"
#include "grids_map.h"
#include "scale.h"

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

/* the steps permuted: position d takes what was at position from[d] (from: a permutation of 0..n-1), the track's locks
 * renumbered the same way and its range sorted again (cycle-following, one step's temporary, 16 bytes of marks) */
static void tl_permute(qd_pattern_t *p, int t, const uint8_t *pos, int n, const uint8_t *from)
{
    qd_step_t *S = p->step[t];
    uint8_t to[QD_NSTEP], seen[QD_NSTEP / 8];
    qd_lock_t *L = &p->lock[p->lock_start[t]];
    const int m = p->lock_n[t];
    int i, c;
    memset(seen, 0, sizeof seen);
    memset(to, 0xFF, sizeof to);
    for (c = 0; c < n; c++) {
        to[pos[from[c]]] = pos[c];                      /* (the physical step each one goes to: the locks') */
        if (!(seen[c >> 3] >> (c & 7) & 1)) {
            const qd_step_t tmp = S[pos[c]];
            int d = c;
            while (from[d] != c) {
                S[pos[d]] = S[pos[from[d]]];
                seen[d >> 3] |= (uint8_t)(1u << (d & 7));
                d = from[d];
            }
            S[pos[d]] = tmp;
            seen[d >> 3] |= (uint8_t)(1u << (d & 7));
        }
    }
    for (i = 0; i < m; i++)
        if (to[L[i].step] != 0xFF)
            L[i].step = to[L[i].step];
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

void qd_tl_rotate(qd_pattern_t *p, int t, const uint8_t *pos, int n, int k)
{
    uint8_t from[QD_NSTEP];
    int d;
    if (n < 2)
        return;
    k %= n;
    if (k < 0)
        k += n;
    if (!k)
        return;
    for (d = 0; d < n; d++)
        from[d] = (uint8_t)((d - k + n) % n);
    tl_permute(p, t, pos, n, from);
}

void qd_tl_reverse(qd_pattern_t *p, int t, const uint8_t *pos, int n)
{
    qd_step_t *S = p->step[t];
    uint8_t from[QD_NSTEP];
    int i;
    for (i = 0; i < n; i++)
        from[i] = (uint8_t)(n - 1 - i);
    if (n > 1)
        tl_permute(p, t, pos, n, from);
    for (i = 0; i < n; i++)                             /* (the timing mirrors) */
        if (S[pos[i]].ovr & QD_OV(QD_PM_MICRO))
            S[pos[i]].micro = -S[pos[i]].micro;
}

int qd_tl_move_map(const uint8_t *pos, int n, const uint8_t *sel, int ns, int k, uint8_t *from)
{
    uint8_t in[QD_NSTEP / 8];
    int i, d = 0, moved = 0;
    if (n < 1)
        return 0;
    k %= n;
    if (k < 0)
        k += n;
    memset(in, 0, sizeof in);
    memset(from, 0xFF, (size_t)n);
    for (i = 0; i < ns; i++) {                          /* the selected: k on, round the list */
        const int j = qd_tl_pos_of(pos, n, sel[i]);
        if (j >= 0 && !(in[j >> 3] >> (j & 7) & 1)) {
            in[j >> 3] |= (uint8_t)(1u << (j & 7));
            from[(j + k) % n] = (uint8_t)j;
            moved = 1;
        }
    }
    for (i = 0; i < n; i++)                             /* the others into the places left, in their order */
        if (!(in[i >> 3] >> (i & 7) & 1)) {
            while (from[d] != 0xFF)
                d++;
            from[d] = (uint8_t)i;
        }
    return moved;
}

void qd_tl_move(qd_pattern_t *p, int t, const uint8_t *pos, int n, const uint8_t *sel, int ns, int k)
{
    uint8_t from[QD_NSTEP];
    if (qd_tl_move_map(pos, n, sel, ns, k, from))
        tl_permute(p, t, pos, n, from);
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

/* ---------------------------------------------------------------- STRETCH (2026-10-25) */
static int tl_fdiv(int a, int b) { return a >= 0 ? a / b : -((b - 1 - a) / b); }   /* (floor; b > 0) */
static int tl_clampi(int v, int lo, int hi);
static int tl_abs(int v) { return v < 0 ? -v : v; }
void qd_tl_stretch_to(int i, int mic, int num, int den, int m, int *j, int *mo)
{
    const int tt = tl_fdiv(2 * (i * 24 + mic) * num + den, 2 * den);   /* (the new time in 24ths, rounded) */
    const int k = tl_clampi(tl_fdiv(tt + 12, 24), 0, m - 1);
    *j = k;
    *mo = tl_clampi(tt - k * 24, -23, 23);
}

static int tl_bit(const uint8_t *b, int i) { return b[i >> 3] >> (i & 7) & 1; }
static void tl_set(uint8_t *b, int i) { b[i >> 3] |= (uint8_t)(1u << (i & 7)); }

void qd_tl_stretch(qd_pattern_t *p, int t, const uint8_t *pos, int n, int base, int m, int num, int den)
{
    qd_step_t *S = p->step[t];
    const qd_sdef_t *d = &p->trk[t].def;
    uint8_t from[QD_NSTEP], id[QD_NSTEP], inv[QD_NSTEP / 8], used[QD_NSTEP / 8];
    int8_t mo[QD_NSTEP];                                /* (a step's new micro; -128: it takes no trig) */
    int i, s, k = 0;
    memset(from, 0xFF, sizeof from);
    memset(inv, 0, sizeof inv);
    memset(used, 0, sizeof used);
    memset(mo, 0x80, sizeof mo);
    for (i = 0; i < n; i++)
        tl_set(inv, pos[i]);
    for (i = 0; i < m && base + i < QD_NSTEP; i++)
        tl_set(inv, base + i);
    for (i = 0; i < n; i++) {                           /* (each trig to its new step: two there, the nearer kept) */
        const qd_step_t *st = &S[pos[i]];
        int j, o;
        if (!(st->flags & QD_SF_ON))
            continue;
        qd_tl_stretch_to(i, (st->ovr & QD_OV(QD_PM_MICRO) ? st->micro : d->micro) - d->micro, num, den, m, &j, &o);
        j += base;
        if (j >= QD_NSTEP || (mo[j] != -128 && tl_abs(o) >= tl_abs(mo[j])))
            continue;
        from[j] = pos[i], mo[j] = (int8_t)o;
    }
    for (s = 0; s < QD_NSTEP; s++) {                    /* (the others stay; the steps left over fill the rest) */
        id[s] = (uint8_t)s;
        if (!tl_bit(inv, s))
            from[s] = (uint8_t)s;
        if (from[s] != 0xFF)
            tl_set(used, from[s]);
    }
    for (s = 0; s < QD_NSTEP; s++)
        if (from[s] == 0xFF) {
            while (tl_bit(used, k))
                k++;
            from[s] = (uint8_t)k;
            tl_set(used, k);
        }
    tl_permute(p, t, id, QD_NSTEP, from);
    for (s = 0; s < QD_NSTEP; s++) {
        if (!tl_bit(inv, s))
            continue;
        if (mo[s] == -128) {                            /* (no trig here now: cleared, its locks with it) */
            if (S[s].flags)
                qd_step_clear(p, t, s);
            continue;
        }
        k = tl_clampi(mo[s] + d->micro, -23, 23);       /* (the track's nudge back on) */
        if (k == d->micro)
            qd_step_reset(p, t, s, QD_PM_MICRO);
        else
            qd_step_set_micro(p, t, s, k);
        if ((S[s].ovr & QD_OV(QD_PM_LEN)) && S[s].len != QD_LEN_INF) {   /* (its own length scaled: the nearest) */
            const int e = qd_len_eighths(S[s].len) * num / den;
            int b = 0, l;
            for (l = 1; l < QD_LEN_INF; l++)
                if (tl_abs(qd_len_eighths(l) - e) < tl_abs(qd_len_eighths(b) - e))
                    b = l;
            S[s].len = (uint16_t)b;
        }
    }
}

int qd_tl_stretch_track(qd_pattern_t *p, int t, int page, int *pwp, int fit, qd_tset_t *only)
{
    qd_tset_t *ts = only ? only : &p->trk[t];
    uint8_t pos[QD_NSTEP];
    int n, m, cap, base = 0, num, den, k, p0 = 0, pw = *pwp;
    n = qd_tl_positions(p, t, page >= 0 ? QD_TS_PAGE : QD_TS_TRACK, page, pos);   /* (ts: the track's own, so far) */
    if (page >= 0) {                                    /* (a page: its start..end; fit: 2 x fit steps) */
        cap = QD_PAGE - pos[0] % QD_PAGE, base = pos[0];
    } else {
        p0 = pos[0] / QD_PAGE;
        cap = QD_NSTEP;
    }
    if (fit) {
        m = page >= 0 ? 2 * fit : fit * QD_PAGE;
        m = m > cap ? cap : m;
        num = m, den = n;
    } else {
        while (pw > 0 && n << pw > cap)                 /* (as far as it fits) */
            pw--;
        *pwp = pw;
        num = pw > 0 ? 1 << pw : 1, den = pw < 0 ? 1 << -pw : 1;
        m = (n * num + den - 1) / den;
    }
    if (m == n && num == den)
        return n;
    if (page >= 0) {
        ts->pend[page & 7] = (uint8_t)((ts->pstart[page & 7] & 15) + m - 1);
    } else {
        const int np = (m + QD_PAGE - 1) / QD_PAGE;
        p0 = p0 + np > QD_NPAGE ? QD_NPAGE - np : p0;
        base = p0 * QD_PAGE;
        for (k = 0; k < np; k++)
            ts->pstart[p0 + k] = 0, ts->pend[p0 + k] = (uint8_t)(k == np - 1 ? (m - 1) % QD_PAGE : 15);
        ts->loop = (uint8_t)(((1u << np) - 1u) << p0);
    }
    if (!only)
        qd_tl_stretch(p, t, pos, n, base, m, num, den);
    return m;
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

/* ---------------------------------------------------------------- T2: the generators */
uint32_t qd_tl_hash(uint32_t seed, int t, int i)       /* (an integer mix: xorshift-multiply) */
{
    uint32_t h = seed * 0x9E3779B1u + (uint32_t)t * 0x85EBCA77u + (uint32_t)i * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

static int tl_clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int tl_spread(uint32_t h, int k) { return k ? (int)(h % (uint32_t)(2 * k + 1)) - k : 0; }   /* -k..k */

int qd_tl_random_on(uint32_t seed, int t, int i, int dens)
{
    return (int)(qd_tl_hash(seed, t, i) & 0xFFu) * 100 < dens * 256;
}

void qd_tl_random(qd_pattern_t *p, int t, const uint8_t *pos, int n, uint32_t seed, int dens, int notes, int vel,
                  int chance, int scale, int root)
{
    const qd_sdef_t *d = &p->trk[t].def;
    int i;
    for (i = 0; i < n; i++) {
        const uint32_t h = qd_tl_hash(seed, t, i);
        const int s = pos[i], on = (p->step[t][s].flags & QD_SF_ON) != 0;
        if (!qd_tl_random_on(seed, t, i, dens)) {
            if (on)
                qd_step_clear(p, t, s);
            continue;
        }
        if (!on)
            qd_step_on(p, t, s);
        if (notes) {                                    /* (degrees of the track's scale round its default note) */
            int k = tl_spread(h >> 8, notes), nt = d->note[0] ? d->note[0] : 60;
            for (; k > 0; k--)
                nt = qd_scale_step(scale, root, nt, 1);
            for (; k < 0; k++)
                nt = qd_scale_step(scale, root, nt, -1);
            qd_step_set_note(p, t, s, 0, tl_clampi(nt, 1, 127));
        }
        if (vel)
            qd_step_set_vel(p, t, s, tl_clampi(d->vel + tl_spread(h >> 16, vel), 1, 127));
        if (chance)
            qd_step_set_chance(p, t, s, tl_clampi(100 - chance * (int)(h >> 24) / 255, 1, 100));
    }
}

void qd_tl_humanize_one(uint32_t h, int micro, int vel, int *m, int *v)
{
    if (micro)
        *m = tl_clampi(*m + tl_spread(h, micro), -23, 23);
    if (vel)
        *v = tl_clampi(*v + tl_spread(h >> 12, vel), 1, 127);
}

void qd_tl_humanize(qd_pattern_t *p, int t, const uint8_t *pos, int n, uint32_t seed, int micro, int vel)
{
    int i;
    for (i = 0; i < n; i++) {
        qd_rstep_t r;
        if (!(p->step[t][pos[i]].flags & QD_SF_ON))
            continue;
        qd_step_resolve(p, t, pos[i], &r);
        qd_tl_humanize_one(qd_tl_hash(seed, t, i), micro, vel, &r.micro, &r.vel);
        if (micro)
            qd_step_set_micro(p, t, pos[i], r.micro);
        if (vel)
            qd_step_set_vel(p, t, pos[i], r.vel);
    }
}

int qd_tl_quant_to(int m, int grid, int amt)
{
    static const uint8_t Q[QD_QG_N] = {24, 12, 8, 6};
    const int q = Q[grid & 3], h = (q - 1) / 2;
    int g = m >= 0 ? (m + h) / q * q : -((-m + h) / q * q);   /* (the nearest line; a tie: the step's side) */
    if (g > 23)
        g -= q;
    else if (g < -23)
        g += q;
    return g + (m - g) * (100 - tl_clampi(amt, 0, 100)) / 100;   /* (rounded toward the line) */
}

void qd_tl_quantize_one(const qd_step_t *st, const qd_sdef_t *d, int grid, int amt, int vamt, int *m, int *v)
{
    if (amt && (st->ovr & QD_OV(QD_PM_MICRO)))
        *m = qd_tl_quant_to(st->micro, grid, amt);
    if (vamt && (st->ovr & QD_OV(QD_PM_VEL)))
        *v = d->vel + (st->vel - d->vel) * (100 - tl_clampi(vamt, 0, 100)) / 100;
}

void qd_tl_quantize(qd_pattern_t *p, int t, const uint8_t *pos, int n, int grid, int amt, int vamt)
{
    const qd_sdef_t *d = &p->trk[t].def;
    int i;
    for (i = 0; i < n; i++) {
        const qd_step_t *st = &p->step[t][pos[i]];
        const int om = st->ovr & QD_OV(QD_PM_MICRO), ov = st->ovr & QD_OV(QD_PM_VEL);
        int m = st->micro, v = st->vel;
        if (!(st->flags & QD_SF_ON))
            continue;
        qd_tl_quantize_one(st, d, grid, amt, vamt, &m, &v);
        if (om && amt) {
            if (m == d->micro)
                qd_step_reset(p, t, pos[i], QD_PM_MICRO);
            else
                qd_step_set_micro(p, t, pos[i], m);
        }
        if (ov && vamt) {
            if (v == d->vel)
                qd_step_reset(p, t, pos[i], QD_PM_VEL);
            else
                qd_step_set_vel(p, t, pos[i], v);
        }
    }
}

int qd_tl_grids_level(int i, int x, int y, int part)
{
    const int fx = tl_clampi(x, 0, 255) * 512 / 255, fy = tl_clampi(y, 0, 255) * 512 / 255;
    int ix = fx >> 8, iy = fy >> 8, ux = fx & 255, uy = fy & 255, a, b, c, d, top, bot;
    if (ix > 1)
        ix = 1, ux = 256;
    if (iy > 1)
        iy = 1, uy = 256;
    i &= 31, part %= 3;
    a = QD_GRIDS_MAP[ix][iy][part][i], b = QD_GRIDS_MAP[ix + 1][iy][part][i];
    c = QD_GRIDS_MAP[ix][iy + 1][part][i], d = QD_GRIDS_MAP[ix + 1][iy + 1][part][i];
    top = a + (b - a) * ux / 256, bot = c + (d - c) * ux / 256;
    return top + (bot - top) * uy / 256;
}

int qd_tl_grids_at(uint32_t seed, int t, int i, int x, int y, int part, int chaos)
{
    const int l = qd_tl_grids_level(i, x, y, part);
    return chaos ? tl_clampi(l + ((int)(qd_tl_hash(seed, t, i) & 0xFFu) - 128) * chaos / 128, 0, 255) : l;
}

void qd_tl_grids(qd_pattern_t *p, int t, const uint8_t *pos, int n, int x, int y, int dens, int part, int chaos,
                 uint32_t seed)
{
    int i;
    for (i = 0; i < n; i++) {
        const int l = qd_tl_grids_at(seed, t, i, x, y, part, chaos), s = pos[i];
        if (l > 255 - dens) {
            if (!(p->step[t][s].flags & QD_SF_ON))
                qd_step_on(p, t, s);
            qd_step_set_vel(p, t, s, 40 + l * 87 / 255);
        } else if (p->step[t][s].flags & QD_SF_ON) {
            qd_step_clear(p, t, s);
        }
    }
}

