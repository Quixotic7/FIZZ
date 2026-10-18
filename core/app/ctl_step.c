/* SPDX-License-Identifier: GPL-3.0-only */
/* The step view's edits (ctl.h): the white keys' tap and hold, the held steps, the fields and their knobs, the p-locks
 * of a held step, CLEAR and its undo, the "last entered" defaults (PLAN §4: a new trig gets the track's last note,
 * velocity and length).
 *
 * A WHITE KEY (step view) goes down PENDING: its LED shows at once what a tap would make. Released under 250 ms with
 * nothing turned, it is a TAP. At 250 ms, or when a knob turns or a black key is pressed while it is down, it is
 * HELD: its step joins the held steps (the first held is the one shown; every edit goes to all). Released, a held step
 * is let go, unless the steps are latched (OCT+): then they stay until OCT- or HOME, and a white tap adds or removes a
 * step from the latched ones.
 *
 * THE FIELDS' KNOBS (PLAN §8; FIELD_KNOBS below): SELECT, KNOB 1..4 per field. A knob the field does not use makes a
 * p-lock of the quick page (KNOB 1 FREQ, 2 DECAY, 3 HARM, 4 FDBK) on the held steps, its takeover marked as a lock.
 * An EMPTY step held (no field key pressed in this hold) has no field to edit: KNOB 1..4 are the quick page's locks,
 * so a turn makes a LOCK step with that lock (Elektron's habit). PRESETS is the step's sound lock (the SOUND field),
 * ALGORITHM an ALGO lock. Edits are relative: each held step moves from its own value. */
#include <string.h>

#include "app.h"
#include "ui.h"
#include "../seq/chord.h"

static const uint8_t QUICK_ID[4] = {QD_QP_FREQ, QD_QP_EDEC, QD_QP_HARM, QD_QP_FDBK};   /* PLAN question 11 */
int qd_step_quick_id(int k) { return QUICK_ID[k & 3]; }

int qd_step_id_detent(int id)
{
    int16_t lo = 0, hi = 0;
    qd_lock_range(id, &lo, &hi);
    return hi - lo > 1000 ? 100 : 1;   /* the hi-res values (hundredths) move a whole unit a detent */
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static qd_pattern_t *PAT(qd_app_t *a) { return qd_ctl_pat(a); }
static const qd_step_t *STEP(qd_app_t *a, int s) { return qd_step(PAT(a), a->sel, s); }
static int step_of(const qd_app_t *a, int w) { return a->ui.page[a->sel] * QD_PAGE + w; }

void qd_step_edited(qd_app_t *a) { a->ui.undo_ok = 0; }

/* ---------------------------------------------------------------- the held steps --- */
static int held_find(const qd_ui_t *u, int s)
{
    int i;
    for (i = 0; i < u->nheld; i++)
        if (u->held[i] == s)
            return i;
    return -1;
}

static void held_add(qd_app_t *a, int s, int key)
{
    qd_ui_t *u = &a->ui;
    if (u->nheld >= 16)
        return;
    if (!u->nheld) {                                    /* a new hold: the field remembered, no field key yet */
        u->field_pressed = 0;
        u->held_empty = qd_step_type(STEP(a, s)) == QD_TRIG_NONE;
        u->lock_cur = 0;
        u->chord_ms = a->ms - 10000u;
        u->held_played = 0;
    }
    u->held[u->nheld] = s;
    u->held_key[u->nheld] = (int8_t)key;
    u->nheld++;
}

static void held_remove(qd_ui_t *u, int i)
{
    for (; i + 1 < u->nheld; i++) {
        u->held[i] = u->held[i + 1];
        u->held_key[i] = u->held_key[i + 1];
    }
    u->nheld--;
    if (!u->nheld)
        u->latch = 0;
}

void qd_step_release_all(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    u->nheld = 0;
    u->latch = 0;
    u->wk_down = u->wk_pending = 0;
}

void qd_step_latch(qd_app_t *a, int on)
{
    qd_ui_t *u = &a->ui;
    int i;
    if (on) {
        u->latch = u->nheld > 0;
        return;
    }
    u->latch = 0;                                       /* let go: only the keys still down stay held */
    for (i = u->nheld - 1; i >= 0; i--)
        if (u->held_key[i] < 0)
            held_remove(u, i);
}

void qd_step_promote(qd_app_t *a, int w)
{
    qd_ui_t *u = &a->ui;
    const int s = step_of(a, w);
    int i;
    if (!((u->wk_pending >> w) & 1u))
        return;
    u->wk_pending &= (uint16_t)~(1u << w);
    i = held_find(u, s);
    if (i >= 0)
        u->held_key[i] = (int8_t)w;
    else
        held_add(a, s, w);
}

/* the pending keys held, in the order they went down (only those down QD_TAP_MS when due_only) */
static void promote_ordered(qd_app_t *a, int due_only)
{
    qd_ui_t *u = &a->ui;
    for (;;) {
        int w, best = -1;
        for (w = 0; w < 16; w++)
            if (((u->wk_pending >> w) & 1u) && (!due_only || a->ms - u->wk_t0[w] >= QD_TAP_MS) &&
                (best < 0 || u->wk_seq[w] < u->wk_seq[best]))
                best = w;
        if (best < 0)
            return;
        qd_step_promote(a, best);
    }
}
void qd_step_promote_all(qd_app_t *a) { promote_ordered(a, 0); }
void qd_step_promote_due(qd_app_t *a) { promote_ordered(a, 1); }

/* ---------------------------------------------------------------- the white keys --- */
void qd_step_white_down(qd_app_t *a, int w)
{
    qd_ui_t *u = &a->ui;
    const uint16_t bit = (uint16_t)(1u << w);
    u->wk_down |= bit;
    u->wk_pending |= bit;
    u->wk_t0[w] = a->ms;
    u->wk_seq[w] = ++u->wk_count;
    if (qd_ctl_func(a))
        u->wk_func |= bit;
    else
        u->wk_func &= (uint16_t)~bit;
}

/* a tap: empty -> a NOTE trig with the track's defaults, a trig -> empty (with its locks); FUNC: a LOCK step, or back
 * to empty. Latched: the step joins or leaves the latched steps */
static void tap(qd_app_t *a, int w, int func)
{
    qd_ui_t *u = &a->ui;
    qd_pattern_t *p = PAT(a);
    const int s = step_of(a, w), ty = qd_step_type(STEP(a, s));
    if (u->latch && u->nheld) {
        const int i = held_find(u, s);
        if (i >= 0)
            held_remove(u, i);
        else
            held_add(a, s, -1);
        return;
    }
    qd_step_edited(a);
    if (func)
        qd_step_set_trig(p, a->sel, s, ty == QD_TRIG_LOCK ? QD_TRIG_NONE : QD_TRIG_LOCK);
    else
        qd_step_set_trig(p, a->sel, s, ty ? QD_TRIG_NONE : QD_TRIG_NOTE);
}

void qd_step_white_up(qd_app_t *a, int w)
{
    qd_ui_t *u = &a->ui;
    const uint16_t bit = (uint16_t)(1u << w);
    int i;
    if (!(u->wk_down & bit))
        return;
    u->wk_down &= (uint16_t)~bit;
    if ((u->wk_pending & bit) && a->ms - u->wk_t0[w] < QD_TAP_MS) {
        u->wk_pending &= (uint16_t)~bit;
        tap(a, w, (u->wk_func & bit) != 0);
        return;
    }
    qd_step_promote(a, w);                              /* (250 ms reached at this very release) */
    for (i = 0; i < u->nheld; i++)
        if (u->held_key[i] == w) {
            if (u->latch)
                u->held_key[i] = -1;                    /* latched: it stays */
            else
                held_remove(u, i);
            break;
        }
}

/* ---------------------------------------------------------------- the fields --- */
void qd_step_field_key(qd_app_t *a, int b)
{
    qd_ui_t *u = &a->ui;
    qd_step_promote_all(a);
    if (!u->nheld)
        return;
    if (b == UI_F_CLEAR) {                              /* CLEAR: the held steps cleared; again within 4 s: undone */
        qd_pattern_t *p = PAT(a);
        int i;
        if (u->undo_ok && a->ms - u->undo_ms < QD_UNDO_MS && u->undo_pat_k == a->seq.cur) {
            *p = u->undo_pat;
            u->undo_ok = 0;
            qd_ctl_confirm(a, "UNDONE", "the steps are back");
            return;
        }
        u->undo_pat = *p;
        u->undo_pat_k = a->seq.cur;
        for (i = 0; i < u->nheld; i++)
            qd_step_clear(p, a->sel, u->held[i]);
        u->undo_ok = 1;
        u->undo_ms = a->ms;
        qd_ctl_confirm(a, "CLEARED", "CLEAR again to undo");
        return;
    }
    u->field = b;
    u->field_pressed = 1;
    if (b == UI_F_LOCKS)
        u->lock_cur = 0;
}

/* what each knob edits, per field (PLAN §8's table): SELECT, KNOB 1..4. SELECT and KNOB 1 turn the big value, except in
 * NOTE, where SELECT picks a chord shape (§8). E_NONE on KNOB 1..4: a p-lock of the quick page */
enum { E_NONE, E_NOTE1, E_NOTE2, E_NOTE3, E_NOTE4, E_SHAPE, E_VEL, E_RAMP, E_LEN, E_TIE, E_MICRO, E_STRUM, E_COUNT,
       E_COND, E_CHANCE, E_TYPE, E_SOUND, E_LKVAL, E_LKSEL };
static const uint8_t FIELD_KNOBS[UI_NFIELD][5] = {
    /*                SELECT    KNOB 1    KNOB 2    KNOB 3   KNOB 4 */
    [UI_F_NOTE] =  {E_SHAPE,  E_NOTE1,  E_NOTE2,  E_NOTE3, E_NOTE4},
    [UI_F_VEL] =   {E_VEL,    E_VEL,    E_RAMP,   E_NONE,  E_NONE},
    [UI_F_LEN] =   {E_LEN,    E_LEN,    E_TIE,    E_NONE,  E_NONE},
    [UI_F_MICRO] = {E_MICRO,  E_MICRO,  E_STRUM,  E_NONE,  E_NONE},
    [UI_F_RATCH] = {E_COUNT,  E_COUNT,  E_RAMP,   E_NONE,  E_NONE},
    [UI_F_COND] =  {E_COND,   E_COND,   E_CHANCE, E_NONE,  E_NONE},
    [UI_F_CHNC] =  {E_CHANCE, E_CHANCE, E_COND,   E_NONE,  E_NONE},
    [UI_F_TRIG] =  {E_TYPE,   E_TYPE,   E_NONE,   E_NONE,  E_NONE},
    [UI_F_SOUND] = {E_SOUND,  E_SOUND,  E_NONE,   E_NONE,  E_NONE},
    [UI_F_LOCKS] = {E_LKVAL,  E_LKVAL,  E_LKSEL,  E_NONE,  E_NONE},
    [UI_F_CLEAR] = {E_NONE,   E_NONE,   E_NONE,   E_NONE,  E_NONE},
};

static void chord_of(const qd_step_t *st, char *name)  /* its name as shown (for the squeeze) */
{
    uint8_t n[4];
    char r[4], q[8];
    const int k = qd_step_notes(st, n);
    name[0] = 0;
    if (k && qd_chord_name(n, k, r, q)) {
        strcpy(name, r);
        strcat(name, q);
    }
}

/* one held step, one detent's worth d of edit e */
static void edit(qd_app_t *a, int s, int e, int d)
{
    qd_pattern_t *p = PAT(a);
    qd_tset_t *ts = &p->trk[a->sel];
    const int t = a->sel;
    const qd_step_t *st = qd_step(p, t, s);
    const int had = qd_step_type(st) != QD_TRIG_NONE;
    int v, k;
    switch (e) {
    case E_NOTE1:
        v = clampi((had ? st->note[0] : ts->def_note) + (had ? d : 0), 1, 127);
        qd_step_set_root(p, t, s, v);
        ts->def_note = (uint8_t)v;
        break;
    case E_NOTE2:
    case E_NOTE3:
    case E_NOTE4:
        k = e - E_NOTE1;
        for (v = d < 0 ? -d : d; v > 0; v--) {          /* a semitone a detent, past an offset another note has */
            int o = st->note[k];
            do
                o = clampi(o + (d > 0 ? 1 : -1), 0, 48);
            while (qd_step_set_offset(p, t, s, k, o) == QD_ENOTE && o > 0 && o < 48);
        }
        break;
    case E_SHAPE:
        k = qd_shape_of(&st->note[1]);
        k = clampi(k < 0 ? (d > 0 ? 0 : QD_NSHAPE - 1) : k + d, 0, QD_NSHAPE - 1);
        for (v = 1; v < QD_NNOTE; v++)
            qd_step_set_offset(p, t, s, v, 0);
        for (v = 0; v < QD_SHAPE[k].n; v++)
            qd_step_set_offset(p, t, s, v + 1, QD_SHAPE[k].off[v]);
        break;
    case E_VEL:
        v = clampi((had ? st->vel : ts->def_vel) + d, 1, 127);
        qd_step_set_vel(p, t, s, v);
        ts->def_vel = (uint8_t)v;
        break;
    case E_RAMP:
        qd_step_set_ratchet(p, t, s, qd_step_ratchet(st), clampi(qd_step_ramp(st) + d, 0, QD_RAMP_ACCENT));
        break;
    case E_LEN:
        v = clampi((had ? st->len : ts->def_len) + d, 0, QD_LEN_INF);
        qd_step_set_len(p, t, s, v);
        ts->def_len = (uint8_t)v;
        break;
    case E_TIE:
        qd_step_set_trig(p, t, s, d > 0 ? QD_TRIG_TIE : QD_TRIG_NOTE);
        break;
    case E_MICRO:
        qd_step_set_micro(p, t, s, clampi(st->micro + d, -23, 23));
        break;
    case E_STRUM:
        qd_step_set_strum(p, t, s, clampi(st->strum + d, 0, 23));
        break;
    case E_COUNT:
        qd_step_set_ratchet(p, t, s, clampi(qd_step_ratchet(st) + d, 1, 8), qd_step_ramp(st));
        break;
    case E_COND:
        qd_step_set_cond(p, t, s, clampi(st->cond + d, 0, QD_NCOND - 1));
        break;
    case E_CHANCE:
        qd_step_set_chance(p, t, s, clampi((had ? st->chance : 100) + d, 0, 100));
        break;
    case E_TYPE:
        qd_step_set_trig(p, t, s, clampi(qd_step_type(st) + d, QD_TRIG_NOTE, QD_TRIG_TIE));
        break;
    case E_SOUND:
        qd_step_set_sound(p, t, s, clampi(st->sound + d, 0, QD_QUAD_NPRESET));
        break;
    default:
        break;
    }
}

/* the value a lock on step s starts from: the step's sound lock's, else the track's sound's */
int qd_step_base(qd_app_t *a, int t, int s, int id)
{
    const qd_step_t *st = qd_step(PAT(a), t, s);
    if (id >= QD_QUAD_NP) {
        const qd_mix_track_t *m = &a->model.t[t].mix;
        return id == QD_LK_PAN ? m->pan : id == QD_LK_DELAY ? m->delay : id == QD_LK_REVERB ? m->reverb
             : id == QD_LK_CHORUS ? m->chorus : id == QD_LK_LEVEL ? m->level : 0;
    }
    if (st->sound) {
        int16_t patch[QD_QUAD_NP];
        qd_quad_preset_patch(st->sound - 1, patch);
        return patch[id];
    }
    return a->model.t[t].patch[id];
}

/* a lock on every held step: value from its own (the lock's, else the base) + d detents. 0: the pool is full */
static int lock_all(qd_app_t *a, int id, int d)
{
    qd_ui_t *u = &a->ui;
    int i, ok = 1;
    for (i = 0; i < u->nheld; i++) {
        int16_t v;
        const int s = u->held[i];
        const int base = qd_lock_get(PAT(a), a->sel, s, id, &v) ? v : qd_step_base(a, a->sel, s, id);
        if (qd_lock_set(PAT(a), a->sel, s, id, base + d * qd_step_id_detent(id)) == QD_EFULL)
            ok = 0;
    }
    if (!ok)
        qd_ctl_confirm(a, "NO ROOM", "for locks: the pattern has 1024");
    return ok;
}

int qd_step_knob(qd_app_t *a, int knob, int d)
{
    qd_ui_t *u = &a->ui;
    int e, i, col, empty;
    char before[16], after[16];
    qd_step_promote_all(a);
    if (!u->nheld)
        return 0;
    qd_step_edited(a);
    empty = u->held_empty && !u->field_pressed;   /* (it stays "empty" after its first lock made it a LOCK step) */
    if (knob == QD_K_PRESETS) {                         /* the step's sound lock: the SOUND field */
        u->field = UI_F_SOUND;
        e = E_SOUND;
    } else if (knob == QD_K_ALGO) {                     /* an ALGO lock */
        lock_all(a, QD_QP_ALGO, d);
        qd_ctl_takeover(a, UI_TK_ALGO, 0, d, 1, u->held[0]);
        return 1;
    } else {
        col = knob == QD_K_SELECT ? 0 : knob - QD_K_1 + 1;
        e = empty ? E_NONE : FIELD_KNOBS[u->field][col];
        if (e == E_NONE) {
            if (knob == QD_K_SELECT)
                return 1;
            lock_all(a, QUICK_ID[knob - QD_K_1], d);   /* the quick page's lock (an empty step: a LOCK step) */
            qd_ctl_takeover(a, UI_TK_FREQ + (knob - QD_K_1), knob - QD_K_1, d, 1, u->held[0]);
            return 1;
        }
    }
    chord_of(STEP(a, u->held[0]), before);
    if (e == E_LKSEL) {                                 /* LOCKS: which lock */
        const int n = qd_lock_count(PAT(a), a->sel, u->held[0]);
        const int c = clampi(u->lock_cur + d, 0, n > 0 ? n - 1 : 0);
        if (c != u->lock_cur) {
            u->roll_ms = a->ms;
            u->roll_dir = d > 0 ? 1 : -1;
        }
        u->lock_cur = c;
        return 1;
    }
    if (e == E_LKVAL) {                                 /* LOCKS: the lock's value (FUNC: the lock removed) */
        int n;
        const qd_lock_t *l = qd_locks(PAT(a), a->sel, u->held[0], &n);
        int id;
        if (!n)
            return 1;
        id = l[clampi(u->lock_cur, 0, n - 1)].id;
        if (qd_ctl_func(a)) {
            for (i = 0; i < u->nheld; i++)
                qd_lock_clear(PAT(a), a->sel, u->held[i], id);
            n = qd_lock_count(PAT(a), a->sel, u->held[0]);
            u->lock_cur = clampi(u->lock_cur, 0, n > 0 ? n - 1 : 0);
        } else {
            for (i = 0; i < u->nheld; i++) {
                int16_t v;
                if (qd_lock_get(PAT(a), a->sel, u->held[i], id, &v))
                    qd_lock_set(PAT(a), a->sel, u->held[i], id, v + d * qd_step_id_detent(id));
            }
        }
        return 1;
    }
    for (i = 0; i < u->nheld; i++)
        edit(a, u->held[i], e, d);
    if (e == E_SOUND) {
        u->roll_ms = a->ms;
        u->roll_dir = d > 0 ? 1 : -1;
    }
    chord_of(STEP(a, u->held[0]), after);
    if (strcmp(before, after) != 0)
        u->chord_ms = a->ms;                            /* a new chord name squeezes in */
    return 1;
}

void qd_step_octave(qd_app_t *a, int d)
{
    qd_ui_t *u = &a->ui;
    int i;
    qd_step_edited(a);
    for (i = 0; i < u->nheld; i++) {
        const qd_step_t *st = STEP(a, u->held[i]);
        int top = st->note[0], k;
        if (qd_step_type(st) == QD_TRIG_NONE || !st->note[0])
            continue;
        for (k = 1; k < QD_NNOTE; k++)
            top = st->note[k] && st->note[0] + st->note[k] > top ? st->note[0] + st->note[k] : top;
        if (st->note[0] + 12 * d >= 1 && top + 12 * d <= 127) {
            qd_step_set_root(PAT(a), a->sel, u->held[i], st->note[0] + 12 * d);
            if (i == 0)
                PAT(a)->trk[a->sel].def_note = st->note[0];
        }
    }
    u->chord_ms = a->ms;
}

void qd_step_notes_in(qd_app_t *a, const uint8_t *notes, int n)
{
    qd_ui_t *u = &a->ui;
    int i;
    qd_step_edited(a);
    for (i = 0; i < u->nheld; i++)
        qd_step_set_notes(PAT(a), a->sel, u->held[i], notes, n);
    if (n > 0)
        PAT(a)->trk[a->sel].def_note = notes[0];
    u->chord_ms = a->ms;
}

unsigned qd_step_fields_set(const qd_app_t *a, int t, int s)
{
    const qd_pattern_t *p = &a->proj.pat[a->seq.cur];
    const qd_step_t *st = qd_step(p, t, s);
    const int ty = qd_step_type(st);
    unsigned m = 0;
    if (!ty)
        return 0;
    if (st->note[1] || st->note[2] || st->note[3])
        m |= 1u << UI_F_NOTE;                           /* a chord */
    if (qd_step_ramp(st) != QD_RAMP_FLAT)
        m |= 1u << UI_F_VEL;
    if (ty == QD_TRIG_TIE)
        m |= 1u << UI_F_LEN;
    if (st->micro || st->strum)
        m |= 1u << UI_F_MICRO;
    if (qd_step_ratchet(st) > 1)
        m |= 1u << UI_F_RATCH;
    if (st->cond)
        m |= 1u << UI_F_COND;
    if (st->chance < 100)
        m |= 1u << UI_F_CHNC;
    if (ty != QD_TRIG_NOTE)
        m |= 1u << UI_F_TRIG;
    if (st->sound)
        m |= 1u << UI_F_SOUND;
    if (qd_lock_count(p, t, s))
        m |= 1u << UI_F_LOCKS;
    return m;
}
