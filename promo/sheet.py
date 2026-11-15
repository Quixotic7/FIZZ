#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""A contact sheet of the promo (promo/fizz-short.mp4 -> promo/build/promo_sheet.png): a row a section of make.py's
CAPS, N frames across it (default 6), each labelled with its time, to check the motion (the rider's wheelie, the
ape's poses, the steps filling) without watching it.

    python3 promo/sheet.py [N] [SCALE]"""
import os
import subprocess
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make  # noqa: E402  (CAPS, SECS, W, H)

N = int(sys.argv[1]) if len(sys.argv) > 1 else 6
SCALE = float(sys.argv[2]) if len(sys.argv) > 2 else 0.2


def frame(t):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-ss", "%.3f" % t, "-i", make.OUT, "-frames:v", "1", "-f", "rawvideo",
                          "-pix_fmt", "rgb24", "-"], capture_output=True, check=True).stdout
    return Image.frombytes("RGB", (make.W, make.H), raw)


def main():
    w, h = int(make.W * SCALE), int(make.H * SCALE)
    starts = [c[0] for c in make.CAPS] + [make.SECS]
    sheet = Image.new("RGB", (N * (w + 4), len(make.CAPS) * (h + 18)), (24, 24, 28))
    d = ImageDraw.Draw(sheet)
    for r, (a, b) in enumerate(zip(starts, starts[1:])):
        for i in range(N):
            t = a + (b - a) * (i + 0.5) / N
            x, y = i * (w + 4), r * (h + 18)
            sheet.paste(frame(t).resize((w, h)), (x, y + 16))
            d.text((x + 3, y + 2), "%s  %.2f s" % (make.CAPS[r][1], t), fill=(255, 220, 0))
    out = os.path.join(make.B, "promo_sheet.png")
    sheet.save(out)
    print(out)


if __name__ == "__main__":
    main()
