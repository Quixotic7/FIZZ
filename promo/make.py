#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""FIZZ's promo: 30 s, 1080 x 1920 (Shorts / Reels), from the firmware's own screen and sound.

    make -C tools tour                                        # the host app with its film command and the demo song
    mkdir -p promo/build && cd promo/build
    ../../tools/build/tour ../song.txt .                      # sunfade.bin: the demo project SUNFADE
    ../../tools/build/tour ../take.txt .                      # idle.rgb, take.rgb, take.wav
    python3 ../make.py                                        # promo/fizz-short.mp4

The picture is the take's 240 x 240 frames at 4x; the lettering is the firmware's typeface (assets/fonts)."""
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
B = os.path.join(HERE, "build")
OUT = os.path.join(HERE, "fizz-short.mp4")
FONTS = os.path.join(HERE, "..", "assets", "fonts")
W, H, FPS, SECS = 1080, 1920, 30, 30.0
LCD, LX, LY = 960, 60, 480
FR = 240 * 240 * 3

BLACK, WHITE, GREY = (0, 0, 0), (255, 255, 255), (150, 150, 158)
BLUE, GREEN, ORANGE, PINK, LIME, VIOLET = (0x3D, 0x7B, 0xFF), (0x2E, 0xD5, 0x8A), (0xFF, 0x6B, 0x2B), (0xFF, 0x3D, 0xC8), (0xC8, 0xF0, 0x3C), (0x9B, 0x6B, 0xFF)

# (from s, line 1, line 2, line 2's colour): the take's marks (promo/take.txt)
CAPS = [
    (0.00, "FIZZ", "An FM groovebox for the FM-1", BLUE),
    (2.73, "Six tracks", "16 voices of four-operator FM", ORANGE),
    (5.45, "Sound locks", "Kick, snare, hat: one track", GREEN),
    (8.18, "8 algorithms", "A deep sound editor", PINK),
    (10.91, "Black hole reverb", "And a ping-pong delay", VIOLET),
    (13.64, "Lock any step", "Filter sweeps, written in", BLUE),
    (16.36, "Generative arps", "MIDI FX: arp, random, chance", ORANGE),
    (19.09, "Mix it", "Levels, mutes, solos", GREEN),
    (21.82, "101 new presets", "Five drum kits. Pads. Bells.", LIME),
    (24.55, "USB host", "Bluetooth MIDI", VIOLET),
    (27.50, "FIZZ", "Free. Open source.", PINK),
]
TITLE_END, CARD = 2.73, 27.50


def font(weight, size):
    return ImageFont.truetype(os.path.join(FONTS, "InterTight-%d.ttf" % weight), size)


F1, F2, FW, FS, FU = font(700, 104), font(500, 58), font(700, 76), font(400, 40), font(600, 54)


def centre(d, y, s, f, col):
    w = d.textlength(s, font=f)
    if w > W - 80:                                      # a long line: smaller, to fit
        f = f.font_variant(size=int(f.size * (W - 80) / w))
        w = d.textlength(s, font=f)
    d.text(((W - w) / 2, y), s, font=f, fill=col)


def wordmark(d, y):
    """FIZZ, a letter a colour (the idle screen's)."""
    cols = (BLUE, GREEN, ORANGE, PINK)
    ws = [d.textlength(c, font=FW) for c in "FIZZ"]
    gap = 18
    x = (W - sum(ws) - 3 * gap) / 2
    for c, w, col in zip("FIZZ", ws, cols):
        d.text((x, y), c, font=FW, fill=col)
        x += w + gap


def base(accent):
    im = Image.new("RGB", (W, H), BLACK)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((LX - 22, LY - 22, LX + LCD + 22, LY + LCD + 22), 40, outline=(38, 38, 44), width=3)
    d.rounded_rectangle((LX - 12, LY - 12, LX + LCD + 12, LY + LCD + 12), 30, outline=accent, width=4)
    wordmark(d, 1540)
    centre(d, 1650, "Firmware for the M-VAVE FM-1", FS, GREY)
    return im


def main():
    idle = open(os.path.join(B, "idle.rgb"), "rb").read()
    take = open(os.path.join(B, "take.rgb"), "rb").read()
    bases = {}
    layers = {}

    def layer(k, a1, a2):
        key = (k, a1, a2)
        if key not in layers:
            _, a, b, col = CAPS[k]
            if col not in bases:
                bases[col] = base(col)
            im = bases[col].copy()
            d = ImageDraw.Draw(im)
            if CAPS[k][0] >= CARD:
                d.rectangle((0, 1520, W, 1740), fill=BLACK)
                centre(d, 1560, "quixotic7.github.io/FIZZ", FU, WHITE)
                centre(d, 1650, "Install it from your browser", FS, GREY)
            # the two lines rise in: a1, a2 0..8 (their way up and their light)
            for s, f, y, c, n in ((a, F1, 150, WHITE, a1), (b, F2, 290, col, a2)):
                if n > 0:
                    k8 = n / 8.0
                    centre(d, y + int((1 - k8) * 26), s, f, tuple(int(v * k8) for v in c))
            layers[key] = im
            if len(layers) > 60:
                layers.pop(next(iter(layers)))
        return layers[key]

    ff = subprocess.Popen(
        ["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "%dx%d" % (W, H), "-r", str(FPS),
         "-i", "-", "-i", os.path.join(B, "take.wav"), "-t", str(SECS),
         "-af", "volume=7.5dB,alimiter=limit=0.89:level=0,afade=t=out:st=%.2f:d=1.4" % (SECS - 1.4),
         "-c:v", "libx264", "-preset", "slow", "-crf", "15", "-pix_fmt", "yuv420p",
         "-c:a", "aac", "-b:a", "192k", "-ar", "44100", "-movflags", "+faststart", OUT],
        stdin=subprocess.PIPE)
    nf = int(SECS * FPS)
    for f in range(nf):
        t = f / FPS
        k = max(i for i, c in enumerate(CAPS) if c[0] <= t + 1e-6)
        dt = t - CAPS[k][0]
        a1 = max(0, min(8, int(dt * 30) + 1))
        a2 = max(0, min(8, int((dt - 0.18) * 30)))
        im = layer(k, a1, a2).copy()
        if t < TITLE_END:
            src, i = idle, f
        elif t >= CARD:
            src, i = idle, min(85, 30 + int((t - CARD) * FPS))  # the title again, held when it is whole
        else:
            src, i = take, f
        scr = Image.frombytes("RGB", (240, 240), src[i * FR:(i + 1) * FR]).resize((LCD, LCD), Image.NEAREST)
        if t >= SECS - 0.5:                                     # out to black
            scr = Image.blend(scr, Image.new("RGB", scr.size, BLACK), min(1.0, (t - (SECS - 0.5)) / 0.5))
        im.paste(scr, (LX, LY))
        ff.stdin.write(im.tobytes())
    ff.stdin.close()
    if ff.wait():
        sys.exit("ffmpeg failed")
    print(OUT)


if __name__ == "__main__":
    main()
