/* SPDX-License-Identifier: GPL-3.0-only */
/* FIZZ's sizes (Phase 2, P2.0; research/PHASE2-BUDGET.md §5 item 1): every capacity of the core in one place, in two
 * profiles of the same source.
 *
 *   QD_HOST    (the default) the Mac: the FM1VST plug-in and Standalone, the tools. Phase 1's sizes: free design.
 *   QD_DEVICE  (-DQD_DEVICE) the FM-1: about 425 KiB of data RAM for FIZZ (the budget, tools/budget). The host tests
 *              also run built this way (make -C tools test: build/dev/), so every change is checked at the device's
 *              sizes (the owner's rule from P2.0: memory and containers must fit them on every merge).
 *
 * QD_TARGET_FM1 (with QD_DEVICE): the device's own build (tools/budget's link): what only the host's tests need at the
 * device's sizes (persist.h's RAM store for the patterns when no flash is attached) is left out.
 *
 * A container that fills up at the device's sizes counts its overflow (as ChoralRoot's shed) instead of growing; the
 * demos peak far below them (the audit's `bench seqpeak`: 25 events, 13 trig records). Nothing here changes what a
 * project sounds like or how it is stored: the store's format is the same in both profiles. */
#pragma once

#if defined(QD_DEVICE) && defined(QD_HOST)
#error "qd_config.h: one profile, QD_DEVICE or QD_HOST"
#endif
#if !defined(QD_DEVICE) && !defined(QD_HOST)
#define QD_HOST 1
#endif

#ifdef QD_DEVICE
#define QD_PROFILE_NAME "device"
/* ---- the sound */
#define QD_POOL_MAX 16                 /* voices (8 + 8 on the two cores) */
#define QE_NCOMB 16                    /* E2's comb lines (1024 x int16 each): one for every voice */
/* ---- the sequencer */
#define QD_SEQ_NEV 512                 /* events waiting (the demos peak at 25) */
#define QD_SEQ_NREC 64                 /* resolved trigs waiting (the demos peak at 13) */
/* ---- the patterns (P2.1: paged; core/seq/pattern.h) */
#define QD_PAT_RAM 2                   /* in RAM: the playing one and the queued one (P2.1b: 2, was 3) */
/* ---- the sounds (P2.1: the pool's and MY presets' in the store; seq/sounds.h) */
#define QD_SND_RAM 8                   /* in RAM (their names, ids and tags all: the lists') */
/* ---- undo (P2.1: a log; app/undo.h) */
#define QD_UNDO_POOL 16384u            /* the levels and the open gesture's pattern as it was (compressed) */
#define QD_UNDO_XLOCKS 512             /* the other patterns' locks a pool slot's emptying cleared */
#define QD_UNDO_NSLOT 8                /* pool / library slots a gesture changes (their before: copies) */
#define QD_SNAP_BYTES 8192u            /* the snapshot and the pattern clipboard, compressed */
#define QD_CLIP_ZBYTES 8192u           /* the eight tracks' clip undo levels (seq/clip.h), compressed */
/* ---- the drawing (P2.1: core/gfx) */
#define QD_STRIP_H 40                  /* the screen drawn in strips of this many rows (a 240 x 40 ARGB surface) */
#define QD_GFX_ARENA 49152u            /* plutovg's fixed arena (qd_arena.h: no heap; P2.1b: render_frames' worst paint
                                        * 41 928 B, nothing refused) */
#define QD_FONTS3 1                    /* Inter Tight in three weights, 300 500 700 (the others: the nearest, upward) */
#define QD_GLYPH_CACHE 1               /* glyph outlines plutovg keeps a weight (P2.1b: one: each read when drawn) */
#define QD_GFX_KEEP 1024u              /* P2.1b: a canvas array above this goes back to the arena after its paint */
/* ---- the flash store */
#define QDS_MAXKEY 1024                /* keys a store */
#define QDS_JOBBUF 512                 /* a record the store copies (longer: streamed from its owner, store.h) */
#define QD_LIB_KEYS 320                /* the library store's index: MY presets (256), the factory tags */
#define QD_PRJ_KEYS 1024               /* the projects' store's index */
#else
#define QD_PROFILE_NAME "host"
#define QD_POOL_MAX 64                 /* (a setting: 16 by default, more for experiments) */
#define QE_NCOMB 64                    /* a line for every voice the pool can have */
#define QD_SEQ_NEV 4096
#define QD_SEQ_NREC 512
#define QD_PAT_RAM 64                  /* (every pattern: QD_NPAT) */
#define QD_SND_RAM 0                   /* (every sound in RAM, as in Phase 1) */
#define QD_UNDO_POOL 65536u            /* (the levels' differences; the gesture's before: whole copies) */
#define QD_STRIP_H 240                 /* (the whole screen at once; the pixel test draws it in 40-row strips too) */
#define QD_UNDO_XLOCKS 2048
#define QDS_MAXKEY 2048
#define QDS_JOBBUF 28800               /* (QDS_MAXREC: every record copied, as in Phase 1) */
#define QD_LIB_KEYS QDS_MAXKEY
#define QD_PRJ_KEYS QDS_MAXKEY
#endif

/* ---- the same in both */
#define QD_POOL_DEFAULT 16             /* the voices a unit plays with */
