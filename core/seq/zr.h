/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P2.1: bytes with their zero runs squeezed, exactly (the device's undo log and snapshot, app/undo.c; its clip levels,
 * seq/clip.c). Static functions: each user includes it. */
#pragma once
#include <stdint.h>
#include <string.h>

/* runs of [u16 zeros][u16 n][n bytes]: a pattern is mostly zeros (its empty steps, its unused locks). out 0: the size
 * only; 0: no room */
static inline uint32_t zr_pack(const uint8_t *src, uint32_t n, uint8_t *out, uint32_t room)
{
    uint32_t i = 0, o = 0;
    while (i < n) {
        uint32_t z = 0, s, e, k, run;
        while (i < n && !src[i] && z < 65535u)
            i++, z++;
        s = i, e = i, run = 0;
        while (e < n && e - s < 65535u && run < 8u) {  /* (a literal ends at 8 zeros in a row) */
            run = src[e] ? 0u : run + 1u;
            e++;
        }
        if (run >= 8u)
            e -= run;
        if (out) {
            if (o + 4u + (e - s) > room)
                return 0;
            out[o] = (uint8_t)z, out[o + 1] = (uint8_t)(z >> 8);
            out[o + 2] = (uint8_t)(e - s), out[o + 3] = (uint8_t)((e - s) >> 8);
            for (k = s; k < e; k++)
                out[o + 4u + k - s] = src[k];
        }
        o += 4u + (e - s);
        i = e;
    }
    return o ? o : 4u;                                  /* (all zero: one empty run) */
}
static inline void zr_unpack(const uint8_t *in, uint32_t len, uint8_t *dst, uint32_t n)
{
    uint32_t o = 0, d = 0;
    memset(dst, 0, n);
    while (o + 4u <= len && d <= n) {
        const uint32_t z = (uint32_t)in[o] | (uint32_t)in[o + 1] << 8, l = (uint32_t)in[o + 2] | (uint32_t)in[o + 3] << 8;
        o += 4u;
        d += z;
        if (d + l > n || o + l > len)
            return;
        memcpy(dst + d, in + o, l);
        d += l, o += l;
    }
}
