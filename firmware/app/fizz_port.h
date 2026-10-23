/* SPDX-License-Identifier: MPL-2.0 */
/* The seam between fm1-base's side of FIZZ (app.c, compiled inside fm1-base's unity build with the platform) and
 * FIZZ's core (fz_port.c and core/, units of their own). Only plain C types cross it: the unity build never sees
 * FIZZ's headers (its C library is felucca-bsp's libc.c; the core's is newlib's, whose size_t differs).
 *
 * One context: every call below is made from fm1-base's main loop, never from an interrupt (FIZZ's core is written
 * for one caller, as FM1VST's Standalone drives it: tick, input, frame and audio in turn). The audio interrupt only
 * plays what fz_audio made ahead (app.c's ring). */
#pragma once
#include <stdint.h>

/* boot: FIZZ's drawing (its fonts from the data flash on the unit) and the app. 0: its data blob is missing or not
 * this build's (fz_xdata_state), nothing else may be called but the xdata calls below */
int fz_init(const char *version);
/* the panel, as the FM1VST glue gives it: a key 0..26 (F3..G5) with its velocity 1..127, a button in fm1-base's B_*
 * order (= FIZZ's QD_B_*), knob detents in the EN_* order (= QD_K_*), MASTER 0..1023 */
void fz_key(int key, int down, int vel);
void fz_button(int button, int down);
void fz_knob(int role, int detents);
void fz_master(int pot);
/* MIDI in: a channel message or a real-time byte (status 0xF8 / FA / FB / FC, d1 = d2 = 0) */
void fz_midi(int status, int d1, int d2);
/* SysEx in: one frame's bytes between F0 and F7 (FIZZ's 7D 46 5A ..); its replies, whole frames F0 .. F7 */
void fz_sysex(const uint8_t *f, uint32_t n);
int fz_sysex_peek(void);
int fz_sysex_take(uint8_t *out, uint32_t max);
/* the clock (plat_ms), the sound (16-bit stereo, interleaved, 44.1 kHz, any count of frames) */
void fz_tick(uint32_t ms);
void fz_audio(int16_t *stereo, uint32_t frames);
/* the LEDs: a level 0..255 a key (27) and a button (14), PLAY's green */
void fz_leds(uint8_t *key, uint8_t *button, uint8_t *play_green);
/* a frame, drawn in FIZZ's strips (core/qd_config.h QD_STRIP_H rows): out gets each one's first row, its rows and
 * its pixels (RGB565, high byte first, 240 a row), valid until out returns */
typedef void (*fz_strip_fn)(void *ctx, int y0, int rows, const uint16_t *px);
void fz_frame(fz_strip_fn out, void *ctx);
/* the store: written now (before an update rewrites the flash); 1 done */
int fz_flush(void);

/* the data blob (the fonts and the TAP ape's lines: core/qd_config.h QD_XDATA) in the data flash, at the XIP address
 * the link gave it: its size, and whether the flash holds this build's (1), none (0) or another's (-1). Unit only;
 * the host simulator links the blob into the program (state 1) */
uint32_t fz_xdata_len(void);
uint32_t fz_xdata_crc(void);                       /* the CRC-32 the build stamped (the blob's) */
uint32_t fz_xdata_flash_off(void);                 /* its first byte's flash offset */
int fz_xdata_state(void);

/* the host simulator's checks (app_expect): 1 = known; its view setter (as render_frames sets PIANO ROLL) */
int fz_expect(const char *what, int *got);
void fz_host_view(int view);
