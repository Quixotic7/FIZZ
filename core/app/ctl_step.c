/* SPDX-License-Identifier: GPL-3.0-only */
/* The step view's edits (ctl.h; PLAN §8b): the white keys' tap and hold, the held steps, the hold modes' ten values and
 * CLEAR, ALGORITHM's fine value and the track's defaults, note entry, the sound pool's banks, the p-locks and the sound
 * lock of a held step.
 *
 * A WHITE KEY (step view, nothing held) goes down PENDING: its LED shows at once what a tap would make. Released under
 * 250 ms with nothing turned, it is a TAP: an empty step becomes a trig with no overrides (it plays the track's
 * defaults), a trig is removed with its locks. At 250 ms, or when a knob turns or a black key is pressed while it is
 * down, it is HELD: its step joins the held steps (the first held is the one shown; every edit goes to all). SHIFT + W
 * is free (PLAN question 38): with SHIFT held and nothing held a white key does nothing.
 *
 * THE TEN VALUES (B1..B10) of each mode with a step held (QD_VK below; §8b's table) set the value on every held step,
 * overridden; B11 clears the mode's override (the track's value again; LOCKS: the chosen lock removed). An empty step
 * given a value becomes a trig. ALGORITHM turns the value finely (every value, each held step from its own); with SHIFT
 * it turns MICRO's strum and RATCH's ramp (§8b has no key for them). PRESETS, in every mode, is the held steps' sound
 * lock: any sound of the pool, 0 = the track's own (the owner, 2026-10-18); SOUND's bank of ten follows it.
 *
 * NOTE entry is Form's "pressed" rule: the first note key down replaces the held steps' notes, keys down while another
 * is still down add to them (up to 5 since P1.3c, within +48 of the lowest); each key sounds its note on the track. The
 * ten keys are ten degrees of the track's scale from its root at the entry octave (OCT- / OCT+: the octave takeover),
 * a chromatic track ten semitones. ALGORITHM moves every note a scale step.
 *
 * P1.3c, NOTE's five knobs (§8b 2.1): SELECT = note 1, absolute: a detent moves it a degree of the track's scale (a
 * chromatic track: a semitone), notes 2..5 keep their offsets (the shape moves with it). KNOB 1..4 = notes 2..5, each an
 * offset above note 1: a detent moves it to the next offset whose note is in the scale (chromatic: every semitone),
 * skipping offsets another note has; below the first it is 0 (none), up to +48. Each change sounds the note (a
 * QD_PREVIEW_MS moment). In NOTE with a step held SELECT is no page and KNOB 1..4 no p-locks (P-LOCK does those).
 *
 * P1.3c, P-LOCK (B11 with nothing held): holding a step keeps the screen (the step view); the screen's lockable knobs
 * (qd_ctl_lock_screen: one table a screen, the step view's KNOB 1..4 FREQ DECAY HARM FDBK and PRESETS the sound lock;
 * the sound editor's pages will add theirs) p-lock the held steps. B11 clears the held steps' p-locks (the sound lock
 * stays: SOUND's B11 clears that), undoable with F1 + B11. B1..B10 do nothing. The other modes' KNOB 1..4 p-lock through
 * the same table (P1.3d: no more: KNOB 1..4 follow the mode's knob layout, below).
 *
 * P1.3d (§8b 2.2): the knobs' jobs (qd_step_job_held / qd_step_job_default, the table qd_ctl_knob_job): on the held
 * steps each from its own value, or on the track's defaults; each turn sets the band (tk, tk_job, tk_src). LOCKS is
 * gone (P-LOCK covers it). The step latch (LFO): qd_step_latch, qd_step_latch_seed; a W while it is on selects. */
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "ui.h"
#include "../seq/chord.h"
#include "../seq/scale.h"
#include "../seq/sounds.h"

static const uint8_t QUICK_ID[4] = {QD_QP_FREQ, QD_QP_EDEC, QD_QP_HARM, QD_QP_FDBK};   /* PLAN question 11 */
int qd_step_quick_id(int k) { return QUICK_ID[k & 3]; }

int qd_step_id_detent(int id)
{
    int16_t lo = 0, hi = 0;
    qd_lock_range(id, &lo, &hi);
    return hi - lo > 1000 ? 100 : 1;   /* the hi-res values (hundredths) move a whole unit a detent */
}
int qd_step_id_steps(int id)
{
    int16_t lo = 0, hi = 0;
    qd_lock_range(id, &lo, &hi);
    return (hi - lo) / qd_step_id_detent(id) + 1;
}

/* ---- the ten values of each mode (PLAN §8b's table) */
static const uint8_t VK_VEL[10] = {13, 25, 38, 51, 64, 76, 89, 102, 114, 127};
static const uint8_t VK_LEN[10] = {1, 2, 3, 4, 5, 6, 8, 10, 12, QD_LEN_INF};   /* 1/4 1/2 3/4 1 1.5 2 4 8 16 INF */
static const int8_t VK_MICRO[10] = {-20, -15, -10, -5, -2, 2, 5, 10, 15, 20};
static const uint8_t MODE_PM[UI_NFIELD] = {QD_PM_NOTE, QD_PM_VEL, QD_PM_LEN, QD_PM_MICRO, QD_PM_RATCH, QD_PM_CHANCE,
                                           QD_PM_COND, QD_PM_TRIG, QD_NPM, QD_PM_SOUND, QD_NPM};   /* (FUNC: no
                                                         * override bit, P1.3w) */

/* ---- P1.3y: FUNC's kinds (KNOB 1): none, RESTART, REVERSE, FORWARD, FLIP, RANDOM JUMP, RANDOM FUNCTION, LEAP, JUMP */
static int fn_kind(int f) { return QD_FN_IS_LEAP(f) ? 7 : f >= QD_FN_JUMP && f < QD_FN_LEAP ? 8 : f < 7 ? f : 0; }

/* ---- P-LOCK's hook: the screens' lockable knobs, one table a screen */
static const qd_lockknob_t STEPVIEW_LOCKS[] = {
    {QD_K_1, QD_QP_FREQ, UI_TK_FREQ}, {QD_K_2, QD_QP_EDEC, UI_TK_DECAY}, {QD_K_3, QD_QP_HARM, UI_TK_HARM},
    {QD_K_4, QD_QP_FDBK, UI_TK_FDBK}, {QD_K_PRESETS, QD_LOCK_SOUND, QD_TK_PRESET},
};
static const qd_lockscreen_t STEPVIEW = {"STEP VIEW", (int)(sizeof STEPVIEW_LOCKS / sizeof STEPVIEW_LOCKS[0]),
                                         STEPVIEW_LOCKS};
const qd_lockscreen_t *qd_ctl_lock_screen(const qd_app_t *a)
{
    if (qd_snd_open(a))                                 /* P1.4a: the sound editor's page */
        return qd_snd_lock_screen(a, a->ui.snd_page);
    return &STEPVIEW;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static qd_pattern_t *PAT(qd_app_t *a) { return qd_ctl_pat(a); }
static const qd_pattern_t *CPAT(const qd_app_t *a) { return qd_ctl_cpat(a); }
static int step_of(const qd_app_t *a, int w) { return a->ui.page[a->sel] * QD_PAGE + w; }
static void resolve(const qd_app_t *a, int s, qd_rstep_t *r) { qd_step_resolve(CPAT(a), a->sel, s, r); }

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
    if (!u->nheld) {                                    /* a new hold */
        qd_rstep_t r;
        resolve(a, s, &r);
        u->held_empty = !r.on;
        u->chord_ms = a->ms - 10000u;
        u->held_played = 0;
        u->note_keys = 0;
        u->entry_n = 0;
        u->sound_bank = r.sound > 0 ? (r.sound - 1) / 10 : 0;   /* SOUND: the bank holding the step's sound */
    }
    u->held[u->nheld] = s;
    u->held_key[u->nheld] = (int8_t)key;
    u->nheld++;
    if (a->sel >= 0 && a->sel < 8)
        u->last_step[a->sel] = s;                       /* (P1.3d: the step latch starts on the last one) */
}

static void held_remove(qd_ui_t *u, int i)
{
    for (; i + 1 < u->nheld; i++) {
        u->held[i] = u->held[i + 1];
        u->held_key[i] = u->held_key[i + 1];
    }
    u->nheld--;
}

void qd_step_release_all(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    u->nheld = 0;
    u->wk_down = u->wk_pending = u->wk_target = 0;
    u->note_keys = 0;
    u->scursor = 0;
    if (u->tk == QD_TK_HELDKNOB)                        /* (the band of a step no longer held) */
        u->tk = QD_TK_NONE;
}

/* P1.3d (§8b 2.2): the step latch (LFO). On, it is as if a step is always held: the held steps are a selection that
 * stays (held_key -1), a W tap selects that step (no trig made or removed), W keys down together select them all; it
 * starts on the last step held or tapped on the track (else the first step of the page edited) and moves the page
 * there. LFO again, HOME or leaving the step view end it */
void qd_step_latch_seed(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    int s = a->sel >= 0 && a->sel < 8 ? u->last_step[a->sel] : -1;
    if (s < 0 || s >= QD_NSTEP)
        s = u->page[a->sel] * QD_PAGE;
    u->nheld = 0;
    held_add(a, s, -1);
    qd_ctl_set_page(a, s / QD_PAGE);
}

/* P1.3q: PRESETS in the STEPS view (P1.3r: and EDIT; no finger on a step): a step cursor. Off, the step latch comes on
 * with its selection where it starts (qd_step_latch_seed); on, its one selection moves d steps through the pages in
 * order, looped or not (P1.3r), each over its start..end: past a page's end the next page's start, before its start
 * the previous page's end, page 8 on to page 1 and back; the page edited follows (the row unfolds). Off steps are
 * selected too: values then turn them on */
void qd_step_cursor(qd_app_t *a, int d)
{
    qd_ui_t *u = &a->ui;
    const qd_tset_t *ts = &PAT(a)->trk[a->sel];
    int s, k;
    if (!u->slatch || !u->nheld) {                      /* the latch on / its selection started */
        if (!u->slatch)
            qd_step_latch(a, 1);
        else
            qd_step_latch_seed(a);
        u->scursor = 1;
        if (u->nheld)
            qd_ctl_preview_once(a, u->held[0]);         /* P1.5s: the step it lands on, once */
        return;
    }
    s = u->held[0];
    for (k = 0; k < (d < 0 ? -d : d); k++) {           /* P1.3r: through every page in order, looped or not */
        int n = 0;
        do
            s = (s + (d < 0 ? -1 : 1) + QD_NSTEP) % QD_NSTEP;
        while (!qd_page_in_range(ts, s) && ++n < QD_NSTEP);
    }
    u->nheld = 0;
    if (u->tk == QD_TK_HELDKNOB)
        u->tk = QD_TK_NONE;
    held_add(a, s, -1);
    qd_ctl_set_page(a, s / QD_PAGE);
    u->scursor = 1;
    qd_ctl_preview_once(a, s);                          /* P1.5s: the step it moved to, once */
}

void qd_step_cursor_page(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    const qd_tset_t *ts = &PAT(a)->trk[a->sel];
    const int pg = u->page[a->sel];
    if (!u->slatch || !u->nheld || u->wk_down)
        return;
    u->nheld = 0;
    if (u->tk == QD_TK_HELDKNOB)
        u->tk = QD_TK_NONE;
    held_add(a, pg * QD_PAGE + (ts->pstart[pg] < QD_PAGE ? ts->pstart[pg] : 0), -1);
}

void qd_step_latch(qd_app_t *a, int on)
{
    qd_step_release_all(a);
    a->ui.slatch = on != 0;
    a->ui.scursor = 0;
    if (on)
        qd_step_latch_seed(a);
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
/* P1.3y: in FUNC, a W pressed while B9 (JUMP) is down makes its step the held (or selected) steps' JUMP target; it
 * is not held, not tapped, not selected (it works with the step latch). 1: taken */
static int jump_target(qd_app_t *a, int w)
{
    qd_ui_t *u = &a->ui;
    const int s = step_of(a, w);
    int i;
    if (u->hmode != UI_F_FUNC || !u->nheld || !u->fn_b9)
        return 0;
    for (i = 0; i < u->nheld; i++)
        qd_step_set_func(PAT(a), a->sel, u->held[i], QD_FN_MAKE_JUMP(s));
    u->fn_target = s;
    u->wk_target |= (uint16_t)(1u << w);
    return 1;
}

void qd_step_white_down(qd_app_t *a, int w)
{
    qd_ui_t *u = &a->ui;
    const uint16_t bit = (uint16_t)(1u << w);
    if (jump_target(a, w))
        return;
    if (qd_ctl_shift(a) && !u->nheld && !u->wk_pending)
        return;                                         /* SHIFT + W: free (question 38) */
    if (u->slatch) {                                    /* the step latch (P1.3f): a gesture's keys replace the
                                                         * selection (shown as they go down) */
        const int s = step_of(a, w);
        u->scursor = 0;                                 /* (P1.3q: a W selection: the held screen as before) */
        if (!u->wk_down) {                              /* a gesture begins */
            u->gest_sole = u->nheld == 1 ? u->held[0] : -1;
            u->gest_n = 0;
            u->nheld = 0;
        }
        if (held_find(u, s) < 0)
            held_add(a, s, -1);
        u->gest_n++;
        u->wk_t0[w] = a->ms;
        u->wk_down |= bit;
        return;
    }
    u->wk_down |= bit;
    if (u->nheld) {                                     /* steps held: another joins them at once */
        u->wk_pending |= bit;
        u->wk_t0[w] = a->ms;
        u->wk_seq[w] = ++u->wk_count;
        qd_step_promote(a, w);
        return;
    }
    u->wk_pending |= bit;
    u->wk_t0[w] = a->ms;
    u->wk_seq[w] = ++u->wk_count;
}

/* a tap: empty -> a trig with no overrides (the track's defaults), a trig -> empty (with its locks) */
static void tap(qd_app_t *a, int w)
{
    qd_pattern_t *p = PAT(a);
    const int s = step_of(a, w);
    if (qd_step_is_on(qd_step(p, a->sel, s)))
        qd_step_clear(p, a->sel, s);
    else
        qd_step_on(p, a->sel, s);
    a->ui.last_step[a->sel] = s;
}

void qd_step_white_up(qd_app_t *a, int w)
{
    qd_ui_t *u = &a->ui;
    const uint16_t bit = (uint16_t)(1u << w);
    int i;
    if (u->wk_target & bit) {                           /* (P1.3w: a jump's target: nothing more) */
        u->wk_target &= (uint16_t)~bit;
        return;
    }
    if (!(u->wk_down & bit))
        return;
    u->wk_down &= (uint16_t)~bit;
    if (u->slatch) {                                    /* the step latch: the gesture's set stays selected, but
                                                         * one tap on the step that was the whole selection empties it */
        if (!u->wk_down && u->gest_n == 1 && u->nheld == 1 && u->held[0] == u->gest_sole &&
            a->ms - u->wk_t0[w] < QD_TAP_MS) {
            u->nheld = 0;
            u->note_keys = 0;
            if (u->tk == QD_TK_HELDKNOB)
                u->tk = QD_TK_NONE;
        }
        return;
    }
    if ((u->wk_pending & bit) && a->ms - u->wk_t0[w] < QD_TAP_MS && !u->nheld) {
        u->wk_pending &= (uint16_t)~bit;
        tap(a, w);
        return;
    }
    qd_step_promote(a, w);                              /* (250 ms reached at this very release) */
    for (i = 0; i < u->nheld; i++)
        if (u->held_key[i] == w) {
            held_remove(u, i);
            break;
        }
    if (!u->nheld) {
        u->note_keys = 0;
        if (u->tk == QD_TK_HELDKNOB)
            u->tk = QD_TK_NONE;
    }
}

/* ---------------------------------------------------------------- the key, the scale --- */
static void key_of(const qd_app_t *a, int *scale, int *root)
{
    qd_track_key(&a->proj, CPAT(a), a->sel, scale, root);
}

int qd_step_entry_note(const qd_app_t *a, int b)
{
    int sc, rt;
    key_of(a, &sc, &rt);
    return qd_scale_note(sc, rt, a->ui.entry_oct, b);
}

int qd_step_sound_bank(const qd_app_t *a) { return a->ui.sound_bank; }

/* ---------------------------------------------------------------- the ten values --- */
/* one held step: B(b+1) of mode m */
static void value_set(qd_app_t *a, int s, int m, int b)
{
    qd_pattern_t *p = PAT(a);
    const int t = a->sel;
    qd_rstep_t r;
    qd_step_resolve(p, t, s, &r);
    switch (m) {
    case UI_F_VEL: qd_step_set_vel(p, t, s, VK_VEL[b]); break;
    case UI_F_LEN: qd_step_set_len(p, t, s, VK_LEN[b]); break;
    case UI_F_MICRO: qd_step_set_micro(p, t, s, VK_MICRO[b]); break;
    case UI_F_RATCH:
        if (b < 8)
            qd_step_set_ratchet(p, t, s, b + 1, r.ramp);
        else if (b == 8)                                /* B9: the ramp, cycling */
            qd_step_set_ratchet(p, t, s, r.ratch, (r.ramp + 1) % 4);
        break;
    case UI_F_CHNC: qd_step_set_chance(p, t, s, (b + 1) * 10); break;
    case UI_F_COND: {
        int ca = 1, cb = 1;                             /* (ALWAYS is 1:1) */
        if (!qd_cond_ab_of(r.cond, &ca, &cb))
            ca = cb = 1;
        if (b == 0) {
            qd_step_set_cond(p, t, s, QD_COND_FILL);
            break;
        }
        if (b == 1) {
            qd_step_set_cond(p, t, s, QD_COND_NFILL);
            break;
        }
        if (b < 6) {                                    /* A = 1..4: B kept, at least A (Form's) */
            ca = b - 1;
            cb = cb < ca ? ca : cb;
        } else {                                        /* B = 1..4: A kept, at most B */
            cb = b - 5;
            ca = ca > cb ? cb : ca;
        }
        qd_step_set_cond(p, t, s, cb < 2 ? QD_COND_ALWAYS : qd_cond_ab(ca, cb));
        break;
    }
    case UI_F_TRIG:
        if (b < 3)
            qd_step_set_trig(p, t, s, QD_TRIG_NOTE + b);
        break;
    case UI_F_FUNC:                                     /* P1.3w: B1 none, B2 RESTART .. B7 RANDOM FUNCTION; P1.3y: B8
                                                         * LEAP (the last n), B9 JUMP (the last target) */
        if (b < QD_FN_JUMP)
            qd_step_set_func(p, t, s, b);
        else if (b == 7)
            qd_step_set_func(p, t, s, QD_FN_MAKE_LEAP(a->ui.fn_leap));
        else if (b == 8)
            qd_step_set_func(p, t, s, QD_FN_MAKE_JUMP(a->ui.fn_target >= 0 ? a->ui.fn_target : s / QD_PAGE * QD_PAGE));
        break;
    case UI_F_SOUND: {
        const int k = a->ui.sound_bank * 10 + b + 1;
        if (k <= a->proj.nsound)
            qd_step_set_sound(p, t, s, k);
        break;
    }
    default:
        break;
    }
}

/* the notes entered so far into every held step */
static void entry_notes(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    int i;
    for (i = 0; i < u->nheld; i++)
        qd_step_set_notes(PAT(a), a->sel, u->held[i], u->entry_chord, u->entry_n);
    u->chord_ms = a->ms;
}

static void chord_add(uint8_t *chord, int *n, int note)
{
    int i;
    if (*n >= QD_NNOTE)
        return;
    for (i = 0; i < *n; i++)
        if (chord[i] == note || note - chord[i] > 48 || chord[i] - note > 48)
            return;
    chord[(*n)++] = (uint8_t)note;
}

void qd_step_value_key(qd_app_t *a, int b, int down)
{
    qd_ui_t *u = &a->ui;
    const int m = u->hmode;
    int i;
    if (!down) {
        u->note_keys &= (uint16_t)~(1u << b);
        if (b == 8)
            u->fn_b9 = 0;                               /* (P1.3y: JUMP's key let go) */
        return;
    }
    qd_step_promote_all(a);
    if (!u->nheld)
        return;
    if (m == UI_F_FUNC && b == 8)
        u->fn_b9 = 1;                                   /* P1.3y: B9 down: a W pressed now is the jump's target */
    if (m == UI_F_PLOCK) {                              /* P-LOCK: B11 clears the p-locks; B1..B10 nothing */
        if (b == 10)
            qd_step_clear_locks(a);
        return;
    }
    if (b == 10) {                                      /* B11: the mode back to the track's */
        for (i = 0; i < u->nheld; i++) {
            if (m == UI_F_FUNC) {                       /* (P1.3w: no default: none) */
                qd_step_set_func(PAT(a), a->sel, u->held[i], QD_FN_NONE);
                continue;
            }
            qd_step_reset(PAT(a), a->sel, u->held[i], MODE_PM[m]);
            if (m == UI_F_MICRO)
                qd_step_reset(PAT(a), a->sel, u->held[i], QD_PM_STRUM);
        }
        u->chord_ms = a->ms;
        return;
    }
    if (m == UI_F_NOTE) {                               /* Form's "pressed": replace, then add while held */
        const int note = qd_step_entry_note(a, b);
        if (!u->note_keys)
            u->entry_n = 0;
        u->note_keys |= (uint16_t)(1u << b);
        chord_add(u->entry_chord, &u->entry_n, note);
        entry_notes(a);
        if (!qd_ctl_previewing(a))                      /* (stopped, the step's preview sounds the chord) */
            qd_app_note_on(a, QD_BLACK_KEY[b], note, a->key_vel[QD_BLACK_KEY[b]] ? a->key_vel[QD_BLACK_KEY[b]] : 100);
        return;
    }
    for (i = 0; i < u->nheld; i++)
        value_set(a, u->held[i], m, b);
    if (m == UI_F_SOUND) {
        u->roll_ms = a->ms;
        u->roll_dir = 1;
    }
}

/* ---------------------------------------------------------------- the sound lock --- */
void qd_step_sound_pick(qd_app_t *a, int d)
{
    qd_ui_t *u = &a->ui;
    qd_rstep_t r;
    int i;
    for (i = 0; i < u->nheld; i++) {
        resolve(a, u->held[i], &r);
        qd_step_set_sound(PAT(a), a->sel, u->held[i], qd_sound_step(&a->proj, r.sound, d, 0));   /* (P1.5a: the pool's
                                                         * holes skipped) */
    }
    resolve(a, u->held[0], &r);
    if (r.sound > 0)
        u->sound_bank = (r.sound - 1) / 10;             /* the keys' bank follows */
    u->roll_ms = a->ms;
    u->roll_dir = d > 0 ? 1 : -1;
}

/* ---------------------------------------------------------------- the knobs with steps held --- */
/* the value a lock on step s starts from: the step's sound lock's (a pool sound), else the track's sound's */
int qd_step_base(qd_app_t *a, int t, int s, int id)
{
    qd_rstep_t r;
    if (id >= QD_QUAD_NP) {
        const qd_mix_track_t *m = &a->model.t[t].mix;
        return id == QD_LK_PAN ? m->pan : id == QD_LK_DELAY ? m->delay : id == QD_LK_REVERB ? m->reverb
             : id == QD_LK_CHORUS ? m->chorus : id == QD_LK_LEVEL ? m->level : 0;
    }
    qd_step_resolve(CPAT(a), t, s, &r);
    if (r.sound)
        return qd_sound_patch(&a->proj, r.sound)[id];
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
        const int base = qd_lock_get(CPAT(a), a->sel, s, id, &v) ? v : qd_step_base(a, a->sel, s, id);
        if (qd_lock_set(PAT(a), a->sel, s, id, base + qd_ctl_acc(a, d, qd_step_id_steps(id)) * qd_step_id_detent(id)) ==
            QD_EFULL)
            ok = 0;
    }
    if (!ok)
        qd_ctl_confirm(a, "NO ROOM", "for locks: the pattern has 1024");
    return ok;
}

void qd_step_lock_fine(qd_app_t *a, int id, int d)
{
    qd_ui_t *u = &a->ui;
    int i;
    for (i = 0; i < u->nheld; i++) {
        int16_t v;
        const int s = u->held[i];
        const int base = qd_lock_get(CPAT(a), a->sel, s, id, &v) ? v : qd_step_base(a, a->sel, s, id);
        if (qd_lock_set(PAT(a), a->sel, s, id, base + d) == QD_EFULL) {
            qd_ctl_confirm(a, "NO ROOM", "for locks: the pattern has 1024");
            return;
        }
    }
}

/* note moved d steps of the scale (a detent a step) */
static int scale_steps(int sc, int rt, int note, int d)
{
    for (; d > 0; d--)
        note = qd_scale_step(sc, rt, note, 1);
    for (; d < 0; d++)
        note = qd_scale_step(sc, rt, note, -1);
    return note;
}

/* P1.3f: the notes are pitches in slots (note k = slot k). Every note a scale step (d detents) in its own slot; two
 * that land on one pitch (out-of-scale notes snapping): the later slot emptied */
static void move_all(const qd_app_t *a, const uint8_t *in, uint8_t *out, int d)
{
    int sc, rt, k, j;
    key_of(a, &sc, &rt);
    for (k = 0; k < QD_NNOTE; k++) {
        out[k] = in[k] ? (uint8_t)clampi(scale_steps(sc, rt, in[k], d), 1, 127) : 0;
        for (j = 0; j < k; j++)
            if (out[k] && out[j] == out[k])
                out[k] = 0;
    }
}

/* P1.5o: the whole chord d scale steps, its shape kept: every note moves by one step at a time, and it stops (the
 * steps left undone) when the top or the bottom note reaches the end of the range (1..127): it never squashes */
static void chord_move(const qd_app_t *a, const uint8_t *in, uint8_t *out, int d)
{
    int sc, rt, k, dir = d > 0 ? 1 : -1;
    key_of(a, &sc, &rt);
    memcpy(out, in, QD_NNOTE);
    for (; d; d -= dir) {
        uint8_t nx[QD_NNOTE];
        int ok = 1;
        for (k = 0; k < QD_NNOTE; k++) {
            int x;
            if (!out[k]) {
                nx[k] = 0;
                continue;
            }
            x = qd_scale_step(sc, rt, out[k], dir);
            if (x == out[k] || x < 1 || x > 127)
                ok = 0;
            nx[k] = (uint8_t)(x < 1 ? 1 : x > 127 ? 127 : x);
        }
        if (!ok)
            break;
        memcpy(out, nx, QD_NNOTE);
    }
}

/* P1.3f: note k alone a scale step (dir), skipping the pitches the other slots have. Note 1 stays within 1..127. Notes
 * 2..6 live above note 1: turned down past it they are removed (0); from none, turned up, they come in a scale step
 * above note 1 (the lowest note when note 1 is empty) */
static int pitch_step(const qd_app_t *a, const uint8_t *note, int k, int dir)
{
    int sc, rt, p = note[k], j, floor = note[0];
    key_of(a, &sc, &rt);
    if (!floor)
        for (j = 1, floor = 128; j < QD_NNOTE; j++)
            floor = j != k && note[j] && note[j] < floor ? note[j] : floor;
    if (k > 0 && !p) {
        if (dir < 0 || floor > 127)
            return 0;
        p = floor;
    }
    for (;;) {
        int taken = 0;
        const int x = qd_scale_step(sc, rt, p, dir);
        if (x == p)
            return note[k];                             /* the end of the range: it stays */
        p = x;
        if (k > 0 && p < floor)
            return 0;                                   /* below note 1: none */
        for (j = 0; j < QD_NNOTE; j++)
            taken |= j != k && note[j] == p;
        if (!taken)
            return p;
    }
}

static void chord_of(const qd_app_t *a, int s, char *name)   /* its name as shown (for the squeeze) */
{
    qd_rstep_t r;
    char rt[4], q[8];
    resolve(a, s, &r);
    name[0] = 0;
    if (r.nn && qd_chord_name(r.notes, r.nn, rt, q)) {
        strcpy(name, rt);
        strcat(name, q);
    }
}

/* ALGORITHM, a step held: the mode's value finely, each held step from its own (SHIFT: strum, ramp) */
static void fine(qd_app_t *a, int s, int m, int d, int shift)
{
    qd_pattern_t *p = PAT(a);
    const int t = a->sel;
    qd_rstep_t r;
    qd_step_resolve(p, t, s, &r);
    switch (m) {
    case UI_F_NOTE: {                                   /* every note a scale step, each in its slot */
        uint8_t n[QD_NNOTE];
        move_all(a, r.note, n, d);
        qd_step_set_slots(p, t, s, n);
        break;
    }
    case UI_F_VEL: qd_step_set_vel(p, t, s, clampi(r.vel + d, 1, 127)); break;
    case UI_F_LEN: qd_step_set_len(p, t, s, clampi(r.len + d, 0, QD_LEN_INF)); break;
    case UI_F_MICRO:
        if (shift)
            qd_step_set_strum(p, t, s, clampi(r.strum + d, 0, 23));
        else
            qd_step_set_micro(p, t, s, clampi(r.micro + d, -23, 23));
        break;
    case UI_F_RATCH:
        if (shift)
            qd_step_set_ratchet(p, t, s, r.ratch, clampi(r.ramp + d, 0, QD_RAMP_ACCENT));
        else
            qd_step_set_ratchet(p, t, s, clampi(r.ratch + d, 1, 8), r.ramp);
        break;
    case UI_F_CHNC: qd_step_set_chance(p, t, s, clampi(r.chance + d, 0, 100)); break;
    case UI_F_COND: qd_step_set_cond(p, t, s, clampi(r.cond + d, 0, QD_NCOND - 1)); break;
    case UI_F_TRIG: qd_step_set_trig(p, t, s, clampi((r.on ? r.type : QD_TRIG_NOTE) + d, QD_TRIG_NOTE, QD_TRIG_TIE)); break;
    case UI_F_FUNC: {                                   /* P1.3y: the kind (ALGORITHM, a finger on the step) */
        const int k = clampi(fn_kind(r.func) + d, 0, 8);
        qd_step_set_func(p, t, s, k < 7 ? k : k == 7 ? QD_FN_MAKE_LEAP(a->ui.fn_leap)
                                          : QD_FN_MAKE_JUMP(a->ui.fn_target >= 0 ? a->ui.fn_target : s / QD_PAGE * QD_PAGE));
        break;
    }
    default: break;
    }
}

/* a lockable knob of the screen (P-LOCK, and KNOB 1..4 in the other modes): the held steps' lock; 1: taken */
int qd_step_lock_knob(qd_app_t *a, int knob, int d)
{
    qd_ui_t *u = &a->ui;
    const qd_lockscreen_t *ls = qd_ctl_lock_screen(a);
    int i;
    for (i = 0; i < ls->n; i++) {
        const qd_lockknob_t *k = &ls->k[i];
        if (k->knob != knob)
            continue;
        if (k->id == QD_LOCK_SOUND) {
            qd_step_sound_pick(a, d);
            qd_ctl_takeover(a, QD_TK_PRESET, 0, d, 1, u->held[0]);   /* (P1.5v: every mode, up while held) */
            u->tk_hold = 1;
        } else {
            lock_all(a, k->id, d);
            if (k->tk != QD_TK_NONE)                    /* (P1.4a: the sound editor's pages show it themselves) */
                qd_ctl_takeover(a, k->tk, knob >= QD_K_1 ? knob - QD_K_1 : 0, d, 1, u->held[0]);
        }
        return 1;
    }
    return 0;
}

void qd_step_clear_locks(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    int i, n = 0;
    for (i = 0; i < u->nheld; i++)
        n += qd_lock_count(CPAT(a), a->sel, u->held[i]);
    if (!n)
        return;
    qd_clip_keep(&u->clip, CPAT(a), a->seq.cur, a->sel);
    for (i = 0; i < u->nheld; i++) {
        int k;
        const qd_lock_t *l = qd_locks(CPAT(a), a->sel, u->held[i], &k);
        while (k > 0) {
            qd_lock_clear(PAT(a), a->sel, u->held[i], l[0].id);
            l = qd_locks(CPAT(a), a->sel, u->held[i], &k);
        }
    }
    qd_ctl_confirm(a, "LOCKS CLEARED", "F1 + B11 undoes it");
}

/* the band (P1.3d) or the default's picture (P1.3e): what a knob turned, for a second over the screen. QUIET (P1.3e):
 * none, and a band or picture showing goes, so the change is seen in place */
static void band(qd_app_t *a, int tk, int job, int knob, int d)
{
    if (job == QD_KJ_QUIET) {
        if (a->ui.tk == QD_TK_HELDKNOB || a->ui.tk == QD_TK_DEFAULT)
            a->ui.tk = QD_TK_NONE;
        return;
    }
    qd_ctl_takeover(a, tk, 0, d, 0, a->ui.nheld ? a->ui.held[0] : -1);
    a->ui.tk_job = job;
    a->ui.tk_src = knob;
}

/* NOTE's knobs on the held steps (P1.3f): SELECT note 1, KNOB 1..4 notes 2..5, each moving its own note alone (a
 * scale step a detent, pitch_step); the note sounds */
static void note_knob(qd_app_t *a, int k, int d)
{
    qd_ui_t *u = &a->ui;
    int i, n, sound = -1;
    char before[16], after[16];
    chord_of(a, u->held[0], before);
    for (i = 0; i < u->nheld; i++) {
        const int s = u->held[i];
        qd_rstep_t r;
        resolve(a, s, &r);
        for (n = d; n > 0; n--)
            r.note[k] = (uint8_t)pitch_step(a, r.note, k, 1);
        for (n = d; n < 0; n++)
            r.note[k] = (uint8_t)pitch_step(a, r.note, k, -1);
        qd_step_set_slots(PAT(a), a->sel, s, r.note);
        if (!i)
            sound = r.note[k] ? r.note[k] : -1;
    }
    chord_of(a, u->held[0], after);
    u->chord_ms = strcmp(before, after) != 0 ? a->ms : u->chord_ms;
    if (sound > 0 && !qd_ctl_previewing(a)) {           /* (stopped, the step's preview sounds the new chord) */
        qd_rstep_t r;
        resolve(a, u->held[0], &r);
        qd_ctl_preview(a, sound, r.vel);
    }
}

/* P1.3d (§8b 2.2): a knob's job on the held steps, each from its own value */
void qd_step_job_held(qd_app_t *a, int job, int knob, int d)
{
    qd_ui_t *u = &a->ui;
    qd_pattern_t *p = PAT(a);
    const int t = a->sel, m = u->hmode;
    char before[16], after[16];
    int i;
    if (!u->nheld)
        return;
    switch (job) {
    case QD_KJ_NOTE1:
    case QD_KJ_NOTE2:
    case QD_KJ_NOTE3:
    case QD_KJ_NOTE4:
    case QD_KJ_NOTE5:
        note_knob(a, job - QD_KJ_NOTE1, d);
        break;
    case QD_KJ_CHORD: {                                 /* P1.5o: SELECT: the held steps' chords, their shapes kept */
        int k1 = -1;
        for (i = 0; i < u->nheld; i++) {
            qd_rstep_t r;
            uint8_t nt[QD_NNOTE];
            resolve(a, u->held[i], &r);
            chord_move(a, r.note, nt, d);
            qd_step_set_slots(p, t, u->held[i], nt);
            if (!i)
                k1 = nt[0];
        }
        u->chord_ms = a->ms;
        if (k1 > 0 && !qd_ctl_previewing(a)) {
            qd_rstep_t r;
            resolve(a, u->held[0], &r);
            qd_ctl_preview(a, k1, r.vel);
        }
        break;
    }
    case QD_KJ_MAIN:
    case QD_KJ_QUIET:
        if (m == UI_F_SOUND) {                          /* the pool sound (as PRESETS) */
            qd_step_sound_pick(a, d);
            break;
        }
        chord_of(a, u->held[0], before);
        if (m == UI_F_VEL || m == UI_F_CHNC)            /* (P1.5b: the wide ones accelerate) */
            d = qd_ctl_acc(a, d, m == UI_F_VEL ? 127 : 101);
        for (i = 0; i < u->nheld; i++)
            fine(a, u->held[i], m, d, 0);
        chord_of(a, u->held[0], after);
        if (strcmp(before, after) != 0)
            u->chord_ms = a->ms;
        break;
    case QD_KJ_FTYPE:                                   /* P1.3y: FUNC's kind */
        for (i = 0; i < u->nheld; i++)
            fine(a, u->held[i], UI_F_FUNC, d, 0);
        break;
    case QD_KJ_LEAP: {                                  /* LEAP's n (-15..-1, 1..15): the function a LEAP */
        qd_rstep_t r;
        int n;
        resolve(a, u->held[0], &r);
        n = QD_FN_IS_LEAP(r.func) ? QD_FN_LEAP_N(r.func) : u->fn_leap;
        if (QD_FN_IS_LEAP(r.func) || d < 0)             /* (the first detent makes it a LEAP of the last n) */
            for (int k = 0; k < (d < 0 ? -d : d); k++) {   /* a detent a step, 0 skipped */
                n += d < 0 ? -1 : 1;
                if (n == 0)
                    n = d < 0 ? -1 : 1;
            }
        n = clampi(n, -15, 15);
        u->fn_leap = n;
        for (i = 0; i < u->nheld; i++)
            qd_step_set_func(p, t, u->held[i], QD_FN_MAKE_LEAP(n));
        break;
    }
    case QD_KJ_JTGT: {                                  /* JUMP's target (1..128): the function a JUMP */
        qd_rstep_t r;
        int x;
        resolve(a, u->held[0], &r);
        x = r.func >= QD_FN_JUMP && r.func < QD_FN_LEAP ? r.func - QD_FN_JUMP
          : u->fn_target >= 0 ? u->fn_target : u->held[0] / QD_PAGE * QD_PAGE;
        x = clampi(x + qd_ctl_acc(a, d, QD_NSTEP), 0, QD_NSTEP - 1);
        u->fn_target = x;
        for (i = 0; i < u->nheld; i++)
            qd_step_set_func(p, t, u->held[i], QD_FN_MAKE_JUMP(x));
        break;
    }
    default:
        for (i = 0; i < u->nheld; i++) {
            const int s = u->held[i];
            qd_rstep_t r;
            resolve(a, s, &r);
            switch (job) {
            case QD_KJ_TIE: qd_step_set_trig(p, t, s, d > 0 ? QD_TRIG_TIE : QD_TRIG_NOTE); break;
            case QD_KJ_STRUM: qd_step_set_strum(p, t, s, clampi(r.strum + d, 0, 23)); break;
            case QD_KJ_RAMP: qd_step_set_ratchet(p, t, s, r.ratch, clampi(r.ramp + d, 0, QD_RAMP_ACCENT)); break;
            case QD_KJ_COND: qd_step_set_cond(p, t, s, clampi(r.cond + d, 0, QD_NCOND - 1)); break;
            case QD_KJ_CHANCE: qd_step_set_chance(p, t, s, clampi(r.chance + qd_ctl_acc(a, d, 101), 0, 100)); break;
            default: return;
            }
        }
        break;
    }
    band(a, QD_TK_HELDKNOB, job, knob, d);
}

/* P1.3d: a knob's job on the track's defaults (nothing held): every step that does not override it follows */
void qd_step_job_default(qd_app_t *a, int job, int knob, int d)
{
    qd_pattern_t *p = PAT(a);
    const int t = a->sel, m = a->ui.hmode;
    qd_sdef_t *df = qd_tdef(p, t);
    int n;
    switch (job) {
    case QD_KJ_MAIN:
    case QD_KJ_QUIET:
        switch (m) {
        case UI_F_NOTE: {                               /* every note a scale step (the owner keeps this, P1.3e) */
            uint8_t nt[QD_NNOTE];
            move_all(a, df->note, nt, d);
            qd_tdef_set_slots(p, t, nt);
            a->ui.chord_ms = a->ms;
            break;
        }
        case UI_F_VEL: qd_tdef_set(p, t, QD_PM_VEL, clampi(df->vel + qd_ctl_acc(a, d, 127), 1, 127)); break;
        case UI_F_LEN: qd_tdef_set(p, t, QD_PM_LEN, clampi(df->len + d, 0, QD_LEN_INF)); break;
        case UI_F_MICRO: qd_tdef_set(p, t, QD_PM_MICRO, clampi(df->micro + d, -23, 23)); break;
        case UI_F_RATCH:
            qd_tdef_set(p, t, QD_PM_RATCH, qd_ratch_make(clampi(qd_ratch_count(df->ratch) + d, 1, 8),
                                                         qd_ratch_ramp(df->ratch)));
            break;
        case UI_F_CHNC: qd_tdef_set(p, t, QD_PM_CHANCE, clampi(df->chance + qd_ctl_acc(a, d, 101), 0, 100)); break;
        case UI_F_COND: qd_tdef_set(p, t, QD_PM_COND, clampi(df->cond + d, 0, QD_NCOND - 1)); break;
        case UI_F_TRIG: qd_tdef_set(p, t, QD_PM_TRIG, clampi(df->type + d, QD_TRIG_NOTE, QD_TRIG_TIE)); break;
        case UI_F_SOUND: {                              /* the track's own sound, from the pool */
            qd_track_t *tr = &a->model.t[t];
            int k0 = tr->sound, k;
            if (!a->proj.nsound)
                return;                                 /* (P1.5t: the pool empty: nothing) */
            if (!k0)                                    /* (P1.5t: not from the pool: from the pool's sound of its
                                                         * preset) */
                for (k = 1; k <= a->proj.nsound && !k0; k++)
                    if (qd_sound_used(&a->proj, k) && tr->preset && a->proj.sound[k - 1].preset == tr->preset)
                        k0 = k;
            k = qd_sound_step(&a->proj, k0 ? k0 : 1, k0 ? d : d - (d > 0), 1);
            qd_track_load_sound(tr, k, qd_sound_name(&a->proj, k), qd_sound_patch(&a->proj, k));
            tr->preset = a->proj.sound[k - 1].preset;
            break;
        }
        default:
            return;
        }
        break;
    case QD_KJ_CHORD: {                                 /* P1.5o: the default chord, its shape kept */
        uint8_t nt[QD_NNOTE];
        chord_move(a, df->note, nt, d);
        qd_tdef_set_slots(p, t, nt);
        a->ui.chord_ms = a->ms;
        break;
    }
    case QD_KJ_NOTE1:                                   /* P1.3f: the default chord's notes 1..5, each alone */
    case QD_KJ_NOTE2:
    case QD_KJ_NOTE3:
    case QD_KJ_NOTE4:
    case QD_KJ_NOTE5: {
        const int k = job - QD_KJ_NOTE1;
        uint8_t nt[QD_NNOTE];
        memcpy(nt, df->note, sizeof nt);
        for (n = d; n > 0; n--)
            nt[k] = (uint8_t)pitch_step(a, nt, k, 1);
        for (n = d; n < 0; n++)
            nt[k] = (uint8_t)pitch_step(a, nt, k, -1);
        qd_tdef_set_slots(p, t, nt);
        a->ui.chord_ms = a->ms;
        break;
    }
    case QD_KJ_TIE: qd_tdef_set(p, t, QD_PM_TRIG, d > 0 ? QD_TRIG_TIE : QD_TRIG_NOTE); break;
    case QD_KJ_STRUM: qd_tdef_set(p, t, QD_PM_STRUM, clampi(df->strum + d, 0, 23)); break;
    case QD_KJ_RAMP:
        qd_tdef_set(p, t, QD_PM_RATCH, qd_ratch_make(qd_ratch_count(df->ratch),
                                                     clampi(qd_ratch_ramp(df->ratch) + d, 0, QD_RAMP_ACCENT)));
        break;
    case QD_KJ_COND: qd_tdef_set(p, t, QD_PM_COND, clampi(df->cond + d, 0, QD_NCOND - 1)); break;
    case QD_KJ_CHANCE: qd_tdef_set(p, t, QD_PM_CHANCE, clampi(df->chance + qd_ctl_acc(a, d, 101), 0, 100)); break;
    default:
        return;
    }
    band(a, QD_TK_DEFAULT, job, knob, d);
}

/* a knob with steps held (P1.3d, §8b 2.2): PRESETS the sound lock; ALGORITHM the mode's value finely (SHIFT: MICRO's
 * strum, RATCH's ramp); SELECT and KNOB 1..4 by the mode's knob layout (qd_ctl_knob_job). P-LOCK: SELECT the page,
 * ALGORITHM nothing, the others the screen's lockable knobs */
int qd_step_knob(qd_app_t *a, int knob, int d)
{
    qd_ui_t *u = &a->ui;
    const int m = u->hmode;
    int i, job;
    char before[16], after[16];
    qd_step_promote_all(a);
    if (!u->nheld)
        return 0;
    if (m == UI_F_PLOCK || (m == UI_F_FUNC && knob == QD_K_SELECT)) {   /* (P1.3w: FUNC's SELECT the page too) */
        if (knob == QD_K_SELECT) {
            qd_ctl_set_page(a, u->page[a->sel] + d);
            if (!QD_VIEW_ROWS(u->view))                 /* (P1.3g: STEPS / EDIT show the pages: no picker) */
                qd_ctl_takeover(a, UI_TK_PAGE, 0, d, 0, -1);
        } else if (knob != QD_K_ALGO) {
            qd_step_lock_knob(a, knob, d);
        }
        return 1;
    }
    switch (knob) {
    case QD_K_PRESETS:                                  /* the sound lock, any pool sound, in every mode: P1.5v: the
                                                         * pool picker, up until the step is let go */
        qd_step_sound_pick(a, d);
        qd_ctl_takeover(a, QD_TK_PRESET, 0, d, 1, u->held[0]);
        u->tk_hold = 1;
        return 1;
    case QD_K_ALGO:
        if (m == UI_F_NOTE)                             /* P1.5o: NOTE: SELECT moves the chord, ALGORITHM no note (the
                                                         * cursor, its layout's, over the latch's selection) */
            return 1;
        if (m == UI_F_SOUND) {                          /* SOUND: as PRESETS */
            qd_step_sound_pick(a, d);
            band(a, QD_TK_HELDKNOB, QD_KJ_MAIN, knob, d);
            return 1;
        }
        chord_of(a, u->held[0], before);
        for (i = 0; i < u->nheld; i++)
            fine(a, u->held[i], m, d, qd_ctl_shift(a));
        chord_of(a, u->held[0], after);
        if (strcmp(before, after) != 0 || m == UI_F_NOTE)
            u->chord_ms = a->ms;                        /* a new chord name squeezes in */
        job = !qd_ctl_shift(a) ? QD_KJ_MAIN : m == UI_F_MICRO ? QD_KJ_STRUM : m == UI_F_RATCH ? QD_KJ_RAMP : QD_KJ_MAIN;
        band(a, QD_TK_HELDKNOB, job, knob, d);
        return 1;
    default:                                            /* SELECT, KNOB 1..4: the mode's layout */
        job = qd_ctl_knob_job(m, knob);
        if (job != QD_KJ_NONE)
            qd_step_job_held(a, job, knob, d);
        return 1;
    }
}

void qd_step_oct(qd_app_t *a, int d)
{
    qd_ui_t *u = &a->ui;
    if (!u->nheld)
        return;
    if (u->hmode == UI_F_NOTE) {
        const int from = u->entry_oct;
        u->entry_oct = clampi(u->entry_oct + d, -1, 8);
        qd_ctl_octave(a, UI_OCT_NOTE, from, u->entry_oct);
    } else if (u->hmode == UI_F_SOUND) {
        const int last = (a->proj.nsound - 1) / 10, from = u->sound_bank;
        u->sound_bank = clampi(u->sound_bank + d, 0, last);
        u->roll_ms = a->ms;
        u->roll_dir = d;
        qd_ctl_octave(a, UI_OCT_BANK, from, u->sound_bank);
    }
}

void qd_step_notes_in(qd_app_t *a, const uint8_t *notes, int n)
{
    qd_ui_t *u = &a->ui;
    int i;
    for (i = 0; i < u->nheld; i++)
        qd_step_set_notes(PAT(a), a->sel, u->held[i], notes, n);
    u->chord_ms = a->ms;
}

/* ---------------------------------------------------------------- the track's defaults (nothing held) --- */
/* P1.3e: a mode's default against the initial one (qd_sdef_init), and back to it */
int qd_step_default_initial(const qd_app_t *a, int m)
{
    const qd_sdef_t *d = &CPAT(a)->trk[a->sel].def;
    qd_sdef_t z;
    qd_sdef_init(&z);
    switch (m) {
    case UI_F_NOTE: return !memcmp(d->note, z.note, sizeof d->note);
    case UI_F_VEL: return d->vel == z.vel;
    case UI_F_LEN: return d->len == z.len;
    case UI_F_MICRO: return d->micro == z.micro && d->strum == z.strum;
    case UI_F_RATCH: return d->ratch == z.ratch;
    case UI_F_CHNC: return d->chance == z.chance;
    case UI_F_COND: return d->cond == z.cond;
    case UI_F_TRIG: return d->type == z.type;
    case UI_F_SOUND: {
        const qd_track_t *t = &a->model.t[a->sel];
        return t->preset == qd_track_start_sound(a->sel) && !t->edited;
    }
    default: return 1;
    }
}

void qd_step_default_reset(qd_app_t *a, int m)
{
    qd_sdef_t *d = qd_tdef(PAT(a), a->sel);
    qd_sdef_t z;
    qd_sdef_init(&z);
    switch (m) {
    case UI_F_NOTE: memcpy(d->note, z.note, sizeof d->note); a->ui.chord_ms = a->ms; break;
    case UI_F_VEL: d->vel = z.vel; break;
    case UI_F_LEN: d->len = z.len; break;
    case UI_F_MICRO: d->micro = z.micro, d->strum = z.strum; break;
    case UI_F_RATCH: d->ratch = z.ratch; break;
    case UI_F_CHNC: d->chance = z.chance; break;
    case UI_F_COND: d->cond = z.cond; break;
    case UI_F_TRIG: d->type = z.type; break;
    case UI_F_SOUND: qd_track_reset_sound(&a->model.t[a->sel], a->sel); break;
    default: break;
    }
}

int qd_step_default_knob(qd_app_t *a, int d)            /* ALGORITHM: the mode's main default */
{
    qd_step_job_default(a, QD_KJ_MAIN, QD_K_ALGO, d);
    return 1;
}

/* ---------------------------------------------------------------- what the screens and the LEDs read --- */
unsigned qd_step_modes_set(const qd_app_t *a, int t, int page)
{
    const qd_pattern_t *p = CPAT(a);
    unsigned m = 0;
    int i, k;
    for (i = 0; i < QD_PAGE; i++) {
        const int s = page * QD_PAGE + i;
        const qd_step_t *st = qd_step(p, t, s);
        if (!qd_step_is_on(st) || !qd_page_in_range(&p->trk[t], s))
            continue;
        for (k = 0; k < UI_F_PLOCK; k++)
            if (k == UI_F_FUNC ? st->func != 0
                               : qd_step_overrides(st, MODE_PM[k]) || (k == UI_F_MICRO && qd_step_overrides(st, QD_PM_STRUM)))
                m |= 1u << k;
        if (st->flags & QD_SF_LOCKS)
            m |= 1u << UI_F_PLOCK;
    }
    return m;
}

static void lab_int(char *o, int v, const char *suffix)
{
    char n[12];
    ui_itoa(n, v);
    strncpy(o, n, 5);
    o[5] = 0;
    if (suffix && strlen(o) + strlen(suffix) < 6)
        strcat(o, suffix);
}

/* P1.3y (the owner's addition): the value keys light the NEAREST value, not only an exact one: the key of the n
 * values whose value is nearest x (a tie: the lower key) */
static int near_key(const int *v, int n, int x)
{
    int i, best = 0;
    for (i = 1; i < n; i++)
        if (abs(v[i] - x) < abs(v[best] - x))
            best = i;
    return best;
}
static uint16_t near_bit(const int *v, int n, int x) { return (uint16_t)(1u << near_key(v, n, x)); }

/* the nearest rules, mode by mode: VEL, CHNC and RATCH by the value (RATCH x1..x8); LEN by the length table's place
 * (INF only for INF); MICRO by the 24ths (0, on the step: none lit, the keys are all off the step); COND's A by 1..4
 * and B by 1..4 (A:B past 4 to the nearest), FILL / NOT FILL exact, 1ST and the rest none; SOUND: the sound's key in
 * the bank shown, a sound in another bank the bank's end key nearest it; NOTE, TRIG, FUNC exact (they are kinds) */
void qd_step_value_keys(const qd_app_t *a, char lab[10][6], uint16_t *lit, uint16_t *def, uint16_t *none, int *clr)
{
    static const int NV_VEL[10] = {13, 25, 38, 51, 64, 76, 89, 102, 114, 127};
    static const int NV_LEN[9] = {1, 2, 3, 4, 5, 6, 8, 10, 12};
    static const int NV_MICRO[10] = {-20, -15, -10, -5, -2, 2, 5, 10, 15, 20};
    static const int NV_CHNC[10] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    static const int NV_RATCH[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    static const int NV_AB[4] = {1, 2, 3, 4};
    static const char *const LEN_LAB[10] = {"1/4", "1/2", "3/4", "1", "1.5", "2", "4", "8", "16", "INF"};
    static const char *const RAMP[4] = {"FLAT", "UP", "DOWN", "ACC"};
    static const char *const COND_LAB[10] = {"FILL", "!FIL", "1:", "2:", "3:", "4:", ":1", ":2", ":3", ":4"};
    static const char *const TRIG_LAB[3] = {"NOTE", "LOCK", "TIE"};
    const qd_ui_t *u = &a->ui;
    const qd_pattern_t *p = CPAT(a);
    const qd_sdef_t *df = &p->trk[a->sel].def;
    const int m = u->hmode;
    qd_rstep_t r;
    int b;
    memset(lab, 0, 10 * 6);
    *lit = *def = *none = 0;
    *clr = 0;
    if (!u->nheld)
        return;
    resolve(a, u->held[0], &r);
    if (m == UI_F_PLOCK) {                              /* P-LOCK: B1..B10 nothing, B11 the p-locks to clear */
        int i, n = 0;
        for (i = 0; i < u->nheld; i++)
            n += qd_lock_count(p, a->sel, u->held[i]);
        *none = 0x3ffu;
        *clr = n > 0;
        return;
    }
    *clr = m == UI_F_MICRO ? (qd_step_overrides(qd_step(p, a->sel, u->held[0]), QD_PM_MICRO) ||
                              qd_step_overrides(qd_step(p, a->sel, u->held[0]), QD_PM_STRUM))
                           : qd_step_overrides(qd_step(p, a->sel, u->held[0]), MODE_PM[m]);
    for (b = 0; b < 10; b++) {
        const uint16_t bit = (uint16_t)(1u << b);
        switch (m) {
        case UI_F_NOTE: {
            const int n = qd_step_entry_note(a, b);
            uint8_t dn[QD_NNOTE];
            int k, nd = qd_notes_abs(df->note, dn);
            qd_note_name(n, lab[b]);
            for (k = 0; k < r.nn && r.on; k++)
                if (r.notes[k] == n)
                    *lit |= bit;
            for (k = 0; k < nd; k++)
                if (dn[k] == n)
                    *def |= bit;
            break;
        }
        case UI_F_VEL:
            lab_int(lab[b], VK_VEL[b], 0);
            *lit |= r.on ? bit & near_bit(NV_VEL, 10, r.vel) : 0;
            *def |= bit & near_bit(NV_VEL, 10, df->vel);
            break;
        case UI_F_LEN:
            strcpy(lab[b], LEN_LAB[b]);
            *lit |= r.on ? bit & (r.len >= QD_LEN_INF ? 1u << 9 : near_bit(NV_LEN, 9, r.len)) : 0;
            *def |= bit & (df->len >= QD_LEN_INF ? 1u << 9 : near_bit(NV_LEN, 9, df->len));
            break;
        case UI_F_MICRO:
            lab_int(lab[b], VK_MICRO[b], 0);
            if (VK_MICRO[b] > 0) {
                memmove(lab[b] + 1, lab[b], 4);
                lab[b][0] = '+';
                lab[b][5] = 0;
            }
            *lit |= r.on && r.micro ? bit & near_bit(NV_MICRO, 10, r.micro) : 0;
            *def |= df->micro ? bit & near_bit(NV_MICRO, 10, df->micro) : 0;
            break;
        case UI_F_RATCH:
            if (b < 8) {
                lab[b][0] = 'x';
                lab[b][1] = (char)('1' + b);
                *lit |= r.on ? bit & near_bit(NV_RATCH, 8, r.ratch) : 0;
                *def |= bit & near_bit(NV_RATCH, 8, qd_ratch_count(df->ratch));
            } else if (b == 8) {
                strcpy(lab[b], RAMP[r.ramp & 3]);
                *lit |= r.on && r.ramp != QD_RAMP_FLAT ? bit : 0;
            } else {
                *none |= bit;
            }
            break;
        case UI_F_CHNC:
            lab_int(lab[b], (b + 1) * 10, "%");
            *lit |= r.on ? bit & near_bit(NV_CHNC, 10, r.chance) : 0;
            *def |= bit & near_bit(NV_CHNC, 10, df->chance);
            break;
        case UI_F_COND: {
            int ca = 1, cb = 1, da = 1, db = 1, ab, dab;
            strcpy(lab[b], COND_LAB[b]);
            ab = r.cond == QD_COND_ALWAYS || qd_cond_ab_of(r.cond, &ca, &cb);
            dab = df->cond == QD_COND_ALWAYS || qd_cond_ab_of(df->cond, &da, &db);
            if (b == 0) {
                *lit |= r.on && r.cond == QD_COND_FILL ? bit : 0;
                *def |= df->cond == QD_COND_FILL ? bit : 0;
            } else if (b == 1) {
                *lit |= r.on && r.cond == QD_COND_NFILL ? bit : 0;
                *def |= df->cond == QD_COND_NFILL ? bit : 0;
            } else if (b < 6) {
                *lit |= r.on && ab && near_key(NV_AB, 4, ca) == b - 2 ? bit : 0;
                *def |= dab && near_key(NV_AB, 4, da) == b - 2 ? bit : 0;
            } else {
                *lit |= r.on && ab && near_key(NV_AB, 4, cb) == b - 6 ? bit : 0;
                *def |= dab && near_key(NV_AB, 4, db) == b - 6 ? bit : 0;
            }
            break;
        }
        case UI_F_TRIG:
            if (b < 3) {
                strcpy(lab[b], TRIG_LAB[b]);
                *lit |= r.on && r.type == QD_TRIG_NOTE + b ? bit : 0;
                *def |= df->type == QD_TRIG_NOTE + b ? bit : 0;
            } else {
                *none |= bit;
            }
            break;
        case UI_F_FUNC:                                 /* P1.3y: the function's key lit (B8 LEAP, B9 JUMP) */
            if (b < 9)
                *lit |= r.on && fn_kind(r.func) == b ? bit : 0;
            else
                *none |= bit;
            break;
        case UI_F_SOUND: {
            const int k = u->sound_bank * 10 + b + 1;
            if (k > a->proj.nsound) {
                *none |= bit;
                break;
            }
            lab_int(lab[b], k, 0);
            if (r.on && r.sound > 0) {                  /* (another bank: its end key nearest the sound) */
                const int lo = u->sound_bank * 10 + 1, hi = lo + 9 < a->proj.nsound ? lo + 9 : a->proj.nsound;
                const int sk = r.sound < lo ? lo : r.sound > hi ? hi : r.sound;
                *lit |= sk == k ? bit : 0;
            }
            break;
        }
        default:
            break;
        }
    }
}
