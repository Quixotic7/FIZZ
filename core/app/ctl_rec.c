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
 * THE TAKE (the record settings' KNOB 3; 2026-10-25, the owner): CAPTURE (the default): armed, nothing changes until
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
 * become the held steps' notes (the notes struck while any is down are one chord). */
#include <math.h>
#include <string.h>

#include "app.h"
#include "ui.h"
#include "../seq/scale.h"

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
    u->cap = a->proj.rec_dub == QD_TAKE_CAPTURE;        /* (CAPTURE: nothing changes until the first note) */
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

/* a MIDI note on the selected track's channel: into the held steps, or the recording */
void qd_rec_midi_note(qd_app_t *a, int note, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    if (u->mode == QD_MODE_STEP && (u->nheld || u->wk_pending)) {
        qd_step_promote_all(a);
        if (down) {
            if (!u->midi_n)
                u->midi_nchord = 0;
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
