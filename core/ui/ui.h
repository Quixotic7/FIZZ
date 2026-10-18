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

#ifdef __cplusplus
}
#endif
