/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's MIDI map (research/FIZZ-MIDI-PLAN.md §2, §3): the fixed CC map, CC number <-> the value it sets (a sound value id,
 * a mix lock id QD_LK_*, or QD_MM_DRY), read both ways (CC in; stage (c): knob moves out), and the CC's 0..127 <-> the
 * value: spread over the value's range; a value from a list (the filter MACHINE, an LFO's DEST) by its place in the list
 * its knob turns through; RATIO B the pair (B1 x 19 + B2, 0..360, the id QD_QP_RB1 standing for it).
 *
 * Built inside app.c (the core's source lists stay as they are). Main loop only: nothing here runs in the audio
 * interrupt (app.c midi_play never calls it) */
#pragma once
#include "../seq/pattern.h"

struct qd_app;

#define QD_MM_DRY QD_LK_N              /* the mixer's DRY / WET (mix.h dry, 0..127): no lock id of its own */
#define QD_MM_NRB 361                  /* RATIO B's pair: 19 x 19 places */

int qd_midi_cc_id(int cc);             /* the value CC cc sets on a track's channel (-1: none, the CC left alone) */
int qd_midi_id_cc(int id);             /* .. and the CC of value id (-1: none; QD_QP_RB2: RATIO B's, 27) */
int qd_midi_cc_value(int id, int cc);  /* CC value 0..127 -> id's value (FTYPE: the machine's own type, LEGACY LP12;
                                        * RB1: the pair 0..360) */
int qd_midi_value_cc(int id, int v);   /* id's value -> 0..127 (RB1: v the pair) */

/* a CC on a track channel (from app.c qd_app_midi_ui): tracks (bit t) set like the editor's knobs with nothing held:
 * live (qd_player_live_edit, the sounding voices follow), or the selected track's recorded with REC and AUTOMATION on
 * (qd_auto_put). THE CC-IN PATH: these edits came from MIDI; stage (c) sends the panel's and the remote's knob moves out
 * (ctl_snd.c, ctl_mix.c) and must not send these back, so this path never goes through the knobs. 1: cc is mapped */
int qd_midi_cc_in(struct qd_app *a, int tracks, int cc, int v);

/* THE REMOTE (§3): channel 16's channel messages (from app.c qd_app_midi_ui, kind the status' high nibble), the panel as
 * if by hand, into qd_app_key / qd_app_button / qd_app_knob / qd_app_master:
 *   notes 53..79      the 27 keys, with velocity (note off or velocity 0 lets go)
 *   CC 20..33         the 14 buttons in QD_B_* order: 64 or more presses, below 64 lets go (on a change only)
 *   CC 40..46         the 7 encoders in QD_K_* order, relative: 64 + n turns n detents, 64 - n turns -n (64 nothing)
 *   CC 47..50         KNOB 1..4 for plain pots and faders: the move since the CC's last value (the first only notes it)
 *   CC 7              MASTER (0..127 over 0..1023)
 *   CC 123            every key and button the remote holds let go
 * Its few bytes of state are qd_app_t's rm_* */
#define QD_RM_BTN0 20
#define QD_RM_ENC0 40
#define QD_RM_FADER0 47
void qd_midi_remote(struct qd_app *a, int kind, int d1, int d2);
