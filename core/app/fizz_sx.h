/* SPDX-License-Identifier: GPL-3.0-only */
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

enum { QD_SX_IDENTITY = 0x01, QD_SX_STORE = 0x10, QD_SX_STORE6 = 0x11, QD_SX_R_IDENTITY = 0x41, QD_SX_R_STORE = 0x50,
       QD_SX_R_ERROR = 0x7F };          /* (E1: STORE6, the 128-byte version-6 blob; its reply is STORE's) */

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
       QD_SX_BAD_COMMAND };

#define QD_SX_NAME 12
#define QD_SX_BLOB 96
#define QD_SX_BLOB7 110                /* 13 groups of 7 bytes -> 8, the last 5 -> 6 */
#define QD_SX_STORE_BODY (1 + 2 + QD_SX_NAME + 1 + 1 + 4 + 1 + QD_SX_BLOB7)   /* 132 (P1.5x: or 133, + a tags byte) */
#define QD_SX_BLOB6 128                /* E1: STORE6's blob (version 6) .. */
#define QD_SX_BLOB6_7 147              /* .. 18 groups of 7 -> 8, the last 2 -> 3 */
#define QD_SX_STORE6_BODY (1 + 2 + QD_SX_NAME + 1 + 1 + 4 + 1 + QD_SX_BLOB6_7)   /* 169 (or 170, + a tags byte) */
#define QD_SX_OUT 4                    /* replies waiting */
#define QD_SX_OUT_MAX 48               /* a reply's bytes, F0 .. F7 */

typedef struct {
    uint8_t q[QD_SX_OUT][QD_SX_OUT_MAX];
    uint8_t n[QD_SX_OUT];
    int r, w;
    const char *version;               /* the identity's version ("FIZZ 0.6.4-p1.5r"; the glue sets it) */
    uint32_t frames, errors;           /* (counters) */
} qd_sx_t;

struct qd_app;
void qd_app_sysex(struct qd_app *a, const uint8_t *f, uint32_t n);   /* one frame: its bytes between F0 and F7 */
int qd_app_sysex_take(struct qd_app *a, uint8_t *out, uint32_t max);  /* a reply, F0 .. F7: its length (0: none) */
void qd_app_sysex_version(struct qd_app *a, const char *version);

/* the 7-bit packing: groups of seven bytes as eight, the first holding their top bits (bit k: byte k's); n bytes ->
 * returns the packed length */
uint32_t qd_sx_pack7(const uint8_t *in, uint32_t n, uint8_t *out);
uint32_t qd_sx_unpack7(const uint8_t *in, uint32_t n, uint8_t *out);   /* packed n -> the bytes */

#ifdef __cplusplus
}
#endif
