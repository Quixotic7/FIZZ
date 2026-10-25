/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.5r: FIZZ's SysEx (docs/SYSEX.md): sounds imported into the LIBRARY or the project's POOL (a web page converting
 * Digitone .dnsnd presets with ChoralRoot's reader hands them over this way), and an identity query. Built through
 * app.c (it includes fizz_sx.c). The glue (core/glue/core_quadrille.c) carries the frames: what the ABI's midi_in
 * collects between F0 and F7 (FM1VST's core-glue/common sysex.h) goes to qd_app_sysex, and qd_app_sysex_take's replies
 * go out through midi_out_take.
 *
 *   F0 7D 46 5A 01 <cmd> <body> <chk> F7         7D: non-commercial; 46 5A "FZ"; 01 the protocol's version
 *   chk: (sum of cmd, body, chk) & 0x7F == 0      every byte between F0 and F7 7-bit
 *
 *   01  IDENTITY       -> 41 <protocol 01> <"FIZZ 0.6.4-p1.5r" ASCII>
 *   10  STORE SOUND    (P1.5t: the LIBRARY is MY presets: 256 places, the sound marked imported, its kind read)
 *                      flags, slot hi, slot lo, name[12], drive, track, pan, chorus, delay, reverb, mixmask,
 *                      patch[110] (the 96-byte FM TONE blob, 7-bit packed)
 *                      -> 50 <status> <0 LIBRARY / 1 POOL> <slot hi> <slot lo>
 *   any other command  -> 7F <status 7> <cmd> */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QD_SX_ID0 0x7Du
#define QD_SX_ID1 0x46u
#define QD_SX_ID2 0x5Au
#define QD_SX_PROTO 1u

enum { QD_SX_IDENTITY = 0x01, QD_SX_STORE = 0x10, QD_SX_STORE6 = 0x11, QD_SX_LABELS = 0x20, QD_SX_LIST = 0x21,
       QD_SX_DUMP = 0x22, QD_SX_R_IDENTITY = 0x41, QD_SX_R_STORE = 0x50, QD_SX_R_LABELS = 0x60, QD_SX_R_LIST = 0x61,
       QD_SX_R_ERROR = 0x7F };          /* (P1.6l: LABELS; P1.6m: LIST, DUMP: its replies STORE / STORE6 frames) */
/* the web manager's Phase 1 (docs/WEB-MANAGER-PLAN.md §5, docs/SYSEX.md): the library, the pool and the projects
 * edited from a page. The quick ones run in the handler (the lock: a directory entry, a record queued); the ones that
 * touch patterns or other projects' records are a job (qd_sx_t.wk) stepped by the store's step (qd_persist_step:
 * between frames, outside the locks), refused while playing (BUSY), paused if PLAY comes meanwhile; their reply comes
 * when they are done */
enum { QD_SX_STATUS = 0x02, QD_SX_FLUSH = 0x03, QD_SX_TAGS = 0x12, QD_SX_RENAME = 0x13, QD_SX_DELETE = 0x14,
       QD_SX_SWAP = 0x15, QD_SX_POOLREF = 0x16, QD_SX_USERS = 0x17, QD_SX_PROJECTS = 0x23, QD_SX_PROJECT = 0x24,
       QD_SX_R_STATUS = 0x42, QD_SX_R_ACK = 0x52, QD_SX_R_USERS = 0x57, QD_SX_R_PROJECTS = 0x63 };
#define QD_SXS_PROJECT 3               /* RENAME's source: a project (slot 1..8); MY PRESETS the other */
#define QD_SXD_CONFIRM 0x01u           /* DELETE's flags: delete even if used (else IN USE and the users) */
enum { QD_SXP_LOAD = 1, QD_SXP_NEW, QD_SXP_COPY, QD_SXP_DELETE };   /* PROJECT's ops */
#define QD_SXT_PLAYING 0x01u           /* STATUS's flags: playing, .. */
#define QD_SXT_NOSTORE 0x02u           /* .. no store attached (safe mode on the unit), .. */
#define QD_SXT_DIRTY 0x04u             /* .. something not written yet, .. */
#define QD_SXT_JOB 0x08u               /* .. a job running (BUSY for another), .. */
#define QD_SXT_DEMO 0x10u              /* .. the project worked on started with the demos */
#define QD_SXF_DUMP 0x08u              /* P1.6m: a STORE that is a DUMP's reply: FIZZ ignores it (clear it to restore) */
#define QD_SXS_MINE 0                  /* P1.6m: LIST / DUMP's sources: MY PRESETS (slot 1..256) .. */
#define QD_SXS_FACTORY 1               /* .. the factory presets (1..23) .. */
#define QD_SXS_POOL 2                  /* .. the project's pool (1..128: its preset) */
#define QD_SXL_FACTORY 0x01u           /* LIST's flags: the factory presets too, .. */
#define QD_SXL_POOL 0x02u              /* .. the pool too */
#define QD_SX_LIST_PER 16              /* a LIST reply's entries at most */
#define QD_SX_ENTRY 20                 /* an entry: source, slot (2), preset id (3), name (12), tags, flags */
#define QD_SXL_SUB 0x01u               /* P1.6l: LABELS' flags: subscribe (a LABELS reply by itself when they change) */
#define QD_SX_LABELS_MS 50u            /* .. looked at every this many ms at most (20 a second) */          /* (E1: STORE6, the 128-byte version-6 blob; its reply is STORE's) */

/* STORE's flags */
#define QD_SXF_POOL 0x01u              /* into the project's POOL (else the LIBRARY) */
#define QD_SXF_BATCH 0x02u             /* joins the previous import's undo level (a batch: one undo) */
#define QD_SXF_LOAD 0x04u              /* also onto track <track> of the pattern playing (its sound, and its mix by mixmask) */
/* STORE's mixmask (with LOAD): which of pan, chorus, delay, reverb the track takes */
#define QD_SXM_PAN 0x01u
#define QD_SXM_CHORUS 0x02u
#define QD_SXM_DELAY 0x04u
#define QD_SXM_REVERB 0x08u

/* the statuses */
enum { QD_SX_OK, QD_SX_FULL, QD_SX_BAD_LENGTH, QD_SX_BAD_CHECKSUM, QD_SX_BAD_PATCH, QD_SX_BAD_SLOT, QD_SX_BAD_VERSION,
       QD_SX_BAD_COMMAND, QD_SX_BUSY, QD_SX_NOT_FOUND, QD_SX_IN_USE, QD_SX_BAD_TRACK };   /* (8..11: Phase 1) */

#define QD_SX_NAME 12
#define QD_SX_BLOB 96
#define QD_SX_BLOB7 110                /* 13 groups of 7 bytes -> 8, the last 5 -> 6 */
#define QD_SX_STORE_BODY (1 + 2 + QD_SX_NAME + 1 + 1 + 4 + 1 + QD_SX_BLOB7)   /* 132 (P1.5x: or 133, + a tags byte) */
#define QD_SX_BLOB6 128                /* E1: STORE6's blob (version 6) .. */
#define QD_SX_BLOB6_7 147              /* .. 18 groups of 7 -> 8, the last 2 -> 3 */
#define QD_SX_STORE6_BODY (1 + 2 + QD_SX_NAME + 1 + 1 + 4 + 1 + QD_SX_BLOB6_7)   /* 169 (or 170, + a tags byte) */
#define QD_SX_OUT_MAX 48               /* an identity / store reply's bytes, F0 .. F7 (the short ones) */
#define QD_SX_FRAME_MAX 1024           /* P1.6l: the longest reply, F0 .. F7 (LABELS ~ 560) */
#define QD_SX_RING 4096                /* P1.6l: the replies waiting, a ring of bytes (each one: 2 bytes its length, then
                                        * it) */

/* (Phase 1) the job a command left (cmd 0: none): its arguments and its cursor; in qd_app_t (the pool on the unit) */
typedef struct {
    uint8_t cmd, src, flags, step;     /* the command, its source, its flags, the cursor (a project, a pattern) .. */
    uint8_t x, y;                      /* .. its slots (SWAP: the pool numbers; PROJECT: p, q; DELETE / USERS: the place) */
    uint16_t id;                       /* .. the preset (DELETE / USERS of MY presets) */
    uint16_t steps;                    /* USERS: the steps counted so far (all projects) .. */
    uint8_t np, tracks;                /* .. the projects using it, the tracks here */
    uint8_t pst[8];                    /* .. each project's steps (255: more), 0 not used (the reply lists the others) */
} qd_sx_work_t;

typedef struct {
    uint8_t ring[QD_SX_RING];
    uint32_t r, w;
    int lbl_sub;                       /* P1.6l: LABELS subscribed .. */
    int job, job_flags, job_src, job_at, job_part;   /* P1.6m: a LIST / DUMP going out (0 none): its cursor, its part */
    uint32_t lbl_count, lbl_hash, lbl_ms;   /* .. the set's change count, its hash, when last looked at */
    const char *version;               /* the identity's version ("FIZZ 0.6.4-p1.5r"; the glue sets it) */
    uint32_t frames, errors;           /* (counters) */
    qd_sx_work_t wk;                   /* (Phase 1) the job running */
    uint32_t rx_ms;                    /* the port's: when its last FIZZ frame came (its clock | 1; 0 none): the frames'
                                        * pacing during a transfer (firmware/app/app.c fz_step) */
    uint32_t xs[12];                   /* the port's transfer counters (app.c FZ_XS_*: stats line 16), here not in the
                                        * port's RAM */
} qd_sx_t;

struct qd_app;
void qd_app_sysex(struct qd_app *a, const uint8_t *f, uint32_t n);   /* one frame: its bytes between F0 and F7 */
int qd_app_sysex_take(struct qd_app *a, uint8_t *out, uint32_t max);  /* a reply, F0 .. F7: its length (0: none) */
int qd_app_sysex_peek(const struct qd_app *a);   /* P1.6l: the next reply's length (0: none) */
int qd_app_sysex_busy(const struct qd_app *a);   /* P1.6m: replies still to be made (an export going out) */
void qd_sx_ms(struct qd_app *a);       /* P1.6l: each ms: a subscribed page's LABELS when they change */
void qd_app_sysex_version(struct qd_app *a, const char *version);
void qd_sx_work(struct qd_app *a);     /* (Phase 1) the job's next step: the store's step calls it (persist.c) */

/* the 7-bit packing: groups of seven bytes as eight, the first holding their top bits (bit k: byte k's); n bytes ->
 * returns the packed length */
uint32_t qd_sx_pack7(const uint8_t *in, uint32_t n, uint8_t *out);
uint32_t qd_sx_unpack7(const uint8_t *in, uint32_t n, uint8_t *out);   /* packed n -> the bytes */

#ifdef __cplusplus
}
#endif
