#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""The TAP ape's poses (mockups/fizz-tap-ape-data.js, traced from the owner's reference) into core/ui/ape_poses.h.

    python3 tools/gen_ape_poses.py [data.js] [out.h]

The mock-up's APES[k].parts[colour] are polylines in half pixels (x from the boombox's left edge, y up from the floor:
negative). Each colour of each pose becomes a run of polylines in one byte stream:
  - two-point segments that meet end to end are merged into straight chains (drawn with lines, as the mock-up draws a
    two-point line: the same picture);
  - the colour's polylines ordered nearest first (each may be reversed: a line drawn back is the same line), so most
    start where the one before ended or near it;
  - a polyline: a head byte (bit 7: straight, else smooth; bit 6: it starts where the one before ended, no start
    bytes; bits 0..5: its points - 1, 1..64), its start (the first of the colour's run: two int16 LE; then relative to
    the previous polyline's end: two int8, or -128 and two int16 LE when it does not fit), then its points as int8
    deltas (dx, dy).
ape_poses.h: APE_BYTES[], and per pose its extent (px, y up) and, per colour (cyan, flowers, shirt, gold, fur: the
drawing order), the offset of its run and its polylines' count."""
import json, struct, sys

ORDER = ['cyan', 'leaf', 'shirt', 'gold', 'fur']


def load(path):
    s = open(path).read()
    return json.loads(s[s.index('['):s.rindex(']') + 1])


def chains(segs):
    """two-point segments ((x0, y0), (x1, y1)) joined end to end into chains (direction free), greedy"""
    left = list(segs)
    out = []
    while left:
        a, b = left.pop(0)
        ch = [a, b]
        grown = True
        while grown:
            grown = False
            for i, (p, q) in enumerate(left):
                if p == ch[-1]:
                    ch.append(q)
                elif q == ch[-1]:
                    ch.append(p)
                elif q == ch[0]:
                    ch.insert(0, p)
                elif p == ch[0]:
                    ch.insert(0, q)
                else:
                    continue
                left.pop(i)
                grown = True
                break
        out.append(ch)
    return out


def order(lines):
    """nearest first: from the first, each next the one (either way round) starting nearest the last end"""
    left = list(lines)
    out = [left.pop(0)]
    while left:
        e = out[-1][1][-1]
        best = None
        for i, (st, p) in enumerate(left):
            for rev in (0, 1):
                q = p[::-1] if rev else p
                d = max(abs(q[0][0] - e[0]), abs(q[0][1] - e[1]))
                if best is None or d < best[0]:
                    best = (d, i, q)
        d, i, q = best
        out.append((left.pop(i)[0], q))
    return out


def encode(lines):
    """[(straight, [(x, y), ..]), ..] -> bytes"""
    b = bytearray()
    prev = None
    n = 0
    for straight, pts in order(lines) if lines else []:
        while len(pts) > 65:                             # (split: 64 deltas at most a polyline; the next starts there)
            b += one(straight, pts[:65], prev)
            prev = pts[64]
            pts = pts[64:]
            n += 1
        b += one(straight, pts, prev)
        prev = pts[-1]
        n += 1
    return bytes(b), n


def one(straight, pts, prev):
    x, y = pts[0]
    cont = prev is not None and (x, y) == prev
    b = bytearray([(0x80 if straight else 0) | (0x40 if cont else 0) | (len(pts) - 1)])
    if cont:
        pass
    elif prev is None:
        b += struct.pack('<hh', x, y)
    else:
        dx, dy = x - prev[0], y - prev[1]
        if -127 <= dx <= 127 and -127 <= dy <= 127:
            b += struct.pack('<bb', dx, dy)
        else:
            b += struct.pack('<b', -128) + struct.pack('<hh', x, y)
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        dx, dy = x1 - x0, y1 - y0
        assert -127 <= dx <= 127 and -127 <= dy <= 127, (dx, dy)
        b += struct.pack('<bb', dx, dy)
    return bytes(b)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else 'mockups/fizz-tap-ape-data.js'
    out = sys.argv[2] if len(sys.argv) > 2 else 'core/ui/ape_poses.h'
    apes = load(src)
    blob = bytearray()
    poses = []
    npts = nl = 0
    for a in apes:
        runs = []
        for col in ORDER:
            ls = a['parts'].get(col) or []
            segs, smooth = [], []
            for l in ls:
                p = [(l[i], l[i + 1]) for i in range(0, len(l), 2)]
                if len(p) == 2:
                    segs.append((p[0], p[1]))
                elif len(p) > 2:
                    smooth.append((False, p))
            lines = smooth + [(True, c) for c in chains(segs)]
            enc, n = encode(lines)
            runs.append((len(blob), n))
            blob += enc
            nl += n
            npts += sum(len(p) for _, p in lines)
        poses.append((a['ext'], runs))
    with open(out, 'w') as f:
        f.write('/* SPDX-License-Identifier: MPL-2.0\n * This Source Code Form is subject to the terms of the Mozilla Public '
                'License, v. 2.0. If a copy of the MPL was not\n * distributed with this file, You can obtain one at '
                'https://mozilla.org/MPL/2.0/. */\n')
        f.write('/* generated by tools/gen_ape_poses.py from mockups/fizz-tap-ape-data.js (the TAP ape\'s twelve poses, traced\n')
        f.write(' * from the owner\'s reference): do not edit. %d polylines, %d points, %d bytes. */\n' % (nl, npts, len(blob)))
        f.write('#pragma once\n#include <stdint.h>\n\n#include "../qd_config.h"                   /* (P2.1b: QD_XDATA, the data region on the device) */\n\n')
        f.write('#define APE_NPOSE %d\n#define APE_NCOL 5\n#define APE_NBYTES %d\n\n' % (len(poses), len(blob)))
        f.write('typedef struct {\n    int16_t ext[4];                     /* x0, y0, x1, y1 (px; y up from the floor) */\n')
        f.write('    uint16_t off[APE_NCOL];             /* each colour\'s run in APE_BYTES (cyan, flowers, shirt, gold, fur) */\n')
        f.write('    uint8_t n[APE_NCOL];                /* .. its polylines */\n} ape_pose_t;\n\n')
        f.write('static const uint8_t APE_BYTES[APE_NBYTES] QD_XDATA(APE_BYTES) = {\n')
        for i in range(0, len(blob), 24):
            f.write('    ' + ', '.join('%d' % v for v in blob[i:i + 24]) + ',\n')
        f.write('};\n\nstatic const ape_pose_t APE_POSES[APE_NPOSE] = {\n')
        for ext, runs in poses:
            assert all(n < 256 for _, n in runs)
            f.write('    {{%d, %d, %d, %d}, {%s}, {%s}},\n' % (ext[0], ext[1], ext[2], ext[3],
                    ', '.join(str(o) for o, _ in runs), ', '.join(str(n) for _, n in runs)))
        f.write('};\n')
    print('%s: %d polylines, %d points, %d bytes of strokes, %d of tables' % (out, nl, npts, len(blob), len(poses) * 24))


if __name__ == '__main__':
    main()
