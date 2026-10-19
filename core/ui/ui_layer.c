/* SPDX-License-Identifier: GPL-3.0-only */
/* The layer maps (PLAN.md §8b): while F1, F2 or F3 is held the screen is that layer's map, so every layer explains
 * itself while it is held; while SHIFT is held, a list of what SHIFT does here. The LINE look: thin outlines, the
 * black keys' strip at the foot with what each key does in this layer written in it (the same strip as a held step's
 * values), white for what is chosen, red only for the playhead.
 *   F1 · PAGES        the eight pages as eight cells (4 x 2, B1..B8): the page's number, its range ("5–12"), its
 *                     sixteen steps as a 4 x 4 of dots (a trig filled, outside start..end only a speck); the page
 *                     edited outlined white, the pages in the loop grey, the others dark; the playing page a red line
 *                     under its number (blinking). How to loop, W's job (COPY STEP; with SHIFT: CUT STEP); the
 *                     strip: 1..8, CLEAR
 *                     PAGE / TRACK on B9 / B10, UNDO on B11 (dark with nothing to undo)
 *   F2 · TRACKS       the eight tracks (B1..B8) as s17's speakers: the selected one boxed white, a muted one grey with
 *                     an orange cross, a soloed one an S in a yellow ring (blinking), the waves lit by a trig just now;
 *                     a track key held: "W1 MUTE · W2 SOLO" for it; W pastes the copied step; the strip: 1..8, COPY /
 *                     PASTE PAGE on B9 / B10 (PASTE dark with no page copied)
 *   F3 · RANGE & RATE the page edited as sixteen cells with its start..end bracketed over them (the W held first
 *                     outlined, blinking), the range and the rate big, the tempo and swing on SELECT / SHIFT; the
 *                     strip: the nine rates, the track's lit
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

/* a word over two keys of the strip (B9 and B10: "CLEAR", "PAGE"), a little bracket under it */
static void over_keys(const char *s, int k0, int k1, qd_rgb col)
{
    const float x0 = ui_strip_x(k0) + 2.f, x1 = ui_strip_x(k1) + 16.f, c = (x0 + x1) / 2.f;
    const float hw = ui_TW(s, 7.f, 600, 1.f) / 2.f + 3.f;
    ui_T(s, c, 215.f, 7.f, 600, 1.f, col, QD_CENTER);
    line2(x0, 212.f, c - hw, 212.f, UI_G2, 1.f);
    line2(c + hw, 212.f, x1, 212.f, UI_G2, 1.f);
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
    char s[16], n[12];
    int p, k;

    for (p = 0; p < 8; p++) {
        const float x = 8.f + (float)(p % 4) * 57.f, y = 30.f + (float)(p / 4) * 70.f, w = 53.f, h = 62.f;
        const int ed = p == v->page, in = (v->loop >> p) & 1;
        const int st = v->pstart[p] > 15 ? 15 : v->pstart[p];
        const int en = v->pend[p] < st ? st : v->pend[p] > 15 ? 15 : v->pend[p];
        const qd_rgb tc = ed ? UI_K3 : in ? UI_G1 : UI_G2;
        ui_RR(x, y, w, h, 5.f, ed ? UI_K3 : in ? UI_G1 : UI_G3, ed ? 1.5f : 1.f, QD_NONE);
        ui_T(NUM[p], x + 8.f, y + 22.f, 18.f, ed ? 400 : 300, 0.f, tc, QD_LEFT);
        s[0] = 0;                                       /* its range */
        cat(cat(cat(s, ui_itoa(n, st + 1)), DASH), ui_itoa(n, en + 1));
        ui_T(s, x + 8.f, y + h - 9.f, 7.f, 600, 0.5f, st > 0 || en < 15 ? tc : UI_G2, QD_LEFT);
        for (k = 0; k < 16; k++) {                      /* its steps, a beat a row */
            const float dx = x + 27.f + (float)(k % 4) * 7.5f, dy = y + 15.f + (float)(k / 4) * 11.f;
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
    legend("TWICE", "LOOP IT ALONE", 181.f, 0);
    legend("HOLD + ANOTHER", "LOOP FROM ONE TO IT", 193.f, 0);
    legend("W", v->shift ? "CUT STEP" : "COPY STEP", 205.f, 0);
    for (k = 0; k < 8; k++)
        lab[k] = NUM[k];
    lab[8] = "PAGE", lab[9] = "TRACK", lab[10] = "UNDO";
    over_keys("CLEAR", 8, 9, UI_G1);
    ui_strip(lab, v->page >= 0 && v->page < 8 ? 1u << v->page : 0u, 0u, v->can_undo ? 0u : 1u << 10, hot, UI_K3);
}

/* ---------------------------------------------------------------- F2: the tracks (s17's speakers) */
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

static void f2(const ui_layer_t *v)
{
    const char *lab[11];
    unsigned faint = 0;
    char s[48];
    int i;

    for (i = 0; i < 8; i++) {
        const float x = 34.f + (float)(i % 4) * 57.f, y = 58.f + (float)(i / 4) * 70.f;
        const int mute = (v->mute >> i) & 1, solo = (v->solo >> i) & 1, sel = i == v->sel, held = i == v->held_track;
        const char *nm = v->tname[i] ? v->tname[i] : "";
        if (sel || held)
            ui_RR(x - 27.f, y - 24.f, 54.f, 68.f, 6.f, held ? UI_K3 : UI_G1, held ? 1.5f : 1.2f, QD_NONE);
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
    if (v->held_track >= 0 && v->held_track < 8) {     /* a track key held: what W1 and W2 do to it */
        s[0] = 0;
        cat(cat(cat(cat(s, "TRACK "), NUM[v->held_track]), ": W1 MUTE"), DOT);
        cat(s, "W2 SOLO");
        ui_T(s, 120.f, 192.f, 10.f, 600, 1.f, UI_K3, QD_CENTER);
    } else {
        legend("HOLD A TRACK +", "W1 MUTE \xc2\xb7 W2 SOLO", 190.f, 0);
    }
    legend("W", v->step_clip ? "PASTE THE COPIED STEP" : "PASTE STEP (NONE COPIED)", 204.f, !v->step_clip);
    for (i = 0; i < 8; i++)
        lab[i] = NUM[i];
    lab[8] = "COPY", lab[9] = "PASTE", lab[10] = 0;
    over_keys("PAGE", 8, 9, UI_G1);
    ui_strip(lab, v->sel >= 0 && v->sel < 8 ? 1u << v->sel : 0u, 0u,
             (faint & ~(1u << v->sel)) | (v->page_clip ? 0u : 1u << 9) | 1u << 10, 0u, UI_K3);
}

/* ---------------------------------------------------------------- F3: the page's range and the rate */
static float cx3(int i) { return 9.f + (float)i * 13.5f + (float)(i / 4) * 2.f; }

static void f3(const ui_layer_t *v)
{
    const int pg = v->page < 0 ? 0 : v->page > 7 ? 7 : v->page;
    const int st = v->pstart[pg] > 15 ? 15 : v->pstart[pg];
    const int en = v->pend[pg] < st ? st : v->pend[pg] > 15 ? 15 : v->pend[pg];
    const float top = 52.f, h = 50.f;
    const char *lab[11];
    char s[64], n[12];
    float x0, x1;
    int i;

    s[0] = 0;
    cat(cat(s, "PAGE "), NUM[pg]);
    ui_lab(s, 9.f, 38.f, UI_G1, QD_LEFT);
    ui_lab("W: THE END \xc2\xb7 W + W: START\xe2\x80\x93" "END", 230.f, 38.f, UI_G2, QD_RIGHT);
    for (i = 0; i < 16; i++) {                          /* the steps: in the range bright, out dark */
        const int in = i >= st && i <= en, an = i == v->anchor;
        const qd_rgb oc = an ? (v->blink ? UI_K3 : UI_G1) : in ? UI_G1 : 0x1c1c22u;
        ui_RR(cx3(i), top, 12.f, h, 3.f, oc, an ? 1.5f : 1.f, QD_NONE);
        if (in && v->trig[pg][i])
            ui_O(cx3(i) + 6.f, top + h - 10.f, 2.2f, QD_NONE, 1.f, UI_G1);
    }
    x0 = cx3(st) + 1.f, x1 = cx3(en) + 11.f;            /* the bracket over the range */
    line2(x0, top - 5.f, x1, top - 5.f, UI_K3, 1.5f);
    line2(x0, top - 5.f, x0, top - 1.f, UI_K3, 1.5f);
    line2(x1, top - 5.f, x1, top - 1.f, UI_K3, 1.5f);
    ui_T(ui_itoa(n, st + 1), cx3(st) + 6.f, top + h + 12.f, 8.f, 600, 0.f, UI_K3, QD_CENTER);
    if (en != st)
        ui_T(ui_itoa(n, en + 1), cx3(en) + 6.f, top + h + 12.f, 8.f, 600, 0.f, UI_K3, QD_CENTER);

    s[0] = 0;                                           /* the range and the rate, big */
    cat(cat(cat(s, ui_itoa(n, st + 1)), DASH), ui_itoa(n, en + 1));
    ui_T(s, 12.f, 160.f, 34.f, 200, 0.f, UI_K3, QD_LEFT);
    ui_lab("STEPS", 14.f, 174.f, UI_G1, QD_LEFT);
    if (v->rate_idx >= 0 && v->rate_idx < 9 && v->rate_names[v->rate_idx]) {
        ui_T(v->rate_names[v->rate_idx], 228.f, 160.f, 34.f, 200, 0.f, UI_K3, QD_RIGHT);
        ui_lab("RATE", 226.f, 174.f, UI_G1, QD_RIGHT);
    }
    s[0] = 0;
    cat(cat(cat(s, "SELECT: TEMPO "), ui_itoa(n, v->bpm)), DOT);
    cat(cat(cat(s, "SHIFT: SWING "), ui_itoa(n, v->swing)), "%");
    ui_lab(s, 120.f, 200.f, UI_G1, QD_CENTER);
    for (i = 0; i < 9; i++)
        lab[i] = v->rate_names[i];
    lab[9] = lab[10] = 0;
    ui_strip(lab, v->rate_idx >= 0 && v->rate_idx < 9 ? 1u << v->rate_idx : 0u, 0u, 3u << 9, 0u, UI_K3);
}

void ui_layer_draw(const ui_layer_t *v)
{
    static const char *const T[3] = {"F1 \xc2\xb7 PAGES", "F2 \xc2\xb7 TRACKS", "F3 \xc2\xb7 RANGE & RATE"};
    const int l = v->layer < 1 ? 1 : v->layer > 3 ? 3 : v->layer;
    ui_head_track(T[l - 1], v->track, v->name ? v->name : "", UI_K3);
    if (l == 1)
        f1(v);
    else if (l == 2)
        f2(v);
    else
        f3(v);
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
