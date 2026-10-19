/* SPDX-License-Identifier: GPL-3.0-only */
/* The layer maps (PLAN.md §8b, version 2.1): while F1, F2, F3, SEL or SEQ is held the screen is its map, so every
 * layer explains itself while it is held; while SHIFT is held, a list of what SHIFT does here. The LINE look: thin
 * outlines, the black keys' strip at the foot with what each key does in this layer written in it (the same strip as
 * a held step's values), white for what is chosen, red only for the playhead.
 *   F1 · PAGES        the eight pages as eight cells (4 x 2, B1..B8): the page's number, its range ("5–12"), its
 *                     sixteen steps as a 4 x 4 of dots (a trig filled, outside start..end only a speck); the page
 *                     edited outlined white, the pages in the loop grey, the others dark; the playing page a red line
 *                     under its number (blinking). After B10's copy the copied page's cell is dashed (blinking) and
 *                     the legend says B1..B8 paste it. The legend: tap / twice / hold, B9 tap / hold, W's copy / paste;
 *                     the strip: 1..8, CLR (B9), COPY (B10, lit once a page is copied), UNDO (B11, dark with none)
 *   F2 · CUT & PASTE  Form's pick-up / drop: the page's sixteen steps (a trig a dot; the last W pressed marked CUT in
 *                     orange or PASTE in green) and the hand: a card holding the step (its notes on a little keybed,
 *                     a LOCK step a padlock, an empty step EMPTY) or a dashed empty card; what the next W does
 *   F3 · RANGE & RATE the page edited as sixteen cells with its start..end bracketed over them (the W held first
 *                     outlined, blinking), the range big and the rate big as a note value with its dot or T (triplet)
 *                     mark, the four knobs' jobs in their colours (VALUE, TYPE, START, END), the tempo and swing on
 *                     SELECT / SHIFT; the strip: the seven bases and the three types, the track's lit
 *   SEL · TRACKS      (layer 4) the eight tracks as s17's speakers: the selected one boxed white, a muted one grey
 *                     with an orange cross, a soloed one an S in a yellow ring (blinking), the waves lit by a trig
 *                     just now; the strip: 1..8 select, B11 CLR (lit while a mute or solo is set)
 *   SEQ · PATTERNS    (ui_patterns_draw, line.html s13 simpler) the queued pattern big (else the playing one),
 *                     A01..A16 as dots, a line emptying to the change, the launch mode
 *   SHIFT             the context at the right, then a row a key: the key white, an arrow, what it does grey */
#include <math.h>

#include "ui.h"

#define SOLO_COL 0xffd23fu

static const char *const DOT = " \xc2\xb7 ";          /* " · " */
static const char *const DASH = "\xe2\x80\x93";        /* "–" */

static char *cat(char *d, const char *s)
{
    char *e = d;
    while (*e)
        e++;
    while (s && *s)
        *e++ = *s++;
    *e = 0;
    return d;
}

static void line2(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, col, lw, 0, QD_NONE);
}

/* a key and what it does on one line, centred: the key white, the rest grey */
static void legend(const char *key, const char *what, float y, int dim)
{
    const float wk = ui_TW(key, 8.f, 600, 1.f), ww = ui_TW(what, 8.f, 600, 1.f), x = 120.f - (wk + 6.f + ww) / 2.f;
    ui_lab(key, x, y, dim ? UI_G2 : UI_K3, QD_LEFT);
    ui_lab(what, x + wk + 6.f, y, dim ? UI_G2 : UI_G1, QD_LEFT);
}

static const char *const NUM[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};

/* ---------------------------------------------------------------- F1: the pages */
static void f1(const ui_layer_t *v)
{
    const char *lab[11];
    unsigned hot = 0;
    char s[48], n[12];
    int p, k;

    for (p = 0; p < 8; p++) {
        const float x = 8.f + (float)(p % 4) * 57.f, y = 30.f + (float)(p / 4) * 66.f, w = 53.f, h = 60.f;
        const int ed = p == v->page, in = (v->loop >> p) & 1, cp = p == v->page_copied;
        const int st = v->pstart[p] > 15 ? 15 : v->pstart[p];
        const int en = v->pend[p] < st ? st : v->pend[p] > 15 ? 15 : v->pend[p];
        const qd_rgb tc = ed ? UI_K3 : in ? UI_G1 : UI_G2;
        if (cp) {                                       /* the page copied: dashed, blinking */
            static const float DSH[2] = {3.f, 3.f};
            qd_save();
            qd_dash(DSH, 2);
            ui_RR(x, y, w, h, 5.f, v->blink ? UI_K3 : UI_G1, 1.5f, QD_NONE);
            qd_restore();
        } else {
            ui_RR(x, y, w, h, 5.f, ed ? UI_K3 : in ? UI_G1 : UI_G3, ed ? 1.5f : 1.f, QD_NONE);
        }
        ui_T(NUM[p], x + 8.f, y + 22.f, 18.f, ed ? 400 : 300, 0.f, tc, QD_LEFT);
        s[0] = 0;                                       /* its range */
        cat(cat(cat(s, ui_itoa(n, st + 1)), DASH), ui_itoa(n, en + 1));
        ui_T(s, x + 8.f, y + h - 8.f, 7.f, 600, 0.5f, st > 0 || en < 15 ? tc : UI_G2, QD_LEFT);
        for (k = 0; k < 16; k++) {                      /* its steps, a beat a row */
            const float dx = x + 27.f + (float)(k % 4) * 7.5f, dy = y + 14.f + (float)(k / 4) * 10.5f;
            if (k < st || k > en)
                ui_O(dx, dy, 0.9f, QD_NONE, 1.f, UI_G3);
            else if (v->trig[p][k])
                ui_O(dx, dy, 2.2f, QD_NONE, 1.f, ed ? UI_K3 : in ? UI_G1 : UI_G2);
            else
                ui_O(dx, dy, 1.6f, ed || in ? UI_G2 : UI_G3, 1.f, QD_NONE);
        }
        if (p == v->play_page)                          /* playing: a red line under its number, blinking */
            line2(x + 8.f, y + 28.f, x + 19.f, y + 28.f, v->blink ? UI_RED : 0xa01c2au, 2.f);
        if (in)
            hot |= 1u << p;
    }
    if (v->page_copied >= 0 && v->page_copied < 8) {
        s[0] = 0;
        cat(cat(cat(s, "PASTE PAGE "), NUM[v->page_copied]), " ONTO IT");
        legend("B1\xe2\x80\x93" "B8", s, 172.f, 0);
        legend("EACH PRESS", "PASTES AGAIN", 184.f, 0);
    } else {
        legend("TAP", "EDIT \xc2\xb7 TWICE: LOOP IT ALONE \xc2\xb7 HOLD: IN / OUT", 172.f, 0);
        legend("HOLD + ANOTHER", "LOOP FROM ONE TO IT", 184.f, 0);
    }
    legend("B9", "TAP: CLEAR PAGE \xc2\xb7 HOLD: CLEAR TRACK", 196.f, 0);
    legend("W", v->f1_copied ? "PASTE THE COPIED STEP" : "COPY A STEP, THEN W PASTES IT", 208.f, 0);
    for (k = 0; k < 8; k++)
        lab[k] = NUM[k];
    lab[8] = "CLR", lab[9] = "COPY", lab[10] = "UNDO";
    ui_strip(lab, (v->page >= 0 && v->page < 8 ? 1u << v->page : 0u) | (v->page_copied >= 0 ? 1u << 9 : 0u), 0u,
             v->can_undo ? 0u : 1u << 10, hot, UI_K3);
}

/* ---------------------------------------------------------------- F2: Form's pick-up / drop */
static void hand_card(const ui_layer_t *v, float x, float y, float w, float h)
{
    if (!v->hand) {                                     /* empty-handed: a dashed card */
        static const float DSH[2] = {3.f, 3.f};
        qd_save();
        qd_dash(DSH, 2);
        ui_RR(x, y, w, h, 6.f, UI_G2, 1.2f, QD_NONE);
        qd_restore();
        ui_T("HAND EMPTY", x + w / 2.f, y + h / 2.f + 3.f, 9.f, 600, 1.f, UI_G1, QD_CENTER);
        return;
    }
    ui_RR(x, y, w, h, 6.f, UI_K3, 1.5f, QD_NONE);
    ui_lab("IN THE HAND", x + 8.f, y + 13.f, UI_G1, QD_LEFT);
    if (v->clip_type == 0) {
        ui_T("EMPTY STEP", x + w / 2.f, y + h / 2.f + 8.f, 11.f, 600, 1.f, UI_K3, QD_CENTER);
    } else if (v->clip_type == 2) {                     /* a LOCK step: a padlock */
        ui_lock(x + w / 2.f - 7.f, y + h / 2.f - 4.f, UI_MODE_COL[UI_F_LOCKS], 2.f);
        ui_lab("LOCK STEP", x + w / 2.f, y + h - 6.f, UI_G1, QD_CENTER);
    } else {                                            /* its notes on a little keybed */
        qd_rgb fill[128];
        int i, lo = 127;
        for (i = 0; i < 128; i++)
            fill[i] = QD_NONE;
        for (i = 0; i < v->clip_nnote && i < 5; i++) {
            fill[v->clip_note[i] & 127] = UI_K3;
            lo = v->clip_note[i] < lo ? v->clip_note[i] : lo;
        }
        ui_keyboard(x + 8.f, y + 20.f, w - 16.f, h - 28.f, lo / 12 * 12, fill);
    }
}

static float cx16(int i) { return 9.f + (float)i * 13.5f + (float)(i / 4) * 2.f; }

static void f2(const ui_layer_t *v)
{
    const float top = 34.f, h = 40.f;
    int i;
    for (i = 0; i < 16; i++) {                          /* the page's steps */
        const int last = i == v->last_w;
        const qd_rgb oc = last ? (v->last_act == 1 ? UI_K4 : UI_K2) : UI_G2;
        ui_RR(cx16(i), top, 12.f, h, 3.f, oc, last ? 1.5f : 1.f, QD_NONE);
        if (v->page_on[i])
            ui_O(cx16(i) + 6.f, top + h - 10.f, 2.4f, QD_NONE, 1.f, last ? oc : UI_K3);
    }
    if (v->last_w >= 0 && v->last_w < 16) {
        const char *w = v->last_act == 1 ? "CUT" : "PASTE";
        float x = cx16(v->last_w) + 6.f;
        x = x < 20.f ? 20.f : x > 220.f ? 220.f : x;
        ui_T(w, x, top + h + 12.f, 8.f, 700, 1.f, v->last_act == 1 ? UI_K4 : UI_K2, QD_CENTER);
    }
    hand_card(v, 60.f, 96.f, 120.f, 66.f);
    legend("NEXT W", !v->hand_loaded ? "CUTS IT, EVEN AN EMPTY STEP"
                     : v->hand ? "PASTES" : "CUTS (AN EMPTY STEP: PASTES)", 180.f, 0);
    legend("FIRST W", "CUTS \xc2\xb7 THEN AN EMPTY STEP: PASTE", 194.f, 1);
    legend("A FULL STEP", "PASTE, CUT, PASTE ..", 206.f, 1);
    legend("F2 UP", "EMPTIES THE HAND", 218.f, 1);
}

/* ---------------------------------------------------------------- F3: the page's range and the rate */
static void f3(const ui_layer_t *v)
{
    static const char *const KJOB[4] = {"VALUE", "TYPE", "START", "END"};
    static const char *const BASE[7] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"};
    const int pg = v->page < 0 ? 0 : v->page > 7 ? 7 : v->page;
    const int st = v->pstart[pg] > 15 ? 15 : v->pstart[pg];
    const int en = v->pend[pg] < st ? st : v->pend[pg] > 15 ? 15 : v->pend[pg];
    const float top = 48.f, h = 44.f;
    const char *lab[11];
    char s[64], n[12];
    float x0, x1, kx;
    int i;

    s[0] = 0;
    cat(cat(s, "PAGE "), NUM[pg]);
    ui_lab(s, 9.f, 36.f, UI_G1, QD_LEFT);
    ui_lab("W: THE END \xc2\xb7 W + W: START\xe2\x80\x93" "END", 230.f, 36.f, UI_G2, QD_RIGHT);
    for (i = 0; i < 16; i++) {                          /* the steps: in the range bright, out dark */
        const int in = i >= st && i <= en, an = i == v->anchor;
        const qd_rgb oc = an ? (v->blink ? UI_K3 : UI_G1) : in ? UI_G1 : 0x1c1c22u;
        ui_RR(cx16(i), top, 12.f, h, 3.f, oc, an ? 1.5f : 1.f, QD_NONE);
        if (in && v->trig[pg][i])
            ui_O(cx16(i) + 6.f, top + h - 10.f, 2.2f, QD_NONE, 1.f, UI_G1);
    }
    x0 = cx16(st) + 1.f, x1 = cx16(en) + 11.f;          /* the bracket over the range: KNOB 3's green start, KNOB 4's
                                                         * orange end */
    line2(x0, top - 5.f, x1, top - 5.f, UI_K3, 1.5f);
    line2(x0, top - 5.f, x0, top - 1.f, UI_K[2 - 1], 1.5f);
    line2(x1, top - 5.f, x1, top - 1.f, UI_K[4 - 1], 1.5f);
    ui_T(ui_itoa(n, st + 1), cx16(st) + 6.f, top + h + 11.f, 8.f, 600, 0.f, UI_K3, QD_CENTER);
    if (en != st)
        ui_T(ui_itoa(n, en + 1), cx16(en) + 6.f, top + h + 11.f, 8.f, 600, 0.f, UI_K3, QD_CENTER);

    s[0] = 0;                                           /* the range big */
    cat(cat(cat(s, ui_itoa(n, st + 1)), DASH), ui_itoa(n, en + 1));
    ui_T(s, 12.f, 148.f, 34.f, 200, 0.f, UI_K3, QD_LEFT);
    ui_lab("STEPS", 14.f, 162.f, UI_G1, QD_LEFT);
    {   /* the rate big: the note value in KNOB 1's blue, its dot or T in KNOB 2's green */
        const char *val = v->rate_val_name ? v->rate_val_name : "1/16";
        const float mark = v->rate_type ? 14.f : 0.f, xr = 228.f - mark;
        ui_T(val, xr, 148.f, 34.f, 200, 0.f, UI_K1, QD_RIGHT);
        if (v->rate_type == 1)
            ui_O(xr + 6.f, 144.f, 3.2f, QD_NONE, 1.f, UI_K2);
        else if (v->rate_type == 2)
            ui_T("T", xr + 7.f, 130.f, 14.f, 500, 0.f, UI_K2, QD_CENTER);
        ui_lab(v->rate_type == 1 ? "DOTTED" : v->rate_type == 2 ? "TRIPLET" : "RATE", 226.f, 162.f, UI_G1, QD_RIGHT);
    }
    kx = 120.f - 2.f * 50.f + 25.f;                     /* the four knobs' jobs, in their colours */
    for (i = 0; i < 4; i++) {
        ui_knob_mark(kx + (float)i * 50.f - 14.f, 180.f, 0.5f, UI_K[i]);
        ui_lab(KJOB[i], kx + (float)i * 50.f - 5.f, 183.f, UI_K[i], QD_LEFT);
    }
    s[0] = 0;
    cat(cat(cat(s, "SELECT: TEMPO "), ui_itoa(n, v->bpm)), DOT);
    cat(cat(cat(s, "SHIFT: SWING "), ui_itoa(n, v->swing)), "%");
    ui_lab(s, 120.f, 204.f, UI_G1, QD_CENTER);
    for (i = 0; i < 7; i++)
        lab[i] = BASE[i];
    lab[7] = "NRM", lab[8] = "DOT", lab[9] = "TRI", lab[10] = 0;
    ui_strip(lab, (v->rate_val >= 0 && v->rate_val < 7 ? 1u << v->rate_val : 0u) |
                      (v->rate_type >= 0 && v->rate_type < 3 ? 1u << (7 + v->rate_type) : 0u),
             0u, 1u << 10, 0u, UI_K3);
}

/* ---------------------------------------------------------------- SEL: the tracks (s17's speakers) */
static void speaker(float x, float y, qd_rgb col, int on, int mute)
{
    const float a[8] = {x - 10.f, y - 5.f, x - 4.f, y - 5.f, x - 4.f, y + 5.f, x - 10.f, y + 5.f};
    const float b[8] = {x - 4.f, y - 5.f, x + 3.f, y - 11.f, x + 3.f, y + 11.f, x - 4.f, y + 5.f};
    int i;
    ui_P(a, 4, col, 1.3f, 1, QD_NONE);
    ui_P(b, 4, col, 1.3f, 0, QD_NONE);
    if (mute) {
        line2(x + 8.f, y - 5.f, x + 16.f, y + 5.f, UI_K4, 1.5f);
        line2(x + 16.f, y - 5.f, x + 8.f, y + 5.f, UI_K4, 1.5f);
        return;
    }
    for (i = 0; i < 2; i++) {
        qd_save();
        qd_alpha(on ? 1.f : 0.35f + 0.1f * (float)i);
        qd_arc(x + 3.f, y, i ? 12.f : 7.f, -0.75f, 0.75f, col, 1.3f);
        qd_restore();
    }
}

static void tracks(const ui_layer_t *v)
{
    const char *lab[11];
    unsigned faint = 0;
    int i;

    for (i = 0; i < 8; i++) {
        const float x = 34.f + (float)(i % 4) * 57.f, y = 58.f + (float)(i / 4) * 70.f;
        const int mute = (v->mute >> i) & 1, solo = (v->solo >> i) & 1, sel = i == v->sel;
        const char *nm = v->tname[i] ? v->tname[i] : "";
        if (sel)
            ui_RR(x - 27.f, y - 24.f, 54.f, 68.f, 6.f, UI_K3, 1.5f, QD_NONE);
        speaker(x - 2.f, y, mute ? UI_G2 : UI_K3, v->flash[i] != 0, mute);
        ui_T(NUM[i], x, y + 26.f, 9.f, 600, 0.f, mute ? UI_G2 : UI_K3, QD_CENTER);
        ui_T(nm, x, y + 37.f, 8.f, 500, 1.f, mute ? UI_G2 : sel ? UI_K3 : UI_G1, QD_CENTER);
        if (solo) {                                     /* soloed: an S in a ring, blinking */
            ui_O(x + 20.f, y - 16.f, 6.f, SOLO_COL, 1.2f, v->blink ? SOLO_COL : QD_NONE);
            ui_T("S", x + 20.f, y - 13.f, 8.f, 700, 0.f, v->blink ? UI_BG : SOLO_COL, QD_CENTER);
        }
        if (mute)
            faint |= 1u << i;
    }
    legend("W1\xe2\x80\x93W8", "TAP: MUTE \xc2\xb7 HOLD: SOLO", 192.f, 0);
    legend("B11", "EVERY MUTE AND SOLO OFF", 205.f, !(v->mute || v->solo));
    for (i = 0; i < 8; i++)
        lab[i] = NUM[i];
    lab[8] = lab[9] = 0;
    lab[10] = "CLR";
    ui_strip(lab, v->sel >= 0 && v->sel < 8 ? 1u << v->sel : 0u, 0u,
             (faint & ~(1u << v->sel)) | 3u << 8 | (v->mute || v->solo ? 0u : 1u << 10),
             v->mute || v->solo ? 1u << 10 : 0u, UI_K3);
}

void ui_layer_draw(const ui_layer_t *v)
{
    static const char *const T[4] = {"F1 \xc2\xb7 PAGES", "F2 \xc2\xb7 CUT & PASTE", "F3 \xc2\xb7 RANGE & RATE",
                                     "SEL \xc2\xb7 TRACKS"};
    const int l = v->layer < 1 ? 1 : v->layer > 4 ? 4 : v->layer;
    ui_head_track(T[l - 1], v->track, v->name ? v->name : "", UI_K3);
    if (l == 1)
        f1(v);
    else if (l == 2)
        f2(v);
    else if (l == 3)
        f3(v);
    else
        tracks(v);
}

/* ---------------------------------------------------------------- SEQ: the patterns (line.html s13) */
void ui_patterns_draw(const ui_patterns_t *v)
{
    static const char *const LAUNCH[4] = {"END", "BAR", "BEAT", "NOW"};
    const int big = v->queued >= 0 ? v->queued : v->cur;
    char s[8], n[12];
    int i;

    ui_head("PATTERNS", "BANK A", 0, UI_K3, UI_G1);
    s[0] = 0;                                           /* the queued one big (orange), else the playing one */
    if (big + 1 < 10)
        cat(s, "0");
    cat(s, ui_itoa(n, big + 1));
    ui_T("A", 12.f, 100.f, 40.f, 200, 0.f, UI_G1, QD_LEFT);
    ui_T(s, 40.f, 100.f, 64.f, 200, 0.f, v->queued >= 0 ? UI_K4 : UI_K3, QD_LEFT);
    if (v->queued >= 0) {
        ui_lab(v->playing ? "QUEUED" : "NEXT AT PLAY", 14.f, 124.f, UI_G1, QD_LEFT);
        line2(14.f, 132.f, 120.f, 132.f, UI_G3, 2.f);
        if (v->playing && v->left > 0.f)
            line2(14.f, 132.f, 14.f + 106.f * v->left, 132.f, UI_K4, 2.f);
    } else {
        ui_lab(v->playing ? "PLAYING" : "STOPPED", 14.f, 124.f, UI_G1, QD_LEFT);
    }
    for (i = 0; i < 16; i++) {                          /* bank A: sixteen dots */
        const float x = 148.f + (float)(i % 4) * 22.f, y = 50.f + (float)(i / 4) * 22.f;
        if (i == v->cur)
            ui_O(x, y, 6.5f, QD_NONE, 1.f, UI_K3);
        else if (i == v->queued)
            ui_O(x, y, 6.5f, v->blink ? UI_K4 : UI_G2, 1.5f, UI_BG);
        else if ((v->used >> i) & 1u)
            ui_O(x, y, 6.5f, UI_G1, 1.2f, UI_BG);
        else
            ui_O(x, y, 1.5f, QD_NONE, 1.f, UI_G2);
    }
    ui_lab("LAUNCH", 14.f, 162.f, UI_G1, QD_LEFT);      /* the launch mode (ALGORITHM), the one in use white */
    ui_knob_mark(68.f, 159.f, (float)v->launch / 3.f, UI_G1);
    for (i = 0; i < 4; i++) {
        const float x = 30.f + (float)i * 56.f;
        ui_T(LAUNCH[i], x, 184.f, 13.f, i == v->launch ? 500 : 300, 0.5f, i == v->launch ? UI_K3 : UI_G2,
             QD_CENTER);
        if (i == v->launch)
            line2(x - 10.f, 191.f, x + 10.f, 191.f, UI_K3, 1.5f);
    }
    legend("W", "TAP: QUEUE \xc2\xb7 HOLD: AT ONCE", 210.f, 0);
    legend("ALGORITHM", "THE LAUNCH MODE", 222.f, 1);
}

/* ---------------------------------------------------------------- SHIFT */
void ui_shift_draw(const ui_shift_t *v)
{
    const int n = v->n < 0 ? 0 : v->n > 8 ? 8 : v->n;
    const float sp = n > 6 ? 21.f : 26.f, y0 = 58.f;
    float kw = 0.f;
    int i;
    ui_head("SHIFT", v->ctx, 0, UI_K3, UI_G1);
    for (i = 0; i < n; i++) {                           /* the keys' column as wide as the widest key */
        const float w = v->key[i] ? ui_TW(v->key[i], 11.f, 600, 0.5f) : 0.f;
        if (w > kw)
            kw = w;
    }
    if (kw > 110.f)
        kw = 110.f;
    for (i = 0; i < n; i++) {
        const float y = y0 + (float)i * sp, ax = 14.f + kw + 8.f;
        ui_T(v->key[i] ? v->key[i] : "", 14.f, y, 11.f, 600, 0.5f, UI_K3, QD_LEFT);
        ui_T("\xe2\x86\x92", ax, y, 11.f, 400, 0.f, UI_G2, QD_LEFT);
        ui_T(v->what[i] ? v->what[i] : "", ax + 16.f, y, 11.f, 400, 0.f, UI_G1, QD_LEFT);
    }
    if (!n)
        ui_T("nothing here", 120.f, 120.f, 11.f, 400, 0.f, UI_G2, QD_CENTER);
}
