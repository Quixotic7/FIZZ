/* SPDX-License-Identifier: GPL-3.0-only */
/* QUADRILLE's app (app.h): the transport, the idle screen's dance, KEYS, the sound picker, the LEDs, the sound (the
 * tracks' FM TONE voices, the mix, ChoralRoot's effects). Deterministic: time is the ticks' milliseconds only. */
#include <math.h>
#include <string.h>

#include "app.h"
#include "qd_gfx.h"
#include "ui.h"
#include "../voice/fx/qd_fx.h"

#define QD_TAU 6.283185307179586
#define QD_SWAY_AMP 0.55f              /* stopped: the sway (a bob a beat at the tempo) smaller than the dance */
#define QD_AMP_MS 300.f                /* the motion's size eases between the two over this long */
#define QD_SETTLE_MS 250.f             /* stopped mid-turn: the quarter turn finishes in this long */
#define QD_ROLL_MS 150.f               /* the picker's roll at a detent */

/* the white keys 1..16 (F3 .. G5) as key indices */
static const uint8_t QD_WHITE[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};

static int qd_white_of(int key)        /* 0..15, or -1 for a black key */
{
    int i;
    for (i = 0; i < 16; i++)
        if (QD_WHITE[i] == key)
            return i;
    return -1;
}

static void qd_app_tempo(qd_app_t *a, int bpm)
{
    a->bpm = bpm < QD_BPM_MIN ? QD_BPM_MIN : bpm > QD_BPM_MAX ? QD_BPM_MAX : bpm;
    qd_model_tempo(&a->model, a->bpm);  /* (FM TONE's synced LFOs and DELAYs) */
    qd_fx_tempo(a->bpm);                /* (the synced delay) */
}

void qd_app_init(qd_app_t *a)
{
    int t;
    memset(a, 0, sizeof *a);
    a->amp = QD_SWAY_AMP;
    a->master_pot = QD_MASTER_POT_DEFAULT;
    qd_model_init(&a->model);
    qd_pool_init(&a->pool, QD_VOICES);
    for (t = 0; t < QD_NTRACK; t++)
        qd_pool_set_track(&a->pool, t, a->model.t[t].limit, a->model.t[t].reserve);
    qd_mix_init(&a->mix);
    qd_fx_init();
    qd_fx_master_pot(a->master_pot);
    qd_app_tempo(a, QD_BPM_DEFAULT);
}

/* ---------------------------------------------------------------- the notes --- */
static void qd_note_on(qd_app_t *a, int track, int note, int vel)
{
    qd_track_t *t = &a->model.t[track];
    if (note < 0 || note > 127)
        return;
    qd_pool_note_on(&a->pool, track, &t->owner, note, vel, t->patch);
}

/* ---------------------------------------------------------------- input --- */
void qd_app_key(qd_app_t *a, int key, int down, int vel)
{
    if (key < 0 || key >= QD_NKEY)
        return;
    if (down) {
        int w = qd_white_of(key);
        a->keys_held |= 1u << key;
        a->key_vel[key] = (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel);
        if ((a->buttons_held >> QD_B_TRACKS) & 1u) {    /* TRACKS held: white keys 1..8 select a track */
            if (w >= 0 && w < QD_NTRACK)
                a->sel = w;
            return;
        }
        a->key_note[key] = (uint8_t)(QD_NOTE0 + key + 12 * a->oct);
        a->key_track[key] = (uint8_t)a->sel;
        a->keys_struck |= 1u << key;
        qd_note_on(a, a->sel, a->key_note[key], a->key_vel[key]);
    } else {
        a->keys_held &= ~(1u << key);
        if ((a->keys_struck >> key) & 1u) {             /* the note it struck (the octave or the track may have
                                                         * changed since) */
            a->keys_struck &= ~(1u << key);
            qd_pool_note_off(&a->pool, a->key_track[key], a->key_note[key]);
        }
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
    switch (button) {
    case QD_B_PLAY:
        if (a->playing)
            qd_app_stop(a);
        else
            qd_app_play(a);
        break;
    case QD_B_KEYS:
        a->screen = a->screen == QD_SCR_KEYS ? QD_SCR_IDLE : QD_SCR_KEYS;
        break;
    case QD_B_HOME:
        a->screen = QD_SCR_IDLE;
        break;
    case QD_B_OCTDN:
    case QD_B_OCTUP:
        if ((a->buttons_held >> QD_B_OCTDN) & (a->buttons_held >> QD_B_OCTUP) & 1u) {
            qd_pool_kill_all(&a->pool);                 /* both: PANIC (PLAN.md §8) */
        } else {
            const int o = a->oct + (button == QD_B_OCTUP ? 1 : -1);
            a->oct = o < QD_OCT_MIN ? QD_OCT_MIN : o > QD_OCT_MAX ? QD_OCT_MAX : o;
        }
        break;
    default:
        break;
    }
}

void qd_app_knob(qd_app_t *a, int knob, int detents)
{
    if (!detents)
        return;
    if (knob == QD_K_SELECT || knob == QD_K_1) {        /* SELECT turns what KNOB 1 turns: the tempo */
        qd_app_tempo(a, a->bpm + detents);
    } else if (knob == QD_K_PRESETS) {                  /* the track's sound: INIT and the 16 (sounding voices keep
                                                         * theirs) */
        qd_track_t *t = &a->model.t[a->sel];
        qd_track_load_preset(t, (int)t->preset + detents);
        a->picker = 1;
        a->picker_ms = a->ms;
        a->picker_dir = detents > 0 ? 1 : -1;
    }
}

void qd_app_master(qd_app_t *a, int pot)
{
    a->master_pot = pot < 0 ? 0 : pot > 1023 ? 1023 : pot;
    qd_fx_master_pot(a->master_pot);
}

/* MIDI in: notes on channels 1..8 play tracks 1..8; all notes off / all sound off release the channel's track */
void qd_app_midi(qd_app_t *a, int status, int d1, int d2)
{
    const int ch = status & 15, kind = status & 0xF0;
    if (ch >= QD_NTRACK)
        return;
    d1 &= 127;
    d2 &= 127;
    if (kind == 0x90 && d2)
        qd_note_on(a, ch, d1, d2);
    else if (kind == 0x80 || kind == 0x90)
        qd_pool_note_off(&a->pool, ch, d1);
    else if (kind == 0xB0 && (d1 == 120 || d1 == 123))
        qd_pool_release_all(&a->pool, ch);
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
        a->sway += QD_TAU * (double)a->bpm / 60000.0;   /* a bob a beat, at the tempo */
        if (a->sway >= QD_TAU)
            a->sway -= QD_TAU;
    }
    if (a->amp < amp_to)
        a->amp = fminf(amp_to, a->amp + (1.f - QD_SWAY_AMP) / QD_AMP_MS);
    else if (a->amp > amp_to)
        a->amp = fmaxf(amp_to, a->amp - (1.f - QD_SWAY_AMP) / QD_AMP_MS);
    if (a->picker && a->ms - a->picker_ms >= QD_PICKER_MS + QD_PICKER_FADE_MS)
        a->picker = 0;
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
    a->ms = ms - n;
    while (n--) {
        a->ms++;
        qd_app_ms(a);
    }
    a->ms = ms;
}

/* ---------------------------------------------------------------- out --- */
/* the notes the selected track sounds (keys held, MIDI notes held, notes waiting for a voice) */
static void qd_app_sounding(const qd_app_t *a, uint8_t *notes)
{
    int i;
    memset(notes, 0, 128);
    for (i = 0; i < a->pool.nvoice; i++) {
        const qd_pool_voice_t *v = &a->pool.v[i];
        if (v->q.active && !v->q.kill && v->q.gate && v->track == a->sel)
            notes[v->q.note & 127] = 1;
        if (v->pend && !v->pend_off && v->pend_track == a->sel)
            notes[v->pend_note & 127] = 1;
    }
}

static void qd_app_draw_keys(const qd_app_t *a)
{
    const qd_track_t *t = &a->model.t[a->sel];
    ui_keys_t v;
    int i;
    memset(&v, 0, sizeof v);
    v.track = a->sel + 1;
    v.name = t->name;
    v.preset = t->preset;
    v.sound = qd_track_sound_name(t);
    v.edited = t->edited;
    v.nvoice = a->pool.nvoice < UI_NVOICE_DOTS ? a->pool.nvoice : UI_NVOICE_DOTS;
    for (i = 0; i < v.nvoice; i++) {
        const qd_pool_voice_t *pv = &a->pool.v[i];
        if (qd_pool_used(&a->pool, i))
            v.voice[i] = (uint8_t)((pv->pend ? pv->pend_track : pv->track) == a->sel ? 2 : 1);
    }
    qd_app_sounding(a, v.sounding);
    v.octave = a->oct;
    ui_keys_draw(&v);
}

static void qd_app_draw_picker(const qd_app_t *a)
{
    const qd_track_t *t = &a->model.t[a->sel];
    const float d = (float)(a->ms - a->picker_ms);
    ui_picker_t v;
    v.track = a->sel + 1;
    v.name = t->name;
    v.k = t->preset;
    v.n = QD_QUAD_NPRESET;
    v.cur = qd_quad_preset_name(v.k);
    v.prev = v.k > 0 ? qd_quad_preset_name(v.k - 1) : 0;
    v.next = v.k + 1 < v.n ? qd_quad_preset_name(v.k + 1) : 0;
    v.roll = d >= QD_ROLL_MS ? 0.f : 1.f - d / QD_ROLL_MS;
    v.dir = a->picker_dir;
    v.reel = (float)fmod((double)a->ms * 0.0025, QD_TAU);
    ui_picker_draw(&v);
}

void qd_app_frame(qd_app_t *a, uint16_t *lcd_be)
{
    qd_begin();
    if (a->screen == QD_SCR_KEYS) {
        qd_app_draw_keys(a);
    } else {
        ui_idle_t v;
        memset(&v, 0, sizeof v);
        memcpy(v.pattern, "A03", 4);
        v.bpm = a->bpm;
        v.playing = a->playing;
        v.turn = a->turn;
        v.phase = (float)(a->playing ? fmod(a->beats * QD_TAU, 2.0 * QD_TAU) : a->sway);
        v.amp = a->amp;
        ui_idle_draw(&v);
    }
    if (a->picker) {                                    /* the takeover, then 120 ms back */
        const uint32_t d = a->ms - a->picker_ms;
        const float al = d < QD_PICKER_MS ? 1.f : 1.f - (float)(d - QD_PICKER_MS) / (float)QD_PICKER_FADE_MS;
        if (al > 0.f) {
            qd_save();
            qd_alpha(al);
            qd_fill_rect(0.f, 0.f, (float)QD_W, (float)QD_H, UI_BG);
            qd_app_draw_picker(a);
            qd_restore();
        }
    }
    qd_end(lcd_be);
}

void qd_app_leds(const qd_app_t *a, qd_leds_t *out)
{
    uint8_t notes[128];
    int i;
    memset(out, 0, sizeof *out);
    qd_app_sounding(a, notes);
    for (i = 0; i < QD_NKEY; i++) {
        const int n = QD_NOTE0 + i + 12 * a->oct;
        if (((a->keys_held >> i) & 1u) || (n >= 0 && n < 128 && notes[n]))
            out->key[i] = QD_LED_LIT;                   /* held, or its note sounding on the track */
        else if (a->screen == QD_SCR_KEYS && (QD_NOTE0 + i) % 12 == 0)
            out->key[i] = QD_LED_FAINT;                 /* KEYS: the C keys faint (PLAN.md §8 LEDs) */
    }
    if ((a->buttons_held >> QD_B_TRACKS) & 1u) {        /* TRACKS held: the selected track's white key dim */
        const int k = QD_WHITE[a->sel];
        if (out->key[k] < QD_LED_DIM)
            out->key[k] = QD_LED_DIM;
    }
    for (i = 0; i < QD_NBTN; i++)
        out->button[i] = (a->buttons_held >> i) & 1u ? QD_LED_LIT : QD_LED_OFF;
    if (a->screen == QD_SCR_KEYS)
        out->button[QD_B_KEYS] = QD_LED_LIT;
    if (a->playing) {
        const double into_beat_ms = (a->beats - floor(a->beats)) * 60000.0 / (double)a->bpm;
        out->play_green = into_beat_ms < (double)QD_BEAT_LED_MS ? QD_LED_LIT : QD_LED_OFF;
    }
}

/* one control block: the voices (the pool, per voice into the mix and the sends), then ChoralRoot's effects and
 * Output Clean, as 16-bit stereo */
static void qd_app_block(qd_app_t *a, int16_t *out)
{
    qd_quad_owner_t *ow[QD_NTRACK];
    qd_mix_track_t mt[QD_NTRACK];
    int32_t l[QD_MIX_CTL], r[QD_MIX_CTL], sc[QD_MIX_CTL], sd[QD_MIX_CTL], sr[QD_MIX_CTL], o[2 * QD_MIX_CTL];
    int i;
    for (i = 0; i < QD_NTRACK; i++) {
        ow[i] = &a->model.t[i].owner;
        mt[i] = a->model.t[i].mix;
    }
    qd_mix_block(&a->mix, &a->pool, ow, mt, QD_NTRACK, l, r, sc, sd, sr);
    qd_fx_block(sc, sd, sr, l, r, o);
    for (i = 0; i < 2 * QD_MIX_CTL; i++)
        out[i] = (int16_t)(o[i] > 32767 ? 32767 : o[i] < -32768 ? -32768 : o[i]);
}

void qd_app_audio(qd_app_t *a, int16_t *stereo, uint32_t frames)
{
    while (frames) {
        uint32_t n;
        if (a->ififo >= a->nfifo) {
            qd_app_block(a, a->fifo);
            a->nfifo = QD_MIX_CTL;
            a->ififo = 0;
        }
        n = (uint32_t)(a->nfifo - a->ififo);
        n = n < frames ? n : frames;
        memcpy(stereo, a->fifo + 2 * a->ififo, n * 2u * sizeof *stereo);
        stereo += 2u * n;
        a->ififo += (int)n;
        frames -= n;
    }
}
