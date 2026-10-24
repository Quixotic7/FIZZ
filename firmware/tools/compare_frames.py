#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""fm1-base's host simulator against FIZZ's own render_frames, pixel for pixel (firmware/app/tests/compare.txt).

  compare_frames.py [--keep DIR]

Builds render_frames at the device's sizes and with the unit's drawing (FIZZ's tools/Makefile, PROFILE=-DQD_DEVICE,
GFX= as fm1.toml's gen line has it) into a temporary directory,
renders its frames at 0, 1000, 2200 and 2500 ms, builds and runs the simulator's compare.txt, and compares each raw
RGB565 dump with render_frames' PNG (its 8-bit expansion turned back into RGB565). Exit 0 when every frame matches.
Needs Pillow."""
import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
FW = HERE.parent
REPO = FW.parent
TIMES = (0, 1000, 2200, 2500)


def q565(path):
    im = Image.open(path).convert("RGB")
    return [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3) for r, g, b in im.getdata()]


def raw565(path):
    d = Path(path).read_bytes()
    return [d[i] << 8 | d[i + 1] for i in range(0, len(d), 2)]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--keep", type=Path, help="work here (and keep it) instead of a temporary directory")
    a = ap.parse_args()
    work = a.keep or Path(tempfile.mkdtemp(prefix="fizz-compare-"))
    work.mkdir(parents=True, exist_ok=True)
    b, ref, sim = work / "rf-build", work / "rf-out", work / "sim"
    for d in (b, ref, sim):
        d.mkdir(exist_ok=True)
    gfx = re.search(r"--gfx (\w+)", (FW / "app/fm1.toml").read_text())     # (the unit's drawing: render_frames' too)
    gfx = gfx.group(1) if gfx else "plutovg"
    print(f"the drawing: {gfx}")
    subprocess.run(["make", "-s", "-C", str(REPO / "tools"), "PROFILE=-DQD_DEVICE", f"GFX={gfx}", f"B={b}",
                    f"{b}/render_frames", "-j8"], check=True, stderr=subprocess.DEVNULL)
    # (its exit status is its own budget checks' too, e.g. the arena's margin: the frames are what is compared here)
    subprocess.run([str(b / "render_frames"), str(ref), "--ms", "2600", "--at", ",".join(map(str, TIMES))],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, str(FW / "build.py"), "--host"], check=True, stdout=subprocess.DEVNULL)
    subprocess.run([str(FW / "app/build/host/fizz_host"), str(FW / "app/tests/compare.txt"), str(sim)], check=True,
                   stdout=subprocess.DEVNULL)
    bad = 0
    for t in TIMES:
        r, s = q565(ref / f"frame-{t:05d}.png"), raw565(sim / f"cmp-{t:05d}.raw")
        diff = [i for i in range(len(r)) if r[i] != s[i]]
        rows = sorted({i // 240 for i in diff})
        print(f"{t:5d} ms: {len(diff)} pixels differ" + (f" (rows {rows[0]}..{rows[-1]})" if rows else ""))
        bad += bool(diff)
    print(("all frames match" if not bad else f"{bad} frames differ") + f"  ({work})")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
