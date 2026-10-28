#!/usr/bin/env python3
"""Render the FIZZ iOS mock-ups (fizz-ios.html) to PNGs with headless Chromium (never a visible window).

    python3 ios/mockups/render_png.py [ID ...]      # default: every screen and the sheet

Each screen is fizz-ios.html?shot=ID&still=1 at its device's size (frame included), the sheet ?sheet=1&still=1.
Output: ios/mockups/png/fizz-ios-NN-ID.png and fizz-ios-sheet.png. Chromium runs in the background with a scratch
profile and is stopped by its own PID if it lingers.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'png')
PAGE = os.path.join(HERE, 'fizz-ios.html')
CHROMIUM = '/Applications/Chromium.app/Contents/MacOS/Chromium'
PHONE = (446, 900)        # 414 x 868 frame + 16 px around
IPAD = (1248, 888)        # 1216 x 856 frame + 16 px around
SHEET = (1640, 4700)    # trimmed to its content afterwards


def screens():
    """The screen ids, in order, read from the page's SCREENS list (ipad ones flagged)."""
    src = open(PAGE, encoding='utf-8').read()
    body = src[src.index('const SCREENS = ['):]
    return [(m.group(1), 'ipad: true' in m.group(0)) for m in re.finditer(r'\{ id: "([a-z0-9-]+)"[^\n]*', body)]


def trim(path):
    """Cut the sheet's empty bottom (the page background) down to a 40 px margin."""
    from PIL import Image
    im = Image.open(path).convert('RGB')
    w, h = im.size
    px = im.load()
    bg = px[w - 2, h - 2]
    y = h - 1
    while y > 0 and all(px[x, y] == bg for x in range(0, w, 7)):
        y -= 1
    im.crop((0, 0, w, min(h, y + 40))).save(path)


def render(query, size, name, scratch):
    out = os.path.join(OUT, name)
    tmp = out + '.new.png'
    if os.path.exists(tmp):
        os.remove(tmp)
    prof = os.path.join(scratch, 'profile-' + name)
    cmd = [CHROMIUM, '--headless=new', '--use-mock-keychain', '--password-store=basic', '--disable-gpu', '--no-sandbox',
           '--no-first-run', '--user-data-dir=' + prof, '--hide-scrollbars', f'--window-size={size[0]},{size[1]}',
           '--timeout=15000', '--virtual-time-budget=3000', '--screenshot=' + tmp, 'file://' + PAGE + '?' + query]
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0 = time.time()
    while time.time() - t0 < 40:
        if proc.poll() is not None:
            break
        if os.path.exists(tmp) and os.path.getsize(tmp) > 0:
            time.sleep(0.6)
            break
        time.sleep(0.2)
    if proc.poll() is None:
        proc.terminate()                       # by its own PID only
        try:
            proc.wait(5)
        except subprocess.TimeoutExpired:
            proc.kill()
    ok = os.path.exists(tmp) and os.path.getsize(tmp) > 0
    if ok and name.endswith('sheet.png'):
        trim(tmp)
    if ok:
        os.replace(tmp, out)
    print(('ok   ' if ok else 'FAIL ') + name, flush=True)
    return ok


def main():
    want = set(sys.argv[1:])
    os.makedirs(OUT, exist_ok=True)
    scratch = os.environ.get('RENDER_SCRATCH') or tempfile.mkdtemp(prefix='fizz-ios-render-')
    jobs = []
    for i, (sid, pad) in enumerate(screens(), 1):
        if not want or sid in want:
            jobs.append((f'shot={sid}&still=1', IPAD if pad else PHONE, f'fizz-ios-{i:02d}-{sid}.png'))
    if not want or 'sheet' in want:
        jobs.append(('sheet=1&still=1', SHEET, 'fizz-ios-sheet.png'))
    with ThreadPoolExecutor(4) as ex:
        res = list(ex.map(lambda j: render(*j, scratch), jobs))
    if not os.environ.get('RENDER_SCRATCH'):
        shutil.move(scratch, scratch + '-done')   # (left for the OS to clean; no rm -rf)
    sys.exit(0 if all(res) else 1)


if __name__ == '__main__':
    main()
