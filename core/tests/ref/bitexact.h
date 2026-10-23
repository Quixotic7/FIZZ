/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The bit-exact tests' two sides: one scenario, rendered by ChoralRoot's code from the tag (cr_*_ref.c) and by
 * QUADRILLE's copy (test_*_bitexact.c), compared sample for sample. */
#pragma once
#include <stdint.h>

/* one voice: a note held `hold` control blocks (32 samples) and released; optionally struck again in place (the same
 * voice, again_note at block again_at, held `hold` more) or killed (the fade steal) at kill_at; then run to silence
 * (the engine says done) or max samples */
typedef struct {
    int note, vel;
    uint32_t hold;
    uint32_t again_at;                  /* 0: none */
    int again_note, again_vel;
    uint32_t kill_at;                   /* 0: none */
    uint32_t vidx;                      /* the voice's index (ChoralRoot: the part's voice slot; QUADRILLE: the pool's) */
    int bpm;
} be_scen_t;

int cr_ref_npresets(void);
const char *cr_ref_preset_name(int k);
void cr_ref_preset_patch(int k, int16_t *p);
int cr_ref_preset_drive(int k);         /* P1.4a: preset k's FX() DRIVE (0: INIT) */
uint32_t cr_ref_render(int preset, const be_scen_t *sc, int32_t *out, uint32_t max);

/* the effects: an impulse (or any send) through one of them at ChoralRoot's defaults, the stereo wet out */
enum { BE_FX_CHORUS, BE_FX_DELAY, BE_FX_REVERB, BE_FX_CLEAN, BE_FX_N };
/* in: n mono samples of the send (the chorus, the delay, the reverb) or, for BE_FX_CLEAN, n stereo frames of the mix
 * (in[2i], in[2i+1]) into Options > Output Clean at MASTER master_q12; out: n stereo frames */
void cr_fx_ref_run(int which, const int32_t *in, int32_t *out, uint32_t n, int bpm, uint32_t master_q12);
/* the whole chain (chorus, delay and reverb, the master filter, Output Clean): five inputs a frame (in[5i .. 5i + 4]:
 * the chorus, delay and reverb sends, the dry left and right), n stereo frames out */
void cr_fx_ref_chain(const int32_t *in, int32_t *out, uint32_t n, int bpm, uint32_t master_q12);
/* P1.4a: ChoralRoot's DIST (fx.c track_dist) on one part: n mono samples in 32-sample blocks, block b at drive[b] (its
 * state carried across, as a part's is) */
void cr_fx_ref_dist(const int32_t *in, int32_t *out, uint32_t n, const int *drive);
