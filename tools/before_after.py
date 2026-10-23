#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""P1.3v: before / after sheets per screen family, for the clarity pass (PLAN §9a).

    python3 tools/before_after.py OUT BEFORE_DIR

OUT holds the renders now (make -C tools frames); BEFORE_DIR the same files rendered before the pass. Writes
OUT/p13v-<family>-sheet.png: each screen as a pair, before on the left and after on the right, at 1.5x.
"""
import fnmatch
import os
import sys

from PIL import Image, ImageDraw

FAMILIES = {
    'lanes': ['p13b-02-lane-*.png'],
    'held': ['p13b-0[4-9]*-held-*.png', 'p13b-1[09]-held-*.png', 'p13b-20-held-*.png', 'p13b-held-empty.png',
             'p13d-held-*.png', 'p13e-default-*.png'],
    'edit': ['p13s-edit-*.png', 'p13u-note-*.png'],
    'steps': ['p13q-steps-*.png'],
    'maps': ['p13b-f1*.png', 'p13b-f2*.png', 'p13b-f3*.png', 'p13b-shift.png', 'p13c-sel*.png', 'p13c-seq*.png',
             'p13d-seq-*.png', 'p13d-f3-*.png'],
    'takeovers': ['p13b-tk-*.png', 'p13c-plock-turn.png', 'p13d-plock-track.png', 'p13f-select-*.png',
                  'p13c-oct-*.png', 'p13c-bank*.png', 'picker.png'],
    'keys': ['keys.png', 'p13b-keys*.png', 'p13b-18-keys-rec.png'],
    'tracks': ['tracks-*.png'],
}
S = 1.5


def main():
    out, before = sys.argv[1], sys.argv[2]
    names = sorted(os.listdir(before))
    for fam, pats in FAMILIES.items():
        files = []
        for pat in pats:
            for n in names:
                if fnmatch.fnmatch(n, pat) and n not in files and os.path.exists(os.path.join(out, n)):
                    files.append(n)
        if not files:
            continue
        w = int(240 * S)
        cell_w, cell_h, cols, pad = 2 * w + 12, w + 26, 2, 16
        rows = (len(files) + cols - 1) // cols
        sheet = Image.new('RGB', (pad + cols * (cell_w + pad), 40 + rows * (cell_h + pad)), (28, 28, 31))
        d = ImageDraw.Draw(sheet)
        d.text((pad, 12), f'FIZZ P1.3v, the clarity pass: {fam} (left before, right after; 1.5x)', fill=(230, 230, 230))
        for i, n in enumerate(files):
            x = pad + (i % cols) * (cell_w + pad)
            y = 40 + (i // cols) * (cell_h + pad)
            d.text((x, y), n, fill=(200, 200, 200))
            for j, src in enumerate((os.path.join(before, n), os.path.join(out, n))):
                im = Image.open(src).convert('RGB').resize((w, w), Image.NEAREST)
                sheet.paste(im, (x + j * (w + 12), y + 18))
        path = os.path.join(out, f'p13v-{fam}-sheet.png')
        sheet.save(path)
        print(path, f'({len(files)} screens)')


if __name__ == '__main__':
    main()
