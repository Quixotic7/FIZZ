/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The layer maps (PLAN.md §8b, version 2.1; P1.3v: §9a's clarity rules): while F1, F2, F3, SEL or SEQ is held the
 * screen is its map; while SHIFT is held, the controls SHIFT changes. VISUAL maps: the keys drawn where they are on the
 * panel, each with an icon or a number for what it does in this layer (ui_keymap), lit or coloured by its state; no
 * sentences, no legends (the gestures are in PLAYING.md). Text: the layer's name, the page / pattern / track numbers
 * and names, the values (12 px or more).
 *   F1 · PAGES        the eight pages as eight cells (4 x 2): the number big, the sixteen steps as a 4 x 4 of dots (a
 *                     trig filled, outside start..end only a speck); the page edited outlined white, the loop grey,
 *                     the others dark; the playing page a red line under its number (blinking); a copied page dashed
 *                     (blinking). The W keys' step copy: a white key with COPY (or PASTE, green, once a step is
 *                     copied). The keys: B1..B8 the pages (white in the loop; green to paste a copied page), B9 CLEAR,
 *                     B10 COPY (lit: a page copied), B11 UNDO (dark with none)
 *   F2 · CUT & PASTE  the page's sixteen steps (a trig a dot; the last W pressed outlined with CUT orange or PASTE
 *                     green under it) and the hand: a card holding the step (its notes on a little keybed, a LOCK step
 *                     a padlock, an empty step an empty cell) or a dashed empty card; beside it, big, what the next W
 *                     does (CUT, or PASTE)
 *   F3 · RANGE & RATE the page edited as sixteen cells with its start..end bracketed over them (the W held first
 *                     outlined, blinking) and its numbers under them, the range big and the rate big as a note value
 *                     with its dot or T mark, the tempo and swing; the keys: B1..B7 the note values as notes, B8
 *                     normal, B9 dotted, B10 triplet, the track's lit. Latched: a back arrow top right (OCT-)
 *   SEL · TRACKS      the eight tracks as speakers: the selected one boxed white, a muted one grey with an orange
 *                     cross, a soloed one an S in a yellow ring (blinking), the waves lit by a trig; the keys: B1..B8
 *                     the tracks (lit: selected, dark: muted), B11 CLEAR (bright while a mute or solo is set)
 *   SEQ · PATTERNS    (ui_patterns_draw) the queued pattern big (else the playing one) with PLAY / STOP, a line
 *                     emptying to the change, the bank's sixteen as dots, the banks A..D (B1..B4), the launch mode
 *   SHIFT             a row per control SHIFT changes here: the control (a knob's badge, REC, F3, SEQ + a key), an
 *                     arrow, the job's icon and its value now (the scale, the root, the swing) */
#include <math.h>
#include <string.h>

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

static const char *const NUM[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};
#define KM_Y 202.f                                      /* the key map's top, its height */
#define KM_H 30.f

static void keys_clear(ui_kkey_t k[11])
{
    int i;
    for (i = 0; i < 11; i++) {
        k[i].icon = UI_IC_NONE, k[i].num = 0, k[i].col = 0;
        k[i].lit = k[i].dim = k[i].mark = 0;
    }
}

/* ---------------------------------------------------------------- F1: the pages */
static void f1(const ui_layer_t *v)
{
    ui_kkey_t k[11];
    int p, i;

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
        ui_T(NUM[p], x + 8.f, y + 24.f, 20.f, ed ? 500 : 400, 0.f, tc, QD_LEFT);
        for (i = 0; i < 16; i++) {                      /* its steps, a beat a row (outside its range: specks) */
            const float dx = x + 27.f + (float)(i % 4) * 7.5f, dy = y + 14.f + (float)(i / 4) * 10.5f;
            if (i < st || i > en)
                ui_O(dx, dy, 0.9f, QD_NONE, 1.f, UI_G3);
            else if (v->trig[p][i])
                ui_O(dx, dy, 2.2f, QD_NONE, 1.f, ed ? UI_K3 : in ? UI_G1 : UI_G2);
            else
                ui_O(dx, dy, 1.6f, ed || in ? UI_G2 : UI_G3, 1.f, QD_NONE);
        }
        if (v->f1_copied && v->f1_src >= 0 && v->f1_src / 16 == p) {   /* P1.6h: F1's copy came from here: ringed */
            const int j = v->f1_src % 16;
            ui_O(x + 27.f + (float)(j % 4) * 7.5f, y + 14.f + (float)(j / 4) * 10.5f, 4.2f, UI_K2, 1.4f, QD_NONE);
        }
        if (p == v->play_page)                          /* playing: a red line under its number, blinking */
            line2(x + 8.f, y + 31.f, x + 21.f, y + 31.f, v->blink ? UI_RED : 0xa01c2au, 2.f);
    }
    /* the W keys' step copy: a white key and COPY, or PASTE (green) once a step is copied */
    ui_icon(UI_IC_KEY, 104.f, 178.f, 1.4f, UI_G1);
    ui_icon(v->f1_copied ? UI_IC_PASTE : UI_IC_COPY, 132.f, 178.f, 1.5f, v->f1_copied ? UI_K2 : UI_G1);
    keys_clear(k);
    for (p = 0; p < 8; p++) {
        k[p].num = p + 1;
        k[p].col = v->page_copied >= 0 ? UI_K2 : (v->loop >> p) & 1 ? UI_K3 : UI_G1;
        k[p].lit = p == v->page && v->page_copied < 0;
    }
    k[8].icon = UI_IC_CLEAR, k[8].col = UI_K4;
    k[9].icon = UI_IC_COPY, k[9].lit = v->page_copied >= 0;
    k[10].icon = UI_IC_UNDO, k[10].dim = !v->can_undo;
    ui_keymap(k, KM_Y, KM_H);
}

/* ---------------------------------------------------------------- F2: Form's pick-up / drop */
void ui_step_card(float x, float y, float w, float h, int full, int type, int nnote, const uint8_t *note)
{
    if (!full) {                                        /* empty-handed: a dashed card */
        static const float DSH[2] = {3.f, 3.f};
        qd_save();
        qd_dash(DSH, 2);
        ui_RR(x, y, w, h, 6.f, UI_G2, 1.2f, QD_NONE);
        qd_restore();
        return;
    }
    ui_RR(x, y, w, h, 6.f, UI_K3, 1.5f, QD_NONE);
    if (type == 0) {                                    /* an empty step: an empty cell */
        ui_icon(UI_IC_KEY, x + w / 2.f, y + h / 2.f, 2.4f * (w < 100.f ? w / 120.f : 1.f), UI_G1);
    } else if (type == 2) {                             /* a LOCK step: a padlock */
        const float s = w < 100.f ? 3.f * w / 120.f : 3.f;
        ui_lock(x + w / 2.f - 3.5f * s, y + h / 2.f - 4.f * s, UI_MODE_COL[UI_F_PLOCK], s);
    } else {                                            /* its notes on a little keybed */
        qd_rgb fill[128];
        int i, lo = 127;
        for (i = 0; i < 128; i++)
            fill[i] = QD_NONE;
        for (i = 0; i < nnote && i < 6; i++) {
            fill[note[i] & 127] = UI_K3;
            lo = note[i] < lo ? note[i] : lo;
        }
        ui_keyboard(x + 8.f, y + 10.f, w - 16.f, h - 20.f, lo / 12 * 12, fill);
    }
}
static void hand_card(const ui_layer_t *v, float x, float y, float w, float h)
{
    ui_step_card(x, y, w, h, v->hand, v->clip_type, v->clip_nnote, v->clip_note);
}

static float cx16(int i) { return 9.f + (float)i * 13.5f + (float)(i / 4) * 2.f; }

static void f2(const ui_layer_t *v)
{
    const float top = 34.f, h = 40.f;
    const int next_paste = v->hand_loaded && v->hand;
    int i;
    for (i = 0; i < 16; i++) {                          /* the page's steps */
        const int last = i == v->last_w;
        const qd_rgb oc = last ? (v->last_act == 1 ? UI_K4 : UI_K2) : UI_G2;
        ui_RR(cx16(i), top, 12.f, h, 3.f, oc, last ? 1.5f : 1.f, QD_NONE);
        if (v->page_on[i])
            ui_O(cx16(i) + 6.f, top + h - 10.f, 2.4f, QD_NONE, 1.f, last ? oc : UI_K3);
    }
    if (v->last_w >= 0 && v->last_w < 16)               /* what the last W did, under it */
        ui_icon(v->last_act == 1 ? UI_IC_CUT : UI_IC_PASTE, cx16(v->last_w) + 6.f, top + h + 14.f, 1.1f,
                v->last_act == 1 ? UI_K4 : UI_K2);
    hand_card(v, 40.f, 108.f, 120.f, 72.f);
    ui_icon(next_paste ? UI_IC_PASTE : UI_IC_CUT, 194.f, 144.f, 2.6f, next_paste ? UI_K2 : UI_K4);   /* the next W */
}

/* ---------------------------------------------------------------- F3: the page's range and the rate */
static void f3(const ui_layer_t *v)
{
    const int pg = v->page < 0 ? 0 : v->page > 7 ? 7 : v->page;
    const int st = v->pstart[pg] > 15 ? 15 : v->pstart[pg];
    const int en = v->pend[pg] < st ? st : v->pend[pg] > 15 ? 15 : v->pend[pg];
    const float top = 48.f, h = 44.f;
    ui_kkey_t k[11];
    char s[64], n[12];
    float x0, x1;
    int i;

    s[0] = 0;
    cat(cat(s, "PAGE "), NUM[pg]);
    ui_lab(s, 9.f, 38.f, UI_G1, QD_LEFT);
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
    ui_T(ui_itoa(n, st + 1), cx16(st) + 6.f, top + h + 15.f, 12.f, 600, 0.f, UI_K[2 - 1], QD_CENTER);
    if (en != st)
        ui_T(ui_itoa(n, en + 1), cx16(en) + 6.f, top + h + 15.f, 12.f, 600, 0.f, UI_K[4 - 1], QD_CENTER);

    s[0] = 0;                                           /* the range big */
    cat(cat(cat(s, ui_itoa(n, st + 1)), DASH), ui_itoa(n, en + 1));
    ui_T(s, 12.f, 152.f, 36.f, 200, 0.f, UI_K3, QD_LEFT);
    {   /* the rate big: the note value in KNOB 1's blue, its dot or T in KNOB 2's green */
        const char *val = v->rate_val_name ? v->rate_val_name : "1/16";
        const float mark = v->rate_type ? 14.f : 0.f, xr = 228.f - mark;
        ui_T(val, xr, 152.f, 36.f, 200, 0.f, UI_K1, QD_RIGHT);
        if (v->rate_type == 1)
            ui_O(xr + 6.f, 148.f, 3.4f, QD_NONE, 1.f, UI_K2);
        else if (v->rate_type == 2)
            ui_T("T", xr + 7.f, 134.f, 16.f, 500, 0.f, UI_K2, QD_CENTER);
    }
    s[0] = 0;                                           /* P1.3h: the tempo and swing, live (lit after a turn) */
    cat(cat(cat(s, v->ext ? "EXT " : ""), ui_itoa(n, v->bpm)), " BPM");
    cat(cat(cat(s, DOT), ui_itoa(n, v->swing)), "%");
    {
        const float w = ui_TW(s, 14.f, 500, 0.5f);
        if (v->tempo_hot > 0.f) {
            qd_save();
            qd_alpha(v->tempo_hot);
            ui_RR(120.f - w / 2.f - 6.f, 170.f, w + 12.f, 20.f, 5.f, UI_KSEL, 1.f, QD_NONE);
            qd_restore();
        }
        ui_T(s, 120.f, 185.f, 14.f, 500, 0.5f, v->tempo_hot > 0.f ? UI_KSEL : UI_K3, QD_CENTER);
    }
    keys_clear(k);                                      /* B1..B7 the note values, B8 normal, B9 dotted, B10 triplet */
    for (i = 0; i < 7; i++) {
        k[i].icon = UI_IC_NOTE + i;
        k[i].col = UI_K1;
        k[i].lit = i == v->rate_val;
    }
    k[7].icon = UI_IC_NOTE + 2, k[8].icon = UI_IC_DOT, k[9].icon = UI_IC_TRI;
    for (i = 7; i < 10; i++) {
        k[i].col = UI_K2;
        k[i].lit = i - 7 == v->rate_type;
    }
    ui_keymap(k, KM_Y, KM_H);
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

/* a name as big as fits in w (12 px, squeezed no narrower than 0.7) */
static void name_fit(const char *s, float cx, float y, float w, qd_rgb col)
{
    const float tw = ui_TW(s, 12.f, 600, 0.5f);
    if (tw <= w) {
        ui_T(s, cx, y, 12.f, 600, 0.5f, col, QD_CENTER);
        return;
    }
    qd_save();
    qd_translate(cx, y);
    qd_scale(w / tw < 0.7f ? 0.7f : w / tw, 1.f);
    ui_T(s, 0.f, 0.f, 12.f, 600, 0.5f, col, QD_CENTER);
    qd_restore();
}

static void tracks(const ui_layer_t *v)
{
    ui_kkey_t k[11];
    int i;

    for (i = 0; i < UI_NTRACK; i++) {                   /* (six tracks: three a row, 80 px each) */
        const float x = 40.f + (float)(i % 3) * 80.f, y = 56.f + (float)(i / 3) * 72.f;
        const int mute = (v->mute >> i) & 1, solo = (v->solo >> i) & 1, sel = i == v->sel;
        if (sel)
            ui_RR(x - 36.f, y - 24.f, 72.f, 70.f, 6.f, UI_K3, 1.5f, QD_NONE);
        speaker(x - 2.f, y, mute ? UI_G2 : UI_K3, v->flash[i] != 0, mute);
        ui_T(NUM[i], x, y + 28.f, 12.f, 600, 0.f, mute ? UI_G2 : UI_K3, QD_CENTER);
        name_fit(v->tname[i] ? v->tname[i] : "", x, y + 42.f, 68.f, mute ? UI_G2 : sel ? UI_K3 : UI_G1);
        if (ui_track_kind(i + 1) >= 0)                  /* P1.6d: its kind */
            ui_kind_icon(ui_track_kind(i + 1), x - 24.f, y - 13.f, 1.f, mute ? UI_G2 : UI_PK_COL[ui_track_kind(i + 1)]);
        if (solo) {                                     /* soloed: an S in a ring, blinking */
            ui_O(x + 24.f, y - 14.f, 8.f, SOLO_COL, 1.2f, v->blink ? SOLO_COL : QD_NONE);
            ui_T("S", x + 24.f, y - 9.5f, 12.f, 700, 0.f, v->blink ? UI_BG : SOLO_COL, QD_CENTER);
        }
    }
    keys_clear(k);
    for (i = 0; i < UI_NTRACK; i++) {
        k[i].num = i + 1;
        k[i].lit = i == v->sel;
        k[i].col = (v->mute >> i) & 1 ? UI_G2 : UI_K3;
    }
    k[10].icon = UI_IC_CLEAR, k[10].col = UI_K4, k[10].dim = !(v->mute || v->solo);
    ui_keymap(k, KM_Y, KM_H);
}

void ui_layer_draw(const ui_layer_t *v)
{
    static const char *const T[4] = {"F1 \xc2\xb7 PAGES", "F2 \xc2\xb7 CUT & PASTE", "F3 \xc2\xb7 RANGE & RATE",
                                     "SEL \xc2\xb7 TRACKS"};
    const int l = v->layer < 1 ? 1 : v->layer > 4 ? 4 : v->layer;
    if (l == 3 && v->latched) {                         /* P1.3d: a latched menu: a back arrow top right (OCT-) */
        ui_head_track(T[l - 1], v->track, "", UI_K3);
        ui_RR(180.f, 4.f, 24.f, 17.f, 4.f, UI_K3, 1.2f, QD_NONE);
        ui_icon(UI_IC_BACK, 192.f, 12.5f, 1.f, UI_K3);
    } else {
        ui_head_track(T[l - 1], v->track, v->name ? v->name : "", UI_K3);
    }
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
    static const char *const BANK[4] = {"A", "B", "C", "D"};
    const int bank = v->bank < 0 ? 0 : v->bank > 3 ? 3 : v->bank;
    const int big = v->queued >= 0 ? v->queued : v->cur;
    char s[8], n[12], hb[8];
    int i;

    hb[0] = 0;
    cat(cat(hb, "BANK "), BANK[bank]);
    ui_head("PATTERNS", hb, 0, UI_K3, UI_G1);
    s[0] = 0;                                           /* the queued one big (orange), else the playing one */
    if ((big & 15) + 1 < 10)
        cat(s, "0");
    cat(s, ui_itoa(n, (big & 15) + 1));
    ui_T(BANK[(big >> 4) & 3], 12.f, 100.f, 40.f, 200, 0.f, UI_G1, QD_LEFT);
    ui_T(s, 40.f, 100.f, 64.f, 200, 0.f, v->queued >= 0 ? UI_K4 : UI_K3, QD_LEFT);
    ui_icon(v->playing ? UI_IC_PLAY : UI_IC_STOP, 20.f, 124.f, 1.f, v->playing ? UI_K3 : UI_G1);   /* PLAY / STOP */
    if (v->queued >= 0) {                               /* queued: a line emptying to the change */
        line2(34.f, 124.f, 120.f, 124.f, UI_G3, 2.f);
        if (v->playing && v->left > 0.f)
            line2(34.f, 124.f, 34.f + 86.f * v->left, 124.f, UI_K4, 2.f);
    }
    for (i = 0; i < 16; i++) {                          /* the bank shown: sixteen dots */
        const int k = bank * 16 + i;
        const float x = 148.f + (float)(i % 4) * 22.f, y = 42.f + (float)(i / 4) * 20.f;
        if (k == v->cur)
            ui_O(x, y, 6.5f, QD_NONE, 1.f, UI_K3);
        else if (k == v->queued)
            ui_O(x, y, 6.5f, v->blink ? UI_K4 : UI_G2, 1.5f, UI_BG);
        else if ((v->used >> i) & 1u)
            ui_O(x, y, 6.5f, UI_G1, 1.2f, UI_BG);
        else
            ui_O(x, y, 1.5f, QD_NONE, 1.f, UI_G2);
    }
    for (i = 0; i < 4; i++) {                           /* the banks (SEQ + B1..B4): the one shown boxed */
        const float x = 148.f + (float)i * 22.f, y = 134.f;
        const int on = i == bank, has = (v->bank_used >> i) & 1u;
        const int playing_here = (v->cur >> 4) == i;
        if (on)
            ui_RR(x - 9.f, y - 13.f, 18.f, 18.f, 4.f, UI_K3, 1.2f, QD_NONE);
        ui_T(BANK[i], x, y + 1.f, 13.f, on ? 600 : 500, 0.f, on || has ? UI_K3 : UI_G2, QD_CENTER);
        if (playing_here)                               /* the playing pattern's bank: a dot under it */
            ui_O(x, y + 10.f, 1.4f, QD_NONE, 1.f, UI_K3);
    }
    ui_knob_mark(22.f, 176.f, (float)v->launch / 3.f, UI_G1);   /* the launch mode (ALGORITHM), the one in use white */
    for (i = 0; i < 4; i++) {
        const float x = 66.f + (float)i * 46.f;
        ui_T(LAUNCH[i], x, 181.f, 14.f, 500, 0.5f, i == v->launch ? UI_K3 : UI_G2, QD_CENTER);
        if (i == v->launch)
            line2(x - 12.f, 188.f, x + 12.f, 188.f, UI_K3, 1.5f);
    }
}

/* ---------------------------------------------------------------- SHIFT */
/* the control as on the panel: a knob's badge (S A P, as the knob band's), REC's red button, F3 (the two OCT keys), SEQ
 * with a white key */
static void control(int c, float cx, float cy)
{
    static const char *const BADGE[3] = {"S", "A", "P"};
    if (c >= UI_SC_SELECT && c <= UI_SC_PRESETS) {
        const qd_rgb bc = c == UI_SC_SELECT ? UI_KSEL : UI_K3;
        ui_O(cx, cy, 10.f, QD_NONE, 1.f, bc);
        ui_T(BADGE[c - UI_SC_SELECT], cx, cy + 4.5f, 12.f, 700, 0.f, UI_BG, QD_CENTER);
    } else if (c == UI_SC_REC) {
        ui_O(cx, cy, 10.f, UI_RED, 1.3f, QD_NONE);
        ui_O(cx, cy, 4.5f, QD_NONE, 1.f, UI_RED);
    } else if (c == UI_SC_F3_ALGO) {                    /* the two OCT keys, then ALGORITHM's badge */
        ui_RR(cx - 22.f, cy - 6.f, 9.f, 12.f, 2.f, UI_K3, 1.2f, QD_NONE);
        ui_RR(cx - 11.f, cy - 6.f, 9.f, 12.f, 2.f, UI_K3, 1.2f, QD_NONE);
        ui_O(cx + 10.f, cy, 10.f, QD_NONE, 1.f, UI_K3);
        ui_T("A", cx + 10.f, cy + 4.5f, 12.f, 700, 0.f, UI_BG, QD_CENTER);
    } else if (c == UI_SC_SEQ_W) {                      /* SEQ's button, then a white key */
        ui_RR(cx - 20.f, cy - 6.f, 18.f, 12.f, 3.f, UI_K3, 1.2f, QD_NONE);
        ui_icon(UI_IC_KEY, cx + 8.f, cy, 1.f, UI_K3);
    } else if (c == UI_SC_SEL || c == UI_SC_HOME || c == UI_SC_ARP) {   /* P1.3y: a button with its printed name */
        const char *nm = c == UI_SC_SEL ? "SEL" : c == UI_SC_ARP ? "ARP" : "HOME";
        const float w = ui_TW(nm, 12.f, 700, 0.5f) + 10.f;
        ui_RR(cx - w / 2.f, cy - 9.f, w, 18.f, 4.f, UI_K3, 1.2f, QD_NONE);
        ui_T(nm, cx, cy + 4.5f, 12.f, 700, 0.5f, UI_K3, QD_CENTER);
    }
}

/* P1.5c: SHIFT's black-key shortcuts, the keys drawn where they are, each its glyph (none: dark) */
static void shift_keys(const ui_shift_t *v)
{
    static const uint8_t SK[11] = {1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1};   /* (ctl_shift.c QD_SHIFT_KEYS; P1.5z: B8) */
    int i;
    for (i = 0; i < 11; i++) {
        const float x = 10.f + (float)i * 20.4f, cx = x + 9.f, cy = 210.f;
        const qd_rgb c = !SK[i] ? UI_G3 : (i == 2 && !v->snap_ok) || (i == 4 && !v->pclip_ok) ? UI_G2
                       : i == 10 && v->armed ? UI_K4 : i == 6 && v->metro ? UI_K2 : i == 7 && v->follow ? UI_K2 : UI_K3;
        ui_RR(x, 190.f, 18.f, 40.f, 4.f, c, SK[i] ? 1.4f : 1.f, QD_NONE);
        switch (i) {
        case 0:                                         /* the snapshot stored: a frame, its shutter, filled */
            ui_RR(cx - 6.f, cy - 4.f, 12.f, 9.f, 2.f, c, 1.2f, QD_NONE);
            ui_O(cx, cy + 0.5f, 2.2f, QD_NONE, 1.f, c);
            break;
        case 2:                                         /* .. recalled: the frame, an arrow out of it */
            ui_RR(cx - 6.f, cy - 1.f, 12.f, 9.f, 2.f, c, 1.2f, QD_NONE);
            line2(cx, cy - 10.f, cx, cy + 2.f, c, 1.2f);
            line2(cx - 3.f, cy - 7.f, cx, cy - 10.f, c, 1.2f);
            line2(cx + 3.f, cy - 7.f, cx, cy - 10.f, c, 1.2f);
            break;
        case 3: ui_icon(UI_IC_COPY, cx, cy, 1.f, c); break;
        case 4: ui_icon(UI_IC_PASTE, cx, cy, 1.f, c); break;
        case 5:                                         /* tap tempo: a tap and its ring */
            ui_O(cx, cy, 2.5f, QD_NONE, 1.f, c);
            ui_O(cx, cy, 6.5f, c, 1.2f, QD_NONE);
            break;
        case 6: {                                       /* the metronome */
            const float m[8] = {cx - 6.f, cy + 8.f, cx - 2.f, cy - 8.f, cx + 2.f, cy - 8.f, cx + 6.f, cy + 8.f};
            ui_P(m, 4, c, 1.2f, 1, QD_NONE);
            line2(cx, cy + 5.f, cx + 5.f, cy - 6.f, c, 1.2f);
            break;
        }
        case 7: ui_icon(UI_IC_FOLLOW, cx, cy, 0.9f, c); break;   /* P1.5z: page follow */
        case 8:                                         /* the track doubled: two pages */
            ui_RR(cx - 7.f, cy - 4.f, 6.f, 8.f, 1.f, c, 1.1f, QD_NONE);
            ui_RR(cx + 1.f, cy - 4.f, 6.f, 8.f, 1.f, c, 1.1f, QD_NONE);
            break;
        case 10: ui_icon(UI_IC_CLEAR, cx, cy, 1.f, c); break;
        default: break;
        }
    }
}

void ui_shift_draw(const ui_shift_t *v)
{
    const int n = v->n < 0 ? 0 : v->n > 9 ? 9 : v->n;
    const float sp = v->keys ? (n > 1 ? 136.f / (float)(n - 1) : 0.f)   /* (P1.5c: the keys below: the rows tighter) */
                   : n > 5 ? 182.f / (float)(n - 1 > 0 ? n - 1 : 1) : 40.f;
    const float y0 = v->keys ? 40.f : n > 5 ? 44.f : 58.f;
    const float big = v->keys ? 15.f : n > 5 ? 18.f : 20.f;
    int i;
    ui_head("SHIFT", 0, 0, UI_K3, UI_G1);
    for (i = 0; i < n; i++) {
        const float y = y0 + (float)i * sp;
        const float a[6] = {78.f, y - 4.f, 84.f, y, 78.f, y + 4.f};
        control(v->ctl[i], 34.f, y);
        line2(64.f, y, 84.f, y, UI_G2, 1.2f);
        ui_P(a, 3, UI_G2, 1.2f, 0, QD_NONE);
        ui_icon(v->icon[i], 106.f, y, 1.4f, UI_K3);
        if (v->val[i] && v->val[i][0]) {                /* its value now, as big as fits */
            const float w = ui_TW(v->val[i], big, 300, 0.f);
            const float sz = w > 104.f ? big * 104.f / w : big;
            ui_T(v->val[i], 128.f, y + 7.f, sz < 12.f ? 12.f : sz, sz < 16.f ? 500 : 300, 0.f, UI_K3, QD_LEFT);
        }
    }
    if (v->keys)
        shift_keys(v);
}

/* ---------------------------------------------------------------- P1.3y: the settings menus */
static const qd_rgb KCOL[4] = {UI_K1, UI_K2, UI_K3, UI_K4};

/* a row's head: the knob's badge, the row's name; its value big at the right, in the knob's colour */
static void menu_row(int k, float y, const char *name, const char *val)
{
    char n[4] = {(char)('1' + k), 0};
    ui_O(18.f, y, 9.f, QD_NONE, 1.f, KCOL[k]);
    ui_T(n, 18.f, y + 4.5f, 12.f, 700, 0.f, UI_BG, QD_CENTER);
    ui_lab(name, 34.f, y + 4.5f, UI_G1, QD_LEFT);
    if (val) {
        const float w = ui_TW(val, 30.f, 300, 0.f);
        const float sz = w > 110.f ? 30.f * 110.f / w : 30.f;
        ui_T(val, 232.f, y + 30.f, sz, 300, 0.f, KCOL[k], QD_RIGHT);
    }
}

static void menu_head(const char *title)
{
    ui_T(title, 8.f, 18.f, 12.f, 600, 0.8f, UI_K3, QD_LEFT);
    ui_RR(208.f, 4.f, 24.f, 17.f, 4.f, UI_K3, 1.2f, QD_NONE);   /* (latched: OCT- back) */
    ui_icon(UI_IC_BACK, 220.f, 12.5f, 1.f, UI_K3);
    line2(8.f, 27.f, 232.f, 27.f, UI_G3, 1.f);
}

/* P1.6d, the record settings' page 2: KNOB 1 AUTOMATION. A lane of eight steps under the moving playhead (its red dot):
 * ON, the knob's moves as bars over every step (the empty steps ghosts, outlined); OFF, the bars faint and crossed */
static void menu_rec2(const ui_menu_t *v)
{
    static const float LV[8] = {0.25f, 0.4f, 0.65f, 0.85f, 0.7f, 0.5f, 0.3f, 0.45f};
    static const uint8_t NOTE[8] = {1, 0, 0, 1, 0, 1, 0, 0};
    const int on = v->rec_auto;
    const float px = 34.f + fmodf(v->t * 44.f, 176.f);
    int i;
    menu_row(0, 44.f, "AUTOMATION", on ? "ON" : "OFF");
    for (i = 0; i < 8; i++) {
        const float x = 34.f + (float)i * 22.f, h = 56.f * LV[i];
        ui_RR(x + 5.f, 168.f - h, 8.f, h, 2.f, QD_NONE, 1.f, on ? UI_K1 : UI_G3);
        if (NOTE[i])
            ui_RR(x, 178.f, 18.f, 18.f, 3.f, UI_G1, 1.2f, UI_G1);
        else
            ui_RR(x, 178.f, 18.f, 18.f, 3.f, on ? UI_K1 : UI_G3, 1.2f, QD_NONE);   /* (ON: a ghost) */
    }
    if (!on)
        line2(30.f, 104.f, 210.f, 172.f, UI_RED, 1.6f);
    line2(px, 100.f, px, 200.f, on ? UI_RED : UI_G2, 1.2f);
    if (on)
        ui_O(px, 96.f, 4.5f, QD_NONE, 1.f, UI_RED);
}

static void menu_rec(const ui_menu_t *v)
{
    static const float OFF[4] = {7.f, -6.f, 4.f, -9.f};    /* the notes' places off their grid lines (px) */
    char q[12];
    int i;
    menu_head("REC");
    for (i = 0; i < 2; i++)                             /* P1.6d: two pages */
        ui_O(52.f + (float)i * 9.f, 14.f, 2.6f, i == v->rec_page ? UI_K3 : UI_G2, 1.f, i == v->rec_page ? UI_K3 : QD_NONE);
    if (v->rec_page) {
        menu_rec2(v);
        return;
    }
    /* KNOB 1: MODE (P1.5b: four rows, 52 px apart) */
    menu_row(0, 42.f, "MODE", v->rec_step ? "STEP" : "LIVE");
    if (v->rec_step) {                                  /* a row of cells, the cursor's lit */
        for (i = 0; i < 8; i++)
            ui_RR(34.f + (float)i * 13.f, 54.f, 11.f, 12.f, 2.f, i == 3 ? UI_K1 : UI_G2, 1.2f, i == 3 ? UI_K1 : QD_NONE);
    } else {                                            /* a line, notes off the grid, a red dot */
        line2(34.f, 66.f, 134.f, 66.f, UI_G2, 1.2f);
        for (i = 0; i < 4; i++) {
            line2(40.f + (float)i * 26.f, 58.f, 40.f + (float)i * 26.f, 70.f, UI_G3, 1.f);
            ui_O(40.f + (float)i * 26.f + OFF[i] * 0.6f, 59.f - (float)(i % 2) * 3.f, 3.f, QD_NONE, 1.f, UI_K1);
        }
        ui_O(146.f, 60.f, 4.f, QD_NONE, 1.f, UI_RED);
    }
    /* KNOB 2: QUANTIZE */
    cat(ui_itoa(q, v->quant), "%");
    menu_row(1, 94.f, "QUANTIZE", q);
    line2(34.f, 120.f, 140.f, 120.f, UI_G2, 1.2f);
    for (i = 0; i < 4; i++) {                           /* the grid lines, the notes pulled onto them by q % */
        const float gx = 44.f + (float)i * 28.f;
        const float nx = gx + OFF[i] * (float)(100 - v->quant) / 100.f;
        line2(gx, 106.f, gx, 124.f, UI_G1, 1.f);
        if (v->quant < 100)
            ui_O(gx + OFF[i], 113.f, 3.5f, UI_G2, 1.f, QD_NONE);
        ui_O(nx, 113.f, 3.5f, QD_NONE, 1.f, UI_K2);
    }
    /* KNOB 3: the TAKE: CAPTURE / REPLACE / OVERDUB */
    menu_row(2, 146.f, "TAKE", v->dub == 0 ? "CAPTURE" : v->dub == 1 ? "OVERDUB" : "REPLACE");
    if (v->dub == 0) {                                  /* CAPTURE: the steps played, cut at the take's end (red) */
        for (i = 0; i < 6; i++)
            ui_RR(34.f + (float)i * 14.f, 160.f, 10.f, 20.f, 2.f, i < 4 ? UI_G1 : UI_G3, 1.2f,
                  i < 4 && i != 2 ? UI_K3 : QD_NONE);
        line2(88.f, 154.f, 88.f, 186.f, UI_RED, 2.f);
    } else {
        ui_RR(34.f, 158.f, 40.f, 24.f, 4.f, UI_G2, 1.2f, QD_NONE);   /* a step: its notes, the new one */
        for (i = 0; i < 2; i++) {
            const float y = 174.f - (float)i * 7.f;
            ui_RR(39.f, y, 30.f, 5.f, 2.f, QD_NONE, 1.f, v->dub == 1 ? UI_G1 : UI_G3);
            if (v->dub != 1)
                line2(37.f, y + 6.f, 71.f, y - 1.f, UI_RED, 1.4f);
        }
        ui_RR(39.f, 160.f, 30.f, 5.f, 2.f, QD_NONE, 1.f, UI_K3);
        ui_RR(84.f, 167.f, 10.f, 6.f, 2.f, QD_NONE, 1.f, UI_K3);   /* the note coming in */
        {
            const float a[6] = {102.f, 167.f, 96.f, 170.f, 102.f, 173.f};
            line2(96.f, 170.f, 114.f, 170.f, UI_K3, 1.2f);
            ui_P(a, 3, UI_K3, 1.2f, 0, QD_NONE);
        }
    }
    /* KNOB 4 (P1.5b): EDIT KEYS: the sound editor's keyboard, SCALE (eight keys, all in the scale, the root brighter;
     * the black ones its pages) or FULL (the keybed) */
    menu_row(3, 198.f, "EDIT KEYS", v->ekeys ? "FULL" : "SCALE");
    for (i = 0; i < 8; i++) {
        const float x = 34.f + (float)i * 13.f;
        ui_RR(x, 214.f, 11.f, 18.f, 2.f, v->ekeys ? UI_G1 : (i % 7 == 0 ? UI_K4 : UI_G1), 1.2f,
              !v->ekeys && i % 7 == 0 ? UI_K4 : QD_NONE);
    }
    for (i = 0; i < 7; i++) {
        static const uint8_t B[7] = {1, 1, 0, 1, 1, 1, 0};
        if (B[i])
            ui_RR(34.f + (float)i * 13.f + 8.f, 210.f, 8.f, 10.f, 2.f, QD_NONE, 1.f, v->ekeys ? UI_G1 : UI_G3);
    }
}

/* P1.6d: automation recording: the screen framed in red, pulsing, while the knobs record; the knob that wrote last
 * a red dot on its band at the foot (KNOB 1..4's cells; an encoder: the frame flashes), fading */
void ui_auto_cue(int k, float since, float t)
{
    const float pulse = 0.5f + 0.5f * sinf(t * 6.f);
    const float al = since < 0.f ? 0.f : since < 0.3f ? 1.f : since < 0.9f ? 1.f - (since - 0.3f) / 0.6f : 0.f;
    qd_save();
    qd_alpha(0.45f + 0.35f * pulse + (k < 0 || k > 3 ? 0.2f * al : 0.f));
    ui_RR(1.f, 1.f, 238.f, 238.f, 8.f, UI_RED, 2.f, QD_NONE);
    qd_restore();
    if (al > 0.f && k >= 0 && k < 4) {
        qd_save();
        qd_alpha(al);
        ui_O(30.f + (float)k * 60.f, 235.f, 4.f, UI_BG, 1.5f, UI_RED);
        qd_restore();
    }
}

/* P1.5b: LFO held alone: the latch mode. Two drawings side by side, the chosen one lit in LFO's colour: VALUES (a step
 * selected, its value on the black keys: a bar of ten, one lit) and MODES (the black keys as the hold modes' colours,
 * the knobs turning the step's value) */
void ui_lmode_draw(int mode, float t)
{
    int side, i;
    (void)t;
    qd_fill_rect(0.f, 0.f, (float)QD_W, (float)QD_H, UI_BG);
    for (side = 0; side < 2; side++) {
        const float x0 = 12.f + (float)side * 112.f;
        const int on = side == mode;
        const qd_rgb c = on ? UI_K3 : UI_G2;
        ui_RR(x0, 30.f, 104.f, 156.f, 10.f, c, on ? 2.f : 1.f, QD_NONE);
        for (i = 0; i < 4; i++)                         /* the step selected: a row of four, one held */
            ui_RR(x0 + 14.f + (float)i * 20.f, 48.f, 16.f, 16.f, 3.f, i == 1 ? c : UI_G3, 1.2f, i == 1 ? c : QD_NONE);
        for (i = 0; i < 10; i++) {                      /* the black keys */
            const float bx = x0 + 9.f + (float)i * 9.2f;
            qd_rgb kc = UI_G3;
            if (side == 0)
                kc = i == 6 ? (on ? UI_K1 : UI_G1) : on ? UI_G2 : UI_G3;   /* VALUES: one value lit */
            else
                kc = on ? UI_MODE_COL[i] : UI_G2;       /* MODES: the hold modes' colours */
            ui_RR(bx, 82.f, 7.f, 30.f, 2.f, QD_NONE, 1.f, kc);
        }
        if (side == 0) {                                /* VALUES: the step's value, a bar */
            ui_RR(x0 + 14.f, 126.f, 76.f, 10.f, 3.f, c, 1.2f, QD_NONE);
            ui_RR(x0 + 14.f, 126.f, 52.f, 10.f, 3.f, QD_NONE, 1.f, on ? UI_K1 : UI_G2);
        } else {                                        /* MODES: a knob turning the step's value */
            ui_O(x0 + 52.f, 136.f, 15.f, c, 1.6f, QD_NONE);
            line2(x0 + 52.f, 136.f, x0 + 52.f + 10.f, 136.f - 10.f, on ? UI_K1 : UI_G2, 2.f);
        }
        ui_T(side ? "MODES" : "VALUES", x0 + 52.f, 172.f, 14.f, 600, 0.8f, c, QD_CENTER);
    }
    ui_T("LATCH", 120.f, 218.f, 20.f, 300, 1.f, UI_K3, QD_CENTER);
}

/* the views' little pictures (a line of the KNOBS roles): the PIANO ROLL's bars, STEPS' rows */
static void view_pic(int rows, float x, float y)
{
    if (rows) {
        int i;
        for (i = 0; i < 4; i++)
            ui_RR(x + (float)i * 5.f, y - 6.f, 4.f, 5.f, 1.f, QD_NONE, 1.f, UI_G1);
        for (i = 0; i < 4; i++)
            ui_RR(x + (float)i * 5.f, y + 1.f, 4.f, 5.f, 1.f, QD_NONE, 1.f, UI_G2);
    } else {
        ui_RR(x, y - 6.f, 8.f, 3.f, 1.f, QD_NONE, 1.f, UI_G1);
        ui_RR(x + 6.f, y - 1.f, 9.f, 3.f, 1.f, QD_NONE, 1.f, UI_G1);
        ui_RR(x + 11.f, y + 4.f, 8.f, 3.f, 1.f, QD_NONE, 1.f, UI_G1);
    }
}

static void menu_set(const ui_menu_t *v)
{
    static const char *const BADGE[3] = {"S", "A", "P"};
    static const float EX[3] = {70.f, 136.f, 202.f};
    char lay[2] = {(char)('A' + (v->layout < 0 ? 0 : v->layout > 3 ? 3 : v->layout)), 0};
    const int two = v->role[0][0] != v->role[1][0] || v->role[0][1] != v->role[1][1] || v->role[0][2] != v->role[1][2];
    int k, r;
    menu_head("SETTINGS");
    /* KNOB 1: KNOBS, the encoders and their roles (no finger on a step) */
    menu_row(0, 44.f, "KNOBS", lay);
    for (k = 0; k < 3; k++) {
        const qd_rgb bc = k == 0 ? UI_KSEL : UI_K3;
        ui_O(EX[k], 92.f, 12.f, QD_NONE, 1.f, bc);
        ui_T(BADGE[k], EX[k], 96.5f, 12.f, 700, 0.f, UI_BG, QD_CENTER);
    }
    for (r = 0; r < (two ? 2 : 1); r++) {
        const float y = 124.f + (float)r * 20.f;
        if (two)
            view_pic(r, 14.f, y - 4.5f);
        for (k = 0; k < 3; k++)
            ui_T(v->role[two ? r : 1][k], EX[k], y, 12.f, 600, 0.5f, UI_K1, QD_CENTER);
    }
    /* KNOB 2: PAGE FOLLOW */
    menu_row(1, 172.f, "PAGE FOLLOW", v->follow ? "ON" : "OFF");
    {
        const float px = 34.f + fmodf(v->t * 22.f, 88.f);
        for (k = 0; k < 4; k++)
            ui_RR(34.f + (float)k * 22.f, 192.f, 20.f, 22.f, 3.f, v->follow && (int)((px - 34.f) / 22.f) == k
                  ? UI_K2 : UI_G2, 1.2f, QD_NONE);
        line2(px, 188.f, px, 218.f, UI_RED, 1.4f);
        line2(34.f, 226.f, 120.f, 226.f, v->follow ? UI_K2 : UI_G2, 1.2f);
        {
            const float a[6] = {114.f, 222.f, 120.f, 226.f, 114.f, 230.f};
            ui_P(a, 3, v->follow ? UI_K2 : UI_G2, 1.2f, 0, QD_NONE);
        }
        if (!v->follow)
            line2(70.f, 220.f, 84.f, 232.f, UI_RED, 1.4f);
    }
}

/* P1.4b: the track settings: the track's number and name in its colour; page 1 SCALE (its twelve pitch classes as
 * dots, the scale's filled, the root ringed), ROOT, FOLLOW KEY, MIDI CH; page 2 VOICES (sixteen dots: the limit lit,
 * the ones sounding white), SWING, PAGE FOLLOW */
static const char *const KIND_NAME[7] = {"BASS", "KEYS", "PLUCK", "PAD", "LEAD", "DRUMS", "FX"};
static void menu_trk(const ui_menu_t *v)
{
    const qd_rgb tc = UI_TRACK_PAL[(v->track - 1) & 7];
    char n[12];
    int k;
    ui_box(8.f, 5.f, v->track, tc);
    if (ui_track_kind(v->track) >= 0)                   /* P1.6d: its kind */
        ui_kind_icon(ui_track_kind(v->track), 38.f, 13.f, 1.f, UI_PK_COL[ui_track_kind(v->track)]);
    ui_T(v->tname ? v->tname : "", ui_track_kind(v->track) >= 0 ? 48.f : 32.f, 18.f, 12.f, 600, 0.8f, tc, QD_LEFT);
    for (k = 0; k < 3; k++)
        ui_O(170.f + (float)k * 9.f, 14.f, 2.6f, k == v->page ? UI_K3 : UI_G2, 1.f, k == v->page ? UI_K3 : QD_NONE);
    ui_RR(208.f, 4.f, 24.f, 17.f, 4.f, UI_K3, 1.2f, QD_NONE);
    ui_icon(UI_IC_BACK, 220.f, 12.5f, 1.f, UI_K3);
    line2(8.f, 27.f, 232.f, 27.f, UI_G3, 1.f);
    if (!v->page) {
        menu_row(0, 44.f, "SCALE", v->scale);
        for (k = 0; k < 12; k++) {                      /* C .. B: the scale's notes */
            const float x = 38.f + (float)k * 9.f, y = 88.f;
            const int in = (v->scale_mask >> k) & 1;
            ui_O(x, y, 3.f, in ? UI_K1 : UI_G3, 1.f, in ? UI_K1 : QD_NONE);
            if (k == v->root_pc)
                ui_O(x, y, 5.5f, UI_K2, 1.3f, QD_NONE);
        }
        menu_row(1, 106.f, "ROOT", v->root);
        menu_row(2, 152.f, "FOLLOW KEY", v->follow_key ? "ON" : "OFF");
        menu_row(3, 198.f, "MIDI CH", ui_itoa(n, v->chan));
    } else if (v->page == 2) {                          /* P1.6d: KIND (the seven icons, its own lit), NAME */
        menu_row(0, 44.f, "KIND", v->tkind >= 0 ? KIND_NAME[v->tkind] : "NONE");
        for (k = 0; k < 7; k++) {
            const float x = 40.f + (float)k * 27.f;
            const int on = k == v->tkind;
            if (on)
                ui_RR(x - 12.f, 84.f, 24.f, 24.f, 5.f, UI_PK_COL[k], 1.4f, QD_NONE);
            ui_kind_icon(k, x, 96.f, 1.f, on ? UI_PK_COL[k] : UI_G2);
        }
        menu_row(1, 124.f, "NAME", v->tname ? v->tname : "");
        for (k = 0; k < 12; k++) {                      /* its twelve places, the ones used lit (typed: KNOB 2's) */
            const int used = v->tname && k < (int)strlen(v->tname);
            ui_RR(36.f + (float)k * 10.f, 178.f, 8.f, 10.f, 2.f, used ? (v->named ? KCOL[1] : UI_G1) : UI_G3, 1.f,
                  used && v->named ? KCOL[1] : QD_NONE);
        }
    } else {
        menu_row(0, 44.f, "VOICES", ui_itoa(n, v->limit));
        for (k = 0; k < 16; k++)                        /* the limit lit, the voices sounding white */
            ui_O(38.f + (float)k * 7.f, 72.f, 2.4f, QD_NONE, 1.f,
                 k < v->voices_now ? UI_K3 : k < v->limit ? UI_K1 : UI_G3);
        menu_row(1, 98.f, "SWING", v->swing ? "ON" : "OFF");
        ui_icon(UI_IC_SWING, 46.f, 126.f, 1.4f, v->swing ? UI_K2 : UI_G2);
        menu_row(2, 152.f, "PAGE FOLLOW", v->follow ? "ON" : "OFF");
        menu_row(3, 198.f, "MIDI FX", v->mfx ? 0 : "OFF");   /* P1.6c: the track's group, a hexagon */
        if (v->mfx)
            ui_mfx_hex(214.f, 220.f, 13.f, v->mfx, KCOL[3], 1.6f, 0);
        else
            ui_mfx_hex(46.f, 222.f, 9.f, 0, UI_G2, 1.2f, 1);
    }
    if (v->last >= 0 && v->last < 4) {                  /* the row turned last: its badge ringed */
        static const float Y0[3][4] = {{44.f, 106.f, 152.f, 198.f}, {44.f, 98.f, 152.f, 198.f},
                                       {44.f, 124.f, -40.f, -40.f}};
        ui_O(18.f, Y0[v->page < 0 || v->page > 2 ? 0 : v->page][v->last], 12.f, KCOL[v->last], 1.2f, QD_NONE);
    }
}

void ui_menu_draw(const ui_menu_t *v)
{
    if (v->kind == UI_MENU_REC)
        menu_rec(v);
    else if (v->kind == UI_MENU_TRK)
        menu_trk(v);
    else
        menu_set(v);
}

/* ---------------------------------------------------------------- P1.4a: the sound editor */
#include "ui_snd.c"

/* ---------------------------------------------------------------- P1.5a: the sounds page, names, the projects */
#include <string.h>
#include "ui_lib.c"
#include "ui_presets.c"                                 /* P1.5t: the library browser */
#include "ui_type.c"                                    /* P1.5u: the typewriter (SAVE PRESET, a project's name) */
#include "ui_mfx.c"                                     /* P1.6c: the MIDI FX screen */
#include "ui_geek.c"                                    /* P2.11: the Geek Out pages */
#include "ui_tools.c"                                   /* T1: the track tools */
