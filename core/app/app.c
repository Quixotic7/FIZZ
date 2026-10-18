/* SPDX-License-Identifier: GPL-3.0-only */
/* QUADRILLE's app, P1.0 (app.h): the transport, the idle screen's dance, the LEDs, a test tone. Deterministic: time
 * is the ticks' milliseconds only. */
#include <math.h>
#include <string.h>

#include "app.h"
#include "qd_gfx.h"
#include "ui.h"

#define QD_TAU 6.283185307179586
#define QD_SWAY_MS 1600.0              /* stopped: one slow sway (a bob and an arm swing) this long */
#define QD_SWAY_AMP 0.55f              /* .. and smaller than the dance */
#define QD_AMP_MS 300.f                /* the motion's size eases between the two over this long */
#define QD_SETTLE_MS 250.f             /* stopped mid-turn: the quarter turn finishes in this long */
#define QD_TONE_GAIN 0.2               /* the test tone: a key at velocity 127, of full scale */
#define QD_TONE_ATTACK_MS 5.0
#define QD_TONE_RELEASE_MS 80.0

void qd_app_init(qd_app_t *a)
{
    memset(a, 0, sizeof *a);
    a->bpm = QD_BPM_DEFAULT;
    a->amp = QD_SWAY_AMP;
}

/* ---------------------------------------------------------------- input --- */
void qd_app_key(qd_app_t *a, int key, int down, int vel)
{
    if (key < 0 || key >= QD_NKEY)
        return;
    if (down) {
        a->keys_held |= 1u << key;
        a->key_vel[key] = (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel);
    } else {
        a->keys_held &= ~(1u << key);
    }
}

static void qd_app_play(qd_app_t *a)
{
    a->playing = 1;
    a->beats = 0.0;                    /* the transport starts at the top of the bar */
    a->turn_base = a->turn_target;     /* (a whole number: where the last stop settled) */
    a->turn = a->turn_base;
}

static void qd_app_stop(qd_app_t *a)
{
    a->playing = 0;
    a->turn_target = ceilf(a->turn - 1e-4f);   /* mid-turn: the turn finishes, then the dancers rest */
    a->sway = fmod(a->beats * QD_TAU, QD_TAU); /* the sway goes on from the dance's pose */
    a->beats = 0.0;
}

void qd_app_button(qd_app_t *a, int button, int down)
{
    if (button < 0 || button >= QD_NBTN)
        return;
    if (!down) {
        a->buttons_held &= ~(1u << button);
        return;
    }
    a->buttons_held |= 1u << button;
    if (button == QD_B_PLAY) {
        if (a->playing)
            qd_app_stop(a);
        else
            qd_app_play(a);
    }
}

void qd_app_knob(qd_app_t *a, int knob, int detents)
{
    if (knob == QD_K_SELECT || knob == QD_K_1) {   /* SELECT turns what KNOB 1 turns: the tempo */
        int b = a->bpm + detents;
        a->bpm = b < QD_BPM_MIN ? QD_BPM_MIN : b > QD_BPM_MAX ? QD_BPM_MAX : b;
    }
}

/* ---------------------------------------------------------------- time --- */
static void qd_app_ms(qd_app_t *a)     /* one device millisecond */
{
    const float amp_to = a->playing ? 1.f : QD_SWAY_AMP;
    if (a->playing) {
        double bars, whole;
        a->beats += (double)a->bpm / 60000.0;
        bars = a->beats / 4.0;
        whole = floor(bars);
        /* line.html s1: a quarter turn each bar, in its last beat */
        a->turn = a->turn_base + (float)whole + ui_ease((float)((bars - whole) - 0.75) * 4.f);
    } else {
        const float d = a->turn_target - a->turn, step = 1.f / QD_SETTLE_MS;
        a->turn = fabsf(d) <= step ? a->turn_target : a->turn + (d > 0.f ? step : -step);
        a->sway += QD_TAU / QD_SWAY_MS;
        if (a->sway >= QD_TAU)
            a->sway -= QD_TAU;
    }
    if (a->amp < amp_to)
        a->amp = fminf(amp_to, a->amp + (1.f - QD_SWAY_AMP) / QD_AMP_MS);
    else if (a->amp > amp_to)
        a->amp = fmaxf(amp_to, a->amp - (1.f - QD_SWAY_AMP) / QD_AMP_MS);
}

void qd_app_tick(qd_app_t *a, uint32_t ms)
{
    uint32_t n;
    if (!a->started || ms < a->ms) {   /* the first tick (or a clock that restarted): the time starts here */
        a->started = 1;
        a->ms = ms;
        return;
    }
    n = ms - a->ms;
    if (n > 60000u)                    /* (a huge gap: the last minute of it) */
        n = 60000u;
    while (n--)
        qd_app_ms(a);
    a->ms = ms;
}

/* ---------------------------------------------------------------- out --- */
void qd_app_frame(qd_app_t *a, uint16_t *lcd_be)
{
    ui_idle_t v;
    memset(&v, 0, sizeof v);
    memcpy(v.pattern, "A03", 4);
    v.bpm = a->bpm;
    v.playing = a->playing;
    v.turn = a->turn;
    v.phase = (float)(a->playing ? fmod(a->beats * QD_TAU, 2.0 * QD_TAU) : a->sway);
    v.amp = a->amp;
    qd_begin();
    ui_idle_draw(&v);
    qd_end(lcd_be);
}

void qd_app_leds(const qd_app_t *a, qd_leds_t *out)
{
    int i;
    memset(out, 0, sizeof *out);
    for (i = 0; i < QD_NKEY; i++)
        out->key[i] = (a->keys_held >> i) & 1u ? QD_LED_LIT : QD_LED_OFF;
    for (i = 0; i < QD_NBTN; i++)
        out->button[i] = (a->buttons_held >> i) & 1u ? QD_LED_LIT : QD_LED_OFF;
    if (a->playing) {
        const double into_beat_ms = (a->beats - floor(a->beats)) * 60000.0 / (double)a->bpm;
        out->play_green = into_beat_ms < (double)QD_BEAT_LED_MS ? QD_LED_LIT : QD_LED_OFF;
    }
}

/* the test tone: each held key a sine at its pitch, its velocity the level, a 5 ms attack and an 80 ms release */
void qd_app_audio(qd_app_t *a, int16_t *stereo, uint32_t frames)
{
    const double att = 1.0 / (QD_TONE_ATTACK_MS * QD_FS / 1000.0), rel = 1.0 / (QD_TONE_RELEASE_MS * QD_FS / 1000.0);
    double inc[QD_NKEY];
    uint32_t i;
    int k, any = 0;
    for (k = 0; k < QD_NKEY; k++) {
        inc[k] = QD_TAU * 440.0 * pow(2.0, (double)(QD_NOTE0 + k - 69) / 12.0) / QD_FS;
        any |= ((a->keys_held >> k) & 1u) || a->tone_env[k] > 0.f;
    }
    if (!any) {
        memset(stereo, 0, (size_t)frames * 2u * sizeof *stereo);
        return;
    }
    for (i = 0; i < frames; i++) {
        double s = 0.0;
        int v;
        for (k = 0; k < QD_NKEY; k++) {
            const double to = (a->keys_held >> k) & 1u ? (double)a->key_vel[k] / 127.0 : 0.0;
            double e = a->tone_env[k];
            if (e < to)
                e = e + att > to ? to : e + att;
            else if (e > to)
                e = e - rel < to ? to : e - rel;
            a->tone_env[k] = (float)e;
            if (e <= 0.0)
                continue;
            s += sin(a->tone_phase[k]) * e * QD_TONE_GAIN;
            a->tone_phase[k] += inc[k];
            if (a->tone_phase[k] >= QD_TAU)
                a->tone_phase[k] -= QD_TAU;
        }
        v = (int)lrint(s * 32767.0);
        v = v > 32767 ? 32767 : v < -32768 ? -32768 : v;
        stereo[2 * i] = stereo[2 * i + 1] = (int16_t)v;
    }
}
