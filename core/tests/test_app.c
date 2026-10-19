/* SPDX-License-Identifier: GPL-3.0-only */
/* The app with its sound (P1.1), driven as the glue drives it (core/app/app.h: input events, a tick a millisecond,
 * the audio in 128-frame blocks as they fall due):
 *   keys        the keybed plays the selected track (velocity, OCT- / OCT+ -2..+2), TRACKS + white key 1..8 selects
 *               a track and plays nothing, the note a key struck is released at its release whatever changed since
 *   presets     PRESETS steps the track's sound through the project's pool (INIT .. OPEN HAT, clamped; P1.3b), the
 *               picker takes the screen and leaves 1.12 s after the last detent; a voice sounding keeps its patch
 *   screens     REC toggles the keyboard (REC's LED lit; P1.3b), HOME: the step view; TRACKS held dims the selected
 *               track's white key; the C keys faint in the keyboard; a note sounding lights its key
 *   midi        notes on channels 1..8 play tracks 1..8; note off and velocity 0 release; channels 9..16 ignored
 *   mix         a voice panned hard left with no sends: nothing on the right; level 0: silence
 *   limiter     16 voices of SQUARE LEAD at velocity 127, every level and send at the top, MASTER full: no sample
 *               above Output Clean's ceiling (CLN_T, 26000 at MASTER full) and the limiter at work; then released:
 *               exact digital silence (the integer chain has no NaN and no denormal tail: it ends in zeros, the
 *               voices free, the effects off)
 *   panic       OCT- and OCT+ together: every voice gone within two blocks
 *   determinism the same session twice: the same samples
 * P1.2, the sequencer on the unit's controls:
 *   play        A02 at power-on (100 BPM); PLAY plays it (its voices start, the dancers turn, PLAY's green LED on the
 *               beat), PLAY again stops: the sequencer's notes released, a held key's note not
 *   patterns    PATTERN + white key 1 while playing: queued (the screen's name blinks: two frames differ), taken
 *               at the end of A02's master length (32 sixteenths), not before; PATTERN + FUNC + white key 3: at the
 *               next 16th; stopped: at once; PATTERN held: the LEDs (playing lit, queued blinking, content dim); ARP held: FILL
 *   tracks      TRACKS tapped: the tracks screen (its LED lit), white key 9 mutes track 1 (its notes stop, key 9's LED
 *               goes dark), white key 2 selects track 2 (no note), the trigs flash white keys 1..8; TRACKS again or
 *               HOME leaves; TRACKS held + white key still selects
 *   along       the keyboard while playing: the keys play the selected track along
 *   clock       MIDI START and 24 clocks a beat at 140 BPM: the transport runs at 140 (the dancers too), STOP stops
 *   determinism a session with the sequencer (PLAY, a queued change, keys, MIDI clock) twice: the same samples
 *
 *     test_app      exit 1 on a failed check */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "qd_gfx.h"

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
static uint32_t now, frames_done;
static int32_t peak_l, peak_r;
static uint64_t hash;
static uint32_t over;                                   /* samples above a threshold (run's argument) */
static int16_t last[256];

/* n ms of the core's clock: a tick a ms, the audio blocks that fall due; the peaks, a hash, the samples over thr */
static void run(uint32_t n, int thr)
{
    static uint16_t lcd[QD_W * QD_H];
    while (n--) {
        now++;
        qd_app_tick(&A, now);
        if (now % 15u == 0u)
            qd_app_frame(&A, lcd);
        while (frames_done + 128u <= (uint64_t)now * QD_FS / 1000u) {
            int k;
            qd_app_audio(&A, last, 128);
            for (k = 0; k < 256; k++) {
                const int32_t v = abs(last[k]);
                if (k & 1)
                    peak_r = v > peak_r ? v : peak_r;
                else
                    peak_l = v > peak_l ? v : peak_l;
                over += thr && v > thr;
                hash = (hash ^ (uint16_t)last[k]) * 1099511628211ull;
            }
            frames_done += 128u;
        }
    }
}
static void fresh(void)
{
    qd_app_init(&A);
    now = frames_done = 0;
    qd_app_tick(&A, 0);
    peak_l = peak_r = 0;
    hash = 14695981039346656037ull;
    over = 0;
}
static void peaks0(void) { peak_l = peak_r = 0; }
static void tap_button(int b);
static void fresh_keys(void)             /* power-on is the idle screen; REC (P1.3b): the keyboard plays the keybed */
{
    fresh();
    tap_button(QD_B_REC);
}
static int voices_of(int track)          /* the track's voices held (gate) */
{
    int i, n = 0;
    for (i = 0; i < A.pool.nvoice; i++)
        n += qd_pool_sounding(&A.pool, i) && A.pool.v[i].track == track && A.pool.v[i].q.gate;
    return n;
}
static int sounding(int track, int note)
{
    int i;
    for (i = 0; i < A.pool.nvoice; i++)
        if (qd_pool_sounding(&A.pool, i) && A.pool.v[i].track == track && A.pool.v[i].q.note == note && A.pool.v[i].q.gate)
            return 1;
    return 0;
}
static int vel_of(int track, int note)
{
    int i;
    for (i = 0; i < A.pool.nvoice; i++)
        if (qd_pool_sounding(&A.pool, i) && A.pool.v[i].track == track && A.pool.v[i].q.note == note)
            return A.pool.v[i].q.vel;
    return -1;
}
static void tap_button(int b)
{
    qd_app_button(&A, b, 1);
    run(20, 0);
    qd_app_button(&A, b, 0);
    run(20, 0);
}

static void t_keys(void)
{
    fresh_keys();
    qd_app_key(&A, 9, 1, 100);                          /* D4 on track 1 (DRUMS) */
    run(50, 0);
    CHECK(sounding(0, 62) && peak_l > 64, "D4 should sound on track 1 (peak %d)", (int)peak_l);
    qd_app_key(&A, 9, 0, 0);
    run(5, 0);
    CHECK(!sounding(0, 62), "D4 released");
    tap_button(QD_B_OCTUP);
    qd_app_key(&A, 0, 1, 90);                           /* F3 + an octave */
    run(5, 0);
    CHECK(A.oct == 1 && sounding(0, 65), "OCT+: F3 plays F4");
    tap_button(QD_B_OCTDN);                             /* the octave changes under the held key */
    qd_app_key(&A, 0, 0, 0);
    run(5, 0);
    CHECK(!sounding(0, 65), "the key's own note released after the octave changed");
    tap_button(QD_B_OCTDN), tap_button(QD_B_OCTDN), tap_button(QD_B_OCTDN);
    CHECK(A.oct == QD_OCT_MIN, "OCT- clamps at -2 (%d)", A.oct);
    tap_button(QD_B_OCTUP), tap_button(QD_B_OCTUP);
    /* TRACKS + white key 3 selects track 3 and plays nothing */
    qd_app_button(&A, QD_B_TRACKS, 1);
    qd_app_key(&A, 4, 1, 100);                          /* A3: the third white key */
    run(10, 0);
    CHECK(A.sel == 2 && !voices_of(0) && !voices_of(2), "TRACKS + white key 3: track 3 selected, nothing played");
    {
        qd_leds_t l;
        qd_app_leds(&A, &l);
        CHECK(l.key[4] == QD_LED_LIT && l.button[QD_B_TRACKS] == QD_LED_LIT, "TRACKS held: its LED, the key held lit");
        qd_app_key(&A, 4, 0, 0);
        qd_app_leds(&A, &l);
        CHECK(l.key[4] == QD_LED_DIM, "TRACKS held: the selected track's white key dim (%d)", l.key[4]);
    }
    qd_app_button(&A, QD_B_TRACKS, 0);
    qd_app_key(&A, 7, 1, 127);                          /* C4 on track 3 (KEYS) */
    run(10, 0);
    CHECK(sounding(2, 60) && vel_of(2, 60) == 127, "track 3 plays C4 at velocity 127");
    qd_app_key(&A, 7, 0, 0);
    run(10, 0);
    printf("  keys: the keybed plays the selected track; OCT -2..+2; TRACKS + white key selects\n");
}

static void t_presets(void)
{
    int16_t before[QD_QUAD_NP];
    int i, v = -1;
    fresh_keys();
    qd_app_key(&A, 7, 1, 100);                          /* a note on track 1 */
    run(20, 0);
    for (i = 0; i < A.pool.nvoice; i++)
        if (qd_pool_sounding(&A.pool, i))
            v = i;
    memcpy(before, A.model.t[0].patch, sizeof before);
    qd_app_knob(&A, QD_K_PRESETS, 2);                   /* MARIMBA (pool 14) -> FEEDBACK (16) */
    run(10, 0);
    CHECK(A.model.t[0].sound == 16 && !strcmp(qd_track_sound_name(&A.model.t[0]), "FEEDBACK") && A.ui.tk == QD_TK_PRESET,
          "PRESETS +2: track 1's sound %d (%s), picker %d", A.model.t[0].sound, qd_track_sound_name(&A.model.t[0]), A.ui.tk);
    CHECK(v >= 0 && !memcmp(A.pool.v[v].q.patch, before, sizeof before), "the sounding voice keeps its patch");
    qd_app_knob(&A, QD_K_PRESETS, 40);
    CHECK(A.model.t[0].sound == A.proj.nsound && !strcmp(qd_track_sound_name(&A.model.t[0]), "OPEN HAT"),
          "PRESETS clamps at the pool's last sound");
    qd_app_knob(&A, QD_K_PRESETS, -60);
    CHECK(A.model.t[0].sound == 1 && !strcmp(qd_track_sound_name(&A.model.t[0]), "INIT"), "PRESETS clamps at INIT");
    run(1100, 0);
    CHECK(A.ui.tk == QD_TK_PRESET, "the picker still up 1.1 s after the last detent (its fade)");
    run(30, 0);
    CHECK(A.ui.tk == QD_TK_NONE, "the picker gone 1.13 s after the last detent");
    qd_app_key(&A, 7, 0, 0);
    printf("  presets: PRESETS steps the pool, INIT .. OPEN HAT (clamped); the picker leaves 1.12 s after; voices keep "
           "theirs\n");
}

static void t_screens(void)
{
    qd_leds_t l;
    fresh();
    tap_button(QD_B_REC);
    qd_app_leds(&A, &l);
    CHECK(A.ui.mode == QD_MODE_KEYS && l.button[QD_B_REC] == QD_LED_LIT, "REC: the keyboard, REC's LED lit");
    CHECK(l.key[7] == QD_LED_FAINT && l.key[19] == QD_LED_FAINT && !l.key[8], "the keyboard: the C keys faint");
    qd_app_midi(&A, 0x90, 64, 100);                     /* E4 from MIDI on track 1 (selected): its key lit */
    run(10, 0);
    qd_app_leds(&A, &l);
    CHECK(l.key[11] == QD_LED_LIT, "a note sounding on the track lights its key (%d)", l.key[11]);
    qd_app_midi(&A, 0x80, 64, 0);
    tap_button(QD_B_REC);
    CHECK(A.ui.mode == QD_MODE_STEP, "REC again: the step view");
    tap_button(QD_B_REC);
    tap_button(QD_B_HOME);
    qd_app_leds(&A, &l);
    CHECK(A.ui.mode == QD_MODE_STEP && l.button[QD_B_REC] == QD_LED_OFF, "HOME: the step view, REC's LED off");
    printf("  screens: REC toggles the keyboard (lit), HOME: the step view; C keys faint; sounding notes light their "
           "keys\n");
}

static void t_midi(void)
{
    fresh();
    qd_app_midi(&A, 0x94, 60, 90);                      /* channel 5: track 5 */
    qd_app_midi(&A, 0x97, 72, 90);                      /* channel 8: track 8 */
    qd_app_midi(&A, 0x98, 50, 90);                      /* channel 9: none */
    run(10, 0);
    CHECK(sounding(4, 60) && sounding(7, 72) && qd_pool_in_use(&A.pool) == 2, "MIDI channels 5 and 8 -> tracks 5 and 8, 9 "
          "ignored (%d voices)", qd_pool_in_use(&A.pool));
    qd_app_midi(&A, 0x84, 60, 0);
    qd_app_midi(&A, 0x97, 72, 0);                       /* velocity 0: off */
    run(5, 0);
    CHECK(!sounding(4, 60) && !sounding(7, 72), "note off and velocity 0 release");
    printf("  midi: channels 1..8 play tracks 1..8\n");
}

static void t_mix(void)
{
    qd_track_t *t;
    fresh();
    t = &A.model.t[2];
    t->mix.pan = -64;
    t->mix.reverb = t->mix.delay = t->mix.chorus = 0;
    qd_app_midi(&A, 0x92, 60, 110);
    peaks0();
    run(400, 0);
    CHECK(peak_l > 1000 && peak_r == 0, "pan hard left, no sends: left %d, right %d", (int)peak_l, (int)peak_r);
    qd_app_midi(&A, 0x82, 60, 0);
    run(2000, 0);
    t->mix.level = 0;
    qd_app_midi(&A, 0x92, 62, 110);
    peaks0();
    run(400, 0);
    CHECK(peak_l == 0 && peak_r == 0, "level 0: silence (%d, %d)", (int)peak_l, (int)peak_r);
    printf("  mix: pan and level per voice (hard left: the right silent; level 0: nothing)\n");
}

static void t_limiter(void)
{
    static const int CH[8][2] = {{36, 48}, {40, 52}, {55, 59}, {62, 66}, {69, 72}, {76, 79}, {83, 86}, {88, 91}};
    int t, k, zero_tail;
    uint32_t n_over;
    fresh();
    qd_app_master(&A, 1023);
    for (t = 0; t < QD_NTRACK; t++) {
        qd_track_load_preset(&A.model.t[t], 8);         /* SQUARE LEAD */
        A.model.t[t].mix.level = 127;
        A.model.t[t].mix.chorus = A.model.t[t].mix.delay = A.model.t[t].mix.reverb = 127;
        qd_pool_set_track(&A.pool, t, 16, 0);
    }
    for (t = 0; t < QD_NTRACK; t++)
        for (k = 0; k < 2; k++)
            qd_app_midi(&A, 0x90 | t, CH[t][k], 127);
    peaks0();
    run(3000, 26000);
    n_over = over;
    over = 0;
    CHECK(qd_pool_in_use(&A.pool) == 16, "16 voices (%d)", qd_pool_in_use(&A.pool));
    CHECK(n_over == 0 && peak_l <= 26000 && peak_r <= 26000, "Output Clean's ceiling: %u samples above 26000, peaks %d %d",
          (unsigned)n_over, (int)peak_l, (int)peak_r);
    CHECK(peak_l > 24000 || peak_r > 24000, "the limiter at work: peaks %d %d", (int)peak_l, (int)peak_r);
    printf("  limiter: 16 voices at full level, every send up, MASTER full: peaks %d / %d, none above 26000\n",
           (int)peak_l, (int)peak_r);
    for (t = 0; t < QD_NTRACK; t++)
        qd_app_midi(&A, 0xB0 | t, 123, 0);              /* all notes off */
    run(9000, 0);
    peaks0();
    run(1000, 0);
    zero_tail = peak_l == 0 && peak_r == 0;
    CHECK(zero_tail && qd_pool_in_use(&A.pool) == 0, "after the release: exact silence (%d %d), voices %d", (int)peak_l,
          (int)peak_r, qd_pool_in_use(&A.pool));
    printf("  tails: 9 s after the release the chain gives exact zeros and every voice is free\n");
}

static void t_panic(void)
{
    int k;
    fresh_keys();
    for (k = 0; k < 10; k++)
        qd_app_midi(&A, 0x90 | (k % 8), 50 + k, 100);
    run(50, 0);
    qd_app_button(&A, QD_B_OCTDN, 1);
    qd_app_button(&A, QD_B_OCTUP, 1);
    run(10, 0);                                         /* (the fade's block, then free) */
    CHECK(qd_pool_in_use(&A.pool) == 0 && A.oct == -1, "PANIC: voices %d, octave %d", qd_pool_in_use(&A.pool), A.oct);
    printf("  panic: OCT- and OCT+ together: every voice gone at once (faded over a block)\n");
}

/* ---------------------------------------------------------------- P1.2 --- */
static const uint8_t WK[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};   /* white keys */
static void tap_key(int key)
{
    qd_app_key(&A, key, 1, 100);
    run(10, 0);
    qd_app_key(&A, key, 0, 0);
    run(10, 0);
}
static int seq_voices(void)              /* voices held (gate) on any track */
{
    int i, n = 0;
    for (i = 0; i < A.pool.nvoice; i++)
        n += qd_pool_sounding(&A.pool, i) && A.pool.v[i].q.gate;
    return n;
}
static uint64_t frame_hash(void)
{
    static uint16_t lcd[QD_W * QD_H];
    uint64_t h = 14695981039346656037ull;
    int i;
    qd_app_frame(&A, lcd);
    for (i = 0; i < QD_W * QD_H; i++)
        h = (h ^ lcd[i]) * 1099511628211ull;
    return h;
}

static void t_play(void)
{
    qd_leds_t l;
    uint32_t starts;
    int green = 0, k;
    fresh_keys();
    CHECK(A.seq.cur == 1 && A.bpm == 100 && !A.seq.playing, "power-on: A02 at 100 BPM, stopped");
    tap_button(QD_B_PLAY);
    starts = A.pool.starts;
    for (k = 0; k < 1200; k++) {
        run(1, 0);
        qd_app_leds(&A, &l);
        green += l.play_green != 0;
    }
    CHECK(A.seq.playing && A.playing && A.pool.starts > starts + 8 && peak_l > 1000, "PLAY: A02 plays (%u starts)",
          A.pool.starts - starts);
    CHECK(green > 100 && green < 300, "PLAY's green LED on the beat (%d of 1200 ms)", green);
    CHECK(A.beats > 1.8 && A.beats < 2.2, "the transport moved 2 beats in 1.2 s at 100 BPM (%f)", A.beats);
    qd_app_key(&A, 9, 1, 100);                          /* a key held across the stop */
    run(5, 0);
    tap_button(QD_B_PLAY);
    run(5, 0);
    CHECK(!A.seq.playing && seq_voices() == 1 && sounding(A.sel, 62), "PLAY again: the sequence's notes released, the "
          "key's held (%d held)", seq_voices());
    qd_app_key(&A, 9, 0, 0);
    printf("  play: A02 at power-on, PLAY plays and stops, the beat LED, a held key outlives the stop\n");
}

static void t_patterns(void)
{
    qd_leds_t l;
    uint64_t h0, h1;
    uint32_t t0;
    fresh();
    tap_button(QD_B_PLAY);
    run(500, 0);
    qd_app_button(&A, QD_B_MIX, 1);
    qd_app_key(&A, WK[0], 1, 100);                      /* A01 */
    run(10, 0);
    qd_app_leds(&A, &l);
    CHECK(qd_seq_queued(&A.seq) == 0 && A.seq.cur == 1, "SEQ + white key 1: A01 queued, A02 plays on");
    CHECK(l.key[WK[1]] == QD_LED_LIT && l.key[WK[2]] == QD_LED_DIM && l.key[WK[4]] == QD_LED_OFF,
          "SEQ held: A02 lit, A03 dim (content), A05 off");
    qd_app_key(&A, WK[0], 0, 0);
    qd_app_button(&A, QD_B_MIX, 0);
    h0 = frame_hash();
    run(250, 0);
    h1 = frame_hash();
    CHECK(h0 != h1, "the screen's pattern blinks while one is queued");
    t0 = now;                                           /* the change: 32 sixteenths at 100 BPM = 4800 ms from PLAY
                                                         * (pressed at 0 ms: the transport starts at the next block) */
    while (A.seq.cur == 1 && now < 10000)
        run(1, 0);
    CHECK(A.seq.cur == 0 && qd_seq_queued(&A.seq) < 0 && now >= 4800 && now <= 4810,
          "A01 takes over at the end of A02's master length (at %u ms; PLAY at 0)", now);
    (void)t0;
    qd_app_button(&A, QD_B_MIX, 1);                     /* P1.3c: the key held 400 ms: at the next 16th */
    qd_app_key(&A, WK[2], 1, 100);
    run(400, 0);
    t0 = now;
    while (A.seq.cur != 2 && now < t0 + 1000)
        run(1, 0);
    qd_app_key(&A, WK[2], 0, 0);
    qd_app_button(&A, QD_B_MIX, 0);
    CHECK(A.seq.cur == 2 && now - t0 <= 151, "SEQ + white key 3 held 400 ms: A03 within a 16th (%u ms)", now - t0);
    run(200, 0);
    CHECK(A.bpm == 118, "A03's tempo (%d)", A.bpm);
    tap_button(QD_B_PLAY);
    qd_app_button(&A, QD_B_MIX, 1);                 /* stopped: at once */
    tap_key(WK[1]);
    qd_app_button(&A, QD_B_MIX, 0);
    CHECK(A.seq.cur == 1 && qd_seq_queued(&A.seq) < 0 && A.bpm == 100, "stopped: A02 at once, its tempo");
    qd_app_button(&A, QD_B_KEYS, 1);
    CHECK(A.seq.fill, "ARP held: FILL");
    qd_app_button(&A, QD_B_KEYS, 0);
    CHECK(!A.seq.fill, "ARP released: no FILL");
    printf("  patterns: queued to the end of the master length (the name blinks), FUNC: the next 16th, stopped: at "
           "once; the LEDs\n");
}

static void t_tracks(void)
{
    qd_leds_t l;
    int k, flashes = 0, n1 = 0;
    uint32_t starts1 = 0;
    fresh();
    tap_button(QD_B_TRACKS);
    CHECK(A.ui.mode == QD_MODE_TRACKS, "TRACKS tapped: the tracks screen");
    qd_app_leds(&A, &l);
    CHECK(l.button[QD_B_TRACKS] == QD_LED_LIT && l.key[WK[8]] == QD_LED_LIT && l.key[WK[15]] == QD_LED_LIT,
          "its LED lit, white keys 9..16 lit (every track heard)");
    tap_button(QD_B_PLAY);
    for (k = 0; k < 2000; k++) {                        /* the trigs flash white keys 1..8 */
        run(1, 0);
        qd_app_leds(&A, &l);
        flashes += l.key[WK[1]] == QD_LED_LIT;          /* (track 2, the bass) */
    }
    CHECK(flashes > 100 && flashes < 1500, "track 2's white key flashes with its trigs (%d ms of 2000)", flashes);
    tap_key(WK[8]);                                     /* white key 9: track 1 muted */
    qd_app_leds(&A, &l);
    CHECK((A.seq.mute & 1u) && l.key[WK[8]] == QD_LED_OFF && l.key[WK[9]] == QD_LED_LIT,
          "white key 9: track 1 muted, its key dark");
    for (k = 0; k < A.pool.nvoice; k++)
        starts1 += A.pool.v[k].track == 0 && A.pool.v[k].q.active;
    run(1000, 0);
    for (k = 0; k < A.pool.nvoice; k++)
        n1 += A.pool.v[k].track == 0 && qd_pool_sounding(&A.pool, k) && A.pool.v[k].q.gate;
    CHECK(n1 == 0, "the muted track plays nothing (%d held)", n1);
    tap_key(WK[8]);
    CHECK(!(A.seq.mute & 1u), "white key 9 again: heard");
    tap_key(WK[1]);
    CHECK(A.sel == 1 && A.keys_struck == 0, "white key 2 on the tracks screen: track 2 selected, no note");
    {
        uint64_t h0 = frame_hash();
        run(100, 0);
        CHECK(h0 != frame_hash(), "the clocks turn");
    }
    tap_button(QD_B_TRACKS);
    CHECK(A.ui.mode == QD_MODE_STEP, "TRACKS again: back to the step view");
    tap_button(QD_B_TRACKS);
    tap_button(QD_B_HOME);
    CHECK(A.ui.mode == QD_MODE_STEP, "HOME leaves the tracks screen");
    qd_app_button(&A, QD_B_TRACKS, 1);
    tap_key(WK[3]);
    qd_app_button(&A, QD_B_TRACKS, 0);
    run(10, 0);
    CHECK(A.sel == 3 && A.ui.mode == QD_MODE_STEP, "TRACKS held + white key 4: track 4, no screen change");
    (void)starts1;
    printf("  tracks: TRACKS tapped / again / HOME, mute 9..16, select 1..8, the trig LEDs, the clocks turn\n");
}

static void t_along(void)
{
    int k, peak_with = 0;
    fresh();
    tap_button(QD_B_REC);
    tap_button(QD_B_PLAY);
    run(300, 0);
    qd_app_key(&A, 14, 1, 110);                         /* G4 on track 1 while A02 plays */
    for (k = 0; k < 200; k++) {
        run(1, 0);
        peak_with |= sounding(0, 67 - 0);
    }
    CHECK(peak_with, "KEYS while playing: the key plays the selected track along");
    qd_app_key(&A, 14, 0, 0);
    printf("  along: in KEYS mode with the transport playing, the keys play along\n");
}

static void t_clock(void)
{
    int k;
    fresh();
    run(990, 0);                                        /* (P1.3i: a clock follows its START at once, as a DAW's does;
                                                         * a START left alone for 0.5 s is given up) */
    qd_app_midi(&A, 0xFA, 0, 0);                        /* START */
    for (k = 0; k < 24 * 12; k++) {                     /* 12 beats at 140 BPM: a pulse every 17.857 ms */
        const uint32_t at = 1000u + (uint32_t)(k * 60000.0 / 140.0 / 24.0 + 0.5);
        if (now < at)
            run(at - now, 0);
        qd_app_midi(&A, 0xF8, 0, 0);
    }
    run(5, 0);
    CHECK(A.seq.playing && A.seq.ext && A.playing && A.bpm == 140, "MIDI START + clock at 140: playing at %d BPM",
          A.bpm);
    CHECK(A.beats > 11.5 && A.beats < 12.5, "the transport followed: %.2f beats after 12 of the clock", A.beats);
    qd_app_midi(&A, 0xFC, 0, 0);                        /* STOP */
    run(5, 0);
    CHECK(!A.seq.playing && !A.playing && seq_voices() == 0, "MIDI STOP: stopped, released");
    printf("  clock: MIDI START and 24 PPQN at 140 BPM drive the transport; STOP stops\n");
}

/* the frame has no colour: every pixel grey (RGB565 big-endian; r, g, b within a step of each other) */
static int frame_grey(void)
{
    static uint16_t lcd[QD_W * QD_H];
    int i;
    qd_app_frame(&A, lcd);
    for (i = 0; i < QD_W * QD_H; i++) {
        const unsigned px = (unsigned)(lcd[i] >> 8 | (lcd[i] & 0xffu) << 8), r = px >> 11, g = (px >> 5 & 63u) / 2u,
                       b = px & 31u;
        if (r > g + 1u || g > r + 1u || r > b + 1u || b > r + 1u)
            return 0;
    }
    return 1;
}

/* P1.3k..P1.3o: FIZZ's idle screen (the logo: champagne bubbles close up): it moves with the clock, the
 * same clock draws the same frame, it has no colour (P1.3m..o), and a running transport (a DAW's MIDI clock, the screen
 * staying idle) adds the beats' spray */
static void t_fizz(void)
{
    uint64_t a1, a2, b1, s1, p1;
    int k, grey = 1;
    fresh();
    run(2000, 0);
    a1 = frame_hash();
    run(100, 0);
    b1 = frame_hash();
    fresh();
    run(2000, 0);
    a2 = frame_hash();
    CHECK(A.ui.mode == QD_MODE_IDLE && a1 == a2 && a1 != b1, "the idle screen: the same at the same time after "
          "power-on, moving 0.1 s later");
    fresh();
    for (k = 0; k < 23; k++) {                          /* the whole loop (11 s), every 0.5 s: no colour */
        run(500, 0);
        grey &= frame_grey();
    }
    CHECK(A.ui.mode == QD_MODE_IDLE && grey, "the idle screen: no colour over its whole loop (outlined, grey on black)");
    fresh();                                            /* P1.3o: seamless, 11 s a loop */
    run(1500, 0);
    a1 = frame_hash();
    run(11000, 0);
    a2 = frame_hash();
    CHECK(A.ui.mode == QD_MODE_IDLE && a1 == a2, "the idle screen loops seamlessly: 1.5 s and 12.5 s the same");
    fresh();
    run(3000, 0);
    s1 = frame_hash();                                  /* stopped at 3 s */
    fresh();
    run(990, 0);
    qd_app_midi(&A, 0xFA, 0, 0);                        /* START, then a clock at 120 BPM from 1 s */
    for (k = 0; k < 96; k++) {
        const uint32_t at = 1000u + (uint32_t)(k * 60000.0 / 120.0 / 24.0 + 0.5);
        if (now < at)
            run(at - now, 0);
        qd_app_midi(&A, 0xF8, 0, 0);
    }
    if (now < 3000)
        run(3000 - now, 0);
    p1 = frame_hash();
    CHECK(A.ui.mode == QD_MODE_IDLE && A.playing && p1 != s1 && frame_grey(), "playing (MIDI clock, still idle): "
          "the beats' surge changes the screen, still no colour");
    qd_app_midi(&A, 0xFC, 0, 0);
    printf("  fizz: the logo screen: deterministic, moving, no colour, seamless, surging with a running transport\n");
}

static uint64_t session_seq(void)
{
    int k;
    fresh();
    tap_button(QD_B_PLAY);
    run(700, 0);
    qd_app_button(&A, QD_B_MIX, 1);
    tap_key(WK[2]);
    qd_app_button(&A, QD_B_MIX, 0);
    tap_button(QD_B_REC);
    qd_app_key(&A, 9, 1, 100);
    run(4500, 0);
    qd_app_key(&A, 9, 0, 0);
    tap_button(QD_B_PLAY);
    qd_app_midi(&A, 0xFA, 0, 0);
    for (k = 0; k < 48; k++) {
        qd_app_midi(&A, 0xF8, 0, 0);
        run(21, 0);
    }
    qd_app_midi(&A, 0xFC, 0, 0);
    run(1500, 0);
    return hash;
}

static uint64_t session(void)
{
    fresh();
    qd_app_key(&A, 9, 1, 100);
    run(300, 0);
    qd_app_knob(&A, QD_K_PRESETS, -3);
    qd_app_midi(&A, 0x91, 40, 100);
    qd_app_midi(&A, 0x95, 64, 80);
    run(700, 0);
    qd_app_key(&A, 9, 0, 0);
    qd_app_midi(&A, 0x81, 40, 0);
    run(2000, 0);
    return hash;
}

int main(void)
{
    uint64_t h1, h2;
    if (!qd_init()) {
        printf("test_app: qd_init failed\n");
        return 1;
    }
    t_keys();
    t_presets();
    t_screens();
    t_midi();
    t_mix();
    t_limiter();
    t_panic();
    h1 = session();
    h2 = session();
    CHECK(h1 == h2, "two runs of one session differ");
    printf("  determinism: one session twice, the same samples (%016llx)\n", (unsigned long long)h1);
    t_play();
    t_patterns();
    t_tracks();
    t_along();
    t_clock();
    t_fizz();
    h1 = session_seq();
    h2 = session_seq();
    CHECK(h1 == h2, "two runs of the sequencer's session differ");
    printf("  determinism: the sequencer's session twice, the same samples (%016llx)\n", (unsigned long long)h1);
    qd_shutdown();
    printf("test_app: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
