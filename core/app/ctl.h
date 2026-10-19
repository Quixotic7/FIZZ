/* SPDX-License-Identifier: GPL-3.0-only */
/* QUADRILLE's controller (P1.3b; PLAN.md §8b, the sequencer controls version 2, after the OMX-27 Form sequencer): the
 * panel's input becomes model edits and UI state.
 *
 *   ctl.c        the dispatch: modes, the buttons' table, the keys' tables (what a white or black key does by mode and
 *                by what is held: a layer F1 / F2 / F3, a step), the knobs' table, the timers, pages, the layers
 *   ctl_step.c   the step view's edits: the tap and the hold, the held steps, the hold modes' ten values (B1..B10) and
 *                CLEAR (B11), ALGORITHM's fine value and the track's defaults, note entry, the sound pool's banks, the
 *                p-locks and the sound lock of a held step
 *   ctl_rec.c    the keyboard: the keybed as a keyboard, step recording, live recording, MIDI notes into a held step
 *   ctl_leds.c   the LEDs (§8b), five levels
 *   view.c       the frame: the view records (ui.h) filled from the model and this state, the screens drawn
 *
 * The UI state (qd_ui_t, here) is all the controller keeps. The screens (core/ui) draw only from the model and this
 * state, through the view records; drawing changes nothing. Time is the app's simulated milliseconds (PLAN §13): no OS
 * calls, deterministic.
 *
 * NAMES (§8b): OCT- = F1, OCT+ = F2, both = F3 (held together: both down, in either order; once both are down it stays
 * F3 until both are up), GLO = SHIFT, the black keys F#3 .. D#5 = B1..B10, F#5 = B11, the white keys W1..W16 = the
 * steps of the page being edited.
 *
 * THE STEP VIEW, NOTHING HELD: W tap (released under 250 ms) = a trig with no overrides / the trig and its locks
 * removed; W held 250 ms (or a knob or a black key while it is down) = held. B1..B10 = the hold mode (NOTE VEL LEN MICRO
 * RATCH CHNC COND TRIG SOUND LOCKS); B11 nothing. ALGORITHM = the track's default for the mode (a takeover draws it);
 * SELECT = the page edited; KNOB 1..4 = the track's FREQ, DECAY, HARM, FDBK; PRESETS = the track's sound (a pool
 * sound); SHIFT + ALGORITHM = the track's scale, SHIFT + PRESETS = its root (temporary, until the track settings).
 * A STEP HELD (several: every edit to all): B1..B10 = the mode's ten values (QD_VK_* in ctl_step.c), B11 = the mode
 * back to the track's (LOCKS: the chosen lock removed); ALGORITHM = the mode's value, finely (SHIFT: MICRO's strum,
 * RATCH's ramp); PRESETS = the sound lock, any pool sound, in every mode (its picker takes the screen; SOUND's bank
 * follows); KNOB 1..4 = p-locks; SELECT = the page; OCT- / OCT+ = NOTE's entry octave, SOUND's bank of ten; other W keys
 * join the held steps. NOTE entry is Form's "pressed": the first key down replaces the steps' notes, keys down together
 * add to them (up to 4); a key sounds its note.
 * F1 (OCT- held): B1..B8 pages (tap < 200 ms: edit it; double tap, the second release within 200 ms of the first: loop
 * it alone; hold A + press B: loop A..B, edit A); B9 clear the page, B10 clear the track, B11 undo / redo; W copy the
 * step (SHIFT: cut). F2 (OCT+ held): B1..B8 tracks (select; held + W1 mute, + W2 solo), B9 copy the page, B10 paste it;
 * W paste the step. F3 (both): B1..B9 the rate (1/8 1/4 1/2 2/3 3/4 1 4/3 3/2 2); W = the page's end (the page joins
 * the loop), W a held + W b = start a .. end b; SELECT tempo, SHIFT + SELECT swing.
 * BUTTONS: REC tap = the keyboard <-> the sequencer; REC + PLAY = play with recording armed (in the keyboard); SHIFT + REC
 * in the keyboard = armed / not (stopped: step recording, playing: live); ARP held = FILL; PAGE tap = page follow on /
 * off, PAGE held + W1..W8 = edit that page; TRACKS, PATTERN, HOME, PLAY as P1.3. */
#pragma once
#include <stdint.h>

#include "../seq/clip.h"
#include "../seq/pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qd_app;

#define QD_TAP_MS 250u                 /* a white key held this long is held, not tapped */
#define QD_CLICK_MS 200u               /* Form's tap (a page key) and its double tap */
#define QD_CHORD_MS 60u                /* F1 / F2 alone this long before its map shows (both within: F3) */
#define QD_IDLE_MS 120000u             /* untouched this long, stopped: the idle screen */
#define QD_TK_MS 1000u                 /* a takeover stays this long after the last detent .. */
#define QD_TK_FADE_MS 120u             /* .. then fades back */
#define QD_CONFIRM_MS 1100u            /* a confirmation word stays this long (its last 120 ms a fade) */
#define QD_BLINK_MS 250u               /* a blink's half period (the playing page, the queued pattern, solo) */
#define QD_FAST_MS 90u                 /* a fast blink's half period (the held step) */
#define QD_SLIDE_MS 160u               /* a page change slides the lane in */
#define QD_ENTRY_OCT 4                 /* NOTE's entry octave at power-on (the root at C4 = MIDI 60) */

enum { QD_MODE_IDLE, QD_MODE_STEP, QD_MODE_KEYS, QD_MODE_TRACKS };
enum { QD_REC_OFF, QD_REC_STEP, QD_REC_LIVE };
/* the hold modes (B1..B10): UI_F_* (ui.h) */
enum { QD_LAYER_NONE, QD_LAYER_F1, QD_LAYER_F2, QD_LAYER_F3 };
/* the takeovers: UI_TK_* (ui.h), and the sound picker */
#define QD_TK_NONE (-1)
#define QD_TK_PRESET 100               /* the pool picker: the track's sound, or the held steps' sound lock */
#define QD_TK_DEFAULT 101              /* ALGORITHM with nothing held: the mode's default, drawn as its held screen */

#define QD_NROLL 32

typedef struct {
    int mode;                          /* QD_MODE_* */
    uint32_t touch_ms;                 /* the last input (the idle screen's clock) */
    /* the step view */
    int hmode;                         /* the hold mode, UI_F_* (NOTE at power-on) */
    int page[8];                       /* the page edited, per track */
    int follow;                        /* page follow (on at power-on) */
    int play_page[8];                  /* the playing page each track was last seen on (follow moves on its edges) */
    uint32_t slide_ms;                 /* the last page change .. */
    int slide_dir;                     /* .. +1: to a later page */
    /* the white keys in the step view */
    uint16_t wk_down;                  /* bit w: white key w is down as a step key */
    uint16_t wk_pending;               /* .. and not yet held: released now, it is a tap */
    uint32_t wk_t0[16];
    uint32_t wk_seq[16], wk_count;     /* the order the keys went down in (held in that order) */
    /* the held steps: absolute steps of the selected track, the first shown */
    int held[16];
    int8_t held_key[16];               /* the white key holding it */
    int nheld;
    int held_empty;                    /* the first step held was empty when it was held */
    int lock_cur;                      /* LOCKS: the lock chosen */
    uint32_t chord_ms;                 /* NOTE: the chord changed (the name squeezes in) */
    uint32_t roll_ms;                  /* SOUND / LOCKS: the last roll .. */
    int roll_dir;
    uint32_t held_play_ms;             /* the held step last played (the mallet strikes, the reels turn) */
    int held_played;                   /* .. it has, since it was held */
    uint64_t flash_seen;               /* the selected track's last trig seen (the sequencer's flash_sample) */
    /* NOTE entry (Form's "pressed") and SOUND's banks */
    int entry_oct;                     /* the octave B1's degree starts at */
    uint16_t note_keys;                /* bit b: B(b+1) is down as a note key */
    uint8_t entry_chord[4];            /* the notes entered while the note keys are down (Form's "pressed") */
    int entry_n;
    int sound_bank;                    /* SOUND: the bank of ten on B1..B10 (0: sounds 1..10) */
    /* the layers: F1 / F2 / F3 (OCT- / OCT+ / both) */
    int f3;                            /* both were down: F3 until both are up */
    uint32_t f_ms;                     /* when the layer's first key went down */
    uint16_t lk_down;                  /* bit b: B(b+1) went down in the layer (its release goes there too) */
    uint32_t pg_t0[8];                 /* F1: a page key's press */
    uint32_t pg_up[8];                 /* .. its last tap's release (a double tap within QD_CLICK_MS) */
    int pg_taps[8];
    int pg_range;                      /* F1: a range was made: no taps until the page keys are up */
    int f2_track;                      /* F2: the track key held (-1 none) */
    int f2_used;                       /* .. W1 / W2 used with it */
    int f3_anchor;                     /* F3: the white key held first (-1 none) */
    /* the takeover (PLAN §9a rule 5) */
    int tk;                            /* QD_TK_NONE, UI_TK_*, QD_TK_PRESET, QD_TK_DEFAULT */
    uint32_t tk_ms;                    /* the last detent */
    int tk_dir;
    int tk_lock;                       /* a p-lock (or the sound lock) on the held step (or the step recorded live) */
    int tk_step;                       /* .. that step */
    int tk_knob;                       /* the colour, 0..3 */
    /* the confirmation word (CLEARED, UNDONE, COPIED, NO ROOM FOR LOCKS) */
    const char *cf_big, *cf_small;
    uint32_t cf_ms;
    int cf_on;
    /* copy, paste and undo (seq/clip.h) */
    qd_clip_t clip;
    /* the keys: the row of the keys' table that took each key's press (0: none), so its release goes there too */
    int8_t key_row[27];
    /* the buttons */
    int tracks_tap;                    /* TRACKS went down alone: its release toggles the tracks screen */
    int page_used;                     /* PAGE held picked a page: its release is not a tap */
    int rec_down_used;                 /* REC + PLAY: REC's release is not a tap */
    int shift_used;                    /* SHIFT did something while down (its map then stops) */
    /* the keyboard (ctl_rec.c) */
    int rec;                           /* recording armed in the keyboard (step recording stopped, live playing) */
    int quantize;                      /* live recording quantized (micro 0): off (no gesture in P1.3b) */
    int cursor;                        /* step recording's cursor (a position in the track's loop) */
    int rec_n;                         /* notes of the chord being recorded, still down */
    uint8_t rec_chord[4];
    int rec_nchord;
    int rec_step;                      /* live: the step the chord went into (-1 none) */
    int64_t rec_t0[128];               /* each note's start (live: ticks; step: 0), -1 not down in the recording */
    int16_t rec_s[128];                /* live: the step each note went into */
    int rec_live;                      /* live notes are down (the transport stopping ends them) */
    struct { int64_t t0, t1; uint8_t note; } roll[QD_NROLL];   /* live: the notes recorded, t1 -1 while down */
    int nroll, iroll;
    /* MIDI notes into a held step */
    int midi_n;                        /* notes down */
    uint8_t midi_chord[4];
    int midi_nchord;
} qd_ui_t;

/* ---- ctl.c */
void qd_ctl_init(struct qd_app *a);
void qd_ctl_key(struct qd_app *a, int key, int down, int vel);
void qd_ctl_button(struct qd_app *a, int button, int down);
void qd_ctl_knob(struct qd_app *a, int knob, int detents);
void qd_ctl_ms(struct qd_app *a);      /* one millisecond: the timers */
void qd_ctl_set_tempo(struct qd_app *a, int bpm);   /* the pattern's tempo (and the sequencer's) */
qd_pattern_t *qd_ctl_pat(struct qd_app *a);         /* the pattern edited: the current one */
const qd_pattern_t *qd_ctl_cpat(const struct qd_app *a);
int qd_ctl_shift(const struct qd_app *a);           /* SHIFT held */
int qd_ctl_layer(const struct qd_app *a);           /* QD_LAYER_*: the F layer held (none while steps are held) */
int qd_ctl_layer_shown(const struct qd_app *a);     /* .. the one whose map is on screen (F1 / F2 after 60 ms) */
int qd_ctl_playing_page(const struct qd_app *a, int t);   /* the page under the track's play head, -1 stopped */
void qd_ctl_set_page(struct qd_app *a, int p);      /* the page edited on the selected track (slides) */
void qd_ctl_takeover(struct qd_app *a, int tk, int knob, int dir, int lock, int step);
void qd_ctl_confirm(struct qd_app *a, const char *big, const char *small);
int qd_ctl_rate_index(int rate);       /* a rate's place in the picker (slowest first) */
int qd_ctl_rate_at(int i);             /* .. and back */
int qd_ctl_white(int key);             /* the white key 0..15 of a note key, or -1 */
int qd_ctl_black(int key);             /* the black key 0..10 of a note key, or -1 */
extern const uint8_t QD_WHITE_KEY[16], QD_BLACK_KEY[11];

/* ---- ctl_step.c */
void qd_step_white_down(struct qd_app *a, int w);
void qd_step_white_up(struct qd_app *a, int w);
void qd_step_promote(struct qd_app *a, int w);      /* a pending white key becomes held */
void qd_step_promote_all(struct qd_app *a);
void qd_step_promote_due(struct qd_app *a);         /* those down 250 ms, in the order they went down */
void qd_step_value_key(struct qd_app *a, int b, int down);   /* B1..B11 with steps held */
int qd_step_knob(struct qd_app *a, int knob, int d);   /* a knob with steps held; 1: taken */
void qd_step_oct(struct qd_app *a, int d);          /* OCT- / OCT+ with steps held: NOTE's octave, SOUND's bank */
void qd_step_release_all(struct qd_app *a);         /* every held step let go (HOME, a track change) */
void qd_step_notes_in(struct qd_app *a, const uint8_t *notes, int n);   /* the held steps' notes (MIDI) */
int qd_step_default_knob(struct qd_app *a, int d);  /* ALGORITHM, nothing held: the track's default of the mode */
int qd_step_quick_id(int k);           /* the quick page's lock id of KNOB k (FREQ, DECAY, HARM, FDBK) */
int qd_step_id_detent(int id);         /* a detent's size for a value (hundredths for the hi-res ones) */
int qd_step_base(struct qd_app *a, int t, int s, int id);   /* the value a lock on step s starts from */
unsigned qd_step_modes_set(const struct qd_app *a, int t, int page);   /* bit m: a step of the page overrides mode m */
/* the ten value keys of a mode for the held steps (the first held): their labels, the ones matching the step's value
 * (lit), the track's default (dim), the keys that do nothing; *clr: the step overrides the mode */
void qd_step_value_keys(const struct qd_app *a, char lab[10][6], uint16_t *lit, uint16_t *def, uint16_t *none,
                        int *clr);
int qd_step_entry_note(const struct qd_app *a, int b);   /* NOTE: B(b+1)'s note at the entry octave */
int qd_step_sound_bank(const struct qd_app *a);     /* SOUND: the bank on the keys */
void qd_step_sound_pick(struct qd_app *a, int d);   /* PRESETS (or SOUND's ALGORITHM) with steps held: the sound lock */

/* ---- ctl_rec.c */
void qd_rec_key(struct qd_app *a, int note, int down, int vel);   /* a note in the keyboard (keybed or MIDI) */
void qd_rec_toggle(struct qd_app *a);  /* armed / not */
void qd_rec_rest(struct qd_app *a);
void qd_rec_cursor(struct qd_app *a, int d);
int qd_rec_knob(struct qd_app *a, int knob, int d);   /* live recording: a knob turn as a lock; 1: taken */
void qd_rec_midi_note(struct qd_app *a, int note, int down, int vel);   /* MIDI into a held step / the recording */
int qd_rec_key_note(const struct qd_app *a, int key);   /* the keyboard's note of key k (the octave, the scale) */

/* ---- ctl_leds.c */
struct qd_leds;
void qd_ctl_leds(const struct qd_app *a, struct qd_leds *out);
void qd_ctl_sounding(const struct qd_app *a, uint8_t *notes);   /* the selected track's notes sounding (128) */
int qd_ctl_trig_flash(const struct qd_app *a, int t);          /* track t's trig flash (80 ms) */

/* ---- view.c */
void qd_view_frame(struct qd_app *a, uint16_t *lcd_be);

#ifdef __cplusplus
}
#endif
