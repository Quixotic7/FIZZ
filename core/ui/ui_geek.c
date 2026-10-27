/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P2.11: the Geek Out pages (ui.h ui_geek_t), mockups/fizz-geekout.html as approved: METERS (core 0's load, core 1 off
 * until it runs, the frame rate, RAM, the store: tall bars that chase their values, a peak tick held 2 s, the value on
 * top), NUMBERS (the voices of the pool as dots, steals, the audio interrupt's longest half, late halves, the longest
 * lock, the frame time, the BPM; a crash's cause at the foot), VOICES (a column a track: its voice slots, steals, its
 * share of the audio) and SCOPE (~3 cycles of the loudest track, one bold line). Yellow plus a spark of rays: trouble.
 * Built through ui_layer.c (it includes this file). */

#define GK_WARN UI_KSEL                                 /* yellow: trouble, on these pages */
#define GK_TAU 6.28318531f

static qd_rgb gk_mix(qd_rgb a, qd_rgb b, float u)       /* a .. b at u (0..1), per channel */
{
    unsigned r, g, bl;
    u = u < 0.f ? 0.f : u > 1.f ? 1.f : u;
    r = (unsigned)((float)((a >> 16) & 255u) + ((float)((b >> 16) & 255u) - (float)((a >> 16) & 255u)) * u + 0.5f);
    g = (unsigned)((float)((a >> 8) & 255u) + ((float)((b >> 8) & 255u) - (float)((a >> 8) & 255u)) * u + 0.5f);
    bl = (unsigned)((float)(a & 255u) + ((float)(b & 255u) - (float)(a & 255u)) * u + 0.5f);
    return r << 16 | g << 8 | bl;
}
static void gk_line(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, col, lw, 0, QD_NONE);
}
static void gk_alpha_begin(float a)
{
    qd_save();
    qd_alpha(qd_get_alpha() * (a < 0.f ? 0.f : a > 1.f ? 1.f : a));
}
static void gk_rr(float x, float y, float w, float h, float r, qd_rgb col, float lw, qd_rgb fill, float a)
{
    gk_alpha_begin(a);
    ui_RR(x, y, w, h, r, col, lw, fill);
    qd_restore();
}
static void gk_dot(float x, float y, float r, qd_rgb col, float a)   /* a lit dot and its faint halo */
{
    gk_alpha_begin(a * 0.18f);
    ui_O(x, y, r * 2.1f, QD_NONE, 1.f, col);
    qd_restore();
    gk_alpha_begin(a);
    ui_O(x, y, r, QD_NONE, 1.f, col);
    qd_restore();
}
static void gk_burst(float x, float y, float r, float a)   /* a steal: a spark of eight rays */
{
    int k;
    gk_alpha_begin(a);
    for (k = 0; k < 8; k++) {
        const float an = (float)k * GK_TAU / 8.f + 0.2f, r0 = r * 0.72f;
        gk_line(x + cosf(an) * r0, y + sinf(an) * r0, x + cosf(an) * r, y + sinf(an) * r, GK_WARN, 2.f);
    }
    qd_restore();
}
static qd_rgb gk_heat(float u)                          /* a load: green, yellow over 60 %, orange over 85 % */
{
    return u < 0.6f ? UI_K2 : u < 0.72f ? gk_mix(UI_K2, GK_WARN, (u - 0.6f) / 0.12f) : u < 0.85f ? GK_WARN
                                                                                       : gk_mix(GK_WARN, UI_K4, (u - 0.85f) / 0.08f);
}

/* ---------------------------------------------------------------- the header */
static const char *const GK_PAGE[4] = {"METERS", "NUMBERS", "VOICES", "SCOPE"};
static void gk_icon(int i, float x, float y, int on)    /* each page as a little picture of itself */
{
    const qd_rgb col = on ? UI_K3 : UI_G2;
    const float lw = on ? 1.5f : 1.3f;
    int k;
    if (i == 0) {
        static const float B[3][2] = {{0.f, 8.f}, {5.f, 12.f}, {10.f, 5.f}};
        for (k = 0; k < 3; k++)
            gk_line(x + 2.f + B[k][0], y + 7.f, x + 2.f + B[k][0], y + 7.f - B[k][1], col, 2.2f);
    } else if (i == 1) {
        static const float L[3][2] = {{12.f, -4.f}, {8.f, 0.f}, {11.f, 4.f}};
        for (k = 0; k < 3; k++)
            gk_line(x + 1.f, y + 1.5f + L[k][1], x + 1.f + L[k][0], y + 1.5f + L[k][1], col, 2.f);
    } else if (i == 2) {
        static const int N[3] = {3, 1, 2};
        int j;
        for (k = 0; k < 3; k++)
            for (j = 0; j < N[k]; j++)
                ui_O(x + 2.f + 5.f * (float)k, y + 5.f - 4.f * (float)j, 1.6f, QD_NONE, 1.f, col);
    } else {
        float p[22];
        for (k = 0; k <= 10; k++)
            p[2 * k] = x + (float)k * 1.3f, p[2 * k + 1] = y + 1.f - sinf((float)k * 0.1f * GK_TAU) * 4.5f;
        ui_P(p, 11, col, lw, 0, QD_NONE);
    }
    if (on)
        gk_line(x, y + 11.f, x + 13.f, y + 11.f, UI_K3, 1.4f);
}
static void gk_header(const ui_geek_t *v)
{
    int i;
    const float w = ui_T(GK_PAGE[v->page], 8.f, 18.f, 12.f, 600, 1.f, UI_K3, QD_LEFT);
    if (v->page == 3 && v->scope_track >= 0)
        ui_box(14.f + w, 5.f, v->scope_track + 1, UI_TRACK_PAL[v->scope_track]);
    if (v->playing)
#if QD_GEEK_STEADY_DOT                                  /* (P2.6i: steady: the header's bands not redrawn every frame) */
        gk_dot(150.f, 13.f, 4.f, UI_RED, 0.85f);
#else
        gk_dot(150.f, 13.f, 3.4f + 1.6f * expf(-v->beat * 6.f), UI_RED, 0.55f + 0.45f * expf(-v->beat * 5.f));
#endif
    else
        ui_RR(146.f, 9.f, 8.f, 8.f, 1.5f, QD_NONE, 1.f, UI_G2);
    for (i = 0; i < 4; i++)
        gk_icon(i, 166.f + (float)i * 18.f, 11.f, i == v->page);
}

/* ---------------------------------------------------------------- 1. METERS */
#define GK_TOP 50.f
#define GK_BOT 198.f
static void gk_bar(float cx, float u, float pk, qd_rgb col, qd_rgb pkcol, int on)
{
    static const float SC[3] = {0.25f, 0.5f, 0.75f};
    const float w = 22.f, H = GK_BOT - GK_TOP;
    float f;
    int k;
    ui_RR(cx - w / 2.f, GK_TOP, w, H, 3.f, UI_G3, 1.f, QD_NONE);
    for (k = 0; k < 3; k++)
        gk_line(cx - w / 2.f - 4.f, GK_BOT - SC[k] * H, cx - w / 2.f - 1.f, GK_BOT - SC[k] * H, UI_G2, 1.f);
    if (!on)
        return;
    f = (u < 0.f ? 0.f : u > 1.f ? 1.f : u) * (H - 2.f);
    if (f > 0.5f) {
        gk_rr(cx - w / 2.f - 2.f, GK_BOT - 1.f - f - 2.f, w + 4.f, f + 3.f, 4.f, QD_NONE, 1.f, col, 0.14f);
        gk_rr(cx - w / 2.f + 2.f, GK_BOT - 1.f - f, w - 4.f, f - 1.f, 2.f, QD_NONE, 1.f, col, 0.85f);
        gk_line(cx - w / 2.f + 3.f, GK_BOT - 1.f - f, cx + w / 2.f - 3.f, GK_BOT - 1.f - f, gk_mix(col, 0xffffffu, 0.5f), 1.5f);
    }
    if (pk >= 0.f) {
        const float y = GK_BOT - 1.f - (pk > 1.f ? 1.f : pk) * (H - 2.f);
        gk_alpha_begin(0.16f);
        gk_line(cx - w / 2.f - 3.f, y, cx + w / 2.f + 3.f, y, pkcol, 6.8f);
        qd_restore();
        gk_line(cx - w / 2.f - 3.f, y, cx + w / 2.f + 3.f, y, pkcol, 2.f);
    }
}
static void gk_val(const char *s, float cx, qd_rgb col) { ui_T(s, cx, 42.f, 17.f, 500, 0.f, col, QD_CENTER); }
static void gk_pct(char *s, int v)
{
    ui_itoa(s, v);
    while (*s)
        s++;
    s[0] = '%', s[1] = 0;
}
static void gk_meters(const ui_geek_t *v)
{
    char s[16];
    const qd_rgb lc = gk_heat(v->load);
    const int fv = v->fps_n;
    const qd_rgb fcol = fv >= 50 ? UI_K2 : fv >= 36 ? GK_WARN : UI_K4;
    /* the two cores: core 0 live, core 1 dashed with a cross and OFF until it runs */
    gk_bar(26.f, v->load, v->load_pk, lc, gk_heat(v->load_pk), v->load_on);
    if (v->load_on)
        gk_pct(s, v->load_pct), gk_val(s, 26.f, lc);
    else
        ui_T("--", 26.f, 42.f, 17.f, 500, 0.f, UI_G2, QD_CENTER);
    if (v->core1_on) {
        gk_bar(68.f, v->load1, -1.f, gk_heat(v->load1), 0, 1);
        gk_pct(s, v->load1_pct), gk_val(s, 68.f, gk_heat(v->load1));
    } else {
        qd_dash((const float[]){3.f, 3.f}, 2);
        ui_RR(68.f - 11.f, GK_TOP, 22.f, GK_BOT - GK_TOP, 3.f, UI_G2, 1.f, QD_NONE);
        qd_dash(0, 0);
        gk_line(63.f, GK_BOT - 12.f, 73.f, GK_BOT - 22.f, UI_G2, 1.4f);
        gk_line(63.f, GK_BOT - 22.f, 73.f, GK_BOT - 12.f, UI_G2, 1.4f);
        ui_T("OFF", 68.f, 42.f, 12.f, 600, 0.f, UI_G2, QD_CENTER);
    }
    {
        const float p[8] = {18.f, 208.f, 18.f, 212.f, 76.f, 212.f, 76.f, 208.f};
        ui_P(p, 4, UI_G2, 1.2f, 0, QD_NONE);
    }
    ui_T("CORE", 47.f, 230.f, 12.f, 600, 1.f, UI_G1, QD_CENTER);
    /* the frame rate: full at 60; its tick the slowest frame of the last two seconds */
    gk_bar(118.f, v->fps, v->fps_min, fcol, v->fps_min * 60.f < 50.f ? GK_WARN : UI_K2, v->fps_on);
    gk_val(v->fps_on ? ui_itoa(s, fv) : "--", 118.f, v->fps_on ? fcol : UI_G2);
    ui_T("FPS", 118.f, 222.f, 12.f, 600, 1.f, UI_G1, QD_CENTER);
    /* RAM and the store: capacities, white (yellow over 90 %) */
    gk_bar(166.f, v->ram, v->ram_pk, v->ram > 0.9f ? GK_WARN : UI_K3, v->ram > 0.9f ? GK_WARN : UI_K3, v->ram_on);
    if (v->ram_on)
        gk_pct(s, v->ram_pct), gk_val(s, 166.f, UI_K3);
    else
        gk_val("--", 166.f, UI_G2);
    ui_T("RAM", 166.f, 222.f, 12.f, 600, 1.f, UI_G1, QD_CENTER);
    gk_bar(212.f, v->store, v->store, UI_K3, UI_K3, v->store_on);
    if (v->store_on)
        gk_pct(s, v->store_pct), gk_val(s, 212.f, UI_K3);
    else
        gk_val("--", 212.f, UI_G2);
    ui_T("STORE", 212.f, 222.f, 12.f, 600, 1.f, UI_G1, QD_CENTER);
}

/* ---------------------------------------------------------------- 2. NUMBERS */
static void gk_grp(float x, float y, float w, float h, qd_rgb col) { ui_RR(x, y, w, h, 6.f, col, 1.2f, QD_NONE); }
static void gk_lab(const char *s, float x, float y, qd_rgb col) { ui_T(s, x, y, 12.f, 600, 1.f, col, QD_LEFT); }
static void gk_cell(float x, const char *label, const char *val, const char *unit, qd_rgb col, float sub, int on)
{
    float w;
    gk_lab(label, x, 127.f, col == GK_WARN ? GK_WARN : UI_G1);
    w = ui_T(on ? val : "--", x, 154.f, 24.f, 300, 0.f, on ? col : UI_G2, QD_LEFT);
    if (unit && on)
        ui_T(unit, x + 3.f + w, 154.f, 12.f, 600, 0.f, UI_G1, QD_LEFT);
    if (sub >= 0.f && on) {
        ui_RR(x, 158.f, 56.f, 4.f, 2.f, QD_NONE, 1.f, UI_G3);
        ui_RR(x, 158.f, 56.f * (sub > 1.f ? 1.f : sub) < 4.f ? 4.f : 56.f * (sub > 1.f ? 1.f : sub), 4.f, 2.f, QD_NONE, 1.f,
              gk_heat(sub));
    }
}
static void gk_numbers(const ui_geek_t *v)
{
    char s[16], s2[16];
    int q;
    float w;
    /* the voices: the count of the pool big, its slots as dots in their tracks' colours */
    gk_grp(4.f, 28.f, 150.f, 78.f, UI_G3);
    gk_lab("VOICES", 14.f, 46.f, UI_G1);
    w = ui_T(ui_itoa(s, v->voices), 14.f, 94.f, 46.f, 300, 0.f, UI_K3, QD_LEFT);
    s2[0] = '/';
    ui_itoa(s2 + 1, v->nvoice);
    ui_T(s2, 16.f + w, 94.f, 18.f, 500, 0.f, UI_G1, QD_LEFT);
    for (q = 0; q < v->nvoice && q < 16; q++) {        /* (8 voices: 4 x 2 dots; 16: 8 x 2, smaller) */
        const int big = v->nvoice <= 8;
        const float x = big ? 92.f + (float)(q % 4) * 16.f : 88.f + (float)(q % 8) * 8.f;
        const float y = big ? 58.f + (float)(q / 4) * 18.f : 60.f + (float)(q / 8) * 14.f, r = big ? 5.f : 3.f;
        ui_O(x, y, r, UI_G2, 1.2f, QD_NONE);
        if (v->slot_lvl[q] > 0.02f)
            gk_dot(x, y, r, v->slot_col[q], 0.25f + v->slot_lvl[q]);
    }
    /* steals: big; a yellow flash and a spark on each new one */
    gk_grp(158.f, 28.f, 78.f, 78.f, v->steal_flash > 0.05f ? gk_mix(UI_G3, GK_WARN, v->steal_flash) : UI_G3);
    gk_lab("STEALS", 168.f, 46.f, v->steals ? GK_WARN : UI_G1);
    ui_T(ui_itoa(s, v->steals), 197.f, 94.f, 38.f, 300, 0.f,
         v->steals ? gk_mix(GK_WARN, 0xffffffu, v->steal_flash * 0.6f) : UI_G1, QD_CENTER);
    if (v->steal_flash > 0.02f)
        gk_burst(197.f, 80.f, 22.f + (1.f - v->steal_flash) * 14.f, v->steal_flash);
    /* the audio interrupt and the locks */
    gk_grp(4.f, 110.f, 232.f, 56.f, UI_G3);
    gk_cell(14.f, "ISR", ui_itoa(s, v->isr_us), "us", UI_K3, v->isr_frac, v->isr_on);
    gk_cell(96.f, "LATE", ui_itoa(s2, v->late), 0, v->late ? gk_mix(GK_WARN, 0xffffffu, v->late_flash * 0.6f) : UI_G1,
            -1.f, v->late_on);
    if (v->late && v->late_flash > 0.02f) {
        gk_alpha_begin(v->late_flash);
        ui_O(104.f, 145.f, 12.f + (1.f - v->late_flash) * 22.f, GK_WARN, 2.f, QD_NONE);
        qd_restore();
    }
    gk_cell(158.f, "LOCK", ui_itoa(s, v->lock_us), "us", UI_K3, -1.f, v->lock_on);
    /* the frame time and the tempo */
    gk_grp(4.f, 170.f, 232.f, 46.f, UI_G3);
    {
        const int slow = v->frame_on && v->frame10 > 200;
        gk_lab("FRAME", 14.f, 186.f, slow ? GK_WARN : UI_G1);
        if (v->frame_on) {
            ui_itoa(s, v->frame10 / 10);
            q = (int)strlen(s);
            s[q] = '.', s[q + 1] = (char)('0' + v->frame10 % 10), s[q + 2] = 0;
            w = ui_T(s, 14.f, 210.f, 22.f, 300, 0.f, slow ? GK_WARN : UI_K3, QD_LEFT);
            ui_T("ms", 17.f + w, 210.f, 12.f, 600, 0.f, UI_G1, QD_LEFT);
        } else {
            ui_T("--", 14.f, 210.f, 22.f, 300, 0.f, UI_G2, QD_LEFT);
        }
    }
    gk_lab("BPM", 140.f, 186.f, UI_G1);
    ui_T(ui_itoa(s, v->bpm), 140.f, 210.f, 22.f, 300, 0.f, UI_K3, QD_LEFT);
    /* the last boot's cause: only after a crash */
    if (v->boot) {
        const float tri[6] = {16.f, 234.f, 22.f, 224.f, 28.f, 234.f};
        ui_RR(4.f, 220.f, 232.f, 19.f, 4.f, GK_WARN, 1.3f, gk_mix(0, GK_WARN, 0.12f));
        ui_P(tri, 3, GK_WARN, 1.5f, 1, QD_NONE);
        gk_line(22.f, 228.f, 22.f, 230.5f, GK_WARN, 1.6f);
        ui_T("BOOT", 36.f, 234.f, 12.f, 600, 1.f, UI_G1, QD_LEFT);
        ui_T(v->boot, 228.f, 234.f, 13.f, 600, 0.5f, GK_WARN, QD_RIGHT);
    }
}

/* ---------------------------------------------------------------- 3. VOICES, a column a track */
static void gk_voices(const ui_geek_t *v)
{
    char s[12];
    int i, k;
    for (i = 0; i < UI_NTRACK; i++) {                   /* (six tracks: 40 px a column) */
        const float cx = 20.f + (float)i * 40.f, top = 140.f, bot = 230.f, H = bot - top;
        const qd_rgb col = UI_TRACK_PAL[i];
        const float fl = v->tflash[i];
        float u, f;
        gk_alpha_begin(v->on[i] ? 1.f : 0.55f);
        ui_box(cx - 8.f, 28.f, i + 1, col);
        qd_restore();
        for (k = 0; k < 3; k++) {                       /* its voice limit as rings, a lit dot a sounding voice */
            const float y = 62.f + (float)k * 18.f;
            if (k >= v->lim[i]) {
                ui_O(cx, y, 1.5f, QD_NONE, 1.f, UI_G3);
                continue;
            }
            ui_O(cx, y, 7.f, UI_G2, 1.2f, QD_NONE);
            if (k < v->on[i] && v->lvl[i][k] > 0.02f)
                gk_dot(cx, y, 7.f, col, 0.2f + v->lvl[i][k]);
        }
        if (fl > 0.02f)
            gk_burst(cx, 62.f, 9.f + (1.f - fl) * 6.f, fl);
        if (v->tsteals[i])                              /* its steals so far (a dash: none) */
            ui_T(ui_itoa(s, v->tsteals[i]), cx, 128.f, 13.f, 600, 0.f, gk_mix(GK_WARN, 0xffffffu, fl * 0.6f), QD_CENTER);
        else
            gk_line(cx - 3.f, 124.f, cx + 3.f, 124.f, UI_G2, 1.5f);
        u = v->share[i];                                /* its share of the audio (full: half of it) */
        f = (u / 0.5f > 1.f ? 1.f : u / 0.5f) * (H - 2.f);
        ui_RR(cx - 10.f, top, 20.f, H, 3.f, UI_G3, 1.f, QD_NONE);
        if (f > 0.5f) {
            gk_rr(cx - 12.f, bot - 1.f - f - 2.f, 24.f, f + 3.f, 4.f, QD_NONE, 1.f, col, 0.14f);
            gk_rr(cx - 8.f, bot - 1.f - f, 16.f, f - 1.f, 2.f, QD_NONE, 1.f, col, 0.85f);
        }
    }
}

/* ---------------------------------------------------------------- 4. SCOPE */
static void gk_scope(const ui_geek_t *v)
{
    const float cy = 132.f, A = 82.f;
    float p[2 * UI_GEEK_SCOPE];
    int i;
    gk_line(8.f, cy, 232.f, cy, UI_G3, 1.f);
    if (v->scope_track < 0) {
        gk_line(8.f, cy, 232.f, cy, UI_G2, 2.f);
        return;
    }
    for (i = 0; i < UI_GEEK_SCOPE; i++)
        p[2 * i] = 8.f + 2.f * (float)i, p[2 * i + 1] = cy - A * v->w.scope[i];
    {
        const qd_rgb col = UI_TRACK_PAL[v->scope_track];
        gk_alpha_begin(0.22f);                          /* (one glow under it, then the line) */
        ui_P(p, UI_GEEK_SCOPE, col, 5.f, 0, QD_NONE);
        qd_restore();
        ui_P(p, UI_GEEK_SCOPE, gk_mix(col, 0xffffffu, 0.15f), 2.6f, 0, QD_NONE);
    }
}

/* 2026-10-27 (the owner): TRACKS, the whole screen, no header: six rows of 40 px, each track's own signal before the
 * effects in its colour (no middle line); a track with no sound: one flat line */
static void gk_tracks(const ui_geek_t *v)
{
    int t, k;
    for (t = 0; t < UI_GEEK_TRK; t++) {
        const float mid = 20.f + (float)t * 40.f;
        const qd_rgb col = UI_TRACK_PAL[t];
        float p[2 * UI_GEEK_TPTS];
        for (k = 0; k < UI_GEEK_TPTS; k++)
            p[2 * k] = (float)k * 4.f, p[2 * k + 1] = mid - (v->tsc_live[t] ? (float)v->w.tsc[t][k] * (17.f / 127.f) : 0.f);
        if (v->tsc_live[t])
            qd_save(), qd_alpha(0.16f), ui_P(p, UI_GEEK_TPTS, col, 4.f, 0, QD_NONE), qd_restore();
        ui_P(p, UI_GEEK_TPTS, col, 1.5f, 0, QD_NONE);
    }
}

void ui_geek_draw(const ui_geek_t *v)
{
    if (v->page == 4) {
        gk_tracks(v);
        return;
    }
    gk_header(v);
    switch (v->page) {
    case 0: gk_meters(v); break;
    case 1: gk_numbers(v); break;
    case 2: gk_voices(v); break;
    default: gk_scope(v); break;
    }
}
