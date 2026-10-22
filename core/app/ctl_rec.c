/* SPDX-License-Identifier: GPL-3.0-only */
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
    const int64_t L = qd_rate_ticks(qd_tset_rate(&PAT(a)->trk[a->sel]));
    const int s = u->rec_s[note];
    const int li = len_index((double)(now - u->rec_t0[note]) * 8.0 / (double)L);
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
    if (track_empty(a)) {                               /* an empty track: page 1, growing */
        qd_page_set_loop(PAT(a), a->sel, 1);
        u->rec_grow = 1;
        u->grow_reached = 0;
    }
    u->rec_kind = QD_REC_LIVE;
    qd_ctl_confirm(a, "LIVE REC", 0);
}

/* P1.3y: REC held with the record settings' STEP: step recording armed (the cursor; playing or not) */
void qd_rec_arm_step(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (!u->rec)
        qd_rec_toggle(a);
    u->rec_kind = QD_REC_STEP;
    u->live_wait = 0;
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

void qd_rec_end(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (u->rec)
        qd_rec_toggle(a);
    u->live_wait = 0;
    grow_trim(a);
}

void qd_rec_ms(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    const qd_tset_t *ts = &PAT(a)->trk[a->sel];
    int pp, k;
    if (!u->rec_grow)
        return;
    if (!u->rec || u->mode != QD_MODE_KEYS || (!a->seq.playing && !u->live_wait)) {
        grow_trim(a);                                   /* (the recording or the transport ended another way) */
        return;
    }
    if (!a->seq.playing)
        return;
    pp = qd_ctl_playing_page(a, a->sel);
    if (pp > u->grow_reached)
        u->grow_reached = pp;
    for (k = 0; k < 8 && ((qd_track_loop(ts) >> k) & 1); k++)
        ;
    if (k < 8 && qd_seq_track_pos(&a->seq, a->sel) >= (double)(qd_track_len(ts) - 4))
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
    if (!u->rec || u->mode != QD_MODE_KEYS)
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
    if (!a->seq.playing && u->live_wait) {              /* P1.3x: armed live, stopped: this note starts it all */
        u->live_wait = 0;
        qd_app_play(a);
    }
    if (qd_rec_stepping(a)) {                           /* step recording */
        const int cs = loop_step(a, u->cursor);
        const int first = !u->rec_n;
        const int dub = first && a->proj.rec_dub && qd_step_is_on(qd_step(p, t, cs));
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
        const double pos = qd_seq_track_pos(&a->seq, t);
        const int n = tlen(a);
        const int64_t now = qd_seq_tick(&a->seq);
        int lp = (int)floor(pos + 0.5), s, micro = (int)lround((pos - floor(pos + 0.5)) * 24.0);
        if (lp >= n)
            lp -= n;
        s = loop_step(a, lp);
        micro = (int)lround(micro * (100 - a->proj.rec_quant) / 100.0);   /* P1.3y: QUANTIZE (100 %: on the step) */
        micro = micro < -23 ? -23 : micro > 23 ? 23 : micro;
        if (u->rec_n && s == u->rec_step) {             /* struck while the chord's notes are down: the chord */
            chord_add(u->rec_chord, &u->rec_nchord, note);
            qd_step_set_notes(p, t, s, u->rec_chord, u->rec_nchord);
        } else if (a->proj.rec_dub && qd_step_is_on(qd_step(p, t, s))) {   /* P1.3y: OVERDUB: the note joins the
                                                         * step's (its velocity, timing and length stay) */
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

/* live recording: KNOB 1..4 (the quick page) as locks on the step playing */
int qd_rec_knob(qd_app_t *a, int knob, int d)
{
    const int t = a->sel;
    int id, s, base;
    int16_t v;
    if (knob >= QD_K_1)
        id = qd_step_quick_id(knob - QD_K_1);
    else
        return 0;
    s = loop_step(a, (int)qd_seq_track_pos(&a->seq, t) % tlen(a));
    base = qd_lock_get(PAT(a), t, s, id, &v) ? v : qd_step_base(a, t, s, id);
    if (qd_lock_set(PAT(a), t, s, id, base + qd_ctl_acc(a, d, qd_step_id_steps(id)) * qd_step_id_detent(id)) == QD_EFULL)
        qd_ctl_confirm(a, "NO ROOM", "for locks: the pattern has 1024");
    qd_ctl_takeover(a, UI_TK_FREQ + (knob - QD_K_1), knob - QD_K_1, d, 1, s);
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
