/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ MIDI's acceptance (research/FIZZ-MIDI-PLAN.md), driven as the glue drives it (qd_app_midi between ticks).
 *
 *   map        the fixed CC map (§2): the table's numbers, the CCs left alone, the reverse lookup, 0..127 <-> a value
 *   channels   a CC on track 1's channel sets track 1's FREQ (CC 74) and not track 2's; channel 15 the selected track;
 *              channel 16 plays nothing; the mixer's (LEVEL, PAN, DRY); a CC an undo level
 *   lists      LFO DEST and the filter MACHINE by their places in the knob's list (LEGACY keeps its TYPE); RATIO B
 *              the pair
 *   rec        REC armed with AUTOMATION ON: a CC on the selected track records a lock (the sound kept); on another
 *              track's channel: a live edit, nothing recorded
 *   midi ch    the track settings' MIDI CH runs 1..15; a stored 16 stays (shown 16) and plays nothing
 *
 *     test_midi    exit 1 on a failed check */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ui.h"
#include "midi_map.h"

static int fails, checks;
#define CHECK(c, ...)                                     \
    do {                                                  \
        checks++;                                         \
        if (!(c)) {                                       \
            fails++;                                      \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

static qd_app_t A;
static uint32_t now;
static uint64_t frames_done;

static void run(uint32_t n)
{
    static uint16_t lcd[QD_W * QD_H];
    while (n--) {
        now++;
        qd_app_tick(&A, now);
        if (now % 15u == 0u)
            qd_app_frame(&A, lcd);
        while (frames_done + 128u <= (uint64_t)now * QD_FS / 1000u) {
            int16_t blk[256];
            qd_app_audio(&A, blk, 128);
            frames_done += 128u;
        }
    }
}
static void bdown(int b) { qd_app_button(&A, b, 1); }
static void bup(int b) { qd_app_button(&A, b, 0); }
static void btap(int b)
{
    bdown(b);
    run(20);
    bup(b);
    run(20);
}
static void turn(int knob, int d)                       /* (one gesture: no acceleration from the turn before) */
{
    A.ui.knob_ms[knob] = 0;
    qd_app_knob(&A, knob, d);
    run(5);
}
static void start(void)                                 /* a fresh unit, the step view, on the empty A05 */
{
    qd_app_init(&A);
    now = 0;
    frames_done = 0;
    qd_app_tick(&A, 0);
    btap(QD_B_HOME);
    bdown(QD_B_MIX);
    qd_app_key(&A, QD_WHITE_KEY[4], 1, 100);
    run(30);
    qd_app_key(&A, QD_WHITE_KEY[4], 0, 0);
    run(10);
    bup(QD_B_MIX);
    btap(QD_B_MIX);
    run(10);
}
static void cc(int ch, int n, int v)                    /* ch 1..16 */
{
    qd_app_midi(&A, 0xB0 | (ch - 1), n, v);
    run(2);
}
static qd_pattern_t *PAT(void) { return qd_ctl_pat(&A); }
static int LK(int t, int s, int id)                     /* the lock's value, or -99999 */
{
    int16_t v;
    return qd_lock_get(PAT(), t, s, id, &v) ? v : -99999;
}
static int nlocks(int t, int id)
{
    int s, n = 0;
    for (s = 0; s < QD_NSTEP; s++)
        n += LK(t, s, id) != -99999;
    return n;
}
static int sounding_any(void)
{
    int i, n = 0;
    for (i = 0; i < A.pool.nvoice; i++)
        n += qd_pool_sounding(&A.pool, i) && A.pool.v[i].q.gate;
    return n;
}
static int16_t lo_of(int id)
{
    int16_t lo, hi;
    qd_lock_range(id, &lo, &hi);
    return lo;
}
static int16_t hi_of(int id)
{
    int16_t lo, hi;
    qd_lock_range(id, &lo, &hi);
    return hi;
}

/* ================================================================ the map */
static void t_map(void)
{
    static const uint8_t ALONE[] = {0, 32, 1, 2, 6, 38, 64, 66, 67, 68, 69, 96, 97, 98, 99, 100, 101,
                                    120, 121, 122, 123, 124, 125, 126, 127};
    static const struct { uint8_t cc; int16_t id; } SOME[] = {
        {7, QD_LK_LEVEL}, {10, QD_LK_PAN}, {91, QD_LK_REVERB}, {93, QD_LK_CHORUS}, {94, QD_LK_DELAY}, {95, QD_MM_DRY},
        {74, QD_QP_FREQ}, {71, QD_QP_RESO}, {5, QD_QP_PTIM}, {65, QD_QP_PORT}, {73, QD_QP_EATK}, {75, QD_QP_EDEC},
        {72, QD_QP_EREL}, {3, QD_QP_ATRIG}, {14, QD_QP_PHRT}, {17, QD_QP_B2KEY}, {19, QD_QP_PSLOPE},
        {20, QD_QP_VMODE}, {24, QD_QP_VPRIO}, {27, QD_QP_RB1}, {29, QD_QP_ALGO}, {33, QD_QP_FDBK}, {37, QD_QP_OFSB2},
        {43, QD_QP_ADLY}, {48, QD_QP_BDLY}, {49, QD_QP_FTYPE}, {52, QD_QP_FGAIN}, {57, QD_QP_FDLY},
        {61, QD_QP_BWPOST}, {62, QD_QP_ESUS}, {63, QD_QP_AMOD}, {70, QD_QP_HOLD}, {76, QD_QP_LEVEL},
        {78, QD_QP_DRPRE}, {81, QD_QP_CRPOST}, {82, QD_QP_LFO(0, QD_QL_SPEED)}, {89, QD_QP_LFO(0, QD_QL_FADE)},
        {90, QD_QP_LFO(1, QD_QL_SPEED)}, {92, QD_QP_LFO(1, QD_QL_MULT)}, {102, QD_QP_LFO(1, QD_QL_DEPTH)},
        {107, QD_QP_LFO(1, QD_QL_FADE)}, {108, QD_QP_LFO(2, QD_QL_SPEED)}, {111, QD_QP_LFO(2, QD_QL_DEST)},
        {115, QD_QP_LFO(2, QD_QL_FADE)}};
    size_t i;
    int c, n = 0, ok = 1, rt = 1, uniq = 1, alone = 1;
    for (i = 0; i < sizeof SOME / sizeof SOME[0]; i++)
        ok &= qd_midi_cc_id(SOME[i].cc) == SOME[i].id;
    CHECK(ok, "the plan's numbers (GM's, the pages')");
    for (i = 0; i < sizeof ALONE; i++)
        alone &= qd_midi_cc_id(ALONE[i]) < 0;
    CHECK(alone, "bank, MW / BREATH, data entry, the pedals, RPN / NRPN and the channel mode left alone");
    for (c = 0; c < 128; c++) {
        const int id = qd_midi_cc_id(c);
        int c2;
        if (id < 0)
            continue;
        n++;
        rt &= qd_midi_id_cc(id) == c;
        for (c2 = c + 1; c2 < 128; c2++)
            uniq &= qd_midi_cc_id(c2) != id;
    }
    CHECK(n == 13 + 83 && rt && uniq, "96 CCs mapped (%d), each value once, read back both ways", n);
    CHECK(qd_midi_id_cc(QD_QP_RB2) == 27 && qd_midi_id_cc(QD_QP_MOD(0, QD_QM_DEPTH)) < 0,
          "RATIO B's halves: CC 27; the MOD slots none");
    for (ok = 1, c = 0; c < 128; c++) {                 /* (a value of 128 places or more: every CC comes back) */
        ok &= qd_midi_value_cc(QD_QP_FREQ, qd_midi_cc_value(QD_QP_FREQ, c)) == c;
        ok &= qd_midi_value_cc(QD_LK_PAN, qd_midi_cc_value(QD_LK_PAN, c)) == c;
        ok &= qd_midi_value_cc(QD_QP_RB1, qd_midi_cc_value(QD_QP_RB1, c)) == c;
    }
    CHECK(ok, "CC -> value -> CC: the same CC (FREQ, PAN, RATIO B's pair)");
    CHECK(qd_midi_cc_value(QD_QP_FREQ, 0) == lo_of(QD_QP_FREQ) && qd_midi_cc_value(QD_QP_FREQ, 127) == hi_of(QD_QP_FREQ) &&
              qd_midi_cc_value(QD_LK_PAN, 0) == -64 && qd_midi_cc_value(QD_LK_PAN, 127) == 63 &&
              qd_midi_cc_value(QD_QP_ALGO, 0) == lo_of(QD_QP_ALGO) && qd_midi_cc_value(QD_QP_ALGO, 127) == hi_of(QD_QP_ALGO),
          "0 .. 127 over the whole range (FREQ, PAN -64..63, ALGORITHM)");
}

/* ================================================================ the channels */
static void t_channels(void)
{
    int f1, f2, u0;
    start();
    f1 = A.model.t[0].patch[QD_QP_FREQ];
    f2 = A.model.t[1].patch[QD_QP_FREQ];
    run(QD_UNDO_QUIET_MS + 50);
    u0 = qd_undo_count(&A, -1);
    cc(1, 74, 0);
    CHECK(A.model.t[0].patch[QD_QP_FREQ] == lo_of(QD_QP_FREQ) && A.model.t[1].patch[QD_QP_FREQ] == f2,
          "CC 74 0 on channel 1: track 1's FREQ at its lowest, track 2's kept");
    cc(1, 74, 100);
    CHECK(A.model.t[0].patch[QD_QP_FREQ] == qd_midi_cc_value(QD_QP_FREQ, 100) &&
              PAT()->snd[0].patch[QD_QP_FREQ] == qd_midi_cc_value(QD_QP_FREQ, 100) && f1 != qd_midi_cc_value(QD_QP_FREQ, 100),
          "CC 74 100: there, in the pattern's sound too");
    run(QD_UNDO_QUIET_MS + 50);
    CHECK(qd_undo_count(&A, -1) == u0 + 1, "the CC's moves: one undo level (%d -> %d)", u0, qd_undo_count(&A, -1));
    qd_undo_step(&A, -1);
    run(5);
    CHECK(A.model.t[0].patch[QD_QP_FREQ] == f1, "undone: the FREQ back (%d)", f1);
    qd_ctl_select_track(&A, 2);
    cc(15, 71, 127);
    CHECK(A.model.t[2].patch[QD_QP_RESO] == hi_of(QD_QP_RESO) && A.model.t[0].patch[QD_QP_RESO] != hi_of(QD_QP_RESO),
          "channel 15: the selected track's (3) RESO");
    cc(3, 7, 10);
    cc(3, 10, 0);
    cc(3, 95, 127);
    cc(3, 91, 127);
    CHECK(A.model.t[2].mix.level == 10 && A.model.t[2].mix.pan == -64 && A.model.t[2].mix.dry == 127 &&
              A.model.t[2].mix.reverb == 127,
          "the mixer's: LEVEL (7), PAN (10), DRY (95), REVERB (91)");
    {
        const int r = A.model.t[2].patch[QD_QP_RESO];
        qd_app_midi_play(&A, 0x9F, 60, 100);            /* (the audio side alone: nothing) */
        qd_app_midi(&A, 0x9F, 40, 100);                 /* (no key of the remote's: nothing) */
        qd_app_midi(&A, 0xBF, 71, 0);
        run(20);
        CHECK(!sounding_any() && A.model.t[2].patch[QD_QP_RESO] == r, "channel 16: no note plays, no CC sets a sound");
        qd_app_midi_play(&A, 0x8F, 60, 0);
        qd_app_midi(&A, 0x8F, 40, 0);
    }
    cc(1, 1, 99);
    cc(1, 64, 127);
    CHECK(A.model.t[0].owner.mw == 99, "CC 1 still the MOD WHEEL (64: nothing)");
}

/* ================================================================ the lists */
static void t_lists(void)
{
    const int dest = QD_QP_LFO(0, QD_QL_DEST), n = qd_quad_ndest();
    const int first = qd_quad_dest_step(0, -n);
    int pos, ok = 1;
    start();
    cc(1, 85, 0);
    CHECK(A.model.t[0].patch[dest] == first, "LFO 1 DEST 0: the list's first (%d)", A.model.t[0].patch[dest]);
    cc(1, 85, 127);
    CHECK(A.model.t[0].patch[dest] == qd_quad_dest_step(first, n), "127: its last (%d)", A.model.t[0].patch[dest]);
    for (pos = 0; pos < n; pos++) {                     /* each place: the knob's place, the CC back */
        const int c = qd_midi_value_cc(dest, qd_quad_dest_step(first, pos));
        cc(1, 85, c);
        ok &= A.model.t[0].patch[dest] == qd_quad_dest_step(first, pos);
    }
    CHECK(ok, "every DEST in the knob's order, CC -> DEST -> CC (%d of them)", n);
    cc(1, 49, 0);
    CHECK(A.model.t[0].patch[QD_QP_FTYPE] == QD_QF_MULTI, "MACHINE 0: MULTI (the menu's first)");
    cc(1, 49, 127);
    CHECK(A.model.t[0].patch[QD_QP_FTYPE] == QD_QF_EQ, "127: EQ (its last)");
    cc(1, 49, qd_midi_value_cc(QD_QP_FTYPE, QD_QF_LP12));
    CHECK(A.model.t[0].patch[QD_QP_FTYPE] == QD_QF_LP12, "LEGACY: LP12, its default TYPE");
    qd_player_live_edit(&A.player, 0, QD_QP_FTYPE, QD_QF_HP12);
    cc(1, 49, qd_midi_value_cc(QD_QP_FTYPE, QD_QF_OFF) + 1);
    CHECK(A.model.t[0].patch[QD_QP_FTYPE] == QD_QF_HP12, "LEGACY again (another CC on its place): HP12 kept");
    cc(1, 49, qd_midi_value_cc(QD_QP_FTYPE, QD_QF_COMBN));
    CHECK(A.model.t[0].patch[QD_QP_FTYPE] == QD_QF_COMBN, "COMB-");
    cc(1, 27, 127);
    CHECK(A.model.t[0].patch[QD_QP_RB1] == 18 && A.model.t[0].patch[QD_QP_RB2] == 18, "RATIO B 127: the pair's last");
    cc(1, 27, qd_midi_value_cc(QD_QP_RB1, 60));
    CHECK(A.model.t[0].patch[QD_QP_RB1] * 19 + A.model.t[0].patch[QD_QP_RB2] == 60, "RATIO B: 1.00 / 1.00 (60)");
}

/* ================================================================ recording */
static void t_rec(void)
{
    int f0, f1, s;
    start();
    A.proj.rec_dub = QD_TAKE_REPLACE;                   /* (armed playing: recording at once) */
    A.proj.rec_auto = 1;
    btap(QD_B_PLAY);
    btap(QD_B_REC);                                     /* the keyboard .. */
    bdown(QD_B_REC);                                    /* .. REC held: armed */
    run(QD_REC_HOLD_MS + 50);
    bup(QD_B_REC);
    run(10);
    CHECK(A.ui.rec && qd_auto_on(&A), "REC armed, AUTOMATION ON, playing");
    f0 = A.model.t[0].patch[QD_QP_FREQ];
    f1 = A.model.t[1].patch[QD_QP_FREQ];
    cc(1, 74, 5);
    s = A.ui.auto_step;
    CHECK(LK(0, s, QD_QP_FREQ) == qd_midi_cc_value(QD_QP_FREQ, 5) && A.model.t[0].patch[QD_QP_FREQ] == f0,
          "CC 74 on the selected track's channel: a lock on step %d (%d), the sound kept", s + 1, LK(0, s, QD_QP_FREQ));
    cc(2, 74, 5);
    CHECK(A.model.t[1].patch[QD_QP_FREQ] != f1 && !nlocks(1, QD_QP_FREQ), "track 2's channel: a live edit, no lock");
    cc(15, 74, 9);
    CHECK(LK(0, A.ui.auto_step, QD_QP_FREQ) == qd_midi_cc_value(QD_QP_FREQ, 9), "channel 15: recorded too");
    btap(QD_B_PLAY);
}

/* ================================================================ MIDI CH */
static void t_midi_ch(void)
{
    qd_tset_t *ts;
    ui_menu_t mv;
    int i;
    start();
    ts = &PAT()->trk[0];
    btap(QD_B_PATTERN);
    for (i = 0; i < 5; i++)
        turn(QD_K_4, 5);
    qd_view_menu(&A, &mv);
    CHECK(ts->chan == 14 && mv.chan == 15, "KNOB 4 MIDI CH: up to 15 (%d)", mv.chan);
    ts->chan = 15;                                      /* (a project's 16, stored before) */
    qd_view_menu(&A, &mv);
    CHECK(mv.chan == 16, "a stored 16 shows 16");
    turn(QD_K_4, 1);
    CHECK(ts->chan == 15, "turned up: stays 16");
    btap(QD_B_HOME);
    qd_app_midi(&A, 0x9F, 30, 100);                     /* (below the remote's keys) */
    run(10);
    CHECK(!sounding_any(), "track 1 on 16: channel 16 does not play it");
    qd_app_midi(&A, 0x8F, 30, 0);
    btap(QD_B_PATTERN);
    turn(QD_K_4, -1);
    CHECK(ts->chan == 14, "turned down: 15");
    btap(QD_B_HOME);
}

int main(void)
{
    static const struct {
        const char *name;
        void (*fn)(void);
    } G[] = {
        {"the map: the plan's numbers, both ways, 0..127", t_map},
        {"channels: a track's, 15 the selected, 16 none; the mixer", t_channels},
        {"lists: LFO DEST, MACHINE (LEGACY's TYPE), RATIO B", t_lists},
        {"REC + AUTOMATION: the selected track records", t_rec},
        {"MIDI CH 1..15, a stored 16", t_midi_ch},
    };
    size_t i;
    for (i = 0; i < sizeof G / sizeof G[0]; i++) {
        const int f0 = fails, c0 = checks;
        G[i].fn();
        printf("  %-60s %d checks%s\n", G[i].name, checks - c0, fails > f0 ? "  FAILED" : "");
    }
    printf("test_midi: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
