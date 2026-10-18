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

#ifdef __cplusplus
}
#endif
