/* SPDX-License-Identifier: GPL-3.0-only */
/* The app with its sound (P1.1), driven as the glue drives it (core/app/app.h: input events, a tick a millisecond,
 * the audio in 128-frame blocks as they fall due):
 *   keys        the keybed plays the selected track (velocity, OCT- / OCT+ -2..+2), TRACKS + white key 1..8 selects
 *               a track and plays nothing, the note a key struck is released at its release whatever changed since
 *   presets     PRESETS steps the track's sound (INIT .. NOISE-ISH, clamped), the picker takes the screen and leaves
 *               1.12 s after the last detent; a voice sounding keeps its patch
 *   screens     KEYS toggles the KEYS screen (its LED lit), HOME goes back to idle; TRACKS held dims the selected
 *               track's white key; the C keys faint in KEYS; a note sounding lights its key
 *   midi        notes on channels 1..8 play tracks 1..8; note off and velocity 0 release; channels 9..16 ignored
 *   mix         a voice panned hard left with no sends: nothing on the right; level 0: silence
 *   limiter     16 voices of SQUARE LEAD at velocity 127, every level and send at the top, MASTER full: no sample
 *               above Output Clean's ceiling (CLN_T, 26000 at MASTER full) and the limiter at work; then released:
 *               exact digital silence (the integer chain has no NaN and no denormal tail: it ends in zeros, the
 *               voices free, the effects off)
 *   panic       OCT- and OCT+ together: every voice gone within two blocks
 *   determinism the same session twice: the same samples
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
    fresh();
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
    fresh();
    qd_app_key(&A, 7, 1, 100);                          /* a note on track 1 */
    run(20, 0);
    for (i = 0; i < A.pool.nvoice; i++)
        if (qd_pool_sounding(&A.pool, i))
            v = i;
    memcpy(before, A.model.t[0].patch, sizeof before);
    qd_app_knob(&A, QD_K_PRESETS, 2);                   /* MARIMBA (13) -> FEEDBACK (15) */
    run(10, 0);
    CHECK(A.model.t[0].preset == 15 && !strcmp(qd_track_sound_name(&A.model.t[0]), "FEEDBACK") && A.picker,
          "PRESETS +2: track 1's sound %d (%s), picker %d", A.model.t[0].preset, qd_track_sound_name(&A.model.t[0]), A.picker);
    CHECK(v >= 0 && !memcmp(A.pool.v[v].q.patch, before, sizeof before), "the sounding voice keeps its patch");
    qd_app_knob(&A, QD_K_PRESETS, 9);
    CHECK(A.model.t[0].preset == 16, "PRESETS clamps at the last sound");
    qd_app_knob(&A, QD_K_PRESETS, -40);
    CHECK(A.model.t[0].preset == 0 && !strcmp(qd_track_sound_name(&A.model.t[0]), "INIT"), "PRESETS clamps at INIT");
    run(1100, 0);
    CHECK(A.picker, "the picker still up 1.1 s after the last detent (its fade)");
    run(30, 0);
    CHECK(!A.picker, "the picker gone 1.13 s after the last detent");
    qd_app_key(&A, 7, 0, 0);
    printf("  presets: PRESETS steps INIT .. NOISE-ISH (clamped); the picker leaves 1.12 s after; voices keep theirs\n");
}

static void t_screens(void)
{
    qd_leds_t l;
    fresh();
    tap_button(QD_B_KEYS);
    qd_app_leds(&A, &l);
    CHECK(A.screen == QD_SCR_KEYS && l.button[QD_B_KEYS] == QD_LED_LIT, "KEYS: the KEYS screen, its LED lit");
    CHECK(l.key[7] == QD_LED_FAINT && l.key[19] == QD_LED_FAINT && !l.key[8], "KEYS: the C keys faint");
    qd_app_midi(&A, 0x90, 64, 100);                     /* E4 from MIDI on track 1 (selected): its key lit */
    run(10, 0);
    qd_app_leds(&A, &l);
    CHECK(l.key[11] == QD_LED_LIT, "a note sounding on the track lights its key (%d)", l.key[11]);
    qd_app_midi(&A, 0x80, 64, 0);
    tap_button(QD_B_KEYS);
    CHECK(A.screen == QD_SCR_IDLE, "KEYS again: idle");
    tap_button(QD_B_KEYS);
    tap_button(QD_B_HOME);
    qd_app_leds(&A, &l);
    CHECK(A.screen == QD_SCR_IDLE && l.button[QD_B_KEYS] == QD_LED_OFF, "HOME: idle, KEYS's LED off");
    printf("  screens: KEYS toggles (lit), HOME idles; C keys faint in KEYS; sounding notes light their keys\n");
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
    fresh();
    for (k = 0; k < 10; k++)
        qd_app_midi(&A, 0x90 | (k % 8), 50 + k, 100);
    run(50, 0);
    qd_app_button(&A, QD_B_OCTDN, 1);
    qd_app_button(&A, QD_B_OCTUP, 1);
    run(10, 0);                                         /* (the fade's block, then free) */
    CHECK(qd_pool_in_use(&A.pool) == 0 && A.oct == -1, "PANIC: voices %d, octave %d", qd_pool_in_use(&A.pool), A.oct);
    printf("  panic: OCT- and OCT+ together: every voice gone at once (faded over a block)\n");
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
    qd_shutdown();
    printf("test_app: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
