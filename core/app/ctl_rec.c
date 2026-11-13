/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The keyboard's recording and MIDI notes into a held step (ctl.h; PLAN §8b "Keyboard and recording").
 *
 * THE KEYBOARD (REC tapped: the KEYS screen): the keybed plays the selected track, F3..G5 shifted by OCT- / OCT+; a
 * track in a scale has every key snapped down onto the scale (its white keys in C major, say, play themselves).
 * Recording is armed by SHIFT + REC (or REC + PLAY); stopped it is step recording, playing it is live recording.
 * P1.3x (§8b "PLAY, F3 and live recording"): REC HELD arms LIVE recording; stopped, nothing moves until the first note,
 * which starts playback and recording together (the note on the loop's first step, offset 0). An EMPTY track (no trig
 * on any page) grows as it is recorded: its loop starts as page 1, and the next page joins the loop as the playhead
 * nears the loop's end; at the recording's end the loop is the pages reached; past page 8 it loops and records over
 * what is there. REC or PLAY while recording ends it; playback goes on. Notes land on the nearest step at the track's
 * rate (base and type), their offset as micro-timing (24ths of that step), their length from the hold.
 * THE TAKE (2026-11-13, the owner: armed while stopped it is always a CAPTURE; armed while playing the record settings'
 * KNOB 3 TAKE, OVERDUB (the default) or REPLACE). CAPTURE: armed, nothing changes until
 * the first note (or PLAY): then the track is cleared (steps, locks, pages: page 1, every page 16 long; its rate
 * stays) and the take runs from the top (playing: the transport starts again with it), the loop growing as above; PLAY
 * or REC ends it: the track's length is the take's, to the nearest step's edge (36 steps: pages 1..3, page 3 4 long)
 * and its loop starts again there, playback going on. REPLACE: a note replaces the notes of the step it lands on;
 * OVERDUB: it joins them. Stopped and armed, the first note starts playback (REPLACE / OVERDUB too), on the loop's
 * first step; so does PLAY, or PLAY pressed while REC is held (REC + PLAY).
 *
 * STEP RECORDING (armed, the transport stopped): a chord played goes into the cursor's step (the notes struck while
 * any is still down are one chord, the step cleared first, the first note's velocity, the track's default length);
 * when the last of them is let go the cursor moves on, round the track's loop (its pages, each from its start to its
 * end). OCT+ is a rest (the cursor's step cleared, the cursor on), OCT- a step back, PRESETS moves the cursor.
 *
 * LIVE RECORDING (armed, playing): a note goes into the step nearest the playhead of the track, with the micro-timing
 * it was played with (-12..+12 24ths); notes struck while another recorded note is still down on the same step are its
 * chord; its length is how long it was held (the nearest of the length table). The step is cleared first (the take
 * replaces it). Knob turns (KNOB 1..4: the quick page) are written as locks on the step playing.
 *
 * MIDI: notes on the selected track's channel do the same in the keyboard; with steps held in the step view they
 * become the held steps' notes (the notes struck while any is down are one chord). 2026-11-10 (owner): on the
 * keyboard with the step latch's selection (B11 + W, ALGORITHM) the keys and MIDI notes become the selected steps'
 * notes the same way (played as ever; not while REC is armed: the recording's). */
#include <math.h>
#include <string.h>

#include "app.h"
#include "ui.h"
#include "../seq/chord.h"
#include "../seq/scale.h"
#include "../seq/sounds.h"

static qd_pattern_t *PAT(qd_app_t *a) { return qd_ctl_pat(a); }
static int tlen(qd_app_t *a) { return qd_track_len(&PAT(a)->trk[a->sel]); }
static int loop_step(qd_app_t *a, int pos) { return qd_track_loop_step(&PAT(a)->trk[a->sel], pos); }

int qd_rec_key_note(const qd_app_t *a, int key)
{
    int sc, rt, n = QD_NOTE0 + key + 12 * a->oct;
    qd_track_key(&a->proj, qd_ctl_cpat(a), a->sel, &sc, &rt);
    if (sc != QD_SCALE_CHROMATIC)
        n = qd_scale_snap(sc, rt, n);
    return n < 0 ? 0 : n > 127 ? 127 : n;
}

/* the nearest length of the table to n / d eighths of a step (never INF), nearest as a ratio (in log: the table's
 * lengths are a ratio apart, so the boundary between two is their geometric mean). P2.4: integer: e^2 against the two
 * lengths' product, e in Q16 (at least half an eighth, at most 2048 eighths: past the last boundary) */
static int len_index(int64_t n, int64_t d)
{
    int64_t eq;
    int i;
    if (n >= 2048 * d)
        return QD_LEN_INF - 1;
    eq = 2 * n < d ? 32768 : qd_div64(n * 65536, (int32_t)d);
    for (i = 0; i + 1 < QD_LEN_INF; i++)
        if (eq * eq < ((int64_t)qd_len_eighths(i) * qd_len_eighths(i + 1)) << 32)
            return i;
    return QD_LEN_INF - 1;
}

/* a live note ends: its length into its step (the chord's longest note wins) */
static void live_off(qd_app_t *a, int note, int64_t now)
{
    qd_ui_t *u = &a->ui;
    const int64_t L = qd_rate_ticks(qd_tset_rate(&PAT(a)->trk[a->sel]));
    const int s = u->rec_s[note];
    const int li = len_index((now - u->rec_t0[note]) * 8, L);
    qd_rstep_t r;
    int i;
    qd_step_resolve(PAT(a), a->sel, s, &r);
    if (r.on && li > r.len)
        qd_step_set_len(PAT(a), a->sel, s, li);
    for (i = 0; i < u->nroll; i++)
        if (u->roll[i].note == note && u->roll[i].t1 < 0)
            u->roll[i].t1 = now;
}

static void chord_add(uint8_t *chord, int *n, int note)
{
    int i;
    if (*n >= QD_NNOTE)
        return;
    for (i = 0; i < *n; i++)
        if (chord[i] == note)
            return;
    if (*n && (note - chord[0] > 48 || chord[0] - note > 48))
        return;
    chord[(*n)++] = (uint8_t)note;
}

int qd_rec_stepping(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    return u->rec && !u->live_wait && (u->rec_kind == QD_REC_STEP || !a->seq.playing);
}

void qd_rec_toggle(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (u->rec) {                                       /* off: the notes still down end now */
        qd_rec_key(a, -1, 0, 0);
        u->rec = 0;
        return;
    }
    u->rec = 1;
    u->cap = 0;
    u->rec_kind = QD_REC_LIVE;
    u->rec_n = 0;
    u->rec_step = -1;
    {   /* step recording starts on the page edited (its first step in the loop), else the loop's top */
        const qd_tset_t *ts = &PAT(a)->trk[a->sel];
        const int pg = u->page[a->sel], pos = qd_track_step_pos(ts, pg * QD_PAGE + ts->pstart[pg]);
        u->cursor = pos < 0 ? 0 : pos;
    }
}

static int track_empty(qd_app_t *a)
{
    int s;
    for (s = 0; s < QD_NSTEP; s++)
        if (qd_step_is_on(qd_step(PAT(a), a->sel, s)))
            return 0;
    return 1;
}

void qd_rec_arm_live(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (!u->rec)
        qd_rec_toggle(a);
    u->live_wait = !a->seq.playing;
    u->cap = !a->seq.playing;                           /* (2026-11-13, owner: armed stopped, always a CAPTURE: nothing
                                                         * changes until the first note; armed playing, the TAKE's
                                                         * OVERDUB or REPLACE) */
    if (!u->cap && track_empty(a)) {                    /* an empty track: page 1, growing */
        qd_page_set_loop(PAT(a), a->sel, 1);
        u->rec_grow = 1;
        u->grow_reached = 0;
    }
    u->rec_kind = QD_REC_LIVE;
    qd_ctl_confirm(a, u->cap ? "CAPTURE" : "LIVE REC", 0);
}

/* armed (stopped, or CAPTURE waiting): the recording starts now. CAPTURE: the track cleared and growing from page 1,
 * the transport from the top (playing: started again, every track from its first step with the take). skip: a key
 * started it, on the first step: the sequencer does not play that step again (the key sounds it) */
void qd_rec_go(qd_app_t *a, int skip)
{
    qd_ui_t *u = &a->ui;
    u->live_wait = 0;
    if (u->cap) {
        qd_undo_hint(a, QD_UK_REC);
        qd_track_clear(PAT(a), a->sel);
        u->rec_grow = 1;
        u->grow_reached = 0;
        u->cap = 2;
        u->cap_full = 0;
        u->cap_hi = -1;
        if (a->seq.playing)                             /* (the sequencer's notes end; the keys' sound on) */
            qd_seq_stop(&a->seq, qd_player_event, &a->player);
    } else if (a->seq.playing) {
        return;
    }
    qd_app_play(a);
    if (skip)
        qd_seq_track_from(&a->seq, a->sel, 0, 1);
}

/* P1.3y: REC held with the record settings' STEP: step recording armed (the cursor; playing or not) */
void qd_rec_arm_step(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (!u->rec)
        qd_rec_toggle(a);
    u->rec_kind = QD_REC_STEP;
    u->live_wait = 0;
    u->cap = 0;
    qd_ctl_confirm(a, "STEP REC", 0);
}

/* an empty track's loop: the pages reached (from page 1) */
static void grow_trim(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (!u->rec_grow)
        return;
    qd_page_set_loop(PAT(a), a->sel, (1 << (u->grow_reached + 1)) - 1);
    u->rec_grow = 0;
}

/* CAPTURE's end: the track as long as the take, to the nearest step's edge and at least to its last note (pages 1..k,
 * the last one cut); its loop from its start again there (the steps past it, resolved already, dropped) */
static void cap_end(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    int64_t pn, pd;
    int n;
    if (u->cap_full || !a->seq.playing)
        return;                                         /* (past page 8: all 128; stopped: the pages reached) */
    qd_seq_track_pos_q(&a->seq, a->sel, &pn, &pd);
    pn -= qd_seq_tick(&a->seq) - u->cap_press;          /* (where REC or PLAY went down: in ticks, pd a step's) */
    n = pn > 0 ? (int)qd_div64(2 * pn + pd, (int32_t)(2 * pd)) : 0;
    n = n <= u->cap_hi ? u->cap_hi + 1 : n < 1 ? 1 : n;
    qd_track_set_length(PAT(a), a->sel, n);
    u->rec_grow = 0;
    qd_seq_track_from(&a->seq, a->sel, n, 0);
}

void qd_rec_end(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (u->rec)
        qd_rec_toggle(a);
    u->live_wait = 0;
    if (u->cap == 2)
        cap_end(a);
    u->cap = 0;
    grow_trim(a);
}

void qd_rec_ms(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    const qd_tset_t *ts = &PAT(a)->trk[a->sel];
    int pp, k;
    int64_t pn, pd;
    if (u->cap == 2 && (!u->rec || !a->seq.playing))
        u->cap = 0;                                     /* (a take ended another way: the pages reached) */
    if (!u->rec_grow)
        return;
    if (!u->rec || (!a->seq.playing && !u->live_wait)) {
        grow_trim(a);                                   /* (the recording or the transport ended another way) */
        return;
    }
    if (!a->seq.playing)
        return;
    pp = qd_ctl_playing_page(a, a->sel);
    if (pp < u->grow_reached)
        u->cap_full = 1;                                /* (round from page 8: a take of 128) */
    if (pp > u->grow_reached)
        u->grow_reached = pp;
    for (k = 0; k < 8 && ((qd_track_loop(ts) >> k) & 1); k++)
        ;
    qd_seq_track_pos_q(&a->seq, a->sel, &pn, &pd);
    if (k < 8 && pn >= (int64_t)(qd_track_len(ts) - 4) * pd)
        qd_page_set_loop(PAT(a), a->sel, qd_track_loop(ts) | 1 << k);   /* the next page joins before the wrap (the
                                                                        * steps are resolved two ahead) */
}

void qd_rec_cursor(qd_app_t *a, int d)
{
    qd_ui_t *u = &a->ui;
    const int n = tlen(a);
    u->cursor = ((u->cursor + d) % n + n) % n;
    u->page[a->sel] = loop_step(a, u->cursor) / QD_PAGE;
}

void qd_rec_rest(qd_app_t *a)
{
    qd_step_clear(PAT(a), a->sel, loop_step(a, a->ui.cursor));
    qd_rec_cursor(a, 1);
}

/* a note down / up while recording (note -1, up: every note still down ends) */
void qd_rec_key(qd_app_t *a, int note, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    qd_pattern_t *p = PAT(a);
    const int t = a->sel;
    if (note < 0) {
        int n;
        const int64_t now = qd_seq_tick(&a->seq);
        for (n = 0; n < 128; n++)
            if (u->rec_t0[n] >= 0) {
                if (u->rec_live)
                    live_off(a, n, now);
                u->rec_t0[n] = -1;
            }
        u->rec_n = 0;
        u->rec_live = 0;
        return;
    }
    if (!u->rec)
        return;
    if (down && (u->cap == 1 || (u->live_wait && !a->seq.playing)))
        qd_rec_go(a, 1);                                /* armed: this note starts it, on the first step (P1.6g: any
                                                         * view's notes) */
    if (!a->seq.playing && u->live_wait)
        return;
    qd_undo_hint(a, QD_UK_REC);                         /* (P1.5c: a recorded take: one undo level) */
    note &= 127;
    if (!down) {
        if (u->rec_t0[note] < 0)
            return;
        if (u->rec_live)
            live_off(a, note, qd_seq_tick(&a->seq));
        u->rec_t0[note] = -1;
        if (u->rec_n > 0 && --u->rec_n == 0) {
            if (!u->rec_live)
                qd_rec_cursor(a, 1);                    /* step recording: the chord is in, the cursor moves on */
            u->rec_live = 0;
        }
        return;
    }
    if (qd_rec_stepping(a)) {                           /* step recording */
        const int cs = loop_step(a, u->cursor);
        const int first = !u->rec_n;
        const int dub = first && a->proj.rec_dub == QD_TAKE_OVERDUB && qd_step_is_on(qd_step(p, t, cs));
        if (first) {
            u->rec_nchord = 0;
            if (dub) {                                  /* P1.3y: OVERDUB: the step's notes (and values) stay */
                qd_rstep_t r;
                int k;
                qd_step_resolve(p, t, cs, &r);
                for (k = 0; k < r.nn; k++)
                    chord_add(u->rec_chord, &u->rec_nchord, r.notes[k]);
            } else {
                qd_step_clear(p, t, cs);
            }
        }
        chord_add(u->rec_chord, &u->rec_nchord, note);
        qd_step_set_notes(p, t, cs, u->rec_chord, u->rec_nchord);
        if (first && !dub)
            qd_step_set_vel(p, t, cs, vel);
        u->rec_t0[note] = 0;
        u->rec_n++;
        return;
    }
    {   /* live recording: the step nearest the playhead, the micro-timing as played */
        const int n = tlen(a);
        const int64_t now = qd_seq_tick(&a->seq);
        int64_t pn, pd, fm;
        int lp, s, micro;
        qd_seq_track_pos_q(&a->seq, t, &pn, &pd);       /* (P2.4: in integers: the nearest step and how far from it in
                                                         * 24ths, rounded half away from zero) */
        lp = (int)qd_div64(2 * pn + pd, (int32_t)(2 * pd));
        fm = (pn - (int64_t)lp * pd) * 24;
        micro = fm >= 0 ? (int)qd_div64(2 * fm + pd, (int32_t)(2 * pd)) : -(int)qd_div64(-2 * fm + pd, (int32_t)(2 * pd));
        if (lp >= n)
            lp -= n;
        s = loop_step(a, lp);
        {   /* P1.3y: QUANTIZE (100 %: on the step), rounded half away from zero */
            const int q = micro * (100 - a->proj.rec_quant);
            micro = q >= 0 ? (2 * q + 100) / 200 : -((-2 * q + 100) / 200);
        }
        micro = micro < -23 ? -23 : micro > 23 ? 23 : micro;
        if (u->cap == 2 && lp > u->cap_hi)
            u->cap_hi = (int16_t)lp;
        if (u->rec_n && s == u->rec_step) {             /* struck while the chord's notes are down: the chord */
            chord_add(u->rec_chord, &u->rec_nchord, note);
            qd_step_set_notes(p, t, s, u->rec_chord, u->rec_nchord);
        } else if (a->proj.rec_dub == QD_TAKE_OVERDUB && qd_step_is_on(qd_step(p, t, s))) {   /* P1.3y: OVERDUB: the
                                                         * note joins the step's (its velocity, timing and length
                                                         * stay) */
            qd_rstep_t r;
            int k;
            qd_step_resolve(p, t, s, &r);
            u->rec_nchord = 0;
            for (k = 0; k < r.nn; k++)
                chord_add(u->rec_chord, &u->rec_nchord, r.notes[k]);
            chord_add(u->rec_chord, &u->rec_nchord, note);
            qd_step_set_notes(p, t, s, u->rec_chord, u->rec_nchord);
            u->rec_step = s;
        } else {
            u->rec_nchord = 0;
            chord_add(u->rec_chord, &u->rec_nchord, note);
            qd_step_clear(p, t, s);
            qd_step_set_notes(p, t, s, u->rec_chord, 1);
            qd_step_set_vel(p, t, s, vel);
            qd_step_set_micro(p, t, s, micro);
            qd_step_set_len(p, t, s, 0);                /* (the shortest until the note ends) */
            u->rec_step = s;
        }
        if (u->rec_t0[note] < 0)
            u->rec_n++;
        u->rec_t0[note] = now;
        u->rec_s[note] = (int16_t)s;
        u->rec_live = 1;
        u->roll[u->iroll].t0 = now;                     /* the roll (line.html s18) */
        u->roll[u->iroll].t1 = -1;
        u->roll[u->iroll].note = (uint8_t)note;
        u->iroll = (u->iroll + 1) % QD_NROLL;
        if (u->nroll < QD_NROLL)
            u->nroll++;
    }
}

/* ---------------------------------------------------------------- P1.6d: automation recording */
int qd_auto_on(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    if (!a->seq.playing || u->mode == QD_MODE_IDLE || qd_type_open(a))
        return 0;
    if (((a->buttons_held >> QD_B_REC) & 1u) && !qd_ctl_shift(a) && u->latch != QD_LATCH_REC)
        return 1;                                       /* REC held: any view */
    return u->rec && !u->live_wait && u->cap != 1 && !qd_rec_stepping(a) && a->proj.rec_auto;   /* recording (P1.6g:
                                                         * any view), AUTOMATION ON (not a CAPTURE armed) */
}

static int auto_step_now(qd_app_t *a)                 /* the step under the selected track's playhead */
{
    return loop_step(a, qd_seq_track_pos_i(&a->seq, a->sel) % tlen(a));
}

int qd_auto_cur(const qd_app_t *a, int id)
{
    const qd_ui_t *u = &a->ui;
    int16_t v;
    int s;
    if (id < 0 || id >= QD_LK_N)
        return 0;
    if (u->auto_take && ((u->auto_has[id >> 5] >> (id & 31)) & 1u))
        return u->auto_v[id];
    s = qd_track_loop_step(&qd_ctl_cpat(a)->trk[a->sel],
                           qd_seq_track_pos_i(&a->seq, a->sel) % qd_track_len(&qd_ctl_cpat(a)->trk[a->sel]));
    return qd_lock_get(qd_ctl_cpat(a), a->sel, s, id, &v) ? v : qd_step_base((qd_app_t *)a, a->sel, s, id);
}

int qd_auto_put(qd_app_t *a, int id, int value)
{
    qd_ui_t *u = &a->ui;
    qd_pattern_t *p = PAT(a);
    const int t = a->sel;
    int16_t lo, hi;
    int s;
    if (!qd_auto_on(a) || id < 0 || id >= QD_LK_N || !qd_lock_range(id, &lo, &hi))
        return 0;
    value = value < lo ? lo : value > hi ? hi : value;
    if (!u->auto_take) {                                /* a take starts (a level of its own: the gesture before it
                                                         * closes) */
        u->auto_take = 1;
        memset(u->auto_has, 0, sizeof u->auto_has);
        if (a->undo.open && a->undo.hint != QD_UK_REC)
            qd_undo_close(a);
    }
    qd_undo_hint(a, QD_UK_REC);                         /* (one undo level: the take's) */
    s = auto_step_now(a);
    if (qd_lock_set(p, t, s, id, value) == QD_EFULL) {
        qd_ctl_confirm(a, "NO ROOM", "for locks: the pattern has 1024");
        return 1;
    }
    if (!qd_step_is_on(qd_step(p, t, s)))               /* an empty step: a ghost (a LOCK trig) holds it */
        qd_step_set_trig(p, t, s, QD_TRIG_LOCK);
    u->auto_has[id >> 5] |= 1u << (id & 31);
    u->auto_v[id] = (int16_t)value;
    u->auto_step = s;
    u->auto_knob = u->last_knob;
    u->auto_ms = a->ms ? a->ms : 1u;
    if ((a->buttons_held >> QD_B_REC) & 1u)
        u->rec_auto_used = 1;
    qd_player_voice_lock(&a->player, t, id, value);   /* (heard now) */
    qd_app_fm_auto(a, t, id, value);                    /* (2026-11-10: FM+MIDI's CC now) */
    return 1;
}

void qd_auto_ms(qd_app_t *a)
{
    if (a->ui.auto_take && !qd_auto_on(a))
        a->ui.auto_take = 0;
}

/* automation: KNOB 1..4 (the quick page) as locks on the step playing (the keyboard, the step view: P1.6d) */
int qd_rec_knob(qd_app_t *a, int knob, int d)
{
    int id;
    if (knob < QD_K_1 || qd_msnd_is(a->model.t[a->sel].patch))   /* (FIZZ MIDI (e): a MIDI track has no FREQ ..) */
        return 0;
    id = qd_step_quick_id(knob - QD_K_1);
    if (!qd_auto_put(a, id, qd_auto_cur(a, id) + qd_step_id_turn(a, id, d)))
        return 0;
    qd_ctl_takeover(a, UI_TK_FREQ + (knob - QD_K_1), knob - QD_K_1, d, 1, a->ui.auto_step);
    return 1;
}

/* a MIDI note on the selected track's channel: into the held steps, or the recording. 2026-11-10 (owner): the
 * keyboard's keys too, and on the keyboard the step latch's selection takes them (not while recording): each chord
 * (the first note after all were up) replaces the selected steps' notes, one undo level a chord */
void qd_rec_midi_note(qd_app_t *a, int note, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    const int keys = (u->nheld || u->wk_pending) && !u->rec && ((u->mode == QD_MODE_KEYS && !u->latch) ||
                                                                 qd_snd_keys(a));   /* (a step held there too: its W
                                                         * keys the steps, a B11 click; 2026-11-10: a B11 click's keys
                                                         * on the other pages, armed recording first) */
    if (keys || (u->mode == QD_MODE_STEP && (u->nheld || u->wk_pending) && !(qd_snd_keys(a) && u->rec))) {
        qd_step_promote_all(a);
        if (down) {
            if (!u->midi_n) {
                u->midi_nchord = 0;
                if (keys)
                    qd_undo_hint(a, QD_UK_EDIT);        /* (a chord a level, however quick the next) */
            }
            chord_add(u->midi_chord, &u->midi_nchord, note);
            qd_step_notes_in(a, u->midi_chord, u->midi_nchord);
            u->midi_n++;
        } else if (u->midi_n > 0) {
            u->midi_n--;
        }
        return;
    }
    if (!down && u->midi_n > 0)
        u->midi_n--;
    qd_rec_key(a, note, down, vel);                     /* (P1.6g: recording, in any view) */
}

/* ---------------------------------------------------------------- 2026-11-12: the orange keys' values (ctl.h qd_mk_*;
 * the owner's mockups/fizz-keys-modes.html). The keys light the value of the step they show: the selection's first,
 * else the track's default */
static const uint8_t MK_SVEL[6] = {20, 40, 60, 80, 100, 127};   /* (SOUND's W11..W16) */
/* TRIG, SLIDE, FUNC: W1..W10 are the black keys' B1..B10 (2026-11-13, owner) */
#define MK_BK(m) ((m) == UI_F_TRIG || (m) == UI_F_SLIDE || (m) == UI_F_FUNC)

/* COND (2026-11-13, owner: the black keys' A:B, wider): W1..W8 A = 1..8 (B kept, at least A), W9..W16 B = 1..8 (A
 * kept, at most B); B 1 is ALWAYS */
static int mk_cond(int cur, int w)
{
    int ca = 1, cb = 1;
    if (!qd_cond_ab_of(cur, &ca, &cb))
        ca = cb = 1;
    if (w < 8)
        ca = w + 1, cb = cb < ca ? ca : cb;
    else
        cb = w - 7, ca = ca > cb ? cb : ca;
    return cb < 2 ? QD_COND_ALWAYS : qd_cond_ab(ca, cb);
}

/* 2026-11-13 (owner): the step keyboard: a step view's W keys the keys (a B11 click) */
int qd_step_kb(const qd_app_t *a)
{
    return a->ui.mode == QD_MODE_STEP && qd_snd_keys(a) && !a->ui.latch && a->ui.view <= QD_VIEW_EDIT;
}

int qd_mk_on(const qd_app_t *a)
{
    const int m = a->ui.hmode;
    return qd_step_kb(a) && m != UI_F_NOTE;            /* (2026-11-13: TRIG, SLIDE, FUNC too: the black keys' values) */
}

int qd_mk_val(const qd_app_t *a, int m, int w)
{
    switch (m) {
    case UI_F_VEL: return w == 15 ? 127 : 8 * (w + 1);
    case UI_F_LEN: return w == 15 ? QD_LEN_INF : w;     /* (the table's 1/8 .. 32) */
    case UI_F_MICRO: return w == 15 ? 23 : 3 * (w - 7);
    case UI_F_CHNC: return (w + 1) * 100 / 16;
    case UI_F_SOUND: return w < 10 ? a->ui.sound_bank * 10 + w + 1 : MK_SVEL[w - 10];
    default: return w;                                  /* (RATCH: x1..x8, then rising; COND: ALWAYS .. !1ST, then
                                                         * QD_COND_AB's 1:2 .. 2:4) */
    }
}

/* the step the keys show (sel: the selection's first if any), else the track's defaults */
static void mk_res(const qd_app_t *a, qd_rstep_t *r, int sel)
{
    const qd_pattern_t *P = qd_ctl_cpat(a);
    qd_step_t on;
    if (sel && a->ui.nheld) {
        qd_step_resolve(P, a->sel, a->ui.held[0], r);
        return;
    }
    if (sel && a->seq.playing && qd_seq_track_step(&a->seq, a->sel) >= 0) {   /* (2026-11-13: LIVE, playing: the
                                                         * step playing, as a selected one) */
        qd_step_resolve(P, a->sel, qd_seq_track_step(&a->seq, a->sel), r);
        return;
    }
    memset(&on, 0, sizeof on);
    on.flags = QD_SF_ON;
    qd_step_resolve_def(&P->trk[a->sel].def, &on, r);
}

/* the key of lo..hi-1 holding v (near: the nearest, else the same; -1 none) */
static int mk_key(const qd_app_t *a, int m, int v, int lo, int hi, int near)
{
    int w, best = -1, bd = near ? 1 << 20 : 1;
    for (w = lo; w < hi; w++) {
        int d = qd_mk_val(a, m, w) - v;
        d = d < 0 ? -d : d;
        if (d < bd)
            bd = d, best = w;
    }
    return best;
}

/* the keys lit: the shown step's value (SOUND: its sound and its velocity), and the keys down */
static uint16_t mk_lit(const qd_app_t *a)
{
    const int m = a->ui.hmode;
    qd_rstep_t r;
    int k, v, w;
    uint16_t b = 0;
    mk_res(a, &r, 1);
    if (m == UI_F_COND || MK_BK(m)) {
        int ca, cb;
        if (MK_BK(m)) {                                 /* (the black keys' lit values, a selection's) */
            char lab[10][6];
            uint16_t lit = 0, def, none;
            int clr;
            if (a->ui.nheld)
                qd_step_value_keys(a, lab, &lit, &def, &none, &clr);
            b = lit & 0x3ffu;
        } else if (r.cond == QD_COND_ALWAYS) {
            b = 1u | 1u << 8;                           /* (1:1) */
        } else if (qd_cond_ab_of(r.cond, &ca, &cb)) {
            b = (uint16_t)(1u << (ca - 1) | 1u << (cb + 7));
        }
        goto down;
    }
    switch (m) {
    case UI_F_VEL: v = r.vel; break;
    case UI_F_LEN: v = r.len; break;
    case UI_F_MICRO: v = r.micro; break;
    case UI_F_RATCH: v = r.ramp > QD_RAMP_UP ? -9 : r.ramp * 8 + r.ratch - 1; break;
    case UI_F_CHNC: v = r.chance; break;
    case UI_F_COND: v = r.cond; break;
    default: v = r.sound ? r.sound : a->model.t[a->sel].sound; break;   /* (SOUND: the track's own when no lock) */
    }
    if (m == UI_F_SOUND) {
        k = mk_key(a, m, !a->ui.nheld && a->ui.mk_svel ? a->ui.mk_svel : r.vel, 10, 16, 1);   /* (LIVE: the pad's) */
        b = (uint16_t)(1u << k);
        k = mk_key(a, m, v, 0, 10, 0);
    } else {
        k = mk_key(a, m, v, 0, 16, m == UI_F_VEL || m == UI_F_MICRO || m == UI_F_CHNC);
    }
    if (k >= 0)
        b |= (uint16_t)(1u << k);
down:
    for (w = 0; w < 16; w++)
        if ((a->keys_held >> QD_WHITE_KEY[w]) & 1u)
            b |= (uint16_t)(1u << w);
    return b;
}

void qd_mk_leds(const qd_app_t *a, uint8_t *key)
{
    const int m = a->ui.hmode;
    const uint16_t lit = mk_lit(a);
    int w, l;
    for (w = 0; w < 16; w++) {
        switch (m) {
        case UI_F_VEL:
        case UI_F_CHNC: l = 13 + 178 * w / 15; break;  /* (a fade, 5 % .. 75 %) */
        case UI_F_LEN: l = 13 + 102 * w / 15; break;
        case UI_F_RATCH: l = 15 + 15 * (w & 7); break;
        case UI_F_MICRO: l = w == 7 ? QD_LED_MID : QD_LED_FAINT; break;   /* (W8 the grid: 2026-11-13, brighter) */
        case UI_F_COND: l = w < 8 ? QD_LED_DIM : QD_LED_FAINT; break;   /* (A, then B) */
        case UI_F_TRIG:
        case UI_F_SLIDE:
        case UI_F_FUNC: l = w < (m == UI_F_SLIDE ? 3 : m == UI_F_FUNC ? 9 : 10) ? QD_LED_DIM : QD_LED_OFF; break;
        default: l = w < 10 ? QD_LED_DIM : 13 + 178 * (w - 10) / 5; break;   /* (SOUND: the sounds dim, the
                                                         * velocities a fade) */
        }
        key[QD_WHITE_KEY[w]] = (uint8_t)((lit >> w) & 1u ? QD_LED_LIT : l);
    }
}

/* .. the drawn keys: the lit one filled, the others a hint, a fill as high as their LED (the fades: VEL, CHNC, LEN,
 * SOUND's velocities; RATCH's counts) */
void qd_mk_kst(const qd_app_t *a, uint8_t *kst)
{
    uint8_t lv[QD_NKEY];
    int w;
    qd_mk_leds(a, lv);
    for (w = 0; w < 16; w++) {
        const int l = lv[QD_WHITE_KEY[w]];
        kst[w] = (uint8_t)(l == QD_LED_LIT ? 4 : 1 | (l > 13 ? (l - 13) * 31 / 178 : 0) << 3);
    }
}

/* step s takes W w's value of mode m */
static void mk_set(qd_app_t *a, int s, int m, int w)
{
    qd_pattern_t *p = PAT(a);
    const int t = a->sel, v = qd_mk_val(a, m, w);
    switch (m) {
    case UI_F_VEL: qd_step_set_vel(p, t, s, v); break;
    case UI_F_LEN: qd_step_set_len(p, t, s, v); break;
    case UI_F_MICRO: qd_step_set_micro(p, t, s, v); break;
    case UI_F_RATCH: qd_step_set_ratchet(p, t, s, (w & 7) + 1, w >> 3); break;   /* (QD_RAMP_FLAT, QD_RAMP_UP) */
    case UI_F_CHNC: qd_step_set_chance(p, t, s, v); break;
    case UI_F_COND: {
        qd_rstep_t r;
        qd_step_resolve(p, t, s, &r);
        qd_step_set_cond(p, t, s, mk_cond(r.cond, w));
        break;
    }
    case UI_F_SOUND:
        if (w >= 10)
            qd_step_set_vel(p, t, s, v);
        else if (v <= a->proj.nsound)
            qd_step_set_sound(p, t, s, v);
        break;
    default: break;
    }
}

/* the keys' notes (one set: the keys sound the track's or the shown step's notes; a pitch ends for every key at once
 * anyway): ended, when the last key that sounds them is let go (w -1: now); struck as a key's (the notes, the
 * velocity, the sound) */
static void mk_end(qd_app_t *a, int w)
{
    qd_ui_t *u = &a->ui;
    int i;
    if (w >= 0 && (u->mk_on &= (uint16_t)~(1u << w)) != 0)
        return;
    for (i = 0; i < QD_NNOTE; i++)
        if (u->mk_nt[i])
            qd_player_key_off(&a->player, u->mk_tr, u->mk_nt[i]);
    memset(u->mk_nt, 0, sizeof u->mk_nt);
    u->mk_on = 0;
}
static void mk_strike(qd_app_t *a, int w, const qd_rstep_t *r)
{
    qd_ui_t *u = &a->ui;
    const int16_t *pt = r->sound > 0 ? qd_sound_patch(&a->proj, r->sound) : 0;
    int i;
    if (u->mk_tr != a->sel || memcmp(u->mk_nt, r->notes, (size_t)r->nn) != 0)
        mk_end(a, -1);                                  /* (other notes: the ones sounding end first) */
    u->mk_tr = (uint8_t)a->sel;
    u->mk_on |= (uint16_t)(1u << w);
    u->mk_off = u->hmode == UI_F_LEN ? (a->ms + qd_ctl_len_ms(a, r->len)) | 1u : 0;   /* (2026-11-13: LEN, its length) */
    for (i = 0; i < r->nn && i < QD_NNOTE; i++) {
        qd_player_key_on_p(&a->player, a->sel, r->notes[i] & 127, r->vel, pt);
        u->mk_nt[i] = (uint8_t)(r->notes[i] & 127);
    }
}

/* the step a recorded note went into (-1 none) */
static int rec_step_of(qd_app_t *a, int note)
{
    const qd_ui_t *u = &a->ui;
    if (u->rec_t0[note & 127] < 0)
        return -1;
    return qd_rec_stepping(a) ? loop_step(a, u->cursor) : u->rec_s[note & 127];
}

/* 2026-11-13 (owner): LIVE, playing, VEL .. COND: a W key held overrides the mode's value of every step the track plays
 * (seq.ovr_*); recording, each step the playhead enters takes it (an empty one a LOCK step: no trig, no notes) */
#define MK_OVR(m) ((m) >= UI_F_VEL && (m) <= UI_F_COND)
static void mk_ovr_write(qd_app_t *a, int s)
{
    const int w = a->ui.mk_ovr - 1;
    if (s < 0)
        return;
    if (!qd_step_is_on(qd_step(PAT(a), a->sel, s)))
        qd_step_set_trig(PAT(a), a->sel, s, QD_TRIG_LOCK);
    mk_set(a, s, a->ui.hmode, w);
    a->ui.mk_ovr_s = (int16_t)s;
}
static void mk_ovr_set(qd_app_t *a, int w)              /* (w -1: off) */
{
    const int m = a->ui.hmode;
    qd_seq_t *q = &a->seq;
    qd_rstep_t r;
    a->ui.mk_ovr = (uint8_t)(w + 1);
    a->ui.mk_ovr_s = -1;
    if (w < 0) {
        q->ovr_t = 0;
        return;
    }
    mk_res(a, &r, 0);
    q->ovr_v = (int16_t)(m == UI_F_RATCH ? (w & 7) | (w >> 3) << 4 : m == UI_F_COND ? mk_cond(r.cond, w) : qd_mk_val(a, m, w));
    q->ovr_pm = (uint8_t)m;                             /* (UI_F_VEL .. UI_F_COND are QD_PM_VEL .. QD_PM_COND) */
    q->ovr_t = (uint8_t)(a->sel + 1);
    if (a->ui.rec)
        mk_ovr_write(a, qd_seq_track_step(q, a->sel));
}

void qd_mk_white(qd_app_t *a, int key, int w, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    const int m = u->hmode, v = qd_mk_val(a, m, w);
    const uint16_t bit = (uint16_t)(1u << w);
    qd_rstep_t r;
    int i, s;
    (void)key, (void)vel;
    if (MK_BK(m)) {                                     /* TRIG, SLIDE, FUNC: the black keys' values (selected steps; LIVE:
                                                         * the track's) */
        if (w < 10)
            u->nheld ? qd_step_value_key(a, w, down) : qd_step_value_key_default(a, w, down);
        return;
    }
    if (!u->nheld) {                                    /* LIVE: the key's value shows (a second after the last) */
        if (down)
            u->mk_shw = (uint8_t)(w + 1);
        u->mk_ms = a->ms;
    }
    if (down && m == UI_F_SOUND && w >= 10)
        u->mk_svel = (uint8_t)v;                        /* (SOUND: the pads' velocity from now on) */
    if (!u->nheld && a->seq.playing && MK_OVR(m)) {     /* LIVE, playing: the override while held (no note struck) */
        if (down) {
            mk_ovr_set(a, w);
        } else if (u->mk_ovr == w + 1) {
            int k, o = -1;
            for (k = 0; k < 16; k++)                    /* (another key still held: its value) */
                if (k != w && ((a->keys_held >> QD_WHITE_KEY[k]) & 1u))
                    o = k;
            mk_ovr_set(a, o);
        }
        return;
    }
    if (!down) {
        if (u->mk_rec & bit) {                          /* (a recorded press: its notes end, LEN's value kept) */
            u->mk_rec &= (uint16_t)~bit;
            s = rec_step_of(a, u->mk_nt[0]);
            for (i = 0; i < QD_NNOTE; i++)
                if (u->mk_nt[i])
                    qd_rec_key(a, u->mk_nt[i], 0, 0);
            if (s >= 0 && m == UI_F_LEN)
                mk_set(a, s, m, w);
        }
        mk_end(a, w);
        return;
    }
    if (u->nheld && !u->rec) {                          /* the selection takes the value; the step heard with it */
        qd_step_promote_all(a);
        for (i = 0; i < u->nheld; i++)
            mk_set(a, u->held[i], m, w);
        if (m == UI_F_SOUND)
            u->roll_ms = a->ms, u->roll_dir = 1;
        if (qd_ctl_previewing(a)) {
            qd_ctl_preview_strike(a, u->held[0]);
            return;
        }
        mk_res(a, &r, 1);
    } else {                                            /* LIVE (or recording): the track with the key's value */
        mk_res(a, &r, 0);
        if (m == UI_F_VEL || (m == UI_F_SOUND && w >= 10))
            r.vel = v;
        else if (m == UI_F_SOUND && v <= a->proj.nsound)
            r.sound = v;
        else if (m == UI_F_LEN)
            r.len = v;
        if (m == UI_F_SOUND && w < 10 && u->mk_svel)
            r.vel = u->mk_svel;
        if (u->rec) {                                   /* REC armed: a trig at the playhead (as the keys' notes) */
            for (i = 0; i < r.nn && i < QD_NNOTE; i++)
                qd_rec_key(a, r.notes[i] & 127, 1, r.vel);
            s = rec_step_of(a, r.notes[0]);
            if (s >= 0) {
                mk_set(a, s, m, w);
                u->mk_rec |= bit;
            }
        }
    }
    mk_strike(a, w, &r);
}

/* the band: the track's value of the mode B11 set now ("TRACK VEL 100", "TRACK NOTES C4 E4 G4"; o: 48 bytes) */
void qd_mk_band(const qd_app_t *a, char *o)
{
    const int m = a->ui.mk_bm;
    char n[12];
    const char *pre = "", *s = n, *suf = "";
    qd_rstep_t r;
    int i;
    mk_res(a, &r, 0);
    strcpy(o, "TRACK ");
    strcat(o, m == UI_F_NOTE ? "NOTES" : UI_FIELD_NAME[m]);
    switch (m) {
    case UI_F_NOTE:
        for (i = 0; i < r.nn && i < QD_NNOTE; i++)
            strcat(strcat(o, " "), qd_note_name(r.notes[i], n));
        s = 0;
        break;
    case UI_F_VEL: ui_itoa(n, r.vel); break;
    case UI_F_LEN: s = qd_len_name(r.len); break;
    case UI_F_MICRO: ui_itoa(n, r.micro), pre = r.micro > 0 ? "+" : ""; break;
    case UI_F_RATCH: ui_itoa(n, r.ratch), pre = "\xc3\x97"; break;
    case UI_F_CHNC: ui_itoa(n, r.chance), suf = "%"; break;
    case UI_F_COND: qd_cond_name(r.cond, n); break;
    case UI_F_SOUND: strncat(strcat(o, " "), qd_track_sound_label(&a->model.t[a->sel]), 16), s = 0; break;
    default: s = 0; break;                              /* (TRIG, FUNC, SLIDE: the mode alone) */
    }
    if (s)
        strcat(strcat(strcat(strcat(o, " "), pre), s), suf);
}

int qd_mk_b11(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    int w, first = -1, n = 0, m = u->hmode;
    uint8_t notes[QD_NNOTE];
    if (!qd_snd_keys(a) || u->latch || u->view > QD_VIEW_EDIT || qd_ctl_shift(a) ||
        ((a->buttons_held >> QD_B_MIX | a->buttons_held >> QD_B_PATTERN) & 1u))
        return 0;
    for (w = 15; w >= 0; w--)
        if ((a->keys_held >> QD_WHITE_KEY[w]) & 1u)
            first = w;
    if (first < 0)
        return 0;
    if (u->nheld) {                                     /* a selection: its value back to the track's (B11 + F2's) */
        qd_step_b11_reset(a);
    } else if (qd_mk_on(a) && MK_BK(m)) {               /* (TRIG, SLIDE, FUNC: their keys set the track's already) */
        return 1;
    } else if (!qd_mk_on(a)) {                          /* the keys play: the held notes the track's notes */
        for (w = first; w < 16 && n < QD_NNOTE; w++)
            if ((a->keys_held >> QD_WHITE_KEY[w]) & 1u)
                notes[n++] = (uint8_t)qd_snd_key_note(a, QD_WHITE_KEY[w]);
        qd_tdef_set_notes(PAT(a), a->sel, notes, n);
        m = UI_F_NOTE;
    } else if (m == UI_F_SOUND && first < 10) {         /* a sound: the track's own sound */
        const int k = qd_mk_val(a, m, first);
        qd_track_t *tr = &a->model.t[a->sel];
        if (k <= a->proj.nsound && qd_sound_used(&a->proj, k)) {
            qd_track_load_sound(tr, k, qd_sound_name(&a->proj, k), qd_sound_patch(&a->proj, k));
            tr->preset = qd_sound_preset(&a->proj, k);
        }
    } else {                                            /* the key's value the track's default (UI_F_VEL .. UI_F_COND
                                                         * are QD_PM_VEL .. QD_PM_COND) */
        const int v = qd_mk_val(a, m, first);
        if (m == UI_F_SOUND)
            m = UI_F_VEL;
        qd_rstep_t r;
        mk_res(a, &r, 0);
        qd_tdef_set(PAT(a), a->sel, m, m == UI_F_RATCH ? qd_ratch_make((first & 7) + 1, first >> 3)
                                       : m == UI_F_COND ? mk_cond(r.cond, first) : v);
    }
    u->mk_bm = (uint8_t)m;
    qd_ctl_confirm_col(a, "", 0, QD_CF_BAND);           /* (its words at the drawing: qd_mk_band) */
    return 1;
}

/* 2026-11-13 (owner): LIVE, the W key's value shows while held and a second after (r: the track's, then the key's) */
int qd_mk_show(const qd_app_t *a, qd_rstep_t *r)
{
    const qd_ui_t *u = &a->ui;
    const int m = u->hmode, w = u->mk_shw - 1;
    int held = 0, k;
    if (!u->mk_shw || u->nheld || !qd_mk_on(a) || MK_BK(m))
        return 0;
    for (k = 0; k < 16; k++)
        held |= (int)((a->keys_held >> QD_WHITE_KEY[k]) & 1u);
    if (!held && a->ms - u->mk_ms >= 1000u)
        return 0;
    k = qd_mk_val(a, m, w);
    switch (m) {
    case UI_F_VEL: r->vel = k; break;
    case UI_F_LEN: r->len = k; break;
    case UI_F_MICRO: r->micro = k; break;
    case UI_F_RATCH: r->ratch = (w & 7) + 1, r->ramp = w >> 3; break;
    case UI_F_CHNC: r->chance = k; break;
    case UI_F_COND: r->cond = mk_cond(r->cond, w); break;
    case UI_F_SOUND:
        if (w >= 10)
            r->vel = k;
        else if (k <= a->proj.nsound)
            r->sound = k;
        break;
    default: return 0;
    }
    return 1;
}

void qd_mk_tick(qd_app_t *a)                            /* (LEN: the keys' notes end at the length) */
{
    qd_ui_t *u = &a->ui;
    if (u->mk_ovr) {                                    /* the override: over when the keys, the mode, LIVE or playback
                                                         * go; recording, the step the playhead entered takes it */
        int k, held = 0;
        for (k = 0; k < 16; k++)
            held |= (int)((a->keys_held >> QD_WHITE_KEY[k]) & 1u);
        if (!held || !a->seq.playing || !qd_mk_on(a) || u->nheld || !MK_OVR(u->hmode) ||
            a->seq.ovr_t != a->sel + 1) {
            mk_ovr_set(a, -1);
        } else if (u->rec) {
            const int s = qd_seq_track_step(&a->seq, a->sel);
            if (s >= 0 && s != u->mk_ovr_s)
                mk_ovr_write(a, s);
        }
    }
    if (a->ui.mk_off && (int32_t)(a->ms - a->ui.mk_off) >= 0) {
        a->ui.mk_off = 0;
        if (!a->ui.mk_rec)
            mk_end(a, -1);
    }
}
