/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's core on iOS and macOS: the host around the core ABI (fm1core.h), the C module CFizzCore that FizzEngine
 * (Swift) imports. One core, one clock.
 *
 * THE CLOCK. Every core entry point runs holding the clock token. fzh_render (the audio thread) and fzh_run (a timer, a
 * test) take it with a try: when it is held elsewhere they do nothing (render: silence). fzh_lock / fzh_unlock take it
 * blocking, for the rare calls from other threads (boot, shutdown, the flash copied with no clock running); never
 * from the audio thread. One device millisecond is FM1VST's engine/device.cpp run_ms: the input queued so far, tick(ms),
 * frame() at ms 0 and every 15 ms or idle(), every 128-frame block due at 44100 Hz into a ring; then MIDI out, the LCD
 * (at most every 16 ms), the LEDs and a requested flash copy published.
 *
 * Any thread: the input (queued, applied at the next device ms), the published output (LCD, LEDs, MIDI out: one
 * reader each), the flash copy's request and take. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FZH_LCD_W = 240, FZH_LCD_H = 240,
    FZH_FS = 44100,        /* the device's sample rate */
    FZH_BLOCK = 128,       /* frames per core audio() block */
    FZH_NKEY = 27,         /* note keys 0 (F3) .. 26 (G5) */
    FZH_NB = 14,           /* button labels, FM1P_B_* order */
    FZH_NROLE = 7,         /* knob roles SELECT ALGO PRESETS K1..K4 */
    FZH_NCOL = 11, FZH_NLINE = 4,
};

/* the core's descriptor (const fm1core_t *, fm1core.h), for whoever wants the raw ABI */
const void *fizzcore_get(void);
/* 1: the core is here (ABI 6) */
int fzh_open(void);
/* 0 id, 1 name, 2 version, 3 source_url, 4 family, 5 license, 6 credits, 7 gated; 100 + i: button label i. NULL none */
const char *fzh_str(uint32_t k);
uint32_t fzh_flash_size(void);

/* ---- the clock token, blocking (never from the audio thread) ---- */
void fzh_lock(void);
void fzh_unlock(void);

/* ---- holding the token (fzh_lock) ---- */
/* power-on (again) from n bytes of flash image (NULL / 0: a fresh, erased flash); a core up is shut down first */
void fzh_boot(const uint8_t *bytes, uint32_t n);
void fzh_shutdown(void);
int fzh_up(void);
uint32_t fzh_ms(void);
void fzh_ui_info(char *buf, uint32_t n);
/* flash_sync, then the whole image into dst (fzh_flash_size bytes); returns the flash's dirty count after the sync */
uint32_t fzh_flash_copy(uint8_t *dst);
/* the panel table after init: btn_id[14] (label -> matrix button), enc_id[7], enc_dir[7], keymap[6 * 11] */
void fzh_panel_table(uint8_t *btn_id, uint8_t *enc_id, int8_t *enc_dir, int8_t *keymap);

/* ---- the clock (try the token: 0 when it is held elsewhere) ---- */
/* n host frames at `rate` (44100: the device's own samples; else resampled), non-interleaved; returns n, 0 busy
 * (silence written) */
uint32_t fzh_render(float *left, float *right, uint32_t n, double rate);
/* n device milliseconds, their audio dropped; returns the ms run (0 busy or not up) */
uint32_t fzh_run(uint32_t n);

/* ---- input (any thread) ---- */
void fzh_button(uint32_t label, int down);              /* FM1P_B_* label */
void fzh_key(uint32_t key, int down, uint32_t velocity);   /* 0..26, velocity 1..127 (0: the core's default) */
void fzh_enc(uint32_t role, int32_t detents);            /* FM1P_E_* role, + clockwise */
void fzh_master(int32_t value);                          /* 0..1023 */
/* one USB-MIDI packet (cable 0: CIN | b0 << 8 | b1 << 16 | b2 << 24); 0: the queue is full */
int fzh_midi_in(uint32_t pkt);
/* drop every input not yet applied, release every key and button */
void fzh_input_clear(void);

/* ---- output (any thread; one reader each) ---- */
/* up to max USB-MIDI packets the firmware sent */
uint32_t fzh_midi_out(uint32_t *pkts, uint32_t max);
/* LCD frames published so far */
uint32_t fzh_lcd_seq(void);
/* the LCD as RGBA8888 (240 x 240 x 4 bytes) when one newer than *seen was published: 1 (and *seen updated), else 0 */
int fzh_lcd_rgba(uint8_t *rgba, uint32_t *seen);
/* the LEDs: led[col * 4 + line] (44 levels 0..255) and PLAY's green; returns the LED publish count */
uint32_t fzh_leds(uint8_t *led44, uint8_t *play_green);
/* the flash's dirty count as of the last device ms */
uint32_t fzh_flash_dirty(void);

/* ---- the flash copied by the clock (flash_sync first) at its next device ms ---- */
void fzh_flash_request(void);
/* 1: the requested copy is ready, copied into dst (and *dirty, the count after its sync); 0: not yet */
int fzh_flash_take(uint8_t *dst, uint32_t *dirty);

/* ---- the clock's cost (the core's own stats() report nothing on this host) ---- */
typedef struct {
    uint64_t blocks;          /* core audio() blocks of 128 frames */
    uint64_t audio_ns;        /* time in audio() */
    uint64_t audio_max_ns;    /* the longest audio() */
    uint64_t ms_run;          /* device ms run */
    uint64_t clock_ns;        /* time in whole device ms (tick, frame / idle, audio, the queues) */
    uint64_t clock_max_ns;    /* the longest device ms */
} fzh_stats_t;
void fzh_stats(fzh_stats_t *out);
void fzh_stats_reset(void);

#ifdef __cplusplus
}
#endif
