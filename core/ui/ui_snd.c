/* SPDX-License-Identifier: GPL-3.0-only */
/* The sound editor's screen (P1.4a; ui.h ui_snd_t). Built through ui_layer.c (it includes this file: its line2 and
 * cat; the core's source lists stay as they are). PLAN §9a's clarity rules: the page's name, the values and their short
 * names are the only text; the rest is drawing, colour and motion. */
#include <stdlib.h>

static const qd_rgb SNDK[5] = {UI_K1, UI_K2, UI_K3, UI_K4, UI_KALG};   /* KNOB 1..4, ALGORITHM */
#define SD_TOP 32.f                                     /* the drawing's area */
#define SD_BOT 182.f

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
static void sd_algo(const ui_snd_t *v)
{
    int dep[4] = {0, 0, 0, 0}, row[4], n[4] = {0, 0, 0, 0}, o, t, it, md = 0;
    float x[4], y[4], sp;
    const unsigned out = (unsigned)(v->outx | v->outy);
    for (it = 0; it < 4; it++)                          /* a modulator sits above what it modulates */
        for (t = 0; t < 4; t++)
            for (o = 0; o < 4; o++)
                if (o != t && ((v->mod[t] >> o) & 1u) && dep[o] < dep[t] + 1)
                    dep[o] = dep[t] + 1;
    for (o = 0; o < 4; o++) {
        row[o] = n[dep[o] > 3 ? 3 : dep[o]]++;
        md = dep[o] > md ? dep[o] : md;
    }
    sp = md >= 3 ? 38.f : 46.f;
    for (o = 0; o < 4; o++) {
        const int d = dep[o] > 3 ? 3 : dep[o], cnt = n[d];
        x[o] = 112.f + ((float)row[o] - (float)(cnt - 1) * 0.5f) * 64.f;
        y[o] = 148.f - (float)d * sp;
    }
    line2(18.f, 178.f, 196.f, 178.f, UI_G2, 1.2f);      /* the bus, the speaker */
    {
        const float sp[8] = {200.f, 172.f, 206.f, 172.f, 206.f, 184.f, 200.f, 184.f};
        const float co[8] = {206.f, 172.f, 214.f, 165.f, 214.f, 191.f, 206.f, 184.f};
        const float pulse = v->t * 2.f - floorf(v->t * 2.f);
        ui_P(sp, 4, UI_G1, 1.2f, 1, QD_NONE);
        ui_P(co, 4, UI_G1, 1.2f, 1, QD_NONE);
        qd_save();
        qd_alpha(1.f - pulse);
        qd_arc(214.f, 178.f, 8.f + pulse * 8.f, -0.7f, 0.7f, UI_G1, 1.2f);
        qd_restore();
    }
    for (o = 0; o < 4; o++)
        if ((out >> o) & 1u) {                          /* a carrier into the bus (from above: down its right side) */
            const float ox = dep[o] ? x[o] + 21.f : x[o];
            if (dep[o])
                line2(x[o] + 17.f, y[o], ox, y[o], UI_G1, 1.2f);
            line2(ox, dep[o] ? y[o] : y[o] + 17.f, ox, 178.f, UI_G1, 1.2f);
            ui_O(ox, 178.f, 2.f, QD_NONE, 1.f, UI_G1);
        }
    for (t = 0; t < 4; t++)                             /* the modulations: arrows, a dot running along each */
        for (o = 0; o < 4; o++)
            if (o != t && ((v->mod[t] >> o) & 1u)) {
                const float dx = x[t] - x[o], dy = y[t] - y[o], l = sqrtf(dx * dx + dy * dy);
                const float ux = dx / l, uy = dy / l, u = v->t * 1.2f + (float)(o + t) * 0.27f;
                const float f = u - floorf(u);
                sd_arrow(x[o] + ux * 18.f, y[o] + uy * 18.f, x[t] - ux * 19.f, y[t] - uy * 19.f, UI_G1);
                ui_O(x[o] + ux * 18.f + (l - 37.f) * f * ux, y[o] + uy * 18.f + (l - 37.f) * f * uy, 1.8f, QD_NONE, 1.f,
                     UI_K3);
            }
    if (v->fb < 4) {                                    /* the feedback: a loop over its operator */
        const int f = v->fb;
        qd_arc(x[f] + 16.f, y[f] - 12.f, 8.f, UI_PI * 0.75f, UI_PI * 2.25f, UI_G1, 1.4f);
    }
    for (o = 0; o < 4; o++) {                           /* the operators: their waves at their ratios */
        const float r = sd_clampf(v->ratio[o], 0.25f, 4.f);
        float p[2 * 25];
        int i;
        ui_O(x[o], y[o], 17.f, SNDK[o], 1.6f, UI_BG);
        for (i = 0; i < 25; i++) {
            const float u = (float)i / 24.f;
            p[2 * i] = x[o] - 11.f + u * 22.f;
            p[2 * i + 1] = y[o] - sinf((u * r * 1.5f - v->t * r) * UI_TAU) * 7.f;
        }
        ui_P(p, 25, SNDK[o], 1.2f, 0, QD_NONE);
    }
}

static void sd_tone(const ui_snd_t *v)
{
    const float harm = (float)v->v[0] / 100.f, dt = (float)v->v[1] / 100.f;
    const int fb = v->v[2], mix = v->v[3];
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
        p[2 * i + 1] = 146.f - sd_clampf(w, -1.2f, 1.2f) * 18.f;
    }
    ui_P(p, 41, SNDK[2], 1.2f, 0, QD_NONE);
    qd_arc(68.f, 146.f, 34.f, UI_PI * 1.15f, UI_PI * 1.85f, UI_G2, 1.f);
    /* MIX: the balance X .. Y */
    {
        const float x = 140.f + (float)(mix + 64) / 127.f * 80.f;
        line2(140.f, 150.f, 220.f, 150.f, UI_G2, 2.f);
        ui_T("X", 140.f, 132.f, 12.f, 600, 0.f, UI_G1, QD_CENTER);
        ui_T("Y", 220.f, 132.f, 12.f, 600, 0.f, UI_G1, QD_CENTER);
        ui_O(x, 150.f, 6.f, QD_NONE, 1.f, SNDK[3]);
    }
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

static void sd_filter(const ui_snd_t *v)
{
    const float fc = 16.f + (float)v->v[0] / 12700.f * 200.f, res = (float)v->v[1] / 12700.f;
    const int ty = v->v[2];
    float p[2 * 61];
    int i;
    line2(16.f, 170.f, 224.f, 170.f, UI_G3, 1.f);
    for (i = 0; i <= 60; i++) {
        const float x = 16.f + (float)i / 60.f * 208.f, d = (x - fc) / 40.f;
        float g = 1.f, pk = res * 50.f * expf(-d * d * 6.f);
        if (ty == 1)
            g = d <= 0.f ? 1.f : 1.f / (1.f + d * d * 2.f);
        else if (ty == 2)
            g = d >= 0.f ? 1.f : 1.f / (1.f + d * d * 2.f);
        else if (ty == 3)
            g = d <= 0.f ? 1.f : 1.f / (1.f + d * d * d * d * 6.f);
        if (!ty)
            pk = 0.f;
        p[2 * i] = x;
        p[2 * i + 1] = 170.f - sd_clampf(g * 90.f + pk, 0.f, 128.f);
    }
    ui_P(p, 61, SNDK[2], 1.6f, 0, QD_NONE);
    if (ty) {
        ui_O(fc, 170.f - sd_clampf(90.f + res * 50.f, 0.f, 128.f), 3.4f, QD_NONE, 1.f, SNDK[1]);
        line2(fc, 170.f, fc, 176.f, SNDK[0], 2.f);
        ui_O(fc, 176.f, 3.4f, QD_NONE, 1.f, SNDK[0]);
        if (v->v[3]) {                                  /* DEPTH: the envelope moves the cutoff */
            const float l = (float)v->v[3] / 6400.f * 60.f;
            sd_arrow(fc + 10.f, 110.f, fc + 10.f + l, 110.f, SNDK[3]);
        }
    }
}

/* an ADSR's points: delay, attack, decay to sustain, held, release (INF: held to the edge) */
static void sd_adsr(const ui_snd_t *v)
{
    const float base = 170.f, top = 44.f;
    const int dly = v->has[4] ? v->v[4] : 0, inf = v->v[3] >= 127;
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
    const float p[12] = {16.f, 170.f, lo - 18.f, 168.f, lo, 80.f, hi, 80.f, hi + 18.f, 168.f, 224.f, 170.f};
    line2(16.f, 170.f, 224.f, 170.f, UI_G3, 1.f);
    sd_poly(p, 6, UI_K3, 1.6f, 0);
    ui_O(lo, 80.f, 3.4f, QD_NONE, 1.f, SNDK[0]);
    ui_O(hi, 80.f, 3.4f, QD_NONE, 1.f, SNDK[1]);
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
    ui_RR(18.f, 40.f, 16.f, 134.f, 4.f, UI_G2, 1.2f, QD_NONE);   /* LEVEL */
    ui_RR(18.f, 40.f + 134.f * (1.f - lev), 16.f, 134.f * lev, 4.f, QD_NONE, 1.f, SNDK[0]);
    line2(52.f, 108.f, 228.f, 108.f, UI_G3, 1.f);
    for (i = 0; i <= 60; i++) {                         /* a sine (grey) through the saturation (green) */
        const float u = (float)i / 60.f, x = sinf((u * 2.f + v->t * 0.25f) * UI_TAU);
        const float y = tanhf(x * g * 1.2f) / tanhf(g * 1.2f);
        p[2 * i] = q[2 * i] = 52.f + u * 176.f;
        p[2 * i + 1] = 108.f - x * 58.f;
        q[2 * i + 1] = 108.f - y * 58.f;
    }
    ui_P(p, 61, UI_G2, 1.2f, 0, QD_NONE);
    ui_P(q, 61, SNDK[1], 2.f, 0, QD_NONE);
}

static void sd_lfo(const ui_snd_t *v)
{
    const float spd = (float)(v->v[0] < 0 ? -v->v[0] : v->v[0]) / 100.f, dep = (float)v->v[2] / 6400.f;
    const float cyc = sd_clampf(1.f + spd / 16.f + (float)(v->v[1] % 12) * 0.25f, 1.f, 6.f);
    const int fade = v->v[3];
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
        sd_poly(e, 21, SNDK[3], 1.2f, 1);
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

/* ---------------------------------------------------------------- the screen */
void ui_snd_draw(const ui_snd_t *v)
{
    char head[40], n[12];
    float x;
    int k, ncell = 0;
    const qd_rgb hc = v->step ? UI_K3 : UI_TRACK_COL;
    if (!v->step)                                       /* the track's sound: the TRACK frame (P1.3y) */
        ui_RR(0.f, 0.f, (float)QD_W, (float)QD_H, 8.f, UI_TRACK_COL, 3.f, QD_NONE);
    head[0] = 0;
    if (v->step)
        cat(cat(cat(head, "STEP "), ui_itoa(n, v->step)), " \xc2\xb7 ");
    cat(head, v->name ? v->name : "");
    x = 8.f;
    if (v->step) {
        ui_lock(8.f, 7.5f, UI_K3, 1.2f);
        x = 22.f;
    }
    x += ui_T(head, x, 18.f, 12.f, 600, 0.8f, hc, QD_LEFT) + 8.f;
    for (k = 0; k < v->npg && v->npg > 1; k++)          /* the group's pages: this one filled */
        ui_O(x + (float)k * 9.f, 14.f, 2.6f, k == v->ipg ? hc : UI_G2, 1.f, k == v->ipg ? hc : QD_NONE);
    if (v->has[4]) {                                    /* ALGORITHM's value: its badge, top right */
        const float w = ui_TW(v->txt[4], 16.f, 500, 0.f);
        ui_T(v->txt[4], 230.f, 20.f, 16.f, 500, 0.f, SNDK[4], QD_RIGHT);
        ui_O(230.f - w - 14.f, 14.f, 8.f, QD_NONE, 1.f, SNDK[4]);
        ui_T("A", 230.f - w - 14.f, 18.5f, 12.f, 700, 0.f, UI_BG, QD_CENTER);
        if (v->locked[4])
            ui_lock(230.f - w - 34.f, 7.5f, SNDK[4], 1.2f);
    }
    switch (v->draw) {
    case UI_SD_ALGO: sd_algo(v); break;
    case UI_SD_TONE: sd_tone(v); break;
    case UI_SD_OFS: sd_ofs(v); break;
    case UI_SD_OPENV: sd_openv(v); break;
    case UI_SD_TRIG: sd_trig(v); break;
    case UI_SD_KEYS: sd_keys(v); break;
    case UI_SD_FILTER: sd_filter(v); break;
    case UI_SD_ADSR: sd_adsr(v); break;
    case UI_SD_BAND: sd_band(v); break;
    case UI_SD_DRIVE: sd_drive(v); break;
    case UI_SD_LFO: sd_lfo(v); break;
    default: sd_lfoshape(v); break;
    }
    for (k = 0; k < 4; k++)
        ncell += v->has[k];
    for (k = 0; k < 4; k++) {                           /* KNOB 1..4: a cell each along the foot */
        const float cx = 30.f + (float)k * 60.f;
        float w;
        if (!v->has[k])
            continue;
        w = ui_TW(v->lab[k] ? v->lab[k] : "", 12.f, 600, 0.3f);
        ui_T(v->lab[k] ? v->lab[k] : "", cx, 204.f, w > 56.f ? 12.f * 56.f / w : 12.f, 600, 0.3f,
             v->locked[k] ? UI_K3 : UI_G1, QD_CENTER);
        w = ui_TW(v->txt[k], 18.f, 500, 0.f);
        ui_T(v->txt[k], cx, 228.f, w > 56.f ? 18.f * 56.f / w : 18.f, 500, 0.f, SNDK[k], QD_CENTER);
        if (v->locked[k])
            ui_lock(cx - 4.f, 186.f, SNDK[k], 1.1f);
        if (v->last == k)                               /* the knob turned last: underlined */
            line2(cx - 18.f, 235.f, cx + 18.f, 235.f, SNDK[k], 2.f);
    }
    (void)ncell;
}
