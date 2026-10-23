/* SPDX-License-Identifier: MPL-2.0 */
/* The seam between fm1-base's side of FIZZ (app.c, compiled inside fm1-base's unity build with the platform) and
 * FIZZ's core (fz_port.c and core/, units of their own). Only plain C types cross it: the unity build never sees
 * FIZZ's headers (its C library is felucca-bsp's libc.c; the core's is newlib's, whose size_t differs).
 *
 * Two contexts (P2.4, app.c's header): fz_audio and fz_midi's real-time messages run in the audio interrupt; everything
 * else that changes FIZZ runs on the main loop with the interrupts off around each call (plat_lock); fz_frame and
 * fz_leds, which only read, run on the main loop with them on. */
#pragma once
#include <stdint.h>

/* boot: FIZZ's drawing (its fonts from the data flash on the unit) and the app. 0: its data blob is missing or not
 * this build's (fz_xdata_state), nothing else may be called but the xdata calls below */
int fz_init(const char *version);
/* P2.5: the store, attached after fz_init (not in safe mode). fz_store_foreign: the region's sectors that are neither
 * blank nor FIZZ's when none is FIZZ's (a region another firmware left: erase it first); fz_store_wipe: every non-blank
 * sector erased (the count) */
int fz_store_foreign(void);
int fz_store_wipe(void);
void fz_store_attach(void);
/* the panel, as the FM1VST glue gives it: a key 0..26 (F3..G5) with its velocity 1..127, a button in fm1-base's B_*
 * order (= FIZZ's QD_B_*), knob detents in the EN_* order (= QD_K_*), MASTER 0..1023 */
void fz_key(int key, int down, int vel);
void fz_button(int button, int down);
void fz_knob(int role, int detents);
void fz_master(int pot);
/* MIDI in (P2.5, core/app/app.h): a message's sound and transport (the clock, START .. STOP, notes to the player,
 * controllers: audio-safe, the interrupt's), then its UI part (undo, recording into a held step, the preview: the main
 * loop's, in the same order); a real-time message has no UI part */
void fz_midi_play(int status, int d1, int d2);
void fz_midi_ui(int status, int d1, int d2);
/* SysEx in: one frame's bytes between F0 and F7 (FIZZ's 7D 46 5A ..); its replies, whole frames F0 .. F7 */
void fz_sysex(const uint8_t *f, uint32_t n);
int fz_sysex_peek(void);
int fz_sysex_take(uint8_t *out, uint32_t max);
/* the clock (plat_ms), the sound (16-bit stereo, interleaved, 44.1 kHz, any count of frames) */
void fz_tick(uint32_t ms);
void fz_audio(int16_t *stereo, uint32_t frames);
uint32_t fz_audio_loads(void);                     /* patterns the sound had to load itself (should be 0) */
int fz_shed_one(void);                              /* (the interrupt's) a voice let go: the key up first, else the oldest */
void fz_state_line(char *line);                     /* FIZZ's state for the stats (up to ~400 characters) */
/* the LEDs: a level 0..255 a key (27) and a button (14), PLAY's green */
void fz_leds(uint8_t *key, uint8_t *button, uint8_t *play_green);
/* a frame, drawn in FIZZ's strips (core/qd_config.h QD_STRIP_H rows): out gets each one's first row, its rows and
 * its pixels (RGB565, high byte first, 240 a row), valid until out returns */
typedef void (*fz_strip_fn)(void *ctx, int y0, int rows, const uint16_t *px);
void fz_frame(fz_strip_fn out, void *ctx);
/* what the frame rate follows: FIZZ's mode (ctl.h QD_MODE_*: 0 the idle screen) and its transport */
void fz_ui_state(int *mode, int *playing);
/* the store: written now (before an update rewrites the flash); 1 done. fz_persist_step: a step of its writes (a page
 * while playing, a few stopped, erases only when quiet; the flash's own locks inside), every ~15 ms, between frames,
 * never inside a lock */
int fz_flush(void);
/* (app.c's, for fz_port.c: the platform's watchdog fed, inside a long flow of flash operations) */
void fz_watchdog(void);
void fz_persist_step(void);
/* would the next frame differ from the last drawn? (qd_app_frame_changed: the screen's logic once, nothing drawn) */
int fz_frame_changed(void);

/* the data blob (the fonts and the TAP ape's lines: core/qd_config.h QD_XDATA) in the data flash, at the XIP address
 * the link gave it: its size, and whether the flash holds this build's (1), none (0) or another's (-1). Unit only;
 * the host simulator links the blob into the program (state 1) */
uint32_t fz_xdata_len(void);
uint32_t fz_xdata_crc(void);                       /* the CRC-32 the build stamped (the blob's) */
uint32_t fz_xdata_flash_off(void);                 /* its first byte's flash offset */
int fz_xdata_state(void);

/* a harness's seams (qd_app_show, qd_app_seed): a screen as the panel would leave it, a fixed seed for the rolls; and
 * the host simulator's checks (app_expect): 1 = known */
void fz_show(int mode, int view);
void fz_seed(uint32_t seed);
int fz_expect(const char *what, int *got);
