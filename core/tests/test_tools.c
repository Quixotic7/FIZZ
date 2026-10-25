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
 *             at once; ROTATE / REVERSE / EUCLID / LENGTH live (the pattern rewritten at each change, heard), OCT+ one
 *             undo level that restores the original byte for byte, OCT- the original back and no level, OCT- again
 *             closes; switching the tool / the scope / the track keeps a pending change; HOME keeps it; scope PAGE /
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
    bdown(QD_B_KEYS);
    run(QD_ARP_HOLD_MS + 10);
    bup(QD_B_KEYS);
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
    run(QD_ARP_HOLD_MS - 10);
    CHECK(!qd_tools_open(&A), "ARP held 390 ms: nothing yet");
    run(15);
    CHECK(qd_tools_open(&A) && qd_view_screen(&A) == QD_SCR_TOOLS && leds().button[QD_B_KEYS] == QD_LED_LIT,
          "ARP held 400 ms: TOOLS at once, ARP lit");
    CHECK(A.ui.tk == UI_TK_TOOL && !A.ui.tk_knob, "its tool's name pops up");
    bup(QD_B_KEYS);
    run(QD_TOOL_POP_MS);
    CHECK(qd_tools_open(&A) && A.ui.tk == QD_TK_NONE, "let go: TOOLS stays (latched), the name gone after ~0.8 s");
    btap(QD_B_HOME);
    CHECK(!qd_tools_open(&A) && leds().button[QD_B_KEYS] == QD_LED_OFF, "HOME: closed, ARP dark (no arp on)");
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
}

static void t_rotate_live(void)
{
    qd_pattern_t *before = malloc(sizeof *before);
    qd_leds_t l;
    int n0, m;
    start();
    bass(0);
    run(600);                                           /* (the edits' gesture closed) */
    *before = *PAT();
    n0 = qd_undo_count(&A, -1);
    tools_open();
    l = leds();
    CHECK(A.ui.tl.tool == QD_TL_ROTATE && A.ui.tl.scope == QD_TS_TRACK && l.key[BK(0)] == QD_LED_LIT &&
              l.key[BK(1)] == QD_LED_DIM && l.key[BK(2)] == QD_LED_OFF && l.key[WK(0)] == QD_LED_LIT &&
              l.key[WK(5)] == QD_LED_DIM && l.key[WK(6)] == QD_LED_OFF && l.key[BK(10)] == QD_LED_LIT &&
              l.button[F2] == QD_LED_OFF, "ROTATE, TRACK; B1 lit, B2 dim, B3 (T2) dark, W1 lit, W6 dim, W7 dark, OCT+ dark");
    turn(QD_K_SELECT, 2);
    CHECK(A.ui.tl.rot == 2 && qd_tools_pending(&A) && A.undo.hold && leds().button[F2] == QD_LED_LIT,
          "SELECT +2: pending, the gesture held open, OCT+ lit");
    CHECK(!memcmp(&PAT()->step[0][2], &before->step[0][0], sizeof(qd_step_t)) &&
              !memcmp(&PAT()->step[0][1], &before->step[0][31], sizeof(qd_step_t)) && qd_pat_check(PAT()),
          "written live: step 1 now at 3, step 32 at 2 (TRACK: the 32-step loop)");
    run(QD_UNDO_QUIET_MS + 200);
    CHECK(A.undo.open && A.undo.hold, "quiet for 700 ms: the gesture still open");
    turn(QD_K_1, -1);
    CHECK(A.ui.tl.rot == 1 && !memcmp(&PAT()->step[0][1], &before->step[0][0], sizeof(qd_step_t)) &&
              !memcmp(&PAT()->step[0][0], &before->step[0][31], sizeof(qd_step_t)) && qd_pat_check(PAT()),
          "KNOB 1 -1: from the original again (offset 1)");
    btap(F2);
    CHECK(!qd_tools_pending(&A) && !A.undo.hold && qd_undo_count(&A, -1) == n0 + 1 && A.ui.tl.rot == 0 &&
              qd_tools_open(&A), "OCT+: kept, one undo level; the offset 0 again; TOOLS stays");
    m = ons(0, 0, 16);
    btap(F1);
    CHECK(!qd_tools_open(&A) && ons(0, 0, 16) == m, "OCT- with nothing pending: TOOLS closes, nothing changed");
    bdown(QD_B_FUNC);
    btap(F1);
    bup(QD_B_FUNC);
    run(10);
    CHECK(!memcmp(PAT()->step, before->step, sizeof before->step) && !memcmp(PAT()->lock, before->lock, sizeof before->lock) &&
              PAT()->nlock == before->nlock, "SHIFT + F1: undone, the original byte for byte");
    /* OCT-: the original back, no level */
    tools_open();
    turn(QD_K_SELECT, 5);
    n0 = qd_undo_count(&A, -1);
    btap(F1);
    CHECK(!memcmp(PAT()->step, before->step, sizeof before->step) && !qd_tools_pending(&A) && qd_tools_open(&A) &&
              qd_undo_count(&A, -1) == n0 && A.ui.tl.rot == 0, "OCT-: reverted byte for byte, no level, TOOLS open");
    btap(F1);
    CHECK(!qd_tools_open(&A), "OCT- again: closed");
    /* B1 again: back to 0 */
    tools_open();
    turn(QD_K_SELECT, 3);
    black(0);
    CHECK(A.ui.tl.rot == 0 && !memcmp(PAT()->step, before->step, sizeof before->step) && qd_tools_pending(&A),
          "B1 again: the offset back to 0 (the original heard), still pending");
    btap(F1);
    btap(QD_B_HOME);
    free(before);
}

static void t_switch_home(void)
{
    qd_pattern_t *before = malloc(sizeof *before);
    int n0, m;
    start();
    bass(0);
    run(600);
    *before = *PAT();
    n0 = qd_undo_count(&A, -1);
    tools_open();
    turn(QD_K_SELECT, 1);
    m = ons(0, 0, 16);
    black(1);                                           /* REVERSE: the rotation kept, a level */
    CHECK(A.ui.tl.tool == QD_TL_REVERSE && !qd_tools_pending(&A) && ons(0, 0, 16) == m &&
              qd_undo_count(&A, -1) == n0 + 1 && A.ui.tk == UI_TK_TOOL, "B2: the pending ROTATE kept (a level), REVERSE "
          "(its name)");
    black(1);
    CHECK(qd_tools_pending(&A) && (A.ui.tl.flags & QD_TLF_FLIP) && qd_step_is_on(&PAT()->step[0][31]) ==
              ((m >> 0) & 1), "B2 again: flipped (live): step 1 at 32");
    black(1);
    CHECK(qd_tools_pending(&A) && ons(0, 0, 16) == m, "B2 again: flipped back");
    black(1);
    m = ons(0, 0, 16);
    white(2);                                           /* another track: kept */
    CHECK(A.sel == 2 && !qd_tools_pending(&A) && ons(0, 0, 16) == m && qd_undo_count(&A, -1) == n0 + 2,
          "W3: track 3, the flip kept first (a level)");
    white(7);
    CHECK(A.sel == 2, "W8: no track (six)");
    white(0);
    black(10);
    CHECK(A.ui.tl.scope == QD_TS_PATTERN && A.ui.tk == UI_TK_TOOL && A.ui.tk_knob == 1, "B11: PATTERN, its takeover");
    run(QD_TOOL_POP_MS + 100);
    CHECK(A.ui.tk == UI_TK_TOOL, "the scope's takeover stays a second (not the name's 0.8 s)");
    black(10);
    CHECK(A.ui.tl.scope == QD_TS_PAGE, "B11 again: PAGE (round)");
    black(10);
    CHECK(A.ui.tl.scope == QD_TS_TRACK, ".. TRACK");
    black(2);
    CHECK(A.ui.tl.tool == QD_TL_REVERSE, "B3 (RANDOM, T2): nothing");
    black(0);
    turn(QD_K_SELECT, 4);
    m = ons(0, 0, 16);
    btap(QD_B_HOME);
    CHECK(!qd_tools_open(&A) && ons(0, 0, 16) == m && !A.undo.hold && qd_undo_count(&A, -1) == n0 + 3,
          "HOME: closed, the pending change kept (a level)");
    free(before);
}

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
    CHECK(A.ui.tl.ehits == 3 && A.ui.tl.elen == 8, "KNOB 1 the hits, KNOB 2 the length (3 of 8)");
    {
        int m = 0, i, o = 0;
        for (i = 0; i < 16; i++)
            m |= qd_tl_euclid_hit(i, 3, 8, 0) << i;
        for (t = 0; t < QD_PAT_NTRACK; t++)
            ok &= ons(t, 0, 16) == m;
        CHECK(ok, "every track: E(3, 8) on page 1 (its loop), heard at once");
        turn(QD_K_4, 1);                                /* ADD */
        for (i = 0; i < 16; i++)
            o |= qd_step_is_on(&before->step[0][i]) << i;
        CHECK(A.ui.tl.emode == QD_EU_ADD && ons(0, 0, 16) == (m | o), "KNOB 4: ADD (from the original: its trigs back)");
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
    black(6);
    turn(QD_K_SELECT, 1);
    CHECK(qd_track_len(&PAT()->trk[1]) == 64 && qd_step_is_on(&PAT()->step[1][32]), "LENGTH, SELECT +1: x2 live (64)");
    turn(QD_K_SELECT, -1);
    turn(QD_K_SELECT, -1);
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
    int t, s, n0, on0, w, on, was, note, ok = 1;
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
    turn(QD_K_SELECT, 1);
    CHECK(qd_tools_pending(&A) && (A.ui.tl.flags & QD_TLF_NOLIVE) && ons(0, 0, 16) == on0,
          "no room for the original: drawn only (nothing written)");
    for (w = 0; w < 16; w++) {
        qd_tools_lane(&A, w, &on, &was, &note);
        ok &= on == !!(w & 1) && was == !(w & 1);
    }
    CHECK(ok, "the lane computed: each trig one on (rings new, the old ones going)");
    run(QD_UNDO_QUIET_MS + 100);
    btap(F2);
    CHECK(!qd_tools_pending(&A) && ons(0, 0, 16) == (on0 << 1 & 0xFFFF) && qd_undo_count(&A, -1) == n0,
          "OCT+: written now, no undo level");
    btap(QD_B_HOME);
}
#endif

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
        black(tool);
        for (sc = 0; sc < QD_TS_N; sc++) {
            turn(QD_K_SELECT, 1);
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
        {"ARP: tapped MIDI FX, held TOOLS, SHIFT + ARP FILL; the LED", t_arp},
        {"ROTATE live: heard, OCT+ a level, OCT- back, undo", t_rotate_live},
        {"switching tool / track / scope keeps; HOME keeps", t_switch_home},
        {"EUCLID, PATTERN scope: every track, one level", t_euclid_pattern},
        {"LENGTH live: x2, /2, kept", t_length_live},
#ifdef QD_UNDO_SLOTS
        {"the device's drawn-only fallback (no room)", t_drawn_only},
#endif
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
