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
 *   ctl_latch.c  (P1.3d) latched menus and confirmations: OCT- back / exit / cancel, OCT+ yes / confirm / enter
 *   ctl_preview.c (P1.3d) the stopped preview: the held steps sound while the transport is stopped
 *   view.c       the frame: the view records (ui.h) filled from the model and this state, the screens drawn
 *
 * The UI state (qd_ui_t, here) is all the controller keeps. The screens (core/ui) draw only from the model and this
 * state, through the view records; drawing changes nothing. Time is the app's simulated milliseconds (PLAN §13): no OS
 * calls, deterministic.
 *
 * NAMES (§8b): OCT- = F1, OCT+ = F2, both = F3 (held together: both down, in either order; once both are down it stays
 * F3 until both are up), GLO = SHIFT, the black keys F#3 .. D#5 = B1..B10, F#5 = B11, the white keys W1..W16 = the
 * steps of the page being edited. Printed SEL = TRACKS-held map (P1.3c), printed SEQ = the patterns (P1.3c).
 *
 * THE STEP VIEW, NOTHING HELD: W tap (released under 250 ms) = a trig with no overrides / the trig and its locks
 * removed; W held 250 ms (or a knob or a black key while it is down) = held. B1..B11 = the hold mode (NOTE VEL LEN MICRO
 * RATCH CHNC COND TRIG SOUND LOCKS, and P1.3c's P-LOCK on B11). ALGORITHM = the track's default for the mode (a
 * takeover draws it); SELECT = the page edited; KNOB 1..4 = the track's FREQ, DECAY, HARM, FDBK; PRESETS = the track's
 * sound (a pool sound); SHIFT + ALGORITHM = the track's scale, SHIFT + PRESETS = its root (temporary).
 * A STEP HELD (several: every edit to all): B1..B10 = the mode's ten values (QD_VK_* in ctl_step.c), B11 = the mode
 * back to the track's (LOCKS: the chosen lock removed); ALGORITHM = the mode's value, finely (SHIFT: MICRO's strum,
 * RATCH's ramp); PRESETS = the sound lock, any pool sound, in every mode; KNOB 1..4 = p-locks (through the current
 * screen's lockable knobs: qd_ctl_lock_screen); SELECT = the page; OCT- / OCT+ = NOTE's entry octave, SOUND's bank of
 * ten (each change: the octave takeover, the OCT LEDs blink by the distance from the centre); other W keys join.
 *   NOTE (P1.3c): SELECT = note 1 (absolute; a detent a degree of the track's scale, chromatic a semitone), KNOB 1..4 =
 *   notes 2..5 (offsets above note 1, a detent the next note of the scale up / down, 0 = none); each note sounds as it
 *   changes. Entry by B1..B10 is Form's "pressed": the first key down replaces, keys down together add (up to 5).
 *   P-LOCK (P1.3c, B11 with nothing held): holding a step keeps the screen; the screen's lockable knobs p-lock the held
 *   steps (the step view: KNOB 1..4 FREQ DECAY HARM FDBK, PRESETS the sound lock); B11 clears the held steps' p-locks
 *   (undoable); B1..B10 nothing.
 * F1 (OCT- held; §8b 2.1's page layer): B1..B8 tap = edit that page, double tap (the second release within 200 ms of
 * the first) = loop it alone, held 400 ms = that page in / out of the loop (never empty), hold A + press B = loop A..B; B9 tap = clear the page, held 600 ms = clear the track; B10 = copy the page,
 * then B1..B8 paste it there (each press again) until F1 is let go; B11 undo / redo; W: the first W copies its step,
 * every later W pastes it. F2 (OCT+ held): W = Form's pick-up / drop (the first W cuts, even an empty step; then an
 * empty step is pasted into, a filled one alternately pasted over and cut; F2 let go: the hand empties; an F1 copy
 * loads the hand, so the next F2's first W drops it). Every edit of one F1 or F2 hold is one undo level (F1 + B11).
 * F3 (both; §8b 2.1): B1..B7 the rate's base (1/1 .. 1/64, the type kept), B8 / B9 / B10 normal / dotted / triplet
 * (the base kept); KNOB 1 the base, KNOB 2 the type, KNOB 3 / 4 the page's start / end; W = the page's end (the page joins the loop), W a held + W b = start a .. end b; SELECT
 * tempo, SHIFT + SELECT swing.
 * SEL held (§8b 2.1): B1..B8 select tracks 1..8; W1..W8 tap = mute / unmute them, held 400 ms = solo / unsolo; B11 =
 * every mute and solo off (the track map on screen).
 * SEQ held: W1..W16 tap = A01..A16 queued by the launch mode (the project's: END, BAR, BEAT, NOW; ALGORITHM turned
 * while SEQ is held), held 400 ms = at once; the pattern map on screen.
 * BUTTONS: REC tap = the keyboard <-> the sequencer; REC + PLAY = play with recording armed (in the keyboard); SHIFT + REC
 * in the keyboard = armed / not (stopped: step recording, playing: live); ARP held = FILL; PAGE tap = page follow on /
 * off, PAGE held + W1..W8 = edit that page; TRACKS, HOME, PLAY as P1.3. The keyboard's OCT- / OCT+: its octave (the
 * octave takeover, the OCT LEDs).
 *
 * P1.3d (§8b VERSION 2.2):
 *   THE KNOBS TAKE OVER THE HOLD MODES (KNOB_JOB in ctl.c, one table [mode][SELECT, KNOB 1..4]): a step held, SELECT and
 *   KNOB 1 = the step's main value (NOTE: SELECT note 1, KNOB 1..4 notes 2..5), KNOB 2 the mode's extra (LEN TIE, MICRO
 *   strum, RATCH ramp, CHNC condition, COND chance), KNOB 3 / 4 nothing; nothing held, KNOB 1..4 the same on the
 *   track's defaults (SELECT the page, ALGORITHM the main default). Every turn shows the knob band (QD_TK_HELDKNOB over
 *   the held step's screen; QD_TK_DEFAULT over the lane, the default lit, "TRACK DEFAULT"). P-LOCK: nothing held, KNOB
 *   1..4 the track's sound (live), ALGORITHM the track (UI_TK_TRACK), SELECT the page; a step held as P1.3c.
 *   LOCKS IS DROPPED: B10 is empty (reserved; UI_F_B10 is never a mode, its LED off).
 *   BANKS: SEQ + B1..B4 = bank A..D (seq_bank): the W keys, the LEDs and the map are that bank's.
 *   THE PREVIEW: stopped, the held steps sound (ctl_preview.c).
 *   F3 IS LATCHED (ctl_latch.c): OCT- and OCT+ together open it, it stays; OCT- (or HOME) closes it, OCT+ is reserved
 *   (confirm). The keys a latch answered are no layer until let go (oct_eaten).
 *   THE STEP LATCH (printed LFO): as if a step is always held: a W selects (W keys down together: all of them), no trig
 *   is made or removed; everything a held step does works on the selection (the screens, the values, the knobs, the
 *   octave, the preview). It starts on the last step held or tapped (else the page's first); LFO again or HOME end it.
 *
 * P1.3e (§8b 2.2's later bullets):
 *   A MODE KEY HELD (B1..B9, nothing else held; the step latch's empty selection counts): "the track" for that mode:
 *   its full TRACK DEFAULT picture (after 250 ms, or at once when used), SELECT / KNOB 1..4 its default by KNOB_JOB,
 *   ALGORITHM the main default, B11 resets it (qd_step_default_reset), the other black keys nothing; let go, the lane.
 *   Nothing held, a default turned shows the same picture for a second (QD_TK_DEFAULT; the band stays for a held step).
 *   SIX NOTES: a held step's ALGORITHM in NOTE is note 6; the default chord's ALGORITHM still moves every note.
 *   KNOB 4, THE QUIET KNOB: where it has no other job, the main value with no overlay (QD_KJ_QUIET).
 *   THE STEP LATCH: P1.3f, a gesture (every W from the first down until all are up) replaces the selection with its
 *   keys, shown as they go down; one tap on the step that was the whole selection empties it. An empty selection acts
 *   as nothing held (the modes, the defaults).
 *   HOME: closes a latch (F3, the step latch) first; else in the sequencer switches its view (PIANO ROLL / STEPS);
 *   elsewhere it returns to the sequencer in its last view.
 *
 * P1.3f (§8b 2.2, "superseded the same day"): the notes are pitches in slots (note k = slot k). In NOTE, with a step
 * held (or selected) and with the NOTE key held for the default, SELECT moves note 1 alone, KNOB 1..4 notes 2..5 alone
 * (a scale step a detent, skipping the other notes' pitches; notes 2..5 below note 1: removed, from none: a step above
 * note 1), ALGORITHM every note together; a sixth note only from the keys. The step latch selects by gesture. */
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
#define QD_PAGE_HOLD_MS 400u           /* F1 + a page key held this long: the page in / out of the loop */
#define QD_SEL_HOLD_MS 400u            /* SEL + W1..W8 held this long: solo (a tap: mute) */
#define QD_SEQ_HOLD_MS 400u            /* SEQ + W held this long: the pattern at once (a tap: queued) */
#define QD_CLEAR_HOLD_MS 600u          /* F1 + B9 held this long: the track cleared (a tap: the page) */
#define QD_PREVIEW_MS 300u             /* a note turned on a knob sounds this long */
#define QD_OCT_SLIDE_MS 350u           /* the octave takeover's keyboard slides this long */
/* the OCT LEDs away from the centre: a blink's half period one, two, three or more octaves (banks) away */
#define QD_OCT_BLINK1_MS 500u
#define QD_OCT_BLINK2_MS 250u
#define QD_OCT_BLINK3_MS 125u

enum { QD_MODE_IDLE, QD_MODE_STEP, QD_MODE_KEYS, QD_MODE_TRACKS };
enum { QD_REC_OFF, QD_REC_STEP, QD_REC_LIVE };
enum { QD_VIEW_ROLL, QD_VIEW_STEPS, QD_NVIEW };   /* P1.3e: the sequencer's views, HOME cycles them */
/* the hold modes (B1..B10): UI_F_* (ui.h) */
enum { QD_LAYER_NONE, QD_LAYER_F1, QD_LAYER_F2, QD_LAYER_F3 };
/* the takeovers: UI_TK_* (ui.h), and the sound picker */
#define QD_TK_NONE (-1)
#define QD_TK_PRESET 100               /* the pool picker: the track's sound, or the held steps' sound lock */
#define QD_TK_DEFAULT 101              /* a default turned, nothing held (P1.3e: the mode's full TRACK DEFAULT picture) */
#define QD_TK_HELDKNOB 102             /* P1.3d: a knob turned with a step held: the band over the held step's screen */

/* P1.3d (§8b 2.2): the knobs' jobs in the hold modes (qd_ctl_knob_job: one table, [mode][SELECT, KNOB 1..4]); with a
 * step held they edit the held steps, with nothing held KNOB 1..4 the track's defaults (SELECT is then the page) */
/* NOTE1..5: SELECT, KNOB 1..4 in NOTE, each moving its note alone (P1.3f); QUIET (P1.3e), the main value with no
 * overlay (KNOB 4 wherever it has no other job: the lane or the held screen changes in place) */
enum { QD_KJ_NONE, QD_KJ_MAIN, QD_KJ_NOTE1, QD_KJ_NOTE2, QD_KJ_NOTE3, QD_KJ_NOTE4, QD_KJ_NOTE5,
       QD_KJ_TIE, QD_KJ_STRUM, QD_KJ_RAMP, QD_KJ_COND, QD_KJ_CHANCE, QD_KJ_QUIET, QD_NKJ };

/* P1.3d (§8b 2.2): latched menus and confirmations (ctl_latch.c). One rule for all: OCT- = back / exit / cancel,
 * OCT+ = yes / confirm / enter; HOME closes any */
enum { QD_LATCH_NONE, QD_LATCH_F3, QD_NLATCH };
typedef struct qd_latch {
    const char *name;                  /* "F3" */
    const char *back, *confirm;        /* what OCT- and OCT+ say on screen ("EXIT"; confirm 0: OCT+ does nothing) */
    void (*on_back)(struct qd_app *a);         /* OCT-: 0 = close */
    void (*on_confirm)(struct qd_app *a);      /* OCT+: 0 = nothing (reserved) */
    void (*on_close)(struct qd_app *a);        /* closed (by OCT-, HOME, leaving the step view): its state let go */
} qd_latch_t;

#define QD_NROLL 32

typedef struct {
    int mode;                          /* QD_MODE_* */
    int view;                          /* P1.3e: the sequencer's view (QD_VIEW_*): kept for the session */
    int home_woke;                     /* (the HOME press that woke the unit: it switches nothing) */
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
    int held_empty;
    /* P1.3e: a mode key (B1..B9) held with nothing else held: "the track" for that mode (its default) */
    int mkey;                          /* the mode held (-1 none) */
    uint32_t mkey_t0;                  /* when it went down .. */
    int mkey_used;                     /* .. a knob or B11 used it (its picture at once) */                    /* the first step held was empty when it was held */
    /* P1.3d: the step latch (LFO): the held steps are a selection that stays (held_key -1); a W selects */
    int slatch;
    int gest_n, gest_sole;             /* P1.3f: the latch's gesture (every W from the first down to all up): its key
                                        * presses, and the step that was the whole selection before it (-1) */
    int last_step[8];                  /* the last step held or tapped, per track (-1 none): the latch starts there */
    uint32_t chord_ms;                 /* NOTE: the chord changed (the name squeezes in) */
    uint32_t roll_ms;                  /* SOUND / LOCKS: the last roll .. */
    int roll_dir;
    uint32_t held_play_ms;             /* the held step last played (the mallet strikes, the reels turn) */
    int held_played;                   /* .. it has, since it was held */
    uint64_t flash_seen;               /* the selected track's last trig seen (the sequencer's flash_sample) */
    /* NOTE entry (Form's "pressed") and SOUND's banks */
    int entry_oct;                     /* the octave B1's degree starts at */
    uint16_t note_keys;                /* bit b: B(b+1) is down as a note key */
    uint8_t entry_chord[QD_NNOTE];     /* the notes entered while the note keys are down (Form's "pressed") */
    int entry_n;
    int sound_bank;                    /* SOUND: the bank of ten on B1..B10 (0: sounds 1..10) */
    /* the layers: F1 / F2 / F3 (OCT- / OCT+ / both; P1.3d: F3 is a latched menu) */
    int latch;                         /* the latched menu open (QD_LATCH_*) */
    uint8_t oct_eaten;                 /* bit 0 OCT-, bit 1 OCT+: its press was a latch's (no layer until it is up) */
    uint32_t f_ms;                     /* when the layer's first key went down */
    uint16_t lk_down;                  /* bit b: B(b+1) went down in the layer (its release goes there too) */
    uint32_t pg_t0[8];                 /* F1: a page key's press */
    uint16_t pg_done;                  /* F1: bit p: page key p's press is used (held: toggled; a paste; a range) */
    uint32_t pg_up[8];                 /* F1: a page key's last tap's release (a double tap within QD_CLICK_MS) .. */
    int pg_taps[8];                    /* .. it was a tap */
    int pg_range;                      /* F1: a range was made: no taps until the page keys are up */
    int pg_copied;                     /* F1: the page copied this hold (-1 none): B1..B8 paste it */
    uint32_t b9_t0;                    /* F1: B9's press .. */
    int b9_down, b9_done;              /* .. down; held long enough: the track cleared */
    int f1_copied;                     /* F1: a step copied this hold: the next W pastes it */
    int f2_loaded, f2_holding;         /* F2: Form's seqF2Loaded_ / seqF2Holding_ (the hand) */
    int f2_last_w, f2_last_act;        /* F2: the last W and what it did (1 CUT, 2 PASTE): the map shows it */
    int f3_anchor;                     /* F3: the white key held first (-1 none) */
    /* SEL and SEQ held (P1.3c): their white keys' presses (a tap / a hold) */
    uint16_t sel_down, sel_done, seq_down, seq_done;
    uint32_t sel_t0[8], seq_t0[16];
    int seq_bank;                      /* P1.3d: the bank SEQ's W keys and map show (SEQ + B1..B4) */
    uint8_t seq_pat[16];               /* .. the pattern each W went down on */
    /* the takeover (PLAN §9a rule 5) */
    int tk;                            /* QD_TK_NONE, UI_TK_*, QD_TK_PRESET, QD_TK_DEFAULT */
    uint32_t tk_ms;                    /* the last detent */
    int tk_dir;
    int tk_lock;                       /* a p-lock (or the sound lock) on the held step (or the step recorded live) */
    int tk_step;                       /* .. that step */
    int tk_knob;                       /* the colour, 0..3 */
    int tk_job, tk_src;                /* P1.3d: the band's job (QD_KJ_*) and the knob turned (QD_K_*) */
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
    uint8_t rec_chord[QD_NNOTE];
    int rec_nchord;
    int rec_step;                      /* live: the step the chord went into (-1 none) */
    int64_t rec_t0[128];               /* each note's start (live: ticks; step: 0), -1 not down in the recording */
    int16_t rec_s[128];                /* live: the step each note went into */
    int rec_live;                      /* live notes are down (the transport stopping ends them) */
    struct { int64_t t0, t1; uint8_t note; } roll[QD_NROLL];   /* live: the notes recorded, t1 -1 while down */
    int nroll, iroll;
    /* MIDI notes into a held step */
    int midi_n;                        /* notes down */
    uint8_t midi_chord[QD_NNOTE];
    int midi_nchord;
    /* P1.3c: a note turned on a knob sounds (QD_PREVIEW_MS) */
    int pv_note, pv_track;             /* -1: none sounding */
    uint32_t pv_ms;
    /* P1.3c: the octave takeover (tk UI_TK_OCTAVE): what changed, from what to what */
    int oct_kind, oct_from, oct_to;    /* UI_OCT_* */
    uint32_t oct_ms;
    /* P1.3d: the stopped preview (ctl_preview.c): the held steps sounding, each with the notes it struck */
    int pvs_n;
    int16_t pvs_step[16];
    uint8_t pvs_track[16], pvs_nn[16];
    uint8_t pvs_note[16][QD_NNOTE];
    uint32_t pvs_sig[16];
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
/* P1.3c: the octave changed (UI_OCT_*): the takeover for a second */
void qd_ctl_octave(struct qd_app *a, int kind, int from, int to);
/* the distance from the centre the OCT LEDs show (entry octave - 4 in NOTE with a step held, the keyboard's shift, the
 * SOUND bank with a step held); 0: none shown (*shown 0: the OCT keys are no octave here) */
int qd_ctl_oct_offset(const struct qd_app *a, int *shown);
uint32_t qd_ctl_oct_blink_ms(int offset);   /* the blink's half period for a distance (0: none) */
/* a note sounded for QD_PREVIEW_MS on the selected track (a knob turned a step's note) */
void qd_ctl_preview(struct qd_app *a, int note, int vel);
/* P-LOCK's hook: the screen's lockable knobs, one table a screen (the step view now; the sound editor's pages later) */
#define QD_LOCK_SOUND (-1)             /* the knob is the sound lock (the pool's picker) */
typedef struct {
    uint8_t knob;                      /* QD_K_* */
    int16_t id;                        /* the lock id (pattern.h), or QD_LOCK_SOUND */
    int16_t tk;                        /* its takeover (UI_TK_*, QD_TK_PRESET) */
} qd_lockknob_t;
typedef struct {
    const char *name;                  /* "STEP VIEW" */
    int n;
    const qd_lockknob_t *k;
} qd_lockscreen_t;
const qd_lockscreen_t *qd_ctl_lock_screen(const struct qd_app *a);
int qd_ctl_black(int key);             /* the black key 0..10 of a note key, or -1 */
int qd_ctl_knob_job(int mode, int knob);
/* P1.3e: the mode whose default a held mode key edits (-1 none), and whether its full picture shows (held 250 ms, or
 * used) */
int qd_ctl_mkey(const struct qd_app *a);
int qd_ctl_mkey_shown(const struct qd_app *a);   /* P1.3d: QD_KJ_* of a knob (QD_K_SELECT, QD_K_1..4) in a hold mode */
void qd_ctl_select_track(struct qd_app *a, int t);

/* ---- ctl_latch.c (P1.3d) */
void qd_latch_open(struct qd_app *a, int kind);
void qd_latch_close(struct qd_app *a);
int qd_latch_oct(struct qd_app *a, int d);   /* an OCT press (d -1 / +1) with a latch open: 1 taken */
const qd_latch_t *qd_latch_cur(const struct qd_app *a);   /* the latch open, 0 none */

/* ---- ctl_preview.c (P1.3d): with the transport stopped, the held steps sound (PLAN §8b 2.2) */
int qd_ctl_previewing(const struct qd_app *a);   /* the held steps are previewed now */
void qd_ctl_preview_sync(struct qd_app *a);      /* after every input and every millisecond: start, re-strike, stop */
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
int qd_step_lock_knob(struct qd_app *a, int knob, int d);   /* a lockable knob of the screen: the held steps; 1 taken */
void qd_step_clear_locks(struct qd_app *a);         /* P-LOCK's B11: the held steps' p-locks cleared (undoable) */
void qd_step_release_all(struct qd_app *a);         /* every held step let go (HOME, a track change) */
void qd_step_notes_in(struct qd_app *a, const uint8_t *notes, int n);   /* the held steps' notes (MIDI) */
int qd_step_default_knob(struct qd_app *a, int d);  /* ALGORITHM, nothing held: the track's default of the mode */
/* P1.3d: a knob's job (QD_KJ_*) on the held steps / on the track's default; knob: QD_K_* turned (the band's badge) */
void qd_step_job_held(struct qd_app *a, int job, int knob, int d);
void qd_step_job_default(struct qd_app *a, int job, int knob, int d);
/* P1.3e: mode m's default back to the track's initial value (qd_sdef_init's; MICRO with the strum, RATCH with the
 * ramp; SOUND: the track's starting sound), and whether it is there now */
void qd_step_default_reset(struct qd_app *a, int m);
int qd_step_default_initial(const struct qd_app *a, int m);
void qd_step_latch(struct qd_app *a, int on);   /* P1.3d: the step latch (LFO) on / off */
void qd_step_latch_seed(struct qd_app *a);      /* .. its selection started (the last step edited, else the page's first) */
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
/* the screen under the takeovers (what qd_view_frame draws first): QD_SCR_* */
enum { QD_SCR_IDLE, QD_SCR_LANE, QD_SCR_HELD, QD_SCR_LAYER, QD_SCR_SHIFT, QD_SCR_KEYS, QD_SCR_TRACKS, QD_SCR_SEL,
       QD_SCR_SEQ, QD_SCR_DEFAULT };   /* (P1.3e: a mode key held: the mode's default, its full picture) */
int qd_view_screen(const struct qd_app *a);

#ifdef __cplusplus
}
#endif
