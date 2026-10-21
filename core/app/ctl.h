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
 * sound (a pool sound; P1.3q, in the STEPS view: a step cursor, the step latch's one selection moved through the
 * loop, with nothing held or latched); SHIFT + ALGORITHM = the track's scale, SHIFT + PRESETS = its root (temporary).
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
 *   LOCKS IS DROPPED (P1.3d). P1.3w: B9 is FUNC (the step functions), SOUND moved to B10.
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
 * note 1), ALGORITHM every note together; a sixth note only from the keys. The step latch selects by gesture.
 *
 * P1.3y (§8b 2.3, the control fixes and the owner's additions; supersedes the HOME, PRESETS-cursor and nothing-held
 * ALGORITHM lines above):
 *   HOME comes back to the view in use from anywhere (a latch closed in the same press), never cycles; SHIFT + HOME
 *   cycles PIANO ROLL / STEPS / EDIT.
 *   ONE MEANING A KNOB: no finger on a step (nothing held, or the step latch's selection), every view, the encoders do
 *   their role in the KNOBS layout (QD_KNOBS_*, the table KNOB_ROLES in ctl.c; A: SELECT the page (the cursor to the
 *   page's first step), ALGORITHM the step cursor, PRESETS the track's preset); a mode key held: its default; with a
 *   finger on a step as before (KNOB_JOB, PRESETS the sound lock). FX held + ALGORITHM: the track (UI_TK_TRACKBIG).
 *   LFO held + a black key: the hold mode (the latch untouched); LFO tapped: the latch.
 *   FUNC: KNOB 1 the kind, KNOB 2 LEAP's n, KNOB 3 JUMP's target; B8 LEAP, B9 JUMP; B9 held + W: the target.
 *   SHIFT + REC: the record settings (QD_LATCH_REC: MODE, QUANTIZE, OVERDUB / REPLACE, the project's); REC held arms in
 *   that MODE. SHIFT + SEL: the global settings (QD_LATCH_SET: KNOBS, PAGE FOLLOW). SEL tapped: nothing (P1.4b).
 *   The value keys light the nearest value (qd_step_value_keys).
 *
 * P1.5b (PLAN version 2.4, the owner after 0.4.1):
 *   THE EDITOR'S KEYBOARD: REC tapped in the sound editor turns its keyboard on (ekeys; EDIT from the keyboard keeps
 *   it on): SCALE (the record settings' EDIT KEYS, KNOB 4) the W keys the track's scale from the root at the keyboard's
 *   octave, the black keys its pages; FULL every key as the keyboard, the pages on SELECT. F1 / F2 the octave. EDIT
 *   closes it back to the keyboard. SHIFT + ALGORITHM / PRESETS the scale / root with the keyboard on (the keyboard's
 *   too); the keys lit by the scale (qd_keys_leds: roots 80 %, the scale 50 %, playing full).
 *   LOCK CHIPS (the screens): no padlock; filled = locked on the held step, outlined = locked on some step.
 *   FX: B1 OVERVIEW (where it opens: the sends; W1..W8 the track, KNOB 1..3 its sends), B2 DELAY, B3 REVERB, B4
 *   CHORUS, B5 MASTER, B6 ROUTING (qd_fx.h QD_FXP_XDR .. XPOS, all 0: ChoralRoot's chain).
 *   ACCELERATION: qd_ctl_acc (every wide value; 24 steps or fewer never; SHIFT never).
 *   LATCH MODES (P1.5h): LFO held alone QD_LFO_HOLD_MS opens the latch-mode screen while held (lmode_open), SELECT
 *   turns VALUES / MODES there (set at once), the release closes it (no toggle); in MODES, no finger on a
 *   step, the black keys choose the hold mode (the knobs set the selection's values as ever). */
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
#define QD_UNFOLD_MS 240u              /* P1.3j: STEPS, a page change: the new page's row unfolds, the old one folds */
#define QD_ENTRY_OCT 4                 /* NOTE's entry octave at power-on (the root at C4 = MIDI 60) */
#define QD_PAGE_HOLD_MS 400u           /* F1 + a page key held this long: the page in / out of the loop */
#define QD_SEL_HOLD_MS 400u            /* SEL + W1..W8 held this long: solo (a tap: mute) */
#define QD_SEQ_HOLD_MS 400u            /* SEQ + W held this long: the pattern at once (a tap: queued) */
#define QD_CLEAR_HOLD_MS 600u          /* F1 + B9 held this long: the track cleared (a tap: the page) */
#define QD_PLAY_HOLD_MS 400u           /* P1.3x: PLAY held this long: F3 (a tap plays / stops on its release) */
#define QD_REC_HOLD_MS 400u            /* P1.3x: REC held this long: live recording armed (a tap: the keyboard) */
#define QD_PREVIEW_MS 300u             /* a note turned on a knob sounds this long */
#define QD_OCT_SLIDE_MS 350u           /* the octave takeover's keyboard slides this long */
/* the OCT LEDs away from the centre: a blink's half period one, two, three or more octaves (banks) away */
#define QD_OCT_BLINK1_MS 500u
#define QD_OCT_BLINK2_MS 250u
#define QD_OCT_BLINK3_MS 125u

enum { QD_MODE_IDLE, QD_MODE_STEP, QD_MODE_KEYS, QD_MODE_TRACKS };
enum { QD_REC_OFF, QD_REC_STEP, QD_REC_LIVE };
enum { QD_VIEW_ROLL, QD_VIEW_STEPS, QD_VIEW_EDIT, QD_NVIEW };   /* P1.3e: the sequencer's views, HOME cycles them
                                                                * (P1.3r: EDIT, the page's row under the mode's picture) */
#define QD_VIEW_ROWS(v) ((v) == QD_VIEW_STEPS || (v) == QD_VIEW_EDIT)   /* P1.3r: the views drawing the pages' rows */
/* the hold modes (B1..B10): UI_F_* (ui.h) */
enum { QD_LAYER_NONE, QD_LAYER_F1, QD_LAYER_F2, QD_LAYER_F3 };
/* the takeovers: UI_TK_* (ui.h), and the sound picker */
#define QD_TK_NONE (-1)
#define QD_TK_PRESET 100               /* the pool picker: the track's sound, or the held steps' sound lock */
#define QD_TK_DEFAULT 101              /* a default turned, nothing held (P1.3e: the mode's full TRACK DEFAULT picture) */
#define QD_TK_HELDKNOB 102             /* P1.3d: a knob turned with a step held: the band over the held step's screen */
#define QD_TK_LMODE 103                /* P1.5b: the latch mode's screen (P1.5h: only while LFO is held: lmode_open) */
#define QD_LFO_HOLD_MS 400u            /* P1.5b: LFO held this long alone: the latch mode's screen (P1.5h: while held) */
enum { QD_LMODE_VALUES, QD_LMODE_MODES };   /* P1.5b: with the step latch on, the black keys: the values / the hold modes */

/* P1.3d (§8b 2.2): the knobs' jobs in the hold modes (qd_ctl_knob_job: one table, [mode][SELECT, KNOB 1..4]); with a
 * step held they edit the held steps, with nothing held KNOB 1..4 the track's defaults (SELECT is then the page) */
/* NOTE1..5: SELECT, KNOB 1..4 in NOTE, each moving its note alone (P1.3f); QUIET (P1.3e), the main value with no
 * overlay (KNOB 4 wherever it has no other job: the lane or the held screen changes in place) */
enum { QD_KJ_NONE, QD_KJ_MAIN, QD_KJ_NOTE1, QD_KJ_NOTE2, QD_KJ_NOTE3, QD_KJ_NOTE4, QD_KJ_NOTE5,
       QD_KJ_TIE, QD_KJ_STRUM, QD_KJ_RAMP, QD_KJ_COND, QD_KJ_CHANCE, QD_KJ_QUIET,
       QD_KJ_FTYPE, QD_KJ_LEAP, QD_KJ_JTGT,   /* P1.3y, FUNC: the function's kind, LEAP's n, JUMP's target */
       QD_NKJ };

/* P1.3d (§8b 2.2): latched menus and confirmations (ctl_latch.c). One rule for all: OCT- = back / exit / cancel,
 * OCT+ = yes / confirm / enter; HOME closes any */
enum { QD_LATCH_NONE, QD_LATCH_F3,
       QD_LATCH_REC,                    /* P1.3y: SHIFT + REC: the record settings (MODE, QUANTIZE, OVERDUB) */
       QD_LATCH_SET,                    /* P1.3y: SHIFT + SEL: the global settings (KNOBS, PAGE FOLLOW) */
       QD_LATCH_SND,                    /* P1.4a: EDIT (printed): the sound editor (ctl_snd.c) */
       QD_LATCH_TRK,                    /* P1.4b: SEL tapped: the track settings (ctl_mix.c) */
       QD_LATCH_MIX,                    /* P1.4b: ENV (printed) = MIX: the faders, again the sends */
       QD_LATCH_FX,                     /* P1.4b: FX (printed) tapped: the effects */
       QD_LATCH_LIB,                    /* P1.5a: SAVE: the sounds page (the LIBRARY, the POOL; ctl_lib.c) */
       QD_LATCH_NAME,                   /* P1.5a: B10 there: the name editor */
       QD_LATCH_PRJ,                    /* P1.5a: SEQ tapped: the project, the projects */
       QD_NLATCH };
#define QD_LATCH_MENU(l) ((l) == QD_LATCH_REC || (l) == QD_LATCH_SET || (l) == QD_LATCH_TRK)   /* a settings menu:
                                                         * its own screen, KNOB 1..4 */
#define QD_LATCH_PANEL(l) ((l) == QD_LATCH_TRK || (l) == QD_LATCH_MIX || (l) == QD_LATCH_FX)   /* (P1.4b: B1..B8 the
                                                         * track, B1..B4 the effect; the W keys nothing) */

/* P1.3y (the owner's addition): the encoders' layouts (the global settings' KNOBS), one table of roles: what SELECT,
 * ALGORITHM and PRESETS do with no finger on a step (nothing held, or the step latch's selection), in the PIANO ROLL
 * and in STEPS / EDIT. With a finger on a step every layout is the same (KNOB_JOB, the sound lock on PRESETS).
 *   A  SELECT page    ALGORITHM cursor  PRESETS preset                 (P1.3y)
 *   B  SELECT page    ALGORITHM value   PRESETS preset (ROLL) / cursor (STEPS, EDIT)   (P1.3q)
 *   C  SELECT cursor  ALGORITHM page    PRESETS preset
 *   D  SELECT cursor  ALGORITHM value   PRESETS preset                 (the cursor carries the page)
 * value: the mode's value, the latch's selection's (as a finger) or else the track's default (P-LOCK: the track) */
enum { QD_KR_NONE, QD_KR_PAGE, QD_KR_CURSOR, QD_KR_PRESET, QD_KR_VALUE, QD_NKR };
enum { QD_KNOBS_A, QD_KNOBS_B, QD_KNOBS_C, QD_KNOBS_D, QD_NKNOBS };
int qd_ctl_sel_map(const struct qd_app *a);
int qd_ctl_seq_map(const struct qd_app *a);
int qd_ctl_latch_modes(const struct qd_app *a);   /* P1.5b: the latch's selection in MODES: the black keys the modes */       /* P1.5a: SEQ's pattern map is up (held a moment, or used) */       /* P1.3y: SEL's track map is up (held a moment, or a key used) */
int qd_ctl_knob_role(const struct qd_app *a, int knob);           /* the encoder's role now (QD_KR_*; KNOB 1..4: NONE) */
int qd_ctl_layout_role(int layout, int rows, int knob);          /* .. in a layout (rows: STEPS / EDIT) */
const char *qd_ctl_role_name(int role);                           /* "PAGE", "CURSOR", "PRESET", "VALUE" */
typedef struct qd_latch {
    const char *name;                  /* "F3" */
    const char *back, *confirm;        /* what OCT- and OCT+ say on screen ("EXIT"; confirm 0: OCT+ does nothing) */
    void (*on_back)(struct qd_app *a);         /* OCT-: 0 = close */
    void (*on_confirm)(struct qd_app *a);      /* OCT+: 0 = nothing (reserved) */
    void (*on_close)(struct qd_app *a);        /* closed (by OCT-, HOME, leaving the step view): its state let go */
} qd_latch_t;

#define QD_NROLL 32

/* P1.5a (ctl_lib.c): the sounds page's, the name editor's and the project pages' state */
typedef struct {
    int tab;                           /* 0 the LIBRARY, 1 the POOL */
    int chosen;                        /* the slot (0..127; the pool's number - 1) */
    int storing;                       /* SAVE opened it: the selected track's sound to store into chosen .. */
    char sname[QD_SOUND_NAME];         /* .. under this name (from the sound: spark) */
    int spark, namevar;
    int armed;                         /* SAVE onto a sound: SAVE again replaces it */
    int offer;                         /* stored: F2 adds it to the pool */
    int w_down;                        /* a W down (1 + its index) .. */
    uint32_t w_ms;
    int aud;                           /* .. held: heard while held, on the screen */
    int pv_on, pv_track;               /* a sound heard */
    uint32_t pv_ms;
    int f1_down, f1_src, f1_tab, f1_armed;   /* F1 + W: copied from (tab, slot), a paste armed onto a sound */
    int f2_down, f2_pick;              /* F2 + W: picked up */
    int oct_used;                      /* bit 0 F1, 1 F2: used with a W (their release is no tap) */
    int b11_down, b11_used;            /* B11 + W: delete */
    uint8_t del_tracks;                /* .. what it took (its users flash) */
    int del_steps, del_kind;
    char del_name[QD_SOUND_NAME];
    int fx_kind, fx_slot;              /* an animation: 1 stored, 2 added (or pasted), 3 deleted, 4 onto the track, 5
                                        * moved */
    uint32_t fx_ms;
} qd_libui_t;
typedef struct {
    char buf[QD_SOUND_NAME];           /* twelve cells (spaces), the end */
    int cur, variant;
    int target, proj;                  /* 0 the chosen sound, 1 the name being stored, 2 project proj */
    int ret;                           /* the latch it came from */
    uint32_t roll_ms;                  /* the letter drum turned (its roll) */
    int roll_dir;
} qd_nameui_t;
typedef struct {
    int page;                          /* 0 the project, 1 the projects */
    int chosen;                        /* the projects' W chosen */
    int f1_down, src, armed;           /* F1 + W: copy from src (onto one that exists: armed) */
    int b11_down, b11_used;
    int oct_used;
} qd_prjui_t;

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
    int slide_from;
    uint32_t tempo_ms;                 /* P1.3j: the tempo or the swing last turned (the F3 map lights it a second) */                    /* .. from this page (P1.3j: STEPS folds it as the new one unfolds) */
    /* the white keys in the step view */
    uint16_t wk_down;                  /* bit w: white key w is down as a step key */
    uint16_t wk_target;                /* P1.3w: .. as a jump's target (FUNC: not held, not a tap) */
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
    int scursor;                       /* P1.3q: the latch's selection is PRESETS' cursor (STEPS): the STEPS view stays */
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
    int lfo_used;                      /* P1.3y: LFO held picked a hold mode (B1..B11): its release is no tap */
    uint32_t lfo_ms;                   /* P1.5b: LFO went down (held QD_LFO_HOLD_MS alone: the latch mode) */
    int lmode;                         /* P1.5b: the latch mode (QD_LMODE_*; a global setting, saved) */
    int lmode_open;                    /* P1.5h: LFO held alone: the latch mode's screen is up (SELECT turns it) */
    int fn_leap, fn_target;            /* P1.3y, FUNC: the last LEAP's n (2) and JUMP's target (-1: the step's page start) */
    int fn_b9;                         /* .. B9 (JUMP) is down: a W pressed now is the target */
    int knobs;                         /* P1.3y: the encoders' layout (QD_KNOBS_*; the global settings) */
    int ekeys;                         /* P1.5b: the keyboard on in the sound editor (REC tapped there) */
    int snd_lk, snd_lk_used, snd_lk_k, snd_lk_id;   /* P1.5k: an LFO key held in the editor (1..3), a knob used with it,
                                        * the knob slot and the value it mapped .. */
    uint32_t snd_lk_ms, snd_lk_bad_ms; /* .. when it went down; a value that is no destination turned (its shake) */
    int snd_page;                      /* P1.4a: the sound editor's page (QD_SND_PAGES) .. */
    int snd_knob;                      /* .. the knob last turned there (0..3 KNOB 1..4, 4 ALGORITHM; -1 none) .. */
    uint32_t snd_ms;                   /* .. and when */
    int trk_page, mix_page, fx_page;   /* P1.4b: the track settings' page (0, 1), the mix's (0 faders, 1 sends), the
                                        * effects' (QD_FX_PAGES) */
    int menu_knob;                     /* .. the knob turned last there (0..3, -1) */
    uint32_t knob_ms[8];               /* P1.5b: each knob's last detent (acceleration) .. */
    int knob_dir[8];                   /* .. its direction */
    uint32_t knob_dt;                  /* .. the time since it for the turn now (QD_ACC_SLOW_MS and up: none) */
    qd_libui_t lb;                     /* P1.5a: the sounds page */
    qd_nameui_t nm;                    /* .. the name editor */
    qd_prjui_t pj;                     /* .. the project pages */
    uint32_t seq_ms;                   /* P1.5a: SEQ went down (its map after QD_TAP_MS, or once used) .. */
    int seq_used;                      /* .. a key or a knob used it (its release is no tap) */
    uint32_t sel_ms;                   /* P1.3y: SEL went down (its map after QD_TAP_MS, or once a key is used) .. */
    int sel_used;                      /* .. a key used it */
    int rec_down_used;                 /* REC + PLAY: REC's release is not a tap */
    uint32_t rec_down_ms;              /* P1.3x: REC went down (held QD_REC_HOLD_MS: live recording armed) */
    uint32_t play_down_ms;             /* P1.3x: PLAY went down (held QD_PLAY_HOLD_MS: F3); .. */
    int play_used;                     /* .. its release is no tap */
    int live_wait;                     /* P1.3x: live recording armed, stopped: the first note starts it all */
    int rec_grow;                      /* P1.3x: an empty track recorded: its loop grows a page as it is reached .. */
    int grow_reached;                  /* .. the last page reached (the loop at the stop) */
    int shift_used;                    /* SHIFT did something while down (its map then stops) */
    /* the keyboard (ctl_rec.c) */
    int rec;                           /* recording armed in the keyboard (step recording stopped, live playing) */
    int rec_kind;                      /* P1.3y: QD_REC_STEP (the cursor, playing or not) / QD_REC_LIVE (playing) */
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

/* ---- ctl_snd.c (P1.4a, built through ctl_latch.c): the sound editor. Its pages: name, group (a black key B1..B5),
 * drawing (ui.h UI_SD_*), the patch ids on KNOB 1..4 and ALGORITHM (-1 none), the drawing's argument (the envelope,
 * the LFO). Every one of the sound's QD_QUAD_NP values is on exactly one page */
enum { QD_SG_TRIG, QD_SG_OPS, QD_SG_OPENV, QD_SG_FILTER, QD_SG_AMP, QD_SG_LFO1, QD_SG_LFO2, QD_SG_LFO3,
       QD_NSG };                       /* P1.5l: a group a black key, B1..B8 */
int qd_snd_page_of(const char *name);   /* P1.5l: a page's index by its name ("FILTER"; 0 none) */
#define QD_SND_START_PAGE 2            /* P1.5l: a fresh start: RATIO (B2), the sound's first page */
#define QD_SND_NPAGE 18
typedef struct {
    const char *name;
    uint8_t group, draw;
    int8_t id[5];
    uint8_t arg;
} qd_sndpage_t;
extern const qd_sndpage_t QD_SND_PAGES[QD_SND_NPAGE];
extern const char *const QD_SND_GROUP_NAME[QD_NSG];
int qd_snd_open(const struct qd_app *a);
int qd_snd_keys(const struct qd_app *a);        /* P1.5b: the editor open with the keyboard on */
int qd_snd_scale_note(const struct qd_app *a, int w);   /* .. SCALE: W w's note (the scale's degree w from the root) */
int qd_keys_on(const struct qd_app *a);         /* P1.5b: the keyboard (the keyboard mode, or the editor's) */
void qd_keys_leds(const struct qd_app *a, uint8_t *key);   /* P1.5b: the keys lit by the scale (27 levels) */
void qd_snd_button(struct qd_app *a);           /* EDIT: open / close */
void qd_snd_black(struct qd_app *a, int b);     /* B1..B5 the groups (again: the next page), B11 the page's locks */
void qd_snd_lfo_key(struct qd_app *a, int n, int down);   /* P1.5k: B6..B8 (n 0..2): tapped its page, held + a knob: the
                                                         * knob's value its destination, the depth */
int qd_snd_knob(struct qd_app *a, int knob, int d);   /* 1: taken */
void qd_snd_leds(const struct qd_app *a, uint8_t *black);   /* the eleven black keys' LEDs */
const qd_lockscreen_t *qd_snd_lock_screen(int page);

/* ---- ctl_mix.c (P1.4b, built through ctl_latch.c): the track settings, the mix, the effects. The effects' pages: a
 * cell is a qd_fx.h parameter, QD_FXC_SEND (the selected track's send into the page's effect), QD_FXC_TIME /
 * QD_FXC_SPREAD (the free or the synced one by SYNC), or none */
enum { QD_FXC_NONE = -1, QD_FXC_SEND = -2, QD_FXC_TIME = -3, QD_FXC_SPREAD = -4 };
#define QD_FXC_SENDX(fx) (-10 - (fx))  /* P1.5b, the overview: the selected track's send into effect fx (0..2) */
#define QD_FX_NPAGE 8
/* P1.5b: the effects (the page's group): 0 DELAY 1 REVERB 2 CHORUS 3 MASTER, 4 the OVERVIEW (the sends), 5 ROUTING;
 * the black keys B1..B6: OVERVIEW, DELAY, REVERB, CHORUS, MASTER, ROUTING (QD_FX_KEY) */
enum { QD_FXG_DELAY, QD_FXG_REVERB, QD_FXG_CHORUS, QD_FXG_MASTER, QD_FXG_OVER, QD_FXG_ROUTE, QD_NFXG };
#define QD_FX_NKEY 6
extern const uint8_t QD_FX_KEY[QD_FX_NKEY];
typedef struct {
    const char *name;
    uint8_t fx, draw;                  /* the group (QD_FXG_*), ui.h UI_FXD_* */
    int8_t c[4];
} qd_fxpage_t;
extern const qd_fxpage_t QD_FX_PAGES[QD_FX_NPAGE];
extern const char *const QD_FX_NAME[QD_NFXG];
int qd_fx_send(const struct qd_app *a, int t, int fx);   /* track t's send into effect fx (0 DELAY 1 REVERB 2 CHORUS) */
int qd_fx_cell_id(int c);               /* a cell's parameter now (TIME, SPREAD: by SYNC) */
int qd_fx_send_id(int fx);              /* the mix lock id of a track's send into effect fx */
void qd_trk_button(struct qd_app *a);   /* SEL tapped: open / the next page */
void qd_mix_button(struct qd_app *a);   /* ENV (MIX): open / the sends / the faders */
void qd_fx_button(struct qd_app *a);    /* FX tapped: open / the next page */
void qd_panel_black(struct qd_app *a, int b);
void qd_panel_white(struct qd_app *a, int w);     /* P1.5b: the effects' overview: W1..W8 the track */
int qd_trk_knob(struct qd_app *a, int knob, int d);
int qd_mix_knob(struct qd_app *a, int knob, int d);
int qd_fx_knob(struct qd_app *a, int knob, int d);
void qd_panel_leds(const struct qd_app *a, uint8_t *black);

/* ---- ctl_lib.c (P1.5a, built through ctl_latch.c): the sounds page, the name editor, the project pages */
int qd_lib_open(const struct qd_app *a);         /* one of them open */
void qd_lib_save(struct qd_app *a);              /* SAVE */
void qd_prj_button(struct qd_app *a);            /* SEQ tapped */
void qd_lib_back(struct qd_app *a);
void qd_name_open(struct qd_app *a, int target, int proj);
void qd_lib_key(struct qd_app *a, int w, int b, int down);
int qd_lib_knob(struct qd_app *a, int knob, int d);
int qd_lib_oct(struct qd_app *a, int d, int down);   /* 1 taken */
void qd_lib_ms(struct qd_app *a);
const qd_sound_t *qd_lib_sound(const struct qd_app *a, int tab, int slot);
void qd_ctl_set_mode(struct qd_app *a, int mode);
int qd_ctl_black(int key);             /* the black key 0..10 of a note key, or -1 */
int qd_ctl_knob_job(int mode, int knob);
/* P1.5b: knob acceleration. A detent's step grows with the spin: detents closer than QD_ACC_SLOW_MS move further, up
 * to about a sixteenth of the range a detent at QD_ACC_FAST_MS or less; a value of QD_ACC_MIN_STEPS steps or fewer
 * (an enumeration) never accelerates, nor does SHIFT (the finest step). steps: the value's range in detents */
#define QD_ACC_SLOW_MS 120u
#define QD_ACC_FAST_MS 15u
#define QD_ACC_MIN_STEPS 24
int qd_knob_accel(int d, uint32_t dt_ms, int steps, int fine);   /* the pure curve (dt: since the last detent) */
int qd_ctl_acc(const struct qd_app *a, int d, int steps);         /* .. for the knob turning now */
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
void qd_step_lock_fine(struct qd_app *a, int id, int d);   /* P1.4a: id locked on the held steps, d finest steps */
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
void qd_step_cursor(struct qd_app *a, int d);   /* P1.3q: PRESETS in STEPS: the latch's one selection moved d steps */
void qd_step_cursor_page(struct qd_app *a);     /* P1.3y: the selection to the page's first step (a page change) */
int qd_step_quick_id(int k);           /* the quick page's lock id of KNOB k (FREQ, DECAY, HARM, FDBK) */
int qd_step_id_detent(int id);         /* a detent's size for a value (hundredths for the hi-res ones) */
int qd_step_id_steps(int id);          /* P1.5b: its range in detents (acceleration) */
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
void qd_rec_arm_live(struct qd_app *a);         /* P1.3x: REC held: the keyboard, live recording armed */
void qd_rec_end(struct qd_app *a);              /* P1.3x: the recording ends (REC / PLAY), playback goes on */
void qd_rec_arm_step(struct qd_app *a);         /* P1.3y: REC held, the settings' STEP: step recording armed */
int qd_rec_stepping(const struct qd_app *a);    /* P1.3y: step recording (the cursor) is what the keys do now */
void qd_rec_ms(struct qd_app *a);               /* P1.3x: an empty track's loop growing as it is recorded */
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
       QD_SCR_SEQ, QD_SCR_DEFAULT,     /* (P1.3e: a mode key held: the mode's default, its full picture) */
       QD_SCR_EDIT,                    /* P1.3r: the EDIT view (holding, latching, a mode key: all in it) */
       QD_SCR_MENU,                    /* P1.3y: a settings menu (the record settings, the global settings) */
       QD_SCR_SOUND,                   /* P1.4a: the sound editor */
       QD_SCR_MIX, QD_SCR_FX,          /* P1.4b: the mix, the effects */
       QD_SCR_LIB, QD_SCR_NAME, QD_SCR_PRJ };   /* P1.5a: the sounds page, the name editor, the project pages */
/* P1.3r: a knob's takeover (band, picture, picker) drawn over the screen now: never over EDIT, whose top is the pop-up */
int qd_view_overlay(const struct qd_app *a);
struct ui_snd;
void qd_view_snd(struct qd_app *a, struct ui_snd *v);   /* P1.4a: the sound editor's model (the tests read it) */
int qd_view_screen(const struct qd_app *a);

#ifdef __cplusplus
}
#endif
