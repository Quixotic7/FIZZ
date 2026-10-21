/* SPDX-License-Identifier: GPL-3.0-only */
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
#include "../seq/seq.h"
#include "../voice/mix.h"
#include "../voice/pool.h"
#include "ctl.h"
#include "persist.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_NKEY 27                     /* the note keys: 0 = F3 (MIDI 53) .. 26 = G5 */
#define QD_NOTE0 53
#define QD_FS 44100                    /* the audio's sample rate */
#define QD_BPM_MIN 30
#define QD_BPM_MAX 300
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
typedef struct qd_leds {
    uint8_t key[QD_NKEY];
    uint8_t button[QD_NBTN];
    uint8_t play_green;
} qd_leds_t;

typedef struct qd_app {
    /* the clock */
    uint32_t ms;                       /* the last tick's ms */
    int started;                       /* a tick has been seen */
    /* the transport */
    int playing;                       /* (the sequencer's, as the dance last saw it) */
    int bpm;                           /* the tempo now (the pattern's, or MIDI clock's) */
    double beats;                      /* beats since PLAY: the sequencer's position */
    /* the panel */
    uint32_t keys_held, buttons_held;  /* bit k: key k; bit b: button QD_B_b */
    uint8_t key_vel[QD_NKEY];
    uint32_t keys_struck;              /* bit k: key k struck a note (it is released at the key's release) */
    uint8_t key_note[QD_NKEY], key_track[QD_NKEY];
    int master_pot;                    /* MASTER, 0..1023 */
    /* the sound */
    qd_model_t model;
    qd_pool_t pool;
    qd_mix_t mix;
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
    /* the audio: blocks of 32 frames made ahead of a call that asks for fewer */
    int16_t fifo[2 * QD_MIX_CTL];
    int nfifo, ififo;
} qd_app_t;

void qd_app_init(qd_app_t *a);
/* input, between ticks (as the panel's state changed): a key with its velocity 1..127, a button, knob detents, the
 * MASTER pot (0..1023), a MIDI message (status, two data bytes) */
void qd_app_key(qd_app_t *a, int key, int down, int vel);
void qd_app_button(qd_app_t *a, int button, int down);
void qd_app_knob(qd_app_t *a, int knob, int detents);
void qd_app_master(qd_app_t *a, int pot);
void qd_app_midi(qd_app_t *a, int status, int d1, int d2);   /* status 0xF8 / FA / FB / FC: MIDI clock */
/* the clock: ms counts up from 0, one call per device millisecond (a gap is run through ms by ms) */
void qd_app_tick(qd_app_t *a, uint32_t ms);
/* the screen: one frame drawn (qd_gfx), written as RGB565 big-endian (240 x 240) */
void qd_app_frame(qd_app_t *a, uint16_t *lcd_be);
void qd_app_leds(const qd_app_t *a, qd_leds_t *out);
/* the sound: frames of 16-bit stereo, interleaved, at QD_FS (rendered in control blocks of 32 frames) */
void qd_app_audio(qd_app_t *a, int16_t *stereo, uint32_t frames);

/* for the controller (ctl*.c): the transport, the sound's tempo, a note key played on the selected track */
void qd_app_play(qd_app_t *a);
void qd_app_stop(qd_app_t *a);
void qd_app_tempo(qd_app_t *a, int bpm);   /* the tempo the sound runs at (FM TONE's synced LFOs, the delay) */
void qd_app_note_on(qd_app_t *a, int key, int note, int vel);   /* key k strikes note on the selected track */
void qd_app_note_off(qd_app_t *a, int key);   /* .. the note key k struck, released */

#ifdef __cplusplus
}
#endif
