// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The learned MIDI map's C face for FIZZ for iOS: the numbers the map's entries use (the core ABI's button labels and
// knob roles, FM1P_B_* / FM1P_E_*, under the names RemoteFM1's ported MidiMap.swift and RFMMidiMap.cpp read), and a
// handle on RFMMidiMap.cpp's MidiMapper (rfm::MidiMapper, C++) for Swift. Written for FIZZ (no RemoteFM1 engine): the
// app feeds the mapper itself, on the main thread, and applies what it returns to PLAY's panel (MidiRouter.swift).
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// button labels and knob roles, in the core ABI's order (FizzButton, FizzKnob)
enum { RFME_B_FX, RFME_B_SEL, RFME_B_ENV, RFME_B_LFO, RFME_B_EDIT, RFME_B_GLO, RFME_B_HOME, RFME_B_SAVE, RFME_B_ARP,
       RFME_B_SEQ, RFME_B_PLAY, RFME_B_REC, RFME_B_OCTDN, RFME_B_OCTUP };
enum { RFME_E_SELECT, RFME_E_ALGO, RFME_E_PRESETS, RFME_E_K1, RFME_E_K2, RFME_E_K3, RFME_E_K4 };

// a source: a CC or a note
enum { RFME_SRC_CC = 0, RFME_SRC_NOTE = 1 };
// a target: 0..6 a knob role, MASTER, or a button (RFME_T_BUTTON + its label: 8..21)
enum { RFME_T_MASTER = 7, RFME_T_BUTTON = 8 };
// how a CC turns an encoder: absolute (a pot), binary offset (65 = +1), two's complement (127 = -1), sign bit (65 = -1)
enum { RFME_MAP_ABS = 0, RFME_MAP_REL64 = 1, RFME_MAP_REL2C = 2, RFME_MAP_RELSIGN = 3 };

typedef struct {
    uint8_t src;                // RFME_SRC_*
    uint8_t channel;            // 0..15
    uint8_t number;             // the CC or note number, 0..127
    uint8_t target;             // a knob role, RFME_T_MASTER, RFME_T_BUTTON + label
    uint8_t mode;               // RFME_MAP_* (an encoder's CC; otherwise ignored)
} rfme_map_entry_t;

// ---- the mapper (one per controller input; not thread-safe: one thread uses it)
typedef struct fzmm fzmm_t;

enum { FZMM_NONE = 0, FZMM_TURN, FZMM_BUTTON, FZMM_MASTER, FZMM_PICKUP };

typedef struct {
    uint8_t kind;               // FZMM_*
    uint8_t first;              // PICKUP: the first of this wait (show the toast once)
    int16_t index;              // TURN: the knob role; BUTTON: the label
    int32_t value;              // TURN: detents; BUTTON: 1 down / 0 up; MASTER and PICKUP: the pot, 0..1023
} fzmm_act_t;

fzmm_t *fzmm_new(void);
void fzmm_free(fzmm_t *m);
// a new map (n <= 512) and knob speed (0.5, 1 or 2); returns the buttons the old map held (bit per label), to let go
uint32_t fzmm_adopt(fzmm_t *m, const rfme_map_entry_t *map, uint32_t n, double speed);
// one channel message (1..3 bytes): 0 no mapping takes it (it flows on); 1 consumed, *out the action (maybe NONE).
// master: MASTER now, 0..1023; -1: unknown, or no pickup (a MASTER pot sets it at once)
int fzmm_feed(fzmm_t *m, const uint8_t *d, uint32_t n, int32_t master, fzmm_act_t *out);
// learn saw this CC's value (an absolute pot turns from there once it is mapped); ch 0..15
void fzmm_seed(fzmm_t *m, uint8_t ch, uint8_t num, uint8_t value);
// nothing held any more (the panel let go of everything)
void fzmm_release(fzmm_t *m);

#ifdef __cplusplus
}
#endif
