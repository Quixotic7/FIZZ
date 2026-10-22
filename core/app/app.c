/* SPDX-License-Identifier: GPL-3.0-only */
/* QUADRILLE's app (app.h): the clock, the transport and the idle screen's dance, the sound (the sequencer and the keys
 * into the tracks' FM TONE voices, the mix, ChoralRoot's effects), MIDI in. The panel's input, the frame and the LEDs
 * are the controller's (ctl.h). Deterministic: time is the ticks' milliseconds and the audio's samples only. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "ui.h"
#include "../seq/demo.h"
#include "../voice/fx/qd_fx.h"


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
    a->snd_pat = -1;                                    /* (the first ms: the pattern's sounds into the tracks) */
    qd_app_tempo(a, qd_seq_bpm(&a->seq));
    qd_ctl_init(a);
    qd_undo_init(a);                                    /* P1.5c */
    a->metro_beat = -1;
    a->metro_lvl = QD_METRO_LVL;
}

/* ---------------------------------------------------------------- the transport and the idle screen --- */
static void qd_dance_play(qd_app_t *a)
{
    a->playing = 1;
    a->beats = 0.0;                    /* the transport starts at the top of the bar */
}

static void qd_dance_stop(qd_app_t *a)
{
    a->playing = 0;
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
    qd_undo_input(a, 0);                                /* P1.5c: a gesture (its edits one undo level) */
    qd_ctl_key(a, key, down, a->key_vel[key]);
    if (!down)
        qd_app_note_off(a, key);                        /* whatever the controller did, a key's note ends with it */
    qd_ctl_preview_sync(a);                             /* P1.3d: the held steps sound, stopped */
}

void qd_app_button(qd_app_t *a, int button, int down)
{
    if (button < 0 || button >= QD_NBTN)
        return;
    if (down)
        a->buttons_held |= 1u << button;
    else
        a->buttons_held &= ~(1u << button);
    qd_undo_input(a, 0);
    qd_ctl_button(a, button, down);
    qd_ctl_preview_sync(a);
}

void qd_app_knob(qd_app_t *a, int knob, int detents)
{
    if (detents && knob >= 0 && knob < QD_NKNOB) {
        qd_undo_input(a, 1);
        qd_ctl_knob(a, knob, detents);
        qd_ctl_preview_sync(a);
    }
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
    if ((status & 0xF0) != 0xF0)
        qd_undo_input(a, 0);                            /* (P1.5c: notes into a held step or a take: a gesture) */
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
    {   /* P1.3d: the notes going into a previewed held step sound through its preview, not as keys */
        const int mine = ch == qd_ctl_pat(a)->trk[a->sel].chan, pv = mine && qd_ctl_previewing(a);
        if (kind == 0x90 && d2) {
            if (!pv)
                qd_player_key_on(&a->player, ch, d1, d2);
            if (mine)
                qd_rec_midi_note(a, d1, 1, d2);
        } else if (kind == 0x80 || kind == 0x90) {
            if (!pv)
                qd_player_key_off(&a->player, ch, d1);
            if (mine)
                qd_rec_midi_note(a, d1, 0, 0);
        }
        if (mine)
            qd_ctl_preview_sync(a);
    }
    if (kind == 0xB0 && (d1 == 120 || d1 == 123)) {
        qd_pool_release_all(&a->pool, ch);
    }
}

/* ---------------------------------------------------------------- time --- */
/* P1.5a: the tracks' sounds are the edited pattern's (pattern.h qd_tsound_t): a pattern change brings its sounds into
 * the model's tracks (what the controller edits and the keys play); an edit of a track's sound goes back into it */
static void qd_app_sounds(qd_app_t *a)
{
    qd_pattern_t *P = &a->proj.pat[a->seq.cur];
    int t;
    if (a->snd_pat != a->seq.cur) {
        a->snd_pat = a->seq.cur;
        for (t = 0; t < QD_NTRACK; t++)
            qd_track_from_tsound(&a->model.t[t], &P->snd[t]);
        qd_br_sync(a);                                  /* (P1.5t: browsing: the preview again over the new one) */
        return;
    }
    for (t = 0; t < QD_NTRACK; t++) {
        qd_tsound_t s;
        if (a->ui.latch == QD_LATCH_BROWSE && t == a->ui.br.t)
            continue;                                   /* (P1.5t: the browser's preview is not the pattern's) */
        qd_track_to_tsound(&a->model.t[t], &s);
        if (memcmp(&s, &P->snd[t], sizeof s))
            P->snd[t] = s;
    }
}

static void qd_app_ms(qd_app_t *a)     /* one device millisecond */
{
    qd_app_sounds(a);
    qd_persist_ms(a);                                   /* P1.5a: written by itself */
    if (a->seq.playing != a->playing) {                 /* (MIDI START / STOP moved the transport) */
        if (a->seq.playing)
            qd_dance_play(a);
        else
            qd_dance_stop(a);
    }
    if (a->playing)
        a->beats = (double)qd_seq_tick(&a->seq) / (4.0 * QD_TPS);   /* the transport's own position */
    qd_ctl_ms(a);
    if (!(a->ui.rec && a->seq.playing))                 /* P1.5c: a gesture closes when quiet (a take: as one) */
        qd_undo_ms(a);
    qd_ctl_preview_sync(a);                             /* (a step held at 250 ms, the transport moved) */
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
    if (a->metro && a->playing) {                       /* P1.5c: the metronome: a click on each beat into the mix
                                                         * (its own level, no track's), accented on the bar's first */
        const int64_t beat = (int64_t)floor(a->beats);
        if (beat != a->metro_beat && beat >= 0) {
            a->metro_beat = beat;
            a->metro_left = QD_FS / 40;                 /* (25 ms) */
            a->metro_ph = 0;
            a->metro_acc = (int)(beat % 4) == 0;
        }
        for (i = 0; i < QD_MIX_CTL && a->metro_left; i++, a->metro_left--) {
            const float t = (float)a->metro_ph / (float)QD_FS, env = expf(-t * 180.f);
            const float v = sinf(t * 6.2831853f * (a->metro_acc ? 2600.f : 1700.f)) * env *
                            (a->metro_acc ? 15000.f : 10000.f) * (float)a->metro_lvl / 100.f;
            l[i] += (int32_t)v, r[i] += (int32_t)v;
            a->metro_ph++;
        }
    } else {
        a->metro_left = 0;
        a->metro_beat = -1;
    }
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

/* ---------------------------------------------------------------- P1.5a: saving (persist.h), the store (core/store) */
#include "../store/store.c"
#include "../seq/sounds.h"
#include "persist.c"
#include "undo.c"
#include "fizz_sx.c"                                  /* P1.5r: SysEx (fizz_sx.h) */
