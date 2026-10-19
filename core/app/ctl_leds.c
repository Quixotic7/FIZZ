/* SPDX-License-Identifier: GPL-3.0-only */
/* The LEDs (ctl.h; PLAN §8b), five levels: lit, mid, dim, faint, off (RemoteFM1 maps them to the unit's three).
 *
 *   step view       white: trigs lit, LOCK steps dim, empty steps faint, steps outside the page's start..end off;
 *                   playing, the playhead's key inverts for its step; a key going down shows at once what its tap
 *                   makes. black, nothing held: B1..B10 the hold modes, the current one lit, the modes some step of
 *                   the page overrides dim, the others faint; B11 off
 *   a step held     the held steps blink fast; B1..B10 the mode's value keys: the step's value lit, the track's
 *                   default dim, the others faint (keys that do nothing off); B11 lit when the step overrides the mode
 *   F1 held         B1..B8 the pages: the one edited lit, the others in the loop dim, the playing one blinking;
 *                   B9 / B10 faint, B11 dim while there is an undo
 *   F2 held         B1..B8 the tracks: the selected lit, the others heard dim, muted off, soloed blinking; B9 faint,
 *                   B10 dim while a page is copied
 *   F3 held         white: the page's start..end lit (the others off); B1..B9 the rate: the current lit, the others
 *                   faint
 *   keyboard        the notes sounding lit, the C keys faint; step recording: the cursor's white key blinks
 *   tracks screen   white 1..8 flash with the tracks' trigs (the selected dim), 9..16 lit for the tracks heard
 *   PATTERN held    the patterns with content dim, the playing one lit, the queued one blinking
 *   buttons         held: lit; REC lit in the keyboard (blinking while armed); ARP lit while FILL is on; PAGE lit
 *                   while follow is on; TRACKS lit on its screen; PLAY's green LED on each beat while playing */
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
    const qd_pattern_t *p = qd_ctl_cpat(a);
    int ty;
    if (!qd_page_in_range(&p->trk[a->sel], s))
        return QD_LED_OFF;
    ty = qd_step_type(p, a->sel, s);
    return ty == QD_TRIG_LOCK ? QD_LED_DIM : ty ? QD_LED_LIT : QD_LED_FAINT;
}

static void leds_whites(const qd_app_t *a, qd_leds_t *out)
{
    const qd_ui_t *u = &a->ui;
    const int page = u->page[a->sel];
    int w, i;
    for (w = 0; w < 16; w++) {
        const int s = page * QD_PAGE + w;
        uint8_t l = step_level(a, s);
        if ((u->wk_pending >> w) & 1u && !u->nheld)     /* going down: what its tap makes, at once */
            l = qd_step_is_on(qd_step(qd_ctl_cpat(a), a->sel, s)) ? QD_LED_FAINT : QD_LED_LIT;
        out->key[QD_WHITE_KEY[w]] = l;
    }
    if (a->seq.playing) {                               /* the playhead's key inverts for its step */
        const int s = qd_seq_track_step(&a->seq, a->sel);
        if (s >= 0 && s / QD_PAGE == page) {
            const int k = QD_WHITE_KEY[s % QD_PAGE];
            out->key[k] = out->key[k] >= QD_LED_MID ? QD_LED_OFF : QD_LED_LIT;
        }
    }
    for (i = 0; i < u->nheld; i++) {                    /* held: a fast blink */
        const int s = u->held[i];
        if (s / QD_PAGE == page)
            out->key[QD_WHITE_KEY[s % QD_PAGE]] = blink(a, QD_FAST_MS) ? QD_LED_LIT : QD_LED_OFF;
    }
}

static void leds_step(const qd_app_t *a, qd_leds_t *out)
{
    const qd_ui_t *u = &a->ui;
    const qd_pattern_t *p = qd_ctl_cpat(a);
    const qd_tset_t *ts = &p->trk[a->sel];
    const int page = u->page[a->sel], layer = qd_ctl_layer(a);
    int i;
    leds_whites(a, out);
    if (u->nheld) {                                     /* the mode's ten values, B11 */
        char lab[10][6];
        uint16_t lit, def, none;
        int clr;
        qd_step_value_keys(a, lab, &lit, &def, &none, &clr);
        for (i = 0; i < 10; i++)
            out->key[QD_BLACK_KEY[i]] = ((lit >> i) & 1u) ? QD_LED_LIT : ((def >> i) & 1u) ? QD_LED_DIM
                                      : ((none >> i) & 1u) ? QD_LED_OFF : QD_LED_FAINT;
        out->key[QD_BLACK_KEY[10]] = clr ? QD_LED_LIT : QD_LED_OFF;
        return;
    }
    if (layer == QD_LAYER_F1) {                         /* the pages */
        const int loop = qd_track_loop(ts), pp = qd_ctl_playing_page(a, a->sel);
        for (i = 0; i < QD_NPAGE; i++) {
            uint8_t l = i == page ? QD_LED_LIT : ((loop >> i) & 1) ? QD_LED_DIM : QD_LED_OFF;
            if (i == pp)
                l = blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF;
            out->key[QD_BLACK_KEY[i]] = l;
        }
        out->key[QD_BLACK_KEY[8]] = out->key[QD_BLACK_KEY[9]] = QD_LED_FAINT;
        out->key[QD_BLACK_KEY[10]] = qd_clip_can_undo(&u->clip, a->seq.cur, a->sel) ? QD_LED_DIM : QD_LED_OFF;
        return;
    }
    if (layer == QD_LAYER_F2) {                         /* the tracks */
        for (i = 0; i < QD_NTRACK; i++) {
            uint8_t l = i == a->sel ? QD_LED_LIT : qd_seq_audible(&a->seq, qd_seq_track_pat(&a->seq, i), i)
                                                       ? QD_LED_DIM : QD_LED_OFF;
            if ((a->seq.solo >> i) & 1u)
                l = blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF;
            out->key[QD_BLACK_KEY[i]] = l;
        }
        out->key[QD_BLACK_KEY[8]] = QD_LED_FAINT;
        out->key[QD_BLACK_KEY[9]] = u->clip.pg.valid ? QD_LED_DIM : QD_LED_OFF;
        return;
    }
    if (layer == QD_LAYER_F3) {                         /* the page's range, the rate */
        const int ri = qd_ctl_rate_index(ts->rate);
        for (i = 0; i < 16; i++)
            out->key[QD_WHITE_KEY[i]] = i >= ts->pstart[page] && i <= ts->pend[page] ? QD_LED_LIT : QD_LED_OFF;
        for (i = 0; i < QD_NRATE; i++)
            out->key[QD_BLACK_KEY[i]] = i == ri ? QD_LED_LIT : QD_LED_FAINT;
        return;
    }
    {   /* the hold modes */
        const unsigned set = qd_step_modes_set(a, a->sel, page);
        for (i = 0; i < UI_NFIELD; i++)
            out->key[QD_BLACK_KEY[i]] = i == u->hmode ? QD_LED_LIT : ((set >> i) & 1u) ? QD_LED_DIM : QD_LED_FAINT;
        out->key[QD_BLACK_KEY[10]] = QD_LED_OFF;
    }
}

static void leds_keys(const qd_app_t *a, qd_leds_t *out)
{
    uint8_t notes[128];
    int i;
    qd_ctl_sounding(a, notes);
    for (i = 0; i < QD_NKEY; i++) {
        const int n = qd_rec_key_note(a, i);
        if (((a->keys_held >> i) & 1u) || (n >= 0 && n < 128 && notes[n]))
            out->key[i] = QD_LED_LIT;                   /* held, or its note sounding on the track */
        else if ((QD_NOTE0 + i) % 12 == 0)
            out->key[i] = QD_LED_FAINT;                 /* the C keys */
    }
    if (a->ui.rec && !a->seq.playing) {                 /* step recording: the cursor's place blinks */
        const int s = qd_track_loop_step(&qd_ctl_cpat(a)->trk[a->sel], a->ui.cursor);
        out->key[QD_WHITE_KEY[s % QD_PAGE]] = blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF;
    }
}

static void leds_tracks(const qd_app_t *a, qd_leds_t *out)
{
    int i;
    for (i = 0; i < QD_NTRACK; i++) {
        out->key[QD_WHITE_KEY[i]] = qd_ctl_trig_flash(a, i) ? QD_LED_LIT : i == a->sel ? QD_LED_DIM : QD_LED_OFF;
        if (qd_seq_audible(&a->seq, qd_seq_track_pat(&a->seq, i), i))
            out->key[QD_WHITE_KEY[QD_NTRACK + i]] = QD_LED_LIT;
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
    if (u->mode == QD_MODE_KEYS)                        /* REC: lit in the keyboard, blinking while armed */
        out->button[QD_B_REC] = !u->rec || blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF;
    if (a->seq.fill)
        out->button[QD_B_KEYS] = QD_LED_LIT;            /* ARP: FILL */
    if (u->follow && u->mode != QD_MODE_IDLE)
        out->button[QD_B_PAGE] = QD_LED_LIT;
    if (u->mode == QD_MODE_TRACKS)
        out->button[QD_B_TRACKS] = QD_LED_LIT;
    if (a->playing) {
        const double into_beat_ms = (a->beats - floor(a->beats)) * 60000.0 / (double)a->bpm;
        out->play_green = into_beat_ms < (double)QD_BEAT_LED_MS ? QD_LED_LIT : QD_LED_OFF;
    }
}
