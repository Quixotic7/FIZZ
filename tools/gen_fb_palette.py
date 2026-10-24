# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""P2.6e: the 8-bit frame's palette (research/SMOOTH-DISPLAY.md 4.8.3), fitted to the colours FIZZ paints.

    python3 tools/gen_fb_palette.py tools/fb_colours.txt core/gfx/qd_fb_pal.h

fb_colours.txt: the paint colours of every render_frames screen with the pixels each wrote ("RRGGBB full partial",
counted by an instrumented fast drawing). The palette is 16 ramps x 16 levels, index 16 h + l:
  - level l of ramp h is its top x l / 15 per channel (8-bit), as RGB565 the renderer's own way (L5 / L6 rounding);
    level 0 of ramp 0 is black, index 0, the only black;
  - index 16 h + 0 for h >= 1 (15 of them) is a "spare": a solid colour no ramp holds (the heaviest such);
  - ramp 0 is neutral grey to white; the other fifteen tops are fitted (k-means over the colours' hues, each top the
    one that holds its colours best, weighted by their pixels).
The nearest-colour table: 4096 RGB444 cells -> the nearest palette entry (redmean distance), for blends that leave
both ramps (the renderer dithers into it). Output: QF_PAL (RGB565), QF_PAL_BE (byte-swapped, for the panel), QF_NEAR."""
import sys

L5 = [(i * 31 + 127) // 255 for i in range(256)]
L6 = [(i * 63 + 127) // 255 for i in range(256)]


def q565(c):
    r, g, b = c
    return L5[r] << 11 | L6[g] << 5 | L5[b]


def rgb_of565(v):
    r, g, b = v >> 11 & 31, v >> 5 & 63, v & 31
    return ((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31)


def dist(a, b):                                         # redmean, squared
    rm = (a[0] + b[0]) / 2.0
    dr, dg, db = a[0] - b[0], a[1] - b[1], a[2] - b[2]
    return (2 + rm / 256) * dr * dr + 4 * dg * dg + (2 + (255 - rm) / 256) * db * db


def level(top, l):
    return tuple(min(255, int(t * l / 15.0 + 0.5)) for t in top)


def ramp_err(top, c):
    """the best level of ramp top for colour c: (squared error in RGB565 terms, level)"""
    best = (1e18, 0)
    for l in range(1, 16):
        e = dist(rgb_of565(q565(level(top, l))), rgb_of565(q565(c)))
        if e < best[0]:
            best = (e, l)
    return best


# P2.6h: the tops held fixed (the UI's own colours: every one of them is drawn anti-aliased over black, so each must
# be a ramp's level, not a spare): the warm white K3 (ramp 0: the whites and the neutral greys under it), the cool
# greys G1 (G2 and G3 are its levels 8 and 4), the tracks' colours, the step views' and the rider's
FIXED = [(0xf1, 0xf0, 0xea), (0x8d, 0x8d, 0x96), (0x3f, 0x7d, 0xff), (0x1f, 0xd4, 0x7e), (0xff, 0x6a, 0x2c),
         (0xff, 0x3c, 0xa0), (0xff, 0xd2, 0x3f), (0x96, 0x64, 0xff), (0x28, 0xe1, 0xff), (0xf0, 0x3c, 0xff),
         (0xff, 0xf0, 0xd2), (0x2f, 0xb4, 0xff), (0xff, 0x3b, 0x4f), (0x63, 0xc4, 0xd6), (0xff, 0x62, 0xd2),
         (0x5c, 0xf2, 0xc4)]


def fit(cols, ntop=15, iters=8):
    neutral = FIXED[0]
    # seeds: the fixed tops, then the heaviest colours the ramps so far hold worst, one at a time
    tops = list(FIXED[1:])
    nfixed = len(tops)
    for c, w in cols:
        if len(tops) >= ntop:
            break
        if sum(c) < 60:
            continue
        e = min([ramp_err(neutral, c)[0]] + [ramp_err(t, c)[0] for t in tops])
        if e > 300:
            m = max(c)
            tops.append(tuple(min(255, int(x * 255.0 / m + 0.5)) for x in c) if m else c)
    for it in range(iters):
        groups = [[] for _ in tops]
        for c, w in cols:
            en = ramp_err(neutral, c)[0]
            es = [ramp_err(t, c)[0] for t in tops]
            k = min(range(len(tops)), key=lambda i: es[i])
            if es[k] < en:
                groups[k].append((c, w))
        new = []
        for ti, (t, g) in enumerate(zip(tops, groups)):
            if ti < nfixed or not g:
                new.append(t)
                continue
            cands = {t}
            for c, w in g[:12]:
                for l in range(8, 16):
                    cands.add(tuple(min(255, int(x * 15.0 / l + 0.5)) for x in c))
            new.append(min(cands, key=lambda T: sum(w * ramp_err(T, c)[0] for c, w in g)))
        if new == tops:
            break
        tops = new
    return [neutral] + tops


def main():
    src, out = sys.argv[1], sys.argv[2]
    cols, edges = [], []
    for line in open(src):
        p = line.split()
        if len(p) < 3 or p[0].startswith('#'):
            continue
        v = int(p[0], 16)
        cols.append(((v >> 16 & 255, v >> 8 & 255, v & 255), int(p[1]) + int(p[2])))
        edges.append(((v >> 16 & 255, v >> 8 & 255, v & 255), int(p[2]) + int(p[1]) // 10))
    cols.sort(key=lambda cw: -cw[1])
    edges.sort(key=lambda cw: -cw[1])
    tops = fit(edges)                                   # (P2.6h: the free tops fitted to the edges' pixels: a solid
                                                        # fill is a spare exactly; a ramp is for what is blended)
    while len(tops) < 16:
        tops.append((255, 255, 255))
    pal = [0] * 256
    for h, t in enumerate(tops):
        for l in range(1, 16):
            pal[16 * h + l] = q565(level(t, l))
    pal[0] = 0
    # the spares: the heaviest colours no ramp level matches exactly (in RGB565)
    have = set(pal[16 * h + l] for h in range(16) for l in range(1, 16)) | {0}
    spares = []
    for c, w in cols:
        v = q565(c)
        if v not in have and v not in spares:
            spares.append(v)
        if len(spares) >= 15:
            break
    for h in range(1, 16):
        pal[16 * h] = spares[h - 1] if h - 1 < len(spares) else 0
    prgb = [rgb_of565(v) for v in pal]
    near = []
    for r in range(16):
        for g in range(16):
            for b in range(16):
                c = (r * 17, g * 17, b * 17)
                near.append(min(range(256), key=lambda i: dist(prgb[i], c)))
    # the report: solid colours not exactly in it, weighted
    tot = sum(w for c, w in cols)
    miss = [(c, w) for c, w in cols if q565(c) not in set(pal)]
    with open(out, 'w') as f:
        f.write('/* SPDX-License-Identifier: MPL-2.0\n * This Source Code Form is subject to the terms of the Mozilla '
                'Public License, v. 2.0. If a copy of the MPL was not\n * distributed with this file, You can obtain '
                'one at https://mozilla.org/MPL/2.0/. */\n')
        f.write('/* P2.6e: the 8-bit frame\'s palette, made by tools/gen_fb_palette.py from tools/fb_colours.txt: do not '
                'edit.\n * 16 ramps x 16 levels (index 16 h + l; 16 h + 0, h >= 1: a spare solid colour); the ramps\' '
                'tops:\n')
        for h, t in enumerate(tops):
            f.write(' *   %2d  %02x%02x%02x\n' % (h, t[0], t[1], t[2]))
        f.write(' * The paint colours\' pixels on an exact entry: %.2f %% (%d colours of %d not exact) */\n' %
                (100.0 * (tot - sum(w for c, w in miss)) / tot, len(miss), len(cols)))
        f.write('#pragma once\n#include <stdint.h>\n\n#define QF_PAL_GEN 1u\n')
        for name, vals in (('QF_PAL', pal), ('QF_PAL_BE', [((v >> 8) | (v << 8)) & 0xFFFF for v in pal])):
            f.write('static const uint16_t %s[256] = {\n' % name)
            for i in range(0, 256, 12):
                f.write('    ' + ', '.join('0x%04x' % v for v in vals[i:i + 12]) + ',\n')
            f.write('};\n')
        # P2.6h: RGB565 -> its entry (a ramp's level before a spare, the first in the palette's order), an open table
        xk, xi = [0] * 512, [0] * 512
        for k in range(256):
            ramp = k == 0 or (k & 15) != 0
            q = ((pal[k] * 40503) >> 7) & 511
            while xk[q] and xk[q] != pal[k] + 1:
                q = (q + 1) & 511
            if not xk[q]:
                xk[q], xi[q] = pal[k] + 1, k
            elif ramp and xi[q] and not (xi[q] & 15):
                xi[q] = k
        f.write('static const uint32_t QF_XK[512] = {   /* (RGB565 + 1; 0: empty) */\n')
        for i in range(0, 512, 12):
            f.write('    ' + ', '.join('0x%05x' % v for v in xk[i:i + 12]) + ',\n')
        f.write('};\nstatic const uint8_t QF_XI[512] = {\n')
        for i in range(0, 512, 24):
            f.write('    ' + ', '.join('%d' % v for v in xi[i:i + 24]) + ',\n')
        f.write('};\n')
        f.write('static const uint8_t QF_NEAR[4096] = {   /* RGB444 (r << 8 | g << 4 | b) -> the nearest entry */\n')
        for i in range(0, 4096, 24):
            f.write('    ' + ', '.join('%d' % v for v in near[i:i + 24]) + ',\n')
        f.write('};\n')
    print('gen_fb_palette: %s: tops %s; %d of %d colours not exact (%.3f %% of the pixels)' %
          (out, ' '.join('%02x%02x%02x' % t for t in tops), len(miss), len(cols),
           100.0 * sum(w for c, w in miss) / tot))


if __name__ == '__main__':
    main()
