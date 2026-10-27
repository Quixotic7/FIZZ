/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* QUADRILLE's app (app.h): the clock, the transport and the idle screen's dance, the sound (the sequencer and the keys
 * into the tracks' FM TONE voices, the mix, ChoralRoot's effects), MIDI in. The panel's input, the frame and the LEDs
 * are the controller's (ctl.h). Deterministic: time is the ticks' milliseconds and the audio's samples only. */
#include <math.h>
#include "../ui/fz_math.h"           /* (the drawing's own sinf / expf: the flash diet) */
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "ui.h"
#include "../seq/demo.h"
#include "../voice/fx/qd_fx.h"
#include "midi_map.h"


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
    qd_persist_io(a);                                   /* P2.1: the patterns' backing (paged at the device's sizes) */
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
    a->beat = 0, a->beat_rem = 0;                    /* the transport starts at the top of the bar */
}

static void qd_dance_stop(qd_app_t *a)
{
    a->playing = 0;
    a->beat = 0, a->beat_rem = 0;
}

void qd_app_play(qd_app_t *a)
{
    qd_seq_seed(&a->seq, 0x5155414452494C4Cull ^ (a->seed_fix ? a->seed_fix : a->ms));   /* (the rolls differ each
                                                         * PLAY; a run is still determined by its input) */
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
    a->edit_gen++;                                      /* (P2.6c: the patterns may change: persist.c looks) */
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
    a->edit_gen++;                                      /* (P2.6c: the patterns may change: persist.c looks) */
    if (key < 0 || key >= QD_NKEY || !((a->keys_struck >> key) & 1u))
        return;
    a->keys_struck &= ~(1u << key);                     /* the note it struck (the octave or the track may have
                                                         * changed since) */
    qd_player_key_off(&a->player, a->key_track[key], a->key_note[key]);
}

/* 2026-10-26: the live keys' HOLD ends: every note a key struck and no key holds now released */
void qd_app_hold_release(qd_app_t *a)
{
    int k;
    for (k = 0; k < QD_NKEY; k++)
        if (((a->keys_struck >> k) & 1u) && !((a->keys_held >> k) & 1u))
            qd_app_note_off(a, k);
}

/* ---------------------------------------------------------------- input: the controller's --- */
void qd_app_key(qd_app_t *a, int key, int down, int vel)
{
    a->edit_gen++;                                      /* (P2.6c: the patterns may change: persist.c looks) */
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
    if (!down && !(a->ui.khold && qd_keys_on(a)))
        qd_app_note_off(a, key);                        /* whatever the controller did, a key's note ends with it
                                                         * (2026-10-26: not under the live keys' HOLD) */
    qd_ctl_preview_sync(a);                             /* P1.3d: the held steps sound, stopped */
}

void qd_app_button(qd_app_t *a, int button, int down)
{
    a->edit_gen++;                                      /* (P2.6c: the patterns may change: persist.c looks) */
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

void qd_app_input_prepare(qd_app_t *a) { qd_undo_input(a, 0); }

void qd_app_knob(qd_app_t *a, int knob, int detents)
{
    a->edit_gen++;                                      /* (P2.6c: the patterns may change: persist.c looks) */
    if (detents && knob >= 0 && knob < QD_NKNOB) {
        qd_undo_input(a, 1);
        qd_ctl_knob(a, knob, detents);
        qd_ctl_preview_sync(a);
    }
}

void qd_app_show(qd_app_t *a, int mode, int view)
{
    if (mode < QD_MODE_IDLE || mode > QD_MODE_TRACKS)
        return;
    qd_latch_close(a);
    qd_step_release_all(a);
    a->ui.tk = QD_TK_NONE;
    a->ui.cf_on = 0;
    if (view >= 0 && view < QD_NVIEW)
        a->ui.view = view;
    a->ui.touch_ms = a->ms;                             /* (as a touch: the idle clock from now) */
    qd_ctl_set_mode(a, mode);
}

void qd_app_seed(qd_app_t *a, uint32_t seed) { a->seed_fix = seed; }

void qd_app_master(qd_app_t *a, int pot)
{
    a->master_pot = pot < 0 ? 0 : pot > 1023 ? 1023 : pot;
    qd_fx_master_pot(a->master_pot);
}

/* MIDI in (P1.6g): a channel plays the tracks set to it (the track settings' MIDI CH, the track's pattern's; 1..8 by
 * default) and channel 15 the selected track too (FIZZ MIDI (a): was 16), notes and controllers (MW, BREATH, AT, PB: the
 * tracks' modulation sources; the CCs of the map (midi_map.h) their sounds); on the selected track the notes also go into
 * a held step or the recording (ctl_rec.c); all notes off / all sound off release the channel's tracks; the clock
 * (0xF8), START (0xFA), CONTINUE (0xFB), STOP (0xFC) drive the transport. Channel 16 never plays a track: it is the
 * remote's (research/FIZZ-MIDI-PLAN.md §3; a track's MIDI CH stored as 16 stays, and plays nothing) */
static int midi_tracks(const qd_app_t *a, int ch)       /* the tracks channel ch plays (bit t) */
{
    int t, m = 0;
    if (ch == QD_MIDI_REMOTE)
        return 0;
    for (t = 0; t < QD_NTRACK; t++)
        if (qd_seq_track_pat(&a->seq, t)->trk[t].chan == ch)
            m |= 1 << t;
    if (ch == QD_MIDI_SEL)
        m |= 1 << a->sel;
    return m;
}

/* P2.4: in two parts. qd_app_midi_play, audio-safe (the clock and the transport, the notes to the player, the
 * controllers; nothing of the UI: no undo, no recording, no preview), so the port can play a note from its interrupt
 * at once; qd_app_midi_ui, the same message's UI side (a gesture's undo, the note into a held step or a take, the
 * preview), from the main loop after it. qd_app_midi is the two in a row, as it always was. P2.4: the audio side reads
 * nothing from the store (pattern.h nopage) */
static void midi_play(qd_app_t *a, int status, int d1, int d2);
void qd_app_midi_play(qd_app_t *a, int status, int d1, int d2)
{
    a->proj.nopage++;
    midi_play(a, status, d1, d2);
    a->proj.nopage--;
}
static void midi_play(qd_app_t *a, int status, int d1, int d2)
{
    const int ch = status & 15, kind = status & 0xF0;
    int m, t;
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
    if (status >= 0xF0 || ch == QD_MIDI_REMOTE)        /* (the remote's: the UI side's alone) */
        return;
    d1 &= 127;
    d2 &= 127;
    if (kind == 0x90 && d2) {                           /* a note: the channel's tracks now (its OFF goes to them) */
        m = midi_tracks(a, ch);
        if (a->midi_on[ch][d1])                         /* (struck again: the last one's released first) */
            for (t = 0; t < QD_NTRACK; t++)
                if ((a->midi_on[ch][d1] >> t) & 1)
                    qd_player_key_off(&a->player, t, d1);
        a->midi_on[ch][d1] = (uint8_t)m;
    } else if (kind == 0x80 || kind == 0x90) {
        m = a->midi_on[ch][d1];
        a->midi_on[ch][d1] = 0;
    } else {
        m = midi_tracks(a, ch);
    }
    {   /* P1.3d: the notes going into a previewed held step sound through its preview, not as keys */
        const int mine = (m >> a->sel) & 1, pv = mine && qd_ctl_previewing(a);
        if (kind == 0x90 && d2) {
            for (t = 0; t < QD_NTRACK; t++)
                if (((m >> t) & 1) && !(pv && t == a->sel))
                    qd_player_key_on(&a->player, t, d1, d2);
        } else if (kind == 0x80 || kind == 0x90) {
            for (t = 0; t < QD_NTRACK; t++)
                if (((m >> t) & 1) && !(pv && t == a->sel))
                    qd_player_key_off(&a->player, t, d1);
        }
    }
    for (t = 0; t < QD_NTRACK; t++) {
        qd_quad_owner_t *o = &a->model.t[t].owner;
        if (!((m >> t) & 1))
            continue;
        if (kind == 0xB0 && (d1 == 120 || d1 == 123))
            qd_player_flush_track(&a->player, t);       /* (P1.6c: its groups' notes too) */
        else if (kind == 0xB0 && d1 == 1)               /* E4: the modulation slots' sources: MOD WHEEL, BREATH, */
            o->mw = (uint8_t)d2;
        else if (kind == 0xB0 && d1 == 2)
            o->br = (uint8_t)d2;
        else if (kind == 0xD0)                          /* .. channel pressure, pitch bend */
            o->at = (uint8_t)d1;
        else if (kind == 0xE0)
            o->pb = (int16_t)((d2 << 7 | d1) - 8192);
    }
    if (kind == 0xB0 && (d1 == 120 || d1 == 123))
        memset(a->midi_on[ch], 0, sizeof a->midi_on[ch]);
}

void qd_app_midi_ui(qd_app_t *a, int status, int d1, int d2)
{
    const int ch = status & 15, kind = status & 0xF0;
    int mine;
    a->edit_gen++;                                      /* (P2.6c) */
    if (status >= 0xF0)
        return;
    if (ch == QD_MIDI_REMOTE) {                         /* FIZZ MIDI (b): the remote, the panel's input (each call its
                                                         * own gesture, as the panel's) */
        qd_midi_remote(a, kind, d1 & 127, d2 & 127);
        return;
    }
    d1 &= 127;
    d2 &= 127;
    qd_undo_input(a, kind == 0xB0 && qd_midi_cc_id(d1) >= 0);   /* (P1.5c: notes into a held step or a take: a gesture;
                                                         * FIZZ MIDI (a): a mapped CC as a knob's detent) */
    if (kind == 0x90 && d2) {                           /* (the selected track's: kept with the note, for its OFF) */
        mine = (midi_tracks(a, ch) >> a->sel) & 1;
        if (mine)
            a->midi_sel[ch][d1 >> 3] |= (uint8_t)(1u << (d1 & 7));
        else
            a->midi_sel[ch][d1 >> 3] &= (uint8_t)~(1u << (d1 & 7));
        if (mine)
            qd_rec_midi_note(a, d1, 1, d2);
    } else if (kind == 0x80 || kind == 0x90) {
        mine = (a->midi_sel[ch][d1 >> 3] >> (d1 & 7)) & 1;
        a->midi_sel[ch][d1 >> 3] &= (uint8_t)~(1u << (d1 & 7));
        if (mine)
            qd_rec_midi_note(a, d1, 0, 0);
    } else {
        const int m = midi_tracks(a, ch);
        mine = (m >> a->sel) & 1;
        if (kind == 0xB0 && (d1 == 120 || d1 == 123))
            memset(a->midi_sel[ch], 0, sizeof a->midi_sel[ch]);
        else if (kind == 0xB0)                          /* FIZZ MIDI (a): the CC map, the tracks' sounds (here, never in
                                                         * the audio side) */
            qd_midi_cc_in(a, m, d1, d2);
    }
    if (mine)
        qd_ctl_preview_sync(a);
}

void qd_app_midi(qd_app_t *a, int status, int d1, int d2)
{
    qd_app_midi_play(a, status, d1, d2);                /* (the player changes no pattern: the gesture's undo after it
                                                         * keeps what it kept before it) */
    qd_app_midi_ui(a, status, d1, d2);
}

/* ---------------------------------------------------------------- time --- */
/* P1.5a: the tracks' sounds are the edited pattern's (pattern.h qd_tsound_t): a pattern change brings its sounds into
 * the model's tracks (what the controller edits and the keys play); an edit of a track's sound goes back into it */
static void qd_app_sounds(qd_app_t *a)
{
    qd_pattern_t *P;
    int t;
    P = qd_proj_pat(&a->proj, a->seq.cur);
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
            P->snd[t] = s, a->edit_gen++;
    }
}

/* P2.6c: a millisecond in three parts. ms_lock1 and ms_lock2 touch what the audio side reads (the patterns' slots and
 * the sound cache, the patterns' track sounds, the transport, the voices through the controller and the preview): the
 * audio lock. ms_free touches what only the main loop uses (the persist looks: hashes of what is in RAM, never reading
 * anything in; the store's job queue; SysEx's labels): no lock */
/* 2026-10-27: the locked millisecond's parts timed (the stats' lkp_*: the port sets the clock, 24-bit ticks) */
uint32_t qd_lkp_max[6];
uint32_t (*qd_lkp_clock)(void);
#define LKP_T0 uint32_t lk_t = qd_lkp_clock ? qd_lkp_clock() : 0u
#define LKP(i)                                                                                                   \
    do {                                                                                                         \
        if (qd_lkp_clock) {                                                                                      \
            const uint32_t lk_n = qd_lkp_clock(), lk_d = (lk_n - lk_t) & 0xFFFFFFu;                              \
            if (lk_d > qd_lkp_max[i])                                                                            \
                qd_lkp_max[i] = lk_d;                                                                            \
            lk_t = lk_n;                                                                                         \
        }                                                                                                        \
    } while (0)
static void ms_lock1(qd_app_t *a)
{
    LKP_T0;
    qd_persist_prefetch(a);                             /* P2.1: the sequencer's patterns in RAM before it asks */
    LKP(0);
    if (a->ui.pat_wait >= 0 && (a->seq.playing || qd_proj_resident(&a->proj, a->ui.pat_wait))) {
        const int p = a->ui.pat_wait;                   /* (2026-10-26: the pattern chosen while stopped: in RAM now) */
        a->ui.pat_wait = -1;
        qd_seq_select_at(&a->seq, p, a->proj.launch);
        if (!a->seq.playing)
            qd_app_tempo(a, qd_seq_bpm(&a->seq));
    }
    LKP(1);
    qd_app_sounds(a);
    LKP(2);
}
static void ms_free(qd_app_t *a)
{
    qd_proj_fetch_work(&a->proj);                       /* 2026-10-26: a pattern read in outside the lock (the pop) */
    qd_persist_snd_work(a);                             /* .. and a pool sound the next steps lock */
    qd_persist_ms(a);                                   /* P1.5a: written by itself */
    qd_sx_ms(a);                                        /* P1.6l: a subscribed page's LABELS, when they change */
}
static void ms_lock2(qd_app_t *a)
{
    LKP_T0;
    if (a->ui.rec)
        a->edit_gen++;                                  /* (P2.6c: a take writes as time goes) */
    if (a->seq.playing != a->playing) {                 /* (MIDI START / STOP moved the transport) */
        if (a->seq.playing)
            qd_dance_play(a);
        else
            qd_dance_stop(a);
    }
    if (a->playing) {                                   /* the transport's own position, in beats (floor) */
        int32_t r;
        a->beat = qd_divmod64(qd_seq_tick(&a->seq), QD_BEAT_TICKS, &r);
        if (r < 0)
            a->beat--, r += QD_BEAT_TICKS;
        a->beat_rem = r;
    }
    qd_player_pattern_ms(&a->player);                   /* (2026-10-26: a pattern change: the MIDI FX's notes end) */
    qd_ctl_ms(a);
    LKP(3);
    qd_ctl_preview_sync(a);                             /* (a step held at 250 ms, the transport moved) */
    LKP(4);
}
/* P2.10: a gesture closes when quiet (a take: as one). Its level is the pattern's (and the MIDI FX's, the slots')
 * difference from the gesture's before: it reads the model and writes only the undo log, which the audio side never
 * reads, so no lock (on the unit ~2.5 ms: the pattern unpacked and compared twice). After ms_lock2, as it was */
static void ms_free2(qd_app_t *a)
{
    if (!(a->ui.rec && a->seq.playing))
        qd_undo_ms(a);
}
static void qd_app_ms(qd_app_t *a)     /* one device millisecond */
{
    ms_lock1(a);
    ms_free(a);
    ms_lock2(a);
    ms_free2(a);
}

/* P2.6c: the port's two halves of qd_app_tick: the locked part once up to ms now (coalesced: its timers compare
 * times), then (the lock released) the free part a ms at a time for the same ms, each with a->ms as it was then (the
 * persist looks' schedule is by the ms) */
/* 2026-10-26, CHAIN: each effect's share of the wet made before it: the tracks' sends, weighted by their level into
 * the chain (computed here at the control rate, on flash: the audio path's RAM code is full) */
static void chain_shares(qd_app_t *a)
{
    const int chain = qd_fx_get(QD_FXP_RMODE);
    int32_t g[3] = {0, 0, 0}, wsum = 0;
    int i, k;
    for (i = 0; i < QD_NTRACK; i++) {                   /* each track's gains (mix.h g_lvl, g_send): its DRY / WET */
        qd_mix_track_t *mt = &a->model.t[i].mix;
        const int32_t td = mt->dry;
        if (!chain) {                                   /* PARALLEL: the middle both full (bit for bit as before) */
            const int32_t gd = td >= 64 ? 4096 : td << 6, gw = td <= 64 ? 4096 : (127 - td) * 65;
            mt->g_lvl = gd;
            mt->g_send[0] = mt->g_send[1] = mt->g_send[2] = gw;
        } else {                                        /* CHAIN: of what the track feeds the chain, send k's share
                                                         * into effect k in the order, the rest on; the remainder dry */
            const int32_t send[3] = {mt->chorus, mt->delay, mt->reverb}, feed = 127 - td;
            int32_t r = 4096;
            for (k = 0; k < 4; k++) {
                const int b = qd_fx_route_block(k);
                if (b < 0 || b > 2)
                    continue;
                mt->g_send[b] = feed * r / 127;
                r = r * (127 - send[b]) / 127;
            }
            mt->g_lvl = (td * 4096 + feed * r) / 127;
        }
        if (!a->ui.boost) {                             /* 2026-10-26 (the owner: the engine clipped inside): 6 dB
                                                         * under the old level, dry and sends alike; SETTINGS' BOOST
                                                         * brings it back */
            mt->g_lvl >>= 1;
            mt->g_send[0] >>= 1, mt->g_send[1] >>= 1, mt->g_send[2] >>= 1;
        }
        if (chain) {
            const int32_t w = (int32_t)mt->level * (127 - td);
            g[0] += w * mt->chorus, g[1] += w * mt->delay, g[2] += w * mt->reverb;
            wsum += w;
        }
    }
    if (chain) {
        for (k = 0; k < 3; k++)                         /* (32-bit: g / 127 <= wsum <= 96774, then << 12) */
            g[k] = wsum ? ((g[k] / 127) << 12) / wsum : 0;
        qd_fx_chain_g(g);
    }
}

void qd_app_tick_locked(qd_app_t *a, uint32_t ms)
{
    uint32_t n;
    LKP_T0;
    chain_shares(a);
    LKP(5);
    if (!a->started || ms < a->ms) {
        a->started = 1;
        a->ms = a->free_ms = ms;
        a->ui.touch_ms = ms;
        return;
    }
    n = ms - a->ms;
    if (n > 60000u)
        n = 60000u;
    a->ms = ms - n;
    if (a->free_ms < a->ms)
        a->free_ms = a->ms;
    if (n) {                                            /* (P2.6c: once for every ms since: its timers compare times,
                                                         * none counts calls; a timer fires up to n ms late) */
        a->ms = ms;
        ms_lock1(a);
        ms_lock2(a);
    }
    a->ms = ms;
}
#define QD_FREE_CATCHUP_MS 64u
void qd_app_tick_free(qd_app_t *a)
{
    const uint32_t ms = a->ms;
    if (ms - a->free_ms > QD_FREE_CATCHUP_MS)           /* (P2.6f: a stalled main loop catches up a bounded slice: the
                                                         * persist looks and the labels' timers compare times, they
                                                         * lose nothing but a delay) */
        a->free_ms = ms - QD_FREE_CATCHUP_MS;
    while (a->free_ms != ms) {
        a->ms = ++a->free_ms;
        ms_free(a);
    }
    a->ms = ms;
    ms_free2(a);                                        /* (P2.10: once, at the locked part's ms: its quiet compares
                                                         * times) */
}

void qd_app_tick(qd_app_t *a, uint32_t ms)
{
    uint32_t n;
    if (!a->started || ms < a->ms) {   /* the first tick (or a clock that restarted): the time starts here */
        a->started = 1;
        a->ms = a->free_ms = ms;
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
    a->ms = a->free_ms = ms;
}

/* ---------------------------------------------------------------- out --- */
void qd_app_frame(qd_app_t *a, uint16_t *lcd_be) { qd_view_frame(a, lcd_be); }

int qd_app_frame_strips_changed(qd_app_t *a, uint32_t *mask)
{
    const int n = (QD_H + QD_STRIP_H - 1) / QD_STRIP_H;
    const uint32_t all = n >= 32 ? 0xFFFFFFFFu : (1u << n) - 1u;
#ifdef QD_DEVICE
    uint32_t sh[QD_H / QD_STRIP_DEV + 1];
    uint32_t m = 0;
    int k;
    if (qd_view_frame_hashes(a, sh, n)) {               /* (P2.6: the frame recorded: its strips' hashes) */
        for (k = 0; k < n; k++)
            if (!a->frame_hash_ok || sh[k] != a->strip_hash[k])
                m |= 1u << k;
        memcpy(a->strip_hash, sh, (size_t)n * sizeof sh[0]);
        a->frame_hash = sh[0] ^ (uint32_t)n;
    } else {                                            /* (no list: the whole frame's hash, P2.4's) */
        m = !a->frame_hash_ok || sh[0] != a->frame_hash ? all : 0u;
        a->frame_hash = sh[0];
        memset(a->strip_hash, 0, sizeof a->strip_hash);
    }
    a->frame_hash_ok = 1;
    *mask = m;
    return m != 0;
#else
    (void)a;
    *mask = all;                                        /* (the host's cached layers (the TAP ape) are built while
                                                         * drawing: every frame drawn) */
    return 1;
#endif
}
int qd_app_frame_changed(qd_app_t *a)
{
    uint32_t m;
    return qd_app_frame_strips_changed(a, &m);
}
void qd_app_frame_strips_mask(qd_app_t *a, uint16_t *strip_be, void (*out)(void *ctx, int y0, int rows, const uint16_t *px),
                              void *ctx, uint32_t mask)
{
    qd_view_frame_strips(a, strip_be, out, ctx, mask);
}
void qd_app_frame_strips(qd_app_t *a, uint16_t *strip_be, void (*out)(void *ctx, int y0, int rows, const uint16_t *px),
                         void *ctx)
{
    qd_view_frame_strips(a, strip_be, out, ctx, 0xFFFFFFFFu);
}

void qd_app_leds(const qd_app_t *a, qd_leds_t *out) { qd_ctl_leds(a, out); }
qd_app_stats_t *qd_app_stats(qd_app_t *a) { return &a->stats; }

#ifdef QD_GFX_FB
typedef char qd_fb_band_ok[QD_FB_BAND == QD_STRIP_H ? 1 : -1];
static void fb_out(void *ctx, int y0, int rows, const uint16_t *px) { (void)ctx, (void)y0, (void)rows, (void)px; }
void qd_app_fb_attach(qd_app_t *a, uint8_t *const rows[240])
{
    qd_fb_attach(rows);
    a->frame_hash_ok = 0;                               /* (a new frame: every band) */
}
const uint16_t *qd_app_palette(const qd_app_t *a, uint32_t *gen)
{
    (void)a;
    return qd_fb_palette_be(gen);
}
int qd_app_frame_fb_begin(qd_app_t *a, uint32_t *bands)
{
    uint32_t m = 0;
    qd_app_frame_strips_changed(a, &m);                 /* (the instant: the list recorded, the bands' hashes) */
    a->fb_pending = m;
    *bands = m;
    return m != 0;
}
int qd_app_frame_fb_step(qd_app_t *a, int n)
{
    uint32_t sub = 0, m = a->fb_pending;
    int left = 0;
    while (m && n-- > 0) {
        const uint32_t low = m & (0u - m);              /* (the topmost band left) */
        sub |= low, m &= ~low;
    }
    a->fb_pending = m;
    if (sub) {
        qd_view_keep_list = m != 0;                     /* (more to come: the list, the instant, kept) */
        qd_view_frame_strips(a, 0, fb_out, 0, sub);
        qd_view_keep_list = 0;
    }
    for (; m; m &= m - 1u)
        left++;
    return left;
}
uint32_t qd_app_frame_fb_latch(qd_app_t *a, uint32_t *changed)
{
    uint32_t m = 0;
    qd_app_frame_fb_begin(a, &m);
    *changed = m;
    if (!m)
        return 0;
    if (!qd_dl_kept()) {                                /* (no list: drawn here, live, now) */
        qd_app_frame_fb_step(a, 32);
        return 0;
    }
    qd_dl_pin(1);
    a->fb_pending = 0;
    return m;
}
int qd_app_frame_fb_band(int k)
{
    if (k < 0 || k * QD_STRIP_H >= QD_H || !qd_dl_kept())
        return 0;
    qd_begin_strip(k * QD_STRIP_H);
    qd_dl_play();
    return 1;
}
void qd_app_frame_fb_done(qd_app_t *a)
{
    (void)a;
    qd_dl_pin(0);
    qd_dl_drop();
}
int qd_app_frame_fb(qd_app_t *a, uint32_t *bands)
{
    if (qd_app_frame_fb_begin(a, bands))
        qd_app_frame_fb_step(a, 32);                    /* (the changed bands drawn into the frame, from that list) */
    return *bands != 0;
}
#endif

/* one control block: the sequencer's events of this block into the pool, the voices (per voice into the mix and the
 * sends), then ChoralRoot's effects and Output Clean, as 16-bit stereo. P2.6: in three (app.h qd_app_block_*) */
static void owners_of(qd_app_t *a, qd_quad_owner_t **ow)
{
    int i;
    for (i = 0; i < QD_NTRACK; i++)
        ow[i] = &a->model.t[i].owner;
}

void qd_app_block_begin(qd_app_t *a)
{
    qd_quad_owner_t *ow[QD_NTRACK];
    int i;
    a->proj.nopage++;                                   /* (the audio side: pattern.h) */
    qd_player_block(&a->player);
    {
        const int b = qd_seq_bpm(&a->seq);              /* (a pattern change or the MIDI clock moved it) */
        if (b != a->bpm)
            qd_app_tempo(a, b);
    }
    owners_of(a, ow);
    for (i = 0; i < QD_NTRACK; i++) {
        ow[i]->defer_off = 1;                           /* (the voices write nothing of their owner's) */
        a->blk_mt[i] = a->model.t[i].mix;
    }
    a->pool.halves = 1;
    qd_mix_begin(&a->mix, &a->pool, ow, QD_NTRACK);
    a->proj.nopage--;
}

void qd_app_block_voices(qd_app_t *a, int v0, int v1, qd_bus_t *bus)
{
    bus->n = qd_mix_voices(&a->mix, &a->pool, a->blk_mt, QD_NTRACK, v0, v1, bus->l, bus->r, bus->sc, bus->sd, bus->sr);
}

/* the end's own part (P2.6g: a pipelined block's too): the sum, the metronome at its block's beat, the effects at its
 * block's tempo, the output, the scope */
static void block_out(qd_app_t *a, const qd_bus_t *A, const qd_bus_t *B, int64_t beat, int metro, int16_t *out)
{
    int32_t l[QD_MIX_CTL], r[QD_MIX_CTL], sc[QD_MIX_CTL], sd[QD_MIX_CTL], sr[QD_MIX_CTL], o[2 * QD_MIX_CTL];
    int i;
    for (i = 0; i < QD_MIX_CTL; i++) {                  /* A then B (integer sums: either order the same) */
        l[i] = A->l[i] + B->l[i];
        r[i] = A->r[i] + B->r[i];
        sc[i] = A->sc[i] + B->sc[i];
        sd[i] = A->sd[i] + B->sd[i];
        sr[i] = A->sr[i] + B->sr[i];
    }
    if (metro) {                                        /* P1.5c: the metronome: a click on each beat into the mix
                                                         * (its own level, no track's), accented on the bar's first */
        if (beat != a->metro_beat && beat >= 0) {
            a->metro_beat = beat;
            a->metro_left = QD_FS / 40;                 /* (25 ms) */
            a->metro_ph = 0;
            a->metro_acc = (beat & 3) == 0;             /* (P2.6: beat >= 0: no 64-bit remainder) */
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
    if (a->geek.scope_on) {                             /* P2.11: the scope's samples, every other one (L + R) / 2 */
        uint32_t w = a->geek.scope_w;
        for (i = 0; i < QD_MIX_CTL; i += 2)
            a->geek.u.scope[w++ & (QD_SCOPE_N - 1u)] = (int16_t)((out[2 * i] + out[2 * i + 1]) / 2);
        a->geek.scope_w = w;
    }
}
/* 2026-10-27, the Geek Out TRACKS page: the block's voices summed by track, every 8th sample, into the rings; then
 * the next block's tap on or off (the mixer copies each voice's block while vtap is set) */
static void tsc_take(qd_app_t *a)
{
    qd_geek_t *g = &a->geek;
    if (g->tsc_on && a->mix.vtap) {
        uint32_t w = g->tsc_w;
        int t, i, k;
        for (k = 0; k < QD_MIX_CTL; k += 8, w++)
            for (t = 0; t < QD_NTRACK; t++) {
                int32_t s = 0;
                for (i = 0; i < a->pool.nvoice; i++)
                    if (a->pool.v[i].track == t)
                        s += g->u.tk.vblk[i][k] >> 2;   /* (a voice peaks near 884000: >> 2, a chord fits) */
                g->u.tk.tsc[t][w % QD_TSC_N] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
            }
        g->tsc_w = w;
        memset(g->u.tk.vblk, 0, sizeof g->u.tk.vblk);
    }
    a->mix.vtap = g->tsc_on ? g->u.tk.vblk : 0;
}
void qd_app_block_end(qd_app_t *a, const qd_bus_t *A, const qd_bus_t *B, int16_t *out)
{
    tsc_take(a);
    qd_quad_owner_t *ow[QD_NTRACK];
    owners_of(a, ow);
    qd_mix_end(&a->mix, ow, QD_NTRACK, A->n + B->n);
    block_out(a, A, B, a->beat, a->metro && a->playing, out);
}

/* P2.6g: the pipelined block (app.h qd_blk_t) */
void qd_app_block_begin_k(qd_app_t *a, qd_blk_t *k)
{
    qd_app_block_begin(a);
    k->beat = a->beat;
    k->metro = a->metro && a->playing;
    k->bpm = a->bpm;
}
void qd_app_block_publish(qd_app_t *a, const qd_blk_t *k)
{
    qd_quad_owner_t *ow[QD_NTRACK];
    owners_of(a, ow);
    qd_mix_end(&a->mix, ow, QD_NTRACK, k->x.n + k->y.n);
    tsc_take(a);
}
void qd_app_block_out(qd_app_t *a, const qd_blk_t *k, int16_t *out)
{
    qd_fx_tempo_fx(k->bpm);                             /* (the effects at the tempo its begin knew: the next begin may
                                                         * have moved a->bpm) */
    block_out(a, &k->x, &k->y, k->beat, k->metro, out);
}

static void qd_app_block(qd_app_t *a, int16_t *out)
{
    qd_bus_t A, B;                                      /* (1.3 KB of the audio side's stack) */
    const int mid = a->pool.nvoice / 2;
    qd_app_block_begin(a);
    qd_app_block_voices(a, 0, mid, &A);
    qd_app_block_voices(a, mid, a->pool.nvoice, &B);
    qd_app_block_end(a, &A, &B, out);
}

void qd_app_audio(qd_app_t *a, int16_t *stereo, uint32_t frames)
{
    a->proj.nopage++;                                   /* P2.4: the audio side reads nothing in (pattern.h) */
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
    a->proj.nopage--;
}

/* ---------------------------------------------------------------- P1.5a: saving (persist.h), the store (core/store) */
#include "../store/store.c"
#include "../seq/sounds.h"
#include "persist.c"
#include "undo.c"
#include "fizz_sx.c"                                  /* P1.5r: SysEx (fizz_sx.h) */
#include "midi_map.c"                                 /* FIZZ MIDI: the CC map, the remote (midi_map.h) */
