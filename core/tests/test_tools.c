/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* T1's acceptance (docs/TRACK-TOOLS-PLAN.md §6): the track tools.
 *
 *   ops       (seq/tools.c, on a bare pattern) rotate by k then by -k is the identity, steps and locks byte for byte,
 *             for n = 1, 7, 16, 37 (pages with ranges) and k = 0, 1, n - 1, n; rotate by 1 moves each step on one;
 *             reverse twice is the identity and negates an overridden micro once; the lock pool stays sorted and
 *             counted; euclid's hits equal `hits` for every (hits, len) and E(3, 8) is x..x..x. up to rotation; NEW /
 *             ADD / MASK; length's x2 / /2 on a track, on one page, on a page's end
 *   TOOLS     (the app, driven as the glue drives it) ARP held opens it, a tap is the MIDI FX screen, SHIFT + ARP FILL
 *             at once; SELECT the page, PRESETS the tool, ALGORITHM / B11 the four scopes, the step lock's SELECTED
 *             STEPS (W toggles); ROTATE / REVERSE / EUCLID / LENGTH live (the pattern rewritten at each change, heard), OCT+ one
 *             undo level that restores the original byte for byte, OCT- the original back and no level (never leaving); switching the tool / the scope / the track keeps a pending change; HOME keeps it; scope PAGE /
 *             TRACK / PATTERN; the name popup (~0.8 s) and the scope's takeover; the LEDs (ARP's rule, the keys);
 *             the device's drawn-only fallback (the log too full to keep the original: nothing written until OCT+,
 *             then no undo level); every screen drawn under the sanitizers
 *
 *     test_tools    exit 1 on a failed check */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ui.h"
#include "tools.h"
#include "grids_map.h"

static int fails, checks;
#define CHECK(c, ...)                                     \
    do {                                                  \
        checks++;                                         \
        if (!(c)) {                                       \
            fails++;                                      \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

/* ================================================================ the ops on a bare pattern */
static qd_pattern_t P0, P1;
static uint32_t rng = 12345u;
static uint32_t rnd(void)
{
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return rng;
}

/* track 1 of P: pages 1..3 looped (0..15, 3..12, 0..10: 37 steps), page 5 2..8 (7), page 6 5..5 (1); random trigs,
 * notes, micro overrides, locks; track 2 locks too (the pool's other range) */
static void fill(qd_pattern_t *p)
{
    int s, t;
    qd_pat_init(p);
    qd_page_set_loop(p, 0, 0x07);
    qd_page_set_range(p, 0, 1, 3, 12);
    qd_page_set_range(p, 0, 2, 0, 10);
    qd_page_set_range(p, 0, 4, 2, 8);
    qd_page_set_range(p, 0, 5, 5, 5);
    for (t = 0; t < 2; t++)
        for (s = 0; s < QD_NSTEP; s++) {
            if (rnd() % 3)
                continue;
            qd_step_set_vel(p, t, s, 1 + (int)(rnd() % 127));
            qd_step_set_note(p, t, s, 0, 40 + (int)(rnd() % 40));
            if (rnd() % 2)
                qd_step_set_micro(p, t, s, (int)(rnd() % 47) - 23);
            if (rnd() % 2)
                qd_lock_set(p, t, s, (int)(rnd() % 20), (int)(rnd() % 100));
            if (rnd() % 3 == 0)
                qd_lock_set(p, t, s, QD_LK_PAN, (int)(rnd() % 100) - 50);
        }
}

static void t_rotate(void)
{
    static const struct { int scope, page, n; } L[] = {{QD_TS_PAGE, 5, 1}, {QD_TS_PAGE, 4, 7}, {QD_TS_PAGE, 0, 16},
                                                       {QD_TS_TRACK, 0, 37}};
    uint8_t pos[QD_NSTEP];
    int c, ok = 1, moved = 1, pool = 1;
    for (c = 0; c < 4; c++) {
        const int n0 = L[c].n, K[4] = {0, 1, n0 - 1, n0};
        int i, n = 0;
        fill(&P0);
        n = qd_tl_positions(&P0, 0, L[c].scope, L[c].page, pos);
        CHECK(n == n0, "the list's length (%d, %d)", n, n0);
        for (i = 0; i < 4; i++) {
            const uint16_t nl = P0.nlock;
            P1 = P0;
            qd_tl_rotate(&P1, 0, pos, n, K[i]);
            pool &= qd_pat_check(&P1) && P1.nlock == nl;
            qd_tl_rotate(&P1, 0, pos, n, -K[i]);
            ok &= !memcmp(&P0, &P1, sizeof P0);
        }
        P1 = P0;
        qd_tl_rotate(&P1, 0, pos, n, 1);
        for (i = 0; i < n; i++) {                       /* each step one on, its locks with it */
            int na, nb, k;
            const qd_lock_t *la = qd_locks(&P0, 0, pos[i], &na), *lb = qd_locks(&P1, 0, pos[(i + 1) % n], &nb);
            qd_step_t a = P0.step[0][pos[i]], b = P1.step[0][pos[(i + 1) % n]];
            moved &= !memcmp(&a, &b, sizeof a) && na == nb;
            for (k = 0; k < na && k < nb; k++)
                moved &= la[k].id == lb[k].id && la[k].value == lb[k].value;
        }
        if (n == 37)                                    /* (a step outside the loop stays) */
            moved &= !memcmp(&P0.step[0][1 * QD_PAGE + 1], &P1.step[0][1 * QD_PAGE + 1], sizeof(qd_step_t)) &&
                     !memcmp(P0.step[1], P1.step[1], sizeof P0.step[1]);
    }
    CHECK(ok, "rotate by k then by -k: the identity, byte for byte (n 1, 7, 16, 37; k 0, 1, n - 1, n)");
    CHECK(moved, "rotate by 1: each step (its locks with it) one on, round the list; the rest untouched");
    CHECK(pool, "the lock pool sorted and counted after every rotate");
}

static void t_reverse(void)
{
    uint8_t pos[QD_NSTEP];
    int n, i, ok, mic = 1;
    fill(&P0);
    n = qd_tl_positions(&P0, 0, QD_TS_TRACK, 0, pos);
    P1 = P0;
    qd_tl_reverse(&P1, 0, pos, n);
    CHECK(qd_pat_check(&P1) && P1.nlock == P0.nlock, "reverse: the pool sorted and counted");
    for (i = 0; i < n; i++) {
        const qd_step_t *a = &P0.step[0][pos[i]], *b = &P1.step[0][pos[n - 1 - i]];
        mic &= a->flags == b->flags && a->note[0] == b->note[0] &&
               (a->ovr & QD_OV(QD_PM_MICRO) ? b->micro == -a->micro : b->micro == a->micro);
    }
    CHECK(mic, "reverse: step i is step n - 1 - i's, an overridden micro negated");
    qd_tl_reverse(&P1, 0, pos, n);
    ok = !memcmp(&P0, &P1, sizeof P0);
    CHECK(ok, "reverse twice: the identity, byte for byte");
}

static void t_euclid(void)
{
    uint8_t pos[QD_NSTEP];
    char e[9];
    int hits, len, r, ok = 1, n, i, k, rotok = 0;
    for (len = 1; len <= 32; len++)
        for (hits = 0; hits <= len; hits++)
            for (r = 0; r < len; r++) {
                int c = 0;
                for (i = 0; i < len; i++)
                    c += qd_tl_euclid_hit(i, hits, len, r);
                ok &= c == hits;
            }
    CHECK(ok, "euclid: hits equal `hits` for every (hits, len), every rotation");
    for (r = 0; r < 8; r++) {
        for (i = 0; i < 8; i++)
            e[i] = qd_tl_euclid_hit(i, 3, 8, r) ? 'x' : '.';
        e[8] = 0;
        for (k = 0; k < 8; k++) {
            int same = 1;
            for (i = 0; i < 8; i++)
                same &= e[(i + k) % 8] == "x..x..x."[i];
            rotok |= same;
        }
    }
    CHECK(rotok, "E(3, 8) is x..x..x. up to rotation");
    qd_pat_init(&P0);
    for (i = 0; i < 16; i++)                            /* x.x.x.x.x.x.x.x. */
        if (!(i & 1))
            qd_step_set_vel(&P0, 0, i, 90);
    qd_lock_set(&P0, 0, 2, QD_LK_PAN, 10);
    n = qd_tl_positions(&P0, 0, QD_TS_PAGE, 0, pos);
    P1 = P0;
    qd_tl_euclid(&P1, 0, pos, n, 4, 16, 0, QD_EU_NEW);  /* hits 0 4 8 12 */
    ok = 1;
    for (i = 0; i < 16; i++)
        ok &= qd_step_is_on(&P1.step[0][i]) == (i % 4 == 0);
    CHECK(ok && P1.step[0][4].vel == 90 && !qd_step_overrides(&P1.step[0][1], QD_PM_VEL) && P1.nlock == 0 &&
              qd_pat_check(&P1), "NEW: the hits only (a kept trig keeps its values; the going one its lock)");
    P1 = P0;
    qd_tl_euclid(&P1, 0, pos, n, 4, 16, 1, QD_EU_ADD);  /* rot 1: hits 3 7 11 15 */
    ok = 1;
    for (i = 0; i < 16; i++)
        ok &= qd_step_is_on(&P1.step[0][i]) == (!(i & 1) || i % 4 == 3);
    CHECK(ok && P1.step[0][3].ovr == 0 && P1.nlock == 1, "ADD: the hits joined (no overrides), the rest kept");
    P1 = P0;
    qd_tl_euclid(&P1, 0, pos, n, 4, 8, 0, QD_EU_MASK);  /* len 8: hits 0 2 4 6 8 10 12 14 */
    ok = 1;
    for (i = 0; i < 16; i++)
        ok &= qd_step_is_on(&P1.step[0][i]) == !(i & 1);
    CHECK(ok, "MASK, length 8 (repeating): the trigs that are hits");
}

static void t_length(void)
{
    qd_tset_t ts;
    int i, ok = 1;
    qd_pat_init(&P0);
    qd_track_set_length(&P0, 0, 32);                    /* pages 1, 2 */
    for (i = 0; i < 32; i += 3)
        qd_step_set_vel(&P0, 0, i, 50 + i);
    P1 = P0;
    ts = P1.trk[0];
    qd_tl_length(&P1, 0, -1, 1, &ts);
    CHECK(qd_track_len(&ts) == 64 && !memcmp(&P0, &P1, sizeof P0), "x2 predicted: 64 steps, nothing written");
    qd_tl_length(&P1, 0, -1, 1, 0);
    for (i = 0; i < 32; i++)
        ok &= !memcmp(&P1.step[0][i], &P1.step[0][32 + i], sizeof(qd_step_t));
    CHECK(ok && qd_track_len(&P1.trk[0]) == 64 && P1.trk[0].loop == 0x0F, "x2: pages 1..4, 3 and 4 copies of 1 and 2");
    qd_tl_length(&P1, 0, -1, -2, 0);
    CHECK(qd_track_len(&P1.trk[0]) == 16 && P1.trk[0].loop == 0x01 && qd_step_is_on(&P1.step[0][35]),
          "/2 twice: page 1 alone (nothing deleted)");
    qd_tl_length(&P1, 0, -1, -1, 0);
    CHECK(qd_track_len(&P1.trk[0]) == 8, "/2 of one page: its end halved (8)");
    qd_tl_length(&P1, 0, 0, 1, 0);
    CHECK(P1.trk[0].pend[0] == 15 && !memcmp(&P1.step[0][0], &P1.step[0][8], sizeof(qd_step_t)) &&
              !memcmp(&P1.step[0][3], &P1.step[0][11], sizeof(qd_step_t)), "PAGE x2: the end doubled, the steps copied on");
    qd_tl_length(&P1, 0, 0, 3, 0);
    CHECK(P1.trk[0].pend[0] == 15, "PAGE x2 at the page's end: nothing more");
    P1.trk[0].loop = 0x7F;
    CHECK(!qd_tl_double(&P1, 0, 0) && P1.trk[0].loop == 0x7F, "x2 with 7 pages looped: no room, nothing");
}

/* STRETCH (2026-10-25): x8 then /8 the identity (trigs, micro, locks, a length); /2 of every step keeps the ones on
 * the beat; fit onto a page; a page's range; the pool sorted, the locks with their steps, the rest untouched */
static int micro_at(const qd_pattern_t *p, int t, int s)
{
    qd_rstep_t r;
    qd_step_resolve(p, t, s, &r);
    return r.micro;
}
#define PW(k) (pw = (k), &pw)
static void t_stretch(void)
{
    int pw;
    int16_t v;
    int i, ok = 1, n;
    qd_pat_init(&P0);
    qd_track_set_length(&P0, 0, 16);                    /* one page: trigs 0, 4, 8, 12 and 3 (+6), a lock on 4 */
    for (i = 0; i < 16; i += 4)
        qd_step_set_vel(&P0, 0, i, 40 + i);
    qd_step_set_vel(&P0, 0, 3, 90);
    qd_step_set_micro(&P0, 0, 3, 6);
    qd_step_set_len(&P0, 0, 8, QD_LEN_1);
    qd_lock_set(&P0, 0, 4, QD_LK_PAN, -30);
    qd_lock_set(&P0, 1, 5, QD_LK_PAN, 20);              /* (track 2: untouched) */
    qd_step_set_vel(&P0, 0, 40, 77);                    /* (a step outside the loop: cleared, it is in the result) */
    P1 = P0;
    n = qd_tl_stretch_track(&P1, 0, -1, PW(3), 0, 0);
    CHECK(n == 128 && P1.trk[0].loop == 0xFF && qd_track_len(&P1.trk[0]) == 128 && qd_pat_check(&P1),
          "x8: 128 steps, all eight pages looped, the pool sorted");
    for (i = 0; i < 128; i++)
        ok &= qd_step_is_on(&P1.step[0][i]) == (i % 32 == 0 || i == 26);
    CHECK(ok && micro_at(&P1, 0, 26) == 0 && P1.step[0][32].vel == 44 && qd_lock_get(&P1, 0, 32, QD_LK_PAN, &v) && v == -30 &&
              qd_len_eighths(P1.step[0][64].len) == 64 && !memcmp(P1.step[1], P0.step[1], sizeof P0.step[1]),
          "x8: 0, 4, 8, 12 on 0, 32, 64, 96; 3 + 6/24 on 26 exactly; the lock with its step; the length x8; track 2 kept");
    n = qd_tl_stretch_track(&P1, 0, -1, PW(-3), 0, 0);
    ok = n == 16 && P1.trk[0].loop == 0x01;
    for (i = 0; i < 16; i++)
        ok &= !memcmp(&P1.step[0][i], &P0.step[0][i], sizeof(qd_step_t));
    for (i = 16; i < 128; i++)
        ok &= !qd_step_is_on(&P1.step[0][i]);
    CHECK(ok && P1.nlock == P0.nlock - 0 && qd_lock_get(&P1, 0, 4, QD_LK_PAN, &v) && v == -30 && qd_pat_check(&P1),
          "/8 back: page 1 byte for byte (3's +6 again), the other pages empty, the locks back");
    qd_pat_init(&P0);
    qd_track_set_length(&P0, 0, 16);
    for (i = 0; i < 16; i++)
        qd_step_set_vel(&P0, 0, i, 10 + i);
    P1 = P0;
    n = qd_tl_stretch_track(&P1, 0, -1, PW(-1), 0, 0);
    ok = n == 8 && qd_track_len(&P1.trk[0]) == 8;
    for (i = 0; i < 8; i++)
        ok &= qd_step_is_on(&P1.step[0][i]) && P1.step[0][i].vel == 10 + 2 * i && micro_at(&P1, 0, i) == 0;
    CHECK(ok, "/2 of sixteen trigs: eight, the ones on the beat kept (no micro)");
    qd_pat_init(&P0);
    qd_track_set_length(&P0, 0, 48);                    /* three pages onto one: x 1/3 */
    for (i = 0; i < 48; i += 6)
        qd_step_set_vel(&P0, 0, i, 50);
    P1 = P0;
    n = qd_tl_stretch_track(&P1, 0, -1, PW(0), 1, 0);
    ok = n == 16 && P1.trk[0].loop == 0x01;
    for (i = 0; i < 16; i++)
        ok &= qd_step_is_on(&P1.step[0][i]) == (i % 2 == 0);
    CHECK(ok && micro_at(&P1, 0, 0) == 0 && micro_at(&P1, 0, 2) == 0, "fit 1 page: 48 steps squeezed, a trig every 2nd");
    P1 = P0;
    {
        qd_tset_t ts = P1.trk[0];
        n = qd_tl_stretch_track(&P1, 0, -1, PW(0), 4, &ts);
        CHECK(n == 64 && ts.loop == 0x0F && !memcmp(&P0, &P1, sizeof P0), "fit 4 pages predicted: nothing written");
    }
    qd_pat_init(&P0);
    qd_page_set_range(&P0, 0, 0, 0, 7);                 /* PAGE: 0..7 x2 -> 0..15 */
    qd_step_set_vel(&P0, 0, 3, 60);
    P1 = P0;
    n = qd_tl_stretch_track(&P1, 0, 0, PW(1), 0, 0);
    CHECK(n == 16 && P1.trk[0].pend[0] == 15 && qd_step_is_on(&P1.step[0][6]) && !qd_step_is_on(&P1.step[0][3]),
          "PAGE x2 of 0..7: 0..15, step 4 now 7");
    qd_page_set_range(&P0, 0, 0, 2, 9);
    P1 = P0;
    n = qd_tl_stretch_track(&P1, 0, 0, PW(1), 0, 0);
    CHECK(n == 8 && !memcmp(&P0, &P1, sizeof P0), "PAGE x2 of 2..9: no room on the page, nothing");
}

/* T2: the generators */
static void t_generators(void)
{
    uint8_t pos[QD_NSTEP];
    int n, i, ok = 1, c = 0, x, y, p, d, mono = 1, prev;
    qd_pat_init(&P0);
    qd_track_set_length(&P0, 0, 128);
    n = qd_tl_positions(&P0, 0, QD_TS_TRACK, 0, pos);
    P1 = P0;
    qd_tl_random(&P1, 0, pos, n, 777u, 40, 3, 20, 50, 0, 0);
    for (i = 0; i < n; i++)
        c += qd_step_is_on(&P1.step[0][i]);
    CHECK(c >= 128 * 30 / 100 && c <= 128 * 50 / 100, "RANDOM 40 %% over 128 steps: %d trigs (within +-10 %%)", c);
    P0 = P1;
    qd_pat_init(&P1);
    qd_track_set_length(&P1, 0, 128);
    qd_tl_random(&P1, 0, pos, n, 777u, 40, 3, 20, 50, 0, 0);
    CHECK(!memcmp(&P0, &P1, sizeof P0), "RANDOM: a pure function of the seed (the same pattern again)");
    for (i = 0; i < n; i++) {
        const qd_step_t *st = &P1.step[0][i];
        if (!qd_step_is_on(st))
            continue;
        ok &= st->note[0] >= 60 - 3 && st->note[0] <= 60 + 3 && st->vel >= 80 && st->vel <= 120 && st->chance >= 50;
    }
    CHECK(ok, "its notes within 3 degrees (chromatic) of C4, velocities 100 +- 20, chances 50..100");
    P0 = P1;
    qd_tl_humanize(&P1, 0, pos, n, 5u, 6, 10);
    ok = qd_pat_check(&P1);
    for (i = 0; i < n; i++)
        if (qd_step_is_on(&P1.step[0][i]))
            ok &= P1.step[0][i].micro >= -6 && P1.step[0][i].micro <= 6 && P1.step[0][i].vel >= P0.step[0][i].vel - 10 &&
                  P1.step[0][i].vel <= P0.step[0][i].vel + 10 && qd_step_is_on(&P0.step[0][i]);
        else
            ok &= !memcmp(&P1.step[0][i], &P0.step[0][i], sizeof(qd_step_t));
    CHECK(ok, "HUMANIZE: the trigs' micro within +-6, velocity +-10; the empty steps untouched");
    ok = 1;
    for (x = 0; x < 3; x++)
        for (y = 0; y < 3; y++)
            for (p = 0; p < 3; p++)
                for (i = 0; i < 32; i++)
                    ok &= qd_tl_grids_level(i, x * 255 / 2 + (x == 1), y * 255 / 2 + (y == 1), p) ==
                          QD_GRIDS_MAP[x][y][p][i] || x == 1 || y == 1;
    for (p = 0; p < 3; p++)                             /* (the corners exactly) */
        for (i = 0; i < 32; i++)
            ok &= qd_tl_grids_level(i, 0, 0, p) == QD_GRIDS_MAP[0][0][p][i] &&
                  qd_tl_grids_level(i, 255, 255, p) == QD_GRIDS_MAP[2][2][p][i] &&
                  qd_tl_grids_level(i, 255, 0, p) == QD_GRIDS_MAP[2][0][p][i];
    CHECK(ok, "GRIDS: the map's corners are its nodes");
    for (d = 0, prev = 0; d <= 255; d += 15) {          /* (more density: a superset) */
        int m = 0;
        P1 = P0;
        qd_tl_grids(&P1, 0, pos, 32, 100, 160, d, 0, 0, 9u);
        for (i = 0; i < 32; i++)
            m += qd_step_is_on(&P1.step[0][i]);
        mono &= m >= prev;
        prev = m;
    }
    CHECK(mono && prev > 16, "GRIDS: the trigs grow with the density (%d at full)", prev);
}

/* ================================================================ TOOLS in the app */
static qd_app_t A;
static uint32_t now;
static uint64_t frames_done;
static void run(uint32_t n)
{
    static uint16_t lcd[QD_W * QD_H];
    while (n--) {
        now++;
        qd_app_tick(&A, now);
        if (now % 15u == 0u)
            qd_app_frame(&A, lcd);
        while (frames_done + 128u <= (uint64_t)now * QD_FS / 1000u) {
            int16_t blk[256];
            qd_app_audio(&A, blk, 128);
            frames_done += 128u;
        }
    }
}
#define WK(w) QD_WHITE_KEY[w]
#define BK(b) QD_BLACK_KEY[b]
#define F1 QD_B_OCTDN
#define F2 QD_B_OCTUP
static void bdown(int b) { qd_app_button(&A, b, 1); }
static void bup(int b) { qd_app_button(&A, b, 0); }
static void btap(int b)
{
    bdown(b);
    run(20);
    bup(b);
    run(20);
}
static void black(int b)
{
    qd_app_key(&A, BK(b), 1, 100);
    run(20);
    qd_app_key(&A, BK(b), 0, 0);
    run(20);
}
static void white(int w)
{
    qd_app_key(&A, WK(w), 1, 100);
    run(20);
    qd_app_key(&A, WK(w), 0, 0);
    run(20);
}
static void turn(int knob, int d)
{
    A.ui.knob_ms[knob] = 0;
    qd_app_knob(&A, knob, d);
    run(5);
}
static qd_pattern_t *PAT(void) { return qd_ctl_pat(&A); }
static qd_leds_t leds(void)
{
    qd_leds_t l;
    qd_app_leds(&A, &l);
    return l;
}
static void start(void)
{
    qd_app_init(&A);
    A.ui.view = QD_VIEW_ROLL;
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    btap(QD_B_HOME);
    bdown(QD_B_MIX);                                    /* the empty A05 */
    white(4);
    bup(QD_B_MIX);
    run(10);
}
static void tools_open(void)
{
    btap(QD_B_MIX);                                     /* (T1b: SEQ tapped) */
    run(20);
}
/* track t: a bass line on page 1 (x..x..x.x.x..x..) with notes, a lock, page 2 a few trigs, loop 1..2 */
static void bass(int t)
{
    static const char *const L = "x..x..x.x.x..x..";
    qd_pattern_t *p = PAT();
    int i;
    for (i = 0; i < 16; i++)
        if (L[i] == 'x')
            qd_step_set_note(p, t, i, 0, 36 + (i * 5) % 12);
    qd_step_set_note(p, t, 16 + 2, 0, 48);
    qd_step_set_note(p, t, 16 + 9, 0, 43);
    qd_lock_set(p, t, 3, QD_LK_PAN, -20);
    qd_page_set_loop(p, t, 0x03);
}
static int ons(int t, int s0, int n)                    /* the trigs of steps s0.. as bits */
{
    int i, m = 0;
    for (i = 0; i < n; i++)
        m |= qd_step_is_on(&PAT()->step[t][s0 + i]) << i;
    return m;
}

static void t_arp(void)
{
    start();
    bdown(QD_B_KEYS);
    run(QD_ARP_HOLD_MS + 200);
    bup(QD_B_KEYS);
    run(20);
    CHECK(!qd_tools_open(&A) && !qd_mfx_open(&A), "T1b: ARP held: no TOOLS, no MIDI FX");
    btap(QD_B_MIX);
    CHECK(qd_tools_open(&A) && qd_view_screen(&A) == QD_SCR_TOOLS && leds().button[QD_B_MIX] == QD_LED_LIT &&
              leds().button[QD_B_KEYS] == QD_LED_OFF, "SEQ tapped: TOOLS, SEQ lit (ARP dark)");
    CHECK(A.ui.tk == UI_TK_TOOL && !A.ui.tk_knob, "its tool's name pops up");
    run(QD_TOOL_POP_MS);
    CHECK(qd_tools_open(&A) && A.ui.tk == QD_TK_NONE, "TOOLS stays (latched), the name gone after ~0.8 s");
    bdown(QD_B_MIX);
    run(QD_TAP_MS + 20);
    CHECK(qd_ctl_seq_map(&A), "SEQ held over TOOLS: the patterns map");
    bup(QD_B_MIX);
    run(10);
    CHECK(qd_tools_open(&A) && !qd_ctl_seq_map(&A), "let go: TOOLS still, nothing latched");
    btap(QD_B_MIX);
    CHECK(!qd_tools_open(&A) && leds().button[QD_B_MIX] == QD_LED_OFF, "SEQ tapped again: closed, SEQ dark");
    btap(QD_B_MIX);
    btap(QD_B_HOME);
    CHECK(!qd_tools_open(&A) && leds().button[QD_B_KEYS] == QD_LED_OFF, "HOME: closed, ARP dark (no arp on)");
    bdown(QD_B_MIX);
    run(QD_TAP_MS + 20);
    bup(QD_B_MIX);
    run(10);
    CHECK(!qd_tools_open(&A) && !qd_lib_open(&A), "SEQ held, let go: nothing latched");
    btap(QD_B_KEYS);
    CHECK(qd_mfx_open(&A) && leds().button[QD_B_KEYS] == QD_LED_LIT, "ARP tapped: the MIDI FX screen, ARP lit");
    btap(QD_B_HOME);
    bdown(QD_B_FUNC);
    bdown(QD_B_KEYS);
    CHECK(A.seq.fill && leds().button[QD_B_KEYS] == QD_LED_LIT, "SHIFT + ARP: FILL at once, ARP lit");
    run(QD_ARP_HOLD_MS + 200);
    CHECK(A.seq.fill && !qd_tools_open(&A), "held long: FILL, never TOOLS");
    bup(QD_B_KEYS);
    bup(QD_B_FUNC);
    run(10);
    CHECK(!A.seq.fill && !qd_tools_open(&A) && !qd_mfx_open(&A), "let go: FILL off, nothing opened");
    PAT()->trk[0].mfx = 1;                              /* the arp's state when nothing is open */
    qd_mfx_slot_init(&A.proj.mfx[0].slot[0], QD_MFX_ARP);
    A.proj.mfx[0].slot[0].v[QD_MA_MODE] = QD_AM_ON;
    run(10);
    CHECK(leds().button[QD_B_KEYS] == QD_LED_LIT, "the track's arp on: ARP lit");
    A.proj.mfx[0].slot[0].v[QD_MA_MODE] = QD_AM_OFF;
    run(10);
    CHECK(leds().button[QD_B_KEYS] == QD_LED_OFF, "off: dark");
    A.proj.mfx[0].slot[0].v[QD_MA_MODE] = QD_AM_ON;
    bdown(QD_B_KEYS);
    run(QD_ARP_HOLD_MS + 10);
    CHECK(A.proj.mfx[0].slot[0].bypass && leds().button[QD_B_KEYS] == QD_LED_OFF && !qd_mfx_open(&A),
          "T1b: ARP held: the arp bypassed at once, ARP dark");
    bup(QD_B_KEYS);
    run(10);
    CHECK(!qd_mfx_open(&A) && !qd_tools_open(&A), "let go: nothing opened");
    bdown(QD_B_KEYS);
    run(QD_ARP_HOLD_MS + 10);
    bup(QD_B_KEYS);
    run(10);
    CHECK(!A.proj.mfx[0].slot[0].bypass && leds().button[QD_B_KEYS] == QD_LED_LIT, "held again: playing, ARP lit");
}

static void t_rotate_live(void)
{
    qd_pattern_t *before = malloc(sizeof *before);
    qd_leds_t l;
    int n0, m, w, ok = 1;
    start();
    bass(0);
    run(600);                                           /* (the edits' gesture closed) */
    *before = *PAT();
    n0 = qd_undo_count(&A, -1);
    tools_open();
    l = leds();
    CHECK(A.ui.tl.tool == QD_TL_ROTATE && A.ui.tl.scope == QD_TS_TRACK && l.key[BK(0)] == QD_LED_LIT &&
              l.key[BK(1)] == QD_LED_DIM && l.key[BK(2)] == QD_LED_DIM && l.key[BK(10)] == QD_LED_LIT &&
              l.key[WK(0)] == QD_LED_LIT && l.key[WK(1)] == QD_LED_OFF && l.key[WK(3)] == QD_LED_LIT &&
              l.button[F2] == QD_LED_OFF, "ROTATE, TRACK; B1 lit, B2 dim, B3 dim; W: the page's steps; OCT+ dark");
    turn(QD_K_1, 2);
    CHECK(A.ui.tl.rot == 2 && qd_tools_pending(&A) && A.undo.hold && leds().button[F2] == QD_LED_LIT,
          "KNOB 1 +2: pending, the gesture held open, OCT+ lit");
    CHECK(!memcmp(&PAT()->step[0][2], &before->step[0][0], sizeof(qd_step_t)) &&
              !memcmp(&PAT()->step[0][1], &before->step[0][31], sizeof(qd_step_t)) && qd_pat_check(PAT()),
          "written live: step 1 now at 3, step 32 at 2 (TRACK: the 32-step loop)");
    l = leds();
    for (w = 0; w < 16; w++)
        ok &= (l.key[WK(w)] == QD_LED_LIT) == qd_step_is_on(&PAT()->step[0][w]);
    CHECK(ok, "the W LEDs: the page as the change makes it");
    run(QD_UNDO_QUIET_MS + 200);
    CHECK(A.undo.open && A.undo.hold, "quiet for 700 ms: the gesture still open");
    turn(QD_K_1, -1);
    CHECK(A.ui.tl.rot == 1 && !memcmp(&PAT()->step[0][1], &before->step[0][0], sizeof(qd_step_t)) &&
              !memcmp(&PAT()->step[0][0], &before->step[0][31], sizeof(qd_step_t)) && qd_pat_check(PAT()),
          "KNOB 1 -1: from the original again (offset 1)");
    btap(F2);
    CHECK(!qd_tools_pending(&A) && !A.undo.hold && qd_undo_count(&A, -1) == n0 + 1 && A.ui.tl.rot == 0 &&
              qd_tools_open(&A) && A.ui.tk == UI_TK_TOOL && A.ui.tk_knob == 2 && A.ui.tk_step == 0,
          "OCT+: kept, one undo level, KEPT popped; the offset 0 again; TOOLS stays");
    m = ons(0, 0, 16);
    btap(F1);
    CHECK(qd_tools_open(&A) && ons(0, 0, 16) == m, "OCT- with nothing pending: nothing (TOOLS stays: HOME leaves)");
    bdown(QD_B_FUNC);
    btap(F1);
    bup(QD_B_FUNC);
    run(10);
    CHECK(!memcmp(PAT()->step, before->step, sizeof before->step) && !memcmp(PAT()->lock, before->lock, sizeof before->lock) &&
              PAT()->nlock == before->nlock, "SHIFT + F1: undone, the original byte for byte");
    turn(QD_K_1, 5);
    n0 = qd_undo_count(&A, -1);
    btap(F1);
    CHECK(!memcmp(PAT()->step, before->step, sizeof before->step) && !qd_tools_pending(&A) && qd_tools_open(&A) &&
              qd_undo_count(&A, -1) == n0 && A.ui.tl.rot == 0 && A.ui.tk_knob == 2 && A.ui.tk_step == 1,
          "OCT-: reverted byte for byte, no level, REVERTED popped, TOOLS open");
    btap(F1);
    CHECK(qd_tools_open(&A), "OCT- again: still open");
    turn(QD_K_1, 3);
    black(0);
    CHECK(A.ui.tl.rot == 0 && !memcmp(PAT()->step, before->step, sizeof before->step) && qd_tools_pending(&A),
          "B1 again: the offset back to 0 (the original heard), still pending");
    btap(F1);
    btap(QD_B_HOME);
    CHECK(!qd_tools_open(&A), "HOME: closed");
    free(before);
}

static void t_knobs(void)                               /* SELECT the page, PRESETS the tool, ALGORITHM the scope */
{
    int m;
    start();
    bass(0);
    run(600);
    tools_open();
    turn(QD_K_SELECT, 1);
    CHECK(A.ui.page[0] == 1 && qd_tools_open(&A), "SELECT: page 2 shown");
    turn(QD_K_SELECT, -2);
    CHECK(A.ui.page[0] == 7, "SELECT -2: round to page 8");
    turn(QD_K_SELECT, 1);
    turn(QD_K_PRESETS, 1);
    CHECK(A.ui.tl.tool == QD_TL_REVERSE && A.ui.tk == UI_TK_TOOL && !A.ui.tk_knob, "PRESETS: the next tool, its name");
    turn(QD_K_PRESETS, 2);
    CHECK(A.ui.tl.tool == QD_TL_EUCLID, "PRESETS +2: RANDOM, EUCLID");
    turn(QD_K_PRESETS, -4);
    CHECK(A.ui.tl.tool == QD_TL_CLIP, "PRESETS back: round to CLIP (the tenth, after STRETCH)");
    turn(QD_K_PRESETS, -1);
    CHECK(A.ui.tl.tool == QD_TL_STRETCH, "PRESETS -1: STRETCH");
    turn(QD_K_ALGO, -1);
    CHECK(A.ui.tl.scope == QD_TS_PAGE && A.ui.tk_knob == 1, "ALGORITHM: the scope (PAGE), its takeover");
    turn(QD_K_ALGO, -5);
    CHECK(A.ui.tl.scope == QD_TS_SEL, "ALGORITHM down: SELECTED (the end)");
    black(10);
    CHECK(A.ui.tl.scope == QD_TS_PAGE, "B11: PAGE");
    black(10), black(10), black(10);
    CHECK(A.ui.tl.scope == QD_TS_SEL, "B11: TRACK, PATTERN, round to SELECTED");
    black(0);
    black(10);                                          /* PAGE */
    turn(QD_K_1, 1);
    m = ons(0, 0, 16);
    turn(QD_K_SELECT, 1);
    CHECK(!qd_tools_pending(&A) && ons(0, 0, 16) == m && A.ui.page[0] == 1,
          "PAGE scope, a page change: the pending change kept first");
    btap(QD_B_HOME);
}

static void t_selected(void)                            /* the step lock: SELECTED STEPS; W toggles the selection */
{
    qd_pattern_t *before = malloc(sizeof *before);
    int i, ok;
    start();
    bass(0);
    run(600);
    *before = *PAT();
    btap(QD_B_MUTE);                                    /* LFO: the step lock on (its selection: one step) */
    CHECK(A.ui.slatch, "the step lock on");
    tools_open();
    CHECK(A.ui.tl.scope == QD_TS_SEL, "TOOLS with the lock on: SELECTED STEPS");
    for (i = 0; i < A.ui.nheld; i++)                    /* (an empty selection to start from) */
        ;
    while (A.ui.nheld)
        qd_step_select_toggle(&A, A.ui.held[0]);
    white(0), white(3), white(5);                       /* steps 1 (on), 4 (on), 6 (off) */
    CHECK(A.ui.nheld == 3 && qd_tools_sel_mask(&A) == 0x29, "W1, W4, W6: selected (the mask 1, 4, 6)");
    white(5);
    CHECK(A.ui.nheld == 2, "W6 again: out");
    white(5);
    black(1);                                           /* REVERSE */
    black(1);                                           /* flip */
    ok = qd_step_is_on(&PAT()->step[0][0]) == qd_step_is_on(&before->step[0][5]) &&
         qd_step_is_on(&PAT()->step[0][5]) == qd_step_is_on(&before->step[0][0]) &&
         !memcmp(&PAT()->step[0][3], &before->step[0][3], sizeof(qd_step_t));
    for (i = 6; i < 32; i++)
        ok &= !memcmp(&PAT()->step[0][i], &before->step[0][i], sizeof(qd_step_t));
    CHECK(ok && qd_tools_pending(&A), "REVERSE on the selection: 1 <-> 6, 4 stays, nothing else");
    white(8);                                           /* step 9 (on) joins: live */
    CHECK(qd_tools_pending(&A) && qd_step_is_on(&PAT()->step[0][0]) == qd_step_is_on(&before->step[0][8]) &&
              qd_step_is_on(&PAT()->step[0][8]) == qd_step_is_on(&before->step[0][0]),
          "W9 while pending: the selection follows, live (1 <-> 9)");
    btap(F1);
    CHECK(!memcmp(PAT()->step, before->step, sizeof before->step), "OCT-: back");
    black(10);
    white(2);
    CHECK(A.ui.tl.scope == QD_TS_SEL && A.ui.tk_knob == 1, "PAGE scope, a W: SELECTED again");
    btap(QD_B_HOME);
    free(before);
}

static void t_switch_home(void)
{
    int n0, m;
    start();
    bass(0);
    bass(2);
    run(600);
    n0 = qd_undo_count(&A, -1);
    tools_open();
    turn(QD_K_1, 1);
    m = ons(0, 0, 16);
    black(1);                                           /* REVERSE: the rotation kept, a level */
    CHECK(A.ui.tl.tool == QD_TL_REVERSE && !qd_tools_pending(&A) && ons(0, 0, 16) == m &&
              qd_undo_count(&A, -1) == n0 + 1 && A.ui.tk == UI_TK_TOOL, "B2: the pending ROTATE kept (a level), REVERSE "
          "(its name)");
    black(1);
    CHECK(qd_tools_pending(&A) && (A.ui.tl.flags & QD_TLF_FLIP) && qd_step_is_on(&PAT()->step[0][31]) == (m & 1),
          "B2 again: flipped (live): step 1 at 32");
    black(1);
    CHECK(qd_tools_pending(&A) && ons(0, 0, 16) == m, "B2 again: flipped back");
    black(1);
    m = ons(0, 0, 16);
    bdown(QD_B_FUNC);                                   /* SHIFT + SELECT: another track; the flip kept */
    turn(QD_K_SELECT, 2);
    bup(QD_B_FUNC);
    run(10);
    CHECK(A.sel == 2 && !qd_tools_pending(&A) && ons(0, 0, 16) == m && qd_undo_count(&A, -1) == n0 + 2,
          "SHIFT + SELECT: track 3, the flip kept first (a level)");
    black(0);
    turn(QD_K_1, 4);
    btap(QD_B_HOME);
    CHECK(!qd_tools_open(&A) && !A.undo.hold && qd_undo_count(&A, -1) == n0 + 3,
          "HOME: closed, the pending change kept (a level)");
}

static void t_t2_live(void)                             /* RANDOM, HUMANIZE, GRIDS live; GRIDS over the PATTERN */
{
    qd_pattern_t *before = malloc(sizeof *before);
    int m, n0, k;
    int8_t tr[3];
    start();
    bass(0);
    bass(1);
    run(600);
    *before = *PAT();
    n0 = qd_undo_count(&A, -1);
    tools_open();
    black(2);
    CHECK(A.ui.tl.tool == QD_TL_RANDOM && A.ui.tl.v.rn.dens == 50 && !qd_tools_pending(&A), "B3: RANDOM (50 %%), "
          "nothing yet");
    turn(QD_K_1, 10);
    m = ons(0, 0, 16);
    CHECK(qd_tools_pending(&A) && A.ui.tl.v.rn.dens == 60, "KNOB 1: the density (60 %%), rolled live");
    black(2);
    CHECK(qd_tools_pending(&A) && ons(0, 0, 16) != m, "B3 again: a new seed, another roll (heard)");
    turn(QD_K_4, 30);
    CHECK(A.ui.tl.v.rn.chance == 30, "KNOB 4: the chance spread");
    btap(F1);
    CHECK(!memcmp(PAT()->step, before->step, sizeof before->step), "OCT-: the original back");
    black(5);
    turn(QD_K_1, 4);
    turn(QD_K_2, 10);
    CHECK(qd_tools_pending(&A) && A.ui.tl.v.hu.micro == 4 && A.ui.tl.v.hu.vel == 10, "HUMANIZE: KNOB 1 timing, KNOB 2 "
          "velocity, live");
    btap(F2);
    CHECK(qd_undo_count(&A, -1) == n0 + 1, "OCT+: a level");
    btap(F1);
    black(4);
    turn(QD_K_ALGO, 1);                                    /* PATTERN */
    turn(QD_K_3, 20);
    qd_tools_grids_tracks(&A, tr);
    CHECK(A.ui.tl.tool == QD_TL_GRIDS && A.ui.tl.scope == QD_TS_PATTERN && tr[0] == 0 && tr[1] == 1 && tr[2] == 2,
          "GRIDS, PATTERN: the parts LOW / MID / HIGH to tracks 1 (DRUMS), 2, 3");
    {
        int ok = 1, t, i;
        for (k = 0; k < 3; k++)
            for (i = 0; i < 16; i++)
                ok &= qd_step_is_on(&PAT()->step[k][i]) ==
                      (qd_tl_grids_at(A.ui.tl.v.gr.seed, k, i, 64, 128, k, 0) > 255 - A.ui.tl.v.gr.dens);
        for (t = 3; t < QD_PAT_NTRACK; t++)
            ok &= !memcmp(PAT()->step[t], before->step[t], sizeof before->step[t]);
        CHECK(ok, "each part on its track (its level over 255 - density), the others untouched");
    }
    black(4);
    CHECK(A.ui.tl.v.gr.part == 1, "B5 again: the next part (for one track)");
    btap(F1);
    btap(QD_B_HOME);
    free(before);
}

/* T3, CLIP (2026-10-25: a page of its own, PRESETS past STRETCH): KNOB 1 COPY / PASTE / CLEAR, OCT+ does it; COPY and
 * PASTE PAGE and PATTERN only (OCT+ dark elsewhere), CLEAR at once for SELECTED and PAGE, twice for TRACK and PATTERN;
 * B9 and B10 nothing now */
static void clip_do(int act)                            /* KNOB 1 to the action, OCT+ */
{
    turn(QD_K_1, act - A.ui.tl.v.cl.act);
    btap(F2);
}
static void t_clip(void)
{
    qd_pattern_t *before = malloc(sizeof *before);
    int n0, ok, i;
    start();
    bass(0);
    bass(3);
    run(600);
    *before = *PAT();
    tools_open();
    black(8), black(9);
    CHECK(A.ui.tl.tool == QD_TL_ROTATE && !A.ui.clip.pg.valid && leds().key[BK(8)] == QD_LED_OFF &&
              leds().key[BK(9)] == QD_LED_OFF, "B9, B10: nothing (dark)");
    turn(QD_K_PRESETS, -1);
    turn(QD_K_ALGO, -1);                                /* PAGE */
    CHECK(A.ui.tl.tool == QD_TL_CLIP && A.ui.tl.scope == QD_TS_PAGE && A.ui.tl.v.cl.act == QD_CL_COPY &&
              leds().button[F2] == QD_LED_LIT, "PRESETS -1: CLIP, on COPY (OCT+ lit)");
    turn(QD_K_1, 1);
    CHECK(A.ui.tl.v.cl.act == QD_CL_PASTE && leds().button[F2] == QD_LED_OFF, "KNOB 1: PASTE, nothing copied (OCT+ dark)");
    clip_do(QD_CL_COPY);
    CHECK(A.ui.clip.pg.valid && A.ui.tk_knob == 2 && A.ui.tk_step == 3, "COPY, OCT+: the page copied (COPIED)");
    turn(QD_K_SELECT, 2);                               /* page 3 */
    n0 = qd_undo_count(&A, -1);
    clip_do(QD_CL_PASTE);
    run(QD_UNDO_QUIET_MS + 50);
    ok = 1;
    for (i = 0; i < 16; i++)
        ok &= !memcmp(&PAT()->step[0][32 + i], &before->step[0][i], sizeof(qd_step_t));
    CHECK(ok && qd_undo_count(&A, -1) == n0 + 1 && A.ui.tk_step == 4, "PASTE on page 3: page 1 pasted there, a level");
    clip_do(QD_CL_CLEAR);
    run(QD_UNDO_QUIET_MS + 50);
    CHECK(ons(0, 32, 16) == 0 && qd_undo_count(&A, -1) == n0 + 2, "CLEAR on PAGE: cleared at once, a level");
    turn(QD_K_ALGO, 1);                                 /* TRACK */
    CHECK(leds().button[F2] == QD_LED_LIT, "TRACK: CLEAR can (OCT+ lit)");
    turn(QD_K_1, -2);
    CHECK(A.ui.tl.v.cl.act == QD_CL_COPY && leds().button[F2] == QD_LED_OFF, "TRACK: no COPY (OCT+ dark)");
    btap(F2);
    CHECK(ons(0, 0, 16) == ons(0, 0, 16) && A.ui.tk_step != 3, "OCT+ on COPY in TRACK: nothing");
    clip_do(QD_CL_CLEAR);
    CHECK(ons(0, 0, 16) && A.ui.tk_step == 5, "CLEAR on TRACK: CLEAR? (nothing yet)");
    btap(F2);
    CHECK(!ons(0, 0, 16) && !ons(0, 16, 16) && ons(3, 0, 16), "OCT+ again: track 1 cleared, track 4 not");
    turn(QD_K_ALGO, 1);                                 /* PATTERN */
    bdown(QD_B_FUNC);
    btap(F1), btap(F1);                                 /* (SHIFT + F1 twice: both clears undone) */
    bup(QD_B_FUNC);
    run(10);
    CHECK(!memcmp(PAT()->step[0], before->step[0], 32 * sizeof(qd_step_t)), "undone twice: track 1 back");
    clip_do(QD_CL_COPY);
    CHECK(A.undo.pclip_ok, "PATTERN, COPY: the pattern copied");
    clip_do(QD_CL_CLEAR);
    btap(F2);
    ok = 1;
    for (i = 0; i < QD_PAT_NTRACK; i++)
        ok &= !ons(i, 0, 16);
    CHECK(ok, "CLEAR twice: every track cleared");
    clip_do(QD_CL_PASTE);
    CHECK(!memcmp(PAT()->step, before->step, 32 * sizeof(qd_step_t)) && ons(3, 0, 16), "PASTE: the pattern pasted back");
    btap(QD_B_HOME);
    btap(QD_B_MUTE);                                    /* the lock: SELECTED */
    tools_open();
    while (A.ui.nheld)
        qd_step_select_toggle(&A, A.ui.held[0]);
    qd_ctl_set_page(&A, 0);
    white(0), white(6);
    CHECK(A.ui.tl.tool == QD_TL_CLIP && A.ui.tl.scope == QD_TS_SEL, "TOOLS again: CLIP as it was left, SELECTED");
    clip_do(QD_CL_CLEAR);
    CHECK(!qd_step_is_on(&PAT()->step[0][0]) && !qd_step_is_on(&PAT()->step[0][6]) && qd_step_is_on(&PAT()->step[0][3]),
          "SELECTED, CLEAR: the selected steps cleared at once, the others kept");
    btap(QD_B_HOME);
    free(before);
}

#ifdef QD_UNDO_SLOTS
/* T3: a change too big for the undo log: the crossed-out arrow, OCT+ twice, no level */
static void t_noroom(void)
{
    qd_pattern_t *p;
    int t, s, n0;
    start();
    p = PAT();
    for (t = 0; t < 2; t++)
        for (s = 0; s < QD_NSTEP; s++) {
            qd_step_set_vel(p, t, s, 1 + (int)(rnd() % 127));
            qd_step_set_note(p, t, s, 0, 30 + (int)(rnd() % 60));
            qd_step_set_micro(p, t, s, 1 + (int)(rnd() % 20));
        }
    for (t = 0; t < 2; t++)
        qd_track_set_length(p, t, 128);
    run(600);
    bdown(QD_B_FUNC);
    black(0);                                           /* (a snapshot: the log's room halved) */
    bup(QD_B_FUNC);
    run(600);
    n0 = qd_undo_count(&A, -1);
    tools_open();
    black(3);
    turn(QD_K_ALGO, 1);                                    /* PATTERN */
    turn(QD_K_1, -4);                                   /* 0 hits: every trig goes */
    CHECK(qd_tools_pending(&A) && !(A.ui.tl.flags & QD_TLF_NOLIVE) && (A.ui.tl.flags & QD_TLF_NOROOM) && !ons(0, 0, 16),
          "a change too big for an undo level: live, marked (the crossed-out arrow)");
    btap(F2);
    CHECK(qd_tools_pending(&A) && (A.ui.tl.flags & QD_TLF_ARMED) && A.ui.tk_step == 2, "OCT+ once: NO UNDO, still "
          "pending");
    btap(F2);
    CHECK(!qd_tools_pending(&A) && !ons(0, 0, 16) && qd_undo_count(&A, -1) == n0, "OCT+ again: kept, no level");
    btap(QD_B_HOME);
}
#endif

static void t_euclid_pattern(void)
{
    qd_pattern_t *before = malloc(sizeof *before);
    int t, ok = 1, n0;
    start();
    bass(0);
    bass(3);
    run(600);
    *before = *PAT();
    n0 = qd_undo_count(&A, -1);
    tools_open();
    black(3);
    black(10);                                          /* TRACK -> PATTERN */
    CHECK(A.ui.tl.tool == QD_TL_EUCLID && A.ui.tl.scope == QD_TS_PATTERN, "EUCLID, PATTERN");
    turn(QD_K_1, -1);                                   /* 3 of 16 */
    turn(QD_K_2, -8);                                   /* 3 of 8 */
    CHECK(A.ui.tl.v.eu.hits == 3 && A.ui.tl.v.eu.len == 8, "KNOB 1 the hits, KNOB 2 the length (3 of 8)");
    {
        int m = 0, i, o = 0;
        for (i = 0; i < 16; i++)
            m |= qd_tl_euclid_hit(i, 3, 8, 0) << i;
        for (t = 0; t < QD_PAT_NTRACK; t++)
            ok &= ons(t, 0, 16) == m;
        CHECK(ok, "every track: E(3, 8) on page 1 (its loop), heard at once");
        turn(QD_K_4, 1);                             /* ADD */
        for (i = 0; i < 16; i++)
            o |= qd_step_is_on(&before->step[0][i]) << i;
        CHECK(A.ui.tl.v.eu.mode == QD_EU_ADD && ons(0, 0, 16) == (m | o),
              "KNOB 4: ADD (from the original: its trigs back)");
    }
    btap(F2);
    CHECK(qd_undo_count(&A, -1) == n0 + 1, "OCT+: one level for the six tracks");
    bdown(QD_B_FUNC);
    btap(F1);
    bup(QD_B_FUNC);
    run(10);
    CHECK(!memcmp(PAT()->step, before->step, sizeof before->step) && PAT()->nlock == before->nlock,
          "undone: every track back byte for byte");
    btap(QD_B_HOME);
    free(before);
}

static void t_length_live(void)
{
    int n0;
    start();
    bass(1);
    qd_ctl_select_track(&A, 1);
    run(600);
    n0 = qd_undo_count(&A, -1);
    tools_open();
    black(7);                                           /* (B8: LENGTH, 2026-10-25) */
    turn(QD_K_1, 1);
    CHECK(qd_track_len(&PAT()->trk[1]) == 64 && qd_step_is_on(&PAT()->step[1][32]), "LENGTH, KNOB 1 +1: x2 live (64)");
    turn(QD_K_1, -1);
    turn(QD_K_1, -1);
    CHECK(qd_track_len(&PAT()->trk[1]) == 16 && A.ui.tl.lpow == -1, "-2: /2 from the original (16)");
    btap(F2);
    CHECK(qd_track_len(&PAT()->trk[1]) == 16 && qd_undo_count(&A, -1) == n0 + 1, "OCT+: kept, a level");
    btap(QD_B_HOME);
}

#ifdef QD_UNDO_SLOTS
/* the device's log: a snapshot of a big pattern leaves no room for a second copy: TOOLS draws the change only */
static void t_drawn_only(void)
{
    qd_pattern_t *p;
    int t, s, n0, on0, w, ok = 1;
    uint8_t on[16], was[16], note[16];
    start();
    p = PAT();
    for (t = 0; t < QD_PAT_NTRACK; t++)
        for (s = 0; s < QD_NSTEP; s++) {
            if (t == 0 && s >= 16)
                continue;                               /* (track 1: its page 1 alone, x.x. ..) */
            if (t == 0 && (s & 1))
                continue;
            qd_step_set_vel(p, t, s, 1 + (int)(rnd() % 127));
            qd_step_set_note(p, t, s, 0, 30 + (int)(rnd() % 60));
            qd_step_set_note(p, t, s, 1, 91 + (int)(rnd() % 30));
            qd_step_set_micro(p, t, s, 1 + (int)(rnd() % 20));
            qd_step_set_chance(p, t, s, 1 + (int)(rnd() % 99));
        }
    run(600);
    bdown(QD_B_FUNC);
    black(0);                                           /* SHIFT + B1: the snapshot stored */
    bup(QD_B_FUNC);
    run(600);
    CHECK(A.undo.snap_ok, "the snapshot taken (the log's top)");
    n0 = qd_undo_count(&A, -1);
    on0 = ons(0, 0, 16);
    tools_open();
    turn(QD_K_1, 1);
    CHECK(qd_tools_pending(&A) && (A.ui.tl.flags & QD_TLF_NOLIVE) && ons(0, 0, 16) == on0,
          "no room for the original: drawn only (nothing written)");
    qd_tools_page(&A, on, was, note);
    for (w = 0; w < 16; w++)
        ok &= on[w] == !!(w & 1) && was[w] == !(w & 1);
    CHECK(ok, "the lane computed: each trig one on (rings new, the old ones going)");
    run(QD_UNDO_QUIET_MS + 100);
    btap(F2);
    CHECK(!qd_tools_pending(&A) && ons(0, 0, 16) == (on0 << 1 & 0xFFFF) && qd_undo_count(&A, -1) == n0,
          "OCT+: written now, no undo level");
    btap(QD_B_HOME);
}
#endif

/* the owner's findings on the unit (2026-10-24, 0.12.0-beta.2): a 64-step track playing, the page shown by SELECT,
 * EUCLID over the TRACK, the ghosts and the LEDs on every page, the selected step rotated, a short loop, RANDOM's names */
static void t_owner(void)
{
    uint8_t on[16], was[16], note[16];
    uint16_t inr;
    qd_leds_t l;
    int s, pg, ok = 1, m = 0, i, w;
    start();
    for (s = 0; s < 64; s++)                            /* track 1: 64 steps, every other one on (x.x.) */
        if (!(s & 1))
            qd_step_on(PAT(), 0, s);
    qd_track_set_length(PAT(), 0, 64);
    qd_step_on(PAT(), 1, 7);                            /* track 2: a 5-step loop (1..5), a trig at 8 outside it */
    qd_step_on(PAT(), 1, 1);
    qd_track_set_length(PAT(), 1, 5);
    run(600);
    qd_app_play(&A);
    run(300);
    tools_open();
    qd_ctl_set_page(&A, 0);
    turn(QD_K_SELECT, 2);
    CHECK(A.ui.page[0] == 2 && A.ui.tk == UI_TK_PAGE, "SELECT: page 3 shown, the pages' takeover (PAGE 3)");
    for (i = 0; i < 200; i++) {                         /* (the playhead crosses every page) */
        run(50);
        ok &= A.ui.page[0] == 2;
        m |= 1 << qd_ctl_playing_page(&A, 0);
    }
    CHECK(ok && m == 0x0F, "playing: TOOLS keeps the page SELECT chose (no page follow)");
    ok = 1, m = 0;
    black(3);                                           /* EUCLID, TRACK */
    turn(QD_K_1, 1);                                    /* E(5, 16) */
    for (i = 0; i < 16; i++)
        m |= qd_tl_euclid_hit(i, 5, 16, 0) << i;
    for (pg = 0; pg < 4; pg++)
        ok &= ons(0, pg * 16, 16) == m;
    CHECK(A.ui.tl.scope == QD_TS_TRACK && ok, "EUCLID over the TRACK: all four pages E(5, 16), live");
    inr = qd_tools_page(&A, on, was, note);
    l = leds();
    ok = inr == 0xFFFF;
    for (w = 0; w < 16; w++) {
        ok &= on[w] == ((m >> w) & 1) && was[w] == !(w & 1);
        ok &= l.key[WK(w)] == (on[w] ? QD_LED_LIT : was[w] ? QD_LED_DIM : QD_LED_OFF);
    }
    CHECK(ok, "page 3: the steps as they are now, the ghosts as they were; W lit now, dim the ghosts");
    btap(F2);
    qd_tools_page(&A, on, was, note);
    CHECK(!memcmp(on, was, 16) && leds().key[WK(1)] == QD_LED_OFF, "OCT+: kept, the ghosts gone");
    bdown(QD_B_FUNC);                                   /* track 2: the 5-step loop */
    turn(QD_K_SELECT, 1);
    bup(QD_B_FUNC);
    run(10);
    inr = qd_tools_page(&A, on, was, note);
    l = leds();
    CHECK(A.sel == 1 && inr == 0x1F && on[7] && l.key[WK(7)] == QD_LED_OFF,
          "a 5-step loop: its steps 1..5 play; step 8 (on, outside) dark on its key");
    turn(QD_K_1, 1);
    CHECK(A.ui.tl.v.eu.len == 5 && ons(1, 0, 5) == 0x1F && qd_step_is_on(&PAT()->step[1][7]),
          "EUCLID on it: the loop's 5 steps, step 8 as it was");
    btap(F2);
    qd_app_stop(&A);
    btap(QD_B_HOME);
}

static void t_sel_rotate(void)                          /* one step selected and rotated: it moves */
{
    int n0;
    start();
    bass(0);                                            /* x..x..x.x.x..x.. | page 2 */
    run(600);
    n0 = qd_undo_count(&A, -1);
    tools_open();
    white(3);                                           /* step 4 (on) selected: SELECTED */
    CHECK(A.ui.tl.scope == QD_TS_SEL && A.ui.nheld == 1 && A.ui.held[0] == 3, "W4: SELECTED, step 4");
    turn(QD_K_1, 1);
    CHECK(qd_tools_pending(&A) && qd_step_is_on(&PAT()->step[0][4]) && !qd_step_is_on(&PAT()->step[0][3]) &&
              PAT()->step[0][4].note[0] == 36 + 15 % 12 && qd_pat_check(PAT()) && qd_tools_sel_mask(&A) == 0x10,
          "KNOB 1 +1: the step one on (its note, its lock), the selection with it");
    turn(QD_K_1, 2);
    CHECK(qd_step_is_on(&PAT()->step[0][6]) && PAT()->step[0][6].note[0] == 36 + 15 % 12 &&
              qd_step_is_on(&PAT()->step[0][5]) && !qd_step_is_on(&PAT()->step[0][3]) && qd_tools_sel_mask(&A) == 0x40,
          "+2 more: at step 7, the steps it passed each one back (7's trig at 6)");
    btap(F2);
    CHECK(!qd_tools_pending(&A) && A.ui.held[0] == 6 && qd_undo_count(&A, -1) == n0 + 1 && qd_tools_sel_mask(&A) == 0x40,
          "OCT+: kept (a level), the selection now step 7");
    black(3);                                           /* EUCLID: the selection's 1 step */
    CHECK(A.ui.tl.v.eu.len == 1, "EUCLID in SELECTED: its length the selection's");
    black(10);
    CHECK(A.ui.tl.scope == QD_TS_PAGE && A.ui.tl.v.eu.len == 16, "out to PAGE (B11): the length 16 again");
    btap(QD_B_HOME);
}

static void t_random_names(void)                        /* RANDOM's knobs: their names pop while turning */
{
    start();
    bass(0);
    run(600);
    tools_open();
    black(2);
    run(QD_TOOL_POP_MS + 10);
    turn(QD_K_2, 1);
    CHECK(A.ui.tk == UI_TK_TOOL && A.ui.tk_knob == 3 && A.ui.tk_step == 1, "KNOB 2: NOTES pops");
    turn(QD_K_4, 1);
    CHECK(A.ui.tk_knob == 3 && A.ui.tk_step == 3, "KNOB 4: CHANCE");
    btap(F1);
    btap(QD_B_HOME);
}

/* QUANTIZE (HUMANIZE's opposite): the steps' own micro toward the grid, their own velocities toward the default */
static void t_quantize(void)
{
    uint8_t pos[QD_NSTEP];
    int n, i, ok = 1, ok2 = 1, ok3 = 1;
    CHECK(qd_tl_quant_to(9, QD_QG_1, 100) == 0 && qd_tl_quant_to(-23, QD_QG_1, 100) == 0 &&
              qd_tl_quant_to(9, QD_QG_2, 100) == 12 && qd_tl_quant_to(6, QD_QG_2, 100) == 0 &&
              qd_tl_quant_to(20, QD_QG_2, 100) == 12 && qd_tl_quant_to(-18, QD_QG_2, 100) == -12 &&
              qd_tl_quant_to(-5, QD_QG_3, 100) == -8 && qd_tl_quant_to(4, QD_QG_3, 100) == 0 &&
              qd_tl_quant_to(22, QD_QG_4, 100) == 18 && qd_tl_quant_to(10, QD_QG_1, 50) == 5 &&
              qd_tl_quant_to(-7, QD_QG_1, 50) == -3 && qd_tl_quant_to(9, QD_QG_2, 50) == 11 &&
              qd_tl_quant_to(13, QD_QG_1, 0) == 13,
          "the grid's lines (a step, 1/2, 1/3, 1/4; the step's own, a tie to the step's), 50 %% halfway (toward the line)");
    fill(&P0);
    P1 = P0;
    n = qd_tl_positions(&P1, 0, QD_TS_TRACK, 0, pos);
    qd_tl_quantize(&P1, 0, pos, n, QD_QG_1, 100, 0);
    for (i = 0; i < QD_NSTEP; i++) {
        const qd_step_t *a = &P0.step[0][i], *b = &P1.step[0][i];
        qd_rstep_t r;
        qd_step_resolve(&P1, 0, i, &r);
        if (qd_tl_pos_of(pos, n, i) >= 0 && qd_step_is_on(a) && (a->ovr & QD_OV(QD_PM_MICRO)))
            ok &= r.micro == 0 && !(b->ovr & QD_OV(QD_PM_MICRO)) && b->vel == a->vel && !memcmp(b->note, a->note, QD_NNOTE);
        else
            ok &= !memcmp(a, b, sizeof *a);
    }
    CHECK(ok && qd_pat_check(&P1) && !memcmp(P0.step[1], P1.step[1], sizeof P0.step[1]) &&
              !memcmp(P0.lock, P1.lock, sizeof P0.lock),
          "100 %%: every trig's own micro on its step (0, the override gone), nothing else, no other track, the locks kept");
    P1 = P0;
    qd_tl_quantize(&P1, 0, pos, n, QD_QG_1, 50, 0);
    for (i = 0; i < n; i++) {
        const qd_step_t *a = &P0.step[0][pos[i]], *b = &P1.step[0][pos[i]];
        qd_rstep_t r;
        qd_step_resolve(&P1, 0, pos[i], &r);
        if (qd_step_is_on(a) && (a->ovr & QD_OV(QD_PM_MICRO)))
            ok2 &= r.micro == a->micro / 2;
        else
            ok2 &= !memcmp(a, b, sizeof *a);
    }
    CHECK(ok2, "50 %%: every own micro halved (toward the step)");
    P1 = P0;
    qd_tl_quantize(&P1, 0, pos, n, QD_QG_1, 0, 100);
    for (i = 0; i < n; i++) {
        const qd_step_t *a = &P0.step[0][pos[i]], *b = &P1.step[0][pos[i]];
        qd_rstep_t r;
        qd_step_resolve(&P1, 0, pos[i], &r);
        if (qd_step_is_on(a) && (a->ovr & QD_OV(QD_PM_VEL)))
            ok3 &= r.vel == P1.trk[0].def.vel && !(b->ovr & QD_OV(QD_PM_VEL)) && b->micro == a->micro;
        else
            ok3 &= !memcmp(a, b, sizeof *a);
    }
    CHECK(ok3, "the velocity's 100 %%: every own velocity back to the track's (the override gone), the timing kept");
}

/* QUANTIZE live in TOOLS: PRESETS reaches it (no B key), each scope, ghosts, OCT- byte-exact, OCT+ a level, undo */
static int micro_of(int t, int s)
{
    qd_rstep_t r;
    qd_step_resolve(PAT(), t, s, &r);
    return r.micro;
}
static int vel_of(int t, int s)
{
    qd_rstep_t r;
    qd_step_resolve(PAT(), t, s, &r);
    return r.vel;
}
static int same_pat(const qd_pattern_t *b)
{
    return !memcmp(PAT()->step, b->step, sizeof b->step) && !memcmp(PAT()->lock, b->lock, sizeof b->lock) &&
           PAT()->nlock == b->nlock && !memcmp(PAT()->trk, b->trk, sizeof b->trk);
}
static void t_quant_live(void)
{
    qd_pattern_t *before = malloc(sizeof *before);
    int8_t mi[16], mi0[16];
    uint8_t ve[16], ve0[16];
    qd_leds_t l;
    int n0, t, i, ok = 1;
    start();
    bass(0);
    bass(1);
    for (t = 0; t < 2; t++)                             /* (off the grid: page 1's trigs +9 / -10, page 2's +7) */
        for (i = 0; i < 32; i++)
            if (qd_step_is_on(&PAT()->step[t][i])) {
                qd_step_set_micro(PAT(), t, i, i >= 16 ? 7 : i & 1 ? -10 : 9);
                qd_step_set_vel(PAT(), t, i, 60 + i);
            }
    run(600);
    *before = *PAT();
    n0 = qd_undo_count(&A, -1);
    tools_open();
    black(6);                                           /* (B7: QUANTIZE, 2026-10-25) */
    l = leds();
    for (i = 0; i < QD_TL_NKEY; i++)
        ok &= l.key[BK(i)] == (i == 6 ? QD_LED_LIT : QD_LED_DIM);
    CHECK(A.ui.tl.tool == QD_TL_QUANTIZE && A.ui.tl.scope == QD_TS_TRACK && !qd_tools_pending(&A) && ok &&
              l.key[BK(8)] == QD_LED_OFF && l.key[BK(9)] == QD_LED_OFF && A.ui.tk == UI_TK_TOOL && !A.ui.tk_knob,
          "B7: QUANTIZE (its name pops; B7 lit, the other tools dim, B9 B10 dark); nothing yet");
    turn(QD_K_1, 10);
    CHECK(qd_tools_pending(&A) && A.ui.tl.v.qu.amt == 50 && micro_of(0, 0) == 4 && micro_of(0, 3) == -5 &&
              micro_of(0, 18) == 3 && !memcmp(PAT()->step[1], before->step[1], sizeof before->step[1]),
          "KNOB 1 +10: 50 %% live, TRACK: both pages' micro halved, track 2 untouched");
    qd_tools_timing(&A, mi, ve, mi0, ve0);
    CHECK(mi[0] == 4 && mi0[0] == 9 && mi[3] == -5 && mi0[3] == -10 && ve[0] == ve0[0], "the ghosts: as they were");
    btap(F1);
    CHECK(same_pat(before) && !qd_tools_pending(&A) && qd_undo_count(&A, -1) == n0, "OCT-: byte for byte, no level");
    turn(QD_K_1, 20);
    CHECK(A.ui.tl.v.qu.amt == 100 && micro_of(0, 0) == 0 && micro_of(0, 3) == 0 && micro_of(0, 18) == 0 &&
              PAT()->step[0][0].vel == before->step[0][0].vel, "100 %%: on the grid, the velocities kept");
    turn(QD_K_3, 20);
    CHECK(A.ui.tl.v.qu.vel == 100 && vel_of(0, 0) == PAT()->trk[0].def.vel &&
              !(PAT()->step[0][0].ovr & QD_OV(QD_PM_VEL)), "KNOB 3 100 %%: the velocities back to the track's");
    turn(QD_K_3, -20);
    turn(QD_K_2, 1);
    CHECK(A.ui.tl.v.qu.grid == QD_QG_2 && micro_of(0, 0) == 12 && micro_of(0, 3) == -12 && micro_of(0, 18) == 12,
          "KNOB 2: the half-step grid (+9 -> +12, -10 -> -12, +7 -> +12)");
    turn(QD_K_2, -1);
    btap(F2);
    CHECK(!qd_tools_pending(&A) && qd_undo_count(&A, -1) == n0 + 1 && micro_of(0, 0) == 0, "OCT+: kept, one level");
    bdown(QD_B_FUNC);
    btap(F1);
    bup(QD_B_FUNC);
    run(10);
    CHECK(same_pat(before), "SHIFT + F1: undone byte for byte");
    turn(QD_K_ALGO, -1);                                /* PAGE */
    turn(QD_K_1, -1);                                   /* (100 % kept from before: a change again) */
    turn(QD_K_1, 20);
    CHECK(A.ui.tl.scope == QD_TS_PAGE && micro_of(0, 0) == 0 && micro_of(0, 18) == 7, "PAGE: page 1 only");
    btap(F1);
    CHECK(same_pat(before), "OCT-: byte for byte");
    turn(QD_K_ALGO, 2);                                 /* PATTERN */
    turn(QD_K_1, -1);                                   /* (100 % kept from before: a change again) */
    turn(QD_K_1, 20);
    CHECK(A.ui.tl.scope == QD_TS_PATTERN && micro_of(0, 3) == 0 && micro_of(1, 3) == 0 && micro_of(1, 18) == 0,
          "PATTERN: every track");
    btap(F1);
    CHECK(same_pat(before), "OCT-: byte for byte");
    while (A.ui.nheld)                                  /* (W1 and W4 selected) */
        qd_step_select_toggle(&A, A.ui.held[0]);
    white(0), white(3);
    turn(QD_K_1, -1);                                   /* (100 % kept from before: a change again) */
    turn(QD_K_1, 20);
    CHECK(A.ui.tl.scope == QD_TS_SEL && micro_of(0, 0) == 0 && micro_of(0, 3) == 0 && micro_of(0, 6) == 9 &&
              micro_of(0, 18) == 7, "SELECTED: steps 1 and 4 only");
    btap(F1);
    CHECK(same_pat(before), "OCT-: byte for byte");
    A.ui.tl.flags = QD_TLF_PEND | QD_TLF_NOLIVE, A.ui.tl.otrack = 0, A.ui.tl.opage = 0;   /* (drawn only: worked out) */
    A.ui.tl.scope = QD_TS_TRACK, A.ui.tl.v.qu.amt = 100;
    qd_tools_timing(&A, mi, ve, mi0, ve0);
    CHECK(mi[0] == 0 && mi0[0] == 9 && mi[6] == 0 && same_pat(before), "drawn only: the result worked out, nothing written");
    A.ui.tl.flags = 0;
    btap(QD_B_HOME);
    free(before);
}

/* STRETCH live (2026-10-25): PRESETS reaches it; x2 heard at once, OCT- back byte for byte, fit 1 page, OCT+ one level,
 * undo byte for byte; its screen's before and after */
static void t_stretch_live(void)
{
    qd_pattern_t *before = malloc(sizeof *before);
    uint8_t on[16], in[16], on0[16], in0[16];
    int n0, ok, i;
    start();
    bass(1);                                            /* (track 2: 32 steps, pages 1 and 2) */
    qd_ctl_select_track(&A, 1);
    run(600);
    *before = *PAT();
    n0 = qd_undo_count(&A, -1);
    tools_open();
    turn(QD_K_PRESETS, -2);
    CHECK(A.ui.tl.tool == QD_TL_STRETCH && A.ui.tl.scope == QD_TS_TRACK && !qd_tools_pending(&A), "PRESETS -2: STRETCH");
    turn(QD_K_1, 1);
    CHECK(qd_tools_pending(&A) && qd_track_len(&PAT()->trk[1]) == 64 && qd_step_is_on(&PAT()->step[1][6]) &&
              !qd_step_is_on(&PAT()->step[1][3]) && qd_lock_get(PAT(), 1, 6, QD_LK_PAN, 0),
          "KNOB 1: x2 live (64 steps; step 4's trig and its lock on step 7)");
    qd_tools_stretch(&A, on, in, on0, in0);
    CHECK((on[0] >> 6 & 1) && !(on[0] >> 3 & 1) && (on0[0] >> 3 & 1) && in[7] && !in0[7] && in0[3],
          "its screen: the trigs now and before, the loop now (64) and before (32)");
    turn(QD_K_1, 1), turn(QD_K_1, 1), turn(QD_K_1, 1);
    CHECK(qd_track_len(&PAT()->trk[1]) == 128 && A.ui.tl.lpow == 2, "KNOB 1 up three more: x4 at most (128 steps)");
    btap(F1);
    CHECK(!qd_tools_pending(&A) && !memcmp(PAT(), before, sizeof *before), "OCT-: back byte for byte");
    turn(QD_K_2, 1);
    {
        int want = 0, got = 0, j, o;
        for (i = 0; i < 32; i++)                        /* (where each trig goes: the op's own mapping) */
            if (qd_step_is_on(&before->step[1][i]))
                qd_tl_stretch_to(i, 0, 16, 32, 16, &j, &o), want |= 1 << j;
        for (i = 0; i < 16; i++)
            got |= qd_step_is_on(&PAT()->step[1][i]) << i;
        ok = qd_track_len(&PAT()->trk[1]) == 16 && got == want && want;
    }
    CHECK(ok && A.ui.tl.v.st.fit == 1, "KNOB 2: onto 1 page (32 -> 16), the trigs halved in time");
    btap(F2);
    run(QD_UNDO_QUIET_MS + 50);
    CHECK(!qd_tools_pending(&A) && qd_undo_count(&A, -1) == n0 + 1, "OCT+: kept, one level");
    bdown(QD_B_FUNC);
    btap(F1);
    bup(QD_B_FUNC);
    run(10);
    CHECK(!memcmp(PAT(), before, sizeof *before), "SHIFT + F1: undone byte for byte");
    A.ui.tl.flags = QD_TLF_PEND | QD_TLF_NOLIVE, A.ui.tl.otrack = 1, A.ui.tl.opage = 0;   /* (drawn only: worked out) */
    A.ui.tl.scope = QD_TS_TRACK, A.ui.tl.lpow = 1, A.ui.tl.v.st.fit = 0;
    {
        uint8_t lp;
        const int len = qd_tools_result_len(&A, &lp);
        qd_tools_stretch(&A, on, in, on0, in0);
        CHECK(len == 64 && lp == 0x0F && in[7] && !on[0] && (on0[0] >> 3 & 1) && !memcmp(PAT(), before, sizeof *before),
              "drawn only: the loop after worked out (64), the trigs before, nothing written");
    }
    A.ui.tl.flags = 0, A.ui.tl.lpow = 0;
    btap(QD_B_HOME);
    free(before);
}

static void t_screens(void)                             /* every tool's screen, every scope, drawn (sanitizers) */
{
    static uint16_t lcd[QD_W * QD_H];
    int tool, sc;
    start();
    bass(0);
    qd_app_play(&A);
    run(300);
    tools_open();
    for (tool = 0; tool < QD_TL_N; tool++) {
        if (!QD_TL_BUILT(tool))
            continue;
        if (tool < QD_TL_NKEY)
            black(tool);
        else                                            /* (QUANTIZE: PRESETS only) */
            while (A.ui.tl.tool != tool)
                turn(QD_K_PRESETS, 1);
        for (sc = 0; sc < QD_TS_N; sc++) {
            turn(QD_K_4, 1);
            turn(QD_K_1, 2);
            turn(QD_K_2, 3);
            turn(QD_K_3, 1);
            run(40);
            qd_app_frame(&A, lcd);
            black(10);
            run(30);
            qd_app_frame(&A, lcd);
        }
    }
    CHECK(qd_view_screen(&A) == QD_SCR_TOOLS, "every tool's screen and the takeovers drawn");
    qd_app_stop(&A);
    btap(QD_B_HOME);
}

int main(void)
{
    static const struct {
        const char *name;
        void (*fn)(void);
    } G[] = {
        {"ops: rotate (identity, one on, the pool)", t_rotate},
        {"ops: reverse (twice, micro mirrored)", t_reverse},
        {"ops: euclid (counts, E(3,8), NEW / ADD / MASK)", t_euclid},
        {"ops: length (x2, /2, a page's end)", t_length},
        {"ops: STRETCH (x8 / 8 the identity, /2, fit, a page)", t_stretch},
        {"ops: RANDOM, HUMANIZE, GRIDS (seeded, ranges, the map)", t_generators},
        {"ARP: tapped MIDI FX, held TOOLS, SHIFT + ARP FILL; the LED", t_arp},
        {"ROTATE live: heard, OCT+ a level, OCT- back, undo", t_rotate_live},
        {"SELECT the page, PRESETS the tool, ALGORITHM / B11 the scope", t_knobs},
        {"the step lock: SELECTED STEPS, W toggles, live", t_selected},
        {"switching tool / track keeps; HOME keeps", t_switch_home},
        {"EUCLID, PATTERN scope: every track, one level", t_euclid_pattern},
        {"T2 live: RANDOM, HUMANIZE, GRIDS (PATTERN: the parts)", t_t2_live},
        {"CLIP: COPY / PASTE / CLEAR by scope (KNOB 1, OCT+)", t_clip},
#ifdef QD_UNDO_SLOTS
        {"T3: no room for undo: OCT+ twice, no level", t_noroom},
#endif
        {"LENGTH live: x2, /2, kept", t_length_live},
#ifdef QD_UNDO_SLOTS
        {"the device's drawn-only fallback (no room)", t_drawn_only},
#endif
        {"the owner's: 64 steps, SELECT's page kept, ghosts, LEDs, a short loop", t_owner},
        {"SELECTED + ROTATE: the selected step moves", t_sel_rotate},
        {"RANDOM: the knobs' names pop", t_random_names},
        {"ops: QUANTIZE (the grid's lines, 100 %, 50 %, the velocity)", t_quantize},
        {"QUANTIZE live: PRESETS, each scope, ghosts, OCT- exact, undo", t_quant_live},
        {"STRETCH live: x2, OCT- exact, fit a page, OCT+ a level, undo", t_stretch_live},
        {"every screen drawn", t_screens},
    };
    size_t i;
    for (i = 0; i < sizeof G / sizeof G[0]; i++) {
        const int f0 = fails, c0 = checks;
        G[i].fn();
        printf("  %-60s %d checks%s\n", G[i].name, checks - c0, fails > f0 ? "  FAILED" : "");
    }
    printf("test_tools: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
