/* SPDX-License-Identifier: GPL-3.0-only */
/* The frame (ctl.h): the screens' view records (ui.h) filled from the model and the UI state, then drawn. The screen
 * under everything is the mode's (idle, the step view or a held step's field, KEYS, the tracks); over it a knob's
 * takeover (PLAN §9a rule 5: shown at the first detent, 1 s after the last one it fades back over 120 ms) and a
 * confirmation word. Nothing here changes the model or the UI state; the motion is the app's milliseconds and the
 * transport's position. */
#include <math.h>
#include <string.h>

#include "app.h"
#include "ui.h"
#include "../seq/chord.h"

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

/* a lock's or a value's name: FM TONE's label, the envelopes told apart */
static const char *value_name(int id)
{
    static const char *const MIX[6] = {"PAN", "DELAY", "REVERB", "CHORUS", "DRIVE", "LEVEL"};
    static const char *const LFO[3] = {"LFO1 ", "LFO2 ", "LFO3 "};
    static char buf[24];
    if (id >= QD_QUAD_NP)
        return id - QD_QUAD_NP < 6 ? MIX[id - QD_QUAD_NP] : "?";
    switch (id) {
    case QD_QP_FATK: return "FILT ATK";
    case QD_QP_FDEC: return "FILT DEC";
    case QD_QP_FSUS: return "FILT SUS";
    case QD_QP_FREL: return "FILT REL";
    case QD_QP_FDEPTH: return "FILT DEPTH";
    case QD_QP_FDLY: return "FILT DELAY";
    case QD_QP_EATK: return "AMP ATK";
    case QD_QP_EDEC: return "DECAY";
    case QD_QP_ESUS: return "AMP SUS";
    case QD_QP_EREL: return "AMP REL";
    default: break;
    }
    if (id >= QD_QP_LFO0 && id < QD_QP_LFO0 + 24) {
        buf[0] = 0;
        cat(cat(buf, LFO[(id - QD_QP_LFO0) / 8]), qd_quad_value_name(id));
        return buf;
    }
    return qd_quad_value_name(id);
}

static void value_text(char *out, int id, int v)
{
    int16_t lo = 0, hi = 0;
    char n[12];
    qd_lock_range(id, &lo, &hi);
    out[0] = 0;
    if (id == QD_LK_PAN) {
        cat(out, v < 0 ? "L" : v > 0 ? "R" : "C");
        if (v)
            cat(out, ui_itoa(n, v < 0 ? -v : v));
    } else if (hi - lo > 1000) {
        fmt_hundredths(out, v, 2);
    } else {
        cat(out, ui_itoa(n, v));
    }
}

static float range01(int id, int v)
{
    int16_t lo = 0, hi = 1;
    qd_lock_range(id, &lo, &hi);
    return hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.f;
}

static float secs(uint32_t ms) { return (float)ms / 1000.f; }

/* ---------------------------------------------------------------- the step view --- */
static void fill_step(qd_app_t *a, ui_step_t *v, char *pat)
{
    const qd_ui_t *u = &a->ui;
    const int t = a->sel, page = u->page[t];
    const qd_pattern_t *P = qd_ctl_pat(a);
    const int len = P->trk[t].length;
    int i, j, lo = 128, hi = -1;
    memset(v, 0, sizeof *v);
    v->track = t + 1;
    v->name = a->model.t[t].name;
    v->pattern = qd_pat_name(a->seq.cur, pat);
    v->bpm = a->bpm;
    v->page = page;
    v->npages = qd_ctl_npages(a, t);
    v->follow = u->follow;
    v->play_page = -1;
    v->pos = -1.f;
    if (a->seq.playing) {
        const double tp = qd_seq_track_pos(&a->seq, t);
        v->play_page = (int)(tp / QD_PAGE);
        if (v->play_page == page)
            v->pos = (float)(tp - page * QD_PAGE);
    }
    for (i = 0; i < 16; i++) {
        const int s = page * QD_PAGE + i;
        const qd_step_t *st = qd_step(P, t, s);
        ui_col_t *c = &v->col[i];
        const int e = qd_len_eighths(st->len);
        c->type = (uint8_t)qd_step_type(st);
        c->beyond = s >= len;
        for (j = 0; j < u->nheld; j++)
            c->held |= u->held[j] == s;
        if (!c->type || c->beyond)
            continue;
        if (c->type != QD_TRIG_LOCK)
            c->nnote = (uint8_t)qd_step_notes(st, c->note);
        c->len = e < 0 ? (float)(16 - i) : (float)e / 8.f;
        c->micro = st->micro;
        c->ratch = (uint8_t)qd_step_ratchet(st);
        c->lock = (st->flags & QD_SF_LOCKS) != 0;
        c->cond = st->cond != QD_COND_ALWAYS;
        c->chance = st->chance < 100;
        c->sound = st->sound != 0;
        if (v->pos >= 0.f) {
            const float x0 = (float)i + (float)st->micro / 24.f;
            c->lit = v->pos >= x0 && v->pos < x0 + (c->len > 0.3f ? c->len : 0.3f);
        }
        for (j = 0; j < c->nnote; j++) {
            lo = c->note[j] < lo ? c->note[j] : lo;
            hi = c->note[j] > hi ? c->note[j] : hi;
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

/* ---------------------------------------------------------------- a held step's field --- */
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

static const char *sound_name(const qd_app_t *a, int k)   /* a sound lock's value 0..17 */
{
    if (k <= 0)
        return a->model.t[a->sel].name[0] ? "TRACK SOUND" : "";
    return qd_quad_preset_name(k - 1);
}

static void fill_held(qd_app_t *a, ui_held_t *v)
{
    static char lk_names[3][24];
    const qd_ui_t *u = &a->ui;
    const int t = a->sel, s = u->held[0];
    const qd_pattern_t *P = qd_ctl_pat(a);
    const qd_step_t *st = qd_step(P, t, s);
    const qd_tset_t *ts = &P->trk[t];
    int i, k;
    memset(v, 0, sizeof *v);
    v->field = u->field;
    v->step = s + 1;
    v->nheld = u->nheld;
    v->track = t + 1;
    v->name = a->model.t[t].name;
    v->set = qd_step_fields_set(a, t, s);
    v->type = qd_step_type(st);
    v->play = u->held_played ? secs(a->ms - u->held_play_ms) : -1.f;
    v->t = secs(a->ms);
    v->tracklen = ts->length;
    /* NOTE */
    if (v->type && st->note[0]) {
        v->note[v->nnote++] = st->note[0];
        for (k = 1; k < QD_NNOTE; k++)
            if (st->note[k] && st->note[0] + st->note[k] <= 127)
                v->note[v->nnote++] = (uint8_t)(st->note[0] + st->note[k]);
        for (k = 0; k < v->nnote; k++)
            qd_note_name(v->note[k], v->note_name[k]);
        if (!qd_chord_name(v->note, v->nnote, v->root, v->qual)) {
            v->root[0] = 0;
            for (k = 0; k < v->nnote; k++) {
                if (k)
                    cat(v->names, " ");
                cat(v->names, qd_pc_name(v->note[k]));
            }
        }
    }
    v->squeeze = secs(a->ms - u->chord_ms);
    /* VEL, RATCH */
    v->vel = st->vel ? st->vel : ts->def_vel;
    v->ramp = qd_step_ramp(st);
    v->ratch = v->count = qd_step_ratchet(st);
    v->ball = (float)(a->ms % 1600u) / 1600.f;
    /* LEN */
    v->len = v->type ? st->len : ts->def_len;
    v->len_name = qd_len_name(v->len);
    v->len_steps = qd_len_eighths(v->len) < 0 ? -1.f : (float)qd_len_eighths(v->len) / 8.f;
    v->tie = v->type == QD_TRIG_TIE;
    for (k = 1; k <= 16 && s - k >= 0; k++) {          /* the note before (within a page back) */
        const qd_step_t *q = qd_step(P, t, s - k);
        const int ty = qd_step_type(q), e = qd_len_eighths(q->len);
        if (ty == QD_TRIG_NOTE || ty == QD_TRIG_TIE) {
            v->has_prev = 1;
            v->prev_at = (float)-k + (float)q->micro / 24.f;
            v->prev_len = e < 0 ? (float)k : (float)e / 8.f;
            break;
        }
    }
    for (k = 1; k <= 8 && s + k < ts->length; k++) {   /* the note after (on the ruler) */
        const qd_step_t *q = qd_step(P, t, s + k);
        const int ty = qd_step_type(q), e = qd_len_eighths(q->len);
        if (ty == QD_TRIG_NOTE || ty == QD_TRIG_TIE) {
            v->next_at = (float)k + (float)q->micro / 24.f;
            v->next_len = e < 0 ? 2.f : (float)e / 8.f;
            break;
        }
    }
    /* MICRO */
    v->micro = st->micro;
    v->strum = st->strum;
    v->walk = (float)(a->ms % 2400u) / 2400.f * 52.f - 14.f;
    /* COND / CHNC */
    qd_cond_name(st->cond, v->cond_name);
    v->cond_text = cond_text(st->cond);
    if (!qd_cond_ab_of(st->cond, &v->ab_a, &v->ab_b))
        v->ab_a = v->ab_b = 0;
    v->pass = -1;
    if (a->seq.playing && v->ab_b)
        v->pass = (int)(a->seq.cur_t[t].pass % v->ab_b);
    v->chance = v->type ? st->chance : 100;
    v->shuffle = (uint32_t)(a->seq.playing ? a->seq.cur_t[t].pass : 0) * 2654435761u + 7919u;
    v->chance_first = u->field == UI_F_CHNC;
    /* SOUND */
    v->sound = st->sound;
    v->sound_cur = sound_name(a, st->sound);
    v->sound_prev = st->sound > 0 ? sound_name(a, st->sound - 1) : 0;
    v->sound_next = st->sound < QD_QUAD_NPRESET ? sound_name(a, st->sound + 1) : 0;
    if (a->ms - u->roll_ms < (uint32_t)QD_ROLL_MS)
        v->roll = 1.f - (float)(a->ms - u->roll_ms) / QD_ROLL_MS;
    v->dir = u->roll_dir;
    for (i = 0; i < 16; i++)
        v->page_sound[i] = qd_step(P, t, (s / QD_PAGE) * QD_PAGE + i)->sound;
    v->held_col = s % QD_PAGE;
    v->nlocks = qd_lock_count(P, t, s);
    /* LOCKS */
    {
        int n;
        const qd_lock_t *l = qd_locks(P, t, s, &n);
        v->nlock = n;
        v->lock_cur = n ? (u->lock_cur < n ? u->lock_cur : n - 1) : 0;
        for (k = 0; k < 3 && n; k++) {
            const int j = v->lock_cur - 1 + k;
            if (j < 0 || j >= n)
                continue;
            strcpy(lk_names[k], value_name(l[j].id));
            v->lk_name[k] = lk_names[k];
            value_text(v->lk_val[k], l[j].id, l[j].value);
        }
        if (n) {
            const qd_lock_t *c = &l[v->lock_cur];
            const int base = qd_step_base(a, t, s, c->id);
            v->lk_x = range01(c->id, c->value);
            v->tr_x = range01(c->id, base);
            value_text(v->tr_val, c->id, base);
        }
    }
}

/* ---------------------------------------------------------------- the takeovers --- */
static void fill_turn(qd_app_t *a, ui_turn_t *v, char *ctx)
{
    static const char *const NAMES[UI_NTK] = {"FREQ", "DECAY", "HARM", "FEEDBACK", "ALGO", "TEMPO", "SWING",
                                              "LENGTH", "RATE", "LIMIT", "SWING"};
    const qd_ui_t *u = &a->ui;
    const int t = a->sel;
    const qd_pattern_t *P = qd_ctl_pat(a);
    const qd_tset_t *ts = &P->trk[t];
    double beats;
    char n[12];
    int id = -1, i;
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
        v->value = ts->length;
        v->v = (float)ts->length / (float)QD_NSTEP;
        ui_itoa(v->val, ts->length);
        break;
    case UI_TK_RATE: {
        const int k = qd_ctl_rate_index(ts->rate);
        const double L = (double)qd_rate_ticks(ts->rate);
        const double tick = a->seq.playing ? (double)qd_seq_tick(&a->seq) : beats * 4.0 * QD_TPS;
        v->value = ts->rate;
        v->v = (float)k / (float)(QD_NRATE - 1);
        strcpy(v->val, qd_rate_name(ts->rate));
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
    default:
        break;
    }
}

/* ---------------------------------------------------------------- KEYS, the tracks, the picker, idle --- */
static void draw_keys(qd_app_t *a)
{
    static ui_keys_t v;
    const qd_ui_t *u = &a->ui;
    const qd_track_t *t = &a->model.t[a->sel];
    const qd_pattern_t *P = qd_ctl_pat(a);
    int i;
    memset(&v, 0, sizeof v);
    v.track = a->sel + 1;
    v.name = t->name;
    v.preset = t->preset;
    v.sound = qd_track_sound_name(t);
    v.edited = t->edited;
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
        v.rec = 1;
        v.cursor = u->cursor + 1;
        for (i = 0; i < 16; i++)
            v.page_trig[i] = qd_step_type(qd_step(P, a->sel, (u->cursor / QD_PAGE) * QD_PAGE + i)) != 0;
    } else if (u->rec) {
        const double L = (double)qd_rate_ticks(P->trk[a->sel].rate);
        const int64_t now = qd_seq_tick(&a->seq);
        v.rec = 2;
        v.quantize = u->quantize;
        v.pos = (float)qd_seq_track_pos(&a->seq, a->sel);
        v.step = (int)v.pos + 1;
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
        c->len = ts->length ? ts->length : 1;
        c->rate = qd_rate_name(ts->rate);
        for (s = 0; s < c->len; s++) {
            const int ty = qd_step_type(&P->step[t][s]);
            c->trig[s] = (uint8_t)(ty == QD_TRIG_LOCK ? 2 : ty ? 1 : 0);
        }
        c->pos = (float)qd_seq_track_pos(&a->seq, t);
        c->flash_step = a->seq.playing ? a->seq.flash_step[t] : -1;
        c->flash_age = (float)((double)(tick - a->seq.flash_tick[t]) / (double)qd_rate_ticks(ts->rate));
        c->mute = ts->mute || ((a->seq.mute >> t) & 1u);
        c->swing = ts->swing;
        for (i = 0; i < a->pool.nvoice; i++)
            c->voices += qd_pool_sounding(&a->pool, i) && a->pool.v[i].track == t;
    }
    ui_tracks_draw(&v);
}

static void draw_picker(const qd_app_t *a)
{
    const qd_track_t *t = &a->model.t[a->sel];
    const float d = (float)(a->ms - a->ui.tk_ms);
    ui_picker_t v;
    v.track = a->sel + 1;
    v.name = t->name;
    v.k = t->preset;
    v.n = QD_QUAD_NPRESET;
    v.cur = qd_quad_preset_name(v.k);
    v.prev = v.k > 0 ? qd_quad_preset_name(v.k - 1) : 0;
    v.next = v.k + 1 < v.n ? qd_quad_preset_name(v.k + 1) : 0;
    v.roll = d >= QD_ROLL_MS ? 0.f : 1.f - d / QD_ROLL_MS;
    v.dir = a->ui.tk_dir;
    v.reel = (float)fmod((double)a->ms * 0.0025, 6.283185307179586);
    ui_picker_draw(&v);
}

static void draw_idle(const qd_app_t *a)
{
    ui_idle_t v;
    memset(&v, 0, sizeof v);
    qd_pat_name(a->seq.cur, v.pattern);
    if (qd_seq_queued(&a->seq) >= 0 && (a->ms / QD_BLINK_MS) % 2u)
        v.pattern[0] = 0;                               /* another is queued: the name blinks */
    v.bpm = a->bpm;
    v.playing = a->playing;
    v.turn = a->turn;
    v.phase = (float)(a->playing ? fmod(a->beats * 6.283185307179586, 2.0 * 6.283185307179586) : a->sway);
    v.amp = a->amp;
    ui_idle_draw(&v);
}

/* an overlay drawn over the screen at alpha al (a black ground first) */
static void overlay_begin(float al)
{
    qd_save();
    qd_alpha(al);
    qd_fill_rect(0.f, 0.f, (float)QD_W, (float)QD_H, UI_BG);
}

void qd_view_frame(qd_app_t *a, uint16_t *lcd_be)
{
    static ui_step_t sv;
    static ui_held_t hv;
    static ui_turn_t tv;
    const qd_ui_t *u = &a->ui;
    char pat[4], ctx[24];
    qd_begin();
    switch (u->mode) {
    case QD_MODE_KEYS:
        draw_keys(a);
        break;
    case QD_MODE_TRACKS:
        draw_tracks(a);
        break;
    case QD_MODE_STEP:
        if (u->nheld) {
            fill_held(a, &hv);
            ui_held_draw(&hv);
        } else {
            fill_step(a, &sv, pat);
            ui_step_draw(&sv);
        }
        break;
    default:
        draw_idle(a);
        break;
    }
    if (u->tk != QD_TK_NONE) {                          /* the takeover, then 120 ms back */
        const uint32_t d = a->ms - u->tk_ms;
        const float al = d < QD_TK_MS ? 1.f : 1.f - (float)(d - QD_TK_MS) / (float)QD_TK_FADE_MS;
        if (al > 0.f) {
            overlay_begin(al);
            if (u->tk == QD_TK_PRESET) {
                draw_picker(a);
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
