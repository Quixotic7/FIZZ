/* SPDX-License-Identifier: GPL-3.0-only */
/* QUADRILLE's app (PLAN.md §13): the panel's input in as events, the simulated clock, the transport, a frame drawn
 * through the screens (ui/), the LEDs and the audio out. It knows nothing of the core ABI (glue/ translates) and
 * calls nothing of the OS: all time is the milliseconds given to qd_app_tick, so two runs with the same input are
 * the same, frame for frame and sample for sample.
 *
 * P1.0: the idle screen. PLAY plays / stops; SELECT or KNOB 1 change the tempo (1 BPM a detent, 30..300); a held
 * key or button lights its LED; PLAY's green LED marks each beat while playing. The sound is a test tone for each
 * held key (a sine at the key's pitch): the stand-in until FM TONE arrives in P1.1. */
#pragma once
#include <stdint.h>

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

/* the buttons, by QUADRILLE's roles (PLAN.md §8), in the panel's order (printed FX, SEL, ENV, LFO, EDIT, GLO, HOME,
 * SAVE, ARP, SEQ, PLAY, REC, OCT-, OCT+) */
enum { QD_B_TRACKS, QD_B_PATTERN, QD_B_PAGE, QD_B_MUTE, QD_B_SOUND, QD_B_FUNC, QD_B_HOME, QD_B_SAVE, QD_B_KEYS,
       QD_B_MIX, QD_B_PLAY, QD_B_REC, QD_B_OCTDN, QD_B_OCTUP, QD_NBTN };
/* the knobs (detents), in the panel's role order */
enum { QD_K_SELECT, QD_K_ALGO, QD_K_PRESETS, QD_K_1, QD_K_2, QD_K_3, QD_K_4, QD_NKNOB };

/* the LEDs as the app sees them: a level 0..255 per key, per button, and PLAY's green LED */
#define QD_LED_OFF 0
#define QD_LED_LIT 255
typedef struct {
    uint8_t key[QD_NKEY];
    uint8_t button[QD_NBTN];
    uint8_t play_green;
} qd_leds_t;

typedef struct {
    /* the clock */
    uint32_t ms;                       /* the last tick's ms */
    int started;                       /* a tick has been seen */
    /* the transport */
    int playing;
    int bpm;
    double beats;                      /* beats since PLAY (the tempo integrated: a change never jumps) */
    /* the idle screen's dance */
    float turn_base;                   /* whole quarter turns done before this play */
    float turn;                        /* the turn shown */
    float turn_target;                 /* stopped: where the turn settles (a whole number) */
    double sway;                       /* stopped: the sway's phase, radians */
    float amp;                         /* the motion's size, eased between playing (1) and stopped */
    /* the panel */
    uint32_t keys_held, buttons_held;  /* bit k: key k; bit b: button QD_B_b */
    uint8_t key_vel[QD_NKEY];
    /* the test tone: one sine a key */
    double tone_phase[QD_NKEY];
    float tone_env[QD_NKEY];
} qd_app_t;

void qd_app_init(qd_app_t *a);
/* input, between ticks (as the panel's state changed): a key with its velocity 1..127, a button, knob detents */
void qd_app_key(qd_app_t *a, int key, int down, int vel);
void qd_app_button(qd_app_t *a, int button, int down);
void qd_app_knob(qd_app_t *a, int knob, int detents);
/* the clock: ms counts up from 0, one call per device millisecond (a gap is run through ms by ms) */
void qd_app_tick(qd_app_t *a, uint32_t ms);
/* the screen: one frame drawn (qd_gfx), written as RGB565 big-endian (240 x 240) */
void qd_app_frame(qd_app_t *a, uint16_t *lcd_be);
void qd_app_leds(const qd_app_t *a, qd_leds_t *out);
/* the sound: frames of 16-bit stereo, interleaved, at QD_FS */
void qd_app_audio(qd_app_t *a, int16_t *stereo, uint32_t frames);

#ifdef __cplusplus
}
#endif
