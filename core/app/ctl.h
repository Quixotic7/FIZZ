/* SPDX-License-Identifier: GPL-3.0-only */
/* QUADRILLE's controller (P1.3; PLAN.md §8): the panel's input becomes model edits and UI state.
 *
 *   ctl.c        the dispatch: modes, the buttons' table, the keys' tables (what a white or black key does by mode and
 *                by what is held), the knobs' tables, the timers (tap / hold, the takeover, the idle screen), pages
 *   ctl_step.c   the step view's edits: the tap and the hold, the held steps and their fields (each field's knobs as a
 *                table), chord shapes, the p-locks of a held step, CLEAR and its undo, the "last entered" defaults
 *   ctl_rec.c    KEYS mode: the keybed, step recording, live recording, MIDI notes into a held step
 *   ctl_leds.c   the LEDs (PLAN §8's table), five levels
 *   view.c       the frame: the view records (ui.h) filled from the model and this state, the screens drawn
 *
 * The UI state (qd_ui_t, here) is all the controller keeps: the mode, the held steps, the field, the takeover and its
 * timer, the latches, the recording. The screens (core/ui) draw only from the model and this state, through the view
 * records; drawing changes nothing. Time is the app's simulated milliseconds (PLAN §13): no OS calls, deterministic.
 *
 * THE GESTURES (PLAN §8; the tables in ctl.c and ctl_step.c):
 *   white key, step view   PATTERN held: queue A01..A16 | TRACKS held: select 1..8 | PAGE held: page 1..8 |
 *                          else the step: down = pending (its LED shows the result at once); released under 250 ms
 *                          with nothing turned = a TAP (empty -> a NOTE trig with the track's defaults, a trig -> empty;
 *                          FUNC: a LOCK step, or back to empty); 250 ms, a knob or a black key = HELD (the field screen)
 *   black key, step view   steps held: the fields NOTE .. LOCKS, F#5 CLEAR (again within 4 s: undo) |
 *                          nothing held: T1..T8 select the track, C#5 / D#5 page - / +
 *   latch                  steps held + OCT+ (in NOTE, whose OCT- / OCT+ move the notes an octave: FUNC + OCT+);
 *                          OCT- (NOTE: FUNC + OCT-) or HOME lets go; a white tap while latched adds / removes a step
 *   knobs                  see ctl.c (nothing held) and ctl_step.c (a step held: the field's table; a knob the field
 *                          does not use = a p-lock of the quick page FREQ / DECAY / HARM / FDBK)
 */
#pragma once
#include <stdint.h>

#include "../seq/pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qd_app;

#define QD_TAP_MS 250u                 /* a white key held this long is held, not tapped */
#define QD_IDLE_MS 120000u             /* untouched this long, stopped: the idle screen */
#define QD_UNDO_MS 4000u               /* CLEAR again within this long: undone */
#define QD_TK_MS 1000u                 /* a takeover stays this long after the last detent .. */
#define QD_TK_FADE_MS 120u             /* .. then fades back */
#define QD_CONFIRM_MS 1100u            /* a confirmation word stays this long (its last 120 ms a fade) */
#define QD_BLINK_MS 250u               /* a blink's half period (latched steps, the queued pattern, REC) */
#define QD_FAST_MS 90u                 /* a fast blink's half period (the held step) */
#define QD_SLIDE_MS 160u               /* a page change slides the roll in */

enum { QD_MODE_IDLE, QD_MODE_STEP, QD_MODE_KEYS, QD_MODE_TRACKS };
enum { QD_REC_OFF, QD_REC_STEP, QD_REC_LIVE };
/* the takeovers: UI_TK_* (ui.h), and the sound picker */
#define QD_TK_NONE (-1)
#define QD_TK_PRESET 100

#define QD_NROLL 32

typedef struct {
    int mode;                          /* QD_MODE_* */
    uint32_t touch_ms;                 /* the last input (the idle screen's clock) */
    /* the step view */
    int page[8];                       /* the page shown, per track */
    int follow;                        /* page follow (on at power-on) */
    int play_page[8];                  /* the playing page each track was last seen on (follow moves on its edges) */
    uint32_t slide_ms;                 /* the last page change .. */
    int slide_dir;                     /* .. +1: to a later page */
    /* the white keys in the step view */
    uint16_t wk_down;                  /* bit w: white key w is down as a step key */
    uint16_t wk_pending;               /* .. and not yet held: released now, it is a tap */
    uint16_t wk_func;                  /* .. FUNC was on at its press */
    uint32_t wk_t0[16];
    uint32_t wk_seq[16], wk_count;     /* the order the keys went down in (held in that order) */
    /* the held steps: absolute steps of the selected track, the first shown */
    int held[16];
    int8_t held_key[16];               /* the white key holding it (-1: only latched) */
    int nheld;
    int latch;                         /* the held steps stay after release */
    int field;                         /* the field shown (remembered from hold to hold; NOTE at first) */
    int field_pressed;                 /* a field key was pressed in this hold (an empty step's knobs edit the field) */
    int held_empty;                    /* the first step held was empty when it was held */
    int lock_cur;                      /* LOCKS: the lock shown */
    uint32_t chord_ms;                 /* NOTE: the chord changed (the name squeezes in) */
    uint32_t roll_ms;                  /* SOUND / LOCKS: the last roll .. */
    int roll_dir;
    uint32_t held_play_ms;             /* the held step last played (the mallet strikes, the reels turn) */
    int held_played;                   /* .. it has, since it was held */
    uint64_t flash_seen;               /* the selected track's last trig seen (the sequencer's flash_sample) */
    /* the takeover (PLAN §9a rule 5) */
    int tk;                            /* QD_TK_NONE, UI_TK_*, QD_TK_PRESET */
    uint32_t tk_ms;                    /* the last detent */
    int tk_dir;
    int tk_lock;                       /* a p-lock on the held step (or the step recorded live) */
    int tk_step;                       /* .. that step */
    int tk_knob;                       /* the colour, 0..3 */
    /* the confirmation word (CLEAR, UNDONE, NO ROOM FOR LOCKS) */
    const char *cf_big, *cf_small;
    uint32_t cf_ms;
    int cf_on;
    /* CLEAR's undo: the pattern as it was, kept until another edit */
    qd_pattern_t undo_pat;
    int undo_pat_k;
    int undo_ok;
    uint32_t undo_ms;
    /* the keys: the row of the keys' table that took each key's press (0: none), so its release goes there too */
    int8_t key_row[27];
    /* the buttons */
    int tracks_tap;                    /* TRACKS went down alone: its release toggles the tracks screen */
    int page_used;                     /* PAGE held picked a page: its release is not a tap */
    int func_latch;                    /* FUNC tapped: latched (lit) until tapped again */
    int func_used;                     /* FUNC was used as a shift while down */
    /* KEYS mode (ctl_rec.c) */
    int rec;                           /* REC on in KEYS mode (step recording stopped, live recording playing) */
    int quantize;                      /* FUNC + REC: live recording quantized (micro 0) */
    int cursor;                        /* step recording's cursor (an absolute step) */
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
int qd_ctl_npages(const struct qd_app *a, int t);   /* the track's pages, 1..8 */
int qd_ctl_func(const struct qd_app *a);            /* FUNC held or latched */
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
void qd_step_field_key(struct qd_app *a, int b);    /* black key b with steps held */
int qd_step_knob(struct qd_app *a, int knob, int d);   /* a knob with steps held; 1: taken */
void qd_step_octave(struct qd_app *a, int d);       /* the held steps' notes an octave */
void qd_step_latch(struct qd_app *a, int on);
void qd_step_release_all(struct qd_app *a);         /* every held step let go (HOME, a track change) */
void qd_step_notes_in(struct qd_app *a, const uint8_t *notes, int n);   /* the held steps' notes (MIDI) */
void qd_step_edited(struct qd_app *a);              /* a model edit: CLEAR's undo closes */
int qd_step_quick_id(int k);           /* the quick page's lock id of KNOB k (FREQ, DECAY, HARM, FDBK) */
int qd_step_id_detent(int id);         /* a detent's size for a value (hundredths for the hi-res ones) */
int qd_step_base(struct qd_app *a, int t, int s, int id);   /* the value a lock on step s starts from */
unsigned qd_step_fields_set(const struct qd_app *a, int t, int s);   /* bit f: field f not at its default */

/* ---- ctl_rec.c */
void qd_rec_key(struct qd_app *a, int note, int down, int vel);   /* a note in KEYS mode (keybed or MIDI) */
void qd_rec_toggle(struct qd_app *a);
void qd_rec_rest(struct qd_app *a);
void qd_rec_cursor(struct qd_app *a, int d);
int qd_rec_knob(struct qd_app *a, int knob, int d);   /* live recording: a knob turn as a lock; 1: taken */
void qd_rec_midi_note(struct qd_app *a, int note, int down, int vel);   /* MIDI into a held step / the recording */

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
