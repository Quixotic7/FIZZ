/* SPDX-License-Identifier: GPL-3.0-only */
/* QUADRILLE's screens (PLAN.md §9a, the LINE look). Each screen draws one frame through qd_gfx from a small view
 * record the app fills: the screens hold no state and read no clock, so a frame is a function of its view.
 *
 * The kit (ui_kit.c) is mockups/line.html's helpers, call for call: ui_T = T, ui_TW = TW, ui_lab = lab, ui_P = P,
 * ui_O = O, ui_RR = RR. Alpha, a dash or a squeeze go around a call with qd_save / qd_alpha / qd_dash / qd_scale /
 * qd_restore, as the helpers' own save / restore do. */
#pragma once
#include "qd_gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- the palette (line.html's C): colour means a knob (K1 blue .. K4 orange), red is time, greys are structure */
#define UI_BG 0x000000u
#define UI_K1 0x3f7dffu
#define UI_K2 0x1fd47eu
#define UI_K3 0xf1f0eau
#define UI_K4 0xff6a2cu
#define UI_KSEL 0xffd23fu               /* P1.3c: SELECT's colour where SELECT moves a part of the drawing (NOTE's note 1) */
#define UI_KALG 0xff8fc8u               /* P1.3e: NOTE's note 6 (from the keys; P1.3f: no knob has it) */
#define UI_RED 0xff3448u
#define UI_G1 0x8d8d96u
#define UI_G2 0x4b4b54u
#define UI_G3 0x26262du
extern const qd_rgb UI_K[4];

#define UI_TAU 6.28318530718f
#define UI_PI 3.14159265359f
float ui_ease(float x);                 /* smoothstep on 0..1, clamped (line.html's ease) */
char *ui_itoa(char *out, int n);        /* n in decimal (out: 12 bytes) */

/* ---- the kit */
float ui_T(const char *s, float x, float y, float size, int weight, float ls, qd_rgb col, int align);
float ui_TW(const char *s, float size, int weight, float ls);
void ui_lab(const char *s, float x, float y, qd_rgb col, int align);   /* 8 px, 600, spacing 1: a label */
void ui_P(const float *pts, int npts, qd_rgb col, float lw, int close, qd_rgb fill);   /* fill QD_NONE: none */
void ui_O(float x, float y, float r, qd_rgb col, float lw, qd_rgb fill);              /* col QD_NONE: no stroke */
void ui_RR(float x, float y, float w, float h, float r, qd_rgb col, float lw, qd_rgb fill);   /* inset by lw / 2 */

/* ---- screen 1: idle (P1.3o): FIZZ, the logo: champagne bubbles close up, grey on black (ui_idle.c) */
typedef struct {
    int bpm;                            /* the tempo (the spray's beat; not shown) */
    int playing;                        /* a DAW plays it: a surge of bubbles on each beat */
    float t;                            /* seconds since power-on: the lazy stream's clock */
    double beats;                       /* beats since PLAY (playing) */
} ui_idle_t;
void ui_idle_draw(const ui_idle_t *v);

/* ---- the kit's drawings shared by screens (line.html's box, head, keyboard) */
void ui_box(float x, float y, int n, qd_rgb col);   /* the track's number in a little box */
/* the top line: [n] left .......... right (n 0: no box) */
void ui_head(const char *left, const char *right, int n, qd_rgb lcol, qd_rgb rcol);
/* a line-drawn keybed of two octaves (14 white keys) from note lo (a C) at x0, y0, w x h; fill[note] (128 entries, or
 * 0) is the colour a key is filled with, QD_NONE: outlined only */
void ui_keyboard(float x0, float y0, float w, float h, int lo, const qd_rgb *fill);

/* ---- KEYS (line.html s18 without the recording roll): the keybed plays the selected track */
#define UI_NVOICE_DOTS 16
typedef struct {
    int track;                          /* 1..8 */
    const char *name;                   /* the track's */
    int preset;                         /* the sound: its pool number (1 INIT, 2..17 the factory sounds, ..) */
    const char *sound;                  /* its name */
    int edited;
    uint8_t voice[UI_NVOICE_DOTS];      /* the pool's first 16 voices: 0 free, 1 another track's, 2 this track's */
    int nvoice;                         /* dots drawn (<= 16) */
    uint8_t sounding[128];              /* the track's notes sounding (key held) */
    int octave;                         /* -2 .. +2 */
    /* P1.3: recording (PLAN §8 KEYS mode). rec 0: playing only (the P1.1 screen). rec 1: STEP recording (stopped): the
     * P1.1 screen with the cursor's page as 16 little squares under the sound (trigs filled grey, the cursor's square
     * outlined red and blinking with blink), "STEP REC" and the cursor's step at the foot. rec 2: LIVE recording
     * (line.html s18): the RECORDING badge (blinking with blink), the roll scrolling in from the red line, the keybed
     * small at the foot, "OCT n · STEP s" */
    int rec;
    int blink;                          /* the blink's lit half */
    int cursor;                         /* rec 1: the cursor's step, 1..128 */
    uint8_t page_trig[16];              /* rec 1: the cursor's page: 1 a step with a trig */
    int quantize;                       /* rec 2: quantize is on ("Q" by the badge) */
    int scale;                          /* P1.3b: the track's scale (not chromatic: its name by the octave) */
    const char *scale_name;
    float pos;                          /* rec 2: the playhead, in steps of the track (fractional, 0..length) */
    int step;                           /* rec 2: the step under the playhead, 1..length */
    struct {
        float start, end;               /* steps before now (start >= end >= 0; end 0 while held) */
        uint8_t note, live;             /* live: still held */
    } roll[32];
    int nroll;
} ui_keys_t;
void ui_keys_draw(const ui_keys_t *v);

/* ---- the sound picker's takeover (PLAN.md §9a rule 5; line.html s10's cassette, simpler): while PRESETS turns. P1.3b:
 * the project's sound pool (up to 128): the track's sound with nothing held, the held steps' sound lock with a step
 * held (lock: "STEP 5 · SOUND", the padlock); the sound's place in the pool as a bar of banks of ten */
typedef struct {
    int track;                          /* 1..8 */
    const char *name;                   /* the track's */
    int k, n;                           /* the sound shown: pool number 1..n (k 0: the track's own sound, a lock off) */
    const char *prev, *cur, *next;      /* the names of k - 1, k, k + 1 (0: none) */
    float roll;                         /* 1 at a detent .. 0: the names roll in (ease) */
    int dir;                            /* the detent's direction: +1 the list moves up */
    float reel;                         /* the reels' turn, radians */
    int lock;                           /* a step's sound lock (the title "STEP n · SOUND") */
    int step;                           /* .. the step, 1..128 */
} ui_picker_t;
void ui_picker_draw(const ui_picker_t *v);

/* ---- screen 3, TRACKS (line.html s3): eight clocks, each at its own length and rate */
typedef struct {
    const char *name;                   /* the track's role */
    int len;                            /* its loop, 1..128 steps */
    const char *rate;                   /* "1x", "1/2x", .. */
    uint8_t trig[128];                  /* the trigs round the ring: 1 NOTE / TIE, 2 LOCK */
    float pos;                          /* the hand: the play head as a step position, 0..len */
    int flash_step;                     /* the step that played last (-1 none) .. */
    float flash_age;                    /* .. how long ago, in the track's own steps (it glows under 1.2) */
    int voices;                         /* the voices it sounds */
    int mute;
    int swing;                          /* swing on: the selected track's orange arc */
} ui_track_clock_t;
typedef struct {
    ui_track_clock_t tr[8];
    int sel;                            /* the selected track, 0..7 */
    int playing;                        /* the hands are drawn while the transport runs */
    int swing_amt;                      /* the pattern's swing, 50..80 */
    int pool_used, pool_n;              /* the voice pool */
    const char *pattern;                /* "A03" */
} ui_tracks_t;
void ui_tracks_draw(const ui_tracks_t *v);

/* ================================================================ P1.3b: the step view's lanes, the held step, the turns,
 * the layer maps (PLAN §8b) === */

/* ---- the hold modes (B1..B10): their names and colours. A step that overrides the mode is drawn in the mode's colour,
 * a step on the track's default grey */
enum { UI_F_NOTE, UI_F_VEL, UI_F_LEN, UI_F_MICRO, UI_F_RATCH, UI_F_CHNC, UI_F_COND, UI_F_TRIG, UI_F_SOUND, UI_F_B10,
       UI_F_PLOCK, UI_NFIELD };         /* P1.3c: B11 = P-LOCK (holding a step keeps the screen; its knobs p-lock).
                                         * P1.3d (§8b 2.2): LOCKS is dropped (P-LOCK covers it): B10 is empty,
                                         * reserved (UI_F_B10 is never a mode; its colour is the padlock's) */
extern const char *const UI_FIELD_NAME[UI_NFIELD];   /* "NOTE" .. "SOUND", "", "P-LOCK" */
extern const qd_rgb UI_MODE_COL[UI_NFIELD];

/* ---- the kit's helpers (line.html's headTrack, lock, knobMark, keyboard's pos) */
void ui_head_track(const char *left, int n, const char *name, qd_rgb lcol);   /* left; [n] name at the right */
void ui_lock(float x, float y, qd_rgb col, float s);   /* a padlock, 7 px wide at s 1, its top left at x, y */
void ui_knob_mark(float x, float y, float v, qd_rgb col);   /* a little knob at v 0..1 */
/* where note n sits on ui_keyboard(x0, .., w, .., lo): its centre x; *black 1 for a black key */
float ui_keyboard_x(float x0, float w, int lo, int n, int *black);
/* the value strip at the foot of a held step (§8b): ten keys B1..B10 grouped 3 | 2 | 3 | 2 with their values' labels,
 * B11 "CLR" apart; bit k of lit: B(k+1) is the step's value (filled in col), of def: the track's default (marked), of
 * none: a key that does nothing here (faint); clr: the step overrides the mode (CLR lit) */
void ui_value_strip(char lab[10][6], unsigned lit, unsigned def, unsigned none, int clr, qd_rgb col);
/* the same eleven keys, general (the value strip and the layer maps draw with it): key k's (0..10) left edge, and the
 * strip with lab[k] written in key k (0: none); bit k of lit: filled in col, of mark: a grey line under it (the
 * track's default), of faint: dark, of hot: outlined in col */
float ui_strip_x(int k);
void ui_strip(const char *const lab[11], unsigned lit, unsigned mark, unsigned faint, unsigned hot, qd_rgb col);

/* ---- screen 2, the step view (§8b): the page drawn as the current mode's lane. Every mode keeps the sixteen key
 * outlines and the page's dashes (the loop, each page's start..end); in the columns:
 *   NOTE   the piano roll (P1.3's s2)        VEL   bars up from the floor      LEN   bars over the steps
 *   MICRO  each step's start off its grid   RATCH the hits in the step        CHNC  a column of dots, lit by chance
 *   COND   the conditions' short names      TRIG  NOTE / LOCK / TIE icons      SOUND the pool number (a dot: none)
 *   LOCKS  the step's lock count
 * a step that overrides the mode in the mode's colour, a step on the track's default grey; the default itself as a
 * dashed line (VEL, LEN, MICRO, RATCH, CHNC, NOTE: the default chord's notes) or a grey label (COND, TRIG, SOUND)
 * across the page. Steps outside the page's start..end dark. The mode's name and the track small in the top line */
typedef struct {
    uint8_t on;                         /* a trig */
    uint8_t type;                       /* resolved QD_TRIG_*: 1 NOTE, 2 LOCK, 3 TIE */
    uint8_t ovr;                        /* it overrides the lane's mode */
    uint8_t in_range;                   /* within the page's start..end (else dark, nothing drawn) */
    uint8_t held;                       /* held: outlined white */
    uint8_t lit;                        /* the playhead is in its span (NOTE: its bars filled white) */
    uint8_t nnote;
    uint8_t note[6];                    /* absolute */
    float len;                          /* the gate in steps (INF: to the page's end) */
    int8_t micro;                       /* 24ths of a step */
    uint8_t ratch, ramp;                /* x1..x8, the ramp (QD_RAMP_*) */
    uint8_t vel;                        /* 1..127 */
    uint8_t chance;                     /* 0..100 */
    char cond[6];                       /* "3:4", "FILL", "!1ST"; "" ALWAYS */
    uint8_t sound;                      /* the pool number of its sound lock, 0 none */
    uint8_t nlock;                      /* p-locks */
    uint8_t strum;
} ui_lcol_t;
typedef struct {
    int mode;                           /* UI_F_* */
    int track;                          /* 1..8 */
    const char *name, *pattern;         /* "DRUMS", "A01" */
    int bpm;
    ui_lcol_t col[16];
    int page;                           /* the page edited 0..7 */
    uint8_t loop;                       /* bit p: page p in the loop */
    uint8_t pstart[8], pend[8];         /* each page's range 0..15 */
    int play_page;                      /* the page the playhead is on (-1: stopped) */
    float pos;                          /* the playhead on this page, 0..16 (< 0: not on this page) */
    int lo, span;                       /* NOTE: the roll, note lo at the foot, lo + span at the top */
    float slide;                        /* a page change: the lane slides in (1 .. 0; + from the right) */
    int follow;                         /* page follow on (off: a small "FOLLOW OFF") */
    float t;                            /* seconds (motion) */
    /* the track's default for the mode */
    int def_nnote;
    uint8_t def_note[6];
    int def_vel;
    float def_len;
    int def_micro, def_ratch, def_ramp, def_chance, def_type;
    char def_cond[6];
    char def_label[24];                 /* the default as words: "DEFAULT 100", "TRACK SOUND" */
    int def_hot;                        /* P1.3d: a default is being turned: its line and label in the mode's colour */
    int latch;                          /* P1.3d: the step latch (LFO) is on: "LATCH" in the top line */
    /* P1.3e/f: the STEPS view (HOME): P1.3f, an overview of the track's eight pages as rows of sixteen cells (in
     * fours), the page edited outlined, the pages out of the loop dim; cell[p][s] UI_C_* bits */
    int steps;
    uint8_t cell[8][16];
    int play_step;                      /* the step (0..127) under the playhead (-1 stopped) */
    int flash_step;                     /* the step whose trig played last (-1 none) .. */
    float flash_age;                    /* .. seconds ago (it flashes white, fading over 0.25 s) */
    float grow[8];                      /* P1.3j: each page's row unfolded 0..1 (the page edited 1; a page change
                                         * unfolds the new one as the old one folds) */
} ui_lane_t;
enum { UI_C_ON = 1, UI_C_LOCK = 2, UI_C_TIE = 4, UI_C_OVR = 8, UI_C_RANGE = 16, UI_C_HELD = 32 };
void ui_lane_draw(const ui_lane_t *v);

/* ---- a held step (line.html s4..s10, s19, s20; PLAN §9a rule 7, §8b): one layout, "STEP n · MODE" top left, the
 * track top right, the value big top left, the mode's drawing, the value strip at the foot. is_default: the same
 * drawing for the track's default (ALGORITHM's takeover with nothing held): "DEFAULT · MODE", no strip */
typedef struct {
    int field;                          /* UI_F_* */
    int is_default;                     /* the track's default (no step) */
    int step;                           /* 1..128 (the first held) */
    int nheld;                          /* steps held (more than one: "+n" after the step's number) */
    int track;                          /* 1..8 */
    const char *name;                   /* the track's */
    int type;                           /* the step's trig (QD_TRIG_*): 0 empty, the drawing grey with "EMPTY STEP" */
    int ovr;                            /* the step overrides the mode (the value in the mode's colour; else grey) */
    float play;                         /* seconds since the held step last played (< 0: not since PLAY) */
    float t;                            /* seconds (the loops' clock: the app's, deterministic) */
    int tracklen;                       /* the track's loop (the rulers' step numbers wrap at it) */
    int latch;                          /* P1.3d: the steps are selected by the step latch (LFO), not held: "SELECTED" */
    /* the value strip (B1..B10, B11 CLR) */
    char vk[10][6];
    uint16_t vk_lit, vk_def, vk_none;
    int clr;
    /* NOTE (s4): the chord name big (root + quality; a name not known: the note names), the notes on the keybed */
    int nnote;
    uint8_t note[6];                    /* absolute */
    char note_name[6][6];               /* "C3", "D#4" */
    /* the notes by their slots (P1.3f: pitches): slot 0 note 1 (SELECT), slots 1..4 notes 2..5 (KNOB 1..4), slot 5 note
     * 6 (the keys only), 0 none; drawn in its knob's colour (SELECT UI_KSEL, KNOB k UI_K[k - 1], note 6 UI_KALG) */
    uint8_t slot_note[6];
    int knobs;                          /* the knobs are the notes: their legend drawn (1: SELECT, KNOB 1..4; 2: nothing
                                         * held, KNOB 1..4 only: SELECT is the page), with "A = ALL" */
    int reset;                          /* P1.3e, a default with its mode key held: 1 B11 resets it (it differs from the
                                         * track's initial value), 2 it is the initial one ("B11 RESET" lit / dim) */
    char root[4];                       /* "C", "F#" (empty: no chord) */
    char qual[8];                       /* "m7", "" (major), "sus4" */
    char names[24];                     /* a chord not known: "C D F#" (root empty) */
    float squeeze;                      /* seconds since the chord changed (the name squeezes in over 0.18 s) */
    int entry_oct;                      /* the entry octave (B1's degree) */
    const char *scale_name;             /* the track's scale ("CHROMATIC", "D DORIAN") */
    /* VEL (s5): the mallet raised by the velocity; strikes when the step plays (play); the ramp in green */
    int vel, ramp, ratch;
    /* LEN (s6): the note as a bar over the next steps; TIE: a slur from the note before */
    int len;                            /* the length's index */
    const char *len_name;               /* "2", "1/4", "INF" */
    float len_steps;                    /* < 0: INF */
    int tie;
    float prev_at, prev_len;            /* the note before on the track: its start (steps, < 0) and length */
    float next_at, next_len;            /* the note after: its start (steps, > 0); next_at 0: none */
    int has_prev;
    /* MICRO (s7): the step as a tape measure, the start flag, the strum's staircase */
    int micro, strum;
    float walk;                         /* the slow red playhead, 24ths from the step's grid (-12 .. 36) */
    /* RATCH (s8): the ball bouncing count times, its heights the ramp */
    int count;                          /* (ramp: VEL's) */
    float ball;                         /* 0..1: the ball's loop */
    /* COND / CHNC (s9): the condition big with its passes as a ring; the chance as thirty dots */
    char cond_name[8];                  /* "3:4", "FILL", "ALWAYS" */
    const char *cond_text;              /* one line under a condition that is not an A:B ("while FILL is held") */
    int ab_a, ab_b;                     /* an A:B (b 0: not one) */
    int pass;                           /* the track's pass now, mod b (-1: stopped) */
    int chance;                         /* 0..100 */
    uint32_t shuffle;                   /* the dots' shuffle: changes each pass */
    int chance_first;                   /* the CHNC mode: the chance big, the condition small */
    /* SOUND (s10): the sound lock as a cassette, its neighbours peeking; the page's sounds as dots */
    int sound;                          /* 0 none (the track's sound), 1..128 a pool number */
    const char *sound_cur, *sound_prev, *sound_next;   /* names (cur for 0: the track's sound); prev / next 0: none */
    float roll;                         /* 1 at a detent .. 0 (the names roll in, line.html's 22 px, eased) */
    int dir;                            /* +1: the list moved up */
    uint8_t page_sound[16];             /* each step's sound lock on the page (0 none) */
    int held_col;                       /* the held step's column on the page, 0..15 */
    int nlocks;                         /* the step's p-locks ("+2 LOCKS ON TOP") */
    int bank;                           /* the bank of ten on the keys (0: 1..10) */
} ui_held_t;
void ui_held_draw(const ui_held_t *v);

/* ---- the layer maps (§8b: while F1, F2 or F3 is held the screen is that layer's map, so it explains itself) */
typedef struct {
    int layer;                          /* 1 F1, 2 F2, 3 F3 */
    int track;                          /* 1..8 */
    const char *name;                   /* the track's */
    float t;                            /* seconds (the blinks) */
    int blink;                          /* the blink's lit half (QD_BLINK_MS) */
    /* F1 (P1.3c, §8b 2.1's page layer): the eight pages (B1..B8): the one edited, the loop, the playing one, each one's
     * start..end, its trigs; tap = edit, hold = in / out of the loop, B9 CLEAR (held: the track), B10 COPY, then B1..B8
     * PASTE; B11 UNDO; W: the quick step copy / paste */
    int page;                           /* edited 0..7 */
    uint8_t loop;
    uint8_t pstart[8], pend[8];
    int play_page;                      /* -1: stopped */
    uint8_t trig[8][16];                /* 1: a trig on that page's step */
    int can_undo, step_clip;            /* undo held; a step copied */
    int shift;                          /* (unused since P1.3c) */
    int page_copied;                    /* F1: the page copied this hold (-1 none): B1..B8 paste it */
    int f1_copied;                      /* F1: a step copied this hold (the next W pastes it) */
    int clip_step;                      /* .. the step in the clipboard (1..128; 0 none) */
    int clip_nnote;                     /* .. its notes (the card's little roll), its trig */
    uint8_t clip_note[6];
    int clip_type;
    /* layer 4, SEL held (P1.3c): the eight tracks (B1..B8 select): selected, muted (W1..W8), soloed (W9..W16).
     * F2 (P1.3c): Form's pick-up / drop: the page's sixteen steps and the hand (hand: 0 empty, 1 holding the step
     * clip_step, drawn as a card) */
    int sel;
    uint8_t mute, solo;
    const char *tname[8];
    uint8_t flash[8];                   /* a trig just now */
    int held_track;                     /* (unused since P1.3c) */
    int page_clip;                      /* a page in the clipboard */
    int hand;                           /* F2: holding a step (Form's "holding"), else empty-handed */
    int hand_loaded;                    /* F2: the hand has grabbed this hold (Form's "loaded") */
    uint8_t page_on[16];                /* F2: the edited page's steps (1 a trig) */
    int last_w;                         /* F2: the last W pressed (-1), and what it did: 1 CUT, 2 PASTE */
    int last_act;
    /* F3: the page's range (W: its end; W a + W b: start..end) and the rate (B1..B9); SELECT: tempo, swing */
    int rate;                           /* QD_RATE_* */
    int rate_idx;                       /* its place on B1..B9 (-1: not a quick pick) */
    int rate_val, rate_type;            /* P1.3c: the rate as a note value (0 1/2 .. 4 1/32; KNOB 1) and its type (0
                                         * normal, 1 dotted, 2 triplet; KNOB 2); KNOB 3 / 4 the page's start / end */
    const char *rate_val_name;          /* "1/16" */
    const char *rate_names[9];
    int bpm, swing;
    int anchor;                         /* F3: the W held first (-1 none) */
    int latched;                        /* P1.3d: F3 is a latched menu: "OCT\xe2\x88\x92 EXIT" on the map */
    int ext;                            /* P1.3i: following a running MIDI clock: "EXT CLOCK" and its BPM */
    float tempo_hot;                    /* P1.3j: 1 just after a tempo / swing turn .. 0 a second later (lit) */
} ui_layer_t;
void ui_layer_draw(const ui_layer_t *v);   /* layer 1 F1, 2 F2, 3 F3, 4 SEL's track map */

/* ---- SEQ held (P1.3c, line.html s13 simpler): the patterns of the bank shown: the queued one big (else the playing
 * one), the sixteen as dots (content a ring, the playing one filled white, the queued one blinking orange), a line
 * emptying to the change, the launch mode (END BAR BEAT NOW) as four words, the one in use white. P1.3d (§8b 2.2): the
 * four banks A..D (SEQ + B1..B4) as four letters, the bank shown boxed, a bank with content white */
typedef struct {
    int cur, queued;                    /* 0..63 (-1 queued: none) */
    int bank;                           /* the bank shown (0 A .. 3 D): the W keys' */
    uint8_t bank_used;                  /* bit k: bank k has a pattern with content */
    uint16_t used;                      /* bit k: pattern k + 1 of the bank shown has content */
    int playing;
    float left;                         /* 0..1 of the master length left to the change (queued and playing) */
    int blink;
    int launch;                         /* the launch mode (seq.h QD_LAUNCH_*: END BAR BEAT NOW), ALGORITHM's */
    const char *launch_name;
} ui_patterns_t;
void ui_patterns_draw(const ui_patterns_t *v);

/* ---- the octave (P1.3c): a change of octave or of SOUND's bank takes the screen for a second: the number big (rolling
 * in), a keyboard (a strip of the pool's banks) sliding to it */
enum { UI_OCT_NOTE, UI_OCT_KEYS, UI_OCT_BANK };
typedef struct {
    int kind;                           /* UI_OCT_* */
    int value, from;                    /* NOTE: the entry octave (-1..8); KEYS: the keyboard's shift (-2..+2); BANK: 0.. */
    float k;                            /* 0..1: the slide from `from` to `value` (eased) */
    float t;                            /* seconds since the change */
    int dir;                            /* +1 up */
    int track;
    const char *name;
    const char *scale_name;             /* NOTE: the track's key */
    int lo_note;                        /* NOTE: B1's note at the octave; KEYS: the keybed's lowest note (F) */
    char sub[40];                       /* the line under the number ("B1 = C4 Â· D DORIAN", "SOUNDS 11-20") */
    int nbank;                          /* BANK: the pool's banks; the sound locked (0 none) and the pool's size */
    int sound, nsound;
} ui_octave_t;
void ui_octave_draw(const ui_octave_t *v);

/* ---- SHIFT held: a compact map of what SHIFT does here */
typedef struct {
    const char *ctx;                    /* "STEP VIEW", "A STEP HELD", "KEYBOARD" */
    int n;
    const char *key[8], *what[8];       /* "+ ALGORITHM", "the track's scale" */
} ui_shift_t;
void ui_shift_draw(const ui_shift_t *v);

/* ---- a brief full-screen word (CLEARED; UNDONE; COPIED; NO ROOM FOR LOCKS) */
void ui_confirm_draw(const char *big, const char *small, qd_rgb col);

/* ---- a knob turned: the whole screen (line.html s21..s25, s16; PLAN §9a rule 5) */
enum { UI_TK_FREQ, UI_TK_DECAY, UI_TK_HARM, UI_TK_FDBK, UI_TK_ALGO, UI_TK_TEMPO, UI_TK_SWING, UI_TK_LEN, UI_TK_RATE,
       UI_TK_LIMIT, UI_TK_TSWING, UI_TK_PAGE, UI_TK_SCALE, UI_TK_ROOT, UI_TK_OCTAVE, UI_TK_TRACK, UI_NTK };
/* (OCTAVE: ui_octave. P1.3d: TRACK, ALGORITHM in P-LOCK with nothing held: the eight tracks, the one chosen boxed) */
typedef struct {
    int kind;                           /* UI_TK_* */
    int knob;                           /* 0..3: its colour (SELECT turns KNOB 1's: 0) */
    const char *name;                   /* "FREQ" */
    const char *ctx;                    /* grey at the right: the track's name, "STEP 5" */
    float v;                            /* 0..1: the knob mark, and the drawings' position */
    int value;                          /* the raw value (FREQ in hundredths, the BPM, the length ..) */
    char val[16];                       /* the value big at the foot */
    int lock;                           /* a p-lock on a held step: a padlock by the name, the track's value ghosted */
    float ghost_v;                      /* .. its position 0..1 */
    char ghost[16];                     /* .. "track 82.00" */
    float t;                            /* seconds (motion) */
    float beat;                         /* the beat's phase 0..1 (DECAY's red dot, the metronome) */
    int beats;                          /* beats since PLAY (the metronome's side, the bar's dot) */
    int playing;
    /* TEMPO / SWING (s16): the BPM big, the swing in green, the metronome; swing > 50 swings unequal */
    int bpm, swing;
    /* RATE (s25), SCALE, ROOT: a picker (the value big, its neighbours small) */
    const char *prev, *next;            /* the neighbours' names (0: none) */
    float rate;                         /* the track's speed against the pattern's (0.125 .. 2) */
    float track_pos, pat_pos;           /* the two hands, steps 0..16 */
    int track;                          /* 1..8 (the clock's label) */
    /* LEN: the track's loop over the 8 pages; LIMIT: the 16-voice pool, the limit lit, the voices in use;
     * TSWING: the track's swing on / off */
    int voices;
    /* PAGE (SELECT): a page picker: the eight pages, the one edited big, the loop, each one's range and trigs */
    int page;                           /* 0..7 */
    uint8_t loop, pstart[8], pend[8];
    uint8_t trig[8][16];
    int play_page;
    /* SCALE / ROOT (SHIFT + ALGORITHM / PRESETS): the scale's notes on a little keyboard (scale_mask over root) */
    uint16_t scale_mask;
    int root;
    /* P1.3d: the colour when it is not a knob's (has_col: col; the track chooser draws in P-LOCK's) */
    int has_col;
    qd_rgb col;
    /* TRACK (P-LOCK + ALGORITHM): the eight tracks as rows: number, name, the loop's trigs (up to 32), muted grey */
    const char *tname[8];
    uint8_t tmute;
    uint8_t tlen[8];
    uint8_t ttrig[8][32];
} ui_turn_t;
void ui_turn_draw(const ui_turn_t *v);

/* ---- P1.3d (§8b 2.2): the knob band. Every knob turned in a hold mode shows what it turned in a band over the top of
 * the screen for a second: its knob as a badge (SELECT yellow, KNOB 1..4 their colours, ALGORITHM an A, PRESETS a P),
 * the context ("TRACK DEFAULT", or "STEP 5" with a step held) and the mode, the value's name and the value big in the
 * mode's colour. Over a held step's screen (a step held), or over the lane with the default's line lit (nothing held:
 * a default) */
typedef struct {
    int knob;                           /* QD_K_* of app.h: 0 SELECT, 1 ALGORITHM, 2 PRESETS, 3..6 KNOB 1..4 */
    qd_rgb col;                         /* the mode's colour */
    const char *ctx;                    /* "TRACK DEFAULT", "STEP 5", "STEP 5+2" */
    const char *mode;                   /* "VEL" */
    const char *name;                   /* "VELOCITY", "STRUM", "NOTE 3" */
    char val[24];                       /* "101", "+3/24", "E4", "014 MARIMBA" */
} ui_band_t;
void ui_band_draw(const ui_band_t *v);

#ifdef __cplusplus
}
#endif
