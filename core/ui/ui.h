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

/* ---- screen 1: idle (line.html s1): the four operators dance a quadrille */
typedef struct {
    char pattern[8];                    /* "A03" */
    int bpm;                            /* the tempo, shown in blue */
    int playing;                        /* the play mark: outlined when stopped, filled when playing */
    float turn;                         /* quarter turns of the square done (1.0 = each dancer one corner on) */
    float phase;                        /* the dance's phase, radians: one bob a beat when playing (2 pi a beat) */
    float amp;                          /* the motion's size: 1 playing, smaller for the stopped sway */
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
    int preset;                         /* the sound: 0 INIT, 1..16 */
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
    int quantize;                       /* rec 2: FUNC + REC's quantize is on ("Q" by the badge) */
    float pos;                          /* rec 2: the playhead, in steps of the track (fractional, 0..length) */
    int step;                           /* rec 2: the step under the playhead, 1..length */
    struct {
        float start, end;               /* steps before now (start >= end >= 0; end 0 while held) */
        uint8_t note, live;             /* live: still held */
    } roll[32];
    int nroll;
} ui_keys_t;
void ui_keys_draw(const ui_keys_t *v);

/* ---- the sound picker's takeover (PLAN.md §9a rule 5; line.html s10's cassette, simpler): while PRESETS turns */
typedef struct {
    int track;                          /* 1..8 */
    const char *name;                   /* the track's */
    int k, n;                           /* the sound on the track (0 INIT .. n - 1) */
    const char *prev, *cur, *next;      /* the names of k - 1, k, k + 1 (0: none) */
    float roll;                         /* 1 at a detent .. 0: the names roll in (ease) */
    int dir;                            /* the detent's direction: +1 the list moves up */
    float reel;                         /* the reels' turn, radians */
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

/* ================================================================ P1.3: the step view, the held step, the turns === */

/* ---- the kit's P1.3 helpers (line.html's headTrack, keyStrip, lock, knobMark, keyboard's pos) */
enum { UI_F_NOTE, UI_F_VEL, UI_F_LEN, UI_F_MICRO, UI_F_RATCH, UI_F_COND, UI_F_CHNC, UI_F_TRIG, UI_F_SOUND, UI_F_LOCKS,
       UI_F_CLEAR, UI_NFIELD };         /* the black keys' fields with a step held, F#3 .. F#5 (PLAN §8) */
extern const char *const UI_FIELD_NAME[UI_NFIELD];   /* "NOTE" .. "CLEAR" */
void ui_head_track(const char *left, int n, const char *name, qd_rgb lcol);   /* left; [n] name at the right */
void ui_key_strip(int cur, unsigned set);   /* the eleven, 3 | 2 | 3 | 2 | 1: cur filled, bit k of set outlined */
void ui_lock(float x, float y, qd_rgb col, float s);   /* a padlock, 7 px wide at s 1, its top left at x, y */
void ui_knob_mark(float x, float y, float v, qd_rgb col);   /* a little knob at v 0..1 */
/* where note n sits on ui_keyboard(x0, .., w, .., lo): its centre x; *black 1 for a black key */
float ui_keyboard_x(float x0, float w, int lo, int n, int *black);

/* ---- screen 2, the step view (line.html s2): the page as a little piano roll inside the 16 key outlines */
typedef struct {
    uint8_t type;                       /* QD_TRIG_*: 0 empty, 1 NOTE, 2 LOCK (no notes drawn: a small ring), 3 TIE */
    uint8_t nnote;
    uint8_t note[4];                    /* absolute */
    float len;                          /* the gate in steps (INF: to the page's end) */
    int8_t micro;                       /* 24ths of a step: the bar starts off the column */
    uint8_t ratch;                      /* 1..8: a ratchet's bar dashed into its hits */
    uint8_t lock, cond, chance, sound;  /* the markers under the column: p-locks (a white dot), a condition (a blue
                                         * ring), a chance under 100 % (a green half ring), a sound lock (an orange
                                         * square) */
    uint8_t beyond;                     /* beyond the track's length: the outline dark, nothing in it */
    uint8_t held;                       /* held or latched: the column and its notes outlined white */
    uint8_t lit;                        /* the playhead is in the note's span: its bars filled white */
} ui_col_t;
typedef struct {
    int track;                          /* 1..8 */
    const char *name, *pattern;         /* "DRUMS", "A01" */
    int bpm;
    ui_col_t col[16];
    int page, npages;                   /* the page shown 0..7; the track's pages 1..8 (bright dashes) */
    int play_page;                      /* the page the playhead is on (-1: stopped): its dash marked red */
    float pos;                          /* the playhead on this page, 0..16 (< 0: not on this page) */
    int lo, span;                       /* the roll: note lo at the foot, lo + span at the top */
    float slide;                        /* a page change: the roll slides in from the side (1 .. 0; + from the right) */
    int follow;                         /* page follow on (off: a small "FOLLOW OFF" mark by the dashes) */
} ui_step_t;
void ui_step_draw(const ui_step_t *v);

/* ---- a held step's fields (line.html s4..s10, s19, s20; PLAN §9a rule 7): one layout, "STEP n · FIELD" top left,
 * the track top right, KNOB 1's value big top left, the drawing, the black-key strip at the foot */
typedef struct {
    int field;                          /* UI_F_* (not CLEAR: CLEAR is ui_confirm's) */
    int step;                           /* 1..128 (the first held) */
    int nheld;                          /* steps held (more than one: "+n" after the step's number) */
    int track;                          /* 1..8 */
    const char *name;                   /* the track's */
    unsigned set;                       /* the strip: bit k = field k holds a value other than its default */
    int type;                           /* the step's trig (QD_TRIG_*): 0 empty, the drawing grey with "EMPTY STEP" */
    float play;                         /* seconds since the held step last played (< 0: not since PLAY) */
    float t;                            /* seconds (the loops' clock: the app's, deterministic) */
    int tracklen;                       /* the track's length (the rulers' step numbers wrap at it) */
    /* NOTE (s4): the chord name big (root + quality; a name not known: the note names), the notes on the keybed */
    int nnote;
    uint8_t note[4];                    /* absolute, in KNOB 1..4's order (note 1 first) */
    char note_name[4][6];               /* "C3", "D#4" */
    char root[4];                       /* "C", "F#" (empty: no chord) */
    char qual[8];                       /* "m7", "" (major), "sus4" */
    char names[24];                     /* a chord not known: "C D F#" (root empty) */
    float squeeze;                      /* seconds since the chord changed (the name squeezes in over 0.18 s) */
    /* VEL (s5): the mallet raised by the velocity; strikes when the step plays (play); the ramp in green */
    int vel, ramp, ratch;
    /* LEN (s6): the note as a bar over the next steps; TIE (KNOB 2, green): a slur from the note before */
    int len;                            /* the length's index */
    const char *len_name;               /* "2", "1/4", "INF" */
    float len_steps;                    /* < 0: INF */
    int tie;
    float prev_at, prev_len;            /* the note before on the track: its start (steps, < 0) and length */
    float next_at, next_len;            /* the note after: its start (steps, > 0); next_at 0: none */
    int has_prev;
    /* MICRO (s7): the step as a tape measure, the start flag (KNOB 1), the strum's staircase (KNOB 2) */
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
    int chance_first;                   /* the CHNC field: the chance is KNOB 1's (blue, big), the condition KNOB 2's */
    /* SOUND (s10): the sound lock as a cassette, its neighbours peeking; the page's sounds as dots */
    int sound;                          /* 0 none, 1 INIT, 2..17 */
    const char *sound_cur, *sound_prev, *sound_next;   /* names (cur for 0: the track's sound); prev / next 0: none */
    float roll;                         /* 1 at a detent .. 0 (the names roll in, line.html's 22 px, eased) */
    int dir;                            /* +1: the list moved up */
    uint8_t page_sound[16];             /* each step's sound lock on the page (0 none) */
    int held_col;                       /* the held step's column on the page, 0..15 */
    int nlocks;                         /* the step's p-locks ("+2 LOCKS ON TOP") */
    /* LOCKS (s20): one lock big (name, value in blue), the line from the track's value to it; prev / next peeking */
    int nlock, lock_cur;
    const char *lk_name[3];             /* prev, cur, next (0: none) */
    char lk_val[3][12];
    float lk_x, tr_x;                   /* the lock's and the track's values, 0..1 of the range */
    char tr_val[12];                    /* "82.00" */
} ui_held_t;
void ui_held_draw(const ui_held_t *v);

/* ---- a brief full-screen word (CLEAR: "CLEARED", "again to undo"; "UNDONE"; "NO ROOM FOR LOCKS") */
void ui_confirm_draw(const char *big, const char *small, qd_rgb col);

/* ---- a knob turned: the whole screen (line.html s21..s25, s16; PLAN §9a rule 5) */
enum { UI_TK_FREQ, UI_TK_DECAY, UI_TK_HARM, UI_TK_FDBK, UI_TK_ALGO, UI_TK_TEMPO, UI_TK_SWING, UI_TK_LEN, UI_TK_RATE,
       UI_TK_LIMIT, UI_TK_TSWING, UI_NTK };
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
    /* ALGO: 1..8 (FM TONE's routings, drawn as s12's operators and wires) */
    /* TEMPO / SWING (s16): the BPM big, the swing in green, the metronome; swing > 50 swings unequal */
    int bpm, swing;
    /* RATE (s25): a picker over the pattern's clock and the track's */
    const char *prev, *next;            /* the neighbours' names (0: none) */
    float rate;                         /* the track's speed against the pattern's (0.125 .. 2) */
    float track_pos, pat_pos;           /* the two hands, steps 0..16 */
    int track;                          /* 1..8 (the clock's label) */
    /* LEN: the track's length over the 8 pages; LIMIT: the 16-voice pool, the limit lit, the voices in use;
     * TSWING: the track's swing on / off (a metronome swinging unequal or even) */
    int voices;
} ui_turn_t;
void ui_turn_draw(const ui_turn_t *v);

#ifdef __cplusplus
}
#endif
