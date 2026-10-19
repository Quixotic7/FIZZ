/* SPDX-License-Identifier: GPL-3.0-only */
/* The frame (ctl.h): the screens' view records (ui.h) filled from the model and the UI state, then drawn. The screen
 * under everything is the mode's: idle; the step view (the page as the hold mode's lane, PLAN §8b), a held step's
 * screen for the mode (with the ten values at its foot; P-LOCK keeps the lane), a layer's map while F1 / F2 / F3 is
 * held, SHIFT's map while SHIFT is held alone; the keyboard; the tracks. P1.3c: SEL held, the track map; SEQ held, the
 * pattern map (over any mode's screen). A change of octave or SOUND bank: the octave takeover. Over it a knob's takeover (PLAN §9a rule 5: shown at the first
 * detent, 1 s after the last one it fades back over 120 ms) and a confirmation word. Nothing here changes the model or
 * the UI state; the motion is the app's milliseconds and the transport's position. */
#include <math.h>
#include <string.h>

#include "app.h"
#include "ui.h"
#include "../seq/chord.h"
#include "../seq/scale.h"
#include "../seq/sounds.h"

#define QD_ROLL_MS 150.f               /* a picker's roll at a detent */

/* ---------------------------------------------------------------- numbers as text (no stdio) --- */
static char *cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
    return d;
}

/* v in hundredths: "38.50", "82" when whole, "-4.5" with one decimal when dec 1 */
static void fmt_hundredths(char *out, int v, int dec)
{
    char n[12];
    const int neg = v < 0, a = neg ? -v : v;
    out[0] = 0;
    if (neg)
        cat(out, "-");
    cat(out, ui_itoa(n, a / 100));
    if (dec == 2 && a % 100) {
        n[0] = '.', n[1] = (char)('0' + a % 100 / 10), n[2] = (char)('0' + a % 10), n[3] = 0;
        cat(out, n);
    } else if (dec == 1) {
        n[0] = '.', n[1] = (char)('0' + (a % 100 + 5) / 10 % 10), n[2] = 0;
        cat(out, n);
    }
}

static float range01(int id, int v)
{
    int16_t lo = 0, hi = 1;
    qd_lock_range(id, &lo, &hi);
    return hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.f;
}

static float secs(uint32_t ms) { return (float)ms / 1000.f; }

static void cond_short(int cond, char *out)            /* "" ALWAYS, "FILL", "!FIL", "PRE", "3:4" */
{
    char b[8];
    qd_cond_name(cond, b);
    out[0] = 0;
    if (cond == QD_COND_ALWAYS)
        return;
    if (cond == QD_COND_NFILL)
        strcpy(b, "!FIL");
    strncpy(out, b, 5);
    out[5] = 0;
}

static const char *sound_name(const qd_app_t *a, int k)   /* a sound lock's value 0..128 */
{
    if (k <= 0)
        return "TRACK SOUND";
    return qd_sound_name(&a->proj, k);
}

static int len_steps_x8(int len) { return qd_len_eighths(len); }

/* ---------------------------------------------------------------- the step view's lane --- */
static int mode_pm(int m)                               /* the step parameter a hold mode edits */
{
    static const int PM[UI_F_B10] = {QD_PM_NOTE, QD_PM_VEL, QD_PM_LEN, QD_PM_MICRO, QD_PM_RATCH, QD_PM_CHANCE,
                                     QD_PM_COND, QD_PM_TRIG, QD_PM_SOUND};
    return m >= 0 && m < UI_F_B10 ? PM[m] : QD_PM_NOTE;
}

static void fill_lane(qd_app_t *a, ui_lane_t *v, char *pat)
{
    const qd_ui_t *u = &a->ui;
    const int t = a->sel, page = u->page[t];
    const qd_pattern_t *P = qd_ctl_cpat(a);
    const qd_tset_t *ts = &P->trk[t];
    const qd_sdef_t *df = &ts->def;
    int i, j, lo = 128, hi = -1;
    memset(v, 0, sizeof *v);
    v->mode = u->hmode;
    v->track = t + 1;
    v->name = a->model.t[t].name;
    v->pattern = qd_pat_name(a->seq.cur, pat);
    v->bpm = a->bpm;
    v->page = page;
    v->loop = (uint8_t)qd_track_loop(ts);
    memcpy(v->pstart, ts->pstart, sizeof v->pstart);
    memcpy(v->pend, ts->pend, sizeof v->pend);
    v->follow = u->follow;
    v->latch = u->slatch;
    v->steps = u->view == QD_VIEW_STEPS;                /* P1.3f: the STEPS overview: every page's cells */
    {   /* P1.3j: the page edited unfolds (QD_UNFOLD_MS), the one before folds */
        const float k = a->ms - u->slide_ms >= QD_UNFOLD_MS ? 1.f
                                                            : ui_ease((float)(a->ms - u->slide_ms) / (float)QD_UNFOLD_MS);
        v->grow[page] = k;
        if (u->slide_from >= 0 && u->slide_from < 8 && u->slide_from != page)
            v->grow[u->slide_from] = 1.f - k;
    }
    v->play_step = v->flash_step = -1;
    if (v->steps) {
        int pg, k;
        for (pg = 0; pg < QD_NPAGE; pg++)
            for (k = 0; k < QD_PAGE; k++) {
                const int s = pg * QD_PAGE + k;
                const qd_step_t *st = qd_step(P, t, s);
                qd_rstep_t r;
                uint8_t c = 0;
                qd_step_resolve(P, t, s, &r);
                c |= qd_page_in_range(ts, s) ? UI_C_RANGE : 0;
                c |= r.on ? UI_C_ON : 0;
                c |= r.on && r.type == QD_TRIG_LOCK ? UI_C_LOCK : 0;
                c |= r.on && r.type == QD_TRIG_TIE ? UI_C_TIE : 0;
                c |= r.on && qd_step_overrides(st, QD_PM_NOTE) ? UI_C_OWN : 0;   /* P1.3q: its own notes */
                if (r.on && (u->hmode == UI_F_PLOCK ? (st->flags & QD_SF_LOCKS) != 0
                                                    : u->hmode < UI_F_B10 && (qd_step_overrides(st, mode_pm(u->hmode)) ||
                                                      (u->hmode == UI_F_MICRO && qd_step_overrides(st, QD_PM_STRUM)))))
                    c |= UI_C_OVR;
                for (j = 0; j < u->nheld; j++)
                    c |= u->held[j] == s ? UI_C_HELD : 0;
                v->cell[pg][k] = c;
            }
        if (a->seq.playing) {
            v->play_step = qd_seq_track_step(&a->seq, t);
            if (a->seq.flash_step[t] >= 0 && a->seq.flash_sample[t] <= a->seq.now) {
                v->flash_step = a->seq.flash_step[t];
                v->flash_age = (float)((double)(a->seq.now - a->seq.flash_sample[t]) / (double)QD_FS);
            }
        }
    }
    v->t = secs(a->ms);
    v->play_page = -1;
    v->pos = -1.f;
    if (a->seq.playing) {
        const double tp = qd_seq_track_pos(&a->seq, t);
        const int s = qd_seq_track_step(&a->seq, t);
        v->play_page = s / QD_PAGE;
        if (v->play_page == page)
            v->pos = (float)(s % QD_PAGE) + (float)(tp - floor(tp));
    }
    for (i = 0; i < 16; i++) {
        const int s = page * QD_PAGE + i;
        const qd_step_t *st = qd_step(P, t, s);
        ui_lcol_t *c = &v->col[i];
        qd_rstep_t r;
        int e;
        qd_step_resolve(P, t, s, &r);
        c->in_range = (uint8_t)qd_page_in_range(ts, s);
        for (j = 0; j < u->nheld; j++)
            c->held |= u->held[j] == s;
        c->on = (uint8_t)r.on;
        if (!r.on || !c->in_range)
            continue;
        c->type = (uint8_t)r.type;
        switch (u->hmode) {
        case UI_F_NOTE: c->ovr = qd_step_overrides(st, QD_PM_NOTE); break;
        case UI_F_VEL: c->ovr = qd_step_overrides(st, QD_PM_VEL); break;
        case UI_F_LEN: c->ovr = qd_step_overrides(st, QD_PM_LEN); break;
        case UI_F_MICRO: c->ovr = qd_step_overrides(st, QD_PM_MICRO) || qd_step_overrides(st, QD_PM_STRUM); break;
        case UI_F_RATCH: c->ovr = qd_step_overrides(st, QD_PM_RATCH); break;
        case UI_F_CHNC: c->ovr = qd_step_overrides(st, QD_PM_CHANCE); break;
        case UI_F_COND: c->ovr = qd_step_overrides(st, QD_PM_COND); break;
        case UI_F_TRIG: c->ovr = qd_step_overrides(st, QD_PM_TRIG); break;
        case UI_F_SOUND: c->ovr = qd_step_overrides(st, QD_PM_SOUND); break;
        default: c->ovr = (st->flags & QD_SF_LOCKS) != 0; break;
        }
        if (r.type != QD_TRIG_LOCK) {
            c->nnote = (uint8_t)r.nn;
            memcpy(c->note, r.notes, sizeof c->note);
        }
        e = len_steps_x8(r.len);
        c->len = e < 0 ? (float)(16 - i) : (float)e / 8.f;
        c->inf = e < 0;
        c->micro = (int8_t)r.micro;
        c->ratch = (uint8_t)r.ratch;
        c->ramp = (uint8_t)r.ramp;
        c->vel = (uint8_t)r.vel;
        c->chance = (uint8_t)r.chance;
        cond_short(r.cond, c->cond);
        c->sound = (uint8_t)r.sound;
        c->nlock = (uint8_t)qd_lock_count(P, t, s);
        c->strum = (uint8_t)r.strum;
        if (v->pos >= 0.f) {
            const float x0 = (float)i + (float)r.micro / 24.f;
            c->lit = v->pos >= x0 && v->pos < x0 + (c->len > 0.3f ? c->len : 0.3f);
        }
        for (j = 0; j < c->nnote; j++) {
            lo = c->note[j] < lo ? c->note[j] : lo;
            hi = c->note[j] > hi ? c->note[j] : hi;
        }
    }
    /* the track's default for the mode */
    v->def_nnote = qd_notes_abs(df->note, v->def_note);
    if (u->hmode == UI_F_NOTE)
        for (j = 0; j < v->def_nnote; j++) {
            lo = v->def_note[j] < lo ? v->def_note[j] : lo;
            hi = v->def_note[j] > hi ? v->def_note[j] : hi;
        }
    v->def_vel = df->vel;
    {
        const int e = len_steps_x8(df->len);
        v->def_len = e < 0 ? 16.f : (float)e / 8.f;
    }
    v->def_micro = df->micro;
    v->def_ratch = qd_ratch_count(df->ratch);
    v->def_ramp = qd_ratch_ramp(df->ratch);
    v->def_chance = df->chance;
    v->def_type = df->type;
    cond_short(df->cond, v->def_cond);
    {
        char n[12], cn[8];
        static const char *const TY[4] = {"", "NOTE", "LOCK", "TIE"};
        v->def_label[0] = 0;
        switch (u->hmode) {
        case UI_F_NOTE: cat(cat(v->def_label, "DEFAULT "), qd_note_name(df->note[0], n)); break;
        case UI_F_VEL: cat(cat(v->def_label, "DEFAULT "), ui_itoa(n, df->vel)); break;
        case UI_F_LEN: cat(cat(v->def_label, "DEFAULT "), qd_len_name(df->len)); break;
        case UI_F_MICRO: cat(cat(v->def_label, "DEFAULT "), ui_itoa(n, df->micro)); break;
        case UI_F_RATCH: cat(cat(v->def_label, "DEFAULT x"), ui_itoa(n, v->def_ratch)); break;
        case UI_F_CHNC: cat(cat(cat(v->def_label, "DEFAULT "), ui_itoa(n, df->chance)), "%"); break;
        case UI_F_COND: cat(cat(v->def_label, "DEFAULT "), qd_cond_name(df->cond, cn)); break;
        case UI_F_TRIG: cat(cat(v->def_label, "DEFAULT "), TY[df->type & 3]); break;
        case UI_F_SOUND: cat(cat(v->def_label, "TRACK SOUND "), qd_track_sound_name(&a->model.t[t])); break;
        default: cat(v->def_label, "P-LOCKS"); break;
        }
    }
    if (hi < 0) {                                       /* no notes: a span round C4 */
        v->lo = 52;
        v->span = 16;
    } else {                                            /* the page's notes, centred, at least 16 semitones */
        v->span = hi - lo + 2 > 16 ? hi - lo + 2 : 16;
        v->lo = (lo + hi) / 2 - v->span / 2;
    }
    if (a->ms - u->slide_ms < QD_SLIDE_MS)
        v->slide = (1.f - ui_ease((float)(a->ms - u->slide_ms) / (float)QD_SLIDE_MS)) * (float)u->slide_dir;
}

/* ---------------------------------------------------------------- a held step's screen --- */
static const char *cond_text(int c)
{
    switch (c) {
    case QD_COND_ALWAYS: return "plays every pass";
    case QD_COND_FILL: return "plays while FILL is held";
    case QD_COND_NFILL: return "plays unless FILL is held";
    case QD_COND_PRE: return "plays when the last one did";
    case QD_COND_NPRE: return "plays when the last one did not";
    case QD_COND_NEI: return "plays when the track left did";
    case QD_COND_NNEI: return "plays when the track left did not";
    case QD_COND_1ST: return "plays on the first pass only";
    case QD_COND_N1ST: return "plays after the first pass";
    default: return 0;
    }
}

/* the screen of mode m for step s (is_default: the track's default, s ignored: a step with no overrides) */
static void fill_held(qd_app_t *a, ui_held_t *v, int is_default)
{
    static char scale_buf[24];
    const qd_ui_t *u = &a->ui;
    const int t = a->sel, s = is_default ? -1 : u->held[0], m = u->hmode;
    const qd_pattern_t *P = qd_ctl_cpat(a);
    const qd_tset_t *ts = &P->trk[t];
    const qd_step_t *st = qd_step(P, t, s < 0 ? 0 : s);
    qd_rstep_t r;
    int i, k;
    memset(v, 0, sizeof *v);
    if (is_default) {                                   /* the track's defaults: a step with no overrides */
        static qd_pattern_t tmp;
        tmp.trk[0] = *ts;
        memset(&tmp.step[0][0], 0, sizeof tmp.step[0][0]);
        tmp.step[0][0].flags = QD_SF_ON;
        qd_step_resolve(&tmp, 0, 0, &r);
        st = &tmp.step[0][0];
    } else {
        qd_step_resolve(P, t, s, &r);
    }
    v->field = m;
    v->is_default = is_default;
    v->step = s + 1;
    v->nheld = u->nheld;
    v->track = t + 1;
    v->name = a->model.t[t].name;
    v->type = r.type;
    v->play = !is_default && u->held_played ? secs(a->ms - u->held_play_ms) : -1.f;
    v->t = secs(a->ms);
    v->tracklen = qd_track_len(ts);
    v->latch = u->slatch;
    if (!is_default) {
        qd_step_value_keys(a, v->vk, &v->vk_lit, &v->vk_def, &v->vk_none, &v->clr);
        v->ovr = v->clr;
    } else {
        v->ovr = 1;
    }
    /* NOTE */
    v->nnote = r.nn;
    memcpy(v->note, r.notes, sizeof v->note);
    for (k = 0; k < v->nnote; k++)
        qd_note_name(v->note[k], v->note_name[k]);
    if (v->nnote && !qd_chord_name(v->note, v->nnote, v->root, v->qual)) {
        v->root[0] = 0;
        for (k = 0; k < v->nnote; k++) {
            if (k)
                cat(v->names, " ");
            cat(v->names, qd_pc_name(v->note[k]));
        }
    }
    for (k = 0; k < QD_NNOTE; k++)                      /* the notes by their knobs (P1.3c) */
        v->slot_note[k] = r.note[k];                    /* (P1.3f: the slots are pitches) */
    v->knobs = m != UI_F_NOTE ? 0 : is_default && qd_ctl_mkey(a) < 0 ? 2 : 1;   /* (nothing held: SELECT pages) */
    if (is_default && qd_ctl_mkey(a) >= 0)              /* P1.3e: the mode key held: B11 resets it */
        v->reset = qd_step_default_initial(a, m) ? 2 : 1;
    v->squeeze = secs(a->ms - u->chord_ms);
    v->entry_oct = u->entry_oct;
    {
        int sc, rt;
        qd_track_key(&a->proj, P, t, &sc, &rt);
        scale_buf[0] = 0;
        if (sc != QD_SCALE_CHROMATIC)
            cat(cat(scale_buf, qd_pc_name(rt)), " ");
        cat(scale_buf, qd_scale_name(sc));
        v->scale_name = scale_buf;
    }
    /* VEL, RATCH */
    v->vel = r.vel;
    v->ramp = r.ramp;
    v->ratch = v->count = r.ratch;
    v->ball = (float)(a->ms % 1600u) / 1600.f;
    /* LEN */
    v->len = r.len;
    v->len_name = qd_len_name(v->len);
    v->len_steps = qd_len_eighths(v->len) < 0 ? -1.f : (float)qd_len_eighths(v->len) / 8.f;
    v->tie = r.type == QD_TRIG_TIE;
    if (!is_default) {
        const int pos = qd_track_step_pos(ts, s), n = qd_track_len(ts);
        for (k = 1; k <= 16 && pos >= 0 && k < n; k++) {   /* the note before, in the loop */
            const int q = qd_track_loop_step(ts, pos - k);
            qd_rstep_t rq;
            qd_step_resolve(P, t, q, &rq);
            if (rq.type == QD_TRIG_NOTE || rq.type == QD_TRIG_TIE) {
                const int e = qd_len_eighths(rq.len);
                v->has_prev = 1;
                v->prev_at = (float)-k + (float)rq.micro / 24.f;
                v->prev_len = e < 0 ? (float)k : (float)e / 8.f;
                break;
            }
        }
        for (k = 1; k <= 8 && pos >= 0 && k < n; k++) {    /* the note after (on the ruler) */
            const int q = qd_track_loop_step(ts, pos + k);
            qd_rstep_t rq;
            qd_step_resolve(P, t, q, &rq);
            if (rq.type == QD_TRIG_NOTE || rq.type == QD_TRIG_TIE) {
                const int e = qd_len_eighths(rq.len);
                v->next_at = (float)k + (float)rq.micro / 24.f;
                v->next_len = e < 0 ? 2.f : (float)e / 8.f;
                break;
            }
        }
    }
    /* MICRO */
    v->micro = r.micro;
    v->strum = r.strum;
    v->walk = (float)(a->ms % 2400u) / 2400.f * 52.f - 14.f;
    /* COND / CHNC */
    qd_cond_name(r.cond, v->cond_name);
    v->cond_text = cond_text(r.cond);
    if (!qd_cond_ab_of(r.cond, &v->ab_a, &v->ab_b))
        v->ab_a = v->ab_b = 0;
    v->pass = -1;
    if (a->seq.playing && v->ab_b)
        v->pass = (int)(a->seq.cur_t[t].pass % v->ab_b);
    v->chance = r.chance;
    v->shuffle = (uint32_t)(a->seq.playing ? a->seq.cur_t[t].pass : 0) * 2654435761u + 7919u;
    v->chance_first = m == UI_F_CHNC;
    /* SOUND */
    v->sound = r.sound;
    v->sound_cur = is_default ? qd_track_sound_name(&a->model.t[t]) : sound_name(a, r.sound);
    v->sound_prev = r.sound > 0 ? sound_name(a, r.sound - 1) : 0;
    v->sound_next = r.sound < a->proj.nsound ? sound_name(a, r.sound + 1) : 0;
    if (a->ms - u->roll_ms < (uint32_t)QD_ROLL_MS)
        v->roll = 1.f - (float)(a->ms - u->roll_ms) / QD_ROLL_MS;
    v->dir = u->roll_dir;
    v->bank = u->sound_bank;
    if (!is_default) {
        for (i = 0; i < 16; i++) {
            qd_rstep_t rq;
            qd_step_resolve(P, t, (s / QD_PAGE) * QD_PAGE + i, &rq);
            v->page_sound[i] = (uint8_t)(rq.on ? rq.sound : 0);
        }
        v->held_col = s % QD_PAGE;
        v->nlocks = qd_lock_count(P, t, s);
    }
    (void)st;
}

/* ---------------------------------------------------------------- the layer maps, SHIFT's map --- */
static void fill_layer(qd_app_t *a, ui_layer_t *v, int layer)
{
    static const char *RN[1];
    const qd_ui_t *u = &a->ui;
    const int t = a->sel;
    const qd_pattern_t *P = qd_ctl_cpat(a);
    const qd_tset_t *ts = &P->trk[t];
    int i, s;
    memset(v, 0, sizeof *v);
    v->layer = layer;
    v->track = t + 1;
    v->name = a->model.t[t].name;
    v->t = secs(a->ms);
    v->blink = (a->ms / QD_BLINK_MS) % 2u == 0;
    v->page = u->page[t];
    v->loop = (uint8_t)qd_track_loop(ts);
    memcpy(v->pstart, ts->pstart, sizeof v->pstart);
    memcpy(v->pend, ts->pend, sizeof v->pend);
    v->play_page = qd_ctl_playing_page(a, t);
    for (i = 0; i < QD_NPAGE; i++)
        for (s = 0; s < QD_PAGE; s++)
            v->trig[i][s] = (uint8_t)qd_step_is_on(qd_step(P, t, i * QD_PAGE + s));
    v->can_undo = qd_clip_can_undo(&u->clip, a->seq.cur, t);
    v->step_clip = u->clip.st.valid;
    v->shift = qd_ctl_shift(a);
    v->page_copied = u->pg_copied;
    v->f1_copied = u->f1_copied;
    if (u->clip.st.valid) {                             /* the step in the clipboard (F1's copy, F2's hand) */
        qd_rstep_t r;
        static qd_pattern_t tmp;
        tmp.trk[0] = *ts;
        tmp.step[0][0] = u->clip.st.step;
        qd_step_resolve(&tmp, 0, 0, &r);
        v->clip_type = r.type;
        v->clip_nnote = r.type == QD_TRIG_LOCK ? 0 : r.nn;
        memcpy(v->clip_note, r.notes, sizeof v->clip_note);
    }
    v->hand = u->f2_loaded && u->f2_holding;
    v->hand_loaded = u->f2_loaded;
    v->last_w = u->f2_last_w;
    v->last_act = u->f2_last_act;
    for (s = 0; s < QD_PAGE; s++)
        v->page_on[s] = (uint8_t)qd_step_is_on(qd_step(P, t, u->page[t] * QD_PAGE + s));
    v->sel = t;
    v->mute = a->seq.mute;
    v->solo = a->seq.solo;
    for (i = 0; i < QD_NTRACK; i++) {
        v->tname[i] = a->model.t[i].name;
        v->flash[i] = (uint8_t)qd_ctl_trig_flash(a, i);
        if (qd_seq_track_pat(&a->seq, i)->trk[i].mute)
            v->mute |= (uint8_t)(1u << i);
    }
    v->held_track = -1;
    v->page_clip = u->clip.pg.valid;
    v->rate = qd_tset_rate(ts);
    v->rate_idx = -1;
    (void)RN;
    v->rate_val = ts->rbase;
    v->rate_type = ts->rtype;
    v->rate_val_name = qd_rate_val_name(v->rate_val);
    v->bpm = a->bpm;
    v->swing = P->swing;
    v->anchor = u->f3_anchor;
    v->latched = layer == 3 && u->latch == QD_LATCH_F3;
    v->ext = qd_seq_ext_running(&a->seq);
    if (u->tempo_ms && a->ms - u->tempo_ms < 1000u)
        v->tempo_hot = 1.f - (float)(a->ms - u->tempo_ms) / 1000.f;
}

static void fill_shift(const qd_app_t *a, ui_shift_t *v)
{
    const qd_ui_t *u = &a->ui;
    memset(v, 0, sizeof *v);
#define ROW(k, w) (v->key[v->n] = (k), v->what[v->n++] = (w))
    if (u->mode == QD_MODE_KEYS) {
        v->ctx = "KEYBOARD";
        ROW("+ REC", a->ui.rec ? "recording off" : a->seq.playing ? "live recording" : "step recording");
        ROW("+ SELECT", "swing");
    } else if (u->mode == QD_MODE_STEP && u->nheld) {
        v->ctx = "A STEP HELD";
        ROW("+ ALGORITHM", u->hmode == UI_F_RATCH ? "the ratchet's ramp" : "the strum (in MICRO)");
    } else {
        v->ctx = "STEP VIEW";
        ROW("+ ALGORITHM", "the track's scale");
        ROW("+ PRESETS", "the track's root");
        ROW("+ F3 + ALGORITHM", "swing");
        ROW("+ SEQ + W", "change pattern now");
    }
#undef ROW
}

/* ---------------------------------------------------------------- the takeovers --- */
static void fill_turn(qd_app_t *a, ui_turn_t *v, char *ctx)
{
    static const char *const NAMES[UI_NTK] = {"FREQ", "DECAY", "HARM", "FEEDBACK", "ALGO", "TEMPO", "SWING",
                                              "LENGTH", "RATE", "LIMIT", "SWING", "PAGE", "SCALE", "ROOT",
                                              "OCTAVE", "TRACK"};
    const qd_ui_t *u = &a->ui;
    const int t = a->sel;
    const qd_pattern_t *P = qd_ctl_cpat(a);
    const qd_tset_t *ts = &P->trk[t];
    double beats;
    char n[12];
    int id = -1, i, s;
    memset(v, 0, sizeof *v);
    v->kind = u->tk;
    v->knob = u->tk_knob;
    v->name = NAMES[u->tk];
    v->t = secs(a->ms);
    v->playing = a->seq.playing;
    v->track = t + 1;
    beats = a->seq.playing ? a->beats : (double)a->ms * (double)a->bpm / 60000.0;
    v->beat = (float)(beats - floor(beats));
    v->beats = (int)floor(beats);
    v->bpm = a->bpm;
    v->swing = P->swing;
    ctx[0] = 0;
    if (u->tk_lock) {
        cat(cat(ctx, "STEP "), ui_itoa(n, u->tk_step + 1));
        v->lock = 1;
    } else {
        cat(ctx, a->model.t[t].name);
    }
    v->ctx = ctx;
    switch (u->tk) {
    case UI_TK_FREQ:
    case UI_TK_DECAY:
    case UI_TK_HARM:
    case UI_TK_FDBK:
    case UI_TK_ALGO: {
        int16_t lv;
        id = u->tk == UI_TK_ALGO ? QD_QP_ALGO : qd_step_quick_id(u->tk - UI_TK_FREQ);
        v->value = a->model.t[t].patch[id];
        if (u->tk_lock) {
            const int tb = qd_step_base(a, t, u->tk_step, id);
            v->value = qd_lock_get(P, t, u->tk_step, id, &lv) ? lv : tb;
            v->ghost_v = range01(id, tb);
            v->ghost[0] = 0;
            cat(v->ghost, "track ");
            if (id == QD_QP_FREQ)
                fmt_hundredths(v->ghost + 6, tb, 2);
            else if (id == QD_QP_HARM)
                fmt_hundredths(v->ghost + 6, tb, 1);
            else
                cat(v->ghost, ui_itoa(n, tb));
        }
        v->v = range01(id, v->value);
        if (id == QD_QP_FREQ)
            fmt_hundredths(v->val, v->value, 0);
        else if (id == QD_QP_HARM) {
            v->val[0] = 0;
            if (v->value > 0)
                cat(v->val, "+");
            fmt_hundredths(v->val + (v->value > 0), v->value, 1);
        } else
            ui_itoa(v->val, v->value);
        break;
    }
    case UI_TK_TEMPO:
        v->value = a->bpm;
        v->v = (float)(a->bpm - QD_BPM_MIN) / (float)(QD_BPM_MAX - QD_BPM_MIN);
        ui_itoa(v->val, a->bpm);
        break;
    case UI_TK_SWING:
        v->value = P->swing;
        v->v = (float)(P->swing - 50) / 30.f;
        cat(ui_itoa(v->val, P->swing), "%");
        break;
    case UI_TK_LEN:
        v->value = qd_track_len(ts);
        v->v = (float)v->value / (float)QD_NSTEP;
        ui_itoa(v->val, v->value);
        break;
    case UI_TK_RATE: {
        const int k = qd_ctl_rate_index(qd_tset_rate(ts));
        const double L = (double)qd_rate_ticks(qd_tset_rate(ts));
        const double tick = a->seq.playing ? (double)qd_seq_tick(&a->seq) : beats * 4.0 * QD_TPS;
        v->value = qd_tset_rate(ts);
        v->v = (float)k / (float)(QD_NRATE - 1);
        strcpy(v->val, qd_rate_name(qd_tset_rate(ts)));
        v->prev = k > 0 ? qd_rate_name(qd_ctl_rate_at(k - 1)) : 0;
        v->next = k < QD_NRATE - 1 ? qd_rate_name(qd_ctl_rate_at(k + 1)) : 0;
        v->rate = (float)(QD_TPS / L);
        v->pat_pos = (float)fmod(tick / QD_TPS, 16.0);
        v->track_pos = (float)fmod(tick / L, 16.0);
        break;
    }
    case UI_TK_LIMIT:
        v->value = ts->limit;
        v->v = (float)(ts->limit - 1) / 15.f;
        ui_itoa(v->val, ts->limit);
        for (i = 0; i < a->pool.nvoice; i++)
            v->voices += qd_pool_sounding(&a->pool, i) && a->pool.v[i].track == t;
        break;
    case UI_TK_TSWING:
        v->value = ts->swing;
        v->v = ts->swing ? 1.f : 0.f;
        strcpy(v->val, ts->swing ? "ON" : "OFF");
        break;
    case UI_TK_PAGE:
        v->page = u->page[t];
        v->value = v->page + 1;
        v->v = (float)v->page / 7.f;
        cat(cat(v->val, "PAGE "), ui_itoa(n, v->page + 1));
        v->loop = (uint8_t)qd_track_loop(ts);
        memcpy(v->pstart, ts->pstart, sizeof v->pstart);
        memcpy(v->pend, ts->pend, sizeof v->pend);
        v->play_page = qd_ctl_playing_page(a, t);
        for (i = 0; i < QD_NPAGE; i++)
            for (s = 0; s < QD_PAGE; s++)
                v->trig[i][s] = (uint8_t)qd_step_is_on(qd_step(P, t, i * QD_PAGE + s));
        break;
    case UI_TK_TRACK:                                   /* P1.3d: P-LOCK's ALGORITHM, the eight tracks */
        v->has_col = 1;
        v->col = UI_MODE_COL[UI_F_PLOCK];
        v->value = t + 1;
        v->v = (float)t / 7.f;
        for (i = 0; i < QD_NTRACK; i++) {
            const qd_pattern_t *TP = qd_seq_track_pat(&a->seq, i);
            const qd_tset_t *tt = &TP->trk[i];
            const int n = qd_track_len(tt);
            v->tname[i] = a->model.t[i].name;
            if (!qd_seq_audible(&a->seq, TP, i))
                v->tmute |= (uint8_t)(1u << i);
            v->tlen[i] = (uint8_t)(n > 32 ? 32 : n);
            for (s = 0; s < v->tlen[i]; s++)
                v->ttrig[i][s] = (uint8_t)(qd_step_type(TP, i, qd_track_loop_step(tt, s)) != QD_TRIG_NONE);
        }
        break;
    case UI_TK_SCALE:
    case UI_TK_ROOT: {
        int sc, rt, k;
        qd_track_key(&a->proj, P, t, &sc, &rt);
        v->root = rt;
        for (k = 0; k < 12; k++)
            if (qd_scale_has(sc, rt, rt + k))
                v->scale_mask |= (uint16_t)(1u << k);
        if (u->tk == UI_TK_SCALE) {
            v->value = sc;
            v->v = (float)sc / (float)(QD_NSCALE - 1);
            strcpy(v->val, qd_scale_name(sc));
            v->prev = sc > 0 ? qd_scale_name(sc - 1) : 0;
            v->next = sc < QD_NSCALE - 1 ? qd_scale_name(sc + 1) : 0;
        } else {
            v->value = rt;
            v->v = (float)rt / 11.f;
            strcpy(v->val, qd_pc_name(rt));
            v->prev = qd_pc_name((rt + 11) % 12);
            v->next = qd_pc_name((rt + 1) % 12);
        }
        break;
    }
    default:
        break;
    }
}

/* ---------------------------------------------------------------- the keyboard, the tracks, the picker, idle --- */
static void draw_keys(qd_app_t *a)
{
    static ui_keys_t v;
    const qd_ui_t *u = &a->ui;
    const qd_track_t *t = &a->model.t[a->sel];
    const qd_pattern_t *P = qd_ctl_cpat(a);
    const qd_tset_t *ts = &P->trk[a->sel];
    int i, sc, rt;
    memset(&v, 0, sizeof v);
    v.track = a->sel + 1;
    v.name = t->name;
    v.preset = t->sound;
    v.sound = qd_track_sound_name(t);
    v.edited = t->edited;
    qd_track_key(&a->proj, P, a->sel, &sc, &rt);
    v.scale = sc;
    v.scale_name = qd_scale_name(sc);
    v.nvoice = a->pool.nvoice < UI_NVOICE_DOTS ? a->pool.nvoice : UI_NVOICE_DOTS;
    for (i = 0; i < v.nvoice; i++) {
        const qd_pool_voice_t *pv = &a->pool.v[i];
        if (qd_pool_used(&a->pool, i))
            v.voice[i] = (uint8_t)((pv->pend ? pv->pend_track : pv->track) == a->sel ? 2 : 1);
    }
    qd_ctl_sounding(a, v.sounding);
    v.octave = a->oct;
    v.blink = (a->ms / QD_BLINK_MS) % 2u == 0;
    if (u->rec && !a->seq.playing) {
        const int cs = qd_track_loop_step(ts, u->cursor);
        v.rec = 1;
        v.cursor = cs + 1;
        for (i = 0; i < 16; i++)
            v.page_trig[i] = (uint8_t)qd_step_is_on(qd_step(P, a->sel, (cs / QD_PAGE) * QD_PAGE + i));
    } else if (u->rec) {
        const double L = (double)qd_rate_ticks(qd_tset_rate(ts));
        const int64_t now = qd_seq_tick(&a->seq);
        v.rec = 2;
        v.quantize = u->quantize;
        v.pos = (float)qd_seq_track_pos(&a->seq, a->sel);
        v.step = qd_seq_track_step(&a->seq, a->sel) + 1;
        for (i = 0; i < u->nroll; i++) {
            const int k = (u->iroll - 1 - i + QD_NROLL) % QD_NROLL;
            const float st = (float)((double)(now - u->roll[k].t0) / L);
            if (st > 17.f || st < 0.f)
                continue;
            v.roll[v.nroll].start = st;
            v.roll[v.nroll].end = u->roll[k].t1 < 0 ? 0.f : (float)((double)(now - u->roll[k].t1) / L);
            v.roll[v.nroll].note = u->roll[k].note;
            v.roll[v.nroll].live = u->roll[k].t1 < 0;
            v.nroll++;
        }
    }
    ui_keys_draw(&v);
}

static void draw_tracks(const qd_app_t *a)
{
    static ui_tracks_t v;
    char pat[4];
    int t, s, i;
    const int64_t tick = qd_seq_tick(&a->seq);
    memset(&v, 0, sizeof v);
    v.sel = a->sel;
    v.playing = a->seq.playing;
    v.pool_n = a->pool.nvoice;
    v.pool_used = qd_pool_in_use(&a->pool);
    v.pattern = qd_pat_name(a->seq.cur, pat);
    v.swing_amt = a->proj.pat[a->seq.cur].swing;
    for (t = 0; t < QD_NTRACK; t++) {
        ui_track_clock_t *c = &v.tr[t];
        const qd_pattern_t *P = qd_seq_track_pat(&a->seq, t);
        const qd_tset_t *ts = &P->trk[t];
        c->name = a->model.t[t].name;
        c->len = qd_track_len(ts);
        c->rate = qd_rate_name(qd_tset_rate(ts));
        for (s = 0; s < c->len; s++) {                  /* the ring: the loop's positions */
            const int ty = qd_step_type(P, t, qd_track_loop_step(ts, s));
            c->trig[s] = (uint8_t)(ty == QD_TRIG_LOCK ? 2 : ty ? 1 : 0);
        }
        c->pos = (float)qd_seq_track_pos(&a->seq, t);
        c->flash_step = a->seq.playing && a->seq.flash_step[t] >= 0 ? qd_track_step_pos(ts, a->seq.flash_step[t]) : -1;
        c->flash_age = (float)((double)(tick - a->seq.flash_tick[t]) / (double)qd_rate_ticks(qd_tset_rate(ts)));
        c->mute = !qd_seq_audible(&a->seq, P, t);
        c->swing = ts->swing;
        for (i = 0; i < a->pool.nvoice; i++)
            c->voices += qd_pool_sounding(&a->pool, i) && a->pool.v[i].track == t;
    }
    ui_tracks_draw(&v);
}

/* the pool's picker: the track's sound, or (a lock) the held steps' sound lock */
static void draw_picker(const qd_app_t *a)
{
    const qd_track_t *t = &a->model.t[a->sel];
    const float d = (float)(a->ms - a->ui.tk_ms);
    ui_picker_t v;
    memset(&v, 0, sizeof v);
    v.track = a->sel + 1;
    v.name = t->name;
    v.n = a->proj.nsound;
    v.lock = a->ui.tk_lock;
    if (v.lock) {
        qd_rstep_t r;
        qd_step_resolve(qd_ctl_cpat(a), a->sel, a->ui.tk_step, &r);
        v.k = r.sound;
        v.step = a->ui.tk_step + 1;
    } else {
        v.k = t->sound;
    }
    v.cur = v.k ? qd_sound_name(&a->proj, v.k) : "TRACK SOUND";
    v.prev = v.k > 1 ? qd_sound_name(&a->proj, v.k - 1) : v.k == 1 && v.lock ? "TRACK SOUND" : 0;
    v.next = v.k < v.n ? qd_sound_name(&a->proj, v.k + 1) : 0;
    v.roll = d >= QD_ROLL_MS ? 0.f : 1.f - d / QD_ROLL_MS;
    v.dir = a->ui.tk_dir;
    v.reel = (float)fmod((double)a->ms * 0.0025, 6.283185307179586);
    ui_picker_draw(&v);
}

/* P1.3c: the octave takeover (a change of NOTE's entry octave, the keyboard's octave, SOUND's bank) */
static void draw_octave(const qd_app_t *a)
{
    static char scale_buf[24];
    const qd_ui_t *u = &a->ui;
    const uint32_t d = a->ms - u->oct_ms;
    ui_octave_t v;
    int sc, rt;
    memset(&v, 0, sizeof v);
    v.kind = u->oct_kind;
    v.value = u->oct_to;
    v.from = u->oct_from;
    v.k = d >= QD_OCT_SLIDE_MS ? 1.f : ui_ease((float)d / (float)QD_OCT_SLIDE_MS);
    v.t = secs(d);
    v.dir = u->oct_to >= u->oct_from ? 1 : -1;
    v.track = a->sel + 1;
    v.name = a->model.t[a->sel].name;
    qd_track_key(&a->proj, qd_ctl_cpat(a), a->sel, &sc, &rt);
    scale_buf[0] = 0;
    if (sc != QD_SCALE_CHROMATIC)
        cat(cat(scale_buf, qd_pc_name(rt)), " ");
    cat(scale_buf, qd_scale_name(sc));
    v.scale_name = scale_buf;
    if (v.kind == UI_OCT_NOTE) {
        char n[8];
        v.lo_note = qd_scale_note(sc, rt, u->oct_to, 0);
        cat(cat(cat(cat(v.sub, "B1 = "), qd_note_name(v.lo_note, n)), " \xc2\xb7 "), scale_buf);
    } else if (v.kind == UI_OCT_KEYS) {
        char n[8], n2[8];
        v.lo_note = QD_NOTE0 + 12 * u->oct_to;
        qd_note_name(v.lo_note, n);
        qd_note_name(v.lo_note + 26, n2);
        cat(cat(cat(v.sub, n), "\xe2\x80\x93"), n2);
        cat(cat(v.sub, " \xc2\xb7 "), scale_buf);
    } else {
        char n[12];
        cat(cat(cat(v.sub, "SOUNDS "), ui_itoa(n, u->oct_to * 10 + 1)), "\xe2\x80\x93");
        cat(v.sub, ui_itoa(n, u->oct_to * 10 + 10 < a->proj.nsound ? u->oct_to * 10 + 10 : a->proj.nsound));
    }
    v.nsound = a->proj.nsound;
    v.nbank = (a->proj.nsound + 9) / 10;
    if (u->nheld) {
        qd_rstep_t r;
        qd_step_resolve(qd_ctl_cpat(a), a->sel, u->held[0], &r);
        v.sound = r.sound;
    }
    ui_octave_draw(&v);
}

/* P1.3c: SEQ held, the patterns */
static void draw_patterns(const qd_app_t *a)
{
    ui_patterns_t v;
    int k;
    memset(&v, 0, sizeof v);
    v.cur = a->seq.cur;
    v.queued = qd_seq_queued(&a->seq);
    v.bank = a->ui.seq_bank;
    for (k = 0; k < 16; k++)
        if (a->proj.pat[v.bank * 16 + k].used)
            v.used |= (uint16_t)(1u << k);
    for (k = 0; k < QD_NPAT; k++)
        if (a->proj.pat[k].used)
            v.bank_used |= (uint8_t)(1u << (k / 16));
    v.playing = a->seq.playing;
    if (v.queued >= 0 && a->seq.playing) {
        const int64_t m = qd_pat_master_ticks(&a->proj.pat[a->seq.cur]), left = qd_seq_to_change(&a->seq);
        v.left = m > 0 ? (float)((double)left / (double)m) : 0.f;
        v.left = v.left < 0.f ? 0.f : v.left > 1.f ? 1.f : v.left;
    }
    v.blink = (a->ms / QD_BLINK_MS) % 2u == 0;
    v.launch = a->proj.launch;
    v.launch_name = qd_launch_name(a->proj.launch);
    ui_patterns_draw(&v);
}

static int held_btn(const qd_app_t *a, int b) { return (int)((a->buttons_held >> b) & 1u); }

static void draw_idle(const qd_app_t *a)
{
    ui_idle_t v;
    memset(&v, 0, sizeof v);
    v.bpm = a->bpm;
    v.playing = a->playing;
    v.t = secs(a->ms);
    v.beats = a->playing ? a->beats : 0.0;
    ui_idle_draw(&v);
}

/* an overlay drawn over the screen at alpha al (a black ground first) */
static void overlay_begin(float al)
{
    qd_save();
    qd_alpha(al);
    qd_fill_rect(0.f, 0.f, (float)QD_W, (float)QD_H, UI_BG);
}

/* P1.3d: the knob band's words: the knob, the context, the value's name and the value (the held steps' first, or
 * the track's default) */
static void fill_band(const qd_app_t *a, ui_band_t *v, int is_default)
{
    static const char *const MAIN[UI_NFIELD] = {"NOTES", "VELOCITY", "LENGTH", "MICRO", "RATCHET", "CHANCE",
                                                "CONDITION", "TRIG", "SOUND", "", ""};
    static const char *const TYPE[4] = {"", "NOTE", "LOCK", "TIE"};
    static const char *const RAMP[4] = {"FLAT", "UP", "DOWN", "ACCENT"};
    static const char *const NOTEN[5] = {"NOTE 1", "NOTE 2", "NOTE 3", "NOTE 4", "NOTE 5"};
    static char ctx[24];
    const qd_ui_t *u = &a->ui;
    const int t = a->sel, m = u->hmode >= 0 && u->hmode < UI_NFIELD ? u->hmode : 0;
    const qd_pattern_t *P = qd_ctl_cpat(a);
    qd_rstep_t r;
    char n[12];
    memset(v, 0, sizeof *v);
    v->knob = u->tk_src;
    v->col = UI_MODE_COL[m];
    v->mode = UI_FIELD_NAME[m];
    if (is_default || !u->nheld) {
        static qd_pattern_t tmp;                        /* the track's defaults: a step with no overrides */
        tmp.trk[0] = P->trk[t];
        memset(&tmp.step[0][0], 0, sizeof tmp.step[0][0]);
        tmp.step[0][0].flags = QD_SF_ON;
        qd_step_resolve(&tmp, 0, 0, &r);
        r.sound = 0;
        v->ctx = "TRACK DEFAULT";
    } else {
        qd_step_resolve(P, t, u->held[0], &r);
        ctx[0] = 0;
        cat(cat(ctx, "STEP "), ui_itoa(n, u->held[0] + 1));
        if (u->nheld > 1)
            cat(cat(ctx, "+"), ui_itoa(n, u->nheld - 1));
        v->ctx = ctx;
    }
    v->name = MAIN[m];
    switch (u->tk_job) {
    case QD_KJ_MAIN:
        switch (m) {
        case UI_F_NOTE: {
            char rt[4], q[8];
            if (r.nn && qd_chord_name(r.notes, r.nn, rt, q))
                cat(cat(v->val, rt), q);
            else
                qd_note_name(r.notes[0], v->val);
            break;
        }
        case UI_F_VEL: ui_itoa(v->val, r.vel); break;
        case UI_F_LEN: cat(v->val, qd_len_name(r.len)); break;
        case UI_F_MICRO: cat(cat(cat(v->val, r.micro > 0 ? "+" : ""), ui_itoa(n, r.micro)), "/24"); break;
        case UI_F_RATCH: cat(cat(v->val, "\xc3\x97"), ui_itoa(n, r.ratch)); break;
        case UI_F_CHNC: cat(ui_itoa(v->val, r.chance), "%"); break;
        case UI_F_COND: qd_cond_name(r.cond, v->val); break;
        case UI_F_TRIG: cat(v->val, TYPE[r.type & 3][0] ? TYPE[r.type & 3] : "EMPTY"); break;
        case UI_F_SOUND: {
            const int k = is_default || !u->nheld ? a->model.t[t].sound : r.sound;
            if (k > 0) {
                cat(cat(v->val, ui_itoa(n, k)), " ");
                cat(v->val, is_default || !u->nheld ? qd_track_sound_name(&a->model.t[t]) : qd_sound_name(&a->proj, k));
            } else {
                cat(v->val, "TRACK");
            }
            break;
        }
        default: break;
        }
        break;
    case QD_KJ_NOTE1:
    case QD_KJ_NOTE2:
    case QD_KJ_NOTE3:
    case QD_KJ_NOTE4:
    case QD_KJ_NOTE5: {
        const int k = u->tk_job - QD_KJ_NOTE1;
        v->name = NOTEN[k];
        if (r.note[k])
            qd_note_name(r.note[k], v->val);
        else
            cat(v->val, "\xe2\x80\x94");                /* none */
        break;
    }
    case QD_KJ_TIE:
        v->name = "TIE";
        cat(v->val, r.type == QD_TRIG_TIE ? "ON" : "OFF");
        break;
    case QD_KJ_STRUM:
        v->name = "STRUM";
        cat(cat(v->val, ui_itoa(n, r.strum)), "/24");
        break;
    case QD_KJ_RAMP:
        v->name = "RAMP";
        cat(v->val, RAMP[r.ramp & 3]);
        break;
    case QD_KJ_COND:
        v->name = "CONDITION";
        qd_cond_name(r.cond, v->val);
        break;
    case QD_KJ_CHANCE:
        v->name = "CHANCE";
        cat(ui_itoa(v->val, r.chance), "%");
        break;
    default:
        break;
    }
}

int qd_view_screen(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    if (u->mode != QD_MODE_IDLE && held_btn(a, QD_B_MIX))
        return QD_SCR_SEQ;
    if (u->mode != QD_MODE_IDLE && held_btn(a, QD_B_PATTERN))
        return QD_SCR_SEL;
    switch (u->mode) {
    case QD_MODE_KEYS:
        return qd_ctl_shift(a) && !u->shift_used ? QD_SCR_SHIFT : QD_SCR_KEYS;
    case QD_MODE_TRACKS:
        return QD_SCR_TRACKS;
    case QD_MODE_STEP:
        if (u->nheld && u->hmode != UI_F_PLOCK &&      /* (P-LOCK: holding keeps the screen; P1.3q: so does */
            !(u->scursor && u->view == QD_VIEW_STEPS))   /* PRESETS' cursor in STEPS) */
            return QD_SCR_HELD;
        if (qd_ctl_mkey_shown(a))                       /* P1.3e: a mode key held: the track's default */
            return QD_SCR_DEFAULT;
        if (qd_ctl_layer_shown(a) != QD_LAYER_NONE)
            return QD_SCR_LAYER;
        if (qd_ctl_shift(a) && !u->shift_used)
            return QD_SCR_SHIFT;
        return QD_SCR_LANE;
    default:
        return QD_SCR_IDLE;
    }
}

void qd_view_frame(qd_app_t *a, uint16_t *lcd_be)
{
    static ui_lane_t lv;
    static ui_held_t hv;
    static ui_turn_t tv;
    static ui_layer_t yv;
    static ui_shift_t sv;
    static ui_band_t bv;
    const qd_ui_t *u = &a->ui;
    char pat[4], ctx[24];
    qd_begin();
    switch (qd_view_screen(a)) {
    case QD_SCR_SEQ:                                    /* printed SEQ held: the patterns */
        draw_patterns(a);
        break;
    case QD_SCR_SEL:                                    /* printed SEL held: the tracks */
        fill_layer(a, &yv, 4);
        ui_layer_draw(&yv);
        break;
    case QD_SCR_SHIFT:
        fill_shift(a, &sv);
        ui_shift_draw(&sv);
        break;
    case QD_SCR_KEYS:
        draw_keys(a);
        break;
    case QD_SCR_TRACKS:
        draw_tracks(a);
        break;
    case QD_SCR_HELD:
        fill_held(a, &hv, 0);
        ui_held_draw(&hv);
        break;
    case QD_SCR_DEFAULT:                                /* P1.3e: a mode key held: its default's full picture */
        fill_held(a, &hv, 1);
        ui_held_draw(&hv);
        break;
    case QD_SCR_LAYER:
        fill_layer(a, &yv, qd_ctl_layer_shown(a));
        ui_layer_draw(&yv);
        break;
    case QD_SCR_LANE:
        fill_lane(a, &lv, pat);
        ui_lane_draw(&lv);
        break;
    default:
        draw_idle(a);
        break;
    }
    if (u->tk == QD_TK_HELDKNOB) {                      /* P1.3d: a held step's knob: the band over its screen */
        const uint32_t d = a->ms - u->tk_ms;
        const float al = d < QD_TK_MS ? 1.f : 1.f - (float)(d - QD_TK_MS) / (float)QD_TK_FADE_MS;
        if (al > 0.f && u->nheld) {
            qd_save();
            qd_alpha(al);
            fill_band(a, &bv, 0);
            ui_band_draw(&bv);
            qd_restore();
        }
    } else if (u->tk != QD_TK_NONE) {                   /* the takeover, then 120 ms back */
        const uint32_t d = a->ms - u->tk_ms;
        const float al = d < QD_TK_MS ? 1.f : 1.f - (float)(d - QD_TK_MS) / (float)QD_TK_FADE_MS;
        if (al > 0.f) {
            overlay_begin(al);
            if (u->tk == QD_TK_PRESET) {
                draw_picker(a);
            } else if (u->tk == UI_TK_OCTAVE) {
                draw_octave(a);
            } else if (u->tk == QD_TK_DEFAULT) {        /* P1.3e: a default: its full TRACK DEFAULT picture */
                fill_held(a, &hv, 1);
                ui_held_draw(&hv);
            } else {
                fill_turn(a, &tv, ctx);
                ui_turn_draw(&tv);
            }
            qd_restore();
        }
    }
    if (u->cf_on) {                                     /* a confirmation word */
        const uint32_t d = a->ms - u->cf_ms;
        const float al = d + QD_TK_FADE_MS < QD_CONFIRM_MS ? 1.f
                                                           : (float)(QD_CONFIRM_MS - d) / (float)QD_TK_FADE_MS;
        if (al > 0.f) {
            overlay_begin(al);
            ui_confirm_draw(u->cf_big, u->cf_small, UI_K3);
            qd_restore();
        }
    }
    qd_end(lcd_be);
}
