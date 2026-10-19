/* SPDX-License-Identifier: GPL-3.0-only */
/* The controller's dispatch (ctl.h): the modes, the buttons' table, the keys' tables, the knobs' table, the layers,
 * the timers.
 *
 * A key, button or knob goes through one table: the first row whose condition holds takes it. A key's release goes
 * to the row that took its press (a page key pressed with F1 held is a page key until it is released, whatever is let
 * go of first). The F layers (OCT- = F1, OCT+ = F2, both = F3) are read at each press: none while steps are held (as
 * in Form: OCT- / OCT+ are then NOTE's octave and SOUND's bank). */
#include <string.h>

#include "app.h"
#include "ui.h"
#include "../seq/scale.h"
#include "../seq/sounds.h"

const uint8_t QD_WHITE_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
const uint8_t QD_BLACK_KEY[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};   /* F#3 .. F#5 */

int qd_ctl_white(int key)
{
    int i;
    for (i = 0; i < 16; i++)
        if (QD_WHITE_KEY[i] == key)
            return i;
    return -1;
}

int qd_ctl_black(int key)
{
    int i;
    for (i = 0; i < 11; i++)
        if (QD_BLACK_KEY[i] == key)
            return i;
    return -1;
}

static int held(const qd_app_t *a, int b) { return (int)((a->buttons_held >> b) & 1u); }
int qd_ctl_shift(const qd_app_t *a) { return held(a, QD_B_FUNC); }
qd_pattern_t *qd_ctl_pat(qd_app_t *a) { return &a->proj.pat[a->seq.cur]; }
const qd_pattern_t *qd_ctl_cpat(const qd_app_t *a) { return &a->proj.pat[a->seq.cur]; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int steps_held(const qd_app_t *a) { return a->ui.nheld > 0 || a->ui.wk_pending; }

int qd_ctl_layer(const qd_app_t *a)
{
    const int dn = held(a, QD_B_OCTDN), up = held(a, QD_B_OCTUP);
    if (a->ui.mode != QD_MODE_STEP || steps_held(a) || (!dn && !up))
        return QD_LAYER_NONE;
    if (a->ui.f3 || (dn && up))
        return QD_LAYER_F3;
    return dn ? QD_LAYER_F1 : QD_LAYER_F2;
}

int qd_ctl_layer_shown(const qd_app_t *a)
{
    const int l = qd_ctl_layer(a);
    if (l == QD_LAYER_F1 || l == QD_LAYER_F2)          /* (alone a moment: not the first of a chord) */
        return a->ms - a->ui.f_ms >= QD_CHORD_MS ? l : QD_LAYER_NONE;
    return l;
}

void qd_ctl_init(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    int i;
    memset(u, 0, sizeof *u);
    u->mode = QD_MODE_IDLE;            /* power-on: the idle screen (any key or button: the step view) */
    u->follow = 1;
    u->hmode = UI_F_NOTE;
    u->tk = QD_TK_NONE;
    u->rec_step = -1;
    u->entry_oct = QD_ENTRY_OCT;
    u->f2_track = -1;
    u->f3_anchor = -1;
    qd_clip_init(&u->clip);
    for (i = 0; i < 8; i++)
        u->play_page[i] = -1;
    for (i = 0; i < 128; i++)
        u->rec_t0[i] = -1;
}

/* the tempo set (F3 + SELECT): the current pattern's, the sequencer's, the sound's */
void qd_ctl_set_tempo(qd_app_t *a, int bpm)
{
    bpm = bpm < QD_BPM_MIN ? QD_BPM_MIN : bpm > QD_BPM_MAX ? QD_BPM_MAX : bpm;
    qd_ctl_pat(a)->bpm = (uint16_t)bpm;
    qd_seq_tempo_mbpm(&a->seq, bpm * 1000);
    qd_app_tempo(a, bpm);
}

void qd_ctl_takeover(qd_app_t *a, int tk, int knob, int dir, int lock, int step)
{
    qd_ui_t *u = &a->ui;
    u->tk = tk;
    u->tk_ms = a->ms;
    u->tk_dir = dir > 0 ? 1 : -1;
    u->tk_knob = knob;
    u->tk_lock = lock;
    u->tk_step = step;
}

void qd_ctl_confirm(qd_app_t *a, const char *big, const char *small)
{
    a->ui.cf_big = big;
    a->ui.cf_small = small;
    a->ui.cf_ms = a->ms;
    a->ui.cf_on = 1;
}

/* the page edited on the selected track (a change slides the lane in) */
void qd_ctl_set_page(qd_app_t *a, int p)
{
    qd_ui_t *u = &a->ui;
    p = clampi(p, 0, QD_NPAGE - 1);
    if (p == u->page[a->sel])
        return;
    u->slide_ms = a->ms;
    u->slide_dir = p > u->page[a->sel] ? 1 : -1;
    u->page[a->sel] = p;
}

int qd_ctl_playing_page(const qd_app_t *a, int t)
{
    const int s = qd_seq_track_step(&a->seq, t);
    return s < 0 ? -1 : s / QD_PAGE;
}

static void select_track(qd_app_t *a, int t)
{
    if (t == a->sel)
        return;
    qd_step_release_all(a);
    a->sel = t;
    if (a->ui.follow && a->seq.playing) {               /* following: the new track's playing page at once */
        a->ui.page[t] = qd_ctl_playing_page(a, t);
        a->ui.play_page[t] = a->ui.page[t];
    }
}

static void set_mode(qd_app_t *a, int mode)
{
    qd_ui_t *u = &a->ui;
    if (u->mode == QD_MODE_KEYS && mode != QD_MODE_KEYS && u->rec)
        qd_rec_toggle(a);                               /* leaving the keyboard: the recording ends */
    if (mode != QD_MODE_STEP)
        qd_step_release_all(a);
    u->mode = mode;
}

/* any input: the idle clock restarts; from the idle screen, the step view (1: it woke) */
static int touch(qd_app_t *a)
{
    a->ui.touch_ms = a->ms;
    if (held(a, QD_B_FUNC))
        a->ui.shift_used = 1;
    if (a->ui.mode == QD_MODE_IDLE) {
        a->ui.mode = QD_MODE_STEP;
        return 1;
    }
    return 0;
}

/* ================================================================ the keys ===
 * A row: when its condition holds at the press, its handler takes the press and, later, the release. */
typedef int (*qd_when_fn)(const qd_app_t *a);
typedef void (*qd_key_fn)(qd_app_t *a, int key, int idx, int down, int vel);   /* idx: white 0..15 / black 0..10 */

static int w_pattern_held(const qd_app_t *a) { return held(a, QD_B_PATTERN); }
static int w_tracks_held(const qd_app_t *a) { return held(a, QD_B_TRACKS); }
static int w_keys_mode(const qd_app_t *a) { return a->ui.mode == QD_MODE_KEYS; }
static int w_tracks_screen(const qd_app_t *a) { return a->ui.mode == QD_MODE_TRACKS; }
static int w_page_held(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && held(a, QD_B_PAGE); }
static int w_step_view(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP; }
static int w_steps_held(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && steps_held(a); }
static int w_f1(const qd_app_t *a) { return qd_ctl_layer(a) == QD_LAYER_F1; }
static int w_f2(const qd_app_t *a) { return qd_ctl_layer(a) == QD_LAYER_F2; }
static int w_f3(const qd_app_t *a) { return qd_ctl_layer(a) == QD_LAYER_F3; }

/* PATTERN held + white key n: pattern n of bank A, queued (SHIFT: at the next 16th; stopped: at once) */
static void k_pattern(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    if (!down || w < 0)
        return;
    qd_seq_select(&a->seq, w, held(a, QD_B_FUNC));
    if (!a->seq.playing)
        qd_app_tempo(a, qd_seq_bpm(&a->seq));
}

/* TRACKS held + white key 1..8: that track */
static void k_track_select(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    a->ui.tracks_tap = 0;
    if (down && w >= 0 && w < QD_NTRACK)
        select_track(a, w);
}

/* the keyboard: the selected track (and the recording's: ctl_rec.c) */
static void k_note(qd_app_t *a, int key, int idx, int down, int vel)
{
    (void)idx;
    if (down) {
        const int note = qd_rec_key_note(a, key);
        qd_app_note_on(a, key, note, vel);
        qd_rec_key(a, note, 1, vel);
    } else if ((a->keys_struck >> key) & 1u) {
        qd_rec_key(a, a->key_note[key], 0, 0);
    }
}

/* the tracks screen: white 1..8 select, 9..16 mute / unmute tracks 1..8 */
static void k_tracks_screen(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    if (!down)
        return;
    if (w < QD_NTRACK)
        select_track(a, w);
    else
        a->seq.mute ^= (uint8_t)(1u << (w - QD_NTRACK));
}

/* PAGE held + white key 1..8: edit that page */
static void k_page(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    if (down && w < QD_NPAGE) {
        a->ui.page_used = 1;
        qd_ctl_set_page(a, w);
    }
}

/* the step view's white keys: the steps (ctl_step.c: tap, hold; with steps held: more held steps) */
static void k_step(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    if (down)
        qd_step_white_down(a, w);
    else
        qd_step_white_up(a, w);
}

static int step_of(const qd_app_t *a, int w) { return a->ui.page[a->sel] * QD_PAGE + w; }

/* F1 + W: copy the step; SHIFT + F1 + W: cut it (copied, then cleared; undoable) */
static void k_f1_white(qd_app_t *a, int key, int w, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    (void)key, (void)vel;
    if (!down)
        return;
    if (qd_ctl_shift(a)) {
        qd_clip_cut_step(&u->clip, qd_ctl_pat(a), a->seq.cur, a->sel, step_of(a, w));
        qd_ctl_confirm(a, "CUT", "F2 + a step pastes it");
    } else {
        qd_clip_copy_step(&u->clip, qd_ctl_pat(a), a->sel, step_of(a, w));
        qd_ctl_confirm(a, "COPIED", "F2 + a step pastes it");
    }
}

/* F2 + W: paste the copied step (each tap again); a track key held: W1 mute, W2 solo that track */
static void k_f2_white(qd_app_t *a, int key, int w, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    (void)key, (void)vel;
    if (!down)
        return;
    if (u->f2_track >= 0) {
        const uint8_t bit = (uint8_t)(1u << u->f2_track);
        u->f2_used = 1;
        if (w == 0)
            a->seq.mute ^= bit;
        else if (w == 1)
            a->seq.solo ^= bit;
        return;
    }
    if (!u->clip.st.valid) {
        qd_ctl_confirm(a, "NOTHING", "F1 + a step copies it first");
        return;
    }
    if (qd_clip_paste_step(&u->clip, qd_ctl_pat(a), a->seq.cur, a->sel, step_of(a, w)) == QD_EFULL)
        qd_ctl_confirm(a, "NO ROOM", "for locks: the pattern has 1024");
}

/* F3 + W: the page's end (the page joins the loop); W a held + W b: start a .. end b */
static void k_f3_white(qd_app_t *a, int key, int w, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    qd_pattern_t *p = qd_ctl_pat(a);
    qd_tset_t *ts = &p->trk[a->sel];
    const int pg = u->page[a->sel];
    (void)key, (void)vel;
    if (!down) {
        if (w == u->f3_anchor)
            u->f3_anchor = -1;
        return;
    }
    if (u->f3_anchor < 0 || u->f3_anchor == w) {
        u->f3_anchor = w;
        qd_page_set_range(p, a->sel, pg, ts->pstart[pg] > w ? w : ts->pstart[pg], w);
    } else {
        const int s0 = u->f3_anchor < w ? u->f3_anchor : w, s1 = u->f3_anchor < w ? w : u->f3_anchor;
        qd_page_set_range(p, a->sel, pg, s0, s1);
    }
    ts->loop = (uint8_t)(qd_track_loop(ts) | (1u << pg));
}

/* the black keys with steps held: the mode's ten values, CLEAR (ctl_step.c) */
static void k_value(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    qd_step_value_key(a, b, down);
}

/* F1 + B1..B8: pages (tap: edit; double tap: loop it alone; hold A + press B: loop A..B); B9 clear the page, B10 the
 * track, B11 undo / redo */
static void k_f1_black(qd_app_t *a, int key, int b, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    qd_pattern_t *p = qd_ctl_pat(a);
    const uint16_t bit = (uint16_t)(1u << b);
    (void)key, (void)vel;
    if (b < QD_NPAGE) {
        if (down) {
            uint16_t pages;
            u->lk_down |= bit;
            u->pg_t0[b] = a->ms;
            pages = u->lk_down & 0xFFu;
            if (pages & ~bit) {                         /* another page key is down: the loop A..B, edit A */
                int lo = 8, hi = -1, k;
                for (k = 0; k < QD_NPAGE; k++)
                    if ((pages >> k) & 1u) {
                        lo = k < lo ? k : lo;
                        hi = k > hi ? k : hi;
                    }
                qd_page_set_loop(p, a->sel, (int)(((1u << (hi + 1)) - 1u) & ~((1u << lo) - 1u)));
                qd_ctl_set_page(a, lo);
                u->pg_range = 1;
                u->pg_taps[lo] = u->pg_taps[hi] = 0;
            }
            return;
        }
        u->lk_down &= (uint16_t)~bit;
        if (u->pg_range) {                              /* a range: no taps until the page keys are up */
            if (!(u->lk_down & 0xFFu))
                u->pg_range = 0;
            return;
        }
        if (a->ms - u->pg_t0[b] >= QD_CLICK_MS) {       /* held, then let go alone: nothing (Form's) */
            u->pg_taps[b] = 0;
            return;
        }
        if (u->pg_taps[b] && a->ms - u->pg_up[b] < QD_CLICK_MS) {   /* the double tap: loop it alone */
            qd_page_set_loop(p, a->sel, 1 << b);
            qd_ctl_set_page(a, b);
            u->pg_taps[b] = 0;
            return;
        }
        qd_ctl_set_page(a, b);                          /* a tap: edit it (the loop unchanged) */
        u->pg_taps[b] = 1;
        u->pg_up[b] = a->ms;
        return;
    }
    if (!down)
        return;
    if (b == 8) {
        qd_clip_clear_page(&u->clip, p, a->seq.cur, a->sel, u->page[a->sel]);
        qd_ctl_confirm(a, "PAGE CLEARED", "F1 + B11 undoes it");
    } else if (b == 9) {
        qd_clip_clear_track(&u->clip, p, a->seq.cur, a->sel);
        qd_ctl_confirm(a, "TRACK CLEARED", "F1 + B11 undoes it");
    } else if (b == 10) {
        const int r = qd_clip_undo(&u->clip, p, a->seq.cur, a->sel);
        qd_ctl_confirm(a, r == 1 ? "UNDONE" : r == QD_EFULL ? "NO ROOM" : "NO UNDO",
                       r == 1 ? "again to redo" : r == QD_EFULL ? "for locks: the pattern has 1024"
                                                                : "nothing cleared or pasted on this track");
    }
}

/* F2 + B1..B8: tracks (select; held + W1 mute, + W2 solo); B9 copy the page, B10 paste it */
static void k_f2_black(qd_app_t *a, int key, int b, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    (void)key, (void)vel;
    if (b < QD_NTRACK) {
        if (down) {
            select_track(a, b);
            u->f2_track = b;
            u->f2_used = 0;
        } else if (u->f2_track == b) {
            u->f2_track = -1;
        }
        return;
    }
    if (!down)
        return;
    if (b == 8) {
        qd_clip_copy_page(&u->clip, qd_ctl_pat(a), a->sel, u->page[a->sel]);
        qd_ctl_confirm(a, "PAGE COPIED", "F2 + B10 pastes it");
    } else if (b == 9) {
        const int r = qd_clip_paste_page(&u->clip, qd_ctl_pat(a), a->seq.cur, a->sel, u->page[a->sel]);
        if (r == QD_EARG)
            qd_ctl_confirm(a, "NOTHING", "F2 + B9 copies a page first");
        else if (r == QD_EFULL)
            qd_ctl_confirm(a, "NO ROOM", "for locks: the pattern has 1024");
        else
            qd_ctl_confirm(a, "PAGE PASTED", "F1 + B11 undoes it");
    }
}

/* the rates on F3's B1..B9, slowest first (as the picker) */
static const uint8_t RATE_ORDER[QD_NRATE] = {QD_RATE_1_8, QD_RATE_1_4, QD_RATE_1_2, QD_RATE_2_3, QD_RATE_3_4,
                                             QD_RATE_1, QD_RATE_4_3, QD_RATE_3_2, QD_RATE_2};
int qd_ctl_rate_index(int rate)
{
    int i;
    for (i = 0; i < QD_NRATE; i++)
        if (RATE_ORDER[i] == rate)
            return i;
    return 5;
}
int qd_ctl_rate_at(int i) { return RATE_ORDER[i < 0 ? 0 : i >= QD_NRATE ? QD_NRATE - 1 : i]; }

/* F3 + B1..B9: the rate */
static void k_f3_black(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    if (down && b < QD_NRATE)
        qd_ctl_pat(a)->trk[a->sel].rate = (uint8_t)qd_ctl_rate_at(b);
}

/* the black keys with nothing held: B1..B10 the hold mode (B11 nothing: PLAN question 37) */
static void k_mode(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    if (down && b < UI_NFIELD) {
        a->ui.hmode = b;
        if (b == UI_F_LOCKS)
            a->ui.lock_cur = 0;
    }
}

static void k_none(qd_app_t *a, int key, int idx, int down, int vel) { (void)a, (void)key, (void)idx, (void)down, (void)vel; }

typedef struct {
    qd_when_fn when;
    qd_key_fn fn;
} qd_key_row_t;
static const qd_key_row_t WHITE_ROWS[] = {
    {w_pattern_held, k_pattern},       /* PATTERN held: A01 .. A16 */
    {w_tracks_held, k_track_select},   /* TRACKS held: tracks 1..8 */
    {w_keys_mode, k_note},             /* the keyboard */
    {w_tracks_screen, k_tracks_screen},   /* the tracks screen: select 1..8, mute 9..16 */
    {w_page_held, k_page},             /* PAGE held: edit pages 1..8 */
    {w_steps_held, k_step},            /* steps held: more held steps */
    {w_f1, k_f1_white},                /* F1: copy (SHIFT: cut) */
    {w_f2, k_f2_white},                /* F2: paste; a track held: W1 mute, W2 solo */
    {w_f3, k_f3_white},                /* F3: the page's end, start..end */
    {w_step_view, k_step},             /* the step view: the steps */
};
static const qd_key_row_t BLACK_ROWS[] = {
    {w_pattern_held, k_none},          /* (PATTERN held + black: the banks, later) */
    {w_keys_mode, k_note},             /* the keyboard */
    {w_steps_held, k_value},           /* steps held: the mode's values, B11 CLEAR */
    {w_f1, k_f1_black},                /* F1: pages, clear page / track, undo */
    {w_f2, k_f2_black},                /* F2: tracks, copy / paste the page */
    {w_f3, k_f3_black},                /* F3: the rate */
    {w_step_view, k_mode},             /* nothing held: the hold mode */
};
void qd_ctl_key(qd_app_t *a, int key, int down, int vel)
{
    const int w = qd_ctl_white(key), b = qd_ctl_black(key);
    const qd_key_row_t *rows = w >= 0 ? WHITE_ROWS : BLACK_ROWS;
    const int n = w >= 0 ? (int)(sizeof WHITE_ROWS / sizeof WHITE_ROWS[0]) : (int)(sizeof BLACK_ROWS / sizeof BLACK_ROWS[0]);
    int i;
    if (down) {
        a->ui.key_row[key] = 0;
        if (touch(a))                                   /* the press that woke the unit does nothing else */
            return;
        for (i = 0; i < n; i++)
            if (rows[i].when(a)) {
                a->ui.key_row[key] = (int8_t)(i + 1);   /* the row that took the press takes the release */
                rows[i].fn(a, key, w >= 0 ? w : b, 1, vel);
                return;
            }
    } else if (a->ui.key_row[key] > 0 && a->ui.key_row[key] <= n) {
        const qd_key_row_t *r = &rows[a->ui.key_row[key] - 1];
        a->ui.key_row[key] = 0;
        r->fn(a, key, w >= 0 ? w : b, 0, 0);
    }
}

/* ================================================================ the buttons === */
static void b_tracks(qd_app_t *a, int down)
{
    if (down) {
        a->ui.tracks_tap = 1;
    } else if (a->ui.tracks_tap) {                      /* a tap: the tracks screen on / off */
        a->ui.tracks_tap = 0;
        set_mode(a, a->ui.mode == QD_MODE_TRACKS ? QD_MODE_STEP : QD_MODE_TRACKS);
    }
}

/* PAGE: a tap = page follow on / off; held + W1..W8 = edit that page */
static void b_page(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (down) {
        u->page_used = 0;
    } else if (!u->page_used && u->mode == QD_MODE_STEP) {
        u->follow = !u->follow;
        u->play_page[a->sel] = -1;
        qd_ctl_confirm(a, u->follow ? "FOLLOW" : "NO FOLLOW", u->follow ? "the page follows the playhead"
                                                                        : "the page stays where you put it");
    }
}

static void b_shift(qd_app_t *a, int down)
{
    if (down)
        a->ui.shift_used = 0;
}

static void b_home(qd_app_t *a, int down)
{
    if (!down)
        return;
    qd_step_release_all(a);
    a->ui.tk = QD_TK_NONE;
    set_mode(a, QD_MODE_STEP);
}

/* ARP held: FILL (the FILL / NOT FILL conditions) */
static void b_fill(qd_app_t *a, int down) { qd_seq_fill(&a->seq, down); }

static void b_play(qd_app_t *a, int down)
{
    if (!down)
        return;
    if (held(a, QD_B_REC)) {                            /* REC + PLAY: play with recording armed, in the keyboard */
        a->ui.rec_down_used = 1;
        if (a->ui.mode != QD_MODE_KEYS)
            set_mode(a, QD_MODE_KEYS);
        if (!a->seq.playing)
            qd_app_play(a);
        if (!a->ui.rec)
            qd_rec_toggle(a);
        return;
    }
    if (a->seq.playing)
        qd_app_stop(a);
    else
        qd_app_play(a);
}

/* REC: a tap switches the keybed between the sequencer and the keyboard; SHIFT + REC in the keyboard: armed / not */
static void b_rec(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (down) {
        u->rec_down_used = 0;
        if (qd_ctl_shift(a)) {
            u->rec_down_used = 1;
            if (u->mode == QD_MODE_KEYS) {
                qd_rec_toggle(a);
                qd_ctl_confirm(a, u->rec ? (a->seq.playing ? "LIVE REC" : "STEP REC") : "REC OFF",
                               u->rec ? (a->seq.playing ? "notes go in as played" : "a chord a step")
                                      : "the keyboard plays");
            }
        }
        return;
    }
    if (u->rec_down_used)
        return;
    set_mode(a, u->mode == QD_MODE_KEYS ? QD_MODE_STEP : QD_MODE_KEYS);
}

/* OCT- / OCT+ (d -1 / +1): the layers in the step view (F1 / F2 / both F3); with steps held: NOTE's octave, SOUND's
 * bank; the keyboard: the octave (step recording: OCT+ a rest, OCT- a step back), both: every voice out (PANIC) */
static void b_oct(qd_app_t *a, int d, int down)
{
    qd_ui_t *u = &a->ui;
    const int both = held(a, QD_B_OCTDN) && held(a, QD_B_OCTUP);
    if (!down) {
        if (!held(a, QD_B_OCTDN) && !held(a, QD_B_OCTUP)) {
            u->f3 = 0;
            u->lk_down = 0;
            u->pg_range = 0;
            u->f2_track = -1;
            u->f3_anchor = -1;
        }
        return;
    }
    if (u->mode == QD_MODE_KEYS || u->mode == QD_MODE_TRACKS) {
        if (both) {
            qd_pool_kill_all(&a->pool);
            return;
        }
        if (u->mode != QD_MODE_KEYS)
            return;
        if (u->rec && !a->seq.playing) {                /* step recording: OCT+ a rest, OCT- a step back */
            if (d > 0)
                qd_rec_rest(a);
            else
                qd_rec_cursor(a, -1);
        } else {
            a->oct = clampi(a->oct + d, QD_OCT_MIN, QD_OCT_MAX);
        }
        return;
    }
    if (u->mode != QD_MODE_STEP)
        return;
    if (steps_held(a)) {                                /* NOTE: the entry octave; SOUND: the bank; else nothing */
        qd_step_promote_all(a);
        qd_step_oct(a, d);
        return;
    }
    if (both)
        u->f3 = 1;
    else
        u->f_ms = a->ms;                                /* the layer's first key */
}
static void b_octdn(qd_app_t *a, int down) { b_oct(a, -1, down); }
static void b_octup(qd_app_t *a, int down) { b_oct(a, 1, down); }
static void b_none(qd_app_t *a, int down) { (void)a, (void)down; }

/* the buttons by role (PLAN §8b; what is not built yet: the sound editor, the mix, saving) */
static void (*const BUTTONS[QD_NBTN])(qd_app_t *a, int down) = {
    [QD_B_TRACKS] = b_tracks,          /* tap: the tracks screen; held + white 1..8: select */
    [QD_B_PATTERN] = b_none,           /* held + white: queue a pattern (the keys' table) */
    [QD_B_PAGE] = b_page,              /* tap: page follow on / off; held + white 1..8: edit that page */
    [QD_B_MUTE] = b_none,
    [QD_B_SOUND] = b_none,             /* the sound editor (next milestone) */
    [QD_B_FUNC] = b_shift,             /* SHIFT, held */
    [QD_B_HOME] = b_home,              /* the step view */
    [QD_B_SAVE] = b_none,
    [QD_B_KEYS] = b_fill,              /* ARP held: FILL */
    [QD_B_MIX] = b_none,
    [QD_B_PLAY] = b_play,              /* play / stop; REC + PLAY: play, recording armed */
    [QD_B_REC] = b_rec,                /* tap: the keyboard <-> the sequencer; SHIFT: armed */
    [QD_B_OCTDN] = b_octdn,            /* F1 */
    [QD_B_OCTUP] = b_octup,            /* F2 */
};

void qd_ctl_button(qd_app_t *a, int button, int down)
{
    if (down) {
        if (button != QD_B_FUNC)
            touch(a);
        else
            a->ui.touch_ms = a->ms;
        if (a->ui.mode == QD_MODE_IDLE)
            a->ui.mode = QD_MODE_STEP;
    }
    BUTTONS[button](a, down);
}

/* ================================================================ the knobs === */
/* the selected track's sound: the quick page (KNOB 1..4: FREQ, DECAY, HARM, FDBK) */
static void live_edit(qd_app_t *a, int id, int d, int tk, int knob)
{
    const qd_track_t *t = &a->model.t[a->sel];
    qd_player_live_edit(&a->player, a->sel, id, t->patch[id] + d * qd_step_id_detent(id));
    qd_ctl_takeover(a, tk, knob, d, 0, -1);
}

/* the track's sound from the pool (PRESETS with nothing held, SOUND's ALGORITHM) */
static void track_sound(qd_app_t *a, int d)
{
    qd_track_t *t = &a->model.t[a->sel];
    const int k = clampi((int)t->sound + d, 1, a->proj.nsound);
    qd_track_load_sound(t, k, qd_sound_name(&a->proj, k), qd_sound_patch(&a->proj, k));
    qd_ctl_takeover(a, QD_TK_PRESET, 0, d, 0, -1);
}

/* SELECT: the page edited (with or without steps held) */
static void page_knob(qd_app_t *a, int d)
{
    qd_ctl_set_page(a, a->ui.page[a->sel] + d);
    qd_ctl_takeover(a, UI_TK_PAGE, 0, d, 0, -1);
}

/* the step view, nothing held: SELECT the page, ALGORITHM the mode's default (SHIFT: the scale), PRESETS the track's
 * sound (SHIFT: the root), KNOB 1..4 the quick page */
static int k_stepview(qd_app_t *a, int knob, int d)
{
    qd_tset_t *ts = &qd_ctl_pat(a)->trk[a->sel];
    switch (knob) {
    case QD_K_SELECT:
        page_knob(a, d);
        return 1;
    case QD_K_ALGO:
        if (qd_ctl_shift(a)) {                          /* (temporary: the track's scale) */
            ts->scale = (uint8_t)clampi(ts->scale + d, 0, QD_NSCALE - 1);
            ts->key_global = 0;
            qd_ctl_takeover(a, UI_TK_SCALE, 0, d, 0, -1);
            return 1;
        }
        return qd_step_default_knob(a, d);
    case QD_K_PRESETS:
        if (qd_ctl_shift(a)) {                          /* (temporary: the track's root) */
            ts->root = (uint8_t)(((ts->root + d) % 12 + 12) % 12);
            ts->key_global = 0;
            qd_ctl_takeover(a, UI_TK_ROOT, 0, d, 0, -1);
            return 1;
        }
        track_sound(a, d);
        return 1;
    default:
        live_edit(a, qd_step_quick_id(knob - QD_K_1), d, UI_TK_FREQ + (knob - QD_K_1), knob - QD_K_1);
        return 1;
    }
}

/* the keyboard (not recording live): SELECT tempo (SHIFT: swing), PRESETS the sound, KNOB 1..4 the quick page */
static int k_track(qd_app_t *a, int knob, int d)
{
    switch (knob) {
    case QD_K_SELECT:
        if (qd_ctl_shift(a)) {
            qd_pattern_t *p = qd_ctl_pat(a);
            p->swing = (uint8_t)clampi(p->swing + d, 50, 80);
            qd_ctl_takeover(a, UI_TK_SWING, 1, d, 0, -1);
        } else {
            if (!a->seq.ext)
                qd_ctl_set_tempo(a, a->bpm + d);
            qd_ctl_takeover(a, UI_TK_TEMPO, 0, d, 0, -1);
        }
        return 1;
    case QD_K_PRESETS:
        track_sound(a, d);
        return 1;
    case QD_K_ALGO:
        return 1;                                       /* (the track's ALGO: the sound editor) */
    default:
        live_edit(a, qd_step_quick_id(knob - QD_K_1), d, UI_TK_FREQ + (knob - QD_K_1), knob - QD_K_1);
        return 1;
    }
}

/* F3 + SELECT: tempo; SHIFT + F3 + SELECT: swing */
static int k_f3(qd_app_t *a, int knob, int d)
{
    if (knob != QD_K_SELECT)
        return 0;
    if (qd_ctl_shift(a)) {
        qd_pattern_t *p = qd_ctl_pat(a);
        p->swing = (uint8_t)clampi(p->swing + d, 50, 80);
        qd_ctl_takeover(a, UI_TK_SWING, 1, d, 0, -1);
    } else {
        if (!a->seq.ext)
            qd_ctl_set_tempo(a, a->bpm + d);
        qd_ctl_takeover(a, UI_TK_TEMPO, 0, d, 0, -1);
    }
    return 1;
}

/* the tracks screen: KNOB 1 (and SELECT) LEN (the loop, Form's flat rebuild), KNOB 2 RATE, KNOB 3 LIMIT, KNOB 4 SWING */
static int k_tracks(qd_app_t *a, int knob, int d)
{
    qd_pattern_t *p = qd_ctl_pat(a);
    qd_tset_t *ts = &p->trk[a->sel];
    switch (knob) {
    case QD_K_SELECT:
    case QD_K_1:
        qd_track_set_length(p, a->sel, clampi(qd_track_len(ts) + d, 1, QD_NSTEP));
        qd_ctl_takeover(a, UI_TK_LEN, 0, d, 0, -1);
        return 1;
    case QD_K_2:
        ts->rate = (uint8_t)qd_ctl_rate_at(qd_ctl_rate_index(ts->rate) + d);
        qd_ctl_takeover(a, UI_TK_RATE, 1, d, 0, -1);
        return 1;
    case QD_K_3:
        ts->limit = (uint8_t)clampi(ts->limit + d, 1, 16);
        qd_ctl_takeover(a, UI_TK_LIMIT, 2, d, 0, -1);
        return 1;
    case QD_K_4:
        ts->swing = d > 0;
        qd_ctl_takeover(a, UI_TK_TSWING, 3, d, 0, -1);
        return 1;
    default:
        return k_track(a, knob, d);
    }
}

static int when_f3(const qd_app_t *a) { return qd_ctl_layer(a) == QD_LAYER_F3; }
static int when_steps_held(const qd_app_t *a) { return w_steps_held(a); }
static int when_live(const qd_app_t *a) { return a->ui.mode == QD_MODE_KEYS && a->ui.rec && a->seq.playing; }
static int when_step_rec(const qd_app_t *a) { return a->ui.mode == QD_MODE_KEYS && a->ui.rec && !a->seq.playing; }
static int when_tracks(const qd_app_t *a) { return a->ui.mode == QD_MODE_TRACKS; }
static int when_stepview(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP; }
static int when_always(const qd_app_t *a) { (void)a; return 1; }
static int k_held(qd_app_t *a, int knob, int d) { return qd_step_knob(a, knob, d); }
static int k_live(qd_app_t *a, int knob, int d) { return qd_rec_knob(a, knob, d); }
static int k_cursor(qd_app_t *a, int knob, int d)
{
    if (knob != QD_K_PRESETS)
        return 0;
    qd_rec_cursor(a, d);                                /* step recording: PRESETS moves the cursor */
    return 1;
}

static const struct {
    qd_when_fn when;
    int (*fn)(qd_app_t *a, int knob, int d);           /* 1: taken (else the next row) */
} KNOB_ROWS[] = {
    {when_f3, k_f3},                   /* F3 + SELECT: tempo (SHIFT: swing) */
    {when_steps_held, k_held},         /* a step held: SELECT page, ALGORITHM value, PRESETS sound lock, p-locks */
    {when_live, k_live},               /* live recording: the quick page as locks on the step playing */
    {when_step_rec, k_cursor},         /* step recording: PRESETS moves the cursor */
    {when_tracks, k_tracks},           /* the tracks screen: LEN RATE LIMIT SWING */
    {when_stepview, k_stepview},       /* the step view: page, the mode's default, the sound, the quick page */
    {when_always, k_track},            /* the keyboard: tempo, the sound, the quick page */
};

void qd_ctl_knob(qd_app_t *a, int knob, int d)
{
    size_t i;
    touch(a);
    for (i = 0; i < sizeof KNOB_ROWS / sizeof KNOB_ROWS[0]; i++)
        if (KNOB_ROWS[i].when(a) && KNOB_ROWS[i].fn(a, knob, d))
            return;
}

/* ================================================================ time === */
void qd_ctl_ms(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    qd_step_promote_due(a);                             /* a white key down 250 ms: held */
    if (u->tk != QD_TK_NONE && a->ms - u->tk_ms >= QD_TK_MS + QD_TK_FADE_MS)
        u->tk = QD_TK_NONE;
    if (u->cf_on && a->ms - u->cf_ms >= QD_CONFIRM_MS)
        u->cf_on = 0;
    if (a->seq.flash_sample[a->sel] != u->flash_seen && a->seq.flash_sample[a->sel] <= a->seq.now) {
        u->flash_seen = a->seq.flash_sample[a->sel];    /* a trig of the selected track: the held step's? */
        if (u->nheld && a->seq.flash_step[a->sel] == u->held[0]) {
            u->held_play_ms = a->ms;
            u->held_played = 1;
        }
    }
    if (u->rec_live && !a->seq.playing)                 /* the transport stopped under live notes: they end */
        qd_rec_key(a, -1, 0, 0);
    {   /* page follow: the page edited moves when the playhead goes to another page (a page chosen by hand stays
         * until then); not while steps are held or a layer is */
        const int pp = qd_ctl_playing_page(a, a->sel);
        if (pp < 0) {
            u->play_page[a->sel] = -1;
        } else if (pp != u->play_page[a->sel]) {
            u->play_page[a->sel] = pp;
            if (u->follow && u->nheld == 0 && !u->wk_down && qd_ctl_layer(a) == QD_LAYER_NONE &&
                !(u->mode == QD_MODE_KEYS && u->rec))
                qd_ctl_set_page(a, pp);
        }
    }
    if (u->mode != QD_MODE_IDLE && !a->seq.playing && a->ms - u->touch_ms >= QD_IDLE_MS && !a->keys_held &&
        !a->buttons_held) {
        set_mode(a, QD_MODE_IDLE);                      /* two minutes untouched, stopped: the idle screen */
        u->tk = QD_TK_NONE;
    }
}
