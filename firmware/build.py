#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""FIZZ's firmware build: fm1-base's tools/build.py (the submodule, unchanged) with what FIZZ needs on top.

  firmware/build.py [--release X.Y[.Z]]     the unit's package: firmware/app/build/fizz.fwsc, fizz-xdata.bin, the
                                            size report fizz-size.txt
  firmware/build.py --host                  the host simulator: firmware/app/build/host/fizz_host
  firmware/build.py --web                   the browser emulator (Emscripten): firmware/app/build/emu/

What it adds to fm1-base's build (each a small patch of its module, applied here):
  - the toolchain's container sees the whole repository (FIZZ's core/ and third_party/ are above firmware/): its root
    is the repository, not fm1-base's and the app's common directory;
  - FIZZ's units find newlib's headers (the JieLi toolchain's pi32v2/include, after everything else) and link newlib's
    libm, libc and the compiler's runtime (r3: the FPU's), which an fm1-base app without Bluetooth links nothing of;
  - the link script is firmware/app/fizz.ld (FIZZ's zero-initialised data in the pool; the data blob at the data
    flash's address), with a map (build/fizz.map);
  - after the link: the data blob (.qd_xdata) cut out into fizz-xdata.bin, checked against the update loader's record
    rule (plat.h: never "AT" at 0xF06 of a sector), its length and CRC-32 stamped into the image (fz_port.c);
  - fm1-base's check refuses soft doubles (the FPU is single precision): FIZZ's core still has some (BUDGET.md), so
    here it is a note with their list, not a failure;
  - the size report (fizz-size.txt): the regions, the blob, the biggest symbols.
Defaults for this Mac: JIELI_TOOLCHAIN ~/.jieli/toolchain, AC79_SDK ~/fw-AC79_AIoT_SDK (only uboot.boot, cfg_tool.bin,
cfg/eq_cfg_hw.bin are read), DYLD_LIBRARY_PATH /opt/homebrew/lib (Pillow's raqm for fm1-base's font generator)."""
import importlib.util
import os
import re
import struct
import subprocess
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
BASE = HERE / "fm1-base"
APP = HERE / "app"
XDATA_LO, XDATA_HI = 0xE5000, 0xFC000          # plat.h PLAT_XDATA1
XIP = lambda off: 0x02000000 + off - 0x4000     # felucca-bsp FM1_XIP

for k, v in (("JIELI_TOOLCHAIN", Path.home() / ".jieli" / "toolchain"), ("AC79_SDK", Path.home() / "fw-AC79_AIoT_SDK"),
             ("DYLD_LIBRARY_PATH", Path("/opt/homebrew/lib"))):
    if k not in os.environ and v.exists():
        os.environ[k] = str(v)

spec = importlib.util.spec_from_file_location("fm1b_build", BASE / "tools" / "build.py")
fb = importlib.util.module_from_spec(spec)
sys.modules["fm1b_build"] = fb
spec.loader.exec_module(fb)

# ---- the container sees the repository
_commonpath = os.path.commonpath
os.path.commonpath = lambda paths: str(REPO) if {Path(p).resolve() for p in paths} == {BASE, APP} else _commonpath(paths)


# ---- newlib's headers and libraries
def jieli(rel):
    return f"/opt/jieli/{rel}" if fb.use_docker() else str(fb.toolchain() / rel)


_tc = fb.tc


def tc(tool, *args):
    args = list(args)
    if tool == "cc" and "-c" in args and any(isinstance(a, Path) and a.suffix == ".c" for a in args):
        args = ["-idirafter", jieli("pi32v2/include"), *args]
    if tool == "pi32v2/bin/ld":
        for i, a in enumerate(args):
            if isinstance(a, Path) and a.name in ("app.ld", "app_wifi.ld"):
                args[i] = APP / "fizz.ld"
        o = args.index("-o")
        args[o:o] = ["--start-group", jieli("pi32v2/lib/r3/libm.a"), jieli("pi32v2/lib/r3/libc.a"),
                     jieli("pi32v2/lib/r3/libcompiler_rt.a"), "--end-group", "-Map", fb.OUT / "fizz.map"]
    return _tc(tool, *args)


fb.tc = tc

# ---- soft doubles: a note
_check = fb.check


def check(img, syms, dis, rt):
    errors, notes = _check(img, syms, dis, rt)
    for e in [e for e in errors if e.startswith("soft-double routines linked")]:
        errors.remove(e)
        notes.append("FIZZ: " + e + ": allowed while the core keeps its doubles (firmware/BUDGET.md)")
    return errors, notes


fb.check = check

# ---- after the link: the blob, the stamp, the report
_build_app = fb.build_app


def section_table(elf):
    out = {}
    for ln in fb.tc("common/bin/objdump", "-h", elf).splitlines():
        p = ln.split()
        if len(p) >= 4 and p[1].startswith("."):
            out[p[1]] = (int(p[2], 16), int(p[3], 16))      # size, VMA
    return out


def build_app():
    img, syms, dis, rt = _build_app()
    elf = fb.OUT / (fb.NAME + ".elf")
    secs = section_table(elf)
    blob_path = fb.OUT / "fizz-xdata.bin"
    blob_path.unlink(missing_ok=True)
    size, vma = secs.get(".qd_xdata", (0, XIP(XDATA_LO)))
    if size:
        if vma != XIP(XDATA_LO):
            raise SystemExit(f"fizz: .qd_xdata at {vma:#x}, not {XIP(XDATA_LO):#x} (fizz.ld)")
        fb.tc("common/bin/objcopy", "-O", "binary", "-j", ".qd_xdata", elf, blob_path)
        blob = blob_path.read_bytes()
        if len(blob) != size:
            raise SystemExit(f"fizz: the blob is {len(blob)} B, its section {size}")
        if XDATA_LO + ((size + 3) & ~3) + 12 > XDATA_HI:
            raise SystemExit("fizz: the blob and its trailer do not fit PLAT_XDATA1")
        crc = zlib.crc32(blob) & 0xFFFFFFFF
        whole = blob + b"\xff" * (-len(blob) % 4) + struct.pack("<III", 0x54585A46, len(blob), crc)   # + its trailer
        for sec in range(0, len(whole), 0x1000):           # bsp/loader/ldr_core.c: a record has "AT" at 0xF06
            if whole[sec + 0xF06:sec + 0xF08] == b"AT":
                raise SystemExit(f"fizz: the blob has \"AT\" at {XDATA_LO + sec + 0xF06:#x} (plat.h): move it")
    else:
        blob, crc = b"", 0
    tag = struct.pack("<II", 0x44585A46, 0x504D5453)        # fz_port.c fz_stamp: "FZXDSTMP", len, crc
    at = img.find(tag)
    if at < 0 or img.find(tag, at + 1) >= 0:
        raise SystemExit("fizz: the stamp (fz_port.c fz_stamp) is not in the image once")
    img = img[:at + 8] + struct.pack("<II", len(blob), crc) + img[at + 16:]
    (fb.OUT / (fb.NAME + ".bin")).write_bytes(img)
    report(img, syms, secs, blob, crc)
    return img, syms, dis, rt


def map_pieces(path):
    """the link map's input sections: (output section, input section, size, owner): FIZZ's units, the platform
    (fm1-base and felucca-bsp: unity.o, crt0 ..) or the C library"""
    out, osec, pend = [], None, None
    for ln in Path(path).read_text(errors="replace").splitlines():
        m = re.match(r"^(\.\S+)\s+0x[0-9a-f]+\s+0x[0-9a-f]+", ln) or re.match(r"^(\.\S+)\s*$", ln)
        if m and not ln.startswith(" "):
            osec = m[1]
            continue
        m = re.match(r"^ (\.\S+|COMMON)\s+0x[0-9a-f]+\s+0x([0-9a-f]+)\s+(\S+)", ln)
        if not m and pend:
            m2 = re.match(r"^\s+0x[0-9a-f]+\s+0x([0-9a-f]+)\s+(\S+)", ln)
            m = (None, pend, m2[1], m2[2]) if m2 else None
        pend = None
        if not m:
            m3 = re.match(r"^ (\.\S+|COMMON)\s*$", ln)
            pend = m3[1] if m3 else None
            continue
        f = m[3]
        who = "fizz" if "/o2/" in f else "libc" if "/opt/jieli/" in f or ".a(" in f else "platform"
        out.append((osec, m[1], int(m[2], 16), who, f.split("/")[-1]))
    return out


def report(img, syms, secs, blob, crc):
    sym = {}
    for ln in syms.splitlines():
        p = ln.split()
        if len(p) >= 2 and re.fullmatch(r"[0-9a-f]{8}", p[0]):
            sym[p[-1]] = int(p[0], 16)
    pieces = map_pieces(fb.OUT / "fizz.map")
    by = {}
    for osec, isec, size, who, f in pieces:
        by.setdefault(osec, {}).setdefault(who, 0)
        by[osec][who] += size
    free_rt = 32768 - (sym["_fz_rt_end"] - 0x01C00000)
    free_ni = 0x3D50 - (sym["_fz_ni_end"] - 0x01C7C000)
    ram = sym["_bss_end"] - 0x01C08000
    pool = sym["_pool_end"] - sym["_pool_start"]
    fizz_data = sum(v.get("fizz", 0) for k, v in by.items() if k in (".data", ".bss", ".pool", ".fz_rt", ".fz_ni"))
    plat_data = sum(v.get("platform", 0) + v.get("libc", 0) for k, v in by.items()
                    if k in (".data", ".bss", ".pool", ".noinit", ".fz_rt", ".fz_ni"))
    budget = 98304 + 344064 - 8192 + (32768 - (sym["_rt_end"] - 0x01C00000)) + (0x3D50 - (sym["_fz_ni_start"] - 0x01C7C000))
    t = by.get(".text", {})

    def row(name, sec):
        v = by.get(sec, {})
        return f"  {name:34s} {v.get('platform', 0):8d} {v.get('fizz', 0):8d} {v.get('libc', 0):8d}"
    lines = [f"FIZZ on fm1-base: {fb.NAME}.bin {len(img)} B of the app slot's {fb.APP_SLOT} ({fb.APP_SLOT - len(img)} free)",
             f"RAM (.data + .bss) {ram} B of 98304 ({98304 - ram} free)",
             f"pool {pool} B of 344064 ({344064 - pool} free; fm1-base insists on 8192)",
             f"RAM code region: code {sym['_rt_end'] - sym['_rt_start']} B, FIZZ's data {sym['_fz_rt_end'] - sym['_fz_rt_start']} B, "
             f"{free_rt} of 32768 free; NOINIT: FIZZ's data {sym['_fz_ni_end'] - sym['_fz_ni_start']} B, {free_ni} of 15696 free",
             f"data blob (.qd_xdata) {len(blob)} B at flash {XDATA_LO:#x} (PLAT_XDATA1, {XDATA_HI - XDATA_LO} B), CRC-32 {crc:08x}",
             f"BUDGET-LINE-FM1BASE flash={len(img)}/{fb.APP_SLOT} fizz_ram={fizz_data} platform_ram={plat_data} "
             f"ram={fizz_data + plat_data}/{budget} xdata={len(blob)}/{XDATA_HI - XDATA_LO - 12}",
             "",
             f"  {'by output section (bytes)':34s} {'platform':>8s} {'FIZZ':>8s} {'libc':>8s}",
             row("code and constants (.text)", ".text"), row("RAM code (.ram_text)", ".ram_text"),
             row("initialised data (.data)", ".data"), row("RAM (.bss)", ".bss"), row("pool (.pool)", ".pool"),
             row("RAM code region, data (.fz_rt)", ".fz_rt"), row("NOINIT, fm1-base's (.noinit)", ".noinit"),
             row("NOINIT, FIZZ's (.fz_ni)", ".fz_ni"), ""]
    units = {}
    for osec, isec, size, who, f in pieces:
        if osec in (".bss", ".pool", ".fz_rt", ".fz_ni", ".data", ".noinit"):
            units[(f, osec)] = units.get((f, osec), 0) + size
    lines.append("RAM by object and region:")
    for (f, osec), size in sorted(units.items(), key=lambda kv: -kv[1]):
        if size >= 64:
            lines.append(f"  {size:8d}  {osec:8s} {f}")
    code = {}
    for osec, isec, size, who, f in pieces:
        if osec == ".text":
            code[f] = code.get(f, 0) + size
    lines += ["", "code and constants by object:"]
    for f, size in sorted(code.items(), key=lambda kv: -kv[1]):
        if size >= 256:
            lines.append(f"  {size:8d}  {f}")
    lines += ["", "64-bit multiplies inlined beside loads (felucca-bsp docs/CPU.md): " +
              ", ".join(fb.mul64_near_loads(Path(fb.OUT / (fb.NAME + ".dis")).read_text()))]
    (fb.OUT / "fizz-size.txt").write_text("\n".join(lines) + "\n")
    print("\n".join("  size  " + ln for ln in lines[:6]))


fb.build_app = build_app


# ---- the browser emulator: plutovg's rasteriser uses setjmp / longjmp. Emscripten's default for them goes through
# JavaScript (invoke_* imports), which fm1-base's page does not give (its env answers every import with nothing: the
# drawing came out black); built with WebAssembly's own exceptions instead, the module needs no such import
class _Subprocess:
    def __getattr__(self, k):
        return getattr(subprocess, k)

    def run(self, cmd, *a, **k):
        if cmd and cmd[0] == "emcc":
            cmd = [cmd[0], "-sSUPPORT_LONGJMP=wasm", *cmd[1:]]
        return subprocess.run(cmd, *a, **k)


fb.subprocess = _Subprocess()

if __name__ == "__main__":
    sys.argv = [str(BASE / "tools" / "build.py"), str(APP), *sys.argv[1:]]
    sys.exit(fb.main())
