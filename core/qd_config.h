/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
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

/* the device's strip: P2.1c 20 rows (19 KB less RAM); P2.4 40 again (the unit: every strip runs the screen's logic
 * and outlines every path again, so half the strips is half that work; the port has the RAM); the host draws the
 * whole screen and its strip tests draw it in these too */
#if defined(QD_GFX_FB) && !defined(QD_GFX_FAST)
#define QD_GFX_FAST 1                  /* (P2.6e: the 8-bit frame is the fast drawing's) */
#endif
#ifdef QD_GFX_FB
#define QD_STRIP_DEV 16                /* P2.6e: the 8-bit frame's bands (the strokes' coverage a band at a time; the
                                        * list played once a band, each band only its calls) */
#else
#define QD_STRIP_DEV 40
#endif

#if defined(QD_DEVICE) && defined(QD_HOST)
#error "qd_config.h: one profile, QD_DEVICE or QD_HOST"
#endif
#if !defined(QD_DEVICE) && !defined(QD_HOST)
#define QD_HOST 1
#endif

/* P2.1b: the constant data that leaves the app slot for the data region (the fonts, the TAP ape's lines): the device's
 * build puts each in its own section .qd_xdata.<name>; its link script gathers them at the data region's XIP address
 * (FM1_XIP(off) = 0x02000000 + off - 0x4000, research/PHASE2-BUDGET.md §1.4), objcopy -j makes the blob, and the
 * installer writes it there with the firmware (tools/budget counts it apart from the slot). Elsewhere: plain constants */
#ifdef QD_TARGET_FM1
#define QD_XDATA(name) __attribute__((section(".qd_xdata." #name)))
#else
#define QD_XDATA(name)
#endif

#ifdef QD_DEVICE
#define QD_PROFILE_NAME "device"
/* ---- the sound */
/* P2.6f, the owner's decision (2026-10-24): 8 voices for good, on every profile ("which is what the Digitone 1 had").
 * QD_VOICES_DEV stays a build knob for the tests (-DQD_VOICES_DEV=16: tools/Makefile test-dev16). Everything sized by
 * the voices follows it: the pool and its voices' state, the mix's, the MIDI FX's note tables, the tracks' limits and
 * reserves (none above it) */
#ifndef QD_VOICES_DEV
#define QD_VOICES_DEV 8
#endif
#if QD_VOICES_DEV != 8 && QD_VOICES_DEV != 16
#error "qd_config.h: QD_VOICES_DEV is 8 or 16"
#endif
#define QD_POOL_MAX QD_VOICES_DEV      /* voices */
#define QE_NCOMB 8                     /* E2's comb lines (1024 x int16 each), lent to the voices that run a comb;
                                        * P2.1c, the owner's rule (16 if it fits, else 8): 8, a ninth comb voice runs
                                        * MULTI (quad_ext.c); at 8 voices every voice can comb */
#define QD_MFX_NOUT (2 * QD_VOICES_DEV)   /* a MIDI FX group's held notes sent (32 at 16 voices: the OMX's) .. */
#define QD_MFX_NPEND (4 * QD_VOICES_DEV)  /* .. its timed notes sounding (64 at 16); full: a note is not sent (counted) */
/* ---- the sequencer */
#define QD_SEQ_NEV 512                 /* events waiting (the demos peak at 25) */
#define QD_SEQ_NREC 64                 /* resolved trigs waiting (the demos peak at 13) */
/* ---- the patterns (P2.1: paged; core/seq/pattern.h) */
#define QD_PAT_RAM 2                   /* in RAM: the playing one and the queued one (P2.1b: 2, was 3) */
/* ---- the sounds (P2.1: the pool's and MY presets' in the store; seq/sounds.h) */
#define QD_SND_RAM 8                   /* in RAM (their names, ids and tags all: the lists') */
/* ---- undo (P2.1: a log; app/undo.h) */
#define QD_UNDO_POOL 16384u            /* the one log (P2.1c): the levels, the open gesture's copies, the snapshot and
                                        * the pattern clipboard (compressed) */
#define QD_UNDO_XLOCKS 512             /* the other patterns' locks a pool slot's emptying cleared */
#define QD_UNDO_NSLOT 8                /* pool / library slots a gesture changes (their before: copies) */
#define QD_CLIP_ZBYTES 8192u           /* the eight tracks' clip undo levels (seq/clip.h), compressed */
/* ---- the drawing (P2.1: core/gfx) */
#define QD_STRIP_H QD_STRIP_DEV        /* the screen drawn in strips of this many rows (plutovg: a 240 x 40 ARGB surface;
                                        * the fast drawing: 240 x 40 RGB565) */
#ifdef QD_GFX_FAST
#define QD_GFX_ARENA 27136u            /* P2.4: the unit's drawing (qd_gfx_fast.c) takes nothing from the arena: what is
                                        * left is the borrowers' (pattern.h qd_tmp_borrow; test_ctl's peak 22,776 B),
                                        * QD_GFX_MARGIN kept */
#else
#define QD_GFX_ARENA 46592u            /* plutovg's fixed arena (qd_arena.h: no heap). P2.4: 45.5 KB (at 40-row strips the
                                        * worst paint is test_ctl's 42,248 B, render_frames' 37,376; QD_GFX_MARGIN kept) */
#endif
#define QD_GFX_MARGIN 4096u            /* P2.1c: kept free at the worst paint (render_frames, test_ctl fail past it) */
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
#define QD_STRIP_H 240                 /* (the whole screen at once; the pixel test draws it in QD_STRIP_DEV-row strips) */
#define QD_UNDO_XLOCKS 2048
#define QDS_MAXKEY 2048
#define QDS_JOBBUF 28800               /* (QDS_MAXREC: every record copied, as in Phase 1) */
#define QD_LIB_KEYS QDS_MAXKEY
#define QD_PRJ_KEYS QDS_MAXKEY
#endif

/* ---- the same in both */
#ifdef QD_DEVICE
#define QD_POOL_DEFAULT QD_VOICES_DEV  /* the voices a unit plays with (P2.6f: 8) */
#else
#define QD_POOL_DEFAULT 8              /* the voices a unit plays with (the host: FM1VST, the site; P2.6f: 8, as the unit) */
#endif
