/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.5t: the library browser, the step's pool picker, SAVE PRESET and the LOADED / BACK / SAVED takeovers (ui.h
 * ui_browse_t, ui_save_t, ui_pfx_draw), drawn as mockups/fizz-presets.html draws them: its positions, sizes, colours
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
    lb_glyph(x + 18.f, y + 14.f, 34.f, 14.f, p->env, pr_kind_col(p->kind), 1.5f);
    {
        const char *nm = p->name ? p->name : "";
        const float room = w - 20.f - 46.f, tw = ui_TW(nm, 18.f, 400, 0.f);
        const float sz = tw > room ? lb_clamp(18.f * room / tw, 12.f, 18.f) : 18.f;
        ui_T(nm, x + w / 2.f + 14.f, y + 8.f + h * 0.42f - 9.f, sz, 400, 0.f, UI_K3, QD_CENTER);
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
    if (p->in_pool)
        pr_tray(x + 8.f, y + h - 18.f, UI_POOL_COL, 1.f);
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
        if (!p->ok || (v->pool && d == 2))
            continue;                                   /* (the POOL: its users' row there) */
        qd_save();
        qd_alpha(qd_get_alpha() * a);
        qd_clip_rect(0.f, 27.f, 222.f, 213.f);
        if (!p->track)
            ui_O(60.f, y - 4.f, 3.f, QD_NONE, 1.f, pr_kind_col(p->kind));
        ui_T(p->track ? "TRACK" : p->name ? p->name : "", 70.f, y, 14.f, 400, 0.f, UI_K3, QD_LEFT);
        if (p->in_pool && !v->pool && !v->picker)
            pr_tray(198.f, y - 13.f, UI_POOL_COL, 1.f);
        qd_restore();
    }
    pr_cassette(34.f, 82.f + dy * 0.3f, 172.f, 76.f, v->e[2].ok ? &v->e[2] : 0, col, v->t, !v->e[2].ok || v->e[2].track,
                "TRACK");
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
    pr_head(v->title, v->tcol, right, v->track, v->chip);
    if (!v->n) {                                        /* nothing in it */
        pr_cassette(34.f, 82.f, 172.f, 76.f, 0, v->col, v->t, 1, "\xe2\x80\x94");
        return;
    }
    {
        const int blink = (int)floorf(v->t * 3.f) % 2;
        pr_rollo(v, v->out_armed && blink ? PR_ORANGE : v->del_armed && blink ? PR_ORANGE : v->col);
    }
    if (v->pool) {                                      /* who uses it; OUT? */
        const qd_rgb uc = v->out_armed ? PR_ORANGE : UI_G1;
        float x = 60.f;
        int t;
        for (t = 0; t < 8; t++)
            if ((v->users >> t) & 1u) {
                ui_box(x, 204.f, t + 1, v->out_armed ? PR_ORANGE : UI_TRACK_PAL[t]);
                x += 24.f;
            }
        if (v->usteps) {
            char n[12] = "\xc3\x97";
            ui_itoa(n + 2, v->usteps);
            {
                const float w = ui_TW(n, 12.f, 600, 0.f) + 12.f;
                ui_RR(x, 205.f, w < 34.f ? 34.f : w, 15.f, 3.f, uc, 1.2f, QD_NONE);
                ui_T(n, x + (w < 34.f ? 34.f : w) / 2.f, 216.f, 12.f, 600, 0.f, uc, QD_CENTER);
            }
        }
        if (v->out_armed)
            ui_T("OUT?", 168.f, 216.f, 12.f, 600, 1.f, PR_ORANGE, QD_CENTER);
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
        }
    }
}

/* ---------------------------------------------------------------- SAVE PRESET */
static void pr_button(float x, float y, float w, const char *s, qd_rgb col)
{
    ui_RR(x, y, w, 26.f, 6.f, col, 1.8f, QD_NONE);
    ui_T(s, x + w / 2.f, y + 18.f, 14.f, 600, 1.f, col, QD_CENTER);
}

void ui_save_draw(const ui_save_t *v)
{
    pr_head("SAVE PRESET", UI_LIB_COL, "", v->track, 0);
    if (v->naming) {                                    /* the twelve cells, the letter under the cursor rolling */
        static const char AL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";
        const float cw = 18.5f, x0 = 9.f;
        int k;
        for (k = 0; k < 12; k++) {
            const float x = x0 + (float)k * cw;
            const int c = k == v->cur;
            char ch[2] = {v->name[k], 0};
            line2(x + 2.f, 120.f, x + cw - 3.f, 120.f, c ? UI_KSEL : UI_G2, c ? 2.2f : 1.2f);
            if (ch[0] && ch[0] != ' ') {
                const float dy = c && v->roll < 1.f ? (1.f - lb_ease(v->roll)) * 10.f * (float)v->roll_dir : 0.f;
                ui_T(ch, x + cw / 2.f, 114.f + dy, 24.f, c ? 400 : 300, 0.f, c ? UI_KSEL : UI_K3, QD_CENTER);
            }
        }
        {
            const char *p = strchr(AL, v->name[v->cur]);
            const int len = (int)sizeof AL - 1, i = p ? (int)(p - AL) : len - 2;
            const char up[2] = {AL[(i - 1 + len) % len], 0}, dn[2] = {AL[(i + 1) % len], 0};
            const float x = x0 + (float)v->cur * cw + cw / 2.f;
            qd_save();
            qd_alpha(qd_get_alpha() * 0.4f);
            if (up[0] != ' ')
                ui_T(up, x, 78.f, 20.f, 300, 0.f, UI_G1, QD_CENTER);
            if (dn[0] != ' ')
                ui_T(dn, x, 150.f, 20.f, 300, 0.f, UI_G1, QD_CENTER);
            qd_restore();
        }
    } else {
        ui_preset_t s = v->s;
        char nm[16];
        int e = 12;
        memcpy(nm, v->name, sizeof nm);
        nm[12] = 0;
        while (e > 0 && nm[e - 1] == ' ')
            nm[--e] = 0;
        s.name = nm;
        s.mine = 1, s.imported = 0, s.in_pool = 0;
        pr_cassette(34.f, 50.f, 172.f, 76.f, &s, UI_LIB_COL, v->t, 0, 0);
        if (v->spark) {                                 /* the name from the sound: the sparkle */
            const float a = 0.5f + 0.5f * (sinf(v->t * 5.f) + 1.f) * 0.5f;
            qd_save();
            qd_alpha(qd_get_alpha() * a);
            line2(200.f, 54.f, 208.f, 54.f, UI_LIB_COL, 1.3f);
            line2(204.f, 50.f, 204.f, 58.f, UI_LIB_COL, 1.3f);
            qd_restore();
        }
        if (!v->replace) {                              /* the dice: another name */
            static const float PIP[5][2] = {{6, 6}, {12, 12}, {18, 18}, {18, 6}, {6, 18}};
            int k;
            ui_RR(108.f, 150.f, 24.f, 24.f, 4.f, UI_LIB_COL, 1.4f, QD_NONE);
            for (k = 0; k < 5; k++)
                ui_O(108.f + PIP[k][0], 150.f + PIP[k][1], 1.6f, QD_NONE, 1.f, UI_LIB_COL);
        }
    }
    if (v->replace) {                                   /* NEW (as the new name) or REPLACE */
        if (!v->naming && v->newname)
            ui_T(v->newname, 120.f, 160.f, 14.f, 400, 0.f, UI_G1, QD_CENTER);
        pr_button(12.f, 196.f, 104.f, "NEW", UI_LIB_COL);
        pr_button(124.f, 196.f, 104.f, "REPLACE", PR_ORANGE);
    } else {
        pr_button(62.f, 196.f, 116.f, "NEW", UI_LIB_COL);
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
