/* SPDX-License-Identifier: GPL-3.0-only */
/* KEYS mode's recording and MIDI notes into a held step (ctl.h; PLAN §8 "KEYS mode").
 *
 * STEP RECORDING (REC on, the transport stopped): a chord played goes into the cursor's step (the notes struck while
 * any is still down are one chord, the step cleared first, the first note's velocity, the track's default length);
 * when the last of them is let go the cursor moves on (round the track's length). OCT+ is a rest (the cursor's step
 * cleared, the cursor on), OCT- a step back, PRESETS moves the cursor; FUNC + OCT- / OCT+ shift the octave.
 *
 * LIVE RECORDING (REC on, playing): a note goes into the step nearest the playhead of the track, with the micro-timing
 * it was played with (-12..+12 24ths; FUNC + REC: quantized, micro 0); notes struck while another recorded note is
 * still down on the same step are its chord; its length is how long it was held (the nearest of the length table).
 * The step is cleared first (the take replaces it). Knob turns (KNOB 1..4: the quick page; ALGORITHM) are written as
 * locks on the step playing.
 *
 * MIDI: notes on the selected track's channel do the same in KEYS mode; with steps held in the step view they become
 * the held steps' notes (the notes struck while any is down are one chord). */
#include <math.h>
#include <string.h>

#include "app.h"
#include "ui.h"

static qd_pattern_t *PAT(qd_app_t *a) { return qd_ctl_pat(a); }
static int tlen(qd_app_t *a)
{
    const int l = PAT(a)->trk[a->sel].length;
    return l < 1 ? 1 : l;
}

/* the nearest length of the table to n eighths of a step (never INF) */
static int len_index(double eighths)
{
    int i, best = 0;
    double bd = 1e30;
    for (i = 0; i < QD_LEN_INF; i++) {
        const double d = fabs(log(eighths > 0.5 ? eighths : 0.5) - log((double)qd_len_eighths(i)));
        if (d < bd)
            bd = d, best = i;
    }
    return best;
}

/* a live note ends: its length into its step (the chord's longest note wins) */
static void live_off(qd_app_t *a, int note, int64_t now)
{
    qd_ui_t *u = &a->ui;
    const int64_t L = qd_rate_ticks(PAT(a)->trk[a->sel].rate);
    const int s = u->rec_s[note];
    const qd_step_t *st = qd_step(PAT(a), a->sel, s);
    const int li = len_index((double)(now - u->rec_t0[note]) * 8.0 / (double)L);
    int i;
    if (qd_step_type(st) != QD_TRIG_NONE && li > st->len)
        qd_step_set_len(PAT(a), a->sel, s, li);
    for (i = 0; i < u->nroll; i++)
        if (u->roll[i].note == note && u->roll[i].t1 < 0)
            u->roll[i].t1 = now;
}

static void chord_add(uint8_t *chord, int *n, int note)
{
    int i;
    if (*n >= 4)
        return;
    for (i = 0; i < *n; i++)
        if (chord[i] == note)
            return;
    if (*n && (note - chord[0] > 48 || chord[0] - note > 48))
        return;
    chord[(*n)++] = (uint8_t)note;
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
    u->rec_n = 0;
    u->rec_step = -1;
    u->cursor = u->page[a->sel] * QD_PAGE;              /* step recording starts on the page shown */
    if (u->cursor >= tlen(a))
        u->cursor = 0;
}

void qd_rec_cursor(qd_app_t *a, int d)
{
    qd_ui_t *u = &a->ui;
    const int n = tlen(a);
    u->cursor = ((u->cursor + d) % n + n) % n;
    u->page[a->sel] = u->cursor / QD_PAGE;
}

void qd_rec_rest(qd_app_t *a)
{
    qd_step_edited(a);
    qd_step_clear(PAT(a), a->sel, a->ui.cursor);
    qd_rec_cursor(a, 1);
}

/* a note down / up while recording (note -1, up: every note still down ends) */
void qd_rec_key(qd_app_t *a, int note, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    qd_pattern_t *p = PAT(a);
    qd_tset_t *ts = &p->trk[a->sel];
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
    if (!u->rec || u->mode != QD_MODE_KEYS)
        return;
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
    qd_step_edited(a);
    if (!a->seq.playing) {                              /* step recording */
        if (!u->rec_n) {
            u->rec_nchord = 0;
            qd_step_clear(p, t, u->cursor);
        }
        chord_add(u->rec_chord, &u->rec_nchord, note);
        qd_step_set_notes(p, t, u->cursor, u->rec_chord, u->rec_nchord);
        if (!u->rec_n) {
            qd_step_set_vel(p, t, u->cursor, vel);
            ts->def_vel = (uint8_t)vel;
            ts->def_note = (uint8_t)note;
        }
        u->rec_t0[note] = 0;
        u->rec_n++;
        return;
    }
    {   /* live recording: the step nearest the playhead, the micro-timing as played */
        const double pos = qd_seq_track_pos(&a->seq, t);
        const int n = tlen(a);
        const int64_t now = qd_seq_tick(&a->seq);
        int s = (int)floor(pos + 0.5), micro = (int)lround((pos - floor(pos + 0.5)) * 24.0);
        if (s >= n)
            s -= n;
        if (u->quantize)
            micro = 0;
        micro = micro < -23 ? -23 : micro > 23 ? 23 : micro;
        if (u->rec_n && s == u->rec_step) {             /* struck while the chord's notes are down: the chord */
            chord_add(u->rec_chord, &u->rec_nchord, note);
            qd_step_set_notes(p, t, s, u->rec_chord, u->rec_nchord);
        } else {
            u->rec_nchord = 0;
            chord_add(u->rec_chord, &u->rec_nchord, note);
            qd_step_clear(p, t, s);
            qd_step_set_notes(p, t, s, u->rec_chord, 1);
            qd_step_set_vel(p, t, s, vel);
            qd_step_set_micro(p, t, s, micro);
            qd_step_set_len(p, t, s, 0);                /* (the shortest until the note ends) */
            u->rec_step = s;
            ts->def_note = (uint8_t)note;
            ts->def_vel = (uint8_t)vel;
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

/* live recording: KNOB 1..4 (the quick page) and ALGORITHM as locks on the step playing */
int qd_rec_knob(qd_app_t *a, int knob, int d)
{
    const int t = a->sel;
    int id, s, base;
    int16_t v;
    if (knob >= QD_K_1)
        id = qd_step_quick_id(knob - QD_K_1);
    else if (knob == QD_K_ALGO)
        id = QD_QP_ALGO;
    else
        return 0;
    s = (int)qd_seq_track_pos(&a->seq, t) % tlen(a);
    base = qd_lock_get(PAT(a), t, s, id, &v) ? v : qd_step_base(a, t, s, id);
    qd_step_edited(a);
    if (qd_lock_set(PAT(a), t, s, id, base + d * qd_step_id_detent(id)) == QD_EFULL)
        qd_ctl_confirm(a, "NO ROOM", "for locks: the pattern has 1024");
    qd_ctl_takeover(a, knob == QD_K_ALGO ? UI_TK_ALGO : UI_TK_FREQ + (knob - QD_K_1),
                    knob == QD_K_ALGO ? 0 : knob - QD_K_1, d, 1, s);
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
    if (u->mode == QD_MODE_KEYS)
        qd_rec_key(a, note, down, vel);
}
