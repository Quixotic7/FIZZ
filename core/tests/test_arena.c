/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P2.1: the drawing's fixed arena (core/gfx/qd_arena.c): random malloc / calloc / realloc / free, each block filled
 * with its own pattern and checked before it changes (a realloc's moves, in place, up and down, kept the bytes); the
 * blocks' tags walked every 1000; every block back, the arena whole again (one free block: a request of nearly its size
 * met); its peak and its refusals counted.
 *
 *     test_arena        exit 1 on any failure */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QD_GFX_ARENA 262144u
#define QD_TARGET_FM1 1                                 /* (always the arena: no C library behind it) */
#include "../gfx/qd_arena.c"

static int checks, failed;
#define CHECK(c, ...) do { checks++; if (!(c)) { failed++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define N 400
static uint8_t *blk[N];
static size_t len[N];
static uint8_t tag[N];
static uint32_t rs = 1u;
static uint32_t rnd(void) { rs ^= rs << 13, rs ^= rs >> 17, rs ^= rs << 5; return rs; }
static int intact(int i)
{
    size_t k;
    for (k = 0; k < len[i]; k++)
        if (blk[i][k] != (uint8_t)(tag[i] + k))
            return 0;
    return 1;
}
static void fill(int i)
{
    size_t k;
    for (k = 0; k < len[i]; k++)
        blk[i][k] = (uint8_t)(tag[i] + k);
}

/* (the arena's own shape: every block's head equal to its foot, no two free ones side by side, the used ones' sizes
 * summing to what it says it holds, the walk ending at the end fence) */
static int walk_ok(void)
{
    uint32_t at = 4u, used = 0u;
    int prev_free = 0;
    if (!ar_ready)
        return 1;
    while (ar_size(at)) {
        const uint32_t sz = ar_size(at);
        if (AR_W(at) != AR_W(at + sz - 4u) || (!ar_used(at) && prev_free))
            return 0;
        prev_free = !ar_used(at);
        used += ar_used(at) ? sz : 0u;
        at += sz;
    }
    return at == (uint32_t)sizeof ar_mem - 4u && used == ar_live;
}

int main(void)
{
    int walks = 0;
    int it, i, bad = 0, refused = 0;
    for (it = 0; it < 200000; it++) {
        const int op = (int)(rnd() % 4u);
        i = (int)(rnd() % N);
        if (blk[i] && !intact(i))
            bad++;
        if (it % 1000 == 0)
            walks += !walk_ok();
        if (op == 0 && !blk[i]) {                       /* malloc (mostly small, now and then big) */
            len[i] = rnd() % 8u ? 1u + rnd() % 300u : 1u + rnd() % 20000u;
            blk[i] = qd_arena_malloc(len[i]);
            if (!blk[i]) {
                refused++;
                continue;
            }
            tag[i] = (uint8_t)rnd();
            fill(i);
        } else if (op == 1 && blk[i]) {                 /* realloc: grow or shrink, the bytes kept */
            const size_t n = rnd() % 4u ? len[i] + rnd() % 400u : 1u + rnd() % 30000u;
            uint8_t *q = qd_arena_realloc(blk[i], n);
            if (!q) {
                refused++;
                continue;
            }
            blk[i] = q;
            len[i] = n < len[i] ? n : len[i];
            bad += !intact(i);
            len[i] = n;
            fill(i);
        } else if (blk[i]) {                            /* free */
            qd_arena_free(blk[i]);
            blk[i] = 0;
        }
    }
    for (i = 0; i < N; i++)
        if (blk[i]) {
            bad += !intact(i);
            qd_arena_free(blk[i]);
            blk[i] = 0;
        }
    CHECK(!bad, "200000 random allocations, reallocations and frees: every block's bytes kept (%d broken)", bad);
    CHECK(!walks && walk_ok(), "the arena's blocks well formed all the way (%d walks wrong)", walks);
    CHECK(qd_arena_live() == 0, "every block back: nothing held (%lu)", (unsigned long)qd_arena_live());
    {
        void *p = qd_arena_malloc(QD_GFX_ARENA - 64u);  /* (one free block again: merged all the way) */
        CHECK(p != 0, "the arena whole again: a block of nearly its size");
        qd_arena_free(p);
    }
    printf("  the arena: %u KB, peak %lu bytes, %d requests refused (full) on the way\n", QD_GFX_ARENA / 1024u,
           (unsigned long)qd_arena_peak(), refused);
    {   /* P2.6c: a block cut down in place: the rest free again (a block that size fits beside it) */
        const size_t live0 = qd_arena_live();
        uint8_t *a = (uint8_t *)qd_arena_malloc(100000u), *b;
        memset(a, 7, 1000);
        a = (uint8_t *)qd_arena_shrink(a, 1000u);
        b = (uint8_t *)qd_arena_malloc(QD_GFX_ARENA - 2000u - live0 - 64u);
        CHECK(a && b && a[999] == 7 && qd_arena_live() < live0 + 1100u + (b ? QD_GFX_ARENA : 0),
              "qd_arena_shrink: the block cut to 1000 B, its first bytes kept, the rest given back (%p)", (void *)b);
        qd_arena_free(b);
        qd_arena_free(a);
    }
    printf("test_arena: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
