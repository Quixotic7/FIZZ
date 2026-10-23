/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P2.0 (tools/budget, from the Phase 2 audit): the heap counter: a 32-byte header before each block (size, site); live and peak bytes, per site and in all */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define NSITE 256
typedef struct { const char *f; int l; long live, peak, nalloc, maxblk; } site_t;
static site_t sites[NSITE];
static int nsites;
long hw_live, hw_peak, hw_nalloc;
typedef struct { size_t n; int site; int pad; char pad2[16]; } hdr_t;

static int site_of(const char *f, int l)
{
    int i;
    const char *b = strrchr(f, '/');
    b = b ? b + 1 : f;
    for (i = 0; i < nsites; i++)
        if (sites[i].l == l && !strcmp(sites[i].f, b))
            return i;
    if (nsites == NSITE)
        return NSITE - 1;
    sites[nsites].f = b, sites[nsites].l = l;
    return nsites++;
}
static void add(int s, long n)
{
    sites[s].live += n;
    if (sites[s].live > sites[s].peak)
        sites[s].peak = sites[s].live;
    hw_live += n;
    if (hw_live > hw_peak)
        hw_peak = hw_live;
}
void *hw_malloc(size_t n, const char *f, int l)
{
    hdr_t *h = (malloc)(n + sizeof(hdr_t));
    if (!h)
        return NULL;
    h->n = n, h->site = site_of(f, l);
    sites[h->site].nalloc++, hw_nalloc++;
    if ((long)n > sites[h->site].maxblk)
        sites[h->site].maxblk = (long)n;
    add(h->site, (long)n);
    return h + 1;
}
void *hw_calloc(size_t a, size_t b, const char *f, int l)
{
    void *p = hw_malloc(a * b, f, l);
    if (p)
        memset(p, 0, a * b);
    return p;
}
void hw_free(void *p)
{
    hdr_t *h;
    if (!p)
        return;
    h = (hdr_t *)p - 1;
    add(h->site, -(long)h->n);
    (free)(h);
}
void *hw_realloc(void *p, size_t n, const char *f, int l)
{
    void *q;
    size_t old;
    if (!p)
        return hw_malloc(n, f, l);
    old = ((hdr_t *)p - 1)->n;
    q = hw_malloc(n, f, l);
    if (q)
        memcpy(q, p, old < n ? old : n);
    hw_free(p);
    return q;
}
void hw_report(FILE *o, const char *title)
{
    int i;
    fprintf(o, "heap %s: live %ld B, peak %ld B, %ld allocations\n", title, hw_live, hw_peak, hw_nalloc);
    for (i = 0; i < nsites; i++)
        if (sites[i].peak >= 256)
            fprintf(o, "  %-28s:%-5d live %8ld  peak %8ld  allocs %7ld  largest %7ld\n", sites[i].f, sites[i].l,
                    sites[i].live, sites[i].peak, sites[i].nalloc, sites[i].maxblk);
}
void hw_reset_peak(void)
{
    int i;
    hw_peak = hw_live;
    for (i = 0; i < nsites; i++)
        sites[i].peak = sites[i].live;
}
void hw_sites(FILE *o, const char *prefix)   /* (P2.0: budget.py's lines: each site's peak, the largest first) */
{
    int i, j, order[NSITE];
    for (i = 0; i < nsites; i++)
        order[i] = i;
    for (i = 1; i < nsites; i++)
        for (j = i; j > 0 && sites[order[j]].peak > sites[order[j - 1]].peak; j--) {
            const int x = order[j];
            order[j] = order[j - 1], order[j - 1] = x;
        }
    for (i = 0; i < nsites; i++)
        if (sites[order[i]].peak >= 256)
            fprintf(o, "%s %s:%d %ld %ld\n", prefix, sites[order[i]].f, sites[order[i]].l, sites[order[i]].peak,
                    sites[order[i]].maxblk);
}
