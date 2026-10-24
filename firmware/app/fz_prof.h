/* SPDX-License-Identifier: MPL-2.0 */
/* The drawing's profile on the unit (P2.4): time in plat_ticks24 ticks (24 a microsecond; the sums in 64 bits, as the
 * tick counter wraps every 179 s), since the last reset (fizz_xdata.py stats --reset).
 * Counted by the wrappers tools/gen_units.py puts around functions of FIZZ's drawing (plutovg's rasteriser, its
 * blender, its glyph outlines; the fast drawing's texts and shapes; either's RGB565 conversion of a strip), by app.c (the LCD, the input between strips,
 * the frame, the audio interrupt) and by fz_port.c (a frame's screen). Plain C: the unity build (app.c) and FIZZ's
 * units both include it. A figure includes the audio interrupt's time when it fell inside (isr_in_frame: how much of
 * the frames that was). */
#pragma once
#include <stdint.h>

typedef struct {
    uint64_t raster;          /* plutovg_rasterize: a path's outline into spans (fills, strokes, text) */
    uint64_t blend;           /* plutovg_blend: the spans painted */
    uint64_t glyph;           /* plutovg_font_face_get_glyph_path: a glyph's outline (stb_truetype) */
    uint64_t text;            /* P2.5, the fast drawing: qd_text (a text drawn, its atlas or its outlines) */
    uint64_t shapes;          /* .. the shapes: qd_line .. qd_fill_rect, qd_path_fill / stroke / paint */
    uint64_t convert;         /* qd_end_strip_rows: a strip's ARGB to RGB565 */
    uint64_t lcd;             /* app.c: the rows' hashes, the blit of the changed ones, the wait for the DMA */
    uint64_t input;           /* app.c: the panel, MIDI, the clock between strips and frames (their locks included) */
    uint64_t frame;           /* app.c: whole frames (fz_frame) */
    uint64_t isr_in_frame;    /* the audio interrupt's time inside frames */
    uint64_t isr;             /* the audio interrupt (app_render): FIZZ's sound, MIDI's sound */
    uint64_t a_seq;           /* inside the audio interrupt: qd_player_block (the sequencer, the player, MIDI FX) */
    uint64_t a_voices;        /* .. qd_mix_voices (the voices rendered and mixed) */
    uint64_t a_fx;            /* .. qd_fx_block (chorus, delay, reverb, the output) */
    uint64_t a_begin;         /* P2.8: .. qd_app_block_begin (the sequencer a_seq inside it, the tempo, the mix's begin) */
    uint64_t a_vblk;          /* .. qd_app_block_voices, both halves (a_voices inside them) */
    uint64_t a_end;           /* .. qd_app_block_end (the buses summed, the mix's end, the metronome, a_fx inside it) */
    uint64_t a_out;           /* .. app.c: MASTER's gain and the 24-bit output */
    uint64_t a_midi;          /* .. app.c: MIDI in (fm1-base's queue, qd_app_midi_play) */
    uint64_t a_split_n;       /* halves made by the port's split (else qd_app_audio's) */
    uint32_t a_begin_max, a_vblk_max, a_end_max;   /* P2.13: each part's longest half (ticks) */
    uint64_t draw;            /* P2.10: a frame's strips drawn (the list replayed, or drawn live), between the strips' outs */
    uint64_t lcd_wait;        /* P2.10: waits for the LCD (a transfer still reading what is written next) */
    uint64_t input_in_frame;  /* P2.10: the input between a frame's strips (the clock, the panel, MIDI, the LEDs) */
    uint64_t check;           /* P2.5: qd_app_frame_changed (the screen's logic once, nothing drawn) */
    uint64_t store;           /* P2.5: qd_persist_step (the store's writes) */
    uint32_t n_raster, n_blend, n_glyph, n_text, n_shapes, n_convert, n_frame, n_isr, n_check, n_skip, n_store;
    uint32_t n_a_seq, n_a_voices, n_a_fx;
    uint32_t frame_max, isr_max, store_max;
    uint32_t lock_max, lock_kind;   /* app.c's longest lock (interrupts off), and what it held (FZ_LOCK_*) */
} fz_prof_t;
extern volatile fz_prof_t fz_prof;
enum { FZ_LOCK_EVENT = 1, FZ_LOCK_TICK, FZ_LOCK_KNOB, FZ_LOCK_MIDI, FZ_LOCK_SYSEX, FZ_LOCK_TAKE };

/* a frame's time by screen: FIZZ's mode (ctl.h QD_MODE_*: 0 idle, 1 step, 2 keys, 3 tracks) x 4 + its sequencer view
 * (QD_VIEW_*: 0 PIANO ROLL, 1 STEPS, 2 EDIT) */
#define FZ_NVIEW 8                       /* (P2.15h: the core's views, the Geek Out pages too: QD_NVIEW 7, room for 8) */
#define FZ_NSCREEN (4 * FZ_NVIEW)
typedef struct {
    uint64_t sum;             /* ticks */
    uint32_t n, max;          /* frames, the longest (ticks) */
} fz_screen_t;
extern fz_screen_t fz_screens[FZ_NSCREEN];
void fz_prof_reset(void);
