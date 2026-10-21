/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.5a: the floating page store (core/store) on a RAM NOR with the NOR's rules (an erase sets a sector to 0xFF, a
 * program only clears bits within one page):
 *   - records in and out, overwritten, deleted, across a remount;
 *   - many versions through a small region: garbage collection, the log going round, the latest of each key kept;
 *   - §7's rules: while playing one page a step and no erase (out of blank sectors it waits), erases_noisy 0;
 *   - A POWER CUT AFTER EVERY PROGRAM AND EVERY ERASE (ChoralRoot's pool_test style): for each cut the flash as it was
 *     then is mounted again and must hold exactly the records committed before the cut (a record whose commit page was
 *     written: the new one; else the old one); also a page torn half way;
 *   - the app's store (test_persist.c) is the other half of P1.5a's tests.
 *
 *     test_store        exit 1 on any failure */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../store/store.h"

static int checks, failed;
#define CHECK(c, ...) do { checks++; if (!(c)) { failed++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- the RAM NOR: an op counter, a cut after op `cut` (later ops do nothing), a torn page at the cut */
#define FSIZE (64u * QDS_SECTOR)
static uint8_t FL[FSIZE];
static uint32_t ops, cut = 0xFFFFFFFFu;
static int torn;
static int f_read(void *c, uint32_t off, void *d, uint32_t n) { (void)c; memcpy(d, FL + off, n); return 0; }
static int f_erase(void *c, uint32_t off)
{
    (void)c;
    if (ops++ >= cut)
        return -1;
    memset(FL + off / QDS_SECTOR * QDS_SECTOR, 0xFF, QDS_SECTOR);
    return 0;
}
static int f_prog(void *c, uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    (void)c;
    if (off / QDS_PAGE != (off + n - 1u) / QDS_PAGE) {
        printf("  FAIL: a program crosses a page\n");
        failed++;
        return -8;
    }
    if (ops++ >= cut) {
        if (torn && ops - 1u == cut)                    /* the cut lands half way through this page */
            for (i = 0; i < n / 2u; i++)
                FL[off + i] &= ((const uint8_t *)src)[i];
        return -1;
    }
    for (i = 0; i < n; i++)
        FL[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
static const qd_flash_t FLASH = {f_read, f_erase, f_prog, 0};

static qd_store_t S, T;

/* record k's bytes at version v (len varies) */
static uint32_t rec(uint8_t *b, uint32_t key, uint32_t v)
{
    const uint32_t len = 1u + (key * 977u + v * 131u) % 3000u;
    uint32_t i;
    for (i = 0; i < len; i++)
        b[i] = (uint8_t)(key * 7u + v * 13u + i * 31u);
    return len;
}

static int same(qd_store_t *s, uint32_t key, int v)     /* v -1: absent / deleted */
{
    static uint8_t a[QDS_MAXREC], b[QDS_MAXREC];
    int got = qd_store_get(s, key, a, sizeof a);
    uint32_t len;
    if (v < 0)
        return got < 0 || got == (int)QDS_DELETED;
    len = rec(b, key, (uint32_t)v);
    return got == (int)len && !memcmp(a, b, len);
}

static void put_now(qd_store_t *s, uint32_t key, int v)
{
    static uint8_t b[QDS_MAXREC];
    if (v < 0)
        qd_store_del(s, key);
    else
        qd_store_put(s, key, b, rec(b, key, (uint32_t)v));
    qd_store_flush(s);
}

/* the scenario: 6 keys, 60 versions (a few deletes) through 10 sectors: GC goes round several times */
#define NK 6
#define NV 60
static int script_v(int step) { return (step * 7) % 11 == 3 ? -1 : step; }
static void script(qd_store_t *s, int want[][NK], uint32_t *commit_op)
{
    int step, k;
    for (step = 0; step < NV; step++) {
        const uint32_t key = 0x30000u + (uint32_t)(step % NK);
        put_now(s, key, script_v(step));
        if (want) {
            for (k = 0; k < NK; k++)
                want[step + 1][k] = want[step][k];
            want[step + 1][step % NK] = script_v(step);
            commit_op[step + 1] = ops;
        }
    }
}

int main(void)
{
    static int want[NV + 1][NK];
    static uint32_t commit_op[NV + 1];
    uint32_t base_ops, total, c;
    int k, v, bad = 0;

    /* records in and out, overwritten, deleted, remounted */
    memset(FL, 0xFF, sizeof FL);
    CHECK(qd_store_mount(&S, &FLASH, 0, 16), "mount an erased region");
    for (k = 0; k < 20; k++)
        put_now(&S, (uint32_t)k + 1u, k);
    put_now(&S, 5, 100);
    put_now(&S, 7, -1);
    qd_store_mount(&T, &FLASH, 0, 16);
    for (k = 0, v = 0; k < 20; k++)
        v += !same(&T, (uint32_t)k + 1u, k == 4 ? 100 : k == 6 ? -1 : k);
    CHECK(!v, "20 records, one overwritten, one deleted: all as written after a remount (%d differ)", v);
    CHECK(S.c.erases_noisy == 0, "no erase while sounding");

    /* GC: 3000 writes of 30 keys through 32 sectors (the log goes round many times) */
    memset(FL, 0xFF, sizeof FL);
    qd_store_mount(&S, &FLASH, 0, 32);
    for (v = 0; v < 100; v++)
        for (k = 0; k < 30; k++)
            put_now(&S, 0x1000u + (uint32_t)k, v);
    qd_store_mount(&T, &FLASH, 0, 32);
    for (k = 0, v = 0; k < 30; k++)
        v += !same(&T, 0x1000u + (uint32_t)k, 99);
    CHECK(!v && S.c.gcs > 50 && !S.c.drops, "3000 records through 32 sectors: the latest of each after a remount (%d "
          "differ, %u GCs)", v, (unsigned)S.c.gcs);

    /* §7 while playing: one page a step, no erase; out of blank sectors it waits */
    {
        static uint8_t b[QDS_MAXREC];
        uint32_t e0;
        int steps = 0, maxpg = 0;
        memset(FL, 0xFF, sizeof FL);
        qd_store_mount(&S, &FLASH, 0, 16);
        e0 = S.c.erases;
        qd_store_put(&S, 0x2000u, b, rec(b, 0x2000u, 1));
        while (qd_store_busy(&S) && steps < 1000) {
            const uint32_t p0 = S.c.progs;
            qd_store_step(&S, 1, 0, 64);
            if ((int)(S.c.progs - p0) > maxpg)
                maxpg = (int)(S.c.progs - p0);
            steps++;
        }
        CHECK(maxpg <= 2 && S.c.erases == e0 && S.c.saves_playing == 1 && S.c.erases_noisy == 0,
              "playing: at most a page a step (and a sector header), no erase (%d, %u erases)", maxpg,
              (unsigned)(S.c.erases - e0));
        for (k = 0; k < 400 && S.c.waits == 0; k++) {   /* fill the blank sectors while playing: it must wait */
            qd_store_put(&S, 0x2100u + (uint32_t)(k % 3), b, rec(b, 0x2100u, (uint32_t)k));
            for (steps = 0; steps < 40 && qd_store_busy(&S); steps++)
                qd_store_step(&S, 1, 0, 64);
        }
        CHECK(S.c.waits > 0 && S.c.erases == e0 && S.c.erases_noisy == 0,
              "out of blank sectors while playing: it waits, no erase (%u waits)", (unsigned)S.c.waits);
        qd_store_flush(&S);
        CHECK(!qd_store_busy(&S) && S.c.erases > e0, "stopped and quiet: it goes on (GC, erases)");
    }

    /* POWER CUTS: the scenario once to count its operations and where each record commits */
    memset(FL, 0xFF, sizeof FL);
    ops = 0;
    cut = 0xFFFFFFFFu;
    qd_store_mount(&S, &FLASH, 0, 10);
    for (k = 0; k < NK; k++)
        want[0][k] = -1;
    base_ops = ops;
    script(&S, want, commit_op);
    total = ops;
    printf("  the scenario: %d records through 10 sectors, %u flash operations, %u GCs\n", NV, (unsigned)(total - base_ops),
           (unsigned)S.c.gcs);
    for (torn = 0; torn < 2; torn++)
        for (c = base_ops; c < total; c++) {
            int st = 0;
            memset(FL, 0xFF, sizeof FL);
            ops = 0;
            cut = c;
            qd_store_mount(&S, &FLASH, 0, 10);
            script(&S, 0, 0);
            cut = 0xFFFFFFFFu;                          /* power back: mount what is there */
            qd_store_mount(&T, &FLASH, 0, 10);
            while (st < NV && commit_op[st + 1] <= c)
                st++;
            for (k = 0, v = 0; k < NK; k++)
                v += !same(&T, 0x30000u + (uint32_t)k, want[st][k]);
            if (v) {
                bad++;
                if (bad < 5)
                    printf("  FAIL a cut after operation %u (%s): %d keys differ from the %d records committed\n",
                           (unsigned)c, torn ? "torn" : "clean", v, st);
            }
            put_now(&T, 0x30000u, 7);                   /* and it goes on working after the cut */
            if (!same(&T, 0x30000u, 7)) {
                bad++;
                if (bad < 5)
                    printf("  FAIL after a cut at %u: no write\n", (unsigned)c);
            }
        }
    CHECK(!bad, "a power cut after every program and erase (and a torn page): the last committed state each time "
          "(%u cuts, %d bad)", (unsigned)(2u * (total - base_ops)), bad);
    printf("test_store: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
