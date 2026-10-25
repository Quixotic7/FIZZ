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
void fz_sx_mark(uint32_t ms);          /* a FIZZ frame (semantic or the store's bulk) came now (qd_sx_t.rx_ms) */
uint32_t fz_sx_last(void);             /* .. when the last one did (0 none) */
uint32_t *fz_sx_xs(void);              /* the transfer counters (qd_sx_t.xs: app.c FZ_XS_*) */
int fz_sysex_take(uint8_t *out, uint32_t max);
/* the clock (plat_ms), the sound (16-bit stereo, interleaved, 44.1 kHz, any count of frames) */
void fz_tick(uint32_t ms);
void fz_audio(int16_t *stereo, uint32_t frames);
uint32_t fz_audio_loads(void);                     /* patterns the sound had to load itself (should be 0) */
int fz_shed_one(void);                              /* (the interrupt's) a voice let go: the key up first, else the oldest */
uint32_t fz_state_line(char *line, char *lim);     /* FIZZ's state for the stats, written up to lim (never past it); the
                                                   * worst-case length it can take (bit 31: a field was left out) */
/* the LEDs: a level 0..255 a key (27) and a button (14), PLAY's green */
void fz_leds(uint8_t *key, uint8_t *button, uint8_t *play_green);
/* a frame, drawn in FIZZ's strips (core/qd_config.h QD_STRIP_H rows): out gets each one's first row, its rows and
 * its pixels (RGB565, high byte first, 240 a row), valid until out returns */
typedef void (*fz_strip_fn)(void *ctx, int y0, int rows, const uint16_t *px);
void fz_frame(fz_strip_fn out, void *ctx, int all_strips);   /* (P2.7: the strips fz_frame_changed found changed, or all) */
/* what the frame rate follows: FIZZ's mode (ctl.h QD_MODE_*: 0 the idle screen) and its transport */
void fz_ui_state(int *mode, int *playing);
/* the store: written now (before an update rewrites the flash); 1 done. fz_persist_step: a step of its writes (a page
 * while playing, a few stopped, erases only when quiet; the flash's own locks inside), every ~15 ms, between frames,
 * never inside a lock */
int fz_flush(void);
/* (app.c's, for fz_port.c: the platform's watchdog fed, inside a long flow of flash operations) */
void fz_watchdog(void);
/* (P2.7, app.c) around each flash operation: the interrupts off and the LED lines dark until the operation's end
 * (fm1-base's flash call turns the interrupts on at its end; fz_flash_done does if it returned early) */
void fz_flash_quiet(void);
void fz_flash_done(void);
void fz_persist_step(void);
void fz_input_prepare(void);
void fz_tick_locked(uint32_t ms);                  /* (P2.10: qd_app_tick_locked, under the audio lock) */
void fz_tick_free(void);                           /* (.. qd_app_tick_free, without it) */   /* (P2.9: before the lock of fz_key / fz_button / fz_knob / fz_midi_ui) */
/* would the next frame differ from the last drawn? (qd_app_frame_changed: the screen's logic once, nothing drawn) */
int fz_frame_changed(void);
void fz_blips(uint32_t *n, uint32_t *what);         /* (2026-10-25) a menu, HOME a moment, the menu again: how often; the
                                                     * last's menu screen | latch << 8 | mode << 16 | takeover << 20 |
                                                     * its ms (to 255) << 24 */

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

/* (P2.7) the screen's stash: room borrowed from FIZZ's drawing arena for the length of a frame (the fast drawing takes
 * nothing from it during a frame). fz_stash_borrow: up to want bytes, leaving keep bytes free for what a frame's
 * edits borrow; 0 when there is not that much (*got: its size). release: called when a borrow inside the frame finds
 * no room (the arena's release hook), the stash then given back by its owner */
void *fz_stash_borrow(uint32_t want, uint32_t keep, uint32_t *got);
void fz_stash_return(void *p);
void fz_stash_on_release(void (*release)(void));
int fz_test_lend(uint32_t n);                      /* (the host's test: n bytes borrowed as an edit would, given back) */
/* (P2.7) the core's display list (one instant a frame: qd_gfx_rec.c) and the arena: for the screen's stash */
extern uint32_t fz_dl_max, fz_dl_over12k, fz_strips_drawn, fz_strips_kept;
int fz_dl_is_kept(void);
void fz_dl_drop(void);
void fz_dl_counts(uint32_t *recorded, uint32_t *outgrown, uint32_t *max_bytes);
uint32_t fz_arena_free(void);
int32_t fz_bench(int k, int note, int nblocks);    /* (qd_quad_bench) */
extern int fz_bench_shared_changed;
uintptr_t fz_app_addr(void);
uint32_t fz_bench_render(int k, int note, int nblocks);   /* (P2.10: quad_render alone; ticks) */
extern int32_t fz_bench_render_sum;
/* (P2.12, QD_GFX_FB) the 8-bit frame: its rows (the port's), the palette (256 RGB565 big-endian), a frame drawn into it
 * at one instant (*bands: bit k, band k of QD_FB_BAND rows, may have changed; 0: nothing drawn) */
void fz_fb_attach(uint8_t *const rows[240]);
const uint16_t *fz_fb_palette(uint32_t *gen);
int fz_frame_fb(uint32_t *bands);
int fz_frame_fb_begin(uint32_t *bands);            /* (P2.15: the instant taken; then a few bands a step, input between) */
int fz_frame_fb_step(int n);                       /* (bands left: 0 the frame is whole) */
/* (P2.12) the Geek Out pages' figures (the core's qd_app_stats_t, app.h), and the store's sectors in use, in bytes */
typedef struct {                       /* (the platform's figures, as qd_app_stats_t has them: app.h) */
    uint32_t have;                     /* QD_ST_* bits (1 LOAD0, 2 LOAD1, 4 FPS, 8 ISR, 16 LOCK, 32 RAM, 64 STORE, 128 BOOT) */
    uint16_t load_pm[2], fps10;
    uint32_t frame_us, isr_us, isr_budget_us, late, lock_us, ram_used, ram_size, store_used, store_size;
    const char *boot;
} fz_plat_stats_t;
uint32_t fz_stats_fill(const fz_plat_stats_t *f);   /* into qd_app_stats; returns the core's reset count */
void fz_store_use(uint32_t *used, uint32_t *size);
int fz_screen_index(void);                          /* (the screen being drawn: fz_prof.h fz_screens) */
void fz_store_region(uint32_t *off, uint32_t *size);   /* (P2.13: the store's region in the data flash) */
/* (P2.14) the second core's work (fz_port.c fz_audio): jobs taken, run by core 0 instead (it was busy: inline; it was
 * given up: fallbacks), core 0's and core 1's voice time and core 0's wait (ticks) */
typedef struct {
    uint32_t jobs, inline_jobs, fallbacks, wait_max, c0_voices, c1_voices;   /* (voices: summed over the blocks) */
    uint64_t c0_ticks, c1_ticks, wait_ticks, overlap_ticks;   /* (overlap: the effects core 0 ran while core 1 rendered) */
} fz_c1_stats_t;
extern fz_c1_stats_t fz_c1s;
