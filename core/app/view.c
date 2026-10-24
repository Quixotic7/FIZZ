/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The frame (ctl.h): the screens' view records (ui.h) filled from the model and the UI state, then drawn. The screen
 * under everything is the mode's: idle; the step view (the page as the hold mode's lane, PLAN §8b), a held step's
 * screen for the mode (with the ten values at its foot; P-LOCK keeps the lane), a layer's map while F1 / F2 / F3 is
 * held, SHIFT's map while SHIFT is held alone; the keyboard; the tracks. P1.3c: SEL held, the track map; SEQ held, the
 * pattern map (over any mode's screen). A change of octave or SOUND bank: the octave takeover. Over it a knob's takeover (PLAN §9a rule 5: shown at the first
 * detent, 1 s after the last one it fades back over 120 ms) and a confirmation word. Nothing here changes the model or
 * the UI state; the motion is the app's milliseconds and the transport's position. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ui.h"
#include "../seq/chord.h"
#include "../seq/scale.h"
#include "../seq/sounds.h"
#include "../voice/fx/qd_fx.h"
#include <stdio.h>

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
    static const int PM[UI_F_PLOCK] = {QD_PM_NOTE, QD_PM_VEL, QD_PM_LEN, QD_PM_MICRO, QD_PM_RATCH, QD_PM_CHANCE,
                                       QD_PM_COND, QD_PM_TRIG, QD_NPM, QD_PM_SOUND};
    return m >= 0 && m < UI_F_PLOCK ? PM[m] : QD_PM_NOTE;
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
    v->steps = QD_VIEW_ROWS(u->view);                   /* P1.3f: the STEPS overview: every page's cells (P1.3r:
                                                         * EDIT's row too) */
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
                c |= r.on && st->func ? UI_C_FUNC : 0;   /* P1.3w: a step function */
                if (r.on && (u->hmode == UI_F_PLOCK ? (st->flags & QD_SF_LOCKS) != 0
                                                    : u->hmode == UI_F_FUNC ? st->func != 0
                                                    : u->hmode < UI_F_PLOCK && (qd_step_overrides(st, mode_pm(u->hmode)) ||
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
                v->flash_age = qd_ratio_f((int64_t)(a->seq.now - a->seq.flash_sample[t]), QD_FS);
            }
        }
    }
    v->t = secs(a->ms);
    v->play_page = -1;
    v->pos = -1.f;
    if (a->seq.playing) {
        int64_t pn, pd;
        const int s = qd_seq_track_step(&a->seq, t);
        qd_seq_track_pos_q(&a->seq, t, &pn, &pd);
        v->play_page = s / QD_PAGE;
        if (v->play_page == page)                       /* (the step and how far into it: P2.4, no double) */
            v->pos = (float)(s % QD_PAGE) + qd_ratio_f(pn - qd_div64(pn, (int32_t)pd) * pd, pd);
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
        case UI_F_FUNC: c->ovr = st->func != 0; break;
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
        c->func = (uint8_t)r.func;
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
        case UI_F_SOUND: cat(cat(v->def_label, "TRACK SOUND "), qd_track_sound_label(&a->model.t[t])); break;
        case UI_F_FUNC: break;                          /* (P1.3w: no default: none) */
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
static void fill_held_at(qd_app_t *a, ui_held_t *v, int is_default, int s_at)
{
    static char scale_buf[24];
    const qd_ui_t *u = &a->ui;
    const int t = a->sel, s = is_default ? -1 : s_at, m = u->hmode;
    const qd_pattern_t *P = qd_ctl_cpat(a);
    const qd_tset_t *ts = &P->trk[t];
    const qd_step_t *st = qd_step(P, t, s < 0 ? 0 : s);
    qd_rstep_t r;
    int i, k;
    memset(v, 0, sizeof *v);
    if (is_default) {                                   /* the track's defaults: a step with no overrides */
        static qd_step_t on;
        memset(&on, 0, sizeof on);
        on.flags = QD_SF_ON;
        qd_step_resolve_def(&ts->def, &on, &r);
        st = &on;
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
        v->ovr = m == UI_F_FUNC ? r.func != 0 : v->clr;   /* (P1.3w: FUNC has no default: a function is its own) */
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
        v->pass = qd_mod64(a->seq.cur_t[t].pass, v->ab_b);
    v->chance = r.chance;
    v->shuffle = (uint32_t)(a->seq.playing ? a->seq.cur_t[t].pass : 0) * 2654435761u + 7919u;
    v->chance_first = m == UI_F_CHNC;
    /* SOUND */
    v->sound = r.sound;
    v->func = is_default ? 0 : r.func;                  /* P1.3w: the step function, a jump's target page */
    qd_fn_name(v->func, v->func_name);
    if (v->func >= QD_FN_JUMP) {
        const int tg = v->func - QD_FN_JUMP;
        v->jump_page = tg / QD_PAGE;
        v->jump_step = tg % QD_PAGE;
        for (i = 0; i < QD_PAGE; i++)
            v->jump_trig[i] = (uint8_t)qd_step_is_on(qd_step(P, t, v->jump_page * QD_PAGE + i));
    }
    v->sound_cur = is_default || r.sound <= 0 ? qd_track_sound_label(&a->model.t[t]) : sound_name(a, r.sound);   /* (P1.3y:
                                                         * no lock: the track's sound by name, under TRACK) */
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
static void fill_held(qd_app_t *a, ui_held_t *v, int is_default) { fill_held_at(a, v, is_default, a->ui.held[0]); }

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
    v->f1_src = u->f1_copied ? u->f1_src : -1;          /* P1.6h: where F1's copy came from */
    if (u->clip.st.valid) {                             /* the step in the clipboard (F1's copy, F2's hand) */
        qd_rstep_t r;
        qd_step_resolve_def(&ts->def, &u->clip.st.step, &r);
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
    static char scale[24], root[4], swing[8], trk[4];
    const qd_ui_t *u = &a->ui;
    const qd_pattern_t *P = qd_ctl_cpat(a);
    char n[12];
    int sc, rt;
    memset(v, 0, sizeof *v);
    qd_track_key(&a->proj, P, a->sel, &sc, &rt);
    scale[0] = root[0] = swing[0] = 0;
    if (sc != QD_SCALE_CHROMATIC)
        cat(cat(scale, qd_pc_name(rt)), " ");
    cat(scale, qd_scale_name(sc));
    cat(root, qd_pc_name(rt));
    cat(cat(swing, ui_itoa(n, P->swing)), "%");
    ui_itoa(trk, a->sel + 1);
#define ROW(c, i, w) (v->ctl[v->n] = (c), v->icon[v->n] = (i), v->val[v->n++] = (w))
    if (u->mode == QD_MODE_KEYS) {                      /* P1.3v: the controls, their jobs' icons, the values now */
        ROW(UI_SC_REC, UI_IC_GEAR, a->proj.rec_mode ? "STEP" : "LIVE");   /* P1.3y: the record settings */
        ROW(UI_SC_SELECT, UI_IC_TRACK, trk);            /* P1.5z: the track (the swing: F3 + ALGORITHM) */
    } else if (u->mode == QD_MODE_STEP && u->nheld) {
        ROW(UI_SC_ALGO, u->hmode == UI_F_RATCH ? UI_IC_RAMP : UI_IC_STRUM, 0);
    } else {
        static char knobs[4];
        static const char *const VIEW[QD_NVIEW] = {"ROLL", "STEPS", "EDIT", "METERS", "NUMBERS", "VOICES", "SCOPE"};
        knobs[0] = (char)('A' + u->knobs);
        knobs[1] = 0;
        ROW(UI_SC_SELECT, UI_IC_TRACK, trk);            /* P1.5z: the track */
        ROW(UI_SC_ALGO, UI_IC_SCALE, scale);
        ROW(UI_SC_PRESETS, UI_IC_KEY, root);
        ROW(UI_SC_F3_ALGO, UI_IC_SWING, swing);
        ROW(UI_SC_SEQ_W, UI_IC_NOW, 0);
        ROW(UI_SC_HOME, UI_IC_VIEWS, VIEW[(u->view + 1) % QD_NVIEW]);   /* P1.3y: the next view */
        ROW(UI_SC_REC, UI_IC_GEAR, a->proj.rec_mode ? "STEP" : "LIVE");   /* .. the record settings */
        ROW(UI_SC_SEL, UI_IC_GEAR, knobs);              /* .. the global settings (the knobs' layout) */
        ROW(UI_SC_ARP, UI_IC_NONE, "FILL");             /* P1.6d: SHIFT + ARP held: FILL */
    }
    if (u->mode == QD_MODE_STEP && (!u->latch || u->latch == QD_LATCH_F3)) {   /* P1.5c: the black keys' shortcuts */
        v->keys = 1;
        v->follow = u->follow;
        v->snap_ok = a->undo.snap_ok;
        v->pclip_ok = a->undo.pclip_ok;
        v->metro = a->metro;
        v->armed = u->clr_armed && a->ms - u->clr_ms < 2000u;
    }
#undef ROW
}

/* ---------------------------------------------------------------- the takeovers --- */
static void fill_turn(qd_app_t *a, ui_turn_t *v, char *ctx)
{
    static const char *const NAMES[UI_NTK] = {"FREQ", "DECAY", "HARM", "FEEDBACK", "ALGO", "TEMPO", "SWING",
                                              "LENGTH", "RATE", "LIMIT", "SWING", "PAGE", "SCALE", "ROOT",
                                              "OCTAVE", "TRACK", "TRACK", "TAP", "PAGE FOLLOW"};
    const qd_ui_t *u = &a->ui;
    const int t = a->sel;
    const qd_pattern_t *P = qd_ctl_cpat(a);
    const qd_tset_t *ts = &P->trk[t];
    int64_t bq;                                         /* (P2.4: the beats as bq / bd, no double) */
    int32_t bd;
    char n[12];
    int id = -1, i, s;
    memset(v, 0, sizeof *v);
    v->kind = u->tk;
    v->knob = u->tk_knob;
    v->name = NAMES[u->tk];
    v->t = secs(a->ms);
    v->playing = a->seq.playing;
    v->track = t + 1;
    if (a->seq.playing)
        bq = a->beat * QD_BEAT_TICKS + a->beat_rem, bd = QD_BEAT_TICKS;
    else
        bq = (int64_t)a->ms * a->bpm, bd = 60000;
    v->beats = (int)qd_div64(bq, bd);
    v->beat = qd_ratio_f(bq - (int64_t)v->beats * bd, bd);
    v->bpm = a->bpm;
    v->swing = P->swing;
    ctx[0] = 0;
    if (u->tk_lock) {
        cat(cat(ctx, "STEP "), ui_itoa(n, u->tk_step + 1));
        v->lock = 1;
    }                                                   /* (P1.3r: a pop-up: no track name in its head) */
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
        const int64_t L = qd_rate_ticks(qd_tset_rate(ts));
        /* the position in ticks as tq / td: playing, the transport's; stopped, the beats' x 403200 (60000 x 25 / 168) */
        const int64_t tq = a->seq.playing ? qd_seq_tick(&a->seq) : bq * (QD_BEAT_TICKS / 2400), td = a->seq.playing ? 1 : 25;
        v->value = qd_tset_rate(ts);
        v->v = (float)k / (float)(QD_NRATE - 1);
        strcpy(v->val, qd_rate_name(qd_tset_rate(ts)));
        v->prev = k > 0 ? qd_rate_name(qd_ctl_rate_at(k - 1)) : 0;
        v->next = k < QD_NRATE - 1 ? qd_rate_name(qd_ctl_rate_at(k + 1)) : 0;
        v->rate = qd_ratio_f(QD_TPS, L);
        v->pat_pos = qd_ratio_f(qd_mod64(tq, (int32_t)(16 * QD_TPS * td)), QD_TPS * td);   /* (mod 16 steps) */
        {
            uint32_t r;
            qd_udivmod64((uint64_t)tq, (uint32_t)(16 * L * td), &r);   /* (mod 16 of the track's steps) */
            v->track_pos = qd_ratio_f(r, L * td);
        }
        break;
    }
    case UI_TK_LIMIT:
        v->value = ts->limit > QD_POOL_LIMIT_MAX ? QD_POOL_LIMIT_MAX : ts->limit;   /* (P2.6: what the pool plays) */
        v->v = (float)(v->value - 1) / (float)(QD_POOL_LIMIT_MAX - 1);
        ui_itoa(v->val, v->value);
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
    case UI_TK_TAP: {                                   /* P1.5z: SHIFT + B6's taps */
        const int nt = u->ntap < 8 ? u->ntap : 8;
        v->has_col = 1;
        v->col = UI_KSEL;
        v->ntap = nt;
        for (i = 0; i < nt; i++)
            v->tap_age[i] = secs(a->ms - u->tap_ms[(u->ntap - 1 - i) % 8]);
        v->since = nt ? v->tap_age[0] : 0.f;
        v->ext = qd_seq_ext_running(&a->seq);
        v->value = a->bpm;
        v->ntotal = u->ntap;                            /* the TAP ape: the count, the BPM of its last two taps */
        v->tap_cont = u->tap_cont;
        for (i = 0; i < 2; i++) {
            const int m = u->ntap - 1 - i, j0 = m - 3 > 0 ? m - 3 : 0;   /* (tap m, 0-based, and up to three before) */
            const uint32_t sum = m > 0 ? u->tap_ms[m % 8] - u->tap_ms[j0 % 8] : 0u;
            v->tap_bpm[i] = sum ? (int)(60000u * (uint32_t)(m - j0) / sum) : 0;
        }
        break;
    }
    case UI_TK_FOLLOW:                                  /* P1.5z: SHIFT + B8: page follow */
        v->has_col = 1;
        v->col = UI_K2;
        v->value = u->follow;
        strcpy(v->val, u->follow ? "ON" : "OFF");
        v->play_page = qd_ctl_playing_page(a, t);
        break;
    case UI_TK_TRACKBIG:                                /* P1.3y: FX held + ALGORITHM: the track big */
        v->has_col = 1;
        v->col = UI_TRACK_PAL[t & 7];
        v->value = t + 1;
        v->v = (float)t / 7.f;
        for (i = 0; i < QD_NTRACK; i++)
            v->tname[i] = a->model.t[i].name;
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
    v.sound = qd_track_sound_label(t);
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
    if (qd_rec_stepping(a)) {
        const int cs = qd_track_loop_step(ts, u->cursor);
        v.rec = 1;
        v.cursor = cs + 1;
        for (i = 0; i < 16; i++)
            v.page_trig[i] = (uint8_t)qd_step_is_on(qd_step(P, a->sel, (cs / QD_PAGE) * QD_PAGE + i));
    } else if (u->rec) {
        const int64_t L = qd_rate_ticks(qd_tset_rate(ts));
        const int64_t now = qd_seq_tick(&a->seq);
        v.rec = 2;
        v.quantize = a->proj.rec_quant > 0;
        v.pos = qd_seq_track_pos_f(&a->seq, a->sel);
        v.step = qd_seq_track_step(&a->seq, a->sel) + 1;
        for (i = 0; i < u->nroll; i++) {
            const int k = (u->iroll - 1 - i + QD_NROLL) % QD_NROLL;
            const float st = qd_ratio_f(now - u->roll[k].t0, L);
            if (st > 17.f || st < 0.f)
                continue;
            v.roll[v.nroll].start = st;
            v.roll[v.nroll].end = u->roll[k].t1 < 0 ? 0.f : qd_ratio_f(now - u->roll[k].t1, L);
            v.roll[v.nroll].note = u->roll[k].note;
            v.roll[v.nroll].live = u->roll[k].t1 < 0;
            v.nroll++;
        }
    }
    ui_keys_draw(&v);
}


/* P1.5t: a preset for the drawing (its envelope, its kind, its marks) */
static qd_sound_t br_ent[5];                           /* (P2.1b: the browser's and the picker's five: one screen at a
                                                         * time) */
static void pr_fill(const qd_app_t *a, const qd_sound_t *s, int mine, ui_preset_t *p)
{
    const int16_t *q = s->patch;
    qd_sound_t probe;
    memset(p, 0, sizeof *p);
    p->ok = 1;
    p->name = s->name;
    p->tags = s->tags & QD_PK_MASK;
    p->kind = qd_tags_first(p->tags) >= 0 ? qd_tags_first(p->tags) : qd_preset_kind(q);   /* (P1.5v: the first tag) */
    p->env[0] = (float)q[QD_QP_EATK] / 127.f * 0.6f;
    p->env[1] = (float)(q[QD_QP_EDEC] > 126 ? 126 : q[QD_QP_EDEC]) / 127.f * 0.7f;
    p->env[2] = (float)q[QD_QP_ESUS] / 127.f;
    p->env[3] = (float)q[QD_QP_EREL] / 127.f * 0.7f;
    p->mine = mine;
    p->imported = mine && (s->flags & QD_SNDF_IMPORTED);
    p->missing = (s->flags & QD_SNDF_MISSING) != 0;     /* (P1.5w) */
    p->send[0] = s->chorus, p->send[1] = s->delay, p->send[2] = s->reverb;   /* (P1.5x) */
    probe = *s;
    p->in_pool = qd_pool_find(a, &probe) != 0;
}

/* P1.5t: the library browser's model */
static void fill_browse(const qd_app_t *a, ui_browse_t *v)
{
    int *const list = qd_br_buf;                        /* (P2.1b: ctl_presets.c's one list) */
    qd_sound_t *const ent = br_ent;
    const qd_brui_t *b = &a->ui.br;
    const int n = qd_br_list(a, b->src, b->kind, list);
    int d;
    memset(v, 0, sizeof *v);
    v->title = b->src == QD_BR_POOL ? "POOL" : b->src == QD_BR_MINE ? "MY PRESETS" : "PRESETS";
    v->tcol = b->src == QD_BR_POOL ? UI_POOL_COL : UI_LIB_COL;
    v->col = v->tcol;
    v->chip = b->kind;
    v->track = b->t + 1;
    v->n = n;
    v->pos = b->pos < 0 ? 0 : b->pos >= n ? (n ? n - 1 : 0) : b->pos;
    v->pool = b->src == QD_BR_POOL;
    for (d = -2; d <= 2; d++) {
        const int i = v->pos + d;
        if (i < 0 || i >= n || !qd_br_entry(a, list[i], &ent[d + 2]))
            continue;
        pr_fill(a, &ent[d + 2], list[i] >= QD_BR_MINE_CODE && list[i] < QD_BR_POOL_CODE, &v->e[d + 2]);
        if (v->pool) {                                  /* (a pool sound: in the pool; from one of MY presets: its
                                                         * mark) */
            const int m = qd_lib_find(a, ent[d + 2].preset);
            v->e[d + 2].in_pool = 1;
            v->e[d + 2].mine = m >= 0;
            v->e[d + 2].imported = m >= 0 && (qd_lib_flags(a, m) & QD_SNDF_IMPORTED);
        }
    }
    if (n && !v->pool)                                  /* P1.5v: the tray: B10 adds it (+) or takes it out */
        v->e[2].tray = v->e[2].in_pool ? 2 : 1;
    if (v->pool && n)
        v->usteps = qd_pool_users_all(a, list[v->pos] - QD_BR_POOL_CODE, &v->users);
    else if (n && b->out_armed)                         /* (OUT? from ALL / MINE: the pool's copy's users) */
        v->usteps = qd_pool_users_all(a, qd_br_pool_k(a), &v->users);
    if (b->del_armed)                                   /* P1.5w: DELETE?: where it is used */
        v->use_proj = b->use_proj, v->use_steps = b->use_steps;
    v->users_row = v->pool || b->out_armed || (b->del_armed && b->use_proj);
    v->out_armed = b->out_armed;
    v->del_armed = b->del_armed;
    v->arm_t = (b->out_armed || b->del_armed) && b->arm_ms ? secs(a->ms - b->arm_ms) : 0.f;
    v->tag_u = 1.f;
    if (b->tag_ms && a->ms - b->tag_ms < 900u)
        v->tag_k = b->tag_k, v->tag_on = b->tag_on, v->tag_u = (float)(a->ms - b->tag_ms) / 900.f;
    if (b->fx == QD_BRFX_DEL && a->ms - b->fx_ms < 800u) {   /* P1.5v: a MY preset deleted: it breaks up */
        v->fx = b->fx;
        v->fx_u = (float)(a->ms - b->fx_ms) / 800.f;
        pr_fill(a, &b->fx_snd, 1, &v->gone);
    } else if (b->fx && b->fx != QD_BRFX_DEL && a->ms - b->fx_ms < 900u) {
        int k, c = 0;
        v->fx = b->fx;
        v->fx_u = (float)(a->ms - b->fx_ms) / 900.f;
        for (k = 1; k <= QD_NSOUND; k++)
            c += qd_sound_used(&a->proj, k);
        v->pool_n = c;
    }
    {   /* P1.5z: the outline says the pool: lime in it, champagne not; + POOL turns it lime as the cassette lands in
         * the tray, - POOL champagne as it pops out */
        int inp = n && v->e[2].in_pool;
        if (v->fx == QD_BRFX_IN && v->fx_u < 0.6f)
            inp = 0;
        if (v->fx == QD_BRFX_OUT && v->fx_u < 0.45f)
            inp = 1;
        v->ocol = inp ? UI_POOL_COL : UI_LIB_COL;
    }
    if (v->fx == QD_BRFX_OUT)                           /* (P1.5z: - POOL's animation at the foot: the users after) */
        v->users_row = 0;
    if (b->moved && a->ms - b->move_ms < 600u)
        v->sound = (float)(a->ms - b->move_ms) / 600.f;
    if (b->moved && a->ms - b->move_ms < (uint32_t)QD_ROLL_MS)
        v->roll = 1.f - (float)(a->ms - b->move_ms) / QD_ROLL_MS, v->dir = b->move_dir;
    v->t = secs(a->ms);
}

/* P1.5t: the step's pool picker (a step held, or the latch's selection, + PRESETS): TRACK, then the pool */
static void draw_picker(const qd_app_t *a)
{
    qd_sound_t *const ent = br_ent;
    ui_browse_t v;
    qd_rstep_t r;
    int list[QD_NSOUND + 1], n = 0, k, at = 0, d;
    static char title[32];
    memset(&v, 0, sizeof v);
    qd_step_resolve(qd_ctl_cpat(a), a->sel, a->ui.tk_step, &r);
    list[n++] = 0;                                      /* TRACK */
    for (k = 1; k <= QD_NSOUND; k++)
        if (qd_sound_used(&a->proj, k)) {
            if (k == r.sound)
                at = n;
            list[n++] = k;
        }
    snprintf(title, sizeof title, "STEP %d \xc2\xb7 SOUND", a->ui.tk_step + 1);
    v.picker = 1;
    v.title = title;
    v.n = n - 1;
    v.pos = at;
    for (d = -2; d <= 2; d++) {
        const int i = at + d;
        if (i < 0 || i >= n)
            continue;
        if (!list[i]) {
            v.e[d + 2].ok = 1;
            v.e[d + 2].track = 1;
            continue;
        }
        qd_sound_read(&a->proj, list[i], &ent[d + 2]);
        pr_fill(a, &ent[d + 2], 0, &v.e[d + 2]);
        v.e[d + 2].in_pool = 1;
    }
    v.roll = a->ms - a->ui.tk_ms < (uint32_t)QD_ROLL_MS ? 1.f - (float)(a->ms - a->ui.tk_ms) / QD_ROLL_MS : 0.f;
    v.dir = a->ui.tk_dir;
    v.t = secs(a->ms);
    ui_browse_draw(&v);
}

/* P1.5u: the typewriter's model (SAVE PRESET's name, a project's) */
static void fill_type(const qd_app_t *a, ui_type_t *v)
{
    static char sub[4];
    const qd_typer_t *y = &a->ui.ty;
    const uint32_t ms = a->ms;
    const int save = a->ui.latch == QD_LATCH_SAVE;
    int k;
    memset(v, 0, sizeof *v);
    v->title = save ? "SAVE PRESET" : a->ui.nm.target == 3 ? "TRACK NAME" : "PROJECT NAME";
    v->track = save ? a->sel + 1 : a->ui.nm.target == 3 ? (a->ui.nm.proj & 7) + 1 : 0;
    v->name = y->name;
    v->sel = y->sel;
    v->cur = y->sel ? (int)strlen(y->name) : y->cur;
    v->spark = y->spark;
    v->hit = y->hit;
    v->fresh = y->hit_ms && ms - y->hit_ms < 1200u ? y->fresh : -1;
    if (y->roll_ms && ms - y->roll_ms < 1200u && (!y->hit_ms || y->roll_ms > y->hit_ms))
        v->fresh = y->fresh, v->hit = -1;               /* (SELECT turned it: champagne, no flight) */
    v->fly = y->hit_ms && v->hit >= 0 && ms - y->hit_ms < 350u ? (float)(ms - y->hit_ms) / 350.f : 1.f;
    v->glow = -1;                                       /* the key held (or just pressed) glows */
    for (k = 0; k < 27 && v->glow < 0; k++)
        if ((a->keys_held >> (k < 11 ? QD_BLACK_KEY[k] : QD_WHITE_KEY[k - 11])) & 1u)
            v->glow = k;
    if (v->glow < 0 && y->hit_ms && ms - y->hit_ms < (uint32_t)QD_TY_HIT_MS)
        v->glow = y->hit;
    v->shift = qd_ctl_shift(a);
    v->full = y->full_ms && ms - y->full_ms < (uint32_t)QD_TY_FULL_MS ? (float)(ms - y->full_ms) / QD_TY_FULL_MS : 1.f;
    v->del_ch = y->del_ch;
    v->del_at = y->del_at;
    v->del = y->del_ms && ms - y->del_ms < (uint32_t)QD_TY_DEL_MS ? (float)(ms - y->del_ms) / QD_TY_DEL_MS : 1.f;
    v->ideas_on = y->idea_ms && ms - y->idea_ms < (uint32_t)QD_TY_IDEA_MS && y->nhist;
    v->nhist = y->nhist;
    v->hpos = y->hpos;
    for (k = 0; k < y->nhist && k < QD_UI_NHIST; k++)
        v->hist[k] = qd_type_hist(a, k);
    v->dice = y->dice_ms && !y->hpos ? (ms - y->dice_ms >= (uint32_t)QD_TY_DICE_MS ? 1.f
                                        : (float)(ms - y->dice_ms) / (float)QD_TY_DICE_MS) : -1.f;
    v->dice_face = y->dice_face;
    if (save) {
        char nm[QD_SOUND_NAME];
        v->replace = a->ui.sv.replace >= 0;
        v->choice = v->replace && a->ui.sv.choice;
        qd_type_trimmed(a, nm);
        v->newname = strcmp(nm, a->ui.sv.newname) ? a->ui.sv.newname : 0;
    } else if (a->ui.nm.target != 3) {
        const int p = a->ui.nm.proj < 0 ? 0 : a->ui.nm.proj;
        sub[0] = (char)('0' + (p + 1) / 10), sub[1] = (char)('0' + (p + 1) % 10), sub[2] = 0;
        v->sub = sub;
    }
    v->t = secs(ms);
}

/* P1.5t: LOADED, BACK, SAVED */
static void draw_pfx(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    ui_preset_t s, own;
    const float u01 = (float)(a->ms - u->tk_ms) / (float)(QD_TK_MS + QD_TK_FADE_MS);
    memset(&s, 0, sizeof s), memset(&own, 0, sizeof own);
    if (u->tk == QD_TK_BACK) {
        static char lab[QD_SOUND_NAME + 3];
        if (u->tk_snd2.name[0])
            pr_fill(a, &u->tk_snd2, 0, &s);
        pr_fill(a, &u->tk_snd, 0, &own);
        snprintf(lab, sizeof lab, "%s%s", u->tk_snd.name, u->tk_snd.flags ? " *" : "");
        own.name = lab;
        own.in_pool = 0;
        ui_pfx_draw(UI_PFX_BACK, &s, &own, a->sel + 1, u01, secs(a->ms));
        return;
    }
    pr_fill(a, &u->tk_snd, u->tk == QD_TK_SAVED, &s);
    ui_pfx_draw(u->tk == QD_TK_SAVED ? UI_PFX_SAVED : UI_PFX_LOADED, &s, 0, a->sel + 1, u01, secs(a->ms));
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
        cat(cat(cat(v.sub, qd_note_name(v.lo_note, n)), " \xc2\xb7 "), scale_buf);   /* (P1.3v: no "B1 =") */
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
        if (qd_proj_used(&a->proj, v.bank * 16 + k))
            v.used |= (uint16_t)(1u << k);
    for (k = 0; k < QD_NPAT; k++)
        if (qd_proj_used(&a->proj, k))
            v.bank_used |= (uint8_t)(1u << (k / 16));
    v.playing = a->seq.playing;
    if (v.queued >= 0 && a->seq.playing) {
        const int64_t m = qd_pat_master_ticks(qd_proj_cpat(&a->proj, a->seq.cur)), left = qd_seq_to_change(&a->seq);
        v.left = m > 0 ? qd_ratio_f(left, m) : 0.f;
        v.left = v.left < 0.f ? 0.f : v.left > 1.f ? 1.f : v.left;
    }
    v.blink = (a->ms / QD_BLINK_MS) % 2u == 0;
    v.launch = a->proj.launch;
    v.launch_name = qd_launch_name(a->proj.launch);
    ui_patterns_draw(&v);
}


static void draw_idle(const qd_app_t *a)
{
    ui_idle_t v;
    memset(&v, 0, sizeof v);
    v.bpm = a->bpm;
    v.playing = a->playing;
    v.t = a->playing ? secs(a->ms) : (float)(a->ms * 2u / 125u) / 16.f;   /* P2.6d (the owner: ~15 fps idle):
                                                         * stopped, the stream's clock in 1/16 s steps (exact in a
                                                         * float: its 11 s loop still seamless); between them the same
                                                         * frame, every strip kept */
    v.beat = a->playing ? a->beat : 0;                 /* (P2.4: whole beats and the ticks into one) */
    v.beat_num = a->playing ? a->beat_rem : 0;
    v.beat_den = QD_BEAT_TICKS;
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
                                                "CONDITION", "TRIG", "FUNCTION", "SOUND", ""};
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
        qd_step_t on;                                   /* the track's defaults: a step with no overrides */
        memset(&on, 0, sizeof on);
        on.flags = QD_SF_ON;
        qd_step_resolve_def(&P->trk[t].def, &on, &r);
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
        case UI_F_FUNC: qd_fn_name(is_default || !u->nheld ? 0 : r.func, v->val); break;
        case UI_F_SOUND: {
            const int k = is_default || !u->nheld ? a->model.t[t].sound : r.sound;
            if (k > 0) {
                cat(cat(v->val, ui_itoa(n, k)), " ");
                cat(v->val, is_default || !u->nheld ? qd_track_sound_label(&a->model.t[t]) : qd_sound_name(&a->proj, k));
            } else {
                cat(v->val, "TRACK");
            }
            break;
        }
        default: break;
        }
        break;
    case QD_KJ_CHORD:                                   /* P1.5o: the chord: its name (or note 1's) */
        v->name = "CHORD";
        {
            char rootn[8], q[16];
            uint8_t nn[QD_NNOTE];
            int c = 0, k;
            for (k = 0; k < QD_NNOTE; k++)              /* (the slots' notes, the empty ones left out) */
                if (r.note[k])
                    nn[c++] = r.note[k];
            if (r.note[0] && qd_chord_name(nn, c, rootn, q))
                cat(cat(v->val, rootn), q);
            else if (r.note[0])
                qd_note_name(r.note[0], v->val);
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
    case QD_KJ_MFX: {                                   /* P1.6c: the track's MIDI FX group (the default) */
        const int g = qd_ctl_cpat(a)->trk[t].mfx;
        v->name = "MIDI FX";
        if (g)
            ui_itoa(v->val, g);
        else
            cat(v->val, "OFF");
        break;
    }
    default:
        break;
    }
}

int qd_view_screen(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    if (u->mode != QD_MODE_IDLE && qd_ctl_seq_map(a))   /* (P1.5a: after a hold, or a key: a tap is the project) */
        return QD_SCR_SEQ;
    if (u->mode != QD_MODE_IDLE && qd_ctl_sel_map(a))   /* (P1.3y: after a hold, or a key: a tap shows nothing) */
        return QD_SCR_SEL;
    if (u->mode == QD_MODE_STEP && QD_LATCH_MENU(u->latch))
        return QD_SCR_MENU;                             /* P1.3y: a settings menu */
    if (u->latch != QD_LATCH_MFX && qd_ctl_main_layer(a) != QD_LAYER_NONE) {   /* P1.6h: the mix, the effects, the
                                                         * editor: F1 / F2 held, the step view's picture (F2 after a
                                                         * knob's unlock: the page) */
        const int l = qd_ctl_layer_shown(a);
        if (l == QD_LAYER_F1 || (l == QD_LAYER_F2 && !u->f2_knob))
            return QD_SCR_LAYER;
    }
    if (qd_snd_open(a))
        return QD_SCR_SOUND;                            /* P1.4a: the sound editor */
    if (qd_mfx_open(a))
        return QD_SCR_MFX;                              /* P1.6c: the MIDI FX screen */
    if (u->mode == QD_MODE_STEP && u->latch == QD_LATCH_BROWSE)
        return QD_SCR_BROWSE;                           /* P1.5t: the library browser */
    if (u->mode == QD_MODE_STEP && u->latch == QD_LATCH_SAVE)
        return QD_SCR_SAVE;                             /* P1.5t: SAVE PRESET */
    if (u->mode == QD_MODE_STEP && u->latch == QD_LATCH_NAME)
        return QD_SCR_NAME;                             /* P1.5a: the name editor */
    if (u->mode == QD_MODE_STEP && u->latch == QD_LATCH_PRJ)
        return QD_SCR_PRJ;                              /* P1.5a: the project pages */
    if (u->mode == QD_MODE_STEP && u->latch == QD_LATCH_MIX)
        return QD_SCR_MIX;                              /* P1.4b: the mix */
    if (u->mode == QD_MODE_STEP && u->latch == QD_LATCH_FX)
        return QD_SCR_FX;                               /* P1.4b: the effects */
    switch (u->mode) {
    case QD_MODE_KEYS:
        return qd_ctl_shift(a) && !u->shift_used ? QD_SCR_SHIFT : QD_SCR_KEYS;
    case QD_MODE_STEP:
        if (u->view == QD_VIEW_EDIT) {                  /* P1.3r: EDIT keeps holding, latching, a mode key in it;
                                                         * the layer maps and SHIFT's take the screen as anywhere */
            if (qd_ctl_layer_shown(a) != QD_LAYER_NONE)
                return QD_SCR_LAYER;
            if (qd_ctl_shift(a) && !u->shift_used)
                return QD_SCR_SHIFT;
            return QD_SCR_EDIT;
        }
        if (u->slatch && qd_ctl_layer_shown(a) != QD_LAYER_NONE)   /* P1.5m: the latch's selection, an F layer: its map */
            return QD_SCR_LAYER;
        if (u->nheld && u->hmode != UI_F_PLOCK &&      /* (P-LOCK: holding keeps the screen; so does the step */
            !u->scursor)                                /* cursor, P1.3q, in every view since P1.3y) */
            return QD_SCR_HELD;
        if (qd_ctl_mkey_shown(a))                       /* P1.3e: a mode key held: the track's default */
            return QD_SCR_DEFAULT;
        if (qd_ctl_layer_shown(a) != QD_LAYER_NONE)
            return QD_SCR_LAYER;
        if (qd_ctl_shift(a) && !u->shift_used)
            return QD_SCR_SHIFT;
        return QD_VIEW_GEEK(u->view) ? QD_SCR_GEEK : QD_SCR_LANE;   /* (P2.11: a Geek Out page in the lane's place) */
    default:
        return QD_SCR_IDLE;
    }
}

/* P1.3r: the EDIT view's models: the page's row (as STEPS'), and the mode's picture: the step held / selected, else
 * (nothing held, or a mode key held) the track's default */
/* P1.5q (the owner: "while I am playing, the glyph updates to show the value of each step that it plays"): EDIT, playing,
 * nothing held, the latch off, no mode key, no takeover, no menu: the picture is the selected track's step as it plays,
 * changing when it is heard (its micro-timing, the track's rate); an empty step keeps the last, dimmed a little */
int qd_view_edit_follows(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    if (!a->seq.playing || u->fol_step < 0 || u->fol_t != a->sel || u->view != QD_VIEW_EDIT || u->mode != QD_MODE_STEP ||
        u->latch || u->slatch || u->nheld || u->wk_pending || qd_ctl_mkey(a) >= 0 || u->tk != QD_TK_NONE)
        return -1;                                      /* (P1.6g, the owner's one rule: the step latch on with nothing
                                                         * selected is the track's values: no following; P1.5s followed) */
    return u->fol_step;
}

void qd_view_edit(qd_app_t *a, ui_held_t *h, ui_lane_t *l)
{
    static char pat[4];
    const qd_ui_t *u = &a->ui;
    const int mk = qd_ctl_mkey_shown(a);
    const int preset = u->tk == QD_TK_PRESET && a->ms - u->tk_ms < QD_TK_MS + QD_TK_FADE_MS;
    const int fs = qd_view_edit_follows(a);
    fill_lane(a, l, pat);
    if (fs >= 0) {                                      /* P1.5q: the step playing */
        static ui_held_t hd;                            /* (P1.5z: the head stays the track's default's) */
        const qd_pattern_t *P = qd_ctl_cpat(a);
        const qd_tset_t *ts = &P->trk[a->sel];
        const qd_step_t *st = qd_step(P, a->sel, fs);
        const float step_s = (float)(int32_t)qd_rate_ticks(qd_tset_rate(ts)) / (float)QD_TPS * 15.f /
                             (float)(a->bpm > 0 ? a->bpm : 120);
        const int ph = qd_seq_track_step(&a->seq, a->sel);
        float k;
        qd_rstep_t r;
        fill_held_at(a, h, 0, fs);
        h->nheld = 0;
        h->follow = 1;
        h->play = secs(a->ms - u->fol_ms);              /* (each picture's "the step plays": the kick, the reels ..) */
        h->squeeze = h->play;                           /* (the chord's name squeezes in) */
        k = h->play / (step_s > 0.f ? step_s : 0.125f);
        h->ball = k > 0.999f ? 0.999f : k;              /* (the ball in, then along its bounces, within the step) */
        h->walk = (float)h->micro + k * 24.f;           /* (MICRO's playhead from the flag on, at the step's pace) */
        h->ovr = u->hmode == UI_F_FUNC ? st->func != 0
               : u->hmode == UI_F_MICRO ? qd_step_overrides(st, QD_PM_MICRO) || qd_step_overrides(st, QD_PM_STRUM)
               : u->hmode == UI_F_PLOCK ? (st->flags & QD_SF_LOCKS) != 0 : qd_step_overrides(st, mode_pm(u->hmode));
        if (ph >= 0 && ph != fs) {
            qd_step_resolve(P, a->sel, ph, &r);
            h->dim = !r.on;                             /* (the playhead on an empty step: the last step, dimmed) */
        }
        fill_held(a, &hd, 1);                           /* P1.5z: the head: "TRACK DEFAULT · MODE", its colours, its
                                                         * value, as when not playing */
        hd.head = 0;
        h->head = &hd;
        return;
    }
    fill_held(a, h, !u->nheld || mk || (preset && !u->tk_lock));
    if (!mk)
        h->reset = 0;
    if (preset) {                                       /* P1.3y: PRESETS turning: the sound picture in the top */
        h->field = UI_F_SOUND;
        h->ovr = h->is_default || h->sound > 0;
    }
}

/* P1.4a: the sound editor's model: the page's values (the held step's: its sound lock's patch with its locks, else the
 * track's sound), the track's beside them, the locks, what the drawing needs */
/* P1.5b: some step of track t locks id */
static int track_locks(const qd_pattern_t *p, int t, int id)
{
    int i;
    for (i = 0; i < p->lock_n[t]; i++)
        if (p->lock[p->lock_start[t] + i].id == id)
            return 1;
    return 0;
}

/* a value's text ("0.25", "16", "1.00") as a number, as atof gives it (P2.4: no double): the digits and the decimals,
 * divided once (the nearest float) */
static float ratio_of_text(const char *s)
{
    int64_t m = 0, d = 1;
    int neg = 0, dot = 0;
    while (*s == ' ')
        s++;
    if (*s == '-' || *s == '+')
        neg = *s++ == '-';
    for (; (*s >= '0' && *s <= '9') || (*s == '.' && !dot); s++) {
        if (*s == '.') {
            dot = 1;
            continue;
        }
        if (m < 100000000) {
            m = m * 10 + (*s - '0');
            if (dot)
                d *= 10;
        }
    }
    return qd_ratio_f(neg ? -m : m, d);
}

void qd_view_snd(qd_app_t *a, ui_snd_t *v)
{
    static qd_trec_t r;
    const qd_ui_t *u = &a->ui;
    const qd_sndpage_t *pg = &QD_SND_PAGES[u->snd_page];
    const int8_t *ids = qd_snd_ids(a, u->snd_page);   /* (E1: AMP in AHD its alt) */
    const int16_t *tp = a->model.t[a->sel].patch, *pp = tp;
    int k, i;
    memset(v, 0, sizeof *v);
    v->name = pg->name;
    v->draw = pg->draw;
    v->arg = pg->arg;
    for (i = 0; i < QD_SND_NPAGE; i++)
        if (QD_SND_PAGES[i].group == pg->group) {
            if (i == u->snd_page)
                v->ipg = v->npg;
            v->npg++;
        }
    if (u->nheld) {
        qd_seq_step_rec(&a->seq, a->seq.cur, a->sel, u->held[0], &r);
        pp = r.patch;
        v->step = u->held[0] + 1;
    } else if (a->seq.playing && !u->slatch && u->fol_t == a->sel && u->fol_step >= 0) {
        qd_seq_step_rec(&a->seq, a->seq.cur, a->sel, u->fol_step, &r);   /* P1.6g, the one rule: the latch off, nothing
                                                         * held, playing: the step as it plays (its sound, its locks;
                                                         * shown only) */
        pp = r.patch;
        v->follow = 1;
    }
    for (k = 0; k < 5; k++) {
        const int id = ids[k];
        int16_t x;
        if (id < 0)
            continue;
        v->has[k] = 1;
        v->v[k] = pp[id];
        v->tv[k] = tp[id];
        qd_quad_value_text(id, v->v[k], v->txt[k]);
        v->lab[k] = qd_snd_label(a, u->snd_page, k);  /* (E2: the FILTER's by its machine) */
        if (pg->draw == UI_SD_FILTER && k == 4)         /* (E2: ALGORITHM the machine's name) */
            snprintf(v->txt[4], sizeof v->txt[4], "%s", QD_SND_MACHINE[qd_snd_machine(pp[QD_QP_FTYPE])]);
        v->locked[k] = (uint8_t)(u->nheld ? qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[0], id, &x)
                                 : v->follow && qd_lock_get(qd_ctl_cpat(a), a->sel, u->fol_step, id, &x));
        v->lockany[k] = (uint8_t)(!u->nheld && track_locks(qd_ctl_cpat(a), a->sel, id));
        if (id == QD_QP_RB1) {                          /* P1.5i: RATIO B, the pair: B2 (the fast hand) over B1 */
            v->pair[k] = 1;
            v->v[k] = pp[QD_QP_RB1] * 19 + pp[QD_QP_RB2];
            v->tv[k] = tp[QD_QP_RB1] * 19 + tp[QD_QP_RB2];
            qd_quad_value_text(QD_QP_RB2, pp[QD_QP_RB2], v->txt[k]);
            qd_quad_value_text(QD_QP_RB1, pp[QD_QP_RB1], v->den[k]);
            v->lab[k] = "RATIO B";
            v->locked[k] |= (uint8_t)(u->nheld && qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[0], QD_QP_RB2, &x));
            v->lockany[k] |= (uint8_t)(!u->nheld && track_locks(qd_ctl_cpat(a), a->sel, QD_QP_RB2));
        }
        {   /* E4: a modulation slot's destination: its source's mark */
            int ms;
            for (ms = 0; ms < QD_NMOD && !v->modmark[k]; ms++)
                if (pp[QD_QP_MOD(ms, QD_QM_SRC)] && pp[QD_QP_MOD(ms, QD_QM_DEPTH)] &&
                    qd_quad_dest_of(id, 1) == pp[QD_QP_MOD(ms, QD_QM_DST)] && qd_quad_dest_of(id, 1) > 0)
                    v->modmark[k] = (uint8_t)pp[QD_QP_MOD(ms, QD_QM_SRC)];
        }
        {   /* P1.5k: the LFOs whose destination this value is (their marks by its label) */
            int lf;
            for (lf = 0; lf < 3; lf++) {
                const int dst = pp[QD_QP_LFO(lf, QD_QL_DEST)];
                if (dst > 0 && qd_quad_dest_of(id, lf) == dst)
                    v->lfomark[k] |= (uint8_t)(1u << lf);
            }
        }
        if (id == QD_QP_MIX && (pg->draw == UI_SD_ALGO || pg->draw == UI_SD_TONE)) {   /* P1.5i: MIX on RATIO (P1.5n:
                                                         * and TONE): the drawing's balance */
            int fb, ox, oy;
            v->has_mix = 1;
            v->mix = pp[QD_QP_MIX];
            qd_quad_algo(pp[QD_QP_ALGO], v->mod, &fb, &ox, &oy);
            v->outx = (uint8_t)ox;
            v->outy = (uint8_t)oy;
        }
    }
    if (qd_snd_keys(a)) {                               /* P1.5b: the keyboard on: its keys' states (as the LEDs) */
        uint8_t lv[QD_NKEY];
        int i;
        memset(lv, 0, sizeof lv);
        qd_keys_leds(a, lv);
        v->keys = a->proj.rec_ekeys ? 2 : 1;
        for (i = 0; i < (v->keys == 1 ? 16 : QD_NKEY); i++) {
            const uint8_t l = lv[v->keys == 1 ? QD_WHITE_KEY[i] : i];
            v->kst[i] = (uint8_t)(l == QD_LED_LIT ? 4 : l == QD_LED_80 ? 2 : l == QD_LED_HALF ? 1 : 0);
        }
    }
    v->last = u->snd_knob;
    v->t = secs(a->ms);
    v->fpop = u->snd_fpop || (u->snd_fpop_ms && a->ms - u->snd_fpop_ms < QD_FPOP_SHOW_MS &&
                              !strcmp(QD_SND_PAGES[u->snd_page].name, "FILTER"));   /* P1.6d: the machines' popup (P1.6g:
                                                         * ALGORITHM's too, a second after its last detent) */
    v->fpop_sel = qd_snd_machine_pos(a);
    if (u->rnd_kind) {                                  /* P1.5c: the dice, the bottle */
        const uint32_t len = u->rnd_kind == 1 ? QD_RND_PAGE_HOLD_MS : u->rnd_kind == 4 ? QD_RND_DICE_MS
                           : u->rnd_kind == 2 ? QD_RND_HOLD_MS : QD_RND_BURST_MS;
        v->rnd_kind = u->rnd_kind;
        v->rnd_u = (float)(a->ms - u->rnd_ms) / (float)len;
        v->rnd_u = v->rnd_u > 1.f ? 1.f : v->rnd_u;
    }
    if (u->snd_lk == 4 && (u->snd_lk_used || a->ms - u->snd_lk_ms >= QD_TAP_MS)) {   /* E4: B1 held: a slot's mapping */
        const int dest = u->snd_lk_k >= 0 || u->snd_lk_id ? qd_quad_dest_of(u->snd_lk_id, 1) : -1;
        const int slot = dest > 0 ? qd_snd_mod_slot_of(a, u->mod_src, dest) : -1;
        v->lk = 4;
        v->lk_src = u->mod_src ? u->mod_src : 1;
        v->lk_k = u->snd_lk_k;
        v->lk_ok = 1;
        if (u->snd_lk_bad_ms && a->ms - u->snd_lk_bad_ms < 450u) {
            v->lk_ok = 0;
            v->lk_shake = 1.f - (float)(a->ms - u->snd_lk_bad_ms) / 450.f;
            v->lk_name = u->snd_lk_id == QD_QP_RB1 ? "RATIO B" : qd_quad_value_name(u->snd_lk_id);
        } else if (slot >= 0) {
            v->lk_name = qd_quad_dest_name(dest);
            v->lk_depth = (float)pp[QD_QP_MOD(slot, QD_QM_DEPTH)] / 127.f;
        }
    } else if (u->snd_lk && u->snd_lk < 4 && (u->snd_lk_used || a->ms - u->snd_lk_ms >= QD_TAP_MS)) {   /* P1.5k: an LFO
                                                         * key held: the mapping */
        const int lf = u->snd_lk - 1, dst = pp[QD_QP_LFO(lf, QD_QL_DEST)];
        const int16_t dep = pp[QD_QP_LFO(lf, QD_QL_DEPTH)];
        v->lk = u->snd_lk;
        v->lk_k = u->snd_lk_k;
        v->lk_ok = 1;
        if (u->snd_lk_bad_ms && a->ms - u->snd_lk_bad_ms < 450u) {   /* (refused: the value's name dim, shaking) */
            v->lk_ok = 0;
            v->lk_shake = 1.f - (float)(a->ms - u->snd_lk_bad_ms) / 450.f;
            v->lk_name = qd_quad_value_name(u->snd_lk_id);
            if (u->snd_lk_id == QD_QP_RB1)
                v->lk_name = "RATIO B";
        } else if (dst > 0) {
            const int did = qd_quad_dest_id(dst);
            v->lk_name = did >= 0 ? (did == QD_QP_RB1 ? "RATIO B" : qd_quad_value_name(did)) : qd_quad_dest_name(dst);
        }
        v->lk_depth = (float)dep / 6400.f;
        if (v->lk_k < 0)                                /* (not turned yet: the column already mapped, if on this page) */
            for (k = 0; k < 4; k++)
                if (ids[k] >= 0 && dst > 0 && qd_quad_dest_of(ids[k], lf) == dst)
                    v->lk_k = k;
    }
    if (pg->draw == UI_SD_ALGO) {
        static const int RID[4] = {QD_QP_RC, QD_QP_RA, QD_QP_RB1, QD_QP_RB2};
        char t[12];
        int fb, ox, oy;
        qd_quad_algo(pp[QD_QP_ALGO], v->mod, &fb, &ox, &oy);
        v->fb = (uint8_t)fb;
        v->outx = (uint8_t)ox;
        v->outy = (uint8_t)oy;
        for (k = 0; k < 4; k++)
            v->ratio[k] = ratio_of_text(qd_quad_value_text(RID[k], pp[RID[k]], t));
    } else if (pg->draw == UI_SD_LFO) {
        v->pv[0] = pp[QD_QP_LFO(pg->arg, QD_QL_WAVE)];
        v->pv[1] = pp[QD_QP_LFO(pg->arg, QD_QL_TRIG)];
        v->pv[2] = pp[QD_QP_LFO(pg->arg, QD_QL_FADE)];
    } else if (pg->draw == UI_SD_FILTER) {             /* E2: the machine, its shape, its gain */
        v->pv[0] = pp[QD_QP_FTYPE];
        v->pv[1] = pp[QD_QP_FSHAPE];
        v->pv[2] = pp[QD_QP_FGAIN];
    } else if (pg->draw == UI_SD_MOD) {                /* E4: the eight slots; ALGORITHM the chosen one */
        int ms;
        static char slot[4];
        for (ms = 0; ms < QD_NMOD; ms++) {
            v->mod_src[ms] = pp[QD_QP_MOD(ms, QD_QM_SRC)];
            v->mod_dep[ms] = pp[QD_QP_MOD(ms, QD_QM_DEPTH)];
            v->mod_dst[ms] = qd_quad_dest_name(pp[QD_QP_MOD(ms, QD_QM_DST)]);
        }
        v->mod_sel = u->mod_slot;
        slot[0] = (char)('1' + u->mod_slot), slot[1] = 0;
        v->has[4] = 1;
        snprintf(v->txt[4], sizeof v->txt[4], "%s", slot);
        v->lab[4] = "SLOT";
    } else if (pg->draw == UI_SD_CRUSH) {              /* E3: the step's width, its height (BITS' bits) */
        if (pp[QD_QP_SRR]) {                            /* (2^32 / inc, rounded: in integers, P2.4) */
            const uint32_t inc = qd_quad_srr_inc(pp[QD_QP_SRR]);
            uint32_t r;
            const uint64_t q = qd_udivmod64((uint64_t)1 << 32, inc, &r);
            v->pv[0] = (int)(q + ((uint64_t)r * 2u >= inc));
        } else {
            v->pv[0] = 0;
        }
        v->pv[1] = pp[QD_QP_BITS] ? qd_quad_crush_bits(pp[QD_QP_BITS]) : 0;
    } else if (pg->draw == UI_SD_BAND) {               /* E2: BWPOST */
        v->pv[0] = pp[QD_QP_BWPOST];
    } else if (pg->draw == UI_SD_LFOSHAPE) {
        v->pv[0] = pp[QD_QP_LFO(pg->arg, QD_QL_SPEED)];
        v->pv[1] = pp[QD_QP_LFO(pg->arg, QD_QL_DEPTH)];
    }
}

/* P1.6h, STEPS: F1 / F2's last key a step (or no group: no slots): the picture is the step's (copied, the hand) */
static void mfx_fstep(const qd_app_t *a, ui_mfx_t *v, int force)
{
    const qd_ui_t *u = &a->ui;
    if ((u->mfx_fstep || force) && u->mode == QD_MODE_STEP && !qd_mfx_lane_page(a)) {
        v->fstep = 1;
        v->f1 = v->f1 || (u->f1_copied && ((a->buttons_held >> QD_B_OCTDN) & 1u));
        v->f2 = (int)((a->buttons_held >> QD_B_OCTUP) & 1u);
        v->f1_src = -1;                             /* (no slot marked) */
        v->st_copied = u->f1_copied;
        v->f2_hand = u->f2_loaded && u->f2_holding ? 1 : -1;
        if (u->clip.st.valid) {
            qd_rstep_t r;
            qd_step_resolve_def(&qd_ctl_cpat(a)->trk[a->sel].def, &u->clip.st.step, &r);
            v->st_type = r.type;
            v->st_nnote = r.type == QD_TRIG_LOCK ? 0 : r.nn;
            memcpy(v->st_note, r.notes, sizeof v->st_note);
        }
    }
}

/* P1.6c: the MIDI FX screen's model: the selected track's group, its rail (the notes passing: the glow), the selected
 * slot's values and, for an ARP, what it holds and plays now */
void qd_view_mfx(qd_app_t *a, ui_mfx_t *v)
{
    qd_ui_t *u = &a->ui;
    const qd_pattern_t *P = qd_ctl_cpat(a);
    const int g = qd_mfx_tgroup(a);
    int k, t;
    memset(v, 0, sizeof *v);
    v->g = g;
    v->track = a->sel + 1;
    v->t = secs(a->ms);
    v->lift_from = -1;
    v->drag_from = v->drop = v->f1_src = v->f2_hand = -1;
    v->steps_frame = qd_mfx_steps_held(a, 0) > 0;
    v->last = -1;
    v->lane_cur = -1;
    v->sel_branch = -1;
    for (t = 0; t < QD_PAT_NTRACK; t++)
        if (g && P->trk[t].mfx == g)
            v->tr[v->ntr++] = (uint8_t)(t + 1);
    if (!g)
        v->tr[v->ntr++] = (uint8_t)(a->sel + 1);
    if (u->mfx_gsel_ms && a->ms - u->mfx_gsel_ms < QD_TK_MS) {   /* ALGORITHM: the five groups (P1.6d: gliding) */
        int gi;
        v->gsel = 1;
        v->gsel_pos = (float)qd_mfx_glide(a, u->mfx_gsel_from, u->mfx_gsel_to, u->mfx_gsel_t0) / 100.f;
        for (gi = 0; gi < QD_MFX_NGROUP; gi++) {
            for (k = 0; k < QD_MFX_NSLOT; k++)
                v->gtype[gi][k] = a->proj.mfx[gi].slot[k].type;
            for (t = 0; t < QD_PAT_NTRACK; t++)
                if (t != a->sel && P->trk[t].mfx == gi + 1 && v->gtrn[gi] < 8)
                    v->gtr[gi][v->gtrn[gi]++] = (uint8_t)(t + 1);
        }
    }
    if (!g) {
        mfx_fstep(a, v, 1);                             /* (P1.6h: no group, no slots: F1 / F2 the steps') */
        return;
    }
    {
        const qd_mfx_group_t *G = &a->proj.mfx[g - 1];
        const qd_mfx_grt_t *R = &a->player.mfx.g[g - 1];
        const qd_mfx_slot_t *s = &G->slot[u->mfx_slot & 7];
        if (u->mfx_seen_g != g) {
            u->mfx_seen_g = g;
            for (k = 0; k < QD_MFX_NSLOT; k++)
                u->mfx_seen[k] = R->passed[k], u->mfx_hit_ms[k] = 0;
        }
        for (k = 0; k < QD_MFX_NSLOT; k++) {
            v->type[k] = G->slot[k].type;
            v->byp[k] = G->slot[k].bypass;
            if (R->passed[k] != u->mfx_seen[k])
                u->mfx_seen[k] = R->passed[k], u->mfx_hit_ms[k] = a->ms ? a->ms : 1u;
            if (u->mfx_hit_ms[k] && a->ms - u->mfx_hit_ms[k] < 300u)
                v->glow[k] = 1.f - (float)(a->ms - u->mfx_hit_ms[k]) / 300.f;
        }
        v->flow = qd_mfx_busy(&a->player.mfx, g) || (a->seq.playing && R->sent);
        v->sel = u->mfx_slot & 7;
        v->npages = qd_mfx_npages(s->type);
        v->page = v->npages ? u->mfx_page % v->npages : 0;
        v->byp_sel = s->type && s->bypass;
        memcpy(v->v, s->v, sizeof v->v);
        memcpy(v->lane, s->lane, sizeof v->lane);
        if (u->mfx_arm == v->sel && s->type)
            v->arm = 1.f;
        if (u->mfx_lift_from >= 0 && u->mfx_lift_ms && a->ms - u->mfx_lift_ms < 900u) {
            v->lift_from = u->mfx_lift_from, v->lift_to = u->mfx_lift_to;
            v->lift_u = (float)(a->ms - u->mfx_lift_ms) / 900.f;
        }
        if (u->mfx_bheld) {                             /* P1.6e: a slot key held: lifted, its move previewed */
            const uint32_t held_ms = a->ms - u->mfx_bms;
            v->drag_from = u->mfx_dfrom, v->drag_to = u->mfx_dto, v->drag_prev = u->mfx_dprev;
            v->drag_moved = u->mfx_bused;
            v->drag_u = u->mfx_dms ? (float)(a->ms - u->mfx_dms) / 160.f : 1.f;
            v->drag_u = v->drag_u > 1.f ? 1.f : v->drag_u;
            v->drag_up = u->mfx_bused ? 1.f : held_ms < QD_TAP_MS ? 0.f : (float)(held_ms - QD_TAP_MS) / 150.f;
            v->drag_up = v->drag_up > 1.f ? 1.f : v->drag_up;
            if (!u->mfx_bused)
                v->drag_u = 1.f;
            v->lift_from = -1;
        }
        if (u->mfx_drop >= 0 && a->ms - u->mfx_drop_ms < 220u) {
            v->drop = u->mfx_drop;
            v->drop_u = (float)(a->ms - u->mfx_drop_ms) / 220.f;
        }
        v->f1 = u->mfx_f1 && ((a->buttons_held >> QD_B_OCTDN) & 1u) && (a->ms - u->mfx_f1_ms >= QD_TAP_MS || u->mfx_f1_copied);
        v->f1_src = u->mfx_f1_copied ? u->mfx_f1_src : -1;
        v->f2 = (int)((a->buttons_held >> QD_B_OCTUP) & 1u);
        v->f2_hand = u->mfx_f2_holding ? u->mfx_clip.type : -1;
        mfx_fstep(a, v, 0);
        if (u->mfx_roll_ms && a->ms - u->mfx_roll_ms < QD_TK_MS) {   /* PRESETS: the rolodex (P1.6d: gliding) */
            v->roll = 1;
            v->roll_pos = (float)qd_mfx_glide(a, u->mfx_roll_from, u->mfx_roll_to, u->mfx_roll_t0) / 100.f;
        }
        for (k = 0; k < 4 && s->type; k++) {           /* the foot */
            const int id = qd_mfx_page_id(s->type, v->page, k);
            if (id == -1)
                continue;
            v->has[k] = 1;
            v->lab[k] = qd_mfx_knob_name(a, k, v->val[k]);
        }
        if (u->mfx_knob >= 0 && a->ms - u->mfx_ms < 2000u)
            v->last = u->mfx_knob;
        if (s->type == QD_MFX_ARP) {                    /* the arp: its held notes, its walk, where it is */
            static const qd_mfx_srt_t none;             /* (P2.1b: a slot with no state: nothing held) */
            const qd_mfx_srt_t *r = qd_mfx_srt(&a->player.mfx, g - 1, v->sel) ? qd_mfx_srt(&a->player.mfx, g - 1, v->sel) : &none;
            const int range = s->v[QD_MA_RANGE] < 1 ? 1 : s->v[QD_MA_RANGE] > 4 ? 4 : s->v[QD_MA_RANGE];
            const int pat = s->v[QD_MA_PAT], mlen = s->v[QD_MA_MLEN], tlen = s->v[QD_MA_TLEN];
            static const uint8_t DEF[3] = {60, 64, 67};
            uint8_t notes[QD_MFX_NHOLD], asp[QD_MFX_NHOLD], idx[QD_MFX_NHOLD * 4];
            int n = 0, i, o, ni, nh = 0;
            uint32_t key = 2166136261u;
            while (n < QD_MFX_NHOLD && r->h[n].used)
                n++;
            v->lane_sel = u->mfx_lane[v->page == 3 ? 1 : 0] & 15;
            if ((R->hd[v->sel].type & 0x7F) == QD_MFX_ARP && n) {
                nh = n;
                v->nheld = n;
                for (i = 0; i < n; i++) {
                    v->held_p[i] = notes[i] = r->h[r->order[i]].n.pitch;
                    v->held_t[i] = (uint8_t)(r->h[r->order[i]].n.track + 1);
                }
                for (i = 0; i < n; i++) {               /* (as played: the arrivals, by their place in pitch) */
                    int j;
                    for (j = 0; j < n && r->order[j] != i; j++)
                        ;
                    asp[i] = (uint8_t)(j < n ? j : 0);
                }
                v->arp_run = r->run;
                v->arp_step = r->oct * r->nseq + (r->pos > 0 ? r->pos - 1 : 0);
                if (r->run)
                    v->lane_cur = v->page == 3 ? (r->tpos + s->v[QD_MA_TLEN] - 1) % (s->v[QD_MA_TLEN] < 1 ? 1 : s->v[QD_MA_TLEN])
                                               : (r->mpos + s->v[QD_MA_MLEN] - 1) % (s->v[QD_MA_MLEN] < 1 ? 1 : s->v[QD_MA_MLEN]);
            } else {                                    /* (nothing held: the pattern on C E G) */
                nh = 3;
                memcpy(notes, DEF, 3);
                asp[0] = 0, asp[1] = 1, asp[2] = 2;
            }
            /* P1.6g: the PATTERN's walk, as the arp makes it (its random ones scattered, changing) */
            ni = qd_mfx_arp_order(pat, nh, idx);
            if (pat == QD_AP_ASP) {
                memcpy(idx, asp, (size_t)nh);
                ni = nh;
            } else if (pat == QD_AP_RAND || pat == QD_AP_ROTH || pat == QD_AP_RONC) {
                uint32_t x = (a->ms / 700u) * 2654435761u + 12345u;
                ni = pat == QD_AP_RONC ? nh : nh * 2 < 6 ? 6 : nh * 2;
                for (i = 0; i < ni; i++) {
                    x = x * 1664525u + 1013904223u;
                    idx[i] = (uint8_t)((x >> 16) % (uint32_t)nh);
                    if (pat == QD_AP_ROTH && i && idx[i] == idx[i - 1] && nh > 1)
                        idx[i] = (uint8_t)((idx[i] + 1) % nh);
                }
                if (pat == QD_AP_RONC) {                /* (every note once, shuffled) */
                    for (i = 0; i < nh; i++)
                        idx[i] = (uint8_t)i;
                    for (i = nh - 1; i > 0; i--) {
                        const int j = (int)((x >> (i & 7)) % (uint32_t)(i + 1));
                        const uint8_t tt = idx[i];
                        idx[i] = idx[j], idx[j] = tt;
                    }
                }
            }
            for (o = 0; o < range && v->nseq < 48; o++)
                for (i = 0; i < ni && v->nseq < 48; i++) {
                    const int j = v->nseq;
                    int p = notes[idx[i] < nh ? idx[i] : 0] + o * s->v[QD_MA_ODIST];
                    if (mlen > 0 && s->lane[0][j % mlen] == QD_AL_REST)
                        v->gap[j] = 1;                  /* (the MOD lane's rests: gaps) */
                    if (tlen > 0)
                        p += s->lane[1][j % tlen];      /* (the TRANSPOSE lane: offsets) */
                    v->seq[v->nseq++] = (uint8_t)(p < 0 ? 0 : p > 127 ? 127 : p);
                }
            for (i = 0; i < v->nseq; i++)
                key = (key ^ (uint32_t)(v->seq[i] | v->gap[i] << 7)) * 16777619u;
            if (key != u->mfx_walk_key) {               /* changed: the old walk kept for the morph */
                memcpy(u->mfx_walk_prev, u->mfx_walk, sizeof u->mfx_walk);
                u->mfx_walk_pn = u->mfx_walk_n;
                memcpy(u->mfx_walk, v->seq, sizeof u->mfx_walk);
                u->mfx_walk_n = v->nseq;
                u->mfx_walk_key = key;
                u->mfx_walk_ms = a->ms ? a->ms : 1u;
            }
            v->morph = u->mfx_walk_pn && a->ms - u->mfx_walk_ms < 180u ? (float)(a->ms - u->mfx_walk_ms) / 180.f : 1.f;
            v->nprev = u->mfx_walk_pn;
            memcpy(v->prev, u->mfx_walk_prev, sizeof v->prev);
        }
        if (s->type == QD_MFX_CHORD) {                  /* CHORD: the chord on A, named */
            const int sh = s->v[QD_MK_SHAPE] > 0 && s->v[QD_MK_SHAPE] < QD_NSHAPE ? s->v[QD_MK_SHAPE] : 1;
            snprintf(v->chord, sizeof v->chord, "A%s", QD_SHAPE[sh].name);
        }
    }
}

/* P1.3y: the settings menus' model */
static void fill_menu(const qd_app_t *a, ui_menu_t *v)
{
    const qd_ui_t *u = &a->ui;
    int r, k;
    memset(v, 0, sizeof *v);
    v->kind = u->latch == QD_LATCH_REC ? UI_MENU_REC : u->latch == QD_LATCH_TRK ? UI_MENU_TRK : UI_MENU_SET;
    v->rec_step = a->proj.rec_mode;
    v->quant = a->proj.rec_quant;
    v->dub = a->proj.rec_dub;
    v->ekeys = a->proj.rec_ekeys;
    v->rec_page = u->rec_page;
    v->rec_auto = a->proj.rec_auto;
    v->layout = u->knobs;
    for (r = 0; r < 2; r++)
        for (k = 0; k < 3; k++)
            v->role[r][k] = qd_ctl_role_name(qd_ctl_layout_role(u->knobs, r, QD_K_SELECT + k));
    v->follow = u->follow;
    v->t = secs(a->ms);
    if (v->kind == UI_MENU_TRK) {                      /* P1.4b: the track settings of the selected track */
        const qd_tset_t *ts = &qd_ctl_cpat(a)->trk[a->sel];
        int sc, rt, i;
        qd_track_key(&a->proj, qd_ctl_cpat(a), a->sel, &sc, &rt);
        v->track = a->sel + 1;
        v->page = u->trk_page;
        v->tname = a->model.t[a->sel].name;
        v->tkind = a->model.t[a->sel].kind < 7 ? a->model.t[a->sel].kind : -1;   /* P1.6d */
        v->named = a->model.t[a->sel].named;
        v->scale = qd_scale_name(sc);
        v->root = qd_pc_name(rt);
        v->root_pc = rt;
        for (i = 0; i < 12; i++)
            v->scale_mask |= qd_scale_has(sc, rt, 60 + i) ? 1 << ((60 + i) % 12) : 0;
        v->follow_key = ts->key_global;
        v->chan = ts->chan + 1;
        v->limit = ts->limit;
        v->swing = ts->swing;
        v->mfx = ts->mfx <= QD_MFX_NGROUP ? ts->mfx : 0;
        for (i = 0; i < a->pool.nvoice; i++)
            v->voices_now += qd_pool_sounding(&a->pool, i) && a->pool.v[i].track == a->sel;
        v->last = u->menu_knob;
    }
}

/* P1.4b: the mix's model: the eight tracks' mix, heard or not, their voices' level now */
static void fill_mix(const qd_app_t *a, ui_mix_t *v)
{
    int t, i;
    memset(v, 0, sizeof *v);
    v->sel = a->sel;
    v->page = a->ui.mix_page;
    v->last = a->ui.menu_knob;
    for (t = 0; t < QD_NTRACK; t++) {
        const qd_mix_track_t *m = &a->model.t[t].mix;
        int32_t lv = 0;
        v->tr[t].name = a->model.t[t].name;
        v->tr[t].level = m->level;
        v->tr[t].pan = m->pan;
        v->tr[t].chorus = m->chorus;
        v->tr[t].delay = m->delay;
        v->tr[t].reverb = m->reverb;
        v->tr[t].mute = !qd_seq_audible(&a->seq, qd_seq_track_pat(&a->seq, t), t);
        for (i = 0; i < a->pool.nvoice; i++)
            if (qd_pool_sounding(&a->pool, i) && a->pool.v[i].track == t) {
                const int32_t l = qd_quad_voice_level(&a->pool.v[i].q);
                lv = l > lv ? l : lv;
            }
        v->tr[t].meter = sqrtf((float)lv / 32767.f) * (float)m->level / 127.f;
    }
    {   /* P1.6d: a step held (or the latch's selection): the selected track's values its locks; the chips */
        const qd_ui_t *u = &a->ui;
        const qd_pattern_t *p = qd_ctl_cpat(a);
        int k;
        v->step = u->nheld > 0 || u->wk_pending;        /* (P1.6h: from the key's press, before its first detent) */
        v->nsel = u->nheld;
        for (k = 0; k < 4; k++) {
            const int id = qd_panel_lock_id(a, QD_K_1 + k);
            int16_t x;
            if (id < 0)
                continue;
            if (u->nheld && qd_lock_get(p, a->sel, u->held[0], id, &x)) {
                v->locked[k] = 1;
                if (id == QD_LK_LEVEL) v->tr[a->sel].level = x;
                else if (id == QD_LK_PAN) v->tr[a->sel].pan = x;
                else if (id == QD_LK_DELAY) v->tr[a->sel].delay = x;
                else if (id == QD_LK_REVERB) v->tr[a->sel].reverb = x;
                else v->tr[a->sel].chorus = x;
                snprintf(v->ltxt[k], sizeof v->ltxt[k], "%d", x);
            } else if (!u->nheld && track_locks(p, a->sel, id)) {
                v->lockany[k] = 1;
                snprintf(v->ltxt[k], sizeof v->ltxt[k], "%d", id == QD_LK_LEVEL ? v->tr[a->sel].level
                         : id == QD_LK_PAN ? v->tr[a->sel].pan : id == QD_LK_DELAY ? v->tr[a->sel].delay
                         : id == QD_LK_REVERB ? v->tr[a->sel].reverb : v->tr[a->sel].chorus);
            }
        }
    }
}

/* P1.5a: the project pages' model */
static void fill_prj(const qd_app_t *a, ui_prj_t *v)
{
    const qd_prjui_t *p = &a->ui.pj;
    const qd_persist_t *ps = &a->per;
    uint32_t live, total;
    int i, n = 0;
    memset(v, 0, sizeof *v);
    v->page = p->page;
    v->num = ps->proj + 1;
    v->name = ps->pname[ps->proj][0] ? ps->pname[ps->proj] : "PROJECT 1";
    for (i = 0; i < QD_NPAT; i++) {
        v->used[i] = qd_proj_used(&a->proj, i);
        v->since[i] = ps->written_ms[i] ? (float)(a->ms - ps->written_ms[i]) / 1000.f : -1.f;
    }
    v->cur = a->seq.cur;
    qd_persist_use(a, &live, &total);
    v->flash = total ? (float)live / (float)total : 0.f;
    v->flash_own = total ? (float)qd_persist_proj_pages(a, ps->proj) / (float)total : 0.f;
    for (i = 1; i <= QD_NSOUND; i++)
        n += qd_sound_used(&a->proj, i);
    v->pool_n = n;
    for (i = 0; i < QD_NPROJ; i++) {
        v->p[i].exists = ps->pexists[i] || (i == ps->proj);
        v->p[i].name = i == ps->proj ? v->name : ps->pname[i];
        qd_persist_proj_used(a, i, v->p[i].used);
    }
    v->chosen = p->chosen;
    v->src = p->f1_down ? p->src : -1;
    v->armed = p->armed;
    v->t = secs(a->ms);
}

/* P1.4b: the effects' model: the page's cells (their values as ChoralRoot reads them), what the drawing needs */
static void fill_fx(const qd_app_t *a, ui_fx_t *v)
{
    static const char *const LAB[QD_NFXP] = {"TIME", "TIME", "SPREAD", "SPREAD", "FDBK", "SYNC", "MODE", "TONE", "WOW",
                                             "SIZE", "DAMP", "DECAY", "PRE", "MOD", "LOW CUT", "WIDTH", "WIDTH",
                                             "LOW", "HIGH", "SLOPE", "FILTER", "RATE", "DEPTH",
                                             "DLY>REV", "CHO>DLY", "CHO>REV", "FILTER"};   /* (P1.5b: the routing) */
    const qd_ui_t *u = &a->ui;
    const qd_fxpage_t *pg = &QD_FX_PAGES[u->fx_page];
    int k, i;
    memset(v, 0, sizeof *v);
    v->name = pg->name;
    v->draw = pg->draw;
    for (i = 0; i < QD_FX_NPAGE; i++)
        if (QD_FX_PAGES[i].fx == pg->fx) {
            if (i == u->fx_page)
                v->ipg = v->npg;
            v->npg++;
        }
    for (k = 0; k < 4; k++) {
        const int c = pg->c[k];
        if (c == QD_FXC_NONE)
            continue;
        v->has[k] = 1;
        if (c == QD_FXC_SEND || c <= QD_FXC_SENDX(0)) {   /* (P1.5b: the overview's cells: their effect's) */
            const int fx = c == QD_FXC_SEND ? pg->fx : QD_FXC_SENDX(0) - c;
            int s = qd_fx_send(a, a->sel, fx);
            int16_t x;
            if (u->nheld && qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[0], qd_fx_send_id(fx), &x))
                s = x, v->locked[k] = 1;                /* P1.6d: the held step's lock */
            else if (!u->nheld && track_locks(qd_ctl_cpat(a), a->sel, qd_fx_send_id(fx)))
                v->lockany[k] = 1;
            v->lab[k] = c == QD_FXC_SEND ? "SEND" : QD_FX_NAME[fx];
            snprintf(v->txt[k], sizeof v->txt[k], "%d%%", (s * 100 + 63) / 127);
            v->x[k] = (float)s / 127.f;
            v->send = v->x[k];
            v->track = a->sel + 1;
        } else {
            const int id = qd_fx_cell_id(c);
            int lo, hi;
            qd_fx_range(id, &lo, &hi, 0);
            v->lab[k] = LAB[id];
            qd_fx_text(id, qd_fx_get(id), v->txt[k]);
            v->x[k] = hi > lo ? (float)(qd_fx_get(id) - lo) / (float)(hi - lo) : 0.f;
        }
    }
    v->t = secs(a->ms);
    if (pg->fx == QD_FXG_OVER) {                        /* P1.5b: every track's sends */
        int t;
        for (t = 0; t < QD_NTRACK; t++) {
            v->sends[t][0] = qd_fx_send(a, t, 2);
            v->sends[t][1] = qd_fx_send(a, t, 0);
            v->sends[t][2] = qd_fx_send(a, t, 1);
            if (!qd_seq_audible(&a->seq, qd_seq_track_pat(&a->seq, t), t))
                v->mute |= (uint8_t)(1u << t);
        }
        for (k = 0; k < 3; k++) {                       /* P1.6d: the selected track's bars: the held step's locks */
            int16_t x;
            if (u->nheld && qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[0], qd_fx_send_id(k == 0 ? 2 : k - 1), &x))
                v->sends[a->sel][k] = x;
        }
        v->track = a->sel + 1;
    }
    for (i = 0; i < 4; i++)                             /* P1.5b: the routing */
        v->rt[i] = i == 3 ? (float)qd_fx_get(QD_FXP_XPOS) : (float)qd_fx_get(QD_FXP_XDR + i) / 127.f;
    v->delay_s = (float)qd_fx_delay_ms() / 1000.f;
    v->chorus_hz = (float)qd_fx_chorus_hz100() / 100.f;
    v->sync = qd_fx_get(QD_FXP_DSYNC);
    v->ping = qd_fx_get(QD_FXP_DMODE);
    v->fdbk = (float)qd_fx_get(QD_FXP_DFDBK) / 127.f;
    v->spread = qd_fx_get(QD_FXP_DSYNC) ? (float)qd_fx_get(QD_FXP_DRATIO) / 3.f : (float)qd_fx_get(QD_FXP_DSPREAD) / 127.f;
    v->tone = (float)qd_fx_get(QD_FXP_DTONE) / 127.f;
    v->wow = (float)qd_fx_get(QD_FXP_DWOW) / 127.f;
    v->size = (float)qd_fx_get(QD_FXP_RSIZE) / 127.f;
    v->damp = (float)qd_fx_get(QD_FXP_RDAMP) / 127.f;
    v->decay = (float)qd_fx_get(QD_FXP_RDECAY) / 127.f;
    v->pre = (float)qd_fx_get(QD_FXP_RPRE) / 127.f;
    v->mod = (float)qd_fx_get(QD_FXP_RMOD) / 127.f;
    v->low = (float)qd_fx_get(QD_FXP_RLOW) / 127.f;
    v->width = (float)qd_fx_get(QD_FXP_RWIDTH) / 127.f;
    v->depth = (float)qd_fx_get(QD_FXP_CDEPTH) / 127.f;
    v->cwidth = (float)qd_fx_get(QD_FXP_CWIDTH) / 127.f;
    v->flow = (float)qd_fx_get(QD_FXP_FLOW) / 127.f;
    v->fhigh = (float)qd_fx_get(QD_FXP_FHIGH) / 127.f;
    v->fslope = qd_fx_get(QD_FXP_FSLOPE);
    v->fon = qd_fx_get(QD_FXP_FON);
    v->last = u->menu_knob;
    v->step = u->nheld > 0 || u->wk_pending;            /* P1.6d (P1.6h: from the key's press) */
    v->nsel = u->nheld;
}

/* P1.5z: the models, for the tests */
void qd_view_browse(const qd_app_t *a, ui_browse_t *v) { fill_browse(a, v); }
void qd_view_mix(const qd_app_t *a, ui_mix_t *v) { fill_mix(a, v); }
void qd_view_fx(const qd_app_t *a, ui_fx_t *v) { fill_fx(a, v); }
void qd_view_layer(qd_app_t *a, ui_layer_t *v) { fill_layer(a, v, qd_ctl_layer_shown(a)); }
void qd_view_menu(const qd_app_t *a, ui_menu_t *v) { fill_menu(a, v); }
void qd_view_type(const qd_app_t *a, ui_type_t *v) { fill_type(a, v); }
void qd_view_turn(qd_app_t *a, ui_turn_t *v)
{
    static char ctx[48];
    fill_turn(a, v, ctx);
}
void qd_view_shift(const qd_app_t *a, ui_shift_t *v) { fill_shift(a, v); }

int qd_view_overlay(const qd_app_t *a)
{
    /* (P1.5n: the octave / bank takeover over EDIT too: an OCT tap's octave was not shown there at all, only F2's map
     * for a moment) */
    return a->ui.tk != QD_TK_NONE && (qd_view_screen(a) != QD_SCR_EDIT || a->ui.tk == UI_TK_OCTAVE ||
                                      a->ui.tk == UI_TK_SCALE || a->ui.tk == UI_TK_ROOT ||   /* (P1.5z: SHIFT's) */
                                      a->ui.tk == UI_TK_TAP || a->ui.tk == UI_TK_FOLLOW || a->ui.tk == UI_TK_TRACKBIG ||
                                      a->ui.tk == QD_TK_UNDO || a->ui.tk == QD_TK_IMPORT || a->ui.tk == QD_TK_LOADED ||
                                      a->ui.tk == QD_TK_BACK || a->ui.tk == QD_TK_SAVED ||
                                      a->ui.tk == QD_TK_MFX_ARP || a->ui.tk == QD_TK_MFX_STEP ||   /* (P1.6c) */
                                      (a->ui.tk == QD_TK_PRESET && a->ui.tk_lock));   /* (P1.5t: the step's pool
                                                         * picker takes the screen in EDIT too) */
}

/* P1.5p: the knob band shows (a held step's knob turned): over the head, y 0..41, its fade included */
static int band_up(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    return u->tk == QD_TK_HELDKNOB && u->nheld && !u->lmode_open && qd_view_overlay(a) &&
           a->ms - u->tk_ms < QD_TK_MS + QD_TK_FADE_MS;
}

static void view_draw(qd_app_t *a, ui_lane_t *lvp, ui_held_t *hvp, ui_turn_t *tvp, ui_layer_t *yvp, ui_shift_t *svp,
                      ui_band_t *bvp, ui_menu_t *mvp, ui_snd_t *sv5p, ui_mix_t *xvp, ui_browse_t *brvp, ui_type_t *tyvp,
                      ui_prj_t *pjvp, ui_fx_t *fvp, ui_mfx_t *mfvp);
extern int qd_view_strip_check;
extern uint32_t qd_view_strip_frames, qd_view_strip_bad, qd_view_strip_px;

typedef void (*qd_strip_fn)(void *ctx, int y0, int rows, const uint16_t *px);
static void view_frame(qd_app_t *a, uint16_t *lcd_be, uint16_t *rows_be, qd_strip_fn out, void *ctx, uint32_t mask);
void qd_view_frame(qd_app_t *a, uint16_t *lcd_be) { view_frame(a, lcd_be, 0, 0, 0, 0xFFFFFFFFu); }
/* P2.1: the frame strip by strip into one strip's rows, out after each (the device's LCD: no whole frame in RAM);
 * P2.1b: rows_be 0: each strip in place, in the core's own strip surface. P2.6: only the strips in mask (bit k: rows
 * k x QD_STRIP_H ..), all of them from one instant (the frame recorded once: qd_gfx_rec.c) */
void qd_view_frame_strips(qd_app_t *a, uint16_t *rows_be, qd_strip_fn out, void *ctx, uint32_t mask)
{
    view_frame(a, 0, rows_be, out, ctx, mask);
}

static int vf_hash, vf_rec;                             /* (P2.4: the frame's calls hashed, nothing drawn; P2.6: the
                                                         * frame being recorded) */
/* P2.6: the frame recorded now (its instant: every strip drawn later is this one), its strips' hashes into sh (n of
 * them): 1; no list (no room, or not the device's fast drawing): 0, the frame's hash (P2.4's way) in sh[0] */
int qd_view_frame_hashes(qd_app_t *a, uint32_t *sh, int n)
{
    vf_rec = 1;
    view_frame(a, 0, 0, 0, 0, 0);
    vf_rec = 0;
    if (qd_dl_kept()) {
        qd_dl_strip_hashes(sh, n, qd_strip_rows());
        return 1;
    }
    qd_hash_begin();
    vf_hash = 1;
    view_frame(a, 0, 0, 0, 0, 0);
    vf_hash = 0;
    sh[0] = qd_hash_end();
    return 0;
}
uint32_t qd_view_frame_hash(qd_app_t *a)
{
    qd_hash_begin();
    vf_hash = 1;
    view_frame(a, 0, 0, 0, 0, 0);
    vf_hash = 0;
    return qd_hash_end();
}

static void view_frame(qd_app_t *a, uint16_t *lcd_be, uint16_t *rows_be, qd_strip_fn out, void *ctx, uint32_t mask)
{
    static ui_lane_t lv;
    static ui_held_t hv;
    static ui_turn_t tv;
    static ui_layer_t yv;
    static ui_shift_t sv;
    static ui_band_t bv;
    static ui_menu_t mv;
    static ui_snd_t sv5;
    static ui_mix_t xv;
    static ui_browse_t brv;
    static ui_type_t tyv;
    static ui_prj_t pjv;
    static ui_fx_t fv;
    static ui_mfx_t mfv;
    const qd_ui_t *u = &a->ui;
    int kt, y0;
    for (kt = 0; kt < QD_NTRACK; kt++)                  /* P1.6d: the tracks' kinds (their icons) */
        UI_TRACK_KIND[kt] = a->model.t[kt].kind;
    if (vf_hash) {                                      /* (P2.4: the hash pass: the screen's calls once, the whole
                                                         * frame; nothing built ahead, nothing drawn) */
        view_draw(a, &lv, &hv, &tv, &yv, &sv, &bv, &mv, &sv5, &xv, &brv, &tyv, &pjv, &fv, &mfv);
        return;
    }
    if (qd_ctl_shift(a) && u->tk != UI_TK_TAP && !qd_type_open(a))
        ui_ape_prewarm(qd_seq_ext_running(&a->seq));                               /* 0.9.2.1: SHIFT held (B6 may come): the TAP ape's first poses */
    if (!out && !vf_rec)
        qd_dl_drop();                                   /* (a whole frame (qd_app_frame): its own instant, now; a list
                                                         * kept is for the strips' call after qd_app_frame_strips_changed) */
    if ((vf_rec || !qd_dl_kept()) && qd_strip_rows() < QD_H && qd_dl_begin()) {   /* P2.6: ONE INSTANT A FRAME: its
                                                         * drawing calls recorded once (a list kept from
                                                         * qd_view_frame_hashes: that instant), every strip from them */
        view_draw(a, &lv, &hv, &tv, &yv, &sv, &bv, &mv, &sv5, &xv, &brv, &tyv, &pjv, &fv, &mfv);
        qd_dl_end();
    }
    if (vf_rec)
        return;
    for (y0 = 0; y0 < QD_H; y0 += qd_strip_rows()) {    /* P2.1: the screen in strips (the host: one, the whole) */
        if (!((mask >> (y0 / qd_strip_rows())) & 1u))
            continue;                                   /* (P2.6: the caller keeps this strip as it is) */
        qd_begin_strip(y0);
        if (qd_dl_kept())
            qd_dl_play();
        else                                            /* (no list: drawn live, as before) */
            view_draw(a, &lv, &hv, &tv, &yv, &sv, &bv, &mv, &sv5, &xv, &brv, &tyv, &pjv, &fv, &mfv);
        if (out) {
            const int rows = QD_H - y0 < qd_strip_rows() ? QD_H - y0 : qd_strip_rows();
            const uint16_t *px = qd_end_strip_rows(rows_be);
            out(ctx, y0, rows, px);
        } else {
            qd_end_strip(lcd_be);
        }
    }
    qd_dl_drop();
#ifndef QD_TARGET_FM1                                   /* (the host's test only: not on the unit) */
    if (qd_view_strip_check && lcd_be) {                /* (render_frames --strips: the same frame in the device's strips) */
        static uint16_t lcd[QD_W * QD_H];
        const int rows = qd_strip_rows();
        uint32_t i, n = 0;
        qd_set_strip_rows(QD_STRIP_DEV);
        for (y0 = 0; y0 < QD_H; y0 += qd_strip_rows()) {
            qd_begin_strip(y0);
            view_draw(a, &lv, &hv, &tv, &yv, &sv, &bv, &mv, &sv5, &xv, &brv, &tyv, &pjv, &fv, &mfv);
            qd_end_strip(lcd);
        }
        qd_set_strip_rows(rows);
        for (i = 0; i < QD_W * QD_H; i++)
            n += lcd[i] != lcd_be[i];
        qd_view_strip_frames++;
        qd_view_strip_bad += n != 0;
        qd_view_strip_px += n;
    }
#endif
}

/* the frame's drawing, as many times as it has strips: the same each time (the state's, not the clock's) */
static void view_draw(qd_app_t *a, ui_lane_t *lvp, ui_held_t *hvp, ui_turn_t *tvp, ui_layer_t *yvp, ui_shift_t *svp,
                      ui_band_t *bvp, ui_menu_t *mvp, ui_snd_t *sv5p, ui_mix_t *xvp, ui_browse_t *brvp, ui_type_t *tyvp,
                      ui_prj_t *pjvp, ui_fx_t *fvp, ui_mfx_t *mfvp)
{
#define lv (*lvp)
#define hv (*hvp)
#define tv (*tvp)
#define yv (*yvp)
#define sv (*svp)
#define bv (*bvp)
#define mv (*mvp)
#define sv5 (*sv5p)
#define xv (*xvp)
#define brv (*brvp)
#define tyv (*tyvp)
#define pjv (*pjvp)
#define fv (*fvp)
#define mfv (*mfvp)
    const qd_ui_t *u = &a->ui;
    char pat[4], ctx[24];
    a->geek.scope_on = 0;                               /* (P2.11: the scope's samples only while it shows) */
    switch (qd_view_screen(a)) {
    case QD_SCR_MENU:                                   /* P1.3y: a settings menu */
        fill_menu(a, &mv);
        ui_menu_draw(&mv);
        break;
    case QD_SCR_BROWSE:                                 /* P1.5t: the library browser */
        fill_browse(a, &brv);
        ui_browse_draw(&brv);
        break;
    case QD_SCR_SAVE:                                   /* P1.5t: SAVE PRESET (P1.5u: the typewriter) */
    case QD_SCR_NAME:                                   /* .. a project's name (B10 on the project page) */
        fill_type(a, &tyv);
        ui_type_draw(&tyv);
        break;
    case QD_SCR_PRJ:
        fill_prj(a, &pjv);
        ui_prj_draw(&pjv);
        break;
    case QD_SCR_MIX:                                    /* P1.4b: the mix */
        fill_mix(a, &xv);
        ui_mix_draw(&xv);
        break;
    case QD_SCR_FX:                                     /* P1.4b: the effects */
        fill_fx(a, &fv);
        ui_fx_draw(&fv);
        break;
    case QD_SCR_SOUND:                                  /* P1.4a: the sound editor */
        qd_view_snd(a, &sv5);
        ui_snd_draw(&sv5);
        break;
    case QD_SCR_MFX:                                    /* P1.6c: the MIDI FX screen */
        qd_view_mfx(a, &mfv);
        ui_mfx_draw(&mfv);
        break;
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
    case QD_SCR_HELD:
        fill_held(a, &hv, 0);
        hv.band = band_up(a);                           /* (P1.5p: the band over the head: its zone left clear) */
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
    case QD_SCR_EDIT:                                   /* P1.3r: the page's row under the mode's picture */
        qd_view_edit(a, &hv, &lv);
        ui_edit_draw(&hv, &lv);
        break;
    case QD_SCR_GEEK: {                                 /* P2.11: a Geek Out page */
        static ui_geek_t gv;
        qd_view_geek(a, &gv);
        ui_geek_draw(&gv);
        break;
    }
    default:
        draw_idle(a);
        break;
    }
    if (u->lmode_open) {                                /* P1.5h: LFO held: the latch mode's screen, while held */
        ui_lmode_draw(u->lmode, secs(a->ms));
    } else if (u->tk == QD_TK_LMODE) {                  /* P1.5b: the latch mode switched (over every screen) */
        const uint32_t d = a->ms - u->tk_ms;
        const float al = d < QD_TK_MS ? 1.f : 1.f - (float)(d - QD_TK_MS) / (float)QD_TK_FADE_MS;
        if (al > 0.f) {
            overlay_begin(al);
            ui_lmode_draw(u->lmode, secs(a->ms));
            qd_restore();
        }
    } else if (!qd_view_overlay(a)) {
        /* (none; P1.3r: EDIT's top is the pop-up: no takeover band, picture or picker over it) */
    } else if (u->tk == QD_TK_HELDKNOB) {                      /* P1.3d: a held step's knob: the band over its screen */
        const uint32_t d = a->ms - u->tk_ms;
        const float al = d < QD_TK_MS ? 1.f : 1.f - (float)(d - QD_TK_MS) / (float)QD_TK_FADE_MS;
        if (band_up(a) && al > 0.f) {
            qd_save();
            qd_alpha(al);
            fill_band(a, &bv, 0);
            ui_band_draw(&bv);
            qd_restore();
        }
    } else if (u->tk != QD_TK_NONE) {                   /* the takeover, then 120 ms back */
        const uint32_t d = a->ms - u->tk_ms;
        const uint32_t tms = u->tk == UI_TK_TAP ? QD_TAP_TK_MS : QD_TK_MS;   /* (P1.5z: TAP's 2 s) */
        const float al = d < tms || u->tk_hold ? 1.f : 1.f - (float)(d - tms) / (float)QD_TK_FADE_MS;   /* (P1.5v:
                                                         * the step's pool picker: while held) */
        if (al > 0.f) {
            overlay_begin(al);
            if (u->tk == QD_TK_LOADED || u->tk == QD_TK_BACK || u->tk == QD_TK_SAVED) {
                draw_pfx(a);                            /* P1.5t: LOADED, BACK, SAVED */
            } else if (u->tk == QD_TK_IMPORT) {         /* P1.5r: a sound imported (SysEx) */
                ui_import_draw(u->imp_name, u->imp_pool, u->imp_slot, secs(a->ms - u->tk_ms));
            } else if (u->tk == QD_TK_MFX_ARP) {        /* P1.6c: ARP tapped: the arp ON / OFF */
                uint8_t ty[8];
                int k;
                for (k = 0; k < 8; k++)
                    ty[k] = u->tk_g ? a->proj.mfx[u->tk_g - 1].slot[k].type : 0;
                ui_mfx_arp_draw(u->tk_g, u->tk_on, ty, u->tk_new, a->sel + 1, secs(a->ms - u->tk_ms), secs(a->ms));
            } else if (u->tk == QD_TK_MFX_STEP) {       /* P1.6c: a held step's MIDI FX */
                static const char *const TRIG[4] = {"NOTE", "NOTE", "LOCK", "TIE"};
                uint8_t ty[8];
                const int s = u->nheld ? u->held[0] : u->tk_step >= 0 ? u->tk_step : 0;
                const int m = qd_ctl_cpat(a)->step[a->sel][s].mfx, g = qd_step_mfx(qd_ctl_cpat(a), a->sel, s);
                int k;
                qd_rstep_t r;
                qd_step_resolve(qd_ctl_cpat(a), a->sel, s, &r);
                for (k = 0; k < 8; k++)
                    ty[k] = g ? a->proj.mfx[g - 1].slot[k].type : 0;
                ui_mfx_step_draw(s + 1, m, g, ty, TRIG[r.type & 3], secs(a->ms));
            } else if (u->tk == QD_TK_UNDO) {           /* P1.5c: UNDO / REDO, what it was */
                ui_undo_draw(u->undo_dir > 0, u->undo_kind, secs(a->ms));
            } else if (u->tk == QD_TK_PRESET) {
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
    if (qd_auto_on(a) || (u->rec && a->seq.playing && !u->live_wait && !qd_rec_stepping(a)))   /* P1.6d: automation
                                                         * recording: the cue (P1.6g: a live recording too) */
        ui_auto_cue(u->auto_take ? u->auto_knob - QD_K_1 : -1,
                    u->auto_take ? (float)(a->ms - u->auto_ms) / 1000.f : -1.f, secs(a->ms));
    if (u->cf_on) {                                     /* a confirmation word */
        const uint32_t d = a->ms - u->cf_ms;
        const float al = d + QD_TK_FADE_MS < QD_CONFIRM_MS ? 1.f
                                                           : (float)(QD_CONFIRM_MS - d) / (float)QD_TK_FADE_MS;
        if (al > 0.f) {
            overlay_begin(al);
            ui_confirm_draw(u->cf_big, u->cf_small, u->cf_col ? u->cf_col : UI_K3);
            qd_restore();
        }
    }
#undef lv
#undef hv
#undef tv
#undef yv
#undef sv
#undef bv
#undef mv
#undef sv5
#undef xv
#undef brv
#undef tyv
#undef pjv
#undef fv
#undef mfv
}

/* P2.1: the strip test (render_frames --strips): the frame drawn again in the device's strips must be the same, pixel for
 * pixel (frames and pixels that differ counted) */
int qd_view_strip_check;
uint32_t qd_view_strip_frames, qd_view_strip_bad, qd_view_strip_px;

#include "view_labels.c"                /* P1.6l: the controls' labels (built here: the source lists stay) */

/* ================================================================ P2.11: the Geek Out pages' models === */
#define QD_GEEK_PEAK_MS 2000u                           /* a meter's peak tick held this long */
#define QD_GEEK_NUM_MS 250u                             /* the numbers step four times a second (readable) */
static float gk_chase(float shown, float v, uint32_t dt)   /* a bar chasing its value (ChoralRoot's: a quarter of the
                                                         * way every 25 ms) */
{
    const float k = dt >= 200u ? 1.f : 1.f - powf(0.75f, (float)dt / 25.f), n = shown + (v - shown) * k;
    return fabsf(v - n) < 0.002f ? v : n;               /* (there: the same frame again, its strips kept) */
}
static float gk_flash(uint32_t since_ms, float rate)
{
    return since_ms > 3000u ? 0.f : expf(-(float)since_ms / 1000.f * rate);
}
void qd_view_geek_reset(qd_app_t *a)
{
    qd_geek_t *g = &a->geek;
    int t;
    for (t = 0; t < 8; t++)
        g->steal0[t] = g->steal_seen[t] = a->pool.stolen[t], g->steal_ms[t] = 0;
    g->late_seen = 0, g->late_ms = 0;
    g->num_ms = 0;
    g->pk_ms[0] = g->pk_ms[1] = g->pk_ms[2] = g->pk_ms[3] = 0;
    a->stats.reset++;                                   /* (the platform zeroes its own) */
}
void qd_view_geek(qd_app_t *a, ui_geek_t *v)
{
    qd_geek_t *g = &a->geek;
    const qd_app_stats_t *st = &a->stats;
    const uint32_t now = a->ms, dt = now - g->ms;
    float val[4];
    int i, t, nsound = 0, steals = 0;
    int32_t tl[8] = {0};
    memset(v, 0, sizeof *v);
    v->page = a->ui.view - QD_VIEW_METERS;
    v->page = v->page < 0 ? 0 : v->page > 3 ? 3 : v->page;
    v->playing = a->seq.playing;
    v->beat = a->playing ? (float)a->beat_rem / (float)QD_BEAT_TICKS : 0.f;
    g->scope_on = v->page == 3;
    /* the platform's figures */
    v->load_on = !!(st->have & QD_ST_LOAD0), v->core1_on = !!(st->have & QD_ST_LOAD1);
    v->fps_on = !!(st->have & QD_ST_FPS), v->ram_on = !!(st->have & QD_ST_RAM), v->store_on = !!(st->have & QD_ST_STORE);
    v->isr_on = !!(st->have & QD_ST_ISR), v->late_on = v->isr_on, v->lock_on = !!(st->have & QD_ST_LOCK);
    v->frame_on = v->fps_on;
    val[0] = v->load_on ? (float)st->load_pm[0] / 1000.f : 0.f;
    val[1] = v->fps_on ? (float)st->fps10 / 10.f : 0.f;
    val[2] = v->ram_on && st->ram_size ? (float)st->ram_used / (float)st->ram_size : 0.f;
    val[3] = v->store_on && st->store_size ? (float)st->store_used / (float)st->store_size : 0.f;
    if (dt || !g->ms) {                                 /* (once an instant: a frame drawn strip by strip moves once) */
        for (i = 0; i < 4; i++) {
            const int low = i == 1;                     /* (the frame rate's tick: its lowest) */
            g->bar[i] = g->ms ? gk_chase(g->bar[i], val[i], dt) : val[i];
            if (!g->pk_ms[i] || now - g->pk_ms[i] >= QD_GEEK_PEAK_MS || (low ? val[i] <= g->pk[i] : val[i] >= g->pk[i]))
                g->pk[i] = val[i], g->pk_ms[i] = now ? now : 1u;
        }
    }
    v->load = g->bar[0], v->load_pk = g->pk[0];
    v->load1 = v->core1_on ? (float)st->load_pm[1] / 1000.f : 0.f;
    v->fps = g->bar[1] / 60.f, v->fps_min = g->pk[1] / 60.f;
    v->ram = g->bar[2], v->ram_pk = g->pk[2], v->store = g->bar[3];
    /* the voices: the pool's slots, each track's */
    v->nvoice = a->pool.nvoice > 16 ? 16 : a->pool.nvoice;
    for (i = 0; i < a->pool.nvoice; i++) {
        const qd_pool_voice_t *pv = &a->pool.v[i];
        float l;
        if (!qd_pool_sounding(&a->pool, i) || pv->track >= 8)
            continue;
        l = sqrtf((float)qd_quad_voice_level(&pv->q) / 32767.f);
        nsound++;
        if (i < 16)
            v->slot_col[i] = UI_TRACK_PAL[pv->track], v->slot_lvl[i] = l < 0.08f ? 0.08f : l;
        t = pv->track;
        tl[t] += qd_quad_voice_level(&pv->q);
        if (v->on[t] < 3)
            v->lvl[t][v->on[t]] = l < 0.08f ? 0.08f : l;
        v->on[t]++;
    }
    for (t = 0; t < 8; t++) {
        const uint32_t s = a->pool.stolen[t];
        if (s != g->steal_seen[t])
            g->steal_seen[t] = s, g->steal_ms[t] = now ? now : 1u;
        v->tsteals[t] = (int)(s - g->steal0[t]);
        v->tflash[t] = g->steal_ms[t] ? gk_flash(now - g->steal_ms[t], 2.6f) : 0.f;
        steals += v->tsteals[t];
        v->steal_flash = v->tflash[t] > v->steal_flash ? v->tflash[t] : v->steal_flash;
        v->lim[t] = a->pool.limit[t] < 3 ? a->pool.limit[t] : 3;
        if (dt || !g->ms)
            g->share[t] = gk_chase(g->share[t], nsound ? (float)v->on[t] / (float)nsound : 0.f, dt);
        v->share[t] = g->share[t];
    }
    v->steals = steals;
    if (st->late != g->late_seen)
        g->late_seen = st->late, g->late_ms = now ? now : 1u;
    v->late_flash = g->late_ms && st->late ? gk_flash(now - g->late_ms, 1.5f) : 0.f;
    /* the numbers: four a second */
    if (!g->num_ms || now - g->num_ms >= QD_GEEK_NUM_MS || now < g->num_ms) {
        g->num_ms = now ? now : 1u;
        g->num[0] = nsound;
        g->num[1] = (int)st->isr_us, g->num[2] = (int)st->late, g->num[3] = (int)st->lock_us;
        g->num[4] = (int)((st->frame_us + 50u) / 100u), g->num[5] = a->bpm;
    }
    v->voices = g->num[0];
    v->isr_us = g->num[1], v->late = g->num[2], v->lock_us = g->num[3], v->frame10 = g->num[4], v->bpm = g->num[5];
    v->isr_frac = st->isr_budget_us ? (float)v->isr_us / (float)st->isr_budget_us : 0.f;
    v->load_pct = (int)(v->load * 100.f + 0.5f), v->load1_pct = (int)(v->load1 * 100.f + 0.5f);
    v->fps_n = (int)(g->bar[1] + 0.5f), v->ram_pct = (int)(v->ram * 100.f + 0.5f);
    v->store_pct = (int)(v->store * 100.f + 0.5f);
    v->boot = st->have & QD_ST_BOOT ? st->boot : 0;
    /* the scope: ~3 cycles of the loudest track's lowest note, held on a rising zero crossing (none: free-running) */
    v->scope_track = -1;
    {
        int best = -1, low = 128, n, start, k;
        int32_t lmax = 0, amp = 1;
        const uint32_t w = g->scope_w;
        for (t = 0; t < 8; t++)
            if (tl[t] > lmax)
                lmax = tl[t], best = t;
        if (best >= 0 && lmax > 64) {
            v->scope_track = best;
            for (i = 0; i < a->pool.nvoice; i++)
                if (qd_pool_sounding(&a->pool, i) && a->pool.v[i].track == best && a->pool.v[i].q.note < low)
                    low = a->pool.v[i].q.note;
        }
        n = 600;                                        /* (no note: 27 ms) */
        if (low < 128) {
            const float f = 440.f * powf(2.f, (float)(low - 69) / 12.f);
            n = (int)(3.f * 22050.f / f + 0.5f);
        }
        n = n < 64 ? 64 : n > QD_SCOPE_N / 2 ? QD_SCOPE_N / 2 : n;
        start = (int)(w - (uint32_t)n) - 1;             /* (the newest n samples' start; a rising crossing before it) */
        if (low < 128)
            for (k = 0; k < QD_SCOPE_N / 2 - 2; k++) {
                const int16_t s0 = g->scope[(uint32_t)(start - k - 1) & (QD_SCOPE_N - 1u)],
                              s1 = g->scope[(uint32_t)(start - k) & (QD_SCOPE_N - 1u)];
                if (s0 < 0 && s1 >= 0) {
                    start -= k;
                    break;
                }
            }
        for (k = 0; k < n; k++) {
            const int s = g->scope[(uint32_t)(start + k) & (QD_SCOPE_N - 1u)];
            amp = abs(s) > amp ? abs(s) : amp;
        }
        amp = amp < 256 ? 256 : amp;                    /* (the line's height: the window's own peak) */
        for (i = 0; i < UI_GEEK_SCOPE; i++) {
            const int s = g->scope[(uint32_t)(start + i * (n - 1) / (UI_GEEK_SCOPE - 1)) & (QD_SCOPE_N - 1u)];
            v->scope[i] = v->scope_track >= 0 ? tanhf(1.3f * (float)s / (float)amp) / tanhf(1.3f) : 0.f;
        }
    }
    g->ms = now ? now : 1u;
}
