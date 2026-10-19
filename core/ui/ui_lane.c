/* SPDX-License-Identifier: GPL-3.0-only */
/* Screen 2, the step view (PLAN.md §8b; mockups/line.html s2's frame): the page edited, drawn as the current hold
 * mode's lane inside the sixteen white keys' outlines.
 *
 * The frame, every mode: the top line with the track's box and name, the mode's name small in its colour, the tempo
 * grey and the pattern; under it the eight pages as dashes, each drawn over its own start..end only (a dark hairline
 * for the rest), in the loop grey, out of it dark, the page edited white and thicker, the playing page's place red (on
 * the page edited: the playhead's own place along its dash). The sixteen outlines (the playhead's column lighter, a
 * held one white, a step outside the page's start..end dotted and empty), the red playhead, the page's step numbers
 * under them; a page change slides the lane in from the side; page follow off: a small FOLLOW OFF left of the tempo.
 *
 * In the columns, the mode's lane. A step that overrides the mode is drawn in the mode's colour, a step on the track's
 * default grey, the step the playhead is in white; the track's default itself is drawn across the page (dashed) and
 * named at the foot, so what a default change would move is in sight:
 *   NOTE   the piano roll (P1.3): the notes as bars at their pitch and length, micro-timing off the column, a
 *          ratchet dashed into its hits; the default chord's notes as dashed lines; under a column its other
 *          overrides as a little stack of dots in their modes' colours (a chance, a condition, a sound lock, p-locks)
 *   VEL    a bar up from the floor as high as the velocity; the default a dashed line at its height
 *   LEN    the gate as a bar over the steps it covers, a row a step (a staircase, so long notes never hide each
 *          other); an overriding step shows the default's length dashed under its own
 *   MICRO  each step's start as a flag off its column's grid line (the offset joined at the foot); the default
 *          offset dashed in every column
 *   RATCH  the hits as a ladder of ticks up from the floor, each as wide as its hit's velocity (the ramp); the
 *          default count a dashed line
 *   CHNC   ten dots up the column, as many lit as the chance's tens; the default a dashed line
 *   COND   an A:B as B little circles up the column with the A-th filled, its name over them; another condition
 *          its name's letters stacked; ALWAYS a dot
 *   TRIG   NOTE a dot, LOCK a padlock, TIE a slur from the step before
 *   SOUND  a dot a step (the track's sound grey), a sound lock's pool number under its dot
 *   P-LOCK (P1.3c, B11) the NOTE roll, the steps with p-locks in the padlock's colour, and under each column a padlock
 *          with its lock count (a held step's column outlined white: holding keeps this screen, the knobs lock it)
 * A LOCK step (trigless) in NOTE, VEL, LEN, MICRO and RATCH is a small ring: it plays no note. */
#include <math.h>
#include <string.h>

#include "ui.h"

#define TOP 36.f                                        /* the outlines' top and bottom */
#define BOT 210.f
#define COLW 12.f
#define XEND (9.f + 15.f * 13.5f + 6.f + COLW)          /* the last outline's right edge */

static const float DASH[2] = {2.f, 3.f};

static float col_x(int i) { return 9.f + (float)i * 13.5f + (float)(i / 4) * 2.f; }

static void line2(float x0, float y0, float x1, float y1, qd_rgb col, float lw)
{
    const float p[4] = {x0, y0, x1, y1};
    ui_P(p, 2, col, lw, 0, QD_NONE);
}

static void dashed(float x0, float y0, float x1, float y1, qd_rgb col)
{
    qd_save();
    qd_dash(DASH, 2);
    line2(x0, y0, x1, y1, col, 1.f);
    qd_restore();
}

/* the default's colour: grey, or (P1.3d) the mode's while a knob turns the default */
static qd_rgb def_col = UI_G1;

/* the default's dashed line across the page */
static void def_line(float y) { dashed(col_x(0), y, XEND, y, def_col); }

/* the sequencer's ramp (core/seq/seq.c ramp_vel): hit h of n at velocity v; ramp 0 flat, 1 up, 2 down, 3 accent */
static int ramp_vel(int v, int ramp, int h, int n)
{
    int x = v;
    if (n > 1) {
        if (ramp == 1)
            x = v * (h + 1) / n;
        else if (ramp == 2)
            x = v * (n - h) / n;
        else if (ramp == 3)
            x = h ? v * 5 / 8 : v;
    }
    return x < 1 ? 1 : x > 127 ? 127 : x;
}

/* a column's colour: the playhead's white, an override the mode's, the track's default grey */
static qd_rgb ccol(const ui_lane_t *v, const ui_lcol_t *c)
{
    return c->lit ? UI_K3 : c->ovr ? UI_MODE_COL[v->mode] : UI_G1;
}

/* a bar outlined in col, filled faintly (solid when lit) */
static void bar(float x, float y, float w, float h, qd_rgb col, int lit)
{
    const float r = (w < h ? w : h) / 2.f;
    if (!lit) {
        qd_save();
        qd_alpha(0.28f);
        ui_RR(x, y, w, h, r, QD_NONE, 1.f, col);
        qd_restore();
    }
    ui_RR(x, y, w, h, r, col, 1.2f, lit ? col : QD_NONE);
}

/* a trigless LOCK step: a small ring */
static void lock_ring(float cx, float y, const ui_lcol_t *c)
{
    ui_O(cx, y, 3.f, c->held ? UI_K3 : UI_G1, 1.2f, QD_NONE);
}

/* ---------------------------------------------------------------- NOTE: the piano roll */
static void l_note(const ui_lane_t *v)
{
    const int span = v->span > 0 ? v->span : 16;
    const float k = 150.f / (float)span;
    int i, j, r;
#define NY(n) (200.f - (float)((int)(n) - v->lo) * k)

    for (j = 0; j < v->def_nnote && j < 6; j++) {      /* the default chord, dashed */
        const float y = NY(v->def_note[j]);
        if (y >= TOP + 2.f && y <= BOT - 2.f)
            def_line(y);
    }
    qd_save();
    qd_clip_rect(0.f, TOP, 240.f, BOT - TOP);
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(col_x(i) + 6.f, 123.f, c);
        } else {
            const float x0 = col_x(i) + 1.f + (float)c->micro / 24.f * 13.5f;
            float w = c->len * 13.5f - 2.f;
            const int nr = c->ratch > 1 ? c->ratch : 1;
            const qd_rgb fill = c->lit ? UI_K3 : c->held ? QD_NONE : c->ovr ? UI_MODE_COL[v->mode] : UI_G1;
            const qd_rgb line = c->held && !c->lit ? UI_K3 : QD_NONE;
            if (!(w < 238.f - x0))                      /* INF and long notes: to the page's end */
                w = 238.f - x0;
            if (w < 3.f)
                w = 3.f;
            for (j = 0; j < c->nnote && j < 6; j++) {
                const float y = NY(c->note[j]);
                if (nr > 1) {
                    for (r = 0; r < nr; r++) {
                        const float pw = w / (float)nr - 1.5f;
                        ui_RR(x0 + (float)r * w / (float)nr, y - 2.f, pw < 1.5f ? 1.5f : pw, 4.f, 2.f, line, 1.f,
                              fill);
                    }
                } else {
                    ui_RR(x0, y - 2.f, w, 4.f, 2.f, line, 1.f, fill);
                }
            }
        }
    }
    qd_restore();
#undef NY
    for (i = 0; i < 16; i++) {                          /* the markers under a column: its other overrides */
        const ui_lcol_t *c = &v->col[i];
        const float mx = col_x(i) + 6.f;
        float y = 215.f;
        if (!c->in_range || !c->on)
            continue;
        if (v->mode == UI_F_PLOCK) {                    /* P-LOCK: a padlock and the count */
            char n[12];
            const qd_rgb col = c->nlock ? UI_MODE_COL[UI_F_PLOCK] : UI_G3;
            ui_lock(mx - 3.5f, 210.5f, col, 1.f);
            if (c->nlock)
                ui_T(ui_itoa(n, c->nlock), mx, 226.f, 7.f, 600, 0.f, col, QD_CENTER);
            continue;
        }
        if (c->chance != v->def_chance)
            ui_O(mx, y, 1.6f, QD_NONE, 1.f, UI_MODE_COL[UI_F_CHNC]), y += 4.5f;
        if (strcmp(c->cond, v->def_cond) != 0)
            ui_O(mx, y, 1.6f, QD_NONE, 1.f, UI_MODE_COL[UI_F_COND]), y += 4.5f;
        if (c->sound)
            ui_O(mx, y, 1.6f, QD_NONE, 1.f, UI_MODE_COL[UI_F_SOUND]), y += 4.5f;
        if (c->nlock)
            ui_O(mx, y, 1.6f, QD_NONE, 1.f, UI_MODE_COL[UI_F_PLOCK]);
    }
}

/* ---------------------------------------------------------------- VEL: bars up from the floor */
static void l_vel(const ui_lane_t *v)
{
    const float floor = 204.f, H = 158.f;
    int i;
    def_line(floor - (float)v->def_vel / 127.f * H);
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float x = col_x(i);
        float h;
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(x + 6.f, floor - 6.f, c);
            continue;
        }
        h = (float)c->vel / 127.f * H;
        if (h < 6.f)
            h = 6.f;
        bar(x + 3.f, floor - h, 6.f, h, ccol(v, c), c->lit);
    }
}

/* ---------------------------------------------------------------- LEN: bars over the steps, a row a step */
static void len_bar(float x, float y, float len, qd_rgb col, int lit, int ghost)
{
    const int inf = !(len < 64.f);
    float w = len * 13.5f - 2.f;
    int cut = 0;
    if (inf || x + w > XEND + 1.f)
        w = XEND + 1.f - x, cut = 1;
    if (w < 4.f)
        w = 4.f;
    if (ghost) {
        qd_save();
        qd_dash(DASH, 2);
        ui_RR(x, y, w, 6.f, 3.f, col, 1.f, QD_NONE);
        qd_restore();
        return;
    }
    bar(x, y, w, 6.f, col, lit);
    if (cut) {                                          /* it goes on past the page: a chevron */
        const float a[6] = {XEND + 3.f, y, XEND + 6.f, y + 3.f, XEND + 3.f, y + 6.f};
        ui_P(a, 3, col, 1.2f, 0, QD_NONE);
    }
}

static void l_len(const ui_lane_t *v)
{
    int i;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float x = col_x(i) + 1.f, y = 43.f + (float)i * 10.2f;
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(x + 5.f, y + 3.f, c);
            continue;
        }
        if (c->ovr)                                     /* the default's length under it, dashed */
            len_bar(x, y, v->def_len, UI_G2, 0, 1);
        len_bar(x, y, c->len, ccol(v, c), c->lit, 0);
    }
}

/* ---------------------------------------------------------------- MICRO: the start flags off the grid */
static void l_micro(const ui_lane_t *v)
{
    const float k = 13.5f / 24.f, top = 72.f, foot = 200.f;
    int i;
    for (i = 0; i < 16; i++) {                          /* the grid, the default's offset dashed */
        const float cx = col_x(i) + 6.f;
        if (!v->col[i].in_range)
            continue;
        line2(cx, foot - 3.f, cx, foot + 3.f, UI_G2, 1.f);
        dashed(cx + (float)v->def_micro * k, top + 6.f, cx + (float)v->def_micro * k, foot,
               v->def_hot ? def_col : UI_G2);
    }
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float cx = col_x(i) + 6.f, sx = cx + (float)c->micro * k;
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(cx, foot - 10.f, c);
            continue;
        }
        line2(sx, foot, sx, top, col, 1.5f);
        {
            const float f[6] = {sx, top, sx + 7.f, top + 4.f, sx, top + 8.f};
            ui_P(f, 3, col, 1.2f, 1, col);
        }
        if (c->micro)                                   /* the offset, joined to the grid at the foot */
            line2(cx, foot, sx, foot, col, 1.5f);
        ui_O(cx, foot, 1.6f, QD_NONE, 1.f, col);
    }
}

/* ---------------------------------------------------------------- RATCH: the hits as a ladder */
static void l_ratch(const ui_lane_t *v)
{
    const float foot = 198.f, sp = 14.f;
    int i, h;
    def_line(foot - (float)((v->def_ratch < 1 ? 1 : v->def_ratch) - 1) * sp - sp / 2.f);
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float cx = col_x(i) + 6.f;
        const int n = c->ratch < 1 ? 1 : c->ratch > 8 ? 8 : c->ratch;
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(cx, foot, c);
            continue;
        }
        for (h = 0; h < n; h++) {
            const float w = 2.f + 8.f * (float)ramp_vel(127, c->ramp, h, n) / 127.f;
            line2(cx - w / 2.f, foot - (float)h * sp, cx + w / 2.f, foot - (float)h * sp, col, 2.f);
        }
    }
}

/* ---------------------------------------------------------------- CHNC: ten dots, lit by chance */
static void l_chnc(const ui_lane_t *v)
{
    const float foot = 196.f, sp = 15.f;
    int i, d;
    def_line(foot - (float)((v->def_chance + 5) / 10) * sp + sp / 2.f);
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float cx = col_x(i) + 6.f;
        const int nl = (c->chance + 5) / 10;
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        for (d = 0; d < 10; d++) {
            const float y = foot - (float)d * sp;
            if (d < nl)
                ui_O(cx, y, 2.4f, QD_NONE, 1.f, col);
            else
                ui_O(cx, y, 2.f, UI_G3, 1.f, QD_NONE);
        }
    }
}

/* ---------------------------------------------------------------- COND: the conditions */
static int parse_ab(const char *s, int *a, int *b)
{
    if (s[0] >= '1' && s[0] <= '9' && s[1] == ':' && s[2] >= '1' && s[2] <= '9' && !s[3]) {
        *a = s[0] - '0', *b = s[2] - '0';
        return 1;
    }
    return 0;
}

static void l_cond(const ui_lane_t *v)
{
    const float foot = 198.f;
    int i, a, b, k;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float cx = col_x(i) + 6.f;
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        if (!c->cond[0]) {                              /* ALWAYS */
            ui_O(cx, foot, 1.8f, QD_NONE, 1.f, col);
        } else if (parse_ab(c->cond, &a, &b)) {         /* A:B: b circles up the column, the a-th filled */
            for (k = 0; k < b; k++)
                ui_O(cx, foot - (float)k * 11.f, 3.2f, k == a - 1 ? QD_NONE : col, 1.f, k == a - 1 ? col : QD_NONE);
            ui_T(c->cond, cx, foot - (float)b * 11.f - 1.f, 8.f, 600, 0.f, col, QD_CENTER);
        } else {                                        /* the name's letters stacked, ending at the foot */
            const int n = (int)strlen(c->cond);
            char ch[2];
            ch[1] = 0;
            for (k = 0; k < n; k++) {
                ch[0] = c->cond[k];
                ui_T(ch, cx, foot + 3.f - (float)(n - 1 - k) * 11.f, 10.f, 600, 0.f, col, QD_CENTER);
            }
        }
    }
}

/* ---------------------------------------------------------------- TRIG: the icons */
static void l_trig(const ui_lane_t *v)
{
    const float y = 123.f;
    int i;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float cx = col_x(i) + 6.f;
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            ui_lock(cx - 5.25f, y - 8.f, col, 1.5f);
        } else if (c->type == 3) {                      /* a slur from the step before */
            const float x0 = cx - 13.5f - (i % 4 == 0 && i ? 2.f : 0.f);
            qd_path_begin();
            qd_move_to(x0, y - 3.f);
            qd_quad_to((x0 + cx) / 2.f, y - 17.f, cx, y - 3.f);
            qd_path_stroke(col, 1.5f);
            ui_O(cx, y, 2.2f, QD_NONE, 1.f, col);
        } else {
            ui_O(cx, y, 4.5f, QD_NONE, 1.f, col);
        }
    }
}

/* ---------------------------------------------------------------- SOUND: a dot a step, the pool number */
static void l_sound(const ui_lane_t *v)
{
    const float y = 112.f;
    char s[12];
    int i;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const float cx = col_x(i) + 6.f;
        const qd_rgb col = c->lit ? UI_K3 : c->sound ? UI_MODE_COL[UI_F_SOUND] : UI_G1;
        if (!c->in_range || !c->on)
            continue;
        if (!c->sound) {
            ui_O(cx, y, 2.4f, col, 1.2f, QD_NONE);
            continue;
        }
        ui_O(cx, y, 3.4f, QD_NONE, 1.f, col);
        ui_itoa(s, c->sound);
        {
            const float w = ui_TW(s, 8.f, 600, 0.f);
            qd_save();
            qd_translate(cx, y + 16.f);
            if (w > 12.f)
                qd_scale(12.f / w, 1.f);
            ui_T(s, 0.f, 0.f, 8.f, 600, 0.f, col, QD_CENTER);
            qd_restore();
        }
    }
}

/* ---------------------------------------------------------------- the lane */
static void lane(const ui_lane_t *v)
{
    const int ph = v->pos >= 0.f ? (int)v->pos : -1;
    char num[12];
    int i;

    for (i = 0; i < 16; i++) {                          /* the keys' outlines */
        const ui_lcol_t *c = &v->col[i];
        if (!c->in_range) {                             /* outside the page's start..end: dark and dotted */
            static const float DOTS[2] = {1.f, 4.f};
            qd_save();
            qd_dash(DOTS, 2);
            ui_RR(col_x(i), TOP, COLW, BOT - TOP, 3.f, UI_G3, 1.f, QD_NONE);
            qd_restore();
            continue;
        }
        ui_RR(col_x(i), TOP, COLW, BOT - TOP, 3.f, c->held ? UI_K3 : i == ph ? UI_G2 : UI_G3, 1.f, QD_NONE);
    }
    switch (v->mode) {
    case UI_F_VEL: l_vel(v); break;
    case UI_F_LEN: l_len(v); break;
    case UI_F_MICRO: l_micro(v); break;
    case UI_F_RATCH: l_ratch(v); break;
    case UI_F_CHNC: l_chnc(v); break;
    case UI_F_COND: l_cond(v); break;
    case UI_F_TRIG: l_trig(v); break;
    case UI_F_SOUND: l_sound(v); break;
    default: l_note(v); break;
    }
    if (ph >= 0 && ph < 16) {                           /* the playhead */
        const float px = col_x(ph) + (v->pos - (float)ph) * 13.5f;
        line2(px, 33.f, px, 213.f, UI_RED, 1.f);
    }
    for (i = 0; i < 16; i += 4)                         /* the page's step numbers */
        ui_T(ui_itoa(num, v->page * 16 + i + 1), col_x(i) + 6.f, 235.f, 8.f, 500, 0.f, UI_G2, QD_CENTER);
}

/* ---------------------------------------------------------------- P1.3f: the STEPS view, an overview */
/* the track's eight pages as eight rows (the page's number at the left), sixteen cells a row in four groups of four: a
 * trig a soft white cell (a little bar of the mode's colour at its foot where it overrides the mode), a LOCK step a
 * ring, an empty step an outline, a step outside its page's start..end a dark speck; the page edited outlined white
 * (its number white), the pages in the loop grey, the others dim; the playhead's cell outlined white as it walks the
 * pages, a trig flashing white as it plays; a held or selected step outlined white */
#define OV_X0 22.f
#define OV_Y0 36.f
#define OV_CW 11.f
#define OV_CH 17.f
static float ov_x(int s) { return OV_X0 + (float)s * 12.5f + (float)(s / 4) * 4.f; }
static float ov_y(int p) { return OV_Y0 + (float)p * 23.5f; }

static void steps_grid(const ui_lane_t *v)
{
    static const char *const NUM[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};
    const qd_rgb mc = UI_MODE_COL[v->mode];
    int p, i;
    for (p = 0; p < 8; p++) {
        const float y = ov_y(p);
        const int in = (v->loop >> p) & 1, ed = p == v->page;
        qd_save();
        qd_alpha(in || ed ? 1.f : 0.38f);
        if (ed)
            ui_RR(OV_X0 - 20.f, y - 3.f, ov_x(15) + OV_CW + 3.f - (OV_X0 - 20.f), OV_CH + 6.f, 5.f, UI_K3, 1.f,
                  QD_NONE);
        ui_T(NUM[p], OV_X0 - 11.f, y + 12.5f, 10.f, ed ? 700 : 500, 0.f, ed ? UI_K3 : in ? UI_G1 : UI_G2, QD_CENTER);
        for (i = 0; i < 16; i++) {
            const uint8_t c = v->cell[p][i];
            const int abs = p * 16 + i;
            const float x = ov_x(i);
            if (!(c & UI_C_RANGE)) {
                ui_O(x + OV_CW / 2.f, y + OV_CH / 2.f, 0.9f, QD_NONE, 1.f, UI_G3);
                continue;
            }
            if ((c & UI_C_ON) && !(c & UI_C_LOCK)) {
                ui_RR(x, y, OV_CW, OV_CH, 2.5f, QD_NONE, 1.f, (c & UI_C_HELD) ? 0x9a9aa2u : 0xd6d6dcu);
                if (c & UI_C_OVR)
                    ui_RR(x + 2.f, y + OV_CH - 4.f, OV_CW - 4.f, 2.5f, 1.2f, QD_NONE, 1.f, mc);
                if (c & UI_C_TIE)                       /* a TIE: a notch on its left edge */
                    ui_RR(x - 1.f, y + OV_CH / 2.f - 2.f, 3.f, 4.f, 1.f, QD_NONE, 1.f, UI_BG);
                if (abs == v->flash_step && v->flash_age >= 0.f && v->flash_age < 0.25f) {
                    qd_save();
                    qd_alpha(1.f - v->flash_age / 0.25f);
                    ui_RR(x, y, OV_CW, OV_CH, 2.5f, QD_NONE, 1.f, UI_K3);
                    qd_restore();
                }
            } else {
                ui_RR(x, y, OV_CW, OV_CH, 2.5f, UI_G3, 1.f, QD_NONE);
                if (c & UI_C_LOCK)
                    ui_O(x + OV_CW / 2.f, y + OV_CH / 2.f, 3.f, (c & UI_C_OVR) ? mc : UI_G1, 1.4f, QD_NONE);
            }
            if (abs == v->play_step)
                ui_RR(x - 1.5f, y - 1.5f, OV_CW + 3.f, OV_CH + 3.f, 3.5f, UI_RED, 1.5f, QD_NONE);
            else if (c & UI_C_HELD)
                ui_RR(x - 1.5f, y - 1.5f, OV_CW + 3.f, OV_CH + 3.f, 3.5f, UI_K3, 1.2f, QD_NONE);
        }
        qd_restore();
    }
    {   /* the foot: the page edited and its range */
        char s[32], n[12];
        const int pg = v->page < 0 ? 0 : v->page > 7 ? 7 : v->page;
        s[0] = 0;
        strcat(s, "PAGE ");
        strcat(s, ui_itoa(n, pg + 1));
        if (v->pstart[pg] > 0 || v->pend[pg] < 15) {
            strcat(s, " \xc2\xb7 ");
            strcat(s, ui_itoa(n, v->pstart[pg] + 1));
            strcat(s, "\xe2\x80\x93");
            strcat(s, ui_itoa(n, v->pend[pg] + 1));
        }
        ui_lab(s, 10.f, 236.f, UI_G1, QD_LEFT);
        ui_lab("STEPS", 230.f, 236.f, UI_G2, QD_RIGHT);
    }
}

/* the default named at the foot (not NOTE: its chord is on the roll), a dashed swatch before it for the modes that
 * draw it dashed */
static void def_name(const ui_lane_t *v)
{
    const int sw = v->mode == UI_F_VEL || v->mode == UI_F_LEN || v->mode == UI_F_MICRO || v->mode == UI_F_RATCH ||
                   v->mode == UI_F_CHNC;
    float x = col_x(0);
    if (v->mode == UI_F_NOTE || v->mode == UI_F_PLOCK || !v->def_label[0])
        return;
    if (sw) {
        dashed(x, 219.f, x + 10.f, 219.f, def_col);
        x += 15.f;
    }
    ui_lab(v->def_label, x, 222.f, def_col, QD_LEFT);
}

void ui_lane_draw(const ui_lane_t *v)
{
    const int mode = v->mode >= 0 && v->mode < UI_NFIELD ? v->mode : UI_F_NOTE;
    ui_lane_t m;
    char bpm[12];
    float mx;
    int p;

    m = *v;                                             /* the mode, made safe */
    m.mode = mode;
    v = &m;
    def_col = v->def_hot ? UI_MODE_COL[mode] : UI_G1;

    ui_head(v->name, v->pattern, v->track, UI_K3, UI_G1);
    mx = 29.f + ui_TW(v->name, 11.f, 500, 1.f) + 6.f;    /* the mode, small in its colour after the name */
    ui_lab(UI_FIELD_NAME[mode], mx, 16.5f, UI_MODE_COL[mode], QD_LEFT);
    mx += ui_TW(UI_FIELD_NAME[mode], 8.f, 600, 1.f);
    ui_T(ui_itoa(bpm, v->bpm), 196.f, 17.f, 11.f, 400, 0.f, UI_G2, QD_RIGHT);

    for (p = 0; p < 8 && !v->steps; p++) {              /* the pages: each over its start..end (STEPS: its rows) */
        const float x = 10.f + (float)p * 28.f, W = 22.f;
        const int s = v->pstart[p] > 15 ? 15 : v->pstart[p], e = v->pend[p] < s ? s : v->pend[p] > 15 ? 15 : v->pend[p];
        const float xs = x + (float)s / 16.f * W, xe = x + (float)(e + 1) / 16.f * W;
        const int in = (v->loop >> p) & 1, ed = p == v->page;
        if (s > 0 || e < 15)
            line2(x, 27.f, x + W, 27.f, 0x1c1c22u, 1.f);
        line2(xs, 27.f, xe, 27.f, ed ? UI_K3 : in ? UI_G1 : UI_G3, ed ? 2.f : 1.5f);
        if (p == v->play_page) {                        /* where the playhead is */
            if (ed && v->pos >= 0.f) {
                const float px = x + v->pos / 16.f * W;
                line2(px, 24.f, px, 30.f, UI_RED, 1.5f);
            } else {
                const float c = (xs + xe) / 2.f;
                line2(c - 3.f, 27.f, c + 3.f, 27.f, UI_RED, 2.f);
            }
        }
    }

    if (v->slide != 0.f && !v->steps) {                 /* a page change: the lane slides in (STEPS: the overview
                                                         * stays, its outline moves) */
        const float a = 1.f - fabsf(v->slide);
        qd_save();
        qd_alpha(a < 0.f ? 0.f : a);
        qd_translate(v->slide * 40.f, 0.f);
        lane(v);
        qd_restore();
    } else if (v->steps) {
        steps_grid(v);
    } else {
        lane(v);
    }
    if (!v->steps)
        def_name(v);
    if (v->latch) {                                     /* P1.3d: the step latch, after the mode */
        ui_lab("LATCH", mx + 6.f, 16.5f, UI_K3, QD_LEFT);
        mx += 6.f + ui_TW("LATCH", 8.f, 600, 1.f);
    }
    if (!v->follow) {                                   /* in the top line's gap, left of the tempo */
        const float r = 196.f - ui_TW(bpm, 11.f, 400, 0.f) - 8.f;
        if (r - ui_TW("FOLLOW OFF", 8.f, 600, 1.f) > mx + 4.f)
            ui_lab("FOLLOW OFF", r, 16.f, UI_G2, QD_RIGHT);
        else
            ui_lab("NO FOLLOW", r, 16.f, UI_G2, QD_RIGHT);
    }
}
