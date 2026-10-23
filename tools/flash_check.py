#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""P2.1: a flash image (the owner's Standalone flash, a saved one) through this core: everything it holds as read,
then every pattern record migrated to the newest format ('P' 5) and read again after a restart; the two must be the
same, line for line (tools/flash_dump.c). With --old, a flash_dump built from an older core (the format before) reads
the same image: its view must be this core's too.

  python3 tools/flash_check.py IMAGE [IMAGE ..] [--old OLD_FLASH_DUMP] [--dump BUILD/flash_dump]

The images are only read: each is copied into a temporary folder first, and the migrated image is written there.
Exit 1 if anything differs."""
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    args = sys.argv[1:]
    old, dump = None, os.path.join(HERE, 'build', 'flash_dump')
    if '--old' in args:
        i = args.index('--old')
        old = args[i + 1]
        del args[i:i + 2]
    if '--dump' in args:
        i = args.index('--dump')
        dump = args[i + 1]
        del args[i:i + 2]
    if not args:
        print(__doc__)
        return 2
    bad = 0
    with tempfile.TemporaryDirectory() as tmp:
        for img in args:
            src = os.path.join(tmp, 'image.bin')
            shutil.copyfile(img, src)
            out = subprocess.run([dump, src, '--migrate', os.path.join(tmp, 'migrated.bin')], capture_output=True,
                                 text=True, check=True).stdout.split('\n')
            i = next(k for k, l in enumerate(out) if l.startswith('---- migrated'))
            before, after = out[:i], out[i + 1:]
            while after and not after[-1]:
                after.pop()
            same = before == after
            line = (f'{os.path.basename(img)}: {sum(" header " in l for l in before)} projects, '
                    f'{sum(" used 1 " in l for l in before)} patterns used, {sum(" pool " in l for l in before)} pool '
                    f'sounds, {sum(l.startswith("mine ") for l in before)} MY presets; {out[i][5:]}: '
                    f'{"the same" if same else "DIFFERENT"}')
            if old:
                o = subprocess.run([old, img], capture_output=True, text=True, check=True).stdout.split('\n')
                while o and not o[-1]:
                    o.pop()
                same_old = o == before
                line += f'; the older core\'s view: {"the same" if same_old else "DIFFERENT"}'
                same = same and same_old
            print(line)
            bad += not same
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
