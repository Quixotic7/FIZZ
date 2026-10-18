/* SPDX-License-Identifier: GPL-3.0-only */
/* The controller's dispatch (ctl.h): the modes, the buttons' table, the keys' tables, the knobs' table, the timers.
 *
 * A key, button or knob goes through one table: the first row whose condition holds takes it. A key's release goes
 * to the row that took its press (a key pressed with PATTERN held is a pattern key until it is released, whatever is
 * let go of first). Any input while FUNC is down makes FUNC a shift (its release does not latch it). */
#include <string.h>

#include "app.h"
#include "ui.h"

const uint8_t QD_WHITE_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
const uint8_t QD_BLACK_KEY[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};   /* F#3 .. F#5 */
#define QD_TRIG_FLASH_MS 80u           /* a track's black key flashes this long from its trig */

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
int qd_ctl_func(const qd_app_t *a) { return held(a, QD_B_FUNC) || a->ui.func_latch; }
qd_pattern_t *qd_ctl_pat(qd_app_t *a) { return &a->proj.pat[a->seq.cur]; }

int qd_ctl_npages(const qd_app_t *a, int t)
{
    const int len = a->proj.pat[a->seq.cur].trk[t].length;
    const int n = (len + QD_PAGE - 1) / QD_PAGE;
    return n < 1 ? 1 : n > QD_NSTEP / QD_PAGE ? QD_NSTEP / QD_PAGE : n;
}

void qd_ctl_init(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    int i;
    memset(u, 0, sizeof *u);
    u->mode = QD_MODE_IDLE;            /* power-on: the idle screen (any key or button: the step view) */
    u->follow = 1;
    u->field = UI_F_NOTE;
    u->tk = QD_TK_NONE;
    u->rec_step = -1;
    for (i = 0; i < 8; i++)
        u->play_page[i] = -1;
    for (i = 0; i < 128; i++)
        u->rec_t0[i] = -1;
}

/* the tempo set (SELECT): the current pattern's, the sequencer's, the sound's */
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

/* the page shown on the selected track (a change slides the roll in) */
static void set_page(qd_app_t *a, int p)
{
    qd_ui_t *u = &a->ui;
    p = p < 0 ? 0 : p > QD_NSTEP / QD_PAGE - 1 ? QD_NSTEP / QD_PAGE - 1 : p;
    if (p == u->page[a->sel])
        return;
    u->slide_ms = a->ms;
    u->slide_dir = p > u->page[a->sel] ? 1 : -1;
    u->page[a->sel] = p;
}

static int playing_page(const qd_app_t *a, int t)
{
    return a->seq.playing ? (int)(qd_seq_track_pos(&a->seq, t) / QD_PAGE) : -1;
}

static void select_track(qd_app_t *a, int t)
{
    if (t == a->sel)
        return;
    qd_step_release_all(a);
    a->sel = t;
    if (a->ui.follow && a->seq.playing) {               /* following: the new track's playing page at once */
        a->ui.page[t] = playing_page(a, t);
        a->ui.play_page[t] = a->ui.page[t];
    }
}

static void set_mode(qd_app_t *a, int mode)
{
    qd_ui_t *u = &a->ui;
    if (u->mode == QD_MODE_KEYS && mode != QD_MODE_KEYS && u->rec)
        qd_rec_toggle(a);                               /* leaving KEYS: the recording ends */
    if (mode != QD_MODE_STEP)
        qd_step_release_all(a);
    u->mode = mode;
}

/* any input: the idle clock restarts; from the idle screen, the step view (1: it woke) */
static int touch(qd_app_t *a)
{
    a->ui.touch_ms = a->ms;
    if (held(a, QD_B_FUNC))
        a->ui.func_used = 1;
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
static int b_steps_held(const qd_app_t *a)
{
    return a->ui.mode == QD_MODE_STEP && (a->ui.nheld > 0 || a->ui.wk_pending);
}
static int b_track_keys(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP || a->ui.mode == QD_MODE_TRACKS; }

/* PATTERN held + white key n: pattern n of bank A, queued (FUNC: at the next 16th; stopped: at once) */
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

/* KEYS mode: the keybed is a keyboard for the selected track (and the recording's: ctl_rec.c) */
static void k_note(qd_app_t *a, int key, int idx, int down, int vel)
{
    (void)idx;
    if (down) {
        const int note = QD_NOTE0 + key + 12 * a->oct;
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

/* PAGE held + white key 1..8: that page */
static void k_page(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    if (down && w < QD_NSTEP / QD_PAGE) {
        a->ui.page_used = 1;
        set_page(a, w);
    }
}

/* the step view's white keys: the steps (ctl_step.c: tap, hold, latch) */
static void k_step(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    if (down)
        qd_step_white_down(a, w);
    else
        qd_step_white_up(a, w);
}

/* the black keys with steps held: the fields (ctl_step.c) */
static void k_field(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    if (down)
        qd_step_field_key(a, b);
}

/* the black keys with nothing held: T1..T8 the tracks, C#5 / D#5 page - / + (F#5: nothing) */
static void k_track_keys(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    if (!down)
        return;
    if (b < QD_NTRACK)
        select_track(a, b);
    else if (b == 8 && a->ui.mode == QD_MODE_STEP)
        set_page(a, a->ui.page[a->sel] > 0 ? a->ui.page[a->sel] - 1 : 0);
    else if (b == 9 && a->ui.mode == QD_MODE_STEP && a->ui.page[a->sel] + 1 < qd_ctl_npages(a, a->sel))
        set_page(a, a->ui.page[a->sel] + 1);
}

static void k_none(qd_app_t *a, int key, int idx, int down, int vel) { (void)a, (void)key, (void)idx, (void)down, (void)vel; }

typedef struct {
    qd_when_fn when;
    qd_key_fn fn;
} qd_key_row_t;
static const qd_key_row_t WHITE_ROWS[] = {
    {w_pattern_held, k_pattern},       /* PATTERN held: A01 .. A16 */
    {w_tracks_held, k_track_select},   /* TRACKS held: tracks 1..8 */
    {w_keys_mode, k_note},             /* KEYS: a keyboard */
    {w_tracks_screen, k_tracks_screen},   /* the tracks screen: select 1..8, mute 9..16 */
    {w_page_held, k_page},             /* PAGE held: pages 1..8 */
    {w_step_view, k_step},             /* the step view: the steps */
};
static const qd_key_row_t BLACK_ROWS[] = {
    {w_pattern_held, k_none},          /* (PATTERN held + black: the banks, P1.4) */
    {w_keys_mode, k_note},             /* KEYS: a keyboard */
    {b_steps_held, k_field},           /* steps held: NOTE .. LOCKS, CLEAR */
    {b_track_keys, k_track_keys},      /* nothing held: T1..T8, page - / + */
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

static void b_page(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (down) {
        u->page_used = 0;
        if (qd_ctl_func(a)) {                           /* FUNC + PAGE: page follow on / off */
            u->follow = !u->follow;
            u->page_used = 1;
            u->play_page[a->sel] = -1;
            qd_ctl_confirm(a, u->follow ? "FOLLOW" : "NO FOLLOW", u->follow ? "the page follows the playhead"
                                                                            : "the page stays where you put it");
        }
    } else if (!u->page_used && u->mode == QD_MODE_STEP) {   /* a tap: the track's next page (round its pages) */
        set_page(a, (u->page[a->sel] + 1) % qd_ctl_npages(a, a->sel));
    }
}

static void b_func(qd_app_t *a, int down)
{
    if (down)
        a->ui.func_used = 0;
    else if (!a->ui.func_used)                          /* a tap: the latch on / off */
        a->ui.func_latch = !a->ui.func_latch;
}

static void b_home(qd_app_t *a, int down)
{
    if (!down)
        return;
    qd_step_release_all(a);                             /* the latch let go */
    a->ui.tk = QD_TK_NONE;
    set_mode(a, QD_MODE_STEP);
}

static void b_keys(qd_app_t *a, int down)
{
    if (down)
        set_mode(a, a->ui.mode == QD_MODE_KEYS ? QD_MODE_STEP : QD_MODE_KEYS);
}

static void b_play(qd_app_t *a, int down)
{
    if (!down)
        return;
    if (a->seq.playing)
        qd_app_stop(a);
    else
        qd_app_play(a);
}

/* REC: KEYS mode: recording on / off (FUNC + REC: quantize on / off); elsewhere: FILL while held */
static void b_rec(qd_app_t *a, int down)
{
    if (down && a->ui.mode == QD_MODE_KEYS) {
        if (qd_ctl_func(a)) {
            a->ui.quantize = !a->ui.quantize;
            qd_ctl_confirm(a, a->ui.quantize ? "QUANTIZE" : "AS PLAYED",
                           a->ui.quantize ? "live notes land on the steps" : "live notes keep their timing");
        } else {
            qd_rec_toggle(a);
        }
    } else if (down) {
        qd_seq_fill(&a->seq, 1);
    } else {
        qd_seq_fill(&a->seq, 0);
    }
}

/* OCT- / OCT+ (d -1 / +1); both: PANIC */
static void b_oct(qd_app_t *a, int d)
{
    qd_ui_t *u = &a->ui;
    if (held(a, QD_B_OCTDN) && held(a, QD_B_OCTUP)) {
        qd_pool_kill_all(&a->pool);
        return;
    }
    if (u->mode == QD_MODE_KEYS) {
        if (u->rec && !a->seq.playing && !qd_ctl_func(a)) {   /* step recording: OCT+ a rest, OCT- a step back */
            if (d > 0)
                qd_rec_rest(a);
            else
                qd_rec_cursor(a, -1);
        } else {
            const int o = a->oct + d;
            a->oct = o < QD_OCT_MIN ? QD_OCT_MIN : o > QD_OCT_MAX ? QD_OCT_MAX : o;
        }
        return;
    }
    if (u->mode != QD_MODE_STEP)
        return;
    if (u->nheld > 0 || u->wk_pending) {                /* steps held */
        qd_step_promote_all(a);
        if (u->field == UI_F_NOTE && !qd_ctl_func(a))
            qd_step_octave(a, d);                       /* NOTE: the notes an octave */
        else
            qd_step_latch(a, d > 0);                    /* OCT+ latches, OCT- lets go */
        return;
    }
    if (u->latch)
        return;
    if (d < 0 || u->page[a->sel] + d < qd_ctl_npages(a, a->sel))   /* (down always; up within the track) */
        set_page(a, u->page[a->sel] + d);
}
static void b_octdn(qd_app_t *a, int down)
{
    if (down)
        b_oct(a, -1);
}
static void b_octup(qd_app_t *a, int down)
{
    if (down)
        b_oct(a, 1);
}
static void b_none(qd_app_t *a, int down) { (void)a, (void)down; }

/* the buttons by role (PLAN §8's table; what is not built yet: P1.4 / P1.5) */
static void (*const BUTTONS[QD_NBTN])(qd_app_t *a, int down) = {
    [QD_B_TRACKS] = b_tracks,          /* tap: the tracks screen; held + white 1..8: select */
    [QD_B_PATTERN] = b_none,           /* held + white: queue a pattern (the keys' table); the screen: P1.4 */
    [QD_B_PAGE] = b_page,              /* tap: the next page; held + white 1..8: that page; FUNC: follow on / off */
    [QD_B_MUTE] = b_none,              /* P1.4 */
    [QD_B_SOUND] = b_none,             /* P1.4: the sound editor */
    [QD_B_FUNC] = b_func,              /* tap: latch; held: the shift */
    [QD_B_HOME] = b_home,              /* the step view; lets go of a latch */
    [QD_B_SAVE] = b_none,              /* P1.5 */
    [QD_B_KEYS] = b_keys,              /* KEYS mode on / off */
    [QD_B_MIX] = b_none,               /* P1.4 */
    [QD_B_PLAY] = b_play,              /* play / stop */
    [QD_B_REC] = b_rec,                /* KEYS: record; else FILL while held */
    [QD_B_OCTDN] = b_octdn,
    [QD_B_OCTUP] = b_octup,
};

void qd_ctl_button(qd_app_t *a, int button, int down)
{
    if (down) {
        const int was_func = button == QD_B_FUNC;
        touch(a);
        if (was_func)
            a->ui.func_used = 0;
    }
    BUTTONS[button](a, down);
}

/* ================================================================ the knobs === */
/* the selected track's sound: the quick page (KNOB 1..4: FREQ, DECAY, HARM, FDBK), ALGO, the sound itself */
static void live_edit(qd_app_t *a, int id, int d, int tk, int knob)
{
    const qd_track_t *t = &a->model.t[a->sel];
    qd_player_live_edit(&a->player, a->sel, id, t->patch[id] + d * qd_step_id_detent(id));
    qd_ctl_takeover(a, tk, knob, d, 0, -1);
}

/* the step view (and KEYS with nothing recording): SELECT tempo (FUNC: swing), PRESETS the sound, ALGORITHM the
 * ALGO, KNOB 1..4 the quick page */
static int k_track(qd_app_t *a, int knob, int d)
{
    switch (knob) {
    case QD_K_SELECT:
        if (qd_ctl_func(a)) {
            qd_pattern_t *p = qd_ctl_pat(a);
            const int sw = p->swing + d;
            p->swing = (uint8_t)(sw < 50 ? 50 : sw > 80 ? 80 : sw);
            qd_ctl_takeover(a, UI_TK_SWING, 1, d, 0, -1);
        } else {
            if (!a->seq.ext)
                qd_ctl_set_tempo(a, a->bpm + d);
            qd_ctl_takeover(a, UI_TK_TEMPO, 0, d, 0, -1);
        }
        return 1;
    case QD_K_PRESETS: {
        qd_track_t *t = &a->model.t[a->sel];
        qd_track_load_preset(t, (int)t->preset + d);
        qd_ctl_takeover(a, QD_TK_PRESET, 0, d, 0, -1);
        return 1;
    }
    case QD_K_ALGO:
        live_edit(a, QD_QP_ALGO, d, UI_TK_ALGO, 0);
        return 1;
    default:
        live_edit(a, qd_step_quick_id(knob - QD_K_1), d, UI_TK_FREQ + (knob - QD_K_1), knob - QD_K_1);
        return 1;
    }
}

/* the rates in the picker's order, slowest first */
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

/* the tracks screen: KNOB 1 (and SELECT) LEN, KNOB 2 RATE, KNOB 3 LIMIT, KNOB 4 SWING on / off */
static int k_tracks(qd_app_t *a, int knob, int d)
{
    qd_tset_t *ts = &qd_ctl_pat(a)->trk[a->sel];
    int v;
    switch (knob) {
    case QD_K_SELECT:
    case QD_K_1:
        v = ts->length + d;
        ts->length = (uint8_t)(v < 1 ? 1 : v > QD_NSTEP ? QD_NSTEP : v);
        qd_ctl_takeover(a, UI_TK_LEN, 0, d, 0, -1);
        return 1;
    case QD_K_2:
        ts->rate = (uint8_t)qd_ctl_rate_at(qd_ctl_rate_index(ts->rate) + d);
        qd_ctl_takeover(a, UI_TK_RATE, 1, d, 0, -1);
        return 1;
    case QD_K_3:
        v = ts->limit + d;
        ts->limit = (uint8_t)(v < 1 ? 1 : v > 16 ? 16 : v);
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

static int when_steps_held(const qd_app_t *a) { return b_steps_held(a); }
static int when_live(const qd_app_t *a) { return a->ui.mode == QD_MODE_KEYS && a->ui.rec && a->seq.playing; }
static int when_step_rec(const qd_app_t *a) { return a->ui.mode == QD_MODE_KEYS && a->ui.rec && !a->seq.playing; }
static int when_tracks(const qd_app_t *a) { return a->ui.mode == QD_MODE_TRACKS; }
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
    {when_steps_held, k_held},         /* a step held: the field's knobs, the p-locks (ctl_step.c) */
    {when_live, k_live},               /* live recording: the quick page as locks on the step playing */
    {when_step_rec, k_cursor},         /* step recording: PRESETS moves the cursor */
    {when_tracks, k_tracks},           /* the tracks screen: LEN RATE LIMIT SWING */
    {when_always, k_track},            /* the step view, KEYS: tempo, the sound, ALGO, the quick page */
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
    if (u->undo_ok && a->ms - u->undo_ms >= QD_UNDO_MS)
        u->undo_ok = 0;
    if (a->seq.flash_sample[a->sel] != u->flash_seen && a->seq.flash_sample[a->sel] <= a->seq.now) {
        u->flash_seen = a->seq.flash_sample[a->sel];    /* a trig of the selected track: the held step's? */
        if (u->nheld && a->seq.flash_step[a->sel] == u->held[0]) {
            u->held_play_ms = a->ms;
            u->held_played = 1;
        }
    }
    if (u->rec_live && !a->seq.playing)                 /* the transport stopped under live notes: they end */
        qd_rec_key(a, -1, 0, 0);
    {   /* page follow: the view moves when the playhead goes to another page (a page chosen by hand stays until
         * then); not while steps are held */
        const int pp = playing_page(a, a->sel);
        if (pp < 0) {
            u->play_page[a->sel] = -1;
        } else if (pp != u->play_page[a->sel]) {
            u->play_page[a->sel] = pp;
            if (u->follow && u->nheld == 0 && !u->wk_down && !(u->mode == QD_MODE_KEYS && u->rec))
                set_page(a, pp);
        }
    }
    if (u->mode != QD_MODE_IDLE && !a->seq.playing && a->ms - u->touch_ms >= QD_IDLE_MS && !a->keys_held &&
        !a->buttons_held) {
        set_mode(a, QD_MODE_IDLE);                      /* two minutes untouched, stopped: the idle screen */
        u->tk = QD_TK_NONE;
    }
}
