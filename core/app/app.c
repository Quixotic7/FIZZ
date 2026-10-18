/* SPDX-License-Identifier: GPL-3.0-only */
/* QUADRILLE's app (app.h): the clock, the transport and the idle screen's dance, the sound (the sequencer and the keys
 * into the tracks' FM TONE voices, the mix, ChoralRoot's effects), MIDI in. The panel's input, the frame and the LEDs
 * are the controller's (ctl.h). Deterministic: time is the ticks' milliseconds and the audio's samples only. */
#include <math.h>
#include <string.h>

#include "app.h"
#include "ui.h"
#include "../seq/demo.h"
#include "../voice/fx/qd_fx.h"

#define QD_TAU 6.283185307179586
#define QD_SWAY_AMP 0.55f              /* stopped: the sway (a bob a beat at the tempo) smaller than the dance */
#define QD_AMP_MS 300.f                /* the motion's size eases between the two over this long */
#define QD_SETTLE_MS 250.f             /* stopped mid-turn: the quarter turn finishes in this long */

/* the tempo the sound runs at (FM TONE's synced LFOs and DELAYs, the synced delay) */
void qd_app_tempo(qd_app_t *a, int bpm)
{
    a->bpm = bpm < QD_BPM_MIN ? QD_BPM_MIN : bpm > QD_BPM_MAX ? QD_BPM_MAX : bpm;
    qd_model_tempo(&a->model, a->bpm);
    qd_fx_tempo(a->bpm);
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
    qd_project_init(&a->proj);
    qd_demo_load(&a->proj);
    qd_seq_init(&a->seq, &a->proj, &a->model);
    qd_player_init(&a->player, &a->seq, &a->pool, &a->model);
    qd_seq_select(&a->seq, QD_PAT_START, 0);
    qd_app_tempo(a, qd_seq_bpm(&a->seq));
    qd_ctl_init(a);
}

/* ---------------------------------------------------------------- the transport and the dance --- */
static void qd_dance_play(qd_app_t *a)
{
    a->playing = 1;
    a->beats = 0.0;                    /* the transport starts at the top of the bar */
    a->turn_base = a->turn_target;     /* (a whole number: where the last stop settled) */
    a->turn = a->turn_base;
}

static void qd_dance_stop(qd_app_t *a)
{
    a->playing = 0;
    a->turn_target = ceilf(a->turn - 1e-4f);   /* mid-turn: the turn finishes, then the dancers rest */
    a->sway = fmod(a->beats * QD_TAU, QD_TAU); /* the sway goes on from the dance's pose */
    a->beats = 0.0;
}

void qd_app_play(qd_app_t *a)
{
    qd_seq_seed(&a->seq, 0x5155414452494C4Cull ^ a->ms);   /* (the rolls differ each PLAY; a run is still
                                                             * determined by its input) */
    qd_seq_play(&a->seq);
    qd_dance_play(a);
}

void qd_app_stop(qd_app_t *a)
{
    qd_player_stop(&a->player);
    qd_dance_stop(a);
}

/* ---------------------------------------------------------------- the keybed's notes --- */
void qd_app_note_on(qd_app_t *a, int key, int note, int vel)
{
    if (key < 0 || key >= QD_NKEY || note < 0 || note > 127)
        return;
    if ((a->keys_struck >> key) & 1u)                   /* (struck already: its note released first) */
        qd_app_note_off(a, key);
    a->key_note[key] = (uint8_t)note;
    a->key_track[key] = (uint8_t)a->sel;
    a->keys_struck |= 1u << key;
    qd_player_key_on(&a->player, a->sel, note, vel);
}

void qd_app_note_off(qd_app_t *a, int key)
{
    if (key < 0 || key >= QD_NKEY || !((a->keys_struck >> key) & 1u))
        return;
    a->keys_struck &= ~(1u << key);                     /* the note it struck (the octave or the track may have
                                                         * changed since) */
    qd_player_key_off(&a->player, a->key_track[key], a->key_note[key]);
}

/* ---------------------------------------------------------------- input: the controller's --- */
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
    qd_ctl_key(a, key, down, a->key_vel[key]);
    if (!down)
        qd_app_note_off(a, key);                        /* whatever the controller did, a key's note ends with it */
}

void qd_app_button(qd_app_t *a, int button, int down)
{
    if (button < 0 || button >= QD_NBTN)
        return;
    if (down)
        a->buttons_held |= 1u << button;
    else
        a->buttons_held &= ~(1u << button);
    qd_ctl_button(a, button, down);
}

void qd_app_knob(qd_app_t *a, int knob, int detents)
{
    if (detents && knob >= 0 && knob < QD_NKNOB)
        qd_ctl_knob(a, knob, detents);
}

void qd_app_master(qd_app_t *a, int pot)
{
    a->master_pot = pot < 0 ? 0 : pot > 1023 ? 1023 : pot;
    qd_fx_master_pot(a->master_pot);
}

/* MIDI in: notes on channels 1..8 play tracks 1..8 (and, on the selected track's channel, go into a held step or the
 * recording: ctl_rec.c); all notes off / all sound off release the channel's track; the clock (0xF8), START (0xFA),
 * CONTINUE (0xFB), STOP (0xFC) drive the transport */
void qd_app_midi(qd_app_t *a, int status, int d1, int d2)
{
    const int ch = status & 15, kind = status & 0xF0;
    switch (status & 0xFF) {
    case 0xF8:
        qd_seq_midi_clock(&a->seq);
        return;
    case 0xFA:
        qd_seq_midi_start(&a->seq);
        return;
    case 0xFB:
        qd_seq_midi_continue(&a->seq);
        return;
    case 0xFC:
        qd_seq_midi_stop(&a->seq, qd_player_event, &a->player);
        return;
    default:
        break;
    }
    if (status >= 0xF0 || ch >= QD_NTRACK)
        return;
    d1 &= 127;
    d2 &= 127;
    if (kind == 0x90 && d2) {
        qd_player_key_on(&a->player, ch, d1, d2);
        if (ch == qd_ctl_pat(a)->trk[a->sel].chan)
            qd_rec_midi_note(a, d1, 1, d2);
    } else if (kind == 0x80 || kind == 0x90) {
        qd_player_key_off(&a->player, ch, d1);
        if (ch == qd_ctl_pat(a)->trk[a->sel].chan)
            qd_rec_midi_note(a, d1, 0, 0);
    } else if (kind == 0xB0 && (d1 == 120 || d1 == 123)) {
        qd_pool_release_all(&a->pool, ch);
    }
}

/* ---------------------------------------------------------------- time --- */
static void qd_app_ms(qd_app_t *a)     /* one device millisecond */
{
    const float amp_to = a->playing ? 1.f : QD_SWAY_AMP;
    if (a->seq.playing != a->playing) {                 /* (MIDI START / STOP moved the transport) */
        if (a->seq.playing)
            qd_dance_play(a);
        else
            qd_dance_stop(a);
    }
    if (a->playing) {
        double bars, whole;
        a->beats = (double)qd_seq_tick(&a->seq) / (4.0 * QD_TPS);   /* the transport's own position */
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
    qd_ctl_ms(a);
}

void qd_app_tick(qd_app_t *a, uint32_t ms)
{
    uint32_t n;
    if (!a->started || ms < a->ms) {   /* the first tick (or a clock that restarted): the time starts here */
        a->started = 1;
        a->ms = ms;
        a->ui.touch_ms = ms;
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
void qd_app_frame(qd_app_t *a, uint16_t *lcd_be) { qd_view_frame(a, lcd_be); }

void qd_app_leds(const qd_app_t *a, qd_leds_t *out) { qd_ctl_leds(a, out); }

/* one control block: the sequencer's events of this block into the pool, the voices (per voice into the mix and the
 * sends), then ChoralRoot's effects and Output Clean, as 16-bit stereo */
static void qd_app_block(qd_app_t *a, int16_t *out)
{
    qd_quad_owner_t *ow[QD_NTRACK];
    qd_mix_track_t mt[QD_NTRACK];
    int32_t l[QD_MIX_CTL], r[QD_MIX_CTL], sc[QD_MIX_CTL], sd[QD_MIX_CTL], sr[QD_MIX_CTL], o[2 * QD_MIX_CTL];
    int i;
    qd_player_block(&a->player);
    {
        const int b = qd_seq_bpm(&a->seq);              /* (a pattern change or the MIDI clock moved it) */
        if (b != a->bpm)
            qd_app_tempo(a, b);
    }
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
