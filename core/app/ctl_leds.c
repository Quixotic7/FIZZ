/* SPDX-License-Identifier: GPL-3.0-only */
/* The LEDs (ctl.h; PLAN §8's table), five levels: lit, mid, dim, faint, off (RemoteFM1 maps them to the unit's three).
 *
 *   step view       white: trigs lit, LOCK steps dim, steps beyond the track's length off, the others faint; playing,
 *                   the playhead's key inverts for its step; a key going down shows at once what its tap makes.
 *                   black, nothing held: the selected track lit, the others faint, a track's key flashing (inverted)
 *                   on each of its trigs; C#5 / D#5 mid when there is a page that way.
 *   a step held     the held steps blink fast (latched ones blink slowly); black: the field shown lit, the fields
 *                   holding a value dim, the rest faint (CLEAR blinks while its undo is open)
 *   KEYS            the notes sounding lit, the C keys faint; step recording: the cursor's white key blinks
 *   tracks screen   white 1..8 flash with the tracks' trigs (the selected dim), 9..16 lit for the tracks heard
 *   PATTERN held    the patterns with content dim, the playing one lit, the queued one blinking
 *   buttons         held: lit; FUNC lit while latched; KEYS lit in KEYS mode; REC lit while recording (FILL: while
 *                   held); TRACKS lit on its screen; PLAY's green LED on each beat while playing */
#include <math.h>
#include <string.h>

#include "app.h"
#include "ui.h"

#define QD_TRIG_LED_MS_ 80u

static int held(const qd_app_t *a, int b) { return (int)((a->buttons_held >> b) & 1u); }
static int blink(const qd_app_t *a, uint32_t half) { return (int)((a->ms / half) & 1u) == 0; }

/* the notes the selected track sounds (keys held, MIDI notes held, notes waiting for a voice) */
void qd_ctl_sounding(const qd_app_t *a, uint8_t *notes)
{
    int i;
    memset(notes, 0, 128);
    for (i = 0; i < a->pool.nvoice; i++) {
        const qd_pool_voice_t *v = &a->pool.v[i];
        if (v->q.active && !v->q.kill && v->q.gate && v->track == a->sel)
            notes[v->q.note & 127] = 1;
        if (v->pend && !v->pend_off && v->pend_track == a->sel)
            notes[v->pend_note & 127] = 1;
    }
}

/* track t's last trig within the flash's time */
int qd_ctl_trig_flash(const qd_app_t *a, int t)
{
    const uint64_t since = a->seq.now - a->seq.flash_sample[t];
    return a->seq.playing && a->seq.flash_step[t] >= 0 && a->seq.flash_sample[t] <= a->seq.now &&
           since < (uint64_t)(QD_TRIG_LED_MS_ * QD_FS / 1000);
}

static uint8_t step_level(const qd_app_t *a, int s)
{
    const qd_pattern_t *p = &a->proj.pat[a->seq.cur];
    const int ty = qd_step_type(qd_step(p, a->sel, s));
    if (s >= p->trk[a->sel].length)
        return QD_LED_OFF;
    return ty == QD_TRIG_LOCK ? QD_LED_DIM : ty ? QD_LED_LIT : QD_LED_FAINT;
}

static void leds_step(const qd_app_t *a, qd_leds_t *out)
{
    const qd_ui_t *u = &a->ui;
    const qd_pattern_t *p = &a->proj.pat[a->seq.cur];
    const int page = u->page[a->sel], len = p->trk[a->sel].length;
    int w, i;
    for (w = 0; w < 16; w++) {
        const int s = page * QD_PAGE + w;
        uint8_t l = step_level(a, s);
        if ((u->wk_pending >> w) & 1u) {                /* going down: what its tap makes, at once */
            const int ty = qd_step_type(qd_step(p, a->sel, s));
            if (u->latch && u->nheld)
                l = QD_LED_LIT;
            else if ((u->wk_func >> w) & 1u)
                l = ty == QD_TRIG_LOCK ? QD_LED_FAINT : QD_LED_DIM;
            else
                l = ty ? QD_LED_FAINT : QD_LED_LIT;
        }
        out->key[QD_WHITE_KEY[w]] = l;
    }
    if (a->seq.playing) {                               /* the playhead's key inverts for its step */
        const int s = (int)qd_seq_track_pos(&a->seq, a->sel);
        if (s / QD_PAGE == page && s < len) {
            const int k = QD_WHITE_KEY[s % QD_PAGE];
            out->key[k] = out->key[k] >= QD_LED_MID ? QD_LED_OFF : QD_LED_LIT;
        }
    }
    for (i = 0; i < u->nheld; i++) {                    /* held: fast blink; latched: a slow one */
        const int s = u->held[i];
        if (s / QD_PAGE == page)
            out->key[QD_WHITE_KEY[s % QD_PAGE]] =
                (u->held_key[i] >= 0 ? blink(a, QD_FAST_MS) : blink(a, QD_BLINK_MS)) ? QD_LED_LIT : QD_LED_OFF;
    }
    if (u->nheld) {                                     /* the fields */
        const unsigned set = qd_step_fields_set(a, a->sel, u->held[0]);
        for (i = 0; i < 11; i++)
            out->key[QD_BLACK_KEY[i]] = i == u->field ? QD_LED_LIT : ((set >> i) & 1u) ? QD_LED_DIM : QD_LED_FAINT;
        if (u->undo_ok)
            out->key[QD_BLACK_KEY[UI_F_CLEAR]] = blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_FAINT;
        return;
    }
    for (i = 0; i < QD_NTRACK; i++) {                   /* the tracks; a track's key flashes on its trigs */
        uint8_t l = i == a->sel ? QD_LED_LIT : QD_LED_FAINT;
        if (qd_ctl_trig_flash(a, i))
            l = i == a->sel ? QD_LED_OFF : QD_LED_LIT;
        out->key[QD_BLACK_KEY[i]] = l;
    }
    out->key[QD_BLACK_KEY[8]] = page > 0 ? QD_LED_MID : QD_LED_OFF;
    out->key[QD_BLACK_KEY[9]] = page + 1 < qd_ctl_npages(a, a->sel) ? QD_LED_MID : QD_LED_OFF;
}

static void leds_keys(const qd_app_t *a, qd_leds_t *out)
{
    uint8_t notes[128];
    int i;
    qd_ctl_sounding(a, notes);
    for (i = 0; i < QD_NKEY; i++) {
        const int n = QD_NOTE0 + i + 12 * a->oct;
        if (((a->keys_held >> i) & 1u) || (n >= 0 && n < 128 && notes[n]))
            out->key[i] = QD_LED_LIT;                   /* held, or its note sounding on the track */
        else if ((QD_NOTE0 + i) % 12 == 0)
            out->key[i] = QD_LED_FAINT;                 /* the C keys */
    }
    if (a->ui.rec && !a->seq.playing)                   /* step recording: the cursor's place blinks */
        out->key[QD_WHITE_KEY[a->ui.cursor % QD_PAGE]] = blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF;
}

static void leds_tracks(const qd_app_t *a, qd_leds_t *out)
{
    int i;
    for (i = 0; i < QD_NTRACK; i++) {
        out->key[QD_WHITE_KEY[i]] = qd_ctl_trig_flash(a, i) ? QD_LED_LIT : i == a->sel ? QD_LED_DIM : QD_LED_OFF;
        if (!(qd_seq_track_pat(&a->seq, i)->trk[i].mute || ((a->seq.mute >> i) & 1u)))
            out->key[QD_WHITE_KEY[QD_NTRACK + i]] = QD_LED_LIT;
        out->key[QD_BLACK_KEY[i]] = i == a->sel ? QD_LED_LIT : QD_LED_FAINT;
    }
}

static void leds_pattern(const qd_app_t *a, qd_leds_t *out)
{
    const int q = qd_seq_queued(&a->seq);
    int i;
    for (i = 0; i < 16; i++) {
        const int k = QD_WHITE_KEY[i];
        if (i == a->seq.cur)
            out->key[k] = QD_LED_LIT;
        else if (i == q)
            out->key[k] = blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF;
        else if (a->proj.pat[i].used)
            out->key[k] = QD_LED_DIM;
    }
}

void qd_ctl_leds(const qd_app_t *a, qd_leds_t *out)
{
    const qd_ui_t *u = &a->ui;
    int i;
    memset(out, 0, sizeof *out);
    if (held(a, QD_B_PATTERN))
        leds_pattern(a, out);
    else if (u->mode == QD_MODE_STEP)
        leds_step(a, out);
    else if (u->mode == QD_MODE_KEYS)
        leds_keys(a, out);
    else if (u->mode == QD_MODE_TRACKS)
        leds_tracks(a, out);
    if (held(a, QD_B_TRACKS)) {                         /* TRACKS held: the selected track's white key dim */
        const int k = QD_WHITE_KEY[a->sel];
        if (out->key[k] < QD_LED_DIM)
            out->key[k] = QD_LED_DIM;
    }
    for (i = 0; i < QD_NBTN; i++)
        out->button[i] = held(a, i) ? QD_LED_LIT : QD_LED_OFF;
    if (u->func_latch)
        out->button[QD_B_FUNC] = QD_LED_LIT;
    if (u->mode == QD_MODE_KEYS)
        out->button[QD_B_KEYS] = QD_LED_LIT;
    if (u->mode == QD_MODE_TRACKS)
        out->button[QD_B_TRACKS] = QD_LED_LIT;
    if (u->rec)
        out->button[QD_B_REC] = QD_LED_LIT;
    if (a->playing) {
        const double into_beat_ms = (a->beats - floor(a->beats)) * 60000.0 / (double)a->bpm;
        out->play_green = into_beat_ms < (double)QD_BEAT_LED_MS ? QD_LED_LIT : QD_LED_OFF;
    }
}
