/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
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
#include <stddef.h>
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
static double app_beats(void) { return (double)A.beat + (double)A.beat_rem / (double)QD_BEAT_TICKS; }   /* (P2.4) */
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
    A.ui.view = QD_VIEW_ROLL;   /* (P1.5s: EDIT is the power-on view; made on the PIANO ROLL) */
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
        CHECK(l.key[4] >= QD_LED_DIM, "TRACKS held: the selected track's white key at least dim (%d)", l.key[4]);
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
    qd_app_knob(&A, QD_K_PRESETS, 1);                   /* P1.5t: the library browser, on MARIMBA (preset 14) .. */
    run(300, 0);                                        /* (no acceleration) */
    qd_app_knob(&A, QD_K_PRESETS, 2);                   /* .. -> FEEDBACK (16), the track plays it */
    run(10, 0);
    CHECK(A.ui.latch == QD_LATCH_BROWSE && A.model.t[0].preset == 16 &&
              !strcmp(qd_track_sound_name(&A.model.t[0]), "FEEDBACK"),
          "PRESETS: the browser; +2: track 1 plays %d (%s)", A.model.t[0].preset, qd_track_sound_name(&A.model.t[0]));
    CHECK(v >= 0 && !memcmp(A.pool.v[v].q.patch, before, sizeof before), "the sounding voice keeps its patch");
    qd_app_knob(&A, QD_K_PRESETS, 40);
    CHECK(!strcmp(qd_track_sound_name(&A.model.t[0]), "OPEN HAT"), "PRESETS clamps at the last preset");
    qd_app_knob(&A, QD_K_PRESETS, -60);
    CHECK(!strcmp(qd_track_sound_name(&A.model.t[0]), "INIT"), "PRESETS clamps at INIT");
    qd_app_button(&A, QD_B_OCTDN, 1);
    run(20, 0);
    qd_app_button(&A, QD_B_OCTDN, 0);
    run(20, 0);
    CHECK(!A.ui.latch && A.ui.mode == QD_MODE_KEYS && !memcmp(A.model.t[0].patch, before, sizeof before) &&
              A.ui.tk == QD_TK_BACK, "F1: cancelled, its own sound back, the keyboard again");
    qd_app_key(&A, 7, 0, 0);
    printf("  presets: PRESETS browses the library, INIT .. OPEN HAT (clamped); F1 back; voices keep theirs\n");
}

static void t_screens(void)
{
    qd_leds_t l;
    fresh();
    tap_button(QD_B_REC);
    qd_app_leds(&A, &l);
    CHECK(A.ui.mode == QD_MODE_KEYS && l.button[QD_B_REC] == QD_LED_DIM, "REC: the keyboard, REC's LED dim (P1.6g: lit "
          "while recording)");
    CHECK(l.key[7] == QD_LED_80 && l.key[19] == QD_LED_80 && l.key[8] == QD_LED_HALF,
          "the keyboard: the scale lit (chromatic on C: the C keys 80 %%, the rest 50 %%; P1.5b)");
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
    /* P1.6g: channel 16 the selected track (notes and controllers); a track's MIDI CH set by hand: that channel */
    qd_ctl_select_track(&A, 2);
    qd_app_midi(&A, 0x9F, 64, 90);
    qd_app_midi(&A, 0xBF, 1, 99);
    run(10, 0);
    CHECK(sounding(2, 64) && A.model.t[2].owner.mw == 99 && A.model.t[0].owner.mw != 99,
          "channel 16: the selected track (3): its note, its MOD WHEEL");
    qd_ctl_select_track(&A, 5);                         /* (selected another while the note sounds: its OFF still goes
                                                         * to track 3) */
    qd_app_midi(&A, 0x8F, 64, 0);
    run(5, 0);
    CHECK(!sounding(2, 64), "its note off: to the track it struck");
    qd_proj_pat(&A.proj, A.seq.cur)->trk[6].chan = 9;              /* track 7 on channel 10 by hand */
    qd_app_midi(&A, 0x99, 55, 90);
    qd_app_midi(&A, 0x96, 57, 90);
    run(10, 0);
    CHECK(sounding(6, 55) && !sounding(6, 57), "track 7 set to channel 10: channel 10 plays it, channel 7 no more");
    qd_app_midi(&A, 0x89, 55, 0);
    qd_app_midi(&A, 0x86, 57, 0);
    printf("  midi: channels 1..8 play tracks 1..8 (their MIDI CH), channel 16 the selected track\n");
}

/* P1.6c: MIDI in through the track's group, CC 123 its notes off, recording pre-FX by design (ctl_rec takes the
 * played note); the groups' edits an undo level */
static void t_mfx(void)
{
    qd_pattern_t *P;
    fresh();
    run(5, 0);
    P = qd_proj_pat(&A.proj, A.seq.cur);
    P->trk[4].mfx = 2;
    qd_mfx_slot_init(&A.proj.mfx[1].slot[0], QD_MFX_TRANSPOSE);
    A.proj.mfx[1].slot[0].v[QD_MT_SEMI] = 7, A.proj.mfx[1].slot[0].v[QD_MT_OCT] = 0;
    qd_mfx_slot_init(&A.proj.mfx[1].slot[1], QD_MFX_HARM);
    qd_app_midi(&A, 0x94, 60, 90);                      /* channel 5: track 5, through group 2 */
    run(5, 0);
    CHECK(sounding(4, 67) && sounding(4, 74) && sounding(4, 79) && !sounding(4, 60), "MIDI in through track 5's group 2: "
          "TRANSPOSE +7, then HARM (+7, +12): 67 74 79");
    qd_app_midi(&A, 0x84, 60, 0);
    run(5, 0);
    CHECK(!voices_of(4), "its note off: all three released");
    A.proj.mfx[1].slot[1].v[QD_MH_N2] = 0, A.proj.mfx[1].slot[1].v[QD_MH_N3] = 0;
    qd_mfx_slot_init(&A.proj.mfx[1].slot[0], QD_MFX_ARP);
    A.proj.mfx[1].slot[0].v[QD_MA_MODE] = QD_AM_HOLD;
    qd_app_midi(&A, 0x94, 60, 90);
    qd_app_midi(&A, 0x84, 60, 0);
    run(400, 0);
    CHECK(qd_mfx_arp_playing(&A.player.mfx, 2), "an ARP latched (HOLD) on MIDI in, stopped: it plays on the tempo");
    qd_app_midi(&A, 0xB4, 123, 0);                      /* ALL NOTES OFF on channel 5 */
    run(300, 0);
    CHECK(!voices_of(4) && !qd_mfx_busy(&A.player.mfx, 2), "ALL NOTES OFF: the group's notes out too");
    /* undo: a group's edit */
    run(QD_UNDO_QUIET_MS + 10, 0);
    qd_undo_input(&A, 0);
    qd_mfx_slot_init(&A.proj.mfx[3].slot[5], QD_MFX_CHORD);
    qd_undo_close(&A);
    qd_undo_step(&A, -1);
    CHECK(A.proj.mfx[3].slot[5].type == QD_MFX_NONE, "undo: the CHORD added to group 4 gone");
    qd_undo_step(&A, 1);
    CHECK(A.proj.mfx[3].slot[5].type == QD_MFX_CHORD, "redo: back");
    printf("  mfx: MIDI in through a group, ALL NOTES OFF, undo\n");
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
    CHECK(qd_pool_in_use(&A.pool) == (A.pool.nvoice < 16 ? A.pool.nvoice : 16), "16 notes on %d voices: all in use (%d)",
          A.pool.nvoice, qd_pool_in_use(&A.pool));
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

/* P2.1: the frame strip by strip (qd_app_frame_strips: the device's LCD, no whole frame in RAM): the same pixels as
 * the whole frame, the strips in order, covering the screen */
static uint16_t strip_lcd[QD_W * QD_H];
static int strip_next, strip_bad;
static void strip_out(void *ctx, int y0, int rows, const uint16_t *px)
{
    (void)ctx;
    strip_bad += y0 != strip_next || rows < 1;
    memcpy(strip_lcd + (size_t)y0 * QD_W, px, (size_t)rows * QD_W * sizeof px[0]);
    strip_next = y0 + rows;
}
static void t_strips(void)
{
    static uint16_t lcd[QD_W * QD_H], strip[QD_W * QD_STRIP_H];
    int i, n = 0;
    fresh();
    run(500, 1);
    qd_app_frame(&A, lcd);
    strip_next = strip_bad = 0;
    qd_app_frame_strips(&A, strip, strip_out, 0);
    for (i = 0; i < QD_W * QD_H; i++)
        n += lcd[i] != strip_lcd[i];
    CHECK(!n && !strip_bad && strip_next == QD_H, "P2.1: the frame in strips of %d rows out one by one: the whole "
          "frame's pixels (%d differ)", QD_STRIP_H, n);
    memset(strip_lcd, 0, sizeof strip_lcd);
    strip_next = strip_bad = n = 0;
    qd_app_frame_strips(&A, 0, strip_out, 0);           /* (P2.1b: in place, in the core's own strip surface) */
    for (i = 0; i < QD_W * QD_H; i++)
        n += lcd[i] != strip_lcd[i];
    CHECK(!n && !strip_bad && strip_next == QD_H, "P2.1b: .. in place, in the core's own strip: the same (%d differ)",
          n);
}

#ifdef QD_GFX_FB
/* P2.6f: the 8-bit frame drawn a few bands at a time, the clock and the panel moving between: the frame is the instant
 * its begin took (the same as drawn at once there) */
static void t_fb_steps(void)
{
    static uint8_t f1[QD_W * QD_H], f2[QD_W * QD_H];
    static uint8_t *rows[QD_H];
    uint32_t bands;
    int y, left, steps = 0, n = 0;
    for (y = 0; y < QD_H; y++)
        rows[y] = f1 + (size_t)y * QD_W;
    fresh();
    qd_app_fb_attach(&A, rows);
    run(700, 1);
    qd_app_play(&A);
    run(300, 1);
    qd_app_frame_fb_begin(&A, &bands);
    do {
        left = qd_app_frame_fb_step(&A, 2);
        steps++;
        for (y = 0; y < 9; y++) {                       /* (time passes, the music moves, between the steps: no
                                                         * other frame drawn) */
            qd_app_tick(&A, ++now);
            qd_app_audio(&A, last, 64);
        }
    } while (left);
    for (y = 0; y < QD_H; y++)
        rows[y] = f2 + (size_t)y * QD_W;
    fresh();
    qd_app_fb_attach(&A, rows);
    run(700, 1);
    qd_app_play(&A);
    run(300, 1);
    qd_app_frame_fb(&A, &bands);
    for (y = 0; y < QD_W * QD_H; y++)
        n += f1[y] != f2[y];
    CHECK(bands && steps == 8 && !n, "P2.6f: the frame two bands a step (%d steps), time passing between: the begin's "
          "instant (%d pixels differ from it drawn at once)", steps, n);
}
#endif

#ifdef QD_GFX_FB
/* P2.6f: the changed bands only, frame after frame on the same frame (the port's way), against every band drawn from
 * the same instant: the same pixels. Screens: the idle logo, EDIT and the PIANO ROLL playing the demo, a held step,
 * the keyboard, the TAP ape, the Geek Out pages */
static uint8_t fbA[QD_W * QD_H], fbB[QD_W * QD_H];
static uint8_t *rowsA[QD_H], *rowsB[QD_H];
static int fb_bad_frames, fb_frames, fb_first_bad = -1, fb_bad_px, fb_bad_y0 = 999, fb_bad_y1 = -1;
static void fb_check(void)                              /* (one instant: its list; every band drawn from it into B, the
                                                         * changed ones also into A, which keeps the others from before) */
{
    uint32_t m;
    int i, k, n = 0;
    qd_app_frame_fb_begin(&A, &m);
    A.fb_pending = (1u << ((QD_H + QD_STRIP_H - 1) / QD_STRIP_H)) - 1u;
    qd_fb_attach(rowsB);
    qd_app_frame_fb_step(&A, 32);
    for (k = 0; k < 15; k++)
        if ((m >> k) & 1u)
            memcpy(fbA + k * 16 * QD_W, fbB + k * 16 * QD_W, 16 * QD_W);
    for (i = 0; i < QD_W * QD_H; i++)
        if (fbA[i] != fbB[i]) {
            n++;
            fb_bad_y0 = i / QD_W < fb_bad_y0 ? i / QD_W : fb_bad_y0;
            fb_bad_y1 = i / QD_W > fb_bad_y1 ? i / QD_W : fb_bad_y1;
        }
    fb_frames++;
    if (n && getenv("FBDBG") && fb_bad_frames < 3) {
        int b;
        printf("    frame at %u ms: drawn bands %05x; differing bands:", (unsigned)now, (unsigned)m);
        for (b = 0; b < 15; b++) {
            int c = 0, x;
            for (x = b * 16 * QD_W; x < (b + 1) * 16 * QD_W; x++)
                c += fbA[x] != fbB[x];
            if (c)
                printf(" %d(%d)", b, c);
        }
        printf("; screen %d view %d\n", qd_view_screen(&A), A.ui.view);
    }
    if (n) {
        fb_bad_frames++, fb_bad_px += n;
        if (fb_first_bad < 0)
            fb_first_bad = (int)now;
        memcpy(fbA, fbB, sizeof fbA);                   /* (from the truth again: each frame's own error counted) */
    }
}
static void fb_run(uint32_t ms)                         /* ms of the clock and the audio, a checked frame every 33 */
{
    while (ms--) {
        qd_app_tick(&A, ++now);
        while (frames_done + 128u <= (uint64_t)now * QD_FS / 1000u) {
            qd_app_audio(&A, last, 128);
            frames_done += 128u;
        }
        if (now % 33u == 0u)
            fb_check();
    }
}
static void t_fb_bands(void)
{
    int y, k;
    for (y = 0; y < QD_H; y++)
        rowsA[y] = fbA + (size_t)y * QD_W, rowsB[y] = fbB + (size_t)y * QD_W;
    fresh();
    qd_app_fb_attach(&A, rowsB);
    fb_run(1500);                                       /* the idle logo */
    tap_button(QD_B_HOME);
    fb_run(500);                                        /* EDIT */
    qd_app_play(&A);
    fb_run(3000);                                       /* .. playing the demo */
    qd_app_show(&A, QD_MODE_STEP, QD_VIEW_ROLL);
    fb_run(2000);                                       /* the PIANO ROLL */
    qd_app_key(&A, QD_WHITE_KEY[2], 1, 100);
    fb_run(600);                                        /* a step held */
    qd_app_key(&A, QD_WHITE_KEY[2], 0, 0);
    fb_run(600);
    tap_button(QD_B_REC);
    for (k = 0; k < 6; k++) {                           /* the keyboard played */
        qd_app_key(&A, QD_WHITE_KEY[k], 1, 100);
        fb_run(120);
        qd_app_key(&A, QD_WHITE_KEY[k], 0, 0);
        fb_run(80);
    }
    tap_button(QD_B_HOME);
    for (k = 0; k < 5; k++) {                           /* SHIFT + B6: the TAP ape */
        qd_app_button(&A, QD_B_FUNC, 1);
        fb_run(20);
        qd_app_key(&A, QD_BLACK_KEY[5], 1, 100);
        fb_run(20);
        qd_app_key(&A, QD_BLACK_KEY[5], 0, 0);
        qd_app_button(&A, QD_B_FUNC, 0);
        fb_run(460);
    }
    fb_run(2500);
    for (k = QD_VIEW_METERS; k <= QD_VIEW_SCOPE; k++) {   /* the Geek Out pages */
        qd_app_show(&A, QD_MODE_STEP, k);
        fb_run(700);
    }
    CHECK(fb_frames > 300 && !fb_bad_frames, "P2.6f: the changed bands only, frame after frame (%d frames): the same "
          "as every band drawn (%d frames differ, %d pixels, rows %d..%d, the first at %d ms)", fb_frames,
          fb_bad_frames, fb_bad_px, fb_bad_y0, fb_bad_y1, fb_first_bad);
}
#endif

/* P2.1b: shapes the strips' culling must leave exact (plutovg-rasterize.c cull_runs): a zigzag across every strip, a
 * closed quad, a square wave whose long lines run straight on through a vertex, a dashed one, curves, a fill, all
 * drawn whole and in the device's strips (the host: its surface can hold the whole screen) */
#if QD_STRIP_H >= QD_H
static void strip_shapes(void)
{
    static const float DASH[2] = {3.f, 2.f};
    int i;
    qd_path_begin();
    qd_move_to(10, 5);
    for (i = 1; i < 30; i++)
        qd_line_to(10.f + (float)i * 7.f, (i % 2) ? 230.f : 5.f);
    qd_path_stroke(0xFFFFFFu, 3.f);
    qd_path_begin();
    qd_move_to(20, 20), qd_line_to(200, 30), qd_line_to(210, 200), qd_line_to(30, 210), qd_path_close();
    qd_path_stroke(0x80FF40u, 4.f);
    qd_path_begin();
    qd_move_to(52, 92);
    for (i = 1; i < 60; i++)
        qd_line_to(52.f + (float)i * 2.9333f, (i / 15) % 2 ? 46.f + (float)(i % 3) * 0.01f : 138.f);
    qd_path_stroke(0x40C0FFu, 2.f);
    qd_dash(DASH, 2);
    qd_path_begin();
    qd_move_to(5, 120);
    for (i = 1; i < 12; i++)
        qd_line_to(5.f + (float)i * 20.f, 120.f + ((i % 2) ? 100.f : -100.f));
    qd_path_stroke(0xFF4080u, 1.5f);
    qd_dash(0, 0);
    qd_path_begin();
    qd_move_to(30, 230), qd_cubic_to(80, -40, 160, 280, 220, 10);
    qd_path_stroke(0xFFFF00u, 5.f);
    qd_path_begin();
    qd_move_to(100, 60), qd_cubic_to(200, 60, 200, 180, 100, 180), qd_cubic_to(0, 180, 0, 60, 100, 60);
    qd_path_fill(0x2040A0u);
}
static void t_strip_shapes(void)
{
    static uint16_t whole[QD_W * QD_H], strips[QD_W * QD_H];
    int y, i, n = 0;
    qd_set_strip_rows(QD_H);
    qd_begin_strip(0), strip_shapes(), qd_end_strip(whole);
    qd_set_strip_rows(QD_STRIP_DEV);
    for (y = 0; y < QD_H; y += QD_STRIP_DEV)
        qd_begin_strip(y), strip_shapes(), qd_end_strip(strips);
    qd_set_strip_rows(QD_STRIP_H);
    for (i = 0; i < QD_W * QD_H; i++)
        n += whole[i] != strips[i];
    CHECK(!n, "P2.1b: strokes (a zigzag, a closed quad, long straight runs, dashes, curves) and a fill, whole and in "
          "%d-row strips: the same (%d differ)", QD_STRIP_DEV, n);
}
#else
static void t_strip_shapes(void) {}
#endif

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
    CHECK(app_beats() > 1.8 && app_beats() < 2.2, "the transport moved 2 beats in 1.2 s at 100 BPM (%f)", app_beats());
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
                                                         * (P1.3x: released at 20 ms: the transport starts at the next
                                                         * block after the release) */
    while (A.seq.cur == 1 && now < 10000)
        run(1, 0);
    CHECK(A.seq.cur == 0 && qd_seq_queued(&A.seq) < 0 && now >= 4815 && now <= 4830,
          "A01 takes over at the end of A02's master length (at %u ms; PLAY released at 20)", now);
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
    qd_app_button(&A, QD_B_FUNC, 1);                    /* (P1.6d: FILL is SHIFT + ARP held) */
    qd_app_button(&A, QD_B_KEYS, 1);
    CHECK(A.seq.fill, "SHIFT + ARP held: FILL");
    qd_app_button(&A, QD_B_KEYS, 0);
    qd_app_button(&A, QD_B_FUNC, 0);
    CHECK(!A.seq.fill, "ARP released: no FILL");
    printf("  patterns: queued to the end of the master length (the name blinks), FUNC: the next 16th, stopped: at "
           "once; the LEDs\n");
}

/* P1.4b: the tracks screen of clocks is retired; FX tapped: the effects (FX lit), again their next page; ENV: the mix
 * (ENV lit; its meters move while playing), again the sends; SEL tapped: the track settings (SEL lit); SEL held + W1:
 * track 1 muted, it plays nothing; FX held + W4: track 4 */
static void t_tracks(void)
{
    qd_leds_t l;
    int k, n1 = 0;
    fresh();
    tap_button(QD_B_TRACKS);
    qd_app_leds(&A, &l);
    CHECK(A.ui.mode == QD_MODE_STEP && A.ui.latch == QD_LATCH_FX && qd_view_screen(&A) == QD_SCR_FX &&
              l.button[QD_B_TRACKS] == QD_LED_LIT && A.ui.fx_page == 0,
          "FX tapped: the effects, the OVERVIEW (FX lit)");
    tap_button(QD_B_TRACKS);
    CHECK(A.ui.fx_page == 1, "FX again: the next page (DELAY)");
    tap_button(QD_B_PAGE);
    qd_app_leds(&A, &l);
    CHECK(A.ui.latch == QD_LATCH_MIX && qd_view_screen(&A) == QD_SCR_MIX && l.button[QD_B_PAGE] == QD_LED_LIT &&
              !A.ui.mix_page, "ENV: the mix (ENV lit)");
    tap_button(QD_B_PLAY);
    run(300, 0);
    {
        uint64_t h0 = frame_hash();
        run(100, 0);
        CHECK(h0 != frame_hash(), "playing: the mix's meters move");
    }
    tap_button(QD_B_PAGE);
    CHECK(A.ui.mix_page == 1 && qd_view_screen(&A) == QD_SCR_MIX, "ENV again: the sends");
    qd_app_button(&A, QD_B_PATTERN, 1);                 /* SEL held + W1 (a tap): track 1 muted */
    run(300, 0);
    tap_key(WK[0]);
    qd_app_button(&A, QD_B_PATTERN, 0);
    run(10, 0);
    CHECK((A.seq.mute & 1u) && A.ui.latch == QD_LATCH_MIX, "SEL held + W1: track 1 muted, the mix still open");
    run(1000, 0);
    for (k = 0; k < A.pool.nvoice; k++)
        n1 += A.pool.v[k].track == 0 && qd_pool_sounding(&A.pool, k) && A.pool.v[k].q.gate;
    CHECK(n1 == 0, "the muted track plays nothing (%d held)", n1);
    tap_button(QD_B_PATTERN);
    qd_app_leds(&A, &l);
    CHECK(A.ui.latch == QD_LATCH_TRK && qd_view_screen(&A) == QD_SCR_MENU && l.button[QD_B_PATTERN] == QD_LED_LIT,
          "SEL tapped: the track settings (SEL lit)");
    tap_button(QD_B_HOME);
    CHECK(!A.ui.latch && A.ui.mode == QD_MODE_STEP, "HOME: back to the step view");
    qd_app_button(&A, QD_B_TRACKS, 1);
    tap_key(WK[3]);
    qd_app_button(&A, QD_B_TRACKS, 0);
    run(10, 0);
    CHECK(A.sel == 3 && !A.ui.latch, "FX held + white key 4: track 4, no page opened");
    tap_button(QD_B_PLAY);
    printf("  P1.4b: FX the effects, ENV the mix / sends, SEL the track settings; SEL + W mutes; FX + W selects\n");
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
    CHECK(app_beats() > 11.5 && app_beats() < 12.5, "the transport followed: %.2f beats after 12 of the clock", app_beats());
    qd_app_midi(&A, 0xFC, 0, 0);                        /* STOP */
    run(5, 0);
    CHECK(!A.seq.playing && !A.playing && seq_voices() == 0, "MIDI STOP: stopped, released");
    printf("  clock: MIDI START and 24 PPQN at 140 BPM drive the transport; STOP stops\n");
}

/* P1.3p: the word's solid pixels: the pixels of the band the word sits in (y 95 .. 145) drawn exactly in F's colour
 * (the knob blue, RGB565): the solid F fills dozens; its particles (tiny rings, anti-aliased) hardly any */
static int word_px(void)
{
    static uint16_t lcd[QD_W * QD_H];
    const unsigned blue = (0x3fu * 31u + 127u) / 255u << 11 | (0x7du * 63u + 127u) / 255u << 5 |   /* as qd_end */
                          (0xffu * 31u + 127u) / 255u;
    int x, y, n = 0;
    qd_app_frame(&A, lcd);
    for (y = 95; y < 145; y++)
        for (x = 0; x < QD_W; x++) {
            const unsigned px = (unsigned)(lcd[y * QD_W + x] >> 8 | (lcd[y * QD_W + x] & 0xffu) << 8);
            n += px == blue;
        }
    return n;
}

/* P1.3k..P1.3p: FIZZ's idle screen (the logo: champagne bubbles in the app's colours, the word from particles): it
 * moves with the clock, the same clock draws the same frame, it loops seamlessly, the word is solid only at its hold
 * (particles while it gathers and dissolves), and a running transport (a DAW's MIDI clock, the screen staying idle)
 * adds the beats' surge */
static void t_fizz(void)
{
    static const struct {
        float t;
        int solid, parts;
    } WP[] = {{2.0f, 0, 0}, {4.3f, 0, 1}, {6.5f, 1, 0}, {8.5f, 1, 0}, {9.4f, 0, 1}, {10.8f, 0, 0}};
    uint64_t a1, a2, b1, s1, p1;
    int k, ok = 1, n_gather, n_hold, n_fizz;
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
    fresh();                                            /* P1.3o: seamless, 11 s a loop */
    run(1500, 0);
    a1 = frame_hash();
    run(11000, 0);
    a2 = frame_hash();
    CHECK(A.ui.mode == QD_MODE_IDLE && a1 == a2, "the idle screen loops seamlessly: 1.5 s and 12.5 s the same");
    for (k = 0; k < (int)(sizeof WP / sizeof WP[0]); k++) {   /* P1.3p: the word's phases */
        float so, pa;
        ui_idle_word(WP[k].t, &so, &pa);
        ok &= (so >= 0.999f) == WP[k].solid && (so <= 0.001f) == !WP[k].solid && (pa > 0.f) == WP[k].parts;
        ui_idle_word(WP[k].t + 11.f, &so, &pa);
        ok &= (so >= 0.999f) == WP[k].solid && (pa > 0.f) == WP[k].parts;
    }
    CHECK(ok, "the word: nothing at 2 s, particles only gathering (4.3 s) and dissolving (9.4 s), solid only at its "
          "hold (6.5, 8.5 s), none by 10.8 s; every loop");
    fresh();
    run(4300, 0);
    n_gather = word_px();
    run(2200, 0);
    n_hold = word_px();
    run(2900, 0);
    n_fizz = word_px();
#ifdef QD_GFX_FB
#define WORD_STRAY 8                                    /* (P2.6e: the 8-bit frame: a nearly opaque particle is the
                                                         * ramp's top, the blue itself, more often) */
#else
#define WORD_STRAY 3
#endif
    CHECK(n_hold >= 40 && n_gather <= WORD_STRAY && n_fizz <= WORD_STRAY, "on the screen: the solid F at the hold (%d px in its blue), "
          "not while gathering (%d) or dissolving (%d)", n_hold, n_gather, n_fizz);
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
    CHECK(A.ui.mode == QD_MODE_IDLE && A.playing && p1 != s1, "playing (MIDI clock, still idle): the beats' surge "
          "changes the screen");
    qd_app_midi(&A, 0xFC, 0, 0);
    printf("  fizz: the logo screen: deterministic, moving, seamless, the word solid only at its hold (%d px), surging "
           "with a running transport\n", n_hold);
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

/* ---------------------------------------------------------------- P2.6: the block in three --- */
/* qd_app_block_begin / _voices / _end: three apps on one script (the demo playing, chords on two tracks over it, a
 * pattern change, the transport stopped and started): one through qd_app_audio, one with the halves B then A, one A
 * then B: every sample the same. And qd_app_block_voices writes nothing but its voices' own slots (the pool's, the
 * mix's): the whole app compared byte for byte around each call, and what the voices share outside it (the song
 * stub, the comb lines' holders) */
static qd_app_t S1, SNAP;
static uint32_t split_bad, split_calls;
static int split_ok_byte(size_t off, int v0, int v1)
{
    const size_t pv = offsetof(qd_app_t, pool) + offsetof(qd_pool_t, v), mx = offsetof(qd_app_t, mix);
    int i;
    for (i = v0; i < v1; i++) {
        const size_t a0 = pv + (size_t)i * sizeof(qd_pool_voice_t);
        if (off >= a0 && off < a0 + sizeof(qd_pool_voice_t))
            return 1;
        if (off >= mx + offsetof(qd_mix_t, sg) + (size_t)i * sizeof S1.mix.sg[0] &&
            off < mx + offsetof(qd_mix_t, sg) + (size_t)(i + 1) * sizeof S1.mix.sg[0])
            return 1;
        if (off >= mx + offsetof(qd_mix_t, serial) + (size_t)i * sizeof S1.mix.serial[0] &&
            off < mx + offsetof(qd_mix_t, serial) + (size_t)(i + 1) * sizeof S1.mix.serial[0])
            return 1;
        if (off >= mx + offsetof(qd_mix_t, dist) + (size_t)i * sizeof S1.mix.dist[0] &&
            off < mx + offsetof(qd_mix_t, dist) + (size_t)(i + 1) * sizeof S1.mix.dist[0])
            return 1;
    }
    return 0;
}
static void split_voices(qd_app_t *a, int v0, int v1, qd_bus_t *bus, int guard)
{
    uint32_t sig = 0;
    if (guard) {
        memcpy(&SNAP, a, sizeof SNAP);
        sig = qd_quad_shared_sig();
    }
    qd_app_block_voices(a, v0, v1, bus);
    if (guard) {
        const uint8_t *x = (const uint8_t *)(const void *)&SNAP, *y = (const uint8_t *)(const void *)a;
        size_t k;
        for (k = 0; k < sizeof SNAP; k++)
            if (x[k] != y[k] && !split_ok_byte(k, v0, v1)) {
                if (!split_bad)
                    printf("  (qd_app_block_voices %d..%d wrote the app's byte %lu)\n", v0, v1, (unsigned long)k);
                split_bad++;
            }
        if (sig != qd_quad_shared_sig()) {
            if (!split_bad)
                printf("  (qd_app_block_voices %d..%d wrote what the voices share)\n", v0, v1);
            split_bad++;
        }
        split_calls++;
    }
}
static void split_block(qd_app_t *a, int b_first, int16_t *out)
{
    qd_bus_t A, B;
    const int mid = a->pool.nvoice / 2;
    qd_app_block_begin(a);
    if (b_first) {
        split_voices(a, mid, a->pool.nvoice, &B, 1);
        split_voices(a, 0, mid, &A, 1);
    } else {
        split_voices(a, 0, mid, &A, 0);
        split_voices(a, mid, a->pool.nvoice, &B, 0);
    }
    qd_app_block_end(a, &A, &B, out);
}
/* one run of the script on S1 (the effects are the process's: the runs one after another, each from qd_app_init):
 * mode 0 qd_app_audio, 1 the halves B then A (guarded), 2 A then B; a hash of each block's samples into hs */
static uint32_t split_run(int mode, uint64_t *hs, uint32_t max, uint32_t *maxv, uint32_t *halfb)
{
    static const uint8_t CH[2][5] = {{48, 52, 55, 59, 62}, {60, 64, 67, 0, 0}};
    qd_app_t *a = &S1;
    int16_t o[2 * QD_MIX_CTL];
    uint32_t ms, done = 0, blocks = 0;
    int j, t;
    qd_app_init(a);
    qd_app_tick(a, 0);
    qd_app_play(a);
    for (ms = 1; ms <= 9000u; ms++) {
        if (ms % 3000u == 300u || ms % 3000u == 1700u)  /* chords on tracks 1 and 4, struck and released */
            for (j = 0; j < 5; j++)
                for (t = 0; t < 2; t++) {
                    if (!CH[t][j])
                        continue;
                    if (ms % 3000u == 300u)
                        qd_player_key_on(&a->player, t * 3, CH[t][j], 100 - 10 * t);
                    else
                        qd_player_key_off(&a->player, t * 3, CH[t][j]);
                }
        if (ms == 4000u)
            qd_seq_select(&a->seq, 0, 1);               /* A01 at the next 16th */
        if (ms == 7000u)
            qd_app_stop(a);
        if (ms == 7600u)
            qd_app_play(a);
        qd_app_tick(a, ms);
        while (done + QD_MIX_CTL <= (uint64_t)ms * QD_FS / 1000u && blocks < max) {
            uint64_t h = 14695981039346656037ull;
            if (mode == 0)
                qd_app_audio(a, o, QD_MIX_CTL);
            else
                split_block(a, mode == 1, o);
            for (j = 0; j < 2 * QD_MIX_CTL; j++)
                h = (h ^ (uint16_t)o[j]) * 1099511628211ull;
            hs[blocks++] = h;
            done += QD_MIX_CTL;
            if (maxv && (uint32_t)qd_pool_in_use(&a->pool) > *maxv)
                *maxv = (uint32_t)qd_pool_in_use(&a->pool);
            for (j = a->pool.nvoice / 2; halfb && j < a->pool.nvoice; j++)
                *halfb += qd_pool_used(&a->pool, j);
        }
    }
    return blocks;
}
static void t_split(void)
{
    static uint64_t h0[13000], h1[13000], h2[13000];
    uint32_t n0, n1, n2, k, differ = 0, maxv = 0, halfb = 0;
    split_bad = split_calls = 0;
    n0 = split_run(0, h0, 13000, &maxv, &halfb);
    n1 = split_run(1, h1, 13000, 0, 0);
    n2 = split_run(2, h2, 13000, 0, 0);
    for (k = 0; k < n0 && k < n1 && k < n2; k++)
        differ += h0[k] != h1[k] || h0[k] != h2[k];
    CHECK(!differ && n0 == n1 && n0 == n2 && n0 > 10000u, "P2.6: the block in three: %u blocks, the halves B then A "
          "and A then B the same samples as qd_app_audio (%u differ)", n0, differ);
    CHECK(!split_bad && split_calls > 20000u, "P2.6: qd_app_block_voices writes nothing but its voices' own slots (%u "
          "calls, %u bytes elsewhere)", split_calls, split_bad);
    CHECK(maxv >= 8u && halfb > 0u, "P2.6: voices in both halves (at most %u at once)", maxv);
    printf("  split: %u blocks the same three ways, %u guarded voice calls clean, up to %u voices\n", n0,
           split_calls, maxv);
}

/* P2.6: the demos at the unit's voices (8 on one core, 16 with two): the steals and the notes dropped */
static void t_busy(void)
{
    int k;
    uint32_t own, cross, drops, maxv = 0;
    fresh();
    tap_button(QD_B_PLAY);
    for (k = 0; k < 3; k++) {
        qd_seq_select(&A.seq, k, 0);
        run(20000, 0);
        if ((uint32_t)qd_pool_in_use(&A.pool) > maxv)
            maxv = (uint32_t)qd_pool_in_use(&A.pool);
    }
    tap_button(QD_B_PLAY);
    own = A.pool.steals_own, cross = A.pool.steals_cross, drops = A.pool.drops;
    CHECK(A.pool.nvoice >= 16 ? drops == 0 : drops * 100u <= A.pool.starts, "the demos at %d voices: %u notes dropped "
          "(at 8: only where more notes start in one block than there are voices; under 1 %%)", A.pool.nvoice, drops);
    printf("  busy: A01..A03 20 s each at %d voices: %u steals within a track, %u across, %u dropped, %u notes\n",
           A.pool.nvoice, own, cross, drops, A.pool.starts);
}

/* P2.6: ONE INSTANT A FRAME (the device's strips, the fast drawing: the frame recorded once, every strip from it). A
 * frame drawn while, between its strips, the clock runs on (9 ms a strip) and PLAY is pressed: the same pixels as that
 * frame drawn with nothing in between (the same session again). And the strips' hashes: a strip the mask says is
 * unchanged draws the last frame's very pixels */
static uint16_t LA[QD_W * QD_H], LB[QD_W * QD_H], LP[QD_W * QD_H];
static int inj_on, inj_k;
static void cap(void *ctx, int y0, int rows, const uint16_t *px)
{
    uint16_t *to = (uint16_t *)ctx;
    int k;
    memcpy(to + (size_t)y0 * QD_W, px, (size_t)rows * QD_W * sizeof *px);
    if (!inj_on)
        return;
    for (k = 0; k < 9; k++)                             /* (the platform between two strips: the clock, a press) */
        qd_app_tick(&A, ++now);
    if (inj_k++ == 1) {
        qd_app_button(&A, QD_B_PLAY, 1);
        qd_app_tick(&A, ++now);
        qd_app_button(&A, QD_B_PLAY, 0);
    }
}
static void t_instant(void)
{
    uint32_t nr0, nr1, nl, mb, m, f, unchanged = 0, bad = 0, drawn = 0;
    int k;
    if (QD_STRIP_H >= QD_H) {
        printf("  one instant a frame: (the host draws the whole screen at once)\n");
        return;
    }
    fresh();
    run(2500, 0);
    qd_dl_stats(&nr0, &nl, &mb);
    inj_on = 1, inj_k = 0;
    qd_app_frame_strips(&A, 0, cap, LA);                /* the clock and PLAY between its strips */
    inj_on = 0;
    qd_dl_stats(&nr1, &nl, &mb);
    if (nr1 == nr0) {
        printf("  one instant a frame: (this drawing draws its strips live: plutovg)\n");
        return;
    }
    qd_app_frame_strips(&A, 0, cap, LP);                /* (the next frame: the press and the 45 ms shown) */
    fresh();
    run(2500, 0);
    qd_app_frame_strips(&A, 0, cap, LB);                /* the same instant, nothing in between */
    CHECK(!memcmp(LA, LB, sizeof LA) && memcmp(LP, LB, sizeof LP),
          "P2.6: a frame drawn while the clock ran 45 ms and PLAY was pressed between its strips: the latched instant's "
          "pixels (%s), the next frame the new ones (%s)", memcmp(LA, LB, sizeof LA) ? "differ" : "the same",
          memcmp(LP, LB, sizeof LP) ? "moved" : "not moved");
    fresh();                                            /* the strips' hashes: the step view playing */
    qd_app_show(&A, QD_MODE_STEP, QD_VIEW_EDIT);
    qd_app_play(&A);
    run(500, 0);
    for (f = 0; f < 150u; f++) {
        run(33, 0);
        qd_app_frame_strips_changed(&A, &m);
        qd_app_frame_strips(&A, 0, cap, LB);
        for (k = 0; f && k < QD_H / QD_STRIP_H; k++) {
            if ((m >> k) & 1u) {
                drawn++;
                continue;
            }
            unchanged++;
            bad += memcmp(LB + (size_t)k * QD_STRIP_H * QD_W, LP + (size_t)k * QD_STRIP_H * QD_W,
                          (size_t)QD_STRIP_H * QD_W * sizeof LB[0]) != 0;
        }
        memcpy(LP, LB, sizeof LP);
    }
    CHECK(!bad && unchanged > 0, "P2.6: the strips' hashes: %u strips said unchanged, %u of them not the last frame's "
          "pixels (%u changed)", unchanged, bad, drawn);
    qd_dl_stats(&nr1, &nl, &mb);
    printf("  one instant a frame: strips drawn from one instant whatever comes between them; the step view playing: "
           "%u of %u strips unchanged (the largest list %u B)\n", unchanged, unchanged + drawn, mb);
}

int main(void)
{
    uint64_t h1, h2;
    if (!qd_init()) {
        printf("test_app: qd_init failed\n");
        return 1;
    }
    qd_text_stats_reset();                              /* P1.3v: the clarity guard (checked at the end) */
    t_keys();
    t_presets();
    t_screens();
    t_midi();
    t_mfx();
    t_mix();
    t_limiter();
    t_panic();
    h1 = session();
    h2 = session();
    CHECK(h1 == h2, "two runs of one session differ");
    printf("  determinism: one session twice, the same samples (%016llx)\n", (unsigned long long)h1);
    t_strips();
#ifdef QD_GFX_FB
    t_fb_steps();
    t_fb_bands();
#endif
    t_strip_shapes();
    t_play();
    t_patterns();
    t_tracks();
    t_along();
    t_clock();
    t_fizz();
    t_split();
    t_busy();
    t_instant();
    h1 = session_seq();
    h2 = session_seq();
    CHECK(h1 == h2, "two runs of the sequencer's session differ");
    printf("  determinism: the sequencer's session twice, the same samples (%016llx)\n", (unsigned long long)h1);
    {
        const char *small = "";
        const float mn = qd_text_stats_min(&small);
        CHECK(mn >= 11.f, "no text under 11 px in the frames drawn (the smallest: %.1f px, \"%s\")", (double)mn, small);
    }
    qd_shutdown();
    printf("test_app: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
