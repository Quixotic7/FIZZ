/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The project pages (P1.5a; ui.h ui_prj_t; P1.5u: the name editor is the typewriter's, ui_type.c; P1.5t: the sounds page's grid is gone, the
 * library browser is ui_presets.c), drawn as mockups/fizz-save-pool.html draws them: its positions, sizes, colours
 * and motions. Built through ui_layer.c (it
 * includes this file: its line2 and cat). PLAN §9a's clarity rules: names, numbers and the tabs' words; the rest is
 * drawing, colour and motion. */

static float lb_clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static float lb_ease(float x) { x = lb_clamp(x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }
static float lb_hash(int n)
{
    const float s = sinf((float)n * 127.1f + 311.7f) * 43758.5453f;
    return s - floorf(s);
}

/* a sound's amp envelope as its silhouette */
static void lb_glyph(float x, float y, float w, float h, const float *env, qd_rgb col, float lw)
{
    const float at = env[0], de = env[1], su = env[2], re = env[3], tot = 0.15f + at + de + 0.35f + re, k = w / tot;
    const float x1 = x + 0.08f * k, x2 = x1 + at * k, x3 = x2 + de * k, x4 = x3 + 0.35f * k, x5 = x4 + re * k;
    const float base = y + h, sy = base - su * h;
    const float p[14] = {x, base, x1, base, x2, y, x3, sy, x4, sy, x5 > x + w ? x + w : x5, base, x + w, base};
    ui_P(p, 7, col, lw, 0, QD_NONE);
}

static void lb_dashed_rr(float x, float y, float w, float h, float r, qd_rgb col, float lw)
{
    static const float D[2] = {2.f, 3.f};
    qd_dash(D, 2);
    ui_RR(x, y, w, h, r, col, lw, QD_NONE);
    qd_dash(0, 0);
}

static void lb_bubble(float x, float y, float r, qd_rgb col, float a)
{
    qd_save();
    qd_alpha(lb_clamp(a, 0.f, 1.f));
    ui_O(x, y, r, col, 1.f, QD_NONE);
    if (r > 2.5f)
        ui_O(x - r * 0.3f, y - r * 0.3f, r * 0.35f, QD_NONE, 1.f, col);
    qd_restore();
}

static void lb_back(void)
{
    ui_RR(208.f, 4.f, 26.f, 17.f, 3.f, UI_K3, 1.3f, QD_NONE);
    ui_icon(UI_IC_BACK, 221.f, 12.5f, 1.f, UI_K3);
}

static void lb_num3(char *b, int n)
{
    b[0] = (char)('0' + n / 100 % 10), b[1] = (char)('0' + n / 10 % 10), b[2] = (char)('0' + n % 10), b[3] = 0;
}

/* the cassette (the firmware's sound picture), its reels turning */
static void lb_cassette(float x, float y, float w, float h, const ui_libtile_t *s, int num, qd_rgb col, float t)
{
    char n[8];
    int j, k;
    ui_RR(x, y, w, h, 7.f, col, 1.6f, UI_BG);
    ui_RR(x + 10.f, y + 8.f, w - 20.f, h * 0.36f, 3.f, UI_G2, 1.f, QD_NONE);
    lb_num3(n, num);
    ui_T(n, x + 16.f, y + 23.f, 12.f, 600, 0.f, col, QD_LEFT);
    ui_T(s->name ? s->name : "", x + w / 2.f, y + 8.f + h * 0.36f - 6.f, 20.f, 500, 0.f, UI_K3, QD_CENTER);
    for (j = 0; j < 2; j++) {
        const float rx = x + w * (j ? 0.7f : 0.3f), ry = y + h * 0.72f, r = h * 0.15f < 13.f ? h * 0.15f : 13.f;
        ui_O(rx, ry, r, UI_G1, 1.3f, QD_NONE);
        for (k = 0; k < 3; k++) {
            const float an = t * 3.f + (float)k * UI_TAU / 3.f + (float)j;
            line2(rx + cosf(an) * r * 0.35f, ry + sinf(an) * r * 0.35f, rx + cosf(an) * r * 0.8f, ry + sinf(an) * r * 0.8f,
                  UI_G1, 1.2f);
        }
    }
}

/* P1.5r: a sound imported over SysEx: its cassette dropping in, its reels turning, the slot it went to, "imported" */
void ui_import_draw(const char *name, int pool, int slot, float t)
{
    const qd_rgb col = pool ? UI_POOL_COL : UI_LIB_COL;
    const float drop = t < 0.25f ? (1.f - lb_ease(t / 0.25f)) * -40.f : 0.f;
    ui_libtile_t s;
    memset(&s, 0, sizeof s);
    s.name = name;
    ui_RR(0.f, 0.f, 240.f, 240.f, 0.f, QD_NONE, 1.f, UI_BG);
    ui_T(pool ? "POOL" : "MY PRESETS", 120.f, 28.f, 12.f, 600, 1.f, col, QD_CENTER);   /* (P1.5t: MINE) */
    lb_cassette(30.f, 46.f + drop, 180.f, 110.f, &s, slot, col, t);
    ui_T("imported", 120.f, 196.f, 22.f, 300, 0.f, UI_K3, QD_CENTER);
}

/* ---------------------------------------------------------------- the project pages */
static void prj_meter(float y, float frac, float own, qd_rgb col, int chip, const char *txt)
{
    const float x0 = 32.f, w = 160.f;
    if (chip) {                                         /* the flash: a chip */
        int k;
        ui_RR(10.f, y - 7.f, 14.f, 14.f, 2.f, UI_G1, 1.2f, QD_NONE);
        for (k = 0; k < 3; k++) {
            line2(13.f + (float)k * 4.f, y - 9.f, 13.f + (float)k * 4.f, y - 7.f, UI_G1, 1.f);
            line2(13.f + (float)k * 4.f, y + 7.f, 13.f + (float)k * 4.f, y + 9.f, UI_G1, 1.f);
        }
    } else {                                            /* the pool: a tape */
        ui_RR(10.f, y - 6.f, 16.f, 12.f, 2.f, UI_G1, 1.2f, QD_NONE);
        ui_O(15.f, y + 1.f, 2.f, UI_G1, 1.f, QD_NONE);
        ui_O(21.f, y + 1.f, 2.f, UI_G1, 1.f, QD_NONE);
    }
    line2(x0, y, x0 + w, y, UI_G3, 4.f);
    if (frac > 0.f)
        line2(x0, y, x0 + w * lb_clamp(frac, 0.f, 1.f), y, UI_G1, 4.f);
    if (own > 0.f)
        line2(x0 + w * lb_clamp(frac - own, 0.f, 1.f), y, x0 + w * lb_clamp(frac, 0.f, 1.f), y, col, 4.f);
    ui_T(txt, 232.f, y + 5.f, 14.f, 400, 0.f, UI_K3, QD_RIGHT);
}

static void prj_dots(float x, int page)
{
    int i;
    for (i = 0; i < 2; i++)
        ui_O(x + (float)i * 9.f, 13.f, 2.6f, i == page ? QD_NONE : UI_G1, 1.f, i == page ? UI_K3 : QD_NONE);
}

void ui_prj_draw(const ui_prj_t *v)
{
    static const char *const BANK[4] = {"A", "B", "C", "D"};
    char n[16];
    int i;
    if (!v->page) {
        ui_T("PROJECT", 8.f, 17.f, 12.f, 600, 1.f, UI_LIB_COL, QD_LEFT);
        prj_dots(82.f, 0);
        lb_back();
        line2(8.f, 25.f, 232.f, 25.f, UI_G3, 1.f);
        n[0] = (char)('0' + v->num / 10), n[1] = (char)('0' + v->num % 10), n[2] = 0;
        ui_T(n, 8.f, 66.f, 38.f, 200, 0.f, UI_LIB_COL, QD_LEFT);
        ui_T(v->name ? v->name : "", 62.f, 53.f, 20.f, 300, 0.f, UI_K3, QD_LEFT);
        for (i = 0; i < 4; i++)                         /* the 64 patterns: a speck empty, a ring used, filled the
                                                         * one worked on; bubbles off one just written */
            ui_T(BANK[i], 10.f, 92.f + (float)i * 15.f + 4.5f, 12.f, 600, 0.f, UI_G1, QD_LEFT);
        for (i = 0; i < 64; i++) {
            const float x = 32.f + (float)(i % 16) * 12.6f + (float)((i % 16) / 4) * 2.f, y = 92.f + (float)(i / 16) * 15.f;
            if (i == v->cur)
                ui_O(x, y, 4.2f, QD_NONE, 1.f, UI_K3);
            else if (v->used[i])
                ui_O(x, y, 4.f, UI_G1, 1.3f, UI_BG);
            else
                ui_O(x, y, 1.3f, QD_NONE, 1.f, UI_G2);
            if (v->since[i] >= 0.f && v->since[i] < 1.35f) {
                const float w = v->since[i] / 3.f;
                int k;
                for (k = 0; k < 3; k++) {
                    const float uu = lb_clamp(w / 0.45f - (float)k * 0.15f, 0.f, 1.f);
                    if (uu > 0.f && uu < 1.f)
                        lb_bubble(x + (float)(k - 1) * 3.f, y - 6.f - uu * 12.f, 1.2f + (float)k * 0.4f, UI_LIB_COL,
                                  1.f - uu);
                }
            }
        }
        n[0] = 0;
        cat(cat(n, ui_itoa(n + 8, (int)(v->flash * 100.f + 0.5f))), "%");
        prj_meter(182.f, v->flash, v->flash_own, UI_LIB_COL, 1, n);
        prj_meter(214.f, (float)v->pool_n / 128.f, (float)v->pool_n / 128.f, UI_POOL_COL, 0, ui_itoa(n, v->pool_n));
        return;
    }
    ui_T("PROJECTS", 8.f, 17.f, 12.f, 600, 1.f, UI_LIB_COL, QD_LEFT);
    prj_dots(92.f, 1);
    lb_back();
    line2(8.f, 25.f, 232.f, 25.f, UI_G3, 1.f);
    for (i = 0; i < UI_NPROJ; i++) {                    /* (four a row; the Bluetooth package's four: one row, in
                                                         * the middle) */
        const float x = 6.f + (float)(i % 4) * 58.f, y = (UI_NPROJ > 4 ? 32.f : 84.f) + (float)(i / 4) * 104.f;
        const float w = 54.f, h = 98.f;
        const int cur = i == v->num - 1, pick = i == v->chosen && !cur, blink = (int)floorf(v->t * 3.f) % 2;
        qd_rgb col;
        int q;
        if (!v->p[i].exists) {
            lb_dashed_rr(x, y, w, h, 6.f, pick ? UI_K3 : i == v->armed ? UI_K4 : UI_G2, 1.f);
            line2(x + w / 2.f - 6.f, y + h / 2.f, x + w / 2.f + 6.f, y + h / 2.f, UI_G2, 1.4f);
            line2(x + w / 2.f, y + h / 2.f - 6.f, x + w / 2.f, y + h / 2.f + 6.f, UI_G2, 1.4f);
            continue;
        }
        col = i == v->armed ? UI_K4 : i == v->src ? UI_POOL_COL : cur ? UI_LIB_COL : pick ? UI_K3 : UI_G2;
        qd_save();
        qd_alpha(pick && !blink ? 0.45f : 1.f);
        ui_RR(x, y, w, h, 6.f, col, cur || pick || i == v->src || i == v->armed ? 1.8f : 1.1f, QD_NONE);
        qd_restore();
        n[0] = (char)('0' + (i + 1) / 10), n[1] = (char)('0' + (i + 1) % 10), n[2] = 0;
        ui_T(n, x + 7.f, y + 30.f, 26.f, 200, 0.f, cur ? UI_LIB_COL : UI_K3, QD_LEFT);
        {   /* its name, a word a line */
            const char *s = v->p[i].name ? v->p[i].name : "";
            char wd[16];
            int line = 0;
            while (*s && line < 2) {
                int m = 0;
                while (*s == ' ')
                    s++;
                while (*s && *s != ' ' && m < 15)
                    wd[m++] = *s++;
                wd[m] = 0;
                if (!m)
                    break;
                qd_save();
                qd_clip_rect(x + 2.f, y, w - 4.f, h);
                ui_T(wd, x + 7.f, y + 50.f + (float)line * 15.f, 12.f, 500, 0.f, UI_G1, QD_LEFT);
                qd_restore();
                line++;
            }
        }
        for (q = 0; q < 64; q++)                        /* its patterns in use */
            qd_fill_rect(x + 8.f + (float)(q % 16) * 2.4f, y + 80.f + (float)(q / 16) * 3.4f, 1.6f, 1.6f,
                         v->p[i].used[q] ? (cur ? UI_LIB_COL : UI_G1) : UI_G3);
    }
}
