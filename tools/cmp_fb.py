# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""P2.6e: the 8-bit frame against the RGB565 drawing, screen by screen (research/SMOOTH-DISPLAY.md 4.8.3's gate).

    python3 tools/cmp_fb.py REF_DIR FB_DIR OUT_DIR [N]

REF_DIR: render_frames of the fast drawing (GFX=fast); FB_DIR: the same through the 8-bit frame (GFX=fb), converted
back through the palette. Every 240 x 240 PNG in both (the sheets, which are made of them, left out). Per screen: the
worst pixel's error (the largest channel difference, 0..255), PSNR, the share of pixels that differ and of those off
by more than 24. OUT_DIR/fb-compare.txt (worst first) and OUT_DIR/fb-worst-NN-name.png for the N worst: the RGB565
frame, the 8-bit one, their difference x4, each at 2x."""
import os
import sys

import numpy as np
from PIL import Image


def load(p):
    return np.asarray(Image.open(p).convert('RGB'), dtype=np.int32)


def main():
    ref, fb, out = sys.argv[1], sys.argv[2], sys.argv[3]
    nworst = int(sys.argv[4]) if len(sys.argv) > 4 else 12
    os.makedirs(out, exist_ok=True)
    rows = []
    for root, _, files in os.walk(ref):
        for f in sorted(files):
            if not f.endswith('.png') or 'sheet' in f:
                continue
            pr = os.path.join(root, f)
            rel = os.path.relpath(pr, ref)
            pf = os.path.join(fb, rel)
            if not os.path.exists(pf):
                rows.append((rel, 999, 0.0, 100.0, 100.0))
                continue
            a, b = load(pr), load(pf)
            if a.shape != b.shape or a.shape[:2] != (240, 240):
                continue
            d = np.abs(a - b)
            px = d.max(axis=2)
            mse = (d.astype(np.float64) ** 2).mean()
            psnr = 99.0 if mse == 0 else 10 * np.log10(255.0 ** 2 / mse)
            rows.append((rel, int(px.max()), psnr, 100.0 * (px > 0).mean(), 100.0 * (px > 24).mean()))
    rows.sort(key=lambda r: (-r[1], r[2]))
    with open(os.path.join(out, 'fb-compare.txt'), 'w') as f:
        f.write('# screen, worst pixel error (0..255), PSNR dB, %% pixels differing, %% off by more than 24\n')
        for r in rows:
            f.write('%-48s %4d %6.2f %7.3f %7.3f\n' % r)
    for i, r in enumerate(rows[:nworst]):
        a, b = load(os.path.join(ref, r[0])), load(os.path.join(fb, r[0]))
        d = np.clip(np.abs(a - b) * 4, 0, 255)
        im = Image.new('RGB', (3 * 480 + 32, 480), (40, 40, 44))
        for j, x in enumerate((a, b, d)):
            im.paste(Image.fromarray(x.astype(np.uint8)).resize((480, 480), Image.NEAREST), (j * 496, 0))
        im.save(os.path.join(out, 'fb-worst-%02d-%s' % (i + 1, r[0].replace('/', '-'))))
    n = len(rows)
    ps = sorted(r[2] for r in rows)
    print('cmp_fb: %d screens; worst pixel error: max %d, median %d; PSNR: min %.1f dB, median %.1f dB; pixels off by '
          'more than 24: mean %.3f %%, max %.3f %% (%s)' %
          (n, rows[0][1], sorted(r[1] for r in rows)[n // 2], ps[0], ps[n // 2], sum(r[4] for r in rows) / n,
           max(r[4] for r in rows), out))


if __name__ == '__main__':
    main()
