/* SPDX-License-Identifier: GPL-3.0-only */
/* The sound editor's screen (P1.4a; ui.h ui_snd_t). Built through ui_layer.c (it includes this file: its line2 and
 * cat; the core's source lists stay as they are). PLAN §9a's clarity rules: the page's name, the values and their short
 * names are the only text; the rest is drawing, colour and motion. */
#include <stdlib.h>
#include <string.h>

static const qd_rgb SNDK[5] = {UI_K1, UI_K2, UI_K3, UI_K4, UI_KALG};   /* KNOB 1..4, ALGORITHM */
#define SD_TOP 32.f                                     /* the drawing's area */
#define SD_BOT 180.f                                    /* (P1.5j: the foot's labels from 184) */
/* P1.5j: the foot, four fixed columns inside the frame's 4 px margin (the frame 3 px: its inner edge at 3) */
#define SD_FOOT_X 7.f
#define SD_COL_W 56.5f
#define SD_LAB_Y 193.f                                  /* the labels' baseline (12 px) */
#define SD_CHIP_Y 200.f                                 /* the chips: every column's the same */
#define SD_CHIP_W 52.5f
#define SD_CHIP_H 32.f                                  /* (RATIO B's two lines; its foot at 232: 4 px clear) */

static float sd_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

static void sd_poly(const float *p, int n, qd_rgb col, float lw, int dashed)
{
    static const float DASH[2] = {3.f, 3.f};
    if (dashed)
        qd_dash(DASH, 2);
    ui_P(p, n, col, lw, 0, QD_NONE);
    if (dashed)
        qd_dash(0, 0);
}

static void sd_arrow(float x0, float y0, float x1, float y1, qd_rgb col)
{
    const float an = atan2f(y1 - y0, x1 - x0);
    const float h[6] = {x1 - cosf(an - 0.5f) * 5.f, y1 - sinf(an - 0.5f) * 5.f, x1, y1,
                        x1 - cosf(an + 0.5f) * 5.f, y1 - sinf(an + 0.5f) * 5.f};
    line2(x0, y0, x1, y1, col, 1.2f);
    ui_P(h, 3, col, 1.2f, 0, QD_NONE);
}

/* a wave of kind w (the LFO's: TRI SINE SQR SAW RAMP EXP RAND) at phase u (cycles), -1..1 */
static float sd_wave(int w, float u)
{
    float f = u - floorf(u);
    switch (w) {
    case 0: return f < 0.5f ? 4.f * f - 1.f : 3.f - 4.f * f;
    case 1: return sinf(f * UI_TAU);
    case 2: return f < 0.5f ? 1.f : -1.f;
    case 3: return 1.f - 2.f * f;
    case 4: return 2.f * f - 1.f;
    case 5: return 2.f * expf(-4.f * f) - 1.f;
    default: {                                          /* RAND: a step a cycle, fixed values */
        const int k = (int)floorf(u);
        const unsigned h = (unsigned)k * 2654435761u;
        return (float)((h >> 8) & 1023u) / 511.5f - 1.f;
    }
    }
}

/* ---------------------------------------------------------------- the pictures */
/* P1.5j: a colour faded by a share (0..1): towards its grey and darker, never under SD_FADE_FLOOR */
#define SD_FADE_FLOOR 0.3f
static qd_rgb sd_fade(qd_rgb c, float share)
{
    const float r = (float)((c >> 16) & 255u), g = (float)((c >> 8) & 255u), b = (float)(c & 255u);
    const float l = 0.3f * r + 0.59f * g + 0.11f * b, sat = share, br = SD_FADE_FLOOR + (1.f - SD_FADE_FLOOR) * share;
    const float rr = (l + (r - l) * sat) * br, gg = (l + (g - l) * sat) * br, bb = (l + (b - l) * sat) * br;
    return (qd_rgb)((uint32_t)(rr > 255.f ? 255.f : rr) << 16 | (uint32_t)(gg > 255.f ? 255.f : gg) << 8 |
                    (uint32_t)(bb > 255.f ? 255.f : bb));
}

/* P1.5j / P1.5n: the mix's own row (RATIO's and TONE's, the same place): centred, X's end and Y's in their first
 * carriers' colours (faded by their share), a tick at MIX 0, the handle in KNOB 4's orange */
static void sd_mixrow(const ui_snd_t *v)
{
    const float sx = v->has_mix ? (63.f - (float)v->mix) / 127.f : 1.f, sy = 1.f - sx;
    int o;
    if (v->has_mix) {
        const float x0 = 30.f, x1 = 210.f, yy = 170.f, hx = x0 + (x1 - x0) * ((float)v->mix + 64.f) / 127.f;
        int cx = -1, cy = -1;
        for (o = 0; o < 4; o++) {
            if (cx < 0 && ((v->outx >> o) & 1u))
                cx = o;
            if (cy < 0 && ((v->outy >> o) & 1u))
                cy = o;
        }
        line2(x0, yy, x1, yy, UI_G2, 2.f);
        line2((x0 + x1) * 0.5f, yy - 4.f, (x0 + x1) * 0.5f, yy + 4.f, UI_G1, 1.2f);   /* MIX 0 */
        ui_O(x0 - 9.f, yy, 4.5f, cx >= 0 ? sd_fade(SNDK[cx], sx) : UI_G1, 1.6f, UI_BG);   /* X */
        ui_O(x1 + 9.f, yy, 4.5f, cy >= 0 ? sd_fade(SNDK[cy], sy) : UI_G1, 1.6f, UI_BG);   /* Y */
        ui_RR(hx - 4.f, yy - 7.f, 8.f, 14.f, 2.5f, QD_NONE, 1.f, SNDK[3]);
    }
}

/* P1.6d (the owner: on TONE "something is missing" lower right): MIX as a glyph in KNOB 4's square, as HARM, DTUNE
 * and FDBK have theirs: X's and Y's outputs as two bars as tall as their shares (their carriers' colours), a short
 * slider under them with KNOB 4's handle, X and Y its ends */
static void sd_mixcell(const ui_snd_t *v)
{
    const float sx = v->has_mix ? (63.f - (float)v->mix) / 127.f : 1.f, sy = 1.f - sx;
    const float x0 = 148.f, x1 = 212.f, yy = 150.f, hx = x0 + (x1 - x0) * ((float)v->mix + 64.f) / 127.f;
    int o, cx = -1, cy = -1;
    if (!v->has_mix)
        return;
    for (o = 0; o < 4; o++) {
        if (cx < 0 && ((v->outx >> o) & 1u))
            cx = o;
        if (cy < 0 && ((v->outy >> o) & 1u))
            cy = o;
    }
    {
        const qd_rgb xc = cx >= 0 ? SNDK[cx] : UI_G1, yc = cy >= 0 ? SNDK[cy] : UI_G1;
        const float hmax = 30.f, base = 136.f;
        ui_RR(160.f, base - 4.f - hmax * sx, 12.f, 4.f + hmax * sx, 3.f, QD_NONE, 1.f, sd_fade(xc, 0.35f + 0.65f * sx));
        ui_RR(188.f, base - 4.f - hmax * sy, 12.f, 4.f + hmax * sy, 3.f, QD_NONE, 1.f, sd_fade(yc, 0.35f + 0.65f * sy));
        line2(152.f, base + 1.f, 208.f, base + 1.f, UI_G3, 1.f);
        line2(x0, yy, x1, yy, UI_G2, 2.f);
        line2((x0 + x1) * 0.5f, yy - 4.f, (x0 + x1) * 0.5f, yy + 4.f, UI_G1, 1.2f);   /* MIX 0 */
        ui_O(x0 - 8.f, yy, 4.f, xc, 1.5f, UI_BG);       /* X */
        ui_O(x1 + 8.f, yy, 4.f, yc, 1.5f, UI_BG);       /* Y */
        ui_RR(hx - 4.f, yy - 7.f, 8.f, 14.f, 2.5f, QD_NONE, 1.f, SNDK[3]);
    }
}

/* the operators wired by the algorithm. P1.5j (the owner): shorter, the bus and the speaker higher (SD_BUS_Y), a deep
 * chain zig-zagging; with MIX (the RATIO page): each operator faded by its output's share (an operator only in X's chain
 * X's, only in Y's Y's, in both full), the wires to the bus as thick as their share, and the mix's own row: a slider
 * centred under the drawing, X's end and Y's marked by their carriers' colours, the handle in KNOB 4's orange */
#define SD_BUS_Y 150.f
#define SD_OPR 14.f
static void sd_algo(const ui_snd_t *v)
{
    int dep[4] = {0, 0, 0, 0}, row[4], n[4] = {0, 0, 0, 0}, o, t, it, md = 0;
    unsigned reach[4];
    float x[4], y[4], sp, sh[4];
    const unsigned out = (unsigned)(v->outx | v->outy);
    const float sx = v->has_mix ? (63.f - (float)v->mix) / 127.f : 1.f, sy = 1.f - sx;
    for (it = 0; it < 4; it++)                          /* a modulator sits above what it modulates */
        for (t = 0; t < 4; t++)
            for (o = 0; o < 4; o++)
                if (o != t && ((v->mod[t] >> o) & 1u) && dep[o] < dep[t] + 1)
                    dep[o] = dep[t] + 1;
    for (o = 0; o < 4; o++) {                           /* which outputs each operator reaches (bit 0 X, 1 Y) */
        reach[o] = ((v->outx >> o) & 1u) | (((v->outy >> o) & 1u) << 1);
    }
    for (it = 0; it < 4; it++)
        for (t = 0; t < 4; t++)
            for (o = 0; o < 4; o++)
                if (o != t && ((v->mod[t] >> o) & 1u))
                    reach[o] |= reach[t];
    for (o = 0; o < 4; o++)
        sh[o] = !v->has_mix || reach[o] == 3u || !reach[o] ? 1.f : reach[o] == 1u ? sx : sy;
    for (o = 0; o < 4; o++) {
        row[o] = n[dep[o] > 3 ? 3 : dep[o]]++;
        md = dep[o] > md ? dep[o] : md;
    }
    sp = md >= 3 ? 27.f : md == 2 ? 34.f : 40.f;
    for (o = 0; o < 4; o++) {
        const int d = dep[o] > 3 ? 3 : dep[o], cnt = n[d];
        x[o] = 112.f + ((float)row[o] - (float)(cnt - 1) * 0.5f) * 60.f + (md >= 3 && cnt == 1 ? (d % 2 ? 20.f : -20.f) : 0.f);
        y[o] = SD_BUS_Y - 26.f - (float)d * sp;
    }
    line2(18.f, SD_BUS_Y, 196.f, SD_BUS_Y, UI_G2, 1.2f);   /* the bus, the speaker */
    {
        const float by = SD_BUS_Y;
        const float spk[8] = {200.f, by - 6.f, 206.f, by - 6.f, 206.f, by + 6.f, 200.f, by + 6.f};
        const float co[8] = {206.f, by - 6.f, 214.f, by - 13.f, 214.f, by + 13.f, 206.f, by + 6.f};
        const float pulse = v->t * 2.f - floorf(v->t * 2.f);
        ui_P(spk, 4, UI_G1, 1.2f, 1, QD_NONE);
        ui_P(co, 4, UI_G1, 1.2f, 1, QD_NONE);
        qd_save();
        qd_alpha(1.f - pulse);
        qd_arc(214.f, by, 8.f + pulse * 8.f, -0.7f, 0.7f, UI_G1, 1.2f);
        qd_restore();
    }
    for (o = 0; o < 4; o++)
        if ((out >> o) & 1u) {                          /* a carrier into the bus (from above: down its right side) */
            const float ox = dep[o] ? x[o] + SD_OPR + 4.f : x[o];
            /* P1.5i: MIX: an output's wires as thick as its share (X: -64, Y: +63); P1.5j: faded with it */
            const float s1 = v->has_mix ? (((v->outx >> o) & 1u) ? sx : sy) : 1.f;
            const float lw = v->has_mix ? 1.2f + 2.f * s1 : 1.2f;
            const qd_rgb wc = v->has_mix ? sd_fade(UI_K3, s1) : UI_G1;
            if (dep[o])
                line2(x[o] + SD_OPR, y[o], ox, y[o], wc, lw);
            line2(ox, dep[o] ? y[o] : y[o] + SD_OPR, ox, SD_BUS_Y, wc, lw);
            ui_O(ox, SD_BUS_Y, 2.f + (v->has_mix ? s1 : 0.f), QD_NONE, 1.f, wc);
        }
    for (t = 0; t < 4; t++)                             /* the modulations: arrows, a dot running along each */
        for (o = 0; o < 4; o++)
            if (o != t && ((v->mod[t] >> o) & 1u)) {
                const float dx = x[t] - x[o], dy = y[t] - y[o], l = sqrtf(dx * dx + dy * dy);
                const float ux = dx / l, uy = dy / l, u = v->t * 1.2f + (float)(o + t) * 0.27f;
                const float f = u - floorf(u), r0 = SD_OPR + 1.f, r1 = SD_OPR + 2.f;
                sd_arrow(x[o] + ux * r0, y[o] + uy * r0, x[t] - ux * r1, y[t] - uy * r1, sd_fade(UI_G1, sh[o]));
                ui_O(x[o] + ux * r0 + (l - r0 - r1) * f * ux, y[o] + uy * r0 + (l - r0 - r1) * f * uy, 1.8f, QD_NONE, 1.f,
                     sd_fade(UI_K3, sh[o]));
            }
    if (v->fb < 4) {                                    /* the feedback: a loop over its operator */
        const int f = v->fb;
        qd_arc(x[f] + SD_OPR - 1.f, y[f] - SD_OPR + 3.f, 7.f, UI_PI * 0.75f, UI_PI * 2.25f, sd_fade(UI_G1, sh[f]), 1.4f);
    }
    for (o = 0; o < 4; o++) {                           /* the operators: their waves at their ratios */
        const float r = sd_clampf(v->ratio[o], 0.25f, 4.f);
        const qd_rgb oc = sd_fade(SNDK[o], sh[o]);
        float p[2 * 25];
        int i;
        ui_O(x[o], y[o], SD_OPR, oc, 1.6f, UI_BG);
        for (i = 0; i < 25; i++) {
            const float u = (float)i / 24.f;
            p[2 * i] = x[o] - 9.f + u * 18.f;
            p[2 * i + 1] = y[o] - sinf((u * r * 1.5f - v->t * r) * UI_TAU) * 6.f;
        }
        ui_P(p, 25, oc, 1.2f, 0, QD_NONE);
    }
    sd_mixrow(v);

}

static void sd_tone(const ui_snd_t *v)
{
    const float harm = (float)v->v[0] / 100.f, dt = (float)v->v[1] / 100.f;
    const int fb = v->v[2];
    float p[2 * 41];
    int i;
    /* HARM: the partials (the bright side up the series, the dark down) */
    for (i = 1; i <= 8; i++) {
        const float b = (harm + 26.f) / 52.f, a = powf(1.f / (float)i, 2.4f - 2.2f * b);
        const float x = 18.f + (float)i * 11.f;
        line2(x, 100.f, x, 100.f - a * 58.f, SNDK[0], 2.f);
    }
    /* DTUNE: two waves drifting apart */
    for (i = 0; i <= 40; i++) {
        const float u = (float)i / 40.f;
        p[2 * i] = 132.f + u * 96.f;
        p[2 * i + 1] = 66.f - sinf((u * 3.f + v->t * 0.5f) * UI_TAU) * 18.f;
    }
    ui_P(p, 41, SNDK[1], 1.2f, 0, QD_NONE);
    for (i = 0; i <= 40; i++) {
        const float u = (float)i / 40.f;
        p[2 * i + 1] = 66.f - sinf((u * 3.f * (1.f + dt / 250.f) + v->t * 0.5f + dt / 60.f) * UI_TAU) * 18.f;
    }
    ui_P(p, 41, SNDK[1], 1.2f, 0, QD_NONE);
    /* FDBK: the loop, its wave from sine to saw to noise */
    for (i = 0; i <= 40; i++) {
        const float u = (float)i / 40.f, k = (float)fb / 127.f;
        float s = sinf(u * 2.f * UI_TAU), w = (1.f - k) * s + k * (1.f - 2.f * (u * 2.f - floorf(u * 2.f)));
        if (fb > 90)
            w += ((float)(((unsigned)i * 2654435761u >> 9) & 255u) / 128.f - 1.f) * (float)(fb - 90) / 80.f;
        p[2 * i] = 20.f + u * 96.f;
        p[2 * i + 1] = 134.f - sd_clampf(w, -1.2f, 1.2f) * 14.f;   /* (P1.5n: higher, the mix row under it) */
    }
    ui_P(p, 41, SNDK[2], 1.2f, 0, QD_NONE);
    qd_arc(68.f, 134.f, 28.f, UI_PI * 1.15f, UI_PI * 1.85f, UI_G2, 1.f);
    sd_mixcell(v);                                      /* P1.6d: MIX in its own square, lower right (KNOB 4's) */
}

static void sd_ofs(const ui_snd_t *v)
{
    int k;
    for (k = 0; k < 4; k++) {
        const float cx = 32.f + (float)k * 58.f, cy = 104.f, a = -UI_PI * 0.5f + (float)v->v[k] / 100.f * UI_PI * 0.75f;
        ui_O(cx, cy, 22.f, SNDK[k], 1.5f, QD_NONE);
        line2(cx, cy - 26.f, cx, cy - 22.f, UI_G1, 1.2f);
        line2(cx, cy, cx + cosf(a) * 18.f, cy + sinf(a) * 18.f, SNDK[k], 2.f);
        ui_O(cx, cy, 2.5f, QD_NONE, 1.f, SNDK[k]);
    }
}

/* an envelope's points: delay, attack to the top, decay to the level, held (or released) */
static void sd_env_pts(int dly, int atk, int dec, int lev, float *p, float base, float top)
{
    const float x0 = 16.f + (float)dly * 0.3f, xa = x0 + 4.f + (float)atk * 0.45f, xd = xa + 4.f + (float)dec * 0.5f;
    const float yl = base - (base - top) * (float)lev / 127.f;
    p[0] = 16.f, p[1] = base;
    p[2] = x0, p[3] = base;
    p[4] = xa, p[5] = top;
    p[6] = xd, p[7] = yl;
    p[8] = 206.f, p[9] = yl;
}

static void sd_run_dot(const float *p, int n, float u)   /* a red dot along the curve (it plays) */
{
    float tot = 0.f, d;
    int k;
    for (k = 0; k + 1 < n; k++)
        tot += sqrtf((p[2 * k + 2] - p[2 * k]) * (p[2 * k + 2] - p[2 * k]) +
                     (p[2 * k + 3] - p[2 * k + 1]) * (p[2 * k + 3] - p[2 * k + 1]));
    d = (u - floorf(u)) * tot;
    for (k = 0; k + 1 < n; k++) {
        const float l = sqrtf((p[2 * k + 2] - p[2 * k]) * (p[2 * k + 2] - p[2 * k]) +
                              (p[2 * k + 3] - p[2 * k + 1]) * (p[2 * k + 3] - p[2 * k + 1]));
        if (d <= l && l > 0.f) {
            ui_O(p[2 * k] + (p[2 * k + 2] - p[2 * k]) * d / l, p[2 * k + 1] + (p[2 * k + 3] - p[2 * k + 1]) * d / l,
                 2.2f, QD_NONE, 1.f, UI_RED);
            return;
        }
        d -= l;
    }
}

static void sd_openv(const ui_snd_t *v)
{
    const float base = 170.f, top = 44.f;
    float p[10], q[10];
    int any = 0, k;
    for (k = 0; k < 5; k++)
        any |= v->locked[k];
    line2(16.f, base, 206.f, base, UI_G3, 1.f);
    if (v->step && any) {                               /* the track's envelope left behind */
        sd_env_pts(v->tv[4], v->tv[0], v->tv[1], v->tv[2], q, base, top);
        sd_poly(q, 5, UI_G2, 1.2f, 1);
    }
    sd_env_pts(v->v[4], v->v[0], v->v[1], v->v[2], p, base, top);
    sd_poly(p, 5, UI_K3, 1.6f, 0);
    if (v->v[4])
        ui_O(p[2], p[3], 3.4f, QD_NONE, 1.f, SNDK[4]);
    ui_O(p[4], p[5], 3.4f, QD_NONE, 1.f, SNDK[0]);
    ui_O(p[6], p[7], 3.4f, QD_NONE, 1.f, SNDK[1]);
    ui_O((p[6] + p[8]) * 0.5f, p[9], 3.4f, QD_NONE, 1.f, SNDK[2]);
    ui_RR(216.f, top, 12.f, base - top, 3.f, UI_G2, 1.2f, QD_NONE);   /* LEV */
    ui_RR(216.f, base - (base - top) * (float)v->v[3] / 127.f, 12.f, (base - top) * (float)v->v[3] / 127.f, 3.f,
          QD_NONE, 1.f, SNDK[3]);
    sd_run_dot(p, 5, v->t * 0.5f);
}

static void sd_switch(float x, float y, int on, qd_rgb col, int icon)
{
    ui_RR(x, y - 11.f, 44.f, 22.f, 11.f, on ? col : UI_G2, 1.5f, QD_NONE);
    ui_O(on ? x + 33.f : x + 11.f, y, 7.f, QD_NONE, 1.f, on ? col : UI_G2);
    ui_icon(icon, x - 16.f, y, 1.2f, on ? col : UI_G2);
}

static void sd_trig(const ui_snd_t *v)
{
    static const uint8_t MASK[5] = {0, 0xF, 0x1, 0xE, 0xA};   /* eng_quad.c QUAD_PHRT_MASK: C A B1 B2 */
    static const char *const OP[4] = {"C", "A", "B1", "B2"};
    const unsigned m = v->v[4] >= 0 && v->v[4] < 5 ? MASK[v->v[4]] : 0;
    int k;
    ui_T("A", 22.f, 74.f, 16.f, 600, 0.f, UI_G1, QD_CENTER);
    ui_T("B", 22.f, 124.f, 16.f, 600, 0.f, UI_G1, QD_CENTER);
    sd_switch(68.f, 68.f, v->v[0], SNDK[0], UI_IC_PLAY);
    sd_switch(160.f, 68.f, v->v[1], SNDK[1], UI_IC_RESET);
    sd_switch(68.f, 118.f, v->v[2], SNDK[2], UI_IC_PLAY);
    sd_switch(160.f, 118.f, v->v[3], SNDK[3], UI_IC_RESET);
    for (k = 0; k < 4; k++) {                           /* PHRT: the operators it restarts */
        const float x = 66.f + (float)k * 36.f;
        const int on = (int)((m >> k) & 1u);
        ui_O(x, 162.f, 12.f, on ? SNDK[4] : UI_G2, 1.5f, on ? SNDK[4] : QD_NONE);
        ui_T(OP[k], x, 166.5f, 12.f, 600, 0.f, on ? UI_BG : UI_G2, QD_CENTER);
    }
}

static void sd_keys(const ui_snd_t *v)
{
    int k;
    ui_keyboard(16.f, 150.f, 176.f, 30.f, 36, 0);
    for (k = 0; k < 3; k++) {                           /* the key scaling: the level falling up the keys */
        const float y0 = 52.f + (float)k * 22.f, y1 = y0 + (float)v->v[k] * 0.5f;
        line2(16.f, y0, 192.f, y1 > 144.f ? 144.f : y1, SNDK[k], 2.f);
    }
    {   /* VEL: a wedge as tall as its depth */
        const float h = (float)v->v[3] / 127.f * 120.f;
        const float w[6] = {204.f, 176.f, 228.f, 176.f, 228.f, 176.f - h};
        ui_P(w, 3, SNDK[3], 1.5f, 1, QD_NONE);
    }
}

/* the filter's response (E2: by its machine, pv[0] its FTYPE, pv[1] FSHAPE, pv[2] FGAIN): LEGACY's LP / HP, LOWPASS 4's
 * steeper skirt, MULTI's LP -> BP -> HP, the combs' teeth at FREQ's harmonics (COMB-: the odd ones) as tall as FDBK and
 * bowed down by LPF, EQ's bell at FREQ as wide as Q and as tall as GAIN (a dip below 0 dB) */
static void sd_filter(const ui_snd_t *v)
{
    const float fc = 16.f + (float)v->v[0] / 12700.f * 200.f;
    const int ft = v->pv[0], mach = ft <= 2 ? 0 : ft - 2;
    const float res = (float)(v->has[1] && mach != 5 ? v->v[1] : 0) / 12700.f, sh = (float)v->pv[1] / 127.f;
    const int ty = mach == 0 ? ft : mach == 1 ? 3 : mach == 2 ? 4 : 0;
    float p[2 * 81];
    int i;
    line2(16.f, 170.f, 224.f, 170.f, UI_G3, 1.f);
    if (mach == 5) {                                    /* EQ: 0 dB across the middle, the bell up or down */
        const float gn = (float)v->pv[2] / 64.f, wd = 6.f + (1.f - sh) * 44.f;
        line2(16.f, 112.f, 224.f, 112.f, UI_G3, 1.f);
        for (i = 0; i <= 80; i++) {
            const float x = 16.f + (float)i / 80.f * 208.f, d = (x - fc) / wd;
            p[2 * i] = x;
            p[2 * i + 1] = 112.f - gn * 64.f * expf(-d * d);
        }
        ui_P(p, 81, SNDK[2], 1.6f, 0, QD_NONE);
        ui_O(fc, 112.f - gn * 64.f, 3.4f, QD_NONE, 1.f, SNDK[1]);
        line2(fc, 170.f, fc, 176.f, SNDK[0], 2.f);
        ui_O(fc, 176.f, 3.4f, QD_NONE, 1.f, SNDK[0]);
        return;
    }
    if (mach == 3 || mach == 4) {                       /* COMB+ / COMB-: the teeth */
        const float sp = 14.f + (fc - 16.f) * 0.22f, amp = 20.f + res * 100.f, damp = 0.55f + 0.45f * sh;
        for (i = 0; i <= 80; i++) {
            const float x = 16.f + (float)i / 80.f * 208.f;
            float g = 0.f, h = 1.f;
            int k;
            for (k = 1; k < 40; k++) {
                const float xk = 16.f + (mach == 4 ? ((float)k - 0.5f) : (float)k) * sp, d = (x - xk) / (2.f + 3.f * (1.f - res));
                if (xk > 230.f)
                    break;
                g = fmaxf(g, h * expf(-d * d));
                h *= damp;
            }
            p[2 * i] = x;
            p[2 * i + 1] = 166.f - g * amp;
        }
        ui_P(p, 81, SNDK[2], 1.4f, 0, QD_NONE);
        if (mach == 4) {                                /* (COMB-: the even places empty, a dim stub each) */
            int k;
            for (k = 1; 16.f + (float)k * sp < 222.f; k++)
                line2(16.f + (float)k * sp, 166.f, 16.f + (float)k * sp, 156.f, UI_G2, 1.4f);
        }
        ui_O(16.f + (mach == 4 ? 0.5f : 1.f) * sp, 166.f - amp, 3.4f, QD_NONE, 1.f, SNDK[1]);
        line2(fc, 170.f, fc, 176.f, SNDK[0], 2.f);
        ui_O(fc, 176.f, 3.4f, QD_NONE, 1.f, SNDK[0]);
        return;
    }
    for (i = 0; i <= 80; i++) {                         /* LEGACY, LOWPASS 4, MULTI: as ever, MULTI morphing */
        const float x = 16.f + (float)i / 80.f * 208.f, d = (x - fc) / 40.f;
        const float lp = d <= 0.f ? 1.f : 1.f / (1.f + d * d * 2.f), hp = d >= 0.f ? 1.f : 1.f / (1.f + d * d * 2.f);
        const float bp = 1.f / (1.f + d * d * 3.f);
        float g = 1.f, pk = res * 50.f * expf(-d * d * 6.f);
        if (ty == 1)
            g = lp;
        else if (ty == 2)
            g = hp;
        else if (ty == 3)
            g = d <= 0.f ? 1.f : 1.f / (1.f + d * d * d * d * 6.f);
        else if (ty == 4)
            g = sh <= 0.5f ? lp + (bp - lp) * sh * 2.f : bp + (hp - bp) * (sh - 0.5f) * 2.f;
        if (!ty)
            pk = 0.f;
        p[2 * i] = x;
        p[2 * i + 1] = 170.f - sd_clampf(g * 90.f + pk, 0.f, 128.f);
    }
    ui_P(p, 81, SNDK[2], 1.6f, 0, QD_NONE);
    if (ty) {
        ui_O(fc, 170.f - sd_clampf(90.f * (ty == 4 && sh > 0.f && sh < 1.f ? 1.f : 1.f) + res * 50.f, 0.f, 128.f), 3.4f,
             QD_NONE, 1.f, SNDK[1]);
        line2(fc, 170.f, fc, 176.f, SNDK[0], 2.f);
        ui_O(fc, 176.f, 3.4f, QD_NONE, 1.f, SNDK[0]);
        if (v->v[3]) {                                  /* DEPTH: the envelope moves the cutoff */
            const float l = (float)v->v[3] / 6400.f * 60.f;
            sd_arrow(fc + 10.f, 110.f, fc + 10.f + l, 110.f, SNDK[3]);
        }
    }
}

/* E1: AMP in AHD: attack, HOLD a flat top (NOTE: over a note bar, to its end), the decay to 0 (INF: held) */
static void sd_ahd(const ui_snd_t *v)
{
    const float base = 170.f, top = 44.f;
    const int note = v->v[1] >= 127, inf = v->v[2] >= 127;
    const float xa = 19.f + (float)v->v[0] * 0.35f;
    const float xh = note ? xa + 70.f : xa + 3.f + (float)v->v[1] * 0.35f;
    const float xd = inf ? 222.f : sd_clampf(xh + 3.f + (float)v->v[2] * 0.4f, 0.f, 222.f);
    const float p[8] = {16.f, base, xa, top, xh, top, xd, inf ? top : base};
    line2(16.f, base, 222.f, base, UI_G3, 1.f);
    if (note)                                           /* NOTE: the key's bar, the top ends where it ends */
        ui_RR(16.f, base + 3.f, xh - 16.f, 7.f, 3.5f, QD_NONE, 1.f, UI_G2);
    sd_poly(p, 4, UI_K3, 1.6f, 0);
    if (inf) {
        const float a[6] = {xd - 5.f, top - 4.f, xd, top, xd - 5.f, top + 4.f};
        ui_P(a, 3, UI_K3, 1.4f, 0, QD_NONE);
    }
    ui_O(xa, top, 3.4f, QD_NONE, 1.f, SNDK[0]);
    ui_O((xa + xh) * 0.5f, top, 3.4f, QD_NONE, 1.f, SNDK[1]);
    ui_O(inf ? xd - 8.f : (xh + xd) * 0.5f, inf ? top : (top + base) * 0.5f, 3.4f, QD_NONE, 1.f, SNDK[2]);
    sd_run_dot(p, 4, v->t * 0.4f);
}

/* E1: portamento: two note bars, the pitch's trace gliding between them */
static void sd_port(const ui_snd_t *v)
{
    const int mode = v->v[0], rate = v->has[2] && v->v[2];
    const float y1 = 146.f, y2 = 72.f, xs = 104.f;     /* the low note, the high one, the second's start */
    const float gw = mode ? 6.f + (float)v->v[1] / 127.f * 96.f : 0.f;
    const float e1 = mode == 2 ? xs + 22.f : xs - 4.f;  /* (LEGATO: the first bar overlaps the second) */
    const qd_rgb on = mode ? SNDK[0] : UI_G2;
    ui_RR(16.f, y1 - 6.f, e1 - 16.f, 12.f, 6.f, QD_NONE, 1.f, UI_G3);
    ui_RR(xs, y2 - 6.f, 222.f - xs, 12.f, 6.f, QD_NONE, 1.f, UI_G3);
    if (mode) {                                         /* SLOPE: a ghost of a smaller interval */
        const float h = (y1 - y2) * 0.5f, w = rate ? gw * 0.5f : gw;
        const float g[6] = {xs, y1, xs + w, y1 - h, 222.f, y1 - h};
        sd_poly(g, 3, UI_G2, 1.2f, 1);
    }
    {
        const float x2 = sd_clampf(xs + gw, xs, 222.f);
        const float p[8] = {16.f, y1, xs, y1, x2, y2, 222.f, y2};
        ui_P(p, 4, on, 2.2f, 0, QD_NONE);
        ui_O(x2, y2, 3.4f, QD_NONE, 1.f, mode ? SNDK[1] : UI_G2);
        if (mode)
            sd_run_dot(p, 4, v->t * 0.4f);
    }
}

/* E3: CRUSH: one sine cycle as stairs (each step as wide as SRR's divider, as tall as BITS' step; both OFF: smooth),
 * the input faint under it; FILTER -> CRUSH or CRUSH -> FILTER below (ROUTE) */
static void sd_crush(const ui_snd_t *v)
{
    const int n = v->pv[0], b = v->pv[1];
    const float sw = n > 1 ? 2.f + (float)n * 1.6f : 0.f, sh = b ? 92.f / (float)(1 << (b > 8 ? 8 : b)) : 0.f;
    float p[2 * 121], q[2 * 241];
    int i, m = 0;
    for (i = 0; i <= 120; i++) {                        /* the sine, faint */
        const float u = (float)i / 120.f;
        p[2 * i] = 16.f + u * 208.f;
        p[2 * i + 1] = 86.f - sinf(u * UI_TAU) * 46.f;
    }
    ui_P(p, 121, UI_G3, 1.2f, 0, QD_NONE);
    for (i = 0; i <= 120 && m < 240; i++) {             /* the stairs */
        float x = 16.f + (float)i / 120.f * 208.f, xs = sw > 0.f ? 16.f + floorf((x - 16.f) / sw) * sw : x;
        float y = sinf((xs - 16.f) / 208.f * UI_TAU) * 46.f;
        if (sh > 0.f)
            y = roundf(y / sh) * sh;
        if (m && (sw > 0.f || sh > 0.f) && q[2 * m - 1] != 86.f - y) {
            q[2 * m] = x, q[2 * m + 1] = q[2 * m - 1];   /* (a riser) */
            m++;
        }
        q[2 * m] = x, q[2 * m + 1] = 86.f - y;
        m++;
    }
    ui_P(q, m, SNDK[0], 1.8f, 0, QD_NONE);
    {   /* the chain: FILTER and CRUSH in ROUTE's order */
        const int after = v->v[2] != 0;
        const float fx = after ? 72.f : 128.f, cx = after ? 128.f : 72.f, y = 146.f;
        const float f[8] = {fx + 6.f, y + 5.f, fx + 18.f, y + 5.f, fx + 26.f, y + 13.f, fx + 34.f, y + 15.f};
        const float st[10] = {cx + 6.f, y + 15.f, cx + 6.f, y + 10.f, cx + 18.f, y + 10.f, cx + 18.f, y + 5.f, cx + 34.f,
                              y + 5.f};
        ui_RR(fx, y, 40.f, 20.f, 4.f, UI_G1, 1.2f, QD_NONE);
        ui_P(f, 4, UI_G1, 1.2f, 0, QD_NONE);
        ui_RR(cx, y, 40.f, 20.f, 4.f, SNDK[2], 1.4f, QD_NONE);
        ui_P(st, 5, SNDK[2], 1.4f, 0, QD_NONE);
        sd_arrow(114.f, y + 10.f, 124.f, y + 10.f, UI_G1);
    }
}

/* E4: a modulation source's glyph (QD_MS_*): VEL a wedge, KEY a key, MW a wheel, AT a finger pressing, PB a bent
 * arrow, BR a breath's puffs, RAND a die, F ENV a small ADSR */
static void sd_mod_icon(int src, float cx, float cy, float s, qd_rgb c)
{
    switch (src) {
    case 1: {
        const float w[6] = {cx - 7.f * s, cy + 6.f * s, cx + 7.f * s, cy + 6.f * s, cx + 7.f * s, cy - 6.f * s};
        ui_P(w, 3, c, 1.3f, 1, QD_NONE);
        break;
    }
    case 2:
        ui_RR(cx - 4.f * s, cy - 7.f * s, 8.f * s, 14.f * s, 1.5f, c, 1.3f, QD_NONE);
        ui_RR(cx - 2.f * s, cy - 7.f * s, 4.f * s, 8.f * s, 1.f, QD_NONE, 1.f, c);
        break;
    case 3:
        ui_O(cx, cy, 6.5f * s, c, 1.3f, QD_NONE);
        line2(cx - 6.5f * s, cy, cx + 6.5f * s, cy, c, 1.3f);
        break;
    case 4:
        ui_O(cx, cy - 3.f * s, 3.f * s, QD_NONE, 1.f, c);
        line2(cx - 7.f * s, cy + 6.f * s, cx + 7.f * s, cy + 6.f * s, c, 1.5f);
        line2(cx, cy, cx, cy + 4.f * s, c, 1.3f);
        break;
    case 5: {
        const float b[6] = {cx - 7.f * s, cy + 4.f * s, cx, cy + 4.f * s, cx + 6.f * s, cy - 5.f * s};
        ui_P(b, 3, c, 1.4f, 0, QD_NONE);
        line2(cx + 6.f * s, cy - 5.f * s, cx + 1.f * s, cy - 5.f * s, c, 1.3f);
        line2(cx + 6.f * s, cy - 5.f * s, cx + 6.f * s, cy, c, 1.3f);
        break;
    }
    case 6:
        ui_O(cx - 4.f * s, cy + 2.f * s, 2.5f * s, c, 1.2f, QD_NONE);
        ui_O(cx + 1.f * s, cy - 2.f * s, 3.f * s, c, 1.2f, QD_NONE);
        ui_O(cx + 6.f * s, cy - 5.f * s, 2.f * s, c, 1.2f, QD_NONE);
        break;
    case 7:
        ui_icon(UI_IC_DICE, cx, cy, s, c);
        break;
    case 8: {
        const float e[10] = {cx - 7.f * s, cy + 5.f * s, cx - 4.f * s, cy - 5.f * s, cx, cy, cx + 4.f * s, cy,
                             cx + 7.f * s, cy + 5.f * s};
        ui_P(e, 5, c, 1.3f, 0, QD_NONE);
        break;
    }
    default:
        line2(cx - 5.f * s, cy, cx + 5.f * s, cy, c, 1.2f);
        break;
    }
}

/* E4: MOD: the eight slots as rows: the source's glyph, an arrow, the destination; the depth a bar from the middle
 * (the chosen slot bright in a box; an empty one a dash) */
static void sd_mod(const ui_snd_t *v)
{
    int k;
    for (k = 0; k < 8; k++) {
        const float y = 44.f + (float)k * 17.f;
        const int sel = k == v->mod_sel, on = v->mod_src[k] > 0;
        const qd_rgb c = sel ? UI_K3 : on ? UI_G1 : UI_G3;
        char n[4] = {(char)('1' + k), 0};
        if (sel)
            ui_RR(10.f, y - 8.f, 220.f, 16.f, 4.f, SNDK[4], 1.3f, QD_NONE);
        ui_T(n, 18.f, y + 4.5f, 12.f, 600, 0.f, sel ? SNDK[4] : UI_G2, QD_CENTER);
        if (!on) {
            line2(34.f, y, 46.f, y, UI_G3, 1.2f);
            continue;
        }
        sd_mod_icon(v->mod_src[k], 40.f, y, 0.72f, sel ? SNDK[0] : c);
        sd_arrow(52.f, y, 62.f, y, c);
        ui_T(v->mod_dst[k] ? v->mod_dst[k] : "", 68.f, y + 4.5f, 12.f, 600, 0.f, sel ? SNDK[1] : c, QD_LEFT);
        {
            const float mid = 180.f, half = 42.f, dx = (float)v->mod_dep[k] / 127.f * half;
            line2(mid - half, y, mid + half, y, UI_G3, 1.f);
            line2(mid, y - 5.f, mid, y + 5.f, UI_G2, 1.f);
            if (dx > 0.5f || dx < -0.5f)
                ui_RR(dx > 0.f ? mid : mid + dx, y - 3.f, dx > 0.f ? dx : -dx, 6.f, 2.f, QD_NONE, 1.f,
                      sel ? SNDK[2] : c);
        }
    }
}

/* an ADSR's points: delay, attack, decay to sustain, held, release (INF: held to the edge) */
static void sd_adsr(const ui_snd_t *v)
{
    const float base = 170.f, top = 44.f;
    const int dly = v->has[4] && v->arg != 1 ? v->v[4] : 0, inf = v->v[3] >= 127;   /* (E1: AMP's ALGORITHM its MODE) */
    const float x0 = 16.f + (float)dly * 0.25f, xa = x0 + 3.f + (float)v->v[0] * 0.35f;
    const float xd = xa + 3.f + (float)v->v[1] * 0.35f, ys = base - (base - top) * (float)v->v[2] / 127.f;
    const float xs = xd + 30.f, xr = inf ? 222.f : xs + 3.f + (float)v->v[3] * 0.3f;
    const float p[12] = {16.f, base, x0, base, xa, top, xd, ys, xs, ys, xr, inf ? ys : base};
    line2(16.f, base, 222.f, base, UI_G3, 1.f);
    sd_poly(p, 6, UI_K3, 1.6f, 0);
    if (inf) {
        const float a[6] = {xr - 5.f, ys - 4.f, xr, ys, xr - 5.f, ys + 4.f};
        ui_P(a, 3, UI_K3, 1.4f, 0, QD_NONE);
    }
    if (dly)
        ui_O(x0, base, 3.4f, QD_NONE, 1.f, SNDK[4]);
    ui_O(xa, top, 3.4f, QD_NONE, 1.f, SNDK[0]);
    ui_O(xd, ys, 3.4f, QD_NONE, 1.f, SNDK[1]);
    ui_O((xd + xs) * 0.5f, ys, 3.4f, QD_NONE, 1.f, SNDK[2]);
    ui_O(inf ? xr - 8.f : xr, inf ? ys : base, 3.4f, QD_NONE, 1.f, SNDK[3]);
    sd_run_dot(p, 6, v->t * 0.4f);
}

static void sd_band(const ui_snd_t *v)
{
    const float lo = 16.f + (float)v->v[0] / 12700.f * 180.f, hi = sd_clampf(lo + 8.f + (float)v->v[1] * 0.8f, 0.f, 224.f);
    const float p[12] = {16.f, 170.f, sd_clampf(lo - 18.f, 12.f, 224.f), 168.f, lo, 80.f, hi, 80.f,   /* (P1.5j: inside
                                                         * the frame's margin) */
                         sd_clampf(hi + 18.f, 16.f, 226.f), 168.f, 224.f, 170.f};
    line2(16.f, 170.f, 224.f, 170.f, UI_G3, 1.f);
    sd_poly(p, 6, UI_K3, 1.6f, 0);
    ui_O(lo, 80.f, 3.4f, QD_NONE, 1.f, SNDK[0]);
    ui_O(hi, 80.f, 3.4f, QD_NONE, 1.f, SNDK[1]);
    {   /* E2: BWPOST: the band (a little window) before or after the filter (a little curve in a box) */
        const int post = v->pv[0] != 0;
        const float fx = post ? 150.f : 196.f, bx = post ? 196.f : 150.f, y = 38.f;
        const float f[8] = {fx + 4.f, y + 5.f, fx + 14.f, y + 5.f, fx + 20.f, y + 13.f, fx + 26.f, y + 15.f};
        ui_RR(fx, y, 30.f, 18.f, 4.f, UI_G1, 1.2f, QD_NONE);
        ui_P(f, 4, UI_G1, 1.2f, 0, QD_NONE);
        ui_RR(bx, y, 30.f, 18.f, 4.f, SNDK[3], 1.4f, QD_NONE);
        ui_RR(bx + 8.f, y + 4.f, 14.f, 10.f, 2.f, QD_NONE, 1.f, SNDK[3]);
        sd_arrow(184.f, y + 9.f, 192.f, y + 9.f, UI_G1);
    }
    if (v->v[2]) {                                      /* KTRK: the band slides up the keys */
        const float k = (float)v->v[2] / 127.f * 40.f;
        sd_arrow((lo + hi) * 0.5f - 20.f, 120.f + k * 0.4f, (lo + hi) * 0.5f + 20.f, 120.f - k * 0.4f, SNDK[2]);
    }
}

static void sd_drive(const ui_snd_t *v)
{
    const float g = (4096.f + (float)(v->v[1] * v->v[1]) * 2.f) / 4096.f, lev = (float)v->v[0] / 127.f;
    float p[2 * 61], q[2 * 61];
    int i;
    const float cy = v->has[2] ? 92.f : 108.f, am = v->has[2] ? 46.f : 58.f;   /* (E1: room for ROUTE's chain) */
    ui_RR(18.f, 40.f, 16.f, 134.f, 4.f, UI_G2, 1.2f, QD_NONE);   /* LEVEL */
    ui_RR(18.f, 40.f + 134.f * (1.f - lev), 16.f, 134.f * lev, 4.f, QD_NONE, 1.f, SNDK[0]);
    line2(52.f, cy, 228.f, cy, UI_G3, 1.f);
    for (i = 0; i <= 60; i++) {                         /* a sine (grey) through the saturation (green) */
        const float u = (float)i / 60.f, x = sinf((u * 2.f + v->t * 0.25f) * UI_TAU);
        const float y = tanhf(x * g * 1.2f) / tanhf(g * 1.2f);
        p[2 * i] = q[2 * i] = 52.f + u * 176.f;
        p[2 * i + 1] = cy - x * am;
        q[2 * i + 1] = cy - y * am;
    }
    ui_P(p, 61, UI_G2, 1.2f, 0, QD_NONE);
    ui_P(q, 61, SNDK[1], 2.f, 0, QD_NONE);
    if (v->has[2]) {                                    /* E1: ROUTE: FILTER then DRIVE (POST), or DRIVE first (PRE) */
        const int pre = v->v[2] != 0;
        const float bx[2] = {72.f, 160.f}, by = 156.f, bw = 52.f, bh = 22.f;
        const float fx = bx[pre ? 1 : 0], dx = bx[pre ? 0 : 1];
        float f[2 * 9], d[2 * 9];
        for (i = 0; i <= 8; i++) {                      /* a low-pass's curve; the saturation's S */
            const float u = (float)i / 8.f, s = u * 2.f - 1.f;
            f[2 * i] = fx + 8.f + u * (bw - 16.f);
            f[2 * i + 1] = by + 6.f + (u < 0.55f ? 0.f : (u - 0.55f) * 22.f);
            d[2 * i] = dx + 8.f + u * (bw - 16.f);
            d[2 * i + 1] = by + bh * 0.5f - tanhf(s * 3.f) * 6.f;
        }
        ui_RR(fx, by, bw, bh, 5.f, UI_G2, 1.4f, QD_NONE);
        ui_P(f, 9, UI_G1, 1.4f, 0, QD_NONE);
        ui_RR(dx, by, bw, bh, 5.f, SNDK[2], 1.6f, QD_NONE);
        ui_P(d, 9, SNDK[1], 1.6f, 0, QD_NONE);
        sd_arrow(bx[0] + bw + 4.f, by + bh * 0.5f, bx[1] - 4.f, by + bh * 0.5f, UI_G1);
    }
}

static void sd_lfo(const ui_snd_t *v)
{
    const float spd = (float)(v->v[0] < 0 ? -v->v[0] : v->v[0]) / 100.f, dep = (float)v->v[2] / 6400.f;
    const float cyc = sd_clampf(1.f + spd / 16.f + (float)(v->v[1] % 12) * 0.25f, 1.f, 6.f);
    const int fade = v->pv[2];                          /* (P1.5k: FADE on the WAVE page now) */
    float p[2 * 81], e[2 * 21];
    int i;
    line2(16.f, 108.f, 224.f, 108.f, UI_G3, 1.f);
    for (i = 0; i <= 80; i++) {                         /* the wave's shape at full depth, faint */
        const float u = (float)i / 80.f;
        p[2 * i] = 16.f + u * 208.f;
        p[2 * i + 1] = 108.f - sd_wave(v->pv[0], u * cyc + v->t * spd / 32.f) * 60.f;
    }
    sd_poly(p, 81, UI_G3, 1.2f, 1);
    for (i = 0; i <= 80; i++) {
        const float u = (float)i / 80.f;
        const float env = fade < 0 ? sd_clampf(u * 64.f / (float)(-fade), 0.f, 1.f)
                        : fade > 0 ? sd_clampf(1.f - u * (float)fade / 64.f, 0.f, 1.f) : 1.f;
        p[2 * i] = 16.f + u * 208.f;
        p[2 * i + 1] = 108.f - sd_wave(v->pv[0], u * cyc + v->t * spd / 32.f) * 60.f * dep * env;
    }
    ui_P(p, 81, SNDK[0], 1.6f, 0, QD_NONE);
    if (fade) {                                         /* FADE: the envelope over it */
        for (i = 0; i <= 20; i++) {
            const float u = (float)i / 20.f;
            const float env = fade < 0 ? sd_clampf(u * 64.f / (float)(-fade), 0.f, 1.f)
                                       : sd_clampf(1.f - u * (float)fade / 64.f, 0.f, 1.f);
            e[2 * i] = 16.f + u * 208.f;
            e[2 * i + 1] = 108.f - 60.f * (dep < 0.f ? -dep : dep) * env;
        }
        sd_poly(e, 21, UI_G1, 1.2f, 1);
    }
    if (v->has[3] && v->txt[3][0]) {                    /* P1.5k: DEST: its name big, an arrow from the wave to it */
        const int none = !strcmp(v->txt[3], "NONE");
        ui_T(v->txt[3], 224.f, 64.f, 22.f, 300, 0.f, none ? UI_G2 : SNDK[3], QD_RIGHT);
        if (!none)
            sd_arrow(150.f, 108.f - 30.f, 224.f - ui_TW(v->txt[3], 22.f, 300, 0.f) - 6.f, 58.f, SNDK[3]);
    }
}

static void sd_lfoshape(const ui_snd_t *v)
{
    static const float NOTE[3] = {16.f, 86.f, 156.f};
    const int w = v->v[0], tr = v->v[2];
    const float ph = (float)v->v[1] / 128.f;
    float p[2 * 141];
    int i, n = 0;
    line2(16.f, 108.f, 224.f, 108.f, UI_G3, 1.f);
    for (i = 0; i < 3; i++)                             /* three notes: where the wave may restart */
        line2(NOTE[i], 170.f, NOTE[i], 160.f, UI_G1, 2.f);
    for (i = 0; i <= 140; i++) {
        const float x = 16.f + (float)i / 140.f * 208.f;
        const int k = x >= NOTE[2] ? 2 : x >= NOTE[1] ? 1 : 0;
        const float since = (x - NOTE[k]) / 52.f, all = (x - 16.f) / 52.f;
        float u = tr == 0 ? all + ph : since + ph, y;     /* FREE: on and on; TRIG / HOLD / ONE / HALF: from each note */
        if (tr == 3 && since > 1.f)
            u = 1.f + ph;                               /* ONE: a cycle, then it stays */
        else if (tr == 4 && since > 0.5f)
            u = 0.5f + ph;                              /* HALF: half a cycle */
        else if (tr == 2)
            u = ph;                                     /* HOLD: held at the note's value */
        y = sd_wave(w, u);
        p[2 * n] = x;
        p[2 * n++ + 1] = 108.f - y * 50.f;
    }
    ui_P(p, n, SNDK[0], 1.6f, 0, QD_NONE);
    for (i = 0; i < 3; i++)                             /* PHASE: where each restart begins */
        if (tr || !i)
            ui_O(NOTE[i], 108.f - sd_wave(w, ph) * 50.f, 3.4f, QD_NONE, 1.f, SNDK[1]);
}

/* P1.5b: the keyboard on: a strip under the head, the keys as they play (SCALE: the 16 W keys, the roots brighter;
 * FULL: the 27 keys as the keybed, the scale's notes lit), a sounding key in the track's colour */
static void sd_keys_strip(const ui_snd_t *v, qd_rgb tc)
{
    static const uint8_t BLK[27] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0};
    int i, w = 0;
    if (v->keys == 1) {
        for (i = 0; i < 16; i++) {
            const int st = v->kst[i];
            const float x = 8.f + (float)i * 14.f;
            const qd_rgb c = st & 4 ? tc : (st & 3) == 2 ? UI_K3 : UI_G1;
            ui_RR(x, 27.f, 12.f, 6.f, 2.f, st & 4 ? QD_NONE : c, 1.f, st & 4 || (st & 3) == 2 ? c : QD_NONE);
        }
        return;
    }
    for (i = 0; i < 27; i++) {                          /* FULL: the whites a row, the blacks over their gaps */
        const int st = v->kst[i];
        const qd_rgb c = st & 4 ? tc : (st & 3) == 2 ? UI_K3 : (st & 3) == 1 ? UI_G1 : UI_G3;
        if (!BLK[i]) {
            const float x = 8.f + (float)w * 14.f;
            ui_RR(x, 27.f, 12.f, 6.f, 2.f, st & 4 || (st & 3) == 2 ? QD_NONE : c, 1.f,
                  st & 4 || (st & 3) == 2 ? c : QD_NONE);
            w++;
        } else {
            const float x = 8.f + (float)w * 14.f - 4.5f;
            ui_RR(x, 25.f, 7.f, 3.f, 1.5f, QD_NONE, 1.f, st & 4 ? tc : (st & 3) ? c : UI_G3);
        }
    }
}


/* ---------------------------------------------------------------- P1.5c: randomise's pictures, in FIZZ's neon lines */
static void sd_glow(const float *p, int n, int close, qd_rgb col, float lw)   /* a line three times: wide faint, the line */
{
    static const float W[3] = {3.4f, 2.f, 1.f}, AL[3] = {0.14f, 0.3f, 1.f};
    const float a0 = qd_get_alpha();
    int i;
    for (i = 0; i < 3; i++) {
        qd_save();
        qd_alpha(a0 * AL[i]);
        ui_P(p, n, col, lw * W[i], close, QD_NONE);
        qd_restore();
    }
}
static float sd_hash(int k) { const float h = sinf((float)k * 127.1f) * 43758.5f; return h - floorf(h); }

/* the dice: two tumbling, their faces flicking, settling as u reaches 1 */
static void sd_dice(float u, float t)
{
    static const float PIP[7][6][2] = {{{0}}, {{0, 0}}, {{-1, -1}, {1, 1}}, {{-1, -1}, {0, 0}, {1, 1}},
        {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}}, {{-1, -1}, {1, -1}, {0, 0}, {-1, 1}, {1, 1}},
        {{-1, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {1, 1}}};
    static const int NP[7] = {0, 1, 2, 3, 4, 5, 6};
    int d;
    for (d = 0; d < 2; d++) {
        const qd_rgb col = d ? 0xff3ca0u : 0x28e1ffu;
        const float spin = (1.f - u) * (1.f - u) * (d ? -9.f : 11.f), x = 120.f + (d ? 34.f : -34.f) - (1.f - u) * (d ? -30.f : 30.f);
        const float y = 106.f - fabsf(sinf(u * 9.f + (float)d)) * 26.f * (1.f - u), h = 20.f;
        const int face = 1 + (int)(sd_hash((int)(t * 16.f) + d * 31) * 6.f) % 6;
        float p[8];
        int k;
        for (k = 0; k < 4; k++) {
            const float an = spin + (float)k * UI_PI * 0.5f + UI_PI * 0.25f;
            p[2 * k] = x + cosf(an) * h * 1.414f;
            p[2 * k + 1] = y + sinf(an) * h * 1.414f;
        }
        sd_glow(p, 4, 1, col, 1.6f);
        for (k = 0; k < NP[face]; k++) {
            const float px = PIP[face][k][0] * 9.f, py = PIP[face][k][1] * 9.f;
            ui_O(x + px * cosf(spin) - py * sinf(spin), y + px * sinf(spin) + py * cosf(spin), 2.6f, QD_NONE, 1.f, col);
        }
    }
}

/* P1.6d: B9 held: the two dice where they will start their roll, shaking harder as u (0..1, a second) grows, a ring
 * closing round them as the roll nears */
static void sd_dice_shake(float u, float t)
{
    int d;
    for (d = 0; d < 2; d++) {
        const qd_rgb col = d ? 0xff3ca0u : 0x28e1ffu;
        const float amp = 1.f + u * u * 7.f, h = 20.f;
        const float x = 120.f + (d ? 34.f : -34.f) - (d ? -30.f : 30.f) + sinf(t * (40.f + 30.f * u) + (float)d * 2.f) * amp;
        const float y = 106.f + cosf(t * (47.f + 30.f * u) + (float)d) * amp * 0.7f;
        const float spin = sinf(t * 23.f + (float)d) * 0.18f * u;
        float p[8];
        int k;
        for (k = 0; k < 4; k++) {
            const float an = spin + (float)k * UI_PI * 0.5f + UI_PI * 0.25f;
            p[2 * k] = x + cosf(an) * h * 1.414f;
            p[2 * k + 1] = y + sinf(an) * h * 1.414f;
        }
        sd_glow(p, 4, 1, col, 1.6f);
        ui_O(x, y, 2.6f, QD_NONE, 1.f, col);
    }
    {   /* the second, as a ring closing */
        float p[2 * 33];
        int i, n = 1 + (int)(u * 32.f);
        for (i = 0; i < n && i <= 32; i++) {
            const float an = -UI_PI / 2.f + (float)i / 32.f * UI_TAU;
            p[2 * i] = 120.f + cosf(an) * 70.f, p[2 * i + 1] = 106.f + sinf(an) * 56.f;
        }
        if (n > 1)
            ui_P(p, n > 33 ? 33 : n, UI_G2, 1.4f, 0, QD_NONE);
    }
}

/* P1.6d: a machine's response as a small glyph (menu order: MULTI LOWPASS 4 LEGACY COMB- COMB+ EQ) in w x h at x, y */
static void sd_machine_glyph(int k, float x, float y, float w, float h, qd_rgb c)
{
    float p[2 * 25];
    int i;
    for (i = 0; i <= 24; i++) {
        const float u = (float)i / 24.f;
        float g;
        switch (k) {
        case 0: g = 0.5f + 0.4f * expf(-(u - 0.5f) * (u - 0.5f) * 40.f) - 0.25f * fabsf(u - 0.5f); break;   /* band */
        case 1: g = u < 0.55f ? 0.85f : 0.85f - (u - 0.55f) * 3.2f; break;            /* 24 dB */
        case 2: g = u < 0.6f ? 0.8f + 0.1f * u : 0.86f - (u - 0.6f) * 1.6f; break;    /* 12 dB */
        case 3: g = 0.3f + 0.6f * powf(fabsf(sinf(u * UI_PI * 3.5f)), 6.f); break;   /* COMB-: teeth between */
        case 4: g = 0.35f + 0.55f * powf(fabsf(cosf(u * UI_PI * 3.5f)), 6.f); break;  /* teeth */
        default: g = 0.45f + 0.45f * expf(-(u - 0.5f) * (u - 0.5f) * 60.f); break;   /* a bell */
        }
        g = g < 0.f ? 0.f : g > 1.f ? 1.f : g;
        p[2 * i] = x + u * w, p[2 * i + 1] = y + h - g * h;
    }
    ui_P(p, 25, c, 1.5f, 0, QD_NONE);
    line2(x, y + h + 1.f, x + w, y + h + 1.f, UI_G3, 1.f);
}

/* the bottle: filling with rising bubbles while held (u 0..1), then (burst) the cork flies and it bursts */
static void sd_bottle(float u, int burst, float t)
{
    static const qd_rgb GLASS = 0x1fd47eu, CHAMP = 0xf3d9a4u, FOIL = 0xffd23fu;
    const float cx = 120.f, top = burst ? 62.f : 62.f, bot = 176.f;
    const float body[] = {cx - 24.f, bot, cx - 24.f, 112.f, cx - 20.f, 100.f, cx - 9.f, 86.f, cx - 9.f, top,
                          cx + 9.f, top, cx + 9.f, 86.f, cx + 20.f, 100.f, cx + 24.f, 112.f, cx + 24.f, bot};
    int k;
    sd_glow(body, 10, 1, GLASS, 1.5f);
    {   /* the foil on the neck */
        const float f[8] = {cx - 9.f, top + 16.f, cx - 9.f, top, cx + 9.f, top, cx + 9.f, top + 16.f};
        sd_glow(f, 4, 0, FOIL, 1.2f);
    }
    {   /* the wine, its level rising with u (full at the burst) */
        const float lv = burst ? 104.f : bot - 4.f - (bot - 112.f) * u;
        const float w[4] = {cx - 21.f, lv, cx + 21.f, lv};
        sd_glow(w, 2, 0, CHAMP, 1.3f);
        for (k = 0; k < (burst ? 0 : 4 + (int)(u * 14.f)); k++) {   /* bubbles rising inside */
            const float ph = fmodf(t * (0.8f + sd_hash(k) * 0.9f) + sd_hash(k + 7), 1.f);
            const float bx = cx - 16.f + sd_hash(k + 3) * 32.f, by = bot - 6.f - ph * (bot - 6.f - lv);
            ui_O(bx, by, 1.2f + sd_hash(k + 11) * 1.8f, CHAMP, 1.2f, QD_NONE);
        }
    }
    if (!burst) {                                       /* the cork, pushed up as it fills */
        const float cy = top - 8.f - u * u * 6.f;
        const float c[8] = {cx - 7.f, top, cx - 8.f, cy - 6.f, cx + 8.f, cy - 6.f, cx + 7.f, top};
        sd_glow(c, 4, 0, CHAMP, 1.4f);
        return;
    }
    {   /* the burst: the cork away, a fountain of bubbles, rays */
        const float cy = top - 14.f - u * 120.f;
        const float c[8] = {cx - 7.f, cy + 10.f, cx - 8.f, cy - 2.f, cx + 8.f, cy - 2.f, cx + 7.f, cy + 10.f};
        if (cy > 30.f)
            sd_glow(c, 4, 1, CHAMP, 1.4f);
        qd_save();
        qd_alpha(qd_get_alpha() * (1.f - u));
        for (k = 0; k < 14; k++) {
            const float an = -UI_PI * 0.5f + (sd_hash(k) - 0.5f) * 1.9f, r = 10.f + u * (50.f + sd_hash(k + 5) * 40.f);
            ui_O(cx + cosf(an) * r, top - 4.f + sinf(an) * r + u * u * 30.f, 1.5f + sd_hash(k + 2) * 2.5f,
                 k % 3 ? CHAMP : FOIL, 1.3f, QD_NONE);
        }
        for (k = 0; k < 8; k++) {
            const float an = (float)k / 8.f * UI_TAU, r0 = 12.f + u * 30.f, r1 = r0 + 10.f;
            const float l[4] = {cx + cosf(an) * r0, top - 4.f + sinf(an) * r0, cx + cosf(an) * r1, top - 4.f + sinf(an) * r1};
            sd_glow(l, 2, 0, FOIL, 1.2f);
        }
        qd_restore();
    }
}

/* ---------------------------------------------------------------- the screen */
void ui_snd_draw(const ui_snd_t *v)
{
    char head[40], n[12];
    float x;
    int k, ncell = 0;
    const qd_rgb hc = v->step ? UI_K3 : UI_TRACK_COL;
    /* P1.5d: the frame: dark orange for the track's sound, bright orange for a step's locks */
    ui_RR(0.f, 0.f, (float)QD_W, (float)QD_H, 8.f, v->step ? UI_SND_FRAME_STEP : UI_SND_FRAME_TRACK, 3.f, QD_NONE);
    head[0] = 0;
    if (v->step)
        cat(cat(cat(head, "STEP "), ui_itoa(n, v->step)), " \xc2\xb7 ");
    cat(head, v->name ? v->name : "");
    x = 10.f;                                           /* P1.5b: the group's pages first (fixed, left), then the name */
    for (k = 0; k < v->npg && v->npg > 1; k++)
        ui_O(x + 2.6f + (float)k * 9.f, 14.f, 2.6f, k == v->ipg ? hc : UI_G2, 1.f, k == v->ipg ? hc : QD_NONE);
    if (v->npg > 1)
        x += (float)v->npg * 9.f + 6.f;
    ui_T(head, x, 18.f, 12.f, 600, 0.8f, hc, QD_LEFT);
    if (v->has[4]) {                                    /* ALGORITHM's value: its badge, top right (a lock: its chip;
                                                         * P1.5j: 13 px, clear of the frame's 4 px margin) */
        float left;
        if (v->locked[4] || v->lockany[4]) {
            float bw = ui_TW(v->txt[4], 13.f, 600, 0.f) + 10.f;
            const float b12 = ui_TW(v->txt[4], 12.f, 600, 0.f) + 10.f;   /* (E2: a long name, LOWPASS 4: at 12 px) */
            bw = bw > 60.f ? (b12 > 60.f ? b12 : 60.f) : bw;
            ui_chip(v->txt[4], 232.f - bw * 0.5f, 21.f, 13.f, bw, SNDK[4], v->locked[4]);
            left = 232.f - bw;
        } else {
            left = 230.f - ui_TW(v->txt[4], 14.f, 500, 0.f);
            ui_T(v->txt[4], 230.f, 20.f, 14.f, 500, 0.f, SNDK[4], QD_RIGHT);
        }
        ui_O(left - 11.f, 15.f, 7.f, QD_NONE, 1.f, SNDK[4]);
        ui_T("A", left - 11.f, 19.f, 12.f, 700, 0.f, UI_BG, QD_CENTER);
    }
    if (v->keys)
        sd_keys_strip(v, UI_TRACK_COL);
    switch (v->draw) {
    case UI_SD_ALGO: sd_algo(v); break;
    case UI_SD_TONE: sd_tone(v); break;
    case UI_SD_OFS: sd_ofs(v); break;
    case UI_SD_OPENV: sd_openv(v); break;
    case UI_SD_TRIG: sd_trig(v); break;
    case UI_SD_KEYS: sd_keys(v); break;
    case UI_SD_FILTER: sd_filter(v); break;
    case UI_SD_ADSR:                                    /* (E1: AMP in AHD, its MODE on ALGORITHM) */
        if (v->arg == 1 && v->has[4] && v->v[4])
            sd_ahd(v);
        else
            sd_adsr(v);
        break;
    case UI_SD_BAND: sd_band(v); break;
    case UI_SD_DRIVE: sd_drive(v); break;
    case UI_SD_LFO: sd_lfo(v); break;
    case UI_SD_PORT: sd_port(v); break;
    case UI_SD_CRUSH: sd_crush(v); break;
    case UI_SD_MOD: sd_mod(v); break;
    default: sd_lfoshape(v); break;
    }
    /* P1.5j (the owner: the lock boxes "change dimensions", "overlap with the orange border"): the foot is four fixed
     * columns inside the frame's 4 px margin (x 7 .. 233), each a label, the knob-turned underline, and one chip
     * rectangle the same on every page (SD_CHIP_W x SD_CHIP_H, the fraction's height), the value centred in it; the
     * chip drawn only when locked (filled: this step, outlined: some step), the geometry the same either way */
    for (k = 0; k < 4; k++)
        ncell += v->has[k];
    for (k = 0; k < 4; k++) {
        const float x0 = SD_FOOT_X + (float)k * SD_COL_W, cx = x0 + SD_COL_W * 0.5f, chx = x0 + 2.f;
        const float mid = SD_CHIP_Y + SD_CHIP_H * 0.5f;
        const int lk = v->locked[k] || v->lockany[k];
        const qd_rgb tc = v->locked[k] ? UI_BG : SNDK[k];
        float w;
        if (!v->has[k])
            continue;
        if (v->modmark[k] && !v->lfomark[k]) {           /* E4: a modulation slot's destination: its source's glyph */
            const float room = SD_COL_W - 18.f;
            sd_mod_icon(v->modmark[k], x0 + 7.f, SD_LAB_Y - 4.5f, 0.75f, UI_K2);
            w = ui_TW(v->lab[k] ? v->lab[k] : "", 12.f, 600, 0.3f);
            ui_T(v->lab[k] ? v->lab[k] : "", x0 + 15.f + room * 0.5f, SD_LAB_Y, w > room ? (12.f * room / w < 12.f ? 12.f
                 : 12.f * room / w) : 12.f, 600, 0.f, v->locked[k] ? UI_K3 : UI_G1, QD_CENTER);
        } else if (v->lfomark[k]) {                     /* P1.5k: an LFO's destination: its number in its colour, left */
            const int lf = v->lfomark[k] & 1u ? 0 : v->lfomark[k] & 2u ? 1 : 2;
            char d[2] = {(char)('1' + lf), 0};
            const float room = SD_COL_W - 18.f;
            ui_O(x0 + 7.f, SD_LAB_Y - 4.5f, 6.5f, QD_NONE, 1.f, UI_LFO_COL[lf]);
            ui_T(d, x0 + 7.f, SD_LAB_Y, 12.f, 700, 0.f, UI_BG, QD_CENTER);
            w = ui_TW(v->lab[k] ? v->lab[k] : "", 12.f, 600, 0.3f);
            ui_T(v->lab[k] ? v->lab[k] : "", x0 + 15.f + room * 0.5f, SD_LAB_Y, w > room ? (12.f * room / w < 12.f ? 12.f
                 : 12.f * room / w) : 12.f, 600, 0.f, v->locked[k] ? UI_K3 : UI_G1, QD_CENTER);
        } else {
            w = ui_TW(v->lab[k] ? v->lab[k] : "", 12.f, 600, 0.3f);
            ui_T(v->lab[k] ? v->lab[k] : "", cx, SD_LAB_Y, w > SD_COL_W - 4.f ? 12.f * (SD_COL_W - 4.f) / w : 12.f,
                 600, 0.3f, v->locked[k] ? UI_K3 : UI_G1, QD_CENTER);
        }
        if (v->lk && v->lk_k == k && v->lk_ok) {        /* P1.5k: the value being mapped: a little wave, the LFO's */
            float p[2 * 17];
            int i;
            for (i = 0; i <= 16; i++) {
                p[2 * i] = cx - 16.f + (float)i * 2.f;
                p[2 * i + 1] = SD_LAB_Y + 3.5f - sinf((float)i / 16.f * UI_TAU * 2.f + v->t * 6.f) * 2.f;
            }
            ui_P(p, 17, v->lk == 4 ? UI_K2 : UI_LFO_COL[v->lk - 1], 1.4f, 0, QD_NONE);
        } else if (v->last == k) {                      /* the knob turned last: underlined, under its label */
            line2(cx - 16.f, SD_LAB_Y + 3.5f, cx + 16.f, SD_LAB_Y + 3.5f, SNDK[k], 2.f);
        }
        if (lk)
            ui_RR(chx, SD_CHIP_Y, SD_CHIP_W, SD_CHIP_H, 7.f, v->locked[k] ? QD_NONE : SNDK[k], 1.4f,
                  v->locked[k] ? SNDK[k] : QD_NONE);
        if (v->pair[k]) {                               /* P1.5i: a ratio pair: B2 over B1, a fraction (ChoralRoot's) */
            const float wn = ui_TW(v->txt[k], 13.f, 600, 0.f), wd = ui_TW(v->den[k], 13.f, 600, 0.f);
            const float wm = (wn > wd ? wn : wd) + 6.f;
            ui_T(v->txt[k], cx, mid - 3.f, 13.f, 600, 0.f, tc, QD_CENTER);
            line2(cx - wm * 0.5f, mid + 0.5f, cx + wm * 0.5f, mid + 0.5f, tc, 1.4f);
            ui_T(v->den[k], cx, mid + 13.5f, 13.f, 600, 0.f, tc, QD_CENTER);
        } else {
            float sz = 18.f;
            w = ui_TW(v->txt[k], sz, 500, 0.f);
            if (w > SD_CHIP_W - 8.f)
                sz = sz * (SD_CHIP_W - 8.f) / w < 12.f ? 12.f : sz * (SD_CHIP_W - 8.f) / w;
            ui_T(v->txt[k], cx, mid + sz * 0.35f, sz, lk ? 600 : 500, 0.f, tc, QD_CENTER);
        }
    }
    (void)ncell;
    if (v->rnd_kind) {                                  /* P1.5c: randomise: the dice, the bottle (over the drawing) */
        ui_RR(12.f, 34.f, 216.f, 146.f, 10.f, QD_NONE, 1.f, UI_BG);
        if (v->rnd_kind == 1)
            sd_dice_shake(v->rnd_u, v->t);
        else if (v->rnd_kind == 4)
            sd_dice(v->rnd_u, v->t);
        else
            sd_bottle(v->rnd_u, v->rnd_kind == 3, v->t);
    }
    if (v->fpop) {                                      /* P1.6d: the FILTER key held: the six machines */
        static const char *const NM[6] = {"MULTI", "LOWPASS 4", "LEGACY", "COMB-", "COMB+", "EQ"};
        int k;
        ui_RR(8.f, 30.f, 224.f, 152.f, 10.f, SNDK[4], 2.f, UI_BG);
        for (k = 0; k < 6; k++) {
            const float x = 14.f + (float)(k % 2) * 108.f, y = 36.f + (float)(k / 2) * 48.f;
            const int on = k == v->fpop_sel;
            const qd_rgb c = on ? SNDK[4] : UI_G1;
            char n[2] = {(char)('1' + k), 0};
            if (on)
                ui_RR(x, y, 104.f, 44.f, 7.f, QD_NONE, 1.f, 0x2a1622u);
            ui_RR(x, y, 104.f, 44.f, 7.f, on ? SNDK[4] : UI_G3, on ? 1.8f : 1.f, QD_NONE);
            sd_machine_glyph(k, x + 10.f, y + 4.f, 64.f, 17.f, c);   /* (P1.6d.1: the curve on top, the name the
                                                         * tile's width under it, the number top right) */
            ui_T(NM[k], x + 52.f, y + 38.f, 12.f, 600, 0.f, on ? UI_K3 : UI_G1, QD_CENTER);
            ui_T(n, x + 92.f, y + 16.f, 12.f, 600, 0.f, on ? SNDK[4] : UI_G2, QD_CENTER);
        }
    }
    if (v->lk) {                                        /* P1.5k: an LFO key held: "LFO n -> VALUE", the depth (E4: B1
                                                         * held: the source's glyph and name -> VALUE) */
        static const char *const SRCN[9] = {"", "VEL", "KEY", "MW", "AT", "PB", "BR", "RAND", "F ENV"};
        const qd_rgb lc = v->lk == 4 ? UI_K2 : UI_LFO_COL[v->lk - 1];
        const float sh = v->lk_ok ? 0.f : sinf(v->lk_shake * 30.f) * 6.f * v->lk_shake;
        char t[8] = "LFO 1";
        t[4] = (char)('0' + v->lk);
        ui_RR(12.f, 36.f, 216.f, 140.f, 10.f, lc, 2.f, UI_BG);
        if (v->lk == 4) {
            sd_mod_icon(v->lk_src, 34.f, 57.f, 1.4f, lc);
            ui_T(SRCN[v->lk_src > 0 && v->lk_src < 9 ? v->lk_src : 1], 52.f, 62.f, 16.f, 600, 0.8f, lc, QD_LEFT);
        } else {
            ui_T(t, 26.f, 62.f, 16.f, 600, 0.8f, lc, QD_LEFT);
        }
        sd_arrow(26.f, 80.f, 58.f, 98.f, lc);
        if (v->lk_name)
            ui_T(v->lk_name, 120.f + sh, 116.f, 26.f, 300, 0.f, v->lk_ok ? UI_K3 : UI_G2, QD_CENTER);
        if (v->lk_ok) {                                 /* the depth: a bipolar meter from the centre */
            const float x0 = 30.f, x1 = 210.f, mid = 120.f, y = 148.f;
            const float dx = sd_clampf(v->lk_depth, -1.f, 1.f) * (x1 - mid);
            line2(x0, y, x1, y, UI_G2, 2.f);
            line2(mid, y - 8.f, mid, y + 8.f, UI_G1, 1.2f);
            if (dx > 0.5f || dx < -0.5f)
                ui_RR(dx > 0.f ? mid : mid + dx, y - 5.f, dx > 0.f ? dx : -dx, 10.f, 3.f, QD_NONE, 1.f, lc);
        }
    }
}
