/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.5t: the library browser, the step's pool picker and the LOADED / BACK / SAVED takeovers (ui.h ui_browse_t,
 * ui_pfx_draw; P1.5u: SAVE PRESET is the typewriter's, ui_type.c), drawn as mockups/fizz-presets.html draws them: its positions, sizes, colours
 * and motions (headP, rollo, cassette, trayIcon, the dice, the name cells). Built through ui_layer.c after ui_lib.c
 * (its lb_* helpers: the glyph, the bubbles, the dashed outline). PLAN §9a's clarity rules: names, numbers and the
 * few words the mock-up has (LOADED, BACK, SAVED, NEW, REPLACE, OUT?); the rest is drawing, colour and motion. */

const qd_rgb UI_PK_COL[7] = {UI_K1, UI_K3, 0x36d6e7u, UI_PAD_COL, UI_K2, UI_K4, 0xff3ca0u};
#define PR_ORANGE UI_K4

static qd_rgb pr_kind_col(int k) { return k >= 0 && k < 7 ? UI_PK_COL[k] : UI_G1; }
static const char *pr_kind_name(int k)                  /* (sounds.h QD_PK_*'s) */
{
    static const char *const N[7] = {"BASS", "KEYS", "PLUCK", "PAD", "LEAD", "DRUMS", "FX"};
    return k >= 0 && k < 7 ? N[k] : "";
}

/* the pool: a little tray of four slots */
static void pr_tray(float x, float y, qd_rgb col, float a)
{
    int k;
    qd_save();
    qd_alpha(qd_get_alpha() * lb_clamp(a, 0.f, 1.f));
    ui_RR(x, y, 18.f, 13.f, 3.f, col, 1.3f, QD_NONE);
    for (k = 0; k < 4; k++)
        ui_RR(x + 3.f + (float)(k % 2) * 7.f, y + 3.f + (float)(k / 2) * 4.5f, 5.5f, 3.2f, 1.f, QD_NONE, 1.f, col);
    qd_restore();
}

/* P1.5v: not in the pool: an empty tray with a + (B10 adds it) */
static void pr_tray_add(float x, float y, qd_rgb col, float a)
{
    qd_save();
    qd_alpha(qd_get_alpha() * lb_clamp(a, 0.f, 1.f));
    ui_RR(x, y, 18.f, 13.f, 3.f, col, 1.3f, QD_NONE);
    line2(x + 9.f, y + 3.f, x + 9.f, y + 10.f, col, 1.5f);
    line2(x + 5.5f, y + 6.5f, x + 12.5f, y + 6.5f, col, 1.5f);
    qd_restore();
}

/* P1.5v: a preset's tags as dots in their kinds' colours, from x (dir +1 right, -1 left); their width */
static float pr_tag_dots(float x, float y, int tags, float r, int dir)
{
    int k, i = 0;
    for (k = 0; k < 7; k++)
        if ((tags >> k) & 1) {
            ui_O(x + (float)(dir * i) * (r * 2.f + 2.6f), y, r, QD_NONE, 1.f, pr_kind_col(k));
            i++;
        }
    return (float)i * (r * 2.f + 2.6f);
}

/* a five-pointed star (one of MY presets), an arrow down (imported) */
static void pr_star(float x, float y, float r, qd_rgb col)
{
    float p[20];
    int k;
    for (k = 0; k < 10; k++) {
        const float an = -UI_PI * 0.5f + (float)k * UI_PI / 5.f, rr = k & 1 ? r * 0.45f : r;
        p[2 * k] = x + cosf(an) * rr;
        p[2 * k + 1] = y + sinf(an) * rr;
    }
    ui_P(p, 10, col, 1.f, 1, col);
}
static void pr_arrow(float x, float y, qd_rgb col)
{
    const float a[6] = {x - 4.f, y + 1.f, x, y + 5.f, x + 4.f, y + 1.f};
    line2(x, y - 6.f, x, y + 5.f, col, 1.6f);
    ui_P(a, 3, col, 1.6f, 0, QD_NONE);
}

/* the head: TITLE (12 px 600), a chip (the kind filter), the track's box, "7/31" */
static void pr_head(const char *title, qd_rgb col, const char *right, int track, int chip)
{
    float x = 8.f + ui_TW(title, 12.f, 600, 1.f) + 8.f;
    ui_T(title, 8.f, 17.f, 12.f, 600, 1.f, col, QD_LEFT);
    if (chip > 0) {
        const char *nm = pr_kind_name(chip - 1);
        const qd_rgb cc = pr_kind_col(chip - 1);
        const float w = ui_TW(nm, 12.f, 600, 1.f) + 10.f;
        ui_RR(x, 5.f, w, 16.f, 3.f, cc, 1.3f, QD_NONE);
        ui_T(nm, x + 5.f, 17.f, 12.f, 600, 1.f, cc, QD_LEFT);
    }
    if (track)
        ui_box(214.f, 4.f, track, UI_TRACK_PAL[(track - 1) & 7]);
    if (right && right[0])
        ui_T(right, track ? 208.f : 232.f, 17.f, 12.f, 500, 0.f, UI_G1, QD_RIGHT);
    line2(8.f, 25.f, 232.f, 25.f, UI_G3, 1.f);
}

/* a preset as a cassette: its envelope in its kind's colour in the label, the name big, its marks */
static void pr_cassette(float x, float y, float w, float h, const ui_preset_t *p, qd_rgb col, float t, int dashed,
                        const char *label)
{
    int j, k;
    if (dashed)
        lb_dashed_rr(x, y, w, h, 8.f, col, 1.8f);
    else
        ui_RR(x, y, w, h, 8.f, col, 1.8f, UI_BG);
    if (!p || !p->ok || p->track) {                     /* (none: TRACK, the step plays the track's sound) */
        ui_T(label ? label : "TRACK", x + w / 2.f, y + h / 2.f + 7.f, 20.f, 300, 0.f, col, QD_CENTER);
        return;
    }
    ui_RR(x + 10.f, y + 8.f, w - 20.f, h * 0.42f, 4.f, UI_G2, 1.f, QD_NONE);
    lb_glyph(x + 18.f, y + 14.f, 34.f, 14.f, p->env, p->missing ? UI_G2 : pr_kind_col(p->kind), 1.5f);
    {
        const char *nm = p->name ? p->name : "";
        const float room = w - 20.f - 46.f, tw = ui_TW(nm, 18.f, 400, 0.f);
        const float sz = tw > room ? lb_clamp(18.f * room / tw, 12.f, 18.f) : 18.f;
        ui_T(nm, x + w / 2.f + 14.f, y + 8.f + h * 0.42f - 9.f, sz, 400, 0.f, p->missing ? UI_G1 : UI_K3, QD_CENTER);
    }
    for (j = 0; j < 2; j++) {                           /* the reels */
        const float rx = x + w * (j ? 0.68f : 0.32f), ry = y + h * 0.74f, r = h * 0.15f < 12.f ? h * 0.15f : 12.f;
        ui_O(rx, ry, r, UI_G1, 1.3f, QD_NONE);
        for (k = 0; k < 3; k++) {
            const float an = t * 3.f + (float)k * UI_TAU / 3.f + (float)j;
            line2(rx + cosf(an) * r * 0.35f, ry + sinf(an) * r * 0.35f, rx + cosf(an) * r * 0.8f, ry + sinf(an) * r * 0.8f,
                  UI_G1, 1.2f);
        }
    }
    if (p->imported)
        pr_arrow(x + w - 14.f, y + h - 13.f, UI_LIB_COL);
    else if (p->mine)
        pr_star(x + w - 14.f, y + h - 13.f, 6.5f, UI_LIB_COL);
    if (p->tray == 1)                                   /* P1.5v: B10 adds it */
        pr_tray_add(x + 8.f, y + h - 18.f, UI_POOL_COL, 0.75f);
    else if (p->tray == 2 || p->in_pool)                /* .. B10 takes it out */
        pr_tray(x + 8.f, y + h - 18.f, UI_POOL_COL, 1.f);
    {   /* P1.5x: its sends, chorus / delay / reverb, three little bars right of the reels (the SENDS page's colours) */
        static const qd_rgb SC[3] = {UI_K1, UI_K2, UI_K3};
        const float bx = x + w * 0.68f + 16.f, by = y + h - 9.f;
        int j2;
        for (j2 = 0; j2 < 3; j2++) {
            const float hh = 2.f + 16.f * (float)(p->send[j2] < 0 ? 0 : p->send[j2] > 127 ? 127 : p->send[j2]) / 127.f;
            ui_RR(bx + (float)j2 * 5.f, by - hh, 3.f, hh, 1.f, QD_NONE, 1.f, p->missing ? UI_G2 : SC[j2]);
        }
    }
    if (p->tags) {                                      /* P1.5v: its tags, between the reels */
        int k, n = 0;
        for (k = 0; k < 7; k++)
            n += (p->tags >> k) & 1;
        pr_tag_dots(x + w / 2.f - (float)(n - 1) * 4.3f, y + h * 0.74f, p->tags, 3.f, 1);   /* (P1.5x: 6 px) */
    }
}

/* P1.5v: armed (DELETE?, OUT?): a big red X across the whole cassette */
static void pr_red_x(float x, float y, float w, float h)
{
    line2(x + 4.f, y + 4.f, x + w - 4.f, y + h - 4.f, UI_RED, 4.f);
    line2(x + w - 4.f, y + 4.f, x + 4.f, y + h - 4.f, UI_RED, 4.f);
}

/* the list as a rolodex: two neighbours above and below, the chosen big, where in the list on the right */
static void pr_rollo(const ui_browse_t *v, qd_rgb col)
{
    static const int D[4] = {-2, -1, 1, 2};
    const float dy = v->roll > 0.f ? lb_ease(v->roll) * 14.f * (float)(v->dir < 0 ? -1 : 1) : 0.f;
    int i;
    for (i = 0; i < 4; i++) {
        const int d = D[i];
        const ui_preset_t *p = &v->e[2 + d];
        const float y = (d < 0 ? 64.f + (float)(d + 1) * 22.f : 160.f + (float)d * 26.f) + dy, a = d * d == 1 ? 0.6f : 0.28f;
        if (!p->ok || (v->users_row && d == 2))
            continue;                                   /* (the POOL, OUT?: its users' row there) */
        if (d > 0 && v->fx && v->fx_u < 1.f && !v->picker)
            continue;                                   /* (P1.5z: + / - POOL's animation and its count there) */
        qd_save();
        qd_alpha(qd_get_alpha() * a);
        qd_clip_rect(0.f, 27.f, 222.f, 213.f);
        if (!p->track && p->tags)                       /* (P1.5v: its tags' dots) */
            pr_tag_dots(60.f, y - 4.f, p->tags, 3.f, -1);
        else if (!p->track)
            ui_O(60.f, y - 4.f, 3.f, QD_NONE, 1.f, pr_kind_col(p->kind));
        ui_T(p->track ? "TRACK" : p->name ? p->name : "", 70.f, y, 14.f, 400, 0.f, p->missing ? UI_G1 : UI_K3, QD_LEFT);
        if (p->in_pool && !v->pool && !v->picker)
            pr_tray(198.f, y - 13.f, UI_POOL_COL, 1.f);
        qd_restore();
    }
    {
        const int armed = v->out_armed || v->del_armed;
        const float sx = armed ? sinf(v->arm_t * 48.f) * (v->arm_t < 0.45f ? 3.5f : 1.2f) : 0.f;   /* (it shakes) */
        pr_cassette(34.f + sx, 82.f + dy * 0.3f, 172.f, 76.f, v->e[2].ok ? &v->e[2] : 0, col, v->t,
                    !v->e[2].ok || v->e[2].track, "TRACK");
        if (armed)
            pr_red_x(34.f + sx, 82.f + dy * 0.3f, 172.f, 76.f);
    }
    if (v->sound > 0.f && v->e[2].ok && !v->e[2].track) {   /* it sounds: its envelope lights along */
        qd_save();
        qd_clip_rect(52.f, 88.f + dy * 0.3f, 34.f * v->sound, 20.f);
        lb_glyph(52.f, 96.f + dy * 0.3f, 34.f, 14.f, v->e[2].env, UI_K3, 1.9f);
        qd_restore();
    }
    {
        const int n = v->n + (v->picker ? 1 : 0), at = v->pos;
        const float yy = 40.f + 170.f * (float)at / (float)(n > 1 ? n - 1 : 1);
        line2(228.f, 40.f, 228.f, 210.f, UI_G3, 2.f);
        if (n > 0)
            line2(228.f, yy - 6.f, 228.f, yy + 6.f, col, 3.f);
    }
}

void ui_browse_draw(const ui_browse_t *v)
{
    char right[16];
    right[0] = 0;
    if (v->picker) {                                    /* the step's pool picker */
        ui_T(v->title, 8.f, 17.f, 12.f, 600, 1.f, UI_K3, QD_LEFT);
        line2(8.f, 25.f, 232.f, 25.f, UI_G3, 1.f);
        ui_RR(0.f, 0.f, 240.f, 240.f, 8.f, UI_STEP_FRAME, 3.f, QD_NONE);
        if (!v->n) {                                    /* the pool empty: TRACK only */
            pr_cassette(34.f, 60.f, 172.f, 76.f, 0, UI_POOL_COL, v->t, 1, "TRACK");
            pr_tray(98.f, 168.f, UI_POOL_COL, 0.6f);
            qd_save();
            qd_alpha(qd_get_alpha() * 0.7f);
            ui_T("0/128", 122.f, 179.f, 12.f, 600, 0.f, UI_POOL_COL, QD_LEFT);
            qd_restore();
            return;
        }
        pr_rollo(v, UI_POOL_COL);
        return;
    }
    if (v->n) {
        char b[8];
        ui_itoa(right, v->pos + 1);
        strcat(right, "/");
        strcat(right, ui_itoa(b, v->pool ? 128 : v->n));
    }
    if (v->del_armed || v->out_armed)                   /* P1.5v: the head red: DELETE? / OUT? */
        pr_head(v->del_armed ? "DELETE?" : "OUT?", UI_RED, right, v->track, v->chip);
    else
        pr_head(v->title, v->tcol, right, v->track, v->chip);
    if (!v->n) {                                        /* nothing in it */
        pr_cassette(34.f, 82.f, 172.f, 76.f, 0, v->col, v->t, 1, "\xe2\x80\x94");
        return;
    }
    pr_rollo(v, v->out_armed || v->del_armed ? UI_RED : v->ocol ? v->ocol : v->col);   /* (P1.5z: lime: in the pool) */
    if (v->users_row && v->del_armed) {                 /* P1.5w: DELETE?: the projects referencing it, its steps */
        float x = 14.f;
        char n[12];
        int k;
        for (k = 1; k >= 0; k--)                        /* (a project: a little stack of patterns) */
            ui_RR(x + (float)k * 3.f, 204.f - (float)k * 3.f, 15.f, 13.f, 2.f, UI_RED, 1.2f, UI_BG);
        ui_itoa(n, v->use_proj);
        ui_T(n, x + 22.f, 217.f, 12.f, 600, 0.f, UI_RED, QD_LEFT);
        x += 22.f + ui_TW(n, 12.f, 600, 0.f) + 10.f;
        if (v->use_steps) {
            char m[12] = "\xc3\x97";
            float w, ww;
            ui_itoa(m + 2, v->use_steps);
            w = ui_TW(m, 12.f, 600, 0.f) + 12.f, ww = w < 34.f ? 34.f : w;
            ui_RR(x, 205.f, ww, 15.f, 3.f, UI_RED, 1.2f, QD_NONE);
            ui_T(m, x + ww / 2.f, 216.f, 12.f, 600, 0.f, UI_RED, QD_CENTER);
            x += ww + 6.f;
            {
                const float ar[6] = {x + 13.f, 208.f, x + 18.f, 212.5f, x + 13.f, 217.f};
                line2(x, 212.5f, x + 17.f, 212.5f, UI_RED, 1.6f);
                ui_P(ar, 3, UI_RED, 1.6f, 0, QD_NONE);
                ui_T("TRACK", x + 24.f, 217.f, 12.f, 600, 0.5f, UI_RED, QD_LEFT);
            }
        }
    } else if (v->users_row) {                          /* who uses it (P1.5v: OUT?: they go back to TRACK) */
        const qd_rgb uc = v->out_armed ? UI_RED : UI_G1;
        float x = v->out_armed ? 14.f : 60.f;
        int t;
        for (t = 0; t < 8; t++)
            if ((v->users >> t) & 1u) {
                ui_box(x, 204.f, t + 1, v->out_armed ? UI_RED : UI_TRACK_PAL[t]);
                x += 22.f;
            }
        if (v->usteps) {
            char n[12] = "\xc3\x97";
            ui_itoa(n + 2, v->usteps);
            {
                const float w = ui_TW(n, 12.f, 600, 0.f) + 12.f, ww = w < 34.f ? 34.f : w;
                ui_RR(x, 205.f, ww, 15.f, 3.f, uc, 1.2f, QD_NONE);
                ui_T(n, x + ww / 2.f, 216.f, 12.f, 600, 0.f, uc, QD_CENTER);
                x += ww + 6.f;
            }
        }
        if (v->out_armed) {                             /* .. -> TRACK */
            const float a[6] = {x + 13.f, 208.f, x + 18.f, 212.5f, x + 13.f, 217.f};
            line2(x, 212.5f, x + 17.f, 212.5f, UI_RED, 1.6f);
            ui_P(a, 3, UI_RED, 1.6f, 0, QD_NONE);
            ui_T("TRACK", x + 24.f, 217.f, 12.f, 600, 0.5f, UI_RED, QD_LEFT);
        }
    }
    if (v->tag_u < 1.f) {                               /* P1.5v: a tag toggled: its chip pops (on: filled; off: struck) */
        const char *nm = pr_kind_name(v->tag_k);
        const qd_rgb cc = pr_kind_col(v->tag_k);
        const float w = ui_TW(nm, 12.f, 600, 1.f) + 12.f, y = 162.f - lb_ease(v->tag_u) * 4.f;
        qd_save();
        qd_alpha(qd_get_alpha() * (1.f - v->tag_u * v->tag_u));
        ui_RR(120.f - w / 2.f, y, w, 16.f, 3.f, cc, 1.3f, v->tag_on ? cc : UI_BG);
        ui_T(nm, 120.f, y + 12.f, 12.f, 600, 1.f, v->tag_on ? UI_BG : cc, QD_CENTER);
        if (!v->tag_on)
            line2(120.f - w / 2.f + 3.f, y + 8.f, 120.f + w / 2.f - 3.f, y + 8.f, cc, 1.4f);
        qd_restore();
    }
    if (v->fx == 5 && v->fx_u < 1.f) {                  /* P1.5v: deleted: the cassette breaks up, fizzes away red */
        const float u = v->fx_u, f = lb_ease(u);
        int k;
        qd_save();
        qd_alpha(qd_get_alpha() * (1.f - f));
        ui_RR(34.f, 82.f, 172.f, 76.f, 8.f, QD_NONE, 1.f, UI_BG);
        qd_restore();
        for (k = 0; k < 14; k++) {                      /* its pieces fly apart */
            const float px = 40.f + lb_hash(k) * 150.f, py = 86.f + lb_hash(k + 20) * 64.f;
            const float vx = (px - 120.f) * 0.9f, vy = (py - 120.f) * 0.9f - 30.f;
            const float x = px + vx * f, y = py + vy * f + 60.f * f * f, s = 6.f + 10.f * lb_hash(k + 40);
            qd_save();
            qd_alpha(qd_get_alpha() * (1.f - f));
            ui_RR(x, y, s, s * 0.6f, 1.5f, UI_RED, 1.4f, QD_NONE);
            qd_restore();
        }
        for (k = 0; k < 18; k++)                        /* .. and fizz */
            lb_bubble(44.f + lb_hash(k + 60) * 152.f, 150.f - f * (40.f + 60.f * lb_hash(k + 80)),
                      1.f + 2.f * lb_hash(k + 90), UI_RED, (1.f - f) * 0.9f);
        if (u < 0.3f && v->gone.name) {                 /* (its name, a moment) */
            qd_save();
            qd_alpha(qd_get_alpha() * (1.f - u / 0.3f));
            ui_T(v->gone.name, 120.f, 126.f, 18.f, 400, 0.f, UI_RED, QD_CENTER);
            qd_restore();
        }
    }
    if (v->fx && v->fx_u < 1.f) {                       /* + POOL */
        const float u = v->fx_u;
        char n[12];
        ui_itoa(n, v->pool_n);
        strcat(n, "/128");
        if (v->fx == 1) {                               /* in: the cassette shrinks into the tray, bubbles */
            pr_tray(104.f, 200.f, UI_POOL_COL, 1.f);
            ui_T(n, 128.f, 211.f, 12.f, 600, 0.f, UI_POOL_COL, QD_LEFT);
            if (u < 0.6f) {
                const float w = lb_ease(u / 0.6f), cx = 120.f + (112.f - 120.f) * w, cy = 120.f + (205.f - 120.f) * w;
                const float s = 1.f + (0.1f - 1.f) * w;
                qd_save();
                qd_alpha(qd_get_alpha() * 0.85f);
                ui_RR(cx - 86.f * s, cy - 38.f * s, 172.f * s, 76.f * s, 8.f * s, UI_POOL_COL, 1.6f, UI_BG);
                qd_restore();
            } else {
                const float f = (u - 0.6f) / 0.4f;
                int k;
                for (k = 0; k < 8; k++)
                    lb_bubble(104.f + lb_hash(k) * 22.f, 196.f - f * 24.f * (0.5f + lb_hash(k + 3)),
                              1.f + 1.6f * lb_hash(k + 7), UI_POOL_COL, 1.f - f);
            }
        } else if (v->fx == 2) {                        /* there already: the tray pulses */
            const float p = 1.f + 0.5f * sinf(u * UI_PI * 4.f);
            pr_tray(104.f, 200.f, UI_POOL_COL, lb_clamp(p, 0.3f, 1.f));
            ui_T(n, 128.f, 211.f, 12.f, 600, 0.f, UI_POOL_COL, QD_LEFT);
        } else if (v->fx == 3) {                        /* full: the tray flashes orange */
            const int on = (int)(u * 6.f) % 2 == 0;
            pr_tray(104.f, 200.f, on ? PR_ORANGE : UI_POOL_COL, 1.f);
            ui_T(n, 128.f, 211.f, 12.f, 600, 0.f, on ? PR_ORANGE : UI_POOL_COL, QD_LEFT);
        } else if (v->fx == 4) {                        /* P1.5z: out: the cassette pops back out of the lime tray,
                                                         * a burst of lime bubbles fizzes away, the tray's + comes back */
            int k;
            if (u < 0.55f)
                pr_tray(104.f, 200.f, UI_POOL_COL, 1.f - lb_ease(u / 0.55f) * 0.6f);
            else
                pr_tray_add(104.f, 200.f, UI_POOL_COL, lb_ease((u - 0.55f) / 0.45f));
            ui_T(n, 128.f, 211.f, 12.f, 600, 0.f, UI_POOL_COL, QD_LEFT);
            if (u < 0.45f) {                            /* (out it pops: from the tray up to the cassette, growing) */
                const float w = lb_ease(u / 0.45f), cx = 113.f + (120.f - 113.f) * w, cy = 206.f + (120.f - 206.f) * w;
                const float s = 0.1f + 0.9f * w, pop = 1.f + 0.12f * sinf(w * UI_PI);
                qd_save();
                qd_alpha(qd_get_alpha() * (0.95f - 0.6f * w));
                ui_RR(cx - 86.f * s * pop, cy - 38.f * s * pop, 172.f * s * pop, 76.f * s * pop, 8.f * s, UI_POOL_COL, 1.8f,
                      QD_NONE);
                qd_restore();
            }
            if (u > 0.12f && u < 0.85f) {               /* (the bubbles: out of the tray, up and away, fizzing) */
                const float f = (u - 0.12f) / 0.73f;
                for (k = 0; k < 14; k++) {
                    const float an = -UI_PI * (0.1f + 0.8f * lb_hash(k + 11)), sp = 30.f + 50.f * lb_hash(k + 23);
                    lb_bubble(113.f + cosf(an) * sp * f, 204.f + sinf(an) * sp * f - 10.f * f * f,
                              1.f + 2.2f * lb_hash(k + 31) * (1.f - f * 0.5f), UI_POOL_COL, (1.f - f) * 0.95f);
                }
            }
        }
    }
}

/* ---------------------------------------------------------------- LOADED, BACK, SAVED */
void ui_pfx_draw(int kind, const ui_preset_t *s, const ui_preset_t *own, int track, float u, float t)
{
    const qd_rgb tc = UI_TRACK_PAL[(track - 1) & 7];
    ui_RR(0.f, 0.f, 240.f, 240.f, 0.f, QD_NONE, 1.f, UI_BG);
    if (kind == UI_PFX_LOADED) {                        /* the preset drops into the track's box, its name big */
        const float v = lb_ease(u / 0.55f);
        pr_head("PRESETS", UI_LIB_COL, "", track, 0);
        if (u < 0.55f) {
            const float x = 34.f + (200.f - 34.f) * v, y = 82.f + (2.f - 82.f) * v, sc = 1.f + (0.12f - 1.f) * v;
            qd_save();
            qd_alpha(qd_get_alpha() * (1.f - v * 0.3f));
            ui_RR(x, y, 172.f * sc, 76.f * sc, 8.f * sc, tc, 1.8f, UI_BG);
            if (sc > 0.5f)
                pr_cassette(x, y, 172.f, 76.f, s, tc, t, 0, 0);
            qd_restore();
        } else {
            const float f = 1.f - lb_ease((u - 0.55f) / 0.45f);
            const char *nm = s && s->name ? s->name : "";
            const float tw = ui_TW(nm, 28.f, 300, 0.f), sz = tw > 220.f ? lb_clamp(28.f * 220.f / tw, 12.f, 28.f) : 28.f;
            qd_save();
            qd_alpha(qd_get_alpha() * f);
            ui_RR(210.f, 1.f, 26.f, 22.f, 4.f, tc, 2.f, QD_NONE);
            qd_restore();
            ui_T(nm, 120.f, 140.f, sz, 300, 0.f, tc, QD_CENTER);
            ui_T("LOADED", 120.f, 166.f, 12.f, 600, 1.f, UI_G1, QD_CENTER);
        }
        return;
    }
    if (kind == UI_PFX_BACK) {                          /* the one browsed slides out, the track's own back in */
        const float v = lb_ease(u / 0.5f);
        pr_head("PRESETS", UI_LIB_COL, "", track, 0);
        if (s && s->ok) {
            qd_save();
            qd_alpha(qd_get_alpha() * (1.f - v));
            pr_cassette(34.f + (-180.f - 34.f) * v, 82.f, 172.f, 76.f, s, UI_LIB_COL, t, 0, 0);
            qd_restore();
        }
        pr_cassette(260.f + (34.f - 260.f) * v, 82.f, 172.f, 76.f, own, tc, t, 0, 0);
        if (u > 0.5f)
            ui_T("BACK", 120.f, 186.f, 12.f, 600, 1.f, UI_G1, QD_CENTER);
        return;
    }
    {                                                   /* SAVED: into MY PRESETS with a fizz */
        const float v = lb_ease(u / 0.5f);
        pr_head("MY PRESETS", UI_LIB_COL, "", track, 0);
        pr_cassette(34.f, 50.f + (82.f - 50.f) * v, 172.f, 76.f, s, UI_LIB_COL, t, 0, 0);
        if (u > 0.5f) {
            const float f = (u - 0.5f) / 0.5f;
            int k;
            for (k = 0; k < 10; k++)
                lb_bubble(50.f + lb_hash(k) * 140.f, 80.f - f * 40.f * (0.5f + lb_hash(k + 2)), 1.f + 2.f * lb_hash(k + 5),
                          UI_LIB_COL, 1.f - f);
            ui_T("SAVED", 120.f, 196.f, 14.f, 600, 1.f, UI_LIB_COL, QD_CENTER);
        }
    }
}
