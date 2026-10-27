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
 *   remote     (§3) channel 16: a note presses its key (a step made, as the panel's key; its velocity), a button's CC
 *              opens what the button opens (a press on a change only), a relative encoder turns SELECT's page, a
 *              fader turns KNOB 1 by its moves (the first only noted; no acceleration however fast), CC 7 MASTER,
 *              CC 123 lets go of what the remote holds
 *   midi out   (c, §4) track settings page 3's MIDI OUT 3: the sequencer's notes out on channel 3 as it plays them
 *              (each ON its OFF), a live key's, MIDI in's; STOP the OFFs owed; MIDI OUT changed: the OFF on the old
 *              channel; OFF: nothing; through an arp (its notes); the panic
 *   clock out  START, 24 clocks a beat, STOP; CLOCK OUT OFF: silent; following an outside clock: silent
 *   out drop   (c2) a new USB connection: the queue emptied; an ON that never went out is owed no more, one that went
 *              keeps its OFF; 31 notes owed at once (six tracks of 4-note chords and more)
 *   midi track (e, §5) ENGINE MIDI (MIDI OUT 1); no voice, the notes out, no CC while untouched; EDIT's KNOB 1..4:
 *              CC 16..19 at once (no FM TONE CC); SELECT, B7 the pages; SHIFT + KNOB 1 the CC number (none sent); a held
 *              step's lock: sent at its step before its note, the track's value back at the next step; untouched slots
 *              never sent; B11, F2 + a knob clear; REC + AUTOMATION records (and sends); the step view locks nothing; a
 *              CC in sets nothing; ENGINE FM TONE: INIT, voices again
 *   cc out     a knob turn on FREQ: CC 74 on the track's MIDI CH (once, its value); a CC 74 coming in: no echo; the
 *              mixer's LEVEL: CC 7; CC OUT OFF: silent
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

/* (c) MIDI OUT as the port takes it, after every audio call: the packets' status, d1, d2 */
#define NLOG 8192
static uint8_t LG[NLOG][3];
static int nlog, nodrain;
static void drain(void)
{
    uint32_t p;
    if (nodrain)                                        /* ((c2) nobody listening: the queue fills) */
        return;
    while (qd_app_midi_out_take(&A, &p, 64u))
        if (nlog < NLOG)
            LG[nlog][0] = (uint8_t)(p >> 8), LG[nlog][1] = (uint8_t)(p >> 16), LG[nlog][2] = (uint8_t)(p >> 24), nlog++;
}
static int nsent(int st, int d1, int d2)                /* (d1, d2 -1: any) */
{
    int i, n = 0;
    for (i = 0; i < nlog; i++)
        n += LG[i][0] == st && (d1 < 0 || LG[i][1] == d1) && (d2 < 0 || LG[i][2] == d2);
    return n;
}
static int nnotes(void)                                 /* notes (ON, OFF) in the log */
{
    int i, n = 0;
    for (i = 0; i < nlog; i++)
        n += (LG[i][0] & 0xE0) == 0x80;
    return n;
}
static int first(int st)                                /* its first place in the log (-1 none) */
{
    int i;
    for (i = 0; i < nlog; i++)
        if (LG[i][0] == st)
            return i;
    return -1;
}

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
            drain();
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
    nlog = 0;
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

/* ================================================================ the remote */
static void note16(int note, int vel)
{
    qd_app_midi(&A, vel ? 0x9F : 0x8F, note, vel);
}
static void t_remote(void)
{
    const int w2 = QD_WHITE_KEY[2];
    int f0, p0;
    start();
    note16(QD_NOTE0 + w2, 77);
    run(30);
    CHECK(((A.keys_held >> w2) & 1u) && A.key_vel[w2] == 77 && A.rm_keys == 1u << w2,
          "note %d on 16: key %d down, velocity 77", QD_NOTE0 + w2, w2);
    note16(QD_NOTE0 + w2, 0);
    run(10);
    CHECK(!A.keys_held && !A.rm_keys && qd_step_is_on(qd_step(PAT(), 0, 2)), "velocity 0: up; the tap made step 3, as W3's");
    qd_app_key(&A, w2, 1, 100);                         /* (the panel's own key: the same step off again) */
    run(30);
    qd_app_key(&A, w2, 0, 0);
    run(10);
    CHECK(!qd_step_is_on(qd_step(PAT(), 0, 2)), "the panel's W3: off again (the same key)");
    note16(QD_NOTE0 - 1, 100);
    note16(QD_NOTE0 + QD_NKEY, 100);
    run(10);
    CHECK(!A.keys_held && !sounding_any(), "notes outside 53..79: nothing");
    note16(QD_NOTE0 - 1, 0);
    note16(QD_NOTE0 + QD_NKEY, 0);
    /* the buttons */
    cc(16, QD_RM_BTN0 + QD_B_SOUND, 127);
    CHECK((A.buttons_held >> QD_B_SOUND) & 1u, "CC %d 127: EDIT down", QD_RM_BTN0 + QD_B_SOUND);
    cc(16, QD_RM_BTN0 + QD_B_SOUND, 100);
    cc(16, QD_RM_BTN0 + QD_B_SOUND, 0);
    cc(16, QD_RM_BTN0 + QD_B_SOUND, 10);
    run(20);
    CHECK(qd_snd_open(&A) && !A.buttons_held && !A.rm_btns, "EDIT tapped by CC (100 and 10 no change): the sound editor");
    /* the encoders */
    p0 = A.ui.snd_page;
    cc(16, QD_RM_ENC0 + QD_K_SELECT, 65);
    CHECK(A.ui.snd_page == (p0 + 1) % QD_SND_NPAGE, "CC %d 65: SELECT a detent right, the next page",
          QD_RM_ENC0 + QD_K_SELECT);
    cc(16, QD_RM_ENC0 + QD_K_SELECT, 62);
    CHECK(A.ui.snd_page == (p0 - 1 + QD_SND_NPAGE) % QD_SND_NPAGE, "62: two left (%d)", A.ui.snd_page);
    cc(16, QD_RM_ENC0 + QD_K_SELECT, 64);
    CHECK(A.ui.snd_page == (p0 - 1 + QD_SND_NPAGE) % QD_SND_NPAGE, "64: nothing");
    /* the faders: FILTER's KNOB 1 is FREQ */
    A.ui.snd_page = qd_snd_page_of("FILTER");
    f0 = A.model.t[0].patch[QD_QP_FREQ];
    cc(16, QD_RM_FADER0, 100);
    CHECK(A.model.t[0].patch[QD_QP_FREQ] == f0, "CC %d's first message: only noted", QD_RM_FADER0);
    cc(16, QD_RM_FADER0, 90);
    CHECK(A.model.t[0].patch[QD_QP_FREQ] == f0 - 10, "100 -> 90: KNOB 1 -10 detents, FREQ %d -> %d", f0,
          A.model.t[0].patch[QD_QP_FREQ]);
    {
        int v;
        for (v = 89; v >= 80; v--)                      /* (fast: no tick between: still one a step) */
            qd_app_midi(&A, 0xBF, QD_RM_FADER0, v);
    }
    CHECK(A.model.t[0].patch[QD_QP_FREQ] == f0 - 20, "90 -> 80 a step at a time, fast: -10 more, no acceleration (%d)",
          A.model.t[0].patch[QD_QP_FREQ]);
    cc(16, QD_RM_FADER0 + 1, 50);
    cc(16, QD_RM_FADER0 + 1, 53);
    CHECK(A.rm_fader[0] == 81 && A.rm_fader[1] == 54, "each fader its own last value");
    /* MASTER */
    cc(16, 7, 127);
    CHECK(A.master_pot == 1023, "CC 7 127: MASTER 1023");
    cc(16, 7, 0);
    CHECK(A.master_pot == 0, "CC 7 0: MASTER 0");
    cc(16, 7, 64);
    CHECK(A.master_pot == 516, "CC 7 64: MASTER 516 (%d)", A.master_pot);
    /* CC 123 */
    btap(QD_B_HOME);
    cc(16, QD_RM_BTN0 + QD_B_FUNC, 127);
    note16(QD_NOTE0 + 5, 90);
    run(10);
    CHECK(((A.buttons_held >> QD_B_FUNC) & 1u) && ((A.keys_held >> 5) & 1u), "SHIFT and a key held by the remote");
    cc(16, 123, 0);
    CHECK(!A.buttons_held && !A.keys_held && !A.rm_btns && !A.rm_keys, "CC 123: both let go");
    run(QD_UNDO_QUIET_MS);
}

/* ================================================================ (c) MIDI OUT */
static void mout_set(int v)                             /* track settings page 3, KNOB 4 */
{
    btap(QD_B_PATTERN);
    turn(QD_K_SELECT, 2);
    turn(QD_K_4, v - PAT()->trk[A.sel].mout);
    btap(QD_B_HOME);
}
static int pairs_ok(int ch)                             /* every ON on channel ch has as many OFFs (note by note) */
{
    int n;
    for (n = 0; n < 128; n++)
        if (nsent(0x90 | (ch - 1), n, -1) != nsent(0x80 | (ch - 1), n, -1))
            return 0;
    return 1;
}
static void t_out_notes(void)
{
    ui_menu_t mv;
    int i, ons;
    start();
    mout_set(3);
    btap(QD_B_PATTERN);
    turn(QD_K_SELECT, 2);
    qd_view_menu(&A, &mv);
    CHECK(PAT()->trk[0].mout == 3 && mv.page == 2 && mv.mout == 3, "page 3 KNOB 4: MIDI OUT 3 (%d)", PAT()->trk[0].mout);
    turn(QD_K_4, 20);
    CHECK(PAT()->trk[0].mout == 16, "up to 16");
    turn(QD_K_4, -20);
    qd_view_menu(&A, &mv);
    CHECK(PAT()->trk[0].mout == 0 && mv.mout == 0, "down to OFF");
    turn(QD_K_4, 3);
    btap(QD_B_HOME);
    for (i = 0; i < 4; i++) {                           /* steps 1, 5, 9, 13 on track 1 (the panel's W keys) */
        qd_app_key(&A, QD_WHITE_KEY[4 * i], 1, 100);
        run(30);
        qd_app_key(&A, QD_WHITE_KEY[4 * i], 0, 0);
        run(10);
    }
    run(200);
    nlog = 0;
    btap(QD_B_PLAY);
    run(1500);
    ons = nsent(0x92, -1, -1);
    CHECK(ons >= 4 && nsent(0x92, -1, -1) == nsent(0x92, -1, 100) && !nsent(0x90, -1, -1) && !nsent(0x93, -1, -1),
          "playing: the steps' notes out on channel 3, velocity 100, on no other channel (%d)", ons);
    for (i = 0; i < 500 && !A.mo_n; i++)
        run(1);
    CHECK(A.mo_n == 1, "a note sounding: its OFF owed (%d)", A.mo_n);
    btap(QD_B_PLAY);
    CHECK(!A.seq.playing && !A.mo_n && pairs_ok(3), "STOP: every OFF owed went (ons %d, offs %d)", nsent(0x92, -1, -1),
          nsent(0x82, -1, -1));
    /* a live key, MIDI in */
    nlog = 0;
    qd_app_note_on(&A, 0, 60, 90);
    run(10);
    CHECK(nsent(0x92, 60, 90) == 1, "a live key: out on 3");
    qd_app_note_off(&A, 0);
    run(10);
    CHECK(nsent(0x82, 60, -1) == 1 && !A.mo_n, "its OFF");
    qd_app_midi(&A, 0x90, 64, 101);                     /* (channel 1 plays track 1) */
    run(10);
    CHECK(nsent(0x92, 64, 101) == 1, "MIDI in on track 1's channel: out on 3");
    qd_app_midi(&A, 0x80, 64, 0);
    run(10);
    CHECK(nsent(0x82, 64, -1) == 1, "its OFF");
    /* MIDI OUT changed while a note sounds: its OFF on 3 */
    qd_app_note_on(&A, 0, 62, 80);
    run(10);
    nlog = 0;
    mout_set(4);
    CHECK(nsent(0x82, 62, -1) == 1 && !A.mo_n, "MIDI OUT 3 -> 4: the held note's OFF on 3");
    qd_app_note_off(&A, 0);
    run(10);
    CHECK(!nsent(0x83, 62, -1) && nsent(0x82, 62, -1) == 1, "the key let go: nothing more");
    qd_app_note_on(&A, 0, 63, 80);
    run(10);
    CHECK(nsent(0x93, 63, 80) == 1, "the next note: on 4");
    nlog = 0;
    mout_set(0);
    CHECK(nsent(0x83, 63, -1) == 1, "MIDI OUT OFF: its OFF on 4");
    qd_app_note_off(&A, 0);
    qd_app_note_on(&A, 0, 65, 80);
    run(10);
    qd_app_note_off(&A, 0);
    btap(QD_B_PLAY);
    run(500);
    btap(QD_B_PLAY);
    CHECK(nnotes() == 1, "OFF: no note more out, playing or played (%d)", nnotes());
    /* the panic */
    mout_set(3);
    qd_app_note_on(&A, 0, 60, 90);
    qd_app_note_on(&A, 1, 67, 90);
    run(10);
    nlog = 0;
    qd_app_midi_out_panic(&A);
    run(10);
    CHECK(nsent(0x82, 60, -1) == 1 && nsent(0x82, 67, -1) == 1 && !A.mo_n, "the panic: every OFF owed");
    qd_app_note_off(&A, 0);
    qd_app_note_off(&A, 1);
    run(10);
    CHECK(nlog == 2, "the keys let go after: nothing more");
    /* through an arp (MIDI FX group 1 on track 1): its notes out */
    qd_mfx_slot_init(&A.proj.mfx[0].slot[0], QD_MFX_ARP);
    PAT()->trk[0].mfx = 1;
    nlog = 0;
    qd_app_note_on(&A, 0, 60, 90);
    qd_app_note_on(&A, 1, 64, 90);
    run(3000);
    qd_app_note_off(&A, 0);
    qd_app_note_off(&A, 1);
    run(500);
    CHECK(nsent(0x92, 60, -1) >= 3 && nsent(0x92, 64, -1) >= 3 && pairs_ok(3) && !A.mo_n,
          "an arp on two keys: its notes out on 3 (%d, %d), each with its OFF", nsent(0x92, 60, -1), nsent(0x92, 64, -1));
    run(QD_UNDO_QUIET_MS);
}

/* (c2) the port drops the queue at a new USB connection (fz_port.c fz_midi_out: plat_midi_out_link changed) */
static void t_out_drop(void)
{
    int i;
    start();
    mout_set(3);
    nlog = 0;
    qd_app_note_on(&A, 0, 60, 90);
    run(10);
    CHECK(nsent(0x92, 60, 90) == 1 && A.mo_n == 1, "C4 out, its OFF owed");
    nodrain = 1;                                        /* (nobody listening) */
    qd_app_note_on(&A, 1, 62, 90);
    run(10);
    CHECK(A.mo_n == 2 && A.mo_w != A.mo_r, "D4 queued, not taken: owed too (%d)", A.mo_n);
    qd_app_midi_out_drop(&A);
    nodrain = 0;
    CHECK(A.mo_w == A.mo_r && A.mo_n == 1 && (A.mo_on[0] & 127u) == 60u,
          "a new connection: the queue empty, D4 (never out) owed no more, C4 (out) still owed");
    qd_app_note_off(&A, 0);
    qd_app_note_off(&A, 1);
    run(10);
    CHECK(nsent(0x82, 60, -1) == 1 && !nsent(0x82, 62, -1) && !nsent(0x92, 62, -1) && !A.mo_n,
          "then: C4's OFF on the new connection, nothing of D4");
    /* 31 notes owed at once (the 32-packet queue keeps room for every OFF) */
    nlog = 0;
    for (i = 0; i < QD_NTRACK; i++)
        PAT()->trk[i].mout = 3;
    for (i = 0; i < 31; i++) {                          /* (six tracks, five or six notes each; a block's worth at a
                                                         * time: one block holds 16 ONs at most, each keeping room for
                                                         * its OFF and those owed) */
        qd_player_key_on(&A.player, i % 6, 30 + i, 90);
        if (i % 4 == 3 || i >= 28)
            run(5);
    }
    run(10);
    CHECK(A.mo_n == 31 && nsent(0x92, -1, -1) == 31, "31 notes out, all owed (%d)", A.mo_n);
    qd_app_midi_out_panic(&A);
    run(10);
    CHECK(!A.mo_n && pairs_ok(3), "the panic: their 31 OFFs");
    for (i = 0; i < 31; i++)
        qd_player_key_off(&A.player, i % 6, 30 + i);
    for (i = 1; i < QD_NTRACK; i++)
        PAT()->trk[i].mout = 0;
    run(QD_UNDO_QUIET_MS);
}

/* ================================================================ (e) the MIDI track */
static int first_at(int st, int d1, int d2, int from)   /* the first such packet at or after from (-1 none) */
{
    int i;
    for (i = from; i < nlog; i++)
        if (LG[i][0] == st && (d1 < 0 || LG[i][1] == d1) && (d2 < 0 || LG[i][2] == d2))
            return i;
    return -1;
}
static int track_voices(int t)
{
    int i, n = 0;
    for (i = 0; i < A.pool.nvoice; i++)
        n += qd_pool_used(&A.pool, i) && A.pool.v[i].track == t;
    return n;
}
static int ccs_in(int lo, int hi)                       /* CCs lo..hi (any channel) in the log */
{
    int i, n = 0;
    for (i = 0; i < nlog; i++)
        n += (LG[i][0] & 0xF0) == 0xB0 && LG[i][1] >= lo && LG[i][1] <= hi;
    return n;
}
static void engine_set(int midi)                        /* track settings page 3, KNOB 3 */
{
    btap(QD_B_PATTERN);
    turn(QD_K_SELECT, 2);
    turn(QD_K_3, midi ? 1 : -1);
    btap(QD_B_HOME);
}
static void t_mtrack(void)
{
    ui_menu_t mv;
    ui_snd_t sv;
    const int i1 = QD_MSND_ID[1], i2 = QD_MSND_ID[2];
    int i, v0, v1, a, b, n, voices = 0, ons;
    start();
    btap(QD_B_PATTERN);
    turn(QD_K_SELECT, 2);
    turn(QD_K_3, 1);
    qd_view_menu(&A, &mv);
    CHECK(qd_msnd_is(A.model.t[0].patch) && qd_msnd_is(PAT()->snd[0].patch) && mv.engine == 1 && PAT()->trk[0].mout == 1,
          "page 3 KNOB 3: ENGINE MIDI, MIDI OUT was OFF: 1 (the track's number)");
    btap(QD_B_HOME);
    for (i = 0; i < 4; i++) {                           /* steps 1, 5, 9, 13 */
        qd_app_key(&A, QD_WHITE_KEY[4 * i], 1, 100);
        run(30);
        qd_app_key(&A, QD_WHITE_KEY[4 * i], 0, 0);
        run(10);
    }
    run(200);
    /* no voice, the notes out */
    nlog = 0;
    btap(QD_B_PLAY);
    for (i = 0; i < 30; i++) {
        run(50);
        voices += track_voices(0);
    }
    btap(QD_B_PLAY);
    ons = nsent(0x90, -1, -1);
    CHECK(!voices && ons >= 4 && pairs_ok(1) && !ccs_in(0, 127), "playing a MIDI track: no voice ever, its notes out on 1 "
          "(%d), every slot untouched: no CC (%d)", ons, ccs_in(0, 127));
    /* EDIT: page 1's KNOB 1..4, CC 16..19 on 1 */
    btap(QD_B_SOUND);
    CHECK(qd_msnd_open(&A), "EDIT on a MIDI track: its CCs' editor");
    nlog = 0;
    turn(QD_K_1, 10);
    turn(QD_K_2, 3);
    turn(QD_K_3, 1);
    turn(QD_K_4, 127);
    v0 = A.model.t[0].patch[QD_MSND_ID[0]], v1 = A.model.t[0].patch[i1];
    CHECK(nsent(0xB0, 16, v0) == 1 && nsent(0xB0, 17, v1) == 1 && nsent(0xB0, 18, A.model.t[0].patch[i2]) == 1 &&
              nsent(0xB0, 19, 127) == 1 && nlog == 4 && v0 > 0 && v1 > 0,
          "KNOB 1..4: CC 16..19 on channel 1 at once, their values (%d, %d), nothing else (no FM TONE CC: %d out)", v0, v1,
          nlog);
    turn(QD_K_4, 5);
    CHECK(nlog == 4, "KNOB 4 at 127 turned up: nothing more");
    qd_view_snd(&A, &sv);
    CHECK(sv.midi && sv.ipg == 0 && sv.has[0] && sv.cc[0] == 16 && sv.v[0] == v0 && sv.mch == 1, "the page: slot 1 "
          "touched, CC 16, %d", v0);
    /* SELECT, B2: page 2, untouched */
    turn(QD_K_SELECT, 1);
    qd_view_snd(&A, &sv);
    CHECK(A.ui.msnd_page == 1 && !sv.has[0] && !sv.has[3] && sv.cc[0] == 20 && sv.cc[3] == 23,
          "SELECT: page 2, CC 20..23, untouched");
    qd_app_key(&A, QD_BLACK_KEY[6], 1, 100);
    run(10);
    qd_app_key(&A, QD_BLACK_KEY[6], 0, 0);
    run(10);
    qd_view_snd(&A, &sv);
    CHECK(A.ui.msnd_page == 6 && sv.cc[0] == 40 && sv.cc[3] == 43, "B7: page 7, CC 40..43");
    qd_app_key(&A, QD_BLACK_KEY[0], 1, 100);
    run(10);
    qd_app_key(&A, QD_BLACK_KEY[0], 0, 0);
    run(10);
    /* SHIFT + KNOB 1: the CC number, nothing sent */
    nlog = 0;
    bdown(QD_B_FUNC);
    run(10);
    turn(QD_K_1, 58);
    qd_view_snd(&A, &sv);
    bup(QD_B_FUNC);
    run(10);
    CHECK((A.model.t[0].patch[QD_MSND_CC(0)] & 127) == 74 && !nlog && sv.shift && sv.shift_k == 0 && sv.cc[0] == 74 &&
              A.model.t[0].patch[QD_MSND_ID[0]] == v0, "SHIFT + KNOB 1: CC 16 -> 74, nothing sent, the value kept");
    turn(QD_K_1, 1);
    CHECK(nsent(0xB0, 74, v0 + 1) == 1 && nlog == 1, "KNOB 1 then: CC 74");
    v0 = A.model.t[0].patch[QD_MSND_ID[0]];
    /* a p-lock: step 1 held, KNOB 2 */
    qd_app_key(&A, QD_WHITE_KEY[0], 1, 100);
    run(300);
    nlog = 0;
    turn(QD_K_2, -2);
    qd_view_snd(&A, &sv);
    qd_app_key(&A, QD_WHITE_KEY[0], 0, 0);
    run(20);
    a = LK(0, 0, i1);
    CHECK(a == v1 - 2 && A.model.t[0].patch[i1] == v1 && sv.step == 1 && sv.locked[1] && sv.v[1] == a && sv.tv[1] == v1,
          "step 1 held, KNOB 2: a lock (%d), the track's value kept (%d); the page shows the lock, the track's tick", a, v1);
    /* playing: the lock at step 1 (before its note), the track's value back at step 5 */
    nlog = 0;
    btap(QD_B_PLAY);
    run(700);
    btap(QD_B_PLAY);
    a = first_at(0xB0, 17, LK(0, 0, i1), 0);
    b = a >= 0 ? first_at(0xB0, 17, v1, a) : -1;
    n = a >= 0 ? first_at(0x90, -1, -1, a) : -1;
    CHECK(a >= 0 && b > a && n > a && n < b && first_at(0x90, -1, -1, n + 1) > b,
          "the lock goes out at step 1 before its note (%d, %d), the track's value back at step 5, before its note (%d)",
          a, n, b);
    CHECK(!ccs_in(20, 43) && !nsent(0xB0, 16, -1), "untouched slots (pages 2..7) never sent, nor CC 16 (now 74)");
    /* B11: the held step's locks on this page cleared */
    qd_app_key(&A, QD_WHITE_KEY[0], 1, 100);
    run(300);
    qd_app_key(&A, QD_BLACK_KEY[10], 1, 100);
    run(10);
    qd_app_key(&A, QD_BLACK_KEY[10], 0, 0);
    run(10);
    qd_app_key(&A, QD_WHITE_KEY[0], 0, 0);
    run(20);
    CHECK(LK(0, 0, i1) == -99999, "B11 with step 1 held: its lock cleared");
    /* REC + AUTOMATION: KNOB 3 recorded as locks, sent as turned */
    btap(QD_B_PLAY);
    run(100);
    bdown(QD_B_REC);
    nlog = 0;
    for (i = 0; i < 6; i++) {
        turn(QD_K_3, 4);
        run(60);
    }
    bup(QD_B_REC);
    run(10);
    n = nlocks(0, i2);
    CHECK(n > 0 && nsent(0xB0, 18, -1) >= 6 && A.model.t[0].patch[i2] < 20, "REC held + KNOB 3: %d locks recorded, each "
          "move sent (%d), the track's value kept", n, nsent(0xB0, 18, -1));
    nlog = 0;
    run(2 * 60000 / A.bpm * 4 + 200);                   /* (the 16 steps twice over) */
    btap(QD_B_PLAY);
    for (i = 0, a = 0, b = 0; i < QD_NSTEP; i++)        /* (a recorded step without a note: a LOCK trig) */
        if (LK(0, i, i2) != -99999) {
            a += nsent(0xB0, 18, LK(0, i, i2)) >= 2;
            b += qd_step_type(PAT(), 0, i) == QD_TRIG_LOCK;
        }
    CHECK(a == n && b > 0, "played back twice: every recorded lock's value sent each time (%d of %d; %d on LOCK trigs)",
          a, n, b);
    /* F2 + KNOB 3: that slot's locks cleared */
    bdown(QD_B_OCTUP);
    turn(QD_K_3, 1);
    bup(QD_B_OCTUP);
    run(10);
    CHECK(nlocks(0, i2) == 0, "F2 + KNOB 3: the slot's locks cleared");
    /* the step view on a MIDI track: a held step's KNOB 2 locks nothing (no FM TONE value) */
    btap(QD_B_SOUND);
    qd_app_key(&A, QD_WHITE_KEY[4], 1, 100);
    run(300);
    turn(QD_K_2, 5);
    qd_app_key(&A, QD_WHITE_KEY[4], 0, 0);
    run(20);
    CHECK(qd_lock_count(PAT(), 0, 4) == 0, "the step view: KNOB 2 on a held step of a MIDI track locks nothing");
    /* a CC in on its channel sets nothing */
    {
        int16_t keep[QD_QUAD_NP];
        memcpy(keep, A.model.t[0].patch, sizeof keep);
        cc(1, 74, 9);
        cc(1, 73, 100);
        CHECK(!memcmp(keep, A.model.t[0].patch, sizeof keep), "CCs in on its channel: the MIDI sound untouched");
    }
    /* ENGINE back to FM TONE: INIT, voices again */
    engine_set(0);
    {
        int16_t init[QD_QUAD_NP];
        qd_quad_preset_patch(0, init);
        CHECK(!qd_msnd_is(A.model.t[0].patch) && !memcmp(A.model.t[0].patch, init, sizeof init) &&
                  !strcmp(A.model.t[0].sound_name, "INIT"), "ENGINE FM TONE: the track's sound INIT");
    }
    voices = 0;
    btap(QD_B_PLAY);
    for (i = 0; i < 10; i++) {
        run(50);
        voices += track_voices(0);
    }
    btap(QD_B_PLAY);
    CHECK(voices > 0, "and its notes take voices again");
    engine_set(1);
    CHECK(qd_msnd_is(A.model.t[0].patch) && (A.model.t[0].patch[QD_MSND_CC(0)] & 0xFF) == 16 && PAT()->trk[0].mout == 1,
          "MIDI again: a new MIDI sound (untouched, CC 16), MIDI OUT kept (1)");
    run(QD_UNDO_QUIET_MS);
}

static void t_out_clock(void)
{
    int i, f8, want;
    start();
    nlog = 0;
    btap(QD_B_PLAY);
    run(2000);
    f8 = nsent(0xF8, -1, -1);
    want = (int)(qd_seq_tick(&A.seq) * 24 / QD_BEAT_TICKS) + 1;
    CHECK(nsent(0xFA, -1, -1) == 1 && first(0xFA) < first(0xF8) && first(0xFA) == 0, "PLAY: START first");
    CHECK(f8 >= want - 1 && f8 <= want, "24 clocks a beat: %d for %d ticks (%d)", f8,
          (int)qd_seq_tick(&A.seq), want);
    CHECK(f8 >= 2000 * A.bpm * 24 / 60000 - 2 && f8 <= 2000 * A.bpm * 24 / 60000 + 2, "%d BPM, 2 s: %d clocks", A.bpm,
          f8);
    btap(QD_B_PLAY);
    run(100);
    CHECK(nsent(0xFC, -1, -1) == 1 && nsent(0xF8, -1, -1) <= f8 + 1, "STOP: STOP, the clock stops");
    /* CLOCK OUT OFF */
    qd_latch_open(&A, QD_LATCH_SET);
    turn(QD_K_3, -1);
    CHECK(!A.ui.clk_out, "SETTINGS KNOB 3: CLOCK OUT OFF");
    turn(QD_K_3, 1);
    CHECK(A.ui.clk_out, ".. and ON");
    turn(QD_K_3, -1);
    btap(QD_B_HOME);
    nlog = 0;
    btap(QD_B_PLAY);
    run(1000);
    btap(QD_B_PLAY);
    run(100);
    CHECK(!nlog, "CLOCK OUT OFF: no START, clock, STOP (%d)", nlog);
    A.ui.clk_out = 1;
    /* following an outside clock */
    nlog = 0;
    qd_app_midi(&A, 0xFA, 0, 0);
    for (i = 0; i < 48; i++) {
        qd_app_midi(&A, 0xF8, 0, 0);
        run(20);
    }
    CHECK(A.seq.playing && A.seq.ext && !nlog, "following MIDI clock: nothing out (%d)", nlog);
    qd_app_midi(&A, 0xFC, 0, 0);
    run(50);
    CHECK(!A.seq.playing && !nlog, "its STOP: nothing out");
    run(QD_UNDO_QUIET_MS);
}

static void t_out_cc(void)
{
    int f1, v, i, want = 0;
    start();
    btap(QD_B_SOUND);
    A.ui.snd_page = qd_snd_page_of("FILTER");
    run(10);
    nlog = 0;
    for (i = 0; i < 40; i++) {                          /* (FREQ's 12700 over 0..127: a CC only when it moves) */
        const int c0 = qd_midi_value_cc(QD_QP_FREQ, A.model.t[0].patch[QD_QP_FREQ]);
        turn(QD_K_1, -4);
        want += qd_midi_value_cc(QD_QP_FREQ, A.model.t[0].patch[QD_QP_FREQ]) != c0;
    }
    f1 = A.model.t[0].patch[QD_QP_FREQ];
    v = qd_midi_value_cc(QD_QP_FREQ, f1);
    CHECK(want > 0 && nsent(0xB0, 74, -1) == want && nlog == want && LG[nlog - 1][2] == v,
          "FREQ turned (%d): CC 74 on track 1's MIDI CH each time its CC value moved (%d of 40 turns), the last %d", f1,
          want, v);
    nlog = 0;
    cc(1, 74, 9);
    CHECK(A.model.t[0].patch[QD_QP_FREQ] == qd_midi_cc_value(QD_QP_FREQ, 9) && !nlog, "CC 74 in: set, not sent back");
    PAT()->trk[0].chan = 5;
    turn(QD_K_1, 200);
    CHECK(nsent(0xB5, 74, qd_midi_value_cc(QD_QP_FREQ, A.model.t[0].patch[QD_QP_FREQ])) == 1 && nlog == 1,
          "MIDI CH 6: on 6");
    PAT()->trk[0].chan = 15;
    turn(QD_K_1, -200);
    CHECK(nlog == 1, "MIDI CH 16 (the remote's): none");
    PAT()->trk[0].chan = 0;
    btap(QD_B_HOME);
    btap(QD_B_PAGE);                                    /* (ENV printed: the mix) */
    nlog = 0;
    turn(QD_K_1, -6);
    CHECK(A.ui.latch == QD_LATCH_MIX && nsent(0xB0, 7, qd_midi_value_cc(QD_LK_LEVEL, A.model.t[0].mix.level)) == 1 &&
              nlog == 1, "the mix's LEVEL: CC 7 (%d out)", nlog);
    btap(QD_B_HOME);
    qd_latch_open(&A, QD_LATCH_SET);
    turn(QD_K_4, -1);
    CHECK(!A.ui.cc_out, "SETTINGS KNOB 4: CC OUT OFF");
    btap(QD_B_HOME);
    btap(QD_B_SOUND);
    A.ui.snd_page = qd_snd_page_of("FILTER");
    run(10);
    nlog = 0;
    turn(QD_K_1, 5);
    CHECK(!nlog, "CC OUT OFF: nothing (%d)", nlog);
    run(QD_UNDO_QUIET_MS);
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
        {"the remote: keys, buttons, encoders, faders, MASTER, CC 123", t_remote},
        {"MIDI OUT: the notes, STOP, a channel changed, OFF, an arp", t_out_notes},
        {"MIDI OUT (c2): a new USB connection; 31 notes owed", t_out_drop},
        {"MIDI OUT: START, 24 clocks a beat, STOP; CLOCK OUT; outside", t_out_clock},
        {"MIDI OUT: the knobs' CCs, none echoed; CC OUT", t_out_cc},
        {"the MIDI track: ENGINE, no voice, CCs, SHIFT, locks, B11, REC", t_mtrack},
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
