/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The promo's own touch on ATRIUM (make -C tools tour SONG=song_promo.c; promo/take.txt's "song"): the film shows
 * the arpeggiator, and the demo song has none, so for the take alone A08's bells hold each bar's chord and MIDI FX
 * group 2 plays it as an arp (UP, sixteenths, two octaves, a little chance), with its two lanes: a MOD lane of six
 * (two notes, a rest, a note, the lowest an octave down, a tie) against a TRANSPOSE lane of eight in octaves (the
 * film shows both pages). The demo song itself is not changed:
 * this is what a player would do to it. The chords are tools/song_atrium.c's second strain (A flat, B flat, C, the
 * Lydian C). */
#include "song_common.h"

int fizz_tour_song(qd_app_t *a, int argc, char **argv)
{
    static const uint8_t CH[4][4] = {{56, 63, 72, 75}, {58, 65, 74, 77}, {60, 67, 72, 76}, {60, 67, 74, 78}};
    static const int8_t MOD[6] = {QD_AL_PLAY, QD_AL_PLAY, QD_AL_REST, QD_AL_PLAY, QD_AL_LO, QD_AL_TIE};
    static const int8_t TR[8] = {0, 0, -12, 0, 12, 0, -12, 12};
    enum { T_BELL = 4, A08 = 7 };
    qd_pattern_t *p = qd_proj_pat(&a->proj, A08);
    qd_mfx_slot_t *m = a->proj.mfx[1].slot;
    int b, rate = 0;
    (void)argc, (void)argv;
    (void)pack_store, (void)pool_add, (void)tsound, (void)note;
    while (rate < 24 && qd_mfx_rate_of(rate) != 16)
        rate++;
    qd_mfx_slot_init(&m[0], QD_MFX_ARP);
    m[0].v[QD_MA_PAT] = QD_AP_UP, m[0].v[QD_MA_RATE] = (int8_t)rate, m[0].v[QD_MA_RANGE] = 2, m[0].v[QD_MA_MODE] = QD_AM_ON;
    m[0].v[QD_MA_MLEN] = 6, m[0].v[QD_MA_TLEN] = 8;
    memcpy(m[0].lane[0], MOD, sizeof MOD), memcpy(m[0].lane[1], TR, sizeof TR);
    qd_mfx_slot_init(&m[1], QD_MFX_CHANCE);
    m[1].v[QD_MC_CHANCE] = 88;
    qd_track_clear(p, T_BELL);
    qd_track_set_length(p, T_BELL, 64);
    p->trk[T_BELL].limit = 4;
    p->trk[T_BELL].mfx = 2;
    for (b = 0; b < 4; b++)
        chord(p, T_BELL, b * 16, CH[b], 4, 84, 12, 0);  /* (length index 12: sixteen steps, the bar) */
    return 0;
}
