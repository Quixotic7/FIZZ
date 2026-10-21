/* SPDX-License-Identifier: GPL-3.0-only */
/* The sounds page, the name editor and the project pages (P1.5a; ui.h ui_lib_t, ui_name_t, ui_prj_t), drawn as
 * mockups/fizz-save-pool.html draws them: its positions, sizes, colours and motions. Built through ui_layer.c (it
 * includes this file: its line2 and cat). PLAN §9a's clarity rules: names, numbers and the tabs' words; the rest is
 * drawing, colour and motion. */

static float lb_clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static float lb_ease(float x) { x = lb_clamp(x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }
static float lb_hash(int n)
{
    const float s = sinf((float)n * 127.1f + 311.7f) * 43758.5453f;
    return s - floorf(s);
}

/* a sound's kind (sounds.h QD_SK_*: BASS KEYS PLUCK BELL PAD LEAD KICK SNARE CLAP HAT NOISE) as its colour */
static qd_rgb lb_kind_col(int k)
{
    static const qd_rgb C[11] = {UI_K1, UI_K3, UI_K3, UI_K3, UI_PAD_COL, UI_K2, UI_K4, UI_K4, UI_K4, UI_K4, UI_G1};
    return k >= 0 && k < 11 ? C[k] : UI_G1;
}

static void lb_tile_xy(int i, float *x, float *y)
{
    *x = 6.f + (float)(i % 4) * 58.f;
    *y = 64.f + (float)(i / 4) * 44.f;
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

/* one sound: its outline, its silhouette in its kind's colour, its name (selected: hi; col: an outline colour) */
static void lb_tile(const ui_libtile_t *s, float x, float y, int sel, qd_rgb hi, qd_rgb col)
{
    const float w = 54.f, h = 40.f;
    if (!s || !s->used) {
        lb_dashed_rr(x, y, w, h, 5.f, col != QD_NONE ? col : UI_G2, 1.f);
        return;
    }
    ui_RR(x, y, w, h, 5.f, col != QD_NONE ? col : sel ? hi : UI_G2, sel || col != QD_NONE ? 1.8f : 1.1f, UI_BG);
    lb_glyph(x + 7.f, y + 6.f, w - 14.f, 13.f, s->env, lb_kind_col(s->kind), sel ? 1.7f : 1.2f);
    qd_save();
    qd_clip_rect(x + 2.f, y, w - 4.f, h);
    ui_T(s->name ? s->name : "", x + 5.f, y + 34.f, 12.f, 500, 0.f, sel ? UI_K3 : UI_G1, QD_LEFT);
    qd_restore();
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

static void lb_fizz(float x, float y, float b, qd_rgb col)   /* a tile fizzing away (b 0..1) */
{
    int k;
    for (k = 0; k < 10; k++)
        lb_bubble(x + 8.f + lb_hash(k) * 38.f, y + 20.f - b * (24.f + 30.f * lb_hash(k + 3)), 1.2f + 2.f * lb_hash(k + 9),
                  col, 1.f - b);
}

static void lb_tray(float x, float y, qd_rgb col, float a)   /* the pool as a little tray of four slots, a plus */
{
    int k;
    qd_save();
    qd_alpha(a);
    ui_RR(x, y, 30.f, 22.f, 4.f, col, 1.5f, QD_NONE);
    for (k = 0; k < 4; k++)
        ui_RR(x + 4.f + (float)(k % 2) * 12.f, y + 4.f + (float)(k / 2) * 8.f, 10.f, 6.f, 1.5f, col, 1.f,
              k == 3 ? QD_NONE : col);
    line2(x + 36.f, y + 11.f, x + 46.f, y + 11.f, col, 1.6f);
    line2(x + 41.f, y + 6.f, x + 41.f, y + 16.f, col, 1.6f);
    qd_restore();
}

static void lb_back(void)
{
    ui_RR(208.f, 4.f, 26.f, 17.f, 3.f, UI_K3, 1.3f, QD_NONE);
    ui_icon(UI_IC_BACK, 221.f, 12.5f, 1.f, UI_K3);
}

static void lb_tabs(int tab, int bank)
{
    const qd_rgb col = tab ? UI_POOL_COL : UI_LIB_COL;
    int i;
    ui_T("LIBRARY", 8.f, 17.f, 12.f, 600, 1.f, tab ? UI_G2 : UI_LIB_COL, QD_LEFT);
    ui_T("POOL", 82.f, 17.f, 12.f, 600, 1.f, tab ? UI_POOL_COL : UI_G2, QD_LEFT);
    if (tab)
        line2(82.f, 22.f, 115.f, 22.f, UI_POOL_COL, 1.6f);
    else
        line2(8.f, 22.f, 71.f, 22.f, UI_LIB_COL, 1.6f);
    for (i = 0; i < 8; i++)                             /* the banks */
        ui_RR(126.f + (float)i * 9.f, 8.f, 7.f, 7.f, 1.5f, i == bank ? QD_NONE : UI_G2, 1.f, i == bank ? col : QD_NONE);
    lb_back();
    line2(8.f, 25.f, 232.f, 25.f, UI_G3, 1.f);
}

static void lb_num3(char *b, int n)
{
    b[0] = (char)('0' + n / 100 % 10), b[1] = (char)('0' + n / 10 % 10), b[2] = (char)('0' + n % 10), b[3] = 0;
}

/* who uses it: the tracks' boxes, then a padlock and the steps' count; its width */
static float lb_usage(uint8_t tracks, int steps, float x, float y, qd_rgb lcol, int draw)
{
    float w = 0.f;
    int t;
    for (t = 0; t < 8; t++)
        if ((tracks >> t) & 1u) {
            if (draw)
                ui_box(x + w, y, t + 1, UI_TRACK_PAL[t]);
            w += 21.f;
        }
    if (steps) {
        char n[16];
        n[0] = 0;
        cat(cat(n, "x"), ui_itoa(n + 8, steps));
        if (draw) {
            ui_lock(x + w + 2.f, y + 2.f, lcol, 1.2f);
            ui_T(n, x + w + 14.f, y + 13.f, 12.f, 600, 0.f, lcol, QD_LEFT);
        }
        w += 36.f;
    }
    return w;
}

static void lb_chosen(const ui_lib_t *v, const ui_libtile_t *s, int num, qd_rgb col, qd_rgb lcol, int track)
{
    char n[8];
    lb_num3(n, num);
    if (!s || !s->used) {
        ui_T(n, 8.f, 41.f, 12.f, 600, 1.f, UI_G2, QD_LEFT);
        return;
    }
    ui_T(n, 8.f, 41.f, 12.f, 600, 1.f, col, QD_LEFT);
    ui_T(s->name ? s->name : "", 8.f, 59.f, 20.f, 300, 0.f, UI_K3, QD_LEFT);
    if (track)
        ui_box(216.f, 30.f, track, UI_TRACK_PAL[(track - 1) & 7]);
    if (v->tab) {
        const float w = lb_usage(v->utracks, v->usteps, 0.f, 0.f, lcol, 0);
        lb_usage(v->utracks, v->usteps, 232.f - w, 30.f, lcol, 1);
    }
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

/* the sound playing: its wave under its envelope, scrolling */
static void lb_scope(float x, float y, float w, float h, float t, const float *env, qd_rgb col)
{
    float p[2 * 161];
    int i;
    for (i = 0; i <= 160; i++) {
        const float u = (float)i / 160.f, ph = fmodf(t * 1.2f, 1.6f);
        const float age = lb_clamp(fmodf(u * 1.6f - (1.6f - ph) + 1.6f, 1.6f), 0.f, 1.6f);
        const float e = age < env[0] * 0.4f ? age / (env[0] * 0.4f + 1e-3f)
                                             : fmaxf(env[2], 1.f - (age - env[0] * 0.4f) / (env[1] * 1.2f + 0.05f));
        const float s = sinf(u * UI_TAU * 9.f) * 0.7f + sinf(u * UI_TAU * 27.f + 1.f) * 0.3f * e;
        p[2 * i] = x + u * w;
        p[2 * i + 1] = y - s * h * lb_clamp(e, 0.f, 1.f) * 0.9f;
    }
    ui_P(p, 161, col, 1.4f, 0, QD_NONE);
    line2(x, y, x + w, y, UI_G3, 1.f);
}

void ui_lib_draw(const ui_lib_t *v)
{
    const qd_rgb col = v->tab ? UI_POOL_COL : UI_LIB_COL;
    const int blink = (int)floorf(v->t * 3.f) % 2;
    int i;
    lb_tabs(v->tab, v->bank);
    if (v->aud) {                                       /* a sound held: it takes the screen */
        lb_cassette(30.f, 36.f, 180.f, 104.f, &v->c, v->cnum, col, v->t);
        lb_scope(16.f, 186.f, 208.f, 24.f, v->t, v->c.env, lb_kind_col(v->c.kind));
        return;
    }
    if (v->fx_kind == 4 && v->fx_u < 1.f) {             /* onto the track: a bar in its colour */
        const qd_rgb tc = UI_TRACK_PAL[(v->track - 1) & 7];
        const float f = 1.f - lb_ease(v->fx_u);
        qd_save();
        qd_alpha(0.4f + 0.6f * f);
        ui_RR(4.f, 28.f, 232.f, 32.f, 6.f, tc, 1.5f, QD_NONE);
        qd_restore();
        ui_box(12.f, 36.f, v->track, tc);
        ui_T(v->track_name ? v->track_name : "", 34.f, 49.f, 12.f, 600, 1.f, tc, QD_LEFT);
        ui_T(v->c.name ? v->c.name : "", 228.f, 51.f, 20.f, 300, 0.f, UI_K3, QD_RIGHT);
    } else if (v->storing) {                            /* the sound being stored, its name from the sound */
        char n[8];
        lb_num3(n, v->cnum);
        ui_T(n, 8.f, 41.f, 12.f, 600, 1.f, col, QD_LEFT);
        ui_T(v->s.name ? v->s.name : "", 8.f, 59.f, 20.f, 300, 0.f, UI_K3, QD_LEFT);
        ui_box(216.f, 30.f, v->track, UI_TRACK_PAL[(v->track - 1) & 7]);
        if (v->spark) {                                 /* a name from the sound: a sparkle */
            const float sp = (sinf(v->t * 5.f) + 1.f) * 0.5f, nx = 8.f + ui_TW(v->s.name ? v->s.name : "", 20.f, 300, 0.f) + 8.f;
            qd_save();
            qd_alpha(0.5f + sp * 0.5f);
            line2(nx, 47.f, nx + 8.f, 47.f, col, 1.3f);
            line2(nx + 4.f, 43.f, nx + 4.f, 51.f, col, 1.3f);
            qd_restore();
        }
    } else if (v->fx_kind == 3 && v->fx_u < 0.35f) {    /* deleting: its users in orange first */
        ui_libtile_t d = v->del;
        char n[8];
        lb_num3(n, v->cnum);
        ui_T(n, 8.f, 41.f, 12.f, 600, 1.f, col, QD_LEFT);
        ui_T(d.name ? d.name : "", 8.f, 59.f, 20.f, 300, 0.f, UI_K3, QD_LEFT);
        {
            const float w = lb_usage(v->del_tracks, v->del_steps, 0.f, 0.f, UI_K4, 0);
            lb_usage(v->del_tracks, v->del_steps, 232.f - w, 30.f, UI_K4, 1);
        }
    } else {
        lb_chosen(v, &v->c, v->cnum, col, UI_G1, 0);
    }
    if (v->offer)                                       /* stored: the pool's tray blinks with F2 */
        lb_tray(162.f, 32.f, UI_POOL_COL, blink ? 1.f : 0.35f);
    for (i = 0; i < 16; i++) {
        float x, y;
        lb_tile_xy(i, &x, &y);
        if (i == v->lift)
            continue;
        if (v->storing && i == v->chosen) {             /* the slot it goes into */
            if (v->armed) {                             /* a sound there: orange, the old and the new in turn */
                lb_tile(blink ? &v->tile[i] : &v->s, x, y, 0, col, UI_K4);
            } else {
                qd_save();
                qd_alpha(0.45f + 0.55f * powf(sinf(v->t * 6.f), 2.f));
                lb_dashed_rr(x, y, 54.f, 40.f, 5.f, col, 1.4f);
                qd_restore();
                if (v->tile[i].used)
                    lb_tile(&v->tile[i], x, y, 0, col, QD_NONE);
                {                                       /* the track's sound flies from its box into the slot */
                    const float u = fmodf(v->t, 2.4f) / 2.4f, e = lb_ease(lb_clamp((u - 0.1f) / 0.5f, 0.f, 1.f));
                    qd_save();
                    qd_alpha(0.5f + 0.5f * e);
                    lb_tile(&v->s, 180.f + (x - 180.f) * e, 4.f + (y - 4.f) * e, 0, col, UI_TRACK_PAL[(v->track - 1) & 7]);
                    qd_restore();
                }
            }
            continue;
        }
        if (v->fx_kind == 3 && i == v->fx_tile && v->fx_u < 1.f) {   /* deleted: orange, then it fizzes away */
            if (v->fx_u < 0.35f) {
                lb_tile(&v->del, x, y, 0, col, UI_K4);
            } else {
                const float b = (v->fx_u - 0.35f) / 0.65f;
                qd_save();
                qd_alpha(b);
                lb_tile(0, x, y, 0, col, QD_NONE);
                qd_restore();
                lb_fizz(x, y, b, lb_kind_col(v->del.kind));
            }
            continue;
        }
        lb_tile(&v->tile[i], x, y, i == v->chosen, col, i == v->armed_tile ? UI_K4 : QD_NONE);
        if ((v->fx_kind == 1 || v->fx_kind == 2) && i == v->fx_tile && v->fx_u < 1.f)
            lb_fizz(x, y, v->fx_u, col);
    }
    if (v->lift >= 0 && v->lift < 16) {                 /* a tile lifted (copied / picked up): over its hole */
        float x, y;
        lb_tile_xy(v->lift, &x, &y);
        if (v->lift_kind == 2)
            lb_dashed_rr(x, y, 54.f, 40.f, 5.f, UI_G2, 1.f);
        ui_RR(x + 3.f, y + 3.f, 54.f, 40.f, 5.f, QD_NONE, 1.f, 0x111111u);
        lb_tile(&v->tile[v->lift], x, y - 6.f, 0, col, col);
    }
}

/* ---------------------------------------------------------------- the name editor */
void ui_name_draw(const ui_name_t *v)
{
    static const char AL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";
    const float cw = 18.5f, x0 = 9.f, cx = x0 + (float)v->cur * cw + cw / 2.f;
    const int len = (int)sizeof AL - 1;
    const char *p = strchr(AL, v->buf[v->cur] ? v->buf[v->cur] : ' ');
    const int li = p ? (int)(p - AL) : len - 2;
    int k, d;
    ui_T("NAME", 8.f, 17.f, 12.f, 600, 1.f, v->col, QD_LEFT);
    lb_back();
    line2(8.f, 25.f, 232.f, 25.f, UI_G3, 1.f);
    for (k = 0; k < 12; k++) {                          /* the twelve cells */
        const float x = x0 + (float)k * cw;
        char ch[2] = {v->buf[k], 0};
        line2(x + 2.f, 150.f, x + cw - 3.f, 150.f, k == v->cur ? UI_KSEL : UI_G2, k == v->cur ? 2.2f : 1.2f);
        if (k != v->cur && ch[0] && ch[0] != ' ')
            ui_T(ch, x + cw / 2.f, 144.f, 24.f, 300, 0.f, UI_K3, QD_CENTER);
    }
    qd_save();                                          /* the letter drum over the cursor */
    qd_clip_rect(cx - 14.f, 40.f, 28.f, 112.f);
    for (d = -3; d <= 3; d++) {
        char ch[2] = {AL[((li + d) % len + len) % len], 0};
        const float yy = 144.f + ((float)d + (1.f - lb_ease(v->roll)) * (float)v->roll_dir) * 34.f;   /* (a turn: the
                                                         * drum rolls the new letter in) */
        qd_save();
        qd_alpha(d ? 0.35f : 1.f);
        ui_T(ch[0] == ' ' ? "_" : ch, cx, yy, 24.f, d ? 300 : 400, 0.f, d ? UI_G1 : UI_KSEL, QD_CENTER);
        qd_restore();
    }
    qd_restore();
    ui_RR(cx - 14.f, 116.f, 28.f, 36.f, 4.f, UI_KSEL, 1.4f, QD_NONE);
    {   /* the dice: B10 again, another name from the sound */
        static const float PIP[5][2] = {{6, 6}, {12, 12}, {18, 18}, {18, 6}, {6, 18}};
        const float dx = 200.f, dy = 192.f;
        ui_RR(dx, dy, 24.f, 24.f, 4.f, v->col, 1.4f, QD_NONE);
        for (k = 0; k < 5; k++)
            ui_O(dx + PIP[k][0], dy + PIP[k][1], 1.6f, QD_NONE, 1.f, v->col);
    }
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
    for (i = 0; i < 8; i++) {
        const float x = 6.f + (float)(i % 4) * 58.f, y = 32.f + (float)(i / 4) * 104.f, w = 54.f, h = 98.f;
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
