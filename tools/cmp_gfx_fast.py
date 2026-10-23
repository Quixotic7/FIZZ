#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""The fast drawing against plutovg, frame by frame (tools/gfx_fast.mk gfx-compare):

    cmp_gfx_fast.py PVG_DIR FAST_DIR OUT_DIR

Every 240 x 240 PNG render_frames wrote in both folders (the contact sheets left out) is compared: the difference of a
pixel is its largest channel's (8 bits, from the panel's RGB565). Per screen (the PNG's name to its last '-' or
digit: p14a-page, tap-ape, fizz-logo/f, ..): the frames, the mean difference, the worst pixel, the share of pixels off
by more than one RGB565 step (> 8), and the difference after a 3 x 3 blur (a sub-pixel shift of a glyph or an edge
is large pixel by pixel and gone under the blur: what the eye sees at the panel's 130 dpi). The verdict: "same" when
the blurred difference stays under 48 and the mean under 3, else "look".

Writes OUT_DIR/gfx-compare.txt and OUT_DIR/gfx-compare-sheet.png: the 12 frames that differ most (blurred), each
plutovg | fast | the difference x4, at 2x.
"""
import os
import re
import sys

import numpy as np
from PIL import Image, ImageDraw


def load(p):
    a = np.asarray(Image.open(p).convert("RGB")).astype(np.int16)
    return a if a.shape == (240, 240, 3) else None


def blur(a):
    p = np.pad(a.astype(np.int32), ((1, 1), (1, 1), (0, 0)), mode="edge")
    s = sum(p[dy:dy + 240, dx:dx + 240] for dy in range(3) for dx in range(3))
    return s // 9


def screen_of(rel):
    base = rel[:-4]
    if "/" in base:
        return base.split("/")[0]
    m = re.match(r"(.*?)[-_]?\d+$", base)
    base = m.group(1) if m and m.group(1) else base
    parts = base.split("-")
    return "-".join(parts[:2]) if len(parts) > 1 else base


def main():
    pv, fa, out = sys.argv[1], sys.argv[2], sys.argv[3]
    rows, per = [], {}
    for root, _, files in os.walk(pv):
        for f in sorted(files):
            if not f.endswith(".png"):
                continue
            rel = os.path.relpath(os.path.join(root, f), pv)
            q = os.path.join(fa, rel)
            if not os.path.exists(q):
                continue
            a, b = load(os.path.join(pv, rel)), load(q)
            if a is None or b is None:
                continue
            d = np.abs(a - b).max(axis=2)
            bd = np.abs(blur(a) - blur(b)).max(axis=2)
            r = (rel, float(d.mean()), int(d.max()), float((d > 8).mean()), int(bd.max()), float(bd.mean()))
            rows.append(r)
            per.setdefault(screen_of(rel), []).append(r)
    rows.sort(key=lambda r: (-r[4], -r[1]))
    lines = ["the fast drawing against plutovg, render_frames at the device's sizes: %d frames" % len(rows),
             "%-28s %6s %8s %6s %8s %10s  %s" % ("screen", "frames", "mean", "max", ">1 step", "blur max", "verdict")]
    same = 0
    for s in sorted(per):
        rs = per[s]
        mean = sum(r[1] for r in rs) / len(rs)
        mx = max(r[2] for r in rs)
        off = sum(r[3] for r in rs) / len(rs)
        bmx = max(r[4] for r in rs)
        ok = bmx < 48 and mean < 3.0
        same += ok
        lines.append("%-28s %6d %8.2f %6d %7.2f%% %10d  %s" % (s, len(rs), mean, mx, 100 * off, bmx,
                                                              "same" if ok else "look"))
    allmean = sum(r[1] for r in rows) / max(1, len(rows))
    lines.append("all: mean %.2f, worst pixel %d, pixels off by more than one step %.2f%%; screens 'same' %d of %d" % (
        allmean, max(r[2] for r in rows), 100 * sum(r[3] for r in rows) / max(1, len(rows)), same, len(per)))
    lines.append("")
    lines.append("the 12 frames that differ most (blurred): file, mean, max, >1 step, blur max")
    for r in rows[:12]:
        lines.append("  %-44s %6.2f %4d %6.2f%% %4d" % (r[0], r[1], r[2], 100 * r[3], r[4]))
    txt = "\n".join(lines) + "\n"
    with open(os.path.join(out, "gfx-compare.txt"), "w") as f:
        f.write(txt)
    print(txt)

    # the sheet: plutovg | fast | |difference| x 4, at 2x
    W, H, pad, cap = 3 * 480 + 4 * 12, 480 + 30, 12, 30
    sheet = Image.new("RGB", (W, 12 * H + pad), (28, 28, 30))
    dr = ImageDraw.Draw(sheet)
    for i, r in enumerate(rows[:12]):
        a = Image.open(os.path.join(pv, r[0])).convert("RGB")
        b = Image.open(os.path.join(fa, r[0])).convert("RGB")
        d = np.clip(np.abs(np.asarray(a).astype(np.int16) - np.asarray(b).astype(np.int16)) * 4, 0, 255)
        dimg = Image.fromarray(d.astype(np.uint8))
        y = pad + i * H
        dr.text((pad, y + 6), "%s   mean %.2f  max %d  blur max %d   (plutovg | fast | difference x4)" % (
            r[0], r[1], r[2], r[4]), fill=(220, 220, 220))
        for k, im in enumerate((a, b, dimg)):
            sheet.paste(im.resize((480, 480), Image.NEAREST), (pad + k * (480 + pad), y + cap))
    sheet.save(os.path.join(out, "gfx-compare-sheet.png"))
    print(os.path.join(out, "gfx-compare-sheet.png"))


if __name__ == "__main__":
    main()
