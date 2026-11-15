/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The built-in demos (demo.h): ATRIUM, and the old demo for the host's tools and tests.
 *
 * P1.3b (the old demo): the drums are the pool's KICK, SNARE, CLAP, GHOST, HAT and OPEN HAT (sounds.h, where their
 * design is noted): what were INIT / NOISE-ISH sound locks with p-locks on each step are pool sounds now, so a drum
 * step is a sound lock and its mix p-locks (LEVEL, PAN, the sends). The tracks of odd lengths are pages with a start
 * and an end (a 12-step track: page 1, 1..12; 24 steps: page 1 1..16 and page 2 1..8). The patches, the timings and
 * the mix are as P1.2's: the demos sound the same. */
#include <string.h>

#include "demo.h"
#include "sounds.h"

/* the lengths by name (pattern.h's table) */
enum { L8 = 0, L4 = 1, L2 = 2, L34 = 3, L1 = 4, L15 = 5, LS2 = 6, LS3 = 7, LS4 = 8, LS6 = 9, LS8 = 10, LS12 = 11,
       LS16 = 12 };

#ifdef QD_DEMO_HOST
/* ---------------------------------------------------------------- the old demo (the host's only) --- */
int qd_demo_first = QD_DEMO_OLD;
int qd_demo_old_on = 1;

static void lk(qd_pattern_t *p, int t, int s, int id, int v) { qd_lock_set(p, t, s, id, v); }

static void note(qd_pattern_t *p, int t, int s, int n, int vel, int len)
{
    const uint8_t x = (uint8_t)n;
    qd_step_on(p, t, s);
    qd_step_set_notes(p, t, s, &x, 1);
    qd_step_set_vel(p, t, s, vel);
    qd_step_set_len(p, t, s, len);
}

static void chord(qd_pattern_t *p, int t, int s, int a, int b, int c, int d, int vel, int len, int strum)
{
    uint8_t x[4];
    int n = 0;
    x[n++] = (uint8_t)a;
    if (b)
        x[n++] = (uint8_t)b;
    if (c)
        x[n++] = (uint8_t)c;
    if (d)
        x[n++] = (uint8_t)d;
    qd_step_on(p, t, s);
    qd_step_set_notes(p, t, s, x, n);
    qd_step_set_vel(p, t, s, vel);
    qd_step_set_len(p, t, s, len);
    if (strum)
        qd_step_set_strum(p, t, s, strum);
}

static void tie(qd_pattern_t *p, int t, int s, int n, int vel, int len)
{
    note(p, t, s, n, vel, len);
    qd_step_set_trig(p, t, s, QD_TRIG_TIE);
}

/* ---------------------------------------------------------------- the drums (track 1): the pool's kit --- */
static void kick(qd_pattern_t *p, int s, int vel)
{
    note(p, 0, s, 33, vel, L2);
    qd_step_set_sound(p, 0, s, QD_SND_KICK);
    lk(p, 0, s, QD_LK_LEVEL, 118);                     /* (the drums a little over the track's mix level) */
    lk(p, 0, s, QD_LK_REVERB, 4);
}

static void snare(qd_pattern_t *p, int s, int vel)
{
    note(p, 0, s, 55, vel, L2);
    qd_step_set_sound(p, 0, s, QD_SND_SNARE);
    lk(p, 0, s, QD_LK_LEVEL, 114);
}

static void clap(qd_pattern_t *p, int s, int vel)
{
    chord(p, 0, s, 64, 65, 66, 0, vel, L4, 2);
    qd_step_set_sound(p, 0, s, QD_SND_CLAP);
    lk(p, 0, s, QD_LK_REVERB, 40);
    lk(p, 0, s, QD_LK_LEVEL, 114);
}

static void ghost(qd_pattern_t *p, int s, int vel)
{
    note(p, 0, s, 55, vel, L4);
    qd_step_set_sound(p, 0, s, QD_SND_GHOST);
}

static void hat(qd_pattern_t *p, int s, int vel, int open)
{
    note(p, 0, s, 96, vel, open ? L1 : L4);
    qd_step_set_sound(p, 0, s, open ? QD_SND_OHAT : QD_SND_HAT);
    lk(p, 0, s, QD_LK_PAN, 14);
    if (!open)
        lk(p, 0, s, QD_LK_REVERB, 0);
}

static void beat(qd_pattern_t *p)
{
    qd_tset_t *d = &p->trk[0];
    d->limit = 4;
    d->swing = 1;
    kick(p, 0, 122);
    hat(p, 1, 64, 0);
    hat(p, 2, 92, 0);
    ghost(p, 3, 40);
    snare(p, 4, 116);
    hat(p, 5, 58, 0);
    kick(p, 6, 104);
    hat(p, 7, 88, 1);                                   /* open */
    hat(p, 8, 96, 0);
    ghost(p, 9, 46);
    kick(p, 10, 112);
    hat(p, 11, 70, 0);
    qd_step_set_micro(p, 0, 11, 2);                     /* (laid back) */
    clap(p, 12, 118);
    hat(p, 13, 84, 0);
    qd_step_set_ratchet(p, 0, 13, 2, QD_RAMP_UP);       /* a little roll */
    ghost(p, 14, 36);
    qd_step_set_cond(p, 0, 14, qd_cond_ab(2, 2));       /* every other bar */
    hat(p, 15, 76, 1);
    qd_step_set_len(p, 0, 15, L2);
}

/* ---------------------------------------------------------------- the patterns --- */
static void demo_a01(qd_pattern_t *p)
{
    p->bpm = 100;
    p->swing = 56;
    beat(p);
}

static void demo_a02(qd_pattern_t *p)
{
    qd_tset_t *b = &p->trk[1], *k = &p->trk[2], *l = &p->trk[3];
    p->bpm = 100;
    p->swing = 56;
    beat(p);
    /* BASS: mono, 32 steps (two bars), A minor; TIE slides */
    b->limit = 1;
    qd_track_set_length(p, 1, 32);                      /* pages 1 and 2 */
    b->swing = 1;
    note(p, 1, 0, 33, 115, LS2);
    note(p, 1, 2, 45, 80, L2);
    note(p, 1, 3, 33, 100, L1);
    tie(p, 1, 4, 40, 100, L1);
    note(p, 1, 6, 31, 90, L2);
    note(p, 1, 7, 33, 105, L1);
    note(p, 1, 10, 36, 90, L2);
    note(p, 1, 11, 38, 100, L1);
    tie(p, 1, 12, 40, 100, LS2);
    note(p, 1, 14, 43, 85, L2);
    note(p, 1, 16, 29, 115, LS2);
    note(p, 1, 18, 41, 80, L2);
    note(p, 1, 19, 29, 100, L1);
    note(p, 1, 22, 36, 90, L2);
    note(p, 1, 23, 40, 100, L1);
    note(p, 1, 26, 31, 90, L2);
    note(p, 1, 27, 31, 100, L1);
    tie(p, 1, 28, 35, 100, L1);
    note(p, 1, 30, 38, 85, L2);
    note(p, 1, 31, 40, 80, L2);
    /* CHORDS (the KEYS track's EP) at 1/2x: 16 steps = two bars; strummed, stabs micro-timed, conditions */
    qd_tset_set_rate(k, QD_RATE_1_2);
    chord(p, 2, 0, 57, 60, 64, 71, 90, LS2, 3);         /* Am9 */
    chord(p, 2, 3, 57, 60, 64, 0, 80, L2, 0);           /* a stab, early */
    qd_step_set_micro(p, 2, 3, -6);
    chord(p, 2, 4, 53, 57, 60, 64, 88, LS2, 3);         /* Fmaj7 */
    chord(p, 2, 6, 53, 57, 60, 0, 75, L2, 0);
    qd_step_set_cond(p, 2, 6, qd_cond_ab(1, 2));        /* every other pass */
    chord(p, 2, 8, 52, 55, 60, 64, 88, LS2, 4);         /* C/E */
    chord(p, 2, 11, 52, 55, 60, 0, 78, L2, 0);
    qd_step_set_micro(p, 2, 11, 5);
    chord(p, 2, 12, 50, 55, 59, 62, 86, LS2, 3);        /* G/D */
    chord(p, 2, 14, 50, 55, 59, 0, 74, L2, 0);
    qd_step_set_cond(p, 2, 14, QD_COND_N1ST);
    lk(p, 2, 14, QD_QP_FREQ, 7000);
    /* LEAD: 24 steps (a bar and a half against the others' two), ratchets and chance */
    l->limit = 2;
    qd_track_set_length(p, 3, 24);                      /* page 1 1..16, page 2 1..8 */
    note(p, 3, 0, 76, 100, LS2);
    note(p, 3, 3, 74, 90, L2);
    qd_step_set_chance(p, 3, 3, 75);
    note(p, 3, 4, 72, 95, L1);
    note(p, 3, 6, 69, 110, L2);
    qd_step_set_ratchet(p, 3, 6, 3, QD_RAMP_UP);
    note(p, 3, 8, 72, 90, L1);
    note(p, 3, 10, 74, 90, L2);
    qd_step_set_chance(p, 3, 10, 50);
    note(p, 3, 11, 76, 100, L15);
    note(p, 3, 14, 79, 100, L2);
    qd_step_set_ratchet(p, 3, 14, 2, QD_RAMP_DOWN);
    note(p, 3, 16, 81, 105, LS2);
    qd_step_set_chance(p, 3, 16, 80);
    note(p, 3, 19, 79, 85, L2);
    note(p, 3, 20, 76, 110, L1);
    qd_step_set_ratchet(p, 3, 20, 4, QD_RAMP_ACCENT);
    qd_step_set_chance(p, 3, 20, 60);
    note(p, 3, 22, 74, 90, L1);
    lk(p, 3, 22, QD_LK_DELAY, 70);
}

static void demo_a03(qd_pattern_t *p)
{
    static const int RATE[QD_PAT_NTRACK] = {QD_RATE_1, QD_RATE_1, QD_RATE_1_2, QD_RATE_3_2, QD_RATE_2, QD_RATE_1_4};
    static const int LEN[QD_PAT_NTRACK] = {16, 12, 7, 10, 9, 5};
    int t;
    p->bpm = 118;
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        qd_tset_set_rate(&p->trk[t], RATE[t]);
        qd_track_set_length(p, t, LEN[t]);               /* page 1, 1..LEN */
    }
    {   /* the limits: each track takes its own voices before another's (16 in all, about) */
        static const uint8_t LIM[QD_PAT_NTRACK] = {3, 1, 3, 2, 3, 3};
        for (t = 0; t < QD_PAT_NTRACK; t++)
            p->trk[t].limit = LIM[t];
    }
    /* DRUMS 16 */
    kick(p, 0, 120);
    hat(p, 2, 80, 0);
    snare(p, 4, 110);
    hat(p, 6, 70, 0);
    hat(p, 8, 86, 1);
    kick(p, 10, 108);
    snare(p, 12, 112);
    hat(p, 14, 76, 0);
    ghost(p, 15, 40);
    /* BASS 12: D dorian */
    note(p, 1, 0, 38, 112, L15);
    note(p, 1, 3, 38, 90, L2);
    note(p, 1, 5, 41, 96, L1);
    tie(p, 1, 6, 43, 96, L1);
    note(p, 1, 8, 33, 104, L1);
    note(p, 1, 10, 36, 90, L2);
    note(p, 1, 11, 38, 86, L2);
    /* KEYS 7 at 1/2x */
    chord(p, 2, 0, 53, 57, 60, 0, 82, LS2, 2);          /* Dm7 (F A C) */
    chord(p, 2, 3, 55, 59, 62, 0, 76, L1, 2);           /* Em7 (G B D) */
    chord(p, 2, 5, 52, 57, 64, 0, 80, LS2, 2);          /* Fmaj7 (E A E) */
    /* LEAD 10 at 3/2x */
    note(p, 3, 0, 74, 96, L1);
    note(p, 3, 2, 77, 88, L2);
    note(p, 3, 3, 81, 92, L1);
    qd_step_set_chance(p, 3, 3, 70);
    note(p, 3, 5, 79, 90, L2);
    note(p, 3, 7, 76, 100, L1);
    qd_step_set_ratchet(p, 3, 7, 2, QD_RAMP_DOWN);
    note(p, 3, 8, 72, 86, L2);
    /* PLUCK 9 at 2x: an arpeggio */
    {
        static const uint8_t ARP[9] = {62, 65, 69, 72, 76, 72, 69, 65, 0};
        int s;
        for (s = 0; s < 9; s++)
            if (ARP[s])
                note(p, 4, s, ARP[s], s == 0 ? 96 : 70 + (s * 7) % 20, L2);
    }
    /* PAD 5 at 1/4x: a step is a bar's quarter .. four 16ths */
    chord(p, 5, 0, 50, 57, 64, 0, 70, LS2, 0);          /* D A E: Dm(add9) without its third */
    chord(p, 5, 2, 55, 59, 62, 0, 66, LS2, 0);          /* G */
    /* (BELL and NOISE were tracks 7 and 8: six tracks since 2026-10-24) */
}

void qd_demo_pattern(qd_pattern_t *p, int k)
{
    qd_pat_init(p);
    p->trk[1].limit = 1;                                /* (the default kit: BASS mono) */
    if (k == 0)
        demo_a01(p);
    else if (k == 1)
        demo_a02(p);
    else if (k == 2)
        demo_a03(p);
}
#endif

/* ---------------------------------------------------------------- ATRIUM (tools/song_atrium.c, built in) ---
 * The song's own code, made compact: the same calls in the same order (a pattern made here is the song's, byte for
 * byte: test_atrium), its sounds factory presets (the pack, ids 24..) where the song stores MY presets. A maker (at_t)
 * either writes a pattern or, with none, only counts the steps that lock a pool slot (qd_demo_locks: nothing made; the
 * steps a note took are kept either way, the drums and the hats taking a step only when it is free) */
enum { T_DR, T_BA, T_PAD, T_MEL, T_BELL, T_HAT };
enum { K_KICK = 1, K_SNARE, K_RIM, K_HAT, K_OPEN, K_SHAKER, K_SNAP, K_THAT, K_TOM, K_CYMBAL, X_RADIO, X_WIND,
       P_SUB, P_PAD, P_CHAPEL, P_LEAD, P_BELL };
enum { A_LEN = 64, LS64 = 16 };
/* the pool's slots 1..17: the pack's DUST KICK, DUST SNARE, DUST RIM, DUST HAT, DUST OPEN, DUST SHAKER, TAPE SNAP,
 * TAPE HAT, DUST TOM, 808 CYMBAL, RADIO, WIND, SUB, JUNO PAD, CHAPEL, JUNO LEAD, HAZY BELL (their places in it) */
const uint8_t QD_ATRIUM_POOL[QD_ATRIUM_NPOOL] = {0, 1, 3, 4, 5, 7, 18, 19, 6, 16, 82, 81, 68, 90, 54, 91, 64};

typedef struct {
    qd_pattern_t *p;                                    /* (0: counting, nothing made) */
    uint64_t on[QD_PAT_NTRACK];                         /* the steps with a note */
    int snd, n;                                         /* (counting: the slot, its users) */
    void (*fn)(void *ctx, int t, int s);
    void *ctx;
} at_t;

static void a_note(at_t *g, int t, int s, const uint8_t *x, int n, int vel, int len, int strum)
{
    qd_pattern_t *p = g->p;
    g->on[t] |= (uint64_t)1 << s;
    if (!p)
        return;
    qd_step_on(p, t, s);
    qd_step_set_notes(p, t, s, x, n);
    qd_step_set_vel(p, t, s, vel);
    qd_step_set_len(p, t, s, len);
    if (strum)
        qd_step_set_strum(p, t, s, strum);
}
static void a_n(at_t *g, int t, int s, int n, int vel, int len)
{
    const uint8_t x = (uint8_t)n;
    a_note(g, t, s, &x, 1, vel, len, 0);
}
static void a_lk(at_t *g, int t, int s, int id, int v)
{
    if (g->p)
        qd_lock_set(g->p, t, s, id, v);
}
static void a_rat(at_t *g, int t, int s)
{
    if (g->p)
        qd_step_set_ratchet(g->p, t, s, 2, QD_RAMP_UP);
}
static void a_used(at_t *g, int t, int s, int k)    /* (s -1: track t's sound) */
{
    if (k == g->snd) {
        g->n++;
        if (g->fn)
            g->fn(g->ctx, t, s);
    }
}
static void a_snd(at_t *g, int t, int s, int k)
{
    if (g->p)
        qd_step_set_sound(g->p, t, s, k);
    else
        a_used(g, t, s, k);
}
static void a_trk(at_t *g, int t, int len, int limit)
{
    if (g->p) {
        qd_track_set_length(g->p, t, len);
        if (limit >= 0)
            g->p->trk[t].limit = (uint8_t)limit;
    }
}

/* THE EDITS: a track's third LFO (FLAT: a square on PITCH, 25 cents down; the opening's air: a ramp on FREQ) */
static int a_dest(const char *name)
{
    int d;
    for (d = 0; d < qd_quad_ndest(); d++)
        if (!strcmp(qd_quad_dest_name(d), name))
            return d;
    return 0;
}
static void a_lfo3(qd_pattern_t *p, int t, int speed, int wave, int trig, const char *to, int depth)
{
    int16_t *q = p->snd[t].patch;
    q[QD_QP_LFO(2, QD_QL_SPEED)] = (int16_t)speed, q[QD_QP_LFO(2, QD_QL_MULT)] = 12;
    q[QD_QP_LFO(2, QD_QL_WAVE)] = (int16_t)wave, q[QD_QP_LFO(2, QD_QL_TRIG)] = (int16_t)trig;
    q[QD_QP_LFO(2, QD_QL_PHASE)] = 0, q[QD_QP_LFO(2, QD_QL_FADE)] = 0;
    q[QD_QP_LFO(2, QD_QL_DEST)] = (int16_t)a_dest(to), q[QD_QP_LFO(2, QD_QL_DEPTH)] = (int16_t)depth;
    p->snd[t].edited = 1;
}
/* track t's sound: pool slot k's factory preset; flat: tuned 25 cents down */
static void a_tsound(at_t *g, int t, int k, int flat)
{
    qd_pattern_t *p = g->p;
    qd_sound_t f;
    if (!p) {
        a_used(g, t, -1, k);
        return;
    }
    qd_factory_preset(QD_ATRIUM_ID0 - 1 + QD_ATRIUM_POOL[k - 1], &f);
    memcpy(p->snd[t].patch, f.patch, sizeof p->snd[t].patch);
    memset(p->snd[t].name, 0, sizeof p->snd[t].name);
    strncpy(p->snd[t].name, f.name, sizeof p->snd[t].name - 1);
    p->snd[t].sound = (uint8_t)k, p->snd[t].edited = 0, p->snd[t].preset = f.preset;
    if (flat)
        a_lfo3(p, t, 1, 2, 1, "PITCH", -133);
}

static void a_hitn(at_t *g, int t, int s, int k, int vel, int n)
{
    if (g->on[t] >> s & 1u)
        return;
    a_n(g, t, s, n, vel, L2);
    a_snd(g, t, s, k);
}
static void a_hit(at_t *g, int t, int s, int k, int vel) { a_hitn(g, t, s, k, vel, 60); }

/* THE BEAT. fill 1: the last bar ends on three snares; 2: it stops on three, a snap; 3: the toms roll down */
static void a_drums(at_t *g, int fill)
{
    static const uint8_t TOM[8] = {60, 60, 57, 57, 53, 53, 48, 48};
    int b, s;
    a_trk(g, T_DR, A_LEN, 3);
    if (g->p)
        g->p->trk[T_DR].swing = 1;
    for (b = 0; b < 4; b++) {
        const int o = b * 16, last = b == 3;
        if (last && fill == 1) {
            a_hit(g, T_DR, o + 13, K_SNARE, 70), a_hit(g, T_DR, o + 14, K_SNARE, 88), a_hit(g, T_DR, o + 15, K_SNARE, 104);
            a_rat(g, T_DR, o + 15);
        }
        a_hit(g, T_DR, o, K_KICK, 122);
        a_hit(g, T_DR, o + 4, K_SNARE, 110);
        a_lk(g, T_DR, o + 4, QD_LK_REVERB, 40);
        a_hit(g, T_DR, o + 7, K_KICK, 96);
        if (last && fill == 2) {
            a_hit(g, T_DR, o + 8, K_KICK, 118);
            a_hit(g, T_DR, o + 15, K_SNAP, 100);
            continue;
        }
        if (last && fill == 3) {
            for (s = 0; s < 8; s++) {
                a_hitn(g, T_DR, o + 8 + s, K_TOM, 76 + s * 6, TOM[s]);
                a_lk(g, T_DR, o + 8 + s, QD_LK_REVERB, 60);
            }
            a_rat(g, T_DR, o + 15);
            continue;
        }
        a_hit(g, T_DR, o + 10, K_KICK, 112);
        a_hit(g, T_DR, o + 12, K_SNARE, 106);
        a_hit(g, T_DR, o + (b & 1 ? 9 : 15), K_RIM, 58);
    }
}

/* THE HATS: eighths, the off ones softer, two ghosts, the open one; shaker: the odd sixteenths. MIDI FX group 1 */
static void a_hats(at_t *g, int shaker)
{
    int s;
    a_trk(g, T_HAT, A_LEN, 4);
    if (g->p)
        g->p->trk[T_HAT].swing = 1, g->p->trk[T_HAT].mfx = 1;
    for (s = 0; s < A_LEN; s++) {
        const int q = s & 15;
        if (s == 54)
            a_hit(g, T_HAT, s, K_OPEN, 84);
        else if (!(s & 1))
            a_hit(g, T_HAT, s, s & 2 ? K_HAT : K_THAT, s & 2 ? 88 : 62);
        else if (shaker)
            a_hit(g, T_HAT, s, K_SHAKER, q == 3 || q == 11 ? 70 : 46);
        else if (q == 11 || (q == 15 && (s >> 4) & 1))
            a_hit(g, T_HAT, s, K_HAT, 44);
    }
}

/* THE AIR on the hats' track, before them: on bit 1 the radio, 2 the wind, 4 the cymbal */
static void a_air(at_t *g, int on)
{
    static const struct { uint8_t s, k, vel, len, dly, rev; int8_t pan; } A[] = {
        {21, X_RADIO, 64, LS8, 60, 60, 30}, {45, X_WIND, 70, LS16, 20, 80, -20}, {0, K_CYMBAL, 92, LS16, 30, 70, 0}};
    int i;
    a_trk(g, T_HAT, A_LEN, 4);
    for (i = 0; i < 3; i++) {
        if (!(on >> i & 1))
            continue;
        a_n(g, T_HAT, A[i].s, 60, A[i].vel, A[i].len);
        a_snd(g, T_HAT, A[i].s, A[i].k);
        a_lk(g, T_HAT, A[i].s, QD_LK_DELAY, A[i].dly);
        a_lk(g, T_HAT, A[i].s, QD_LK_REVERB, A[i].rev);
        a_lk(g, T_HAT, A[i].s, QD_LK_PAN, A[i].pan);
    }
}

/* THE HARMONY: a bar's bass root and its pad chord; each progression four bars of it */
enum { CS, AM, FA, LY, AB, BB, CM, AA, FS };
static const struct { uint8_t root, v[4]; } A_CH[] = {
    {36, {48, 55, 60, 65}}, {33, {45, 52, 60, 67}}, {29, {53, 57, 60, 67}}, {36, {48, 55, 62, 66}},
    {32, {44, 51, 60, 63}}, {34, {46, 53, 62, 65}}, {36, {48, 55, 60, 64}}, {33, {45, 52, 59, 61}},
    {30, {42, 49, 57, 64}}};
static const uint8_t A_H[3][4] = {{CS, LY, AM, FA}, {AB, BB, CM, LY}, {AA, FS, AA, AA}};   /* H_A, H_B, the coda's */

static void a_pad(at_t *g, const uint8_t *h)
{
    int b;
    a_trk(g, T_PAD, A_LEN, 8);
    for (b = 0; b < 4; b++)
        a_note(g, T_PAD, b * 16, A_CH[h[b]].v, 4, 84, LS16, 4);
}

/* THE BASS, with the kick; the fourth bar walks up; stop: the last bar ends on three */
static void a_bass(at_t *g, const uint8_t *h, int stop)
{
    int b;
    a_trk(g, T_BA, A_LEN, 1);
    if (g->p)
        g->p->trk[T_BA].swing = 1;
    for (b = 0; b < 4; b++) {
        const int o = b * 16, r = A_CH[h[b]].root;
        a_n(g, T_BA, o, r, 118, LS6);
        a_n(g, T_BA, o + 7, r, 92, LS2);
        if (b == 3 && stop) {
            a_n(g, T_BA, o + 8, r, 112, LS4);
            continue;
        }
        a_n(g, T_BA, o + 10, r, 108, LS4);
        if (b == 3)
            a_n(g, T_BA, o + 14, r + 2, 96, LS2);
    }
}

/* THE BELLS: five notes on twelve steps */
static void a_bells(at_t *g)
{
    static const uint8_t B[5][3] = {{0, 72, 84}, {3, 67, 66}, {6, 66, 72}, {8, 67, 60}, {10, 74, 76}};
    int i;
    a_trk(g, T_BELL, 12, 3);
    for (i = 0; i < 5; i++)
        a_n(g, T_BELL, B[i][0], B[i][1], B[i][2], LS2);
}

/* the lines: step, note, velocity, length each; one after another, each ended by 255: the bridge's fanfare, the tune,
 * its variation, the opening's bell, the coda's */
static const uint8_t A_MEL[] = {
    0, 67, 102, LS4, 4, 72, 104, LS4, 8, 76, 112, LS8, 16, 69, 104, LS4, 20, 74, 108, LS4, 24, 78, 116, LS8,
    32, 81, 120, LS12, 44, 79, 104, LS4, 48, 77, 108, LS4, 52, 76, 104, LS4, 56, 79, 112, LS8, 255,
    0, 72, 104, LS4, 4, 75, 106, LS4, 8, 80, 114, LS8, 16, 77, 106, LS4, 20, 82, 110, LS4, 24, 86, 118, LS8,
    32, 84, 122, LS12, 44, 79, 104, LS4, 48, 86, 114, LS4, 52, 84, 108, LS4, 56, 79, 110, LS8, 255,
    0, 72, 104, LS4, 4, 75, 106, LS4, 8, 80, 112, LS4, 12, 84, 116, LS4, 16, 77, 106, LS4, 20, 82, 110, LS4,
    24, 86, 116, LS4, 28, 89, 120, LS4, 32, 88, 124, LS12, 44, 84, 106, LS4, 48, 86, 114, LS4, 52, 88, 116, LS4,
    56, 84, 118, LS8, 255,
    8, 68, 84, LS8, 24, 74, 84, LS8, 32, 72, 90, LS16, 56, 67, 80, LS8, 255,
    8, 64, 84, LS8, 16, 69, 80, LS8, 24, 71, 84, LS8, 40, 76, 90, LS16, 255};
enum { M_A, M_B, M_B2, M_OPEN, M_CODA, M_FIG, M_NONE = 15 };
static void a_line(at_t *g, int t, int m, int limit, int up, int dvel)
{
    const uint8_t *x = A_MEL;
    while (m--)
        while (*x++ != 255)
            ;
    a_trk(g, t, A_LEN, limit);
    for (; *x != 255; x += 4)
        a_n(g, t, x[0], x[1] + up, x[2] + dvel, x[3]);
}

/* THE PATTERNS. f: the pad CHAPEL (else JUNO PAD), the hats' track the opening air, the air's radio, wind, cymbal,
 * the shaker, the bass's stop, a pad; dr: the drums' fill (0 no drums, air, hats or bass); h: the harmony; mel, bell:
 * the lead's and the bell's lines (M_FIG the bell figure), bup, bdv: the bell's octave and softness */
enum { F_CHAPEL = 1, F_AIRY = 2, F_SHAKER = 32, F_STOP = 64, F_PAD = 128 };   /* (4, 8, 16: the air's radio, wind, cymbal) */
static const struct { uint8_t f, dr, h, ml; int8_t bup, bdv; } A_PAT[QD_ATRIUM_N - 1] = {
    {F_CHAPEL | F_AIRY | F_PAD, 0, 1, M_NONE << 4 | M_OPEN, 0, 0},
    {F_CHAPEL | F_PAD, 3, 1, M_NONE << 4 | M_B, -12, 0},
    {F_CHAPEL | 16 | F_PAD, 1, 1, M_B << 4 | M_B, 12, -14},
    {F_CHAPEL | 8 | 16 | F_STOP | F_PAD, 2, 1, M_B2 << 4 | M_B2, 0, -24},
    {4, 1, 0, M_NONE << 4 | M_FIG, 0, 0},
    {16 | F_PAD, 3, 0, M_A << 4 | M_A, 12, -10},
    {F_CHAPEL | 16 | F_SHAKER | F_PAD, 1, 1, M_B << 4 | M_B, 12, -14},
    {F_CHAPEL | 8 | 16 | F_SHAKER | F_STOP | F_PAD, 2, 1, M_B2 << 4 | M_B2, 0, -24},
    {F_CHAPEL | F_AIRY | F_PAD, 0, 2, M_NONE << 4 | M_CODA, 0, 0}};

static void atrium(at_t *g, int k)
{
    qd_pattern_t *p = g->p;
    const int f = k < QD_ATRIUM_N - 1 ? A_PAT[k].f : F_CHAPEL, air = (f & F_AIRY) != 0;
    int t;
    /* the track sounds (the song's sounds()) */
    a_tsound(g, T_DR, K_KICK, 0);
    a_tsound(g, T_BA, P_SUB, 1);
    a_tsound(g, T_PAD, f & F_CHAPEL ? P_CHAPEL : P_PAD, 1);
    a_tsound(g, T_MEL, P_LEAD, 1);
    a_tsound(g, T_BELL, P_BELL, 1);
    if (air) {
        a_tsound(g, T_HAT, X_WIND, 0);
        if (p) {
            int16_t *q = p->snd[T_HAT].patch;
            q[QD_QP_FREQ] = 12700, q[QD_QP_EATK] = 100, q[QD_QP_EREL] = 60;
            a_lfo3(p, T_HAT, 1600, 4, 3, "FREQ", -6400);
        }
        a_trk(g, T_HAT, A_LEN, -1);
        a_n(g, T_HAT, 0, 60, 84, LS64);
        a_lk(g, T_HAT, 0, QD_LK_REVERB, 70);
    } else
        a_tsound(g, T_HAT, K_HAT, 0);
    if (p)
        p->bpm = 71, p->swing = 58, p->used = 1;
    if (k == QD_ATRIUM_N - 1) {                         /* A10 END: one bar, A held */
        for (t = 0; t < QD_PAT_NTRACK; t++)
            if (p)
                qd_track_set_length(p, t, 16);
        a_note(g, T_PAD, 0, A_CH[AA].v, 4, 80, LS16, 4);
        a_n(g, T_BELL, 0, 73, 72, LS8);
        if (p)
            p->master = 16;
        return;
    }
    {
        const int dr = A_PAT[k].dr, ml = A_PAT[k].ml;
        const uint8_t *h = A_H[A_PAT[k].h];
        if (dr) {
            a_drums(g, dr), a_air(g, f >> 2 & 7), a_hats(g, f & F_SHAKER), a_bass(g, h, f & F_STOP);
        }
        if (f & F_PAD)
            a_pad(g, h);
        if ((ml & 15) == M_FIG)
            a_bells(g);
        if (ml >> 4 != M_NONE)
            a_line(g, T_MEL, ml >> 4, 2, 0, 0);
        if ((ml & 15) < M_FIG)
            a_line(g, T_BELL, ml & 15, 3, A_PAT[k].bup, A_PAT[k].bdv);
    }
}

/* ---------------------------------------------------------------- the demos --- */
int qd_demo_n(int kind)
{
#ifdef QD_DEMO_HOST
    if (kind == QD_DEMO_OLD)
        return qd_demo_old_on ? QD_DEMO_N : 0;
#endif
    return kind == QD_DEMO_ATRIUM ? QD_ATRIUM_N : 0;
}

void qd_demo_default(qd_pattern_t *p, int kind, int k)
{
    qd_pat_init(p);
    if (k < 0 || k >= qd_demo_n(kind))
        return;
#ifdef QD_DEMO_HOST
    if (kind == QD_DEMO_OLD) {
        qd_demo_pattern(p, k);
        return;
    }
#endif
    {
        at_t g;
        memset(&g, 0, sizeof g);
        g.p = p;
        atrium(&g, k);
    }
}

int qd_demo_locks(int kind, int k, int snd, void (*fn)(void *ctx, int t, int s), void *ctx)
{
    if (k < 0 || k >= qd_demo_n(kind))
        return 0;
#ifdef QD_DEMO_HOST
    if (kind == QD_DEMO_OLD) {                          /* (the host's: made, and looked at) */
        static qd_pattern_t P;
        int t, s, n = 0;
        qd_demo_pattern(&P, k);
        for (t = 0; t < QD_PAT_NTRACK; t++) {
            for (s = 0; s < QD_NSTEP; s++)
                if (P.step[t][s].sound == snd && (P.step[t][s].flags & QD_SF_ON)) {
                    n++;
                    if (fn)
                        fn(ctx, t, s);
                }
            if (P.snd[t].sound == snd) {
                n++;
                if (fn)
                    fn(ctx, t, -1);
            }
        }
        return n;
    }
#endif
    {
        at_t g;
        memset(&g, 0, sizeof g);
        g.snd = snd, g.fn = fn, g.ctx = ctx;
        atrium(&g, k);
        return g.n;
    }
}

void qd_demo_load(qd_project_t *pr, int kind)
{
    const int n = qd_demo_n(kind);
    int k;
    pr->demo = (uint8_t)kind;                           /* (P2.1: the patterns' defaults are the demo's) */
    if (kind == QD_DEMO_ATRIUM) {                       /* (its MIDI FX group 1, the hats': their velocities moved) */
        qd_mfx_slot_t *m = pr->mfx[0].slot;
        qd_mfx_slot_init(&m[0], QD_MFX_RANDOM);
        m[0].v[QD_MD_VEL] = 36, m[0].v[QD_MD_CHANCE] = 70;
    }
    for (k = 0; k < n; k++) {
#if QD_PAT_RAM < QD_NPAT
        if (!qd_proj_resident(pr, k)) {                 /* (out of RAM: its default is the demo now; a record in the
                                                         * store still wins when it is read in) */
            pr->pat_used[k] = 1;
            continue;
        }
#endif
        qd_demo_default(qd_proj_pat(pr, k), kind, k);
    }
}
