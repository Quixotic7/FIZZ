/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.3's acceptance: the controller driven as the glue drives it (core/app/app.h: input events between millisecond
 * ticks, the audio in 128-frame blocks, a frame every 15 ms, so every screen is drawn under the sanitizers too).
 *
 *   gestures     a table of scenarios, each a scripted sequence of presses, holds and turns at given milliseconds,
 *                each ending in assertions on the model: tap on / off, the LED at the press, hold vs tap at the
 *                250 ms edge, several held, the latch, FUNC + tap (LOCK steps), an empty step held + a knob (a LOCK
 *                step with that lock), CLEAR and its undo, pages, page follow, the tracks screen's knobs, the step
 *                view's knobs (the quick page live, tempo, swing), KEYS step recording (chords, rests), live recording
 *                (micro-timing kept, quantized), MIDI notes into a held step, p-locks of the quick page with a step
 *                held, the "last entered" defaults, the idle screen
 *   fields       a table: every field's SELECT and KNOB 1..4 change exactly what they should (and nothing else)
 *   beat         "program a beat": from the empty A05, panel presses and turns only build a 2-page drum + bass
 *                pattern with a sound lock, a p-lock, a ratchet, a condition and a chord; its timeline (the player's
 *                events: tick, kind, track, note, velocity, the voice's patch) equals the one of the same pattern
 *                built through the API
 *   leds         the levels per situation (PLAN §8's LED table)
 *   determinism  one gesture session twice: the same model, the same samples
 *
 *     test_ctl      exit 1 on a failed check */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "qd_gfx.h"
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

/* the panel */
#define WK(w) QD_WHITE_KEY[w]
#define BK(b) QD_BLACK_KEY[b]
enum { T1, T2, T3, T4, T5, T6, T7, T8, PGDN, PGUP };   /* the black keys with nothing held */
enum { F_NOTE, F_VEL, F_LEN, F_MICRO, F_RATCH, F_COND, F_CHNC, F_TRIG, F_SOUND, F_LOCKS, F_CLEAR };   /* held */
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
static void black(int b)
{
    kdown(BK(b));
    run(20);
    kup(BK(b));
    run(10);
}
static void turn(int knob, int d)
{
    qd_app_knob(&A, knob, d);
    run(5);
}
static void wake(void)                                  /* power-on: idle; HOME: the step view */
{
    btap(QD_B_HOME);
}
static void empty_pattern(int k)                        /* PATTERN held + white key k + 1 (stopped: at once) */
{
    bdown(QD_B_PATTERN);
    wtap(k);
    bup(QD_B_PATTERN);
    run(10);
}
static qd_pattern_t *PAT(void) { return qd_ctl_pat(&A); }
static const qd_step_t *ST(int t, int s) { return qd_step(PAT(), t, s); }
static int TY(int t, int s) { return qd_step_type(ST(t, s)); }
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
    int ty0;
    fresh();
    ty0 = TY(0, 0);
    CHECK(A.ui.mode == QD_MODE_IDLE, "power-on: the idle screen");
    wtap(0);
    CHECK(A.ui.mode == QD_MODE_STEP && TY(0, 0) == ty0, "a key wakes to the step view and does nothing else");
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
    CHECK(l.key[WK(1)] == QD_LED_LIT && TY(0, 1) == QD_TRIG_NONE, "the LED at the press, the step on at the release");
    run(30);
    kup(WK(1));
    run(5);
    CHECK(TY(0, 1) == QD_TRIG_NOTE && ST(0, 1)->note[0] == 60 && ST(0, 1)->vel == 100 && ST(0, 1)->len == QD_LEN_1,
          "tap: a NOTE with the defaults (note %d vel %d len %d)", ST(0, 1)->note[0], ST(0, 1)->vel, ST(0, 1)->len);
    qd_lock_set(PAT(), 0, 1, QD_QP_FREQ, 5000);
    wtap(1);
    CHECK(TY(0, 1) == QD_TRIG_NONE && qd_lock_count(PAT(), 0, 1) == 0, "tap again: empty, its locks gone");
}

static void g_edge(void)
{
    start();
    kdown(WK(2));
    run(249);
    kup(WK(2));
    run(5);
    CHECK(TY(0, 2) == QD_TRIG_NOTE && A.ui.nheld == 0, "released at 249 ms: a tap");
    kdown(WK(3));
    run(250);
    CHECK(A.ui.nheld == 1 && A.ui.held[0] == 3, "250 ms: held");
    kup(WK(3));
    run(5);
    CHECK(TY(0, 3) == QD_TRIG_NONE && A.ui.nheld == 0, "released after the hold: no toggle, let go");
    kdown(WK(4));                                       /* a knob turned at 100 ms: held at once */
    run(100);
    turn(QD_K_1, 2);
    CHECK(A.ui.nheld == 1, "a knob turn makes a pending key held");
    kup(WK(4));
    run(5);
    CHECK(TY(0, 4) == QD_TRIG_LOCK, "an empty step held + KNOB 1: no toggle at the release (a LOCK step)");
}

static void g_several(void)
{
    start();
    wtap(0), wtap(2), wtap(4);
    kdown(WK(2));
    kdown(WK(0));
    kdown(WK(4));
    run(300);
    CHECK(A.ui.nheld == 3 && A.ui.held[0] == 2, "three held, the first held shown (%d, step %d)", A.ui.nheld,
          A.ui.held[0] + 1);
    black(F_VEL);
    turn(QD_K_1, -20);
    CHECK(ST(0, 0)->vel == 80 && ST(0, 2)->vel == 80 && ST(0, 4)->vel == 80, "VEL -20 on all three");
    kup(WK(0)), kup(WK(2)), kup(WK(4));
    run(10);
    CHECK(A.ui.nheld == 0, "all let go");
}

static void g_latch(void)
{
    start();
    wtap(0);
    whold(0);
    black(F_VEL);
    btap(QD_B_OCTUP);
    wrel(0);
    CHECK(A.ui.latch && A.ui.nheld == 1, "steps held + OCT+ (VEL): latched, it stays after the release");
    turn(QD_K_1, 5);
    CHECK(ST(0, 0)->vel == 105, "both hands free: KNOB 1 edits the latched step");
    wtap(5);
    CHECK(A.ui.nheld == 2 && A.ui.held[1] == 5 && TY(0, 5) == QD_TRIG_NONE, "a white tap adds a step to the latch");
    wtap(5);
    CHECK(A.ui.nheld == 1, "and again removes it");
    btap(QD_B_OCTDN);
    CHECK(!A.ui.latch && A.ui.nheld == 0, "OCT- lets go");
    whold(0);                                           /* NOTE: OCT+ is the octave; FUNC + OCT+ latches */
    black(F_NOTE);
    btap(QD_B_OCTUP);
    CHECK(ST(0, 0)->note[0] == 72 && !A.ui.latch, "NOTE field: OCT+ moves the notes an octave (%d)", ST(0, 0)->note[0]);
    bdown(QD_B_FUNC);
    btap(QD_B_OCTUP);
    bup(QD_B_FUNC);
    wrel(0);
    CHECK(A.ui.latch && A.ui.nheld == 1 && !A.ui.func_latch, "NOTE field: FUNC + OCT+ latches (FUNC used as a shift)");
    btap(QD_B_HOME);
    CHECK(!A.ui.latch && A.ui.nheld == 0, "HOME lets go");
}

static void g_func_lock(void)
{
    start();
    bdown(QD_B_FUNC);
    wtap(6);
    CHECK(TY(0, 6) == QD_TRIG_LOCK, "FUNC + tap: a LOCK step");
    wtap(6);
    bup(QD_B_FUNC);
    run(5);
    CHECK(TY(0, 6) == QD_TRIG_NONE && !A.ui.func_latch, "FUNC + tap again: empty (FUNC was a shift: not latched)");
    wtap(7);
    btap(QD_B_FUNC);
    CHECK(A.ui.func_latch && leds().button[QD_B_FUNC] == QD_LED_LIT, "FUNC tapped: latched, lit");
    wtap(7);
    CHECK(TY(0, 7) == QD_TRIG_LOCK, "FUNC latched + tap on a NOTE: a LOCK step");
    btap(QD_B_FUNC);
    CHECK(!A.ui.func_latch, "FUNC tapped again: unlatched");
}

static void g_empty_knob(void)
{
    const int f = A.model.t[0].patch[QD_QP_FREQ];
    start();
    whold(8);
    turn(QD_K_1, -3);
    turn(QD_K_2, 2);
    wrel(8);
    CHECK(TY(0, 8) == QD_TRIG_LOCK && LK(0, 8, QD_QP_FREQ) == A.model.t[0].patch[QD_QP_FREQ] - 300 &&
              LK(0, 8, QD_QP_EDEC) == A.model.t[0].patch[QD_QP_EDEC] + 2,
          "an empty step held + KNOB 1 / 2: a LOCK step with FREQ %d, DECAY %d locks", LK(0, 8, QD_QP_FREQ),
          LK(0, 8, QD_QP_EDEC));
    CHECK(A.model.t[0].patch[QD_QP_FREQ] == f, "the track's own FREQ untouched");
    CHECK(A.ui.tk == UI_TK_DECAY && A.ui.tk_lock, "its takeover, marked as a lock");
}

static void g_clear(void)
{
    static qd_pattern_t before;
    start();
    wtap(0), wtap(1);
    whold(0);
    kdown(WK(1));
    run(300);
    turn(QD_K_PRESETS, 3);
    black(F_VEL);
    turn(QD_K_4, 10);
    before = *PAT();
    black(F_CLEAR);
    CHECK(TY(0, 0) == QD_TRIG_NONE && TY(0, 1) == QD_TRIG_NONE && PAT()->nlock == 0 && A.ui.cf_on,
          "CLEAR: both held steps cleared, their locks and sounds, a confirmation shown");
    run(3000);
    black(F_CLEAR);
    CHECK(!memcmp(&before, PAT(), sizeof before), "CLEAR again within 4 s: undone");
    black(F_CLEAR);
    run(4100);
    black(F_CLEAR);
    CHECK(TY(0, 0) == QD_TRIG_NONE && PAT()->nlock == 0, "CLEAR again after 4 s: cleared again, not undone");
    wrel(0), wrel(1);
    whold(2);
    black(F_CLEAR);
    wtap(9);                                            /* another edit closes the undo */
    whold(2);
    black(F_CLEAR);
    CHECK(TY(0, 9) == QD_TRIG_NOTE, "another edit between: no undo (step 10 kept)");
    wrel(2);
}

static void g_pages(void)
{
    start();
    btap(QD_B_TRACKS);
    turn(QD_K_1, 16);                                   /* LEN 32 */
    btap(QD_B_TRACKS);
    CHECK(PAT()->trk[0].length == 32 && A.ui.mode == QD_MODE_STEP, "TRACKS, KNOB 1 +16: length 32");
    black(PGUP);
    CHECK(A.ui.page[0] == 1, "D#5: page 2");
    black(PGUP);
    CHECK(A.ui.page[0] == 1, "D#5 on the last page: stays");
    wtap(2);
    CHECK(TY(0, 18) == QD_TRIG_NOTE, "a tap on page 2: step 19");
    black(PGDN);
    CHECK(A.ui.page[0] == 0, "C#5: page 1");
    btap(QD_B_PAGE);
    CHECK(A.ui.page[0] == 1, "PAGE tapped: the next page");
    btap(QD_B_PAGE);
    CHECK(A.ui.page[0] == 0, "PAGE tapped on the last: round to page 1");
    bdown(QD_B_PAGE);
    wtap(4);
    bup(QD_B_PAGE);
    run(5);
    CHECK(A.ui.page[0] == 4 && TY(0, 4) == QD_TRIG_NONE, "PAGE held + white key 5: page 5 (no step toggled)");
    btap(QD_B_OCTDN);
    CHECK(A.ui.page[0] == 3, "OCT- with nothing held: page -");
}

static void g_follow(void)
{
    start();
    btap(QD_B_TRACKS);
    turn(QD_K_1, 16);
    btap(QD_B_TRACKS);
    btap(QD_B_PLAY);                                    /* 120 BPM: a 16th is 125 ms; page 2 from 2 s */
    run(1000);
    CHECK(A.ui.page[0] == 0, "playing page 1: shown");
    run(1200);
    CHECK(A.ui.page[0] == 1, "the playhead on page 2: the view follows");
    black(PGDN);
    run(500);
    CHECK(A.ui.page[0] == 0, "a page chosen by hand stays while the playhead stays on its page");
    run(2000);
    CHECK(A.ui.page[0] == 0, "the playhead moved to page 1: the view (already there) stays");
    run(2100);
    CHECK(A.ui.page[0] == 1, "page 2 again: follows");
    bdown(QD_B_FUNC);
    btap(QD_B_PAGE);
    bup(QD_B_FUNC);
    CHECK(!A.ui.follow, "FUNC + PAGE: follow off");
    run(2500);
    CHECK(A.ui.page[0] == 1, "follow off: the page stays");
    btap(QD_B_PLAY);
}

static void g_track_knobs(void)
{
    const int f = A.model.t[0].patch[QD_QP_FREQ];
    int bpm;
    start();
    (void)f;
    turn(QD_K_1, -10);
    turn(QD_K_3, 4);
    turn(QD_K_4, 7);
    turn(QD_K_2, -5);
    CHECK(A.model.t[0].patch[QD_QP_FREQ] == 12700 - 1000 && A.model.t[0].patch[QD_QP_HARM] == 400 &&
              A.model.t[0].patch[QD_QP_FDBK] == 7 && A.model.t[0].patch[QD_QP_EDEC] == 19,
          "the step view's KNOB 1..4: the track's FREQ %d, HARM %d, FDBK %d, DECAY %d (live)",
          A.model.t[0].patch[QD_QP_FREQ], A.model.t[0].patch[QD_QP_HARM], A.model.t[0].patch[QD_QP_FDBK],
          A.model.t[0].patch[QD_QP_EDEC]);
    CHECK(A.ui.tk == UI_TK_DECAY && !A.ui.tk_lock, "KNOB 2's takeover: DECAY");
    run(QD_TK_MS + QD_TK_FADE_MS + 5);
    CHECK(A.ui.tk == QD_TK_NONE, "the takeover gone 1.12 s after the last detent");
    bpm = A.bpm;
    turn(QD_K_SELECT, 6);
    CHECK(A.bpm == bpm + 6 && PAT()->bpm == bpm + 6 && A.ui.tk == UI_TK_TEMPO, "SELECT: tempo %d", A.bpm);
    bdown(QD_B_FUNC);
    turn(QD_K_SELECT, 8);
    bup(QD_B_FUNC);
    CHECK(PAT()->swing == 58 && A.ui.tk == UI_TK_SWING && !A.ui.func_latch, "FUNC + SELECT: swing 58");
    turn(QD_K_ALGO, 2);
    CHECK(A.model.t[0].patch[QD_QP_ALGO] == 3 && A.ui.tk == UI_TK_ALGO, "ALGORITHM: the track's ALGO");
    turn(QD_K_PRESETS, 1);
    CHECK(A.model.t[0].preset == 14 && A.ui.tk == QD_TK_PRESET, "PRESETS: the track's sound");
    btap(QD_B_TRACKS);
    turn(QD_K_1, 8);
    turn(QD_K_2, -3);
    turn(QD_K_3, -3);
    turn(QD_K_4, 1);
    CHECK(PAT()->trk[0].length == 24 && PAT()->trk[0].rate == QD_RATE_1_2 && PAT()->trk[0].limit == 3 &&
              PAT()->trk[0].swing == 1,
          "tracks screen: LEN 24, RATE 1/2x, LIMIT 3, SWING on (%d %s %d %d)", PAT()->trk[0].length,
          qd_rate_name(PAT()->trk[0].rate), PAT()->trk[0].limit, PAT()->trk[0].swing);
    CHECK(A.ui.tk == UI_TK_TSWING, "its takeover");
    turn(QD_K_SELECT, 1);
    CHECK(PAT()->trk[0].length == 25, "SELECT turns what KNOB 1 turns (LEN)");
}

static void g_defaults(void)
{
    start();
    wtap(0);
    whold(0);
    black(F_VEL);
    turn(QD_K_1, 10);
    black(F_LEN);
    turn(QD_K_1, 2);
    black(F_NOTE);
    turn(QD_K_1, -5);
    wrel(0);
    CHECK(PAT()->trk[0].def_vel == 110 && PAT()->trk[0].def_len == QD_LEN_1 + 2 && PAT()->trk[0].def_note == 55,
          "the last entered: note %d, vel %d, len %d", PAT()->trk[0].def_note, PAT()->trk[0].def_vel,
          PAT()->trk[0].def_len);
    wtap(3);
    CHECK(ST(0, 3)->note[0] == 55 && ST(0, 3)->vel == 110 && ST(0, 3)->len == QD_LEN_1 + 2,
          "a new trig gets them");
    whold(3);
    CHECK(A.ui.field == UI_F_NOTE, "a new hold opens the field last used (NOTE)");
    wrel(3);
}

static void g_plocks(void)
{
    start();
    wtap(5);
    whold(5);
    black(F_VEL);
    turn(QD_K_3, 3);
    turn(QD_K_4, -1);
    CHECK(LK(0, 5, QD_QP_HARM) == 300 && LK(0, 5, QD_QP_FDBK) == 0 && A.model.t[0].patch[QD_QP_HARM] == 0,
          "VEL: KNOB 3 / 4 = HARM / FDBK p-locks (HARM %d, FDBK %d), the track untouched", LK(0, 5, QD_QP_HARM),
          LK(0, 5, QD_QP_FDBK));
    CHECK(A.ui.tk == UI_TK_FDBK && A.ui.tk_lock && A.ui.tk_step == 5, "the takeover marked as a lock on step 6");
    turn(QD_K_ALGO, 1);
    CHECK(LK(0, 5, QD_QP_ALGO) == 2, "ALGORITHM: an ALGO lock");
    turn(QD_K_PRESETS, 4);
    CHECK(ST(0, 5)->sound == 4 && A.ui.field == UI_F_SOUND, "PRESETS: the sound lock, the SOUND field");
    black(F_LOCKS);
    turn(QD_K_2, 1);
    turn(QD_K_1, -1);
    CHECK(LK(0, 5, QD_QP_HARM) == 200, "LOCKS: KNOB 2 picks the second lock (HARM), KNOB 1 moves it (%d)",
          LK(0, 5, QD_QP_HARM));
    bdown(QD_B_FUNC);
    turn(QD_K_1, 1);
    bup(QD_B_FUNC);
    CHECK(LK(0, 5, QD_QP_HARM) == -99999 && qd_lock_count(PAT(), 0, 5) == 2, "FUNC + KNOB 1 removes it");
    wrel(5);
}

static void g_step_rec(void)
{
    start();
    btap(QD_B_KEYS);
    btap(QD_B_REC);
    CHECK(A.ui.rec && A.ui.cursor == 0 && leds().button[QD_B_REC] == QD_LED_LIT, "KEYS + REC stopped: step "
          "recording from step 1, REC lit");
    kdown(7), kdown(11), kdown(14);                     /* C4 E4 G4 */
    run(50);
    kup(7), kup(11), kup(14);
    run(10);
    CHECK(TY(0, 0) == QD_TRIG_NOTE && ST(0, 0)->note[0] == 60 && ST(0, 0)->note[1] == 4 && ST(0, 0)->note[2] == 7 &&
              A.ui.cursor == 1,
          "a chord into step 1, the cursor on");
    kdown(9);                                           /* D4, released, then E4 alone */
    run(30);
    kup(9);
    run(10);
    btap(QD_B_OCTUP);                                   /* a rest */
    kdown(11);
    run(30);
    kup(11);
    run(10);
    CHECK(ST(0, 1)->note[0] == 62 && TY(0, 2) == QD_TRIG_NONE && ST(0, 3)->note[0] == 64 && A.ui.cursor == 4,
          "a note, a rest (OCT+), a note; cursor at step 5");
    CHECK(PAT()->trk[0].def_note == 64, "the last entered note");
    btap(QD_B_OCTDN);
    CHECK(A.ui.cursor == 3, "OCT-: a step back");
    turn(QD_K_PRESETS, 2);
    CHECK(A.ui.cursor == 5, "PRESETS moves the cursor");
    btap(QD_B_REC);
    CHECK(!A.ui.rec, "REC: recording off");
}

static void g_live_rec(void)
{
    int micro;
    start();
    btap(QD_B_KEYS);
    btap(QD_B_PLAY);
    btap(QD_B_REC);
    while (qd_seq_track_pos(&A.seq, 0) < 4.0 + 3.0 / 24.0)   /* step 5 + 3/24 */
        run(1);
    kdown(16);                                          /* A4 (69), held two steps */
    run(250);
    kup(16);
    run(5);
    micro = ST(0, 4)->micro;
    CHECK(TY(0, 4) == QD_TRIG_NOTE && ST(0, 4)->note[0] == 69 && micro >= 2 && micro <= 4 && ST(0, 4)->len == 6,
          "live: step 5, micro %d (+3 played), length %s (2 held)", micro, qd_len_name(ST(0, 4)->len));
    while (qd_seq_track_pos(&A.seq, 0) < 8.0 - 4.0 / 24.0)   /* step 9 - 4/24 */
        run(1);
    kdown(14), kdown(18);                               /* a chord, early */
    run(60);
    kup(14), kup(18);
    run(5);
    CHECK(ST(0, 8)->note[0] == 67 && ST(0, 8)->note[1] == 4 && ST(0, 8)->micro <= -3 && ST(0, 8)->micro >= -5,
          "live: a chord early into step 9 (micro %d)", ST(0, 8)->micro);
    bdown(QD_B_FUNC);
    btap(QD_B_REC);
    bup(QD_B_FUNC);
    CHECK(A.ui.quantize && A.ui.rec, "FUNC + REC: quantize on, still recording");
    while (qd_seq_track_pos(&A.seq, 0) < 12.0 + 5.0 / 24.0)
        run(1);
    kdown(12);
    run(40);
    kup(12);
    run(5);
    CHECK(ST(0, 12)->note[0] == 65 && ST(0, 12)->micro == 0, "quantized: step 13, micro 0");
    while (qd_seq_track_pos(&A.seq, 0) < 14.5)
        run(1);
    turn(QD_K_3, 2);
    CHECK(LK(0, 14, QD_QP_HARM) == 200 && TY(0, 14) == QD_TRIG_LOCK, "a knob turned while recording: a lock on the "
          "step playing (step 15)");
    btap(QD_B_PLAY);
}

static void g_midi_held(void)
{
    start();
    wtap(0);
    whold(0);
    qd_app_midi(&A, 0x90, 64, 100);
    qd_app_midi(&A, 0x90, 67, 100);
    run(5);
    CHECK(ST(0, 0)->note[0] == 64 && ST(0, 0)->note[1] == 3, "MIDI notes while a step is held: its chord");
    qd_app_midi(&A, 0x80, 64, 0);
    qd_app_midi(&A, 0x80, 67, 0);
    qd_app_midi(&A, 0x90, 70, 90);
    run(5);
    CHECK(ST(0, 0)->note[0] == 70 && !ST(0, 0)->note[1], "a new MIDI note after the chord's release: replaces it");
    qd_app_midi(&A, 0x80, 70, 0);
    wrel(0);
    qd_app_midi(&A, 0x90, 50, 90);
    run(5);
    qd_app_midi(&A, 0x80, 50, 0);
    CHECK(ST(0, 0)->note[0] == 70, "nothing held: MIDI just plays");
}

/* ================================================================ every field's knobs === */
enum { D_N1 = 1, D_O2 = 2, D_O3 = 4, D_O4 = 8, D_VEL = 16, D_LEN = 32, D_MICRO = 64, D_STRUM = 128, D_RATCH = 256,
       D_RAMP = 512, D_COND = 1024, D_CHANCE = 2048, D_SOUND = 4096, D_TYPE = 8192, D_LOCK = 16384 };
static unsigned diff(const qd_step_t *a, const qd_step_t *b)
{
    unsigned m = 0;
    m |= a->note[0] != b->note[0] ? D_N1 : 0;
    m |= a->note[1] != b->note[1] ? D_O2 : 0;
    m |= a->note[2] != b->note[2] ? D_O3 : 0;
    m |= a->note[3] != b->note[3] ? D_O4 : 0;
    m |= a->vel != b->vel ? D_VEL : 0;
    m |= a->len != b->len ? D_LEN : 0;
    m |= a->micro != b->micro ? D_MICRO : 0;
    m |= a->strum != b->strum ? D_STRUM : 0;
    m |= qd_step_ratchet(a) != qd_step_ratchet(b) ? D_RATCH : 0;
    m |= qd_step_ramp(a) != qd_step_ramp(b) ? D_RAMP : 0;
    m |= a->cond != b->cond ? D_COND : 0;
    m |= a->chance != b->chance ? D_CHANCE : 0;
    m |= a->sound != b->sound ? D_SOUND : 0;
    m |= qd_step_type(a) != qd_step_type(b) ? D_TYPE : 0;
    return m;
}
static void t_fields(void)
{
    static const struct {
        int field, knob, d;
        unsigned want;
        int lock;                                       /* the lock id that must appear (-1: none) */
    } ROWS[] = {
        {F_NOTE, QD_K_SELECT, 1, D_O2 | D_O3, -1},     /* a chord shape: major */
        {F_NOTE, QD_K_1, 2, D_N1, -1},
        {F_NOTE, QD_K_2, 3, D_O2, -1},
        {F_NOTE, QD_K_3, 5, D_O3, -1},
        {F_NOTE, QD_K_4, 7, D_O4, -1},
        {F_VEL, QD_K_SELECT, -3, D_VEL, -1},
        {F_VEL, QD_K_1, -3, D_VEL, -1},
        {F_VEL, QD_K_2, 1, D_RAMP, -1},
        {F_VEL, QD_K_3, 1, D_LOCK, QD_QP_HARM},
        {F_VEL, QD_K_4, 1, D_LOCK, QD_QP_FDBK},
        {F_LEN, QD_K_SELECT, 1, D_LEN, -1},
        {F_LEN, QD_K_1, 1, D_LEN, -1},
        {F_LEN, QD_K_2, 1, D_TYPE, -1},                /* TIE */
        {F_LEN, QD_K_3, 1, D_LOCK, QD_QP_HARM},
        {F_MICRO, QD_K_SELECT, 2, D_MICRO, -1},
        {F_MICRO, QD_K_1, -2, D_MICRO, -1},
        {F_MICRO, QD_K_2, 3, D_STRUM, -1},
        {F_MICRO, QD_K_4, 1, D_LOCK, QD_QP_FDBK},
        {F_RATCH, QD_K_SELECT, 1, D_RATCH, -1},
        {F_RATCH, QD_K_1, 3, D_RATCH, -1},
        {F_RATCH, QD_K_2, 1, D_RAMP, -1},
        {F_RATCH, QD_K_3, 1, D_LOCK, QD_QP_HARM},
        {F_COND, QD_K_SELECT, 3, D_COND, -1},
        {F_COND, QD_K_1, 3, D_COND, -1},
        {F_COND, QD_K_2, -25, D_CHANCE, -1},
        {F_COND, QD_K_4, 1, D_LOCK, QD_QP_FDBK},
        {F_CHNC, QD_K_SELECT, -10, D_CHANCE, -1},
        {F_CHNC, QD_K_1, -10, D_CHANCE, -1},
        {F_CHNC, QD_K_2, 2, D_COND, -1},
        {F_CHNC, QD_K_3, 1, D_LOCK, QD_QP_HARM},
        {F_TRIG, QD_K_SELECT, 1, D_TYPE, -1},
        {F_TRIG, QD_K_1, 2, D_TYPE, -1},
        {F_TRIG, QD_K_2, 1, D_LOCK, QD_QP_EDEC},
        {F_TRIG, QD_K_3, 1, D_LOCK, QD_QP_HARM},
        {F_TRIG, QD_K_4, 1, D_LOCK, QD_QP_FDBK},
        {F_SOUND, QD_K_SELECT, 2, D_SOUND, -1},
        {F_SOUND, QD_K_1, 5, D_SOUND, -1},
        {F_SOUND, QD_K_PRESETS, 1, D_SOUND, -1},
        {F_SOUND, QD_K_2, 1, D_LOCK, QD_QP_EDEC},
        {F_LOCKS, QD_K_SELECT, 1, D_LOCK, QD_QP_FDBK},  /* (the step has an FDBK lock: its value) */
        {F_LOCKS, QD_K_1, 2, D_LOCK, QD_QP_FDBK},
        {F_LOCKS, QD_K_2, 1, 0, -1},                     /* which lock: nothing in the model */
        {F_LOCKS, QD_K_3, 1, D_LOCK, QD_QP_HARM},
        {F_VEL, QD_K_ALGO, 1, D_LOCK, QD_QP_ALGO},     /* ALGORITHM: an ALGO lock, in any field */
        {F_NOTE, QD_K_PRESETS, 2, D_SOUND, -1},        /* PRESETS: the sound lock, from any field */
    };
    size_t i;
    int ok = 0;
    for (i = 0; i < sizeof ROWS / sizeof ROWS[0]; i++) {
        qd_step_t before;
        int nl0, lv0 = -99999;
        unsigned got;
        start();
        wtap(0);
        if (ROWS[i].field == F_LOCKS)
            qd_lock_set(PAT(), 0, 0, QD_QP_FDBK, 20);
        whold(0);
        black(ROWS[i].field);
        before = *ST(0, 0);
        nl0 = qd_lock_count(PAT(), 0, 0);
        if (ROWS[i].lock >= 0)
            lv0 = LK(0, 0, ROWS[i].lock);
        turn(ROWS[i].knob, ROWS[i].d);
        got = diff(&before, ST(0, 0));
        if (qd_lock_count(PAT(), 0, 0) != nl0 || (ROWS[i].lock >= 0 && LK(0, 0, ROWS[i].lock) != lv0))
            got |= D_LOCK;
        CHECK(got == ROWS[i].want, "field %s, knob %d %+d: changed %#x, wanted %#x", UI_FIELD_NAME[ROWS[i].field],
              ROWS[i].knob, ROWS[i].d, got, ROWS[i].want);
        ok += got == ROWS[i].want;
        wrel(0);
    }
    printf("  fields: %d of %d knob mappings change exactly their field\n", ok, (int)(sizeof ROWS / sizeof ROWS[0]));
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
    run(8200);                                          /* two passes of 32 steps at 120 BPM, and a bit */
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
    /* DRUMS (track 1), 2 pages */
    btap(QD_B_TRACKS);
    turn(QD_K_1, 16);
    btap(QD_B_TRACKS);
    for (k = 0; k < 16; k += 4)                         /* kicks on 1 5 9 13 */
        wtap(k);
    wtap(14), wtap(14), wtap(14);                       /* step 15 on, off, on: on */
    whold(4);                                           /* 5 and 13: snares, a sound lock (NOISE-ISH) */
    kdown(WK(12));
    run(300);
    turn(QD_K_PRESETS, 17);
    kup(WK(4)), kup(WK(12));
    run(10);
    whold(15);                                          /* step 16: a 1:2 condition (an empty step: a field key
                                                         * first, then the field's knob makes the trig) */
    black(F_COND);
    turn(QD_K_1, 9);
    wrel(15);
    black(PGUP);                                        /* page 2 */
    for (k = 0; k < 16; k += 4)
        wtap(k);
    wtap(15);                                           /* step 32: a ratchet x3 */
    whold(15);
    black(F_RATCH);
    turn(QD_K_1, 2);
    wrel(15);
    whold(12);                                          /* step 29: FDBK p-lock (VEL's KNOB 4) */
    black(F_VEL);
    turn(QD_K_4, 30);
    wrel(12);
    /* BASS (track 2): 2 pages, notes, a chord-less line */
    black(T2);
    btap(QD_B_TRACKS);
    turn(QD_K_1, 16);
    btap(QD_B_TRACKS);
    black(PGDN);
    wtap(0), wtap(3), wtap(6), wtap(10);
    whold(3);
    black(F_NOTE);
    turn(QD_K_1, -12);                                  /* 48, and the default now 48 */
    wrel(3);
    wtap(12);
    black(PGUP);
    wtap(0), wtap(6);
    /* KEYS (track 3): a chord, Am7 */
    black(T3);
    wtap(0);
    whold(0);
    black(F_NOTE);
    turn(QD_K_SELECT, 4);                               /* m7 */
    turn(QD_K_1, -3);                                   /* on A3 */
    black(F_LEN);
    turn(QD_K_1, 4);                                    /* 4 steps */
    wrel(0);
}

/* the same beat through the API */
static void beat_by_api(void)
{
    qd_pattern_t *p;
    int k;
    static const uint8_t AM7[4] = {57, 60, 64, 67};
    fresh();
    wake();
    empty_pattern(4);
    p = PAT();
    p->trk[0].length = 32;
    for (k = 0; k < 16; k += 4)
        qd_step_set_trig(p, 0, k, QD_TRIG_NOTE);
    qd_step_set_trig(p, 0, 14, QD_TRIG_NOTE);
    qd_step_set_sound(p, 0, 4, 17);
    qd_step_set_sound(p, 0, 12, 17);
    qd_step_set_trig(p, 0, 15, QD_TRIG_NOTE);
    qd_step_set_cond(p, 0, 15, qd_cond_ab(1, 2));
    for (k = 16; k < 32; k += 4)
        qd_step_set_trig(p, 0, k, QD_TRIG_NOTE);
    qd_step_set_trig(p, 0, 31, QD_TRIG_NOTE);
    qd_step_set_ratchet(p, 0, 31, 3, QD_RAMP_FLAT);
    qd_lock_set(p, 0, 28, QD_QP_FDBK, A.model.t[0].patch[QD_QP_FDBK] + 30);
    p->trk[1].length = 32;
    qd_step_set_trig(p, 1, 0, QD_TRIG_NOTE);
    qd_step_set_trig(p, 1, 3, QD_TRIG_NOTE);
    qd_step_set_trig(p, 1, 6, QD_TRIG_NOTE);
    qd_step_set_trig(p, 1, 10, QD_TRIG_NOTE);
    qd_step_set_root(p, 1, 3, 48);
    p->trk[1].def_note = 48;
    qd_step_set_trig(p, 1, 12, QD_TRIG_NOTE);
    qd_step_set_trig(p, 1, 16, QD_TRIG_NOTE);
    qd_step_set_trig(p, 1, 22, QD_TRIG_NOTE);
    qd_step_set_trig(p, 2, 0, QD_TRIG_NOTE);
    qd_step_set_notes(p, 2, 0, AM7, 4);
    qd_step_set_len(p, 2, 0, QD_LEN_1 + 4);
}

static void t_beat(void)
{
    static qd_pattern_t panel;
    int i, same = 1, t, s;
    beat_by_panel();
    panel = *PAT();
    timeline(0);
    beat_by_api();
    for (t = 0; t < 3; t++)                             /* the steps and the locks equal */
        for (s = 0; s < QD_NSTEP; s++)
            if (memcmp(&panel.step[t][s], ST(t, s), sizeof(qd_step_t)) != 0) {
                CHECK(0, "the beat: track %d step %d differs (panel: type %d note %d+%d/%d/%d vel %d len %d "
                      "cond %d sound %d; api: type %d note %d+%d/%d/%d)", t + 1, s + 1, qd_step_type(&panel.step[t][s]),
                      panel.step[t][s].note[0], panel.step[t][s].note[1], panel.step[t][s].note[2],
                      panel.step[t][s].note[3], panel.step[t][s].vel, panel.step[t][s].len, panel.step[t][s].cond,
                      panel.step[t][s].sound, TY(t, s), ST(t, s)->note[0], ST(t, s)->note[1], ST(t, s)->note[2],
                      ST(t, s)->note[3]);
                same = 0;
            }
    CHECK(panel.nlock == PAT()->nlock && !memcmp(panel.lock, PAT()->lock, panel.nlock * sizeof panel.lock[0]),
          "the beat: the same locks (%d, %d)", panel.nlock, PAT()->nlock);
    CHECK(panel.trk[0].length == 32 && panel.trk[1].length == 32, "two pages on drums and bass");
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
    {   /* what it holds */
        int ons = 0, sl = 0, rat = 0;
        for (i = 0; i < nev[0]; i++) {
            ons += evs[0][i].kind == QD_LOG_ON;
            rat += evs[0][i].kind == QD_LOG_ON && evs[0][i].track == 0 && evs[0][i].tick % QD_TPS != 0;
        }
        sl = panel.step[0][4].sound == 17 && panel.step[0][12].sound == 17;
        CHECK(sl && rat >= 4 && panel.step[2][0].note[3] == 10 && panel.step[0][15].cond == qd_cond_ab(1, 2) &&
                  panel.nlock == 1,
              "the beat holds a sound lock, a p-lock, a ratchet (%d hits off the grid), a condition and a chord", rat);
        printf("  beat: by panel presses only, a 2-page drum + bass pattern with a sound lock, a p-lock, a ratchet, a "
               "condition and a chord; its timeline (%d events, %d notes) equals the API-built one: %s\n", nev[0], ons,
               same ? "yes" : "NO");
    }
}

/* ================================================================ the LEDs === */
static void t_leds(void)
{
    qd_leds_t l;
    int lit = 0, dark = 0, k, inv = 0;
    start();
    wtap(0);
    bdown(QD_B_FUNC);
    wtap(1);
    bup(QD_B_FUNC);
    btap(QD_B_TRACKS);
    turn(QD_K_1, -4);                                   /* length 12 */
    btap(QD_B_TRACKS);
    l = leds();
    CHECK(l.key[WK(0)] == QD_LED_LIT && l.key[WK(1)] == QD_LED_DIM && l.key[WK(2)] == QD_LED_FAINT &&
              l.key[WK(12)] == QD_LED_OFF,
          "stopped: a trig lit, a LOCK step dim, an empty one faint, beyond the length off (%d %d %d %d)",
          l.key[WK(0)], l.key[WK(1)], l.key[WK(2)], l.key[WK(12)]);
    CHECK(l.key[BK(T1)] == QD_LED_LIT && l.key[BK(T2)] == QD_LED_FAINT && l.key[BK(PGDN)] == QD_LED_OFF &&
              l.key[BK(PGUP)] == QD_LED_OFF,
          "black: the selected track lit, the others faint, no page either way");
    btap(QD_B_TRACKS);
    turn(QD_K_1, 20);                                   /* 32: a page that way */
    btap(QD_B_TRACKS);
    CHECK(leds().key[BK(PGUP)] == QD_LED_MID && leds().key[BK(PGDN)] == QD_LED_OFF, "a page that way: D#5 mid");
    btap(QD_B_PLAY);
    for (k = 0; k < 2000; k++) {                        /* the playhead inverts its key; the trig flashes T1 */
        const int s = (int)qd_seq_track_pos(&A.seq, 0);
        run(1);
        l = leds();
        if (s >= 2 && s < 16 && s == (int)qd_seq_track_pos(&A.seq, 0))
            inv += l.key[WK(s)] == QD_LED_LIT;
        lit += l.key[BK(T1)] == QD_LED_OFF;
        dark += l.play_green == QD_LED_LIT;
    }
    CHECK(inv > 500, "playing: the playhead's (empty) key lit (%d ms)", inv);
    CHECK(lit > 20 && lit < 600, "the selected track's key inverts on its trigs (%d ms)", lit);
    CHECK(dark > 200 && dark < 600, "PLAY's green LED on the beat (%d ms of 2000)", dark);
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
    black(F_COND);
    turn(QD_K_1, 2);
    black(F_VEL);
    l = leds();
    CHECK(l.key[BK(F_VEL)] == QD_LED_LIT && l.key[BK(F_COND)] == QD_LED_DIM && l.key[BK(F_NOTE)] == QD_LED_FAINT,
          "a step held: the field lit, a field with a value dim, the rest faint");
    wrel(0);
    btap(QD_B_KEYS);
    l = leds();
    CHECK(l.button[QD_B_KEYS] == QD_LED_LIT && l.key[7] == QD_LED_FAINT && l.key[8] == QD_LED_OFF,
          "KEYS: lit, the C keys faint");
    kdown(9);
    run(10);
    CHECK(leds().key[9] == QD_LED_LIT, "KEYS: a note sounding lit");
    kup(9);
    btap(QD_B_REC);
    {
        int on = 0;
        for (k = 0; k < 600; k++) {
            run(1);
            on += leds().key[WK(A.ui.cursor % 16)] == QD_LED_LIT;
        }
        CHECK(on > 150 && on < 450 && leds().button[QD_B_REC] == QD_LED_LIT, "step recording: the cursor blinks "
              "(%d of 600 ms), REC lit", on);
    }
    printf("  leds: the step view stopped / playing, the black keys, a held step and its fields, KEYS, step "
           "recording, FUNC, PLAY's beat\n");
}

/* ================================================================ determinism === */
static uint64_t session(void)
{
    beat_by_panel();
    btap(QD_B_PLAY);
    btap(QD_B_KEYS);
    btap(QD_B_REC);
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
        {"tap on / off, the LED at the press", g_tap},
        {"hold vs tap at the 250 ms edge", g_edge},
        {"hold several", g_several},
        {"latch (OCT+ / FUNC + OCT+), OCT- / HOME let go", g_latch},
        {"FUNC + tap: LOCK steps; FUNC's latch", g_func_lock},
        {"an empty step held + a knob: a LOCK step", g_empty_knob},
        {"CLEAR and its undo", g_clear},
        {"pages", g_pages},
        {"page follow", g_follow},
        {"the step view's and the tracks screen's knobs", g_track_knobs},
        {"the last entered defaults", g_defaults},
        {"p-locks of the quick page with a step held, LOCKS", g_plocks},
        {"KEYS step recording", g_step_rec},
        {"KEYS live recording", g_live_rec},
        {"MIDI notes into a held step", g_midi_held},
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
        printf("  gesture: %-50s %d checks%s\n", G[i].name, checks - c0, fails > f0 ? "  FAILED" : "");
    }
    t_fields();
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
