/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* QUADRILLE's app (PLAN.md §13): the panel's input in as events, the simulated clock, the transport, a frame drawn
 * through the screens (ui/), the LEDs and the audio out. It knows nothing of the core ABI (glue/ translates) and
 * calls nothing of the OS: all time is the milliseconds given to qd_app_tick, so two runs with the same input are
 * the same, frame for frame and sample for sample.
 *
 * P1.0: the idle screen. PLAY plays / stops; SELECT or KNOB 1 change the tempo (1 BPM a detent, 30..300); a held
 * key or button lights its LED; PLAY's green LED marks each beat while playing.
 * P1.1: the sound. Eight tracks (seq/model) of FM TONE voices from one pool (voice/pool), mixed per voice into
 * ChoralRoot's effects and output chain (voice/mix, voice/fx). The keybed plays the selected track (F3..G5, OCT- / OCT+
 * shift it -2..+2 octaves, the keys' velocity); MIDI notes on channels 1..8 play tracks 1..8. TRACKS (printed FX) held
 * + white key 1..8 selects a track; PRESETS steps the track's sound through INIT and the 16 factory sounds (the picker
 * takes the screen while it turns, and for a second after); KEYS (printed ARP) toggles the KEYS screen, HOME goes back
 * to the idle screen; MASTER is the volume; OCT- and OCT+ together: every voice faded out (a panic).
 * P1.2: the sequencer (seq/: the project of 64 patterns, the demos A01..A03 built in; seq/player puts its events into
 * the pool). It runs from the sample clock: each 32-frame audio block runs a block of the sequencer first. PLAY plays
 * the current pattern (A02 at power-on) from its top and stops it (the notes released, their tails ring); the dancers
 * follow the transport. PATTERN (printed SEL) held + white key 1..16: A01..A16, queued to the end of the master length,
 * at the next 16th with FUNC (printed GLO) held, at once when stopped; the idle screen's pattern blinks while one is
 * queued. TRACKS (printed FX) tapped: the tracks screen (line.html s3, the eight clocks); TRACKS again or HOME leaves.
 * On it white keys 1..8 select a track and 9..16 mute / unmute tracks 1..8 (no notes); TRACKS held + white key 1..8
 * selects anywhere. REC held: FILL (the FILL / NOT FILL conditions). MIDI clock in (24 PPQN, START / STOP / CONTINUE) drives the transport; MIDI notes keep playing
 * tracks 1..8. LEDs: PLAY's green on the beat while playing; on the tracks screen white keys 1..8 flash as their tracks
 * trigger (the selected one dim between), 9..16 lit for the tracks heard; PATTERN held: patterns with content dim, the
 * playing one lit, the queued one blinking.
 * P1.3: the unit is programmable. The panel's input goes to the controller (ctl.h: ctl.c, ctl_step.c, ctl_rec.c,
 * ctl_leds.c), which turns it into model edits and UI state (qd_ui_t, a->ui); the frame is view.c's (the model and the
 * UI state into the screens' view records). Power-on shows the idle screen; any key or button goes to the step view
 * (the home screen); two minutes untouched and stopped: idle again. The app keeps the clock, the transport, the sound
 * and the keybed's notes; the gestures, the screens and the LEDs are the controller's (PLAN §8, the header of ctl.h).
 * P1.3b: the sequencer controls version 2 (PLAN §8b, after the OMX-27 Form sequencer): track defaults and step
 * overrides, pages with their own start and end and a loop, the project's sound pool, scales, solo, copy / paste and
 * undo (seq/); the black keys choose the hold mode and set its ten values, OCT- / OCT+ are F1 / F2 (both F3), GLO is
 * SHIFT, REC switches the keybed between the sequencer and the keyboard, ARP held is FILL (ctl.h's header).
 * P1.3c (§8b version 2.1): five notes a step (NOTE's five knobs), the tracks on SEL held, the patterns on SEQ held (a
 * launch mode: END BAR BEAT NOW), F1's page layer and quick step copy / paste, F2's pick-up / drop (Form's), P-LOCK on
 * B11, F3's rate as a note value and a type (21 rates; QD_TPS 100800), the octave takeover and the OCT LEDs.
 * P1.3d (§8b version 2.2): the knobs take over the hold modes (and the track's defaults with nothing held), P-LOCK's
 * ALGORITHM chooses the track, LOCKS dropped (B10 reserved), banks on SEQ + B1..B4, the stopped preview (the held steps
 * sound: qd_ctl_preview_sync after every input and millisecond), F3 latched (OCT- exits), the step latch on LFO.
 * P1.3k: the product is FIZZ; the idle screen's dancers are gone. P1.3l: the idle screen is a glass filling up and
 * fizzing over (ui_idle.c, drawn from the clock and the beats). */
#pragma once
#include <stdint.h>

#include "../seq/model.h"
#include "../seq/pattern.h"
#include "../seq/player.h"
#include "../seq/qd_div.h"                    /* (P2.4: 32-bit divisions, qd_ratio_f) */
#include "../seq/seq.h"
#include "../voice/mix.h"
#include "../voice/pool.h"
#include "ctl.h"
#include "persist.h"
#include "undo.h"
#include "fizz_sx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_NKEY 27                     /* the note keys: 0 = F3 (MIDI 53) .. 26 = G5 */
#define QD_NOTE0 53
#define QD_FS 44100                    /* the audio's sample rate */
#define QD_BEAT_TICKS (4 * QD_TPS)            /* (P2.4) a beat, a quarter note, in ticks */
#define QD_BPM_MIN 30
#define QD_BPM_MAX 300
#define QD_METRO_LVL 60                /* P1.5c: the click's level, % (SHIFT + B7 held + a knob) */
#define QD_BPM_DEFAULT 122             /* line.html's BPM */
#define QD_BEAT_LED_MS 80              /* PLAY's green LED: lit this long from each beat */
#define QD_OCT_MIN (-2)
#define QD_OCT_MAX 2
#define QD_PICKER_MS 1000              /* the picker stays this long after the last detent .. */
#define QD_PICKER_FADE_MS 120          /* .. then fades back over this long (PLAN.md §9a rule 5) */
#ifndef QD_VOICES
#define QD_VOICES QD_POOL_DEFAULT      /* the pool's voices (a setting: up to QD_POOL_MAX for experiments) */
#endif
#define QD_MASTER_POT_DEFAULT 724      /* MASTER before the panel says (master_of_pot: 2047 of 4096, Felucca's) */

/* the buttons, by QUADRILLE's roles (PLAN.md §8), in the panel's order (printed FX, SEL, ENV, LFO, EDIT, GLO, HOME,
 * SAVE, ARP, SEQ, PLAY, REC, OCT-, OCT+). P1.3b (§8b): QD_B_FUNC is SHIFT, QD_B_KEYS (ARP) is FILL while held,
 * QD_B_OCTDN / QD_B_OCTUP are F1 / F2 */
enum { QD_B_TRACKS, QD_B_PATTERN, QD_B_PAGE, QD_B_MUTE, QD_B_SOUND, QD_B_FUNC, QD_B_HOME, QD_B_SAVE, QD_B_KEYS,
       QD_B_MIX, QD_B_PLAY, QD_B_REC, QD_B_OCTDN, QD_B_OCTUP, QD_NBTN };
/* the knobs (detents), in the panel's role order */
enum { QD_K_SELECT, QD_K_ALGO, QD_K_PRESETS, QD_K_1, QD_K_2, QD_K_3, QD_K_4, QD_NKNOB };
#define QD_PAT_START 1                 /* the pattern at power-on: A02 */
#define QD_TRIG_LED_MS 80              /* the tracks screen: a track's white key lit this long from its trig */

/* the LEDs as the app sees them: a level 0..255 per key, per button, and PLAY's green LED */
#define QD_LED_OFF 0
#define QD_LED_FAINT 12
#define QD_LED_DIM 40
#define QD_LED_MID 96
#define QD_LED_LIT 255
#define QD_LED_HALF 128                /* P1.5b: half (the latch in MODES; the scale's keys) */
#define QD_LED_80 204                  /* P1.5b: 80 % (the scale's roots) */
typedef struct qd_leds {
    uint8_t key[QD_NKEY];
    uint8_t button[QD_NBTN];
    uint8_t play_green;
} qd_leds_t;

/* P2.11: THE GEEK OUT PAGES' FIGURES. The platform's own (qd_app_stats: the port fills them as it measures them,
 * whenever it likes; the host and FM1VST what they have): a field whose QD_ST_* bit is not in have is drawn dim. The
 * counters run from the last reset: SHIFT + B11 on a Geek Out page bumps reset (the platform then zeroes its own) */
enum { QD_ST_LOAD0 = 1, QD_ST_LOAD1 = 2, QD_ST_FPS = 4, QD_ST_ISR = 8, QD_ST_LOCK = 16, QD_ST_RAM = 32,
       QD_ST_STORE = 64, QD_ST_BOOT = 128 };
typedef struct {
    uint32_t have;                     /* QD_ST_*: the fields filled */
    uint16_t load_pm[2];               /* the audio's load of core 0 and core 1, per mille */
    uint16_t fps10;                    /* frames a second x 10 .. */
    uint32_t frame_us;                 /* .. and the last frame's time (drawn and sent) */
    uint32_t isr_us, isr_budget_us;    /* the audio interrupt's longest half since the reset; a half's time */
    uint32_t late;                     /* halves late since the reset */
    uint32_t lock_us;                  /* the longest audio lock since the reset */
    uint32_t ram_used, ram_size;       /* bytes */
    uint32_t store_used, store_size;   /* the store's bytes */
    const char *boot;                  /* the last boot's cause after a crash ("WATCHDOG", "HARD FAULT" ..); 0 clean */
    uint32_t reset;                    /* the core's: bumped at each reset of the counters */
} qd_app_stats_t;
#define QD_SCOPE_N 1024                /* P2.11: the scope's samples (the output at 22.05 kHz: 46 ms) */
typedef struct {                       /* P2.11: the Geek Out pages' own state (view.c's) */
    uint32_t ms;                       /* the last fill's */
    float bar[4];                      /* the meters as shown (they chase the values): load 0, FPS, RAM, STORE */
    float pk[4];                       /* .. their peaks (FPS: its lowest), held QD_GEEK_PEAK_MS from pk_ms */
    uint32_t pk_ms[4];
    float share[QD_NTRACK];                    /* the tracks' shares as shown */
    uint32_t num_ms;                   /* the numbers' last step (four a second) .. */
    int num[6];                        /* .. voices, ISR us, late, lock us, frame (0.1 ms), BPM */
    uint32_t steal0[QD_NTRACK], steal_seen[QD_NTRACK], steal_ms[QD_NTRACK];   /* the tracks' steals at the reset, last seen, when */
    uint32_t late_seen, late_ms;
    int16_t scope[QD_SCOPE_N];         /* the output (L + R) / 2, every other sample: the audio side writes it .. */
    volatile uint32_t scope_w;         /* .. at this index */
    volatile uint8_t scope_on;         /* (only while the scope is shown) */
    uint8_t scope_half;
#if QD_SCOPE_FPS < 30
    uint32_t scope_step;               /* (P2.6i: the scope's line kept between its steps) */
    int scope_trk;
    float scope_pts[113];
#endif
} qd_geek_t;

typedef struct qd_app {
    /* the clock */
    uint32_t ms;                       /* the last tick's ms */
    int started;                       /* a tick has been seen */
    /* the transport */
    int playing;                       /* (the sequencer's, as the dance last saw it) */
    int bpm;                           /* the tempo now (the pattern's, or MIDI clock's) */
    int64_t beat;                      /* whole beats since PLAY: the sequencer's position (P2.4: no double) .. */
    int32_t beat_rem;                  /* .. and the ticks into the beat (of QD_BEAT_TICKS) */
    /* the panel */
    uint32_t keys_held, buttons_held;  /* bit k: key k; bit b: button QD_B_b */
    uint8_t key_vel[QD_NKEY];
    uint32_t keys_struck;              /* bit k: key k struck a note (it is released at the key's release) */
    uint8_t key_note[QD_NKEY], key_track[QD_NKEY];
    uint8_t midi_on[16][128];          /* P1.6g: MIDI in: the tracks (bit t) each channel's note struck (its OFF: those) */
    uint8_t midi_sel[16][16];          /* P2.4: .. a bit a note: it went to the selected track (its OFF: recorded too) */
    int master_pot;                    /* MASTER, 0..1023 */
    uint32_t seed_fix;                 /* P2.4: qd_app_seed's (0: the clock's) */
    uint32_t free_ms;                  /* P2.6c: qd_app_tick_free's ms so far */
    uint32_t snd_gen, snd_ms;          /* P2.6c: qd_app_sounds' last edit_gen and ms */
    uint32_t edit_gen;                 /* P2.6c: bumped by every input (and a take, the tracks' sounds into the pattern):
                                        * persist.c's looks hash a pattern again only after it moved */
    uint32_t frame_hash;               /* P2.4: qd_app_frame_changed's last frame .. */
    int frame_hash_ok;
    uint32_t strip_hash[240 / QD_STRIP_DEV + 1];   /* P2.6: .. its strips' (240: the screen's rows) */
    /* the sound */
    qd_model_t model;
    qd_pool_t pool;
    qd_mix_t mix;
    qd_mix_track_t blk_mt[QD_NTRACK];   /* P2.6: the tracks' mix as qd_app_block_begin found it (the voices' calls read it) */
    /* the sequencer (P1.2) */
    qd_project_t proj;
    qd_seq_t seq;
    qd_player_t player;
    int sel;                           /* the selected track, 0..7 */
    int snd_pat;                       /* P1.5a: the pattern whose track sounds the model's tracks are (seq.cur) */
    /* P1.5a: saving (persist.h): the stores, the library, the projects */
    qd_persist_t per;
    int oct;                           /* the keybed's octave shift */
    /* the UI (P1.3: core/app/ctl.h) */
    qd_ui_t ui;
    qd_hist_t undo;                    /* P1.5c: undo / redo, the snapshot, the pattern clipboard (undo.h) */
    qd_sx_t sx;                        /* P1.5r: SysEx: the replies waiting (fizz_sx.h) */
    int metro, metro_lvl;                         /* P1.5c: the metronome on (SHIFT + B7) .. */
    int64_t metro_beat;                /* .. the last beat it clicked (-1) .. */
    uint32_t metro_ph, metro_left;     /* .. its click's phase and samples left */
    int metro_acc;                     /* .. an accent (beat 1) */
    qd_app_stats_t stats;              /* P2.11: the platform's figures (qd_app_stats) */
    qd_geek_t geek;                    /* P2.11: the Geek Out pages' state */
    uint32_t fb_pending;               /* P2.6f: the bands of the frame being drawn still to draw */
    /* the audio: blocks of 32 frames made ahead of a call that asks for fewer */
    int16_t fifo[2 * QD_MIX_CTL];
    int nfifo, ififo;
} qd_app_t;

void qd_app_init(qd_app_t *a);
/* P2.4: a supported seam for a harness (the host simulator, the device's checks): the screen shown (ctl.h QD_MODE_*,
 * and QD_VIEW_* for the step view), as the panel would leave it (the latches closed, nothing held), and a fixed seed for
 * the rolls of each PLAY (0: from the clock, as the panel's; the conditions' chance) */
void qd_app_show(qd_app_t *a, int mode, int view);
void qd_app_seed(qd_app_t *a, uint32_t seed);
/* input, between ticks (as the panel's state changed): a key with its velocity 1..127, a button, knob detents, the
 * MASTER pot (0..1023), a MIDI message (status, two data bytes) */
void qd_app_key(qd_app_t *a, int key, int down, int vel);
void qd_app_button(qd_app_t *a, int button, int down);
void qd_app_knob(qd_app_t *a, int knob, int detents);
/* P2.6b: an input's undo gesture opened now (its snapshot of the pattern: on the device it is compressed into the
 * undo log, ~330 K host instructions, the gesture's first input only). The port calls it before it takes the audio
 * lock for qd_app_key / qd_app_button / qd_app_knob / qd_app_midi_ui: it reads the patterns and writes the undo log,
 * which the audio side never touches; the locked call then finds the gesture open and costs a few thousand */
void qd_app_input_prepare(qd_app_t *a);
void qd_app_master(qd_app_t *a, int pot);
void qd_app_midi(qd_app_t *a, int status, int d1, int d2);   /* status 0xF8 / FA / FB / FC: MIDI clock */
void qd_app_midi_play(qd_app_t *a, int status, int d1, int d2);   /* P2.4: its audio-safe part (the interrupt's) .. */
void qd_app_midi_ui(qd_app_t *a, int status, int d1, int d2);     /* .. and its UI part, after it (the main loop's) */
/* the clock: ms counts up from 0, one call per device millisecond (a gap is run through ms by ms) */
void qd_app_tick(qd_app_t *a, uint32_t ms);
/* P2.6c: qd_app_tick in two, for the port's audio lock: qd_app_tick_locked(a, ms) under it (each ms: the patterns
 * and sounds the sequencer will want, the tracks' sounds into the pattern, the transport, the controller's timers,
 * the preview), then qd_app_tick_free(a) without it (each of those ms again: the persist looks, which hash what is in
 * RAM and read nothing in; the store's next job; SysEx's labels; with a->ms as it was then; P2.10: then, once, the
 * undo's gesture closed when quiet, which reads the model and writes only the undo log). qd_app_tick
 * is the two a ms at a time, as before */
void qd_app_tick_locked(qd_app_t *a, uint32_t ms);
void qd_app_tick_free(qd_app_t *a);
/* the screen: one frame drawn (qd_gfx), written as RGB565 big-endian (240 x 240) */
void qd_app_frame(qd_app_t *a, uint16_t *lcd_be);
/* P2.4: would the frame now differ from the last one this said? The screen's logic runs once with nothing drawn and
 * every call that would draw hashed (the paths, the transforms, the alpha, the colours, the texts): 1 changed (or the
 * first), 0 the same pixels as before, the frame can be skipped. Call it once a frame, before drawing (the host: 1) */
int qd_app_frame_changed(qd_app_t *a);
/* P2.6: the same, a strip at a time: the frame is taken now (its instant: the qd_app_frame_strips* call after this
 * draws this very frame, whatever the clock and the input do meanwhile), *mask gets bit k for strip k (rows k x
 * QD_STRIP_H ..) when it would draw other pixels than the last frame's; returns *mask != 0 (the host: every strip) */
int qd_app_frame_strips_changed(qd_app_t *a, uint32_t *mask);
/* P2.6: qd_app_frame_strips for the strips in mask only (the others are not drawn, not given to out) */
void qd_app_frame_strips_mask(qd_app_t *a, uint16_t *strip_be, void (*out)(void *ctx, int y0, int rows, const uint16_t *px),
                              void *ctx, uint32_t mask);
/* P2.1: .. or strip by strip (QD_STRIP_H rows: the device's 20; the host's one strip, the whole), each written into
 * strip_be (240 x QD_STRIP_H) and then given to out (its first row, its rows, its pixels): no whole frame in RAM.
 * P2.1b: strip_be 0: each strip written in place, in the core's own strip surface (px), good until out returns (the
 * platform sends it, or copies it, before the next strip is drawn: no buffer of the LCD's in FIZZ's RAM) */
void qd_app_frame_strips(qd_app_t *a, uint16_t *strip_be, void (*out)(void *ctx, int y0, int rows, const uint16_t *px),
                         void *ctx);
void qd_app_leds(const qd_app_t *a, qd_leds_t *out);
/* P2.11: the platform's figures for the Geek Out pages (qd_app_stats_t above): filled in place, any time (the main loop) */
qd_app_stats_t *qd_app_stats(qd_app_t *a);
#ifdef QD_GFX_FB
/* P2.6e: THE 8-BIT FRAME (the unit's QD_GFX_FB build; research/SMOOTH-DISPLAY.md 4.8). The port owns the frame (240 x 240
 * palette indices, row y at rows[y]: any two blocks of memory), the line buffers and the send; the core draws into it,
 * owns the palette and says which bands changed. Main loop only, no audio lock.
 *   qd_app_fb_attach   the frame's rows (kept; every band drawn at the next qd_app_frame_fb)
 *   qd_app_palette     the 256 entries as the panel takes them (RGB565, big-endian); *gen changes when they do (the
 *                      port then sends the whole frame again). One theme: it never changes while running
 *   qd_app_frame_fb    one frame from one instant: the bands (QD_FB_BAND rows each) whose drawing changed are drawn
 *                      into the frame (the others are left as they are); *bands bit k: band k (rows 16 k ..) may have
 *                      changed (the port compares its rows); 0: nothing drawn. Returns *bands != 0 */
#define QD_FB_BAND 16
void qd_app_fb_attach(qd_app_t *a, uint8_t *const rows[240]);
const uint16_t *qd_app_palette(const qd_app_t *a, uint32_t *gen);
int qd_app_frame_fb(qd_app_t *a, uint32_t *bands);
/* P2.6f: the same in steps, so the main loop can take its input (and MIDI) between bands while the audio is heavy:
 * qd_app_frame_fb_begin takes the instant (the list recorded, *bands the bands that will be drawn: 0 nothing), then
 * qd_app_frame_fb_step draws up to n of them (top first) and returns how many are left (0: the frame is whole; the port
 * sends it then). Ticks, keys and MIDI between steps are fine: the frame stays the instant begin took */
int qd_app_frame_fb_begin(qd_app_t *a, uint32_t *bands);
int qd_app_frame_fb_step(qd_app_t *a, int n);
/* P2.6f: THE BANDS ON ANOTHER CORE. qd_app_frame_fb_latch (core 0, main loop): the instant taken (the display list
 * recorded and pinned: the arena's borrowers cannot take it back; *changed: the bands that changed); returns the bands
 * to draw (0: none, or no list: then the changed ones were drawn here, now, and only need sending). Then
 * qd_app_frame_fb_band(k) for each of them, on any core, in any order, one core at a time: it reads only the list,
 * the fonts and the palette (constants) and writes rows 16 k .. 16 k + 15 of the frame and the drawing's own statics.
 * Then qd_app_frame_fb_done (core 0) once every band is drawn: the list unpinned and dropped. Between latch and done
 * core 0 may tick, take input and run the audio; it must not latch, draw or hash another frame */
uint32_t qd_app_frame_fb_latch(qd_app_t *a, uint32_t *changed);
int qd_app_frame_fb_band(int k);
void qd_app_frame_fb_done(qd_app_t *a);
#endif
/* the sound: frames of 16-bit stereo, interleaved, at QD_FS (rendered in control blocks of 32 frames) */
void qd_app_audio(qd_app_t *a, int16_t *stereo, uint32_t frames);
/* P2.6: one control block (QD_MIX_CTL frames) in three, for the two cores (qd_app_audio is the three in a row, the voices
 * in two halves: 0 .. n/2 - 1 into A, n/2 .. n - 1 into B):
 *   qd_app_block_begin   all the shared work: the sequencer's events, the voices started and stolen, the owners' LFOs
 *                        and glides, the MIDI FX, the comb lines claimed;
 *   qd_app_block_voices  voices v0 .. v1 - 1 rendered and mixed into bus (cleared first). Pure: it writes those voices'
 *                        own state and bus, nothing else (no pool counter, owner, arena, flash or global), so the two
 *                        halves may run at once on the two cores; its stack: under 1 KB;
 *   qd_app_block_end     A + B (integer sums: the order of the halves changes nothing), the latest voices' LFO offsets
 *                        to their tracks, the metronome, the effects, the output: QD_MIX_CTL stereo frames into out.
 * The same samples whichever half runs first, bit for bit (test_app). The pool takes a free voice from the half with
 * fewer in use (pool.h halves) */
typedef struct {
    int32_t l[QD_MIX_CTL], r[QD_MIX_CTL], sc[QD_MIX_CTL], sd[QD_MIX_CTL], sr[QD_MIX_CTL];   /* dry, chorus, delay, reverb */
    int n;                                  /* voices rendered into it */
} qd_bus_t;
void qd_app_block_begin(qd_app_t *a);
void qd_app_block_voices(qd_app_t *a, int v0, int v1, qd_bus_t *bus);
void qd_app_block_end(qd_app_t *a, const qd_bus_t *A, const qd_bus_t *B, int16_t *out);
/* P2.6g: PIPELINED, one block (QD_MIX_CTL frames, 0.73 ms) of latency more: a block's effects and output run while the
 * next block's voices render (core 0 the effects, core 1 the voices). A block is a qd_blk_t (its two buses and what its
 * end needs from its begin's moment):
 *   qd_app_block_begin_k(a, k)        qd_app_block_begin, and k's moment kept (the beat, the metronome, the tempo)
 *   qd_app_block_voices(a, v0, v1, &k->x / &k->y)   as before
 *   qd_app_block_publish(a, k)        once both halves are rendered: the latest voices' offsets to their tracks (was
 *                                     in qd_app_block_end; before the next block's begin)
 *   qd_app_block_out(a, k, out)       k's sum, metronome, effects and output, any time before the next block's out:
 *                                     it touches only k, the effects, the metronome's and the scope's own state
 * The order: begin_k(n), voices(n) .. [and meanwhile out(n - 1)], publish(n), begin_k(n + 1) .. out(n) .. The samples
 * are qd_app_block_end's, one block later (test_app) */
typedef struct {
    qd_bus_t x, y;                     /* the two halves' buses */
    int64_t beat;                      /* (its begin's moment: the transport's beat, the metronome on, the tempo) */
    int metro, bpm;
} qd_blk_t;
void qd_app_block_begin_k(qd_app_t *a, qd_blk_t *k);
void qd_app_block_publish(qd_app_t *a, const qd_blk_t *k);
void qd_app_block_out(qd_app_t *a, const qd_blk_t *k, int16_t *out);

/* for the controller (ctl*.c): the transport, the sound's tempo, a note key played on the selected track */
void qd_app_play(qd_app_t *a);
void qd_app_stop(qd_app_t *a);
void qd_app_tempo(qd_app_t *a, int bpm);   /* the tempo the sound runs at (FM TONE's synced LFOs, the delay) */
void qd_app_note_on(qd_app_t *a, int key, int note, int vel);   /* key k strikes note on the selected track */
void qd_app_note_off(qd_app_t *a, int key);   /* .. the note key k struck, released */

#ifdef __cplusplus
}
#endif
