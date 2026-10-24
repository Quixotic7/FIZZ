#!/usr/bin/env python3
"""Render the mock-ups of index.html and line.html to PNGs with headless Chromium (never a visible window).

    python3 mockups/render_png.py [SHOT ...]      # default: every shot

Each shot is index.html?shot=ID (one canvas at 1x, top left) or ?sheet=DIR, captured at its own window size.
Chromium runs in the background with a scratch profile; it is stopped by its PID if it lingers.
"""
import os
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'png')
CHROMIUM = '/Applications/Chromium.app/Contents/MacOS/Chromium'
NAMES = ['step-view', 'held-step-note', 'held-step-cond-chance', 'p-lock', 'sound-lock', 'track-overview',
         'pattern-select', 'micro-timing']

SHOTS = {}
for d in ('mod', 'riso'):
    for i, n in enumerate(NAMES, 1):
        SHOTS[f'{d}-{i}'] = (f'shot={d}-{i}', 240, 240, f'{d}-{i:02d}-{n}.png')
    SHOTS[f'sheet-{d}'] = (f'sheet={d}', 2030, 1120, f'sheet-{d}.png')
for k in range(1, 7):
    SHOTS[f'key-{k}'] = (f'shot=key-{k}', 960, 268, f'keybed-{k}.png')
SHOTS['panel'] = ('shot=panel', 960, 650, 'panel-map.png')
LINE = ['idle', 'step-view', 'tracks', 'held-note', 'held-vel', 'held-len', 'held-micro', 'held-ratchet', 'held-cond-chance',
        'held-sound', 'p-lock', 'engine-syn1', 'pattern', 'mix', 'delay', 'tempo', 'mute', 'keys-rec', 'held-trig', 'held-locks',
        'turn-freq', 'turn-decay', 'turn-harm', 'turn-feedback', 'turn-rate']
for i, n in enumerate(LINE, 1):
    SHOTS[f'line-{i}'] = (f'shot=line-{i}', 240, 240, f'line-{i:02d}-{n}.png', 'line.html')
SHOTS['sheet-line'] = ('sheet=line', 2030, 3720, 'sheet-line.png', 'line.html')
SHOTS['legend'] = ('shot=legend', 960, 96, 'led-legend.png')
for i in range(1, 15):                         # P1.5a: fizz-save-pool.html's fourteen screens (render_p15a sets FIZZ's beside)
    SHOTS[f'sp-{i}'] = (f'shot={i}', 240, 240, f'fizz-save-pool-{i:02d}.png', 'fizz-save-pool.html')
for i in range(1, 16):                         # P1.5t: fizz-presets.html's fifteen screens (render_p15t sets FIZZ's beside)
    SHOTS[f'pr-{i}'] = (f'shot={i}', 240, 240, f'fizz-presets-{i:02d}.png', 'fizz-presets.html')
for i in range(1, 9):                          # P1.5u: fizz-names.html's eight screens (render_p15u sets FIZZ's beside)
    SHOTS[f'nm-{i}'] = (f'shot={i}', 240, 240, f'fizz-names-{i:02d}.png', 'fizz-names.html')
GEEK = ['meters', 'numbers', 'voices', 'scope', 'numbers-crash']
for i, n in enumerate(GEEK, 1):                # fizz-geekout.html: the four Geek Out pages (+ NUMBERS after a crash)
    SHOTS[f'go-{i}'] = (f'shot={i}', 240, 240, f'fizz-geekout-{i:02d}-{n}.png', 'fizz-geekout.html')
SHOTS['sheet-go'] = ('sheet=1', 1524, 1140, 'fizz-geekout-sheet.png', 'fizz-geekout.html')


def render(key, scratch):
    query, w, h, name = SHOTS[key][:4]
    page = SHOTS[key][4] if len(SHOTS[key]) > 4 else 'index.html'
    out = os.path.join(OUT, name)
    if os.path.exists(out):
        os.replace(out, out + '.old')
    url = 'file://' + os.path.join(HERE, page) + '?' + query
    prof = os.path.join(scratch, 'chrome-profile-' + key)
    cmd = [CHROMIUM, '--headless=new', '--use-mock-keychain', '--password-store=basic', '--disable-gpu',
           '--no-sandbox', '--no-first-run', '--user-data-dir=' + prof, '--hide-scrollbars',
           f'--window-size={w},{h}', '--timeout=15000', '--virtual-time-budget=3000', '--screenshot=' + out, url]
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0 = time.time()
    while time.time() - t0 < 30:
        if proc.poll() is not None:
            break
        if os.path.exists(out) and os.path.getsize(out) > 0:
            time.sleep(0.5)
            break
        time.sleep(0.2)
    if proc.poll() is None:
        proc.terminate()                      # by its own PID only
        try:
            proc.wait(5)
        except subprocess.TimeoutExpired:
            proc.kill()
    old = out + '.old'
    if os.path.exists(old):
        if os.path.exists(out):
            os.remove(old)
        else:
            os.replace(old, out)
    ok = os.path.exists(out) and os.path.getsize(out) > 0
    print(('ok   ' if ok else 'FAIL ') + name)
    return ok


def main():
    keys = sys.argv[1:] or list(SHOTS)
    os.makedirs(OUT, exist_ok=True)
    scratch = os.environ.get('RENDER_SCRATCH') or tempfile.mkdtemp(prefix='quadrille-render-')
    bad = [k for k in keys if not render(k, scratch)]
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
