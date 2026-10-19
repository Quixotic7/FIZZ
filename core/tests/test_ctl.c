/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.3b's acceptance (PLAN §8b, the sequencer controls version 2): the controller driven as the glue drives it
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
 *   beat         "program a beat": from the empty A05, panel presses and turns only build a pattern with a page that
 *                has its own start and end, a two-page loop, a pool sound lock, a velocity override, a ratchet, a
 *                condition, a chord, a 12-step track and two default changes; it equals the same pattern built
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
static void empty_pattern(int k)                        /* PATTERN held + white key k + 1 (stopped: at once) */
{
    bdown(QD_B_PATTERN);
    wtap(k);
    bup(QD_B_PATTERN);
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
    kdown(WK(4));                                       /* a knob turned at 100 ms: held at once */
    run(100);
    turn(QD_K_1, 2);
    CHECK(A.ui.nheld == 1, "a knob turn makes a pending key held");
    kup(WK(4));
    run(5);
    CHECK(R(0, 4).type == QD_TRIG_LOCK && LK(0, 4, QD_QP_FREQ) != -99999,
          "an empty step held + KNOB 1: a LOCK step with a FREQ lock");
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
    CHECK(A.ui.hmode == M_LOCKS && !R(0, 0).on, "B11 with nothing held: nothing (question 37)");
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

/* F1: pages (tap, double tap, A..B), copy / cut, clear page / track, undo */
static void g_f1_pages(void)
{
    start();
    bdown(F1);
    run(100);
    CHECK(qd_ctl_layer(&A) == QD_LAYER_F1 && qd_ctl_layer_shown(&A) == QD_LAYER_F1, "F1 held: its layer and map");
    black(B3);
    CHECK(A.ui.page[0] == 2 && qd_track_loop(&PAT()->trk[0]) == 1, "F1 + tap B3: edit page 3, the loop unchanged");
    black(B2);
    run(50);
    black(B2);
    CHECK(A.ui.page[0] == 1 && PAT()->trk[0].loop == 2, "F1 + double tap B2: page 2 looped alone");
    run(300);
    black(B4);
    CHECK(A.ui.page[0] == 3 && PAT()->trk[0].loop == 2, "a tap after the double tap's time: just edit page 4");
    kdown(BK(B2));
    run(400);
    kdown(BK(B5));
    run(30);
    kup(BK(B5));
    run(30);
    kup(BK(B2));
    run(10);
    CHECK(PAT()->trk[0].loop == 0x1E && A.ui.page[0] == 1, "F1 + hold B2 + press B5: the loop 2..5, edit page 2");
    kdown(BK(B6));
    run(400);
    kup(BK(B6));
    run(10);
    CHECK(A.ui.page[0] == 1, "a page key held alone and let go: nothing (Form's)");
    bup(F1);
    run(10);
    CHECK(qd_ctl_layer(&A) == QD_LAYER_NONE, "F1 up: no layer");
    btap(F1);
    CHECK(!R(0, 16).on, "F1 tapped alone: nothing");
}

static void g_f1_edit(void)
{
    start();
    wtap(0);
    mode(M_VEL);
    whold(0);
    black(B4);                                          /* step 1: vel 51 */
    wrel(0);
    qd_lock_set(PAT(), 0, 0, QD_QP_FREQ, 4000);
    bdown(F1);
    wtap(0);                                            /* copy step 1 */
    bup(F1);
    CHECK(A.ui.clip.st.valid && A.ui.cf_on, "F1 + W1: the step copied");
    bdown(F2);
    wtap(5);
    wtap(7);
    bup(F2);
    CHECK(R(0, 5).vel == 51 && R(0, 7).vel == 51 && LK(0, 7, QD_QP_FREQ) == 4000, "F2 + W6, W8: pasted twice, with "
          "its lock");
    bdown(F2);                                          /* another track: allowed */
    black(B2);
    bup(F2);
    bdown(F2);
    run(70);
    wtap(3);
    bup(F2);
    CHECK(A.sel == 1 && R(1, 3).vel == 51 && LK(1, 3, QD_QP_FREQ) == 4000, "pasted onto track 2");
    bdown(SHIFT);
    bdown(F1);
    wtap(3);                                            /* SHIFT + F1 + W4: cut */
    bup(F1);
    bup(SHIFT);
    CHECK(!R(1, 3).on && A.ui.clip.st.valid, "SHIFT + F1 + W4: cut (cleared, copied)");
    bdown(F1);
    black(B11);
    bup(F1);
    CHECK(R(1, 3).on && R(1, 3).vel == 51, "F1 + B11: undo the cut");
    bdown(F1);
    black(B11);
    bup(F1);
    CHECK(!R(1, 3).on, "F1 + B11 again: redo");
    /* clear page / track, and their undo */
    bdown(F2);
    black(B1);
    bup(F2);
    run(10);
    CHECK(A.sel == 0, "F2 + B1: track 1");
    bdown(F1);
    black(B9);
    bup(F1);
    CHECK(!R(0, 0).on && !R(0, 5).on && PAT()->lock_n[0] == 0, "F1 + B9: the page cleared");
    bdown(F1);
    black(B11);
    bup(F1);
    CHECK(R(0, 0).on && R(0, 7).on && LK(0, 7, QD_QP_FREQ) == 4000, "F1 + B11: undone");
    wtap(15);
    turn(QD_K_SELECT, 1);
    wtap(0);
    bdown(F1);
    black(B10);
    bup(F1);
    CHECK(!R(0, 15).on && !R(0, 16).on, "F1 + B10: the track cleared");
    bdown(F1);
    black(B11);
    bup(F1);
    CHECK(R(0, 15).on && R(0, 16).on && R(0, 0).on, "F1 + B11: the track back");
    bdown(F1);
    black(B11);
    black(B11);
    bup(F1);
    CHECK(R(0, 16).on, "undo, redo: as it was");
}

/* F2: tracks, mute, solo; page copy / paste */
static void g_f2(void)
{
    qd_leds_t l;
    start();
    bdown(F2);
    run(100);
    black(B3);
    CHECK(A.sel == 2, "F2 + B3: track 3");
    kdown(BK(B2));
    run(20);
    wtap(0);
    CHECK(A.sel == 1 && (A.seq.mute & 2), "F2 + hold B2 + W1: track 2 muted");
    wtap(1);
    CHECK(A.seq.solo == 2, "+ W2: soloed");
    kup(BK(B2));
    run(10);
    l = leds();
    CHECK(l.key[BK(B2)] == QD_LED_LIT || l.key[BK(B2)] == QD_LED_OFF, "F2: the soloed track blinks");
    CHECK(l.key[BK(B1)] == QD_LED_OFF, "F2: with a solo, track 1 is not heard: off");
    kdown(BK(B2));
    run(20);
    wtap(0);
    wtap(1);
    kup(BK(B2));
    bup(F2);
    run(10);
    CHECK(!A.seq.mute && !A.seq.solo, "W1, W2 again: unmuted, unsoloed");
    /* page copy / paste */
    bdown(F2);
    black(B1);
    bup(F2);
    wtap(2), wtap(6);
    bdown(F1);
    black(B1);
    bup(F1);
    f3_down();
    kdown(WK(1)), kdown(WK(9));                         /* page 1: 2..10 */
    run(20);
    kup(WK(1)), kup(WK(9));
    f3_up();
    bdown(F2);
    black(B9);                                          /* copy page 1 */
    bup(F2);
    bdown(F1);
    black(B4);                                          /* edit page 4 */
    bup(F1);
    bdown(F2);
    black(B10);                                         /* paste */
    bup(F2);
    CHECK(R(0, 50).on && R(0, 54).on && PAT()->trk[0].pstart[3] == 1 && PAT()->trk[0].pend[3] == 9 &&
              (PAT()->trk[0].loop & 8),
          "F2 + B9 on page 1, F2 + B10 on page 4: its steps, its 2..10, and page 4 joins the loop");
    bdown(F1);
    black(B11);
    bup(F1);
    CHECK(!R(0, 50).on && !(PAT()->trk[0].loop & 8), "F1 + B11: the paste undone (the loop too)");
}

/* F3: the rate, the page's end, start..end; tempo and swing */
static void g_f3(void)
{
    int bpm;
    start();
    f3_down();
    CHECK(qd_ctl_layer(&A) == QD_LAYER_F3, "both: F3");
    black(B1);
    CHECK(PAT()->trk[0].rate == QD_RATE_1_8, "F3 + B1: 1/8x");
    black(B4);
    CHECK(PAT()->trk[0].rate == QD_RATE_2_3, "F3 + B4: 2/3x");
    black(B9);
    CHECK(PAT()->trk[0].rate == QD_RATE_2, "F3 + B9: 2x");
    black(B6);
    wtap(11);
    CHECK(PAT()->trk[0].pend[0] == 11 && PAT()->trk[0].pstart[0] == 0 && qd_track_len(&PAT()->trk[0]) == 12,
          "F3 + W12: page 1 ends at 12");
    kdown(WK(2));
    run(30);
    kdown(WK(9));
    run(30);
    kup(WK(9)), kup(WK(2));
    run(10);
    CHECK(PAT()->trk[0].pstart[0] == 2 && PAT()->trk[0].pend[0] == 9, "F3 + hold W3, press W10: 3..10");
    bpm = A.bpm;
    turn(QD_K_SELECT, 5);
    CHECK(A.bpm == bpm + 5 && A.ui.tk == UI_TK_TEMPO, "F3 + SELECT: tempo");
    bdown(SHIFT);
    turn(QD_K_SELECT, 6);
    bup(SHIFT);
    CHECK(PAT()->swing == 56 && A.ui.tk == UI_TK_SWING, "SHIFT + F3 + SELECT: swing");
    f3_up();
    turn(QD_K_SELECT, 2);                               /* page 3 */
    f3_down();
    wtap(7);
    f3_up();
    CHECK(PAT()->trk[0].loop == 5 && PAT()->trk[0].pend[2] == 7 && qd_track_len(&PAT()->trk[0]) == 16,
          "F3 + W8 on page 3: its end 8, it joins the loop (pages 1 and 3: 8 + 8 steps)");
    bdown(F1);                                          /* the F3 chord: F1 first, F2 after: F3 until both up */
    run(30);
    bdown(F2);
    run(10);
    bup(F1);
    run(10);
    CHECK(qd_ctl_layer(&A) == QD_LAYER_F3, "F1 let go first: still F3 until both are up");
    bup(F2);
    run(10);
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
    CHECK(qd_track_len(&PAT()->trk[0]) == 24 && PAT()->trk[0].pend[1] == 7 && PAT()->trk[0].rate == QD_RATE_1_2 &&
              PAT()->trk[0].limit == 3 && PAT()->trk[0].swing == 1,
          "tracks screen: LEN 24 (page 2 1..8), RATE 1/2x, LIMIT 3, SWING on");
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
    bdown(F2);
    black(B2);
    bup(F2);
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
    bdown(F2);
    black(B3);
    bup(F2);
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
    qd_step_on(p, 0, 14);
    qd_step_set_sound(p, 0, 4, QD_SND_SNARE);
    qd_step_set_sound(p, 0, 12, QD_SND_SNARE);
    qd_step_set_vel(p, 0, 8, 127);
    qd_step_set_cond(p, 0, 14, qd_cond_ab(1, 2));
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
}

static void t_beat(void)
{
    static qd_pattern_t panel;
    int i, same = 1, t, s;
    beat_by_panel();
    panel = *PAT();
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
                  panel.step[0][14].cond == qd_cond_ab(1, 2),
              "a pool sound lock, a chord, a condition");
        printf("  beat: by panel presses only, a pattern with page 2's own 3..12, a two-page loop, a pool sound "
               "lock, a velocity override, a ratchet, a condition, a chord, a 12-step track and two default changes; "
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
              l.key[BK(B11)] == QD_LED_OFF,
          "black: the mode lit, a mode some step overrides dim, the others faint, B11 off");
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
    CHECK(l.key[BK(B1)] == QD_LED_LIT && l.key[BK(B2)] == QD_LED_DIM, "F2: the selected track lit, the others heard "
          "dim");
    bup(F2);
    f3_down();
    l = leds();
    CHECK(l.key[WK(0)] == QD_LED_LIT && l.key[WK(11)] == QD_LED_LIT && l.key[WK(12)] == QD_LED_OFF &&
              l.key[BK(5)] == QD_LED_LIT && l.key[BK(0)] == QD_LED_FAINT,
          "F3: the page's 1..12 lit, the rest off; the rate 1x (B6) lit");
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
        {"B1..B10: the hold mode; B11 nothing", g_modes},
        {"every mode's ten values, B11, several held", g_values},
        {"ALGORITHM: defaults (nothing held), finely (held)", g_algo},
        {"SELECT: pages", g_select},
        {"NOTE entry: replace / add / octave / scale", g_notes},
        {"SOUND's banks; PRESETS = the sound lock", g_sound},
        {"KNOB 1..4: p-locks held, the sound not", g_plocks},
        {"F1: pages tap / double tap / A..B", g_f1_pages},
        {"F1 copy, F2 paste, cut, clear, undo / redo", g_f1_edit},
        {"F2: tracks, mute, solo; page copy / paste", g_f2},
        {"F3: rate, end, start..end, tempo, swing", g_f3},
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
