/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.6c: the MIDI FX groups' tests (core/seq/mfx.h; research/OMX-MIDIFX.md, the OMX-27 v1.15.4 rules):
 *
 *   the engine alone (a recording output: each note on and off with its tick, the notes sounding counted as a pool
 *   would: an off ends every voice of that track and pitch):
 *     an empty group; TRANSPOSE (out of range dropped); CHANCE (0, 100, about half); HARM (duplicates, ORIG); SCALER
 *     (snapping down, OWN and TRACK); CHORD (shapes, NOTES, INVERT, VOICING, SPREAD, the velocity kept); MONO; RANDOM
 *     (the ranges reached and kept, OCTAVE, DELAY and its cancel, LENGTH); SELECTOR (UP / DOWN / RND, the block
 *     skipped, chance, its note-offs); REPEAT (ON, 1-ST, ONCE, HOLD, FADE, GATE, QUANT); ARP (the 17 patterns on
 *     C E G B, RANGE and OCT DIST, the modes, the timing and QUANT, GATE, the MOD lane's 14 values and ties, the
 *     TRANSPOSE lane, RESET NOTE / MPAT, the first note's velocity, two tracks in one arp, the arp toggle)
 *     the note-off bookkeeping: random chains of every effect, random notes on and off: once every key is up and
 *     the timed notes have ended nothing sounds (no latching modes); after a reset nothing sounds (any)
 *   through the sequencer and the player (the pool's voices):
 *     a track routed through TRANSPOSE, an empty group the same timeline as none, the step's OFF / group locks, two
 *     tracks in one arp each on its own track, the voice limit after the chain, no stuck voices after a pattern
 *     change, STOP and a mute, the keys through the group while stopped (the arp runs on the tempo)
 *
 *     test_mfx                      exit 1 on a failed check */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mfx.h"
#include "mix.h"
#include "player.h"
#include "scale.h"
#include "seq.h"

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

#define TPS 100800LL                    /* a 16th */
#define BAR (16 * TPS)

/* ================================================================ the engine alone */
typedef struct {
    int on, track, pitch, vel;
    int64_t tick;
    int32_t len;
} ev_t;
static qd_mfx_group_t G[QD_MFX_NGROUP];
static qd_mfx_t M;
static ev_t EV[20000];
static int NEV;
static int SOUND[8][128];
static int KEY_SCALE = QD_SCALE_CHROMATIC, KEY_ROOT;

static void io_on(void *ctx, int g, const qd_mfx_note_t *n)
{
    (void)ctx, (void)g;
    if (NEV < (int)(sizeof EV / sizeof EV[0]))
        EV[NEV].on = 1, EV[NEV].track = n->track, EV[NEV].pitch = n->pitch, EV[NEV].vel = n->vel,
        EV[NEV].tick = M.now, EV[NEV].len = n->len, NEV++;
    SOUND[n->track & 7][n->pitch]++;
}
static void io_off(void *ctx, int g, int track, int pitch)
{
    (void)ctx, (void)g;
    if (NEV < (int)(sizeof EV / sizeof EV[0]))
        EV[NEV].on = 0, EV[NEV].track = track, EV[NEV].pitch = pitch, EV[NEV].tick = M.now, NEV++;
    SOUND[track & 7][pitch] = 0;
}
static void io_key(void *ctx, int track, int *scale, int *root)
{
    (void)ctx, (void)track;
    *scale = KEY_SCALE, *root = KEY_ROOT;
}
static void fresh(void)
{
    qd_mfx_io_t io;
    memset(G, 0, sizeof G);
    io.on = io_on, io.off = io_off, io.key = io_key, io.ctx = 0;
    qd_mfx_init(&M, G, &io);
    NEV = 0;
    memset(SOUND, 0, sizeof SOUND);
}
static qd_mfx_slot_t *slot(int g, int k, int type)
{
    qd_mfx_slot_init(&G[g - 1].slot[k], type);
    return &G[g - 1].slot[k];
}
static void note(int g, int on, int track, int pitch, int vel, int64_t tick)
{
    qd_mfx_note_t n;
    memset(&n, 0, sizeof n);
    n.track = (uint8_t)track, n.pitch = n.prev = (uint8_t)pitch, n.vel = (uint8_t)vel, n.pat = 0, n.step = 0;
    qd_mfx_advance(&M, tick);
    qd_mfx_note(&M, g, on, &n, tick);
}
static int sounding(void)
{
    int t, p, n = 0;
    for (t = 0; t < 8; t++)
        for (p = 0; p < 128; p++)
            n += SOUND[t][p] > 0;
    return n;
}
/* the note-ons' pitches (from event i0), up to max */
static int ons(int i0, int *out, int max)
{
    int i, n = 0;
    for (i = i0; i < NEV && n < max; i++)
        if (EV[i].on)
            out[n++] = EV[i].pitch;
    return n;
}
static int same(const int *a, const int *b, int n)
{
    int i;
    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}
static const char *list(const int *a, int n)
{
    static char b[512];
    int i, o = 0;
    b[0] = 0;
    for (i = 0; i < n && o < 480; i++)
        o += snprintf(b + o, sizeof b - (size_t)o, "%s%d", i ? " " : "", a[i]);
    return b;
}

static void t_simple(void)
{
    int p[16], n, i, k;
    /* an empty group: straight through */
    fresh();
    note(1, 1, 0, 60, 100, 0);
    note(1, 0, 0, 60, 0, 10);
    CHECK(NEV == 2 && EV[0].on && EV[0].pitch == 60 && !EV[1].on && !sounding(), "an empty group: the note through");
    CHECK(!qd_mfx_group_active(&G[0]), "an empty group is not active");
    /* TRANSPOSE */
    fresh();
    slot(1, 0, QD_MFX_TRANSPOSE)->v[QD_MT_SEMI] = 5;
    G[0].slot[0].v[QD_MT_OCT] = -1;
    note(1, 1, 0, 60, 100, 0);
    note(1, 1, 0, 120, 100, 0);
    G[0].slot[0].v[QD_MT_SEMI] = 24, G[0].slot[0].v[QD_MT_OCT] = 1;
    note(1, 1, 0, 100, 100, 0);
    CHECK(NEV == 2 && EV[0].pitch == 53 && EV[1].pitch == 113, "TRANSPOSE +5 -1 oct: 60 -> 53, 120 -> 113; +24 +1 oct "
          "on 100: out of range, dropped (%d events)", NEV);
    note(1, 0, 0, 60, 0, 5);
    note(1, 0, 0, 120, 0, 5);
    note(1, 0, 0, 100, 0, 5);
    CHECK(!sounding(), "TRANSPOSE: the offs release what the ons made (by the pitch that entered)");
    /* CHANCE */
    for (k = 0; k < 3; k++) {
        const int c = k == 0 ? 0 : k == 1 ? 100 : 50;
        int got = 0;
        fresh();
        slot(1, 0, QD_MFX_CHANCE)->v[QD_MC_CHANCE] = (int16_t)c;
        for (i = 0; i < 1000; i++) {
            note(1, 1, 0, 60, 100, i * 10);
            note(1, 0, 0, 60, 0, i * 10 + 5);
        }
        for (i = 0; i < NEV; i++)
            got += EV[i].on;
        CHECK(c == 0 ? got == 0 : c == 100 ? got == 1000 : got > 420 && got < 580, "CHANCE %d %%: %d of 1000", c, got);
        CHECK(!sounding(), "CHANCE %d %%: nothing left sounding", c);
    }
    CHECK(G[0].slot[0].type == QD_MFX_CHANCE && qd_mfx_par(QD_MFX_CHANCE, 0)->def == 100, "a new CHANCE starts at 100 %%");
    /* HARM */
    fresh();
    slot(1, 0, QD_MFX_HARM);
    G[0].slot[0].v[QD_MH_N2] = 4, G[0].slot[0].v[QD_MH_N3] = 7, G[0].slot[0].v[QD_MH_N4] = 4;
    G[0].slot[0].v[QD_MH_N5] = 80;
    note(1, 1, 2, 60, 90, 0);
    n = ons(0, p, 16);
    CHECK(n == 4 && p[0] == 60 && p[1] == 64 && p[2] == 67 && p[3] == 127 && EV[1].vel == 90 && EV[1].track == 2,
          "HARM +4 +7 +4 +80: 60 64 67 127 (the duplicate skipped, clamped), the velocity and track kept (%s)", list(p, n));
    note(1, 0, 2, 60, 0, 10);
    CHECK(!sounding(), "HARM: one off ends all four");
    G[0].slot[0].v[QD_MH_ORIG] = 0;
    NEV = 0;
    note(1, 1, 2, 60, 90, 20);
    n = ons(0, p, 16);
    CHECK(n == 3 && p[0] == 64, "HARM ORIG OFF: the added notes only (%s)", list(p, n));
    /* SCALER */
    fresh();
    slot(1, 0, QD_MFX_SCALER);
    G[0].slot[0].v[QD_MZ_SCALE] = QD_SCALE_MAJOR, G[0].slot[0].v[QD_MZ_ROOT] = 0;
    for (i = 60; i < 72; i++)
        note(1, 1, 0, i, 100, 0);
    n = ons(0, p, 16);
    {
        static const int W[12] = {60, 60, 62, 62, 64, 65, 65, 67, 67, 69, 69, 71};
        CHECK(n == 12 && same(p, W, 12), "SCALER C MAJOR: each note snapped down (%s)", list(p, n));
    }
    G[0].slot[0].v[QD_MZ_KEY] = 1;
    KEY_SCALE = QD_SCALE_MINOR, KEY_ROOT = 2;
    NEV = 0;
    note(1, 1, 0, 66, 100, 0);
    note(1, 1, 0, 71, 100, 0);
    n = ons(0, p, 16);
    CHECK(n == 2 && p[0] == 65 && p[1] == 70, "SCALER KEY TRACK: the track's key (D minor): 66 -> 65, 71 -> 70 (%s)",
          list(p, n));
    KEY_SCALE = QD_SCALE_CHROMATIC;
    /* CHORD */
    fresh();
    slot(1, 0, QD_MFX_CHORD);
    note(1, 1, 0, 60, 77, 0);
    n = ons(0, p, 16);
    CHECK(n == 3 && p[0] == 60 && p[1] == 64 && p[2] == 67 && EV[2].vel == 77, "CHORD MAJ, 3 notes: 60 64 67, the played "
          "velocity (%s)", list(p, n));
    G[0].slot[0].v[QD_MK_SHAPE] = 4, G[0].slot[0].v[QD_MK_NOTES] = 4;   /* m7 */
    NEV = 0;
    note(1, 1, 0, 57, 100, 0);
    n = ons(0, p, 16);
    CHECK(n == 4 && p[0] == 57 && p[1] == 60 && p[2] == 64 && p[3] == 67, "CHORD m7 on A: A C E G (%s)", list(p, n));
    G[0].slot[0].v[QD_MK_INV] = 1;
    NEV = 0;
    note(1, 1, 0, 57, 100, 0);
    n = ons(0, p, 16);
    CHECK(n == 4 && p[0] == 60 && p[3] == 69, "CHORD INVERT 1: C E G A (%s)", list(p, n));
    G[0].slot[0].v[QD_MK_INV] = 0, G[0].slot[0].v[QD_MK_VOICING] = QD_CV_DROP2;
    NEV = 0;
    note(1, 1, 0, 60, 100, 0);
    n = ons(0, p, 16);
    CHECK(n == 4 && p[0] == 55 && p[1] == 60 && p[2] == 63 && p[3] == 70, "CHORD DROP 2: G3 C Eb Bb (%s)", list(p, n));
    G[0].slot[0].v[QD_MK_VOICING] = QD_CV_CLOSE, G[0].slot[0].v[QD_MK_SHAPE] = 1, G[0].slot[0].v[QD_MK_NOTES] = 5;
    NEV = 0;
    note(1, 1, 0, 60, 100, 0);
    n = ons(0, p, 16);
    CHECK(n == 5 && p[3] == 72 && p[4] == 76, "CHORD NOTES 5 of a triad: doubled an octave up (%s)", list(p, n));
    G[0].slot[0].v[QD_MK_NOTES] = 3, G[0].slot[0].v[QD_MK_SPREAD] = 1;
    NEV = 0;
    note(1, 1, 0, 60, 100, 0);
    n = ons(0, p, 16);
    CHECK(n == 3 && p[1] == 76, "CHORD SPREAD +1: the middle note up an octave (%s)", list(p, n));
    note(1, 0, 0, 60, 0, 1);
    note(1, 0, 0, 57, 0, 1);
    CHECK(!sounding(), "CHORD: the offs end every chord");
    /* MONO */
    fresh();
    slot(1, 0, QD_MFX_MONO);
    note(1, 1, 0, 60, 100, 0);
    note(1, 1, 1, 50, 100, 0);
    note(1, 1, 0, 64, 100, 10);
    CHECK(NEV == 4 && EV[2].on == 0 && EV[2].pitch == 60 && EV[3].pitch == 64 && SOUND[1][50],
          "MONO: 64 on track 1 ends its 60 first; track 2's 50 left alone");
    note(1, 0, 0, 60, 0, 20);
    CHECK(SOUND[0][64], "MONO: the earlier note's late off does not end the newer");
    note(1, 0, 0, 64, 0, 30);
    note(1, 0, 1, 50, 0, 30);
    CHECK(!sounding(), "MONO: nothing left");
}

static void t_random(void)
{
    int i, lo = 999, hi = -1, vlo = 999, vhi = -1, ok = 1, n;
    fresh();
    slot(1, 0, QD_MFX_RANDOM);
    G[0].slot[0].v[QD_MD_PITCH] = 3, G[0].slot[0].v[QD_MD_VEL] = 20;
    for (i = 0; i < 600; i++) {
        note(1, 1, 0, 60, 100, i * 10);
        note(1, 0, 0, 60, 0, i * 10 + 5);
    }
    for (i = 0; i < NEV; i++)
        if (EV[i].on) {
            lo = EV[i].pitch < lo ? EV[i].pitch : lo, hi = EV[i].pitch > hi ? EV[i].pitch : hi;
            vlo = EV[i].vel < vlo ? EV[i].vel : vlo, vhi = EV[i].vel > vhi ? EV[i].vel : vhi;
        }
    CHECK(lo == 57 && hi == 63 && vlo == 80 && vhi == 120, "RANDOM PITCH 3 VEL 20: 57..63 (%d..%d), 80..120 (%d..%d)", lo,
          hi, vlo, vhi);
    CHECK(!sounding(), "RANDOM: every off found its note");
    fresh();
    slot(1, 0, QD_MFX_RANDOM);
    G[0].slot[0].v[QD_MD_PITCH] = 0, G[0].slot[0].v[QD_MD_VEL] = 0, G[0].slot[0].v[QD_MD_OCT] = 1;
    for (i = 0; i < 300; i++)
        note(1, 1, 0, 60, 100, i);
    {
        int seen[3] = {0, 0, 0};
        for (i = 0; i < NEV; i++) {
            const int p = EV[i].pitch;
            ok &= p == 48 || p == 60 || p == 72;
            seen[p == 48 ? 0 : p == 60 ? 1 : 2]++;
        }
        CHECK(ok && seen[0] && seen[1] && seen[2], "RANDOM OCTAVE 1: 48, 60 or 72, each reached");
    }
    /* DELAY: up to 1/16 (DELAY 7: the 16th of a bar): every start within it; an off before the start cancels it */
    fresh();
    slot(1, 0, QD_MFX_RANDOM);
    G[0].slot[0].v[QD_MD_PITCH] = 0, G[0].slot[0].v[QD_MD_VEL] = 0, G[0].slot[0].v[QD_MD_DELAY] = 7;
    for (ok = 1, n = 0, i = 0; i < 200; i++) {
        const int64_t t0 = (int64_t)i * 4 * TPS;
        int j;
        note(1, 1, 0, 60, 100, t0);
        qd_mfx_advance(&M, t0 + TPS + 1);
        for (j = 0; j < NEV; j++)
            if (EV[j].on && EV[j].tick >= t0) {
                ok &= EV[j].tick <= t0 + TPS;
                n += EV[j].tick > t0;
            }
        note(1, 0, 0, 60, 0, t0 + 2 * TPS);
    }
    CHECK(ok && n > 150, "RANDOM DELAY 1/16: every start within a 16th, most later (%d)", n);
    fresh();
    slot(1, 0, QD_MFX_RANDOM);
    G[0].slot[0].v[QD_MD_PITCH] = 0, G[0].slot[0].v[QD_MD_VEL] = 0, G[0].slot[0].v[QD_MD_DELAY] = 16;   /* (a bar) */
    for (i = 0; i < 50; i++) {
        note(1, 1, 0, 60, 100, (int64_t)i * 2 * BAR);
        note(1, 0, 0, 60, 0, (int64_t)i * 2 * BAR + 1);
    }
    qd_mfx_advance(&M, 120 * BAR);
    for (n = 0, i = 0; i < NEV; i++)
        n += EV[i].on;
    CHECK(n <= 2 && !sounding(), "RANDOM DELAY: a key released before its delayed start: no note (%d of 50 started)", n);
    /* LENGTH: a sequencer note's end later by up to LENGTH % of its known length */
    fresh();
    slot(1, 0, QD_MFX_RANDOM);
    G[0].slot[0].v[QD_MD_PITCH] = 0, G[0].slot[0].v[QD_MD_VEL] = 0, G[0].slot[0].v[QD_MD_LEN] = 50;
    for (ok = 1, n = 0, i = 0; i < 100; i++) {
        qd_mfx_note_t x;
        const int64_t t0 = (int64_t)i * 4 * TPS;
        int j;
        memset(&x, 0, sizeof x);
        x.track = 0, x.pitch = x.prev = 60, x.vel = 100, x.glen = (int32_t)TPS;
        qd_mfx_advance(&M, t0);
        qd_mfx_note(&M, 1, 1, &x, t0);
        qd_mfx_advance(&M, t0 + TPS);
        qd_mfx_note(&M, 1, 0, &x, t0 + TPS);
        qd_mfx_advance(&M, t0 + 3 * TPS);
        for (j = 0; j < NEV; j++)
            if (!EV[j].on && EV[j].tick >= t0) {
                ok &= EV[j].tick >= t0 + TPS && EV[j].tick <= t0 + TPS + TPS / 2;
                n += EV[j].tick > t0 + TPS;
            }
    }
    CHECK(ok && n > 80 && !sounding(), "RANDOM LENGTH 50 %%: each end within +50 %% of its 16th, most later (%d)", n);
}

static void t_selector(void)
{
    int i, a = 0, b = 0, c = 0, p[64], n;
    fresh();
    slot(1, 0, QD_MFX_SELECTOR)->v[QD_ML_MODE] = QD_SM_UP;
    slot(1, 1, QD_MFX_TRANSPOSE)->v[QD_MT_OCT] = 1;
    slot(1, 2, QD_MFX_TRANSPOSE)->v[QD_MT_OCT] = 2;
    slot(1, 3, QD_MFX_TRANSPOSE)->v[QD_MT_OCT] = 0;
    G[0].slot[3].v[QD_MT_SEMI] = 1;                    /* (after the block: every note) */
    for (i = 0; i < 6; i++)
        note(1, 1, 0, 40 + i, 100, i);
    n = ons(0, p, 64);
#define TAP(pp, tt) (note(1, 1, 0, pp, 100, tt), note(1, 0, 0, pp, 0, tt))
    {
        static const int W[6] = {53, 66, 55, 68, 57, 70};
        CHECK(n == 6 && same(p, W, 6), "SELECTOR UP, LENGTH 2: +12 and +24 in turn (the other skipped), then slot 4's +1 "
              "(%s)", list(p, n));
    }
    for (i = 0; i < 6; i++)
        note(1, 0, 0, 40 + i, 0, 10);
    CHECK(!sounding(), "SELECTOR: the offs reach whichever had the note");
    G[0].slot[0].v[QD_ML_MODE] = QD_SM_DOWN;
    NEV = 0;
    for (i = 0; i < 4; i++)
        TAP(40, 20 + i);
    n = ons(0, p, 64);
    CHECK(n == 4 && p[0] == 65 && p[1] == 53 && p[2] == 65, "SELECTOR DOWN: the other way (%s)", list(p, n));
    G[0].slot[0].v[QD_ML_MODE] = QD_SM_RND;
    NEV = 0;
    for (i = 0; i < 400; i++)
        TAP(40, 100 + i);
    for (i = 0; i < NEV; i++)
        if (EV[i].on)
            a += EV[i].pitch == 53, b += EV[i].pitch == 65, c++;
    CHECK(c == 400 && a > 140 && b > 140 && a + b == 400, "SELECTOR RND: each of the two about half (%d, %d)", a, b);
    G[0].slot[0].v[QD_ML_CHANCE] = 0;
    NEV = 0;
    TAP(40, 1000);
    CHECK(NEV == 2 && EV[0].pitch == 41, "SELECTOR, a chance miss: the block skipped (40 -> 41)");
    G[0].slot[0].v[QD_ML_CHANCE] = 100, G[0].slot[0].v[QD_ML_LEN] = 0;
    NEV = 0;
    TAP(40, 1001);
    CHECK(NEV == 2 && EV[0].pitch == 77, "SELECTOR LENGTH 0: straight on through every slot (40 -> 77)");
    G[0].slot[0].v[QD_ML_LEN] = 2;
    G[0].slot[2].type = QD_MFX_NONE;
    G[0].slot[0].v[QD_ML_MODE] = QD_SM_UP;
    NEV = 0;
    TAP(40, 1002);
    TAP(40, 1003);
    n = ons(0, p, 64);
    CHECK(n == 2 && ((p[0] == 41 && p[1] == 53) || (p[0] == 53 && p[1] == 41)), "SELECTOR: an empty slot chosen: the block "
          "skipped (%s)", list(p, n));
}

static void t_repeat(void)
{
    int i, n, ok;
    fresh();
    slot(1, 0, QD_MFX_REPEAT)->v[QD_MR_RATE] = 9;   /* 1/16 */
    note(1, 1, 0, 60, 100, 0);
    qd_mfx_advance(&M, 4 * TPS - 1);
    note(1, 0, 0, 60, 0, 4 * TPS - 1);
    qd_mfx_advance(&M, 8 * TPS);
    for (n = 0, ok = 1, i = 0; i < NEV; i++)
        if (EV[i].on) {
            ok &= EV[i].tick == n * TPS && EV[i].len == (int32_t)(TPS * 90 / 100) && EV[i].vel == 100;
            n++;
        }
    CHECK(n == 4 && ok && !sounding(), "REPEAT ON 1/16: a hit each 16th while held (%d), 90 %% long; none after", n);
    /* 1-ST: HITS even after a quick release */
    fresh();
    slot(1, 0, QD_MFX_REPEAT)->v[QD_MR_RATE] = 9;
    G[0].slot[0].v[QD_MR_MODE] = QD_AM_1ST, G[0].slot[0].v[QD_MR_HITS] = 6;
    note(1, 1, 0, 60, 100, 0);
    note(1, 0, 0, 60, 0, 10);
    qd_mfx_advance(&M, 20 * TPS);
    for (n = 0, i = 0; i < NEV; i++)
        n += EV[i].on;
    CHECK(n == 6 && !sounding(), "REPEAT 1-ST, HITS 6: six hits after a 10-tick press (%d)", n);
    /* ONCE: at most HITS, ended early by the release */
    G[0].slot[0].v[QD_MR_MODE] = QD_AM_ONCE;
    NEV = 0;
    note(1, 1, 0, 60, 100, 40 * TPS);
    qd_mfx_advance(&M, 42 * TPS - 1);
    note(1, 0, 0, 60, 0, 42 * TPS - 1);
    qd_mfx_advance(&M, 60 * TPS);
    for (n = 0, i = 0; i < NEV; i++)
        n += EV[i].on;
    CHECK(n == 2, "REPEAT ONCE: released after two hits: two (%d)", n);
    NEV = 0;
    note(1, 1, 0, 60, 100, 80 * TPS);
    qd_mfx_advance(&M, 100 * TPS);
    for (n = 0, i = 0; i < NEV; i++)
        n += EV[i].on;
    CHECK(n == 6, "REPEAT ONCE: held: HITS (%d)", n);
    note(1, 0, 0, 60, 0, 100 * TPS);
    /* HOLD: on after the release; the next note after every key is up replaces the set */
    fresh();
    slot(1, 0, QD_MFX_REPEAT)->v[QD_MR_RATE] = 9;
    G[0].slot[0].v[QD_MR_MODE] = QD_AM_HOLD;
    note(1, 1, 0, 60, 100, 0);
    note(1, 0, 0, 60, 0, 10);
    qd_mfx_advance(&M, 8 * TPS);
    for (n = 0, i = 0; i < NEV; i++)
        n += EV[i].on && EV[i].pitch == 60;
    CHECK(n == 8, "REPEAT HOLD: latched after the release (%d hits in 8 16ths)", n);
    note(1, 1, 0, 62, 100, 8 * TPS);
    NEV = 0;
    qd_mfx_advance(&M, 16 * TPS);
    for (ok = 1, n = 0, i = 0; i < NEV; i++)
        if (EV[i].on)
            ok &= EV[i].pitch == 62, n++;
    CHECK(ok && n == 8, "REPEAT HOLD: a new note after all keys up replaces the set (%d hits of 62)", n);
    qd_mfx_reset(&M);
    CHECK(!sounding(), "REPEAT HOLD: a reset ends it");
    /* FADE -60 %: 100 % down to 40 % over HITS; QUANT: the first hit on the grid */
    fresh();
    slot(1, 0, QD_MFX_REPEAT)->v[QD_MR_RATE] = 9;
    G[0].slot[0].v[QD_MR_MODE] = QD_AM_1ST, G[0].slot[0].v[QD_MR_HITS] = 4, G[0].slot[0].v[QD_MR_FADE] = -60;
    G[0].slot[0].v[QD_MR_GATE] = 50;
    note(1, 1, 0, 60, 100, 1000);
    note(1, 0, 0, 60, 0, 1001);
    qd_mfx_advance(&M, 10 * TPS);
    {
        int v[4] = {0, 0, 0, 0}, k = 0;
        int64_t t0 = -1;
        int32_t l0 = 0;
        for (i = 0; i < NEV && k < 4; i++)
            if (EV[i].on) {
                if (!k)
                    t0 = EV[i].tick, l0 = EV[i].len;
                v[k++] = EV[i].vel;
            }
        CHECK(k == 4 && t0 == TPS && v[0] == 100 && v[1] == 80 && v[2] == 60 && v[3] == 40 && l0 == (int32_t)(TPS / 2),
              "REPEAT FADE -60 %%: 100 80 60 40; QUANT 1/16: the first on the grid; GATE 50 %% (%d %d %d %d)", v[0], v[1],
              v[2], v[3]);
    }
}

/* the ARP: chord held (C E G B at ticks 0), n steps of 1/16 played, the pitches */
static int arp_run(int pat, int range, int odist, int nsteps, int *p)
{
    static const int CH[4] = {60, 64, 67, 71};
    qd_mfx_slot_t *s;
    int i;
    fresh();
    s = slot(1, 0, QD_MFX_ARP);
    s->v[QD_MA_PAT] = (int16_t)pat, s->v[QD_MA_RATE] = 9, s->v[QD_MA_RANGE] = (int16_t)range;
    s->v[QD_MA_ODIST] = (int16_t)odist, s->v[QD_MA_QUANT] = 0;
    for (i = 0; i < 4; i++)
        note(1, 1, 0, CH[i], 100 - i, 0);
    qd_mfx_advance(&M, nsteps * TPS);
    return ons(0, p, nsteps);
}

static void t_arp(void)
{
    static const struct {
        int pat;
        const char *name;
        int n;
        int w[12];
    } P[] = {
        {QD_AP_UP, "UP", 4, {60, 64, 67, 71}},
        {QD_AP_DN, "DOWN", 4, {71, 67, 64, 60}},
        {QD_AP_UPDN, "UP DN", 6, {60, 64, 67, 71, 67, 64}},
        {QD_AP_DNUP, "DN UP", 6, {71, 67, 64, 60, 64, 67}},
        {QD_AP_UANDD, "U+D", 8, {60, 64, 67, 71, 71, 67, 64, 60}},
        {QD_AP_DANDU, "D+U", 8, {71, 67, 64, 60, 60, 64, 67, 71}},
        {QD_AP_CON, "CONV", 4, {60, 71, 64, 67}},
        {QD_AP_DIV, "DIV", 4, {67, 64, 71, 60}},
        {QD_AP_CONDIV, "CON DIV", 6, {60, 71, 64, 67, 64, 71}},
        {QD_AP_HI1, "HI UP", 6, {71, 67, 71, 64, 71, 60}},
        {QD_AP_HI2, "HI UPDN", 8, {71, 67, 71, 64, 71, 60, 71, 64}},
        {QD_AP_LO1, "LO UP", 6, {60, 64, 60, 67, 60, 71}},
        {QD_AP_LO2, "LO UPDN", 8, {60, 64, 60, 67, 60, 71, 60, 67}},
        {QD_AP_ASP, "AS PLAYED", 4, {60, 64, 67, 71}},
    };
    int p[256], n, i, k, ok;
    for (k = 0; k < (int)(sizeof P / sizeof P[0]); k++) {
        n = arp_run(P[k].pat, 1, 12, P[k].n * 2, p);
        CHECK(n == P[k].n * 2 && same(p, P[k].w, P[k].n) && same(p + P[k].n, P[k].w, P[k].n),
              "ARP %s: %s", P[k].name, list(p, n));
    }
    /* AS PLAYED: the order the keys came in */
    fresh();
    slot(1, 0, QD_MFX_ARP)->v[QD_MA_PAT] = QD_AP_ASP;
    G[0].slot[0].v[QD_MA_RATE] = 9, G[0].slot[0].v[QD_MA_RANGE] = 1, G[0].slot[0].v[QD_MA_QUANT] = 0;
    note(1, 1, 0, 67, 100, 0);
    note(1, 1, 0, 60, 100, 0);
    note(1, 1, 0, 64, 100, 0);
    qd_mfx_advance(&M, 3 * TPS);
    n = ons(0, p, 8);
    CHECK(n == 3 && p[0] == 67 && p[1] == 60 && p[2] == 64, "ARP AS PLAYED: 67 60 64 (%s)", list(p, n));
    /* RAND, RAND OTHER, SHUFFLE */
    n = arp_run(QD_AP_RAND, 1, 12, 200, p);
    for (ok = n == 200, i = 0; i < n; i++)
        ok &= p[i] == 60 || p[i] == 64 || p[i] == 67 || p[i] == 71;
    CHECK(ok, "ARP RAND: the held notes, at random");
    n = arp_run(QD_AP_ROTH, 1, 12, 200, p);
    for (ok = n == 200, i = 1; i < n; i++)
        ok &= p[i] != p[i - 1];
    CHECK(ok, "ARP RAND OTHER: never the same twice in a row");
    n = arp_run(QD_AP_RONC, 1, 12, 40, p);
    for (ok = n == 40, i = 0; i + 4 <= n; i += 4) {
        const int s = p[i] + p[i + 1] + p[i + 2] + p[i + 3];
        ok &= s == 60 + 64 + 67 + 71 && p[i] != p[i + 1] && p[i] != p[i + 2] && p[i] != p[i + 3] && p[i + 1] != p[i + 2] &&
              p[i + 1] != p[i + 3] && p[i + 2] != p[i + 3];
    }
    CHECK(ok, "ARP SHUFFLE: each pass every note once (%s)", list(p, 12));
    /* RANGE and OCT DIST */
    n = arp_run(QD_AP_UP, 2, 12, 9, p);
    {
        static const int W[9] = {60, 64, 67, 71, 72, 76, 79, 83, 60};
        CHECK(n == 9 && same(p, W, 9), "ARP RANGE 2, OCT DIST +12: two octaves, then back (%s)", list(p, n));
    }
    n = arp_run(QD_AP_UP, 3, -12, 12, p);
    CHECK(n == 12 && p[4] == 48 && p[8] == 36, "ARP RANGE 3, OCT DIST -12: down (%s)", list(p, n));
    n = arp_run(QD_AP_UP, 2, 7, 8, p);
    CHECK(n == 8 && p[4] == 67, "ARP OCT DIST +7: a fifth up (%s)", list(p, n));
    /* the timing, GATE, velocity: the first held note's */
    arp_run(QD_AP_UP, 1, 12, 4, p);
    for (ok = 1, k = 0, i = 0; i < NEV; i++)
        if (EV[i].on) {
            ok &= EV[i].tick == k * TPS && EV[i].len == (int32_t)(TPS * 90 / 100) && EV[i].vel == 100;
            k++;
        }
    CHECK(ok && k == 4, "ARP 1/16: a step each 16th, 90 %% long, every note at the first key's velocity");
    /* QUANT 1/16 (the default): a note at tick 1000 waits for the grid */
    fresh();
    slot(1, 0, QD_MFX_ARP)->v[QD_MA_RATE] = 9;
    note(1, 1, 0, 60, 100, 1000);
    qd_mfx_advance(&M, 3 * TPS);
    CHECK(NEV > 0 && EV[0].tick == TPS, "ARP QUANT 1/16: the first step on the grid (%lld)", (long long)(NEV ? EV[0].tick : -1));
    CHECK(G[0].slot[0].v[QD_MA_MODE] == QD_AM_ON, "a new ARP starts ON");
    /* the modes */
    fresh();
    slot(1, 0, QD_MFX_ARP)->v[QD_MA_RATE] = 9;
    G[0].slot[0].v[QD_MA_QUANT] = 0, G[0].slot[0].v[QD_MA_RANGE] = 1;
    note(1, 1, 0, 60, 100, 0);
    note(1, 1, 0, 64, 100, 0);
    qd_mfx_advance(&M, 3 * TPS);
    note(1, 0, 0, 60, 0, 3 * TPS);
    note(1, 0, 0, 64, 0, 3 * TPS);
    qd_mfx_advance(&M, 10 * TPS);
    for (n = 0, i = 0; i < NEV; i++)
        n += EV[i].on;
    CHECK(n == 3 && !sounding(), "ARP ON: three steps while held, none after (%d)", n);
    G[0].slot[0].v[QD_MA_MODE] = QD_AM_ONCE;
    NEV = 0;
    note(1, 1, 0, 60, 100, 20 * TPS);
    note(1, 1, 0, 64, 100, 20 * TPS);
    qd_mfx_advance(&M, 30 * TPS);
    for (n = 0, i = 0; i < NEV; i++)
        n += EV[i].on;
    CHECK(n == 2, "ARP ONCE: held, one pass (%d)", n);
    note(1, 0, 0, 60, 0, 30 * TPS);
    note(1, 0, 0, 64, 0, 30 * TPS);
    G[0].slot[0].v[QD_MA_MODE] = QD_AM_1ST, G[0].slot[0].v[QD_MA_RANGE] = 2;
    NEV = 0;
    note(1, 1, 0, 60, 100, 40 * TPS);
    note(1, 1, 0, 64, 100, 40 * TPS);
    note(1, 0, 0, 60, 0, 40 * TPS + 5);
    note(1, 0, 0, 64, 0, 40 * TPS + 5);
    qd_mfx_advance(&M, 60 * TPS);
    for (n = 0, i = 0; i < NEV; i++)
        n += EV[i].on;
    CHECK(n == 4 && !sounding(), "ARP 1-ST: released at once, still one pass over RANGE 2 (%d)", n);
    G[0].slot[0].v[QD_MA_MODE] = QD_AM_HOLD, G[0].slot[0].v[QD_MA_RANGE] = 1;
    NEV = 0;
    note(1, 1, 0, 60, 100, 64 * TPS);
    note(1, 1, 0, 64, 100, 64 * TPS);
    note(1, 0, 0, 60, 0, 64 * TPS + 5);
    note(1, 0, 0, 64, 0, 64 * TPS + 5);
    qd_mfx_advance(&M, 72 * TPS);
    for (n = 0, i = 0; i < NEV; i++)
        n += EV[i].on;
    CHECK(n == 8, "ARP HOLD: latched after the release (%d)", n);
    note(1, 1, 0, 50, 100, 72 * TPS);
    NEV = 0;
    qd_mfx_advance(&M, 76 * TPS);
    n = ons(0, p, 16);
    for (ok = n == 4, i = 0; i < n; i++)
        ok &= p[i] == 50;
    CHECK(ok, "ARP HOLD: a key after all were up starts a new set (%s)", list(p, n));
    note(1, 1, 0, 53, 100, 76 * TPS);
    NEV = 0;
    qd_mfx_advance(&M, 78 * TPS);
    n = ons(0, p, 16);
    CHECK(n == 2 && p[0] != p[1], "ARP HOLD: one more key while one is down joins it (%s)", list(p, n));
    qd_mfx_reset(&M);
    CHECK(!sounding() && !qd_mfx_arp_playing(&M, 1), "ARP: a reset stops it");
    /* the MOD lane: as played, rest, tie, repeat, low -oct, high +oct, power chord, chord, note 1, note 2 */
    fresh();
    {
        qd_mfx_slot_t *s = slot(1, 0, QD_MFX_ARP);
        static const int8_t L[10] = {QD_AL_PLAY, QD_AL_REST, QD_AL_PLAY, QD_AL_TIE, QD_AL_REPEAT, QD_AL_LO, QD_AL_HI,
                                     QD_AL_PWR, QD_AL_CHORD, QD_AL_N1 + 1};
        int e;
        s->v[QD_MA_RATE] = 9, s->v[QD_MA_QUANT] = 0, s->v[QD_MA_RANGE] = 1, s->v[QD_MA_MLEN] = 10, s->v[QD_MA_GATE] = 50;
        memcpy(s->lane[0], L, sizeof L);
        note(1, 1, 0, 64, 100, 0);
        note(1, 1, 0, 60, 100, 0);
        note(1, 1, 0, 67, 100, 0);
        qd_mfx_advance(&M, 10 * TPS);
        for (e = 0; e < NEV && EV[e].tick < 3 * TPS; e++)
            ;
        n = ons(0, p, 32);
        {   /* step 1 C, 2 rest, 3 G (tied: 1.5 16ths long at 50 %), 4 tie, 5 repeat G, 6 C-12, 7 G+12, 8 C+G, 9 C E G,
             * 10 note 2 as played (60) */
            static const int W[12] = {60, 67, 67, 48, 79, 60, 67, 60, 64, 67, 60};
            int lt = 0;
            for (i = 0; i < NEV; i++)
                if (EV[i].on && EV[i].tick == 2 * TPS)
                    lt = EV[i].len;
            CHECK(n == 11 && same(p, W, 11) && lt == (int32_t)(TPS * 2 / 2),
                  "ARP's MOD lane: x . x - R < > \" # 2 (%s; the tied note %d long)", list(p, n), lt);
        }
    }
    /* the TRANSPOSE lane, its own length (a polymeter), RESET MPAT */
    fresh();
    {
        qd_mfx_slot_t *s = slot(1, 0, QD_MFX_ARP);
        s->v[QD_MA_RATE] = 9, s->v[QD_MA_QUANT] = 0, s->v[QD_MA_RANGE] = 1, s->v[QD_MA_TLEN] = 3;
        s->lane[1][1] = 7, s->lane[1][2] = -12;
        note(1, 1, 0, 60, 100, 0);
        note(1, 1, 0, 64, 100, 0);
        qd_mfx_advance(&M, 6 * TPS);
        n = ons(0, p, 16);
        {
            static const int W[6] = {60, 71, 48, 64, 67, 52};
            CHECK(n == 6 && same(p, W, 6), "ARP's TRANSPOSE lane, length 3: 0 +7 -12 against two notes (%s)", list(p, n));
        }
        NEV = 0;
        qd_mfx_reset(&M);
        s->v[QD_MA_RESET] = QD_AR_MPAT, s->v[QD_MA_MLEN] = 3;
        s->v[QD_MA_TLEN] = 16, s->lane[1][1] = 0, s->lane[1][2] = 0;
        note(1, 1, 0, 60, 100, 100 * TPS);
        note(1, 1, 0, 64, 100, 100 * TPS);
        note(1, 1, 0, 67, 100, 100 * TPS);
        note(1, 1, 0, 71, 100, 100 * TPS);
        qd_mfx_advance(&M, 106 * TPS);
        n = ons(0, p, 16);
        CHECK(n == 6 && p[3] == 60 && p[4] == 64, "ARP RESET MPAT: the pattern restarts as the MOD lane wraps (%s)",
              list(p, n));
    }
    /* RESET NOTE */
    fresh();
    slot(1, 0, QD_MFX_ARP)->v[QD_MA_RATE] = 9;
    G[0].slot[0].v[QD_MA_QUANT] = 0, G[0].slot[0].v[QD_MA_RANGE] = 1, G[0].slot[0].v[QD_MA_RESET] = QD_AR_NOTE;
    note(1, 1, 0, 60, 100, 0);
    note(1, 1, 0, 64, 100, 0);
    note(1, 1, 0, 67, 100, 0);
    qd_mfx_advance(&M, 2 * TPS);
    note(1, 1, 0, 50, 100, 2 * TPS);
    qd_mfx_advance(&M, 3 * TPS);
    n = ons(0, p, 16);
    CHECK(n == 3 && p[2] == 50, "ARP RESET NOTE: a new note starts the pattern again (%s)", list(p, n));
    /* two tracks in one arp: each note on its own track */
    fresh();
    slot(1, 0, QD_MFX_ARP)->v[QD_MA_RATE] = 9;
    G[0].slot[0].v[QD_MA_QUANT] = 0, G[0].slot[0].v[QD_MA_RANGE] = 1;
    note(1, 1, 0, 60, 100, 0);
    note(1, 1, 3, 64, 90, 0);
    qd_mfx_advance(&M, 4 * TPS);
    CHECK(NEV >= 4 && EV[0].track == 0 && EV[0].pitch == 60 && EV[2].track == 3 && EV[2].pitch == 64 && EV[2].vel == 100,
          "two tracks in one arp: one pattern over both, each note on its own track");
    /* the toggle: none -> added ON in the first empty slot; again OFF; again its mode back */
    fresh();
    slot(1, 0, QD_MFX_CHANCE);
    {
        int on = -1, k2 = qd_mfx_arp_toggle(&M, 1, &on);
        CHECK(k2 == 1 && on == 1 && G[0].slot[1].type == QD_MFX_ARP && G[0].slot[1].v[QD_MA_MODE] == QD_AM_ON,
              "ARP tapped with none: one added in slot 2, ON");
        G[0].slot[1].v[QD_MA_MODE] = QD_AM_HOLD;
        k2 = qd_mfx_arp_toggle(&M, 1, &on);
        CHECK(k2 == 1 && on == 0 && G[0].slot[1].v[QD_MA_MODE] == QD_AM_OFF, "ARP tapped: OFF");
        k2 = qd_mfx_arp_toggle(&M, 1, &on);
        CHECK(on == 1 && G[0].slot[1].v[QD_MA_MODE] == QD_AM_HOLD, "ARP tapped again: its mode back (HOLD)");
    }
}

/* the bookkeeping: random chains, random notes on and off; no latch: nothing sounds once every key is up and the
 * timed notes ended; with latches: nothing after a reset */
static void t_fuzz(void)
{
    int run, bad = 0, bad2 = 0;
    srand(12345);
    for (run = 0; run < 300; run++) {
        int k, i, held[8][128];
        const int latch = run % 3 == 0;
        int64_t t = 0;
        fresh();
        qd_mfx_seed(&M, (uint32_t)run + 1u);
        memset(held, 0, sizeof held);
        for (k = 0; k < QD_MFX_NSLOT; k++) {
            const int ty = rand() % QD_MFX_NTYPE;
            qd_mfx_slot_t *s = slot(1 + run % 2, k, ty);
            if (ty == QD_MFX_NONE)
                continue;
            for (i = 0; i < QD_MFX_NV; i++) {
                const qd_mfx_par_t *pp = qd_mfx_par(ty, i);
                if (pp && rand() % 2)
                    s->v[i] = (int16_t)(pp->lo + rand() % (pp->hi - pp->lo + 1));
            }
            if (ty == QD_MFX_ARP || ty == QD_MFX_REPEAT) {
                const int mode = s->v[ty == QD_MFX_ARP ? QD_MA_MODE : QD_MR_MODE];
                if (!latch && mode == QD_AM_HOLD)
                    s->v[ty == QD_MFX_ARP ? QD_MA_MODE : QD_MR_MODE] = QD_AM_ON;
                if (ty == QD_MFX_ARP)
                    for (i = 0; i < 16; i++)
                        s->lane[0][i] = (int8_t)(rand() % QD_NAL), s->lane[1][i] = (int8_t)(rand() % 25 - 12);
            }
            s->bypass = rand() % 8 == 0;
        }
        for (i = 0; i < 400; i++) {
            const int tr = rand() % 3, p = 36 + rand() % 48;
            t += rand() % (TPS / 2);
            qd_mfx_advance(&M, t);
            if (held[tr][p]) {
                note(1 + run % 2, 0, tr, p, 0, t);
                held[tr][p] = 0;
            } else {
                note(1 + run % 2, 1, tr, p, 1 + rand() % 127, t);
                held[tr][p] = 1;
            }
            if (rand() % 50 == 0) {                     /* (a slot changed or bypassed while notes are in flight) */
                const int kk = rand() % QD_MFX_NSLOT;
                if (rand() % 2)
                    G[run % 2].slot[kk].bypass ^= 1;
                else
                    qd_mfx_slot_init(&G[run % 2].slot[kk], rand() % 2 ? QD_MFX_NONE : QD_MFX_TRANSPOSE);
            }
        }
        for (i = 0; i < 3 * 128; i++)
            if (held[i / 128][i % 128])
                note(1 + run % 2, 0, i / 128, i % 128, 0, t);
        for (i = 1; i <= 1100; i++)                     /* (1-ST over RANGE 8 at 1/1: some hundreds of bars) */
            qd_mfx_advance(&M, t + i * BAR);
        if (!latch && sounding()) {
            int kk;
            bad++;
            if (bad <= 3) {
                printf("  (run %d left notes: chain", run);
                for (kk = 0; kk < QD_MFX_NSLOT; kk++)
                    printf(" %s%s", qd_mfx_type_name(G[run % 2].slot[kk].type), G[run % 2].slot[kk].bypass ? "(b)" : "");
                printf(")\n");
                for (kk = 0; kk < QD_MFX_NSLOT; kk++) {
                    static const qd_mfx_srt_t none;
                    const qd_mfx_srt_t *r = qd_mfx_srt(&M, run % 2, kk) ? qd_mfx_srt(&M, run % 2, kk) : &none;
                    const qd_mfx_slot_t *s = &G[run % 2].slot[kk];
                    int h;
                    printf("   slot %d %s mode %d/%d run %d down %d:", kk, qd_mfx_type_name(s->type), s->v[4], s->v[3], r->run, r->down);
                    for (h = 0; h < QD_MFX_NHOLD; h++)
                        if (r->h[h].used)
                            printf(" %d/%d%s u%lld n%lld", r->h[h].n.pitch, r->h[h].n.prev, r->h[h].released ? "r" : "", (long long)r->h[h].until, (long long)r->h[h].next);
                    printf("\n");
                }
                for (kk = 0; kk < QD_MFX_NOUT; kk++)
                    if (M.g[run % 2].out[kk].used)
                        printf("   out t%d %d/%d\n", M.g[run % 2].out[kk].track, M.g[run % 2].out[kk].pitch, M.g[run % 2].out[kk].prev);
                for (kk = 0; kk < 3 * 128; kk++)
                    if (SOUND[kk / 128][kk % 128])
                        printf("   sounding t%d %d\n", kk / 128, kk % 128);
            }
        }
        qd_mfx_reset(&M);
        if (sounding())
            bad2++;
    }
    CHECK(!bad, "300 random chains (every effect, bypasses and changes on the way), random notes: once every key is up "
          "nothing sounds (%d runs left notes)", bad);
    CHECK(!bad2, "after a reset nothing sounds, latches too (%d)", bad2);
}

/* ================================================================ through the sequencer and the player */
static qd_project_t PROJ;
static qd_model_t MODEL;
static qd_pool_t POOL;
static qd_mix_t MIX;
static qd_seq_t SEQ;
static qd_player_t PL;
typedef struct {
    int kind, track, note, step;
    int64_t sample;
} pe_t;
static pe_t PE[8000];
static int NPE;
static void log_cb(void *ctx, const qd_plog_t *l)
{
    (void)ctx;
    if (NPE < (int)(sizeof PE / sizeof PE[0]) && (l->kind == QD_LOG_ON || l->kind == QD_LOG_OFF))
        PE[NPE].kind = l->kind, PE[NPE].track = l->track, PE[NPE].note = l->note, PE[NPE].step = l->step,
        PE[NPE].sample = (int64_t)l->sample, NPE++;
}
static void rig(void)
{
    int t;
    qd_model_init(&MODEL);
    qd_project_init(&PROJ);
    qd_pool_init(&POOL, 16);
    for (t = 0; t < QD_NTRACK; t++)
        qd_pool_set_track(&POOL, t, MODEL.t[t].limit, MODEL.t[t].reserve);
    qd_mix_init(&MIX);
    qd_seq_init(&SEQ, &PROJ, &MODEL);
    qd_player_init(&PL, &SEQ, &POOL, &MODEL);
    PL.log = log_cb;
    NPE = 0;
}
static void blocks(int n)
{
    while (n-- > 0) {
        qd_quad_owner_t *ow[QD_NTRACK];
        qd_mix_track_t mt[QD_NTRACK];
        int32_t l[32], r[32], sc[32], sd[32], sr[32];
        int i;
        for (i = 0; i < QD_NTRACK; i++)
            ow[i] = &MODEL.t[i].owner, mt[i] = MODEL.t[i].mix;
        qd_player_block(&PL);
        qd_mix_block(&MIX, &POOL, ow, mt, QD_NTRACK, l, r, sc, sd, sr);
    }
}
static int gated(void)
{
    int t, n = 0;
    for (t = 0; t < QD_NTRACK; t++)
        n += qd_pool_gated(&POOL, t);
    return n;
}
#define BLK_16TH (44100 * 15 / 120 / 32)        /* blocks a 16th at 120 BPM (about) */
static void four_steps(int t, int note)
{
    int s;
    for (s = 0; s < 16; s += 4) {
        uint8_t n1 = (uint8_t)note;
        qd_step_on(qd_proj_pat(&PROJ, 0), t, s);
        qd_step_set_notes(qd_proj_pat(&PROJ, 0), t, s, &n1, 1);
    }
}

/* P2.1b: the state pool (QD_MFX_NSRT for the five groups): every slot of every group an effect with a state; the
 * places go to the first QD_MFX_NSRT, the others let their notes by (counted), nothing hangs, and a place freed is
 * taken by a slot still without one */
static int places(void)
{
    int g, k, n = 0;
    for (g = 0; g < QD_MFX_NGROUP; g++)
        for (k = 0; k < QD_MFX_NSLOT; k++)
            n += qd_mfx_srt(&M, g, k) != 0;
    return n;
}
static void t_pool(void)
{
    const int all = QD_MFX_NGROUP * QD_MFX_NSLOT;
    int g, k, p, i;
    fresh();
    for (g = 1; g <= QD_MFX_NGROUP; g++)
        for (k = 0; k < QD_MFX_NSLOT; k++)
            slot(g, k, g == QD_MFX_NGROUP ? QD_MFX_ARP : k % 2 ? QD_MFX_REPEAT : QD_MFX_RANDOM);
    for (g = 1; g <= QD_MFX_NGROUP; g++)
        for (p = 0; p < 4; p++)
            note(g, 1, g - 1, 48 + 7 * g + p, 100, (int64_t)p * TPS);
    CHECK(places() == (all < QD_MFX_NSRT ? all : QD_MFX_NSRT), "pool: %d places of %d", places(), QD_MFX_NSRT);
    CHECK(QD_MFX_NSRT >= all ? M.starved == 0 : M.starved > 0, "pool: %u notes let by (%d places)", (unsigned)M.starved,
          QD_MFX_NSRT);
    CHECK(NEV > 0, "pool: notes out");
    for (g = 1; g <= QD_MFX_NGROUP; g++)
        for (p = 0; p < 4; p++)
            note(g, 0, g - 1, 48 + 7 * g + p, 0, 4 * TPS + (int64_t)p);
    for (i = 1; i <= 400; i++)
        qd_mfx_advance(&M, 4 * TPS + i * BAR);
    CHECK(!sounding(), "pool: %d notes left sounding", sounding());
    for (k = 0; k < QD_MFX_NSLOT; k++)                  /* group 1 emptied: its places to the others */
        qd_mfx_slot_init(&G[0].slot[k], QD_MFX_NONE);
    note(2, 1, 1, 60, 100, 500 * BAR);
    note(2, 0, 1, 60, 0, 501 * BAR);
    for (i = 1; i <= 400; i++)
        qd_mfx_advance(&M, 501 * BAR + i * BAR);
    CHECK(places() == (all - QD_MFX_NSLOT < QD_MFX_NSRT ? all - QD_MFX_NSLOT : QD_MFX_NSRT), "pool: %d places after",
          places());
    for (k = 0; k < QD_MFX_NSLOT; k++)
        CHECK(!qd_mfx_srt(&M, 0, k), "pool: group 1 slot %d still has a place", k + 1);
    CHECK(!sounding(), "pool: %d notes left sounding after", sounding());
    printf("  the state pool: %d places (%d bytes each); every slot with a state: %u notes let by\n", QD_MFX_NSRT,
           (int)sizeof(qd_mfx_srt_t), (unsigned)M.starved);
}

static void t_player(void)
{
    int i, ok, n;
    pe_t a[200];
    int na;
    /* an empty group routed: the same timeline as none */
    rig();
    four_steps(0, 60);
    qd_seq_select(&SEQ, 0, 0);
    qd_seq_play(&SEQ);
    blocks(16 * BLK_16TH * 2);
    na = NPE < 200 ? NPE : 200;
    memcpy(a, PE, (size_t)na * sizeof a[0]);
    rig();
    four_steps(0, 60);
    qd_proj_pat(&PROJ, 0)->trk[0].mfx = 1;
    qd_seq_select(&SEQ, 0, 0);
    qd_seq_play(&SEQ);
    blocks(16 * BLK_16TH * 2);
    for (ok = NPE == na, i = 0; ok && i < na; i++)
        ok &= PE[i].kind == a[i].kind && PE[i].note == a[i].note && PE[i].sample == a[i].sample;
    CHECK(ok && na > 4, "a track routed to an empty group: the same timeline as none (%d events)", na);
    /* TRANSPOSE +12 in group 1 */
    rig();
    four_steps(0, 60);
    qd_proj_pat(&PROJ, 0)->trk[0].mfx = 1;
    qd_mfx_slot_init(&PROJ.mfx[0].slot[0], QD_MFX_TRANSPOSE);
    PROJ.mfx[0].slot[0].v[QD_MT_SEMI] = 12, PROJ.mfx[0].slot[0].v[QD_MT_OCT] = 0;
    qd_proj_pat(&PROJ, 0)->step[0][4].mfx = QD_SMFX_OFF;           /* step 5 straight on */
    qd_proj_pat(&PROJ, 0)->step[0][8].mfx = QD_SMFX_G1 + 1;        /* step 9: group 2 (empty: straight on) */
    qd_mfx_slot_init(&PROJ.mfx[1].slot[0], QD_MFX_TRANSPOSE);
    PROJ.mfx[1].slot[0].v[QD_MT_SEMI] = 24, PROJ.mfx[1].slot[0].v[QD_MT_OCT] = 0;
    qd_seq_select(&SEQ, 0, 0);
    qd_seq_play(&SEQ);
    blocks(16 * BLK_16TH + 4);
    {
        int got[4] = {0, 0, 0, 0}, k = 0;
        for (i = 0; i < NPE && k < 4; i++)
            if (PE[i].kind == QD_LOG_ON)
                got[k++] = PE[i].note;
        CHECK(k == 4 && got[0] == 72 && got[1] == 60 && got[2] == 84 && got[3] == 72,
              "TRANSPOSE +12 on the track; step 5 locked OFF, step 9 to group 2 (+24): 72 60 84 72 (%d %d %d %d)", got[0],
              got[1], got[2], got[3]);
    }
    qd_player_stop(&PL);
    blocks(4);
    CHECK(!gated(), "STOP: nothing held");
    /* two tracks in one arp, each note on its own track */
    rig();
    {
        uint8_t ch[2] = {60, 64}, n1 = 48;
        qd_step_on(qd_proj_pat(&PROJ, 0), 0, 0);
        qd_step_set_notes(qd_proj_pat(&PROJ, 0), 0, 0, ch, 2);
        qd_step_set_len(qd_proj_pat(&PROJ, 0), 0, 0, QD_LEN_1 + 6);   /* (8 steps) */
        qd_step_on(qd_proj_pat(&PROJ, 0), 3, 0);
        qd_step_set_notes(qd_proj_pat(&PROJ, 0), 3, 0, &n1, 1);
        qd_step_set_len(qd_proj_pat(&PROJ, 0), 3, 0, QD_LEN_1 + 6);
    }
    qd_proj_pat(&PROJ, 0)->trk[0].mfx = 1, qd_proj_pat(&PROJ, 0)->trk[3].mfx = 1;
    qd_mfx_slot_init(&PROJ.mfx[0].slot[0], QD_MFX_ARP);
    PROJ.mfx[0].slot[0].v[QD_MA_RATE] = 9, PROJ.mfx[0].slot[0].v[QD_MA_RANGE] = 1;
    qd_seq_select(&SEQ, 0, 0);
    qd_seq_play(&SEQ);
    blocks(6 * BLK_16TH);
    {
        int t0 = 0, t3 = 0, p[8], k = 0;
        for (i = 0; i < NPE; i++)
            if (PE[i].kind == QD_LOG_ON) {
                t0 += PE[i].track == 0, t3 += PE[i].track == 3;
                if (k < 8)
                    p[k++] = PE[i].note;
            }
        CHECK(t0 >= 3 && t3 >= 1 && k >= 3 && p[0] == 48 && p[1] == 60 && p[2] == 64,
              "two tracks in one arp: 48 (track 4) 60 64 (track 1) .. each on its own track (%s)", list(p, k));
    }
    /* the voice limit after the chain: HARM's four notes on a track limited to 2 */
    rig();
    {
        uint8_t n1 = 60;
        qd_step_on(qd_proj_pat(&PROJ, 0), 1, 0);
        qd_step_set_notes(qd_proj_pat(&PROJ, 0), 1, 0, &n1, 1);
        qd_step_set_len(qd_proj_pat(&PROJ, 0), 1, 0, QD_LEN_1 + 4);
    }
    qd_proj_pat(&PROJ, 0)->trk[1].mfx = 2, qd_proj_pat(&PROJ, 0)->trk[1].limit = 2;
    qd_mfx_slot_init(&PROJ.mfx[1].slot[0], QD_MFX_HARM);
    PROJ.mfx[1].slot[0].v[QD_MH_N4] = 3, PROJ.mfx[1].slot[0].v[QD_MH_N5] = 24;
    qd_seq_select(&SEQ, 0, 0);
    qd_seq_play(&SEQ);
    blocks(BLK_16TH);
    for (n = 0, i = 0; i < NPE; i++)
        n += PE[i].kind == QD_LOG_ON;
    CHECK(n == 5 && qd_pool_count(&POOL, 1) <= 2, "HARM: five notes into a track limited to 2: %d voices", qd_pool_count(&POOL, 1));
    qd_player_stop(&PL);
    /* no stuck voices: an ARP + HARM + REPEAT chain, a pattern change, then STOP; a mute */
    rig();
    {
        uint8_t ch[3] = {60, 64, 67};
        int s;
        for (s = 0; s < 16; s += 2) {
            qd_step_on(qd_proj_pat(&PROJ, 0), 0, s);
            qd_step_set_notes(qd_proj_pat(&PROJ, 0), 0, s, ch, 3);
            qd_step_set_len(qd_proj_pat(&PROJ, 0), 0, s, QD_LEN_1 + 2);
        }
        qd_proj_pat(&PROJ, 0)->trk[0].mfx = 1;
        qd_step_on(qd_proj_pat(&PROJ, 1), 2, 0);                 /* (pattern 2: another track only) */
    }
    qd_mfx_slot_init(&PROJ.mfx[0].slot[0], QD_MFX_HARM);
    qd_mfx_slot_init(&PROJ.mfx[0].slot[1], QD_MFX_ARP);
    PROJ.mfx[0].slot[1].v[QD_MA_RATE] = 12;
    qd_mfx_slot_init(&PROJ.mfx[0].slot[2], QD_MFX_REPEAT);
    PROJ.mfx[0].slot[2].v[QD_MR_RATE] = 12, PROJ.mfx[0].slot[2].v[QD_MR_MODE] = QD_AM_1ST;
    qd_seq_select(&SEQ, 0, 0);
    qd_seq_play(&SEQ);
    blocks(20 * BLK_16TH);
    ok = qd_mfx_busy(&PL.mfx, 1) && PL.mfx_ons > 20;
    qd_seq_select(&SEQ, 1, 1);                          /* (at the next 16th) */
    blocks(24 * BLK_16TH);
    CHECK(ok && qd_pool_gated(&POOL, 0) == 0 && !qd_mfx_busy(&PL.mfx, 1), "a pattern change: track 1's chain runs out, "
          "nothing held (%d)", qd_pool_gated(&POOL, 0));
    qd_player_stop(&PL);
    blocks(2);
    CHECK(!gated(), "then STOP: nothing held");
    PROJ.mfx[0].slot[1].v[QD_MA_MODE] = QD_AM_HOLD;
    PROJ.mfx[0].slot[2].v[QD_MR_MODE] = QD_AM_HOLD;
    qd_seq_select(&SEQ, 0, 0);
    qd_seq_play(&SEQ);
    blocks(6 * BLK_16TH);
    ok = qd_mfx_busy(&PL.mfx, 1);
    SEQ.mute = 1;
    blocks(10 * BLK_16TH);
    CHECK(ok && !qd_mfx_busy(&PL.mfx, 1) && qd_pool_gated(&POOL, 0) == 0, "latched (HOLD), then the track muted: its "
          "notes out of the group, nothing held");
    qd_player_stop(&PL);
    /* the keys through the group while stopped: the arp runs on the tempo */
    rig();
    qd_proj_pat(&PROJ, 0)->trk[0].mfx = 1;
    qd_seq_select(&SEQ, 0, 0);
    qd_mfx_slot_init(&PROJ.mfx[0].slot[0], QD_MFX_ARP);
    PROJ.mfx[0].slot[0].v[QD_MA_RATE] = 9, PROJ.mfx[0].slot[0].v[QD_MA_RANGE] = 1;
    qd_player_key_on(&PL, 0, 60, 100);
    qd_player_key_on(&PL, 0, 67, 100);
    blocks(8 * BLK_16TH + 8);
    for (n = 0, i = 0; i < NPE; i++)
        n += PE[i].kind == QD_LOG_ON;
    CHECK(n >= 8 && n <= 9, "the keys, stopped, into an ARP at 1/16: a note each 16th at the tempo (%d in 8)", n);
    qd_player_key_off(&PL, 0, 60);
    qd_player_key_off(&PL, 0, 67);
    blocks(2 * BLK_16TH);
    CHECK(!gated() && !qd_mfx_busy(&PL.mfx, 1), "the keys up: nothing held");
}

int main(void)
{
    t_simple();
    t_random();
    t_selector();
    t_repeat();
    t_arp();
    t_fuzz();
    t_pool();
    t_player();
    printf("test_mfx: the RAM: a group's settings %d bytes (5: %d), its run-time %d bytes (5: %d), the state pool %d\n",
           (int)sizeof(qd_mfx_group_t), (int)sizeof(qd_mfx_group_t) * 5, (int)sizeof(qd_mfx_grt_t),
           (int)sizeof(qd_mfx_grt_t) * 5, (int)sizeof(qd_mfx_srt_t) * QD_MFX_NSRT);
    printf("test_mfx: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
