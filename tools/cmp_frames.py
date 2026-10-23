#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""P2.1: two render_frames outputs compared pixel by pixel (every PNG under A against the same path under B).

  python3 tools/cmp_frames.py A_DIR B_DIR [--list]

Prints how many images are identical, which differ (with the count of differing pixels and their largest channel
difference) and which are missing on either side; exit 1 if any differs. Phase 2's rule: every render sheet
identical, or the difference explained."""
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def main():
    a, b = Path(sys.argv[1]), Path(sys.argv[2])
    listing = "--list" in sys.argv
    pa = {p.relative_to(a) for p in a.rglob("*.png")}
    pb = {p.relative_to(b) for p in b.rglob("*.png")}
    same, diff = 0, []
    for r in sorted(pa & pb):
        x = np.asarray(Image.open(a / r).convert("RGBA"), dtype=np.int16)
        y = np.asarray(Image.open(b / r).convert("RGBA"), dtype=np.int16)
        if x.shape != y.shape:
            diff.append((r, -1, -1))
            continue
        d = np.abs(x - y).max(axis=2)
        n = int((d > 0).sum())
        if n:
            diff.append((r, n, int(d.max())))
        else:
            same += 1
    for r, n, m in diff:
        print(f"  differs: {r} ({'size' if n < 0 else f'{n} px, at most {m}'})")
    for r in sorted(pa - pb):
        print(f"  only in {a}: {r}")
    for r in sorted(pb - pa):
        print(f"  only in {b}: {r}")
    print(f"{same} identical, {len(diff)} differ, {len(pa - pb)} + {len(pb - pa)} missing")
    if listing:
        for r in sorted(pa & pb):
            print(r)
    return 1 if diff or pa != pb else 0


if __name__ == "__main__":
    sys.exit(main())
