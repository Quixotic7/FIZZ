/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 ChoralRoot FM-1 contributors (a fork of Felucca) */
/* QUADRILLE: the parts of ChoralRoot v0.25.1's (15ca8d0) firmware/src/core.h, GPL-3.0-only, that FM TONE
 * (eng_quad.c, dsp.c) and the effects (fx.c, cr_fx.c) read, copied as they are there: NELEM, the parameter formats
 * and param_desc_t, the global parameters' ids (G_*), the voice modes (V_*), vmod_t, preset_t with FX() / PAT(),
 * eng_page_t, ENG_BLOB_MAX. Changed: P_* is cut to P_VOICE (the one track parameter FM TONE reads); voice_t and track_t
 * are not here (quad_voice.c makes them QUADRILLE's voice and owner, quad_voice.h); song_t is a stub of the fields
 * these files read (g[], master_q12), one per unit that includes this, set by that unit's QUADRILLE code. */
#pragma once
#include <stdint.h>

#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

/* ------------------------------------------------------- parameters --- */
enum {
    F_INT, F_PCT, F_BIPCT, F_TIME, F_LFOHZ, F_CUTOFF, F_DB, F_SEMI, F_ENUM, F_BPM, F_NOTE,
    F_ONOFF, F_OCT, F_STEPS,
    F_OFS, F_FMNOTE, F_FMFRQ,   /* FM6's pages: 0 at the middle of the range, the DX7 break point, an operator's frequency
                                 * (params.c param_format; F_FMFRQ: Melodee's operator pages, here a plain number) */
    F_TIMEI,                    /* F_TIME with 127 = "INF" (a release that holds, the Digitone's 0..126 + INF; QUAD's
                                 * filter REL until 2026-10-14, now F_INTI: its times are the Digitone's, not
                                 * F_TIME's); the editor protocol's fmt 17 */
    F_FIX2,                     /* a hi-res value in hundredths (FM TONE, the Digitone's two decimals): 4201 = "42.01",
                                 * a range below 0 signed ("+41.34", "-64.00"); its finest step 1, its whole-number step
                                 * 100; names[0] (if any) = the word for the top whole number (AMP REL: "INF" at
                                 * 127.00 and up); the editor protocol's fmt 18 */
    F_INTI                      /* a number with its max = "INF" (FM TONE's AMP DEC and REL, its filter REL: 0..126 +
                                 * INF, the Digitone's); the editor protocol's fmt 19 */
};
#define FMT_HI(f) ((f) == F_FIX2)   /* a hundredths format (cr_pages.c kn_whole: finest step 1, whole numbers 100) */

typedef struct {
    const char *label;
    uint8_t fmt;
    int16_t min, max, def;
    const char *const *names;   /* F_ENUM */
    const char *unit;           /* F_INT / F_ENUM optional unit */
} param_desc_t;

enum { P_VOICE, P_COUNT };      /* QUADRILLE: ChoralRoot's per-track parameters cut to the one FM TONE reads */

enum {                          /* global parameters */
    G_BPM, G_SWING, G_CLOCK, G_TUNE,
    G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX,
    G_RSIZE, G_RDAMP, G_CRATE, G_CDEPTH,
    G_MIDI, G_SYNC, G_ROUTE, G_INFO,   /* G_ROUTE: MIDI IN, 0 CH1-4 (channels 1..4 -> parts 1..4), 1 SEL (seq.c) */
    G_SLOT, G_NAME, G_LOAD, G_SAVE,
    G_ENGSEL, G_ENGGO,          /* no page: the editor switches the engine with a SET of G_ENGSEL; G_ENGGO is
                                 * unused (ids are fixed by the formats and the protocol) */
    G_CLRSEQ, G_INITSND,
    G_RTYPE,                    /* REVERB TYPE: 0 ROOM, 1 SPRING (fx.c). Was G_DRCH, the GM drum part's MIDI
                                 * channel (inert since 1.0, never read); projects of formats before FUN7 load it
                                 * as ROOM (project.c proj_rtype_room) */
    G_DRLVL, G_DRREV,           /* inert (label "-", on no page): the GM drum part they set is gone; kept
                                 * because the ids and G_COUNT are fixed by the formats and the protocol (only
                                 * the import of an old project reads them: proj_drums_to_part) */
    G_COUNT
};

/* ------------------------------------------------- voices, engines --- */
enum { V_POLY, V_MONO, V_LEGATO, V_UNISON };   /* P_VOICE */

typedef struct {                 /* per-voice control-rate modulation, computed in voice.c */
    uint32_t inc;                /* phase increment of the base pitch */
    int32_t pitch16;
    int32_t amp0, amp1;          /* Q15 ramp over the block */
    int32_t cutoff;              /* 0..127 << 8 */
    int32_t shape;               /* 0..127 << 8 */
    int32_t envq15;              /* env value (for engines that use it as a mod source) */
    int32_t fine;                /* the residual below 1/16 semitone in inc: unison detune, TUNE, bend (1/4096) */
    int32_t plog;                /* the voice's own pitch offset without TUNE and bend (glide, LFO and ENV pitch, the
                                  * matrix's, unison detune), Q24 octaves: for engines that figure their pitch (FM6) */
} vmod_t;

typedef struct {
    const char *name;
    int8_t e[8];                 /* P_E0..P_E7 (signed: an interval below the note; every value fits) */
    uint8_t env[4];              /* ATK DEC SUS REL */
    int8_t fenv;                 /* ENV -> FILTER amount (-64..63) */
    uint8_t mono;                /* 1 = MONO (bass / lead), 0 = POLY */
    /* the rest of the patch; each value is stored + 1, 0 = the default */
    uint8_t fx[4];               /* DIST, CHORUS, DELAY, REVERB sends */
    uint8_t pat;                 /* suggested pattern (PATTERNS[pat - 1], 0 = none): only a hint. A sound load
                                  * never touches the steps; SEQ > PATTERNS loads a pattern (ui.c pat_load). The
                                  * arp is the track's too: a preset does not set it */
} preset_t;
#define FX(d, c, dl, r) .fx = {(d) + 1, (c) + 1, (dl) + 1, (r) + 1}
#define PAT(n) .pat = (n)

typedef struct {
    const char *title;           /* "OSC 1" (<= 7 chars) */
    param_desc_t col[4];         /* KNOB 1..4; label 0 = an empty column */
} eng_page_t;
#define ENG_BLOB_MAX 160u       /* the largest blob (CZ-1: 144; the VA's 110 and FM6's 128 keep their store records) */

/* QUADRILLE: ChoralRoot's song_t, cut to what these files read: the global parameters (the tempo, the chorus's rate
 * and depth) and MASTER (Q12). One per unit; the unit's QUADRILLE code sets it */
typedef struct {
    int16_t g[G_COUNT];
    uint32_t master_q12;
} song_t;
static song_t song;
