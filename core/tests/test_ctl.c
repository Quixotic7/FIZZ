/* SPDX-License-Identifier: GPL-3.0-only */
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
 *   beat         "program a beat": from the empty A05, panel presses and turns only build a pattern with a page that
 *                has its own start and end, a two-page loop, a pool sound lock, a velocity override, a ratchet, a
 *                condition (moved by F2's cut / drop), a chord, a five-note chord (keys, then KNOB 4), a P-LOCK lock, a
 *                12-step track, two default changes and SEL's mute of track 8; it equals the same pattern built
 *                through the API byte for byte (steps, locks, track settings) and in its timeline
 *   leds         the levels per situation (§8b's LEDs)
 *   determinism  one gesture session twice: the same model, the same samples
 *
 *     test_ctl      exit 1 on a failed check */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "qd_gfx.h"
#include "chord.h"
#include "scale.h"
#include "sounds.h"
#include "ui.h"

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
    now = 0;
    frames_done = 0;
    hash = 14695981039346656037ull;
    qd_app_tick(&A, 0);
}

/* the panel (PLAN §8b's names) */
#define WK(w) QD_WHITE_KEY[w]
#define BK(b) QD_BLACK_KEY[b]
enum { B1, B2, B3, B4, B5, B6, B7, B8, B9, B10, B11 };
enum { M_NOTE, M_VEL, M_LEN, M_MICRO, M_RATCH, M_CHNC, M_COND, M_TRIG, M_SOUND, M_LOCKS };
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
static void turn(int knob, int d)
{
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
static void f3_up(void)
{
    bup(F2);
    bup(F1);
    run(10);
}
static void wake(void) { btap(QD_B_HOME); }             /* power-on: idle; HOME: the step view */
static void empty_pattern(int k)                        /* SEQ held + white key k + 1 (stopped: at once) */
{
    bdown(QD_B_MIX);
    wtap(k);
    bup(QD_B_MIX);
    run(10);
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
static qd_leds_t leds(void)
{
    qd_leds_t l;
    qd_app_leds(&A, &l);
    return l;
}
/* a fresh unit on the empty pattern A05, the step view, track 1 */
static void start(void)
{
    fresh();
    wake();
    empty_pattern(4);
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
    CHECK(R(0, 4).type == QD_TRIG_LOCK && LK(0, 4, QD_QP_FREQ) != -99999,
          "an empty step held + KNOB 1 (VEL): a LOCK step with a FREQ lock");
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
    for (m = 0; m < 10; m++) {
        mode(m);
        CHECK(A.ui.hmode == m && leds().key[BK(m)] == QD_LED_LIT, "nothing held: B%d = the hold mode %s", m + 1,
              UI_FIELD_NAME[m]);
    }
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
    /* LOCKS: B1..B10 choose, B11 removes, ALGORITHM moves the chosen */
    qd_lock_set(PAT(), 0, 0, QD_QP_FREQ, 5000);
    qd_lock_set(PAT(), 0, 0, QD_QP_HARM, 300);
    mode(M_LOCKS);
    whold(0);
    black(QD_QP_HARM < QD_QP_FREQ ? B1 : B2);           /* (a step's locks in id order) */
    CHECK(A.ui.lock_cur == (QD_QP_HARM < QD_QP_FREQ ? 0 : 1), "LOCKS B1 / B2: HARM's lock chosen");
    turn(QD_K_ALGO, -1);
    CHECK(LK(0, 0, QD_QP_HARM) == 200, "LOCKS ALGORITHM: the chosen lock's value (HARM %d)", LK(0, 0, QD_QP_HARM));
    black(B5);
    CHECK(A.ui.lock_cur == (QD_QP_HARM < QD_QP_FREQ ? 0 : 1), "LOCKS B5 with two locks: nothing");
    black(B11);
    CHECK(LK(0, 0, QD_QP_HARM) == -99999 && LK(0, 0, QD_QP_FREQ) == 5000, "LOCKS B11: the chosen lock removed");
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
    printf("  values: VEL / LEN / MICRO / CHNC %d of %d key rows, RATCH, COND, TRIG, LOCKS, B11, several held\n", ok,
           n);
}

/* ALGORITHM: nothing held the track's default (a takeover), a step held the value finely; a default change moves only
 * the steps that do not override it */
static void g_algo(void)
{
    start();
    wtap(0), wtap(1), wtap(2);
    mode(M_VEL);
    whold(1);
    black(B10);                                         /* step 2: 127, its own */
    wrel(1);
    turn(QD_K_ALGO, -20);
    CHECK(PAT()->trk[0].def.vel == 80 && A.ui.tk == QD_TK_DEFAULT, "nothing held: ALGORITHM = the track's VEL "
          "(80), the default's takeover");
    CHECK(R(0, 0).vel == 80 && R(0, 2).vel == 80 && R(0, 1).vel == 127, "the default moved steps 1 and 3, not "
          "step 2 (it overrides VEL)");
    whold(0);
    turn(QD_K_ALGO, 3);
    wrel(0);
    CHECK(R(0, 0).vel == 83 && OV(0, 0, QD_PM_VEL) && PAT()->trk[0].def.vel == 80, "a step held: ALGORITHM its "
          "value finely (83), overridden; the default stays");
    mode(M_LEN);
    turn(QD_K_ALGO, 2);
    CHECK(PAT()->trk[0].def.len == QD_LEN_1 + 2 && R(0, 2).len == QD_LEN_1 + 2, "LEN: the default 2 steps");
    mode(M_COND);
    turn(QD_K_ALGO, 1);
    CHECK(PAT()->trk[0].def.cond == QD_COND_FILL, "COND: the default FILL");
    mode(M_NOTE);
    turn(QD_K_ALGO, 2);
    CHECK(PAT()->trk[0].def.note[0] == 62 && R(0, 2).notes[0] == 62, "NOTE: the default up two steps (chromatic: D4)");
    mode(M_TRIG);
    turn(QD_K_ALGO, 1);
    CHECK(PAT()->trk[0].def.type == QD_TRIG_LOCK && R(0, 2).type == QD_TRIG_LOCK, "TRIG: the default LOCK");
    mode(M_SOUND);
    turn(QD_K_ALGO, 1);
    CHECK(A.model.t[0].sound == 15 && A.ui.tk == QD_TK_PRESET, "SOUND: the track's own sound from the pool (15)");
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
    mode(M_VEL);                                        /* (P1.3c: in NOTE SELECT is note 1) */
    whold(1);
    turn(QD_K_SELECT, 1);                               /* a step held: SELECT still pages; another W joins */
    kdown(WK(5));
    run(20);
    CHECK(A.ui.page[0] == 1 && A.ui.nheld == 2 && A.ui.held[1] == 21, "a step held: SELECT pages, a W there joins "
          "(step 22)");
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
    turn(QD_K_ALGO, 1);
    CHECK(R(0, 0).notes[0] == 58, "ALGORITHM: a scale step up (chromatic: A#3)");
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
    turn(QD_K_ALGO, 1);
    CHECK(R(0, 0).notes[0] == 55 && R(0, 0).notes[1] == 59 && R(0, 0).notes[2] == 64, "ALGORITHM: each a scale step "
          "up (G B E)");
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
    CHECK(A.model.t[0].sound == 16 && !A.ui.tk_lock, "nothing held: PRESETS = the track's sound (16)");
}

static void g_plocks(void)
{
    start();
    wtap(5);
    mode(M_VEL);
    whold(5);
    turn(QD_K_3, 3);
    turn(QD_K_4, -1);
    turn(QD_K_1, -10);
    turn(QD_K_2, 2);
    wrel(5);
    CHECK(LK(0, 5, QD_QP_HARM) == 300 && LK(0, 5, QD_QP_FDBK) == 0 && LK(0, 5, QD_QP_FREQ) == 12700 - 1000 &&
              LK(0, 5, QD_QP_EDEC) == A.model.t[0].patch[QD_QP_EDEC] + 2 && A.model.t[0].patch[QD_QP_HARM] == 0,
          "a step held: KNOB 1..4 = FREQ, DECAY, HARM, FDBK p-locks; the track untouched");
    CHECK(A.ui.tk == UI_TK_DECAY && A.ui.tk_lock && A.ui.tk_step == 5, "the takeover marked as a lock on step 6");
    turn(QD_K_1, -10);
    turn(QD_K_3, 4);
    CHECK(A.model.t[0].patch[QD_QP_FREQ] == 12700 - 1000 && A.model.t[0].patch[QD_QP_HARM] == 400 &&
              A.ui.tk == UI_TK_HARM && !A.ui.tk_lock,
          "nothing held: KNOB 1..4 the track's sound, live");
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
    ktap(BK(B4), 30);                                   /* two taps 210 ms apart: two taps */
    run(180);
    ktap(BK(B4), 30);
    run(10);
    CHECK(PAT()->trk[0].loop == 2 && A.ui.page[0] == 3, "B4 twice 210 ms apart: just edit page 4");
    run(300);
    ktap(BK(B5), 30);                                   /* a tap then a hold: the tap edits, the hold toggles */
    run(100);
    ktap(BK(B5), 420);
    run(10);
    CHECK(PAT()->trk[0].loop == 0x12 && A.ui.page[0] == 4, "B5 tapped then held: edited, then into the loop (no "
          "double tap)");
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
    bdown(F1);
    run(70);
    kdown(BK(B9));
    run(610);
    CHECK(!R(0, 0).on && !R(0, 18).on, "F1 + B9 held 600 ms: the track cleared (at the mark, still down)");
    kup(BK(B9));
    bup(F1);
    run(10);
    CHECK(!R(0, 0).on && !R(0, 18).on, "its release: nothing more");
    bdown(F1);
    black(B11);
    bup(F1);
    CHECK(R(0, 0).on && R(0, 18).on && R(0, 5).on, "F1 + B11: the track back");
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
    run(10);
    l = leds();
    CHECK(l.key[BK(B3)] == QD_LED_LIT && l.key[BK(B1)] == QD_LED_DIM && l.key[WK(6)] == QD_LED_OFF &&
              l.key[WK(0)] == QD_LED_OFF && l.key[BK(B11)] == QD_LED_LIT,
          "LEDs: the selected track lit, the others dim; W7 muted off, W1 off (track 5 soloed); B11 lit");
    for (k = 0; k < 500; k++) {
        run(1);
        on += leds().key[WK(4)] == QD_LED_LIT;
    }
    CHECK(on > 150 && on < 350, "the soloed track's W blinks (%d of 500 ms)", on);
    black(B11);
    l = leds();
    CHECK(!A.seq.mute && !A.seq.solo && l.key[BK(B11)] == QD_LED_OFF && l.key[WK(6)] == QD_LED_LIT &&
              l.key[WK(0)] == QD_LED_LIT,
          "B11: every mute and solo off; all heard, lit; B11 off");
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
    CHECK(A.bpm == bpm + 5 && A.ui.tk == UI_TK_TEMPO, "F3 + SELECT: tempo");
    bdown(SHIFT);
    turn(QD_K_SELECT, 6);
    bup(SHIFT);
    CHECK(PAT()->swing == 56 && A.ui.tk == UI_TK_SWING, "SHIFT + F3 + SELECT: swing");
    f3_up();
    bdown(F1);                                          /* the F3 chord: F1 first, F2 after: F3 until both are up */
    run(30);
    bdown(F2);
    run(10);
    bup(F1);
    run(10);
    CHECK(qd_ctl_layer(&A) == QD_LAYER_F3, "F1 let go first: still F3 until both are up");
    bup(F2);
    run(10);
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
    CHECK(ST(0, 0)->note[1] == 4 && R(0, 0).nn == 2 && R(0, 0).notes[1] == 66 && sounding(66),
          "KNOB 1 +4: note 2 = +4 (F#4), sounding");
    turn(QD_K_2, 7);
    turn(QD_K_3, 7);
    CHECK(ST(0, 0)->note[2] == 8 && ST(0, 0)->note[3] == 9, "KNOB 2 +7: +8 (note 2's +4 skipped); KNOB 3 +7: +9 "
          "(+4 and +8 skipped)");
    turn(QD_K_4, 12);
    CHECK(R(0, 0).nn == 5 && ST(0, 0)->note[4] == 15 && R(0, 0).notes[4] == 77, "KNOB 4 +12: +15, five notes");
    turn(QD_K_SELECT, -2);
    CHECK(R(0, 0).notes[0] == 60 && R(0, 0).notes[1] == 64 && R(0, 0).notes[4] == 75, "SELECT -2: the shape moves "
          "with note 1 (C E G# A D#)");
    turn(QD_K_1, -4);
    CHECK(ST(0, 0)->note[1] == 0 && R(0, 0).nn == 4, "KNOB 1 back below +1: none (four notes)");
    turn(QD_K_4, 60);
    CHECK(ST(0, 0)->note[4] == 48, "KNOB 4 stops at +48");
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
    CHECK(ST(0, 4)->note[1] == 4, "KNOB 1 +2 in the scale above F: G (+2), A (+4)");
    turn(QD_K_2, 3);
    CHECK(ST(0, 4)->note[2] == 7, "KNOB 2 +3: G, B, C (A, note 2's, skipped): +7");
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
    CHECK(R(0, 0).nn == 5 && R(0, 0).notes[4] == 68, "a sixth key: ignored (five at most)");
    kup(BK(B1)), kup(BK(B3)), kup(BK(B5)), kup(BK(B7)), kup(BK(B9)), kup(BK(B10));
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
    CHECK(LK(0, 5, QD_QP_EDEC) == qd_sound_patch(&A.proj, 2)[QD_QP_EDEC] + 2 && qd_view_screen(&A) == QD_SCR_HELD,
          "in VEL KNOB 2 still p-locks (the same table), on VEL's own screen");
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
static void g_rec(void)
{
    int micro;
    start();
    btap(QD_B_REC);
    CHECK(A.ui.mode == QD_MODE_KEYS && leds().button[QD_B_REC] == QD_LED_LIT, "REC: the keyboard, REC lit");
    btap(QD_B_REC);
    CHECK(A.ui.mode == QD_MODE_STEP && leds().button[QD_B_REC] == QD_LED_OFF, "REC again: the sequencer");
    btap(QD_B_REC);
    bdown(SHIFT);
    btap(QD_B_REC);
    bup(SHIFT);
    CHECK(A.ui.mode == QD_MODE_KEYS && A.ui.rec && !A.seq.playing, "SHIFT + REC in the keyboard, stopped: step "
          "recording");
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
    bdown(SHIFT);
    btap(QD_B_REC);
    bup(SHIFT);
    CHECK(!A.ui.rec && A.ui.mode == QD_MODE_KEYS, "SHIFT + REC: disarmed, still the keyboard");
    btap(QD_B_REC);
    CHECK(A.ui.mode == QD_MODE_STEP, "REC: back to the sequencer");
    /* REC + PLAY: playing, armed, the keyboard: live recording */
    bdown(QD_B_REC);
    btap(QD_B_PLAY);
    bup(QD_B_REC);
    run(10);
    CHECK(A.seq.playing && A.ui.rec && A.ui.mode == QD_MODE_KEYS, "REC + PLAY: playing, recording armed, the "
          "keyboard");
    while (qd_seq_track_pos(&A.seq, 0) < 4.0 + 3.0 / 24.0)   /* step 5 + 3/24 */
        run(1);
    kdown(16);                                          /* A4 (69), held two steps */
    run(250);
    kup(16);
    run(5);
    micro = R(0, 4).micro;
    CHECK(R(0, 4).on && R(0, 4).notes[0] == 69 && micro >= 2 && micro <= 4 && R(0, 4).len == 6,
          "live: step 5, micro %d (+3 played), length %s (2 held)", micro, qd_len_name(R(0, 4).len));
    while (qd_seq_track_pos(&A.seq, 0) < 14.5)
        run(1);
    turn(QD_K_3, 2);
    CHECK(LK(0, 14, QD_QP_HARM) == 200 && R(0, 14).type == QD_TRIG_LOCK, "a knob turned while recording: a lock on "
          "the step playing (step 15)");
    btap(QD_B_PLAY);
    run(10);
    CHECK(A.ui.rec && !A.seq.playing, "stopped: still armed (step recording now)");
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
    bdown(QD_B_KEYS);
    run(10);
    CHECK(A.seq.fill && leds().button[QD_B_KEYS] == QD_LED_LIT, "ARP held: FILL, ARP lit");
    bup(QD_B_KEYS);
    run(10);
    CHECK(!A.seq.fill, "ARP up: FILL off");
    CHECK(A.ui.follow && leds().button[QD_B_PAGE] == QD_LED_LIT, "follow on at power-on, PAGE lit");
    btap(QD_B_PAGE);
    CHECK(!A.ui.follow && leds().button[QD_B_PAGE] == QD_LED_OFF, "PAGE tap: follow off");
    bdown(QD_B_PAGE);
    wtap(5);
    bup(QD_B_PAGE);
    run(5);
    CHECK(A.ui.page[0] == 5 && !A.ui.follow && !R(0, 5).on, "PAGE held + W6: page 6 (no step toggled, follow kept)");
    btap(QD_B_PAGE);
    CHECK(A.ui.follow, "PAGE tap: follow on");
}

static void g_follow(void)
{
    start();
    btap(QD_B_TRACKS);
    turn(QD_K_1, 16);                                   /* 32 steps: pages 1 and 2 */
    btap(QD_B_TRACKS);
    CHECK(PAT()->trk[0].loop == 3 && qd_track_len(&PAT()->trk[0]) == 32, "TRACKS, KNOB 1 +16: 32 steps (pages 1, 2)");
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
    btap(QD_B_PAGE);
    run(2500);
    CHECK(A.ui.page[0] == 1, "follow off: the page stays");
    btap(QD_B_PLAY);
}

static void g_tracks_screen(void)
{
    start();
    btap(QD_B_TRACKS);
    turn(QD_K_1, 8);
    turn(QD_K_2, -3);
    turn(QD_K_3, -3);
    turn(QD_K_4, 1);
    CHECK(qd_track_len(&PAT()->trk[0]) == 24 && PAT()->trk[0].pend[1] == 7 &&
              qd_tset_rate(&PAT()->trk[0]) == qd_ctl_rate_at(qd_ctl_rate_index(QD_RATE_1) - 3) &&
              PAT()->trk[0].limit == 3 && PAT()->trk[0].swing == 1,
          "tracks screen: LEN 24 (page 2 1..8), RATE three slower (%s), LIMIT 3, SWING on",
          qd_rate_name(qd_tset_rate(&PAT()->trk[0])));
    wtap(9);
    CHECK(A.seq.mute == 2, "tracks screen: W10 mutes track 2");
    wtap(2);
    CHECK(A.sel == 2, "W3 selects track 3");
    btap(QD_B_TRACKS);
    CHECK(A.ui.mode == QD_MODE_STEP, "TRACKS again: the step view");
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
    mode(M_VEL);                                        /* the track's velocity: 80 (a default change) */
    turn(QD_K_ALGO, -20);
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
    turn(QD_K_ALGO, -24);                               /* the track's note: C2 (two octaves down) */
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
    mode(M_NOTE);                                       /* P1.3c: step 9 a five-note chord by the keys (C D E G A), */
    whold(8);                                           /* then KNOB 4 moves note 5 up a semitone (A#) */
    kdown(BK(B1)), kdown(BK(B3)), kdown(BK(B5)), kdown(BK(B8)), kdown(BK(B10));
    run(30);
    kup(BK(B1)), kup(BK(B3)), kup(BK(B5)), kup(BK(B8)), kup(BK(B10));
    run(10);
    turn(QD_K_4, 1);
    wrel(8);
    bdown(QD_B_PATTERN);                                /* P1.3c: SEL + W8 tapped: track 8 muted */
    wtap(7);
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
    {
        static const uint8_t FIVE[5] = {60, 62, 64, 67, 69};
        qd_step_set_notes(p, 2, 8, FIVE, 5);
        qd_step_set_offset(p, 2, 8, 4, 10);
    }
    A.seq.mute = 0x80;
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
    CHECK(panel_mute == 0x80 && A.seq.mute == panel_mute, "the beat: SEL's mute of track 8 (%#x)", panel_mute);
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
        CHECK(panel.step[0][4].sound == QD_SND_SNARE && panel.step[2][0].note[2] == 7 &&
                  panel.step[0][15].cond == qd_cond_ab(1, 2) && !(panel.step[0][14].flags & QD_SF_ON),
              "a pool sound lock, a chord, a condition (moved by F2 from step 15 to 16)");
        {
            int five = 0;
            for (i = 0; i < nev[0]; i++)
                five += evs[0][i].kind == QD_LOG_ON && evs[0][i].track == 2 && evs[0][i].note == 70;
            CHECK(panel.step[2][8].note[4] == 10 && five >= 1 && LK(0, 0, QD_QP_HARM) != -99999,
                  "the five-note chord (note 5 on KNOB 4: A#4, heard %d times), the P-LOCK HARM lock", five);
        }
        printf("  beat: by panel presses only, a pattern with page 2's own 3..12, a two-page loop, a pool sound "
               "lock, a velocity override, a ratchet, a condition moved by F2, a chord and a five-note chord, a P-LOCK lock, a "
               "12-step track, two default changes and SEL's mute; "
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
    CHECK(l.key[BK(B10)] == QD_LED_LIT && l.key[BK(B8)] == QD_LED_FAINT && l.key[BK(B11)] == QD_LED_LIT,
          "a step held (VEL 127): B10 lit, B11 lit (it overrides VEL)");
    black(B11);
    l = leds();
    CHECK(l.key[BK(B11)] == QD_LED_OFF && l.key[BK(B10)] == QD_LED_FAINT && l.key[BK(B8)] == QD_LED_FAINT,
          "B11: back to the track's 100 (on no key): B11 off, the keys faint");
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
    CHECK(l.button[QD_B_REC] == QD_LED_LIT && l.key[7] == QD_LED_FAINT && l.key[8] == QD_LED_OFF,
          "the keyboard: REC lit, the C keys faint");
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
        {"NOTE, a step held: the five knobs", g_note_knobs},
        {"five notes by the keys", g_five_keys},
        {"P-LOCK: the screen stays, knobs lock, B11, undo", g_plock},
        {"the OCT LEDs' blink levels; the octave takeover", g_octave},
        {"REC, REC + PLAY, SHIFT + REC, step / live rec", g_rec},
        {"the keyboard in a scale, the global key", g_keyboard_scale},
        {"ARP = FILL; PAGE: follow, + W", g_fill_page},
        {"page follow", g_follow},
        {"the tracks screen", g_tracks_screen},
        {"MIDI notes into a held step", g_midi_held},
        {"the layer maps, SHIFT's map", g_maps},
    };
    size_t i;
    uint64_t h1, h2;
    if (!qd_init()) {
        printf("test_ctl: qd_init failed\n");
        return 1;
    }
    for (i = 0; i < sizeof G / sizeof G[0]; i++) {
        const int f0 = fails, c0 = checks;
        G[i].fn();
        printf("  gesture: %-55s %d checks%s\n", G[i].name, checks - c0, fails > f0 ? "  FAILED" : "");
    }
    t_beat();
    t_leds();
    h1 = session();
    h2 = session();
    CHECK(h1 == h2, "two runs of one session differ");
    printf("  determinism: a session twice, the same model and samples (%016llx)\n", (unsigned long long)h1);
    qd_shutdown();
    printf("test_ctl: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
