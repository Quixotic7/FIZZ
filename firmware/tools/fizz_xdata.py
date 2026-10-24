#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""FIZZ's data blob onto an FM-1 running FIZZ, over USB-MIDI, and FIZZ's port counters back.

The blob (firmware/app/build/fizz-xdata.bin: Inter Tight and the TAP ape's lines, core/qd_config.h QD_XDATA) lives in
the FM-1's data flash at 0xE5000 (fm1-base's PLAT_XDATA1), outside the update package. Install the package first
(fm1-base's tools/fm1_install.py); FIZZ then shows "FIZZ NEEDS ITS DATA" until this has run once. Each build stamps
its blob's length and CRC-32 into the firmware, and the unit takes only its own build's blob.

  fizz_xdata.py push [--blob FILE] [--port NAME]   write the blob (the unit answers FZOK, or FZER and a code)
  fizz_xdata.py stats [--reset] [--port NAME]      the port's counters: the audio interrupt, the locks, the frames,
                                                   a frame's time by phase and by screen (firmware/BUDGET.md)
  fizz_xdata.py wipe [--port NAME]                 FIZZ's store (0x97000..0xDFFFF) erased, FIZZ started again on it
  fizz_xdata.py info [--blob FILE]                 the blob's length and CRC-32 (no unit)
Safe mode (the store not mounted): hold REC while switching the FM-1 on.

The transfer is the update's own (fm1-base app_usb_cmd, plat_host_read): one command, 46 "FZXD" len crc off, then the
unit asks for the blob in 512-byte reads and this answers them. Needs mido with python-rtmidi, as fm1_install.py.
FZER codes: 1 another build's blob, 2 FIZZ already running, 3 erase, 4 a read or write failed, 5 the trailer, 6 the
check after writing.

To drive the unit from here while measuring, fm1-base's own remote works with FIZZ (presses, keys, knobs, MASTER):
  python3 firmware/fm1-base/tools/fm1_remote.py tap HOME wait 2000 tap PLAY wait 5000 tap PLAY
then `fizz_xdata.py stats`. (fm1-base takes these between frames: a press waits for the frame being drawn.)"""
import argparse
import struct
import sys
import time
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "fm1-base" / "tools"))
import fm1_install as fi  # noqa: E402

XDATA_OFF = 0xE5000                             # plat.h PLAT_XDATA1_LO (fizz.ld)
BLOB = HERE.parent / "app" / "build" / "fizz-xdata.bin"


MODES = ["idle", "step", "keys", "tracks"]
VIEWS = ["roll", "steps", "edit", "view3"]
SCREENS = {f"s{m}.{v}": f"{MODES[m]}/{VIEWS[v]}" for m in range(4) for v in range(4)}


def is_fizz(ident):
    return ident.text.startswith("FM-1_46") and not ident.loader


def find(port):
    up = fi.Updater(fi.MidoBackend(), port)
    dev = up.find(is_fizz)
    if not dev:
        raise SystemExit("no FM-1 running FIZZ answered (identity FM-1_46..)" + (f" on {port!r}" if port else ""))
    return dev


def push(a):
    blob = Path(a.blob).read_bytes()
    crc = zlib.crc32(blob) & 0xFFFFFFFF
    dev = find(a.port)
    print(f"FIZZ {dev.id.text} on {dev.name}: blob {len(blob)} B, CRC-32 {crc:08x}, to flash {XDATA_OFF:#x}")
    link = dev.link
    link.drain()
    link.send(fi.frame(0x46, b"FZXD" + struct.pack("<III", len(blob), crc, XDATA_OFF)))
    served, last = 0, time.monotonic()
    try:
        while time.monotonic() - last < 30.0:
            p = link.read(1.0)
            if p is None:
                continue
            r = fi.parse_request(p)
            if r:
                fl, addr, n = r
                last = time.monotonic()
                if n > fi.MAXDATA or addr + n > len(blob):
                    raise SystemExit(f"the unit asked for {addr:#x}+{n}, outside the blob")
                link.send(fi.response(addr, blob[addr:addr + n], fl))
                served += 1
                print(f"\r{min(100, (addr + n) * 100 // len(blob)):3d}%", end="", flush=True)
                continue
            u = fi.unpack7(p[1:-1]) if len(p) > 2 and p[0] == 0xF0 and p[-1] == 0xF7 else b""
            if len(u) >= 15 and u[:3] == b"\x00\x59\x46":
                tag, code = u[6:10].decode("latin-1"), int.from_bytes(u[10:14], "little")
                print()
                if tag == "FZOK":
                    print("done: the unit holds this build's blob" + (" (it already did)" if code == 0 else ""))
                    return 0
                print(f"the unit refused or failed: FZER {code} (see --help)")
                return 1
        print(f"\nno answer from the unit after {served} reads")
        return 1
    finally:
        link.close()


def stats(a):
    dev = find(a.port)
    link = dev.link
    link.drain()
    link.send(bytes([0xF0, 0x7D, 0x46, 0x50] + ([0x7F] if a.reset else list(b"BNCH") if a.bench else list(b"SCAN") if a.scan else []) + [0xF7]))
    if a.reset:
        print("counters started again")
        link.close()
        return 0
    end, got = time.monotonic() + 4.0, {}
    try:
        while time.monotonic() < end and len(got) < 9:
            p = link.read(end - time.monotonic())
            if p and p[:4] == bytes([0xF0, 0x7D, 0x46, 0x50]) and len(p) > 5:
                got[p[4]] = p[5:-1].decode("ascii", "replace")
        if not got:
            print("no answer")
            return 1
        for part, title in ((1, "sound, locks, frames"), (2, "a frame by phase (us, averages)"),
                            (3, "by screen: frames/avg us/max us"),
                            (4, "inside the audio interrupt (us a half), FIZZ's state"),
                            (5, "the platform: the guard, MIDI and SysEx in, the scan, the locks"),
                            (6, "core 1 (firmware/DUALCORE.md): state, jobs, failures"),
                            (7, "the screen's bursts and the LEDs (P2.7)"),
                            (8, "the speed at boot: the voice bench, the clock, the RAM regions"),
                            (9, "the panel's scanline reads (stats --scan: a second of them)")):
            line = got.get(part, "(no answer)")
            if part == 3:                   # s<mode>.<view>: name them
                for k, v in SCREENS.items():
                    line = line.replace(k + "=", v + "=")
            print(f"{title}:\n  {line}")
        return 0
    finally:
        link.close()


def wipe(a):
    dev = find(a.port)
    link = dev.link
    link.drain()
    link.send(bytes([0xF0, 0x7D, 0x46, 0x50]) + b"WIPE" + bytes([0xF7]))
    end = time.monotonic() + 15.0
    try:
        while time.monotonic() < end:
            p = link.read(end - time.monotonic())
            if p and p[:5] == bytes([0xF0, 0x7D, 0x46, 0x50, 0x09]):
                print("the store's region erased, FIZZ started again on it:", p[5:-1].decode("ascii", "replace"))
                return 0
        print("no answer")
        return 1
    finally:
        link.close()


def info(a):
    blob = Path(a.blob).read_bytes()
    print(f"{a.blob}: {len(blob)} B, CRC-32 {zlib.crc32(blob) & 0xFFFFFFFF:08x}, at flash {XDATA_OFF:#x}..{XDATA_OFF + len(blob):#x}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("push", "stats", "wipe", "info"):
        p = sub.add_parser(name)
        if name in ("push", "info"):
            p.add_argument("--blob", default=str(BLOB))
        if name != "info":
            p.add_argument("--port", metavar="NAME", help="MIDI port to use (part of its name)")
        if name == "stats":
            p.add_argument("--bench", action="store_true", help="run the voice bench again now (line 8: bench_now; "
                                                                 "interrupts off ~10 ms, one half of the sound late)")
            p.add_argument("--scan", action="store_true", help="read the panel's scanline for a second (line 9; the "
                                                                "screen is not drawn meanwhile)")
            p.add_argument("--reset", action="store_true", help="start the counters again")
    a = ap.parse_args()
    return {"push": push, "stats": stats, "wipe": wipe, "info": info}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
