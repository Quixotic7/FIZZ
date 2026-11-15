/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The built-in demos: a project's patterns' defaults (pattern.h qd_project_t.demo, the HDR record's demo byte).
 *
 * ATRIUM (2026-11-15, QD_DEMO_ATRIUM): the demo song tools/song_atrium.c made here, the first project of a fresh unit
 * (persist.c). Ten four-bar patterns at 71 BPM; their track sounds and drum locks are the pool's (slots 1..17,
 * references to factory presets 24..116: QD_ATRIUM_POOL), three tracks' sounds edited in the pattern (a flat tuning,
 * the opening's air). The patterns and the MIDI FX group are this file's code (no record until they change); the
 * pool, the mix and the tracks' names are records persist.c writes once, the effects the settings' record.
 *
 * THE OLD DEMO (P1.2, QD_DEMO_OLD): A01..A03 (a beat on track 1 of pool sound locks, A01's beat with bass, keys and a
 * lead, a polymeter on every track), the default kit's. Built into the host's tools and tests only (tools/Makefile
 * -DQD_DEMO_HOST: their fixtures); the firmware and the browser emulator have none: a project whose header says
 * QD_DEMO_OLD there has blank patterns where the old demo's had no record. */
#pragma once
#include "pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { QD_DEMO_NONE = 0, QD_DEMO_OLD = 1, QD_DEMO_ATRIUM = 2 };
#define QD_ATRIUM_N 10                  /* A01..A10 */
#define QD_ATRIUM_NPOOL 17              /* its pool: slots 1..17 */
extern const uint8_t QD_ATRIUM_POOL[QD_ATRIUM_NPOOL];   /* each slot's factory preset id, less QD_ATRIUM_ID0 */
#define QD_ATRIUM_ID0 24                /* (the pack's first id: sounds.h) */

#ifdef QD_DEMO_HOST
#define QD_DEMO_N 3                     /* the old demo: A01..A03 */
extern int qd_demo_first;               /* the demo a fresh flash (and an app with none) starts with: QD_DEMO_OLD, the
                                         * fixtures'; a test sets QD_DEMO_ATRIUM */
extern int qd_demo_old_on;              /* 1: the old demo's patterns built in (0: as the firmware, none) */
void qd_demo_pattern(qd_pattern_t *p, int k);   /* the old demo's k (0..2) into p */
#else
#define qd_demo_first QD_DEMO_ATRIUM
#endif

int qd_demo_n(int kind);                /* the patterns demo `kind` gives (0: none) */
void qd_demo_load(qd_project_t *pr, int kind);   /* pr's demo: kind; the patterns in RAM made, the MIDI FX groups */
void qd_demo_default(qd_pattern_t *p, int kind, int k);   /* demo kind's pattern k into p (blank past its patterns) */
/* demo kind's pattern k, not made: its steps locking pool slot snd (fn: each, t and s) and its track sounds from it
 * (fn: s -1). Their count (persist.c: a slot's users in the patterns with no record) */
int qd_demo_locks(int kind, int k, int snd, void (*fn)(void *ctx, int t, int s), void *ctx);

#ifdef __cplusplus
}
#endif
