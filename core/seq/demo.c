/* SPDX-License-Identifier: GPL-3.0-only */
/* The built-in demo patterns (demo.h).
 *
 * THE DRUMS (track 1's sound locks and p-locks). Designed by rendering one voice of each and measuring it (the attack,
 * the decay to -20 and -40 dB, the spectral centroid over 0..30 and 30..100 ms, the pitch from zero crossings; the
 * numbers are render_audio's demo.txt, made the same way):
 *   kick    INIT (sound 1: a sine on operator C) at A1, AMP DEC 32 SUS 0 REL 20; LFO 1 on PITCH, EXP wave, HALF trig
 *           (one fall, then it holds), SPEED 40.00 x F64 (a 100 ms cycle at any tempo), DEPTH 63.99 (one octave): the
 *           pitch falls from 100 to 62 Hz in 0.1 s; A LEV 60 with A DEC 4, RATIO A 2.00: a click of FM on the attack;
 *           LEVEL 127 and a mix LEVEL lock of 118 (the drums sit over the track's 104; the snare and clap 114).
 *           -20 dB at 74 ms, -40 dB at 180 ms; centroid 118 Hz on the attack, 68 Hz after
 *   snare   NOISE-ISH (sound 17: operator A at full feedback) at G3, its high-pass swapped for LP12 at 95.00, RESO 10,
 *           FDBK 110, AMP DEC 24 SUS 0 REL 18: centroid 2.3 kHz, -20 dB at 68 ms, -40 dB at 172 ms
 *   clap    the same noise, LP12 at 105.00 RESO 30, FDBK 120, AMP DEC 14 SUS 0 REL 12, as a three-note chord (E4 F4
 *           F#4) strummed 2/24 of a step: three bursts about 10 ms apart; centroid 2.9 kHz
 *   ghost   the snare quieter (velocity 36..48, LEVEL 70) and darker (LP12 at 80.00), AMP DEC 16: centroid 1.1 kHz,
 *           -40 dB at 64 ms
 *   hats    NOISE-ISH's own HP12 raised to 95.00 (closed: AMP DEC 10 REL 8, -40 dB at 36 ms; open: 90.00, DEC 32 REL 26,
 *           -20 dB at 255 ms), at C7; centroid 9 kHz; panned a little right (a PAN lock), no reverb on the closed */
#include <string.h>

#include "demo.h"

static void lk(qd_pattern_t *p, int t, int s, int id, int v) { qd_lock_set(p, t, s, id, v); }

static void note(qd_pattern_t *p, int t, int s, int n, int vel, int len)
{
    const uint8_t x = (uint8_t)n;
    qd_step_set_trig(p, t, s, QD_TRIG_NOTE);
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
    qd_step_set_trig(p, t, s, QD_TRIG_NOTE);
    qd_step_set_notes(p, t, s, x, n);
    qd_step_set_vel(p, t, s, vel);
    qd_step_set_len(p, t, s, len);
    qd_step_set_strum(p, t, s, strum);
}

static void tie(qd_pattern_t *p, int t, int s, int n, int vel, int len)
{
    note(p, t, s, n, vel, len);
    qd_step_set_trig(p, t, s, QD_TRIG_TIE);
}

/* the lengths by name (pattern.h's table) */
enum { L8 = 0, L4 = 1, L2 = 2, L34 = 3, L1 = 4, L15 = 5, LS2 = 6, LS3 = 7, LS4 = 8, LS6 = 9, LS8 = 10 };

/* ---------------------------------------------------------------- the drums (track 1) --- */
#define SND_INIT 1
#define SND_NOISE 17

static void kick(qd_pattern_t *p, int s, int vel)
{
    note(p, 0, s, 33, vel, L2);
    qd_step_set_sound(p, 0, s, SND_INIT);
    lk(p, 0, s, QD_QP_EDEC, 32);
    lk(p, 0, s, QD_QP_ESUS, 0);
    lk(p, 0, s, QD_QP_EREL, 20);
    lk(p, 0, s, QD_QP_LFO(0, QD_QL_SPEED), 4000);
    lk(p, 0, s, QD_QP_LFO(0, QD_QL_MULT), 18);         /* F64: x 64 at a fixed 120 BPM */
    lk(p, 0, s, QD_QP_LFO(0, QD_QL_DEST), 1);          /* PITCH */
    lk(p, 0, s, QD_QP_LFO(0, QD_QL_WAVE), 5);          /* EXP */
    lk(p, 0, s, QD_QP_LFO(0, QD_QL_TRIG), 4);          /* HALF */
    lk(p, 0, s, QD_QP_LFO(0, QD_QL_DEPTH), 6399);
    lk(p, 0, s, QD_QP_ALEV, 60);
    lk(p, 0, s, QD_QP_ADEC, 4);
    lk(p, 0, s, QD_QP_AEND, 0);
    lk(p, 0, s, QD_QP_RA, 8);                          /* RATIO A 2.00 */
    lk(p, 0, s, QD_QP_LEVEL, 127);
    lk(p, 0, s, QD_LK_LEVEL, 118);                     /* (the drums a little over the track's mix level) */
    lk(p, 0, s, QD_LK_REVERB, 4);
}

static void noise_body(qd_pattern_t *p, int s, int freq, int reso, int fdbk, int dec, int rel)
{
    qd_step_set_sound(p, 0, s, SND_NOISE);
    lk(p, 0, s, QD_QP_FTYPE, QD_QF_LP12);
    lk(p, 0, s, QD_QP_FREQ, freq);
    lk(p, 0, s, QD_QP_RESO, reso);
    lk(p, 0, s, QD_QP_FDBK, fdbk);
    lk(p, 0, s, QD_QP_EDEC, dec);
    lk(p, 0, s, QD_QP_ESUS, 0);
    lk(p, 0, s, QD_QP_EREL, rel);
}

static void snare(qd_pattern_t *p, int s, int vel)
{
    note(p, 0, s, 55, vel, L2);
    noise_body(p, s, 9500, 1000, 110, 24, 18);
    lk(p, 0, s, QD_QP_LEVEL, 120);
    lk(p, 0, s, QD_LK_LEVEL, 114);
}

static void clap(qd_pattern_t *p, int s, int vel)
{
    chord(p, 0, s, 64, 65, 66, 0, vel, L4, 2);
    noise_body(p, s, 10500, 3000, 120, 14, 12);
    lk(p, 0, s, QD_LK_REVERB, 40);
    lk(p, 0, s, QD_LK_LEVEL, 114);
}

static void ghost(qd_pattern_t *p, int s, int vel)
{
    note(p, 0, s, 55, vel, L4);
    noise_body(p, s, 8000, 0, 110, 16, 12);
    lk(p, 0, s, QD_QP_LEVEL, 70);
}

static void hat(qd_pattern_t *p, int s, int vel, int open)
{
    note(p, 0, s, 96, vel, open ? L1 : L4);
    qd_step_set_sound(p, 0, s, SND_NOISE);
    lk(p, 0, s, QD_QP_FREQ, open ? 9000 : 9500);
    lk(p, 0, s, QD_QP_RESO, 1000);
    lk(p, 0, s, QD_QP_EDEC, open ? 32 : 10);
    lk(p, 0, s, QD_QP_ESUS, 0);
    lk(p, 0, s, QD_QP_EREL, open ? 26 : 8);
    lk(p, 0, s, QD_LK_PAN, 14);
    if (!open)
        lk(p, 0, s, QD_LK_REVERB, 0);
}

static void beat(qd_pattern_t *p)
{
    qd_tset_t *d = &p->trk[0];
    d->length = 16;
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
    b->length = 32;
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
    k->rate = QD_RATE_1_2;
    k->length = 16;
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
    qd_step_set_trig(p, 2, 14, QD_TRIG_NOTE);
    /* LEAD: 24 steps (a bar and a half against the others' two), ratchets and chance */
    l->limit = 2;
    l->length = 24;
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
    static const int RATE[8] = {QD_RATE_1, QD_RATE_1, QD_RATE_1_2, QD_RATE_3_2, QD_RATE_2, QD_RATE_1_4, QD_RATE_4_3,
                                QD_RATE_2_3};
    static const int LEN[8] = {16, 12, 7, 10, 9, 5, 11, 6};
    int t;
    p->bpm = 118;
    for (t = 0; t < 8; t++) {
        p->trk[t].rate = (uint8_t)RATE[t];
        p->trk[t].length = (uint8_t)LEN[t];
    }
    {   /* the limits: each track takes its own voices before another's (16 in all, about) */
        static const uint8_t LIM[8] = {3, 1, 3, 2, 3, 3, 2, 2};
        for (t = 0; t < 8; t++)
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
    /* BELL 11 at 4/3x */
    note(p, 6, 0, 81, 70, L2);
    note(p, 6, 3, 86, 60, L2);
    note(p, 6, 5, 88, 64, L2);
    note(p, 6, 8, 84, 58, L2);
    /* NOISE 6 at 2/3x */
    note(p, 7, 0, 60, 70, L4);
    note(p, 7, 2, 72, 50, L4);
    note(p, 7, 3, 67, 60, L4);
    lk(p, 7, 3, QD_LK_REVERB, 60);
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

void qd_demo_load(qd_project_t *pr)
{
    int k;
    for (k = 0; k < QD_DEMO_N; k++)
        qd_demo_pattern(&pr->pat[k], k);
}
