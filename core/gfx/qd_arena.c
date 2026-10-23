/* SPDX-License-Identifier: GPL-3.0-only */
/* P2.1: the drawing's fixed arena (qd_arena.h): boundary tags, free neighbours merged; small blocks first fit
 * from the bottom, large ones from the top (the long-lived and the short-lived apart: little fragmentation). Built through
 * qd_gfx_plutovg.c at the device's sizes (the core's source lists stay as they are). Single-threaded, as the drawing.
 * On the host at the device's sizes (the tests and tools), plutovg's allocations outside the core's drawing (a tool's
 * own sheets) go to the C library: qd_arena_use() says when the core draws (QD_TARGET_FM1: always the arena) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qd_arena.h"

#ifndef QD_TARGET_FM1
static int ar_active;
void qd_arena_use(int on) { ar_active = on; }
#define AR_LIBC !ar_active
#else
void qd_arena_use(int on) { (void)on; }
#define AR_LIBC 0
#endif

/* a block: a 4-byte head (its size, payload and tags, a multiple of 8; bit 0 used), its payload, a 4-byte foot (the
 * size again); the arena starts and ends with a used zero-size fence */
#define AR_ALIGN 8u
#define AR_MIN 16u
#ifndef AR_BIG
#define AR_BIG 192u                     /* (a large block: from the top; P2.1b: render_frames' whole run fits 48 KB) */
#endif
static uint32_t ar_mem[QD_GFX_ARENA / 4u];
static uint32_t ar_live, ar_peak, ar_fails;
static int ar_ready;

#define AR_W(i) ar_mem[(i) / 4u]                        /* (a 4-byte word at byte offset i) */
static uint32_t ar_size(uint32_t at) { return AR_W(at) & ~7u; }
static int ar_used(uint32_t at) { return (int)(AR_W(at) & 1u); }
static void ar_set(uint32_t at, uint32_t size, int used)
{
    AR_W(at) = size | (uint32_t)(used != 0);
    AR_W(at + size - 4u) = size | (uint32_t)(used != 0);
}

/* (the first block's head sits at 4, so a payload (head + 4) is 8-aligned) */
static void ar_init(void)
{
    const uint32_t end = (uint32_t)sizeof ar_mem;
    AR_W(0) = 1u;                                       /* the start fence (a used foot) */
    ar_set(4u, end - 8u, 0);
    AR_W(end - 4u) = 1u;                                /* the end fence (a used head, size 0) */
    ar_ready = 1;
}

#ifdef QD_ARENA_SITES
/* (the host's debugging: each live block's call site, by its offset; the live bytes by site) */
static const char *as_file[64];
static int as_line[64], as_n, as_cur = -1;
static uint8_t as_of[QD_GFX_ARENA / 8u];
void qd_arena_site(const char *file, int line)
{
    int i;
    for (i = 0; i < as_n; i++)
        if (as_line[i] == line && as_file[i] == file)
            break;
    if (i == as_n && as_n < 63)
        as_file[as_n] = file, as_line[as_n] = line, as_n++;
    as_cur = i < 63 ? i : 63;
}
static uint32_t as_peak[64], as_peak_at;
static void as_count(uint32_t *live)
{
    uint32_t at = 4u;
    memset(live, 0, 64 * sizeof *live);
    while (ar_size(at)) {
        if (ar_used(at))
            live[as_of[at / 8u]] += ar_size(at);
        at += ar_size(at);
    }
}
static void as_peaked(void)                             /* (a new peak, 4 KB above the last seen: its sites kept) */
{
    if (ar_peak >= as_peak_at + 4096u)
        as_count(as_peak), as_peak_at = ar_peak;
}
void qd_arena_sites(void)
{
    static uint32_t live[64];
    int i;
    as_count(live);
    fprintf(stderr, "  arena %-46s %8s %8s\n", "(site)", "live", "at peak");
    for (i = 0; i < as_n; i++)
        if (live[i] || as_peak[i])
            fprintf(stderr, "  arena %-40s:%-5d %8u %8u\n", as_file[i], as_line[i], live[i], as_peak[i]);
}
#define AS_MARK(at) (as_of[(at) / 8u] = (uint8_t)(as_cur < 0 ? 63 : as_cur))
#define AS_PEAKED() as_peaked()
#else
#define AS_MARK(at) ((void)0)
#define AS_PEAKED() ((void)0)
#endif
static int ar_in(const void *p) { return (const uint8_t *)p >= (const uint8_t *)ar_mem && (const uint8_t *)p < (const uint8_t *)ar_mem + sizeof ar_mem; }

void *qd_arena_malloc(size_t n)
{
    uint32_t need, at, start, end = (uint32_t)sizeof ar_mem;
    if (AR_LIBC)
        return malloc(n);
    if (!ar_ready)
        ar_init();
    if (n > end) {
        ar_fails++;
#if !defined(QD_TARGET_FM1)
        return malloc(n);
#else
        return 0;
#endif
    }
    need = ((uint32_t)n + 8u + AR_ALIGN - 1u) & ~(AR_ALIGN - 1u);
    need = need < AR_MIN ? AR_MIN : need;
    {   /* small blocks: the lowest free one that fits; large ones (the stroker's borders, the outlines, the span
         * arrays, mostly short-lived): the highest, taken from its top. The two kinds kept apart, the free space stays
         * in one piece in the middle */
        const int big = need >= AR_BIG;
        uint32_t best = 0, bsz = 0;
        for (at = 4u; (start = ar_size(at)) != 0u; at += start)
            if (!ar_used(at) && start >= need) {
                best = at, bsz = start;
                if (!big)
                    break;
            }
        if (best) {
            at = best;
            if (bsz - need >= AR_MIN) {                 /* (split: the rest stays free) */
                if (big) {
                    ar_set(at, bsz - need, 0);
                    at += bsz - need;
                    ar_set(at, need, 1);
                } else {
                    ar_set(at, need, 1);
                    ar_set(at + need, bsz - need, 0);
                }
            } else {
                ar_set(at, bsz, 1);
                need = bsz;
            }
            AS_MARK(at);
            ar_live += need;
            ar_peak = ar_live > ar_peak ? ar_live : ar_peak;
            AS_PEAKED();
            return (uint8_t *)ar_mem + at + 4u;
        }
    }
    ar_fails++;
#if !defined(QD_TARGET_FM1)
#ifdef QD_ARENA_DEBUG
    fprintf(stderr, "qd_arena: %lu bytes refused (live %u, peak %u)\n", (unsigned long)n, ar_live, ar_peak);
#endif
    return malloc(n);                                   /* (the host's tests: counted, drawn on: the device has none) */
#else
    return 0;
#endif
}

void *qd_arena_calloc(size_t a, size_t b)
{
    void *p = qd_arena_malloc(a * b);
    if (p)
        memset(p, 0, a * b);
    return p;
}

void qd_arena_free(void *p)
{
    uint32_t at, sz;
    if (!p)
        return;
    if (!ar_in(p)) {
        free(p);
        return;
    }
    at = (uint32_t)((uint8_t *)p - (uint8_t *)ar_mem) - 4u;
    sz = ar_size(at);
    ar_live -= sz;
    if (!ar_used(at + sz))                              /* (the free next one merged) */
        sz += ar_size(at + sz);
    if (!(AR_W(at - 4u) & 1u)) {                        /* (the free one before merged) */
        const uint32_t ps = AR_W(at - 4u) & ~7u;
        at -= ps, sz += ps;
    }
    ar_set(at, sz, 0);
}

void *qd_arena_realloc(void *p, size_t n)
{
    uint32_t at, sz, need;
    void *q;
    if (!p)
        return qd_arena_malloc(n);
    if (!n) {
        qd_arena_free(p);
        return 0;
    }
    if (!ar_in(p))
        return realloc(p, n);
    at = (uint32_t)((uint8_t *)p - (uint8_t *)ar_mem) - 4u;
    sz = ar_size(at);
    need = ((uint32_t)n + 8u + AR_ALIGN - 1u) & ~(AR_ALIGN - 1u);
    need = need < AR_MIN ? AR_MIN : need;
    if (need <= sz)
        return p;
    if (!ar_used(at + sz) && sz + ar_size(at + sz) >= need) {   /* (grown into the free next one) */
        const uint32_t all = sz + ar_size(at + sz);
        if (all - need >= AR_MIN) {
            ar_set(at, need, 1);
            ar_set(at + need, all - need, 0);
        } else {
            ar_set(at, all, 1);
            need = all;
        }
        ar_live += need - sz;
        ar_peak = ar_live > ar_peak ? ar_live : ar_peak;
        AS_PEAKED();
        return p;
    }
    if (!(AR_W(at - 4u) & 1u)) {                        /* (grown into the free one before (and after): moved down) */
        const uint32_t ps = AR_W(at - 4u) & ~7u, nx = ar_used(at + sz) ? 0u : ar_size(at + sz), all = ps + sz + nx;
        if (all >= need) {
            const uint32_t base = at - ps;
            uint32_t to = base;
            if (all - need < AR_MIN)
                need = all;
            else if (need >= AR_BIG)                    /* (a large one at the top, the free rest below) */
                to += all - need;
            memmove((uint8_t *)ar_mem + to + 4u, p, sz - 8u);   /* (the bytes first: the tags land where they were) */
            if (need < all && to != base)
                ar_set(base, all - need, 0);
            else if (need < all)
                ar_set(to + need, all - need, 0);
            ar_set(to, need, 1);
#ifdef QD_ARENA_SITES
            as_of[to / 8u] = as_of[at / 8u];
#endif
            ar_live += need - sz;
            ar_peak = ar_live > ar_peak ? ar_live : ar_peak;
            AS_PEAKED();
            return (uint8_t *)ar_mem + to + 4u;
        }
    }
    q = qd_arena_malloc(n);
    if (!q)
        return 0;
    memcpy(q, p, sz - 8u);
    qd_arena_free(p);
    return q;
}

size_t qd_arena_peak(void) { return ar_peak; }
size_t qd_arena_live(void) { return ar_live; }
size_t qd_arena_size(void) { return sizeof ar_mem; }
unsigned qd_arena_fails(void) { return ar_fails; }
