/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.4b: THE MIX and THE EFFECTS (ui.h ui_mix_t, ui_fx_t; mockups/line.html s14, s15). The file keeps its name from
 * P1.3's tracks screen of eight clocks, retired in P1.4b (its LENGTH and RATE are F3's, LIMIT and SWING the track
 * settings', its mutes SEL's). PLAN §9a's clarity rules: the values and the short names over them, the page's name;
 * the rest is drawing, colour and motion. */
#include <math.h>
#include <string.h>

#include "ui.h"

static const qd_rgb FK[4] = {UI_K1, UI_K2, UI_K3, UI_K4};

static void ln(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, col, lw, 0, QD_NONE);
}
static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

/* the head: the title, an effect's pages as dots; P1.6h: no back arrow (F1 is no back on these main pages), the steps
 * held or selected as a magenta count (their locks are what the knobs write) */
static void panel_head(const char *title, int npg, int ipg, int nsel)
{
    const float w = ui_T(title, 8.f, 18.f, 12.f, 600, 0.8f, UI_K3, QD_LEFT);
    int k;
    for (k = 0; k < npg && npg > 1; k++)
        ui_O(8.f + w + 10.f + (float)k * 9.f, 14.f, 2.6f, k == ipg ? UI_K3 : UI_G2, 1.f, k == ipg ? UI_K3 : QD_NONE);
    if (nsel > 0) {
        char n[12];
        ui_chip(ui_itoa(n, nsel), 220.f, 17.f, 13.f, 24.f, UI_STEP_FRAME, 1);
    }
}

/* P1.6h: a value the knob writes as a lock on the held steps: a filled chip on the part it moves */
static void lock_chip(float x, float y, qd_rgb col)
{
    ui_RR(x - 6.f, y - 5.f, 12.f, 10.f, 3.f, UI_BG, 1.4f, col);
    ui_RR(x - 3.f, y - 1.5f, 6.f, 3.f, 1.f, QD_NONE, 1.f, UI_BG);
}

/* ---------------------------------------------------------------- the mix */
void ui_mix_draw(const ui_mix_t *v)
{
    static const float DASH[2] = {2.f, 3.f};
    int i;
    panel_head(v->page ? "SENDS" : "MIX", 2, v->page ? 1 : 0, v->nsel);
    for (i = 0; i < 8; i++) {
        const float x = 22.f + (float)i * 28.f;
        const int sel = i == v->sel, mute = v->tr[i].mute;
        const qd_rgb col = mute ? UI_G3 : sel ? UI_K3 : UI_G1;
        char n[4] = {(char)('1' + i), 0};
        if (!v->page) {
            const float a = -UI_PI * 0.5f + (float)v->tr[i].pan / 64.f * 2.2f;
            const float top = 64.f, bot = 176.f, fy = bot - (bot - top) * (float)v->tr[i].level / 127.f;
            ui_O(x, 44.f, 7.5f, sel ? UI_K2 : col, 1.2f, QD_NONE);   /* PAN */
            ln(x, 44.f, x + cosf(a) * 6.5f, 44.f + sinf(a) * 6.5f, sel ? UI_K2 : col, 1.4f);
            if (mute)
                qd_dash(DASH, 2);
            ln(x, top, x, bot, UI_G3, 1.f);
            if (mute)
                qd_dash(0, 0);
            if (!mute && v->tr[i].meter > 0.f)          /* the meter */
                ln(x + 5.f, bot, x + 5.f, bot - (bot - top) * clampf(v->tr[i].meter, 0.f, 1.f), UI_G2, 2.f);
            ln(x, bot, x, fy, sel ? UI_K1 : col, 1.6f);   /* LEVEL */
            ui_RR(x - 7.f, fy - 3.f, 14.f, 6.f, 2.f, sel ? UI_K1 : col, 1.2f, UI_BG);
            {   /* the sends: DELAY, REVERB */
                int j;
                for (j = 0; j < 2; j++) {
                    const float yy = 216.f + (float)j * 8.f, f = (float)(j ? v->tr[i].reverb : v->tr[i].delay) / 127.f;
                    ln(x - 9.f, yy, x + 9.f, yy, UI_G3, 2.f);
                    if (f > 0.f)
                        ln(x - 9.f, yy, x - 9.f + 18.f * f, yy, mute ? UI_G3 : sel ? (j ? UI_K4 : UI_K3) : UI_G2, 2.f);
                }
            }
            if (sel && v->last >= 0)                    /* the knob turned last: its part brightened */
                ln(x - 9.f, 234.f, x + 9.f, 234.f, FK[v->last], 2.f);
            if (sel) {                                  /* P1.6h: the parts locked on the held step: a chip on each */
                if (v->locked[0])
                    lock_chip(x, fy, FK[0]);
                if (v->locked[1])
                    lock_chip(x, 44.f, FK[1]);
                if (v->locked[2])
                    lock_chip(x + 13.f, 216.f, FK[2]);
                if (v->locked[3])
                    lock_chip(x + 13.f, 224.f, FK[3]);
            }
        } else {
            int j;
            for (j = 0; j < 3; j++) {                   /* CHORUS, DELAY, REVERB: three bars a track */
                const float bx = x - 9.f + (float)j * 7.f, top = 46.f, bot = 176.f;
                const int val = j == 0 ? v->tr[i].chorus : j == 1 ? v->tr[i].delay : v->tr[i].reverb;
                const float h = (bot - top) * (float)val / 127.f;
                ui_RR(bx - 2.5f, top, 5.f, bot - top, 2.f, UI_G3, 1.f, QD_NONE);
                if (h > 0.f)
                    ui_RR(bx - 2.5f, bot - h, 5.f, h, 2.f, QD_NONE, 1.f, mute ? UI_G3 : sel ? FK[j] : UI_G1);
                if (sel && v->locked[j])                /* P1.6h: locked on the held step: a chip on the bar */
                    lock_chip(bx, bot - h, FK[j]);
            }
        }
        if (ui_track_kind(i + 1) >= 0)                  /* P1.6d: its kind, over the faders */
            ui_kind_icon(ui_track_kind(i + 1), x, 31.f, 0.75f, mute ? UI_G3 : UI_PK_COL[ui_track_kind(i + 1)]);
        if (mute) {
            ln(x - 4.f, 190.f, x + 4.f, 198.f, UI_K4, 1.4f);
            ln(x + 4.f, 190.f, x - 4.f, 198.f, UI_K4, 1.4f);
        } else {
            ui_T(n, x, 200.f, 12.f, sel ? 700 : 500, 0.f, sel ? UI_K3 : UI_G1, QD_CENTER);
        }
        if (sel)
            ui_RR(x - 11.f, 186.f, 22.f, 19.f, 4.f, UI_K3, 1.2f, QD_NONE);
    }
    if (v->page) {                                      /* the sends' names, once, under the selected track's bars */
        static const char *const S[3] = {"CHORUS", "DELAY", "REVERB"};
        int j;
        for (j = 0; j < 3; j++)
            ui_T(S[j], 40.f + (float)j * 80.f, 226.f, 12.f, 600, 0.5f, FK[j], QD_CENTER);
    }
    {   /* P1.6d: the locks as chips in the head (KNOB 1..4's colours), the step's frame */
        int k;
        for (k = 0; k < 4; k++)
            if (v->locked[k] || v->lockany[k])
                ui_chip(v->ltxt[k], 98.f + (float)k * 30.f, 18.f, 12.f, 27.f, FK[k], v->locked[k]);
        if (v->step)
            ui_RR(0.f, 0.f, 240.f, 240.f, 8.f, UI_STEP_FRAME, 3.f, QD_NONE);
    }
}

/* ---------------------------------------------------------------- the effects */
static void fx_delay(const ui_fx_t *v)
{
    const float half = 18.f + (1.f - v->spread) * 70.f;    /* SPREAD: the walls' distance */
    const float wl = 120.f - half - 20.f, wr = 120.f + half + 20.f;
    const int n = 7;
    float pts[2 * 7];
    int i;
    ln(wl, 40.f, wl, 180.f, UI_K2, 1.5f);
    ln(wr, 40.f, wr, 180.f, UI_K2, 1.5f);
    if (v->send > 0.f) {                                /* the selected track's SEND coming in */
        const float a[6] = {wl - 14.f, 40.f + 6.f, wl - 4.f, 50.f, wl - 14.f, 52.f};
        ln(wl - 30.f, 36.f, wl - 4.f, 50.f, UI_K4, 1.f + 2.f * v->send);
        ui_P(a, 3, UI_K4, 1.5f, 0, QD_NONE);
    }
    for (i = 0; i < n; i++) {
        pts[2 * i] = v->ping ? (i % 2 ? wr - 10.f : wl + 10.f) : wl + 10.f + (float)i * (wr - wl - 20.f) / (float)(n - 1);
        pts[2 * i + 1] = 56.f + (float)i * 20.f;
    }
    {
        static const float DASH[2] = {1.f, 4.f};
        qd_dash(DASH, 2);
        ui_P(pts, n, UI_G3, 1.f, 0, QD_NONE);
        qd_dash(0, 0);
    }
    for (i = 0; i < n; i++) {                           /* FEEDBACK: the echoes fading */
        const float a = powf(0.2f + 0.78f * v->fdbk, (float)i);
        qd_save();
        qd_alpha(clampf(a, 0.05f, 1.f));
        ui_O(pts[2 * i], pts[2 * i + 1], fmaxf(1.8f, 7.f - (float)i * 0.8f), UI_K3, 1.3f, QD_NONE);
        qd_restore();
    }
    {   /* TIME: the ball, a bounce a delay time */
        const float per = v->delay_s > 0.02f ? v->delay_s : 0.02f, u = fmodf(v->t / per, (float)(n - 1));
        const int k = (int)u;
        const float f = u - (float)k;
        ui_O(pts[2 * k] + (pts[2 * k + 2] - pts[2 * k]) * f, pts[2 * k + 1] + (pts[2 * k + 3] - pts[2 * k + 1]) * f,
             5.f, QD_NONE, 1.f, UI_K1);
    }
}

static void fx_delay2(const ui_fx_t *v)
{
    int r, i;
    for (r = 0; r < 2; r++) {                           /* L and R: STEREO both rows, PING-PONG alternating */
        const float y = 70.f + (float)r * 50.f;
        float p[2 * 41];
        for (i = 0; i <= 40; i++) {                     /* WOW: the line's wobble */
            p[2 * i] = 20.f + (float)i * 5.f;
            p[2 * i + 1] = y + sinf((float)i * 0.6f + v->t * 3.f + (float)r) * v->wow * 5.f;
        }
        ui_P(p, 41, UI_G3, 1.f, 0, QD_NONE);
        ui_T(r ? "R" : "L", 10.f, y + 4.5f, 12.f, 600, 0.f, UI_G1, QD_CENTER);
        for (i = 0; i < 6; i++) {
            const float x = 30.f + (float)i * 34.f;
            const int on = v->ping ? (i % 2 == r) : 1;
            const float b = powf(0.75f, (float)i) * (0.35f + 0.65f * v->tone);
            if (!on)
                continue;
            qd_save();
            qd_alpha(clampf(b, 0.08f, 1.f));
            ui_O(x, y, 6.f, QD_NONE, 1.f, UI_K2);
            qd_restore();
        }
    }
    if (v->sync)                                        /* SYNC: on the tempo's grid (beat ticks) */
        for (i = 0; i < 7; i++)
            ln(30.f + (float)i * 34.f, 156.f, 30.f + (float)i * 34.f, i % 2 ? 162.f : 168.f, UI_K1, 1.5f);
    else
        ui_icon(UI_IC_NOW, 120.f, 162.f, 1.4f, UI_K1);
}

static void fx_reverb(const ui_fx_t *v)
{
    const float s = 0.35f + 0.65f * v->size, w = 150.f * s, h = 100.f * s, x0 = 120.f - w * 0.5f, y0 = 112.f - h * 0.5f;
    const float d = 18.f * s;
    int i;
    ui_RR(x0, y0, w, h, 2.f, UI_K1, 1.5f, QD_NONE);     /* SIZE: the room */
    ln(x0, y0, x0 + d, y0 + d, UI_G2, 1.f);
    ln(x0 + w, y0, x0 + w - d, y0 + d, UI_G2, 1.f);
    ln(x0, y0 + h, x0 + d, y0 + h - d, UI_G2, 1.f);
    ln(x0 + w, y0 + h, x0 + w - d, y0 + h - d, UI_G2, 1.f);
    ui_RR(x0 + d, y0 + d, w - 2.f * d, h - 2.f * d, 1.f, UI_G2, 1.f, QD_NONE);
    for (i = 0; i < 9; i++) {                           /* the reflections, darker with DAMP */
        const float u = fmodf(v->t * 0.6f + (float)i * 0.37f, 1.f);
        qd_save();
        qd_alpha((1.f - u) * (1.f - 0.7f * v->damp));
        ui_O(x0 + w * (0.2f + 0.6f * fmodf((float)i * 0.61f, 1.f)), y0 + h * (0.2f + 0.6f * fmodf((float)i * 0.43f, 1.f)),
             2.f + u * 10.f, UI_K2, 1.2f, QD_NONE);
        qd_restore();
    }
    {   /* DECAY: the tail's length under the room */
        const float len = v->decay >= 1.f ? 200.f : 20.f + 180.f * v->decay;
        float p[2 * 31];
        for (i = 0; i <= 30; i++) {
            const float u = (float)i / 30.f;
            p[2 * i] = 20.f + u * len;
            p[2 * i + 1] = 182.f - 14.f * (v->decay >= 1.f ? 1.f : expf(-u * 3.f));
        }
        ui_P(p, 31, UI_K3, 1.5f, 0, QD_NONE);
    }
    if (v->send > 0.f) {                                /* SEND in */
        const float a[6] = {x0 - 10.f, y0 + 6.f, x0 - 2.f, y0 + 12.f, x0 - 11.f, y0 + 15.f};
        ln(x0 - 30.f, y0 - 6.f, x0 - 2.f, y0 + 12.f, UI_K4, 1.f + 2.f * v->send);
        ui_P(a, 3, UI_K4, 1.5f, 0, QD_NONE);
    }
}

static void fx_reverb2(const ui_fx_t *v)
{
    const float x0 = 20.f, pre = 6.f + v->pre * 50.f;
    float p[2 * 61];
    int i;
    ln(x0, 50.f, x0, 120.f, UI_K3, 2.f);                /* the sound in */
    ln(x0 + 2.f, 112.f, x0 + pre, 112.f, UI_K1, 1.5f);  /* PRE-DELAY: the gap */
    for (i = 0; i <= 60; i++) {                         /* the tail, MOD its wobble */
        const float u = (float)i / 60.f, a = expf(-u * 2.5f);
        p[2 * i] = x0 + pre + u * (200.f - pre);
        p[2 * i + 1] = 112.f - a * (36.f + v->mod * 10.f * sinf(u * 40.f + v->t * 4.f));
    }
    ui_P(p, 61, UI_K2, 1.4f, 0, QD_NONE);
    {   /* LOW CUT: the slope at the bottom of the band */
        const float fx = 40.f + v->low * 120.f;
        const float q[8] = {20.f, 166.f, fx - 10.f, 166.f, fx + 10.f, 136.f, 220.f, 136.f};
        ui_P(q, 4, UI_K3, 1.5f, 0, QD_NONE);
    }
    {   /* WIDTH: L .. R apart */
        const float w = 10.f + v->width * 80.f;
        ln(120.f - w, 182.f, 120.f + w, 182.f, UI_K4, 2.f);
        ui_O(120.f - w, 182.f, 3.f, QD_NONE, 1.f, UI_K4);
        ui_O(120.f + w, 182.f, 3.f, QD_NONE, 1.f, UI_K4);
    }
}

static void fx_chorus(const ui_fx_t *v)
{
    int k, i;
    for (k = 0; k < 3; k++) {                           /* three voices, WIDTH apart, DEPTH deep, at RATE */
        const float y = 112.f + ((float)k - 1.f) * (14.f + v->cwidth * 26.f);
        float p[2 * 61];
        for (i = 0; i <= 60; i++) {
            const float u = (float)i / 60.f;
            p[2 * i] = 20.f + u * 200.f;
            p[2 * i + 1] = y + sinf(u * 18.f + v->t * v->chorus_hz * UI_TAU + (float)k * 1.3f) * (2.f + v->depth * 12.f);
        }
        ui_P(p, 61, FK[k], 1.5f, 0, QD_NONE);
    }
    if (v->send > 0.f) {
        const float a[6] = {12.f, 40.f, 20.f, 46.f, 11.f, 49.f};
        ln(4.f, 30.f, 20.f, 46.f, UI_K4, 1.f + 2.f * v->send);
        ui_P(a, 3, UI_K4, 1.5f, 0, QD_NONE);
    }
}

static void fx_master(const ui_fx_t *v)
{
    float p[2 * 61];
    const float lo = 20.f + v->flow * 200.f, hi = 20.f + v->fhigh * 200.f, sl = v->fslope ? 2.f : 1.f;
    int i;
    for (i = 0; i <= 60; i++) {
        const float x = 20.f + (float)i / 60.f * 200.f;
        float g = 1.f;
        if (v->flow > 0.f && x < lo)
            g *= powf(clampf(1.f - (lo - x) / 70.f, 0.f, 1.f), sl);
        if (v->fhigh < 1.f && x > hi)
            g *= powf(clampf(1.f - (x - hi) / 70.f, 0.f, 1.f), sl);
        p[2 * i] = x;
        p[2 * i + 1] = 170.f - (v->fon ? g : 1.f) * 110.f;
    }
    ln(20.f, 170.f, 220.f, 170.f, UI_G3, 1.f);
    ui_P(p, 61, v->fon ? UI_K3 : UI_G2, 1.6f, 0, QD_NONE);
    if (v->flow > 0.f)
        ui_O(lo, 60.f, 3.4f, QD_NONE, 1.f, UI_K1);
    if (v->fhigh < 1.f)
        ui_O(hi, 60.f, 3.4f, QD_NONE, 1.f, UI_K2);
}

/* P1.5b, OVERVIEW: three columns, CHORUS, DELAY, REVERB (a line on top in KNOB 1..3's colours), each the eight tracks'
 * sends as bars in the tracks' colours: the selected track's bright (its column's top), the others dim, unheard grey */
static void fx_over(const ui_fx_t *v)
{
    int c, t;
    for (c = 0; c < 3; c++) {
        const float x0 = 10.f + (float)c * 76.f, top = 50.f, bot = 176.f;
        const int fx = c == 0 ? 0 : c == 1 ? 1 : 2;     /* (sends[t]: CHORUS, DELAY, REVERB) */
        ln(x0 + 2.f, 40.f, x0 + 66.f, 40.f, FK[c], 2.f);
        for (t = 0; t < 8; t++) {
            const float x = x0 + 3.f + (float)t * 8.f, h = (bot - top) * (float)v->sends[t][fx] / 127.f;
            const int sel = t + 1 == v->track, mute = (v->mute >> t) & 1u;
            const qd_rgb col = mute ? UI_G3 : UI_TRACK_PAL[t];
            ui_RR(x, top, 6.f, bot - top, 2.f, sel ? UI_G2 : UI_G3, 1.f, QD_NONE);
            if (h > 0.f) {
                if (!sel)
                    qd_save(), qd_alpha(0.4f);
                ui_RR(x, bot - h, 6.f, h, 2.f, QD_NONE, 1.f, col);
                if (!sel)
                    qd_restore();
            }
            if (sel)
                ui_O(x + 3.f, bot + 6.f, 2.f, QD_NONE, 1.f, col);
            if (sel && v->locked[c])                    /* P1.6h: locked on the held step: a chip on its bar */
                lock_chip(x + 3.f, bot - h, FK[c]);
        }
    }
}

/* P1.5b, ROUTING: the chain. The tracks' bus (left) sends into CHORUS, DELAY and REVERB (stacked), each returning
 * into the mix; the dry path along the foot; the master filter after the mix (or BEFORE: on the dry path); OUT. The
 * feeds as arrows in KNOB 1..3's colours, as thick as their amount (none: a faint dashed line) */
static void fx_node(float x, float y, int kind, qd_rgb col, float t)
{
    int i;
    ui_RR(x - 20.f, y - 12.f, 40.f, 24.f, 6.f, col, 1.4f, UI_BG);
    if (kind == 0) {                                    /* CHORUS: two waves */
        for (i = 0; i < 2; i++) {
            float p[2 * 13];
            int k;
            for (k = 0; k <= 12; k++) {
                p[2 * k] = x - 13.f + (float)k * 26.f / 12.f;
                p[2 * k + 1] = y - 3.f + (float)i * 6.f + sinf((float)k * 0.9f + t * 3.f + (float)i) * 2.2f;
            }
            ui_P(p, 13, col, 1.2f, 0, QD_NONE);
        }
    } else if (kind == 1) {                             /* DELAY: echoes fading */
        for (i = 0; i < 4; i++)
            ui_O(x - 11.f + (float)i * 7.5f, y, 3.2f - (float)i * 0.6f, QD_NONE, 1.f, col);
    } else {                                            /* REVERB: a room's arcs */
        for (i = 0; i < 3; i++)
            qd_arc(x - 10.f, y + 6.f, 5.f + (float)i * 5.f, -UI_PI * 0.5f, 0.f, col, 1.2f);
    }
}
static void fx_feed(float x0, float y0, float x1, float y1, float amt, qd_rgb col)
{
    static const float DASH[2] = {2.f, 3.f};
    const float an = atan2f(y1 - y0, x1 - x0);
    if (amt <= 0.f) {
        qd_dash(DASH, 2);
        ln(x0, y0, x1, y1, UI_G3, 1.f);
        qd_dash(0, 0);
        return;
    }
    ln(x0, y0, x1, y1, col, 1.f + amt * 3.f);
    {
        const float h[6] = {x1 - cosf(an - 0.5f) * 6.f, y1 - sinf(an - 0.5f) * 6.f, x1, y1,
                            x1 - cosf(an + 0.5f) * 6.f, y1 - sinf(an + 0.5f) * 6.f};
        ui_P(h, 3, col, 1.5f, 0, QD_NONE);
    }
}
static void fx_route(const ui_fx_t *v)
{
    static const float EY[3] = {60.f, 106.f, 152.f};     /* CHORUS, DELAY, REVERB */
    const float bx = 20.f, by = 106.f, ex = 102.f, mx = 162.f, dry = 184.f;
    const int before = v->rt[3] > 0.5f;
    int i;
    ui_O(bx, by, 7.f, UI_K3, 1.4f, QD_NONE);            /* the tracks' bus */
    for (i = 0; i < 3; i++) {
        ln(bx + 7.f, by, ex - 20.f, EY[i], UI_G2, 1.f);  /* the sends in */
        ln(ex + 20.f, EY[i], mx - 6.f, by, UI_G2, 1.f); /* the returns out */
    }
    ln(bx, by + 7.f, bx, dry, UI_G2, 1.f);              /* the dry path along the foot */
    ln(bx, dry, mx, dry, UI_G2, 1.f);
    ln(mx, dry, mx, by + 6.f, UI_G2, 1.f);
    ui_O(mx, by, 6.f, UI_K3, 1.4f, UI_BG);              /* the mix */
    fx_feed(ex + 8.f, EY[0] + 12.f, ex + 8.f, EY[1] - 12.f, v->rt[1], FK[1]);   /* CHORUS -> DELAY */
    fx_feed(ex + 8.f, EY[1] + 12.f, ex + 8.f, EY[2] - 12.f, v->rt[0], FK[0]);   /* DELAY -> REVERB */
    {   /* CHORUS -> REVERB: around the left of DELAY */
        const float x = ex - 10.f;
        if (v->rt[2] > 0.f) {
            ln(x, EY[0] + 12.f, x - 14.f, EY[1], FK[2], 1.f + v->rt[2] * 3.f);
            fx_feed(x - 14.f, EY[1], x, EY[2] - 12.f, v->rt[2], FK[2]);
        } else {
            fx_feed(x, EY[0] + 12.f, x - 14.f, EY[1], 0.f, FK[2]);
            fx_feed(x - 14.f, EY[1], x, EY[2] - 12.f, 0.f, FK[2]);
        }
    }
    for (i = 0; i < 3; i++)
        fx_node(ex, EY[i], i, UI_K3, v->t);
    {   /* the master filter: after the mix (on the way out) or before (on the dry path); OUT */
        const float fx = before ? 76.f : 192.f, fy = before ? dry : by;
        float p[2 * 9];
        int k;
        ln(mx + 6.f, by, 222.f, by, UI_G2, 1.f);
        ui_RR(fx - 12.f, fy - 9.f, 24.f, 18.f, 4.f, FK[3], 1.4f, UI_BG);
        for (k = 0; k <= 8; k++) {                      /* a low-pass's shape */
            const float u = (float)k / 8.f;
            p[2 * k] = fx - 8.f + u * 16.f;
            p[2 * k + 1] = fy - 3.f + (u > 0.5f ? (u - 0.5f) * 14.f : 0.f);
        }
        ui_P(p, 9, FK[3], 1.2f, 0, QD_NONE);
        {                                               /* OUT: a speaker */
            const float sp[8] = {224.f, by - 3.f, 228.f, by - 3.f, 234.f, by - 8.f, 234.f, by + 8.f};
            const float sq[4] = {228.f, by + 3.f, 224.f, by + 3.f};
            float pp[12];
            memcpy(pp, sp, sizeof sp);
            memcpy(pp + 8, sq, sizeof sq);
            ui_P(pp, 6, UI_K3, 1.3f, 1, QD_NONE);
        }
    }
}

void ui_fx_draw(const ui_fx_t *v)
{
    int k;
    panel_head(v->name ? v->name : "", v->npg, v->ipg, v->nsel);
    if (v->track >= 1 && v->track <= 8)                 /* the track whose SEND the page shows */
        ui_box(184.f, 5.f, v->track, UI_K4);
    switch (v->draw) {
    case UI_FXD_DELAY: fx_delay(v); break;
    case UI_FXD_DELAY2: fx_delay2(v); break;
    case UI_FXD_REVERB: fx_reverb(v); break;
    case UI_FXD_REVERB2: fx_reverb2(v); break;
    case UI_FXD_CHORUS: fx_chorus(v); break;
    case UI_FXD_OVER: fx_over(v); break;
    case UI_FXD_ROUTE: fx_route(v); break;
    default: fx_master(v); break;
    }
    for (k = 0; k < 4; k++) {                           /* KNOB 1..4 along the foot */
        const float cx = 30.f + (float)k * 60.f;
        float w;
        if (!v->has[k])
            continue;
        w = ui_TW(v->lab[k] ? v->lab[k] : "", 12.f, 600, 0.3f);
        (void)w;
        ui_T(v->lab[k] ? v->lab[k] : "", cx, 204.f, 12.f, 600, 0.3f, UI_G1, QD_CENTER);
        w = ui_TW(v->txt[k], 16.f, 500, 0.f);
        if (v->locked[k] || v->lockany[k])              /* P1.6d: a send's lock: a chip (filled: on this step) */
            ui_chip(v->txt[k], cx, 228.f, 15.f, 54.f, FK[k], v->locked[k]);
        else
            ui_T(v->txt[k], cx, 228.f, w > 58.f ? fmaxf(12.f, 16.f * 58.f / w) : 16.f, 500, 0.f, FK[k], QD_CENTER);
        if (v->last == k)
            ln(cx - 18.f, 235.f, cx + 18.f, 235.f, FK[k], 2.f);
    }
    if (v->step)                                        /* P1.6d: the step's frame */
        ui_RR(0.f, 0.f, 240.f, 240.f, 8.f, UI_STEP_FRAME, 3.f, QD_NONE);
}
