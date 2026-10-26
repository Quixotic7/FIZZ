/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* QUADRILLE's screens (PLAN.md §9a, the LINE look). Each screen draws one frame through qd_gfx from a small view
 * record the app fills: the screens hold no state and read no clock, so a frame is a function of its view.
 *
 * The kit (ui_kit.c) is mockups/line.html's helpers, call for call: ui_T = T, ui_TW = TW, ui_lab = lab, ui_P = P,
 * ui_O = O, ui_RR = RR. Alpha, a dash or a squeeze go around a call with qd_save / qd_alpha / qd_dash / qd_scale /
 * qd_restore, as the helpers' own save / restore do. */
#pragma once
#include "qd_gfx.h"
#include "../qd_config.h"

#define UI_NTRACK QD_TRACKS              /* the tracks the screens show (qd_config.h) */

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
void ui_lab(const char *s, float x, float y, qd_rgb col, int align);   /* 12 px, 600 (P1.3v): a label */
void ui_P(const float *pts, int npts, qd_rgb col, float lw, int close, qd_rgb fill);   /* fill QD_NONE: none */
void ui_O(float x, float y, float r, qd_rgb col, float lw, qd_rgb fill);              /* col QD_NONE: no stroke */
void ui_RR(float x, float y, float w, float h, float r, qd_rgb col, float lw, qd_rgb fill);   /* inset by lw / 2 */

/* ---- screen 1: idle (P1.3p): FIZZ, the logo: champagne bubbles in the app's colours, the word gathering from particles,
 * solid, then fizzing apart (ui_idle.c) */
typedef struct {
    int bpm;                            /* the tempo (the spray's beat; not shown) */
    int playing;                        /* a DAW plays it: a surge of bubbles on each beat */
    float t;                            /* seconds since power-on: the lazy stream's clock */
    long beat;                          /* beats since PLAY (playing): whole beats .. (P2.4: no double) */
    int32_t beat_num, beat_den;         /* .. and the part of the next, num / den */
} ui_idle_t;
void ui_idle_draw(const ui_idle_t *v);
/* the word at t seconds: the solid word's alpha and the particles' (0..1; either may be NULL) */
void ui_idle_word(float t, float *solid, float *parts);

/* ---- the kit's drawings shared by screens (line.html's box, head, keyboard) */
void ui_box(float x, float y, int n, qd_rgb col);   /* the track's number in a little box */
/* the top line: [n] left .......... right (n 0: no box) */
void ui_head(const char *left, const char *right, int n, qd_rgb lcol, qd_rgb rcol);
float ui_head_x(int n);                 /* P1.6d: where ui_head's left text starts (the track's box, its kind's icon) */
/* P1.6d: the tracks' kinds (sounds.h QD_PK_*: BASS .. FX; 255 none), set by the view each frame: their icons beside
 * the tracks' names (the headers, the track picker, the mix, the track takeovers); a kind's icon centred at cx, cy,
 * 12 px at s 1 */
extern uint8_t UI_TRACK_KIND[UI_NTRACK];
void ui_kind_icon(int kind, float cx, float cy, float s, qd_rgb col);
int ui_track_kind(int n);               /* track n's (1..UI_NTRACK) kind, -1 none */
/* a line-drawn keybed of two octaves (14 white keys) from note lo (a C) at x0, y0, w x h; fill[note] (128 entries, or
 * 0) is the colour a key is filled with, QD_NONE: outlined only */
void ui_keyboard(float x0, float y0, float w, float h, int lo, const qd_rgb *fill);
/* the same with nw white keys from any white note lo (the FM-1's keybed: 16 from F) */
void ui_keybed(float x0, float y0, float w, float h, int lo, int nw, const qd_rgb *fill);

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
    int lib;                            /* P1.5s: k is a LIBRARY slot (1..128; its champagne), past the pool's end */
    int pk, nk, plib, nlib;             /* .. the neighbours' numbers and places */
    int step;                           /* .. the step, 1..128 */
} ui_picker_t;
void ui_picker_draw(const ui_picker_t *v);

/* ---- P1.4b (ui_tracks.c; the clocks screen of P1.3 is retired): THE MIX (line.html s14) and THE EFFECTS (s15)
 *   MIX     eight faders: a pan dial on top, the fader with its knob, a meter beside it, the number (a muted track:
 *           dashed, crossed), its delay and reverb sends as two little bars under it; the selected track in the knobs'
 *           colours (LEVEL blue, PAN green, DELAY white, REVERB orange). SENDS (MIX again): every track's chorus,
 *           delay and reverb as three bars (the selected in KNOB 1..3's colours)
 *   EFFECTS a page an effect's part, a drawing of it with KNOB 1..4's values in cells along the foot:
 *           DELAY    a ball bouncing between two walls at the delay time (TIME blue), the echoes fading by FEEDBACK
 *                    (green), the walls as far apart as SPREAD (white), the selected track's SEND coming in (orange)
 *           DELAY 2  two rows of echoes (STEREO straight, PING-PONG alternating; SYNC: on a beat grid), TONE their
 *                    dimming, WOW a wobble in the line
 *           REVERB   a room as big as SIZE, its reflections, DECAY the tail's length, DAMP its darkening, SEND in
 *           REVERB 2 the tail after its PRE-DELAY gap, MOD its wobble, LOW CUT a slope, WIDTH the stereo spread
 *           CHORUS   three wavy lines at RATE, as deep as DEPTH, WIDTH their spread apart, SEND in
 *           MASTER   the master filter's response: LOW CUT, HIGH CUT, their SLOPE, ON / OFF */
typedef struct {
    int sel, page;                      /* the selected track; 0 the faders, 1 the sends */
    struct {
        const char *name;
        int level, pan, chorus, delay, reverb;   /* 0..127, -64..63 */
        int mute;                       /* 1 not heard (muted, or another soloed), 2 soloed */
        float meter;                    /* its voices' level now, 0..1 */
    } tr[UI_NTRACK];
    int last;                           /* the knob turned last (0..3, -1) */
    int step;                           /* P1.6d: a step held (or the latch's selection): its locks (the frame) */
    int nsel;                           /* P1.6h: .. how many (the head's magenta count; 0 none) */
    char ltxt[4][8];                    /* .. KNOB 1..4's values as chips in the head: */
    uint8_t locked[4], lockany[4];      /* .. locked on the step (filled), on some step of the track (outlined) */
} ui_mix_t;
void ui_mix_draw(const ui_mix_t *v);
enum { UI_FXD_DELAY, UI_FXD_DELAY2, UI_FXD_REVERB, UI_FXD_REVERB2, UI_FXD_CHORUS, UI_FXD_MASTER,
       UI_FXD_OVER,                     /* P1.5b: the sends: CHORUS, DELAY, REVERB, eight tracks' bars each */
       UI_FXD_ROUTE };                  /* P1.5b: the chain (the bus, CHORUS, DELAY, REVERB, the filter, out), the feeds */
typedef struct {
    const char *name;                   /* the page's */
    int draw, npg, ipg;                 /* UI_FXD_*, the effect's pages, this one's place */
    int has[4];
    char txt[4][16];
    const char *lab[4];
    float x[4];                         /* the cells' values 0..1 */
    int track;                          /* the selected track (its SEND), 1..8 */
    float t;                            /* seconds */
    float delay_s, chorus_hz;           /* the delay's time, the chorus's rate */
    int sync, ping;                     /* DELAY: synced, ping-pong */
    float fdbk, spread, tone, wow;      /* 0..1 */
    float size, damp, decay, pre, mod, low, width, depth, cwidth;   /* 0..1 (decay 1: INF) */
    float send;                         /* the selected track's send into this effect, 0..1 */
    float flow, fhigh;                  /* MASTER: the cuts 0..1 (0 / 1: off), */
    int fslope, fon;
    int last;                           /* the knob turned last (0..3, -1) */
    int sends[UI_NTRACK][3];                    /* P1.5b, OVERVIEW: each track's CHORUS, DELAY, REVERB sends 0..127 */
    uint8_t mute;                       /* .. the tracks not heard */
    float rt[4];                        /* P1.5b, ROUTING: DELAY > REVERB, CHORUS > DELAY, CHORUS > REVERB 0..1, the
                                         * filter BEFORE (1) */
    int step;                           /* P1.6d: a step held (or the latch's selection): the sends lock (the frame) */
    int nsel;                           /* P1.6h: .. how many (the head's magenta count; 0 none) */
    uint8_t locked[4], lockany[4];      /* .. a cell locked on the step (a filled chip), on some step (outlined) */
} ui_fx_t;
void ui_fx_draw(const ui_fx_t *v);

/* ---- P1.5a (ui_lib.c, built through ui_layer.c; mockups/fizz-save-pool.html; P1.5u: the name editor is the
 * typewriter's, ui_type.c): THE PROJECT: its number and name, the 64 patterns (bubbles off one when written), the flash
 * and the pool in use; THE PROJECTS: eight tiles. (P1.5t: the sounds page's grid is gone: ui_presets.c) */
#define UI_LIB_COL 0xf3d9a4u
#define UI_POOL_COL 0xb5e853u
#define UI_PAD_COL 0x9664ffu
typedef struct {
    const char *name;
    int used, kind;                     /* (sounds.h QD_SK_*) */
    float env[4];                       /* the amp envelope: attack, decay, sustain, release (0..1) */
} ui_libtile_t;
/* ---- P1.5t (ui_presets.c, built through ui_layer.c; mockups/fizz-presets.html): THE LIBRARY BROWSER (PRESETS,
 * MY PRESETS, POOL), the step's pool picker (STEP n . SOUND, TRACK first), SAVE PRESET, and the LOADED / BACK / SAVED
 * takeovers. A preset as a cassette: its amp envelope in its kind's colour, its name big, a star (one of MY presets)
 * or an arrow (imported), a lime tray (in this project's pool); its neighbours above and below; a bar for where in
 * the list */
extern const qd_rgb UI_PK_COL[7];       /* the kinds' colours: BASS KEYS PLUCK PAD LEAD DRUMS FX */
typedef struct {
    int ok;                             /* an entry here (0: none) */
    int track;                          /* (the pool picker's first) TRACK: no lock */
    const char *name;
    int kind;                           /* QD_PK_* (P1.5v: its first tag: the glyph's colour) */
    int tags;                           /* P1.5v: its tags (bit QD_PK_*: the cassette's dots) */
    float env[4];                       /* the amp envelope (0..1) */
    int mine, imported, in_pool;
    int missing;                        /* P1.5w: a pool reference whose preset is not there (INIT, greyed) */
    int send[3];                        /* P1.5x: its sends, chorus / delay / reverb 0..127 (three little bars) */
    int tray;                           /* P1.5v: the cassette's tray: 0 by in_pool, 1 empty with a + (not in the pool:
                                         * B10 adds it), 2 full (in the pool: B10 takes it out) */
} ui_preset_t;
typedef struct {
    int picker;                         /* 0 the browser, 1 the step's pool picker */
    const char *title;                  /* "PRESETS", "MY PRESETS", "POOL"; the picker "STEP 5 . SOUND" */
    qd_rgb tcol;
    int chip;                           /* the kind filter's chip (1 + QD_PK_*; 0 none) */
    int track;                          /* 1..8 (the head's box) */
    int pos, n;                         /* "7/31" (n 0: the list empty) */
    ui_preset_t e[5];                   /* the chosen (e[2]) and two either side */
    qd_rgb col;                         /* the cassette's colour (the library's, the pool's; armed: orange) */
    qd_rgb ocol;                        /* P1.5z: the chosen cassette's outline: lime in this project's pool, champagne
                                         * not (any source; through + POOL / - POOL's animations: the old one first) */
    int pool;                           /* the POOL source: who uses it at the foot (B10 takes it out) */
    uint8_t users;                      /* .. the tracks (bit t) .. */
    int usteps;                         /* .. the steps */
    int out_armed, del_armed;           /* B10 / B11 armed (P1.5v: a red X, the head red: OUT? / DELETE?) .. */
    float arm_t;                        /* .. seconds since (the cassette shakes) */
    int users_row;                      /* P1.5v: the users at the foot (the POOL; OUT? from any source: to TRACK) */
    int use_proj, use_steps;            /* P1.5w: DELETE?: the projects referencing it, the steps locking it (to TRACK) */
    int fx;                             /* + POOL's animation (ctl.h QD_BRFX_*), its progress, the pool's count */
    float fx_u;
    int pool_n;
    ui_preset_t gone;                   /* P1.5v: QD_BRFX_DEL: the preset deleted (its pieces fizz away) */
    int tag_k, tag_on;                  /* P1.5v: a tag toggled (SHIFT + B2..B8): which, on / off, 0..1 (1: none) */
    float tag_u;
    float sound;                        /* 0..1 after a detent: its envelope lights along (it sounds) */
    float roll;                         /* 1 at a detent .. 0, its direction */
    int dir;
    float t;
} ui_browse_t;
void ui_browse_draw(const ui_browse_t *v);
/* P1.5u: the typewriter (SAVE PRESET's name, a project's): the name, the keybed with its characters */
#define QD_UI_NHIST 10                  /* (ctl.h QD_TY_NHIST) */
typedef struct {
    const char *title;                  /* SAVE PRESET, PROJECT NAME */
    int track;                          /* the head's box (0: none) */
    const char *name;                   /* the name (a string, up to twelve) */
    int cur, sel, spark;                /* the cursor; selected (the first key replaces it); made from the sound */
    int fresh;                          /* the character just typed (champagne; -1 none) */
    int hit;                            /* the key that typed it (0..10 black, 11..26 white; -1): the flight's start */
    float fly;                          /* 0..1 the typed character's flight (1: done) */
    int glow;                           /* the key glowing (held, or just pressed; -1 none) */
    int shift;                          /* SHIFT held: the keybed's signs, the chip */
    float full;                         /* 0..1 a key on a full name: the row flashes (1: none) */
    char del_ch;                        /* F1: the character deleted, its place, 0..1 its drop (1: done) */
    int del_at;
    float del;
    int ideas_on;                       /* PRESETS / KNOB 4 turned: the names' rolodex .. */
    const char *hist[QD_UI_NHIST];      /* .. P1.5z: the names seen, the newest first (0: none) .. */
    int nhist, hpos;                    /* .. how many, the one shown (0 the newest) .. */
    float dice;                         /* .. a new one rolled: 0..1 the die tumbling (1 landed; < 0 none) .. */
    int dice_face;                      /* .. its face 1..6 */
    int replace;                        /* SAVE PRESET: REPLACE offered (F2) .. */
    int choice;                         /* .. P1.5v: 0 NEW, 1 REPLACE (the one SAVE does: bright, filled) */
    const char *newname;                /* .. NEW then saves as this (shown when it differs) */
    const char *sub;                    /* a project's number ("01") */
    float t;
} ui_type_t;
void ui_type_draw(const ui_type_t *v);
enum { UI_PFX_LOADED, UI_PFX_BACK, UI_PFX_SAVED };
/* the takeovers: LOADED (s onto the track), BACK (own back, s the one left), SAVED (s into MY PRESETS); u 0..1 */
void ui_pfx_draw(int kind, const ui_preset_t *s, const ui_preset_t *own, int track, float u, float t);
typedef struct {
    int page;                           /* 0 the project, 1 the projects */
    int num;                            /* the one worked on, 1..8 */
    const char *name;
    uint8_t used[64];
    int cur;                            /* its pattern playing / edited */
    float since[64];                    /* seconds since each pattern was written (its bubbles; < 0 none) */
    float flash, flash_own;             /* the store's use, this project's part (0..1) */
    int pool_n;
    struct {
        int exists;
        const char *name;
        uint8_t used[64];
    } p[8];
    int chosen, src, armed;             /* the projects: chosen (blinks), copied from, armed onto (-1) */
    float t;
} ui_prj_t;
void ui_prj_draw(const ui_prj_t *v);

/* ================================================================ P1.3b: the step view's lanes, the held step, the turns,
 * the layer maps (PLAN §8b) === */

/* ---- the hold modes (B1..B10): their names and colours. A step that overrides the mode is drawn in the mode's colour,
 * a step on the track's default grey */
enum { UI_F_NOTE, UI_F_VEL, UI_F_LEN, UI_F_MICRO, UI_F_RATCH, UI_F_CHNC, UI_F_COND, UI_F_TRIG, UI_F_FUNC, UI_F_SOUND,
       UI_F_PLOCK, UI_NFIELD };         /* P1.3c: B11 = P-LOCK (holding a step keeps the screen; its knobs p-lock).
                                         * P1.3w (§8b "FUNC on B9"): B9 FUNC, the step functions; SOUND moved to
                                         * B10 (it was B9; B10 was empty since LOCKS was dropped, P1.3d) */
extern const char *const UI_FIELD_NAME[UI_NFIELD];   /* "NOTE" .. "FUNC", "SOUND", "P-LOCK" */
extern const qd_rgb UI_MODE_COL[UI_NFIELD];

/* ---- the kit's helpers (line.html's headTrack, lock, knobMark, keyboard's pos) */
void ui_head_track(const char *left, int n, const char *name, qd_rgb lcol);   /* left; [n] name at the right */
void ui_step_card(float x, float y, float w, float h, int full, int type, int nnote, const uint8_t *note);
                                         /* P1.6h (ui_layer.c): a step as a card: its notes on a little keybed, a LOCK
                                          * step a lock, an empty step a key; not full: a dashed empty card */
void ui_lock(float x, float y, qd_rgb col, float s);   /* a lock (P1.5b: a filled chip, no padlock), 7 px wide at s 1,
                                                        * its top left at x, y */
/* P1.5b: a value as a chip, centred at cx on its text's baseline y: filled (locked here: black text) or outlined
 * (locked somewhere); the text shrinks to maxw (never under 12 px); its width */
float ui_chip(const char *txt, float cx, float y, float size, float maxw, qd_rgb col, int filled);
/* P1.5b: LFO held: the latch mode switched, two drawings (the keys as values / the keys as modes), the one chosen lit */
void ui_lmode_draw(int mode, float t);
/* P1.6d: automation recording's cue (a red frame; a red dot on the band under the knob that wrote): k the knob that
 * wrote last (0..3 KNOB 1..4, else an encoder), since its write
 * (seconds; < 0 none yet) */
void ui_auto_cue(int k, float since, float t);
void ui_knob_mark(float x, float y, float v, qd_rgb col);   /* a little knob at v 0..1 */
/* where note n sits on ui_keyboard(x0, .., w, .., lo): its centre x; *black 1 for a black key */
float ui_keyboard_x(float x0, float w, int lo, int n, int *black);
/* the value strip at the foot of a held step (§8b): ten keys B1..B10 grouped 3 | 2 | 3 | 2 with their values' labels,
 * B11 "CLR" apart; bit k of lit: B(k+1) is the step's value (filled in col), of def: the track's default (marked), of
 * none: a key that does nothing here (faint); clr: the step overrides the mode (CLR lit) */
/* P1.3v (§9a's clarity rules): icons, and the black keys drawn where they are on the panel with an icon or a number
 * each (the layer maps; no text legends) */
enum { UI_IC_NONE, UI_IC_CLEAR, UI_IC_COPY, UI_IC_PASTE, UI_IC_UNDO, UI_IC_CUT, UI_IC_BACK, UI_IC_PLAY, UI_IC_STOP,
       UI_IC_DOT, UI_IC_TRI, UI_IC_RESET, UI_IC_SCALE, UI_IC_SWING, UI_IC_REC, UI_IC_NOW, UI_IC_RAMP, UI_IC_STRUM,
       UI_IC_KEY, UI_IC_LEFT, UI_IC_RIGHT, UI_IC_FLIP, UI_IC_DICE, UI_IC_DICE_FN, UI_IC_JUMP, UI_IC_LEAP,
       UI_IC_GEAR, UI_IC_VIEWS,         /* P1.3y: a settings menu (a cog); the views (three panes) */
       UI_IC_TRACK, UI_IC_FOLLOW,       /* P1.5z: the track (three lanes, one lit); page follow (pages, an arrow) */
       UI_IC_NOTE };                    /* UI_IC_NOTE + v: a note value's glyph (v 0 1/1 .. 6 1/64) */
int ui_fn_icon(int func);               /* P1.3w: a step function's icon (UI_IC_NONE for none) */
#define UI_FN_JUMP 7                    /* (pattern.h's QD_FN_JUMP: JUMP n is UI_FN_JUMP + n) */
#define UI_FN_LEAP 135                  /* (.. QD_FN_LEAP: LEAP n is UI_FN_LEAP + 15 + n, n -15..15) */
#define UI_TRACK_COL 0xf03cffu          /* P1.3y: the TRACK colour: a track's default being edited (its drawing and head;
                                         * P1.5d: no frame round it any more); no mode uses it */
#define UI_STEP_FRAME 0xf03cffu         /* P1.5d (the frames the other way round): the frame round a held step's screen
                                         * (or the latch's selection's) in the sequencer */
#define UI_SND_FRAME_TRACK 0x8a3412u    /* P1.5d: the sound editor's frame, the track's sound (nothing held): dark orange */
#define UI_SND_FRAME_STEP 0xff6a2cu     /* .. a step held or selected (its locks): bright orange (UI_K4) */
extern const qd_rgb UI_LFO_COL[3];      /* P1.5k: LFO 1, 2, 3's colours (their marks, the mapping's takeover) */
extern const qd_rgb UI_TRACK_PAL[8];    /* P1.3y: each track's colour (its big picture) */
void ui_icon(int id, float cx, float cy, float s, qd_rgb col);   /* about 12 x 12 px at s 1, centred */
typedef struct {
    int icon;                           /* UI_IC_* (NONE with num 0: nothing in this layer) */
    int num;                            /* a number instead (1..8), 0 none */
    qd_rgb col;                         /* 0: white */
    int lit, dim, mark;
} ui_kkey_t;
float ui_strip_x(int k);
void ui_keymap(const ui_kkey_t k[11], float y, float h);

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
    uint8_t inf;                        /* P1.3q: the gate is INF */
    int8_t micro;                       /* 24ths of a step */
    uint8_t ratch, ramp;                /* x1..x8, the ramp (QD_RAMP_*) */
    uint8_t vel;                        /* 1..127 */
    uint8_t chance;                     /* 0..100 */
    char cond[6];                       /* "3:4", "FILL", "!1ST"; "" ALWAYS */
    uint8_t sound;                      /* the pool number of its sound lock, 0 none */
    uint8_t nlock;                      /* p-locks */
    uint8_t strum;
    uint8_t func;                       /* P1.3w: the step function (QD_FN_*) */
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
enum { UI_C_ON = 1, UI_C_LOCK = 2, UI_C_TIE = 4, UI_C_OVR = 8, UI_C_RANGE = 16, UI_C_HELD = 32,
       UI_C_OWN = 64, UI_C_FUNC = 128 };   /* P1.3q: UI_C_OWN: the step has its own notes (the NOTE override);
                                         * P1.3w: UI_C_FUNC: a step function */
/* P1.3q: the STEPS cells' kinds: own notes, the track's notes (filled), a ghost (LOCK: an outline) */
#define UI_STEP_OWN 0xfff0d2u           /* warm white */
#define UI_STEP_TRACK 0x63c4d6u         /* soft cyan */
#define UI_STEP_GHOST 0xa57cffu         /* violet */
qd_rgb ui_steps_fill(uint8_t cell);     /* a cell's fill: UI_STEP_OWN / UI_STEP_TRACK, QD_NONE (a ghost, off, out of range) */
qd_rgb ui_steps_ink(uint8_t cell);      /* the notes' colour on that fill (a dark shade of it) */
void ui_lane_draw(const ui_lane_t *v);
void ui_steps_row(const ui_lane_t *v, float y, float open);   /* P1.3r: the page edited's row alone (EDIT) */

/* ---- a held step (line.html s4..s10, s19, s20; PLAN §9a rule 7, §8b): one layout, "STEP n · MODE" top left, the
 * track top right, the value big top left, the mode's drawing, the value strip at the foot. is_default: the same
 * drawing for the track's default (ALGORITHM's takeover with nothing held): "DEFAULT · MODE", no strip */
typedef struct ui_held {
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
    /* FUNC (P1.3w): the step function; a jump's target page and step (0..7, 0..15), the target page's trigs */
    int func;
    char func_name[12];
    int jump_page, jump_step;
    uint8_t jump_trig[16];
    int bank;                           /* the bank of ten on the keys (0: 1..10) */
    int follow;                         /* P1.5q: EDIT follows the step playing (nothing held, the latch off) .. */
    int dim;                            /* .. the playhead on an empty step: the last step played, dimmed a little */
    int band;                           /* P1.5p: the knob band is over the head (y 0..41): what the body draws there
                                         * (NOTE's scale, VEL's value and ramp) is left out while it shows */
    const struct ui_held *head;         /* P1.5z (the owner: "lets not change the header during playback"): EDIT
                                         * following, the head (its words, colours, tag and value) is this model's (the
                                         * track's default), the picture the step playing's; 0: v's own */
} ui_held_t;
void ui_held_draw(const ui_held_t *v);
/* P1.3r: the EDIT view: the mode's picture (v: the step held / selected, or the track's default) re-laid-out in the
 * top area, the value strip between, the page edited's row (lane, as STEPS' open row) at the foot */
void ui_edit_draw(const ui_held_t *v, const ui_lane_t *lane);
struct qd_app;
void qd_view_edit(struct qd_app *a, ui_held_t *h, ui_lane_t *l);   /* (view.c) its models, as its frame fills them */
int qd_view_edit_follows(const struct qd_app *a);   /* P1.5q: EDIT shows the step playing (the step, else -1) */

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
    int f1_src;                         /* P1.6h: .. from this step (0..127, -1): ringed in its page's cell */
    int clip_step;                      /* .. the step in the clipboard (1..128; 0 none) */
    int clip_nnote;                     /* .. its notes (the card's little roll), its trig */
    uint8_t clip_note[6];
    int clip_type;
    /* layer 4, SEL held (P1.3c): the eight tracks (B1..B8 select): selected, muted (W1..W8), soloed (W9..W16).
     * F2 (P1.3c): Form's pick-up / drop: the page's sixteen steps and the hand (hand: 0 empty, 1 holding the step
     * clip_step, drawn as a card) */
    int sel;
    uint8_t mute, solo;
    const char *tname[UI_NTRACK];
    uint8_t flash[UI_NTRACK];                   /* a trig just now */
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

/* ---- SHIFT held (P1.3v: visual): a row per control SHIFT changes here: the control, the job's icon, its value now */
enum { UI_SC_SELECT, UI_SC_ALGO, UI_SC_PRESETS, UI_SC_REC, UI_SC_F3_ALGO, UI_SC_SEQ_W,
       UI_SC_SEL, UI_SC_HOME,          /* (P1.3y: SEL's button, HOME's) */
       UI_SC_ARP };                    /* (P1.6d: ARP's: FILL) */
typedef struct {
    int n;
    int ctl[9];                         /* UI_SC_* */
    int icon[9];                        /* UI_IC_*: what it does */
    const char *val[9];                 /* its value now ("D DORIAN", "D", "55%"), or 0 */
    int keys;                           /* P1.5c: the step view: its black-key shortcuts drawn under the rows .. */
    int snap_ok, pclip_ok, metro, armed;   /* .. a snapshot to recall, a pattern to paste, the click on, B11 armed */
    int follow;                         /* .. P1.5z: page follow on (B8's glyph lit) */
} ui_shift_t;
void ui_shift_draw(const ui_shift_t *v);

/* ---- P1.3y: the settings menus (latched, OCT- / HOME back; ui_layer.c): a row a knob (its badge, its name, its value
 * big in the knob's colour) and a drawing of the value.
 *   REC (SHIFT + REC)      KNOB 1 MODE: LIVE (notes on a line, off the grid) / STEP (a row of cells, the cursor's
 *                          lit); KNOB 2 QUANTIZE 0..100 %: four notes pulled onto their grid lines by that much;
 *                          KNOB 3 TAKE: CAPTURE (a track of steps cut at a red end mark), REPLACE (the step's notes
 *                          go, crossed) / OVERDUB (they stay, the new one joins)
 *   SETTINGS (SHIFT + SEL) KNOB 1 PAGE FOLLOW ON / OFF: pages with the playhead and an arrow */
enum { UI_MENU_REC, UI_MENU_SET, UI_MENU_TRK };   /* (P1.4b: the track settings) */
typedef struct {
    int kind;                           /* UI_MENU_* */
    int rec_step, quant, dub;           /* REC: MODE STEP (else LIVE), QUANTIZE 0..100, TAKE (pattern.h QD_TAKE_*) */
    int ekeys;                          /* .. P1.5b: EDIT KEYS FULL (else SCALE) */
    int rec_page, rec_auto;             /* .. P1.6d: the page (2: AUTOMATION), AUTOMATION ON */
    int follow;                         /* SETTINGS: PAGE FOLLOW */
    float t;                            /* seconds (the playhead moves) */
    /* P1.4b, the track settings: the track (its number, name, colour), the page; page 1: the scale's name, its root,
     * its notes (bit k: pitch class k), FOLLOW KEY, the MIDI channel (1..16); page 2: VOICES, SWING, PAGE FOLLOW */
    int track, page;
    const char *tname, *scale, *root;
    int scale_mask, root_pc, follow_key, chan, limit, swing, voices_now;
    int mfx;                            /* P1.6c: the track's MIDI FX group (0 OFF) */
    int tkind, named;                   /* P1.6d, page 3: its kind (-1 none), its name typed */
    int last;                           /* the knob turned last (0..3, -1) */
} ui_menu_t;
void ui_menu_draw(const ui_menu_t *v);

/* ---- P1.4a: the sound editor (ui_snd.c, built through ui_layer.c): a page of FM TONE as one big drawing, KNOB 1..4's
 * values in cells along the foot (their colours; P1.5b: a lock a chip), ALGORITHM's value (where the page has one) at
 * the top right in its pink badge. The track's sound (nothing held): the TRACK colour in the head, P1.5d a dark orange
 * frame (UI_SND_FRAME_TRACK); a step held: "STEP n", a bright orange frame (UI_SND_FRAME_STEP), and under each locked
 * value the track's left behind (grey, dashed).
 *   ALGO      the four operators (C A B1 B2 in KNOB 1..4's colours, each with its wave at its ratio) wired by the
 *             algorithm: the modulators above what they modulate (arrows, dots running along), the feedback
 *             operator's loop, the carriers down into the bus and the speaker
 *   TONE      four pictures: HARM the partials' comb, DTUNE two waves beating, FDBK a loop whose wave roughens, MIX a
 *             balance X .. Y
 *   OFS       four dials, the operators' offsets (-1.00 .. +1.00)
 *   OPENV     the operator envelope: its DELAY (ALGORITHM), attack, decay to END, held; LEV a bar
 *   TRIG      A's and B's TRIG and RESET as switches; PHRT the operators it resets (lit circles)
 *   KEYS      a keybed with the three key-scaling slopes; VEL a wedge
 *   FILTER    the response: the type's shape, the cutoff, the resonance's peak, DEPTH an arrow at the cutoff
 *   ADSR      an ADSR (the filter's: its DELAY on ALGORITHM; the amp's); INF release: held to the edge
 *   BAND      the base-width band (a high-pass at BASE, a low-pass WIDTH above), KTRK a slant
 *   DRIVE     LEVEL a fader; DRIVE a sine through the saturation (the input grey, the output shaped)
 *   LFO       the wave (its shape from the WAVE page) at its speed, its depth (a negative one upside down), the fade
 *   LFOSHAPE  the wave over three notes, restarting by its TRIG mode, the start PHASE a dot
 *   PORT      (E1) two note bars, the second higher, the pitch gliding between them as long as TIME (OFF a hard step;
 *             LEGATO the bars overlapping); SLOPE a ghost glide of a smaller interval: TIME the same length, RATE the
 *             same slant
 * (E1: AMP in AHD (ALGORITHM MODE): attack, a flat top as long as HOLD (NOTE: over a note bar, to its end), the decay
 * to 0; LEVEL's ROUTE: a FILTER / DRIVE chain in its order) */
enum { UI_SD_ALGO, UI_SD_TONE, UI_SD_OFS, UI_SD_OPENV, UI_SD_TRIG, UI_SD_KEYS, UI_SD_FILTER, UI_SD_ADSR, UI_SD_BAND,
       UI_SD_DRIVE, UI_SD_LFO, UI_SD_LFOSHAPE, UI_SD_PORT, UI_SD_CRUSH,   /* (E3: CRUSH: a sine as stairs) */
       UI_SD_MOD };                     /* (E4: the eight slots as rows) */
typedef struct ui_snd {
    const char *name;                   /* the page's ("RATIO", "ENV A") */
    int npg, ipg;                       /* its group's pages, this one's place (the dots in the head) */
    int draw, arg;                      /* UI_SD_*, the envelope (0 A / filter, 1 B / amp) or the LFO */
    int has[5];                         /* KNOB 1..4, ALGORITHM: a value there */
    int v[5], tv[5];                    /* the values (the held step's, else the track's), the track's */
    char txt[5][12];                    /* .. as text */
    const char *lab[5];                 /* their names ("A DEC") */
    uint8_t locked[5];                  /* the held step has a lock there */
    int step;                           /* the step held (1..128), 0: the track's sound */
    int last;                           /* the knob turned last (0..4), -1 */
    float t;                            /* seconds (the waves move) */
    uint8_t mod[4], fb, outx, outy;     /* ALGO: the algorithm's routing (C A B1 B2: bit 0 C .. 3 B2) */
    float ratio[4];                     /* .. the operators' ratios */
    int pv[3];                          /* LFO: its WAVE, TRIG, FADE (P1.5k); LFOSHAPE: its SPEED, DEPTH */
    uint8_t lockany[5];                 /* P1.5b: no step held: some step of the track locks it (an outlined chip) */
    int keys;                           /* P1.5b: the keyboard on: 1 SCALE (16 keys: the scale), 2 FULL (27) */
    uint8_t kst[27];                    /* .. each key: 0 out of the scale, 1 in it, 2 a root; +4 sounding */
    uint8_t pair[5];                    /* P1.5i: a ratio pair (RATIO B): txt the fast hand (B2) over den (B1) */
    char den[5][8];
    int has_mix, mix;                   /* P1.5i: RATIO's MIX (-64 X .. 63 Y): the outputs' balance in the drawing */
    uint8_t lfomark[5];                 /* P1.5k: bit n: LFO n + 1 has this value as its destination (a mark by its label) */
    uint8_t modmark[5];                 /* E4: a modulation slot has it as its destination: its source (QD_MS_*), 0 none */
    int mod_src[8], mod_dep[8], mod_sel;   /* E4: MOD: the slots' sources, depths (-127..127), the chosen one .. */
    const char *mod_dst[8];             /* .. their destinations' names */
    int lk_src;                         /* E4: B1 held (lk 4): the source mapped (QD_MS_*) */
    int lk;                             /* P1.5k: an LFO key held (1..3): the mapping's takeover .. */
    const char *lk_name;                /* .. the value it maps ("FREQ"; 0: none yet) */
    int lk_ok, lk_k;                    /* .. a destination (else dim, shaking), the knob's column (-1) */
    float lk_depth, lk_shake;           /* .. the depth -1..1, the shake 0..1 (1: just refused) */
    int fpop, fpop_sel;                 /* P1.6d: the FILTER key held: the machines' popup up, the current (menu order) */
    int follow;                         /* P1.6g: the values the step playing's (the latch off, nothing held) */
    int rnd_kind;                       /* P1.5c: randomise: 1 the dice shaking (P1.6d: held), 4 rolling, 2 the bottle
                                         * filling, 3 its burst .. */
    float rnd_u;                        /* .. 0..1 through it */
} ui_snd_t;
void ui_snd_draw(const ui_snd_t *v);

/* ---- a brief full-screen word (CLEARED; UNDONE; COPIED; NO ROOM FOR LOCKS) */
void ui_confirm_draw(const char *big, const char *small, qd_rgb col);
/* P1.5c: SHIFT + F1 / F2: UNDO / REDO big, under it what it was as a glyph (undo.h QD_UK_*; -1: nothing there, dim) */
void ui_undo_draw(int redo, int kind, float t);
void ui_import_draw(const char *name, int pool, int slot, float t);   /* P1.5r (ui_lib.c): a sound imported */

/* ---- a knob turned: the whole screen (line.html s21..s25, s16; PLAN §9a rule 5) */
enum { UI_TK_FREQ, UI_TK_DECAY, UI_TK_HARM, UI_TK_FDBK, UI_TK_ALGO, UI_TK_TEMPO, UI_TK_SWING, UI_TK_LEN, UI_TK_RATE,
       UI_TK_LIMIT, UI_TK_TSWING, UI_TK_PAGE, UI_TK_SCALE, UI_TK_ROOT, UI_TK_OCTAVE, UI_TK_TRACK,
       UI_TK_TRACKBIG,                  /* P1.3y: FX held + ALGORITHM: the track big (number, name, its colour) */
       UI_TK_TAP,                       /* P1.5z: SHIFT + B6: the taps (the BPM big, the taps' dots, a pulse) */
       UI_TK_FOLLOW,                    /* P1.5z: SHIFT + B8: PAGE FOLLOW on / off (the pages, the playhead's arrow) */
       UI_TK_TOOL,                      /* T1: TOOLS: a tool's name popped (tk_knob 0), the scope (tk_knob 1) */
       UI_NTK };
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
    const char *tname[UI_NTRACK];
    uint8_t tmute;
    uint8_t tlen[UI_NTRACK];
    uint8_t ttrig[UI_NTRACK][32];
    /* P1.5z, TAP (SHIFT + B6): the taps' ages (s, the newest first; up to 8), the count, the clock external (dimmed) */
    int ntap;
    float tap_age[8];
    int ext;                            /* (SCALE / ROOT: the global key's) */
    float since;                        /* seconds since the last tap (the takeover closes 2.8 s after it) */
    int ntotal;                         /* TAP ape: the taps of this count (not capped), .. */
    int tap_cont;                       /* .. its first came while the screen was still up (no bounce in), .. */
    int tap_bpm[2];                     /* .. the BPM each of the last two taps made (0: none yet) */
} ui_turn_t;
/* the TAP ape (ui_ape.c): the pose a tap shows: the n-th tap of a count (1..), its BPM (180 and up: the flurry) */
int ui_ape_pose(int n, int bpm);
void ui_ape_draw(const ui_turn_t *v);   /* the TAP takeover, whole (SHIFT + B6) */
void ui_ape_tempo(const ui_turn_t *v, qd_rgb col);   /* 2026-10-25: TEMPO / SWING (F3's SELECT, ALGORITHM) */
void ui_ape_prewarm(int ext);
void ui_ape_forget(void);               /* (its layers forgotten: the render's cold measures) */              /* 0.9.2.1: SHIFT held: the first poses' layers built, a part a frame */
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

/* ---- P1.6c: the MIDI FX screen (ui_mfx.c; mockups/fizz-midifx.html). The head: the page dots, MIDI FX, the group's
 * number in a hexagon (OFF: an empty dashed one), the tracks routed to it as squares (the selected ringed). The rail:
 * the eight slots as tiles (each its effect's glyph in its colour; empty dashed; bypassed dimmed and crossed; the
 * selected raised and outlined), dots running along it as notes pass, a SELECTOR's block bracketed. The middle: the
 * selected effect's drawing (its name above in its colour); the foot: KNOB 1..4's labels and values. PRESETS' type
 * rolodex and ALGORITHM's group choice draw in the middle / over the screen while turning */
#define UI_MFX_MINT 0x5cf2c4u          /* the MIDI FX colour (a group's hexagon) */
extern const qd_rgb UI_MFX_COL[11];     /* each effect's colour (mfx.h QD_MFX_*; NONE grey) */
void ui_mfx_glyph(int type, float cx, float cy, float s, qd_rgb col, float lw);   /* an effect's glyph, 2 s wide */
void ui_mfx_hex(float x, float y, float r, int n, qd_rgb col, float lw, int dashed);   /* a group: n in a hexagon */
typedef struct ui_mfx {
    int g;                              /* the group shown (1..5); 0: the selected track is OFF */
    int track;                          /* the selected track (1..8) */
    int ntr;                            /* the tracks routed to the group (1-based) .. */
    uint8_t tr[UI_NTRACK];
    uint8_t type[8], byp[8];            /* the rail: each slot's effect (QD_MFX_*), bypassed */
    float glow[8];                      /* .. a note passing it (0..1, decaying) */
    int flow;                           /* notes are passing (the dots run) */
    int sel, page, npages;              /* the selected slot, its page */
    int has[4], last;                   /* the foot: KNOB 1..4's values, the knob turned last (-1) */
    const char *lab[4];
    char val[4][12];
    int16_t v[12];                      /* the selected slot's values (mfx.h QD_M*) .. */
    int8_t lane[2][16];                 /* .. its lanes */
    int lane_sel, lane_cur;             /* the lane step selected; the one playing (-1) */
    int nheld;                          /* ARP: the notes it holds (by pitch), their tracks .. */
    uint8_t held_p[8], held_t[8];
    int arp_run, arp_step;              /* .. it runs, the step of its walk playing (into seq) */
    int nseq;                           /* .. its walk over the range (pitches, up to 48; P1.6g: the PATTERN's real order,
                                         * on the notes held or C E G, the lanes' rests as gaps, transposes as offsets) */
    uint8_t seq[48], gap[48];
    int nprev;                          /* P1.6g: the walk before the last change (its morph) .. */
    uint8_t prev[48];
    float morph;                        /* .. 0..1 into the new one */
    int sel_branch;                     /* SELECTOR: the slot chosen last (-1) */
    char chord[12];                     /* CHORD: the chord's name ("Am7") */
    int byp_sel;                        /* the selected slot is bypassed */
    float arm;                          /* B11 armed: 1 just armed .. 0 (the red X shakes) */
    int lift_from, lift_to;             /* F2 + B: the slot carried (-1 none) .. */
    float lift_u;                       /* .. 0..1 through it */
    /* P1.6e: a slot key held: the slot lifted (past the tap time: arrows either side), a knob turned: its move
     * previewed along the rail (the others making room), dropped at the release */
    int drag_from, drag_to, drag_prev;  /* (-1 none); where it is, where it was before the last detent */
    float drag_u, drag_up;              /* .. the glide since that detent 0..1; the lift 0..1 */
    int drag_moved;                     /* .. a knob moved it */
    int drop;                           /* the slot just dropped (-1), 0..1 through its landing */
    float drop_u;
    int f1, f1_src;                     /* P1.6e: F1 held: COPY (the source slot, once picked; -1) */
    int f2, f2_hand;                    /* .. F2 held: the hand (the effect in it; -1 empty) */
    int fstep;                          /* P1.6h, STEPS: F1 / F2's last key a step: the picture the step's (F1:
                                         * st_copied; F2: f2_hand >= 0), the step as st_* */
    int st_copied, st_type, st_nnote;              /* .. the step in the clipboard: its trig (0 empty, 2 LOCK), its notes */
    uint8_t st_note[6];
    int steps_frame;                    /* .. STEPS: steps held (the step's frame) */
    int route_off;                      /* 2026-10-25: the track's routing bypassed (QD_TMFX_OFF): OFF in the head */
    int roll;                           /* PRESETS turned: the rolodex up .. */
    float roll_pos, roll_from;          /* .. at type pos (0 NONE .. 10), coming from */
    int gsel;                           /* ALGORITHM turned: the five groups' choice up .. */
    float gsel_pos;                     /* .. the track's square at (0 OFF .. 5) */
    uint8_t gtype[5][8];                /* .. each group's slots' effects */
    uint8_t gtr[5][UI_NTRACK];                  /* .. the tracks on it (but the selected), how many */
    int gtrn[5];
    float t;                            /* seconds (motion) */
} ui_mfx_t;
void ui_mfx_draw(const ui_mfx_t *v);
/* a held step's MIDI FX (TRIG's KNOB 2, B4..B10): its group's hexagon big (TRACK: the track's, OFF: none) and rail */
void ui_mfx_step_draw(int step, int m, int g, const uint8_t type[8], const char *trig, float t);

/* ================================================================ T1: TOOLS (ui_tools.c) ===
 * mockups/fizz-tools.html: the head (the eight tools as pictures, the chosen underlined; the orange dot: a change
 * pending; the crossed-out arrow: drawn only, no undo; the track's box; the scope's picture), the tool's picture, the
 * lane at the foot (the edited page as it will be: solid kept, ring new, faded going) */
typedef struct {
    int tool, scope, track;             /* QD_TL_*, QD_TS_*, 1..6 */
    uint16_t sel;                       /* the page's selected steps (SELECTED STEPS: bit w) */
    int pending, nolive;
    qd_rgb tcol;                        /* the track's colour */
    uint8_t on[16], was[16];            /* the lane: each step as it will be, as it was */
    uint8_t note[16];                   /* .. their first notes (0: none): REVERSE's bar heights */
    int lo, hi;                         /* .. the notes' range */
    int play;                           /* the playing step in the page (-1) */
    int rot, flip;                      /* ROTATE's offset; REVERSE flipped */
    int hits, len, erot, emode, epos;   /* EUCLID; epos: the playing position in its length (-1) */
    int count, lpow;                    /* LENGTH: the track's steps after, the doublings */
    int r[4];                           /* T2: RANDOM's density %, notes, velocity, chance spreads; HUMANIZE's micro and
                                         * velocity jitters (r[0], r[1]); GRIDS' x, y, density, chaos (0..255);
                                         * QUANTIZE's amount %, grid (QD_QG_*), velocity % */
    int seed, part;                     /* .. the seed (the die's face); GRIDS' part (0 LOW, 1 MID, 2 HIGH) */
    int8_t gtr[3];                      /* .. PATTERN: the tracks the parts go to (-1 none) */
    int8_t micro[16], micro0[16];       /* .. HUMANIZE, QUANTIZE: the lane's micro-timing (24ths), its velocities, as the
                                         * change makes them and as they were (the ghosts) */
    uint8_t vel[16], vel0[16];
    uint8_t loop, loop0;                /* .. the loop's pages after, before */
    uint8_t page;                       /* the page shown (0..7: the page strip over the lane) */
    uint16_t inr;                       /* .. its steps that play (bit w: within its start..end) */
    float t;                            /* seconds (motion) */
    /* STRETCH (2026-10-25): the track's 128 steps now and before (bit s: a trig; in, in0: the loop's steps); the pages
     * it fits onto (KNOB 2: 0 off); CLIP: the action (KNOB 1), which can be done (bit QD_CL_*), CLEAR armed, a page or a
     * pattern to paste */
    uint8_t ton[16], tin[16], ton0[16], tin0[16];
    int fit;
    int act, act_ok, armed;
} ui_tools_t;
void ui_tools_draw(const ui_tools_t *v);
/* kind 0: the tool's name (the scope's under it), 1: the scope, 2: message msg, 3: RANDOM's knob msg (0..3) named */
void ui_tools_pop(int kind, int msg, int tool, int scope, int page);   /* (page: CLIP's PAGE n) */
void ui_tools_icon(int tool, float x, float y, int on);
void ui_tools_scope_icon(int scope, float x, float y, qd_rgb col, float s);

/* ================================================================ P2.11: the Geek Out pages (ui_geek.c) ===
 * mockups/fizz-geekout.html, approved: four pages after EDIT in SHIFT + HOME's cycle, one header (the page's name, the
 * transport, the four pages as little pictures). Yellow (UI_KSEL) and a spark of rays mark trouble. A figure the
 * platform does not give is drawn dim (its *_on 0) */
#define UI_GEEK_SCOPE 113                               /* the scope's points (x 8 .. 232, 2 px apart) */
typedef struct {
    int page;                           /* 0 METERS, 1 NUMBERS, 2 VOICES, 3 SCOPE */
    int playing;                        /* the transport .. */
    float beat;                         /* .. the beat's phase (0..1) */
    /* METERS: core 0's load (core 1 off until it runs), the frame rate, RAM, the store: the bars as shown, their peaks
     * (the frame rate's: its lowest), the values (0..1; the frame rate in frames a second) */
    float load, load_pk, fps, fps_min, ram, ram_pk, store;
    int load_on, core1_on, fps_on, ram_on, store_on;
    float load1;
    int load_pct, load1_pct, fps_n, ram_pct, store_pct;   /* (the numbers on top: four a second) */
    /* NUMBERS */
    int voices, nvoice;                 /* sounding of the pool's */
    qd_rgb slot_col[16];
    float slot_lvl[16];                 /* each slot's level (0: silent) */
    int steals;                         /* since the reset .. */
    float steal_flash;                  /* .. the last one's flash (1 .. 0) */
    int isr_us, isr_on, late, late_on, lock_us, lock_on, frame10, frame_on, bpm;
    float isr_frac, late_flash;
    const char *boot;                   /* after a crash: its cause (0: none) */
    /* VOICES, a column a track */
    int lim[UI_NTRACK], tsteals[UI_NTRACK];
    float lvl[UI_NTRACK][3], tflash[UI_NTRACK], share[UI_NTRACK];
    int on[UI_NTRACK];
    /* SCOPE: the loudest track (-1: silence), the line's heights (-1 .. 1) */
    int scope_track;
    float scope[UI_GEEK_SCOPE];
} ui_geek_t;
void ui_geek_draw(const ui_geek_t *v);

/* P1.5z: the models the frame fills (view.c), for the tests */
struct qd_app;
void qd_view_geek(struct qd_app *a, ui_geek_t *v);              /* P2.11: a Geek Out page's (its state moved on) */
void qd_view_geek_reset(struct qd_app *a);                      /* P2.11: SHIFT + B11 there: the counters from zero */
void qd_view_browse(const struct qd_app *a, ui_browse_t *v);   /* the library browser's */
void qd_view_mix(const struct qd_app *a, ui_mix_t *v);         /* P1.6d: the mix's */
void qd_view_fx(const struct qd_app *a, ui_fx_t *v);           /* P1.6d: the effects' */
void qd_view_layer(struct qd_app *a, ui_layer_t *v);         /* P1.6h: the F layer's map on screen */
void qd_view_menu(const struct qd_app *a, ui_menu_t *v);       /* P1.6d: the settings menus' */
void qd_view_type(const struct qd_app *a, ui_type_t *v);       /* P1.6d: the typewriter's */
void qd_view_turn(struct qd_app *a, ui_turn_t *v);              /* the takeover's (UI_TK_*) */
void qd_view_shift(const struct qd_app *a, ui_shift_t *v);      /* SHIFT's map's */

#ifdef __cplusplus
}
#endif
