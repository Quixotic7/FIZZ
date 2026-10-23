#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""P2.1: the host's screens beside the device's (two render_frames outputs), pair by pair, for the eye.

  python3 tools/pair_sheet.py HOST_DIR DEVICE_DIR OUT.png NAME [NAME ..] [--scale 2] [--cols 3]

Each NAME is a PNG under both folders (keys.png, p14a-page-03.png, ..). Each pair: the host's on the left, the
device's (QD_DEVICE: three font weights, the procedural TAP ape) on the right, the name above. Used for the
Phase 2 comparison sheets (the fonts, the ape)."""
import sys
from pathlib import Path

from PIL import Image, ImageDraw


def main():
    args = sys.argv[1:]
    scale, cols = 2, 3
    if "--scale" in args:
        i = args.index("--scale")
        scale = int(args[i + 1])
        del args[i:i + 2]
    if "--cols" in args:
        i = args.index("--cols")
        cols = int(args[i + 1])
        del args[i:i + 2]
    host, dev, out, names = Path(args[0]), Path(args[1]), args[2], args[3:]
    w = 240 * scale
    gap, head = 12, 22
    cw = 2 * w + gap
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * cw + (cols + 1) * 2 * gap, rows * (w + head + gap) + gap), (40, 40, 44))
    d = ImageDraw.Draw(sheet)
    for k, n in enumerate(names):
        x = 2 * gap + (k % cols) * (cw + 2 * gap)
        y = gap + (k // cols) * (w + head + gap)
        d.text((x, y + 4), f"{n}   host | device", fill=(220, 220, 220))
        for j, src in enumerate((host, dev)):
            im = Image.open(src / n).convert("RGB").resize((w, w), Image.NEAREST)
            sheet.paste(im, (x + j * (w + gap), y + head))
    sheet.save(out)
    print(f"pair_sheet: {out} ({len(names)} pairs)")


if __name__ == "__main__":
    main()
