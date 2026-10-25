/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.3b's and P1.3c's acceptance (PLAN §8b, the sequencer controls version 2 and 2.1): the controller driven as the glue
 * drives it
 * (core/app/app.h: input events between millisecond ticks, the audio in 128-frame blocks, a frame every 15 ms, so every
 * screen is drawn under the sanitizers too).
 *
 *   gestures     a table of scenarios, one per §8b gesture, each a scripted sequence of presses, holds and turns,
 *                ending in assertions on the model and the UI state: tap / hold, the hold modes (B1..B10), every
 *                mode's ten values and B11, ALGORITHM (the track's default with nothing held, the value finely with a
 *                step held), SELECT (pages), note entry (replace / add / the octave / a scale), SOUND's banks and
 *                PRESETS as the sound lock, p-locks, F1 (pages: tap, double tap, A..B; copy, cut; clear page / track,
 *                undo / redo), F2 (tracks, mute, solo; paste; page copy / paste), F3 (rate, end, start..end, tempo,
 *                swing), REC (the keyboard, REC + PLAY, SHIFT + REC, step and live recording), ARP (FILL), PAGE
 *                (follow; + W), the tracks screen, MIDI into a held step, the layer maps and SHIFT's map, a default
 *                change moving only the steps that do not override it
 *   P1.3c        F1's page layer (tap, double tap, hold toggle, never-empty loop, A..B; B9 tap / hold; B10 copy then
 *                paste onto two pages; undo of each, as one per hold); F1 + W copy then paste and F2's pre-load; F2's
 *                pick-up / drop press by press against the test's reading of Form's code; SEL (select, tap mute, hold
 *                solo, B11, LEDs); SEQ (tap queues by the launch mode, hold at once, ALGORITHM END BAR BEAT NOW); F3
 *                (base / type keys and knobs, start / end clamps, all 21 rates' step lengths); NOTE's five knobs
 *                (chromatic and in a scale); five notes by the keys; P-LOCK (the screen stays, the knobs lock, B11,
 *                undo); the OCT LEDs' blink levels in NOTE, the keyboard and SOUND's banks, the octave takeover
 *   P1.3d        (§8b 2.2) every mode's knob layout with a step held and with nothing held (each knob changes exactly
 *                its value, or the track's default, and shows its band); P-LOCK's ALGORITHM choosing tracks; the banks;
 *                the stopped preview (struck when held, re-struck by edits, released, several at once, nothing while
 *                playing, nothing written); F3 latched (open, used, closed by OCT- or HOME, nothing leaking into F1 /
 *                F2); the step latch on LFO (select, add, no deletes, edits, preview, off)
 *   beat         "program a beat": from the empty A05, panel presses and turns only build a pattern with a page that
 *                has its own start and end, a two-page loop, a pool sound lock, a velocity override, a ratchet, a
 *                condition (moved by F2's cut / drop), a chord, a five-note chord (keys, then KNOB 4), a P-LOCK lock, a
 *                12-step track, two default changes and SEL's mute of track 6; it equals the same pattern built
 *                through the API byte for byte (steps, locks, track settings) and in its timeline
 *   leds         the levels per situation (§8b's LEDs)
 *   determinism  one gesture session twice: the same model, the same samples
 *
 *     test_ctl      exit 1 on a failed check */
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#ifdef QD_DEVICE
#include "qd_arena.h"
#endif
#include "qd_gfx.h"
#include "chord.h"
#include "scale.h"
#include "sounds.h"
#include "names.h"
#include "ui.h"
#include "qd_fx.h"
#include "snd_peek.h"                          /* (P2.1: LIB, POOL) */

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

static qd_app_t A;
static uint32_t now;
static uint64_t frames_done, hash;

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
            int k;
            qd_app_audio(&A, blk, 128);
            for (k = 0; k < 256; k++)
                hash = (hash ^ (uint16_t)blk[k]) * 1099511628211ull;
            frames_done += 128u;
        }
    }
}
static void fresh(void)
{
    qd_app_init(&A);
    A.ui.view = QD_VIEW_ROLL;   /* (P1.5s: EDIT is the power-on view; made on the PIANO ROLL) */
    A.ui.lmode = QD_LMODE_VALUES;   /* (the scenarios were written for VALUES, the old default; MODES is the default
                                     * since 2026-10-25: g_knob_layouts checks it) */
    now = 0;
    frames_done = 0;
    hash = 14695981039346656037ull;
    qd_app_tick(&A, 0);
}

/* the panel (PLAN §8b's names) */
#define WK(w) QD_WHITE_KEY[w]
#define BK(b) QD_BLACK_KEY[b]
enum { B1, B2, B3, B4, B5, B6, B7, B8, B9, B10, B11 };
enum { M_NOTE, M_VEL, M_LEN, M_MICRO, M_RATCH, M_CHNC, M_COND, M_TRIG, M_FUNC, M_SOUND, M_PLOCK };   /* (P1.3w: B9 FUNC, B10 SOUND) */
#define F1 QD_B_OCTDN
#define F2 QD_B_OCTUP
#define SHIFT QD_B_FUNC
static void kdown(int k) { qd_app_key(&A, k, 1, 100); }
static void kup(int k) { qd_app_key(&A, k, 0, 0); }
static void bdown(int b) { qd_app_button(&A, b, 1); }
static void bup(int b) { qd_app_button(&A, b, 0); }
static void btap(int b)
{
    bdown(b);
    run(20);
    bup(b);
    run(20);
}
static void wtap(int w)                                 /* a tap: 30 ms */
{
    kdown(WK(w));
    run(30);
    kup(WK(w));
    run(10);
}
static void whold(int w)                                /* held: past 250 ms */
{
    kdown(WK(w));
    run(300);
}
static void wrel(int w)
{
    kup(WK(w));
    run(10);
}
static void black(int b)                                /* a black key tapped (B1..B11) */
{
    kdown(BK(b));
    run(20);
    kup(BK(b));
    run(10);
}
static void mode(int m) { black(m); }                   /* nothing held: the hold mode */
static void turn(int knob, int d)                      /* (one gesture: no acceleration from the turn before) */
{
    A.ui.knob_ms[knob] = 0;
    qd_app_knob(&A, knob, d);
    run(5);
}
static void f3_down(void)
{
    bdown(F1);
    run(10);
    bdown(F2);
    run(10);
}
static void f3_up(void)                                /* P1.3d: F3 is latched: let go, then OCT- exits it */
{
    bup(F2);
    bup(F1);
    run(10);
    btap(F1);
}
static void wake(void) { btap(QD_B_HOME); }             /* power-on: idle; HOME: the step view */
static void mkturn(int m, int knob, int d)              /* P1.3y: a mode key held, a knob (its default), let go */
{
    kdown(BK(m));
    run(20);
    turn(knob, d);
    kup(BK(m));
    run(10);
}
static void shome(void)                                 /* P1.3y: SHIFT + HOME: the next view (HOME alone: back) */
{
    bdown(SHIFT);
    run(10);
    btap(QD_B_HOME);
    bup(SHIFT);
    run(10);
}
static void shome_wrap(void)                            /* P2.11: from EDIT past the four Geek Out pages: the PIANO ROLL */
{
    int i;
    for (i = 0; i < 5; i++)
        shome();
}
static void empty_pattern(int k)                        /* SEQ held + white key k + 1 (stopped: at once) */
{
    bdown(QD_B_MIX);
    wtap(k);
    bup(QD_B_MIX);
    run(10);
}
/* track 1's playhead in steps (P2.4: the core gives num / den; the test reads it as a double) */
static double track_pos0(void)
{
    int64_t n, d;
    qd_seq_track_pos_q(&A.seq, 0, &n, &d);
    return (double)n / (double)d;
}
static qd_pattern_t *PAT(void) { return qd_ctl_pat(&A); }
static const qd_step_t *ST(int t, int s) { return qd_step(PAT(), t, s); }
static qd_rstep_t R(int t, int s)
{
    qd_rstep_t r;
    qd_step_resolve(PAT(), t, s, &r);
    return r;
}
static int OV(int t, int s, int pm) { return qd_step_overrides(ST(t, s), pm); }
static int LK(int t, int s, int id)                     /* the lock's value, or -99999 */
{
    int16_t v;
    return qd_lock_get(PAT(), t, s, id, &v) ? v : -99999;
}
static int pages_full(int t, int from)                  /* pages from..8 each 0..15 */
{
    int pg, ok = 1;
    for (pg = from; pg < QD_NPAGE; pg++)
        ok &= PAT()->trk[t].pstart[pg] == 0 && PAT()->trk[t].pend[pg] == QD_PAGE - 1;
    return ok;
}
static qd_leds_t leds(void)
{
    qd_leds_t l;
    qd_app_leds(&A, &l);
    return l;
}
/* a fresh unit on the empty pattern A05, the step view, track 1 */
static int long_names;                                  /* P2.6h: every name at its longest (start) */
static void names_long(void)
{
    int t, p, k;
    for (t = 0; t < QD_NTRACK; t++) {
        memset(A.model.t[t].name, 'W', QD_TRACK_NAME - 1);
        A.model.t[t].name[QD_TRACK_NAME - 1] = 0;
        memset(A.model.t[t].sound_name, 'M', QD_SOUND_NAME - 1);
        A.model.t[t].sound_name[QD_SOUND_NAME - 1] = 0;
        for (p = 0; p < 64; p++) {
            qd_pattern_t *P = qd_proj_pat(&A.proj, p);
            memset(P->snd[t].name, 'W', QD_SOUND_NAME - 1);
            P->snd[t].name[QD_SOUND_NAME - 1] = 0;
        }
    }
    for (k = 0; k < QD_NPROJ; k++) {
        memset(A.per.pname[k], 'W', QD_PROJ_NAME - 1);
        A.per.pname[k][QD_PROJ_NAME - 1] = 0;
    }
}
static void start(void)
{
    fresh();
    wake();
    empty_pattern(4);
    if (long_names)
        names_long();
}

/* ================================================================ the gesture scenarios === */
static void g_wake(void)
{
    int on0;
    fresh();
    on0 = R(0, 0).on;
    CHECK(A.ui.mode == QD_MODE_IDLE, "power-on: the idle screen");
    wtap(0);
    CHECK(A.ui.mode == QD_MODE_STEP && R(0, 0).on == on0, "a key wakes to the step view and does nothing else");
    run(QD_IDLE_MS + 10);
    CHECK(A.ui.mode == QD_MODE_IDLE, "two minutes untouched, stopped: idle again");
    btap(QD_B_PLAY);
    run(QD_IDLE_MS + 10);
    CHECK(A.ui.mode == QD_MODE_STEP, "playing: no idle");
    btap(QD_B_PLAY);
}

static void g_tap(void)
{
    qd_leds_t l;
    start();
    kdown(WK(1));
    run(1);
    l = leds();
    CHECK(l.key[WK(1)] == QD_LED_LIT && !R(0, 1).on, "the LED at the press, the step on at the release");
    run(30);
    kup(WK(1));
    run(5);
    CHECK(R(0, 1).on && ST(0, 1)->ovr == 0 && R(0, 1).type == QD_TRIG_NOTE && R(0, 1).notes[0] == 60 &&
              R(0, 1).vel == 100 && R(0, 1).len == QD_LEN_1,
          "tap: a trig with no overrides, the track's defaults");
    qd_lock_set(PAT(), 0, 1, QD_QP_FREQ, 5000);
    wtap(1);
    CHECK(!R(0, 1).on && qd_lock_count(PAT(), 0, 1) == 0, "tap again: empty, its locks gone");
    bdown(SHIFT);
    wtap(3);
    bup(SHIFT);
    CHECK(!R(0, 3).on, "SHIFT + W: free (nothing)");
}

static void g_edge(void)
{
    start();
    kdown(WK(2));
    run(249);
    kup(WK(2));
    run(5);
    CHECK(R(0, 2).on && A.ui.nheld == 0, "released at 249 ms: a tap");
    kdown(WK(3));
    run(250);
    CHECK(A.ui.nheld == 1 && A.ui.held[0] == 3, "250 ms: held");
    kup(WK(3));
    run(5);
    CHECK(!R(0, 3).on && A.ui.nheld == 0, "released after the hold: no toggle, let go");
    mode(M_VEL);                                        /* (P1.3c: in NOTE KNOB 1 is note 2) */
    kdown(WK(4));                                       /* a knob turned at 100 ms: held at once */
    run(100);
    turn(QD_K_1, 2);
    CHECK(A.ui.nheld == 1, "a knob turn makes a pending key held");
    kup(WK(4));
    run(5);
    CHECK(R(0, 4).on && R(0, 4).vel == 102 && OV(0, 4, QD_PM_VEL) && qd_lock_count(PAT(), 0, 4) == 0,
          "an empty step held + KNOB 1 (VEL, P1.3d): the velocity 102, the step on (no lock)");
    mode(M_NOTE);
    kdown(WK(6));                                       /* a black key while pending: held, the value set */
    run(50);
    black(B5);                                          /* (NOTE mode: B5 = E4) */
    kup(WK(6));
    run(5);
    CHECK(R(0, 6).on && R(0, 6).notes[0] == 64 && R(0, 6).nn == 1, "a black key while a white is down: held, the "
          "value set (E4)");
}

static void g_modes(void)
{
    int m;
    start();
    for (m = 0; m < 10; m++) {                          /* (P1.3w: B9 FUNC, B10 SOUND) */
        mode(m);
        CHECK(A.ui.hmode == m && leds().key[BK(m)] == QD_LED_LIT, "nothing held: B%d = the hold mode %s", m + 1,
              UI_FIELD_NAME[m]);
    }
    CHECK(!strcmp(UI_FIELD_NAME[M_FUNC], "FUNC") && !strcmp(UI_FIELD_NAME[M_SOUND], "SOUND"), "B9 FUNC, B10 SOUND");
    black(B11);
    CHECK(A.ui.hmode == UI_F_PLOCK && !R(0, 0).on, "B11 with nothing held: the P-LOCK mode (P1.3c, question 37)");
}

/* every mode's ten values on a held step, and B11 */
static void g_values(void)
{
    static const int VEL[10] = {13, 25, 38, 51, 64, 76, 89, 102, 114, 127};
    static const int LEN[10] = {1, 2, 3, 4, 5, 6, 8, 10, 12, QD_LEN_INF};
    static const int MIC[10] = {-20, -15, -10, -5, -2, 2, 5, 10, 15, 20};
    int b, ok = 0, n = 0;
    start();
    wtap(0);
    /* VEL, LEN, MICRO, CHNC: each key its value, overridden */
    for (b = 0; b < 10; b++) {
        int ok1;
        mode(M_VEL);
        whold(0);
        black(b);
        ok1 = R(0, 0).vel == VEL[b] && OV(0, 0, QD_PM_VEL);
        black(B11);
        ok1 &= !OV(0, 0, QD_PM_VEL) && R(0, 0).vel == 100;
        wrel(0);
        mode(M_LEN);
        whold(0);
        black(b);
        ok1 &= R(0, 0).len == LEN[b] && OV(0, 0, QD_PM_LEN);
        wrel(0);
        mode(M_MICRO);
        whold(0);
        black(b);
        ok1 &= R(0, 0).micro == MIC[b] && OV(0, 0, QD_PM_MICRO);
        wrel(0);
        mode(M_CHNC);
        whold(0);
        black(b);
        ok1 &= R(0, 0).chance == (b + 1) * 10 && OV(0, 0, QD_PM_CHANCE);
        wrel(0);
        CHECK(ok1, "B%d: VEL %d, LEN %s, MICRO %+d, CHNC %d%% (and VEL's B11 back to the track's)", b + 1, VEL[b],
              qd_len_name(LEN[b]), MIC[b], (b + 1) * 10);
        ok += ok1;
        n++;
    }
    /* B11 for each: back to the track's */
    mode(M_LEN), whold(0), black(B11), wrel(0);
    mode(M_MICRO), whold(0), black(B11), wrel(0);
    mode(M_CHNC), whold(0), black(B11), wrel(0);
    CHECK(ST(0, 0)->ovr == 0 && R(0, 0).on, "B11 in LEN, MICRO, CHNC: no overrides left");
    /* RATCH: B1..B8 counts, B9 the ramp cycling, B10 nothing */
    mode(M_RATCH);
    whold(0);
    for (b = 0; b < 8; b++) {
        black(b);
        CHECK(R(0, 0).ratch == b + 1, "RATCH B%d: x%d", b + 1, b + 1);
    }
    black(B9);
    CHECK(R(0, 0).ramp == QD_RAMP_UP && R(0, 0).ratch == 8, "RATCH B9: the ramp UP (the count kept)");
    black(B9), black(B9), black(B9);
    CHECK(R(0, 0).ramp == QD_RAMP_FLAT, "B9 four times: round to FLAT");
    black(B10);
    CHECK(R(0, 0).ratch == 8, "B10: nothing");
    black(B11);
    CHECK(!OV(0, 0, QD_PM_RATCH) && R(0, 0).ratch == 1, "B11: the track's x1");
    wrel(0);
    /* COND: FILL, NOT FILL, A = 1..4, B = 1..4 (A:B as Form: A <= B) */
    mode(M_COND);
    whold(0);
    black(B1);
    CHECK(R(0, 0).cond == QD_COND_FILL, "COND B1: FILL");
    black(B2);
    CHECK(R(0, 0).cond == QD_COND_NFILL, "COND B2: NOT FILL");
    black(B10);                                         /* B = 4, A kept (1: ALWAYS is 1:1) */
    CHECK(R(0, 0).cond == qd_cond_ab(1, 4), "COND B10: 1:4");
    black(B5);                                          /* A = 3 */
    CHECK(R(0, 0).cond == qd_cond_ab(3, 4), "COND B5: 3:4");
    black(B8);                                          /* B = 2: A at most 2 */
    CHECK(R(0, 0).cond == qd_cond_ab(2, 2), "COND B8: B = 2, A down to 2: 2:2");
    black(B6);                                          /* A = 4: B at least 4 */
    CHECK(R(0, 0).cond == qd_cond_ab(4, 4), "COND B6: A = 4: 4:4");
    black(B7);                                          /* B = 1: 1:1 = ALWAYS */
    CHECK(R(0, 0).cond == QD_COND_ALWAYS && OV(0, 0, QD_PM_COND), "COND B7: B = 1: ALWAYS (overridden)");
    black(B11);
    CHECK(!OV(0, 0, QD_PM_COND), "COND B11: the track's");
    wrel(0);
    /* TRIG */
    mode(M_TRIG);
    whold(0);
    black(B2);
    CHECK(R(0, 0).type == QD_TRIG_LOCK, "TRIG B2: LOCK");
    black(B3);
    CHECK(R(0, 0).type == QD_TRIG_TIE, "TRIG B3: TIE");
    black(B1);
    CHECK(R(0, 0).type == QD_TRIG_NOTE && OV(0, 0, QD_PM_TRIG), "TRIG B1: NOTE");
    black(B4);
    CHECK(R(0, 0).type == QD_TRIG_NOTE, "TRIG B4: nothing");
    black(B11);
    CHECK(!OV(0, 0, QD_PM_TRIG), "TRIG B11: the track's");
    wrel(0);
    /* several held: every value to all */
    wtap(2), wtap(4);
    mode(M_VEL);
    kdown(WK(0)), kdown(WK(2)), kdown(WK(4));
    run(300);
    black(B3);
    kup(WK(0)), kup(WK(2)), kup(WK(4));
    run(10);
    CHECK(R(0, 0).vel == 38 && R(0, 2).vel == 38 && R(0, 4).vel == 38, "three held: B3 = 38 on all three");
    mode(M_LEN);                                        /* an empty step given a value: on */
    whold(9);
    black(B4);
    wrel(9);
    CHECK(R(0, 9).on && R(0, 9).len == QD_LEN_1 && ST(0, 9)->ovr == QD_OV(QD_PM_LEN), "an empty step given a "
          "value: a trig with that one override");
    printf("  values: VEL / LEN / MICRO / CHNC %d of %d key rows, RATCH, COND, TRIG, B11, several held\n", ok,
           n);
}

/* ALGORITHM: a mode key held the track's default (a takeover; P1.3y: nothing held it is the step cursor), a step held
 * the value finely; a default change moves only the steps that do not override it */
static void g_algo(void)
{
    start();
    wtap(0), wtap(1), wtap(2);
    mode(M_VEL);
    whold(1);
    black(B10);                                         /* step 2: 127, its own */
    wrel(1);
    kdown(BK(B2));
    run(20);
    turn(QD_K_ALGO, -20);
    CHECK(PAT()->trk[0].def.vel == 80 && A.ui.tk == QD_TK_DEFAULT, "VEL key held: ALGORITHM = the track's VEL "
          "(80), the default's takeover");
    kup(BK(B2));
    run(10);
    CHECK(R(0, 0).vel == 80 && R(0, 2).vel == 80 && R(0, 1).vel == 127, "the default moved steps 1 and 3, not "
          "step 2 (it overrides VEL)");
    whold(0);
    turn(QD_K_ALGO, 3);
    wrel(0);
    CHECK(R(0, 0).vel == 83 && OV(0, 0, QD_PM_VEL) && PAT()->trk[0].def.vel == 80, "a step held: ALGORITHM its "
          "value finely (83), overridden; the default stays");
    mkturn(M_LEN, QD_K_ALGO, 2);
    CHECK(PAT()->trk[0].def.len == QD_LEN_1 + 2 && R(0, 2).len == QD_LEN_1 + 2, "LEN: the default 2 steps");
    mkturn(M_COND, QD_K_ALGO, 1);
    CHECK(PAT()->trk[0].def.cond == QD_COND_FILL, "COND: the default FILL");
    mkturn(M_NOTE, QD_K_ALGO, 2);
    CHECK(PAT()->trk[0].def.note[0] == 62 && R(0, 2).notes[0] == 62, "NOTE: the default up two steps (chromatic: D4)");
    mkturn(M_TRIG, QD_K_ALGO, 1);
    CHECK(PAT()->trk[0].def.type == QD_TRIG_LOCK && R(0, 2).type == QD_TRIG_LOCK, "TRIG: the default LOCK");
    mkturn(M_SOUND, QD_K_ALGO, 1);
    CHECK(A.model.t[0].sound == 15, "SOUND: the track's own sound from the pool (15)");
    mode(M_MICRO);
    whold(2);
    bdown(SHIFT);
    turn(QD_K_ALGO, 4);
    bup(SHIFT);
    turn(QD_K_ALGO, -3);
    wrel(2);
    CHECK(R(0, 2).strum == 4 && R(0, 2).micro == -3, "MICRO held: SHIFT + ALGORITHM the strum, ALGORITHM the micro");
    mode(M_RATCH);
    whold(2);
    turn(QD_K_ALGO, 2);
    bdown(SHIFT);
    turn(QD_K_ALGO, 3);
    bup(SHIFT);
    wrel(2);
    CHECK(R(0, 2).ratch == 3 && R(0, 2).ramp == QD_RAMP_ACCENT, "RATCH held: ALGORITHM x3, SHIFT + ALGORITHM the "
          "ramp");
    turn(QD_K_ALGO, 1);
    CHECK(A.ui.slatch && A.ui.scursor && A.ui.nheld == 1 && PAT()->trk[0].def.vel == 80,
          "P1.3y: nothing held, ALGORITHM: the step cursor (the latch on), no default (%d %d %d %d)", A.ui.slatch,
          A.ui.scursor, A.ui.nheld, PAT()->trk[0].def.ratch);
    btap(QD_B_HOME);
}

static void g_select(void)
{
    start();
    turn(QD_K_SELECT, 2);
    CHECK(A.ui.page[0] == 2 && A.ui.tk == UI_TK_PAGE, "SELECT: the page edited (3), its picker");
    wtap(0);
    CHECK(R(0, 32).on, "a tap on page 3: step 33");
    turn(QD_K_SELECT, 9);
    CHECK(A.ui.page[0] == 7, "SELECT clamps at page 8");
    turn(QD_K_SELECT, -7);
    black(B11);                                         /* (P1.3d: a step held, SELECT pages only in P-LOCK) */
    whold(1);
    turn(QD_K_SELECT, 1);                               /* P-LOCK, a step held: SELECT pages; another W joins */
    kdown(WK(5));
    run(20);
    CHECK(A.ui.page[0] == 1 && A.ui.nheld == 2 && A.ui.held[1] == 21, "P-LOCK, a step held: SELECT pages, a W there "
          "joins (step 22)");
    kup(WK(5)), kup(WK(1));
    run(10);
}

/* NOTE entry: Form's "pressed" rule, the octave, a scale */
static void g_notes(void)
{
    int k, sounding = 0;
    start();
    mode(M_NOTE);
    wtap(0);
    whold(0);
    kdown(BK(B1));
    run(20);
    for (k = 0; k < A.pool.nvoice; k++)
        sounding |= qd_pool_sounding(&A.pool, k) && A.pool.v[k].q.note == 60;
    CHECK(R(0, 0).nn == 1 && R(0, 0).notes[0] == 60 && sounding, "B1: C4 replaces the notes, and sounds");
    kdown(BK(B5));
    run(20);
    kdown(BK(B8));
    run(20);
    CHECK(R(0, 0).nn == 3 && R(0, 0).notes[1] == 64 && R(0, 0).notes[2] == 67, "B5 and B8 while B1 is down: added "
          "(C E G)");
    kup(BK(B1)), kup(BK(B5)), kup(BK(B8));
    run(10);
    black(B3);
    CHECK(R(0, 0).nn == 1 && R(0, 0).notes[0] == 62, "a key alone again: it replaces (D4)");
    btap(F2);
    black(B1);
    CHECK(A.ui.entry_oct == 5 && R(0, 0).notes[0] == 72, "OCT+ (F2) with a step held: the entry octave up (C5)");
    btap(F1), btap(F1);
    black(B10);
    CHECK(R(0, 0).notes[0] == 57, "OCT- twice: octave 3, B10 = A3 (chromatic: ten semitones)");
    turn(QD_K_SELECT, 1);
    CHECK(R(0, 0).nn == 1 && R(0, 0).notes[0] == 58, "SELECT, a step held: the chord a scale step (P1.5o: "
          "chromatic: A#3)");
    wrel(0);
    btap(F2);                                           /* (nothing held: F2 is a layer, the octave stays) */
    CHECK(A.ui.entry_oct == 3, "nothing held: OCT+ is F2, not the octave");
    bdown(SHIFT);
    turn(QD_K_ALGO, QD_SCALE_DORIAN);                   /* the track's scale: DORIAN, root D */
    turn(QD_K_PRESETS, 2);
    bup(SHIFT);
    CHECK(PAT()->trk[0].scale == QD_SCALE_DORIAN && PAT()->trk[0].root == 2 && A.ui.tk == UI_TK_ROOT,
          "SHIFT + ALGORITHM: the scale; SHIFT + PRESETS: the root");
    whold(0);
    black(B1);
    CHECK(R(0, 0).notes[0] == 50, "D dorian at octave 3: B1 = D3");
    kdown(BK(B3)), kdown(BK(B5)), kdown(BK(B8));        /* degrees 2, 4, 7: F3, A3, D4 */
    run(20);
    kup(BK(B3)), kup(BK(B5)), kup(BK(B8));
    run(10);
    CHECK(R(0, 0).nn == 3 && R(0, 0).notes[0] == 53 && R(0, 0).notes[1] == 57 && R(0, 0).notes[2] == 62,
          "chord F3 A3 D4 from degrees 3, 5, 8 (%d %d %d)", R(0, 0).notes[0], R(0, 0).notes[1], R(0, 0).notes[2]);
    turn(QD_K_SELECT, 1);
    CHECK(R(0, 0).notes[0] == 55 && R(0, 0).notes[1] == 59 && R(0, 0).notes[2] == 64, "SELECT (P1.5o): each a scale "
          "step up (G B E)");
    black(B11);
    CHECK(!OV(0, 0, QD_PM_NOTE) && R(0, 0).notes[0] == 60, "B11: the track's notes again");
    wrel(0);
}

/* SOUND: the pool's banks of ten; PRESETS is the sound lock in every mode */
static void g_sound(void)
{
    start();
    wtap(0), wtap(1);
    mode(M_VEL);
    whold(0);
    turn(QD_K_PRESETS, QD_SND_SNARE);
    CHECK(R(0, 0).sound == QD_SND_SNARE && OV(0, 0, QD_PM_SOUND) && A.ui.tk == QD_TK_PRESET && A.ui.tk_lock,
          "VEL mode, a step held: PRESETS = the sound lock (SNARE, 19), the pool's picker as a lock");
    CHECK(A.ui.hmode == M_VEL, "the mode stays VEL");
    wrel(0);
    mode(M_SOUND);
    whold(0);
    CHECK(A.ui.sound_bank == 1, "SOUND: the bank holding the step's sound (11..20)");
    black(B1);
    CHECK(R(0, 0).sound == 11, "B1: sound 11");
    btap(F2);
    black(B3);
    CHECK(A.ui.sound_bank == 2 && R(0, 0).sound == 23, "OCT+: the next bank (21..30), B3 = sound 23");
    black(B5);
    CHECK(R(0, 0).sound == 23, "B5 beyond the pool's 23: nothing");
    turn(QD_K_ALGO, -5);
    CHECK(R(0, 0).sound == 18 && A.ui.sound_bank == 1, "ALGORITHM: any sound (18), the bank follows");
    turn(QD_K_PRESETS, -20);
    CHECK(R(0, 0).sound == 0 && !OV(0, 0, QD_PM_SOUND), "PRESETS down to 0: the track's own sound");
    turn(QD_K_PRESETS, 3);
    black(B11);
    CHECK(R(0, 0).sound == 0 && !OV(0, 0, QD_PM_SOUND), "B11: the lock cleared");
    wrel(0);
    turn(QD_K_PRESETS, 2);
    CHECK(A.ui.latch == QD_LATCH_BROWSE && !A.ui.tk_lock, "nothing held: PRESETS opens the library browser (P1.5t)");
    btap(QD_B_HOME);
}

static void g_plocks(void)
{
    start();
    wtap(5);
    mode(M_VEL);                                        /* P1.3d: KNOB 1..4 lock only in P-LOCK */
    whold(5);
    turn(QD_K_3, 3);
    wrel(5);
    CHECK(qd_lock_count(PAT(), 0, 5) == 0 && R(0, 5).vel == 100, "VEL, a step held: KNOB 3 has no job: nothing");
    black(B11);
    whold(5);
    turn(QD_K_3, 3);
    turn(QD_K_4, -1);
    turn(QD_K_1, -10);
    turn(QD_K_2, 2);
    wrel(5);
    CHECK(LK(0, 5, QD_QP_HARM) == 300 && LK(0, 5, QD_QP_FDBK) == 0 && LK(0, 5, QD_QP_FREQ) == 12700 - 1000 &&
              LK(0, 5, QD_QP_EDEC) == A.model.t[0].patch[QD_QP_EDEC] + 2 && A.model.t[0].patch[QD_QP_HARM] == 0,
          "P-LOCK, a step held: KNOB 1..4 = FREQ, DECAY, HARM, FDBK p-locks; the track untouched");
    CHECK(A.ui.tk == UI_TK_DECAY && A.ui.tk_lock && A.ui.tk_step == 5, "the takeover marked as a lock on step 6");
    turn(QD_K_1, -10);
    turn(QD_K_3, 4);
    CHECK(A.model.t[0].patch[QD_QP_FREQ] == 12700 - 1000 && A.model.t[0].patch[QD_QP_HARM] == 400 &&
              A.ui.tk == UI_TK_HARM && !A.ui.tk_lock,
          "P-LOCK, nothing held: KNOB 1..4 the track's sound, live");
}

/* ---- P1.3c (§8b 2.1) */
static void ktap(int k, uint32_t ms)                    /* a key down ms, then up */
{
    kdown(k);
    run(ms);
    kup(k);
}

/* F1's page layer: tap edits, a double tap loops a page alone, a hold (400 ms) toggles it in the loop (never empty),
 * hold A + B loops A..B; B9 tap / hold clears the page / the track; B10 copies a page, B1..B8 paste it; undo each */
static void g_f1_pages(void)
{
    start();
    wtap(0), wtap(5);
    turn(QD_K_SELECT, 1);
    wtap(2);
    turn(QD_K_SELECT, -1);
    run(QD_TK_MS + 200);
    bdown(F1);
    run(100);
    CHECK(qd_ctl_layer(&A) == QD_LAYER_F1 && qd_view_screen(&A) == QD_SCR_LAYER, "F1 held: its layer, its map");
    ktap(BK(B3), 399);
    run(10);
    CHECK(A.ui.page[0] == 2 && qd_track_loop(&PAT()->trk[0]) == 1, "B3 down 399 ms: a tap, edit page 3, the loop as "
          "it was");
    run(300);
    ktap(BK(B3), 400);
    run(10);
    CHECK(qd_track_loop(&PAT()->trk[0]) == 5 && A.ui.page[0] == 2, "B3 held 400 ms: page 3 joins the loop (1 and 3)");
    run(300);
    kdown(BK(B3));
    run(450);
    CHECK(qd_track_loop(&PAT()->trk[0]) == 1, "held again: out of the loop, at the 400 ms mark (still down)");
    kup(BK(B3));
    run(300);
    ktap(BK(B1), 450);
    run(10);
    CHECK(qd_track_loop(&PAT()->trk[0]) == 1 && A.ui.cf_on, "B1 held, the loop's only page: it stays (never empty)");
    run(300);
    ktap(BK(B2), 30);                                   /* a double tap: the second release 190 ms after the first */
    run(160);
    ktap(BK(B2), 30);
    run(10);
    CHECK(PAT()->trk[0].loop == 2 && A.ui.page[0] == 1, "B2 twice within 200 ms: page 2 looped alone");
    run(300);
    ktap(BK(B4), 30);                                   /* the second press 210 ms after the first release: two taps */
    run(210);
    ktap(BK(B4), 30);
    run(10);
    CHECK(PAT()->trk[0].loop == 2 && A.ui.page[0] == 3, "B4, the second press 210 ms after: just edit page 4");

    run(300);
    ktap(BK(B5), 30);                                   /* a tap then a hold: the tap edits, the hold toggles */
    run(100);
    ktap(BK(B5), 420);
    run(10);
    CHECK(PAT()->trk[0].loop == 0x12 && A.ui.page[0] == 4, "B5 tapped then held: edited, then into the loop (no "
          "double tap)");
    run(300);
    ktap(BK(B6), 110);                                  /* (2026-10-25, the unit's: presses of 110 ms, 150 ms between:
                                                         * the releases 260 ms apart) */
    run(150);
    ktap(BK(B6), 110);
    run(10);
    CHECK(PAT()->trk[0].loop == 0x20 && A.ui.page[0] == 5,
          "B6 double-tapped at the unit's pace (110 ms presses, 150 ms apart): page 6 looped alone, edited");
    run(300);
    kdown(BK(B1));
    run(500);                                           /* (B1 held alone 400 ms first: it toggled; the range wins) */
    kdown(BK(B4));
    run(30);
    kup(BK(B4));
    run(30);
    kup(BK(B1));
    run(10);
    CHECK(PAT()->trk[0].loop == 0x0F && A.ui.page[0] == 0, "hold B1 + press B4: the loop 1..4, edit page 1");
    bup(F1);
    run(10);
    /* B9: a tap clears the page, a hold the track; each undone */
    bdown(F1);
    run(70);
    ktap(BK(B9), 590);
    bup(F1);
    run(10);
    CHECK(!R(0, 0).on && !R(0, 5).on && R(0, 18).on, "F1 + B9 tapped (590 ms): page 1 cleared, page 2 kept");
    bdown(F1);
    black(B11);
    bup(F1);
    CHECK(R(0, 0).on && R(0, 5).on, "F1 + B11: the page back");
    PAT()->trk[0].rbase = 3;                            /* (2026-10-25: the clear resets the pages, the rate stays) */
    PAT()->trk[0].pend[1] = 9;
    bdown(F1);
    run(70);
    kdown(BK(B9));
    run(610);
    CHECK(!R(0, 0).on && !R(0, 18).on, "F1 + B9 held 600 ms: the track cleared (at the mark, still down)");
    CHECK(qd_track_loop(&PAT()->trk[0]) == 1 && pages_full(0, 0) && PAT()->trk[0].rbase == 3,
          "the clear resets the track: page 1 only, every page 16 (loop %02x), the rate kept",
          qd_track_loop(&PAT()->trk[0]));
    kup(BK(B9));
    bup(F1);
    run(10);
    CHECK(!R(0, 0).on && !R(0, 18).on, "its release: nothing more");
    bdown(F1);
    black(B11);
    bup(F1);
    CHECK(R(0, 0).on && R(0, 18).on && R(0, 5).on && PAT()->trk[0].loop == 0x0F && PAT()->trk[0].pend[1] == 9,
          "F1 + B11: the track back, its pages too");
    PAT()->trk[0].rbase = 4;
    /* B10: copy page 1, paste onto pages 3 and 6 (B3 twice), undo as one */
    bdown(F1);
    run(70);
    black(B10);
    CHECK(A.ui.pg_copied == 0, "F1 + B10: page 1 copied");
    {
        const qd_leds_t l = leds();
        int on = 0, k;
        CHECK(l.key[BK(B10)] == QD_LED_LIT, "B10 lit once a page is copied");
        for (k = 0; k < 400; k++) {
            run(1);
            on += leds().key[BK(B1)] == QD_LED_LIT;
        }
        CHECK(on > 100 && on < 300, "the copied page's key blinks (%d of 400 ms lit)", on);
    }
    black(B3);
    black(B3);
    black(B6);
    CHECK(R(0, 32).on && R(0, 37).on && R(0, 80).on && R(0, 85).on && A.ui.page[0] == 5 && (PAT()->trk[0].loop & 0x24),
          "B3 (twice) and B6: page 1 pasted onto pages 3 and 6, they join the loop, page 6 edited");
    bup(F1);
    run(10);
    CHECK(A.ui.pg_copied < 0, "F1 let go: B1..B8 select again");
    bdown(F1);
    run(70);
    black(B2);
    CHECK(A.ui.page[0] == 1 && !R(0, 33).on, "the next F1 hold: B2 edits page 2 (no paste)");
    black(B11);
    bup(F1);
    CHECK(!R(0, 32).on && !R(0, 80).on && PAT()->trk[0].loop == 0x0F, "F1 + B11: the hold's pastes undone as one "
          "(both pages; the loop 1..4 again)");
}

/* F1 + W: the first W copies, every later W pastes; one undo level a hold; F1 let go loads F2's hand (Form's) */
static void g_f1_steps(void)
{
    start();
    wtap(0);
    mode(M_VEL);
    whold(0);
    black(B4);                                          /* step 1: vel 51 */
    wrel(0);
    qd_lock_set(PAT(), 0, 0, QD_QP_FREQ, 4000);
    wtap(9);
    bdown(F1);
    run(70);
    wtap(0);                                            /* copy step 1 */
    CHECK(A.ui.clip.st.valid && A.ui.f1_copied && !R(0, 3).on, "F1 + W1: copied, nothing pasted");
    wtap(3);
    wtap(7);
    wtap(9);
    CHECK(R(0, 3).vel == 51 && R(0, 7).vel == 51 && R(0, 9).vel == 51 && LK(0, 7, QD_QP_FREQ) == 4000,
          "W4, W8, W10: pasted each (with its lock, over a trig too)");
    bup(F1);
    run(10);
    bdown(F1);
    black(B11);
    bup(F1);
    CHECK(!R(0, 3).on && !R(0, 7).on && R(0, 9).on && R(0, 9).vel == 100, "F1 + B11: the hold's three pastes undone "
          "as one");
    bdown(F2);
    run(70);
    wtap(12);
    bup(F2);
    run(10);
    CHECK(R(0, 12).vel == 51 && R(0, 0).on, "F1's copy stays: the next F2's first W drops it (Form's pre-load), "
          "no cut");
    bdown(F1);
    run(70);
    wtap(9);
    wtap(14);
    bup(F1);
    CHECK(R(0, 14).vel == 100, "a new F1 hold: its first W copies again (step 10), the next pastes it");
}

/* F2: Form's pick-up / drop (omx_mode_form.cpp v1.15.4, seqF2Loaded_ / seqF2Holding_), checked press by press against
 * the test's own reading of Form's code */
static int form_action(int loaded, int holding, int on)  /* 1 CUT, 2 PASTE */
{
    if (!loaded)
        return 1;                                       /* the first press: grab, even an empty step */
    if (!on)
        return 2;                                       /* loaded, an empty step: always paste */
    return holding ? 2 : 1;                             /* a filled step: holding drops, empty-handed grabs */
}
static void g_f2(void)
{
    static const struct {
        int w;                                          /* the W pressed (0-based) */
        const char *what;
    } SEQ_[] = {
        {1, "cut an empty step (the eraser)"}, {0, "holding, a filled step: paste over it"},
        {2, "empty-handed, a filled step: cut it"}, {4, "loaded, an empty step: paste"},
        {5, "empty-handed, an empty step: paste again"}, {3, "empty-handed, filled: cut"},
        {3, "the same step, now empty: paste it back"}, {3, "filled, empty-handed: cut"},
        {3, "empty, loaded: paste"},
    };
    int loaded = 0, holding = 0, i, okall = 1;
    uint8_t vel_hand = 0;
    start();
    wtap(0), wtap(2), wtap(3);
    mode(M_VEL);
    whold(0), black(B4), wrel(0);                       /* W1 51, W3 89, W4 127 */
    whold(2), black(B7), wrel(2);
    whold(3), black(B10), wrel(3);
    bdown(F2);
    run(70);
    CHECK(qd_view_screen(&A) == QD_SCR_LAYER && qd_ctl_layer_shown(&A) == QD_LAYER_F2 && !A.ui.f2_loaded,
          "F2 held: its map, the hand empty");
    for (i = 0; i < (int)(sizeof SEQ_ / sizeof SEQ_[0]); i++) {
        const int w = SEQ_[i].w, on = R(0, w).on, vel = R(0, w).vel;
        const int want = form_action(loaded, holding, on);
        int ok;
        wtap(w);
        ok = A.ui.f2_last_act == want;
        if (want == 1) {
            ok &= !R(0, w).on;
            vel_hand = on ? (uint8_t)vel : 0;
            loaded = holding = 1;
        } else {
            ok &= vel_hand ? R(0, w).on && R(0, w).vel == vel_hand : !R(0, w).on;
            holding = 0;
        }
        ok &= A.ui.f2_loaded == loaded && A.ui.f2_holding == holding;
        CHECK(ok, "F2 press %d, W%d: %s (%s)", i + 1, w + 1, SEQ_[i].what, want == 1 ? "CUT" : "PASTE");
        okall &= ok;
    }
    CHECK(!R(0, 0).on && !R(0, 1).on && R(0, 4).vel == 89 && R(0, 5).vel == 89 && !R(0, 2).on,
          "after the hold: W1 erased by the empty step, W3's 89 moved to W5 and copied to W6");
    bup(F2);
    run(10);
    CHECK(!A.ui.f2_loaded && !A.ui.f2_holding, "F2 let go: the hand empties");
    bdown(F2);
    run(70);
    wtap(4);
    CHECK(A.ui.f2_last_act == 1 && !R(0, 4).on, "the next F2 hold: the first W cuts again (a full step)");
    wtap(10);
    bup(F2);
    CHECK(R(0, 10).vel == 89, ".. and drops it on W11");
    bdown(F1);
    black(B11);
    bup(F1);
    CHECK(R(0, 4).vel == 89 && !R(0, 10).on, "F1 + B11: that hold's cut and paste undone as one");
    bdown(F1);
    black(B11);
    bup(F1);
    CHECK(!R(0, 4).on && R(0, 10).vel == 89, "again: redone");
    bdown(F2);
    run(70);
    black(B9), black(B10), black(B1);
    bup(F2);
    CHECK(!A.ui.clip.pg.valid && A.sel == 0, "F2 + B1 / B9 / B10: nothing (no tracks, no page copy on F2 now)");
    (void)okall;
}

/* SEL held: B1..B8 select; W1..W8 tap mute, hold (400 ms) solo; B11 all off; the LEDs */
static void g_sel(void)
{
    qd_leds_t l;
    int k, on = 0;
    start();
    bdown(QD_B_PATTERN);
    run(30);
    CHECK(qd_view_screen(&A) != QD_SCR_SEL, "P1.3y: SEL down 30 ms: no map yet (a tap shows nothing)");
    run(250);
    CHECK(qd_view_screen(&A) == QD_SCR_SEL, "SEL held: the track map");
    black(B3);
    CHECK(A.sel == 2, "SEL + B3: track 3");
    ktap(WK(1), 30);
    run(10);
    CHECK(A.seq.mute == 2 && !A.seq.solo, "W2 tapped: track 2 muted");
    ktap(WK(1), 399);
    run(10);
    CHECK(A.seq.mute == 0 && !A.seq.solo, "W2 down 399 ms: a tap, unmuted");
    kdown(WK(4));
    run(401);
    CHECK(A.seq.solo == 0x10 && !A.seq.mute, "W5 held 400 ms: track 5 soloed (at the mark)");
    kup(WK(4));
    run(10);
    CHECK(A.seq.solo == 0x10 && !A.seq.mute, ".. its release: no mute");
    ktap(WK(9), 30);
    ktap(WK(15), 500);
    CHECK(A.seq.solo == 0x10 && !A.seq.mute, "W10, W16: nothing");
    ktap(WK(6), 30);
    ktap(WK(7), 500);
    CHECK(A.seq.solo == 0x10 && !A.seq.mute, "W7, W8 (six tracks): nothing");
    ktap(WK(5), 30);
    run(10);
    l = leds();
    CHECK(l.key[BK(B3)] == QD_LED_LIT && l.key[BK(B1)] == QD_LED_DIM && l.key[WK(5)] == QD_LED_OFF &&
              l.key[WK(0)] == QD_LED_OFF && l.key[BK(B11)] == QD_LED_LIT && l.key[BK(B7)] == QD_LED_OFF,
          "LEDs: the selected track lit, the others dim (B7: no track, dark); W6 muted off, W1 off (track 5 soloed); "
          "B11 lit");
    for (k = 0; k < 500; k++) {
        run(1);
        on += leds().key[WK(4)] == QD_LED_LIT;
    }
    CHECK(on > 150 && on < 350, "the soloed track's W blinks (%d of 500 ms)", on);
    black(B11);
    l = leds();
    CHECK(!A.seq.mute && !A.seq.solo && l.key[BK(B11)] == QD_LED_OFF && l.key[WK(5)] == QD_LED_LIT &&
              l.key[WK(0)] == QD_LED_LIT && l.key[WK(6)] == QD_LED_OFF && l.key[WK(7)] == QD_LED_OFF,
          "B11: every mute and solo off; all heard, lit; W7, W8 dark (six tracks); B11 off");
    bup(QD_B_PATTERN);
    run(10);
    CHECK(qd_view_screen(&A) == QD_SCR_LANE, "SEL let go: the step view");
}

/* SEQ held: W tap queues by the launch mode, held (400 ms) at once; ALGORITHM the launch mode */
static void g_seq(void)
{
    int64_t x;
    start();
    empty_pattern(1);                                   /* A02 */
    btap(QD_B_PLAY);
    run(700);
    bdown(QD_B_MIX);
    run(30);
    CHECK(qd_view_screen(&A) != QD_SCR_SEQ, "P1.5a: SEQ down 30 ms: no map yet (a tap is the project page)");
    run(250);
    CHECK(qd_view_screen(&A) == QD_SCR_SEQ, "SEQ held: the pattern map");
    ktap(WK(0), 30);
    run(10);
    CHECK(qd_seq_queued(&A.seq) == 0 && A.seq.cur == 1 && A.proj.launch == QD_LAUNCH_END,
          "W1 tapped: A01 queued (END, the default)");
    {
        const qd_leds_t l = leds();
        CHECK(l.key[WK(1)] == QD_LED_LIT && (l.key[WK(2)] == QD_LED_DIM), "LEDs: the playing one lit, content dim");
    }
    turn(QD_K_ALGO, 1);
    CHECK(A.proj.launch == QD_LAUNCH_BAR, "ALGORITHM: BAR");
    ktap(WK(2), 30);
    x = A.seq.sw_x;
    CHECK(qd_seq_queued(&A.seq) == 2 && (x - A.seq.origin) % (16 * QD_TPS) == 0 && x > qd_seq_tick(&A.seq),
          "W3 tapped, BAR: A03 queued to the next bar");
    turn(QD_K_ALGO, 1);
    ktap(WK(3), 30);
    x = A.seq.sw_x;
    CHECK(A.proj.launch == QD_LAUNCH_BEAT && qd_seq_queued(&A.seq) == 3 && (x - A.seq.origin) % (4 * QD_TPS) == 0 &&
              x - qd_seq_tick(&A.seq) <= 4 * QD_TPS,
          "BEAT: A04 queued to the next beat");
    turn(QD_K_ALGO, 5);
    CHECK(A.proj.launch == QD_LAUNCH_NOW, "ALGORITHM up: NOW (the last)");
    turn(QD_K_ALGO, -10);
    CHECK(A.proj.launch == QD_LAUNCH_END, ".. down: END");
    run(600);
    CHECK(A.seq.cur == 3 && qd_seq_queued(&A.seq) < 0, "A04 took over at its beat");
    kdown(WK(2));
    run(399);
    CHECK(qd_seq_queued(&A.seq) == 2, "W3 down 399 ms: queued (END) (queued %d, cur %d)", qd_seq_queued(&A.seq),
          A.seq.cur);
    run(2);
    CHECK(A.seq.sw_x - qd_seq_tick(&A.seq) <= QD_TPS, "held 400 ms: at once (the next 16th)");
    kup(WK(2));
    run(200);
    CHECK(A.seq.cur == 2 && qd_seq_queued(&A.seq) < 0, "A03 plays");
    bup(QD_B_MIX);
    btap(QD_B_PLAY);
    bdown(QD_B_MIX);
    ktap(WK(5), 30);
    bup(QD_B_MIX);
    CHECK(A.seq.cur == 5, "stopped: at once");
}

/* F3: B1..B7 the base (the type kept), B8..B10 the type (the base kept); KNOB 1..4 base, type, start, end; every base
 * x type resolves to its step's length; the LEDs */
static void g_f3(void)
{
    static const int SIXTEENTHS_X4[7] = {64, 32, 16, 8, 4, 2, 1};   /* (quarters of a 16th) */
    qd_tset_t *ts;
    qd_leds_t l;
    int b, t, ok = 1, bpm;
    start();
    ts = &PAT()->trk[0];
    f3_down();
    CHECK(qd_ctl_layer(&A) == QD_LAYER_F3, "both: F3");
    black(B9);                                          /* dotted */
    black(B4);                                          /* 1/8: dotted kept */
    CHECK(ts->rbase == 3 && ts->rtype == QD_RTYPE_DOTTED && qd_tset_rate(ts) == QD_RATE_CODE(3, 1),
          "F3 + B9 then B4: dotted 1/8 (the type kept)");
    l = leds();
    CHECK(l.key[BK(B4)] == QD_LED_LIT && l.key[BK(B9)] == QD_LED_LIT && l.key[BK(B1)] == QD_LED_FAINT &&
              l.key[BK(B8)] == QD_LED_FAINT && l.key[BK(B11)] == QD_LED_OFF,
          "LEDs: the base key and the type key lit");
    for (b = 0; b < 7; b++)
        for (t = 0; t < 3; t++) {
            int64_t want = (int64_t)QD_TPS * SIXTEENTHS_X4[b] / 4;
            black(b);
            black(7 + t);
            want = t == 1 ? want * 3 / 2 : t == 2 ? want * 2 / 3 : want;
            ok &= ts->rbase == b && ts->rtype == t && qd_rate_ticks(qd_tset_rate(ts)) == want;
        }
    CHECK(ok, "every base (B1..B7) x type (B8..B10): the step's length right (21 rates)");
    black(B5), black(B8);                               /* 1/16 */
    turn(QD_K_1, 1);
    CHECK(qd_tset_rate(ts) == QD_RATE_2, "KNOB 1 up: 1/32");
    turn(QD_K_2, 1);
    CHECK(qd_tset_rate(ts) == QD_RATE_4_3, "KNOB 2 up: dotted 1/32 (P1.3b's 4/3x)");
    turn(QD_K_1, -1);
    CHECK(qd_tset_rate(ts) == QD_RATE_2_3, "KNOB 1 down: dotted 1/16 (the type kept)");
    turn(QD_K_2, 1);
    CHECK(qd_tset_rate(ts) == QD_RATE_3_2, "KNOB 2: triplet 1/16");
    turn(QD_K_1, 20), turn(QD_K_2, 9);
    CHECK(ts->rbase == 6 && ts->rtype == 2, "the clamps: 1/64, triplet");
    turn(QD_K_1, -20), turn(QD_K_2, -9);
    CHECK(ts->rbase == 0 && ts->rtype == 0, ".. 1/1, normal");
    turn(QD_K_3, 3);
    turn(QD_K_4, -4);
    CHECK(ts->pstart[0] == 3 && ts->pend[0] == 11, "KNOB 3: the start 4, KNOB 4: the end 12");
    turn(QD_K_3, 20);
    CHECK(ts->pstart[0] == 11 && ts->pend[0] == 11, "the start stops at the end");
    turn(QD_K_4, -20);
    CHECK(ts->pend[0] == 11, "the end stops at the start");
    turn(QD_K_4, 20);
    turn(QD_K_3, -20);
    CHECK(ts->pstart[0] == 0 && ts->pend[0] == 15 && A.ui.tk == QD_TK_NONE, "back to 1..16; no takeover (the map)");
    black(B6), black(B8);
    wtap(11);
    CHECK(ts->pend[0] == 11 && qd_track_len(ts) == 12, "F3 + W12: page 1 ends at 12");
    kdown(WK(2));
    run(30);
    kdown(WK(9));
    run(30);
    kup(WK(9)), kup(WK(2));
    run(10);
    CHECK(ts->pstart[0] == 2 && ts->pend[0] == 9, "F3 + hold W3, press W10: 3..10");
    bpm = A.bpm;
    turn(QD_K_SELECT, 5);
    CHECK(A.bpm == bpm + 5 && A.ui.tk == UI_TK_TEMPO && qd_view_screen(&A) == QD_SCR_LAYER && A.ui.page[0] == 0,
          "P1.3x: F3 + SELECT: the tempo, its metronome over the map (the page untouched)");
    turn(QD_K_ALGO, 6);
    CHECK(PAT()->swing == 56 && A.ui.tk == UI_TK_SWING && A.bpm == bpm + 5, "P1.3x: F3 + ALGORITHM: the swing, its "
          "metronome");
    run(QD_TK_MS + QD_TK_FADE_MS + 5);
    CHECK(A.ui.tk == QD_TK_NONE && qd_ctl_layer_shown(&A) == QD_LAYER_F3, "a second later: the map again");
    {
        const int snd = A.model.t[0].sound;
        turn(QD_K_PRESETS, 2);
        CHECK(A.model.t[0].sound == snd && A.ui.tk == QD_TK_NONE && qd_ctl_layer_shown(&A) == QD_LAYER_F3,
              "P1.3j: F3 + PRESETS: nothing (the track's sound untouched, no picker)");
    }
    f3_up();
    bdown(F1);                                          /* the F3 chord: F1 first, F2 after: F3, latched (P1.3d) */
    run(30);
    bdown(F2);
    run(10);
    bup(F1);
    run(10);
    CHECK(qd_ctl_layer(&A) == QD_LAYER_F3, "F1 let go first: still F3");
    bup(F2);
    run(10);
    CHECK(qd_ctl_layer(&A) == QD_LAYER_F3 && A.ui.latch == QD_LATCH_F3, "both let go: F3 stays (latched)");
    btap(F1);
    CHECK(qd_ctl_layer(&A) == QD_LAYER_NONE && !A.ui.latch, "OCT-: F3 closed");
}

/* NOTE, a step held: SELECT = note 1 (a scale degree; chromatic a semitone), KNOB 1..4 = notes 2..5 (offsets, 0 none);
 * each sounds; no page, no p-lock */
static int sounding(int note)
{
    int k, s = 0;
    for (k = 0; k < A.pool.nvoice; k++)
        s |= qd_pool_sounding(&A.pool, k) && A.pool.v[k].q.note == note && A.pool.v[k].q.gate;
    return s;
}
static void g_note_knobs(void)
{
    start();
    mode(M_NOTE);
    wtap(0);
    whold(0);
    turn(QD_K_SELECT, 2);
    CHECK(R(0, 0).notes[0] == 62 && R(0, 0).nn == 1 && A.ui.page[0] == 0 && sounding(62),
          "chromatic: SELECT +2 = note 1 D4 (sounding), no page change");
    turn(QD_K_1, 4);
    CHECK(ST(0, 0)->note[1] == 66 && R(0, 0).nn == 2 && sounding(66),
          "KNOB 1 +4 from none: note 2 from note 1 up (F#4), sounding");
    turn(QD_K_2, 7);
    turn(QD_K_3, 7);
    CHECK(ST(0, 0)->note[2] == 70 && ST(0, 0)->note[3] == 71, "KNOB 2 +7: A#4 (F#4, note 2's, skipped); KNOB 3 +7: "
          "B4 (F#4 and A#4 skipped)");
    turn(QD_K_4, 12);
    CHECK(R(0, 0).nn == 5 && ST(0, 0)->note[4] == 77, "KNOB 4 +12: F5, five notes");
    turn(QD_K_SELECT, -2);
    CHECK(ST(0, 0)->note[0] == 60 && ST(0, 0)->note[1] == 64 && ST(0, 0)->note[2] == 68 && ST(0, 0)->note[3] == 69 &&
              ST(0, 0)->note[4] == 75,
          "P1.5o: SELECT -2 moves the whole chord, its shape kept (C4 E4 G#4 A4 D#5)");
    turn(QD_K_1, -2);
    CHECK(ST(0, 0)->note[1] == 62 && ST(0, 0)->note[0] == 60 && ST(0, 0)->note[2] == 68, "KNOB 1 -2: note 2 alone "
          "(D4)");
    turn(QD_K_3, 1);
    CHECK(ST(0, 0)->note[3] == 70 && ST(0, 0)->note[4] == 75, "KNOB 3 +1: note 4 alone (A#4)");
    turn(QD_K_1, -5);
    CHECK(ST(0, 0)->note[1] == 0 && R(0, 0).nn == 4, "KNOB 1 down into note 1: removed (four notes)");
    turn(QD_K_1, 1);
    CHECK(ST(0, 0)->note[1] == 61, "KNOB 1 up again: back, from note 1 up (C#4)");
    turn(QD_K_ALGO, 2);
    CHECK(ST(0, 0)->note[0] == 60 && ST(0, 0)->note[1] == 61, "P1.5o: ALGORITHM moves no note now");
    turn(QD_K_SELECT, 2);
    CHECK(ST(0, 0)->note[0] == 62 && ST(0, 0)->note[1] == 63 && ST(0, 0)->note[2] == 70 && ST(0, 0)->note[3] == 72 &&
              ST(0, 0)->note[4] == 77,
          "SELECT +2: every note together, each in its slot");
    turn(QD_K_4, 60);
    CHECK(ST(0, 0)->note[4] == 127, "KNOB 4 stops at G9 (127)");
    CHECK(qd_lock_count(PAT(), 0, 0) == 0 && A.ui.tk != UI_TK_FREQ, "no p-locks in NOTE");
    run(QD_PREVIEW_MS + 10);
    CHECK(A.ui.pv_note < 0, "the knob's note: a moment, then off");
    wrel(0);
    bdown(SHIFT);                                       /* C major */
    turn(QD_K_ALGO, QD_SCALE_MAJOR);
    bup(SHIFT);
    wtap(4);
    whold(4);
    turn(QD_K_SELECT, 1);
    CHECK(R(0, 4).notes[0] == 62, "C major: SELECT +1 = a degree up (C4 -> D4)");
    turn(QD_K_SELECT, 2);
    CHECK(R(0, 4).notes[0] == 65, "+2 more: F4 (E, F)");
    turn(QD_K_1, 2);
    CHECK(ST(0, 4)->note[1] == 69, "KNOB 1 +2 from none in the scale above F: G, A");
    turn(QD_K_2, 3);
    CHECK(ST(0, 4)->note[2] == 72, "KNOB 2 +3: G, B, C (A, note 2's, skipped)");
    turn(QD_K_SELECT, 1);
    CHECK(ST(0, 4)->note[0] == 67 && ST(0, 4)->note[1] == 71 && ST(0, 4)->note[2] == 74, "SELECT +1 in C major (P1.5o): "
          "all a degree up (G B D)");
    turn(QD_K_2, -1);
    CHECK(ST(0, 4)->note[2] == 72 && ST(0, 4)->note[0] == 67 && ST(0, 4)->note[1] == 71, "KNOB 2 -1: note 3 alone a "
          "degree down (C5; B4, note 2's, skipped)");
    wrel(4);
}

/* five notes by the keys: Form's "pressed" (B1 B3 B5 B7 B9 together); a sixth: ignored */
static void g_five_keys(void)
{
    start();
    mode(M_NOTE);
    wtap(0);
    whold(0);
    kdown(BK(B1)), run(10), kdown(BK(B3)), run(10), kdown(BK(B5)), run(10), kdown(BK(B7)), run(10), kdown(BK(B9));
    run(10);
    CHECK(R(0, 0).nn == 5 && R(0, 0).notes[0] == 60 && R(0, 0).notes[4] == 68, "B1 B3 B5 B7 B9 down together: five "
          "notes (C D E F# G#)");
    kdown(BK(B10));
    run(10);
    CHECK(R(0, 0).nn == 6 && R(0, 0).notes[5] == 69, "P1.3e: a sixth key: six notes (A4)");
    kdown(BK(B2));
    run(10);
    CHECK(R(0, 0).nn == 6 && R(0, 0).notes[1] == 62, "a seventh key: ignored (six at most)");
    kup(BK(B1)), kup(BK(B2)), kup(BK(B3)), kup(BK(B5)), kup(BK(B7)), kup(BK(B9)), kup(BK(B10));
    run(10);
    {
        char root[4], q[8];
        const uint8_t n9[5] = {60, 64, 67, 70, 74};
        CHECK(qd_chord_name(n9, 5, root, q) && !strcmp(root, "C") && !strcmp(q, "9"), "five notes named: C9 (%s%s)",
              root, q);
    }
    wrel(0);
}

/* P-LOCK (B11 with nothing held): holding keeps the screen, the screen's knobs lock, B11 clears (undoable) */
static void g_plock(void)
{
    start();
    wtap(5);
    black(B11);
    CHECK(A.ui.hmode == UI_F_PLOCK && leds().key[BK(B11)] == QD_LED_LIT, "B11, nothing held: the P-LOCK mode (B11 lit)");
    whold(5);
    CHECK(A.ui.nheld == 1 && qd_view_screen(&A) == QD_SCR_LANE, "a step held in P-LOCK: the screen stays the step "
          "view");
    turn(QD_K_1, -10);
    turn(QD_K_3, 3);
    CHECK(LK(0, 5, QD_QP_FREQ) == 12700 - 1000 && LK(0, 5, QD_QP_HARM) == 300 && A.model.t[0].patch[QD_QP_HARM] == 0,
          "KNOB 1, KNOB 3: FREQ and HARM p-locked on the step, the track untouched");
    CHECK(A.ui.tk == UI_TK_HARM && A.ui.tk_lock && A.ui.tk_step == 5, "the lock's takeover");
    turn(QD_K_PRESETS, 2);
    CHECK(R(0, 5).sound == 2 && A.ui.tk == QD_TK_PRESET, "PRESETS: the sound lock (the table's fifth knob)");
    turn(QD_K_ALGO, 3);
    black(B3);
    CHECK(R(0, 5).vel == 100 && R(0, 5).nn == 1, "ALGORITHM and B1..B10: nothing");
    CHECK(qd_ctl_lock_screen(&A)->n == 5, "the step view's lockable knobs: one table (KNOB 1..4, PRESETS)");
    {
        const qd_leds_t l = leds();
        CHECK(l.key[BK(B11)] == QD_LED_LIT && l.key[BK(B1)] == QD_LED_OFF, "LEDs: B11 lit (locks to clear), "
              "B1..B10 off");
    }
    black(B11);
    CHECK(qd_lock_count(PAT(), 0, 5) == 0 && R(0, 5).sound == 2 && R(0, 5).on, "B11: the p-locks cleared (the sound "
          "lock stays)");
    wrel(5);
    bdown(F1);
    black(B11);
    bup(F1);
    CHECK(LK(0, 5, QD_QP_FREQ) == 12700 - 1000 && qd_lock_count(PAT(), 0, 5) == 2, "F1 + B11: the locks back");
    mode(M_VEL);
    whold(5);
    turn(QD_K_2, 2);
    CHECK(LK(0, 5, QD_QP_EDEC) == -99999 && qd_lock_count(PAT(), 0, 5) == 2 && R(0, 5).vel == 100 && qd_view_screen(&A) == QD_SCR_HELD,
          "P1.3d: in VEL KNOB 2 has no job (no lock, nothing changed), on VEL's own screen");
    wrel(5);
}

/* the OCT LEDs: none blinks at the centre, then slow, faster, fastest; NOTE, the keyboard, SOUND's banks; the octave
 * takeover */
static uint32_t half_period(int button)                 /* measured over 2 s: the mean between flips; 0 steady */
{
    int k, flips = 0, last = -1, first = -1, lastf = -1;
    for (k = 0; k < 2000; k++) {
        const int l = leds().button[button] == QD_LED_LIT;
        if (last >= 0 && l != last) {
            flips++;
            if (first < 0)
                first = k;
            lastf = k;
        }
        last = l;
        run(1);
    }
    return flips > 1 ? (uint32_t)(lastf - first) / (uint32_t)(flips - 1) : 0u;
}
static int near(uint32_t got, uint32_t want) { return got + 30 >= want && got <= want + 30; }
static void g_octave(void)
{
    uint32_t dn, up;
    start();
    mode(M_NOTE);
    wtap(0);
    whold(0);
    dn = half_period(F1), up = half_period(F2);
    CHECK(!dn && !up, "NOTE, octave 4: no blink (%u, %u)", dn, up);
    btap(F2);
    CHECK(A.ui.tk == UI_TK_OCTAVE && A.ui.oct_kind == UI_OCT_NOTE && A.ui.oct_from == 4 && A.ui.oct_to == 5,
          "OCT+: the octave takeover (4 -> 5)");
    up = half_period(F2);
    CHECK(near(up, QD_OCT_BLINK1_MS) && !half_period(F1), "octave 5: OCT+ slow (%u ms)", up);
    btap(F2);
    up = half_period(F2);
    CHECK(near(up, QD_OCT_BLINK2_MS), "octave 6: OCT+ faster (%u ms)", up);
    btap(F2);
    up = half_period(F2);
    CHECK(near(up, QD_OCT_BLINK3_MS), "octave 7: OCT+ fastest (%u ms)", up);
    btap(F1), btap(F1), btap(F1), btap(F1);
    dn = half_period(F1);
    CHECK(near(dn, QD_OCT_BLINK1_MS) && !half_period(F2), "octave 3: OCT- slow (%u ms)", dn);
    btap(F1);
    dn = half_period(F1);
    CHECK(near(dn, QD_OCT_BLINK2_MS), "octave 2: OCT- faster (%u ms)", dn);
    run(QD_TK_MS + QD_TK_FADE_MS);
    CHECK(A.ui.tk == QD_TK_NONE, "the takeover: about a second");
    wrel(0);
    CHECK(!half_period(F1), "nothing held: OCT- / OCT+ are F1 / F2, no octave shown");
    btap(QD_B_REC);                                     /* the keyboard */
    CHECK(!half_period(F1) && !half_period(F2), "the keyboard at 0: no blink");
    btap(F2);
    CHECK(A.ui.tk == UI_TK_OCTAVE && A.ui.oct_kind == UI_OCT_KEYS && near(half_period(F2), QD_OCT_BLINK1_MS),
          "the keyboard +1: the takeover, OCT+ slow");
    btap(F2);
    CHECK(near(half_period(F2), QD_OCT_BLINK2_MS), "+2: faster");
    btap(F1), btap(F1), btap(F1);
    CHECK(near(half_period(F1), QD_OCT_BLINK1_MS), "-1: OCT- slow");
    btap(F1);
    CHECK(near(half_period(F1), QD_OCT_BLINK2_MS) && A.oct == -2, "-2: OCT- faster");
    btap(QD_B_REC);
    mode(M_SOUND);                                      /* SOUND's banks */
    whold(0);
    CHECK(!half_period(F2) && A.ui.sound_bank == 0, "SOUND, the first bank: no blink");
    btap(F2);
    CHECK(A.ui.oct_kind == UI_OCT_BANK && near(half_period(F2), QD_OCT_BLINK1_MS), "bank 2: the takeover, OCT+ slow");
    btap(F2);
    CHECK(A.ui.sound_bank == 2 && near(half_period(F2), QD_OCT_BLINK2_MS), "bank 3: faster");
    wrel(0);
}

/* REC: the keyboard; REC + PLAY; SHIFT + REC; step and live recording */
static void live_arm(void);
static void g_rec(void)
{
    int micro;
    start();
    btap(QD_B_REC);
    CHECK(A.ui.mode == QD_MODE_KEYS && leds().button[QD_B_REC] == QD_LED_DIM, "REC: the keyboard, REC dim (P1.6g)");
    btap(QD_B_REC);
    CHECK(A.ui.mode == QD_MODE_STEP && leds().button[QD_B_REC] == QD_LED_OFF, "REC again: the sequencer");
    btap(QD_B_REC);
    bdown(SHIFT);                                       /* P1.3y: SHIFT + REC: the record settings; KNOB 1 STEP */
    btap(QD_B_REC);
    bup(SHIFT);
    CHECK(A.ui.mode == QD_MODE_STEP && A.ui.latch == QD_LATCH_REC && qd_view_screen(&A) == QD_SCR_MENU && !A.ui.rec,
          "P1.3y: SHIFT + REC in the keyboard: the record settings (latched), nothing armed");
    turn(QD_K_1, 1);
    btap(QD_B_OCTDN);
    CHECK(A.proj.rec_mode == 1 && !A.ui.latch, "KNOB 1: MODE STEP; OCT-: back");
    btap(QD_B_REC);                                     /* (P1.6g: arming no longer switches the view: the keyboard) */
    bdown(QD_B_REC);
    run(QD_REC_HOLD_MS + 50);
    bup(QD_B_REC);
    run(10);
    CHECK(A.ui.mode == QD_MODE_KEYS && A.ui.rec && !A.seq.playing && qd_rec_stepping(&A),
          "REC held (MODE STEP), stopped: step recording");
    kdown(7), kdown(11), kdown(14);                     /* C4 E4 G4 */
    run(50);
    kup(7), kup(11), kup(14);
    run(10);
    CHECK(R(0, 0).nn == 3 && R(0, 0).notes[0] == 60 && R(0, 0).notes[2] == 67 && A.ui.cursor == 1,
          "a chord into step 1, the cursor on");
    btap(QD_B_OCTUP);                                   /* a rest */
    kdown(11);
    run(30);
    kup(11);
    run(10);
    CHECK(!R(0, 1).on && R(0, 2).notes[0] == 64 && A.ui.cursor == 3, "OCT+: a rest; a note; the cursor at step 4");
    btap(QD_B_OCTDN);
    CHECK(A.ui.cursor == 2, "OCT-: a step back");
    turn(QD_K_PRESETS, 2);
    CHECK(A.ui.cursor == 4, "PRESETS moves the cursor");
    btap(QD_B_REC);
    CHECK(!A.ui.rec && A.ui.mode == QD_MODE_KEYS, "P1.3y: REC tapped: the recording ends, still the keyboard");
    btap(QD_B_REC);
    CHECK(A.ui.mode == QD_MODE_STEP, "REC: back to the sequencer");
    bdown(SHIFT);
    btap(QD_B_REC);
    bup(SHIFT);
    turn(QD_K_1, -1);
    btap(QD_B_HOME);
    CHECK(A.proj.rec_mode == 0 && !A.ui.latch && A.ui.mode == QD_MODE_STEP, "MODE back to LIVE; HOME: back");
    /* P1.6g: REC held (stopped): armed; PLAY: playing and recording (the keyboard); REPLACE (PLAY ends it with the
     * transport) */
    A.proj.rec_dub = QD_TAKE_REPLACE;
    btap(QD_B_REC);
    live_arm();
    CHECK(A.ui.rec && A.ui.live_wait && !A.seq.playing, "REC held (stopped): armed, waiting");
    btap(QD_B_PLAY);
    run(10);
    CHECK(A.seq.playing && A.ui.rec && !A.ui.live_wait && A.ui.mode == QD_MODE_KEYS && leds().button[QD_B_REC] == QD_LED_LIT,
          "then PLAY: playing, recording (REC lit), the keyboard");
    while (track_pos0() < 4.0 + 3.0 / 24.0)   /* step 5 + 3/24 */
        run(1);
    kdown(16);                                          /* A4 (69), held two steps */
    run(250);
    kup(16);
    run(5);
    micro = R(0, 4).micro;
    CHECK(R(0, 4).on && R(0, 4).notes[0] == 69 && micro >= 2 && micro <= 4 && R(0, 4).len == 6,
          "live: step 5, micro %d (+3 played), length %s (2 held)", micro, qd_len_name(R(0, 4).len));
    while (track_pos0() < 14.5)
        run(1);
    turn(QD_K_3, 2);
    CHECK(LK(0, 14, QD_QP_HARM) == 200 && R(0, 14).type == QD_TRIG_LOCK, "a knob turned while recording: a lock on "
          "the step playing (step 15)");
    btap(QD_B_PLAY);
    run(10);
    CHECK(!A.ui.rec && !A.seq.playing, "P1.6g: PLAY while recording: stopped, the recording ends");
}

/* P1.3x: PLAY held opens F3 (a tap plays on its release); REC held arms live recording: stopped, the first key starts
 * playback and recording (the note on step 1, offset 0); an empty track's loop grows a page as it is reached (3 pages
 * reached: a 3-page loop when REC ends the recording, playback going on); to 8 pages, then it wraps and records on;
 * notes placed at a dotted and a triplet rate with micro against this test's own tick math */
static int64_t step_ticks(int base, int type)           /* (by hand: QD_TPS a 16th; 1/1 is 16 of them) */
{
    int64_t l = (int64_t)QD_TPS * 16;
    int i;
    for (i = 0; i < base; i++)
        l /= 2;
    return type == QD_RTYPE_DOTTED ? l + l / 2 : type == QD_RTYPE_TRIPLET ? l * 2 / 3 : l;
}
static void live_arm(void)
{
    bdown(QD_B_REC);
    run(QD_REC_HOLD_MS + 50);
    bup(QD_B_REC);
    run(10);
}
static void place_at(int base, int type, double at, const char *name)
{
    const int64_t L = step_ticks(base, type);
    int64_t T;
    double pos;
    int st, micro;
    start();
    PAT()->trk[0].rbase = (uint8_t)base;
    PAT()->trk[0].rtype = (uint8_t)type;
    btap(QD_B_REC);                                     /* (the keyboard) */
    live_arm();
    btap(QD_B_PLAY);                                    /* (P1.6g: PLAY starts it) */
    while ((double)qd_seq_tick(&A.seq) < at * (double)L)
        run(1);
    T = qd_seq_tick(&A.seq);
    kdown(16);
    run(20);
    kup(16);
    run(5);
    pos = (double)T / (double)L;                        /* the step nearest, the 24ths off it */
    st = (int)floor(pos + 0.5);
    micro = (int)lround((pos - (double)st) * 24.0);
    CHECK(R(0, st).on && R(0, st).notes[0] == 69 && abs(R(0, st).micro - micro) <= 1,
          "%s: the note on step %d, micro %d (by hand %d)", name, st + 1, R(0, st).micro, micro);
    btap(QD_B_PLAY);
    btap(QD_B_PLAY);
}
static void g_live(void)
{
    int k;
    start();
    bdown(QD_B_PLAY);                                   /* PLAY held: F3 (no transport) */
    run(QD_PLAY_HOLD_MS + 20);
    bup(QD_B_PLAY);
    run(10);
    CHECK(A.ui.latch == QD_LATCH_F3 && !A.seq.playing, "PLAY held: F3 latched, the transport untouched");
    btap(F1);
    btap(QD_B_PLAY);
    CHECK(A.seq.playing, "PLAY tapped: playing (on the release)");
    btap(QD_B_PLAY);
    CHECK(!A.seq.playing, "tapped again: stopped");

    A.proj.rec_dub = QD_TAKE_REPLACE;                   /* (the growing loop: REPLACE on an empty track) */
    btap(QD_B_REC);                                     /* (the keyboard) */
    live_arm();                                         /* REC held: live armed, waiting */
    CHECK(A.ui.mode == QD_MODE_KEYS && A.ui.rec && A.ui.live_wait && !A.seq.playing && A.ui.rec_grow &&
              qd_track_loop(&PAT()->trk[0]) == 1, "REC held: live recording armed, stopped, the empty track on page 1");
    kdown(7);
    run(40);
    kup(7);
    CHECK(A.seq.playing && R(0, 0).on && R(0, 0).notes[0] == 60 && R(0, 0).micro == 0,
          "2026-10-25: the first key starts playing and recording: C4 on step 1 (micro %d)", R(0, 0).micro);
    while (qd_ctl_playing_page(&A, 0) < 2)              /* into page 3: pages 2 and 3 joined as they came */
        run(1);
    run(200);
    kdown(11);
    run(40);
    kup(11);
    CHECK(qd_track_loop(&PAT()->trk[0]) & 4, "page 3 reached: in the loop (%02x)", qd_track_loop(&PAT()->trk[0]));
    btap(QD_B_REC);
    CHECK(!A.ui.rec && A.seq.playing && A.ui.mode == QD_MODE_KEYS && qd_track_loop(&PAT()->trk[0]) == 7,
          "REC: the recording ends, playback goes on, the loop the 3 pages reached (%02x)",
          qd_track_loop(&PAT()->trk[0]));
    btap(QD_B_PLAY);
    CHECK(!A.seq.playing, "PLAY: stopped");

    start();                                            /* to 8 pages, then it wraps and records on */
    A.proj.rec_dub = QD_TAKE_REPLACE;
    btap(QD_B_REC);
    live_arm();
    btap(QD_B_PLAY);
    kdown(7);
    run(40);
    kup(7);
    while (qd_ctl_playing_page(&A, 0) < 7)
        run(10);
    CHECK(qd_track_loop(&PAT()->trk[0]) == 0xff && A.ui.rec_grow, "page 8 reached: all eight pages in the loop");
    for (k = 0; k < 4000 && qd_ctl_playing_page(&A, 0) != 0; k++)
        run(1);
    run(300);
    kdown(14);
    run(40);
    kup(14);
    CHECK(qd_ctl_playing_page(&A, 0) == 0 && A.ui.rec && qd_track_loop(&PAT()->trk[0]) == 0xff,
          "past page 8: round to page 1, still recording");
    {
        int s, got = 0;
        for (s = 0; s < 16; s++)
            got |= R(0, s).on && R(0, s).notes[0] == 67;
        CHECK(got, "a note on page 1 again (recorded over)");
    }
    btap(QD_B_REC);
    CHECK(!A.ui.rec && A.seq.playing && qd_track_loop(&PAT()->trk[0]) == 0xff, "REC tapped: the recording ends, playing on");
    btap(QD_B_PLAY);

    place_at(3, QD_RTYPE_DOTTED, 3.0 + 5.0 / 24.0, "dotted 1/8");   /* the placement at the track's rate */
    place_at(4, QD_RTYPE_TRIPLET, 6.0 - 4.0 / 24.0, "triplet 1/16");
    place_at(4, QD_RTYPE_NORMAL, 9.0 + 7.0 / 24.0, "1/16");
}

/* 2026-10-25 (the owner): the TAKE. CAPTURE: armed, nothing changes until the first note; then the track cleared
 * (steps, locks, page 1 only, every page 16), its rate kept, the take with its micro-timing from step 1; PLAY or REC
 * ends it: the track exactly the take's length (36 steps: pages 1..3, page 3 four long), looping from there, playback
 * going on; past page 8: 128. Playing, REC held arms the next take: its first note starts the transport again */
static void run_to(double pos)                          /* playing until track 1's playhead passes pos */
{
    int k;
    for (k = 0; k < 200000 && track_pos0() < pos; k++)
        run(1);
}
static void g_capture(void)
{
    int k, ok, m;
    start();
    PAT()->trk[0].rbase = 3;                            /* 1/8: a step is 250 ms at 120 BPM */
    qd_step_on(PAT(), 0, 2);                            /* a track to be replaced: steps on pages 1..3, a lock, page 2
                                                         * cut */
    qd_step_on(PAT(), 0, 20);
    qd_step_on(PAT(), 0, 40);
    qd_lock_set(PAT(), 0, 2, QD_QP_HARM, 300);
    qd_track_set_length(PAT(), 0, 48);
    PAT()->trk[0].pend[1] = 9;
    CHECK(A.proj.rec_dub == QD_TAKE_CAPTURE, "CAPTURE is the take at first");
    btap(QD_B_REC);                                     /* (the keyboard) */
    live_arm();
    CHECK(A.ui.rec && A.ui.live_wait && A.ui.cap == 1 && !A.seq.playing && R(0, 20).on &&
              qd_track_loop(&PAT()->trk[0]) == 7, "REC held: CAPTURE armed, nothing changed yet");
    k = leds().button[QD_B_REC];
    run(QD_BLINK_MS);
    CHECK(k != leds().button[QD_B_REC], "REC blinking while armed");
    kdown(16);                                          /* A4: the take starts */
    run(40);
    kup(16);
    run(5);
    CHECK(A.seq.playing && A.ui.cap == 2 && R(0, 0).on && R(0, 0).notes[0] == 69 && R(0, 0).micro == 0 &&
              !R(0, 2).on && !R(0, 20).on && !R(0, 40).on && LK(0, 2, QD_QP_HARM) == -99999,
          "the first note: playing, on step 1 (micro %d), the old steps and locks gone", R(0, 0).micro);
    CHECK(qd_track_loop(&PAT()->trk[0]) == 1 && pages_full(0, 0) && PAT()->trk[0].rbase == 3,
          "the track reset: page 1 only, every page 16, its rate (1/8) kept");
    for (k = 0; k < 200 && A.seq.heard_step[0] != 0; k++)
        run(1);
    CHECK(k == 200 || A.seq.heard_step[0] < 0, "the first step not played again by the sequencer (the key sounds it)");
    run_to(5.0 + 4.0 / 24.0);                           /* step 6, 4/24 late */
    kdown(11);
    run(100);
    kup(11);
    run(5);
    m = R(0, 5).micro;
    CHECK(R(0, 5).on && R(0, 5).notes[0] == 64 && m >= 3 && m <= 5, "step 6: E4, micro %d (+4 played)", m);
    run_to(17.0 - 3.0 / 24.0);                          /* step 18, 3/24 early (page 2 joined) */
    kdown(14);
    run(60);
    kup(14);
    run(5);
    m = R(0, 17).micro;
    CHECK(R(0, 17).on && R(0, 17).notes[0] == 67 && m >= -4 && m <= -2 && (qd_track_loop(&PAT()->trk[0]) & 2),
          "step 18 on page 2: G4, micro %d (-3)", m);
    run_to(35.9);                                       /* 36 steps played: PLAY ends the take */
    bdown(QD_B_PLAY);
    run(20);
    bup(QD_B_PLAY);
    run(5);
    CHECK(!A.ui.rec && !A.ui.cap && A.seq.playing && qd_track_len(&PAT()->trk[0]) == 36 &&
              qd_track_loop(&PAT()->trk[0]) == 7 && PAT()->trk[0].pend[0] == 15 && PAT()->trk[0].pend[1] == 15 &&
              PAT()->trk[0].pend[2] == 3 && PAT()->trk[0].rbase == 3,
          "PLAY: the take's end, playing on; 36 steps: pages 1..3, page 3 four long (len %d, loop %02x, pend %d)",
          qd_track_len(&PAT()->trk[0]), qd_track_loop(&PAT()->trk[0]), PAT()->trk[0].pend[2]);
    CHECK(R(0, 0).on && R(0, 5).on && R(0, 17).on && R(0, 5).micro >= 3 && R(0, 17).micro <= -2, "the notes and their "
          "micro-timing kept");
    run(400);                                           /* (1.6 steps on) */
    CHECK(track_pos0() > 0.5 && track_pos0() < 2.5, "it loops from there: the playhead at %.2f", track_pos0());
    for (k = 0; k < 9000 && A.seq.heard_step[0] != 17; k++)   /* the next pass: step 1 at 36 steps' time */
        run(1);
    for (k = 0, ok = 0; k < 9000 && !ok; k++) {
        run(1);
        ok = A.seq.heard_step[0] == 0;
    }
    CHECK(ok && fabs((double)qd_seq_tick(&A.seq) / (double)qd_rate_ticks(qd_tset_rate(&PAT()->trk[0])) - 72.0) < 0.5,
          "step 1 heard again at the loop's start (%.2f steps from the top)",
          (double)qd_seq_tick(&A.seq) / (double)qd_rate_ticks(qd_tset_rate(&PAT()->trk[0])));

    bdown(QD_B_REC);                                    /* playing: REC held arms the next take */
    run(QD_REC_HOLD_MS + 50);
    bup(QD_B_REC);
    run(10);
    CHECK(A.ui.rec && A.ui.cap == 1 && A.seq.playing && R(0, 17).on && !qd_auto_on(&A), "playing, REC held: the next "
          "take armed, nothing changed, no automation yet");
    run(700);
    kdown(9);                                           /* D4: the take starts, the transport from the top */
    run(30);
    kup(9);
    run(5);
    CHECK(A.ui.cap == 2 && R(0, 0).notes[0] == 62 && !R(0, 17).on && !R(0, 5).on &&
              qd_track_loop(&PAT()->trk[0]) == 1 && qd_seq_tick(&A.seq) < qd_rate_ticks(qd_tset_rate(&PAT()->trk[0])),
          "its first note: the track cleared, D4 on step 1, the transport from the top");
    run_to(20.1);
    btap(QD_B_REC);                                     /* REC ends it */
    CHECK(!A.ui.rec && A.seq.playing && qd_track_len(&PAT()->trk[0]) == 20 && qd_track_loop(&PAT()->trk[0]) == 3 &&
              PAT()->trk[0].pend[1] == 3, "REC: the take's end, 20 steps (pages 1, 2: 16 + 4), playing on (len %d)",
          qd_track_len(&PAT()->trk[0]));
    btap(QD_B_PLAY);
    run(QD_UNDO_QUIET_MS + 50);
    bdown(SHIFT);
    btap(F1);
    bup(SHIFT);
    run(10);
    CHECK(qd_track_len(&PAT()->trk[0]) == 36 && R(0, 17).on && R(0, 5).on && !R(0, 9).on,
          "SHIFT + F1: the take undone as one level (the first take back: len %d)", qd_track_len(&PAT()->trk[0]));
    CHECK(!A.seq.playing, "PLAY: stopped");

    live_arm();                                         /* REC tapped while armed: nothing changed */
    btap(QD_B_REC);
    CHECK(!A.ui.rec && !A.ui.cap && R(0, 0).on && qd_track_len(&PAT()->trk[0]) == 36,
          "armed, REC tapped: disarmed, the track as it was");

    start();                                            /* past page 8: 128 steps */
    PAT()->trk[0].rbase = 6;                            /* 1/64: 128 steps in 4 s */
    btap(QD_B_REC);
    live_arm();
    kdown(7);
    run(20);
    kup(7);
    for (k = 0; k < 20000 && !(qd_ctl_playing_page(&A, 0) == 0 && A.ui.cap_full); k++)
        run(1);
    run(200);
    btap(QD_B_PLAY);
    CHECK(A.seq.playing && qd_track_loop(&PAT()->trk[0]) == 0xff && qd_track_len(&PAT()->trk[0]) == 128 &&
              R(0, 0).on && PAT()->trk[0].rbase == 6, "round past page 8, PLAY: 128 steps (loop %02x)",
          qd_track_loop(&PAT()->trk[0]));
    btap(QD_B_PLAY);
}

/* REPLACE / OVERDUB on a track with steps: stopped and armed, the first note starts playback and the recording (on
 * step 1); the loop stays; REPLACE: a note replaces the step's notes; OVERDUB: joins them; PLAY stops both */
static void g_take_replace(void)
{
    start();
    A.proj.rec_dub = QD_TAKE_REPLACE;
    {
        const uint8_t ch[2] = {60, 64};
        qd_step_on(PAT(), 0, 4);
        qd_step_set_notes(PAT(), 0, 4, ch, 2);
        qd_step_on(PAT(), 0, 8);
        qd_step_on(PAT(), 0, 12);
    }
    btap(QD_B_REC);
    live_arm();
    CHECK(A.ui.rec && A.ui.live_wait && !A.ui.cap && !A.seq.playing, "REPLACE, REC held: armed, waiting");
    kdown(14);                                          /* G4: starts it, on step 1 */
    run(30);
    kup(14);
    run(5);
    CHECK(A.seq.playing && A.ui.rec && !A.ui.live_wait && R(0, 0).notes[0] == 67 && R(0, 4).nn == 2 &&
              R(0, 12).on && qd_track_loop(&PAT()->trk[0]) == 1, "stopped, the first note: playing and recording, G4 "
          "on step 1, the other steps stay");
    run_to(4.0 + 2.0 / 24.0);
    kdown(16);
    run(30);
    kup(16);
    run(5);
    CHECK(R(0, 4).nn == 1 && R(0, 4).notes[0] == 69, "REPLACE: A4 replaces step 5's chord");
    A.proj.rec_dub = QD_TAKE_OVERDUB;
    run_to(8.0 + 1.0 / 24.0);
    kdown(11);
    run(30);
    kup(11);
    run(5);
    CHECK(R(0, 8).nn == 2 && R(0, 8).notes[1] == 64, "OVERDUB: E4 joins step 9's note (%d notes)", R(0, 8).nn);
    btap(QD_B_PLAY);
    CHECK(!A.seq.playing && !A.ui.rec && qd_track_len(&PAT()->trk[0]) == 16, "PLAY: stopped, the recording ended, the "
          "loop as it was");
}

/* the owner's REC / PLAY bugs: REC held (stopped) + PLAY: recording with playback, never a cancel or the step view;
 * armed + PLAY (CAPTURE): the take from the top, the track cleared */
static void g_rec_play(void)
{
    start();
    A.proj.rec_dub = QD_TAKE_REPLACE;
    qd_step_on(PAT(), 0, 6);
    btap(QD_B_REC);                                     /* the keyboard */
    bdown(QD_B_REC);
    run(150);                                           /* (before the hold arms) */
    bdown(QD_B_PLAY);
    run(10);
    CHECK(A.seq.playing && A.ui.rec && !A.ui.live_wait && A.ui.mode == QD_MODE_KEYS,
          "REC held, PLAY pressed (stopped): playing and recording at once, the keyboard stays");
    bup(QD_B_PLAY);
    run(30);
    bup(QD_B_REC);
    run(10);
    CHECK(A.seq.playing && A.ui.rec && A.ui.mode == QD_MODE_KEYS && R(0, 6).on, "let go of both: still recording, "
          "still the keyboard (REC's release no tap)");
    btap(QD_B_PLAY);
    CHECK(!A.seq.playing && !A.ui.rec, "PLAY: stopped");

    btap(QD_B_REC);                                     /* the step view: REC held past the hold, then PLAY */
    CHECK(A.ui.mode == QD_MODE_STEP, "(the step view)");
    bdown(QD_B_REC);
    run(QD_REC_HOLD_MS + 50);
    CHECK(A.ui.rec && A.ui.live_wait, "REC held: armed");
    bdown(QD_B_PLAY);
    run(20);
    bup(QD_B_PLAY);
    run(20);
    bup(QD_B_REC);
    run(10);
    CHECK(A.seq.playing && A.ui.rec && !A.ui.live_wait && A.ui.mode == QD_MODE_STEP && !A.ui.latch,
          "armed, REC still held, PLAY: playing and recording, the step view stays (no F3)");
    btap(QD_B_PLAY);

    A.proj.rec_dub = QD_TAKE_CAPTURE;                   /* CAPTURE: REC + PLAY, armed + PLAY: the take from the top */
    bdown(QD_B_REC);
    run(100);
    bdown(QD_B_PLAY);
    run(20);
    bup(QD_B_PLAY);
    bup(QD_B_REC);
    run(10);
    CHECK(A.seq.playing && A.ui.cap == 2 && !R(0, 6).on && qd_track_loop(&PAT()->trk[0]) == 1,
          "CAPTURE, REC + PLAY: the take runs, the track cleared");
    run_to(7.4);
    btap(QD_B_REC);
    CHECK(A.seq.playing && !A.ui.rec && qd_track_len(&PAT()->trk[0]) == 7,
          "REC: its end, 7 steps (a take of rests: %d)", qd_track_len(&PAT()->trk[0]));
    btap(QD_B_PLAY);
    qd_step_on(PAT(), 0, 3);
    live_arm();
    btap(QD_B_PLAY);
    CHECK(A.seq.playing && A.ui.cap == 2 && !R(0, 3).on && qd_track_len(&PAT()->trk[0]) == 16,
          "CAPTURE armed, PLAY: the take from the top, the track cleared (16 again)");
    btap(QD_B_PLAY);
    CHECK(A.seq.playing && !A.ui.rec, "PLAY: its end, playing on");
    btap(QD_B_PLAY);
    CHECK(!A.seq.playing, "PLAY again: stopped");
}

/* the keyboard in a scale: the keys snap down onto it */
static void g_keyboard_scale(void)
{
    int n;
    start();
    PAT()->trk[0].scale = QD_SCALE_MIN_PENTA;
    PAT()->trk[0].root = 9;                             /* A minor pentatonic: A C D E G */
    btap(QD_B_REC);
    n = qd_rec_key_note(&A, 8);                         /* C#4 (61) -> C4 */
    CHECK(n == 60 && qd_rec_key_note(&A, 13) == 64 && qd_rec_key_note(&A, 7) == 60, "a minor-pentatonic track: "
          "C#4 -> C4, F#4 -> E4, C4 stays");
    A.proj.key_scale = QD_SCALE_MAJOR;
    A.proj.key_root = 2;
    PAT()->trk[0].key_global = 1;
    CHECK(qd_rec_key_note(&A, 7) == 59, "following the global key (D major): C4 -> B3");
}

static void g_fill_page(void)
{
    start();
    bdown(SHIFT), bdown(QD_B_KEYS);                     /* (P1.6d: FILL is SHIFT + ARP held) */
    run(10);
    CHECK(A.seq.fill && leds().button[QD_B_KEYS] == QD_LED_LIT, "SHIFT + ARP held: FILL, ARP lit (blinking)");
    bup(QD_B_KEYS), bup(SHIFT);
    run(10);
    CHECK(!A.seq.fill && qd_view_screen(&A) != QD_SCR_MFX, "ARP up: FILL off (no MIDI FX screen)");
    CHECK(A.ui.follow && leds().button[QD_B_PAGE] == QD_LED_OFF, "follow on at power-on (P1.4b: ENV is MIX, unlit)");
}

static void g_follow(void)
{
    start();
    qd_track_set_length(PAT(), 0, 32);                  /* 32 steps: pages 1 and 2 */
    CHECK(PAT()->trk[0].loop == 3 && qd_track_len(&PAT()->trk[0]) == 32, "32 steps (pages 1, 2)");
    btap(QD_B_PLAY);                                    /* 120 BPM: a 16th is 125 ms; page 2 from 2 s */
    run(1000);
    CHECK(A.ui.page[0] == 0, "playing page 1: shown");
    run(1200);
    CHECK(A.ui.page[0] == 1, "the playhead on page 2: the view follows");
    turn(QD_K_SELECT, -1);
    run(500);
    CHECK(A.ui.page[0] == 0, "a page chosen by hand stays while the playhead stays on its page");
    run(2000);
    run(2100);
    CHECK(A.ui.page[0] == 1, "page 2 again: follows");
    btap(QD_B_PATTERN);                                 /* P1.4b: the track settings' second page, KNOB 3 */
    btap(QD_B_PATTERN);
    turn(QD_K_3, -1);
    btap(QD_B_HOME);
    run(2500);
    CHECK(A.ui.page[0] == 1, "follow off: the page stays");
    btap(QD_B_PLAY);
}

/* P1.4b: SEL tapped, the track settings (two pages, B1..B8 the track); ENV, the mix (the faders, again the sends; B1..B8
 * the track); FX tapped, the effects (B1..B4 the effects, again their second page; SELECT the pages; SEND the track's);
 * the W keys nothing there; OCT- closes; FX held + ALGORITHM still the track */
static void g_tracks_screen(void)
{
    qd_tset_t *ts;
    int v0;
    start();
    ts = &PAT()->trk[0];
    btap(QD_B_PATTERN);
    CHECK(A.ui.latch == QD_LATCH_TRK && qd_view_screen(&A) == QD_SCR_MENU && !A.ui.trk_page,
          "SEL tapped: the track settings");
    turn(QD_K_1, 2);
    turn(QD_K_2, 3);
    turn(QD_K_4, 2);
    CHECK(ts->scale == 2 && ts->root == 3 && !ts->key_global && ts->chan == 2,
          "KNOB 1 SCALE +2, KNOB 2 ROOT +3, KNOB 4 MIDI CH 3 (%d %d %d)", ts->scale, ts->root, ts->chan + 1);
    turn(QD_K_3, 1);
    CHECK(ts->key_global == 1, "KNOB 3: FOLLOW KEY on");
    btap(QD_B_PATTERN);
    v0 = ts->limit;
    turn(QD_K_1, 2);
    turn(QD_K_2, 1);
    CHECK(A.ui.trk_page == 1 && ts->limit == (v0 + 2 > 16 ? 16 : v0 + 2) && ts->swing == 1,
          "SEL again: VOICES +2 (%d), SWING on", ts->limit);
    wtap(4);
    CHECK(!R(0, 4).on, "a W does nothing here");
    black(B3);
    CHECK(A.sel == 2 && A.ui.latch == QD_LATCH_TRK, "B3: track 3's settings");
    btap(QD_B_OCTDN);
    CHECK(!A.ui.latch, "OCT-: closed");
    /* the mix */
    btap(QD_B_PAGE);
    v0 = A.model.t[2].mix.level;
    turn(QD_K_1, -10);
    turn(QD_K_2, 5);
    turn(QD_K_3, 7);
    turn(QD_K_4, 3);
    CHECK(A.ui.latch == QD_LATCH_MIX && A.model.t[2].mix.level == v0 - 10 && A.model.t[2].mix.pan == 5,
          "ENV: the mix; track 3's LEVEL -10, PAN +5 (%d)", A.model.t[2].mix.pan);
    black(B2);
    btap(QD_B_PAGE);
    v0 = A.model.t[1].mix.chorus;
    turn(QD_K_1, 9);
    CHECK(A.sel == 1 && A.ui.mix_page == 1 && A.model.t[1].mix.chorus == v0 + 9,
          "B2, ENV again: the sends; KNOB 1 track 2's CHORUS +9");
    btap(QD_B_HOME);
    /* the effects */
    btap(QD_B_TRACKS);
    CHECK(A.ui.latch == QD_LATCH_FX && A.ui.fx_page == 0 && !strcmp(QD_FX_PAGES[0].name, "OVERVIEW"),
          "FX tapped: the effects, the OVERVIEW (P1.5b)");
    black(B2);
    CHECK(!strcmp(QD_FX_PAGES[A.ui.fx_page].name, "DELAY"), "B2: DELAY");
    v0 = qd_fx_get(QD_FXP_DDIV);
    turn(QD_K_1, 1);
    CHECK(qd_fx_get(QD_FXP_DSYNC) && qd_fx_get(QD_FXP_DDIV) == v0 + 1, "DELAY, KNOB 1: TIME (synced: the division) +1");
    v0 = A.model.t[1].mix.delay;
    turn(QD_K_4, 4);
    CHECK(A.model.t[1].mix.delay == v0 + 4, "KNOB 4: SEND, track 2's delay send +4");
    black(B3), black(B3);
    CHECK(!strcmp(QD_FX_PAGES[A.ui.fx_page].name, "REVERB 2"), "B3 twice: REVERB 2");
    v0 = qd_fx_get(QD_FXP_RLOW);
    turn(QD_K_3, 5);
    CHECK(qd_fx_get(QD_FXP_RLOW) == v0 + 5, "KNOB 3: LOW CUT +5");
    black(B4);
    v0 = qd_fx_get(QD_FXP_CRATE);
    turn(QD_K_1, -3);
    CHECK(qd_fx_get(QD_FXP_CRATE) == v0 - 3, "B4 CHORUS, KNOB 1: RATE -3");
    black(B5);
    turn(QD_K_4, 1);
    CHECK(qd_fx_get(QD_FXP_FON) == 1 && !strcmp(QD_FX_PAGES[A.ui.fx_page].name, "MASTER"), "B5 MASTER, KNOB 4: ON");
    turn(QD_K_4, -1);
    black(B6);
    CHECK(!strcmp(QD_FX_PAGES[A.ui.fx_page].name, "ROUTING"), "B6: ROUTING");
    btap(QD_B_TRACKS);
    CHECK(A.ui.fx_page == 0, "FX again on the last page: round to the OVERVIEW");
    bdown(QD_B_TRACKS);
    run(10);
    turn(QD_K_ALGO, 3);
    bup(QD_B_TRACKS);
    run(10);
    CHECK(A.sel == 4 && A.ui.latch == QD_LATCH_FX && A.ui.fx_page == 0, "FX held + ALGORITHM: track 5, the page stays");
    btap(QD_B_HOME);
}

static void g_midi_held(void)
{
    start();
    wtap(0);
    whold(0);
    qd_app_midi(&A, 0x90, 64, 100);
    qd_app_midi(&A, 0x90, 67, 100);
    run(5);
    CHECK(R(0, 0).notes[0] == 64 && R(0, 0).notes[1] == 67, "MIDI notes while a step is held: its chord");
    qd_app_midi(&A, 0x80, 64, 0);
    qd_app_midi(&A, 0x80, 67, 0);
    qd_app_midi(&A, 0x90, 70, 90);
    run(5);
    CHECK(R(0, 0).notes[0] == 70 && R(0, 0).nn == 1, "a new MIDI note after the chord's release: replaces it");
    qd_app_midi(&A, 0x80, 70, 0);
    wrel(0);
}

static void g_maps(void)
{
    start();
    bdown(F1);
    run(30);
    CHECK(qd_ctl_layer_shown(&A) == QD_LAYER_NONE, "F1 down 30 ms: its map not yet (it may become F3)");
    run(40);
    CHECK(qd_ctl_layer_shown(&A) == QD_LAYER_F1, "F1 down 70 ms: its map");
    bup(F1);
    bdown(F1);
    run(20);
    bdown(F2);
    run(5);
    CHECK(qd_ctl_layer_shown(&A) == QD_LAYER_F3, "both within 60 ms: F3's map at once");
    bup(F1), bup(F2);
    run(10);
    btap(F1);                                           /* (P1.3d: F3 is latched: OCT- exits) */
    bdown(SHIFT);
    run(100);
    CHECK(qd_ctl_shift(&A) && !A.ui.shift_used, "SHIFT held alone: its map");
    turn(QD_K_ALGO, 1);
    CHECK(A.ui.shift_used, "SHIFT used: the map gives way");
    bup(SHIFT);
    run(10);
    whold(0);
    bdown(F1);
    run(100);
    CHECK(qd_ctl_layer(&A) == QD_LAYER_NONE, "a step held: no layer (OCT- / OCT+ are the octave or the bank)");
    bup(F1);
    wrel(0);
}

/* ================================================================ P1.3d (§8b version 2.2) === */
#define LFO QD_B_MUTE                                   /* printed LFO: the step latch */

/* what a turn changed on track 1: step s as it plays, its locks, the track's defaults (as a step on them plays), the
 * track's sound, the page, anything else of the pattern */
enum { X_NOTE = 1 << 0, X_VEL = 1 << 1, X_LEN = 1 << 2, X_MICRO = 1 << 3, X_RATCH = 1 << 4, X_RAMP = 1 << 5,
       X_CHANCE = 1 << 6, X_COND = 1 << 7, X_TYPE = 1 << 8, X_SOUND = 1 << 9, X_STRUM = 1 << 10, X_LOCKS = 1 << 11,
       X_TRK = 1 << 12, X_PAGE = 1 << 13, X_OTHER = 1 << 14 };
static qd_pattern_t snap_p, base_p, tmp_p;
static qd_track_t snap_t, base_t;
static int snap_page;
static void snap(void)
{
    snap_p = *PAT();
    snap_t = A.model.t[0];
    snap_page = A.ui.page[0];
}
static unsigned rdiff(const qd_rstep_t *x, const qd_rstep_t *y)
{
    unsigned m = 0;
    m |= x->nn != y->nn || memcmp(x->notes, y->notes, sizeof x->notes) ? X_NOTE : 0u;
    m |= x->vel != y->vel ? X_VEL : 0u;
    m |= x->len != y->len ? X_LEN : 0u;
    m |= x->micro != y->micro ? X_MICRO : 0u;
    m |= x->ratch != y->ratch ? X_RATCH : 0u;
    m |= x->ramp != y->ramp ? X_RAMP : 0u;
    m |= x->chance != y->chance ? X_CHANCE : 0u;
    m |= x->cond != y->cond ? X_COND : 0u;
    m |= x->type != y->type || x->on != y->on ? X_TYPE : 0u;
    m |= x->sound != y->sound ? X_SOUND : 0u;
    m |= x->strum != y->strum ? X_STRUM : 0u;
    return m;
}
static void def_r(const qd_pattern_t *p, qd_rstep_t *r)   /* a step on the track's defaults */
{
    tmp_p.trk[0] = p->trk[0];
    memset(&tmp_p.step[0][0], 0, sizeof tmp_p.step[0][0]);
    tmp_p.step[0][0].flags = QD_SF_ON;
    qd_step_resolve(&tmp_p, 0, 0, r);
}
/* the step's changes; *defm: the defaults' */
static unsigned changed(int s, unsigned *defm)
{
    const qd_pattern_t *p = PAT();
    qd_rstep_t x, y;
    unsigned m;
    int t, k;
    qd_step_resolve(&snap_p, 0, s, &x);
    qd_step_resolve(p, 0, s, &y);
    m = rdiff(&x, &y);
    if (snap_p.nlock != p->nlock || memcmp(snap_p.lock, p->lock, sizeof p->lock[0] * p->nlock))
        m |= X_LOCKS;
    for (t = 0; t < QD_PAT_NTRACK; t++)
        for (k = 0; k < QD_NSTEP; k++)
            if (!(t == 0 && k == s) && memcmp(&snap_p.step[t][k], &p->step[t][k], sizeof p->step[t][k]))
                m |= X_OTHER;
    for (t = 1; t < QD_PAT_NTRACK; t++)
        if (memcmp(&snap_p.trk[t], &p->trk[t], sizeof p->trk[t]))
            m |= X_OTHER;
    {
        qd_tset_t a = snap_p.trk[0], b = p->trk[0];
        memset(&a.def, 0, sizeof a.def), memset(&b.def, 0, sizeof b.def);
        if (memcmp(&a, &b, sizeof a))
            m |= X_OTHER;
    }
    def_r(&snap_p, &x);
    def_r(p, &y);
    *defm = rdiff(&x, &y);
    if (memcmp(snap_t.patch, A.model.t[0].patch, sizeof snap_t.patch) || snap_t.sound != A.model.t[0].sound)
        m |= X_TRK;
    if (snap_page != A.ui.page[0])
        m |= X_PAGE;
    return m;
}
static const char *KNAME(int k)
{
    static const char *const N[QD_NKNOB] = {"SELECT", "ALGORITHM", "PRESETS", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4"};
    return N[k];
}

/* the knob layout with a step held: each knob changes exactly its value of the held step (and shows its band) */
static const struct {
    int mode, knob, d;
    unsigned want;
} HELD_ROWS[] = {
    {M_NOTE, QD_K_SELECT, 1, X_NOTE}, {M_NOTE, QD_K_1, 1, X_NOTE}, {M_NOTE, QD_K_2, 2, X_NOTE},
    {M_NOTE, QD_K_3, 3, X_NOTE}, {M_NOTE, QD_K_4, 4, X_NOTE},
    {M_VEL, QD_K_SELECT, 1, X_VEL}, {M_VEL, QD_K_1, 1, X_VEL}, {M_VEL, QD_K_2, 1, 0}, {M_VEL, QD_K_3, 1, 0},
    {M_VEL, QD_K_4, 1, X_VEL},
    {M_LEN, QD_K_SELECT, 1, X_LEN}, {M_LEN, QD_K_1, 1, X_LEN}, {M_LEN, QD_K_2, 1, X_TYPE}, {M_LEN, QD_K_3, 1, 0},
    {M_LEN, QD_K_4, 1, X_LEN},
    {M_MICRO, QD_K_SELECT, 1, X_MICRO}, {M_MICRO, QD_K_1, -1, X_MICRO}, {M_MICRO, QD_K_2, 1, X_STRUM},
    {M_MICRO, QD_K_3, 1, 0}, {M_MICRO, QD_K_4, 1, X_MICRO},
    {M_RATCH, QD_K_SELECT, 1, X_RATCH}, {M_RATCH, QD_K_1, 2, X_RATCH}, {M_RATCH, QD_K_2, 1, X_RAMP},
    {M_RATCH, QD_K_3, 1, 0}, {M_RATCH, QD_K_4, 1, X_RATCH},
    {M_CHNC, QD_K_SELECT, -1, X_CHANCE}, {M_CHNC, QD_K_1, -5, X_CHANCE}, {M_CHNC, QD_K_2, 1, X_COND},
    {M_CHNC, QD_K_3, 1, 0}, {M_CHNC, QD_K_4, -1, X_CHANCE},
    {M_COND, QD_K_SELECT, 1, X_COND}, {M_COND, QD_K_1, 3, X_COND}, {M_COND, QD_K_2, -1, X_CHANCE},
    {M_COND, QD_K_3, 1, 0}, {M_COND, QD_K_4, 1, X_COND},
    {M_TRIG, QD_K_SELECT, 1, X_TYPE}, {M_TRIG, QD_K_1, 2, X_TYPE}, {M_TRIG, QD_K_3, 1, 0},   /* (P1.6c: KNOB 2 the
                                                         * MIDI FX: g_p16c_trig) */
    {M_TRIG, QD_K_4, 1, X_TYPE},
    {M_SOUND, QD_K_SELECT, 1, X_SOUND}, {M_SOUND, QD_K_1, 2, X_SOUND}, {M_SOUND, QD_K_2, 1, 0},
    {M_SOUND, QD_K_3, 1, 0}, {M_SOUND, QD_K_4, 1, X_SOUND},
};
static void g_knobs_held(void)
{
    size_t i;
    int ok = 0;
    start();
    wtap(0);
    base_p = *PAT();
    base_t = A.model.t[0];
    for (i = 0; i < sizeof HELD_ROWS / sizeof HELD_ROWS[0]; i++) {
        unsigned got, defm;
        int band;
        mode(HELD_ROWS[i].mode);
        whold(0);
        snap();
        turn(HELD_ROWS[i].knob, HELD_ROWS[i].d);
        got = changed(0, &defm);
        band = A.ui.tk == QD_TK_HELDKNOB && A.ui.tk_src == HELD_ROWS[i].knob;
        if (HELD_ROWS[i].knob == QD_K_4 && HELD_ROWS[i].mode != M_NOTE)   /* P1.3e: KNOB 4 quiet: no overlay */
            band = A.ui.tk == QD_TK_NONE;
        CHECK(got == HELD_ROWS[i].want && !defm && (HELD_ROWS[i].want ? band : A.ui.tk == QD_TK_NONE),
              "%s held, %s %+d: changed %#x (want %#x), defaults %#x, band %d", UI_FIELD_NAME[HELD_ROWS[i].mode],
              KNAME(HELD_ROWS[i].knob), HELD_ROWS[i].d, got, HELD_ROWS[i].want, defm, band);
        ok += got == HELD_ROWS[i].want && !defm;
        wrel(0);
        *PAT() = base_p;
        A.model.t[0] = base_t;
    }
    /* a few values exactly */
    mode(M_LEN);
    whold(0);
    turn(QD_K_2, 1);
    CHECK(R(0, 0).type == QD_TRIG_TIE && OV(0, 0, QD_PM_TRIG), "LEN, KNOB 2 up: TIE on");
    turn(QD_K_2, -1);
    CHECK(R(0, 0).type == QD_TRIG_NOTE, "KNOB 2 down: TIE off (NOTE)");
    turn(QD_K_1, 2);
    CHECK(R(0, 0).len == QD_LEN_1 + 2 && OV(0, 0, QD_PM_LEN), "KNOB 1: the length (2 steps)");
    turn(QD_K_SELECT, -1);
    CHECK(R(0, 0).len == QD_LEN_1 + 1 && A.ui.page[0] == 0, "SELECT: the length too, no page change");
    wrel(0);
    mode(M_MICRO);
    whold(0);
    turn(QD_K_2, 5);
    CHECK(R(0, 0).strum == 5, "MICRO, KNOB 2: the strum (5/24)");
    wrel(0);
    mode(M_RATCH);
    whold(0);
    turn(QD_K_1, 3);
    turn(QD_K_2, 2);
    CHECK(R(0, 0).ratch == 4 && R(0, 0).ramp == QD_RAMP_DOWN, "RATCH, KNOB 1: x4, KNOB 2: the ramp DOWN");
    wrel(0);
    mode(M_CHNC);
    whold(0);
    turn(QD_K_1, -30);
    turn(QD_K_2, 1);
    CHECK(R(0, 0).chance == 70 && R(0, 0).cond == QD_COND_FILL, "CHNC, KNOB 1: 70 %%, KNOB 2: the condition FILL");
    wrel(0);
    mode(M_VEL);
    whold(0);
    turn(QD_K_1, -20);
    CHECK(R(0, 0).vel == 80 && A.ui.tk == QD_TK_HELDKNOB && qd_view_screen(&A) == QD_SCR_HELD,
          "VEL, KNOB 1: 80, the band over VEL's screen");
    wrel(0);
    CHECK(A.ui.tk == QD_TK_NONE, "the step let go: its band goes");
    printf("  knobs, a step held: %d of %d (mode, knob) rows change exactly their value\n", ok,
           (int)(sizeof HELD_ROWS / sizeof HELD_ROWS[0]));
}

/* the knob layout with nothing held: the same knobs on the track's defaults; SELECT the page */
static const struct {
    int mode, knob, d;
    unsigned want, trk;                                 /* the defaults' change; the track's sound */
} DEF_ROWS[] = {
    {M_NOTE, QD_K_1, 1, X_NOTE, 0}, {M_NOTE, QD_K_2, 2, X_NOTE, 0}, {M_NOTE, QD_K_3, 3, X_NOTE, 0},
    {M_NOTE, QD_K_4, 4, X_NOTE, 0},
    {M_VEL, QD_K_1, 1, X_VEL, 0}, {M_VEL, QD_K_2, 1, 0, 0}, {M_VEL, QD_K_3, 1, 0, 0}, {M_VEL, QD_K_4, 1, X_VEL, 0},
    {M_LEN, QD_K_1, 1, X_LEN, 0}, {M_LEN, QD_K_2, 1, X_TYPE, 0}, {M_LEN, QD_K_3, 1, 0, 0}, {M_LEN, QD_K_4, 1, X_LEN, 0},
    {M_MICRO, QD_K_1, 1, X_MICRO, 0}, {M_MICRO, QD_K_2, 1, X_STRUM, 0}, {M_MICRO, QD_K_3, 1, 0, 0},
    {M_MICRO, QD_K_4, 1, X_MICRO, 0},
    {M_RATCH, QD_K_1, 1, X_RATCH, 0}, {M_RATCH, QD_K_2, 1, X_RAMP, 0}, {M_RATCH, QD_K_3, 1, 0, 0},
    {M_RATCH, QD_K_4, 1, X_RATCH, 0},
    {M_CHNC, QD_K_1, -1, X_CHANCE, 0}, {M_CHNC, QD_K_2, 1, X_COND, 0}, {M_CHNC, QD_K_3, 1, 0, 0},
    {M_CHNC, QD_K_4, -1, X_CHANCE, 0},
    {M_COND, QD_K_1, 1, X_COND, 0}, {M_COND, QD_K_2, -1, X_CHANCE, 0}, {M_COND, QD_K_3, 1, 0, 0},
    {M_COND, QD_K_4, 1, X_COND, 0},
    {M_TRIG, QD_K_1, 1, X_TYPE, 0}, {M_TRIG, QD_K_3, 1, 0, 0}, {M_TRIG, QD_K_4, 1, X_TYPE, 0},   /* (P1.6c: KNOB 2) */
    {M_SOUND, QD_K_1, 1, 0, X_TRK}, {M_SOUND, QD_K_2, 1, 0, 0}, {M_SOUND, QD_K_3, 1, 0, 0},
    {M_SOUND, QD_K_4, 1, 0, X_TRK},
};
static void g_knobs_default(void)
{
    size_t i;
    int ok = 0, m;
    start();
    wtap(0);                                            /* step 1: no overrides, it follows the defaults */
    wtap(1);
    mode(M_VEL);
    whold(1);
    black(B10);                                         /* step 2 overrides VEL */
    wrel(1);
    base_p = *PAT();
    base_t = A.model.t[0];
    for (i = 0; i < sizeof DEF_ROWS / sizeof DEF_ROWS[0]; i++) {
        unsigned got, defm;
        mode(DEF_ROWS[i].mode);
        snap();
        turn(DEF_ROWS[i].knob, DEF_ROWS[i].d);
        got = changed(0, &defm);
        CHECK(defm == DEF_ROWS[i].want && got == (DEF_ROWS[i].want | DEF_ROWS[i].trk) &&
                  ((DEF_ROWS[i].want || DEF_ROWS[i].trk) && !(DEF_ROWS[i].knob == QD_K_4 && DEF_ROWS[i].mode != M_NOTE)
                       ? A.ui.tk == QD_TK_DEFAULT
                       : A.ui.tk == QD_TK_NONE),
              "%s, nothing held, %s %+d: the defaults %#x (want %#x), step 1 / the rest %#x, tk %d",
              UI_FIELD_NAME[DEF_ROWS[i].mode], KNAME(DEF_ROWS[i].knob), DEF_ROWS[i].d, defm, DEF_ROWS[i].want, got,
              A.ui.tk);
        ok += defm == DEF_ROWS[i].want && got == (DEF_ROWS[i].want | DEF_ROWS[i].trk);
        run(QD_TK_MS + QD_TK_FADE_MS + 5);
        *PAT() = base_p;
        A.model.t[0] = base_t;
    }
    mode(M_VEL);
    turn(QD_K_1, -30);
    CHECK(PAT()->trk[0].def.vel == 70 && R(0, 0).vel == 70 && R(0, 1).vel == 127, "VEL, KNOB 1: the default 70; step 2 "
          "(its own 127) stays");
    CHECK(qd_view_screen(&A) == QD_SCR_LANE && A.ui.tk == QD_TK_DEFAULT && A.ui.tk_src == QD_K_1,
          "the default's band over the lane (KNOB 1)");
    mode(M_NOTE);
    turn(QD_K_1, 4);
    turn(QD_K_2, 6);                                    /* (+1 +2 +3 +5 +6 +7: +4 is note 2's, skipped) */
    CHECK(PAT()->trk[0].def.note[1] == 64 && PAT()->trk[0].def.note[2] == 67 && R(0, 0).nn == 3 &&
              R(0, 0).notes[2] == 67,
          "NOTE, KNOB 1 / 2: the default chord's notes 2 and 3 (C E G)");
    for (m = 0; m < 9; m++) {
        int pg;
        mode(m);
        pg = A.ui.page[0];
        turn(QD_K_SELECT, 1);
        CHECK(A.ui.page[0] == pg + 1, "%s, nothing held: SELECT = the page", UI_FIELD_NAME[m]);
        turn(QD_K_SELECT, -1);
    }
    printf("  knobs, nothing held: %d of %d (mode, knob) rows change exactly the track's default\n", ok,
           (int)(sizeof DEF_ROWS / sizeof DEF_ROWS[0]));
}

/* P-LOCK, nothing held: KNOB 1..4 the track's sound, PRESETS its sound, SELECT the page; P1.3y: the track by FX held
 * + ALGORITHM (its big picture), any mode */
static void g_plock_track(void)
{
    qd_pattern_t before;
    int dec;
    start();
    black(B11);
    before = *PAT();
    bdown(QD_B_TRACKS);
    run(10);
    turn(QD_K_ALGO, 2);
    CHECK(A.sel == 2 && A.ui.tk == UI_TK_TRACKBIG && A.ui.hmode == UI_F_PLOCK, "P-LOCK, FX held + ALGORITHM +2: "
          "track 3, the big track picture, still P-LOCK");
    turn(QD_K_ALGO, -9);
    CHECK(A.sel == 0, "ALGORITHM down: track 1 (the first)");
    turn(QD_K_ALGO, 20);
    CHECK(A.sel == QD_NTRACK - 1, "ALGORITHM up: track 6 (the last)");
    turn(QD_K_ALGO, -7);
    bup(QD_B_TRACKS);
    run(10);
    CHECK(A.ui.mode == QD_MODE_STEP && A.sel == 0, "FX let go after a turn: no tracks screen, track 1");
    dec = A.model.t[0].patch[QD_QP_EDEC];
    turn(QD_K_2, 3);
    CHECK(A.model.t[0].patch[QD_QP_EDEC] == dec + 3 &&
              A.ui.tk == UI_TK_DECAY && !memcmp(&before, PAT(), offsetof(qd_pattern_t, snd)),
          "KNOB 2: the track's DECAY, live (the pattern untouched)");
    turn(QD_K_PRESETS, 1);
    CHECK(A.ui.latch == QD_LATCH_BROWSE && !A.ui.tk_lock, "PRESETS: the library browser (P1.5t)");
    btap(QD_B_HOME);
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    turn(QD_K_SELECT, 1);
    CHECK(A.ui.page[0] == 1 && A.ui.tk == UI_TK_PAGE, "SELECT: the page");
}

/* SEQ + B1..B4: the banks; the W keys, the LEDs and the map follow the bank */
static void g_banks(void)
{
    qd_leds_t l;
    start();
    bdown(QD_B_MIX);
    run(30);
    l = leds();
    CHECK(A.ui.seq_bank == 0 && l.key[BK(B1)] == QD_LED_LIT && l.key[BK(B3)] == QD_LED_OFF,
          "SEQ held: bank A lit, the empty banks off");
    black(B2);
    l = leds();
    CHECK(A.ui.seq_bank == 1 && l.key[BK(B2)] == QD_LED_LIT && l.key[BK(B1)] == QD_LED_DIM &&
              l.key[BK(B4)] == QD_LED_OFF && l.key[WK(0)] == QD_LED_OFF,
          "SEQ + B2: bank B lit, A (content) dim, D off; B's patterns empty: W1 off");
    wtap(2);
    l = leds();
    CHECK(A.seq.cur == 18 && l.key[WK(2)] == QD_LED_LIT && qd_view_screen(&A) == QD_SCR_SEQ,
          "SEQ + W3 in bank B, stopped: B03 at once (lit)");
    black(B5);
    CHECK(A.ui.seq_bank == 1, "SEQ + B5: nothing");
    bup(QD_B_MIX);
    run(10);
    wtap(0);
    CHECK(R(0, 0).on && A.seq.cur == 18, "B03 is edited (W1 a trig on it)");
    btap(QD_B_PLAY);
    run(300);
    bdown(QD_B_MIX);
    black(B4);
    kdown(WK(15));
    run(401);
    kup(WK(15));
    run(200);
    CHECK(A.seq.cur == 63, "bank D, W16 held: D16 at once (%d)", A.seq.cur);
    black(B1);
    ktap(WK(0), 30);
    CHECK(qd_seq_queued(&A.seq) == 0, "bank A, W1 tapped: A01 queued");
    bup(QD_B_MIX);
    btap(QD_B_PLAY);
}

/* the stopped preview: a held step sounds (notes, velocity, sound, locks, mix, the track's limit), re-struck by an
 * edit, released with its key; several together; nothing while playing; nothing written */
static const qd_pool_voice_t *voice_of(int note)        /* the gated voice of track 1 on note */
{
    int k;
    for (k = 0; k < A.pool.nvoice; k++)
        if (qd_pool_sounding(&A.pool, k) && A.pool.v[k].q.gate && A.pool.v[k].q.note == note &&
            A.pool.v[k].track == 0)
            return &A.pool.v[k];
    return 0;
}
static void g_preview(void)
{
    qd_pattern_t before;
    qd_seq_t seq0;
    uint32_t pv;
    const qd_pool_voice_t *v;
    start();
    wtap(0);
    wtap(4);
    wtap(8);
    mode(M_VEL);
    pv = A.player.previews;
    kdown(WK(0));
    run(200);
    CHECK(!sounding(60), "pending (200 ms): no preview yet");
    run(100);
    v = voice_of(60);
    CHECK(v && v->q.vel == 100 && A.player.previews == pv + 1, "held, stopped: step 1 sounds (C4, velocity 100)");
    black(B10);
    v = voice_of(60);
    CHECK(v && v->q.vel == 127 && A.player.previews == pv + 2, "an edit (VEL 127): re-struck at 127");
    turn(QD_K_1, -27);
    v = voice_of(60);
    CHECK(v && v->q.vel == 100 && A.player.previews == pv + 3, "KNOB 1: re-struck at 100");
    kup(WK(0));
    run(10);
    CHECK(!sounding(60) && !A.ui.pvs_n, "released with the key");
    /* the sound lock, a p-lock, a mix lock, the track's limit */
    qd_step_set_sound(PAT(), 0, 4, QD_SND_KICK);
    qd_lock_set(PAT(), 0, 4, QD_QP_HARM, 300);
    qd_lock_set(PAT(), 0, 4, QD_LK_PAN, -30);
    qd_step_set_vel(PAT(), 0, 4, 90);
    PAT()->trk[0].limit = 3;
    before = *PAT();
    seq0 = A.seq;
    whold(4);
    v = voice_of(60);
    {
        int16_t patch[QD_QUAD_NP];
        int ok = v != 0;
        if (v) {
            qd_player_voice_patch(&A.pool, (int)(v - A.pool.v), patch);
            ok = patch[QD_QP_HARM] == 300 && patch[QD_QP_FREQ] == qd_sound_patch(&A.proj, QD_SND_KICK)[QD_QP_FREQ] &&
                 patch[QD_QP_ALGO] == qd_sound_patch(&A.proj, QD_SND_KICK)[QD_QP_ALGO] && v->q.vel == 90 &&
                 (v->at.mixm & 1u) && v->at.mixv[0] == -30;
        }
        CHECK(ok && A.pool.limit[0] == 3, "step 5: the KICK, its HARM lock, its pan lock, velocity 90, the track's "
              "limit 3");
    }
    run(500);
    wrel(4);
    CHECK(!memcmp(&before, PAT(), offsetof(qd_pattern_t, snd)), "held, previewed and let go: the pattern unchanged");
    CHECK(seq0.resolved == A.seq.resolved && seq0.played == A.seq.played && seq0.nev == A.seq.nev &&
              seq0.serial == A.seq.serial && seq0.pos == A.seq.pos && !A.seq.playing &&
              !memcmp(seq0.cur_t, A.seq.cur_t, sizeof seq0.cur_t),
          "the sequencer's state unchanged");
    /* several held: together; one let go, the other sounds on (a shared note too) */
    qd_step_set_sound(PAT(), 0, 4, 0);
    {
        static const uint8_t E4 = 64;
        qd_step_set_notes(PAT(), 0, 4, &E4, 1);
    }
    kdown(WK(0)), kdown(WK(4)), kdown(WK(8));
    run(300);
    CHECK(sounding(60) && sounding(64) && A.ui.pvs_n == 3, "three held: C4 (steps 1, 9) and E4 (step 5) together");
    kup(WK(4));
    run(10);
    CHECK(sounding(60) && !sounding(64), "step 5 let go: E4 stops, C4 sounds on");
    kup(WK(0));
    run(10);
    CHECK(sounding(60), "step 1 let go: C4 sounds on (step 9 plays it too)");
    kup(WK(8));
    run(10);
    CHECK(!sounding(60), "the last let go: silence");
    /* a LOCK step: nothing */
    mode(M_TRIG);
    whold(8);
    black(B2);
    CHECK(!sounding(60), "a LOCK step: no note");
    wrel(8);
    /* playing: no preview; PLAY while previewing ends it */
    mode(M_VEL);
    whold(0);
    CHECK(sounding(60), "held: sounding");
    btap(QD_B_PLAY);
    CHECK(!A.ui.pvs_n, "PLAY: the preview released");
    wrel(0);
    pv = A.player.previews;
    whold(4);
    black(B3);
    wrel(4);
    CHECK(A.player.previews == pv, "playing: holding and editing a step previews nothing");
    btap(QD_B_PLAY);
}

/* F3 latched: open with both, used, closed with OCT- (nothing leaks into F1) or HOME; OCT+ does nothing */
static void g_f3_latch(void)
{
    qd_tset_t *ts;
    int bpm;
    start();
    ts = &PAT()->trk[0];
    f3_down();
    bup(F2), bup(F1);
    run(100);
    CHECK(A.ui.latch == QD_LATCH_F3 && qd_ctl_layer(&A) == QD_LAYER_F3 && qd_view_screen(&A) == QD_SCR_LAYER &&
              qd_ctl_layer_shown(&A) == QD_LAYER_F3 && leds().button[F1] == QD_LED_LIT &&
              leds().button[F2] == QD_LED_OFF,
          "both pressed together, let go: F3 stays open (its map; OCT- lit, OCT+ off)");
    black(B4), black(B9);
    CHECK(ts->rbase == 3 && ts->rtype == QD_RTYPE_DOTTED, "open: B4, B9 = dotted 1/8");
    wtap(11);
    CHECK(ts->pend[0] == 11, "open: W12 = the page's end");
    turn(QD_K_3, 2);
    CHECK(ts->pstart[0] == 2, "open: KNOB 3 = the start");
    bpm = A.bpm;
    turn(QD_K_SELECT, 3);
    turn(QD_K_ALGO, 4);
    CHECK(A.bpm == bpm + 3 && PAT()->swing == 54, "open: SELECT the tempo, ALGORITHM the swing (P1.3x)");
    btap(F2);
    CHECK(A.ui.latch == QD_LATCH_F3 && !A.ui.f2_loaded, "OCT+ (confirm, reserved): nothing, still open");
    bdown(F1);
    run(100);
    CHECK(!A.ui.latch && qd_ctl_layer(&A) == QD_LAYER_NONE && qd_ctl_layer_shown(&A) == QD_LAYER_NONE &&
              qd_view_screen(&A) == QD_SCR_LANE,
          "OCT-: closed; while it is still down it is no F1 (no map)");
    wtap(0);
    CHECK(R(0, 0).on && !A.ui.f1_copied, "W1 then: a plain tap (a trig), no F1 copy");
    bup(F1);
    run(10);
    bdown(F1);
    run(100);
    CHECK(qd_ctl_layer_shown(&A) == QD_LAYER_F1, "F1 pressed afresh: F1 again");
    bup(F1);
    run(10);
    f3_down();
    bup(F2), bup(F1);
    run(10);
    btap(QD_B_HOME);
    CHECK(!A.ui.latch && qd_ctl_layer(&A) == QD_LAYER_NONE, "HOME closes it");
    f3_down();
    bup(F2), bup(F1);
    run(10);
    btap(F1);
    bdown(F2);
    run(100);
    wtap(3);
    CHECK(qd_ctl_layer(&A) == QD_LAYER_F2 && A.ui.f2_loaded && A.ui.f2_last_act == 1,
          "after it: F2 afresh is F2 (W4 cut)");
    bup(F2);
    run(10);
}

/* the step latch (LFO): as if a step is always held; P1.3e: a W tap toggles its step in the selection, W keys held
 * together select them all, an empty selection gives the mode keys and the defaults back; edits, screens and the
 * preview act on the selection; LFO again or HOME end it, OCT- does not */
static void g_step_latch(void)
{
    int k, on = 0, off = 0;
    start();
    wtap(2);                                            /* step 3: the last edited */
    mode(M_VEL);
    btap(LFO);
    CHECK(A.ui.slatch && A.ui.nheld == 1 && A.ui.held[0] == 2 && leds().button[LFO] == QD_LED_LIT &&
              qd_view_screen(&A) == QD_SCR_HELD,
          "LFO: the latch on (lit), step 3 (the last edited) selected, its screen");
    CHECK(!sounding(60), "stopped: the selection alone is silent (P1.5s: only a finger, or the cursor once)");
    wtap(5);
    CHECK(A.ui.nheld == 1 && A.ui.held[0] == 5 && !R(0, 5).on && R(0, 2).on, "P1.3f: W6 tapped: it replaces the "
          "selection (no trig made); step 3 kept its trig");
    run(QD_CLICK_MS + 10);                              /* (P1.6d: two taps within QD_CLICK_MS are a double tap) */
    wtap(5);
    CHECK(A.ui.nheld == 0 && A.ui.slatch && leds().button[LFO] == QD_LED_LIT && qd_view_screen(&A) == QD_SCR_LANE &&
              !sounding(60),
          "W6 tapped again (the whole selection): nothing selected, the latch still on, the lane");
    bdown(LFO), black(B3), bup(LFO), run(10);           /* (P1.6d: an empty selection in VALUES: the black keys the
                                                         * default's values; LFO + a black key the mode) */
    CHECK(A.ui.hmode == M_LEN && A.ui.slatch, "an empty selection: LFO + B3, the mode (LEN)");
    turn(QD_K_1, 1);
    CHECK(PAT()->trk[0].def.len == QD_LEN_1 + 1 && A.ui.tk == QD_TK_DEFAULT, "the knobs: the track's default (LEN)");
    run(QD_TK_MS + QD_TK_FADE_MS + 5);
    bdown(LFO), black(B2), bup(LFO), run(10);
    kdown(WK(0));
    run(20);
    CHECK(A.ui.nheld == 1 && A.ui.held[0] == 0, "a gesture: W1 down, selected at once (the fingers shown)");
    kdown(WK(4));
    run(20);
    CHECK(A.ui.nheld == 2, ".. W5 down too: both");
    turn(QD_K_1, -1);
    CHECK(R(0, 0).vel == 99 && R(0, 4).vel == 99, "an edit mid-gesture: both keys pressed so far (99)");
    kup(WK(0)), kup(WK(4));
    run(20);
    CHECK(A.ui.nheld == 2 && A.ui.held[0] == 0 && A.ui.held[1] == 4, "all up: W1 and W5 are the selection");
    kdown(WK(8));
    run(20);
    kdown(WK(9));
    run(20);
    kup(WK(8)), kup(WK(9));
    run(20);
    CHECK(A.ui.nheld == 2 && A.ui.held[0] == 8 && A.ui.held[1] == 9, "another gesture (W9, W10): it replaces them");
    wtap(9);
    CHECK(A.ui.nheld == 1 && A.ui.held[0] == 9, "a tap on one of two selected: just it");
    kdown(WK(0));
    run(20);
    kdown(WK(4));
    run(20);
    kup(WK(0)), kup(WK(4));
    run(20);
    CHECK(R(0, 9).vel == 100 && A.ui.nheld == 2, "W1 + W5 again (step 10 dropped, unchanged)");
    black(B10);
    CHECK(R(0, 0).vel == 127 && R(0, 4).vel == 127 && R(0, 0).on && R(0, 4).on && R(0, 2).vel == 100 && !R(0, 8).on,
          "B10: velocity 127 on the selection; steps 3 and 9 not selected, unchanged");
    turn(QD_K_1, -7);
    CHECK(R(0, 0).vel == 120 && R(0, 4).vel == 120 && A.ui.tk == QD_TK_HELDKNOB, "KNOB 1: 120 on both, the band");
    CHECK(!sounding(60) && A.ui.pvs_n == 0, "P1.5s: an edit on the selection alone: silent");
    for (k = 0; k < 400; k++) {
        run(1);
        on += leds().key[WK(4)] == QD_LED_LIT;
        off += leds().key[WK(4)] == QD_LED_OFF;
    }
    CHECK(on > 100 && off > 100, "a selected step's key blinks (%d on, %d off)", on, off);
    btap(F1);
    CHECK(A.ui.slatch && A.ui.nheld == 2, "OCT-: not an exit (the latch stays)");
    btap(LFO);
    CHECK(!A.ui.slatch && !A.ui.nheld && leds().button[LFO] == QD_LED_OFF && !sounding(60) &&
              qd_view_screen(&A) == QD_SCR_LANE,
          "LFO again: off, nothing selected, silence, the lane");
    btap(LFO);
    CHECK(A.ui.slatch && A.ui.held[0] == 4, "on again: it starts on the last step edited (5)");
    bdown(QD_B_PATTERN);                                /* another track: its own selection (none edited: step 1) */
    black(B2);
    bup(QD_B_PATTERN);
    run(10);
    CHECK(A.sel == 1 && A.ui.slatch && A.ui.nheld == 1 && A.ui.held[0] == 0, "track 2: step 1 selected");
    btap(QD_B_HOME);
    CHECK(!A.ui.slatch && !A.ui.nheld, "HOME: off");
}

/* ---- P1.3e (§8b 2.2: editing a track's default, six notes, the quiet KNOB 4, HOME's views) */
static const struct {
    int mode;
    unsigned what;                                      /* the default's change a KNOB 1 turn makes */
} MKEY_ROWS[] = {
    {M_NOTE, X_NOTE}, {M_VEL, X_VEL}, {M_LEN, X_LEN}, {M_MICRO, X_MICRO}, {M_RATCH, X_RATCH}, {M_CHNC, X_CHANCE},
    {M_COND, X_COND}, {M_TRIG, X_TYPE}, {M_SOUND, 0},
};
/* a mode key held: the default's full picture; SELECT / KNOB 1..4 / ALGORITHM edit it; B11 resets it; the other black
 * keys nothing; let go: the lane */
static void g_mkey(void)
{
    size_t i;
    int ok = 0, okr = 0;
    start();
    wtap(0);
    for (i = 0; i < sizeof MKEY_ROWS / sizeof MKEY_ROWS[0]; i++) {
        const int m = MKEY_ROWS[i].mode;
        unsigned got, defm;
        int scr, k1, b11;
        kdown(BK(m));
        run(300);
        scr = qd_view_screen(&A) == QD_SCR_DEFAULT && A.ui.hmode == m;
        snap();
        turn(QD_K_1, m == M_CHNC ? -1 : 1);
        got = changed(0, &defm);
        k1 = m == M_SOUND ? (got & X_TRK) != 0 : defm == MKEY_ROWS[i].what;
        b11 = leds().key[BK(B11)] == QD_LED_LIT && leds().key[BK(m)] == QD_LED_LIT &&
              leds().key[BK(m == 0 ? 1 : 0)] == QD_LED_OFF;
        black(m == B1 ? B2 : B1);                       /* another black key: nothing */
        CHECK(scr && k1 && b11 && A.ui.hmode == m && qd_ctl_mkey(&A) == m,
              "%s key held: its default picture %d, KNOB 1 the default (%#x), B11 lit, the mode key lit (%d), "
              "another black key nothing", UI_FIELD_NAME[m], scr, defm, b11);
        ok += scr && k1 && b11;
        black(B11);
        CHECK(qd_step_default_initial(&A, m) && leds().key[BK(B11)] == QD_LED_OFF,
              "%s key held + B11: the default reset (B11 off)", UI_FIELD_NAME[m]);
        okr += qd_step_default_initial(&A, m);
        kup(BK(m));
        run(10);
        CHECK(qd_view_screen(&A) == QD_SCR_LANE && A.ui.tk == QD_TK_NONE && qd_ctl_mkey(&A) < 0,
              "%s key let go: the lane", UI_FIELD_NAME[m]);
    }
    /* NOTE: a three-note default chord by the knobs, ALGORITHM moves it all, B11 back to one note */
    kdown(BK(B1));
    run(20);
    turn(QD_K_1, 4);
    turn(QD_K_2, 6);
    CHECK(qd_view_screen(&A) == QD_SCR_DEFAULT && R(0, 0).nn == 3, "NOTE key held, KNOB 1 / 2: a C E G default "
          "(the picture at once: used)");
    turn(QD_K_ALGO, 2);
    CHECK(R(0, 0).nn == 3 && R(0, 0).notes[0] == 62 && R(0, 0).notes[1] == 66 && R(0, 0).notes[2] == 69,
          "ALGORITHM: the default chord moves together, two steps (D F# A)");
    turn(QD_K_SELECT, -2);
    CHECK(PAT()->trk[0].def.note[0] == 60 && PAT()->trk[0].def.note[1] == 64 && PAT()->trk[0].def.note[2] == 67,
          "P1.5o: SELECT (NOTE key held): the default chord, its shape kept (C E G)");
    turn(QD_K_2, 1);
    CHECK(PAT()->trk[0].def.note[2] == 68 && PAT()->trk[0].def.note[1] == 64, "KNOB 2 (NOTE key held): note 3 alone");
    black(B11);
    CHECK(R(0, 0).nn == 1 && R(0, 0).notes[0] == 60 && PAT()->trk[0].def.note[1] == 0,
          "B11: the default back to its single C4 (the chord cleared)");
    kup(BK(B1));
    run(10);
    /* a tap of a mode key: no picture (under 250 ms, unused) */
    kdown(BK(B2));
    run(100);
    CHECK(qd_view_screen(&A) == QD_SCR_LANE && A.ui.hmode == M_VEL, "a mode key tapped: the mode, no picture");
    kup(BK(B2));
    run(10);
    /* nothing held: a default turned shows the full picture a second, then the lane */
    turn(QD_K_1, 5);
    CHECK(A.ui.tk == QD_TK_DEFAULT && qd_view_screen(&A) == QD_SCR_LANE, "nothing held, KNOB 1: the TRACK DEFAULT "
          "picture (over the lane)");
    run(QD_TK_MS);
    CHECK(A.ui.tk == QD_TK_DEFAULT, ".. still there at 1 s");
    run(QD_TK_FADE_MS + 2);
    CHECK(A.ui.tk == QD_TK_NONE, ".. faded back by 1.12 s");
    /* the latch with an empty selection: a mode key held edits the default too */
    btap(LFO);
    wtap(0);
    bdown(LFO), black(B2), bup(LFO), run(10);
    turn(QD_K_1, 3);
    CHECK(PAT()->trk[0].def.vel == 108 && A.ui.tk == QD_TK_DEFAULT, "the latch, nothing selected (P1.6d: VEL by LFO + "
          "B2): KNOB 1 the default (108)");
    black(B10);
    CHECK(PAT()->trk[0].def.vel == 127 && A.ui.hmode == M_VEL, "P1.6d: .. B10: the default's velocity 127, the mode kept");
    btap(LFO);
    printf("  mode keys held: %d of 9 modes edit their default with the picture, %d reset by B11\n", ok, okr);
}

/* the track's default chord: ALGORITHM moves every note together (nothing held, the NOTE key held); on a held step it
 * is note 6 (the owner, P1.3e) */
static void g_six(void)
{
    static const uint8_t TRIAD[3] = {60, 64, 67};
    start();
    mode(M_NOTE);
    qd_tdef_set_notes(PAT(), 0, TRIAD, 3);
    wtap(0);
    mkturn(M_NOTE, QD_K_ALGO, 1);
    CHECK(R(0, 0).nn == 3 && R(0, 0).notes[0] == 61 && R(0, 0).notes[1] == 65 && R(0, 0).notes[2] == 68,
          "NOTE key held, ALGORITHM: the default chord moves together (C# F A)");
    kdown(BK(B1));
    run(20);
    turn(QD_K_ALGO, -1);
    kup(BK(B1));
    run(10);
    CHECK(R(0, 0).notes[0] == 60 && R(0, 0).notes[1] == 64 && R(0, 0).notes[2] == 67, "NOTE key held, ALGORITHM: "
          "together again (C E G)");
    whold(0);                                           /* a held step: six notes by the keys; ALGORITHM all */
    kdown(BK(B1)), kdown(BK(B5)), kdown(BK(B8)), kdown(BK(B10)), kdown(BK(B3)), kdown(BK(B6));
    run(20);
    kup(BK(B1)), kup(BK(B5)), kup(BK(B8)), kup(BK(B10)), kup(BK(B3)), kup(BK(B6));
    run(10);
    CHECK(R(0, 0).nn == 6 && ST(0, 0)->note[0] == 60 && ST(0, 0)->note[1] == 64 && ST(0, 0)->note[5] == 65,
          "six keys: six notes, in the order pressed (C E G A D F)");
    turn(QD_K_SELECT, 1);
    CHECK(ST(0, 0)->note[0] == 61 && ST(0, 0)->note[5] == 66 && R(0, 0).nn == 6 && A.ui.tk == QD_TK_HELDKNOB &&
              A.ui.tk_job == QD_KJ_CHORD,
          "a step held, SELECT: all six up together (P1.5o)");
    {
        static const uint8_t M13[6] = {48, 52, 55, 59, 62, 69};
        char rt[4], q[8];
        CHECK(qd_chord_name(M13, 6, rt, q) && !strcmp(q, "maj13") && !strcmp(rt, "C"), "six notes named Cmaj13 "
              "(%s%s)", rt, q);
    }
    wrel(0);
}

/* KNOB 4 where it has no other job: the main value, no overlay (the lane / the held screen change in place) */
static void g_quiet(void)
{
    start();
    wtap(0);
    mode(M_VEL);
    turn(QD_K_4, -10);
    CHECK(PAT()->trk[0].def.vel == 90 && A.ui.tk == QD_TK_NONE && qd_view_screen(&A) == QD_SCR_LANE,
          "VEL, nothing held, KNOB 4: the default 90, no overlay (the lane)");
    turn(QD_K_1, 1);
    CHECK(A.ui.tk == QD_TK_DEFAULT, "KNOB 1: the default's picture ..");
    turn(QD_K_4, 1);
    CHECK(A.ui.tk == QD_TK_NONE && PAT()->trk[0].def.vel == 92, ".. KNOB 4 after it: the picture goes, in place");
    whold(0);
    turn(QD_K_4, 5);
    CHECK(R(0, 0).vel == 97 && OV(0, 0, QD_PM_VEL) && A.ui.tk == QD_TK_NONE && qd_view_screen(&A) == QD_SCR_HELD,
          "a step held, KNOB 4: its velocity 97, no band (the held screen)");
    wrel(0);
    mode(M_NOTE);
    whold(0);
    turn(QD_K_4, 2);
    CHECK(ST(0, 0)->note[4] == 62 && A.ui.tk == QD_TK_HELDKNOB, "NOTE: KNOB 4 is note 5 (with its band)");
    wrel(0);
}

/* P1.3y: HOME comes back to the view in use from anywhere (a latch closed in the same press) and never cycles; SHIFT +
 * HOME cycles PIANO ROLL, STEPS, EDIT; the view stays through idle */
static void g_home(void)
{
    start();
    CHECK(A.ui.view == QD_VIEW_ROLL, "the sequencer starts in the PIANO ROLL");
    btap(QD_B_HOME);
    CHECK(A.ui.view == QD_VIEW_ROLL, "P1.3y: HOME alone: the PIANO ROLL still (it never cycles)");
    shome();
    CHECK(A.ui.view == QD_VIEW_STEPS && qd_view_screen(&A) == QD_SCR_LANE && !qd_ctl_shift(&A), "SHIFT + HOME: STEPS");
    wtap(3);
    CHECK(R(0, 3).on, "STEPS: a W tap is a trig as before");
    mode(M_VEL);
    whold(3);
    CHECK(qd_view_screen(&A) == QD_SCR_HELD, "STEPS: a held step opens its screen");
    turn(QD_K_4, -20);
    CHECK(R(0, 3).vel == 80 && A.ui.tk == QD_TK_NONE, "STEPS: KNOB 4 quiet on the held step");
    wrel(3);
    turn(QD_K_1, 3);
    CHECK(A.ui.tk == QD_TK_DEFAULT, "STEPS: a default's picture as in the roll");
    run(QD_TK_MS + QD_TK_FADE_MS + 5);
    turn(QD_K_SELECT, 2);
    CHECK(A.ui.page[0] == 2 && A.ui.tk == QD_TK_NONE, "P1.3g: STEPS, SELECT: page 3, no picker (the row moves)");
    turn(QD_K_SELECT, -2);
    shome();
    CHECK(A.ui.view == QD_VIEW_EDIT && qd_view_screen(&A) == QD_SCR_EDIT, "SHIFT + HOME again: EDIT");
    shome_wrap();
    CHECK(A.ui.view == QD_VIEW_ROLL, "SHIFT + HOME again (P2.11: past the Geek Out pages): the PIANO ROLL");
    turn(QD_K_SELECT, 1);
    CHECK(A.ui.page[0] == 1 && A.ui.tk == UI_TK_PAGE, "the PIANO ROLL, SELECT: page 2 with its picker");
    turn(QD_K_SELECT, -1);
    run(QD_TK_MS + QD_TK_FADE_MS + 5);
    shome();
    f3_down();
    bup(F2), bup(F1);
    run(10);
    btap(QD_B_HOME);
    CHECK(!A.ui.latch && A.ui.view == QD_VIEW_STEPS && qd_view_screen(&A) == QD_SCR_LANE,
          "F3 open: HOME closes it, STEPS (one press)");
    btap(LFO);
    wtap(4);
    CHECK(A.ui.slatch && qd_view_screen(&A) == QD_SCR_HELD, "the step latch, step 5 selected: its screen");
    btap(QD_B_HOME);
    CHECK(!A.ui.slatch && !A.ui.nheld && A.ui.view == QD_VIEW_STEPS && qd_view_screen(&A) == QD_SCR_LANE,
          "HOME: the latch ended and STEPS in the same press");
    btap(QD_B_REC);
    btap(QD_B_HOME);
    CHECK(A.ui.mode == QD_MODE_STEP && A.ui.view == QD_VIEW_STEPS, "from the keyboard: HOME back to STEPS (the last)");
    btap(QD_B_TRACKS);
    btap(QD_B_HOME);
    CHECK(A.ui.mode == QD_MODE_STEP && A.ui.view == QD_VIEW_STEPS, "from the tracks screen: back to STEPS");
    run(QD_IDLE_MS + 10);
    CHECK(A.ui.mode == QD_MODE_IDLE, "idle");
    btap(QD_B_HOME);
    CHECK(A.ui.mode == QD_MODE_STEP && A.ui.view == QD_VIEW_STEPS, "woken by HOME: STEPS still (no switch)");
}

/* P1.3q: STEPS, the steps' kinds by colour, on the screen: own notes warm white, the track's notes cyan (filled), a
 * ghost (LOCK) and an off step not filled; on the open row (page 1: its fill from the bottom, the velocity) and on a
 * folded one (page 2) */
static uint16_t lcd_q[QD_W * QD_H];
static unsigned c565(qd_rgb c)                          /* as qd_end rounds */
{
    return ((c >> 16 & 0xffu) * 31u + 127u) / 255u << 11 | ((c >> 8 & 0xffu) * 63u + 127u) / 255u << 5 |
           ((c & 0xffu) * 31u + 127u) / 255u;
}
static unsigned px(int x, int y) { return (unsigned)(lcd_q[y * QD_W + x] >> 8 | (lcd_q[y * QD_W + x] & 0xffu) << 8); }
static int cell_x(int i) { return 20 + i * 13 + (i / 4) * 2 + 1; }   /* a pixel inside the cell's left edge (ui_lane.c;
                                                                        * + 3: clear of an outline) */
static void g_steps_kinds(void)
{
    static const uint8_t own[2] = {60, 64};
    int pg;
    start();
    PAT()->trk[0].loop = 3;                             /* pages 1 and 2 in the loop (else page 2 is dim) */
    for (pg = 0; pg < 2; pg++) {
        const int s0 = pg * 16;
        qd_step_on(PAT(), 0, s0);                       /* own notes */
        qd_step_set_notes(PAT(), 0, s0, own, 2);
        qd_step_on(PAT(), 0, s0 + 1);                   /* the track's notes */
        qd_step_set_trig(PAT(), 0, s0 + 2, QD_TRIG_LOCK);   /* a ghost; s0 + 3 off */
    }
    shome();
    run(400);
    qd_app_frame(&A, lcd_q);
    CHECK(A.ui.view == QD_VIEW_STEPS && OV(0, 0, QD_PM_NOTE) && R(0, 1).on && !OV(0, 1, QD_PM_NOTE) &&
          R(0, 2).type == QD_TRIG_LOCK, "STEPS: own notes, the track's, a ghost on pages 1 and 2");
    CHECK(px(cell_x(0), 85) == c565(UI_STEP_OWN) && px(cell_x(1), 85) == c565(UI_STEP_TRACK) &&
          px(cell_x(2) + 3, 85) == 0 && px(cell_x(3) + 3, 85) == 0,
          "the open row: own notes warm white, the track's cyan, a ghost and an off step not filled (%04x %04x %04x "
          "%04x)", px(cell_x(0), 85), px(cell_x(1), 85), px(cell_x(2) + 3, 85), px(cell_x(3) + 3, 85));
    CHECK(px(cell_x(0), 103) == c565(UI_STEP_OWN) && px(cell_x(1), 103) == c565(UI_STEP_TRACK) &&
          px(cell_x(2) + 1, 98) == 0 && px(cell_x(3) + 3, 103) == 0,
          "a folded row (page 2): the same kinds (%04x %04x %04x %04x)", px(cell_x(0), 103), px(cell_x(1), 103),
          px(cell_x(2) + 1, 98), px(cell_x(3) + 3, 103));
    CHECK(ui_steps_fill(UI_C_RANGE | UI_C_ON | UI_C_LOCK | UI_C_OWN) == QD_NONE && ui_steps_fill(UI_C_RANGE) == QD_NONE &&
          ui_steps_fill(UI_C_ON | UI_C_OWN) == QD_NONE, "no fill for a ghost, an off step, a step out of range");
}

/* P1.3s: playing, the playhead outlines no cell (only the selection is outlined): STEPS' open row while step 6
 * pops, the gap left of its cell dark; selected by the cursor, the outline there */
static void g_play_outline(void)
{
    int k, x0;
    start();
    for (k = 0; k < 16; k++)
        qd_step_on(PAT(), 0, k);
    shome();
    run(400);
    btap(QD_B_PLAY);
    while (qd_seq_track_step(&A.seq, 0) != 5)
        run(1);
    run(20);
    qd_app_frame(&A, lcd_q);
    x0 = 20 + 5 * 13 + 2 - 1;                           /* the pixel left of step 6's cell (ui_lane.c's ov_x - 1) */
    CHECK(A.ui.nheld == 0 && px(x0, 66) == 0 && px(x0, 50) == 0,
          "playing, nothing selected: no outline round the playing step (%04x %04x)", px(x0, 66), px(x0, 50));
    btap(QD_B_PLAY);
    turn(QD_K_ALGO, 1);
    turn(QD_K_ALGO, 5);
    run(50);
    qd_app_frame(&A, lcd_q);
    CHECK(A.ui.held[0] == 5 && px(x0, 66) != 0, "selected (the cursor on step 6): its outline (%04x)", px(x0, 66));
}

/* P1.3q / P1.3y: ALGORITHM (no finger on a step, any view) is the step cursor: the latch on, its one selection moved
 * through the loop (into the next page and round), the page following; SELECT a page puts it on the page's first
 * step; values apply to it (an off step turned on); PRESETS is the track's sound in every view */
static void g_cursor(void)
{
    int snd, k;
    start();
    PAT()->trk[0].loop = 3;                             /* pages 1 and 2 in the loop */
    mode(M_VEL);
    shome();
    CHECK(A.ui.view == QD_VIEW_STEPS && !A.ui.slatch, "STEPS, the latch off");
    snd = A.model.t[0].preset;
    turn(QD_K_PRESETS, 1);
    CHECK(!A.ui.slatch && A.ui.latch == QD_LATCH_BROWSE,
          "P1.3y: STEPS, PRESETS: the track's preset (P1.5t: the library browser), the latch untouched");
    btap(QD_B_HOME);
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    CHECK(A.ui.view == QD_VIEW_STEPS, "HOME: the browser closed, STEPS still");
    turn(QD_K_ALGO, 1);
    CHECK(A.ui.slatch && A.ui.nheld == 1 && A.ui.held[0] == 0 && A.model.t[0].preset == snd &&
          qd_view_screen(&A) == QD_SCR_LANE, "ALGORITHM: the latch on, step 1 selected, STEPS stays");
    turn(QD_K_ALGO, 3);
    CHECK(A.ui.nheld == 1 && A.ui.held[0] == 3, "three detents: step 4 (%d)", A.ui.held[0] + 1);
    turn(QD_K_ALGO, 13);
    CHECK(A.ui.held[0] == 16 && A.ui.page[0] == 1, "on past the page's end: page 2's step 1, the page follows");
    turn(QD_K_ALGO, -1);
    CHECK(A.ui.held[0] == 15 && A.ui.page[0] == 0, "back: page 1's step 16");
    turn(QD_K_ALGO, 17);
    CHECK(A.ui.held[0] == 32 && A.ui.page[0] == 2, "P1.3r: on into page 3 (not in the loop): its step 1");
    turn(QD_K_ALGO, -33);
    CHECK(A.ui.held[0] == 127 && A.ui.page[0] == 7, "back past page 1's start: page 8's step 16");
    turn(QD_K_ALGO, 1);
    CHECK(A.ui.held[0] == 0 && A.ui.page[0] == 0, "page 8's end on to page 1's step 1");
    PAT()->trk[0].pstart[1] = 4;                        /* page 2 plays steps 5 .. 12 */
    PAT()->trk[0].pend[1] = 11;
    turn(QD_K_ALGO, 16);
    CHECK(A.ui.held[0] == 20, "a page's range: page 2 starts at its step 5 (%d)", A.ui.held[0] + 1);
    turn(QD_K_ALGO, -1);
    CHECK(A.ui.held[0] == 15, "before it: page 1's step 16 (%d)", A.ui.held[0] + 1);
    turn(QD_K_SELECT, 1);
    CHECK(A.ui.page[0] == 1 && A.ui.slatch && A.ui.nheld == 1 && A.ui.held[0] == 20,
          "P1.3y: SELECT: page 2, the selection on its first step in range (5) (%d)", A.ui.held[0] + 1);
    turn(QD_K_SELECT, -1);
    CHECK(A.ui.page[0] == 0 && A.ui.held[0] == 0, "SELECT back: page 1's step 1 (%d)", A.ui.held[0] + 1);
    turn(QD_K_ALGO, 5);
    CHECK(!R(0, 5).on, "step 6 selected, off");
    black(B3);
    CHECK(R(0, 5).on && OV(0, 5, QD_PM_VEL) && A.ui.held[0] == 5 && qd_view_screen(&A) == QD_SCR_LANE,
          "a value (VEL B3): it applies to the cursor's step, turned on (STEPS stays)");
    wtap(7);
    CHECK(A.ui.nheld == 1 && A.ui.held[0] == 7 && qd_view_screen(&A) == QD_SCR_HELD,
          "a W in the latch selects as before (its held screen)");
    btap(QD_B_HOME);
    CHECK(!A.ui.slatch && A.ui.view == QD_VIEW_STEPS, "HOME ends the latch");
    shome();
    shome_wrap();
    CHECK(A.ui.view == QD_VIEW_ROLL, "SHIFT + HOME on (P2.11: past the Geek Out pages): the PIANO ROLL");
    turn(QD_K_PRESETS, 1);
    CHECK(!A.ui.slatch && A.ui.latch == QD_LATCH_BROWSE, "the PIANO ROLL: PRESETS is the browser too");
    btap(QD_B_HOME);
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    turn(QD_K_ALGO, 1);
    k = A.ui.held[0];
    turn(QD_K_ALGO, 2);
    CHECK(A.ui.slatch && A.ui.held[0] == k + 2 && qd_view_screen(&A) == QD_SCR_LANE,
          "P1.3y: the PIANO ROLL: ALGORITHM the cursor too (two on: step %d, the roll stays)", A.ui.held[0] + 1);
    btap(QD_B_HOME);
}

/* P1.3r: the EDIT view: the mode's picture over the page's row: the track's default with nothing held, a held step's
 * values held, the cursor's step; a VEL edit changes the row's cell and the picture alike; no takeover over it */
static void g_edit(void)
{
    static ui_held_t h;
    static ui_lane_t l;
    int v0;
    start();
    mode(M_VEL);
    shome();
    shome();
    CHECK(A.ui.view == QD_VIEW_EDIT && qd_view_screen(&A) == QD_SCR_EDIT, "SHIFT + HOME twice: EDIT");
    qd_view_edit(&A, &h, &l);
    CHECK(h.is_default && h.field == UI_F_VEL && h.vel == PAT()->trk[0].def.vel,
          "nothing held: the track's default VEL (%d)", h.vel);
    wtap(3);
    whold(3);
    CHECK(qd_view_screen(&A) == QD_SCR_EDIT, "a step held: still EDIT");
    qd_view_edit(&A, &h, &l);
    v0 = h.vel;
    CHECK(!h.is_default && h.step == 4 && l.col[3].on && l.col[3].vel == v0, "the held step's values (step 4, VEL %d)",
          v0);
    black(B2);
    qd_view_edit(&A, &h, &l);
    CHECK(h.vel != v0 && l.col[3].vel == h.vel && R(0, 3).vel == h.vel,
          "a VEL value: the picture's value and the row's cell both %d (was %d)", h.vel, v0);
    turn(QD_K_ALGO, 5);
    qd_view_edit(&A, &h, &l);
    CHECK(l.col[3].vel == h.vel && R(0, 3).vel == h.vel && A.ui.tk != QD_TK_NONE && !qd_view_overlay(&A),
          "ALGORITHM finely: both follow (%d); its takeover is not drawn over EDIT", h.vel);
    turn(QD_K_PRESETS, 1);
    qd_view_edit(&A, &h, &l);
    CHECK(h.field == UI_F_SOUND && !h.is_default && A.ui.tk_lock && qd_view_overlay(&A),
          "P1.3y: PRESETS, a step held: its sound lock (P1.5t: the pool picker over EDIT, the mock-up's)");
    run(QD_TK_MS + QD_TK_FADE_MS + 5);
    qd_view_edit(&A, &h, &l);
    CHECK(h.field == UI_F_VEL, "a second on: VEL's picture again");
    wrel(3);
    turn(QD_K_1, 2);
    CHECK(A.ui.tk != QD_TK_NONE && !qd_view_overlay(&A) && qd_view_screen(&A) == QD_SCR_EDIT,
          "nothing held, a default's knob: no full picture over EDIT");
    turn(QD_K_ALGO, 1);
    qd_view_edit(&A, &h, &l);
    CHECK(A.ui.slatch && qd_view_screen(&A) == QD_SCR_EDIT && !h.is_default && h.step == A.ui.held[0] + 1,
          "ALGORITHM: the cursor's step in EDIT (step %d)", h.step);
    btap(QD_B_HOME);
    CHECK(!A.ui.slatch && A.ui.view == QD_VIEW_EDIT, "HOME: the latch ended, EDIT still");
    shome_wrap();
    turn(QD_K_1, 2);
    CHECK(A.ui.view == QD_VIEW_ROLL && qd_view_overlay(&A), "the PIANO ROLL: the takeover as before");
}

/* P1.3v (PLAN §9a's clarity rules): no text under 11 px on any screen. qd_gfx records the smallest text it draws (its
 * size times the transform's scale); every gesture above has drawn its frames since main's reset, and this one visits
 * the rest: every hold mode in the PIANO ROLL, STEPS and EDIT, nothing held and a step held, the layer maps, SHIFT's
 * map in each context, the keyboard, the tracks screen, a confirmation, playing */
static void g_clarity(void);
static void g_long_names(void)
{
    long_names = 1;
    g_clarity();
    long_names = 0;
}
static void g_clarity(void)
{
    static const int MODES[11] = {M_NOTE, M_VEL, M_LEN, M_MICRO, M_RATCH, M_CHNC, M_COND, M_TRIG, M_FUNC, M_SOUND,
                                  M_PLOCK};
    static const uint8_t CH[3] = {60, 64, 67};
    const char *small = "";
    float mn;
    int v, m, k;
    start();
    for (k = 0; k < 16; k += 2)
        qd_step_on(PAT(), 0, k);
    qd_step_set_notes(PAT(), 0, 0, CH, 3);
    qd_step_set_trig(PAT(), 0, 5, QD_TRIG_LOCK);
    qd_step_set_cond(PAT(), 0, 2, 3);
    qd_step_set_sound(PAT(), 0, 4, 12);
    for (v = 0; v < 3; v++) {                           /* PIANO ROLL, STEPS, EDIT */
        for (m = 0; m < 11; m++) {
            mode(MODES[m]);
            run(60);
            whold(0);
            run(300);
            turn(QD_K_1, 1);
            run(60);
            wrel(0);
            run(400);
        }
        shome();
    }
    btap(QD_B_SOUND);                                   /* P1.4a: the sound editor's pages, nothing held and a step held */
    for (k = 0; k < QD_SND_NPAGE; k++) {
        A.ui.snd_page = k;
        run(60);
        whold(0);
        turn(QD_K_1, 1);
        run(60);
        wrel(0);
        run(30);
    }
    btap(QD_B_SOUND);
    bdown(SHIFT);                                       /* SHIFT's maps: the step view, a step held, the keyboard */
    run(100);
    bup(SHIFT);
    whold(0);
    bdown(SHIFT);
    run(100);
    bup(SHIFT);
    wrel(0);
    btap(QD_B_REC);
    bdown(SHIFT);
    run(100);
    bup(SHIFT);
    btap(QD_B_HOME);
    for (k = 0; k < QD_FX_NPAGE; k++) {                 /* P1.4b: the effects' pages, the mix's, the track settings' */
        if (!k)
            btap(QD_B_TRACKS);
        else
            turn(QD_K_SELECT, 1);
        run(120);
    }
    btap(QD_B_SAVE);                                    /* P1.5a: the sounds page, the name editor, the projects */
    run(150);
    btap(QD_B_SAVE);
    run(300);
    btap(QD_B_OCTUP);
    run(300);
    kdown(WK(1));
    run(400);
    kup(WK(1));
    run(20);
    black(B10);
    run(150);
    turn(QD_K_SELECT, 1);
    run(60);
    btap(QD_B_OCTDN);
    kdown(BK(B11));
    run(20);
    wtap(3);
    kup(BK(B11));
    run(200);
    btap(QD_B_OCTDN);
    btap(QD_B_MIX);
    run(150);
    btap(QD_B_MIX);
    run(150);
    btap(QD_B_HOME);
    btap(QD_B_PAGE);
    run(120);
    btap(QD_B_PAGE);
    run(120);
    btap(QD_B_PATTERN);
    run(120);
    btap(QD_B_PATTERN);
    run(120);
    btap(QD_B_HOME);
    btap(QD_B_SOUND);                                   /* P1.5b: the editor's keyboard (SCALE, FULL), its chips */
    btap(QD_B_REC);
    for (k = 0; k < QD_SND_NPAGE; k++) {
        A.ui.snd_page = k;
        A.proj.rec_ekeys = k & 1;
        run(60);
    }
    A.proj.rec_ekeys = 0;
    btap(QD_B_HOME);
    bdown(QD_B_MUTE);                                   /* .. the latch mode's takeover, both ways */
    run(450);
    bup(QD_B_MUTE);
    run(300);
    bdown(QD_B_MUTE);
    run(450);
    bup(QD_B_MUTE);
    run(300);
    bdown(SHIFT);                                       /* .. the record settings' four rows */
    btap(QD_B_REC);
    bup(SHIFT);
    run(100);
    turn(QD_K_4, 1);
    run(100);
    turn(QD_K_4, -1);
    btap(QD_B_HOME);
    btap(QD_B_PLAY);                                    /* playing */
    run(600);
    btap(QD_B_PLAY);
    run(100);
    mn = qd_text_stats_min(&small);
    CHECK(mn >= 12.f, "no text under 12 px on any screen (the smallest: %.1f px, \"%s\")", (double)mn, small);
    printf("  clarity: the smallest text drawn in every gesture's frames: %.1f px (\"%s\")\n", (double)mn, small);
}

/* P1.3w / P1.3y: FUNC (B9): B1..B7 the functions, B8 LEAP (the last n), B9 JUMP (the last target); KNOB 1 the kind,
 * KNOB 2 LEAP's n, KNOB 3 JUMP's target; B9 held + a W: the target (not held), on another page too (SELECT while
 * holding), in the latch too; the LEDs; B11 clears it, an empty step gets a trig; LFO held + a black key: the mode,
 * the latch kept; SOUND on B10 */
static void g_func(void)
{
    start();
    mode(M_FUNC);
    CHECK(A.ui.hmode == UI_F_FUNC, "B9: FUNC");
    whold(2);
    black(B3);
    CHECK(ST(0, 2)->func == QD_FN_REVERSE && R(0, 2).on, "B3: REVERSE (an empty step: a trig)");
    CHECK(leds().key[BK(B3)] == QD_LED_LIT && leds().key[BK(B2)] != QD_LED_LIT, "FUNC's LEDs: B3 lit (REVERSE)");
    turn(QD_K_1, 5);
    CHECK(ST(0, 2)->func == QD_FN_MAKE_LEAP(2), "KNOB 1 +5: the kind past RANDOM FUNCTION, LEAP +2 (%d)",
          ST(0, 2)->func);
    turn(QD_K_2, 3);
    CHECK(ST(0, 2)->func == QD_FN_MAKE_LEAP(5), "KNOB 2 +3: LEAP +5");
    turn(QD_K_2, -7);
    CHECK(ST(0, 2)->func == QD_FN_MAKE_LEAP(-3), "KNOB 2 -7: past 0 (skipped), LEAP -3 (%d)", ST(0, 2)->func);
    turn(QD_K_3, 4);
    CHECK(ST(0, 2)->func == QD_FN_MAKE_JUMP(4), "KNOB 3 +4: a JUMP, to step 5 (from the page's start)");
    turn(QD_K_ALGO, -2);
    CHECK(ST(0, 2)->func == QD_FN_RFUNC, "ALGORITHM -2 (a finger on the step): the kind, RANDOM FUNCTION");
    kdown(BK(B9));
    run(20);
    CHECK(ST(0, 2)->func == QD_FN_MAKE_JUMP(4) && leds().key[BK(B9)] == QD_LED_LIT,
          "B9: JUMP, to the last target (step 5), B9 lit");
    kdown(WK(9));
    run(40);
    CHECK(ST(0, 2)->func == QD_FN_MAKE_JUMP(9) && A.ui.nheld == 1 && A.ui.held[0] == 2,
          "B9 held + W10: the target (JUMP 10), not held");
    kup(WK(9));
    run(20);
    kup(BK(B9));
    run(10);
    CHECK(!R(0, 9).on && A.ui.nheld == 1, "the target's key let go: no tap (step 10 stays empty)");
    kdown(WK(5));
    run(40);
    kup(WK(5));
    run(20);
    CHECK(ST(0, 2)->func == QD_FN_MAKE_JUMP(9) && A.ui.nheld == 1, "a W without B9: no target (the old gesture gone)");
    turn(QD_K_SELECT, 1);
    CHECK(A.ui.page[0] == 1 && A.ui.nheld == 1, "SELECT while holding: page 2, the step still held");
    kdown(BK(B9));
    run(20);
    kdown(WK(4));
    run(40);
    kup(WK(4));
    run(20);
    kup(BK(B9));
    run(10);
    CHECK(ST(0, 2)->func == QD_FN_MAKE_JUMP(20), "B9 + a W on page 2: the target is step 21 (%d)", ST(0, 2)->func);
    black(B8);
    CHECK(ST(0, 2)->func == QD_FN_MAKE_LEAP(-3) && leds().key[BK(B8)] == QD_LED_LIT, "B8: LEAP, the last n (-3), lit");
    black(B11);
    CHECK(ST(0, 2)->func == QD_FN_NONE, "B11: no function");
    black(B2);
    CHECK(ST(0, 2)->func == QD_FN_RESTART, "B2: RESTART");
    wrel(2);
    turn(QD_K_SELECT, -1);
    btap(LFO);                                          /* the latch: B9 held + W1 makes step 6's target step 1 */
    CHECK(A.ui.slatch && A.ui.nheld == 1 && A.ui.held[0] == 5, "LFO: the latch on, at the last step touched (6)");
    kdown(BK(B9));
    run(20);
    kdown(WK(0));
    run(40);
    kup(WK(0));
    run(20);
    kup(BK(B9));
    run(10);
    CHECK(A.ui.slatch && A.ui.held[0] == 5 && ST(0, 5)->func == QD_FN_MAKE_JUMP(0) && !R(0, 0).on,
          "the latch: B9 held + W1: step 6 jumps to step 1, still selected (%d %d %d %d)", A.ui.slatch, A.ui.held[0],
          ST(0, 5)->func, R(0, 0).on);
    bdown(LFO);
    run(10);
    black(B2);
    bup(LFO);
    run(10);
    CHECK(A.ui.hmode == UI_F_VEL && A.ui.slatch && A.ui.nheld == 1 && A.ui.held[0] == 5 && !OV(0, 5, QD_PM_VEL),
          "LFO held + B2: VEL, the latch and the selection kept, no value (%d %d %d %d)", A.ui.hmode, A.ui.slatch,
          A.ui.nheld, A.ui.held[0]);
    btap(LFO);
    CHECK(!A.ui.slatch, "LFO tapped: the latch off");
    mode(M_SOUND);
    CHECK(A.ui.hmode == UI_F_SOUND, "B10: SOUND");
}

/* P1.3y (the owner's additions): the value keys light the NEAREST value in every hold mode (a tie: the lower key),
 * a mode at a time on a held step set by API */
static uint16_t lit_keys(void)
{
    qd_leds_t l = leds();
    uint16_t m = 0;
    int b;
    for (b = 0; b < 10; b++)
        m |= l.key[BK(b)] == QD_LED_LIT ? (uint16_t)(1u << b) : 0;
    return m;
}
static void g_nearest(void)
{
    start();
    qd_step_on(PAT(), 0, 0);
    mode(M_VEL);
    whold(0);
    qd_step_set_vel(PAT(), 0, 0, 100);
    CHECK(lit_keys() == 1u << B8, "VEL 100: B8 (102) lit (%#x)", lit_keys());
    qd_step_set_vel(PAT(), 0, 0, 70);                   /* 64 | 76: 70 is 6 from each: the lower, B5 */
    CHECK(lit_keys() == 1u << B5, "VEL 70 (a tie 64 / 76): B5, the lower (%#x)", lit_keys());
    wrel(0);
    mode(M_LEN);
    whold(0);
    qd_step_set_len(PAT(), 0, 0, 7);                    /* between 2 (6) and 4 (8): a tie, the lower: 2 = B6 */
    CHECK(lit_keys() == 1u << B6, "LEN between 2 and 4 (a tie): B6, the lower (%#x)", lit_keys());
    qd_step_set_len(PAT(), 0, 0, QD_LEN_INF);
    CHECK(lit_keys() == 1u << B10, "LEN INF: B10 (%#x)", lit_keys());
    wrel(0);
    mode(M_MICRO);
    whold(0);
    qd_step_set_micro(PAT(), 0, 0, 7);                  /* 5 | 10: nearer 5 = B7 */
    CHECK(lit_keys() == 1u << B7, "MICRO +7: B7 (+5) (%#x)", lit_keys());
    qd_step_set_micro(PAT(), 0, 0, 0);
    CHECK(lit_keys() == 0, "MICRO 0 (on the step): no key lit (%#x)", lit_keys());
    wrel(0);
    mode(M_CHNC);
    whold(0);
    qd_step_set_chance(PAT(), 0, 0, 33);
    CHECK(lit_keys() == 1u << B3, "CHNC 33 %%: B3 (30 %%) (%#x)", lit_keys());
    qd_step_set_chance(PAT(), 0, 0, 5);
    CHECK(lit_keys() == 1u << B1, "CHNC 5 %%: B1 (10 %%, the nearest) (%#x)", lit_keys());
    wrel(0);
    mode(M_RATCH);
    whold(0);
    qd_step_set_ratchet(PAT(), 0, 0, 12, QD_RAMP_FLAT);
    CHECK(lit_keys() == 1u << B8 || (R(0, 0).ratch <= 8 && lit_keys() == 1u << (R(0, 0).ratch - 1)),
          "RATCH x%d: its nearest key (%#x)", R(0, 0).ratch, lit_keys());
    wrel(0);
    mode(M_COND);
    whold(0);
    qd_step_set_cond(PAT(), 0, 0, qd_cond_ab(6, 8));    /* 6:8: A nearest 4 (B6), B nearest 4 (B10) */
    CHECK(lit_keys() == ((1u << B6) | (1u << B10)), "COND 6:8: B6 (4:) and B10 (:4), the nearest (%#x)", lit_keys());
    wrel(0);
    mode(M_SOUND);
    whold(0);
    qd_step_set_sound(PAT(), 0, 0, 14);                 /* bank 1 shown (1..10): 14 is past it, B10 the nearest */
    CHECK(A.ui.sound_bank == 0 && lit_keys() == 1u << B10, "SOUND 14, bank 1..10 shown: B10, the bank's end (%#x)",
          lit_keys());
    wrel(0);
    mode(M_FUNC);
    whold(0);
    qd_step_set_func(PAT(), 0, 0, QD_FN_MAKE_LEAP(3));
    CHECK(lit_keys() == 1u << B8, "FUNC LEAP +3: B8 (%#x)", lit_keys());
    wrel(0);
}

/* P1.3y: SHIFT + REC: the record settings (MODE, QUANTIZE, OVERDUB / REPLACE), the project's; the keys do nothing
 * while it is open; live: 50 % quantize halves the micro-timing, OVERDUB adds a note to a step's */
static void g_rec_settings(void)
{
    int m0, m1;
    start();
    bdown(SHIFT);
    btap(QD_B_REC);
    bup(SHIFT);
    CHECK(qd_view_screen(&A) == QD_SCR_MENU && A.ui.latch == QD_LATCH_REC, "SHIFT + REC: the record settings");
    turn(QD_K_2, 10);
    CHECK(A.proj.rec_dub == QD_TAKE_CAPTURE, "the TAKE: CAPTURE at first");
    turn(QD_K_3, 1);
    CHECK(A.proj.rec_dub == QD_TAKE_REPLACE, "KNOB 3 right: REPLACE");
    turn(QD_K_3, 1);
    turn(QD_K_3, 1);
    CHECK(A.proj.rec_quant == 50 && A.proj.rec_dub == QD_TAKE_OVERDUB, "KNOB 2: QUANTIZE 50 %%; KNOB 3: OVERDUB (the "
          "last)");
    wtap(3);
    black(B2);
    CHECK(!R(0, 3).on && A.ui.hmode == UI_F_NOTE, "open: a W or a black key does nothing");
    btap(QD_B_OCTDN);
    CHECK(!A.ui.latch && qd_view_screen(&A) == QD_SCR_LANE, "OCT-: back to the lane");
    qd_step_on(PAT(), 0, 4);                            /* step 5: C4 (the track's), to overdub on */
    btap(QD_B_REC);                                     /* (the keyboard) */
    live_arm();
    btap(QD_B_PLAY);                                    /* (P1.6g: armed, PLAY starts it) */
    while (track_pos0() < 4.0 + 6.0 / 24.0)   /* step 5 + 6/24 */
        run(1);
    kdown(7 + 7);                                       /* G4 onto step 5 */
    run(30);
    kup(7 + 7);
    run(5);
    m0 = R(0, 4).micro;
    m1 = R(0, 4).nn;
    CHECK(m1 == 2 && R(0, 4).notes[0] == 60 && R(0, 4).notes[1] == 67 && m0 == 0,
          "OVERDUB: G4 joins step 5's C4 (%d notes), its timing kept (micro %d)", m1, m0);
    while (track_pos0() < 8.0 + 6.0 / 24.0)
        run(1);
    kdown(7 + 9);                                       /* A4 onto empty step 9, +6/24 played: 50 %: +3 */
    run(30);
    kup(7 + 9);
    run(5);
    CHECK(R(0, 8).on && R(0, 8).micro >= 2 && R(0, 8).micro <= 4, "QUANTIZE 50 %%: +6/24 played, micro %d (+3)",
          R(0, 8).micro);
    btap(QD_B_REC);
    btap(QD_B_PLAY);
    run(10);
}

/* P1.3y: SHIFT + SEL: the global settings: KNOB 1 page follow (2026-10-25: the layouts B..D gone, A hard-wired);
 * the roles with no finger on a step; a fresh app's latch mode is MODES */
static void g_knob_layouts(void)
{
    int k;
    qd_app_init(&A);
    CHECK(A.ui.lmode == QD_LMODE_MODES, "a fresh app: the latch mode MODES (the owner, 2026-10-25)");
    start();
    bdown(SHIFT);
    bdown(QD_B_PATTERN);
    run(20);
    bup(QD_B_PATTERN);
    bup(SHIFT);
    run(10);
    CHECK(qd_view_screen(&A) == QD_SCR_MENU && A.ui.latch == QD_LATCH_SET, "SHIFT + SEL: the global settings");
    turn(QD_K_1, -1);
    CHECK(!A.ui.follow, "KNOB 1: PAGE FOLLOW off");
    turn(QD_K_1, 1);
    CHECK(A.ui.follow, "KNOB 1: PAGE FOLLOW on");
    CHECK(qd_ctl_layout_role(QD_K_SELECT) == QD_KR_PAGE && qd_ctl_layout_role(QD_K_ALGO) == QD_KR_CURSOR &&
          qd_ctl_layout_role(QD_K_PRESETS) == QD_KR_PRESET && qd_ctl_layout_role(QD_K_1) == QD_KR_NONE,
          "the roles: SELECT the page, ALGORITHM the cursor, PRESETS the preset");
    btap(QD_B_HOME);
    CHECK(!A.ui.latch && qd_view_screen(&A) == QD_SCR_LANE, "HOME: back");
    k = A.ui.page[0];
    turn(QD_K_SELECT, 1);
    CHECK(A.ui.page[0] == k + 1 && !A.ui.slatch, "SELECT: the page (page %d, was %d)", A.ui.page[0] + 1, k + 1);
    turn(QD_K_ALGO, 1);
    CHECK(A.ui.slatch && A.ui.nheld == 1, "ALGORITHM: the cursor (the latch on)");
    btap(QD_B_HOME);
}

/* P1.3y: HOME from SHIFT's map; SEL tapped shows and does nothing; the track colour frame on a default */
static void g_sel_tap(void)
{
    start();
    bdown(QD_B_PATTERN);
    run(60);
    CHECK(qd_view_screen(&A) == QD_SCR_LANE, "SEL tapped (60 ms): no map");
    bup(QD_B_PATTERN);
    run(10);
    CHECK(A.ui.latch == QD_LATCH_TRK && A.sel == 0 && !A.seq.mute, "P1.4b: SEL tapped: the track settings, nothing "
          "else changed");
    btap(QD_B_HOME);
}

/* P1.4a: the sound editor on EDIT: every value of the sound on one page; the groups on B1..B5 (again: the next page);
 * SELECT the pages; nothing held a turn edits the track's sound, a finger on a step locks it (SHIFT: the finest step);
 * B11 clears the page's locks; the latch works; EDIT, OCT- and HOME close it; DRIVE is the sound's (a preset's, a
 * lock's, a voice's) */
static void g_snd(void)
{
    int seen[QD_QUAD_NP], i, k, ok = 1, a0, d0;
    static ui_snd_t sv;
    memset(seen, 0, sizeof seen);
    for (i = 0; i < QD_SND_NPAGE; i++) {               /* (E1: a page's ids and its alt ids; E2: every machine's) */
        int8_t ids[32];
        const int nids = qd_snd_page_all_ids(i, ids);
        for (k = 0; k < nids; k++) {
            seen[ids[k]]++;
            if (ids[k] == QD_QP_RB1)                    /* (P1.5i: RATIO B, the pair: B1 and B2 in one) */
                seen[QD_QP_RB2]++;
            if (ids[k] == QD_QP_MIX && !strcmp(QD_SND_PAGES[i].name, "TONE"))
                seen[QD_QP_MIX]--;                      /* (P1.5n: the one exception: MIX on RATIO and on TONE) */
        }
    }
    for (i = 0; i < QD_QUAD_NP; i++)
        ok &= seen[i] == 1;
    CHECK(ok, "every one of the sound's %d values on exactly one page (RATIO B the pair B1 / B2; E4: MOD's slots)",
          QD_QUAD_NP);
    start();
    wtap(0);
    btap(QD_B_SOUND);
    CHECK(qd_view_screen(&A) == QD_SCR_SOUND && A.ui.latch == QD_LATCH_SND && leds().button[QD_B_SOUND] == QD_LED_LIT &&
              leds().key[BK(B2)] == QD_LED_LIT && leds().key[BK(B1)] == QD_LED_DIM && leds().key[BK(B8)] == QD_LED_DIM &&
              !strcmp(QD_SND_PAGES[A.ui.snd_page].name, "RATIO"),
          "EDIT: the sound editor (latched, EDIT lit), on RATIO (P1.5l): B2 OPERATORS lit, the other groups dim");
    black(B3);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "ENV A"), "B3: OP ENVELOPES, ENV A");
    black(B3);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "ENV B"), "B3 again: its next page, ENV B");
    black(B3), black(B3), black(B3);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "ENV A"), "B3 x3 more: round to ENV A");
    turn(QD_K_SELECT, -1);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "OFFSET"), "SELECT -1: the page before (OFFSET)");
    turn(QD_K_SELECT, 1);
    d0 = A.model.t[0].patch[QD_QP_ADEC];
    turn(QD_K_2, 3);
    CHECK(A.model.t[0].patch[QD_QP_ADEC] == d0 + 3 && A.model.t[0].edited && qd_view_screen(&A) == QD_SCR_SOUND,
          "nothing held, KNOB 2 on ENV A: the track's A DEC +3 (live), the editor stays");
    black(B2);
    a0 = A.model.t[0].patch[QD_QP_ALGO];
    turn(QD_K_ALGO, 1);
    CHECK(A.model.t[0].patch[QD_QP_ALGO] == a0 + 1, "OPERATORS (RATIO): ALGORITHM the algorithm (%d)", a0 + 1);
    black(B5), black(B5);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "LEVEL"), "B5 twice: LEVEL (and DRIVE)");
    turn(QD_K_2, 40);
    CHECK(A.model.t[0].patch[QD_QP_DRIVE] == 40, "KNOB 2: the track's DRIVE 40");
    /* a finger on a step: p-locks */
    whold(0);
    turn(QD_K_2, 20);
    qd_view_snd(&A, &sv);
    CHECK(LK(0, 0, QD_QP_DRIVE) == 60 && A.model.t[0].patch[QD_QP_DRIVE] == 40 && sv.locked[1] && sv.step == 1 &&
              sv.v[1] == 60 && sv.tv[1] == 40 && qd_view_screen(&A) == QD_SCR_SOUND,
          "step 1 held, KNOB 2: DRIVE locked at 60 on it (the track's 40 stays), marked locked, the editor stays");
    black(B4);
    bdown(SHIFT);
    turn(QD_K_1, -3);
    bup(SHIFT);
    CHECK(LK(0, 0, QD_QP_FREQ) == A.model.t[0].patch[QD_QP_FREQ] - 3, "FILTER, SHIFT + KNOB 1: FREQ locked 0.03 down "
          "(the finest step)");
    turn(QD_K_1, -2);
    CHECK(LK(0, 0, QD_QP_FREQ) == A.model.t[0].patch[QD_QP_FREQ] - 203, ".. KNOB 1 -2: two whole units more");
    CHECK(leds().key[BK(B11)] == QD_LED_LIT, "B11 lit: the step has locks on this page");
    black(B11);
    CHECK(LK(0, 0, QD_QP_FREQ) == -99999 && LK(0, 0, QD_QP_DRIVE) == 60, "B11: FILTER's locks cleared, DRIVE's kept");
    wrel(0);
    /* the latch: the selection's turns are locks too, the editor stays */
    btap(LFO);
    CHECK(A.ui.slatch && qd_snd_open(&A), "LFO: the step latch, the editor still open");
    black(B5);
    turn(QD_K_1, -5);
    CHECK(LK(0, A.ui.held[0], QD_QP_EATK) == A.model.t[0].patch[QD_QP_EATK] - 5 || A.model.t[0].patch[QD_QP_EATK] < 5,
          "the latch's step (%d), AMP's KNOB 1: a lock", A.ui.held[0] + 1);
    btap(LFO);
    /* closing */
    btap(QD_B_OCTDN);
    CHECK(A.ui.latch == QD_LATCH_SND && qd_view_screen(&A) == QD_SCR_SOUND, "F1 tapped: open (P1.6h: a main page)");
    btap(QD_B_HOME);
    CHECK(!A.ui.latch, "HOME: closed");
    btap(QD_B_SOUND);
    btap(QD_B_SOUND);
    CHECK(!A.ui.latch, "EDIT again: closed");
    /* DRIVE is the sound's: WOBBLE's 10 comes with it; a voice plays its patch's drive */
    for (k = 1; k <= A.proj.nsound && strcmp(qd_sound_name(&A.proj, k), "WOBBLE"); k++)
        ;
    CHECK(k <= A.proj.nsound && qd_sound_patch(&A.proj, k)[QD_QP_DRIVE] == 10, "the pool's WOBBLE (%d): DRIVE 10", k);
    btap(QD_B_SOUND);
    turn(QD_K_PRESETS, 1);                              /* (P1.5t: the browser, then on to WOBBLE, F2) */
    {
        int list[QD_BR_LIST], n = qd_br_list(&A, QD_BR_ALL, 0, list), i;
        for (i = 0; i < n && list[i] != 10; i++)        /* (WOBBLE: factory preset 10, id 11) */
            ;
        turn(QD_K_PRESETS, i - A.ui.br.pos);
    }
    btap(F2);
    CHECK(A.model.t[0].preset == 11 && A.model.t[0].patch[QD_QP_DRIVE] == 10 && qd_snd_open(&A),
          "the editor, PRESETS: the browser, WOBBLE loaded (F2), its DRIVE 10 with it; back in the editor");
    btap(QD_B_SOUND);
}

/* P1.3i: the tempo and MIDI clock: a turn after a START and STOP works; a running clock shows EXT CLOCK and a turn says
 * so; the clock gone quiet ends it (the turn works again); the keyboard's SELECT the same */
static void clock_pulses(int n, uint32_t ms)
{
    int k;
    for (k = 0; k < n; k++) {
        qd_app_midi(&A, 0xF8, 0, 0);
        run(ms);
    }
}
static void g_ext_clock(void)
{
    int b0;
    start();
    qd_app_midi(&A, 0xFA, 0, 0);                        /* a DAW's START and STOP, no clock */
    run(50);
    qd_app_midi(&A, 0xFC, 0, 0);
    run(50);
    f3_down();
    bup(F2), bup(F1);
    run(10);
    b0 = A.bpm;
    turn(QD_K_SELECT, 4);
    CHECK(A.bpm == b0 + 4 && !A.seq.ext && !A.seq.playing, "after a START and STOP: F3 + SELECT moves the tempo "
          "(%d, now %d)", b0, A.bpm);
    qd_app_midi(&A, 0xFA, 0, 0);                        /* a clock at 100 BPM */
    clock_pulses(96, 25);
    b0 = A.bpm;
    turn(QD_K_SELECT, 4);
    CHECK(qd_seq_ext_running(&A.seq) && b0 >= 97 && b0 <= 102 && A.bpm == b0 && A.ui.cf_on &&
              !strcmp(A.ui.cf_big, "EXT CLOCK") && qd_ctl_layer_shown(&A) == QD_LAYER_F3,
          "a running clock: the tempo is its (%d), a turn shows EXT CLOCK and moves nothing (F3 still open)", b0);
    run(600);                                           /* the clock gone quiet */
    CHECK(!A.seq.ext && !A.seq.playing, "no pulse for 0.6 s: not following, stopped");
    b0 = A.bpm;
    turn(QD_K_SELECT, -3);
    CHECK(A.bpm == b0 - 3, "the turn works again (%d, now %d)", b0, A.bpm);
    btap(F1);
    qd_app_midi(&A, 0xFA, 0, 0);
    clock_pulses(48, 25);
    qd_app_midi(&A, 0xFC, 0, 0);
    run(20);
    CHECK(!A.seq.ext && !A.seq.playing, "STOP: not following");
    btap(QD_B_REC);                                     /* the keyboard: SELECT the tempo */
    b0 = A.bpm;
    turn(QD_K_SELECT, 2);
    CHECK(A.bpm == b0 + 2 && A.ui.tk == UI_TK_TEMPO, "the keyboard, stopped after a clock: SELECT the tempo");
    qd_app_midi(&A, 0xFA, 0, 0);
    clock_pulses(48, 25);
    b0 = A.bpm;
    turn(QD_K_SELECT, 2);
    CHECK(A.bpm == b0 && A.ui.cf_on && !strcmp(A.ui.cf_big, "EXT CLOCK"), "the keyboard, a clock running: EXT CLOCK");
    qd_app_midi(&A, 0xFC, 0, 0);
    run(20);
    btap(QD_B_REC);
}

/* ================================================================ program a beat === */
typedef struct {
    int64_t tick;
    int kind, track, note, vel;
    uint32_t hash;
} ev_t;
static ev_t evs[2][4096];
static int nev[2], which;
static void logger(void *ctx, const qd_plog_t *l)
{
    (void)ctx;
    if (nev[which] < 4096 && (l->kind == QD_LOG_ON || l->kind == QD_LOG_OFF || l->kind == QD_LOG_LOCK ||
                              l->kind == QD_LOG_LEGATO)) {
        ev_t *e = &evs[which][nev[which]++];
        e->tick = l->tick;
        e->kind = l->kind;
        e->track = l->track;
        e->note = l->note;
        e->vel = l->vel;
        e->hash = l->kind == QD_LOG_ON ? l->hash : 0;
    }
}
static void timeline(int w)
{
    which = w;
    nev[w] = 0;
    A.player.log = logger;
    A.player.log_ctx = 0;
    btap(QD_B_PLAY);
    run(8200);                                          /* the drums' 26-step loop twice and more at 120 BPM */
    A.player.log = 0;                                   /* (the stop's releases fall at the press: not compared) */
    btap(QD_B_PLAY);
}

static int beat_chord_n;                                /* the default chord's size before B11 reset it */
static int beat_note1_alone;                            /* P1.3f: SELECT moved note 1 alone */

/* the beat, by hand: only presses and turns */
static void beat_by_panel(void)
{
    int k;
    fresh();
    wake();
    empty_pattern(4);                                   /* A05, empty */
    /* DRUMS (track 1): page 1 */
    for (k = 0; k < 16; k += 4)                         /* 1 5 9 13 */
        wtap(k);
    wtap(14), wtap(14), wtap(14);                       /* step 15 on, off, on: on */
    mode(M_VEL);
    kdown(WK(4));                                       /* 5 and 13: PRESETS = the pool's SNARE (a sound lock) */
    kdown(WK(12));
    run(300);
    turn(QD_K_PRESETS, QD_SND_SNARE);
    kup(WK(4)), kup(WK(12));
    run(10);
    whold(8);                                           /* step 9: velocity 127 (an override) */
    black(B10);
    wrel(8);
    mode(M_COND);                                       /* step 15: 1:2 (A = 1, B = 2) */
    whold(14);
    black(B3);
    black(B8);
    wrel(14);
    bdown(F2);                                          /* P1.3c: F2 cuts step 15 and drops it on 16 */
    run(70);
    wtap(14);
    wtap(15);
    bup(F2);
    run(10);
    black(B11);                                         /* P1.3c: P-LOCK; step 1 held, KNOB 3: a HARM lock */
    whold(0);
    turn(QD_K_3, 2);
    wrel(0);
    mkturn(M_VEL, QD_K_ALGO, -20);                      /* the track's velocity: 80 (VEL key held: the default) */
    /* page 2: the loop 1..2 (F1 + hold B1 + B2), page 2 edited (F1 + tap B2), its range 3..12 (F3 + W3 held + W12) */
    bdown(F1);
    kdown(BK(B1));
    run(30);
    kdown(BK(B2));
    run(30);
    kup(BK(B2)), kup(BK(B1));
    run(10);
    black(B2);
    bup(F1);
    run(10);
    f3_down();
    kdown(WK(2));
    run(20);
    kdown(WK(11));
    run(20);
    kup(WK(11)), kup(WK(2));
    f3_up();
    wtap(2), wtap(6), wtap(10);                         /* steps 19, 23, 27 */
    mode(M_RATCH);                                      /* step 28: an empty step given x4 */
    whold(11);
    black(B4);
    wrel(11);
    /* BASS (track 2): a 12-step loop, notes */
    bdown(QD_B_PATTERN);                                /* SEL + B2: track 2 */
    black(B2);
    bup(QD_B_PATTERN);
    run(10);
    f3_down();
    wtap(11);                                           /* page 1 ends at 12 */
    f3_up();
    wtap(0), wtap(3), wtap(6), wtap(9);
    mode(M_NOTE);
    whold(3);                                           /* step 4: G2 (OCT- twice: octave 2, B8 = 7 semitones) */
    btap(F1), btap(F1);
    black(B8);
    wrel(3);
    mkturn(M_NOTE, QD_K_ALGO, -24);                     /* the track's note: C2 (two octaves down; NOTE key held) */
    /* KEYS (track 3): a C major chord, two steps long */
    bdown(QD_B_PATTERN);                                /* SEL + B3: track 3 */
    black(B3);
    bup(QD_B_PATTERN);
    run(10);
    whold(0);
    btap(F2), btap(F2);                                 /* octave 4 again */
    kdown(BK(B1)), kdown(BK(B5)), kdown(BK(B8));
    run(30);
    kup(BK(B1)), kup(BK(B5)), kup(BK(B8));
    run(10);
    wrel(0);
    mode(M_LEN);
    whold(0);
    black(B6);
    wrel(0);
    mode(M_NOTE);                                       /* step 9: six notes by the keys (C D E G A F), KNOB 4 moves */
    whold(8);                                           /* note 5 alone (A#), P1.3f: SELECT note 1 alone (C#), then */
    kdown(BK(B1)), kdown(BK(B3)), kdown(BK(B5)), kdown(BK(B8)), kdown(BK(B10)), kdown(BK(B6));   /* ALGORITHM +2 */
    run(30);                                            /* moves the chord */
    kup(BK(B1)), kup(BK(B3)), kup(BK(B5)), kup(BK(B8)), kup(BK(B10)), kup(BK(B6));
    run(10);
    turn(QD_K_4, 1);
    turn(QD_K_SELECT, 1);                               /* (P1.5o: SELECT the whole chord) */
    beat_note1_alone = ST(2, 8)->note[0] == 61 && ST(2, 8)->note[1] == 63 && ST(2, 8)->note[4] == 71;
    turn(QD_K_SELECT, 2);
    wrel(8);
    /* P1.3d: knob edits (§8b 2.2) on KEYS: a default by a knob (MICRO, KNOB 2: the strum 2), a step's value by KNOB 1
     * (VEL: step 1 at 90), an extra by KNOB 2 (RATCH: step 9 x2, ramp DOWN); the step latch for one edit (LFO, W5
     * selected, B4 in RATCH: x4; LFO off) */
    mode(M_MICRO);
    turn(QD_K_2, 2);
    mode(M_VEL);
    whold(0);
    turn(QD_K_1, -10);
    wrel(0);
    mode(M_RATCH);
    whold(8);
    turn(QD_K_1, 1);
    turn(QD_K_2, 2);
    wrel(8);
    btap(QD_B_MUTE);                                    /* (it starts on step 9: a tap unselects it, W5 selects) */
    wtap(8);
    wtap(4);
    black(B4);
    btap(QD_B_MUTE);
    /* P1.3y: the cursor: ALGORITHM (no finger on a step) turns the latch on at its last step (5), seven on: step 12;
     * LFO held + B9: FUNC (the latch kept); B8: LEAP +2 (an empty step: a trig); HOME ends the latch */
    turn(QD_K_ALGO, 1);
    turn(QD_K_ALGO, 7);
    bdown(LFO);
    run(10);
    black(B9);
    bup(LFO);
    run(10);
    black(B8);
    btap(QD_B_HOME);
    /* P1.3e: the NOTE key held, a three-note default chord by KNOB 1 / 2, then B11 resets it (one note again); step 9's
     * sixth note on ALGORITHM (D5); the track's velocity on the quiet KNOB 4 (95) */
    kdown(BK(B1));
    run(300);
    turn(QD_K_1, 4);
    turn(QD_K_2, 6);
    beat_chord_n = qd_notes_abs(PAT()->trk[2].def.note, (uint8_t[QD_NNOTE]){0});
    black(B11);
    kup(BK(B1));
    run(10);
    mode(M_VEL);
    turn(QD_K_4, -5);
    bdown(QD_B_PATTERN);                                /* P1.3c: SEL + W6 tapped: track 6 muted */
    wtap(5);
    bup(QD_B_PATTERN);
    run(10);
}

/* the same beat through the API */
static void beat_by_api(void)
{
    qd_pattern_t *p;
    int k;
    static const uint8_t G2 = 43, C2 = 36, CMAJ[3] = {60, 64, 67};
    fresh();
    wake();
    empty_pattern(4);
    p = PAT();
    for (k = 0; k < 16; k += 4)
        qd_step_on(p, 0, k);
    qd_step_on(p, 0, 15);                               /* (step 15, cut and dropped on 16 by F2) */
    qd_step_set_sound(p, 0, 4, QD_SND_SNARE);
    qd_step_set_sound(p, 0, 12, QD_SND_SNARE);
    qd_step_set_vel(p, 0, 8, 127);
    qd_step_set_cond(p, 0, 15, qd_cond_ab(1, 2));
    qd_lock_set(p, 0, 0, QD_QP_HARM, A.model.t[0].patch[QD_QP_HARM] + 200);   /* P-LOCK's KNOB 3 */
    qd_tdef_set(p, 0, QD_PM_VEL, 80);
    qd_page_set_loop(p, 0, 3);
    qd_page_set_range(p, 0, 1, 2, 11);
    qd_step_on(p, 0, 18);
    qd_step_on(p, 0, 22);
    qd_step_on(p, 0, 26);
    qd_step_set_ratchet(p, 0, 27, 4, QD_RAMP_FLAT);
    qd_page_set_range(p, 1, 0, 0, 11);
    qd_step_on(p, 1, 0);
    qd_step_on(p, 1, 3);
    qd_step_on(p, 1, 6);
    qd_step_on(p, 1, 9);
    qd_step_set_notes(p, 1, 3, &G2, 1);
    qd_tdef_set_notes(p, 1, &C2, 1);
    qd_step_on(p, 2, 0);
    qd_step_set_notes(p, 2, 0, CMAJ, 3);
    qd_step_set_len(p, 2, 0, 6);
    {   /* step 9: C D E G A F, A -> A#, C -> C#, then all +2 */
        static const uint8_t SIX[6] = {63, 65, 67, 70, 73, 68};   /* (P1.5o: SELECT +1, +2: all +3) */
        qd_step_set_slots(p, 2, 8, SIX);
    }
    qd_tdef_set(p, 2, QD_PM_STRUM, 2);                  /* P1.3d: the knob edits and the latch's */
    qd_step_set_vel(p, 2, 0, 90);
    qd_step_set_ratchet(p, 2, 8, 2, QD_RAMP_DOWN);
    qd_step_set_ratchet(p, 2, 4, 4, QD_RAMP_FLAT);
    qd_step_on(p, 2, 11);                               /* P1.3y: the cursor's LEAP +2 */
    qd_step_set_func(p, 2, 11, QD_FN_MAKE_LEAP(2));
    qd_tdef_set(p, 2, QD_PM_VEL, 95);                   /* P1.3e: the default velocity 95 */
    A.seq.mute = 0x20;
}

static void t_beat(void)
{
    static qd_pattern_t panel;
    int i, same = 1, t, s, panel_mute;
    beat_by_panel();
    panel = *PAT();
    panel_mute = A.seq.mute;
    timeline(0);
    beat_by_api();
    for (t = 0; t < 3; t++)                             /* the steps, the track settings and the locks equal */
        for (s = 0; s < QD_NSTEP; s++)
            if (memcmp(&panel.step[t][s], ST(t, s), sizeof(qd_step_t)) != 0) {
                CHECK(0, "the beat: track %d step %d differs (panel: on %d ovr %#x note %d vel %d len %d ratch %d cond "
                      "%d sound %d; api: on %d ovr %#x note %d)", t + 1, s + 1, panel.step[t][s].flags,
                      panel.step[t][s].ovr, panel.step[t][s].note[0], panel.step[t][s].vel, panel.step[t][s].len,
                      panel.step[t][s].ratch, panel.step[t][s].cond, panel.step[t][s].sound, ST(t, s)->flags,
                      ST(t, s)->ovr, ST(t, s)->note[0]);
                same = 0;
            }
    for (t = 0; t < QD_PAT_NTRACK; t++)
        if (memcmp(&panel.trk[t], &PAT()->trk[t], sizeof panel.trk[t]) != 0) {
            CHECK(0, "the beat: track %d's settings differ (loop %d / %d, def vel %d / %d, note %d / %d)", t + 1,
                  panel.trk[t].loop, PAT()->trk[t].loop, panel.trk[t].def.vel, PAT()->trk[t].def.vel,
                  panel.trk[t].def.note[0], PAT()->trk[t].def.note[0]);
            same = 0;
        }
    CHECK(panel.nlock == PAT()->nlock && !memcmp(panel.lock, PAT()->lock, panel.nlock * sizeof panel.lock[0]),
          "the beat: the same locks (%d, %d)", panel.nlock, PAT()->nlock);
    CHECK(panel.trk[0].loop == 3 && panel.trk[0].pstart[1] == 2 && panel.trk[0].pend[1] == 11 &&
              qd_track_len(&panel.trk[0]) == 26 && qd_track_len(&panel.trk[1]) == 12,
          "drums: pages 1 and 2 looped, page 2 3..12 (26 steps); bass: 12 steps");
    CHECK(panel_mute == 0x20 && A.seq.mute == panel_mute, "the beat: SEL's mute of track 6 (%#x)", panel_mute);
    timeline(1);
    CHECK(nev[0] > 100 && nev[0] == nev[1], "the beat's timeline: %d events by hand, %d by the API", nev[0], nev[1]);
    for (i = 0; i < nev[0] && i < nev[1]; i++)
        if (memcmp(&evs[0][i], &evs[1][i], sizeof evs[0][i]) != 0) {
            CHECK(0, "the timelines differ at event %d: tick %lld kind %d track %d note %d / tick %lld kind %d track %d "
                  "note %d", i, (long long)evs[0][i].tick, evs[0][i].kind, evs[0][i].track, evs[0][i].note,
                  (long long)evs[1][i].tick, evs[1][i].kind, evs[1][i].track, evs[1][i].note);
            same = 0;
            break;
        }
    {   /* what it holds, as heard */
        int ons = 0, v80 = 0, v127 = 0, rat = 0, page2 = 0, bass36 = 0, bass43 = 0;
        for (i = 0; i < nev[0]; i++) {
            const ev_t *e = &evs[0][i];
            if (e->kind != QD_LOG_ON)
                continue;
            ons++;
            v80 += e->track == 0 && e->vel == 80;
            v127 += e->track == 0 && e->vel == 127;
            rat += e->track == 0 && e->tick % QD_TPS != 0;
            page2 += e->track == 0 && e->tick >= 16 * QD_TPS && e->tick < 26 * QD_TPS;
            bass36 += e->track == 1 && e->note == 36;
            bass43 += e->track == 1 && e->note == 43;
        }
        CHECK(v80 > 8 && v127 >= 2 && rat >= 6 && page2 >= 4 && bass36 > 6 && bass43 >= 2,
              "heard: the default velocity 80 (%d), the override 127 (%d), the ratchet's hits off the grid (%d), "
              "page 2's steps right after page 1 (%d), the bass on its new default C2 (%d) and its own G2 (%d)", v80,
              v127, rat, page2, bass36, bass43);
        CHECK(beat_chord_n == 3 && panel.trk[2].def.note[1] == 0 && panel.step[2][8].note[5] == 68 && beat_note1_alone &&
                  panel.trk[2].def.vel == 95,
              "P1.3e: a default chord by the held NOTE key (%d notes), reset by B11; a six-note step (P1.3f: SELECT "
              "moved note 1 alone, ALGORITHM the chord); the default velocity by the quiet KNOB 4", beat_chord_n);
        CHECK(panel.trk[2].def.strum == 2 && panel.step[2][0].vel == 90 && qd_ratch_ramp(panel.step[2][8].ratch) ==
                  QD_RAMP_DOWN && qd_ratch_count(panel.step[2][4].ratch) == 4 && (panel.step[2][4].flags & QD_SF_ON),
              "P1.3d: a default by KNOB 2 (strum 2), a value by KNOB 1 (90), an extra by KNOB 2 (ramp DOWN), the "
              "step latch's x4 on step 5");
        CHECK(panel.step[0][4].sound == QD_SND_SNARE && panel.step[2][0].note[2] == 67 &&
                  panel.step[0][15].cond == qd_cond_ab(1, 2) && !(panel.step[0][14].flags & QD_SF_ON),
              "a pool sound lock, a chord, a condition (moved by F2 from step 15 to 16)");
        {
            int five = 0;
            for (i = 0; i < nev[0]; i++)
                five += evs[0][i].kind == QD_LOG_ON && evs[0][i].track == 2 && evs[0][i].note == 73;
            CHECK(panel.step[2][8].note[4] == 73 && five >= 1 && LK(0, 0, QD_QP_HARM) != -99999,
                  "the six-note chord (note 5 on KNOB 4, then all +2: C5, heard %d times), the P-LOCK HARM lock", five);
        }
        printf("  beat: by panel presses only, a pattern with page 2's own 3..12, a two-page loop, a pool sound "
               "lock, a velocity override, a ratchet, a condition moved by F2, a chord and a six-note chord (note 1 moved "
               "alone, then the whole chord), a P-LOCK lock, a "
               "12-step track, two default changes, P1.3d's knob edits (a default, a value, an extra) and a step-latch "
               "edit, P1.3e's default chord held and reset, a six-note chord and a quiet KNOB 4, and SEL's mute; "
               "it equals the API-built one byte for byte, and its timeline (%d events, %d notes): %s\n", nev[0], ons,
               same ? "yes" : "NO");
    }
}

/* ================================================================ the LEDs === */
static void t_leds(void)
{
    qd_leds_t l;
    int lit = 0, k, inv = 0;
    start();
    wtap(0);
    mode(M_TRIG);
    whold(1);
    black(B2);                                          /* step 2: LOCK */
    wrel(1);
    mode(M_VEL);
    whold(0);
    black(B10);
    wrel(0);
    f3_down();
    wtap(11);                                           /* page 1: 1..12 */
    f3_up();
    l = leds();
    CHECK(l.key[WK(0)] == QD_LED_LIT && l.key[WK(1)] == QD_LED_DIM && l.key[WK(2)] == QD_LED_FAINT &&
              l.key[WK(12)] == QD_LED_OFF,
          "stopped: a trig lit, a LOCK step dim, an empty one faint, outside the page's range off (%d %d %d %d)",
          l.key[WK(0)], l.key[WK(1)], l.key[WK(2)], l.key[WK(12)]);
    CHECK(l.key[BK(M_VEL)] == QD_LED_LIT && l.key[BK(M_TRIG)] == QD_LED_DIM && l.key[BK(M_NOTE)] == QD_LED_FAINT &&
              l.key[BK(B11)] == QD_LED_FAINT,
          "black: the mode lit, a mode some step overrides dim, the others faint (B11 P-LOCK: no p-locks, faint)");
    whold(0);
    l = leds();
    CHECK(l.key[BK(B10)] == QD_LED_LIT && l.key[BK(B8)] == QD_LED_DIM && l.key[BK(B11)] == QD_LED_LIT,
          "a step held (VEL 127): B10 lit, B11 lit (it overrides VEL); P1.3y: the track's 100 dims B8 (102, nearest)");
    black(B11);
    l = leds();
    CHECK(l.key[BK(B11)] == QD_LED_OFF && l.key[BK(B10)] == QD_LED_FAINT && l.key[BK(B8)] == QD_LED_LIT,
          "B11: back to the track's 100: B11 off, P1.3y: B8 (102) lit, the nearest");
    wrel(0);
    bdown(F1);
    run(20);
    l = leds();
    CHECK(l.key[BK(B1)] == QD_LED_LIT && l.key[BK(B2)] == QD_LED_OFF, "F1: the page edited lit, out of the loop off");
    black(B2);
    l = leds();
    CHECK(l.key[BK(B2)] == QD_LED_LIT && l.key[BK(B1)] == QD_LED_DIM, "F1: page 2 edited lit, page 1 in the loop dim");
    black(B1);
    bup(F1);
    bdown(F2);
    run(20);
    l = leds();
    CHECK(l.key[BK(B1)] == QD_LED_OFF && l.key[BK(B10)] == QD_LED_OFF && l.key[WK(0)] == QD_LED_LIT,
          "F2: the black keys off, the steps as the step view");
    bup(F2);
    f3_down();
    l = leds();
    CHECK(l.key[WK(0)] == QD_LED_LIT && l.key[WK(11)] == QD_LED_LIT && l.key[WK(12)] == QD_LED_OFF &&
              l.key[BK(B5)] == QD_LED_LIT && l.key[BK(B8)] == QD_LED_LIT && l.key[BK(B6)] == QD_LED_FAINT &&
              l.key[BK(B9)] == QD_LED_FAINT,
          "F3: the page's 1..12 lit, the rest off; 1/16 (B5) and normal (B8) lit");
    f3_up();
    btap(QD_B_PLAY);
    for (k = 0; k < 2000; k++) {                        /* the playhead inverts its key */
        const int s = qd_seq_track_step(&A.seq, 0);
        run(1);
        l = leds();
        if (s >= 2 && s < 12 && s == qd_seq_track_step(&A.seq, 0))
            inv += l.key[WK(s)] == QD_LED_LIT;
        lit += l.play_green == QD_LED_LIT;
    }
    CHECK(inv > 500, "playing: the playhead's (empty) key lit (%d ms)", inv);
    CHECK(lit > 200 && lit < 600, "PLAY's green LED on the beat (%d ms of 2000)", lit);
    btap(QD_B_PLAY);
    whold(0);
    {
        int on = 0, off = 0;
        for (k = 0; k < 400; k++) {
            run(1);
            l = leds();
            on += l.key[WK(0)] == QD_LED_LIT;
            off += l.key[WK(0)] == QD_LED_OFF;
        }
        CHECK(on > 100 && off > 100, "a held step blinks fast (%d on, %d off)", on, off);
    }
    wrel(0);
    btap(QD_B_REC);
    l = leds();
    CHECK(l.button[QD_B_REC] == QD_LED_DIM && l.key[7] == QD_LED_80 && l.key[8] == QD_LED_HALF,
          "the keyboard: REC dim (P1.6g), the scale lit (P1.5b: chromatic on C, C 80 %%, the rest 50 %%)");
    btap(QD_B_REC);
    btap(QD_B_PLAY);                                    /* F1: the playing page flashes */
    bdown(F1);
    {
        int on = 0, dim = 0;
        for (k = 0; k < 400; k++) {
            run(1);
            l = leds();
            on += l.key[BK(B1)] == QD_LED_LIT;
            dim += l.key[BK(B1)] == QD_LED_DIM;
        }
        CHECK(on > 120 && dim > 120, "F1: the playing (and edited) page flashes fast (%d lit, %d dim)", on, dim);
    }
    bup(F1);
    btap(QD_B_PLAY);
    printf("  leds: the step view (trigs, ranges, the modes), a held step's values and B11, F1 / F2 / F3, the "
           "playhead, PLAY's beat, the keyboard\n");
}

/* ================================================================ determinism === */
static uint64_t session(void)
{
    beat_by_panel();
    btap(QD_B_PLAY);
    btap(QD_B_REC);
    bdown(SHIFT);
    btap(QD_B_REC);
    bup(SHIFT);
    run(700);
    kdown(9);
    run(300);
    kup(9);
    run(2000);
    return hash ^ (uint64_t)PAT()->step[0][4].note[0] ^ ((uint64_t)PAT()->nlock << 32);
}


/* ================================================================ P1.5b (PLAN version 2.4, the owner after 0.4.1) */
static int key_note_of(int k) { return (A.keys_struck >> k) & 1u ? A.key_note[k] : -1; }
static int play_key(int k)                              /* a key struck and let go: the note it struck (-1 none) */
{
    int n;
    kdown(k);
    run(5);
    n = key_note_of(k);
    kup(k);
    run(5);
    return n;
}
static void set_key(int sc, int rt)                     /* the selected track's own scale and root */
{
    qd_tset_t *ts = &PAT()->trk[A.sel];
    ts->scale = (uint8_t)sc;
    ts->root = (uint8_t)rt;
    ts->key_global = 0;
}

/* 1, 2: the sound editor with the keyboard (REC tapped there): SCALE (W the scale from the root at the keyboard's
 * octave, B the pages) and FULL (every key as the keyboard); F1 / F2 the octave; SHIFT + ALGORITHM / PRESETS the
 * scale / root (the keyboard too), SHIFT + KNOB the fine step; the keys lit by the scale */
static void g_p15b_keys(void)
{
    static const int MAJ[8] = {48, 50, 52, 53, 55, 57, 59, 60}, MINP[6] = {57, 60, 62, 64, 67, 69};
    int i, ok, p0, d0;
    qd_leds_t l;
    start();
    btap(QD_B_SOUND);
    btap(QD_B_REC);
    CHECK(qd_snd_keys(&A) && qd_view_screen(&A) == QD_SCR_SOUND && A.ui.mode == QD_MODE_STEP,
          "EDIT, then REC tapped: the editor stays, its keyboard on");
    set_key(QD_SCALE_MAJOR, 0);
    for (ok = 1, i = 0; i < 8; i++)
        ok &= play_key(WK(i)) == MAJ[i];
    CHECK(ok, "SCALE, C major: W1..W8 play C3 D3 E3 F3 G3 A3 B3 C4");
    CHECK(play_key(WK(14)) == 72 && !R(0, 0).on && !R(0, 1).on, "W15: C5 (two octaves of degrees); no step made");
    set_key(QD_SCALE_MIN_PENTA, 9);
    for (ok = 1, i = 0; i < 6; i++)
        ok &= play_key(WK(i)) == MINP[i];
    CHECK(ok, "SCALE, A minor pentatonic: W1..W6 play A3 C4 D4 E4 G4 A4");
    set_key(QD_SCALE_CHROMATIC, 0);
    for (ok = 1, i = 0; i < 16; i++)
        ok &= play_key(WK(i)) == 48 + i;
    CHECK(ok, "SCALE, chromatic: W1..W16 sixteen semitones from C3");
    p0 = A.ui.snd_page;
    kdown(BK(B4));
    run(5);
    CHECK(key_note_of(BK(B4)) < 0, "SCALE: B4 is no note");
    kup(BK(B4));                                        /* (P1.6d: the FILTERS key acts on its release: a tap) */
    run(5);
    CHECK(QD_SND_PAGES[A.ui.snd_page].group == QD_SG_FILTER && A.ui.snd_page != p0, "SCALE: B4 the FILTERS page");
    btap(F2);
    CHECK(A.oct == 1 && play_key(WK(0)) == 60 && A.ui.latch == QD_LATCH_SND, "F2: the octave up (W1 C4), the editor stays");
    btap(F1);
    l = leds();
    set_key(QD_SCALE_MAJOR, 0);
    l = leds();
    CHECK(l.key[WK(0)] == QD_LED_80 && l.key[WK(1)] == QD_LED_HALF && l.key[WK(7)] == QD_LED_80 &&
              l.key[BK(B4)] == QD_LED_LIT,
          "SCALE's LEDs: the roots 80 %%, the other degrees 50 %%; the black keys the pages (FILTERS lit)");
    kdown(WK(2));
    run(5);
    CHECK(leds().key[WK(2)] == QD_LED_LIT, "a key playing: full");
    kup(WK(2));
    run(5);
    /* FULL */
    A.proj.rec_ekeys = 1;
    CHECK(play_key(0) == 53 && play_key(1) == 53 && play_key(7) == 60, "FULL, C major: F3, F#3 snapped to F3, C4 (the keybed)");
    set_key(QD_SCALE_CHROMATIC, 0);
    p0 = A.ui.snd_page;
    CHECK(play_key(BK(B3)) == 53 + BK(B3) && A.ui.snd_page == p0, "FULL, chromatic: B3 plays its note, the page stays");
    turn(QD_K_SELECT, 1);
    CHECK(A.ui.snd_page == (p0 + 1) % QD_SND_NPAGE, "FULL: the pages on SELECT");
    set_key(QD_SCALE_MAJOR, 2);
    l = leds();
    CHECK(l.key[9] == QD_LED_80 && l.key[11] == QD_LED_HALF && l.key[10] == QD_LED_OFF && l.key[BK(B1)] == QD_LED_HALF,
          "FULL's LEDs, D major: D4 80 %%, E4 50 %%, D#4 dark, F#3 (B1) in the scale 50 %%");
    /* SHIFT + ALGORITHM / PRESETS: the scale / root; SHIFT + KNOB: the fine step */
    bdown(SHIFT);
    run(5);
    turn(QD_K_ALGO, 1);
    turn(QD_K_PRESETS, 3);
    CHECK(PAT()->trk[0].scale == QD_SCALE_MAJOR + 1 && PAT()->trk[0].root == 5 && A.ui.tk == UI_TK_ROOT,
          "the keyboard on, SHIFT + ALGORITHM the scale (+1), SHIFT + PRESETS the root (D + 3 = F)");
    A.ui.snd_page = qd_snd_page_of("ENV A");            /* ENV A: KNOB 2 A DEC */
    d0 = A.model.t[0].patch[QD_QP_ADEC];
    turn(QD_K_2, 1);
    bup(SHIFT);
    run(5);
    CHECK(A.model.t[0].patch[QD_QP_ADEC] == d0 + 1, "SHIFT + KNOB 2: the fine step (A DEC +1)");
    /* EDIT again: back to the keyboard; EDIT from the keyboard: the editor with it */
    btap(QD_B_SOUND);
    CHECK(A.ui.mode == QD_MODE_KEYS && !A.ui.latch, "EDIT again (the keyboard on): back to the keyboard");
    bdown(SHIFT);
    run(5);
    turn(QD_K_ALGO, -1);
    turn(QD_K_PRESETS, -1);
    bup(SHIFT);
    run(5);
    CHECK(PAT()->trk[0].scale == QD_SCALE_MAJOR && PAT()->trk[0].root == 4, "the keyboard: SHIFT + ALGORITHM / PRESETS too");
    l = leds();
    CHECK(l.key[11] == QD_LED_80 && l.key[12] == QD_LED_OFF && l.key[13] == QD_LED_HALF,
          "the keyboard's LEDs, E major: E4 80 %%, F4 dark, F#4 50 %%");
    btap(QD_B_SOUND);
    CHECK(qd_snd_keys(&A), "EDIT from the keyboard: the editor, its keyboard on");
    btap(QD_B_REC);
    CHECK(qd_snd_open(&A) && !qd_snd_keys(&A), "REC again: the keyboard off, the editor stays (W: the steps)");
    wtap(3);
    CHECK(R(0, 3).on, "W4 tapped: a trig again");
    btap(QD_B_HOME);
    A.proj.rec_ekeys = 0;
    btap(QD_B_REC);                                     /* recording live (P1.6g: armed, PLAY): SHIFT + ALGORITHM still
                                                         * the scale */
    live_arm();
    btap(QD_B_PLAY);
    run(50);
    bdown(SHIFT);
    run(5);
    turn(QD_K_ALGO, 2);
    bup(SHIFT);
    run(5);
    CHECK(A.ui.rec && A.seq.playing && PAT()->trk[0].scale == QD_SCALE_MAJOR + 2,
          "recording live: SHIFT + ALGORITHM the scale (no lock)");
    btap(QD_B_PLAY);
    btap(QD_B_PLAY);
    btap(QD_B_HOME);
}

/* 3, 4: the lock chips (the editor's model: locked here filled, somewhere outlined) */
static void g_p15b_chips(void)
{
    static ui_snd_t sv;
    start();
    wtap(2);
    btap(QD_B_SOUND);
    A.ui.snd_page = qd_snd_page_of("FILTER");                                  /* FILTER: KNOB 1 FREQ */
    whold(2);
    turn(QD_K_1, -5);
    qd_view_snd(&A, &sv);
    CHECK(sv.step == 3 && sv.locked[0] && !sv.locked[1] && !sv.lockany[0], "a step held, FREQ locked: its chip filled");
    wrel(2);
    qd_view_snd(&A, &sv);
    CHECK(!sv.step && sv.lockany[0] && !sv.locked[0] && !sv.lockany[1],
          "nothing held: FREQ locked on some step (an outlined chip), RESO not");
    whold(5);
    qd_view_snd(&A, &sv);
    CHECK(sv.step == 6 && !sv.locked[0] && !sv.lockany[0], "another step held (no lock): no chip");
    wrel(5);
    btap(QD_B_HOME);
}

/* 5, 6: FX opens on the OVERVIEW (W1..W8 the track, KNOB 1..3 its sends); B6 the ROUTING (ChoralRoot's: all 0) */
static void g_p15b_fx(void)
{
    int c0, d0, r0;
    qd_leds_t l;
    start();
    btap(QD_B_TRACKS);
    CHECK(QD_FX_PAGES[A.ui.fx_page].fx == QD_FXG_OVER, "FX: the OVERVIEW");
    wtap(2);
    l = leds();
    CHECK(A.sel == 2 && l.key[WK(2)] == QD_LED_LIT && l.key[WK(0)] == QD_LED_DIM && l.key[BK(B1)] == QD_LED_LIT,
          "W3: track 3 (W1..W8 lit by the tracks, B1 OVERVIEW lit)");
    c0 = A.model.t[2].mix.chorus, d0 = A.model.t[2].mix.delay, r0 = A.model.t[2].mix.reverb;
    turn(QD_K_1, 4);
    turn(QD_K_2, 3);
    turn(QD_K_3, -2);
    CHECK(A.model.t[2].mix.chorus == c0 + 4 && A.model.t[2].mix.delay == d0 + 3 && A.model.t[2].mix.reverb == r0 - 2,
          "KNOB 1..3: track 3's CHORUS +4, DELAY +3, REVERB -2");
    CHECK(qd_fx_get(QD_FXP_XDR) == 0 && qd_fx_get(QD_FXP_XCD) == 0 && qd_fx_get(QD_FXP_XCR) == 0 &&
              qd_fx_get(QD_FXP_XPOS) == 0, "the routing at power-on: none (ChoralRoot's chain)");
    black(B6);
    CHECK(!strcmp(QD_FX_PAGES[A.ui.fx_page].name, "ROUTING") && leds().key[BK(B6)] == QD_LED_LIT, "B6: ROUTING");
    turn(QD_K_1, 10);
    turn(QD_K_2, 20);
    turn(QD_K_3, 30);
    turn(QD_K_4, 1);
    CHECK(qd_fx_get(QD_FXP_XDR) == 10 && qd_fx_get(QD_FXP_XCD) == 20 && qd_fx_get(QD_FXP_XCR) == 30 &&
              qd_fx_get(QD_FXP_XPOS) == 1, "KNOB 1..4: DELAY > REVERB, CHORUS > DELAY, CHORUS > REVERB, the filter BEFORE");
    black(B2);
    CHECK(!strcmp(QD_FX_PAGES[A.ui.fx_page].name, "DELAY"), "B2: DELAY");
    btap(QD_B_HOME);
    btap(QD_B_TRACKS);
    CHECK(A.ui.fx_page == 0, "FX again later: the OVERVIEW");
    btap(QD_B_HOME);
    qd_fx_init();
    /* the routes heard: a burst into the chorus alone, its return fed into the delay / the reverb or not */
    {
        int r, pass;
        double e[3];
        for (r = 0; r < 3; r++) {
            int32_t sc[QD_FX_CTL], z[QD_FX_CTL], l2[QD_FX_CTL], r2[QD_FX_CTL], out[2 * QD_FX_CTL];
            qd_fx_init();
            if (r == 1)
                qd_fx_set(QD_FXP_XCD, 127);
            if (r == 2)
                qd_fx_set(QD_FXP_XCR, 127);
            e[r] = 0.;
            for (pass = 0; pass < 1400; pass++) {       /* 1 s */
                int k;
                for (k = 0; k < QD_FX_CTL; k++) {
                    sc[k] = pass < 40 ? (int32_t)(sin((double)(pass * QD_FX_CTL + k) * 0.05) * 8000.) : 0;
                    z[k] = l2[k] = r2[k] = 0;
                }
                qd_fx_block(sc, z, z, l2, r2, out);
                if (pass >= 200)                        /* (after the chorus's own return has gone) */
                    for (k = 0; k < 2 * QD_FX_CTL; k++)
                        e[r] += (double)out[k] * out[k];
            }
        }
        CHECK(e[0] * 1000. < e[1] && e[0] * 1000. < e[2],
              "the chorus alone: next to nothing after it; CHORUS > DELAY: echoes; CHORUS > REVERB: a tail (%g %g %g)", e[0], e[1],
              e[2]);
        qd_fx_init();
    }
}

/* 7: the acceleration curve; spun, a value moves further a detent; an enumeration never; SHIFT never */
static void g_p15b_accel(void)
{
    int v0, i;
    CHECK(qd_knob_accel(1, 200u, 128, 0) == 1 && qd_knob_accel(1, QD_ACC_SLOW_MS, 128, 0) == 1, "slow: x1");
    CHECK(qd_knob_accel(1, 10u, 128, 0) == 8 && qd_knob_accel(-1, 0u, 128, 0) == -8, "spun: a sixteenth of 128 (8)");
    CHECK(qd_knob_accel(1, 67u, 128, 0) > 1 && qd_knob_accel(1, 67u, 128, 0) < 8 &&
              qd_knob_accel(1, 40u, 128, 0) > qd_knob_accel(1, 67u, 128, 0),
          "between: rising with the speed (%d at 67 ms, %d at 40 ms)", qd_knob_accel(1, 67u, 128, 0),
          qd_knob_accel(1, 40u, 128, 0));
    CHECK(qd_knob_accel(1, 0u, 24, 0) == 1 && qd_knob_accel(1, 0u, 16, 0) == 1 && qd_knob_accel(1, 0u, 128, 1) == 1,
          "an enumeration (24 steps or fewer) never; SHIFT never");
    CHECK(qd_knob_accel(4, 40u, 128, 0) == 4 * qd_knob_accel(1, 10u, 128, 0) && qd_knob_accel(3, 200u, 128, 0) == 3,
          "four detents in 40 ms: as fast as one in 10; three after a pause: as they come");
    start();
    btap(QD_B_PAGE);                                    /* the mix: LEVEL */
    v0 = A.model.t[0].mix.level;
    for (i = 0; i < 5; i++) {                           /* five detents 150 ms apart: x1 */
        qd_app_knob(&A, QD_K_1, -1);
        run(150);
    }
    CHECK(A.model.t[0].mix.level == v0 - 5, "the mix's LEVEL turned slowly: a step a detent");
    v0 = A.model.t[0].mix.level;
    for (i = 0; i < 5; i++) {                           /* spun: 8 ms apart */
        qd_app_knob(&A, QD_K_1, -1);
        run(8);
    }
    CHECK(A.model.t[0].mix.level < v0 - 25, "spun: far further (%d -> %d)", v0, A.model.t[0].mix.level);
    btap(QD_B_HOME);
    btap(QD_B_SOUND);                                   /* the editor: FREQ (hundredths, a unit a detent) */
    A.ui.snd_page = qd_snd_page_of("FILTER");
    v0 = A.model.t[0].patch[QD_QP_FREQ];
    for (i = 0; i < 4; i++) {
        qd_app_knob(&A, QD_K_1, -1);
        run(8);
    }
    CHECK(A.model.t[0].patch[QD_QP_FREQ] < v0 - 1000, "the editor's FREQ spun: whole units, accelerated (%d -> %d)", v0,
          A.model.t[0].patch[QD_QP_FREQ]);
    v0 = A.model.t[0].patch[QD_QP_FTYPE];
    for (i = 0; i < 3; i++) {
        qd_app_knob(&A, QD_K_3, 1);
        run(5);
    }
    CHECK(A.model.t[0].patch[QD_QP_FTYPE] <= v0 + 3, "the filter's TYPE (an enumeration) spun: a step a detent");
    btap(QD_B_HOME);
}

/* 8: the latch modes: LFO held alone switches VALUES / MODES (its takeover; LFO lit full / half); in MODES the black
 * keys choose the hold mode and the knobs set the selected steps' values; a finger on a step: the values */
static void g_p15b_latch(void)
{
    int v0;
    start();
    wtap(0);
    wtap(1);
    btap(QD_B_MUTE);                                    /* the latch on (VALUES) */
    CHECK(A.ui.slatch && A.ui.lmode == QD_LMODE_VALUES && leds().button[QD_B_MUTE] == QD_LED_LIT, "LFO: the latch, VALUES, lit full");
    mode(M_VEL);
    CHECK(A.ui.hmode == M_NOTE && R(0, 1).vel != 0, "VALUES: a black key is a value (the mode stays NOTE)");
    bdown(QD_B_MUTE);                                   /* P1.5h: the screen while held, SELECT turns it */
    run(350);
    CHECK(!A.ui.lmode_open, "LFO held 350 ms: no screen yet");
    run(100);
    CHECK(A.ui.lmode_open && A.ui.lmode == QD_LMODE_VALUES, "LFO held 450 ms alone: the latch mode's screen, while held");
    bup(QD_B_MUTE);
    run(10);
    CHECK(!A.ui.lmode_open && A.ui.lmode == QD_LMODE_VALUES && A.ui.slatch,
          "let go without a turn: the screen closes, nothing changed (no toggle), the latch stays");
    bdown(QD_B_MUTE);
    run(450);
    turn(QD_K_SELECT, 1);
    CHECK(A.ui.lmode_open && A.ui.lmode == QD_LMODE_MODES, "SELECT +1 on the screen: MODES at once, the screen stays");
    turn(QD_K_1, 3);
    turn(QD_K_SELECT, -1);
    CHECK(A.ui.lmode == QD_LMODE_VALUES && R(0, A.ui.held[0]).vel == R(0, A.ui.held[0]).vel,
          "SELECT -1: VALUES (KNOB 1 nothing meanwhile)");
    turn(QD_K_SELECT, 1);
    bup(QD_B_MUTE);
    run(10);
    CHECK(A.ui.slatch && A.ui.lmode == QD_LMODE_MODES && !A.ui.lmode_open && leds().button[QD_B_MUTE] >= 153,
          "let go: the screen closes, MODES kept (the latch stays; LFO breathing, P1.5n)");
    mode(M_VEL);
    CHECK(A.ui.hmode == M_VEL, "MODES: B2 chooses VEL");
    v0 = R(0, A.ui.held[0]).vel;
    turn(QD_K_1, -5);
    CHECK(R(0, A.ui.held[0]).vel == v0 - 5, "MODES: KNOB 1 sets the selection's VEL (-5)");
    CHECK(leds().key[BK(B2)] == QD_LED_LIT, "MODES: the black keys light the modes");
    kdown(WK(1));                                        /* a finger on a step: the values */
    run(20);
    black(B10);
    kup(WK(1));
    run(20);
    CHECK(A.ui.hmode == M_VEL && R(0, 1).vel == 127, "a W down: B10 is VEL's value (127), the mode stays");
    bdown(QD_B_MUTE);
    run(100);
    black(B3);
    run(400);
    CHECK(!A.ui.lmode_open, "LFO held + B3 first: no screen after");
    bup(QD_B_MUTE);
    run(10);
    CHECK(A.ui.lmode == QD_LMODE_MODES && A.ui.hmode == M_LEN && A.ui.slatch, "LFO held + B3: LEN, no switch");
    bdown(QD_B_MUTE);
    run(450);
    black(B4);
    CHECK(!A.ui.lmode_open && A.ui.hmode == M_MICRO && A.ui.lmode == QD_LMODE_MODES,
          "the screen up, B4: it closes, MICRO chosen, the latch mode kept");
    bup(QD_B_MUTE);
    run(10);
    btap(QD_B_MUTE);
    CHECK(!A.ui.slatch, "LFO tapped: the latch off");
    bdown(QD_B_MUTE);
    run(450);
    turn(QD_K_SELECT, -2);
    bup(QD_B_MUTE);
    run(10);
    CHECK(A.ui.lmode == QD_LMODE_VALUES && !A.ui.slatch, "LFO held, SELECT down: VALUES (the latch stays off)");
    btap(QD_B_HOME);
}


/* ================================================================ P1.5d (PLAN version 2.5): the frames the other way
 * round: a held step's screen (and the latch's selection's) framed in 0xf03cff, a track default not; the sound editor
 * dark orange (the track's sound), bright orange (a step's locks) */
static uint32_t edge_px(void)                           /* the left edge's middle pixel now, as 0xRRGGBB (565) */
{
    static uint16_t lcd[QD_W * QD_H];
    const uint8_t *b = (const uint8_t *)lcd;
    uint16_t v;
    qd_app_frame(&A, lcd);
    v = (uint16_t)(b[2 * (120 * QD_W + 1)] << 8 | b[2 * (120 * QD_W + 1) + 1]);
    return (uint32_t)((v >> 11) & 31u) << 19 | (uint32_t)((v >> 5) & 63u) << 10 | (uint32_t)(v & 31u) << 3;
}
static int near_col(uint32_t a, uint32_t b)            /* within 24 a channel (565, the frame's soft edge) */
{
    int k;
    for (k = 0; k < 24; k += 8) {
        const int d = (int)((a >> k) & 255u) - (int)((b >> k) & 255u);
        if (d > 24 || d < -24)
            return 0;
    }
    return 1;
}
static void g_p15d_frames(void)
{
    start();
    wtap(2);
    mode(M_VEL);
    CHECK(edge_px() == 0, "the lane: no frame");
    whold(2);
    CHECK(qd_view_screen(&A) == QD_SCR_HELD && near_col(edge_px(), UI_STEP_FRAME), "a step held (VEL): the frame (%06x)",
          (unsigned)edge_px());
    wrel(2);
    kdown(BK(B2));                                      /* the VEL key held: the track's default */
    run(300);
    CHECK(qd_view_screen(&A) == QD_SCR_DEFAULT && edge_px() == 0, "a mode key held (the track default): no frame");
    kup(BK(B2));
    run(20);
    shome(), shome();                                   /* EDIT */
    CHECK(A.ui.view == QD_VIEW_EDIT && edge_px() == 0, "EDIT, nothing held (the default): no frame");
    btap(QD_B_MUTE);
    run(20);
    CHECK(A.ui.slatch && A.ui.nheld && near_col(edge_px(), UI_STEP_FRAME), "EDIT, the latch's selection: the frame (%d %d %06x)",
          A.ui.slatch, A.ui.nheld, (unsigned)edge_px());
    btap(QD_B_MUTE);
    btap(QD_B_HOME);
    shome();
    btap(QD_B_SOUND);
    CHECK(near_col(edge_px(), UI_SND_FRAME_TRACK), "the editor, the track's sound: dark orange (%06x)", (unsigned)edge_px());
    whold(2);
    CHECK(near_col(edge_px(), UI_SND_FRAME_STEP), "the editor, a step held: bright orange (%06x)", (unsigned)edge_px());
    wrel(2);
    btap(QD_B_HOME);
}


/* P1.5f: VEL 127 is the stunt show (its buses in the tracks' colours on the road), 126 the wheelie */
static int bus_seen(void)                               /* over one loop (2.6 s): track 1's colour on the road's row */
{
    static uint16_t lcd[QD_W * QD_H];
    const uint8_t *b = (const uint8_t *)lcd;
    int f, x, y;
    for (f = 0; f < 130; f++) {
        run(20);
        qd_app_frame(&A, lcd);
        for (y = 186; y < 204; y++)
            for (x = 0; x < QD_W; x++) {
                const uint16_t v = (uint16_t)(b[2 * (y * QD_W + x)] << 8 | b[2 * (y * QD_W + x) + 1]);
                const uint32_t c = (uint32_t)((v >> 11) & 31u) << 19 | (uint32_t)((v >> 5) & 63u) << 10 |
                                   (uint32_t)(v & 31u) << 3;
                if (near_col(c, UI_TRACK_PAL[0]))
                    return 1;
            }
    }
    return 0;
}
/* P1.5f / P1.5g: VEL 127 is the stunt show (the buses scroll under the bike: track 1's at some point of the loop),
 * 126 the wheelie (none) */
static void g_p15f_jump(void)
{
    int s126, s127;
    start();
    wtap(4);
    mode(M_VEL);
    qd_step_set_vel(PAT(), 0, 4, 126);
    whold(4);
    s126 = bus_seen();
    qd_step_set_vel(PAT(), 0, 4, 127);
    s127 = bus_seen();
    wrel(4);
    CHECK(!s126 && s127, "VEL 126: the wheelie (no bus); VEL 127: the show, track 1's bus scrolls past");
}


/* P1.5i: the RATIO page: KNOB 3 RATIO B, ChoralRoot's pair (B1 x 19 + B2, a detent B2 carrying into B1, SHIFT B1),
 * KNOB 4 MIX (moved from TONE: its KNOB 4 empty); a step held locks both halves */
static void g_p15i_ratio(void)
{
    static ui_snd_t sv;
    int16_t *pt;
    start();
    btap(QD_B_SOUND);
    A.ui.snd_page = qd_snd_page_of("RATIO");
    pt = A.model.t[0].patch;
    qd_player_live_edit(&A.player, 0, QD_QP_RB1, 3);
    qd_player_live_edit(&A.player, 0, QD_QP_RB2, 3);
    turn(QD_K_3, 1);
    qd_view_snd(&A, &sv);
    CHECK(pt[QD_QP_RB1] == 3 && pt[QD_QP_RB2] == 4 && sv.pair[2] && !strcmp(sv.txt[2], "2.00") &&
              !strcmp(sv.den[2], "1.00") && !strcmp(sv.lab[2], "RATIO B"),
          "RATIO, KNOB 3: RATIO B 1.00/1.00 -> 2.00 over 1.00 (B2 the fast hand)");
    qd_player_live_edit(&A.player, 0, QD_QP_RB2, 18);
    turn(QD_K_3, 1);
    CHECK(pt[QD_QP_RB1] == 4 && pt[QD_QP_RB2] == 0, "B2 at 16.00, +1: carries into B1 (2.00), B2 0.25");
    bdown(SHIFT);
    turn(QD_K_3, -2);
    bup(SHIFT);
    CHECK(pt[QD_QP_RB1] == 2 && pt[QD_QP_RB2] == 0, "SHIFT + KNOB 3: B1 two steps (the slow hand), B2 kept");
    turn(QD_K_4, 20);
    qd_view_snd(&A, &sv);
    CHECK(pt[QD_QP_MIX] == -64 + 20 && sv.has_mix && sv.mix == -44 && !strcmp(sv.lab[3], "MIX"),
          "KNOB 4: MIX (-64 -> -44), the drawing's balance");
    A.ui.snd_page = qd_snd_page_of("TONE");
    qd_view_snd(&A, &sv);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "TONE") && sv.has[3] && sv.has_mix && sv.mix == -44 &&
              sv.has[0] && sv.has[1] && sv.has[2],
          "TONE: HARM, DTUNE, FDBK, and (P1.5n) MIX again: the same value as RATIO's (-44)");
    A.ui.snd_page = qd_snd_page_of("RATIO");
    wtap(3);
    whold(3);
    turn(QD_K_3, 1);
    qd_view_snd(&A, &sv);
    CHECK(LK(0, 3, QD_QP_RB1) == 2 && LK(0, 3, QD_QP_RB2) == 1 && pt[QD_QP_RB2] == 0 && sv.locked[2],
          "a step held, KNOB 3: both halves locked (B1 2, B2 1), the track's stay");
    wrel(3);
    btap(QD_B_HOME);
}


/* P1.5j: the editor's foot inside the frame: nothing lit in the 4 px between the frame's inner edge (3) and the content,
 * on every page, a step held with every value locked (filled chips) and nothing held (outlined) */
static int margin_clear(void)
{
    static uint16_t lcd[QD_W * QD_H];
    const uint8_t *b = (const uint8_t *)lcd;
    int x, y;
    qd_app_frame(&A, lcd);
    for (y = 3; y < QD_H - 3; y++)
        for (x = 3; x < QD_W - 3; x++) {
            const int in_band = x < 7 || x >= QD_W - 7 || y < 7 || y >= QD_H - 7;
            uint16_t v;
            if (!in_band || ((x < 9 || x >= QD_W - 9) && (y < 9 || y >= QD_H - 9)))   /* (the frame's round corners) */
                continue;
            v = (uint16_t)(b[2 * (y * QD_W + x)] << 8 | b[2 * (y * QD_W + x) + 1]);
            if (((v >> 11) & 31u) > 5u || ((v >> 5) & 63u) > 10u || (v & 31u) > 5u) {
                if (getenv("MARGIN"))
                    printf("    (the first pixel in the margin: %d, %d)\n", x, y);
                return 0;
            }
        }
    return 1;
}
static void g_p15j_margins(void)
{
    int pg, ok = 1, bad = -1;
    start();
    wtap(2);
    btap(QD_B_SOUND);
    for (pg = 0; pg < QD_SND_NPAGE; pg++) {
        int k;
        A.ui.snd_page = pg;
        whold(2);
        for (k = QD_K_1; k <= QD_K_4; k++)
            turn(k, 1);
        turn(QD_K_ALGO, 1);
        run(20);
        if (!margin_clear()) {
            printf("    (margin: page %d, held)\n", pg);
            if (bad < 0)
                bad = pg;
            ok = 0;
        }
        wrel(2);
        run(20);
        if (!margin_clear()) {
            printf("    (margin: page %d, nothing held)\n", pg);
            if (bad < 0)
                bad = pg + 100;
            ok = 0;
        }
    }
    CHECK(ok, "the editor's 18 pages, locked and not: nothing in the frame's 4 px margin (first bad: %d)", bad);
    btap(QD_B_HOME);
}


/* P1.5k: the LFOs in the editor: B6..B8 their pages (again: WAVE, again back); DEST on the LFO page (KNOB 4, ChoralRoot's
 * list in its order); an LFO key held + a knob: that value LFO n's destination, the depth from 0 (or on from the depth
 * it had), not a destination: nothing; the release stays on the page; a step held: the step's locks */
static void g_p15k_lfo(void)
{
    static ui_snd_t sv;
    const int16_t *pt;
    int fr, rs, did2, pid2;
    start();
    wtap(2);
    btap(QD_B_SOUND);
    pt = A.model.t[0].patch;
    black(B6);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "LFO 1") && leds().key[BK(B6)] == QD_LED_LIT &&
              leds().key[BK(B7)] == QD_LED_DIM, "B6: LFO 1 (B6 lit, B7 dim)");
    black(B6);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "LFO 1 WAVE"), "B6 again: LFO 1 WAVE");
    black(B6);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "LFO 1"), "B6 again: back to LFO 1");
    black(B8);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "LFO 3") && QD_SND_PAGES[A.ui.snd_page].id[3] ==
              QD_QP_LFO(2, QD_QL_DEST) && QD_SND_PAGES[A.ui.snd_page + 1].id[3] == QD_QP_LFO(2, QD_QL_FADE),
          "B8: LFO 3: KNOB 4 DEST; its WAVE page's KNOB 4 FADE");
    {
        const int d0 = pt[QD_QP_LFO(2, QD_QL_DEST)];
        turn(QD_K_4, 1);
        qd_view_snd(&A, &sv);
        CHECK(pt[QD_QP_LFO(2, QD_QL_DEST)] == qd_quad_dest_step(d0, 1) && !strcmp(sv.txt[3],
              qd_quad_dest_name(qd_quad_dest_step(d0, 1))), "KNOB 4: DEST the next of ChoralRoot's list (%s)", sv.txt[3]);
        turn(QD_K_4, -1);
    }
    A.ui.snd_page = qd_snd_page_of("FILTER");                                  /* FILTER: FREQ, RESO, TYPE, DEPTH */
    fr = qd_quad_dest_of(QD_QP_FREQ, 1), rs = qd_quad_dest_of(QD_QP_RESO, 1);
    did2 = QD_QP_LFO(1, QD_QL_DEST), pid2 = QD_QP_LFO(1, QD_QL_DEPTH);
    kdown(BK(B7));
    run(300);
    turn(QD_K_1, 3);
    qd_view_snd(&A, &sv);
    CHECK(fr > 0 && pt[did2] == fr && pt[pid2] == 300 && sv.lk == 2 && sv.lk_ok && sv.lk_k == 0,
          "B7 held + KNOB 1 (FREQ): LFO 2 -> FREQ, depth +3.00 (the takeover)");
    turn(QD_K_1, -1);
    CHECK(pt[pid2] == 200, "the same destination turned on: the depth from where it was (+2.00)");
    turn(QD_K_2, -2);
    CHECK(pt[did2] == rs && pt[pid2] == -200, "KNOB 2 (RESO): LFO 2 -> RESO, the depth from 0 (-2.00)");
    turn(QD_K_3, 1);
    qd_view_snd(&A, &sv);
    CHECK(pt[did2] == rs && pt[pid2] == -200 && !sv.lk_ok && qd_quad_dest_of(QD_QP_FTYPE, 1) < 0,
          "KNOB 3 (TYPE, no destination): nothing changes, the takeover refuses");
    kup(BK(B7));
    run(20);
    qd_view_snd(&A, &sv);
    CHECK(A.ui.snd_page == qd_snd_page_of("FILTER") && !sv.lk && (sv.lfomark[1] & 2u), "released: the page stays (FILTER), RESO marked LFO 2");
    black(B7);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "LFO 2"), "B7 tapped: LFO 2's page");
    A.ui.snd_page = qd_snd_page_of("FILTER");
    whold(2);                                           /* a step held: the step's locks */
    kdown(BK(B6));
    run(20);
    turn(QD_K_1, 2);
    kup(BK(B6));
    run(20);
    CHECK(LK(0, 2, QD_QP_LFO(0, QD_QL_DEST)) == qd_quad_dest_of(QD_QP_FREQ, 0) &&
              LK(0, 2, QD_QP_LFO(0, QD_QL_DEPTH)) == 200 && pt[QD_QP_LFO(0, QD_QL_DEST)] != qd_quad_dest_of(QD_QP_FREQ, 0),
          "a step held, B6 + KNOB 1: LFO 1 -> FREQ and its depth locked on the step, the track's sound untouched");
    wrel(2);
    btap(QD_B_HOME);
}


/* E4: the modulation slots: B1 held + a knob maps the chosen source (W1..W8 while held; VEL at first) to the value's
 * destination in a slot (the same source and destination's, else the first empty one), each detent its depth; refused:
 * no destination, every slot taken; a step held: locks; B1 tapped: its pages; the MOD page; MIDI's sources */
static void g_e4_mods(void)
{
    static ui_snd_t sv;
    const int16_t *pt;
    int fr, rs, k, ok;
#define MS(k, f) pt[QD_QP_MOD(k, QD_QM_##f)]
    start();
    wtap(2);
    btap(QD_B_SOUND);
    pt = A.model.t[0].patch;
    fr = qd_quad_dest_of(QD_QP_FREQ, 1), rs = qd_quad_dest_of(QD_QP_RESO, 1);
    A.ui.snd_page = qd_snd_page_of("FILTER");
    kdown(BK(B1));
    run(300);
    qd_view_snd(&A, &sv);
    CHECK(sv.lk == 4 && sv.lk_src == QD_MS_VEL && leds().key[WK(0)] == QD_LED_LIT && leds().key[WK(1)] == QD_LED_DIM &&
              leds().key[WK(8)] == QD_LED_OFF, "B1 held (past a tap): the mapping, VEL chosen (W1 lit, W2..W8 dim)");
    turn(QD_K_1, 3);
    qd_view_snd(&A, &sv);
    CHECK(MS(0, SRC) == QD_MS_VEL && MS(0, DST) == fr && MS(0, DEPTH) == 3 && sv.lk_ok && sv.lk_k == 0,
          "B1 + KNOB 1 (FREQ): slot 1 VEL -> FREQ, depth +3 (%d %d %d)", MS(0, SRC), MS(0, DST), MS(0, DEPTH));
    turn(QD_K_1, -1);
    CHECK(MS(0, DEPTH) == 2 && !MS(1, SRC), "the same again: slot 1's depth from where it was (+2), no other slot");
    wtap(2);
    qd_view_snd(&A, &sv);
    CHECK(A.ui.mod_src == QD_MS_MW && sv.lk_src == QD_MS_MW && leds().key[WK(2)] == QD_LED_LIT && A.ui.snd_lk == 4,
          "W3 while B1 held: MW the source (W3 lit), no step");
    turn(QD_K_2, -2);
    turn(QD_K_1, 1);
    CHECK(MS(1, SRC) == QD_MS_MW && MS(1, DST) == rs && MS(1, DEPTH) == -2 && MS(2, SRC) == QD_MS_MW &&
              MS(2, DST) == fr && MS(2, DEPTH) == 1 && MS(0, DEPTH) == 2,
          "MW + KNOB 2: slot 2 MW -> RESO -2; MW + KNOB 1: slot 3 MW -> FREQ (VEL -> FREQ's slot kept)");
    turn(QD_K_3, 1);
    qd_view_snd(&A, &sv);
    CHECK(!sv.lk_ok && !MS(3, SRC), "KNOB 3 (TYPE, no destination): refused, no slot taken");
    kup(BK(B1));
    run(20);
    qd_view_snd(&A, &sv);
    CHECK(A.ui.snd_page == qd_snd_page_of("FILTER") && !sv.lk && sv.modmark[0] && sv.modmark[1],
          "released: the page stays (FILTER), FREQ and RESO marked");
    for (k = 3; k < 8; k++)                             /* the other five slots taken */
        qd_player_live_edit(&A.player, 0, QD_QP_MOD(k, QD_QM_SRC), QD_MS_AT),
            qd_player_live_edit(&A.player, 0, QD_QP_MOD(k, QD_QM_DST), qd_quad_dest_of(QD_QP_HARM, 1));
    kdown(BK(B1));
    run(20);
    turn(QD_K_4, 1);
    qd_view_snd(&A, &sv);
    ok = !sv.lk_ok;
    turn(QD_K_1, 1);
    qd_view_snd(&A, &sv);
    CHECK(ok && sv.lk_ok && MS(2, DEPTH) == 2, "every slot taken: a new destination refused, MW -> FREQ still turned (+2)");
    kup(BK(B1));
    run(20);
    black(B1);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "TRIG"), "B1 tapped: its pages (TRIG)");
    black(B1), black(B1), black(B1);
    qd_view_snd(&A, &sv);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "MOD") && sv.mod_src[0] == QD_MS_VEL && sv.mod_dep[0] == 2 &&
              sv.mod_src[1] == QD_MS_MW && sv.mod_dep[1] == -2 && sv.mod_sel == 2 && sv.mod_dst[0] &&
              !strcmp(sv.mod_dst[0], "FREQ"), "MOD: the slots' rows, the last mapped slot chosen (3)");
    turn(QD_K_ALGO, -1);
    turn(QD_K_3, 5);
    CHECK(A.ui.mod_slot == 1 && MS(1, DEPTH) == 3, "MOD: ALGORITHM the slot (2), its DEPTH turned (-2 -> +3)");
    btap(QD_B_HOME);
    /* a step held: the slot locked on the step, the track's sound untouched */
    start();
    wtap(2);
    btap(QD_B_SOUND);
    pt = A.model.t[0].patch;
    A.ui.snd_page = qd_snd_page_of("FILTER");
    whold(2);
    kdown(BK(B1));
    run(20);
    turn(QD_K_1, 2);
    kup(BK(B1));
    run(20);
    CHECK(LK(0, 2, QD_QP_MOD(0, QD_QM_SRC)) == QD_MS_VEL && LK(0, 2, QD_QP_MOD(0, QD_QM_DST)) == fr &&
              LK(0, 2, QD_QP_MOD(0, QD_QM_DEPTH)) == 2 && !MS(0, SRC),
          "a step held, B1 + KNOB 1: slot 1 VEL -> FREQ +2 locked on the step, the track's sound untouched");
    wrel(2);
    btap(QD_B_HOME);
    /* MIDI's sources: channel n's CC 1, CC 2, pressure, pitch bend into track n's state */
    qd_app_midi(&A, 0xB0, 1, 99);
    qd_app_midi(&A, 0xB1, 2, 77);
    qd_app_midi(&A, 0xD0, 55, 0);
    qd_app_midi(&A, 0xE2, 0, 0);
    CHECK(A.model.t[0].owner.mw == 99 && A.model.t[1].owner.br == 77 && A.model.t[0].owner.at == 55 &&
              A.model.t[2].owner.pb == -8192 && A.model.t[1].owner.mw == 0, "MIDI: CC 1, CC 2, pressure, bend: the "
          "channel's track's (%d %d %d %d)", A.model.t[0].owner.mw, A.model.t[1].owner.br, A.model.t[0].owner.at,
          A.model.t[2].owner.pb);
    qd_app_midi(&A, 0xE2, 0x7F, 0x7F);
    CHECK(A.model.t[2].owner.pb == 8191, "bend at the top: +8191");
#undef MS
}

/* E4.1 (the owner): FILTER's ALGORITHM turns the machines in the Digitone II's menu order, MULTI, LOWPASS 4, LEGACY,
 * COMB-, COMB+, EQ (the stored FTYPE as before); LEGACY lands on LP12, its TYPE on KNOB 3 */
static void g_e41_machines(void)
{
    static const char *const WANT[6] = {"MULTI", "LOWPASS 4", "LEGACY", "COMB-", "COMB+", "EQ"};
    static const int FT[6] = {QD_QF_MULTI, QD_QF_LP24, QD_QF_LP12, QD_QF_COMBN, QD_QF_COMBP, QD_QF_EQ};
    const int16_t *pt;
    int k, ok = 1;
    start();
    btap(QD_B_SOUND);
    pt = A.model.t[0].patch;
    A.ui.snd_page = qd_snd_page_of("FILTER");
    turn(QD_K_ALGO, -1), turn(QD_K_ALGO, -1), turn(QD_K_ALGO, -1);
    for (k = 0; k < 6; k++) {
        ok &= pt[QD_QP_FTYPE] == FT[k] && !strcmp(QD_SND_MACHINE[qd_snd_machine(pt[QD_QP_FTYPE])], WANT[k]);
        turn(QD_K_ALGO, 1);
    }
    CHECK(ok && pt[QD_QP_FTYPE] == QD_QF_EQ, "ALGORITHM: MULTI, LOWPASS 4, LEGACY, COMB-, COMB+, EQ (and stops)");
    for (k = 0; k < 3; k++)
        turn(QD_K_ALGO, -1);
    turn(QD_K_3, 1);
    CHECK(pt[QD_QP_FTYPE] == QD_QF_HP12, "LEGACY: KNOB 3 its TYPE (HP12)");
    turn(QD_K_ALGO, 1);
    CHECK(pt[QD_QP_FTYPE] == QD_QF_COMBN, "LEGACY, then COMB-");
    turn(QD_K_ALGO, -1);
    CHECK(pt[QD_QP_FTYPE] == QD_QF_LP12, "back to LEGACY: LP12, its default");
    btap(QD_B_HOME);
}

/* P1.6c: the MIDI FX screen: SHIFT + ARP; ALGORITHM the track's group; PRESETS the effect; the pages, the arp's lanes;
 * B10 bypass, B11 remove (twice) and its undo; F1 + B copy, F2 + B move; F1 tapped closes; the keys through the group;
 * the LEDs and the labels */
static void g_p16c_screen(void)
{
    static ui_mfx_t v;
    qd_labels_t lb;
    qd_mfx_group_t *G;
    start();
    btap(QD_B_KEYS);
    qd_view_mfx(&A, &v);
    CHECK(qd_view_screen(&A) == QD_SCR_MFX && !v.g && leds().key[BK(B1)] == QD_LED_OFF && !A.seq.fill,
          "ARP tapped (P1.6d): the MIDI FX screen, track 1 OFF (the slots dark), no FILL");
    turn(QD_K_ALGO, 1);
    qd_view_mfx(&A, &v);
    G = &A.proj.mfx[0];
    CHECK(PAT()->trk[0].mfx == 1 && v.g == 1 && v.gsel && v.ntr == 1 && v.tr[0] == 1,
          "ALGORITHM: track 1 on group 1 (the groups' choice up; its square)");
    run(QD_TK_MS + 10);
    turn(QD_K_PRESETS, 1);
    qd_view_mfx(&A, &v);
    CHECK(G->slot[0].type == QD_MFX_ARP && G->slot[0].v[QD_MA_MODE] == QD_AM_ON && v.roll && v.has[0] &&
              !strcmp(v.lab[0], "PATTERN") && !strcmp(v.val[0], "UP") && !strcmp(v.lab[3], "CHANCE"),
          "PRESETS: slot 1 an ARP (ON); the foot PATTERN UP .. CHANCE");
    turn(QD_K_1, 2);
    qd_view_mfx(&A, &v);
    CHECK(G->slot[0].v[QD_MA_PAT] == QD_AP_UPDN && !strcmp(v.val[0], "UP DN") && v.last == 0, "KNOB 1: PATTERN UP DN");
    black(B1);
    CHECK(A.ui.mfx_page == 1, "B1 again: page 2");
    turn(QD_K_SELECT, 1);
    CHECK(A.ui.mfx_page == 2 && qd_mfx_lane_page(&A) == 1, "SELECT: page 3, the MOD lane");
    wtap(4);
    turn(QD_K_1, 3);
    turn(QD_K_4, -4);
    CHECK(A.ui.mfx_lane[0] == 4 && G->slot[0].lane[0][4] == QD_AL_REPEAT && G->slot[0].v[QD_MA_MLEN] == 12 &&
              A.ui.mfx_slot == 0, "the lane: W5 its step, KNOB 1 its value (R), KNOB 4 the length (12); slot 1 still");
    CHECK(leds().key[WK(11)] == QD_LED_DIM && leds().key[WK(12)] == QD_LED_OFF,
          "the lane's LEDs: its length dim, beyond it off");
    {
        int k, ch = 0;
        for (k = 0; k < 6; k++) {
            const int8_t was = G->slot[0].lane[0][7];
            bdown(SHIFT), wtap(7), bup(SHIFT);
            ch += G->slot[0].lane[0][7] != was;
        }
        CHECK(ch >= 3, "SHIFT + W8: a random value (changed %d of 6)", ch);
    }
    turn(QD_K_SELECT, 1);
    CHECK(A.ui.mfx_page == 3 && qd_mfx_lane_page(&A) == 2, "SELECT: page 4, the TRANSPOSE lane");
    wtap(1);
    turn(QD_K_1, 7);
    CHECK(G->slot[0].lane[1][1] == 7, "KNOB 1: the step's offset +7");
    black(B2);
    turn(QD_K_PRESETS, 2);
    turn(QD_K_1, -6);
    CHECK(A.ui.mfx_slot == 1 && G->slot[1].type == QD_MFX_CHANCE && G->slot[1].v[QD_MC_CHANCE] == 70,
          "B2, PRESETS +2: a CHANCE in slot 2; KNOB 1: 70 %%");
    black(B10);
    qd_view_mfx(&A, &v);
    CHECK(G->slot[1].bypass && v.byp_sel && leds().key[BK(B10)] == QD_LED_OFF, "B10: bypassed (B10 dark)");
    black(B10);
    CHECK(!G->slot[1].bypass && leds().key[BK(B10)] == QD_LED_LIT, "B10 again: playing (lit)");
    run(QD_UNDO_QUIET_MS + 10);
    black(B11);
    qd_view_mfx(&A, &v);
    CHECK(G->slot[1].type == QD_MFX_CHANCE && v.arm > 0.f, "B11: armed (the red X), nothing removed yet");
    black(B11);
    CHECK(G->slot[1].type == QD_MFX_NONE, "B11 again: removed");
    run(QD_UNDO_QUIET_MS + 10);
    bdown(SHIFT), btap(F1), bup(SHIFT), run(10);
    CHECK(G->slot[1].type == QD_MFX_CHANCE && G->slot[1].v[QD_MC_CHANCE] == 70, "SHIFT + F1: the removal undone");
    CHECK(qd_view_screen(&A) == QD_SCR_MFX, "(the screen stays)");
    bdown(F1), run(10), black(B2), black(B4), black(B6), bup(F1), run(10);
    CHECK(G->slot[3].type == QD_MFX_CHANCE && G->slot[1].type == QD_MFX_CHANCE && G->slot[5].type == QD_MFX_CHANCE &&
              A.ui.mfx_slot == 5 && qd_view_screen(&A) == QD_SCR_MFX,
          "P1.6d: F1 + B2 copies, B4 and B6 pasted (F1's release: no close)");
    bdown(F2), run(10), black(B6);                      /* (the hand: F1's copy loaded it) */
    CHECK(G->slot[5].type == QD_MFX_CHANCE && !A.ui.mfx_f2_holding, "F2 + B6: F1's copy pasted (Form's: the copy "
          "loads the hand)");
    black(B1);
    CHECK(G->slot[0].type == QD_MFX_NONE && A.ui.mfx_f2_holding, ".. B1 (full, empty-handed): cut into the hand");
    black(B3);
    bup(F2), run(10);
    CHECK(G->slot[0].type == QD_MFX_NONE && G->slot[1].type == QD_MFX_CHANCE && G->slot[2].type == QD_MFX_ARP &&
              A.ui.mfx_slot == 2, "F2 + B3 (empty): the ARP dropped there");
    qd_ctl_labels(&A, &lb);
    CHECK(!strcmp(lb.s[QD_LBL_B0 + 2], "ARP") && !strcmp(lb.s[QD_LBL_KNOB0 + QD_K_ALGO], "GROUP") &&
              !strcmp(lb.s[QD_LBL_KNOB0 + QD_K_PRESETS], "EFFECT") && !strcmp(lb.s[QD_LBL_KNOB0 + QD_K_1], "PATTERN") &&
              !strcmp(lb.s[QD_LBL_B0 + 10], "REMOVE"), "the labels: B3 ARP, GROUP, EFFECT, PATTERN, REMOVE");
    btap(QD_B_REC);                                     /* (P1.6f: opened from the step view: STEPS; REC: KEYS) */
    kdown(WK(0)), kdown(WK(2));                         /* the keys through the group: the arp plays them */
    run(600);
    CHECK(qd_mfx_arp_playing(&A.player.mfx, 1) && leds().button[QD_B_KEYS] >= 153,
          "W1 W3 held: the arp plays (ARP breathing high, P1.6f)");
    kup(WK(0)), kup(WK(2));
    run(300);
    CHECK(!qd_mfx_arp_playing(&A.player.mfx, 1), "let go: it stops");
    btap(QD_B_REC);
    bdown(SHIFT), turn(QD_K_SELECT, 1), bup(SHIFT), run(10);
    qd_view_mfx(&A, &v);
    CHECK(A.sel == 1 && !v.g && qd_view_screen(&A) == QD_SCR_MFX, "SHIFT + SELECT: track 2, its group (OFF)");
    btap(F1);
    CHECK(qd_view_screen(&A) == QD_SCR_MFX && A.ui.latch == QD_LATCH_MFX, "F1 tapped: open (P1.6h)");
    btap(QD_B_KEYS);
    CHECK(qd_view_screen(&A) != QD_SCR_MFX, "ARP tapped: closed");
    btap(QD_B_KEYS);
    CHECK(qd_view_screen(&A) == QD_SCR_MFX, "ARP tapped again: open");
    btap(QD_B_HOME);
    CHECK(qd_view_screen(&A) != QD_SCR_MFX, "HOME: closed");
    btap(QD_B_PATTERN);                                 /* the track settings, page 2, KNOB 4 */
    btap(QD_B_PATTERN);
    turn(QD_K_4, 4);
    CHECK(PAT()->trk[1].mfx == 4, "the track settings' page 2, KNOB 4: the track's MIDI FX (4)");
    btap(QD_B_HOME);
}

/* T1 (the owner, 2026-10-24): ARP held opens TOOLS (the arp on / off toggle is gone: no group made, no arp added); a tap
 * the MIDI FX screen; SHIFT + ARP is FILL at once, and stays FILL however long it is held */
static void arp_hold(void)
{
    bdown(QD_B_KEYS);
    run(QD_ARP_HOLD_MS + 20);
    bup(QD_B_KEYS);
    run(20);
}
static void g_p16c_arp(void)
{
    start();
    qd_ctl_select_track(&A, 2);
    bdown(QD_B_KEYS);
    run(QD_ARP_HOLD_MS - 20);
    CHECK(!qd_tools_open(&A), "ARP held 380 ms: not yet");
    run(30);
    CHECK(qd_tools_open(&A) && qd_view_screen(&A) == QD_SCR_TOOLS, "held 400 ms: TOOLS at once (before the release)");
    bup(QD_B_KEYS);
    run(20);
    CHECK(qd_tools_open(&A) && PAT()->trk[2].mfx == 0 && A.proj.mfx[2].slot[0].type == QD_MFX_NONE &&
              qd_view_screen(&A) != QD_SCR_MFX, "let go: TOOLS stays; no group, no arp made");
    btap(QD_B_HOME);
    CHECK(!qd_tools_open(&A), "HOME: closed");
    btap(QD_B_KEYS);
    CHECK(qd_view_screen(&A) == QD_SCR_MFX, "ARP tapped: the MIDI FX screen (unchanged)");
    arp_hold();
    CHECK(qd_tools_open(&A) && !qd_mfx_open(&A), "ARP held over the MIDI FX screen: TOOLS instead");
    btap(QD_B_HOME);
    bdown(SHIFT), bdown(QD_B_KEYS);
    CHECK(A.seq.fill, "SHIFT + ARP: FILL at once (the press)");
    run(600);
    CHECK(A.seq.fill && !qd_tools_open(&A), "held 600 ms: still FILL, no TOOLS");
    bup(QD_B_KEYS), bup(SHIFT);
    run(10);
    CHECK(!A.seq.fill && !qd_tools_open(&A) && qd_view_screen(&A) != QD_SCR_MFX, "let go: no FILL, nothing opened");
    btap(QD_B_HOME);
}

/* P1.6c: a held step in TRIG: KNOB 2 the step's MIDI FX (TRACK, OFF, 1..5), B4 TRACK, B5 OFF, B6..B10 groups 1..5 */
static void g_p16c_trig(void)
{
    start();
    mode(M_TRIG);
    whold(0);
    turn(QD_K_2, 3);
    CHECK(PAT()->step[0][0].mfx == QD_SMFX_G1 + 1 && A.ui.tk == QD_TK_MFX_STEP, "TRIG, KNOB 2 +3: the step on group 2 "
          "(its picture)");
    black(B5);
    CHECK(PAT()->step[0][0].mfx == QD_SMFX_OFF, "B5: OFF");
    black(B10);
    CHECK(PAT()->step[0][0].mfx == QD_SMFX_G1 + 4, "B10: group 5");
    black(B4);
    CHECK(PAT()->step[0][0].mfx == QD_SMFX_TRACK, "B4: TRACK");
    black(B7);
    CHECK(leds().key[BK(B7)] == QD_LED_LIT && leds().key[BK(B4)] != QD_LED_LIT, "B7 (group 2) lit");
    wrel(0);
    CHECK(A.ui.tk != QD_TK_MFX_STEP, "let go: its picture gone");
    turn(QD_K_2, 3);                                    /* (nothing held: the track's group, the default) */
    CHECK(PAT()->trk[0].mfx == 3 && qd_step_mfx(PAT(), 0, 0) == 2 && qd_step_mfx(PAT(), 0, 1) == 3,
          "nothing held, KNOB 2: the track's group (3); step 1 locked to group 2, the others follow the track");
    btap(QD_B_HOME);
}

/* P1.6d: the sends lock on a held step (or the latch's selection): the mix's LEVEL / PAN / DELAY / REVERB and its
 * CHORUS, the effects' OVERVIEW KNOB 1..3 and each effect's SEND; the frame and the chips; F2 + knob clears; nothing
 * held: the track's values */
static void g_p16d_sends(void)
{
    static ui_mix_t mv;
    static ui_fx_t fv;
    int v0, l0;
    start();
    wtap(2);                                            /* (a trig on step 3) */
    btap(QD_B_PAGE);
    v0 = A.model.t[0].mix.level;
    whold(2);
    turn(QD_K_1, -20);
    qd_view_mix(&A, &mv);
    CHECK(LK(0, 2, QD_LK_LEVEL) == v0 - 20 && A.model.t[0].mix.level == v0 && qd_view_screen(&A) == QD_SCR_MIX &&
              mv.step && mv.locked[0] && mv.tr[0].level == v0 - 20,
          "the mix, W3 held + KNOB 1 -20: a LEVEL lock on step 3 (%d), the track's level kept; the frame, the chip",
          LK(0, 2, QD_LK_LEVEL));
    turn(QD_K_2, 9);
    CHECK(LK(0, 2, QD_LK_PAN) == A.model.t[0].mix.pan + 9, "KNOB 2: a PAN lock");
    wrel(2);
    qd_view_mix(&A, &mv);
    CHECK(!mv.step && !mv.locked[0] && mv.lockany[0] && mv.tr[0].level == v0 && A.ui.latch == QD_LATCH_MIX,
          "let go: the track's values, an outlined chip (some step locks it)");
    l0 = A.model.t[0].mix.level;
    turn(QD_K_1, 3);
    CHECK(A.model.t[0].mix.level == l0 + 3 && LK(0, 2, QD_LK_LEVEL) == v0 - 20, "nothing held: the track's LEVEL");
    wtap(6);
    CHECK(!R(0, 6).on, "a W tapped in the mix makes no trig");
    whold(2);
    bdown(F2);
    run(10);
    turn(QD_K_1, 1);
    bup(F2);
    run(10);
    CHECK(LK(0, 2, QD_LK_LEVEL) == -99999 && LK(0, 2, QD_LK_PAN) != -99999, "F2 + KNOB 1: the LEVEL lock cleared");
    wrel(2);
    btap(QD_B_PAGE);                                    /* the sends' page */
    whold(2);
    turn(QD_K_1, 5);
    CHECK(A.ui.mix_page == 1 && LK(0, 2, QD_LK_CHORUS) == A.model.t[0].mix.chorus + 5, "SENDS, KNOB 1: a CHORUS lock");
    wrel(2);
    btap(QD_B_HOME);
    /* the latch's selection */
    btap(LFO);
    wtap(4);
    btap(QD_B_PAGE);
    CHECK(A.ui.slatch && A.ui.nheld == 0 && A.ui.latch == QD_LATCH_MIX, "P1.6h: the step view's selection let go "
          "(the latch on)");
    turn(QD_K_2, 3);
    CHECK(LK(0, 4, QD_LK_PAN) == -99999, "KNOB 2: the track's PAN, no lock");
    wtap(4);
    CHECK(A.ui.nheld == 1 && A.ui.held[0] == 4, "W5 in the mix: selected there");
    turn(QD_K_3, 10);
    CHECK(LK(0, 4, QD_LK_DELAY) == A.model.t[0].mix.delay + 10, "KNOB 3: step 5's DELAY lock");
    btap(QD_B_OCTDN);
    CHECK(A.ui.latch == QD_LATCH_MIX, "F1 tapped: the mix stays (P1.6h)");
    btap(QD_B_TRACKS);
    CHECK(A.ui.latch == QD_LATCH_FX && A.ui.slatch && A.ui.nheld == 1, "the effects: the selection kept");
    turn(QD_K_1, 7);
    qd_view_fx(&A, &fv);
    CHECK(LK(0, 4, QD_LK_CHORUS) == A.model.t[0].mix.chorus + 7 && fv.step && fv.locked[0] && !fv.locked[2],
          "OVERVIEW KNOB 1 (CHORUS): the lock, its chip, the frame");
    black(B2);
    turn(QD_K_4, -4);
    qd_view_fx(&A, &fv);
    CHECK(!strcmp(QD_FX_PAGES[A.ui.fx_page].name, "DELAY") && LK(0, 4, QD_LK_DELAY) == A.model.t[0].mix.delay + 6 &&
              fv.locked[3],
          "DELAY's SEND -4: the step's lock (%d)", LK(0, 4, QD_LK_DELAY));
    btap(QD_B_HOME);
    CHECK(!A.ui.slatch, "HOME: the latch off");
    btap(QD_B_TRACKS);
    wtap(3);
    CHECK(A.sel == 3 && !A.ui.nheld, "nothing held: the OVERVIEW's W4 picks track 4");
    btap(QD_B_HOME);
}

/* P1.6d: automation recording. Playing, REC held in the step view: KNOB 1 (the quick page's first) as locks into the
 * steps the playhead passes (an empty step a ghost, a LOCK trig), the track's sound unchanged; several moves in a step:
 * the last; one undo level a take; REC let go: no tap, no arming, the view stays. Then the take through the player
 * (the next pass's voice has the lock); the mix (REC held + KNOB 1: LEVEL); the record settings' page 2 AUTOMATION;
 * the keyboard armed (ON: knob moves recorded, ghosts; OFF: notes only, the sound edited; REC held: recorded anyway) */
static int ph(void)                                     /* the step under track 1's playhead */
{
    return qd_track_loop_step(&PAT()->trk[0], (int)track_pos0() % qd_track_len(&PAT()->trk[0]));
}
static void to_step(int s)
{
    int n = 0;
    while (ph() != s && n++ < 5000)
        run(1);
    run(10);
}
static void g_p16d_auto(void)
{
    const int id = qd_step_quick_id(0);
    int f0, u0, v2, i, got = 0, s;
    int16_t vp[QD_QUAD_NP];
    start();
    A.proj.rec_dub = QD_TAKE_REPLACE;                   /* (armed playing: recording at once, knobs included) */
    wtap(4);                                            /* a note on step 5 */
    run(QD_UNDO_QUIET_MS + 50);
    f0 = A.model.t[0].patch[id];
    u0 = qd_undo_count(&A, -1);
    btap(QD_B_PLAY);
    to_step(2);
    bdown(QD_B_REC);
    run(10);
    turn(QD_K_1, -3);
    turn(QD_K_1, -2);
    v2 = LK(0, 2, id);
    CHECK(v2 != -99999 && v2 != f0 && R(0, 2).on && R(0, 2).type == QD_TRIG_LOCK && A.model.t[0].patch[id] == f0,
          "playing, REC held + KNOB 1 on step 3: a lock (%d, the track's %d kept), an empty step a ghost (LOCK)", v2, f0);
    turn(QD_K_1, 1);
    CHECK(LK(0, 2, id) > v2 && LK(0, 2, id) != -99999, "a third move within the step: the last value (%d)", LK(0, 2, id));
    v2 = LK(0, 2, id);
    to_step(4);
    turn(QD_K_1, -1);
    CHECK(LK(0, 4, id) < v2 && R(0, 4).type == QD_TRIG_NOTE, "step 5 (a note): the lock goes on from the last value "
          "(%d < %d), the note kept", LK(0, 4, id), v2);
    run(QD_REC_HOLD_MS);
    CHECK(A.ui.mode == QD_MODE_STEP && !A.ui.rec && qd_auto_on(&A), "REC held long in the step view: no arming, the view "
          "stays");
    bup(QD_B_REC);
    run(10);
    CHECK(A.ui.mode == QD_MODE_STEP && !qd_auto_on(&A) && !A.ui.auto_take, "REC let go: no tap (the sequencer stays), "
          "the take ends");
    run(QD_UNDO_QUIET_MS + 50);
    CHECK(qd_undo_count(&A, -1) == u0 + 1, "one undo level for the take (%d -> %d)", u0, qd_undo_count(&A, -1));
    /* the take played: step 5's voice on the next pass has the lock */
    to_step(3);
    to_step(4);
    for (i = 0; i < A.pool.nvoice; i++)
        if (qd_pool_sounding(&A.pool, i) && A.pool.v[i].track == 0) {
            qd_player_voice_patch(&A.pool, i, vp);
            got |= vp[id] == LK(0, 4, id);
        }
    CHECK(got, "through the player: step 5's voice plays the recorded value (%d)", LK(0, 4, id));
    qd_undo_step(&A, -1);
    CHECK(LK(0, 2, id) == -99999 && !R(0, 2).on && LK(0, 4, id) == -99999 && R(0, 4).on, "undo: the take gone (the "
          "ghost too), the note kept");
    /* the mix */
    btap(QD_B_PAGE);
    f0 = A.model.t[0].mix.level;
    to_step(6);
    bdown(QD_B_REC);
    run(10);
    turn(QD_K_1, -12);
    bup(QD_B_REC);
    run(10);
    CHECK(LK(0, 6, QD_LK_LEVEL) == f0 - 12 && A.model.t[0].mix.level == f0 && A.ui.latch == QD_LATCH_MIX,
          "the mix, REC held + KNOB 1: a LEVEL lock on step 7 (%d)", LK(0, 6, QD_LK_LEVEL));
    btap(QD_B_HOME);
    /* the record settings' page 2 */
    bdown(SHIFT);
    btap(QD_B_REC);
    btap(QD_B_REC);
    bup(SHIFT);
    CHECK(A.ui.latch == QD_LATCH_REC && A.ui.rec_page == 1 && A.proj.rec_auto == 1, "SHIFT + REC twice: the record "
          "settings' page 2, AUTOMATION ON");
    turn(QD_K_1, -1);
    CHECK(A.proj.rec_auto == 0, "KNOB 1: AUTOMATION OFF");
    turn(QD_K_SELECT, -1);
    CHECK(A.ui.rec_page == 0, "SELECT: page 1");
    btap(QD_B_OCTDN);
    /* the keyboard, armed */
    btap(QD_B_REC);
    live_arm();
    CHECK(A.ui.mode == QD_MODE_KEYS && A.ui.rec && !qd_auto_on(&A), "the keyboard, REC held: armed; AUTOMATION OFF: no "
          "automation");
    f0 = A.model.t[0].patch[id];
    to_step(9);
    turn(QD_K_1, -2);
    CHECK(LK(0, 9, id) == -99999 && !R(0, 9).on && A.model.t[0].patch[id] != f0, "OFF: a knob edits the sound, nothing "
          "recorded");
    bdown(QD_B_REC);
    run(10);
    to_step(10);
    turn(QD_K_1, -2);
    bup(QD_B_REC);
    run(10);
    CHECK(LK(0, 10, id) != -99999 && R(0, 10).type == QD_TRIG_LOCK && A.ui.rec, "REC held, armed: recorded anyway (a "
          "ghost); its release no tap (still armed)");
    A.proj.rec_auto = 1;
    to_step(12);
    s = ph();
    turn(QD_K_2, -3);
    CHECK(LK(0, s, qd_step_quick_id(1)) != -99999 && R(0, s).type == QD_TRIG_LOCK, "ON: armed live recording records "
          "KNOB 2, a ghost on step %d", s + 1);
    btap(QD_B_PLAY);
    btap(QD_B_HOME);
}

/* P1.6d: the track settings' page 3: KIND (the name follows until one is typed), NAME (the name keyboard: typed,
 * SAVE kept, back to page 3; HOME cancels; PRESETS ideas); the kinds' icons the view's */
static void g_p16d_kinds(void)
{
    static ui_menu_t mv;
    static ui_type_t tv;
    char was[QD_TRACK_NAME];
    int k, ok;
    start();
    qd_ctl_select_track(&A, 2);                         /* KEYS */
    btap(QD_B_PATTERN);
    btap(QD_B_PATTERN);
    btap(QD_B_PATTERN);
    CHECK(A.ui.latch == QD_LATCH_TRK && A.ui.trk_page == 2, "SEL three times: the track settings' page 3");
    turn(QD_K_1, 2);
    CHECK(A.model.t[2].kind == QD_PK_PAD && !strcmp(A.model.t[2].name, "PAD"), "KNOB 1 +2: KIND PAD, the name PAD");
    qd_view_menu(&A, &mv);
    CHECK(mv.page == 2 && mv.tkind == QD_PK_PAD && !strcmp(mv.tname, "PAD") && !mv.named, "its picture: PAD, not typed");
    turn(QD_K_2, 1);
    qd_view_type(&A, &tv);
    CHECK(qd_type_open(&A) && qd_view_screen(&A) == QD_SCR_NAME && !strcmp(tv.title, "TRACK NAME") && tv.track == 3 &&
              !strcmp(A.ui.ty.name, "PAD"),
          "KNOB 2: the name keyboard, TRACK NAME, track 3's name selected");
    black(B1);                                          /* A (it replaces the selection) */
    wtap(6);                                            /* R */
    wtap(4);                                            /* P */
    btap(QD_B_SAVE);
    CHECK(!strcmp(A.model.t[2].name, "ARP") && A.model.t[2].named && A.ui.latch == QD_LATCH_TRK && A.ui.trk_page == 2,
          "A R P, SAVE: named ARP (typed), back on page 3 (%s)", A.model.t[2].name);
    turn(QD_K_1, -1);
    CHECK(A.model.t[2].kind == QD_PK_PLUCK && !strcmp(A.model.t[2].name, "ARP"), "a kind chosen: the typed name stays");
    turn(QD_K_2, 1);
    for (ok = 0, k = 0; k < 6; k++) {
        turn(QD_K_PRESETS, 1);
        ok += A.ui.ty.name[0] != 0 && strlen(A.ui.ty.name) <= 12;
    }
    CHECK(ok == 6, "PRESETS: names rolled (%s)", A.ui.ty.name);
    strcpy(was, A.model.t[2].name);
    btap(QD_B_HOME);
    CHECK(!strcmp(A.model.t[2].name, was) && A.ui.latch == QD_LATCH_TRK && A.ui.trk_page == 2, "HOME: cancelled, page 3");
    btap(QD_B_PATTERN);
    CHECK(A.ui.trk_page == 0, "SEL again: round to page 1");
    btap(QD_B_HOME);
    {
        static uint16_t lcd[QD_W * QD_H];
        qd_view_frame(&A, lcd);
    }
    CHECK(UI_TRACK_KIND[2] == QD_PK_PLUCK && UI_TRACK_KIND[0] == QD_PK_DRUMS, "the view: the kinds for the icons");
}

/* P1.6d, the step latch: SHIFT + ALGORITHM the scale, SHIFT + PRESETS the root in every latch state (VALUES and
 * MODES, a selection or none: no sound lock, no browser); VALUES with nothing selected: the black keys the track's
 * default (NOTE: its notes; LFO + a black key still the mode); a W double-tapped: the step made, again removed (one
 * undo level each, the first tap leaving none) */
static void g_p16d_latch(void)
{
    qd_tset_t *ts;
    int lm, sel, s0, r0, u0;
    start();
    ts = &PAT()->trk[0];
    wtap(2);
    run(QD_UNDO_QUIET_MS + 50);
    btap(LFO);
    for (lm = 0; lm < 2; lm++)
        for (sel = 1; sel >= 0; sel--) {
            if (lm == 1 && sel == 1) {                  /* MODES */
                bdown(LFO), run(QD_LFO_HOLD_MS + 20), turn(QD_K_SELECT, 1), bup(LFO), run(10);
            }
            if (!sel && A.ui.nheld) {                   /* (the selection emptied: its step tapped again) */
                run(QD_CLICK_MS + 10);
                wtap(A.ui.held[0] % 16);
                run(QD_CLICK_MS + 10);
            }
            if (sel && !A.ui.nheld) {
                wtap(2);
                run(QD_CLICK_MS + 10);
            }
            s0 = ts->scale, r0 = ts->root;
            bdown(SHIFT), turn(QD_K_ALGO, 2), turn(QD_K_PRESETS, 3), bup(SHIFT), run(10);
            CHECK(ts->scale == s0 + 2 && ts->root == (r0 + 3) % 12 && qd_view_screen(&A) != QD_SCR_BROWSE &&
                      !ST(0, 2)->sound && A.ui.slatch && A.ui.nheld == sel,
                  "the latch (%s, %s): SHIFT + ALGORITHM the scale, SHIFT + PRESETS the root (%d %d)",
                  lm ? "MODES" : "VALUES", sel ? "a selection" : "none", ts->scale, ts->root);
        }
    bdown(LFO), run(QD_LFO_HOLD_MS + 20), turn(QD_K_SELECT, -1), bup(LFO), run(10);   /* VALUES, nothing selected */
    bdown(LFO), black(B1), bup(LFO), run(10);           /* (NOTE: LFO + B1) */
    CHECK(A.ui.hmode == M_NOTE && !A.ui.nheld && A.ui.lmode == QD_LMODE_VALUES, "LFO + B1: the mode NOTE");
    {
        uint8_t n0[QD_NNOTE];
        memcpy(n0, PAT()->trk[0].def.note, sizeof n0);
        black(B5);
        s0 = memcmp(n0, PAT()->trk[0].def.note, sizeof n0);
    }
    CHECK(s0 && A.ui.tk == QD_TK_DEFAULT && A.ui.hmode == M_NOTE,
          "VALUES, nothing selected: B5 the default's note (the picture; the mode kept)");
    /* the double tap */
    run(QD_UNDO_QUIET_MS + 50);
    u0 = qd_undo_count(&A, -1);
    wtap(9);
    wtap(9);
    run(QD_UNDO_QUIET_MS + 50);
    CHECK(R(0, 9).on && qd_undo_count(&A, -1) == u0 + 1, "W10 double-tapped (empty): the step made, one undo level "
          "(%d -> %d)", u0, qd_undo_count(&A, -1));
    run(QD_CLICK_MS + 10);
    wtap(9);
    wtap(9);
    run(QD_UNDO_QUIET_MS + 50);
    CHECK(!R(0, 9).on && qd_undo_count(&A, -1) == u0 + 2, "double-tapped again: removed (one more level)");
    run(QD_CLICK_MS + 10);
    wtap(11);
    run(QD_UNDO_QUIET_MS + 50);
    CHECK(!R(0, 11).on && A.ui.nheld == 1 && A.ui.held[0] == 11 && qd_undo_count(&A, -1) == u0 + 2,
          "a single tap: selected, nothing made, no undo level");
    btap(LFO);
}

/* P1.6d: the FILTER key held: the machines' popup (B1..B6 MULTI .. EQ, ALGORITHM or a knob turns it, the current
 * lit); let go: closed, the choice kept, the page not cycled; a tap still cycles */
static void g_p16d_filter(void)
{
    static ui_snd_t sv;
    int pg;
    start();
    btap(QD_B_SOUND);
    kdown(BK(B4));
    run(QD_FPOP_MS + 20);
    qd_view_snd(&A, &sv);
    CHECK(A.ui.snd_fpop && sv.fpop && !strcmp(QD_SND_PAGES[A.ui.snd_page].name, "FILTER"), "FILTER held 400 ms: the "
          "machines' popup");
    kdown(BK(B1)), run(20), kup(BK(B1)), run(10);
    CHECK(qd_snd_machine_pos(&A) == 0 && A.ui.snd_fpop, "B1: MULTI");
    turn(QD_K_ALGO, 1);
    qd_view_snd(&A, &sv);
    CHECK(qd_snd_machine_pos(&A) == 1 && sv.fpop_sel == 1, "ALGORITHM +1: LOWPASS 4 (lit)");
    turn(QD_K_2, 1);
    turn(QD_K_2, 1);
    CHECK(qd_snd_machine_pos(&A) == 3, "KNOB 2, two detents: COMB-");
    kdown(BK(B6)), run(20), kup(BK(B6)), run(10);
    CHECK(qd_snd_machine_pos(&A) == 5 && A.ui.snd_fpop, "B6: EQ, the popup still up");
    pg = A.ui.snd_page;
    kup(BK(B4));
    run(10);
    qd_view_snd(&A, &sv);
    CHECK(!A.ui.snd_fpop && !sv.fpop && qd_snd_machine_pos(&A) == 5 && A.ui.snd_page == pg, "let go: closed, EQ kept, "
          "the page not cycled");
    kdown(BK(B1)), run(20), kup(BK(B1)), run(10);
    CHECK(qd_snd_machine_pos(&A) == 5, "B1 after: a group key again (no machine)");
    btap(QD_B_HOME);
}

/* P1.6d: ARP's LED steady while the selected track's group has an arp ON, off otherwise; FILL blinks it; ARP held:
 * the arp on / off without the screen; the groups' screen: ALGORITHM keeps the slot, the glide continuous (the rails
 * up, the square from where it was); a slot key held + a knob moves the slot (the lift), a tap still selects */
static void g_p16d_mfx(void)
{
    static ui_mfx_t v;
    qd_mfx_group_t *G;
    float p1;
    start();
    CHECK(leds().button[QD_B_KEYS] == QD_LED_OFF, "no group: ARP dark");
    PAT()->trk[0].mfx = 1;                              /* (T1: ARP held is TOOLS: the arp made here) */
    G = &A.proj.mfx[0];
    qd_mfx_slot_init(&G->slot[0], QD_MFX_ARP);
    G->slot[0].v[QD_MA_MODE] = QD_AM_HOLD;
    run(20);
    CHECK(leds().button[QD_B_KEYS] == QD_LED_LIT && !half_period(QD_B_KEYS), "the arp ON: ARP lit, steady");
    G->slot[0].v[QD_MA_MODE] = QD_AM_OFF;
    run(20);
    CHECK(leds().button[QD_B_KEYS] == QD_LED_OFF, "the arp OFF: ARP dark");
    bdown(SHIFT), bdown(QD_B_KEYS), run(10);
    CHECK(A.seq.fill && leds().button[QD_B_KEYS] == QD_LED_LIT && !half_period(QD_B_KEYS),
          "SHIFT + ARP held: FILL, ARP lit (T1: steady)");
    bup(QD_B_KEYS), bup(SHIFT), run(10);
    G->slot[0].v[QD_MA_MODE] = QD_AM_HOLD;
    /* the screen */
    btap(QD_B_KEYS);
    qd_mfx_slot_init(&G->slot[1], QD_MFX_CHANCE);
    qd_mfx_slot_init(&G->slot[2], QD_MFX_SCALER);
    black(B3);
    CHECK(A.ui.mfx_slot == 2, "B3: slot 3");
    turn(QD_K_ALGO, 1);
    CHECK(PAT()->trk[0].mfx == 2 && A.ui.mfx_slot == 2, "ALGORITHM: group 2, slot 3 kept");
    run(100);
    qd_view_mfx(&A, &v);
    p1 = v.gsel_pos;
    turn(QD_K_ALGO, 1);
    qd_view_mfx(&A, &v);
    CHECK(v.gsel && p1 > 1.f && p1 < 2.f && v.gsel_pos >= p1 - 0.01f && v.gsel_pos < 2.f,
          "a detent mid-glide: the rails up, the square goes on from where it was (%.2f -> %.2f)", p1, v.gsel_pos);
    run(QD_MFX_GLIDE_MS + 20);
    qd_view_mfx(&A, &v);
    CHECK(v.gsel && v.gsel_pos > 2.99f, "settled on group 3 (%.2f)", v.gsel_pos);
    turn(QD_K_ALGO, -2);                                /* back to group 1: ARP, CHANCE, SCALE */
    run(QD_TK_MS + 300);
    kdown(BK(B1));
    run(20);
    turn(QD_K_1, 2);
    qd_view_mfx(&A, &v);
    CHECK(G->slot[0].type == QD_MFX_ARP && v.drag_from == 0 && v.drag_to == 2, "B1 held + KNOB 1 +2: previewed in "
          "slot 3, nothing moved yet (P1.6e)");
    kup(BK(B1));
    run(10);
    qd_view_mfx(&A, &v);
    CHECK(G->slot[0].type == QD_MFX_CHANCE && G->slot[1].type == QD_MFX_SCALER && G->slot[2].type == QD_MFX_ARP &&
              A.ui.mfx_slot == 2 && A.ui.mfx_page == 0 && v.drop == 2,
          "let go: the ARP dropped into slot 3, the others closed up, no page change");
    black(B2);
    CHECK(A.ui.mfx_slot == 1 && A.ui.mfx_page == 0, "B2 tapped: selected");
    btap(QD_B_HOME);
}

/* P1.6e (the owner: "the slot move gets stuck"): B1 held, KNOB 1 +2, B1 let go: KNOB 1 is the slot's value again (it
 * moved no slot); the same with the release in the knob's tick, and with another key down at the release */
static void g_p16e_stuck(void)
{
    qd_mfx_group_t *G;
    int v0;
    start();
    btap(QD_B_KEYS);                                    /* (the MIDI FX screen) */
    turn(QD_K_ALGO, 1);
    G = &A.proj.mfx[0];
    qd_mfx_slot_init(&G->slot[0], QD_MFX_ARP);
    qd_mfx_slot_init(&G->slot[1], QD_MFX_CHANCE);
    qd_mfx_slot_init(&G->slot[2], QD_MFX_HARM);
    run(QD_TK_MS + 300);
    kdown(BK(B1));
    run(20);
    turn(QD_K_1, 2);
    kup(BK(B1));
    run(20);
    CHECK(G->slot[2].type == QD_MFX_ARP && G->slot[0].type == QD_MFX_CHANCE && A.ui.mfx_slot == 2,
          "B1 held, KNOB 1 +2, let go: the ARP dropped into slot 3");
    v0 = G->slot[2].v[QD_MA_PAT];
    turn(QD_K_1, 1);
    CHECK(G->slot[2].type == QD_MFX_ARP && G->slot[2].v[QD_MA_PAT] == v0 + 1 && G->slot[0].type == QD_MFX_CHANCE,
          "then KNOB 1: the ARP's PATTERN, no move (the stuck move: fixed)");
    kdown(BK(B3));                                      /* the release in the knob's own tick */
    run(20);
    qd_app_knob(&A, QD_K_1, -1);
    kup(BK(B3));
    run(20);
    turn(QD_K_1, -1);
    CHECK(G->slot[1].type == QD_MFX_ARP && G->slot[1].v[QD_MA_PAT] == v0 && G->slot[0].type == QD_MFX_CHANCE,
          "a release in the knob's tick: dropped into slot 2, then KNOB 1 its PATTERN");
    kdown(BK(B2));                                      /* another key down at the release */
    run(20);
    turn(QD_K_1, 1);
    kdown(WK(3));
    kup(BK(B2));
    run(20);
    kup(WK(3));
    run(20);
    turn(QD_K_1, 1);
    CHECK(G->slot[2].type == QD_MFX_ARP && G->slot[2].v[QD_MA_PAT] == v0 + 1, "a W down at the release: dropped, "
          "then KNOB 1 its PATTERN");
    btap(QD_B_HOME);
}

/* P1.6e: the MIDI FX screen: a slot held lifts (arrows) past the tap time, its move a preview until the release (one
 * undo level); F1 / F2 held: their states; REC: STEPS / KEYS (REC lit on KEYS), STEPS' steps held + ALGORITHM their
 * MIDI FX lock (its picture, the frame), several, the latch's selection; KEYS again plays */
static void g_p16e_mfx(void)
{
    static ui_mfx_t v;
    qd_mfx_group_t *G;
    int u0, n;
    start();
    wtap(0), wtap(4), wtap(8);                          /* (trigs on steps 1, 5, 9) */
    btap(QD_B_KEYS);
    turn(QD_K_ALGO, 1);
    G = &A.proj.mfx[0];
    qd_mfx_slot_init(&G->slot[0], QD_MFX_ARP);
    qd_mfx_slot_init(&G->slot[1], QD_MFX_CHANCE);
    run(QD_TK_MS + 300);
    kdown(BK(B2));
    run(100);
    qd_view_mfx(&A, &v);
    CHECK(v.drag_from == 1 && v.drag_up == 0.f, "B2 down 100 ms: not lifted yet (a tap)");
    run(QD_TAP_MS + 200);
    qd_view_mfx(&A, &v);
    CHECK(v.drag_from == 1 && v.drag_up > 0.9f && !v.drag_moved, "held past the tap time: lifted (the arrows)");
    run(QD_UNDO_QUIET_MS + 50);
    u0 = qd_undo_count(&A, -1);
    turn(QD_K_3, -1);
    qd_view_mfx(&A, &v);
    CHECK(G->slot[1].type == QD_MFX_CHANCE && v.drag_to == 0 && v.drag_moved, "KNOB 3 -1: previewed in slot 1, the "
          "chain as it was");
    kup(BK(B2));
    run(QD_UNDO_QUIET_MS + 50);
    CHECK(G->slot[0].type == QD_MFX_CHANCE && G->slot[1].type == QD_MFX_ARP && qd_undo_count(&A, -1) == u0 + 1,
          "let go: dropped, one undo level");
    kdown(BK(B1)), run(QD_TAP_MS + 100), kup(BK(B1)), run(10);
    CHECK(G->slot[0].type == QD_MFX_CHANCE && A.ui.mfx_slot == 0 && A.ui.mfx_page == 0, "held, no knob: nothing moved, "
          "no page turn");
    /* F1, F2 held */
    bdown(F1);
    run(QD_TAP_MS + 50);
    qd_view_mfx(&A, &v);
    CHECK(v.f1 && v.f1_src < 0, "F1 held: its COPY picture");
    black(B2);
    qd_view_mfx(&A, &v);
    CHECK(v.f1 && v.f1_src == 1, ".. B2: the source marked");
    bup(F1);
    run(10);
    bdown(F2);
    run(20);
    qd_view_mfx(&A, &v);
    CHECK(v.f2 && v.f2_hand == QD_MFX_ARP, "F2 held: the hand holds F1's copy (the ARP)");
    black(B3);
    qd_view_mfx(&A, &v);
    CHECK(G->slot[2].type == QD_MFX_ARP && v.f2_hand < 0, ".. B3 (empty): dropped there, the hand empty");
    black(B1);
    qd_view_mfx(&A, &v);
    CHECK(v.f2 && v.f2_hand == QD_MFX_CHANCE && G->slot[0].type == QD_MFX_NONE, ".. B1: picked up (in the hand)");
    black(B1);
    bup(F2);
    run(10);
    CHECK(G->slot[0].type == QD_MFX_CHANCE, ".. B1 again: put back");
    /* REC: STEPS / KEYS */
    CHECK(qd_mfx_open(&A) && leds().button[QD_B_REC] == QD_LED_OFF && A.ui.mode == QD_MODE_STEP &&
              leds().key[WK(4)] == QD_LED_LIT && leds().key[WK(5)] == QD_LED_FAINT,
          "opened from the step view: STEPS (REC dark, P1.6f), the steps lit as in the step view");
    kdown(WK(4)), kdown(WK(8));
    run(20);
    turn(QD_K_ALGO, 3);
    qd_view_mfx(&A, &v);
    CHECK(PAT()->step[0][4].mfx == QD_SMFX_G1 + 1 && PAT()->step[0][8].mfx == QD_SMFX_G1 + 1 && A.ui.tk == QD_TK_MFX_STEP &&
              v.steps_frame && PAT()->trk[0].mfx == 1 && !sounding(64),
          "W5, W9 held + ALGORITHM +3: both locked to group 2 (the picture, the frame), the track's group kept");
    kup(WK(4)), kup(WK(8));
    run(20);
    qd_view_mfx(&A, &v);
    CHECK(A.ui.tk != QD_TK_MFX_STEP && !v.steps_frame, "let go: the picture and the frame gone");
    turn(QD_K_ALGO, 1);
    CHECK(PAT()->trk[0].mfx == 2, "nothing held: ALGORITHM the track's group again");
    turn(QD_K_ALGO, -1);
    btap(QD_B_REC);
    CHECK(A.ui.mode == QD_MODE_KEYS && qd_mfx_open(&A) && leds().button[QD_B_REC] == QD_LED_DIM, "REC: KEYS (REC dim, P1.6g)");
    kdown(WK(0));
    run(400);
    for (n = 0; n < 128; n++)
        if (sounding(n))
            break;
    CHECK(n < 128, "KEYS: W1 plays through the group");
    kup(WK(0));
    run(20);
    btap(QD_B_REC);                                     /* (back to STEPS: the step view after) */
    btap(QD_B_HOME);
    /* the latch's selection counts */
    btap(LFO);
    wtap(4);
    btap(QD_B_KEYS);
    turn(QD_K_ALGO, -2);
    CHECK(A.ui.slatch && A.ui.nheld == 1 && PAT()->step[0][4].mfx == QD_SMFX_OFF && PAT()->step[0][8].mfx == QD_SMFX_G1 + 1,
          "the latch's selection (step 5): ALGORITHM -2 its MIDI FX (OFF)");
    btap(QD_B_HOME);
    btap(LFO);
}

/* P1.6e (the owner): a step held, stopped, sounds through its group (the arp's notes, not the dry chord); its release:
 * every note ends, a HOLD arp's too */
static void g_p16e_preview(void)
{
    qd_mfx_group_t *G;
    int k, on;
    start();
    G = &A.proj.mfx[0];
    qd_mfx_slot_init(&G->slot[0], QD_MFX_ARP);
    G->slot[0].v[QD_MA_MODE] = QD_AM_HOLD;
    PAT()->trk[0].mfx = 1;
    wtap(2);                                            /* (a trig: its note C4) */
    run(400);
    whold(2);
    run(700);
    CHECK(qd_mfx_arp_playing(&A.player.mfx, 1) && sounding(60), "step 3 held, stopped: through the group (the arp "
          "plays its note)");
    wrel(2);
    run(400);
    for (on = 0, k = 0; k < 128; k++)
        on |= sounding(k);
    CHECK(!qd_mfx_arp_playing(&A.player.mfx, 1) && !on, "let go: the arp stops (HOLD too), nothing sounds");
    PAT()->trk[0].mfx = 0;                              /* the step's own lock: group 1, the track OFF */
    qd_step_set_mfx(PAT(), 0, 2, QD_SMFX_G1);
    whold(2);
    run(700);
    CHECK(qd_mfx_arp_playing(&A.player.mfx, 1), "the step's own group (the track OFF): through it");
    wrel(2);
    run(400);
    CHECK(!qd_mfx_arp_playing(&A.player.mfx, 1), "let go: stopped");
    btap(QD_B_HOME);
}

/* P1.6f (the owner: "F1 / F2 close the MIDI FX screen on release"): F1 held past the tap time and let go, F1 + B and
 * let go, F2 held and let go: the screen stays; P1.6h: an F1 tap too (HOME or ARP close it) */
static void g_p16f_f1f2(void)
{
    start();
    btap(QD_B_KEYS);
    turn(QD_K_ALGO, 1);
    qd_mfx_slot_init(&A.proj.mfx[0].slot[0], QD_MFX_ARP);
    run(QD_TK_MS + 300);
    bdown(F1), run(QD_TAP_MS + 100), bup(F1), run(20);
    CHECK(qd_mfx_open(&A), "F1 held past the tap time, let go: the screen stays");
    bdown(F1), run(30), black(B1), bup(F1), run(20);
    CHECK(qd_mfx_open(&A), "F1 + B1, let go: the screen stays");
    bdown(F2), run(30), bup(F2), run(20);
    CHECK(qd_mfx_open(&A), "F2 tapped: the screen stays");
    bdown(F2), run(QD_TAP_MS + 300), bup(F2), run(20);
    CHECK(qd_mfx_open(&A), "F2 held, let go: the screen stays");
    btap(F1);
    CHECK(qd_mfx_open(&A), "F1 tapped: the screen stays (P1.6h: no exit on F1)");
    btap(QD_B_HOME);
}

/* P1.6f: ARP's LED in the MIDI FX screen breathes, low (5 .. 25 %) with the arp off, high (60 .. 100 %) with it on;
 * outside it steady. STEPS / KEYS is the main view's REC keyboard: opened from the step view STEPS, from the keyboard
 * KEYS; REC switches it; leaving, the view follows; REC's light and label the same; REC held arms, staying; SHIFT +
 * REC the record settings */
static uint8_t arp_led_min, arp_led_max;
static void arp_led_range(void)
{
    int k;
    arp_led_min = 255, arp_led_max = 0;
    for (k = 0; k < 1600; k += 20) {
        const uint8_t l = leds().button[QD_B_KEYS];
        arp_led_min = l < arp_led_min ? l : arp_led_min;
        arp_led_max = l > arp_led_max ? l : arp_led_max;
        run(20);
    }
}
static void g_p16f_keys(void)
{
    qd_labels_t lb;
    start();
    btap(QD_B_KEYS);
    turn(QD_K_ALGO, 1);                                 /* (group 1, no arp) */
    arp_led_range();
    CHECK(arp_led_min == QD_LED_LIT && arp_led_max == QD_LED_LIT, "the screen, no arp: ARP lit (T1: the screen open; "
          "%d .. %d)", arp_led_min, arp_led_max);
    qd_mfx_slot_init(&A.proj.mfx[0].slot[0], QD_MFX_ARP);
    arp_led_range();
    CHECK(arp_led_min == QD_LED_LIT && arp_led_max == QD_LED_LIT, "the arp on: lit (%d .. %d)", arp_led_min, arp_led_max);
    btap(QD_B_HOME);
    arp_led_range();
    CHECK(arp_led_min == QD_LED_LIT && arp_led_max == QD_LED_LIT, "closed: steady lit");
    /* STEPS / KEYS */
    btap(QD_B_KEYS);
    qd_ctl_labels(&A, &lb);
    CHECK(qd_mfx_open(&A) && A.ui.mode == QD_MODE_STEP && leds().button[QD_B_REC] == QD_LED_OFF &&
              !strcmp(lb.s[QD_LBL_BTN0 + QD_B_REC], "KEYS"), "opened from the step view: STEPS (REC dark, its label KEYS)");
    btap(QD_B_REC);
    qd_ctl_labels(&A, &lb);
    CHECK(qd_mfx_open(&A) && A.ui.mode == QD_MODE_KEYS && leds().button[QD_B_REC] == QD_LED_DIM &&
              !strcmp(lb.s[QD_LBL_BTN0 + QD_B_REC], "STEPS"), "REC: KEYS (REC dim, its label STEPS)");
    btap(QD_B_KEYS);
    CHECK(!qd_mfx_open(&A) && A.ui.mode == QD_MODE_KEYS && qd_view_screen(&A) == QD_SCR_KEYS, "left on KEYS: the keyboard");
    qd_ctl_labels(&A, &lb);
    CHECK(leds().button[QD_B_REC] == QD_LED_DIM && !strcmp(lb.s[QD_LBL_BTN0 + QD_B_REC], "STEPS"), "the keyboard: REC dim, "
          "its label STEPS (the same)");
    btap(QD_B_KEYS);
    CHECK(qd_mfx_open(&A) && A.ui.mode == QD_MODE_KEYS, "opened from the keyboard: KEYS");
    btap(QD_B_REC);
    btap(QD_B_HOME);
    CHECK(!qd_mfx_open(&A) && A.ui.mode == QD_MODE_STEP && qd_view_screen(&A) != QD_SCR_KEYS, "left on STEPS: the step view");
    btap(QD_B_KEYS);
    bdown(QD_B_REC);
    run(QD_REC_HOLD_MS + 50);
    bup(QD_B_REC);
    run(10);
    CHECK(qd_mfx_open(&A) && A.ui.mode == QD_MODE_STEP && A.ui.rec && A.ui.live_wait, "REC held there (stopped): "
          "recording armed, the screen stays as it was (P1.6g)");
    btap(QD_B_REC);
    CHECK(!A.ui.rec && A.ui.mode == QD_MODE_STEP && qd_mfx_open(&A), "REC tapped while armed: disarmed (no switch)");
    bdown(SHIFT);
    btap(QD_B_REC);
    bup(SHIFT);
    run(10);
    CHECK(A.ui.latch == QD_LATCH_REC, "SHIFT + REC: the record settings");
    btap(QD_B_HOME);
}

/* P1.6h (the owner: "None of these should exit with F1, only the pop-up menus"): F1 tapped leaves no main page (the
 * step view, the mix and its sends, the effects, the sound editor with and without its keyboard, the MIDI FX screen in
 * STEPS and KEYS); every pop-up still goes back on F1 (F3, the track settings, the record settings, the global
 * settings, the projects, the browser) */
static void g_p16h_f1(void)
{
    qd_labels_t lb;
    int oct;
    start();
    btap(F1);
    CHECK(A.ui.mode == QD_MODE_STEP && !A.ui.latch, "the step view: F1 tapped, nothing opens or leaves");
    btap(QD_B_PAGE);
    btap(F1);
    CHECK(A.ui.latch == QD_LATCH_MIX && qd_view_screen(&A) == QD_SCR_MIX && !A.ui.mix_page, "the mix: F1 tapped, open");
    qd_ctl_labels(&A, &lb);
    CHECK(leds().button[F1] == QD_LED_OFF && strcmp(lb.s[QD_LBL_BTN0 + F1], "BACK") != 0, "the mix: F1 no back key "
          "(dark, its label %s)", lb.s[QD_LBL_BTN0 + F1]);
    btap(QD_B_PAGE);
    btap(F1);
    CHECK(A.ui.latch == QD_LATCH_MIX && A.ui.mix_page == 1, "the sends: F1 tapped, open");
    bdown(F1), run(10), bdown(F2), run(200), bup(F2), bup(F1), run(10);
    CHECK(A.ui.latch == QD_LATCH_MIX, "the sends: F1 + F2, open (no F3 over a main page)");
    btap(QD_B_TRACKS);
    btap(F1);
    CHECK(A.ui.latch == QD_LATCH_FX && qd_view_screen(&A) == QD_SCR_FX, "the effects: F1 tapped, open");
    btap(QD_B_SOUND);
    btap(F1);
    CHECK(qd_snd_open(&A) && qd_view_screen(&A) == QD_SCR_SOUND, "the sound editor: F1 tapped, open");
    btap(QD_B_REC);
    oct = A.oct;
    btap(F1);
    CHECK(qd_snd_keys(&A) && A.oct == oct - 1, "its keyboard: F1 the octave (%d -> %d), open", oct, A.oct);
    btap(QD_B_HOME);
    CHECK(!A.ui.latch, "HOME: closed");
    btap(QD_B_KEYS);
    btap(F1);
    CHECK(qd_mfx_open(&A) && A.ui.mode == QD_MODE_STEP, "MIDI FX, STEPS: F1 tapped, open");
    btap(QD_B_REC);
    oct = A.oct;
    btap(F1);
    CHECK(qd_mfx_open(&A) && A.ui.mode == QD_MODE_KEYS && A.oct == oct, "MIDI FX, KEYS: F1 tapped, open (no octave)");
    btap(QD_B_KEYS);
    CHECK(!qd_mfx_open(&A), "ARP: closed");
    btap(QD_B_HOME);
    /* the pop-ups */
    f3_down();
    bup(F2), bup(F1), run(10);
    CHECK(A.ui.latch == QD_LATCH_F3, "F1 + F2 in the step view: F3");
    btap(F1);
    CHECK(!A.ui.latch, "F3: F1 back");
    btap(QD_B_PATTERN);
    CHECK(A.ui.latch == QD_LATCH_TRK, "SEL: the track settings");
    btap(F1);
    CHECK(!A.ui.latch, "the track settings: F1 back");
    bdown(SHIFT), btap(QD_B_REC), bup(SHIFT), run(10);
    CHECK(A.ui.latch == QD_LATCH_REC, "SHIFT + REC: the record settings");
    btap(F1);
    CHECK(!A.ui.latch, "the record settings: F1 back");
    bdown(SHIFT), btap(QD_B_PATTERN), bup(SHIFT), run(10);
    CHECK(A.ui.latch == QD_LATCH_SET, "SHIFT + SEL: the global settings");
    btap(F1);
    CHECK(!A.ui.latch, "the global settings: F1 back");
    btap(QD_B_MIX);
    CHECK(A.ui.latch == QD_LATCH_PRJ, "SEQ tapped: the project");
    btap(F1);
    CHECK(!A.ui.latch, "the project: F1 back");
    turn(QD_K_PRESETS, 1);
    CHECK(A.ui.latch == QD_LATCH_BROWSE, "PRESETS: the browser");
    btap(F1);
    CHECK(!A.ui.latch, "the browser: F1 cancels");
}

/* P1.6h: wherever the W keys are the steps, F1 / F2 + W are the step view's: on the mix, the effects (the overview's
 * W1..W8 too), the sound editor, the MIDI FX screen's STEPS; a copy carries the step whole (its mix, send and sound
 * locks, its sound, its MIDI FX); F1 + B9 / B11 the page's CLEAR, UNDO; the pictures */
static void g_p16h_copy(void)
{
    ui_layer_t yv;
    ui_mfx_t v;
    int s, ok;
    start();
    wtap(0);
    qd_lock_set(PAT(), 0, 0, QD_LK_LEVEL, 90);
    qd_lock_set(PAT(), 0, 0, QD_LK_DELAY, 40);
    qd_lock_set(PAT(), 0, 0, QD_QP_EATK, 7);
    qd_step_set_sound(PAT(), 0, 0, 3);
    qd_step_set_mfx(PAT(), 0, 0, QD_SMFX_G1);
    CHECK(qd_step_mfx(PAT(), 0, 0) == 1, "(step 1: a note, LEVEL, DELAY and a sound lock, sound 3, MIDI FX group 1)");
    btap(QD_B_PAGE);
    bdown(F1);
    run(QD_CHORD_MS + 40);
    CHECK(qd_view_screen(&A) == QD_SCR_LAYER, "the mix, F1 held: F1's picture (the pages)");
    wtap(0);
    qd_view_layer(&A, &yv);
    CHECK(yv.f1_copied && yv.f1_src == 0, "W1: copied, its picture marks step 1");
    wtap(4);
    bup(F1);
    run(10);
#define SAME(d) (R(0, d).on && LK(0, d, QD_LK_LEVEL) == 90 && LK(0, d, QD_LK_DELAY) == 40 && LK(0, d, QD_QP_EATK) == 7 && \
                 ST(0, d)->sound == 3 && ST(0, d)->mfx == QD_SMFX_G1)
    CHECK(SAME(4) && A.ui.latch == QD_LATCH_MIX && !A.ui.nheld && qd_view_screen(&A) == QD_SCR_MIX,
          "W5: pasted whole (LEVEL, DELAY, a sound lock, the sound, the MIDI FX); the mix open");
    btap(F2);                                           /* (F1's copy loaded F2's hand, as Form's: emptied) */
    bdown(F2);
    run(QD_CHORD_MS + 40);
    CHECK(qd_view_screen(&A) == QD_SCR_LAYER, "F2 held: its picture (the hand)");
    wtap(4);
    qd_view_layer(&A, &yv);
    CHECK(!R(0, 4).on && yv.hand && yv.clip_type == QD_TRIG_NOTE, "F2 + W5: picked up (in the hand)");
    wtap(8);
    bup(F2);
    run(10);
    CHECK(SAME(8) && !R(0, 4).on, "W9: dropped there, whole");
    bdown(F1);
    run(QD_CHORD_MS + 40);
    black(B9);
    CHECK(!R(0, 0).on && !R(0, 8).on && A.ui.latch == QD_LATCH_MIX, "F1 + B9: the page cleared, the mix open");
    black(B11);
    bup(F1);
    run(10);
    CHECK(SAME(0) && SAME(8), "F1 + B11: undone");
    btap(QD_B_TRACKS);                                  /* the effects' overview: W1..W8 the tracks, but under F1 */
    bdown(F1);
    run(QD_CHORD_MS + 40);
    CHECK(qd_view_screen(&A) == QD_SCR_LAYER, "the effects, F1 held: its picture");
    wtap(0), wtap(1);
    bup(F1);
    run(10);
    CHECK(SAME(1) && A.sel == 0 && A.ui.latch == QD_LATCH_FX, "the overview, F1 + W1, W2: step 2 the copy (no track "
          "chosen)");
    btap(QD_B_SOUND);                                   /* the sound editor (its W keys the steps) */
    bdown(F1);
    run(QD_CHORD_MS + 40);
    CHECK(qd_view_screen(&A) == QD_SCR_LAYER, "the sound editor, F1 held: its picture");
    wtap(0), wtap(6);
    bup(F1);
    run(10);
    CHECK(SAME(6) && qd_snd_open(&A), "the editor: F1 + W1, W7: copied whole, open");
    bdown(F2);
    run(QD_CHORD_MS + 40);
    turn(QD_K_1, 1);
    CHECK(qd_view_screen(&A) == QD_SCR_SOUND, "F2 + a knob (the unlock): the editor's page, not F2's picture");
    bup(F2);
    run(10);
    btap(QD_B_HOME);
    btap(QD_B_KEYS);                                    /* MIDI FX, STEPS */
    bdown(F1);
    run(QD_TAP_MS + 50);
    wtap(0);
    qd_view_mfx(&A, &v);
    CHECK(v.f1 && v.fstep && v.st_copied && v.st_type == QD_TRIG_NOTE && v.f1_src < 0, "MIDI FX, STEPS: F1 + W1, the "
          "step's picture (copied, a note)");
    wtap(2);
    bup(F1);
    run(10);
    CHECK(SAME(2) && qd_mfx_open(&A) && !A.ui.mfx_wheld, "W3: pasted whole; the screen open, no step held");
    btap(F2);
    bdown(F2);
    run(30);
    wtap(2);
    qd_view_mfx(&A, &v);
    CHECK(v.f2 && v.fstep && v.f2_hand >= 0 && !R(0, 2).on, "F2 + W3: picked up (the hand's picture: the step)");
    wtap(3);
    bup(F2);
    run(10);
    CHECK(SAME(3) && !R(0, 2).on && qd_mfx_open(&A), "W4: dropped whole");
    bdown(F1);
    run(30);
    kdown(BK(B11)), run(20);
    kup(BK(B11)), run(10);
    bup(F1);
    run(10);
    CHECK(R(0, 2).on && !R(0, 3).on, "F1 + B11 there: undone");
    btap(QD_B_HOME);
    for (s = 0, ok = 1; s < 16; s++)                    /* (nothing left held anywhere) */
        ok &= !A.ui.nheld && !A.ui.wk_down;
    CHECK(ok, "nothing held after");
#undef SAME
}

/* P1.6h (the owner: "the level kept reverting"): the step view's latch selection is let go entering the mix; one made there
 * is kept (into the effects too) and counted in the head; a knob's lock: the frame from the key's press, a chip on
 * the part */
static void g_p16h_mixlock(void)
{
    ui_mix_t mv;
    ui_fx_t fv;
    start();
    wtap(0);
    btap(LFO);
    CHECK(A.ui.slatch && A.ui.nheld == 1, "the latch on, step 1 selected");
    btap(QD_B_PAGE);
    qd_view_mix(&A, &mv);
    CHECK(A.ui.slatch && !A.ui.nheld && !mv.step && !mv.nsel, "into the mix: the selection let go (no frame)");
    turn(QD_K_1, 2);
    CHECK(LK(0, 0, QD_LK_LEVEL) == -99999, "KNOB 1: the track's LEVEL, no lock");
    kdown(WK(1)), kdown(WK(2)), kdown(WK(3)), run(30);
    kup(WK(1)), kup(WK(2)), kup(WK(3)), run(10);
    qd_view_mix(&A, &mv);
    CHECK(A.ui.nheld == 3 && mv.step && mv.nsel == 3, "W2..W4 together: three selected, the frame, the head's 3");
    turn(QD_K_1, 1);
    qd_view_mix(&A, &mv);
    CHECK(LK(0, 1, QD_LK_LEVEL) >= 0 && LK(0, 3, QD_LK_LEVEL) >= 0 && mv.locked[0] && !mv.locked[1],
          "KNOB 1: LEVEL locks on them, its chip");
    btap(QD_B_TRACKS);
    qd_view_fx(&A, &fv);
    CHECK(A.ui.latch == QD_LATCH_FX && A.ui.nheld == 3 && fv.nsel == 3 && fv.step, "the effects: the mix's selection kept");
    btap(QD_B_PAGE);
    CHECK(A.ui.nheld == 3, "back to the mix: kept");
    btap(LFO);
    CHECK(!A.ui.slatch && !A.ui.nheld && A.ui.latch == QD_LATCH_MIX, "LFO there: the latch off, the mix open");
    btap(LFO);
    CHECK(A.ui.slatch && !A.ui.nheld && A.ui.latch == QD_LATCH_MIX, "LFO again: on, the mix open, nothing selected yet");
    btap(QD_B_HOME);
    btap(QD_B_PAGE);
    kdown(WK(5)), run(20);
    qd_view_mix(&A, &mv);
    CHECK(mv.step && !mv.nsel, "a W just pressed (not yet held): the frame already");
    turn(QD_K_2, 3);
    qd_view_mix(&A, &mv);
    CHECK(LK(0, 5, QD_LK_PAN) != -99999 && mv.locked[1] && mv.nsel == 1, "its first detent: PAN's lock, its chip");
    kup(WK(5)), run(10);
    btap(QD_B_HOME);
}

/* P1.6g (the owner's recording, in every view): stopped, REC held arms (latched, blinking), PLAY starts it (the sound
 * editor's keys recorded, a knob a lock: AUTOMATION ON); a tap stops it, playback going on. Playing in the step view,
 * REC held and let go with no knob: recording latched (a MIDI note recorded); a tap stops it. Playing, REC held + a
 * knob: a punch-in only while held, nothing latched; latched + REC held + a knob: the knob, the latch goes on */
static void g_p16g_rec(void)
{
    const int id = qd_step_quick_id(0);
    int s, k, any;
    start();
    A.proj.rec_dub = QD_TAKE_REPLACE;                   /* (2026-10-25: into the steps as they are; CAPTURE:
                                                         * g_capture) */
    btap(QD_B_SOUND);                                   /* the sound editor, its keyboard */
    btap(QD_B_REC);
    CHECK(qd_snd_open(&A) && qd_snd_keys(&A) && !A.ui.rec, "the editor: REC tapped: its keys (not recording)");
    live_arm();
    CHECK(qd_snd_open(&A) && A.ui.rec && A.ui.live_wait && !A.seq.playing, "REC held, stopped: armed, the editor stays");
    {
        uint32_t ph = half_period(QD_B_REC);
        CHECK(ph > 0, "armed: REC blinking (%u ms)", ph);
    }
    btap(QD_B_PLAY);
    CHECK(A.seq.playing && A.ui.rec && !A.ui.live_wait && leds().button[QD_B_REC] == QD_LED_LIT,
          "PLAY: playing, recording (REC lit)");
    while (track_pos0() < 2.1)
        run(1);
    kdown(WK(0)), run(60), kup(WK(0)), run(10);
    for (any = 0, s = 1; s <= 3; s++)
        any |= R(0, s).on;
    CHECK(any, "the editor's key played: recorded on the step it fell on");
    turn(QD_K_3, 1);                                    /* (the editor's KNOB 3 on its page: AUTOMATION ON) */
    for (any = 0, s = 0; s < 16; s++) {
        int nl = 0;
        qd_locks(PAT(), 0, s, &nl);
        any |= nl > 0;
    }
    CHECK(any, "a knob turned: a lock on the step playing");
    btap(QD_B_REC);
    CHECK(!A.ui.rec && A.seq.playing && qd_snd_open(&A), "REC tapped: the recording stops, playback goes on");
    btap(QD_B_HOME);
    /* playing, the step view: REC held, let go, no knob: latched */
    bdown(QD_B_REC);
    run(QD_REC_HOLD_MS + 50);
    CHECK(!A.ui.rec, "playing, REC held: not yet");
    bup(QD_B_REC);
    run(10);
    CHECK(A.ui.rec && !A.ui.live_wait && A.ui.mode == QD_MODE_STEP, "let go (no knob): recording, latched, the step view");
    to_step(5);
    qd_app_midi(&A, 0x90, 72, 100);
    run(60);
    qd_app_midi(&A, 0x80, 72, 0);
    run(10);
    for (any = 0, s = 4; s <= 7; s++)
        any |= R(0, s).on && R(0, s).notes[0] == 72;
    CHECK(any, "a MIDI note (the track's channel): recorded");
    btap(QD_B_REC);
    CHECK(!A.ui.rec && A.seq.playing, "REC tapped: stopped recording, playing on");
    /* playing: REC held + a knob: a punch-in only */
    bdown(QD_B_REC);
    run(QD_REC_HOLD_MS + 50);
    k = ph();
    turn(QD_K_1, -3);
    k = LK(0, k, id);
    bup(QD_B_REC);
    run(10);
    CHECK(k != -99999 && !A.ui.rec, "REC held + KNOB 1: a lock (punch-in); let go: nothing latched");
    run(300);
    s = ph();
    turn(QD_K_1, -3);
    CHECK(LK(0, s, id) == -99999, "after it, KNOB 1 records nothing");
    /* latched, REC held + a knob: the knob, the latch goes on */
    bdown(QD_B_REC), run(QD_REC_HOLD_MS + 50), bup(QD_B_REC), run(10);
    CHECK(A.ui.rec, "latched again");
    bdown(QD_B_REC);
    run(QD_REC_HOLD_MS + 50);
    s = ph();
    turn(QD_K_1, -2);
    bup(QD_B_REC);
    run(10);
    CHECK(LK(0, s, id) != -99999 && A.ui.rec, "latched, REC held + KNOB 1: the lock; let go: still recording");
    btap(QD_B_PLAY);
    CHECK(!A.seq.playing && !A.ui.rec, "PLAY: stopped, the recording ends");
}

/* P1.6g: the FILTER page's ALGORITHM shows the machines' popup a second; RATIO B accelerates (a fast spin sweeps B1);
 * the sound editor follows the step playing with the latch off (its lock shown), not with the latch on; the ARP's
 * picture: the PATTERN's real walk (DOWN falls, UP DN rises then falls, AS PLAYED the order played), the lanes' rests
 * gaps and transposes offsets */
static void g_p16g_misc(void)
{
    static ui_snd_t sv;
    static ui_mfx_t mv;
    int16_t *pt;
    int k, b0, i, ok;
    start();
    btap(QD_B_SOUND);
    A.ui.snd_page = qd_snd_page_of("FILTER");
    turn(QD_K_ALGO, 1);
    qd_view_snd(&A, &sv);
    CHECK(sv.fpop && !A.ui.snd_fpop, "FILTER, ALGORITHM turned: the machines' popup shown (not held)");
    run(QD_FPOP_SHOW_MS + 20);
    qd_view_snd(&A, &sv);
    CHECK(!sv.fpop, "a second after the last detent: gone");
    /* RATIO B's acceleration */
    A.ui.snd_page = qd_snd_page_of("RATIO");
    pt = A.model.t[0].patch;
    qd_player_live_edit(&A.player, 0, QD_QP_RB1, 0);
    qd_player_live_edit(&A.player, 0, QD_QP_RB2, 0);
    run(500);
    b0 = pt[QD_QP_RB1];
    for (k = 0; k < 24; k++) {                          /* a fast spin: a detent every 8 ms */
        qd_app_knob(&A, QD_K_3, 1);
        run(8);
    }
    CHECK(pt[QD_QP_RB1] - b0 >= 8, "RATIO B, a fast spin of 24 detents: B1 swept %d places (accelerated)", pt[QD_QP_RB1] - b0);
    /* the editor follows the playing step's locks, the latch off */
    A.ui.snd_page = qd_snd_page_of("FILTER");
    btap(QD_B_HOME);
    for (k = 0; k < 16; k += 4)
        qd_step_set_trig(PAT(), 0, k, QD_TRIG_NOTE);
    qd_lock_set(PAT(), 0, 4, QD_QP_FREQ, 3000);
    btap(QD_B_SOUND);
    btap(QD_B_PLAY);
    ok = 0;
    for (i = 0; i < 2000; i++) {
        qd_view_snd(&A, &sv);
        ok |= sv.follow && sv.v[0] == 3000 && sv.locked[0];
        run(1);
    }
    CHECK(ok, "playing, the latch off, nothing held: the editor shows step 5's FREQ lock as it plays");
    btap(QD_B_HOME);
    btap(LFO);
    wtap(A.ui.held[0] % 16);                            /* (nothing selected) */
    btap(QD_B_SOUND);
    for (ok = 1, i = 0; i < 2000; i++) {
        qd_view_snd(&A, &sv);
        ok &= !sv.follow && sv.v[0] == pt[QD_QP_FREQ];
        run(1);
    }
    CHECK(ok, "the latch on, nothing selected: the track's values, still");
    {   /* the latch off again, an LFO on FREQ: the editor shows FREQ as heard, moving (and only following) */
        int seen[8], ns = 0, j, moved = 0;
        btap(QD_B_HOME);
        btap(QD_B_SOUND);
        qd_player_live_edit(&A.player, 0, QD_QP_LFO(0, QD_QL_DEST), (int16_t)qd_quad_dest_of(QD_QP_FREQ, 0));
        qd_player_live_edit(&A.player, 0, QD_QP_LFO(0, QD_QL_DEPTH), 3200);
        qd_player_live_edit(&A.player, 0, QD_QP_LFO(0, QD_QL_SPEED), 6000);
        for (i = 0; i < 2000; i++) {
            qd_view_snd(&A, &sv);
            if (sv.follow && !sv.locked[0] && sv.v[0] != pt[QD_QP_FREQ])
                moved = 1;
            for (j = 0; j < ns && seen[j] != sv.v[0]; j++)
                ;
            if (j == ns && ns < 8)
                seen[ns++] = sv.v[0];
            run(1);
        }
        CHECK(moved && ns >= 4, "playing, following, an LFO on FREQ: the shown FREQ moves with it (%d values)", ns);
        qd_player_live_edit(&A.player, 0, QD_QP_LFO(0, QD_QL_DEST), 0);
    }
    btap(QD_B_PLAY);
    btap(QD_B_HOME);                                    /* (HOME: the latch off too) */
    /* the ARP's walk */
    btap(QD_B_KEYS);
    turn(QD_K_ALGO, 1);
    qd_mfx_slot_init(&A.proj.mfx[0].slot[0], QD_MFX_ARP);
    A.proj.mfx[0].slot[0].v[QD_MA_RANGE] = 1;
    A.proj.mfx[0].slot[0].v[QD_MA_PAT] = QD_AP_DN;
    run(300);
    qd_view_mfx(&A, &mv);
    CHECK(mv.nseq == 3 && mv.seq[0] == 67 && mv.seq[1] == 64 && mv.seq[2] == 60, "DOWN on C E G: G E C (falls)");
    A.proj.mfx[0].slot[0].v[QD_MA_PAT] = QD_AP_UPDN;
    run(10);
    qd_view_mfx(&A, &mv);
    CHECK(mv.nseq == 4 && mv.seq[0] == 60 && mv.seq[2] == 67 && mv.seq[3] == 64 && mv.morph < 1.f,
          "UP DN: C E G E (rises, falls), morphing from DOWN");
    A.proj.mfx[0].slot[0].v[QD_MA_MLEN] = 2;
    A.proj.mfx[0].slot[0].lane[0][1] = QD_AL_REST;
    A.proj.mfx[0].slot[0].v[QD_MA_TLEN] = 1;
    A.proj.mfx[0].slot[0].lane[1][0] = 12;
    run(10);
    qd_view_mfx(&A, &mv);
    CHECK(mv.gap[1] && mv.gap[3] && !mv.gap[0] && mv.seq[0] == 72, "the lanes: the MOD lane's rests gaps, TRANSPOSE +12");
    btap(QD_B_HOME);
}

/* P1.5l: the editor's black keys regrouped: B1 TRIG / KEYS, B2 OPERATORS, B3 OP ENVELOPES, B4 FILTERS, B5 ENVELOPES,
 * B6..B8 the LFOs; SELECT in that order; a fresh start on RATIO; the LEDs */
static void g_p15l_groups(void)
{
    static const char *const FIRST[8] = {"TRIG", "RATIO", "ENV A", "FILTER", "AMP", "LFO 1", "LFO 2", "LFO 3"};
    static const char *const ORDER[QD_SND_NPAGE] = {"TRIG", "KEYS", "PORT", "MOD", "RATIO", "TONE", "OFFSET", "ENV A", "ENV B",
        "FILTER", "F ENV", "BASE", "AMP", "LEVEL", "CRUSH", "LFO 1", "LFO 1 WAVE", "LFO 2", "LFO 2 WAVE", "LFO 3", "LFO 3 WAVE"};
    static ui_snd_t sv;
    int b, ok = 1, p;
    start();
    btap(QD_B_SOUND);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "RATIO"), "a fresh start: RATIO");
    for (b = 0; b < 8; b++) {
        black(b);
        ok &= !strcmp(QD_SND_PAGES[A.ui.snd_page].name, FIRST[b]) && leds().key[BK(b)] == QD_LED_LIT &&
              leds().key[BK(b == 0 ? 1 : 0)] == QD_LED_DIM;
    }
    CHECK(ok, "B1..B8: TRIG, RATIO, ENV A, FILTER, AMP, LFO 1, LFO 2, LFO 3 (each lit, the others dim)");
    black(B1);
    black(B1);
    qd_view_snd(&A, &sv);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "KEYS") && sv.npg == 4 && sv.ipg == 1,
          "B1 again: KEYS (four dots: E1's PORT, E4's MOD)");
    black(B1);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "PORT"), "B1 again: PORT (E1)");
    black(B1);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "MOD"), "B1 again: MOD (E4)");
    black(B1);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "TRIG"), "B1 again: round to TRIG");
    for (ok = 1, p = 0; p < QD_SND_NPAGE; p++)
        ok &= !strcmp(QD_SND_PAGES[p].name, ORDER[p]);
    CHECK(ok, "SELECT's order: TRIG KEYS PORT MOD RATIO TONE OFFSET ENV A ENV B FILTER F ENV BASE AMP LEVEL CRUSH LFO 1 .. LFO 3 "
          "WAVE");
    A.ui.snd_page = qd_snd_page_of("CRUSH");
    turn(QD_K_SELECT, 1);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "LFO 1"), "SELECT from CRUSH (E3): LFO 1");
    btap(QD_B_SOUND);
    btap(QD_B_SOUND);
    CHECK(!strcmp(QD_SND_PAGES[A.ui.snd_page].name, "LFO 1"), "closed and opened: the last page used");
    btap(QD_B_HOME);
}


/* P1.5m (the owner: "the F1 / F2 shortcuts do not work when the step lock is turned on"): every F gesture with the step
 * latch on and a selection, in VALUES and in MODES, as without the latch; the selection untouched; F1 / F2 tapped the
 * selection's octave (NOTE) / bank (SOUND), as for a held step */
static void g_p15m_flatch(void)
{
    int lm;
    for (lm = 0; lm < 2; lm++) {
        const char *M = lm ? "MODES" : "VALUES";
        int o0, b0;
        start();
        A.ui.lmode = lm;
        wtap(0), wtap(2), wtap(4);
        wtap(1);                                        /* (the last tapped: the latch starts on step 2) */
        btap(LFO);
        CHECK(A.ui.slatch && A.ui.nheld == 1 && A.ui.held[0] == 1, "%s: the latch on, step 2 selected", M);
        bdown(F1);                                      /* F1 + W: copy, then paste */
        run(300);                                       /* (P1.5n: alone over the latch, its map after a tap's time) */
        CHECK(qd_view_screen(&A) == QD_SCR_LAYER && qd_ctl_layer(&A) == QD_LAYER_F1, "%s: F1 held: its map", M);
        wtap(4);
        wtap(7);
        bup(F1);
        run(20);
        CHECK(R(0, 7).on && A.ui.nheld == 1 && A.ui.held[0] == 1 && A.ui.slatch,
              "%s: F1 + W5 + W8: step 5 copied onto 8, the selection untouched", M);
        bdown(F2);                                      /* F2 + W: pick up, drop */
        run(80);
        wtap(7);
        wtap(9);
        bup(F2);
        run(20);
        CHECK(R(0, 9).on && A.ui.held[0] == 1, "%s: F2 (loaded by F1's copy) + W8 + W10: pasted, the selection kept", M);
        bdown(F2);                                      /* (the hand empty now: Form's pick-up / drop) */
        run(80);
        wtap(9);
        wtap(11);
        bup(F2);
        run(20);
        CHECK(!R(0, 9).on && R(0, 11).on && A.ui.held[0] == 1, "%s: F2 + W10 + W12: moved to 12, the selection kept", M);
        bdown(F1);                                      /* F1 + B9: the page cleared, F1 + B11: undone */
        run(80);
        black(B9);
        bup(F1);
        run(20);
        CHECK(!R(0, 0).on && !R(0, 11).on, "%s: F1 + B9: the page cleared", M);
        bdown(F1);
        run(80);
        black(B11);
        bup(F1);
        run(20);
        CHECK(R(0, 0).on && R(0, 11).on && A.ui.slatch, "%s: F1 + B11: undone", M);
        bdown(F1);                                      /* F1 + B2: page 2 edited */
        run(80);
        black(B2);
        bup(F1);
        run(20);
        CHECK(A.ui.page[0] == 1 && A.ui.slatch, "%s: F1 + B2: page 2", M);
        bdown(F1);
        run(80);
        black(B1);
        bup(F1);
        run(20);
        bdown(F1);                                      /* F3: both, latched; W the page's end */
        run(10);
        bdown(F2);
        run(10);
        bup(F1), bup(F2);
        run(20);
        CHECK(A.ui.latch == QD_LATCH_F3 && qd_ctl_layer(&A) == QD_LAYER_F3, "%s: F1 + F2: F3 open", M);
        wtap(11);
        CHECK(PAT()->trk[0].pend[0] == 11 && A.ui.held[0] == 1, "%s: F3 + W12: the page's end 12, the selection kept", M);
        btap(F1);
        CHECK(!A.ui.latch && A.ui.slatch, "%s: OCT-: F3 closed, the latch stays", M);
        mode(M_NOTE);                                   /* (MODES: B1 is NOTE; VALUES: B1 a note value: the mode by LFO) */
        if (!lm) {
            bdown(LFO);
            run(20);
            black(B1);
            bup(LFO);
            run(20);
        }
        o0 = A.ui.entry_oct;
        btap(F2);
        CHECK(A.ui.entry_oct == o0 + 1 && A.ui.slatch && A.ui.nheld == 1, "%s: NOTE, F2 tapped: the selection's octave up", M);
        btap(F1);
        CHECK(A.ui.entry_oct == o0, "%s: F1 tapped: down again", M);
        bdown(LFO);
        run(20);
        black(B10);                                     /* SOUND */
        bup(LFO);
        run(20);
        b0 = A.ui.sound_bank;
        btap(F2);
        CHECK(A.ui.hmode == M_SOUND && A.ui.sound_bank == b0 + 1, "%s: SOUND, F2 tapped: the next ten", M);
        btap(QD_B_HOME);
    }
}


/* P1.5n: (1) the latch's OCT tap: the octave / bank takeover its whole time, in every view (EDIT too), no F map for a
 * tap; (2) NOTE (P1.5o): SELECT moves all the held or selected steps' notes together, in every layout and view; (3) LFO's
 * LED: full in VALUES, breathing 60 .. 100 % in MODES, off without the latch; (4) MIX on TONE and RATIO, one value */
static void g_p15n(void)
{
    static const uint8_t CH[3] = {60, 64, 67};
    static ui_snd_t sv;
    int v, f, lay, ok, t, mn, mx;
    for (v = 0; v < 3; v += 2) {                        /* (1) PIANO ROLL and EDIT */
        start();
        A.ui.view = v;
        wtap(0), wtap(1);
        btap(LFO);
        t = A.ui.entry_oct;
        bdown(F2);
        run(120);
        CHECK(qd_view_screen(&A) != QD_SCR_LAYER, "view %d: an OCT tap over the latch: no F2 map", v);
        bup(F2);
        run(5);
        ok = A.ui.entry_oct == t + 1 && A.ui.tk == UI_TK_OCTAVE && qd_view_overlay(&A);
        run(495);
        ok &= A.ui.tk == UI_TK_OCTAVE && qd_view_overlay(&A);
        run(550);
        ok &= A.ui.tk == UI_TK_OCTAVE && qd_view_overlay(&A);
        CHECK(ok, "view %d: the octave's takeover 5, 500 and 1050 ms after the tap", v);
        run(200);
        CHECK(A.ui.tk == QD_TK_NONE, "view %d: .. gone after its time", v);
        bdown(LFO);
        run(20);
        black(B10);                                     /* SOUND */
        bup(LFO);
        run(20);
        t = A.ui.sound_bank;
        btap(F2);
        run(5);
        ok = A.ui.sound_bank == t + 1 && A.ui.tk == UI_TK_OCTAVE && qd_view_overlay(&A);
        run(1000);
        ok &= A.ui.tk == UI_TK_OCTAVE && qd_view_overlay(&A);
        CHECK(ok, "view %d: SOUND, an OCT tap: the next ten, its takeover its whole time", v);
        btap(QD_B_HOME);
    }
    for (lay = 0; lay < 1; lay++)                       /* (2) layout A; PIANO ROLL, EDIT; a finger, the latch */
        for (v = 0; v < 3; v += 2)
            for (f = 0; f < 2; f++) {
                start();
                A.ui.view = v;
                PAT()->trk[0].scale = QD_SCALE_MAJOR, PAT()->trk[0].root = 0, PAT()->trk[0].key_global = 0;
                wtap(1);
                qd_step_set_notes(PAT(), 0, 1, CH, 3);
                if (f)
                    btap(LFO);
                else {
                    kdown(WK(1));
                    run(300);
                }
                turn(QD_K_SELECT, 1);
                ok = R(0, 1).note[0] == 62 && R(0, 1).note[1] == 65 && R(0, 1).note[2] == 69;
                turn(QD_K_SELECT, -2);
                ok &= R(0, 1).note[0] == 59 && R(0, 1).note[1] == 62 && R(0, 1).note[2] == 65;
                CHECK(ok, "layout %c, view %d, %s: SELECT moves the chord a scale step up, two down (C E G -> D F A -> "
                      "B D F) (P1.5o)", 'A' + lay, v, f ? "the latch's selection" : "a finger");
                turn(QD_K_SELECT, 80);
                mx = R(0, 1).note[2];
                ok = mx <= 127 && mx > 120 && R(0, 1).note[2] - R(0, 1).note[0] >= 6 &&
                     R(0, 1).note[1] > R(0, 1).note[0] && R(0, 1).note[2] > R(0, 1).note[1];
                turn(QD_K_SELECT, -160);
                mn = R(0, 1).note[0];
                ok &= mn >= 1 && mn < 6 && R(0, 1).note[1] > mn && R(0, 1).note[2] > R(0, 1).note[1];
                CHECK(ok, "layout %c, view %d, %s: the chord stops at the top (%d) and the bottom (%d), its shape kept",
                      'A' + lay, v, f ? "latch" : "finger", mx, mn);
                mn = R(0, 1).note[0];
                turn(QD_K_ALGO, 1);
                CHECK(R(0, 1).note[0] == mn, "layout %c, view %d, %s: ALGORITHM moves no note (%s)", 'A' + lay, v,
                      f ? "latch" : "finger", f && !lay ? "A: the cursor" : "its role");
                if (!f)
                    kup(WK(1));
                btap(QD_B_HOME);
            }
    start();                                            /* (3) LFO's LED */
    CHECK(leds().button[LFO] == QD_LED_OFF, "the latch off: LFO dark");
    wtap(0);
    btap(LFO);
    for (ok = 1, t = 0; t < 10; t++, run(150))
        ok &= leds().button[LFO] == 255;
    CHECK(ok, "VALUES: LFO full (255, the panel's top), steady");
    A.ui.lmode = QD_LMODE_MODES;
    for (mn = 255, mx = 0, t = 0; t < 160; t++, run(10)) {
        const int l = leds().button[LFO];
        mn = l < mn ? l : mn, mx = l > mx ? l : mx;
    }
    CHECK(mn >= 153 && mn <= 156 && mx == 255, "MODES: LFO breathing between 60 %% and full (%d .. %d)", mn, mx);
    btap(QD_B_HOME);
    start();                                            /* (4) MIX on TONE and RATIO */
    btap(QD_B_SOUND);
    A.ui.snd_page = qd_snd_page_of("TONE");
    turn(QD_K_4, 30);
    A.ui.snd_page = qd_snd_page_of("RATIO");
    qd_view_snd(&A, &sv);
    ok = sv.mix == -34 && !strcmp(sv.lab[3], "MIX");
    turn(QD_K_4, 10);
    A.ui.snd_page = qd_snd_page_of("TONE");
    qd_view_snd(&A, &sv);
    CHECK(ok && sv.has_mix && sv.mix == -24, "MIX on TONE moves RATIO's, and back (-34, -24)");
    wtap(3);
    whold(3);
    turn(QD_K_4, 5);
    qd_view_snd(&A, &sv);
    ok = sv.locked[3];
    A.ui.snd_page = qd_snd_page_of("RATIO");
    qd_view_snd(&A, &sv);
    CHECK(ok && sv.locked[3] && LK(0, 3, QD_QP_MIX) == -19, "a step held, MIX locked on TONE: the same chip on RATIO");
    wrel(3);
    btap(QD_B_HOME);
}


/* P1.5o: locks over a sound lock (track < the step's sound lock < its p-locks; EDIT shows them so, a knob p-locks, the
 * pool's sound untouched, a new sound lock keeps the p-locks); F2 + a knob unlocks (a step, the track; the editor and
 * P-LOCK); the default's VEL value green */
static int col_in(int x0, int y0, int x1, int y1, uint32_t col)
{
    static uint16_t lcd[QD_W * QD_H];
    const uint8_t *b = (const uint8_t *)lcd;
    int x, y;
    qd_app_frame(&A, lcd);
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            const uint16_t v = (uint16_t)(b[2 * (y * QD_W + x)] << 8 | b[2 * (y * QD_W + x) + 1]);
            const uint32_t c = (uint32_t)((v >> 11) & 31u) << 19 | (uint32_t)((v >> 5) & 63u) << 10 | (uint32_t)(v & 31u) << 3;
            if (near_col(c, col))
                return 1;
        }
    return 0;
}
static void g_p15o_locks(void)
{
    static ui_snd_t sv;
    int16_t f0;
    const int16_t *pool;
    start();
    wtap(2), wtap(5);
    qd_step_set_sound(PAT(), 0, 2, 12);                 /* step 3: a sound lock, pool sound 12 */
    pool = qd_sound_patch(&A.proj, 12);
    f0 = pool[QD_QP_FREQ];
    btap(QD_B_SOUND);
    A.ui.snd_page = qd_snd_page_of("FILTER");
    whold(2);
    qd_view_snd(&A, &sv);
    CHECK(sv.v[0] == pool[QD_QP_FREQ] && sv.tv[0] == A.model.t[0].patch[QD_QP_FREQ], "EDIT, the step held: the locked "
          "sound's FREQ shown (%d), the track's behind it", sv.v[0]);
    turn(QD_K_1, -3);
    qd_view_snd(&A, &sv);
    CHECK(LK(0, 2, QD_QP_FREQ) == f0 - 300 && sv.v[0] == f0 - 300 && sv.locked[0] && pool[QD_QP_FREQ] == f0,
          "KNOB 1: a p-lock on the step from the locked sound's FREQ (-3.00), the pool's sound untouched");
    qd_step_set_sound(PAT(), 0, 2, 13);
    CHECK(LK(0, 2, QD_QP_FREQ) == f0 - 300, "a new sound lock: the p-lock kept");
    bdown(F2);
    run(20);
    turn(QD_K_1, 1);
    bup(F2);
    run(20);
    CHECK(LK(0, 2, QD_QP_FREQ) == -99999 && A.ui.cf_on && !strcmp(A.ui.cf_big, "FREQ") && !strcmp(A.ui.cf_small, "unlocked")
              && A.ui.cf_col == UI_K1, "F2 + KNOB 1: the step's FREQ unlocked (\"FREQ unlocked\" in KNOB 1's blue)");
    turn(QD_K_1, 2);
    wrel(2);
    whold(5);
    turn(QD_K_1, 2);
    wrel(5);
    CHECK(LK(0, 2, QD_QP_FREQ) != -99999 && LK(0, 5, QD_QP_FREQ) != -99999, "two steps' FREQ locked");
    bdown(F2);
    run(20);
    turn(QD_K_1, -1);
    bup(F2);
    run(20);
    CHECK(LK(0, 2, QD_QP_FREQ) == -99999 && LK(0, 5, QD_QP_FREQ) == -99999,
          "nothing held, F2 + KNOB 1: FREQ unlocked on every step of the track");
    btap(QD_B_HOME);
    mode(M_PLOCK);                                      /* P-LOCK: the same */
    whold(5);
    turn(QD_K_3, 4);
    CHECK(LK(0, 5, QD_QP_HARM) == A.model.t[0].patch[QD_QP_HARM] + 400, "P-LOCK, step 6 held: HARM locked");
    bdown(F2);
    run(20);
    CHECK(qd_ctl_layer(&A) == QD_LAYER_NONE, "P-LOCK: F2 held is no layer");
    turn(QD_K_3, 1);
    bup(F2);
    run(20);
    wrel(5);
    CHECK(LK(0, 5, QD_QP_HARM) == -99999, "P-LOCK, F2 + KNOB 3: HARM unlocked");
    btap(QD_B_HOME);
    mode(M_VEL);                                        /* the default's VEL value green */
    kdown(BK(B2));
    run(300);
    CHECK(qd_view_screen(&A) == QD_SCR_DEFAULT && col_in(120, 30, 236, 100, UI_MODE_COL[UI_F_VEL]),
          "the VEL key held: the default's value in VEL's green");
    kup(BK(B2));
    run(20);
}

/* P1.5c: undo / redo (a gesture a level, eight, the redo tail dropped by a new edit, a project load forgets), the
 * snapshot, SHIFT's keys (copy / paste the pattern, tap tempo, the metronome and its level, double, clear twice; only
 * the shortcuts lit), randomise in the editor (the dice's page at 600 ms, the bottle's sound at 1500 ms, released early
 * nothing, the values in their rules, a step held: locks, undone) */
static void shift_black(int b)                          /* SHIFT + a black key */
{
    bdown(SHIFT);
    run(20);
    black(b);
    bup(SHIFT);
    run(20);
}
static void undo_redo(int dir)
{
    bdown(SHIFT);
    run(20);
    btap(dir < 0 ? F1 : F2);
    bup(SHIFT);
    run(20);
}
/* P2.1c: the one log (the device's): the snapshot and the pattern clipboard stored while a gesture is open (its
 * copies moved under them), every level undone exactly, the snapshot recalled exactly; a dense clipboard takes its
 * room from the oldest levels, the rest still exact */
static void g_p21c_one_log(void)
{
    static qd_pattern_t st[8], snap, dense;
    int e, n, ok = 1, t, s2;
    start();
    for (e = 0; e < 6; e++) {
        st[e] = *PAT();
        qd_undo_input(&A, 0);
        qd_step_on(PAT(), 0, e);
        qd_step_set_vel(PAT(), 0, e, 10 + e);
        qd_undo_close(&A);
    }
    st[6] = *PAT();
    qd_undo_input(&A, 0);                               /* a gesture: the snapshot and the clipboard stored in it */
    qd_step_on(PAT(), 1, 0);
    snap = *PAT();
    CHECK(qd_hist_store(&A, 0) && qd_hist_store(&A, 1), "the snapshot and the clipboard stored with a gesture open");
    qd_step_on(PAT(), 1, 1);
    qd_undo_close(&A);
    n = qd_undo_count(&A, -1);
    for (e = 6; e >= 0 && qd_undo_count(&A, -1) > 0; e--) {
        qd_undo_step(&A, -1);
        ok &= !memcmp(PAT()->step, st[e].step, sizeof st[e].step);
    }
    CHECK(n == 7 && ok, "seven levels (%d), each undone exactly (the gesture's copies moved under the snapshot)", n);
    CHECK(qd_hist_recall(&A, 0) && !memcmp(PAT()->step, snap.step, sizeof snap.step), "the snapshot recalled exactly");
    static qd_pattern_t big[4];
    for (e = 0; e < 4; e++) {                           /* four big levels (48 steps' velocities each) */
        big[e] = *PAT();
        qd_undo_input(&A, 0);
        for (s2 = 0; s2 < 48; s2++) {
            qd_step_on(PAT(), 2 + e, s2);
            qd_step_set_vel(PAT(), 2 + e, s2, 1 + (e * 37 + s2 * 11) % 126);
        }
        qd_undo_close(&A);
    }
    qd_pat_init(&dense);
    for (t = 0; t < QD_PAT_NTRACK; t++)                 /* (a dense pattern for the clipboard) */
        for (s2 = 0; s2 < QD_NSTEP; s2++) {
            qd_step_on(&dense, t, s2);
            qd_step_set_vel(&dense, t, s2, 1 + (t * 37 + s2 * 11) % 126);
            if (s2 % 4 == t % 4)
                qd_lock_set(&dense, t, s2, QD_QP_HARM, s2 * 3 + t);
        }
    {
        qd_pattern_t keep = *PAT();
        const int before = qd_undo_count(&A, -1);
        int left, ok2 = 1;
        *PAT() = dense;
        CHECK(qd_hist_store(&A, 1), "a dense pattern copied");
        *PAT() = keep;
        left = qd_undo_count(&A, -1);
        for (e = 3; e >= 4 - left; e--) {
            qd_undo_step(&A, -1);
            ok2 &= !memcmp(PAT()->step, big[e].step, sizeof big[e].step);
        }
        printf("    (the one log: a dense clipboard of %u bytes: %d of %d levels kept)\n",
#ifdef QD_UNDO_SLOTS
               (unsigned)A.undo.pclip_len,
#else
               0u,
#endif
               left, before);
#ifdef QD_UNDO_SLOTS
        CHECK(left <= before && left >= 1 && ok2, "its room from the oldest levels if it needs it (six tracks: %d of %d "
              "left), those undone exactly", left, before);
#else
        CHECK(left == before && ok2, "the host: every level kept (%d), undone exactly", left);
#endif
        CHECK(qd_hist_recall(&A, 0) && !memcmp(PAT()->step, snap.step, sizeof snap.step) && qd_hist_recall(&A, 1) &&
                  !memcmp(PAT()->step, dense.step, sizeof dense.step),
              "the snapshot and the dense clipboard recalled exactly");
    }
}

/* P2.1: undo at the device's sizes (a 16 KB log): a pattern too dense to keep with the levels: its gesture is no
 * level (P2.1c: its snapshot, 16-byte steps, now fits the log, and then fills it); the host keeps both. An ordinary
 * pattern: undo as before at both sizes */
static void g_p21_undo_log(void)
{
    int t, s, n;
    start();
    qd_step_on(PAT(), 0, 0);
    wtap(1);
    run(QD_UNDO_QUIET_MS + 50);
    n = qd_undo_count(&A, -1);
    bdown(SHIFT), black(B1), bup(SHIFT), run(QD_UNDO_QUIET_MS + 50);
    CHECK(n == 1 && A.undo.snap_ok, "an ordinary pattern: an edit, one level; its snapshot stored");
    for (t = 0; t < QD_PAT_NTRACK; t++)                 /* (every step of every track, each with its own lock) */
        for (s = 0; s < QD_NSTEP; s++) {
            qd_step_on(PAT(), t, s);
            qd_step_set_vel(PAT(), t, s, 1 + (t * 37 + s * 11) % 126);
            qd_lock_set(PAT(), t, s, QD_QP_HARM, s * 3 + t);
            if (t < (QD_NLOCK - QD_PAT_NTRACK * QD_NSTEP) / QD_NSTEP)   /* (six tracks: a second lock on the first
                                                         * tracks' steps, the pool's 1024 locks full again) */
                qd_lock_set(PAT(), t, s, QD_QP_FREQ, 1000 + s * 5 + t);
        }
    A.undo.snap_ok = 0;
    bdown(SHIFT), black(B1), bup(SHIFT), run(10);
    n = qd_undo_count(&A, -1);
    wtap(2);
    run(QD_UNDO_QUIET_MS + 50);
#ifdef QD_UNDO_SLOTS
    CHECK(A.undo.snap_ok && qd_undo_count(&A, -1) == n,
          "the device's log: a full pattern (768 steps, 1024 locks): its snapshot stored (P2.1c: 16-byte steps), its "
          "edit no level (the log holds the snapshot), the levels before it kept (%d of %d)", qd_undo_count(&A, -1), n);
#else
    CHECK(A.undo.snap_ok && qd_undo_count(&A, -1) == n + 1, "the host: a full pattern: its snapshot, its edit a level");
#endif
}

static void g_p15c_undo(void)
{
    static qd_pattern_t p0, p1;
    int i, ok, n0;
    start();
    wtap(0);
    run(600);
    memcpy(&p0, PAT(), sizeof p0);
    mode(M_VEL);                                        /* a knob gesture: VEL turned in three bursts < 500 ms apart */
    run(600);
    n0 = qd_undo_count(&A, -1);
    memcpy(&p0, PAT(), sizeof p0);
    whold(0);
    for (i = 0; i < 3; i++) {
        turn(QD_K_ALGO, -4);
        run(200);
    }
    wrel(0);
    run(600);
    memcpy(&p1, PAT(), sizeof p1);
    CHECK(qd_undo_count(&A, -1) == n0 + 1 && memcmp(PAT(), &p0, sizeof p0),
          "three turns under 500 ms apart: one level (%d -> %d)", n0, qd_undo_count(&A, -1));
    undo_redo(-1);
    CHECK(!memcmp(PAT(), &p0, sizeof p0) && A.ui.tk == QD_TK_UNDO && A.ui.undo_dir < 0 && A.ui.undo_kind == QD_UK_KNOB,
          "SHIFT + F1: the pattern as before the turns; UNDO's takeover, a knob's glyph");
    CHECK(qd_view_overlay(&A), "UNDO drawn over the step view");
    undo_redo(1);
    CHECK(!memcmp(PAT(), &p1, sizeof p1) && A.ui.undo_dir > 0, "SHIFT + F2: redone, byte for byte");
    btap(QD_B_HOME);
    run(600);
    start();                                            /* a paste, a clear: each its own level, its kind */
    wtap(0), wtap(3), wtap(9);
    run(600);
    memcpy(&p0, PAT(), sizeof p0);
    shift_black(B4);                                    /* the pattern copied */
    wtap(5);
    run(600);
    memcpy(&p1, PAT(), sizeof p1);
    shift_black(B11);
    shift_black(B11);                                   /* the track cleared */
    run(600);
    CHECK(!R(0, 3).on && !R(0, 9).on && A.ui.undo_kind >= -1, "SHIFT + B11 twice: the track cleared");
    undo_redo(-1);
    CHECK(!memcmp(PAT(), &p1, sizeof p1) && A.ui.undo_kind == QD_UK_CLEAR, "undo: the clear back (its glyph CLEAR)");
    shift_black(B5);
    run(600);
    CHECK(!memcmp(PAT(), &p0, sizeof p0), "SHIFT + B5: the copied pattern pasted onto the active one");
    undo_redo(-1);
    CHECK(!memcmp(PAT(), &p1, sizeof p1) && A.ui.undo_kind == QD_UK_PASTE, "undo: the paste gone (PASTE)");
    CHECK(qd_undo_count(&A, 1) == 1, "the clear's redo dropped by the paste: one redo there (%d)", qd_undo_count(&A, 1));
    for (i = 0; i < 12; i++) {                          /* eight levels at most */
        wtap(i);
        run(600);
    }
    CHECK(qd_undo_count(&A, -1) == QD_UNDO_LEVELS && qd_undo_count(&A, 1) == 0, "twelve edits: eight levels (%d), no redo",
          qd_undo_count(&A, -1));
    for (ok = 1, i = 0; i < 8; i++) {
        undo_redo(-1);
        ok &= A.ui.undo_kind == QD_UK_EDIT;
    }
    CHECK(ok && !R(0, 0).on && R(0, 1).on && !R(0, 3).on && !R(0, 4).on && R(0, 5).on,
          "eight undos: the four oldest of the twelve taps stay, the eight newest undone");
    undo_redo(-1);
    CHECK(A.ui.undo_kind == -1, "a ninth: nothing there (the takeover's dash)");
    btap(QD_B_PAGE);                                    /* page and view changes keep it */
    btap(QD_B_HOME);
    for (i = 0; i < 8; i++)
        undo_redo(1);
    CHECK(R(0, 11).on && qd_undo_count(&A, 1) == 0, "after a page / view change: all eight redone");
    n0 = qd_pool_add(&A, "UNDO", A.model.t[0].patch);   /* a pool operation */
    run(600);
    ok = A.proj.nsound;
    undo_redo(-1);
    CHECK(n0 >= 0 && A.proj.nsound == ok - 1 && A.ui.undo_kind == QD_UK_SOUNDS, "a pool add undone (%d -> %d sounds)", ok,
          A.proj.nsound);
    undo_redo(1);
    CHECK(A.proj.nsound == ok, "and redone");
    /* (a project load forgetting it: test_persist, which has a flash) */
}

static int run_peak(uint32_t n)                         /* run (as run()), the audio's peak */
{
    int pk = 0;
    while (n--) {
        now++;
        qd_app_tick(&A, now);
        while (frames_done + 128u <= (uint64_t)now * QD_FS / 1000u) {
            int16_t blk[256];
            int k;
            qd_app_audio(&A, blk, 128);
            for (k = 0; k < 256; k++)
                pk = abs(blk[k]) > pk ? abs(blk[k]) : pk;
            frames_done += 128u;
        }
    }
    return pk;
}
static void g_p15c_shift(void)
{
    static qd_pattern_t p0;
    qd_leds_t l;
    int i, ok, pk, pk2;
    start();
    wtap(1), wtap(5);
    run(600);
    memcpy(&p0, PAT(), sizeof p0);
    shift_black(B1);
    CHECK(A.undo.snap_ok && A.ui.cf_on && !strcmp(A.ui.cf_big, "SNAPSHOT"), "SHIFT + B1: the snapshot stored");
    wtap(1), wtap(12);
    run(600);
    shift_black(B3);
    run(600);
    CHECK(!memcmp(PAT(), &p0, sizeof p0), "SHIFT + B3: recalled at once");
    undo_redo(-1);
    CHECK(R(0, 12).on && !R(0, 1).on && A.ui.undo_kind == QD_UK_SNAP, "the recall undone");
    bdown(SHIFT);                                       /* the LEDs: the shortcuts only */
    run(300);
    l = leds();
    for (ok = 1, i = 0; i < 16; i++)
        ok &= l.key[WK(i)] == QD_LED_OFF;
    for (i = 0; i < 11; i++)
        ok &= QD_SHIFT_KEYS[i] ? l.key[BK(i)] >= QD_LED_DIM : l.key[BK(i)] == QD_LED_OFF;
    CHECK(ok && l.key[BK(B3)] == QD_LED_LIT && l.key[BK(B5)] == QD_LED_DIM && l.key[BK(B6)] == QD_LED_LIT,
          "SHIFT held: only B1 B3 B4 B5 B6 B7 B9 B11 lit (B5 dim: nothing copied)");
    CHECK(qd_view_screen(&A) == QD_SCR_SHIFT, "SHIFT's map");
    for (i = 0; i < 4; i++) {                           /* tap tempo: 500 ms apart -> 120 */
        black(B6);
        run(470);
    }
    CHECK(PAT()->bpm == 120, "SHIFT + B6 four times 500 ms apart: 120 BPM (%d)", PAT()->bpm);
    black(B6);
    run(370);
    black(B6);                                          /* (the last three intervals: 500 500 400 -> 128) */
    CHECK(PAT()->bpm >= 127 && PAT()->bpm <= 129, "the last taps averaged (%d)", PAT()->bpm);
    run(2100);
    black(B6);
    run(570);
    black(B6);
    CHECK(PAT()->bpm == 100, "after two seconds a new count (600 ms: 100, %d)", PAT()->bpm);
    bup(SHIFT);
    run(20);
    start();
    btap(QD_B_PLAY);                                    /* the metronome */
    pk = run_peak(1200);
    shift_black(B7);
    CHECK(A.metro && !strcmp(A.ui.cf_small, "on"), "SHIFT + B7: the click on");
    pk2 = run_peak(1200);
    CHECK(pk == 0 && pk2 > 2000, "an empty pattern playing: silent (%d); the click heard (peak %d)", pk, pk2);
    bdown(SHIFT);
    run(20);
    kdown(BK(B7));
    run(20);
    turn(QD_K_1, -4);
    kup(BK(B7));
    run(20);
    bup(SHIFT);
    run(20);
    CHECK(A.metro && A.metro_lvl == QD_METRO_LVL - 20 && !strcmp(A.ui.cf_big, "CLICK"),
          "SHIFT + B7 held + a knob: the click's level (%d), still on", A.metro_lvl);
    shift_black(B7);
    CHECK(!A.metro, "SHIFT + B7 again: off");
    btap(QD_B_PLAY);
    start();                                            /* double the track */
    wtap(0), wtap(7);
    run(600);
    shift_black(B9);
    CHECK(qd_track_loop(&PAT()->trk[0]) == 3 && R(0, 16).on && R(0, 23).on && !R(0, 17).on,
          "SHIFT + B9: the track doubled (page 2 = page 1, the loop both)");
    shift_black(B9);
    CHECK(qd_track_loop(&PAT()->trk[0]) == 15 && R(0, 48).on, "twice: four pages");
    bdown(SHIFT);
    run(20);
    black(B11);
    CHECK(R(0, 0).on && A.ui.clr_armed && !strcmp(A.ui.cf_big, "CLEAR?"), "SHIFT + B11 once: armed, nothing cleared");
    run(QD_FAST_MS);
    i = leds().key[BK(B11)];
    run(QD_FAST_MS);
    CHECK(i != leds().key[BK(B11)], "B11 blinking while armed");
    run(2100);
    black(B11);
    CHECK(R(0, 0).on && A.ui.clr_armed, "two seconds late: armed again, nothing cleared");
    black(B11);
    CHECK(!R(0, 0).on && !R(0, 16).on && !A.ui.clr_armed && qd_track_loop(&PAT()->trk[0]) == 1 && pages_full(0, 0),
          "the second press in time: cleared, the track reset (page 1 only)");
    btap(F1);
    ok = A.ui.undo_kind == QD_UK_CLEAR && R(0, 48).on;
    btap(F1);
    ok &= A.ui.undo_kind == QD_UK_DOUBLE && qd_track_loop(&PAT()->trk[0]) == 3;
    CHECK(ok, "SHIFT held all along: the clear and the second double each their own level");
    bup(SHIFT);
    run(20);
}

static void g_p15c_random(void)
{
    static qd_pattern_t p0;
    static int16_t t0[QD_QUAD_NP];
    static ui_snd_t sv;
    int i, k, ok, same, pg;
    start();
    wtap(0);
    btap(QD_B_SOUND);
    pg = qd_snd_page_of("FILTER");
    A.ui.snd_page = pg;
    run(600);
    memcpy(t0, A.model.t[0].patch, sizeof t0);
    memcpy(&p0, PAT(), sizeof p0);
    kdown(BK(B9));                                      /* P1.6d: a tap, half a second: nothing */
    run(20);
    kup(BK(B9));
    run(200);
    kdown(BK(B9));
    run(500);
    kup(BK(B9));
    run(800);
    CHECK(!A.ui.rnd_kind && !memcmp(t0, A.model.t[0].patch, sizeof t0), "P1.6d: B9 tapped, held half a second: nothing");
    kdown(BK(B9));
    run(500);
    qd_view_snd(&A, &sv);
    CHECK(sv.rnd_kind == 1 && sv.rnd_u > 0.4f && sv.rnd_u < 0.6f && !memcmp(t0, A.model.t[0].patch, sizeof t0),
          "B9 held: the dice shaking (%.2f of the second), nothing changed yet", sv.rnd_u);
    run(520);
    qd_view_snd(&A, &sv);
    CHECK(sv.rnd_kind == 4, "held a second: they roll");
    kup(BK(B9));
    run(700);
    for (same = 0, i = 0; i < 5; i++) {
        const int id = QD_SND_PAGES[pg].id[i];
        if (id >= 0)
            same += A.model.t[0].patch[id] == t0[id];
    }
    for (ok = 1, k = 0; k < QD_QUAD_NP; k++) {
        int on = 0;
        for (i = 0; i < 5; i++)
            on |= QD_SND_PAGES[pg].id[i] == k;
        if (!on)
            ok &= A.model.t[0].patch[k] == t0[k];
    }
    CHECK(!A.ui.rnd_kind && same < 3 && ok, "rolled: FILTER's values new (%d the same), no other value", same);
    run(600);
    undo_redo(-1);
    CHECK(!memcmp(t0, A.model.t[0].patch, sizeof t0) && A.ui.undo_kind == QD_UK_RANDOM, "undone (the dice's glyph)");
    kdown(BK(B10));                                     /* the bottle, released early */
    run(1000);
    qd_view_snd(&A, &sv);
    CHECK(sv.rnd_kind == 2 && sv.rnd_u > 0.6f && sv.rnd_u < 0.7f, "B10 held 1 s: the bottle two thirds full (%.2f)", sv.rnd_u);
    kup(BK(B10));
    run(1000);
    CHECK(!A.ui.rnd_kind && !memcmp(t0, A.model.t[0].patch, sizeof t0), "released early: nothing");
    kdown(BK(B10));
    run(1520);
    qd_view_snd(&A, &sv);
    CHECK(sv.rnd_kind == 3, "held 1.5 s: the cork pops (the burst)");
    kup(BK(B10));
    run(700);
    for (same = 0, k = 0; k < QD_QUAD_NP; k++)
        same += A.model.t[0].patch[k] == t0[k];
    CHECK(!A.ui.rnd_kind && same < QD_QUAD_NP - 30, "the whole sound new (%d of %d the same)", same, QD_QUAD_NP);
    for (i = 0; i < 40; i++) {                          /* the rules, forty times */
        int16_t lo, hi;
        const int16_t *v = A.model.t[0].patch;
        ok = v[QD_QP_ALGO] >= 1 && v[QD_QP_ALGO] <= 8 && v[QD_QP_LEVEL] == t0[QD_QP_LEVEL] && v[QD_QP_FTYPE] == t0[QD_QP_FTYPE];
        ok &= v[QD_QP_RC] == 1 || v[QD_QP_RC] == 3 || v[QD_QP_RC] == 4 || v[QD_QP_RC] == 5 || v[QD_QP_RC] == 6;
        ok &= v[QD_QP_RA] == 1 || v[QD_QP_RA] == 3 || v[QD_QP_RA] == 7 || v[QD_QP_RA] == 11 || v[QD_QP_RA] == 15;
        qd_quad_value_range(QD_QP_FREQ, &lo, &hi, 0);
        ok &= v[QD_QP_FREQ] >= lo + (hi - lo) * 45 / 100;
        qd_quad_value_range(QD_QP_RESO, &lo, &hi, 0);
        ok &= v[QD_QP_RESO] <= lo + (hi - lo) * 60 / 100;
        qd_quad_value_range(QD_QP_ALEV, &lo, &hi, 0);
        ok &= v[QD_QP_ALEV] >= lo + (hi - lo) * 40 / 100;
        for (k = 0; k < QD_QUAD_NP; k++)
            if (qd_quad_value_range(k, &lo, &hi, 0) && hi > lo)
                ok &= v[k] >= lo && v[k] <= hi;
        if (!ok)
            break;
        qd_snd_randomise(&A, -1);
        run(600);
    }
    CHECK(ok, "forty randomised sounds: every value in range, ALGO 1..8, the ratios from their sets, FREQ >= 45 %%, RESO "
          "<= 60 %%, A's level >= 40 %%, LEVEL and FTYPE kept (%d)", i);
    memcpy(t0, A.model.t[0].patch, sizeof t0);
    whold(0);                                           /* a step held: locks */
    kdown(BK(B9));
    run(1020);
    kup(BK(B9));
    run(700);
    wrel(0);
    run(600);
    for (ok = 0, i = 0; i < 5; i++) {
        const int id = QD_SND_PAGES[pg].id[i];
        if (id >= 0)
            ok += LK(0, 0, id) != -99999;
    }
    CHECK(ok >= 3 && !memcmp(t0, A.model.t[0].patch, sizeof t0), "a step held: the dice make p-locks (%d), the track's "
          "sound untouched", ok);
    undo_redo(-1);
    for (ok = 0, i = 0; i < 5; i++) {
        const int id = QD_SND_PAGES[pg].id[i];
        if (id >= 0)
            ok += LK(0, 0, id) != -99999;
    }
    CHECK(!ok, "undo: the locks gone");
    btap(QD_B_HOME);
}

/* P1.5p: the knob band over a held screen's head: nothing of the screen under it cut by its edge */
static uint16_t band_lcd[QD_W * QD_H];
static int row_ink(const uint16_t *lcd, int y, int x0, int x1)     /* pixels not black on a row */
{
    const uint8_t *b = (const uint8_t *)lcd;
    int x, n = 0;
    for (x = x0; x < x1; x++) {
        const uint16_t v = (uint16_t)(b[2 * (y * QD_W + x)] << 8 | b[2 * (y * QD_W + x) + 1]);
        const int r = (v >> 11) & 31, g = (v >> 5) & 63, bl = v & 31;
        n += r + g / 2 + bl > 6;
    }
    return n;
}
static void g_p15p_band(void)
{
    static uint16_t nb[QD_W * QD_H];
    int m, v, y, ok, clear, band, under, sc;
    for (v = 0; v < 2; v++)                             /* PIANO ROLL, STEPS (EDIT's top is its own: no band) */
        for (m = M_NOTE; m <= M_SOUND; m++) {
            start();
            A.ui.view = v;
            if (m == M_NOTE)
                PAT()->trk[0].scale = QD_SCALE_MAJOR, PAT()->trk[0].root = 0, PAT()->trk[0].key_global = 0;
            wtap(1);
            mode(m);
            whold(1);
            run(1200);
            qd_app_frame(&A, nb);
            turn(m == M_NOTE ? QD_K_SELECT : QD_K_1, 1);
            run(30);
            qd_app_frame(&A, band_lcd);
            band = row_ink(band_lcd, 40, 0, QD_W) > 200;
            for (clear = 1, y = 41; y < 47; y++)
                clear &= row_ink(band_lcd, y, 6, 234) == 0;
            CHECK(band && clear, "view %d, mode %d, a step held, a knob: the band (%d), nothing of the screen cut under "
                  "its edge (rows 41..46 clear)", v, m, band);
            if (m == M_NOTE) {
                for (sc = 0, y = 36; y < 45; y++)
                    sc += row_ink(nb, y, 6, 120);
                CHECK(sc > 100, "NOTE, no band: the scale's name under the head (%d)", sc);
                run(QD_TK_MS + QD_TK_FADE_MS + 50);
                qd_app_frame(&A, band_lcd);
                for (under = 0, y = 36; y < 45; y++)
                    under += row_ink(band_lcd, y, 6, 120);
                CHECK(under > 100, "NOTE: the band gone, the scale's name back (%d)", under);
            }
            wrel(1);
            btap(QD_B_HOME);
        }
    start();                                            /* the editor: a knob over a held step, no band over its head */
    wtap(1);
    btap(QD_B_SOUND);
    whold(1);
    run(600);
    turn(QD_K_1, 1);
    run(30);
    qd_app_frame(&A, band_lcd);
    ok = qd_view_screen(&A) == QD_SCR_SOUND && !(A.ui.tk == QD_TK_HELDKNOB && qd_view_overlay(&A));
    CHECK(ok, "the editor, a step held, a knob: no band over its head (tk %d)", A.ui.tk);
    wrel(1);
    btap(QD_B_HOME);
}

/* P1.5q: EDIT follows the selected track's step as it plays (the picture, the value; P1.5z: not the head, the track's
 * default's; an empty step keeps the last, dimmed;
 * micro-timing and the track's rate; a ratchet once a step; the latch, a held step, a stop, a takeover: back) */
static ui_held_t fh;
static ui_lane_t fl;
static int follow_now(void)                             /* the step EDIT shows (1..), following; 0: not following */
{
    qd_view_edit(&A, &fh, &fl);
    return fh.follow ? fh.step : 0;
}
static void g_p15q_follow(void)
{
    static const uint8_t CHA[3] = {60, 64, 67}, CHB[3] = {62, 65, 69};
    static const int VEL[4] = {30, 60, 90, 120};
    uint32_t t0 = 0, tch[8];
    int i, ok, last, n, seen[8], vel_at[8], dim_ok, ndim, nupd;
    start();
    A.ui.view = QD_VIEW_EDIT;
    qd_ctl_set_tempo(&A, 120);                          /* (16ths: 125 ms) */
    for (i = 0; i < 4; i++) {
        qd_step_set_trig(PAT(), 0, i, QD_TRIG_NOTE);
        qd_step_set_vel(PAT(), 0, i, VEL[i]);
    }
    qd_step_set_trig(PAT(), 0, 5, QD_TRIG_NOTE);        /* step 6: half a step late */
    qd_step_set_vel(PAT(), 0, 5, 50);
    qd_step_set_micro(PAT(), 0, 5, 12);
    qd_step_set_ratchet(PAT(), 0, 2, 4, 0);             /* step 3: four hits */
    mode(M_VEL);
    run(300);
    CHECK(!follow_now() && fh.is_default, "stopped: EDIT as before (the track's default)");
    btap(QD_B_PLAY);
    for (n = 0, last = 0, dim_ok = 1, ndim = 0, nupd = 0, i = 0; i < 2000 + 2000; i++) {
        const uint32_t fm = A.ui.fol_ms;
        const int st = follow_now();
        if (st != last && st && n < 8 && (i < 2000)) {
            if (!n)
                t0 = now;
            seen[n] = st, vel_at[n] = fh.vel, tch[n] = now - t0, n++;
        }
        if (st == 4 && qd_seq_track_step(&A.seq, 0) == 4)   /* (the playhead on step 5, empty: step 4 kept, dimmed) */
            dim_ok &= fh.dim && fh.vel == 120, ndim++;
        else if (st)
            dim_ok &= !fh.dim || qd_seq_track_step(&A.seq, 0) >= 6;
        last = st;
        run(1);
        nupd += A.ui.fol_ms != fm;
    }
    ok = n >= 5 && seen[0] == 1 && seen[1] == 2 && seen[2] == 3 && seen[3] == 4 && seen[4] == 6;
    CHECK(ok, "playing: EDIT shows steps 1 2 3 4 6 in turn (%d: %d %d %d %d %d)", n, seen[0], seen[1], seen[2], seen[3],
          seen[4]);
    ok = vel_at[0] == 30 && vel_at[1] == 60 && vel_at[2] == 90 && vel_at[3] == 120 && vel_at[4] == 50;
    CHECK(ok, "the value each step's own (30 60 90 120 50)");
    ok = tch[2] - tch[1] >= 122 && tch[2] - tch[1] <= 128 && tch[3] - tch[1] >= 247 && tch[3] - tch[1] <= 253;
    CHECK(ok, "each at its play time (steps 2 to 3, 2 to 4: 125, 250 ms: %u %u)", tch[2] - tch[1], tch[3] - tch[1]);
    CHECK(tch[4] - tch[1] >= 559 && tch[4] - tch[1] <= 566, "step 6, micro +12: half a step late (2 to 6: 562.5 ms: %u)",
          tch[4] - tch[1]);
    CHECK(dim_ok && ndim > 50, "step 5 empty: step 4 kept, dimmed (%d ms)", ndim);
    CHECK(nupd == 10, "two loops: ten updates (step 3's four hits one; a step again on the next loop: again) (%d)", nupd);
    {   /* P1.5z (the owner: "lets not change the header during playback"): the picture the step playing's, the head
         * the track's default's ("TRACK DEFAULT · VEL", its value) */
        int st = follow_now();
        CHECK(st >= 1 && fh.step == st && !fh.is_default && !fh.nheld && fh.head && fh.head->is_default &&
                  fh.head->vel == qd_proj_pat(&A.proj, A.seq.cur)->trk[0].def.vel,
              "following: the picture step %d's, the head the track's default's (VEL %d)", st, fh.head ? fh.head->vel : -1);
    }
    btap(LFO);                                          /* the latch: back */
    ok = !follow_now() && qd_view_edit_follows(&A) < 0;
    btap(LFO);
    run(300);
    ok &= follow_now() > 0;
    whold(9);                                           /* a step held: that step */
    ok &= !follow_now() && fh.step == 10;
    wrel(9);
    run(300);
    ok &= follow_now() > 0;
    CHECK(ok, "the latch on, a step held: not following (the held step); off, let go: following again");
    turn(QD_K_1, 1);                                    /* a knob, nothing held: its takeover (VEL's default), then
                                                         * following */
    ok = A.ui.tk != QD_TK_NONE && !follow_now();
    run(QD_TK_MS + QD_TK_FADE_MS + 300);
    ok &= follow_now() > 0;
    CHECK(ok, "a knob's takeover: not following while it shows, then again");
    btap(QD_B_PLAY);
    run(50);
    CHECK(!follow_now() && fh.is_default, "stopped: back to the default");
    start();                                            /* the track's rate: 1/8, a step 250 ms */
    A.ui.view = QD_VIEW_EDIT;
    qd_ctl_set_tempo(&A, 120);
    qd_tset_set_rate(&PAT()->trk[0], qd_rate_make(3, QD_RTYPE_NORMAL));
    qd_step_set_trig(PAT(), 0, 0, QD_TRIG_NOTE);
    qd_step_set_trig(PAT(), 0, 1, QD_TRIG_NOTE);
    qd_step_set_trig(PAT(), 0, 2, QD_TRIG_NOTE);
    qd_step_set_notes(PAT(), 0, 0, CHA, 3);
    qd_step_set_notes(PAT(), 0, 1, CHB, 3);
    qd_step_set_notes(PAT(), 0, 2, CHA, 3);
    mode(M_NOTE);
    btap(QD_B_PLAY);
    for (n = 0, last = 0, i = 0; i < 900; i++) {
        const int st = follow_now();
        if (st != last && st && n < 8) {
            if (!n)
                t0 = now;
            seen[n] = st, vel_at[n] = fh.note[1], tch[n] = now - t0, n++;
        }
        last = st;
        run(1);
    }
    ok = n >= 3 && seen[0] == 1 && seen[1] == 2 && seen[2] == 3 && vel_at[0] == 64 && vel_at[1] == 65 && vel_at[2] == 64 &&
         tch[2] - tch[1] >= 247 && tch[2] - tch[1] <= 253;
    CHECK(ok, "NOTE, the track at 1/8: C E G, D F A, C E G, a step 250 ms (%u)", n >= 3 ? tch[2] - tch[1] : 0);
    btap(QD_B_PLAY);
}

/* P1.5s: the stopped preview with the step latch on: silent while the selection stands or is edited; once (short) for
 * each step the cursor moves to; a W held sounds while held */
static int onsets(uint32_t ms, int note, int *still)    /* the note's starts over ms; *still: sounding at the end */
{
    int n = 0, was = sounding(note);
    while (ms--) {
        int now_s;
        run(1);
        now_s = sounding(note);
        n += now_s && !was;
        was = now_s;
    }
    if (still)
        *still = was;
    return n;
}
static void g_p15s_preview(void)
{
    int n, still;
    start();
    wtap(2);                                            /* (the latch starts on step 3, the last edited) */
    for (n = 3; n < 8; n++)
        qd_step_set_trig(PAT(), 0, n, QD_TRIG_NOTE);
    mode(M_VEL);
    btap(LFO);
    n = onsets(3000, 60, &still);
    CHECK(A.ui.slatch && A.ui.nheld == 1 && n == 0 && !still, "the latch on, nothing pressed, 3 s: no note (%d)", n);
    turn(QD_K_ALGO, 1);
    n = onsets(1500, 60, &still);
    CHECK(A.ui.held[0] == 3 && n <= 1 && !still, "ALGORITHM: the cursor on step 4, one short preview (%d), then silence",
          n);
    CHECK(A.ui.pvs_n == 0, ".. nothing left sounding");
    turn(QD_K_ALGO, 1);
    run(5);
    CHECK(sounding(60), "the cursor again (step 5): it sounds at once");
    n = onsets(1500, 60, &still);
    CHECK(n == 0 && !still, ".. once (no restrike, %d)", n);
    turn(QD_K_1, -3);
    n = onsets(1000, 60, &still);
    CHECK(n == 0 && !still && R(0, 4).vel == 97 && A.ui.held[0] == 4, "a knob edit on the selection: no preview (%d)", n);
    kdown(WK(4));
    run(400);
    CHECK(sounding(60), "W5 held (the latch on): it sounds while held");
    turn(QD_K_1, 1);
    run(5);
    CHECK(sounding(60), ".. an edit with the finger on it: sounding (struck again)");
    kup(WK(4));
    run(50);
    CHECK(!sounding(60), ".. let go: silent");
    btap(LFO);                                          /* the latch off: a held W as before */
    whold(6);
    CHECK(sounding(60), "the latch off, W7 held: sounds");
    wrel(6);
    run(20);
    CHECK(!sounding(60), ".. let go: silent");
    btap(QD_B_HOME);
}

/* P1.5s: EDIT follows from PLAY also with the step latch on and nothing selected (the default shown) */
static void g_p15s_follow(void)
{
    int i, last = 0, n = 0;
    qd_app_init(&A);                                    /* (P1.5s: the power-on view) */
    CHECK(A.ui.view == QD_VIEW_EDIT, "power-on: EDIT is the view (the owner: the default one)");
    start();
    A.ui.view = QD_VIEW_EDIT;
    for (i = 0; i < 16; i += 2)
        qd_step_set_trig(PAT(), 0, i, QD_TRIG_NOTE);
    mode(M_VEL);
    btap(LFO);
    wtap(A.ui.held[0] % QD_PAGE);                       /* (the selection's step tapped: nothing selected) */
    run(300);
    CHECK(A.ui.slatch && !A.ui.nheld && !follow_now() && fh.is_default, "the latch on, nothing selected: the default");
    btap(QD_B_PLAY);
    for (i = 0; i < 1500; i++) {
        const int st = follow_now();
        if (st && st != last)
            n++, last = st;
        run(1);
    }
    CHECK(n == 0, "P1.6g (the owner's one rule): PLAY, the latch on, nothing selected: no following, the track's "
          "values (%d seen)", n);
    btap(LFO);                                          /* the latch off, nothing held: it follows */
    for (n = 0, i = 0; i < 1500; i++) {
        const int st = follow_now();
        if (st && st != last)
            n++, last = st;
        run(1);
    }
    CHECK(n >= 5, "the latch off, nothing held: it follows the steps (%d seen)", n);
    PAT()->trk[0].mute = 1;                             /* a muted track: followed too */
    for (n = 0, i = 0; i < 1500; i++) {
        const int st = follow_now();
        if (st && st != last)
            n++, last = st;
        run(1);
    }
    CHECK(n >= 5, "the track muted: still followed (%d seen)", n);
    PAT()->trk[0].mute = 0;
    btap(QD_B_PLAY);
    run(50);
    CHECK(!follow_now() && fh.is_default, "stopped: the default");
}

/* ================================================================ P1.5t: the sound system (PLAN.md "The sound
 * system, redesigned"): the library browser, the pool, the step's picker, SAVE PRESET */
static int br_code(void)                                /* the chosen entry's list code (-1: none) */
{
    qd_sound_t s;
    int c = -1;
    return qd_br_chosen(&A, &s, &c, 0) ? c : -1;
}
static int br_n(void)
{
    qd_sound_t s;
    int n = 0;
    qd_br_chosen(&A, &s, 0, &n);
    return n;
}
static void g_p15t_browse(void)
{
    static qd_tsound_t own, pat0;
    qd_sound_t s;
    int list[QD_BR_LIST], n, i, ok;
    start();
    run(20);
    A.model.t[0].patch[QD_QP_HARM] += 300;              /* the track's sound edited */
    A.model.t[0].edited = 1;
    run(QD_TK_MS + QD_TK_FADE_MS + 600);
    qd_track_to_tsound(&A.model.t[0], &own);
    pat0 = PAT()->snd[0];
    CHECK(own.edited && !memcmp(&own, &pat0, sizeof own), "(the track's sound edited: its pattern's copy the same)");
    turn(QD_K_PRESETS, 1);                              /* opens on the track's preset (MARIMBA, 14) */
    CHECK(A.ui.latch == QD_LATCH_BROWSE && qd_view_screen(&A) == QD_SCR_BROWSE && !A.ui.br.moved &&
              br_code() == own.preset - 1 && !memcmp(A.model.t[0].patch, own.patch, sizeof own.patch),
          "PRESETS: the browser, on the track's preset (%d), the track's own sound still", br_code() + 1);
    turn(QD_K_PRESETS, 1);
    qd_br_chosen(&A, &s, 0, 0);
    CHECK(A.ui.br.moved && !memcmp(A.model.t[0].patch, s.patch, sizeof s.patch) && !strcmp(A.model.t[0].sound_name, s.name) &&
              !memcmp(&PAT()->snd[0], &pat0, sizeof pat0) && A.seq.pv_patch[0] == A.model.t[0].patch,
          "a detent: the track plays the next (%s); the pattern's copy untouched (the preview apart)", s.name);
    run(5);
    CHECK(A.ui.pv_note >= 0, "stopped: a short note");
    run(400);
    CHECK(A.ui.pv_note < 0, ".. short (it ends)");
    turn(QD_K_SELECT, -1);
    CHECK(A.ui.br.pos == 13 + 1 - 10, "SELECT: ten back (%d)", A.ui.br.pos);
    btap(F1);
    CHECK(!A.ui.latch && A.ui.tk == QD_TK_BACK && !memcmp(A.model.t[0].patch, own.patch, sizeof own.patch) &&
              A.model.t[0].edited && A.model.t[0].preset == own.preset && !strcmp(A.model.t[0].sound_name, own.name) &&
              !A.seq.pv_patch[0] && !memcmp(&PAT()->snd[0], &pat0, sizeof pat0),
          "F1: CANCEL: the own sound back exactly (edited, its patch, its name, its preset); BACK");
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    turn(QD_K_PRESETS, 1);
    turn(QD_K_PRESETS, 3);
    btap(QD_B_HOME);
    CHECK(!A.ui.latch && !memcmp(A.model.t[0].patch, own.patch, sizeof own.patch) && A.model.t[0].edited,
          "HOME: a cancel too");
    turn(QD_K_PRESETS, 1);
    turn(QD_K_PRESETS, 2);
    btap(QD_B_SOUND);                                   /* EDIT: any other way out is a cancel */
    CHECK(qd_snd_open(&A) && !memcmp(A.model.t[0].patch, own.patch, sizeof own.patch), "EDIT: a cancel (the editor)");
    btap(QD_B_HOME);
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    run(600);
    n = qd_undo_count(&A, -1);
    turn(QD_K_PRESETS, 1);
    turn(QD_K_PRESETS, -3);                             /* MARIMBA - 3: WOBBLE (10), id 11 */
    btap(F2);
    CHECK(!A.ui.latch && A.ui.tk == QD_TK_LOADED && A.model.t[0].preset == 11 && !A.model.t[0].edited &&
              !strcmp(A.model.t[0].sound_name, "WOBBLE") && PAT()->snd[0].preset == 11 && !A.seq.pv_patch[0],
          "F2: LOAD: WOBBLE onto the track (its preset 11, not edited), its pattern's copy too; LOADED");
    run(700);
    CHECK(qd_undo_count(&A, -1) == n + 1, "one undo level");
    undo_redo(-1);
    CHECK(!memcmp(A.model.t[0].patch, own.patch, sizeof own.patch) && A.model.t[0].edited && PAT()->snd[0].preset == own.preset,
          "undo: the own sound back (edited)");
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    turn(QD_K_PRESETS, 1);                              /* the kinds: B2 BASS */
    black(B2);
    n = qd_br_list(&A, A.ui.br.src, A.ui.br.kind, list);
    for (ok = n > 0, i = 0; i < n; i++) {
        qd_br_entry(&A, list[i], &s);
        ok &= (s.tags & QD_PKB(QD_PK_BASS)) != 0;
    }
    CHECK(A.ui.br.kind == 1 + QD_PK_BASS && ok && n == 2, "B2: BASS only (%d: BASS, WOBBLE)", n);
    black(B7);
    CHECK(br_n() == 6, "B7: DRUMS (the six)");
    black(B1);
    CHECK(br_n() == QD_NFACTORY, "B1: ALL (the factory's 23; no MY presets yet)");
    black(B9);
    CHECK(A.ui.br.src == QD_BR_MINE && br_n() == 0 && qd_view_screen(&A) == QD_SCR_BROWSE, "B9: MY PRESETS (none)");
    black(B9);
    CHECK(A.ui.br.src == QD_BR_POOL && br_n() == A.proj.nsound, "B9: the POOL (%d)", br_n());
    black(B9);
    CHECK(A.ui.br.src == QD_BR_ALL, "B9: ALL again");
    btap(F1);
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    turn(QD_K_PRESETS, 1);                              /* the W keys play it (in the track's scale) */
    kdown(WK(2));
    run(10);
    CHECK(sounding(qd_snd_scale_note(&A, 2)), "a W key: its note in the track's scale");
    kup(WK(2));
    run(10);
    btap(QD_B_PLAY);                                    /* playing: the pattern's trigs play the preview */
    turn(QD_K_PRESETS, 2);
    CHECK(A.seq.pv_patch[0] == A.model.t[0].patch, "playing: the sequencer plays the chosen");
    btap(F1);
    CHECK(!A.seq.pv_patch[0], "cancelled: its own again");
    btap(QD_B_PLAY);
}

static void g_p15t_pool(void)
{
    int k, n0, s0;
    start();
    qd_sounds_clear(&A.proj);                           /* (a new project's pool: empty) */
    turn(QD_K_PRESETS, 1);
    turn(QD_K_PRESETS, 17 - A.ui.br.pos);               /* KICK (factory 17, id 18) */
    CHECK(br_code() == 17, "(KICK chosen)");
    black(B10);
    CHECK(A.proj.nsound == 1 && !strcmp(POOL(&A.proj, 1)->name, "KICK") && POOL(&A.proj, 1)->preset == 18 &&
              A.ui.br.fx == QD_BRFX_IN && A.model.t[0].preset == 18, "B10: + POOL: KICK into the empty pool (not loaded: "
          "the track still plays the chosen, its own sound apart)");
    black(B10);
    CHECK(A.proj.nsound == 0 && A.ui.br.fx == QD_BRFX_OUT && !qd_br_pool_k(&A), "B10 again (P1.5v): in the pool, no "
          "step uses it: out at once");
    black(B10);
    CHECK(A.proj.nsound == 1 && qd_br_pool_k(&A) == 1, "B10: in again (no duplicate: one KICK)");
    for (k = 2; k <= QD_NSOUND; k++) {                  /* the pool full */
        qd_sound_t s = *POOL(&A.proj, k);
        strcpy(s.name, "X");
        s.preset = 0;
        qd_sound_write(&A.proj, k, &s);
    }
    qd_sound_recount(&A.proj);
    turn(QD_K_PRESETS, 1);
    black(B10);
    CHECK(A.ui.br.fx == QD_BRFX_FULL && strcmp(POOL(&A.proj, QD_NSOUND)->name, "X") == 0, "the pool full: nothing, "
          "the tray flashes orange");
    btap(F1);
    start();                                            /* - POOL (the demo's pool: 23) */
    qd_step_on(PAT(), 0, 5);
    qd_step_set_sound(PAT(), 0, 5, 19);                 /* step 6 locks SNARE */
    run(600);
    turn(QD_K_PRESETS, 1);
    black(B9), black(B9);                               /* the POOL */
    turn(QD_K_PRESETS, -200);
    turn(QD_K_PRESETS, 18);                             /* 19: SNARE */
    CHECK(br_code() == QD_BR_POOL_CODE + 19, "(SNARE chosen in the POOL)");
    n0 = A.proj.nsound;
    black(B10);
    CHECK(A.ui.br.out_armed && qd_sound_used(&A.proj, 19), "B10 on a sound steps use: armed (OUT?), not out yet");
    black(B10);
    run(600);
    CHECK(!qd_sound_used(&A.proj, 19) && !R(0, 5).sound && !OV(0, 5, QD_PM_SOUND) && A.ui.br.fx == QD_BRFX_OUT,
          "B10 again: out; the step back to the track's sound");
    turn(QD_K_PRESETS, -200);                           /* INIT (pool 1): no step uses it */
    s0 = br_code();
    black(B10);
    CHECK(s0 == QD_BR_POOL_CODE + 1 && !qd_sound_used(&A.proj, 1), "an unused one: out at once");
    run(600);
    undo_redo(-1);
    CHECK(qd_sound_used(&A.proj, s0 - QD_BR_POOL_CODE), "undo: back");
    undo_redo(-1);
    CHECK(qd_sound_used(&A.proj, 19) && R(0, 5).sound == 19, "undo: SNARE back, the step's lock too");
    (void)n0;
    btap(QD_B_HOME);
    start();                                            /* undo of + POOL */
    qd_sounds_clear(&A.proj);
    run(600);
    turn(QD_K_PRESETS, 1);
    black(B10);
    run(600);
    CHECK(A.proj.nsound == 1, "(+ POOL)");
    undo_redo(-1);
    CHECK(A.proj.nsound == 0, "undo: the pool empty again");
    btap(QD_B_HOME);
}

static void g_p15t_delete(void)
{
    int16_t p[QD_QUAD_NP];
    int id;
    start();
    qd_quad_preset_patch(3, p);
    qd_lib_store(&A, 4, "DARK BASS", p);
    id = LIB(&A, 4)->preset;
    run(600);
    turn(QD_K_PRESETS, 1);
    black(B11);
    CHECK(!A.ui.br.del_armed, "B11 on a factory preset: nothing");
    black(B9);                                          /* MY PRESETS */
    turn(QD_K_PRESETS, 1);
    CHECK(br_code() == QD_BR_MINE_CODE + 4, "(DARK BASS chosen in MY PRESETS)");
    CHECK(leds().key[BK(B11)] == QD_LED_LIT, "B11 lit on one of MY presets");
    black(B11);
    CHECK(A.ui.br.del_armed && LIB(&A, 4)->name[0], "B11: armed (blinking), still there");
    black(B11);
    run(600);
    CHECK(!LIB(&A, 4)->name[0] && br_n() == 0, "B11 again: deleted");
    undo_redo(-1);
    CHECK(!strcmp(LIB(&A, 4)->name, "DARK BASS") && LIB(&A, 4)->preset == id, "undo: back (its id)");
    btap(QD_B_HOME);
}

static void g_p15t_picker(void)
{
    static uint16_t lcd[QD_W * QD_H];
    start();
    qd_sounds_clear(&A.proj);                           /* an empty pool: TRACK only */
    wtap(4);
    whold(4);
    turn(QD_K_PRESETS, 3);
    qd_app_frame(&A, lcd);
    CHECK(R(0, 4).sound == 0 && A.ui.tk == QD_TK_PRESET && A.ui.tk_lock, "a step + PRESETS, the pool empty: TRACK "
          "(no lock)");
    wrel(4);
    start();
    wtap(4);
    whold(4);
    turn(QD_K_PRESETS, 2);
    CHECK(R(0, 4).sound == 2 && A.ui.tk_lock, "the demo's pool: TRACK, 1, 2 (EP)");
    turn(QD_K_PRESETS, -5);
    CHECK(R(0, 4).sound == 0, "down: TRACK, the lock off");
    qd_app_frame(&A, lcd);
    wrel(4);
    CHECK(R(0, 4).sound == 0 && !A.ui.nheld, "let go: kept");
}

static void g_p15t_save(void)
{
    char nm[QD_SOUND_NAME];
    int n;
    start();
    run(600);
    btap(QD_B_SAVE);
    {   /* P1.5y: the suggestion is the namesizer pair of the name's seed */
        uint32_t sd = A.ui.ty.seed;
        qd_name_rnd(&sd, nm);
    }
    CHECK(A.ui.latch == QD_LATCH_SAVE && qd_view_screen(&A) == QD_SCR_SAVE && A.ui.ty.spark && A.ui.ty.sel &&
              A.ui.sv.replace < 0 && !strcmp(A.ui.ty.name, nm) && qd_name_ok(nm),
          "SAVE: SAVE PRESET, a namesizer name (%s), selected", nm);
    CHECK(leds().button[QD_B_OCTDN] == QD_LED_LIT, "F1 lit");
    turn(QD_K_PRESETS, 2);                              /* P1.5u: another idea */
    CHECK(strcmp(A.ui.ty.name, nm) != 0 && A.ui.ty.sel, "PRESETS: another name (an idea: %s)", A.ui.ty.name);
    CHECK(A.ui.ty.nhist == 3 && !strcmp(qd_type_hist(&A, 2), nm) && !strcmp(qd_type_hist(&A, 0), A.ui.ty.name) &&
              A.ui.ty.dice_ms, "P1.5z: two names rolled, the suggestion the oldest of three, a die rolled");
    turn(QD_K_SELECT, 1);                               /* the last letter on one (P1.5u: the cursor at the end) */
    turn(QD_K_ALGO, -1);
    btap(F1);                                           /* .. the one before it deleted */
    CHECK(!A.ui.ty.sel && A.ui.ty.cur == (int)strlen(A.ui.ty.name) - 2, "SELECT / ALGORITHM / F1: edited, the cursor "
          "two before the end");
    memcpy(nm, A.ui.sv.newname, QD_SOUND_NAME);
    n = qd_undo_count(&A, -1);
    btap(QD_B_SAVE);
    run(600);
    CHECK(!strcmp(LIB(&A, 0)->name, nm) && A.model.t[0].preset == LIB(&A, 0)->preset && !A.model.t[0].edited &&
              A.ui.tk == QD_TK_SAVED && !A.ui.latch, "SAVE again: NEW (%s), the track plays it; SAVED", nm);
    CHECK(qd_undo_count(&A, -1) == n + 1, "one undo level");
    undo_redo(-1);
    CHECK(!LIB(&A, 0)->name[0], "undo: not saved");
    undo_redo(1);
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    A.model.t[0].patch[QD_QP_HARM] = 1200;              /* edited: SAVE offers REPLACE, NEW as "NAME 2" */
    A.model.t[0].edited = 1;
    run(20);
    btap(QD_B_SAVE);
    CHECK(A.ui.sv.replace == 0 && !strncmp(A.ui.sv.newname, nm, 4) && strstr(A.ui.sv.newname, " 2") &&
              leds().button[QD_B_OCTUP] != QD_LED_OFF + 1, "from one of MY presets, edited: REPLACE offered; NEW as %s (%s)",
          A.ui.sv.newname, nm);
    btap(QD_B_HOME);
    CHECK(!A.ui.latch && LIB(&A, 0)->patch[QD_QP_HARM] != 1200 && !LIB(&A, 1)->name[0],
          "HOME: cancelled, nothing saved");
    btap(QD_B_SAVE);
    CHECK(A.ui.sv.choice == 0 && leds().button[F2] == QD_LED_LIT, "REPLACE offered: NEW chosen first, F2 lit");
    btap(F2);
    CHECK(A.ui.sv.choice == 1 && A.ui.latch == QD_LATCH_SAVE, "F2 (P1.5v): REPLACE chosen, nothing saved yet");
    btap(F2);
    btap(F2);
    btap(QD_B_SAVE);
    run(600);
    CHECK(LIB(&A, 0)->patch[QD_QP_HARM] == 1200 && !LIB(&A, 1)->name[0] && !A.model.t[0].edited &&
              A.model.t[0].preset == LIB(&A, 0)->preset, "F2 again (NEW), again (REPLACE), SAVE: REPLACED (the same preset), nothing new");
    A.model.t[0].edited = 1;
    btap(QD_B_SAVE);
    btap(QD_B_SAVE);
    CHECK(LIB(&A, 1)->name[0] && strstr(LIB(&A, 1)->name, " 2") && A.model.t[0].preset == LIB(&A, 1)->preset,
          "SAVE, SAVE: NEW as %s", LIB(&A, 1)->name);
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
}

static void g_p15t_leds(void)
{
    qd_leds_t l;
    int i, ok;
    start();
    turn(QD_K_PRESETS, 1);
    black(B3);
    l = leds();
    {
        qd_sound_t s;
        qd_br_chosen(&A, &s, 0, 0);                     /* (P1.5x: the filter solid, the chosen's tags breathing) */
        for (ok = 1, i = 0; i < 8; i++)
            ok &= l.key[BK(i)] == (i == 2 ? QD_LED_LIT : i && ((s.tags >> (i - 1)) & 1) ? qd_led_breath_lo(A.ms)
                                                                                        : QD_LED_OFF);
    }
    CHECK(ok && l.key[BK(B9)] == QD_LED_LIT && l.key[BK(B10)] == (qd_br_pool_k(&A) ? qd_led_breath(A.ms) : QD_LED_LIT) &&
              l.key[BK(B11)] == QD_LED_OFF &&
              l.button[F1] == QD_LED_LIT && l.button[F2] == QD_LED_LIT,
          "browsing: the filter (KEYS) lit, the chosen's tags breathing, the rest dark; B9, B10 lit, B11 dark (a factory preset), F1 F2 lit");
    btap(QD_B_HOME);
}

/* ================================================================ P1.5u: the typewriter === */
static const char TY_B[] = "ABCDEFGHIJK", TY_W[] = "LMNOPQRSTUVWXYZ ";
static const char TY_SB[] = "0123456789-", TY_SW[] = ".,'&!?#+/()*:_~ ";
static void ty_clear(void)                              /* SHIFT + F1 */
{
    bdown(SHIFT);
    run(10);
    btap(F1);
    bup(SHIFT);
    run(10);
}
static void ty_open(void)                               /* SAVE on track 1's sound */
{
    start();
    run(600);
    btap(QD_B_SAVE);
}
static void ty_type(const char *s)                      /* typed on the keys (upper case and SPACE) */
{
    for (; *s; s++) {
        const char *b = strchr(TY_B, *s), *w = strchr(TY_W, *s);
        if (b)
            black((int)(b - TY_B));
        else if (w)
            wtap((int)(w - TY_W));
    }
}

static void g_p15u_keys(void)
{
    int k, ok, okc;
    ty_open();
    for (ok = 0, k = 0; k < 11; k++) {
        ty_clear();
        black(k);
        ok += !strcmp(A.ui.ty.name, (char[]){TY_B[k], 0}) && A.ui.ty.cur == 1;
    }
    for (k = 0; k < 16; k++) {
        ty_clear();
        wtap(k);
        ok += A.ui.ty.name[0] == TY_W[k] && !A.ui.ty.name[1] && A.ui.ty.cur == 1;
    }
    CHECK(ok == 27, "the black keys A..K, the white keys L..Z and SPACE (%d of 27)", ok);
    bdown(SHIFT);
    run(10);
    for (okc = 0, k = 0; k < 11; k++) {
        btap(F1);                                       /* (SHIFT held: F1 clears) */
        black(k);
        okc += A.ui.ty.name[0] == TY_SB[k] && !A.ui.ty.name[1];
    }
    for (k = 0; k < 16; k++) {
        btap(F1);
        wtap(k);
        okc += A.ui.ty.name[0] == TY_SW[k] && !A.ui.ty.name[1];
    }
    bup(SHIFT);
    run(10);
    CHECK(okc == 27 && A.ui.latch == QD_LATCH_SAVE, "SHIFT: 0..9 -, . , ' & ! ? # + / ( ) * : _ ~ SPACE (%d of 27), "
          "SAVE PRESET still open", okc);
    ty_clear();
    kdown(BK(B1));
    run(400);                                           /* (held: no repeat) */
    kup(BK(B1));
    run(10);
    CHECK(!strcmp(A.ui.ty.name, "A"), "a key held: no repeat");
    btap(QD_B_HOME);
}

static void g_p15u_edit(void)
{
    char nm[QD_SOUND_NAME];
    ty_open();
    snprintf(nm, sizeof nm, "%s", A.ui.ty.name);
    CHECK(A.ui.ty.sel && nm[0] && A.ui.ty.cur == (int)strlen(nm), "the suggested name (%s), selected", nm);
    black(B4);
    CHECK(!strcmp(A.ui.ty.name, "D") && !A.ui.ty.sel && A.ui.ty.cur == 1 && A.ui.ty.fresh == 0,
          "the first key replaces it: D");
    ty_type("UST");
    CHECK(!strcmp(A.ui.ty.name, "DUST") && A.ui.ty.cur == 4, "U S T: DUST, the cursor after it");
    turn(QD_K_ALGO, -2);
    black(B1);
    CHECK(!strcmp(A.ui.ty.name, "DUAST") && A.ui.ty.cur == 3, "ALGORITHM back two, A: inserted (DUAST)");
    btap(F1);
    CHECK(!strcmp(A.ui.ty.name, "DUST") && A.ui.ty.cur == 2 && A.ui.ty.del_ch == 'A', "F1: the one before the cursor "
          "deleted (DUST)");
    turn(QD_K_ALGO, 9);
    ty_type(" PAD");
    CHECK(!strcmp(A.ui.ty.name, "DUST PAD") && A.ui.ty.cur == 8, "ALGORITHM to the end (clamped), SPACE P A D: DUST PAD");
    turn(QD_K_SELECT, 2);
    CHECK(!strcmp(A.ui.ty.name, "DUST PADB") && A.ui.ty.cur == 8, "SELECT at the end: a new character turned (B)");
    turn(QD_K_ALGO, -8);
    turn(QD_K_SELECT, 3);
    CHECK(!strcmp(A.ui.ty.name, "GUST PADB"), "SELECT: the character under the cursor turned (D -> G)");
    turn(QD_K_SELECT, -8);
    CHECK(A.ui.ty.name[0] == '~', "SELECT back past A: SPACE, the signs (%c)", A.ui.ty.name[0]);
    turn(QD_K_ALGO, 20);
    ty_type("XYZ");
    CHECK(!strcmp(A.ui.ty.name, "~UST PADBXYZ") && A.ui.ty.full_ms == 0, "twelve characters");
    black(B5);
    run(30);
    CHECK(!strcmp(A.ui.ty.name, "~UST PADBXYZ") && A.ui.ty.full_ms, "full: a key types nothing, the row flashes");
    ty_clear();
    CHECK(!A.ui.ty.name[0] && A.ui.ty.cur == 0 && A.ui.latch == QD_LATCH_SAVE, "SHIFT + F1: cleared");
    btap(F1);
    CHECK(!A.ui.ty.name[0] && A.ui.latch == QD_LATCH_SAVE, "F1 on nothing: nothing (still open)");
    btap(QD_B_SAVE);
    run(600);
    CHECK(!A.ui.latch && !strcmp(LIB(&A, 0)->name, "SOUND"), "SAVE on an empty name: saved as SOUND (%s)",
          LIB(&A, 0)->name);
    btap(QD_B_SAVE);                                    /* a selected name: F1 deletes all of it */
    btap(F1);
    CHECK(!A.ui.ty.name[0] && !A.ui.ty.sel, "the suggestion selected, F1: all of it");
    btap(QD_B_HOME);
}

static void g_p15u_ideas(void)
{
    /* P1.5z (the owner): PRESETS (or KNOB 4) right rolls a new name every detent, endless (mostly namesizer pairs, a
     * made-up word now and then, the sound's own now and then); left walks back through the last ten, stopping at the
     * oldest; right then forward first */
    char nm[QD_SOUND_NAME], seen[40][QD_SOUND_NAME];
    int k, j, ok = 1, words = 0, sound = 0, n;
    ty_open();
    snprintf(nm, sizeof nm, "%s", A.ui.ty.name);
    turn(QD_K_PRESETS, 1);
    CHECK(A.ui.ty.nhist == 2 && strcmp(A.ui.ty.name, nm) && !strcmp(qd_type_hist(&A, 1), nm) && A.ui.ty.sel &&
              A.ui.ty.spark && A.ui.ty.dice_ms && A.ui.ty.idea_ms, "PRESETS right: a new name (%s), selected, a die rolled",
          A.ui.ty.name);
    for (k = 0; k < 40; k++) {                          /* forty more: each new, the mix */
        char own[8][QD_SOUND_NAME];
        int m;
        turn(k & 1 ? QD_K_4 : QD_K_PRESETS, 1);
        snprintf(seen[k], QD_SOUND_NAME, "%s", A.ui.ty.name);
        ok &= qd_name_ok(seen[k]) && (!k || strcmp(seen[k], seen[k - 1]));
        words += !strchr(seen[k], ' ');
        n = qd_sound_ideas(A.model.t[0].patch, own, 8);
        for (m = 0; m < n; m++)
            sound += !strcmp(own[m], seen[k]);
    }
    CHECK(ok && A.ui.ty.nhist == QD_TY_NHIST, "forty more (PRESETS and KNOB 4): each a new name, each fits; ten kept");
    CHECK(words >= 3 && sound >= 2 && sound <= 18, "the mix: made-up words %d, the sound's own %d of 40 (%s, %s, %s ..)",
          words, sound, seen[0], seen[1], seen[2]);
    turn(QD_K_PRESETS, -3);
    CHECK(A.ui.ty.hpos == 3 && !strcmp(A.ui.ty.name, seen[36]) && A.ui.ty.sel, "left 3: back to the fourth newest (%s)",
          A.ui.ty.name);
    turn(QD_K_4, -20);
    CHECK(A.ui.ty.hpos == QD_TY_NHIST - 1 && !strcmp(A.ui.ty.name, seen[30]), "on left: it stops at the oldest kept (%s)",
          A.ui.ty.name);
    turn(QD_K_PRESETS, 2);
    CHECK(A.ui.ty.hpos == QD_TY_NHIST - 3 && !strcmp(A.ui.ty.name, seen[32]), "right in the history: forward through it "
          "first (%s)", A.ui.ty.name);
    turn(QD_K_PRESETS, QD_TY_NHIST - 3);
    CHECK(!A.ui.ty.hpos && !strcmp(A.ui.ty.name, seen[39]), "to the newest");
    turn(QD_K_PRESETS, 1);
    CHECK(!A.ui.ty.hpos && strcmp(A.ui.ty.name, seen[39]) && !strcmp(qd_type_hist(&A, 1), seen[39]),
          "past it: a new one rolled (%s)", A.ui.ty.name);
    for (j = 0, ok = 1; j < 2; j++) {                   /* the same seed: the same names */
        ty_open();
        A.ui.ty.rseed = 777u;
        turn(QD_K_PRESETS, 1);
        if (!j)
            snprintf(nm, sizeof nm, "%s", A.ui.ty.name);
        else
            ok &= !strcmp(nm, A.ui.ty.name);
    }
    CHECK(ok, "deterministic: the same seed, the same name (%s)", nm);
    run(QD_TY_IDEA_MS + 20);
    wtap(4);
    CHECK(!strcmp(A.ui.ty.name, "P"), "a key: the name replaced (selected)");
    {   /* the classifier's words: a pluck gets PLUCK and its other words */
        char ideas[QD_TY_NIDEA][QD_SOUND_NAME];
        int16_t p[QD_QUAD_NP];
        int n, words = 0;
        qd_quad_preset_patch(4, p);                     /* (PLUCK) */
        n = qd_sound_ideas(p, ideas, QD_TY_NIDEA);
        for (k = 0; k < n; k++)
            words += strstr(ideas[k], "PLUCK") || strstr(ideas[k], "PING") || strstr(ideas[k], "TINE") ||
                     strstr(ideas[k], "HARP");
        CHECK(n == QD_TY_NIDEA && words == n, "a PLUCK's ideas: twelve, each with PLUCK, PING, TINE or HARP (%s, %s ..)",
              ideas[1], ideas[2]);
    }
    btap(QD_B_HOME);
}

static void g_p15u_save(void)
{
    int id, n;
    ty_open();
    ty_type("DUST");
    n = qd_undo_count(&A, -1);
    btap(QD_B_SAVE);
    run(600);
    CHECK(!A.ui.latch && !strcmp(LIB(&A, 0)->name, "DUST") && A.model.t[0].preset == LIB(&A, 0)->preset &&
              !strcmp(A.model.t[0].sound_name, "DUST") && qd_undo_count(&A, -1) == n + 1,
          "D U S T, SAVE: NEW as DUST, the track plays it; one undo level");
    id = LIB(&A, 0)->preset;
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    A.model.t[0].patch[QD_QP_HARM] = 700;
    A.model.t[0].edited = 1;
    run(20);
    btap(QD_B_SAVE);
    CHECK(A.ui.sv.replace == 0 && A.ui.ty.sel && !strcmp(A.ui.ty.name, "DUST"), "edited, SAVE: DUST selected, REPLACE "
          "offered");
    ty_type("DUSK");
    btap(F2);
    btap(QD_B_SAVE);
    run(600);
    CHECK(!A.ui.latch && !strcmp(LIB(&A, 0)->name, "DUSK") && LIB(&A, 0)->preset == id &&
              LIB(&A, 0)->patch[QD_QP_HARM] == 700 && !LIB(&A, 1)->name[0] && !strcmp(A.model.t[0].sound_name, "DUSK"),
          "D U S K, F2, SAVE: REPLACED under the typed name, the same preset, nothing new");
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    A.model.t[0].edited = 1;
    btap(QD_B_SAVE);
    ty_type("ZZZ");
    btap(QD_B_HOME);
    run(20);
    CHECK(!A.ui.latch && !strcmp(LIB(&A, 0)->name, "DUSK") && !LIB(&A, 1)->name[0] && A.model.t[0].edited &&
              !strcmp(A.model.t[0].sound_name, "DUSK"), "Z Z Z, HOME: cancelled, nothing changed");
}

static void g_p15u_shift(void)
{
    int k, view;
    ty_open();
    view = A.ui.view;
    ty_clear();
    bdown(SHIFT);
    run(10);
    for (k = 0; k < 11; k++)
        black(k);                                       /* (in the step view: SHIFT's shortcuts) */
    bdown(QD_B_REC);                                    /* (SHIFT + REC: the record settings; held: armed) */
    run(500);
    bup(QD_B_REC);
    run(10);
    bdown(QD_B_PLAY);                                   /* (PLAY held: F3) */
    run(600);
    bup(QD_B_PLAY);
    run(10);
    btap(QD_B_PATTERN);                                 /* (SHIFT + SEL: the global settings) */
    btap(QD_B_HOME);                                    /* (SHIFT + HOME: the next view) */
    btap(F2);                                           /* (SHIFT + F2: redo) */
    bup(SHIFT);
    run(10);
    CHECK(A.ui.latch == QD_LATCH_SAVE && !strcmp(A.ui.ty.name, "0123456789-") && A.ui.view == view &&
              A.ui.mode == QD_MODE_STEP, "SHIFT + the black keys typed 0..9 -; SHIFT + REC (held), PLAY (held), SEL, HOME, F2 "
          "nothing (%s)",
          A.ui.ty.name);
    btap(QD_B_HOME);
    CHECK(!A.ui.latch, "HOME: cancelled");
}

static void g_p15u_leds(void)
{
    qd_leds_t l;
    int i, ok;
    ty_open();
    l = leds();
    for (ok = 1, i = 0; i < 11; i++)
        ok &= l.key[BK(i)] == QD_LED_DIM;
    for (i = 0; i < 16; i++)
        ok &= l.key[WK(i)] == QD_LED_DIM;
    CHECK(ok && l.button[F1] == QD_LED_LIT, "naming: all 27 keys lit (dim), F1 lit");
    kdown(BK(B3));
    run(10);
    l = leds();
    for (ok = 1, i = 0; i < 11; i++)
        ok &= l.key[BK(i)] == (i == B3 ? QD_LED_LIT : QD_LED_DIM);
    kup(BK(B3));
    kdown(WK(9));
    run(10);
    l = leds();
    ok &= l.key[WK(9)] == QD_LED_LIT && l.key[WK(8)] == QD_LED_DIM;
    kup(WK(9));
    run(10);
    CHECK(ok && !strcmp(A.ui.ty.name, "CU"), "the key pressed bright (C, U typed)");
    btap(QD_B_HOME);
    bdown(QD_B_MIX);                                    /* the project's name: the same keyboard */
    run(10);
    bup(QD_B_MIX);
    run(10);
    black(B10);
    l = leds();
    for (ok = 1, i = 0; i < 16; i++)
        ok &= l.key[WK(i)] == QD_LED_DIM;
    CHECK(qd_view_screen(&A) == QD_SCR_NAME && ok && A.ui.ty.sel == !!A.per.pname[A.per.proj][0], "B10 on the project page: the name, the same keys "
          "lit");
    ty_type("NIGHT BUS");
    btap(QD_B_SAVE);
    CHECK(!strcmp(A.per.pname[A.per.proj], "NIGHT BUS") && A.ui.latch == QD_LATCH_PRJ, "N I G H T SPACE B U S, SAVE: "
          "the project named; its page (%s)", A.per.pname[A.per.proj]);
    black(B10);
    ty_type("XX");
    btap(QD_B_HOME);
    CHECK(!strcmp(A.per.pname[A.per.proj], "NIGHT BUS") && A.ui.latch == QD_LATCH_PRJ, "HOME: the old name, the page");
    btap(QD_B_HOME);
}

/* ================================================================ P1.5v: the owner's fixes === */
static void mine_add(const char *name, int k)           /* one of MY presets, from FM TONE preset k's sound */
{
    int16_t p[QD_QUAD_NP];
    qd_quad_preset_patch(k, p);
    qd_lib_store(&A, qd_lib_free_slot(&A), name, p);
}
static void g_p15v_delete(void)
{
    int k, lit = 0, off = 0, i;
    start();
    run(600);
    mine_add("GONE SOON", 3);
    turn(QD_K_PRESETS, 1);
    black(B9);
    CHECK(A.ui.br.src == QD_BR_MINE && br_code() == QD_BR_MINE_CODE, "(MY PRESETS: GONE SOON)");
    black(B10);
    k = qd_br_pool_k(&A);
    CHECK(k > 0, "B10: into the pool (a copy)");
    black(B11);
    CHECK(A.ui.br.del_armed && LIB(&A, 0)->name[0], "B11: armed (DELETE?: the red X), not deleted");
    for (i = 0; i < 12; i++) {
        const int l = leds().key[BK(B11)];
        lit += l == QD_LED_LIT, off += l == QD_LED_OFF;
        run(QD_FAST_MS / 2 + 1);
    }
    CHECK(lit && off, "B11 blinks (%d lit, %d dark)", lit, off);
    black(B1);
    CHECK(!A.ui.br.del_armed && LIB(&A, 0)->name[0], "another key (B1): disarmed, kept");
    black(B11);
    wtap(3);
    CHECK(!A.ui.br.del_armed && LIB(&A, 0)->name[0], "a white key: disarmed");
    black(B11);
    turn(QD_K_PRESETS, 1);
    CHECK(!A.ui.br.del_armed && LIB(&A, 0)->name[0], "a turn: disarmed");
    turn(QD_K_PRESETS, -5);
    black(B11);
    black(B11);
    CHECK(!LIB(&A, 0)->name[0] && A.ui.br.fx == QD_BRFX_DEL && !strcmp(A.ui.br.fx_snd.name, "GONE SOON"),
          "B11 again: deleted, it breaks up");
    CHECK(!qd_sound_used(&A.proj, k), "its reference gone from the pool too (P1.5w: the pool holds references)");
    run(900);
    btap(QD_B_HOME);
}

static void g_p15v_pool(void)
{
    int k;
    start();
    run(600);
    qd_sounds_clear(&A.proj);
    turn(QD_K_PRESETS, 1);
    turn(QD_K_PRESETS, 4 - A.ui.br.pos);
    CHECK(br_code() == 4 && !qd_br_pool_k(&A) && leds().key[BK(B10)] == QD_LED_LIT, "ALL, not in the pool: B10 lit (adds)");
    black(B10);
    k = qd_br_pool_k(&A);
    CHECK(k == 1 && A.ui.br.fx == QD_BRFX_IN && leds().key[BK(B10)] == qd_led_breath(A.ms), "B10: in; B10 breathing "
          "(takes out)");
    qd_step_on(PAT(), 1, 3);
    qd_step_set_sound(PAT(), 1, 3, k);
    black(B10);
    CHECK(A.ui.br.out_armed && qd_sound_used(&A.proj, k), "B10 on a sound a step uses (from ALL): armed (OUT?)");
    wtap(0);
    CHECK(!A.ui.br.out_armed && qd_sound_used(&A.proj, k), "a white key: disarmed");
    black(B10);
    black(B10);
    CHECK(!qd_sound_used(&A.proj, k) && !qd_br_pool_k(&A) && !R(1, 3).sound && !OV(1, 3, QD_PM_SOUND) &&
              A.ui.br.src == QD_BR_ALL && br_code() == 4, "B10 again: out, the step back to TRACK, still on it in ALL");
    mine_add("MINE ONE", 5);
    black(B9);
    CHECK(A.ui.br.src == QD_BR_MINE && br_code() == QD_BR_MINE_CODE, "(MY PRESETS: MINE ONE)");
    black(B10);
    CHECK(qd_br_pool_k(&A) > 0, "MY PRESETS, B10: in");
    black(B10);
    CHECK(!qd_br_pool_k(&A) && A.proj.nsound == 0, "B10: out at once (no step uses it)");
    btap(QD_B_HOME);
}

static void g_p15v_tags(void)
{
    qd_sound_t f, f0;
    qd_leds_t l;
    int i, ok;
    start();
    run(600);
    turn(QD_K_PRESETS, 1);
    turn(QD_K_PRESETS, -200);
    CHECK(br_code() == 0, "(INIT chosen)");
    shift_black(B5);
    qd_factory_get(&A, 0, &f);
    qd_factory_preset(0, &f0);
    CHECK(f.tags == (QD_PKB(QD_PK_LEAD) | QD_PKB(QD_PK_PAD)) && A.per.ftags[0] == (0x80 | f.tags) && A.ui.br.kind == 0 &&
              !memcmp(f.patch, f0.patch, sizeof f.patch), "SHIFT + B5: INIT tagged PAD too (LEAD, PAD), its sound as it "
          "was, the filter unchanged");
    black(B5);
    CHECK(A.ui.br.kind == 1 + QD_PK_PAD && br_code() == 0, "B5 (PAD): INIT listed");
    shift_black(B6);
    qd_factory_get(&A, 0, &f);
    CHECK(f.tags == QD_PKB(QD_PK_PAD), "SHIFT + B6: LEAD off (PAD only)");
    black(B6);
    for (ok = 1, i = 0; i < br_n(); i++) {
        A.ui.br.pos = i;
        ok &= br_code() != 0;
    }
    CHECK(ok, "B6 (LEAD): INIT not listed");
    mine_add("TAGGED", 3);
    black(B1);
    black(B9);
    shift_black(B8);
    CHECK(LIB(&A, 0)->tags == (QD_PKB(QD_PK_BASS) | QD_PKB(QD_PK_FX)), "MY preset: SHIFT + B8: FX too (BASS, FX)");
    shift_black(B1);
    shift_black(B9);
    CHECK(A.ui.latch == QD_LATCH_BROWSE && A.ui.br.kind == 0 && A.ui.br.src == QD_BR_MINE &&
              LIB(&A, 0)->tags == (QD_PKB(QD_PK_BASS) | QD_PKB(QD_PK_FX)), "SHIFT + B1, B9: nothing (no snapshot, no "
          "filter)");
    bdown(SHIFT);
    run(10);
    l = leds();
    CHECK(l.key[BK(B2)] == qd_led_breath_lo(A.ms) && l.key[BK(B8)] == qd_led_breath_lo(A.ms) &&
              l.key[BK(B3)] == QD_LED_OFF && l.key[BK(B1)] == QD_LED_LIT,
          "SHIFT held: the chosen's tags breathing (BASS, FX), the filter (ALL) solid (P1.5x)");
    bup(SHIFT);
    run(10);
    btap(QD_B_HOME);
}

static void g_p15v_save(void)
{
    start();
    run(600);
    btap(QD_B_SAVE);
    CHECK(A.ui.sv.replace < 0 && leds().button[F2] == QD_LED_OFF, "a factory sound: NEW only, F2 dark");
    btap(F2);
    CHECK(A.ui.sv.choice == 0 && A.ui.latch == QD_LATCH_SAVE, "F2: nothing");
    btap(QD_B_SAVE);
    run(600);
    CHECK(LIB(&A, 0)->name[0] && !A.ui.latch, "SAVE: NEW");
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    A.model.t[0].edited = 1;
    btap(QD_B_SAVE);
    CHECK(A.ui.sv.replace == 0 && A.ui.sv.choice == 0 && leds().button[F2] == QD_LED_LIT, "from one of MY presets: "
          "NEW chosen, F2 lit");
    btap(F2);
    btap(QD_B_SAVE);
    run(600);
    CHECK(!LIB(&A, 1)->name[0] && !A.ui.latch, "F2, SAVE: REPLACE (nothing new)");
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    A.model.t[0].edited = 1;
    btap(QD_B_SAVE);
    btap(F2);
    btap(F2);
    btap(QD_B_SAVE);
    run(600);
    CHECK(LIB(&A, 1)->name[0], "F2, F2, SAVE: NEW (%s)", LIB(&A, 1)->name);
}

static void g_p15v_picker(void)
{
    start();
    run(600);
    wtap(4);
    whold(4);
    turn(QD_K_PRESETS, 2);
    CHECK(A.ui.tk == QD_TK_PRESET && A.ui.tk_hold && qd_view_overlay(&A), "a step held + PRESETS: the pool picker");
    run(QD_TK_MS + QD_TK_FADE_MS + 800);
    CHECK(A.ui.tk == QD_TK_PRESET && qd_view_overlay(&A), "still up while held (no time-out)");
    wrel(4);
    run(20);
    CHECK(A.ui.tk == QD_TK_NONE && R(0, 4).sound == 2, "let go: closed, the choice kept (2)");
    mode(M_SOUND);
    whold(4);
    turn(QD_K_PRESETS, 1);
    run(QD_TK_MS + QD_TK_FADE_MS + 300);
    CHECK(A.ui.tk == QD_TK_PRESET && R(0, 4).sound == 3, "SOUND held: the picker too, up");
    wrel(4);
    run(20);
    CHECK(A.ui.tk == QD_TK_NONE, "let go: closed");
    mode(M_NOTE);
    btap(QD_B_MUTE);                                    /* the step latch: a selection (the cursor's step) */
    if (!A.ui.nheld)
        wtap(4);
    turn(QD_K_PRESETS, 1);
    run(QD_TK_MS + QD_TK_FADE_MS + 300);
    CHECK(A.ui.slatch && A.ui.tk == QD_TK_PRESET, "the latch's selection + PRESETS: up while selected");
    btap(QD_B_MUTE);
    run(20);
    CHECK(!A.ui.slatch && A.ui.tk == QD_TK_NONE, "the latch off (the selection cleared): closed");
}

/* ================================================================ P1.5w: the pool holds references === */
static void g_p15w_refs(void)
{
    int k, id, harm;
    start();
    run(600);
    mine_add("REF ME", 3);
    id = LIB(&A, 0)->preset;
    k = qd_pool_add_sound(&A, LIB(&A, 0));
    qd_step_on(PAT(), 1, 2);
    qd_step_set_sound(PAT(), 1, 2, k);
    qd_step_on(qd_proj_pat(&A.proj, A.seq.cur == 9 ? 10 : 9), 2, 3);
    qd_step_set_sound(qd_proj_pat(&A.proj, A.seq.cur == 9 ? 10 : 9), 2, 3, k);
    qd_ctl_select_track(&A, 0);                         /* track 1 plays REF ME (its own copy) */
    turn(QD_K_PRESETS, 1);
    black(B9);
    btap(F2);
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    harm = A.model.t[0].patch[QD_QP_HARM];
    CHECK(A.model.t[0].preset == id && !A.model.t[0].edited, "(track 1 loaded REF ME)");
    qd_ctl_select_track(&A, 1);                         /* track 2: from it, edited, SAVE: REPLACE */
    A.model.t[1].preset = (uint16_t)id;
    memcpy(A.model.t[1].patch, LIB(&A, 0)->patch, sizeof A.model.t[1].patch);
    A.model.t[1].patch[QD_QP_HARM] = 1500;
    A.model.t[1].edited = 1;
    run(20);
    btap(QD_B_SAVE);
    btap(F2);
    btap(QD_B_SAVE);
    run(600);
    CHECK(LIB(&A, 0)->patch[QD_QP_HARM] == 1500 && POOL(&A.proj, k)->patch[QD_QP_HARM] == 1500 &&
              R(1, 2).sound == k, "REPLACE: the pool's reference (and the step locking it) play the new REF ME");
    CHECK(A.model.t[0].patch[QD_QP_HARM] == harm && !A.model.t[0].edited, "track 1's own sound: its copy as it was");
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    turn(QD_K_PRESETS, 1);
    black(B9);
    if (br_code() != QD_BR_MINE_CODE)
        turn(QD_K_PRESETS, -5);
    black(B11);
    CHECK(A.ui.br.del_armed && A.ui.br.use_proj == 1 && A.ui.br.use_steps == 2, "DELETE?: its use, 1 project, 2 steps "
          "(%d, %d)", A.ui.br.use_proj, A.ui.br.use_steps);
    run(300);
    black(B11);
    CHECK(qd_lib_find(&A, id) < 0 && !qd_sound_used(&A.proj, k) && !R(1, 2).sound, "B11 again: the preset, its "
          "reference, the steps back to TRACK");
    btap(QD_B_HOME);
    run(600);
    undo_redo(-1);
    CHECK(qd_lib_find(&A, id) == 0 && POOL(&A.proj, k)->preset == id && R(1, 2).sound == k &&
              qd_proj_pat(&A.proj, A.seq.cur == 9 ? 10 : 9)->step[2][3].sound == k, "undo: the preset, its reference, the locks "
          "(the other pattern's too)");
}

/* ================================================================ P1.5x: presets carry their mix === */
static int mix_is(int t, int pan, int ch, int dl, int rv)
{
    const qd_mix_track_t *m = &A.model.t[t].mix;
    return m->pan == pan && m->chorus == ch && m->delay == dl && m->reverb == rv;
}
/* P1.5x (the owner): B2..B8 show the chosen preset's tags: breathing (30 % .. full); the filter key solid; the rest off */
static void g_p15x_tag_leds(void)
{
    qd_leds_t l;
    int i, ok, lo = 1000, hi = 0;
    start();
    run(600);
    mine_add("TWO TAGS", 3);
    qd_tags_set(&A, QD_BR_MINE_CODE, QD_PKB(QD_PK_BASS) | QD_PKB(QD_PK_PAD));
    turn(QD_K_PRESETS, 1);
    black(B9);                                          /* MY PRESETS, ALL: TWO TAGS */
    l = leds();
    for (ok = 1, i = 0; i < 8; i++)
        ok &= l.key[BK(i)] == (i == 0 ? QD_LED_LIT : i == 1 || i == 4 ? qd_led_breath_lo(A.ms) : QD_LED_OFF);
    CHECK(ok, "ALL: B1 solid, B2 (BASS) and B5 (PAD) breathing, the rest off");
    for (i = 0; i < 40; i++) {
        const int v = leds().key[BK(B2)];
        lo = v < lo ? v : lo, hi = v > hi ? v : hi;
        run(40);
    }
    CHECK(lo >= 70 && lo <= 90 && hi >= 250, "the breath: about 30 %% .. full over 1.5 s (%d .. %d)", lo, hi);
    black(B5);                                          /* the filter on one of its tags */
    l = leds();
    CHECK(l.key[BK(B5)] == QD_LED_LIT && l.key[BK(B2)] == qd_led_breath_lo(A.ms) && l.key[BK(B1)] == QD_LED_OFF,
          "B5 (PAD), one of its tags: solid; BASS breathing; B1 off");
    black(B1);                                          /* (back to ALL: the filter on another key, B1) */
    bdown(SHIFT);
    run(10);
    black(B6);                                          /* SHIFT + B6: LEAD on, at once */
    l = leds();
    CHECK(l.key[BK(B6)] == qd_led_breath_lo(A.ms) && l.key[BK(B1)] == QD_LED_LIT, "SHIFT + B6: LEAD starts breathing "
          "at once (SHIFT held)");
    black(B2);
    l = leds();
    CHECK(l.key[BK(B2)] == QD_LED_OFF, "SHIFT + B2: BASS off at once");
    bup(SHIFT);
    run(10);
    btap(QD_B_HOME);
}

static void g_p15x_mix(void)
{
    qd_sound_t ep;
    int lvl, k;
    start();
    run(600);
    qd_factory_preset(1, &ep);
    CHECK(!strcmp(ep.name, "EP") && ep.chorus == 30 && ep.delay == 20 && ep.reverb == 40 && !ep.pan,
          "factory EP: ChoralRoot's sends (chorus 30, delay 20, reverb 40), pan centre");
    A.model.t[0].mix.pan = -10, A.model.t[0].mix.chorus = 5, A.model.t[0].mix.delay = 6, A.model.t[0].mix.reverb = 7;
    lvl = A.model.t[0].mix.level;
    turn(QD_K_PRESETS, 1);
    turn(QD_K_PRESETS, -200);
    turn(QD_K_PRESETS, 1);                              /* EP */
    CHECK(br_code() == 1 && mix_is(0, 0, 30, 20, 40) && A.model.t[0].mix.level == lvl, "browsing EP: its pan and sends "
          "previewed (the level stays)");
    btap(F1);
    CHECK(!A.ui.latch && mix_is(0, -10, 5, 6, 7), "CANCEL: the track's own mix back");
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    turn(QD_K_PRESETS, 1);
    turn(QD_K_PRESETS, -200);
    turn(QD_K_PRESETS, 1);
    btap(F2);
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    CHECK(!A.ui.latch && mix_is(0, 0, 30, 20, 40) && A.model.t[0].preset == 2, "LOAD: EP with its pan and sends");
    A.model.t[0].mix.pan = 12, A.model.t[0].mix.chorus = 1, A.model.t[0].mix.delay = 2, A.model.t[0].mix.reverb = 3;
    A.model.t[0].edited = 1;
    btap(QD_B_SAVE);
    btap(QD_B_SAVE);
    run(600);
    CHECK(LIB(&A, 0)->pan == 12 && LIB(&A, 0)->chorus == 1 && LIB(&A, 0)->delay == 2 && LIB(&A, 0)->reverb == 3,
          "SAVE (NEW): the track's pan and sends saved with it");
    run(QD_TK_MS + QD_TK_FADE_MS + 10);
    A.model.t[0].mix.reverb = 99;
    A.model.t[0].edited = 1;
    btap(QD_B_SAVE);
    btap(F2);
    btap(QD_B_SAVE);
    run(600);
    CHECK(LIB(&A, 0)->reverb == 99 && !LIB(&A, 1)->name[0], "REPLACE: its mix too (reverb 99)");
    k = qd_pool_add_sound(&A, LIB(&A, 0));           /* a step's sound lock: the sound only */
    CHECK(k > 0 && POOL(&A.proj, k)->reverb == 99, "(in the pool, its mix in the cache)");
    A.model.t[1].mix.pan = -30, A.model.t[1].mix.chorus = 4, A.model.t[1].mix.delay = 4, A.model.t[1].mix.reverb = 4;
    qd_ctl_select_track(&A, 1);
    wtap(6);
    whold(6);
    turn(QD_K_PRESETS, k);
    wrel(6);
    CHECK(R(1, 6).sound == k && mix_is(1, -30, 4, 4, 4), "a step locking it: the track's pan and sends untouched");
}


/* ================================================================ P1.5z: the owner's batch === */
/* - POOL: its own animation (the cassette out of the lime tray, bubbles, the + back); the outline lime in the pool,
 *   champagne not, in every source, the animations ending in the right one */
static void g_p15z_pool(void)
{
    ui_browse_t v;
    int k;
    start();
    run(600);
    qd_sounds_clear(&A.proj);
    turn(QD_K_PRESETS, 1);
    turn(QD_K_PRESETS, 4 - A.ui.br.pos);
    qd_view_browse(&A, &v);
    CHECK(v.ocol == UI_LIB_COL && v.e[2].tray == 1, "ALL, not in the pool: the champagne outline, the tray's +");
    black(B10);
    k = qd_br_pool_k(&A);
    qd_view_browse(&A, &v);
    CHECK(k > 0 && v.fx == QD_BRFX_IN && v.ocol == UI_LIB_COL, "B10 in: the cassette on its way into the tray, still "
          "champagne");
    run(700);
    qd_view_browse(&A, &v);
    CHECK(v.ocol == UI_POOL_COL, "landed: the outline lime");
    run(400);
    black(B10);                                         /* out: its own animation */
    qd_view_browse(&A, &v);
    CHECK(!qd_br_pool_k(&A) && v.fx == QD_BRFX_OUT && v.fx_u < 0.2f && v.ocol == UI_POOL_COL && !v.users_row,
          "B10 out: - POOL's animation (the cassette pops out of the lime tray), still lime as it starts");
    run(500);
    qd_view_browse(&A, &v);
    CHECK(v.fx == QD_BRFX_OUT && v.fx_u > 0.45f && v.ocol == UI_LIB_COL, "popped out: the outline champagne again");
    run(500);
    qd_view_browse(&A, &v);
    CHECK(!v.fx && v.ocol == UI_LIB_COL && v.e[2].tray == 1, "after it: champagne, the tray's + back");
    black(B10);                                         /* the POOL source: lime; its OUT (confirmed) animates too */
    run(1000);
    k = qd_br_pool_k(&A);
    qd_step_on(PAT(), 1, 3);
    qd_step_set_sound(PAT(), 1, 3, k);
    black(B9);
    black(B9);
    qd_view_browse(&A, &v);
    CHECK(A.ui.br.src == QD_BR_POOL && v.ocol == UI_POOL_COL, "the POOL: lime");
    black(B10);
    black(B10);
    qd_view_browse(&A, &v);
    CHECK(!qd_sound_used(&A.proj, k) && v.fx == QD_BRFX_OUT && !v.users_row, "the POOL's OUT, confirmed: - POOL's "
          "animation (the users' row after it)");
    mine_add("MINE LIME", 5);
    black(B9);                                          /* (POOL -> ALL -> MY PRESETS) */
    black(B9);
    qd_view_browse(&A, &v);
    CHECK(A.ui.br.src == QD_BR_MINE && v.ocol == UI_LIB_COL, "MY PRESETS, not in the pool: champagne");
    black(B10);
    run(1000);
    qd_view_browse(&A, &v);
    CHECK(v.ocol == UI_POOL_COL, "MY PRESETS, in the pool: lime");
    btap(QD_B_HOME);
}

/* - SAVE PRESET: KNOB 1 turns the letter under the cursor, as SELECT */
static void g_p15z_save_knob1(void)
{
    start();
    run(600);
    btap(QD_B_SAVE);
    bdown(SHIFT);
    run(10);
    btap(F1);                                           /* (cleared) */
    bup(SHIFT);
    run(10);
    black(B1);                                          /* A */
    turn(QD_K_ALGO, -1);
    turn(QD_K_1, 2);                                    /* A -> C */
    CHECK(!strcmp(A.ui.ty.name, "C"), "KNOB 1: A turned to C (%s)", A.ui.ty.name);
    turn(QD_K_SELECT, -1);                              /* SELECT still: C -> B */
    CHECK(!strcmp(A.ui.ty.name, "B"), "SELECT too: B (%s)", A.ui.ty.name);
    turn(QD_K_ALGO, 1);
    turn(QD_K_1, 1);                                    /* at the end: a new character */
    CHECK(!strcmp(A.ui.ty.name, "BA"), "KNOB 1 at the end: a new one (%s)", A.ui.ty.name);
    btap(QD_B_HOME);
}

/* - SHIFT + ALGORITHM / PRESETS: the scale / root takeover in EDIT, the PIANO ROLL and STEPS */
static void g_p15z_scale_popup(void)
{
    static const int VIEWS[3] = {QD_VIEW_EDIT, QD_VIEW_ROLL, QD_VIEW_STEPS};
    ui_turn_t tv;
    int i, ok = 1, sc0;
    start();
    for (i = 0; i < 3; i++) {
        A.ui.view = VIEWS[i];
        run(50);
        sc0 = PAT()->trk[0].scale;
        bdown(SHIFT);
        run(10);
        turn(QD_K_ALGO, 1);
        qd_view_turn(&A, &tv);
        ok &= A.ui.tk == UI_TK_SCALE && qd_view_overlay(&A) && PAT()->trk[0].scale == sc0 + 1 &&
              !strcmp(tv.val, qd_scale_name(sc0 + 1)) && tv.scale_mask != 0;
        turn(QD_K_PRESETS, 2);
        qd_view_turn(&A, &tv);
        ok &= A.ui.tk == UI_TK_ROOT && qd_view_overlay(&A) && tv.root == 2;
        bup(SHIFT);
        run(QD_TK_MS + QD_TK_FADE_MS + 20);
        PAT()->trk[0].scale = (uint8_t)sc0, PAT()->trk[0].root = 0;
        if (!ok)
            printf("    (view %d)\n", VIEWS[i]);
    }
    CHECK(ok, "SHIFT + ALGORITHM: the scale's takeover (its name, its keys), SHIFT + PRESETS the root's, over EDIT, the "
          "PIANO ROLL and STEPS");
    A.ui.view = QD_VIEW_EDIT;
    btap(QD_B_HOME);
}

/* - the sound editor: SHIFT + B1..B11 do their jobs (GLO held + a black key, as the panel delivers them); no SHIFT map */
static void g_p15z_editor_shift(void)
{
    qd_leds_t l;
    int m0, f0, t0, ok;
    start();
    qd_step_set_trig(PAT(), 0, 0, QD_TRIG_NOTE);
    btap(QD_B_SOUND);
    run(50);
    bdown(SHIFT);
    run(600);                                           /* (held long: a map would be up by now) */
    l = leds();
    CHECK(qd_view_screen(&A) == QD_SCR_SOUND && l.key[BK(B1)] == QD_LED_LIT && l.key[BK(B2)] == QD_LED_OFF &&
              l.key[BK(B8)] == QD_LED_LIT, "SHIFT held in the editor: no map (the editor stays), its shortcuts lit");
    black(B1);
    CHECK(A.undo.snap_ok && qd_snd_open(&A), "SHIFT + B1: the snapshot stored, the editor open");
    black(B4);
    CHECK(A.undo.pclip_ok, "SHIFT + B4: the pattern copied");
    m0 = A.metro;
    black(B7);
    CHECK(A.metro == !m0, "SHIFT + B7: the click on / off");
    f0 = A.ui.follow;
    black(B8);
    CHECK(A.ui.follow == !f0 && A.ui.tk == UI_TK_FOLLOW, "SHIFT + B8: page follow on / off, its takeover");
    t0 = A.ui.ntap;
    black(B6);
    CHECK(A.ui.ntap == t0 + 1 && A.ui.tk == UI_TK_TAP, "SHIFT + B6: a tap, the TAP takeover");
    black(B9);
    CHECK(qd_track_loop(&PAT()->trk[0]) == 3, "SHIFT + B9: the track doubled");
    black(B11);
    black(B11);
    CHECK(!R(0, 0).on, "SHIFT + B11 twice: the track cleared");
    black(B3);
    CHECK(R(0, 0).on, "SHIFT + B3: the snapshot recalled");
    bup(SHIFT);
    run(20);
    ok = A.ui.snd_page == QD_SND_START_PAGE;
    black(B5);
    CHECK(ok && QD_SND_PAGES[A.ui.snd_page].group == QD_SG_AMP, "SHIFT let go: B5 the editor's group again");
    btap(QD_B_SOUND);
    btap(QD_B_HOME);
}

/* - SHIFT + B6: TAP: from the first tap, the BPM after two, closed 2.8 s after the last (0.9.2, the TAP ape: its
 * finale at 2 s); an external clock: dimmed. The ape's poses: the first tap point-and-squat, taps 2.. the ten round
 * (step .. back freeze), 180 BPM and up the headstand; a count begun while the screen is up: no bounce in */
static void g_p15z_tap(void)
{
    ui_turn_t tv;
    int i;
    start();
    qd_ctl_set_tempo(&A, 120);
    bdown(SHIFT);
    run(20);
    black(B6);
    qd_view_turn(&A, &tv);
    CHECK(A.ui.tk == UI_TK_TAP && tv.ntap == 1 && A.bpm == 120 && qd_view_overlay(&A),
          "the first tap: the TAP takeover, one dot, no tempo yet");
    run(400 - 30);
    black(B6);
    qd_view_turn(&A, &tv);
    CHECK(tv.ntap == 2 && A.bpm >= 148 && A.bpm <= 152 && tv.value == A.bpm, "the second tap, 400 ms on: the BPM (%d)",
          A.bpm);
    for (i = 0; i < 2; i++) {
        run(400 - 30);
        black(B6);
    }
    qd_view_turn(&A, &tv);
    CHECK(tv.ntap == 4 && A.bpm >= 148 && A.bpm <= 152 && tv.tap_age[0] < tv.tap_age[3], "four taps: %d BPM, the dots "
          "newest first", A.bpm);
    CHECK(tv.ntotal == 4 && tv.tap_bpm[0] == A.bpm && tv.tap_bpm[1] >= 148 && tv.tap_bpm[1] <= 152 && !tv.tap_cont,
          "the ape's model: the count 4, the BPM of the last two taps (%d %d)", tv.tap_bpm[0], tv.tap_bpm[1]);
    bup(SHIFT);
    run(2700);
    CHECK(A.ui.tk == UI_TK_TAP, "2.7 s after the last tap: still up (the ape's finale from 2 s)");
    bdown(SHIFT);
    run(20);
    black(B6);                                          /* (2.75 s on: a new count, the screen still up) */
    qd_view_turn(&A, &tv);
    CHECK(A.ui.ntap == 1 && tv.ntotal == 1 && tv.tap_cont && !tv.tap_bpm[0], "a tap 2.75 s on: a new count, begun "
          "while the screen was up (no bounce in)");
    bup(SHIFT);
    run(2700);
    CHECK(A.ui.tk == UI_TK_TAP, "2.7 s on: still up");
    run(250);
    CHECK(A.ui.tk == QD_TK_NONE, "2.8 s after it (and the fade): closed");
    bdown(SHIFT);
    run(20);
    black(B6);
    qd_view_turn(&A, &tv);
    CHECK(tv.ntotal == 1 && !tv.tap_cont, "a tap after the close: the ape bounces in");
    for (i = 0; i < 4; i++) {                           /* 0.3 s apart: 200 BPM, the flurry */
        run(300 - 40);
        black(B6);
    }
    qd_view_turn(&A, &tv);
    CHECK(tv.tap_bpm[0] >= 195 && ui_ape_pose(tv.ntotal, tv.tap_bpm[0]) == 9, "taps at 200 BPM: the headstand's "
          "flurry (%d BPM)", tv.tap_bpm[0]);
    bup(SHIFT);
    run(3000);
    {   /* the order: 1 the point, 2..11 the ten, round from 12 (the step again); 180 BPM: the headstand */
        static const int WANT[14] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1, 2, 3};
        int ok = 1, n;
        for (n = 1; n <= 14; n++)
            ok &= ui_ape_pose(n, 120) == WANT[n - 1];
        CHECK(ok && ui_ape_pose(1, 200) == 0 && ui_ape_pose(5, 179) == 4 && ui_ape_pose(5, 180) == 9,
              "the poses' order: point, step .. back freeze, round; the first tap always the point; 180 up the headstand");
    }
}

/* - SHIFT + B8: PAGE FOLLOW, its takeover; SHIFT's map: B8's glyph, SELECT's track row */
static void g_p15z_follow(void)
{
    ui_turn_t tv;
    ui_shift_t sv;
    int f0, i, sel_row = 0;
    start();
    bdown(SHIFT);
    run(400);
    qd_view_shift(&A, &sv);
    for (i = 0; i < sv.n; i++)
        sel_row |= sv.ctl[i] == UI_SC_SELECT && sv.icon[i] == UI_IC_TRACK;
    CHECK(sv.keys && sel_row && leds().key[BK(B8)] == QD_LED_LIT && qd_view_screen(&A) == QD_SCR_SHIFT,
          "SHIFT's map: SELECT the track, B8 (page follow) lit");
    f0 = A.ui.follow;
    black(B8);
    qd_view_turn(&A, &tv);
    CHECK(A.ui.follow == !f0 && A.ui.tk == UI_TK_FOLLOW && tv.value == A.ui.follow &&
              !strcmp(tv.val, A.ui.follow ? "ON" : "OFF"), "SHIFT + B8: page follow %s, its takeover",
          A.ui.follow ? "ON" : "OFF");
    black(B8);
    CHECK(A.ui.follow == f0, "again: back");
    bup(SHIFT);
    run(20);
}

/* - P2.11: the Geek Out pages: SHIFT + HOME's cycle after EDIT (and back to the PIANO ROLL), the knobs do nothing
 *   there, the platform's figures dim until given, a steal counted on its track and flashing, SHIFT + B11 the counters
 *   from zero (the track not cleared), the scope on the loudest track held on a rising zero crossing */
static void g_p211_geek(void)
{
    static const int V[5] = {QD_VIEW_METERS, QD_VIEW_NUMBERS, QD_VIEW_VOICES, QD_VIEW_SCOPE, QD_VIEW_ROLL};
    ui_geek_t gv;
    int i, ok = 1, bpm0, sel0, on0, flat = 1;
    uint32_t r0;
    float mx = 0.f;
    start();
    A.ui.view = QD_VIEW_EDIT;
    for (i = 0; i < 5; i++) {
        shome();
        ok &= A.ui.view == V[i] && (i == 4 || qd_view_screen(&A) == QD_SCR_GEEK);
    }
    CHECK(ok, "SHIFT + HOME: EDIT -> METERS -> NUMBERS -> VOICES -> SCOPE -> PIANO ROLL (each a Geek Out page)");
    A.ui.view = QD_VIEW_METERS;
    bpm0 = A.bpm, sel0 = A.sel;
    turn(QD_K_ALGO, 3);
    turn(QD_K_SELECT, 1);
    turn(QD_K_PRESETS, 2);
    turn(QD_K_1, 2);
    CHECK(A.bpm == bpm0 && A.sel == sel0 && A.ui.view == QD_VIEW_METERS && A.ui.tk == QD_TK_NONE,
          "the knobs do nothing there (tempo %d, track %d)", A.bpm, A.sel + 1);
    qd_view_geek(&A, &gv);
    CHECK(!gv.load_on && !gv.isr_on && !gv.ram_on && gv.page == 0, "no figures from the platform: dim");
    A.stats.have = QD_ST_LOAD0 | QD_ST_ISR | QD_ST_FPS;
    A.stats.load_pm[0] = 650, A.stats.isr_us = 1027, A.stats.isr_budget_us = 2902, A.stats.fps10 = 600;
    run(600);
    qd_view_geek(&A, &gv);
    CHECK(gv.load_on && gv.load > 0.63f && gv.load < 0.67f && gv.load_pct == 65 && gv.isr_us == 1027 && gv.fps_n == 60,
          "the platform's figures: the bars chase them, the numbers read them (load %.3f, ISR %d, FPS %d)", gv.load,
          gv.isr_us, gv.fps_n);
    A.pool.stolen[2] += 1;                              /* (a held note of track 3 taken) */
    run(30);
    qd_view_geek(&A, &gv);
    CHECK(gv.tsteals[2] == 1 && gv.steals == 1 && gv.tflash[2] > 0.5f && gv.steal_flash > 0.5f,
          "a steal: on its track, counted, flashing (%.2f)", gv.tflash[2]);
    on0 = R(0, 0).on;
    r0 = A.stats.reset;
    A.ui.view = QD_VIEW_NUMBERS;
    bdown(SHIFT);
    run(10);
    black(B11);
    bup(SHIFT);
    run(300);
    qd_view_geek(&A, &gv);
    CHECK(A.stats.reset == r0 + 1 && gv.steals == 0 && R(0, 0).on == on0 && !A.ui.clr_armed,
          "SHIFT + B11 there: the counters from zero (the platform told), the track not cleared");
    A.ui.view = QD_VIEW_SCOPE;
    qd_app_midi(&A, 0x90, 57, 110);                     /* (A3, track 1, held) */
    run(400);
    qd_view_geek(&A, &gv);
    for (i = 0; i < UI_GEEK_SCOPE; i++) {
        mx = fabsf(gv.scope[i]) > mx ? fabsf(gv.scope[i]) : mx;
        flat &= fabsf(gv.scope[i]) < 0.02f;
    }
    CHECK(gv.scope_track == 0 && !flat && mx > 0.5f && fabsf(gv.scope[0]) < 0.25f && gv.scope[2] >= gv.scope[0],
          "the scope: track 1 (the loudest), its wave from a rising zero crossing (%.2f, %.2f; peak %.2f)",
          gv.scope[0], gv.scope[2], mx);
    qd_app_midi(&A, 0x80, 57, 0);
    run(2000);
    qd_view_geek(&A, &gv);
    CHECK(gv.scope_track < 0, "silence: the flat line");
}

/* - SHIFT + SELECT: the track in every view; the browser's preview put back first; not in the typewriter */
static void shift_sel(int d)
{
    bdown(SHIFT);
    run(10);
    turn(QD_K_SELECT, d);
    bup(SHIFT);
    run(10);
}
static void g_p15z_shift_select(void)
{
    static const int VIEWS[3] = {QD_VIEW_ROLL, QD_VIEW_STEPS, QD_VIEW_EDIT};
    int16_t own[QD_QUAD_NP];
    qd_mix_track_t mix0;
    int i, ok = 1, pg;
    start();
    for (i = 0; i < 3; i++) {
        A.ui.view = VIEWS[i];
        pg = A.ui.page[A.sel];
        shift_sel(1);
        ok &= A.sel == i + 1 && A.ui.tk == UI_TK_TRACKBIG && qd_view_overlay(&A) && A.ui.page[0] == pg;
    }
    CHECK(ok, "the PIANO ROLL, STEPS, EDIT: SHIFT + SELECT the next track, the big track takeover (%d)", A.sel + 1);
    shift_sel(-5);
    CHECK(A.sel == 4, "round: track 4 - 5 = track 5 (six tracks) (%d)", A.sel + 1);
    shift_sel(2);
    CHECK(A.sel == 0, "and on round: track 1 (%d)", A.sel + 1);
    btap(QD_B_REC);                                     /* the keyboard */
    shift_sel(1);
    CHECK(A.ui.mode == QD_MODE_KEYS && A.sel == 1, "the keyboard: the track (the swing is F3's)");
    btap(QD_B_REC);
    btap(QD_B_SOUND);                                   /* the sound editor: the new track's sound */
    shift_sel(1);
    {
        static ui_snd_t sv;
        qd_view_snd(&A, &sv);
        CHECK(qd_snd_open(&A) && A.sel == 2 && sv.v[0] == A.model.t[2].patch[qd_snd_ids(&A, A.ui.snd_page)[0]],
              "the sound editor: track 3, its sound on the page");
    }
    btap(QD_B_SOUND);
    btap(QD_B_HOME);
    memcpy(own, A.model.t[2].patch, sizeof own);        /* the browser: the preview put back exactly */
    mix0 = A.model.t[2].mix;
    turn(QD_K_PRESETS, 1);
    turn(QD_K_PRESETS, 3);
    CHECK(memcmp(own, A.model.t[2].patch, sizeof own) != 0, "(browsing on track 3: a preset previewed)");
    shift_sel(1);
    CHECK(A.ui.latch == QD_LATCH_BROWSE && A.sel == 3 && A.ui.br.t == 3 && !A.ui.br.moved &&
              !memcmp(own, A.model.t[2].patch, sizeof own) && !memcmp(&mix0, &A.model.t[2].mix, sizeof mix0),
          "the browser: track 3's own sound and mix back exactly, track 4 browsed (latch %d sel %d t %d moved %d own %d mix %d)",
          A.ui.latch, A.sel, A.ui.br.t, A.ui.br.moved, !memcmp(own, A.model.t[2].patch, sizeof own),
          !memcmp(&mix0, &A.model.t[2].mix, sizeof mix0));
    btap(QD_B_HOME);
    btap(QD_B_PAGE);                                    /* the mix */
    shift_sel(1);
    CHECK(A.ui.latch == QD_LATCH_MIX && A.sel == 4, "the mix: track 5");
    btap(QD_B_HOME);
    btap(QD_B_SAVE);                                    /* the typewriter: SELECT the letter, not the track */
    bdown(SHIFT);
    run(10);
    btap(F1);
    bup(SHIFT);
    run(10);
    black(B1);
    turn(QD_K_ALGO, -1);
    shift_sel(1);
    CHECK(A.sel == 4 && !strcmp(A.ui.ty.name, "B"), "the typewriter: SHIFT + SELECT the letter (%s), the track kept",
          A.ui.ty.name);
    btap(QD_B_HOME);
}


/* ================================================================ P1.6l: the controls' labels === */
static qd_labels_t LB;
static const char *lab(int i)
{
    qd_ctl_labels(&A, &LB);
    return LB.s[i];
}
#define LW(w) (QD_LBL_W0 + (w))
#define LBK(b) (QD_LBL_B0 + (b))
#define LBT(b) (QD_LBL_BTN0 + (b))
#define LKN(k) (QD_LBL_KNOB0 + (k))
static int labels_clean(void)                           /* every label: at most ten, upper case, printable */
{
    int i, k;
    qd_ctl_labels(&A, &LB);
    for (i = 0; i < QD_LBL_N; i++) {
        if (strlen(LB.s[i]) > 10)
            return 0;
        for (k = 0; LB.s[i][k]; k++)
            if ((LB.s[i][k] >= 'a' && LB.s[i][k] <= 'z') || LB.s[i][k] < 0x20 || LB.s[i][k] > 0x7E)
                return 0;
    }
    return 1;
}
static void g_p16l_labels(void)
{
    start();
    mode(M_VEL);
    CHECK(!strcmp(lab(LW(0)), "STEP 1") && !strcmp(lab(LW(15)), "STEP 16") && !strcmp(lab(LBK(0)), "NOTE") &&
              !strcmp(lab(LBK(1)), "VEL") && !strcmp(lab(LBK(10)), "P-LOCK") && !strcmp(lab(LKN(QD_K_SELECT)), "PAGE") &&
              !strcmp(lab(LKN(QD_K_ALGO)), "VEL DEF") && !strcmp(lab(LBT(QD_B_PLAY)), "PLAY") &&
              !strcmp(lab(LBT(QD_B_OCTDN)), "F1") && !strcmp(lab(QD_LBL_MASTER), "VOLUME") && labels_clean(),
          "the step view: STEP 1..16, the hold modes, SELECT PAGE, ALGORITHM VEL DEF, PLAY, F1, MASTER VOLUME");
    bdown(SHIFT);
    run(20);
    CHECK(!strcmp(lab(LBK(0)), "SNAP SAVE") && !lab(LBK(1))[0] && !strcmp(lab(LBK(5)), "TAP") &&
              !strcmp(lab(LBK(7)), "FOLLOW") && !strcmp(lab(LBK(10)), "CLEAR TRK") && !lab(LW(0))[0] &&
              !strcmp(lab(LKN(QD_K_SELECT)), "TRACK") && !strcmp(lab(LKN(QD_K_ALGO)), "SCALE") &&
              !strcmp(lab(LKN(QD_K_PRESETS)), "ROOT") && !strcmp(lab(LBT(QD_B_OCTDN)), "UNDO") &&
              !strcmp(lab(LBT(QD_B_OCTUP)), "REDO") && !strcmp(lab(LBT(QD_B_HOME)), "VIEW") && labels_clean(),
          "SHIFT held: its shortcuts (B2 none), SELECT TRACK, SCALE, ROOT, UNDO, REDO, VIEW; the white keys none");
    bup(SHIFT);
    run(20);
    whold(2);                                           /* a step held in VEL: the values */
    CHECK(!strcmp(lab(LBK(0)), "13") && !strcmp(lab(LBK(9)), "127") && !strcmp(lab(LBK(10)), "RESET") &&
              !strcmp(lab(LKN(QD_K_PRESETS)), "SOUND LOCK") && labels_clean(),
          "a step held in VEL: B1 13 .. B10 127, B11 RESET, PRESETS SOUND LOCK (%s %s)", lab(LBK(0)), lab(LBK(9)));
    wrel(2);
    btap(QD_B_SOUND);                                   /* the sound editor on FILTER */
    A.ui.snd_page = qd_snd_page_of("FILTER");
    CHECK(!strcmp(lab(LKN(QD_K_1)), qd_quad_value_name(QD_QP_FREQ)) &&
              !strcmp(lab(LKN(QD_K_2)), qd_quad_value_name(QD_QP_RESO)) && !strcmp(lab(LBK(3)), "FILTERS") &&
              !strcmp(lab(LBK(0)), "PLAY/MOD") && !strcmp(lab(LBK(8)), "DICE") && !strcmp(lab(LBT(QD_B_SOUND)), "CLOSE") &&
              labels_clean(), "the sound editor on FILTER: KNOB 1 %s, KNOB 2 %s, the groups, B9 DICE, EDIT CLOSE",
          lab(LKN(QD_K_1)), lab(LKN(QD_K_2)));
    btap(QD_B_SOUND);
    btap(QD_B_HOME);
    run(600);
    turn(QD_K_PRESETS, 1);                              /* the browser */
    {
        const int in = qd_br_pool_k(&A) > 0;
        CHECK(A.ui.latch == QD_LATCH_BROWSE && !strcmp(lab(LBK(0)), "ALL") && !strcmp(lab(LBK(1)), "BASS") &&
                  !strcmp(lab(LBK(7)), "FX") && !strcmp(lab(LBK(8)), "MY PRESETS") &&
                  !strcmp(lab(LBK(9)), in ? "-POOL" : "+POOL") && !strcmp(lab(LBT(QD_B_OCTUP)), "LOAD") &&
                  !strcmp(lab(LBT(QD_B_OCTDN)), "CANCEL") && labels_clean(),
              "the browser: ALL, BASS .. FX, B9 the next source, B10 %s, F2 LOAD, F1 CANCEL", lab(LBK(9)));
        black(B10);
        CHECK(!strcmp(lab(LBK(9)), in ? "+POOL" : "-POOL"), "B10: the other way now (%s)", lab(LBK(9)));
    }
    btap(QD_B_HOME);
    btap(QD_B_SAVE);                                    /* the typewriter, with SHIFT */
    CHECK(!strcmp(lab(LBK(0)), "A") && !strcmp(lab(LW(15)), "SPACE") && !strcmp(lab(LKN(QD_K_ALGO)), "CURSOR") &&
              !strcmp(lab(LKN(QD_K_PRESETS)), "NEW NAME") && !strcmp(lab(LKN(QD_K_1)), "LETTER") &&
              !lab(LKN(QD_K_2))[0] && !strcmp(lab(LBT(QD_B_OCTDN)), "DELETE") && !lab(LBT(QD_B_PLAY))[0],
          "the typewriter: A .. K, L .. SPACE, CURSOR, NEW NAME, LETTER, DELETE; PLAY none");
    bdown(SHIFT);
    run(20);
    CHECK(!strcmp(lab(LBK(0)), "0") && !strcmp(lab(LW(0)), ".") && !strcmp(lab(LBT(QD_B_OCTDN)), "CLEAR") &&
              labels_clean(), "the typewriter with SHIFT: B1 0, W1 ., F1 CLEAR");
    bup(SHIFT);
    run(20);
    btap(QD_B_HOME);
    fresh();
    CHECK(!strcmp(lab(LBT(QD_B_HOME)), "WAKE") && !lab(LW(0))[0] && !lab(LKN(QD_K_SELECT))[0] && labels_clean(),
          "asleep: HOME WAKE, the rest none but PLAY and MASTER");
}

int main(void)
{
    static const struct {
        const char *name;
        void (*fn)(void);
    } G[] = {
        {"wake and idle", g_wake},
        {"W tap: a trig with no overrides / removed; SHIFT + W free", g_tap},
        {"hold vs tap at the 250 ms edge", g_edge},
        {"B1..B11: the hold mode (B11 P-LOCK)", g_modes},
        {"every mode's ten values, B11, several held", g_values},
        {"ALGORITHM: defaults (nothing held), finely (held)", g_algo},
        {"SELECT: pages", g_select},
        {"NOTE entry: replace / add / octave / scale", g_notes},
        {"SOUND's banks; PRESETS = the sound lock", g_sound},
        {"KNOB 1..4: p-locks held, the sound not", g_plocks},
        {"F1 pages: tap, twice, hold, A..B; B9 / B10; undo", g_f1_pages},
        {"F1 + W: copy, then paste; one undo; F2 pre-loaded", g_f1_steps},
        {"F2: Form's pick-up / drop, press by press", g_f2},
        {"SEL: select, tap mute, hold solo, B11, LEDs", g_sel},
        {"SEQ: tap queues by launch mode, hold at once", g_seq},
        {"F3: base / type keys and knobs, start / end, 21 rates", g_f3},
        {"NOTE, a step held: each knob its own note; ALGO all", g_note_knobs},
        {"five and six notes by the keys", g_five_keys},
        {"P-LOCK: the screen stays, knobs lock, B11, undo", g_plock},
        {"the OCT LEDs' blink levels; the octave takeover", g_octave},
        {"REC, REC + PLAY, SHIFT + REC, step / live rec", g_rec},
        {"the keyboard in a scale, the global key", g_keyboard_scale},
        {"ARP = FILL", g_fill_page},
        {"page follow", g_follow},
        {"P1.4b: the track settings, the mix, the effects", g_tracks_screen},
        {"MIDI notes into a held step", g_midi_held},
        {"the layer maps, SHIFT's map", g_maps},
        {"P1.3d: the knob layout, a step held, every mode", g_knobs_held},
        {"P1.3d: the knob layout, nothing held (defaults)", g_knobs_default},
        {"P1.3d: P-LOCK, nothing held: ALGORITHM the track", g_plock_track},
        {"P1.3d: SEQ + B1..B4 banks", g_banks},
        {"P1.3d: the stopped preview", g_preview},
        {"P1.3d: F3 latched: open, use, OCT- / HOME close", g_f3_latch},
        {"P1.3d/f: the step latch (LFO), selection by gesture", g_step_latch},
        {"P1.3e: a mode key held edits the default; B11", g_mkey},
        {"P1.3y: the value keys light the nearest value", g_nearest},
        {"P1.3y: SHIFT + REC: the record settings; quantize, overdub", g_rec_settings},
        {"P1.3y: SHIFT + SEL: page follow; layout A; lmode MODES", g_knob_layouts},
        {"P1.3y: SEL tapped: nothing", g_sel_tap},
        {"P1.4a: the sound editor on EDIT", g_snd},
        {"P1.3e: six notes; the default chord moves together", g_six},
        {"P1.3e: KNOB 4, the quiet knob", g_quiet},
        {"P1.3e: HOME cycles PIANO ROLL / STEPS", g_home},
        {"P1.3i: the tempo and MIDI clock (EXT CLOCK)", g_ext_clock},
        {"P1.3q: STEPS: the steps' kinds by colour", g_steps_kinds},
        {"P1.3q: STEPS: PRESETS the step cursor", g_cursor},
        {"P1.3s: STEPS playing: no outline but the selection's", g_play_outline},
        {"P1.3r: the EDIT view", g_edit},
        {"P1.3w: FUNC on B9: the functions, the jump's target, SOUND on B10", g_func},
        {"P1.3x: PLAY held F3; REC held live recording, growing", g_live},
        {"the TAKE CAPTURE: cleared, the take's length, micro kept", g_capture},
        {"the TAKE REPLACE / OVERDUB: the first note starts it", g_take_replace},
        {"REC + PLAY, armed + PLAY: recording with playback", g_rec_play},
        {"P1.5b: the editor's keyboard SCALE / FULL, the scale", g_p15b_keys},
        {"P1.5b: the lock chips", g_p15b_chips},
        {"P1.5b: FX overview, routing", g_p15b_fx},
        {"P1.5b: knob acceleration", g_p15b_accel},
        {"P1.5b: the latch modes VALUES / MODES", g_p15b_latch},
        {"P1.5d: the frames the other way round", g_p15d_frames},
        {"P1.5f: VEL 127, the stunt show", g_p15f_jump},
        {"P1.5i: RATIO B the pair, MIX on RATIO", g_p15i_ratio},
        {"P1.5j: the editor's foot clear of the frame", g_p15j_margins},
        {"P1.5k: the LFOs: keys, DEST, hold-to-map", g_p15k_lfo},
        {"E4: the modulation slots: B1's hold-to-map, MOD, MIDI", g_e4_mods},
        {"E4.1: FILTER's machines in the Digitone II's order", g_e41_machines},
        {"P1.6c: the MIDI FX screen", g_p16c_screen},
        {"P1.6c: ARP tapped: the arp on / off", g_p16c_arp},
        {"P1.6c: TRIG's MIDI FX on a held step", g_p16c_trig},
        {"P1.6d: the sends lock on a held step", g_p16d_sends},
        {"P1.6d: automation recording", g_p16d_auto},
        {"P1.6d: track kinds and names", g_p16d_kinds},
        {"P1.6e: the stuck slot move (regression)", g_p16e_stuck},
        {"P1.6e: the MIDI FX screen's holds, F1 / F2, STEPS", g_p16e_mfx},
        {"P1.6e: the preview through the step's group", g_p16e_preview},
        {"P1.6f: F1 / F2 in the MIDI FX screen", g_p16f_f1f2},
        {"P1.6f: ARP's breath, STEPS / KEYS shared", g_p16f_keys},
        {"P1.6g: recording in every view", g_p16g_rec},
        {"P1.6g: FILTER's popup, RATIO B, following, the ARP's walk", g_p16g_misc},
        {"P1.6h: F1 leaves no main page; the pop-ups go back", g_p16h_f1},
        {"P2.1: the undo log at the device's sizes", g_p21_undo_log},
        {"P2.1c: the one log: the snapshot, the clipboard, the gesture's copies", g_p21c_one_log},
        {"P1.6h: F1 / F2 + W on every page of steps, whole", g_p16h_copy},
        {"P1.6h: the mix's locks: the selection, the count, the chips", g_p16h_mixlock},
        {"P1.6d: the step latch's SHIFT, defaults, double tap", g_p16d_latch},
        {"P1.6d: the FILTER machines' popup", g_p16d_filter},
        {"P1.6d: ARP's LED and hold, the groups' glide, slot moves", g_p16d_mfx},
        {"P1.5l: the editor's black keys regrouped", g_p15l_groups},
        {"P1.5m: the F layers with the step latch on", g_p15m_flatch},
        {"P1.5n/o: the octave tap's takeover, SELECT moves all, LFO's LED, MIX on TONE", g_p15n},
        {"P1.5o: locks over a sound lock, F2 unlocks, the default's VEL green", g_p15o_locks},
        {"P1.5c: undo / redo, eight levels", g_p15c_undo},
        {"P1.5c: SHIFT's keys: snapshot, tap, click, double, clear", g_p15c_shift},
        {"P1.5c: randomise: the dice, the bottle, the rules", g_p15c_random},
        {"P1.5p: the knob band clear of every held screen's head", g_p15p_band},
        {"P1.5q: EDIT follows the step playing", g_p15q_follow},
        {"P1.5s: the latch's preview only on a finger or the cursor", g_p15s_preview},
        {"P1.5s / P1.6g: EDIT follows with the latch off only; muted", g_p15s_follow},
        {"P1.5t: the library browser: preview, cancel, load, kinds, sources", g_p15t_browse},
        {"P1.5t: + POOL, - POOL, undo", g_p15t_pool},
        {"P1.5t: delete one of MY presets", g_p15t_delete},
        {"P1.5t: a step + PRESETS: TRACK and the pool", g_p15t_picker},
        {"P1.5t: SAVE PRESET: NEW, the dice, naming, REPLACE", g_p15t_save},
        {"P1.5u: the typewriter: every key, with and without SHIFT", g_p15u_keys},
        {"P1.5u: the selection, insert, delete, clear, SELECT, full", g_p15u_edit},
        {"P1.5u: PRESETS' name ideas", g_p15u_ideas},
        {"P1.5u: SAVE NEW and REPLACE with a typed name; HOME cancels", g_p15u_save},
        {"P1.5u: SHIFT is the signs: its shortcuts off while naming", g_p15u_shift},
        {"P1.5u: the LEDs: all 27 lit, the pressed one bright", g_p15u_leds},
        {"P1.5v: DELETE?: armed, cancelled by any other key, deleted", g_p15v_delete},
        {"P1.5v: B10 toggles the pool from ALL and MINE; OUT?", g_p15v_pool},
        {"P1.5v: tags: SHIFT + B2..B8, the filters, the factory's", g_p15v_tags},
        {"P1.5v: NEW / REPLACE: F2 chooses, SAVE does it", g_p15v_save},
        {"P1.5v: a step + PRESETS: the picker up while held", g_p15v_picker},
        {"P1.5w: the pool's references: REPLACE, DELETE?'s use, undo", g_p15w_refs},
        {"P1.5x: presets carry their mix: LOAD, CANCEL, SAVE, a lock", g_p15x_mix},
        {"P1.5x: the chosen preset's tags on B2..B8", g_p15x_tag_leds},
        {"P1.5t: the browser's LEDs", g_p15t_leds},
        {"P1.5z: - POOL's animation; the outline lime in the pool", g_p15z_pool},
        {"P1.5z: SAVE PRESET: KNOB 1 turns the letter", g_p15z_save_knob1},
        {"P1.5z: SHIFT's scale / root takeover in every view", g_p15z_scale_popup},
        {"P1.5z: SHIFT's shortcuts in the sound editor, no map", g_p15z_editor_shift},
        {"P1.5z: SHIFT + B6: the TAP takeover", g_p15z_tap},
        {"P1.5z: SHIFT + B8: page follow; SHIFT's map", g_p15z_follow},
        {"P2.11: the Geek Out pages", g_p211_geek},
        {"P1.5z: SHIFT + SELECT: the track everywhere", g_p15z_shift_select},
        {"P1.6l: the controls' labels", g_p16l_labels},
        {"P1.3v: no text under 11 px on any screen", g_clarity},
        {"P2.6h: every screen again, every name at its longest (ASan: no buffer overrun)", g_long_names},
    };
    size_t i;
    uint64_t h1, h2;
    setvbuf(stdout, NULL, _IOLBF, 0);                   /* (a hung gesture: the lines before it seen) */
    if (!qd_init()) {
        printf("test_ctl: qd_init failed\n");
        return 1;
    }
    qd_text_stats_reset();                              /* P1.3v: the clarity guard over every gesture's frames */
    for (i = 0; i < sizeof G / sizeof G[0]; i++) {
        const int f0 = fails, c0 = checks;
        if (getenv("QD_ONLY") && !strstr(G[i].name, getenv("QD_ONLY")))   /* (QD_ONLY=text: those gestures alone) */
            continue;
        G[i].fn();
        printf("  gesture: %-55s %d checks%s\n", G[i].name, checks - c0, fails > f0 ? "  FAILED" : "");
    }
    if (getenv("QD_ONLY")) {
        printf("test_ctl: %d checks, %d failed (QD_ONLY)\n", checks, fails);
        return fails != 0;
    }
    t_beat();
    t_leds();
    h1 = session();
    h2 = session();
    CHECK(h1 == h2, "two runs of one session differ");
    printf("  determinism: a session twice, the same model and samples (%016llx)\n", (unsigned long long)h1);
    qd_shutdown();
#ifdef QD_DEVICE
    CHECK(qd_arena_fails() == 0, "P2.1b: the drawing's arena never short (it lends its room between frames: %u "
          "refused)", qd_arena_fails());
    CHECK(qd_arena_peak() + QD_GFX_MARGIN <= qd_arena_size(), "P2.1c: the arena's peak %lu with the margin %u within its %lu",
          (unsigned long)qd_arena_peak(), (unsigned)QD_GFX_MARGIN, (unsigned long)qd_arena_size());
#endif
#ifdef QD_DEVICE
    printf("  the arena: peak %lu of %lu bytes\n", (unsigned long)qd_arena_peak(), (unsigned long)qd_arena_size());
#endif
    printf("test_ctl: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
