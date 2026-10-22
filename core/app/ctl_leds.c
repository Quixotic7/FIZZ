/* SPDX-License-Identifier: GPL-3.0-only */
/* The LEDs (ctl.h; PLAN §8b), five levels: lit, mid, dim, faint, off (RemoteFM1 maps them to the unit's three).
 *
 *   step view       white: trigs lit, LOCK steps dim, empty steps faint, steps outside the page's start..end off;
 *                   playing, the playhead's key inverts for its step; a key going down shows at once what its tap
 *                   makes. black, nothing held: B1..B11 the hold modes (B11 P-LOCK), the current one lit, the modes
 *                   some step of the page overrides dim (P-LOCK: a step with p-locks), the others faint
 *   a step held     the held steps blink fast; B1..B10 the mode's value keys: the step's value lit, the track's
 *                   default dim, the others faint (keys that do nothing off); B11 lit when the step overrides the mode
 *   F1 held         (§8b 2.1) B1..B8 the pages: the one edited lit, the others in the loop dim, out of it off, the
 *                   playing one flashing (fast), a page copied (B10) blinking; B9 faint, B10 faint (lit once a page is
 *                   copied: B1..B8 paste), B11 dim while there is an undo
 *   F2 held         the step view's whites (the pick-up / drop works on them); the black keys off
 *   F3 held         white: the page's start..end lit (the others off); B1..B7 the bases, B8..B10 the types: the
 *                   track's base and type lit, the others faint; B11 off
 *   SEL held        (P1.3c) B1..B8 the tracks: the selected lit, the others dim; W1..W8 lit = heard, off = not,
 *                   blinking = soloed; B11 lit while any mute or solo is set
 *   SEQ held        (P1.3c) the patterns with content dim, the playing one lit, the queued one blinking; P1.3d: the
 *                   bank shown's (SEQ + B1..B4), B1..B4 the banks: the one shown lit, one with content dim
 *   P1.3e           a mode key held (the track's default): it lit, B11 lit while the default differs from the initial
 *                   value, the other black keys off
 *   P1.3d           B10 off in the step view (reserved); LFO lit while the step latch is on (its selection blinks as
 *                   held steps do); a latched menu (F3): OCT- lit (back), OCT+ lit only when it confirms
 *   OCT- / OCT+     (P1.3c) where they are an octave (NOTE with a step held: octave 4 the centre; the keyboard: 0;
 *                   SOUND with a step held: the first bank): none blinks at the centre; below it OCT- blinks, above it
 *                   OCT+, a half period of 500 ms one away, 250 two, 125 three or more
 *   keyboard        (P1.5b) the scale: its roots 80 %, its other notes 50 %, the notes sounding lit; step recording:
 *                   the cursor's white key blinks. The sound editor's keyboard the same (SCALE: the W keys)
 *   tracks screen   white 1..8 flash with the tracks' trigs (the selected dim), 9..16 lit for the tracks heard
 *   buttons         held: lit; REC lit in the keyboard (blinking while armed); ARP lit while FILL is on; PAGE lit
 *                   while follow is on; TRACKS lit on its screen; PLAY's green LED on each beat while playing */
#include <math.h>
#include <string.h>

#include "app.h"
#include "ui.h"

#define QD_TRIG_LED_MS_ 80u

static int held(const qd_app_t *a, int b) { return (int)((a->buttons_held >> b) & 1u); }
static int blink(const qd_app_t *a, uint32_t half) { return (int)((a->ms / half) & 1u) == 0; }
/* P1.5n: a slow breath between 60 % and full (QD_LED_BREATH_MS a cycle; full at its start) */
uint8_t qd_led_breath(uint32_t ms)
{
    const float u = (float)(ms % QD_LED_BREATH_MS) / (float)QD_LED_BREATH_MS;
    return (uint8_t)(153.f + 102.f * (0.5f + 0.5f * cosf(u * 6.2831853f)) + 0.5f);
}

/* P1.5x: a deeper breath, 30 % .. full over QD_LED_BREATH_MS (the browser's B2..B8: the chosen preset's tags) */
uint8_t qd_led_breath_lo(uint32_t ms)
{
    const float u = (float)(ms % QD_LED_BREATH_MS) / (float)QD_LED_BREATH_MS;
    return (uint8_t)(77.f + 178.f * (0.5f + 0.5f * cosf(u * 6.2831853f)) + 0.5f);
}

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

/* P1.5a (the mock-up's key strips; P1.5t: the browser's are ctl_presets.c's): the name editor and SAVE PRESET (P1.5u:
 * the typewriter): all 27 keys lit, the ones held bright; the projects: W1..W8 (one dim, the one worked on lit, the
 * chosen blinking) */
static void leds_lib(const qd_app_t *a, qd_leds_t *out)
{
    const qd_ui_t *u = &a->ui;
    int i;
    for (i = 0; i < 16; i++)
        out->key[QD_WHITE_KEY[i]] = QD_LED_OFF;
    for (i = 0; i < 11; i++)
        out->key[QD_BLACK_KEY[i]] = QD_LED_OFF;
    if (u->latch == QD_LATCH_BROWSE) {                  /* P1.5t: the kinds, the source, + / - POOL, delete */
        qd_br_leds(a, out->key);
        return;
    }
    if (u->latch == QD_LATCH_SAVE) {
        qd_sv_leds(a, out->key);
        return;
    }
    if (u->latch == QD_LATCH_NAME) {                    /* P1.5u: the typewriter: all 27 lit, the ones held bright */
        qd_type_leds(a, out->key);
        return;
    }
    if (u->latch == QD_LATCH_PRJ) {
        out->key[QD_BLACK_KEY[9]] = QD_LED_LIT;
        if (u->pj.page == 1) {
            for (i = 0; i < 8; i++)
                out->key[QD_WHITE_KEY[i]] = i == a->per.proj ? QD_LED_LIT : i == u->pj.chosen ?
                                            (blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF)
                                          : a->per.pexists[i] ? QD_LED_DIM : QD_LED_FAINT;
            out->key[QD_BLACK_KEY[10]] = u->pj.b11_down ? QD_LED_LIT : QD_LED_DIM;
        }
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
    if (qd_lib_open(a)) {                               /* P1.5a: the sounds page, the name editor, the projects */
        leds_lib(a, out);
        return;
    }
    if (QD_LATCH_PANEL(u->latch)) {                     /* P1.4b: the track settings, the mix, the effects */
        uint8_t bk[11];
        qd_panel_leds(a, bk);
        for (i = 0; i < 11; i++)
            out->key[QD_BLACK_KEY[i]] = bk[i];
        for (i = 0; i < 16; i++)                        /* (P1.5b: the effects' overview: W1..W8 the tracks) */
            out->key[QD_WHITE_KEY[i]] = u->latch == QD_LATCH_FX && QD_FX_PAGES[u->fx_page].fx == QD_FXG_OVER &&
                                        i < QD_NTRACK ? (i == a->sel ? QD_LED_LIT : QD_LED_DIM) : QD_LED_OFF;
        return;
    }
    if (qd_snd_open(a)) {                               /* P1.4a: the sound editor: its groups, B11 the page's locks */
        uint8_t bk[11];
        qd_snd_leds(a, bk);
        for (i = 0; i < 11; i++)
            out->key[QD_BLACK_KEY[i]] = bk[i];
        if (u->ekeys) {                                 /* P1.5b: its keyboard: the scale (SCALE: the W keys; FULL:
                                                         * every key) */
            uint8_t k[QD_NKEY];
            memset(k, 0, sizeof k);
            qd_keys_leds(a, k);
            for (i = 0; i < QD_NKEY; i++)
                if (qd_ctl_white(i) >= 0 || a->proj.rec_ekeys)
                    out->key[i] = k[i];
        }
        return;
    }

    if (u->nheld && !qd_ctl_latch_modes(a) && !(u->slatch && layer != QD_LAYER_NONE)) {   /* the mode's ten values, B11
                                                         * (P1.5m: the latch's selection under an F layer: the layer's;
                                                         * P1.5b: the latch in MODES:
                                                         * the modes, below) */
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
            if (i == u->pg_copied)                      /* the page copied: blinking (B1..B8 paste it) */
                l = blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF;
            else if (i == pp)                           /* the playing page: flashing */
                l = blink(a, QD_FAST_MS) ? QD_LED_LIT : l == QD_LED_LIT ? QD_LED_DIM : QD_LED_OFF;
            out->key[QD_BLACK_KEY[i]] = l;
        }
        out->key[QD_BLACK_KEY[8]] = QD_LED_FAINT;
        out->key[QD_BLACK_KEY[9]] = u->pg_copied >= 0 ? QD_LED_LIT : QD_LED_FAINT;
        out->key[QD_BLACK_KEY[10]] = qd_clip_can_undo(&u->clip, a->seq.cur, a->sel) ? QD_LED_DIM : QD_LED_OFF;
        return;
    }
    if (layer == QD_LAYER_F2) {                         /* the pick-up / drop: the whites; the blacks off */
        for (i = 0; i < 11; i++)
            out->key[QD_BLACK_KEY[i]] = QD_LED_OFF;
        return;
    }
    if (layer == QD_LAYER_F3) {                         /* the page's range, the rate's base and type */
        for (i = 0; i < 16; i++)
            out->key[QD_WHITE_KEY[i]] = i >= ts->pstart[page] && i <= ts->pend[page] ? QD_LED_LIT : QD_LED_OFF;
        for (i = 0; i < QD_NVAL + QD_NRTYPE; i++)
            out->key[QD_BLACK_KEY[i]] = (i < QD_NVAL ? i == ts->rbase : i - QD_NVAL == ts->rtype) ? QD_LED_LIT
                                                                                                  : QD_LED_FAINT;
        out->key[QD_BLACK_KEY[10]] = QD_LED_OFF;
        return;
    }
    if (qd_ctl_mkey(a) >= 0) {                          /* P1.3e: a mode key held: it lit, B11 lit while its default
                                                         * differs from the initial one, the rest off */
        const int m = qd_ctl_mkey(a);
        for (i = 0; i < 10; i++)
            out->key[QD_BLACK_KEY[i]] = i == m ? QD_LED_LIT : QD_LED_OFF;
        out->key[QD_BLACK_KEY[10]] = qd_step_default_initial(a, m) ? QD_LED_OFF : QD_LED_LIT;
        return;
    }
    {   /* the hold modes */
        const unsigned set = qd_step_modes_set(a, a->sel, page);
        for (i = 0; i < UI_NFIELD; i++)                 /* (B11: P-LOCK, P1.3c; P1.3w: B9 FUNC, B10 SOUND) */
            out->key[QD_BLACK_KEY[i]] = i == u->hmode ? QD_LED_LIT : ((set >> i) & 1u) ? QD_LED_DIM : QD_LED_FAINT;
    }
}

static void leds_keys(const qd_app_t *a, qd_leds_t *out)
{
    qd_keys_leds(a, out->key);                          /* P1.5b: the scale (roots 80 %, its notes 50 %, sounding full) */
    if (qd_rec_stepping(a)) {                           /* step recording: the cursor's place blinks */
        const int s = qd_track_loop_step(&qd_ctl_cpat(a)->trk[a->sel], a->ui.cursor);
        out->key[QD_WHITE_KEY[s % QD_PAGE]] = blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF;
    }
}


/* SEL held (§8b 2.1): B1..B8 the tracks (selected lit, the others dim); W1..W8 lit = heard, off = not (muted, or
 * another track soloed), blinking = soloed; W9..W16 off; B11 lit while any mute or solo is set */
static void leds_sel(const qd_app_t *a, qd_leds_t *out)
{
    int i;
    for (i = 0; i < QD_NTRACK; i++) {
        uint8_t l = qd_seq_audible(&a->seq, qd_seq_track_pat(&a->seq, i), i) ? QD_LED_LIT : QD_LED_OFF;
        if ((a->seq.solo >> i) & 1u)
            l = blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF;
        out->key[QD_BLACK_KEY[i]] = i == a->sel ? QD_LED_LIT : QD_LED_DIM;
        out->key[QD_WHITE_KEY[i]] = l;
    }
    out->key[QD_BLACK_KEY[10]] = a->seq.mute || a->seq.solo ? QD_LED_LIT : QD_LED_OFF;
}

/* SEQ held: the bank shown's patterns on W1..W16; P1.3d: B1..B4 the banks, the one shown lit, one with content dim */
static void leds_pattern(const qd_app_t *a, qd_leds_t *out)
{
    const int q = qd_seq_queued(&a->seq), bank = a->ui.seq_bank;
    int i, b;
    for (i = 0; i < 16; i++) {
        const int k = QD_WHITE_KEY[i], pat = bank * 16 + i;
        if (pat == a->seq.cur)
            out->key[k] = QD_LED_LIT;
        else if (pat == q)
            out->key[k] = blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF;
        else if (a->proj.pat[pat].used)
            out->key[k] = QD_LED_DIM;
    }
    for (b = 0; b < QD_NBANK; b++) {
        int used = 0;
        for (i = 0; i < 16; i++)
            used |= a->proj.pat[b * 16 + i].used;
        out->key[QD_BLACK_KEY[b]] = b == bank ? QD_LED_LIT : used ? QD_LED_DIM : QD_LED_OFF;
    }
}

void qd_ctl_leds(const qd_app_t *a, qd_leds_t *out)
{
    const qd_ui_t *u = &a->ui;
    int i;
    memset(out, 0, sizeof *out);
    if (held(a, QD_B_MIX) && u->mode != QD_MODE_IDLE)  /* printed SEQ: the patterns */
        leds_pattern(a, out);
    else if (held(a, QD_B_PATTERN) && u->mode != QD_MODE_IDLE)   /* printed SEL: the tracks */
        leds_sel(a, out);
    else if (u->mode == QD_MODE_STEP)
        leds_step(a, out);
    else if (u->mode == QD_MODE_KEYS)
        leds_keys(a, out);
    if (held(a, QD_B_FUNC) && u->mode == QD_MODE_STEP && (!u->latch || u->latch == QD_LATCH_F3))
        qd_shift_leds(a, out->key);                     /* P1.5c: SHIFT held: only its shortcuts lit */
    if (held(a, QD_B_TRACKS)) {                         /* TRACKS held: the selected track's white key dim */
        const int k = QD_WHITE_KEY[a->sel];
        if (out->key[k] < QD_LED_DIM)
            out->key[k] = QD_LED_DIM;
    }
    for (i = 0; i < QD_NBTN; i++)
        out->button[i] = held(a, i) ? QD_LED_LIT : QD_LED_OFF;
    {   /* the OCT keys where they are an octave: blinking by the distance from the centre */
        int shown;
        const int off = qd_ctl_oct_offset(a, &shown);
        if (shown && off)
            out->button[off < 0 ? QD_B_OCTDN : QD_B_OCTUP] =
                blink(a, qd_ctl_oct_blink_ms(off)) ? QD_LED_LIT : QD_LED_OFF;
    }
    if (u->mode == QD_MODE_KEYS)                        /* REC: lit in the keyboard, blinking while armed */
        out->button[QD_B_REC] = !u->rec || blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF;
    if (a->seq.fill)
        out->button[QD_B_KEYS] = QD_LED_LIT;            /* ARP: FILL */
    if (u->latch == QD_LATCH_MIX)                       /* P1.4b: ENV (MIX), FX, SEL lit while their pages are open */
        out->button[QD_B_PAGE] = QD_LED_LIT;
    if (u->latch == QD_LATCH_FX)
        out->button[QD_B_TRACKS] = QD_LED_LIT;
    if (u->latch == QD_LATCH_TRK)
        out->button[QD_B_PATTERN] = QD_LED_LIT;
    if (u->slatch)                                      /* P1.3d: LFO lit while the step latch is on (P1.5n: full in
                                                         * VALUES; MODES breathing 60 .. 100 %, a 1.5 s period: the
                                                         * owner found P1.5b's half too dim) */
        out->button[QD_B_MUTE] = u->lmode == QD_LMODE_MODES ? qd_led_breath(a->ms) : QD_LED_LIT;
    if (qd_snd_open(a))                                 /* P1.4a: EDIT lit while the sound editor is open */
        out->button[QD_B_SOUND] = QD_LED_LIT;
    if (qd_latch_cur(a)) {                              /* P1.3d: a latched menu: OCT- (back) lit, OCT+ lit when it
                                                         * confirms something */
        out->button[QD_B_OCTDN] = QD_LED_LIT;
        out->button[QD_B_OCTUP] = qd_latch_cur(a)->confirm ? QD_LED_LIT : QD_LED_OFF;
    }
    if (u->latch == QD_LATCH_BROWSE) {                  /* P1.5t: F1 (cancel) and F2 (load) lit */
        out->button[QD_B_OCTDN] = QD_LED_LIT;
        out->button[QD_B_OCTUP] = QD_LED_LIT;
    } else if (u->latch == QD_LATCH_SAVE) {             /* .. SAVE blinking, F1 lit (delete), F2 lit with REPLACE
                                                         * offered (P1.5v: it switches NEW / REPLACE) */
        out->button[QD_B_SAVE] = blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_DIM;
        out->button[QD_B_OCTDN] = QD_LED_LIT;
        out->button[QD_B_OCTUP] = u->sv.replace >= 0 ? QD_LED_LIT : QD_LED_OFF;
    } else if (u->latch == QD_LATCH_PRJ) {
        out->button[QD_B_MIX] = QD_LED_LIT;
        out->button[QD_B_OCTUP] = u->pj.page == 1 && u->pj.chosen != a->per.proj
                                      ? (blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_OFF) : QD_LED_OFF;
    } else if (u->latch == QD_LATCH_NAME) {             /* P1.5u: SAVE blinking (keep), F1 lit (delete) */
        out->button[QD_B_SAVE] = blink(a, QD_BLINK_MS) ? QD_LED_LIT : QD_LED_DIM;
        out->button[QD_B_OCTDN] = QD_LED_LIT;
        out->button[QD_B_OCTUP] = QD_LED_OFF;
    }
    if (a->playing) {
        const double into_beat_ms = (a->beats - floor(a->beats)) * 60000.0 / (double)a->bpm;
        out->play_green = into_beat_ms < (double)QD_BEAT_LED_MS ? QD_LED_LIT : QD_LED_OFF;
    }
}
