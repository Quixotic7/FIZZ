/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The voice pool's rules (core/voice/pool.h, PLAN.md §2), one test each:
 *   limits      a track never sounds more than its limit; at it, a new note takes its oldest held voice
 *   choke       limit 1: every note cuts the last one over one block; the new note starts in that slot the block after
 *   released    at the limit, a released voice of the track goes before a held one (the quietest of them)
 *   reserve     a track at or under its reserve is not robbed; the steal takes from the track furthest over its
 *               reserve, a released voice of any such track first
 *   17th        16 voices sounding: a 17th note (its track at its limit, or under it: a steal across the tracks)
 *   same note   the same note again with the same sound is struck again in place; with another sound a fresh voice
 *               takes it and the old one releases; a note still waiting takes the newer strike
 *   tap         a note whose key went up while it waited for its voice still sounds (and is released at once)
 *   fade        the click of a steal: the largest sample step around it against the clean renders (the old note
 *               alone, the new note alone) and against a hard cut; the fade is the old note's own ramp to 0
 *   free        voices end after their release and their slots are free again; 64 voices as a setting
 *   glide       E1: portamento's starts: a lone note from the track's last, chords by rank, LEGATO, OFF, limit 1
 *
 *     test_pool          exit 1 on a failed check */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pool.h"

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

#define CTL QD_QUAD_CTL
static qd_pool_t P;
static qd_quad_owner_t O[QD_POOL_NTRACK];
static int16_t PA[QD_QUAD_NP], PB[QD_QUAD_NP];   /* two sounds: INIT (a held tone) and EP */

static void reset(int nvoice)
{
    int t;
    qd_pool_init(&P, nvoice);
    for (t = 0; t < QD_POOL_NTRACK; t++) {
        qd_quad_owner_init(&O[t]);
        O[t].quad_patch = PA;
    }
}
/* one control block of the pool, its voices summed into out (mono) */
static int block(int32_t *out)
{
    int i, t, n = 0;
    int32_t tmp[CTL];
    if (!out)
        out = tmp;
    memset(out, 0, CTL * sizeof *out);
    qd_pool_block_begin(&P);
    for (t = 0; t < QD_POOL_NTRACK; t++)
        qd_quad_owner_block(&O[t]);
    for (i = 0; i < P.nvoice; i++)
        n += qd_pool_render(&P, i, out);
    return n;
}
static void blocks(int n)
{
    while (n-- > 0)
        block(0);
}
static int on(int t, int note) { return qd_pool_note_on(&P, t, &O[t], note, 100, PA); }
static int sounding_note(int t, int note)          /* the slot sounding note on track t (gate or released), -1 */
{
    int i;
    for (i = 0; i < P.nvoice; i++)
        if (qd_pool_sounding(&P, i) && P.v[i].track == t && P.v[i].q.note == note)
            return i;
    return -1;
}

static void t_limits(void)
{
    int k, max = 0;
    reset(16);
    qd_pool_set_track(&P, 0, 3, 0);
    for (k = 0; k < 5; k++) {
        on(0, 60 + k);
        block(0);
        max = qd_pool_count(&P, 0) > max ? qd_pool_count(&P, 0) : max;
        block(0);
    }
    blocks(2);
    CHECK(max <= 3 && qd_pool_count(&P, 0) == 3, "limit 3: the track sounded %d voices at most, %d now", max,
          qd_pool_count(&P, 0));
    CHECK(sounding_note(0, 60) < 0 && sounding_note(0, 61) < 0 && sounding_note(0, 62) >= 0 && sounding_note(0, 63) >= 0 &&
              sounding_note(0, 64) >= 0, "limit 3: the three newest notes should sound");
    CHECK(P.steals_own == 2 && P.steals_cross == 0 && qd_pool_in_use(&P) == 3, "limit 3: own steals %u, cross %u, in use %d",
          P.steals_own, P.steals_cross, qd_pool_in_use(&P));
    printf("  limits: limit 3, five held notes: at most %d sounding, the oldest two taken by the newest\n", max);
}

static void t_choke(void)
{
    int s0, s1, k, first = -1;
    int32_t out[CTL];
    reset(16);
    qd_pool_set_track(&P, 1, 1, 0);
    s0 = qd_pool_note_on(&P, 1, &O[1], 40, 100, PA);
    blocks(50);
    s1 = qd_pool_note_on(&P, 1, &O[1], 43, 100, PA);
    CHECK(s1 == s0 && P.v[s0].q.kill && P.v[s0].pend, "choke: the new note waits on the old one's slot (%d, %d)", s0, s1);
    CHECK(qd_pool_count(&P, 1) == 1, "choke: the track counts %d", qd_pool_count(&P, 1));
    block(out);                                         /* the fade */
    CHECK(!P.v[s0].q.active && P.v[s0].pend && abs(out[CTL - 1]) < 2000, "choke: after the fade block the old note is gone"
          " (last sample %d)", out[CTL - 1]);
    for (k = 0; k < 3 && first < 0; k++) {
        block(out);
        if (P.v[s0].q.active && P.v[s0].q.note == 43)
            first = k;
    }
    CHECK(first == 0, "choke: the new note started %d blocks after the fade", first);
    on(1, 47);
    blocks(4);
    on(1, 48);
    blocks(4);
    CHECK(qd_pool_count(&P, 1) == 1 && qd_pool_in_use(&P) == 1 && sounding_note(1, 48) == s0, "choke: one voice, the last note");
    printf("  choke: limit 1, every note cuts the last over one block and starts in its slot the block after\n");
}

static void t_released(void)
{
    int a, b, c, d;
    reset(16);
    qd_pool_set_track(&P, 0, 3, 0);
    a = on(0, 60);
    blocks(2);
    b = on(0, 64);
    blocks(2);
    c = on(0, 67);
    blocks(2);
    qd_pool_note_off(&P, 0, 64);                        /* b released, a (older) still held */
    blocks(10);
    d = on(0, 72);
    CHECK(d == b, "released first: the new note took slot %d, the released one is %d (the oldest held %d)", d, b, a);
    blocks(3);
    CHECK(sounding_note(0, 60) == a && sounding_note(0, 67) == c && sounding_note(0, 72) == b, "released first: the held notes kept");
    /* two released: the quieter goes */
    qd_pool_note_off(&P, 0, 60);
    blocks(100);                                        /* (60 decays longer) */
    qd_pool_note_off(&P, 0, 67);
    blocks(2);
    d = on(0, 74);
    CHECK(d == a && qd_quad_voice_level(&P.v[a].q) <= qd_quad_voice_level(&P.v[c].q), "released: the quietest (slot %d) "
          "should go, %d went", a, d);
    printf("  released: at the limit a released voice goes first, the quietest of them\n");
}

#if QD_POOL_DEFAULT >= 16                               /* (the 16-voice scenes: the knob's build only) */
static void t_reserve(void)
{
    int k, s;
    reset(16);
    qd_pool_set_track(&P, 2, 6, 2);                     /* track 2 keeps 2 */
    qd_pool_set_track(&P, 3, 16, 0);
    on(2, 36);
    on(2, 43);
    for (k = 0; k < 14; k++)
        on(3, 50 + k);
    blocks(3);
    CHECK(qd_pool_in_use(&P) == 16, "reserve: the pool full (%d)", qd_pool_in_use(&P));
    on(3, 70);
    on(3, 71);
    blocks(3);
    CHECK(sounding_note(2, 36) >= 0 && sounding_note(2, 43) >= 0 && qd_pool_count(&P, 2) == 2, "reserve: track 2 robbed");
    CHECK(sounding_note(3, 50) < 0 && sounding_note(3, 51) < 0 && sounding_note(3, 70) >= 0 && P.steals_cross == 2,
          "reserve: track 3's oldest should have gone (cross steals %u)", P.steals_cross);
    /* every track at or under its reserve: nothing to take, the note is dropped */
    reset(4);
    qd_pool_set_track(&P, 0, 16, 2);
    qd_pool_set_track(&P, 1, 16, 2);
    on(0, 40), on(0, 41), on(1, 50), on(1, 51);
    blocks(2);
    s = on(2, 60);
    CHECK(s < 0 && P.drops == 1 && qd_pool_count(&P, 0) == 2 && qd_pool_count(&P, 1) == 2, "reserve: a note should be "
          "dropped when no track is over its reserve (slot %d, drops %u)", s, P.drops);
    /* furthest over its reserve: track 3 holds 10, track 4 holds 6: track 3's oldest held goes */
    reset(16);
    qd_pool_set_track(&P, 5, 6, 0);
    for (k = 0; k < 10; k++)
        on(3, 40 + k);
    blocks(1);
    for (k = 0; k < 6; k++)
        on(4, 60 + k);
    blocks(2);
    on(5, 80);
    blocks(3);
    CHECK(sounding_note(3, 40) < 0 && qd_pool_count(&P, 3) == 9 && qd_pool_count(&P, 4) == 6 && sounding_note(5, 80) >= 0,
          "reserve: the furthest over (track 3) should give its oldest held note");
    /* a released voice of any track over its reserve goes before a held one */
    qd_pool_note_off(&P, 4, 63);
    blocks(5);
    k = sounding_note(4, 63);
    s = on(5, 81);
    CHECK(s == k && k >= 0, "reserve: the released voice (slot %d) should go first, %d went", k, s);
    printf("  reserve: a track at its reserve kept its voices; the steal took from the furthest over, released first\n");
}

static void t_17th(void)
{
    int k, s;
    reset(16);
    for (k = 0; k < 16; k++)
        on(0, 48 + k);
    blocks(2);
    s = on(0, 70);                                      /* track 0 at its limit (16) */
    blocks(2);
    CHECK(s >= 0 && sounding_note(0, 48) < 0 && sounding_note(0, 70) == s && qd_pool_in_use(&P) == 16 && P.steals_own == 1,
          "17th on its own track: the oldest held should go");
    reset(16);
    qd_pool_set_track(&P, 0, 8, 0);
    qd_pool_set_track(&P, 1, 8, 0);
    qd_pool_set_track(&P, 2, 6, 0);
    for (k = 0; k < 8; k++)
        on(0, 48 + k);
    blocks(1);
    for (k = 0; k < 8; k++)
        on(1, 60 + k);
    blocks(2);
    s = on(2, 72);                                      /* track 2 under its limit, the pool full: across */
    blocks(2);
    CHECK(s >= 0 && sounding_note(0, 48) < 0 && sounding_note(2, 72) == s && P.steals_cross == 1 && !P.drops,
          "17th across: tracks 0 and 1 tie at 8 over, the oldest held (track 0's 48) should go");
    printf("  17th: on its own full track the oldest held went; across the tracks the furthest over gave its oldest\n");
}
#endif

static void t_same_note(void)
{
    int a, b, c;
    reset(16);
    a = on(0, 60);
    blocks(20);
    b = on(0, 60);
    CHECK(a == b && P.restrikes == 1 && qd_pool_in_use(&P) == 1, "same sound: struck again in place (%d, %d)", a, b);
    blocks(5);
    c = qd_pool_note_on(&P, 0, &O[0], 60, 100, PB);     /* another sound */
    CHECK(c != a && c >= 0 && P.v[a].q.active && !P.v[a].q.gate && P.v[c].q.gate && qd_pool_in_use(&P) == 2,
          "another sound: a fresh voice (%d) and the old one (%d) released", c, a);
    CHECK(!memcmp(P.v[a].q.patch, PA, sizeof PA) && !memcmp(P.v[c].q.patch, PB, sizeof PB),
          "another sound: each voice keeps its own patch");
    /* a released voice of the note, the same sound: struck again (no new voice) */
    reset(16);
    a = on(0, 62);
    blocks(5);
    qd_pool_note_off(&P, 0, 62);
    blocks(3);
    b = on(0, 62);
    CHECK(a == b && P.v[a].q.gate && qd_pool_in_use(&P) == 1, "same sound in its release: struck again in place");
    /* a note waiting for a fade takes the newer strike */
    reset(16);
    qd_pool_set_track(&P, 0, 1, 0);
    on(0, 40);
    blocks(5);
    a = on(0, 45);
    b = qd_pool_note_on(&P, 0, &O[0], 45, 30, PB);
    CHECK(a == b && P.v[a].pend && P.v[a].pend_vel == 30 && !memcmp(P.v[a].pend_patch, PB, sizeof PB) && qd_pool_in_use(&P) == 1,
          "a waiting note struck again: one note, the newer velocity and sound");
    printf("  same note: the same sound re-struck in place, another sound took a fresh voice and released the old\n");
}

static void t_tap(void)
{
    int a, k, heard = 0;
    reset(16);
    qd_pool_set_track(&P, 0, 1, 0);
    on(0, 40);
    blocks(5);
    a = on(0, 47);
    qd_pool_note_off(&P, 0, 47);                        /* the key up before its voice was free */
    for (k = 0; k < 6; k++) {
        block(0);
        heard |= P.v[a].q.active && P.v[a].q.note == 47;
    }
    CHECK(heard && !P.v[a].q.gate, "tap: the waiting note should sound, released");
    printf("  tap: a note released while it waited still sounds\n");
}

/* the click: the largest step between two samples within a block either side of the steal */
#define FB 120u                                         /* blocks rendered */
#define KB 60u                                          /* the steal's block */
static int32_t maxstep(const int32_t *x, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    uint32_t i;
    for (i = a + 1u; i < b; i++)
        m = abs(x[i] - x[i - 1]) > m ? abs(x[i] - x[i - 1]) : m;
    return m;
}
static void t_fade(void)
{
    static int32_t cleanA[FB * CTL], cleanB[FB * CTL], st[FB * CTL], cut[FB * CTL];
    static const int notes[][2] = {{69, 72}, {45, 52}, {81, 62}};
    static const int presets[] = {0, 1, 3, 8, 13};
    uint32_t k, w0 = (KB - 1u) * CTL, w1 = (KB + 3u) * CTL;
    int pi, ni;
    int32_t worst_excess = 0;
    printf("  fade: the largest sample step around a steal (blocks %u..%u), the old note's fade against the clean renders\n",
           KB - 1u, KB + 2u);
    for (pi = 0; pi < (int)(sizeof presets / sizeof presets[0]); pi++)
        for (ni = 0; ni < 3; ni++) {
            int16_t pp[QD_QUAD_NP];
            int32_t sa, sb, ss, sc, peak = 0, bound, fade_err = 0, a0 = 0, a1 = 0;
            uint32_t i;
            int a;
            qd_quad_preset_patch(presets[pi], pp);
            /* A alone (its amplitude at the end of the block before the steal's, A0, and of the steal's, A1) */
            reset(16);
            qd_pool_set_track(&P, 0, 1, 0);
            a = qd_pool_note_on(&P, 0, &O[0], notes[ni][0], 100, pp);
            for (k = 0; k < FB; k++) {
                block(cleanA + k * CTL);
                if (k == KB - 1u)
                    a0 = qd_quad_voice_level(&P.v[a].q);
                if (k == KB)
                    a1 = qd_quad_voice_level(&P.v[a].q);
            }
            /* B alone, from where it starts in the steal (the block after the fade) */
            reset(16);
            memset(cleanB, 0, sizeof cleanB);
            for (k = 0; k < FB; k++) {
                if (k == KB + 1u)
                    qd_pool_note_on(&P, 0, &O[0], notes[ni][1], 100, pp);
                block(cleanB + k * CTL);
            }
            /* the steal: A, then B at block KB on a limit-1 track */
            reset(16);
            qd_pool_set_track(&P, 0, 1, 0);
            a = qd_pool_note_on(&P, 0, &O[0], notes[ni][0], 100, pp);
            for (k = 0; k < FB; k++) {
                if (k == KB)
                    qd_pool_note_on(&P, 0, &O[0], notes[ni][1], 100, pp);
                block(st + k * CTL);
            }
            CHECK(P.v[a].q.note == notes[ni][1] && P.steals_own == 1, "fade: B should sound in A's slot");
            /* a hard cut for contrast: A until the block, nothing in the fade block, then B */
            for (i = 0; i < FB * CTL; i++)
                cut[i] = i < KB * CTL ? cleanA[i] : cleanB[i];
            sa = maxstep(cleanA, w0, w1);
            sb = maxstep(cleanB, (KB + 1u) * CTL, w1);
            ss = maxstep(st, w0, w1);
            sc = maxstep(cut, w0, w1);
            for (i = w0; i < (KB + 1u) * CTL; i++)
                peak = abs(cleanA[i]) > peak ? abs(cleanA[i]) : peak;
            /* the fade: A's own samples, its amplitude ramped from A0 to 0 across the block where the clean render ramps
             * from A0 to A1 (the engine's ramp, voice.c's m.amp1 = 0): within a few units of that */
            for (i = 0; i < CTL; i++) {
                double u = (i + 1.0) / CTL, gc = a0 + (a1 - a0) * u, gs = a0 * (1.0 - u);
                double want = gc > 0.0 ? cleanA[KB * CTL + i] * gs / gc : 0.0;
                int32_t e = (int32_t)fabs(st[KB * CTL + i] - want);
                fade_err = e > fade_err ? e : fade_err;
            }
            bound = (sa > sb ? sa : sb) + peak / (int32_t)CTL + 2;
            printf("    %-11s %d -> %d: steal %5d, clean A %5d, B %5d, hard cut %5d (bound %5d); the fade within %d of A's"
                   " ramp\n", qd_quad_preset_name(presets[pi]), notes[ni][0], notes[ni][1], ss, sa, sb, sc, bound, fade_err);
            CHECK(ss <= bound, "fade %s: a step of %d at the steal (clean %d / %d, one block's ramp %d)",
                  qd_quad_preset_name(presets[pi]), ss, sa, sb, peak / CTL);
            CHECK(fade_err <= 4 + peak / 1000, "fade %s: the fade block is not A's ramp to 0 (off by %d)",
                  qd_quad_preset_name(presets[pi]), fade_err);
            if (ss - (sa > sb ? sa : sb) > worst_excess)
                worst_excess = ss - (sa > sb ? sa : sb);
        }
    printf("    the steal's largest step at most %d above the clean renders' own\n", worst_excess);
}

static void t_free(void)
{
    const int nn = QD_POOL_MAX * 5 / 8;                 /* (P2.0: the profile's pool: 40 of 64, the device's 10 of 16) */
    int k, i, left = 0;
    reset(QD_POOL_MAX);
    CHECK(P.nvoice == QD_POOL_MAX, "%d voices as a setting", QD_POOL_MAX);
    for (k = 0; k < nn; k++)
        qd_pool_note_on(&P, k % QD_POOL_NTRACK, &O[k % QD_POOL_NTRACK], 40 + k, 90, PB);
    blocks(10);
    CHECK(qd_pool_in_use(&P) == nn && !P.steals_own && !P.steals_cross, "%d voices: %d notes on %d voices (%d)",
          QD_POOL_MAX, nn, nn, qd_pool_in_use(&P));
    qd_pool_release_all(&P, -1);
    for (k = 0; k < 20 * 44100 / CTL && block(0); k++)
        ;
    for (i = 0; i < P.nvoice; i++)
        left += qd_pool_used(&P, i);
    CHECK(!left, "free: %d voices still in use 20 s after the release", left);
    printf("  free: %d notes on a %d-voice pool, released: every slot free again after %.1f s\n", nn, QD_POOL_MAX,
           k * CTL / 44100.0);
}

/* E1: portamento's starts: a lone note from the track's last; a chord by rank against the last chord (one more: the
 * nearest); LEGATO only held; PORT OFF none; a choked (waiting) note glides as it starts */
static int16_t PG[QD_QUAD_NP];
static int gon(int t, int note, const void *key, int64_t tick)
{
    const int held = qd_pool_gated(&P, t);
    const int i = qd_pool_note_on(&P, t, &O[t], note, 100, PG);
    qd_pool_glide_note(&P, i, &O[t], key, tick, held);
    return i;
}
static void t_glide(void)
{
    static const int C1[3] = {55, 48, 52}, C2[4] = {57, 50, 60, 53}, W2[4] = {55, 48, 55, 52};
    int i, k, ok;
    char key1 = 1, key2 = 2;
    memcpy(PG, PA, sizeof PG);
    PG[QD_QP_PORT] = QD_PORT_ON, PG[QD_QP_PTIM] = 20;
    reset(16);
    i = gon(0, 48, 0, 0);
    block(0);
    CHECK(P.v[i].q.pitch_cur == 48 * 16, "the track's first note: no glide");
    i = gon(0, 60, 0, 0);
    CHECK(P.v[i].q.pitch_cur == 60 * 16, "a note's glide waits for the block (its chord may grow)");
    block(0);
    CHECK(P.v[i].q.pitch_cur > 48 * 16 && P.v[i].q.pitch_cur < 60 * 16 && P.v[i].q.pitch16 == 60 * 16,
          "a lone note glides from the track's last (48 -> 60: at %d / 16)", P.v[i].q.pitch_cur);
    blocks(200);
    CHECK(P.v[i].q.pitch_cur == 60 * 16, "the glide ends on the note");
    reset(16);                                          /* chords: by rank, the fourth from the nearest */
    for (k = 0; k < 3; k++)
        gon(1, C1[k], &key1, 100);
    block(0);
    for (k = 0; k < 4; k++)
        gon(1, C2[k], &key2, 200);
    block(0);
    for (ok = 1, k = 0; k < 4; k++) {
        int j, s = -1;
        for (j = 0; j < P.nvoice; j++)
            if (qd_pool_sounding(&P, j) && P.v[j].track == 1 && P.v[j].q.pitch16 == C2[k] * 16)
                s = j;
        ok &= s >= 0 && P.v[s].q.gstep > 0 &&
              abs(P.v[s].q.pitch_cur - W2[k] * 16) <= abs(P.v[s].q.pitch16 - W2[k] * 16) &&
              (P.v[s].q.pitch_cur - W2[k] * 16) * (C2[k] - W2[k]) > 0;
    }
    CHECK(ok, "48 52 55 then 50 53 57 60: 48 -> 50, 52 -> 53, 55 -> 57, 60 from the nearest (55)");
    CHECK(O[1].lc_n == 4 && O[1].lc[0] == 50 * 16 && O[1].lc[3] == 60 * 16 && O[1].last_pitch == 60 * 16,
          "the last chord (low to high) kept for the next");
    reset(16);                                          /* LEGATO: only from a note still held */
    PG[QD_QP_PORT] = QD_PORT_LEGATO;
    gon(2, 48, 0, 0);
    block(0);
    qd_pool_note_off(&P, 2, 48);
    blocks(2);
    i = gon(2, 55, 0, 0);
    block(0);
    CHECK(P.v[i].q.pitch_cur == 55 * 16, "LEGATO, the last key up: no glide");
    i = gon(2, 60, 0, 0);
    block(0);
    CHECK(P.v[i].q.pitch_cur < 60 * 16 && P.v[i].q.pitch_cur > 55 * 16, "LEGATO, overlapping: glides 55 -> 60");
    reset(16);                                          /* PORT OFF: as before */
    PG[QD_QP_PORT] = QD_PORT_OFF;
    gon(3, 48, 0, 0);
    block(0);
    i = gon(3, 60, 0, 0);
    block(0);
    CHECK(P.v[i].q.pitch_cur == 60 * 16 && !P.v[i].q.gstep, "PORT OFF: no glide");
    reset(16);                                          /* mono (limit 1): the new note waits a block, then glides */
    PG[QD_QP_PORT] = QD_PORT_ON;
    qd_pool_set_track(&P, 4, 1, 0);
    gon(4, 48, 0, 0);
    blocks(3);
    i = gon(4, 60, 0, 0);
    CHECK(i >= 0 && P.v[i].pend, "limit 1: the new note waits for the fade");
    blocks(3);
    i = sounding_note(4, 60);
    CHECK(i >= 0 && P.v[i].q.pitch_cur < 60 * 16 && P.v[i].q.pitch_cur > 48 * 16,
          "limit 1: it glides from 48 as it starts (at %d / 16)", i >= 0 ? P.v[i].q.pitch_cur : -1);
}

/* P2.6: 8 voices shared by the tracks (the unit on one core): a note with every voice busy takes a released voice
 * first (any track's, the quietest), and with all held, the oldest note of the track furthest over its reserve: a held
 * chord loses its oldest note, never its newest */
static void t_steal8(void)
{
    static const int CHORD[5] = {48, 52, 55, 59, 62};
    int k, a, held = 0;
    reset(8);
    for (k = 0; k < 6; k++) {                           /* six held on track 1 */
        on(0, 48 + 3 * k);
        blocks(1);
    }
    on(1, 70);
    on(1, 74);
    blocks(2);
    qd_pool_note_off(&P, 1, 70);                        /* two released on track 2 */
    qd_pool_note_off(&P, 1, 74);
    blocks(4);
    a = on(2, 80);                                      /* every voice busy: a ninth note */
    blocks(3);
    for (k = 0; k < 6; k++)
        held += sounding_note(0, 48 + 3 * k) >= 0;
    CHECK(a >= 0 && held == 6 && sounding_note(2, 80) >= 0 && (sounding_note(1, 70) < 0 || sounding_note(1, 74) < 0),
          "8 voices, 6 held and 2 released: the new note took a released voice (slot %d), the held ones all kept (%d)",
          a, held);
    reset(8);
    for (k = 0; k < 5; k++) {                           /* a chord of five on track 1, struck low to high .. */
        on(0, CHORD[k]);
        blocks(1);
    }
    for (k = 0; k < 3; k++) {                           /* .. three held on track 4: all 8 held */
        on(3, 64 + k);
        blocks(1);
    }
    blocks(2);
    a = on(2, 84);
    blocks(3);
    CHECK(a >= 0 && sounding_note(0, CHORD[0]) < 0 && sounding_note(0, CHORD[4]) >= 0 && sounding_note(2, 84) >= 0 &&
              sounding_note(3, 64) >= 0, "8 voices all held: the note took the chord's oldest (of the track furthest "
          "over), its newest kept");
    printf("  steal at 8 voices: a released voice first, then the oldest held note of the busiest track\n");
}

/* 2026-10-26, the VOICE page: UNISON n voices a note (detuned, spread; a LIMIT keeps the middle ones), MONO takes the
 * track's voices, LEGATO moves them, PRIORITY: the lowest robbed first, a note never robs a higher one */
static int track_voices(int t, int note, uint32_t *dmin, uint32_t *dmax, int *pmin, int *pmax)
{
    int i, n = 0;
    *dmin = 0xFFFFFFFFu, *dmax = 0, *pmin = 99, *pmax = -99;
    for (i = 0; i < P.nvoice; i++)
        if (qd_pool_sounding(&P, i) && P.v[i].track == t && P.v[i].q.note == note && P.v[i].q.gate) {
            const uint32_t d = P.v[i].q.udet ? P.v[i].q.udet : 65536u;
            const int pn = P.v[i].at.mixm & (1u << QD_VMIX_PAN) ? P.v[i].at.mixv[QD_VMIX_PAN] : P.pan[t];
            n++;
            *dmin = d < *dmin ? d : *dmin, *dmax = d > *dmax ? d : *dmax;
            *pmin = pn < *pmin ? pn : *pmin, *pmax = pn > *pmax ? pn : *pmax;
        }
    return n;
}
static void t_voice(void)
{
    static int16_t U[QD_QUAD_NP], H[QD_QUAD_NP];
    uint32_t dmin, dmax;
    int pmin, pmax, n, a, k;
    uint8_t blob[QD_QUAD_BLOB];
    int16_t q[QD_QUAD_NP];
    memcpy(U, PA, sizeof U);
    CHECK(U[QD_QP_VMODE] == 0 && U[QD_QP_UNI] == 1 && U[QD_QP_UDET] == 20 && U[QD_QP_USPR] == 64 && U[QD_QP_VPRIO] == 1,
          "a factory sound's VOICE page: POLY, UNISON OFF, DETUNE 20, SPREAD 64, PRIORITY MID");
    qd_quad_pack(U, blob);
    CHECK(!blob[124] && !blob[125] && !blob[126] && !blob[127], "its blob's last four bytes zeros (as every older one)");
    U[QD_QP_VMODE] = 1, U[QD_QP_UNI] = 5, U[QD_QP_UDET] = 127, U[QD_QP_USPR] = 0, U[QD_QP_VPRIO] = 2;
    qd_quad_pack(U, blob);
    CHECK(qd_quad_unpack(blob, QD_QUAD_BLOB, q) == 1 && !memcmp(q, U, sizeof q), "the VOICE page's values round-trip");
    CHECK(!qd_quad_lockable(QD_QP_UNI) && !qd_quad_lockable(QD_QP_VPRIO) && qd_quad_lockable(QD_QP_PORT) &&
          qd_quad_lockable(QD_QP_MOD(7, QD_QM_DEPTH)), "the VOICE page's values are not lockable, the others are");
    /* UNISON 4, SPREAD 127 on track 1 (pan 0): four voices, detuned both ways, across the stereo */
    reset(8);
    memcpy(U, PA, sizeof U);
    U[QD_QP_UNI] = 4, U[QD_QP_UDET] = 127, U[QD_QP_USPR] = 127;
    qd_pool_set_pan(&P, 0, 0);
    a = qd_pool_note_on(&P, 0, &O[0], 60, 100, U);
    blocks(2);
    n = track_voices(0, 60, &dmin, &dmax, &pmin, &pmax);
    CHECK(a >= 0 && n == 4 && dmin < 65536u - 1000u && dmax > 65536u + 1000u && pmin <= -60 && pmax >= 60,
          "UNISON 4: %d voices on the note, detune %u..%u, pan %d..%d", n, (unsigned)dmin, (unsigned)dmax, pmin, pmax);
    qd_pool_note_off(&P, 0, 60);
    blocks(1);
    CHECK(track_voices(0, 60, &dmin, &dmax, &pmin, &pmax) == 0, "the note's key up: all four released");
    /* a LIMIT of 2 under UNISON 5: the two middle ones (the least detuned) */
    reset(8);
    U[QD_QP_UNI] = 5, U[QD_QP_USPR] = 0;
    qd_pool_set_track(&P, 0, 2, 0);
    qd_pool_note_on(&P, 0, &O[0], 60, 100, U);
    blocks(2);
    n = track_voices(0, 60, &dmin, &dmax, &pmin, &pmax);
    CHECK(n == 2 && dmin >= 65536u - 1200u && dmax <= 65536u + 1200u, "UNISON 5 on a LIMIT 2: the middle two (%d, %u..%u)",
          n, (unsigned)dmin, (unsigned)dmax);
    /* MONO: one note at a time on the track (UNISON 3: three voices, the next note takes them) */
    reset(8);
    U[QD_QP_VMODE] = 1, U[QD_QP_UNI] = 3;
    qd_pool_note_on(&P, 0, &O[0], 60, 100, U);
    blocks(1);
    qd_pool_note_on(&P, 0, &O[0], 64, 100, U);
    blocks(3);
    CHECK(track_voices(0, 60, &dmin, &dmax, &pmin, &pmax) == 0 && track_voices(0, 64, &dmin, &dmax, &pmin, &pmax) == 3,
          "MONO, UNISON 3: the second note took the first's three voices");
    /* MONO LEGATO, a key down: moved, no new attack (the same slots) */
    reset(8);
    U[QD_QP_UNI] = 1, U[QD_QP_PORT] = QD_PORT_LEGATO;
    a = qd_pool_note_on(&P, 0, &O[0], 60, 100, U);
    blocks(2);
    k = qd_pool_note_on(&P, 0, &O[0], 67, 100, U);
    blocks(1);
    CHECK(k == a && P.v[a].q.note == 67 && P.v[a].q.gate && P.legatos == 1, "MONO LEGATO: the held voice moved to the "
          "new note (slot %d -> %d)", a, k);
    /* PRIORITY: 8 voices full, HIGH on track 1 and LOW on track 2: a MID note robs the LOW track; a LOW note into 8
     * HIGH ones is dropped */
    reset(8);
    memcpy(H, PA, sizeof H);
    H[QD_QP_VPRIO] = 2;
    memcpy(U, PA, sizeof U);
    U[QD_QP_VPRIO] = 0;
    for (k = 0; k < 6; k++)
        qd_pool_note_on(&P, 0, &O[0], 60 + k, 100, H), blocks(1);
    for (k = 0; k < 2; k++)
        qd_pool_note_on(&P, 1, &O[1], 70 + k, 100, U), blocks(1);
    a = qd_pool_note_on(&P, 2, &O[2], 80, 100, PA);
    blocks(3);
    CHECK(a >= 0 && sounding_note(2, 80) >= 0 && sounding_note(0, 60) >= 0 && sounding_note(1, 70) < 0,
          "PRIORITY: track 1 (HIGH) is the furthest over, yet the MID note robbed track 2's LOW voice");
    reset(8);
    for (k = 0; k < 8; k++)
        qd_pool_note_on(&P, 0, &O[0], 50 + k, 100, H), blocks(1);
    a = qd_pool_note_on(&P, 1, &O[1], 80, 100, U);
    CHECK(a < 0 && P.prio_drops == 1, "PRIORITY: a LOW note into eight HIGH voices: dropped");
    printf("  VOICE: unison (detune, spread, a LIMIT's middle), MONO, LEGATO, PRIORITY\n");
}

int main(void)
{
    qd_quad_preset_patch(0, PA);
    qd_quad_preset_patch(1, PB);
    t_limits();
    t_choke();
    t_released();
#if QD_POOL_DEFAULT >= 16                               /* (P2.6: these two fill a 16-voice pool; at 8, t_steal8) */
    t_reserve();
    t_17th();
#else
    printf("  reserve, 17th: 16-voice scenes (the pool holds %d here: t_steal8 checks the same rules at 8)\n", QD_POOL_DEFAULT);
#endif
    t_same_note();
    t_tap();
    t_fade();
    t_free();
    t_glide();
    t_steal8();
    t_voice();
    CHECK(qd_pool_is_dip_id(QD_QP_FTYPE) && qd_pool_is_dip_id(QD_QP_BWPOST) && !qd_pool_is_dip_id(QD_QP_FSHAPE) &&
          !qd_pool_is_dip_id(QD_QP_FGAIN), "E2: a machine switched, BWPOST: the dip (the morph, the gain: not)");
    printf("test_pool: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
