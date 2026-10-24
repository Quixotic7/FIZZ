#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""FIZZ's firmware build: fm1-base's tools/build.py (the submodule, unchanged) with what FIZZ needs on top.

  firmware/build.py [--release X.Y[.Z]]     the unit's package: firmware/app/build/fizz.fwsc, fizz-xdata.bin, the
                                            size report fizz-size.txt
  firmware/build.py --host                  the host simulator: firmware/app/build/host/fizz_host
  firmware/build.py --web                   the browser emulator (Emscripten): firmware/app/build/emu/
  ... --single-core                         the single-core package (FZ_SINGLE_CORE=1: core 1 never started; 8
                                            voices, QD_VOICES_DEV=8, and the voices' render in RAM), its files
                                            named fizz-1core* (fizz-1core.fwsc, the host's fizz-1core_host).
                                            Without it: Standard, QD_VOICES_DEV=16

What it adds to fm1-base's build (each a small patch of its module, applied here):
  - the toolchain's container sees the whole repository (FIZZ's core/ and third_party/ are above firmware/): its root
    is the repository, not fm1-base's and the app's common directory;
  - FIZZ's units are compiled for long calls (-pi32v2-large-program: they call between the flash and the RAM code);
  - FIZZ's units find newlib's headers (the JieLi toolchain's pi32v2/include, after everything else) and link newlib's
    libm, libc and the compiler's runtime (r3: the FPU's), which an fm1-base app without Bluetooth links nothing of;
  - the link script is firmware/app/fizz.ld (FIZZ's zero-initialised data in the pool; the data blob at the data
    flash's address), with a map (build/fizz.map);
  - after the link: the data blob (.qd_xdata) cut out into fizz-xdata.bin, checked against the update loader's record
    rule (plat.h: never "AT" at 0xF06 of a sector), its length and CRC-32 stamped into the image (fz_port.c);
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
SINGLE = "--single-core" in sys.argv[1:]        # the single-core package (firmware/DUALCORE.md)
# the owner (2026-10-23): 8 voices on one core, 16 when core 1 renders half (core/qd_config.h QD_VOICES_DEV)
# P2.14b (the owner): 8 voices on every build; the dual build's voices on core 1 first, the rest on core 0. Its main RAM
# holds core 1's stacks: 12 of the 8-bit frame's rows go to the pool
DEFS = ["-DFZ_SINGLE_CORE=1", "-DQD_VOICES_DEV=8"] if SINGLE else ["-DQD_VOICES_DEV=8", "-DFZ_FB_LO=88u", "-DFM1B_CPU2=1"]
if SINGLE:
    os.environ["FM1B_CPU2"] = "0"                  # (P2.14: the single-core package never starts the second core)
if SINGLE and "FM1B_POOL_SPARE" not in os.environ:
    # the comb lines (16 KB) move from the RAM code region to the pool for quad_render: the pool keeps ~3 KB spare,
    # not fm1-base's 8,192. That spare is the build's margin only (nothing uses the pool past _pool_end: the stacks
    # are above it, at 0x01C74000); fm1-base's own override (FM1B_POOL_SPARE, "an app's own build") sets it
    os.environ["FM1B_POOL_SPARE"] = "2048"         # (P2.10: the core's LFO caches and looks' state, +1,080 B: 3,120 left)

# ---- audio code in RAM, by function (fizz.ld: .fzrt.* in the RAM code region's window). The compiler is not told:
# the function's section in the compiled object is renamed from .text.NAME to .fzrt.NAME (five bytes of the section
# names' table, no code byte), so the code is the one the flash build has. A function the compiler did not emit on
# its own (inlined) fails the build, unless its name starts with ? (a helper the core may not have). Calls in and
# out are long calls already (FIZZ's units: -pi32v2-large-program). The design is ChoralRoot's (cr_ram.h: markers,
# the object's section renamed), this code FIZZ's
HOT = {"build_units_c_qd_fx.o": ["xf_reverb", "xf_delay", "out_master", "xq_block", "xc_block",
                                 "fz_real_qd_fx_block", "qd_fx_block"]}
# (P2.8) the speed bisect: FZ_VARIANT=xip | o2 | crflags builds the voices' unit (quad_voice.c: the engine) another
# way, into fizz-1core-VARIANT.fwsc, its id in stats line 8 (build=): xip (1) its render left in the flash, o2 (2) at
# -O2 (in the flash too), crflags (3) with ChoralRoot's flags (-Os -ffunction-sections -fno-builtin: no -mcpu=r3, -mfprev1,
# -ffp-contract=off, -std=gnu11). The plain build is id 0
# P2.9: the unit's bench (us a 128 frames): o2ram 223 (218 warm), o2 240, os 259, crflags 260. The single-core
# package's default is o2ram (id 4): the voices' unit at -O2 with its render in RAM, the effects' hot functions back in
# the flash (their time 114 us a half stopped against 104); os (0) is P2.7's (-Os, the render in RAM), o2 (2) -O2 in
# the flash
VARIANT = os.environ.get("FZ_VARIANT", "")
VARIANTS = {"os": 0, "xip": 1, "o2": 2, "crflags": 3, "o2ram": 4}
NAMED = VARIANT
if SINGLE and not VARIANT:
    VARIANT = "o2ram"                            # (P2.9b: the unit's best: 223 us a 128 frames, 218 warm)
if VARIANT and VARIANT not in VARIANTS:
    raise SystemExit(f"FZ_VARIANT={VARIANT}: one of " + ", ".join(VARIANTS))
DEFS.append(f"-DFZ_VARIANT_ID={VARIANTS.get(VARIANT, 0)}u")
if VARIANT == "o2ram":                           # (the render's 22.6 KB at -O2 fits only with the effects out)
    HOT.pop("build_units_c_qd_fx.o")
# P2.15: the control block's begin (the sequencer, the player, the voices' starts, the owners' LFOs: branchy code that
# missed the flash's cache, its spikes ~8x its average on the unit) in RAM too, in the dual build (the single-core
# build's region is full); FZ_BEGIN_RAM=0 leaves it in the flash (the comparison)
BEGIN_RAM = not SINGLE and os.environ.get("FZ_BEGIN_RAM", "1") == "1"
DEFS.append(f"-DFZ_BEGIN_RAM={int(BEGIN_RAM)}")    # (stats line 8: begin_ram=)
if BEGIN_RAM:
    HOT.update({"build_units_c_player.o": ["qd_player_block", "fz_real_qd_player_block", "?qd_player_event"],
                "build_units_c_app.o": ["qd_app_block_begin", "?qd_app_tempo"],
                "build_units_c_pool.o": ["qd_pool_block_begin"],
                "build_units_c_seq.o": ["?qd_seq_bpm"],
                "build_units_c_quad_voice.o": ["qd_quad_owner_block"]})
if SINGLE and VARIANT not in ("xip", "o2"):      # (o2: the render at -O2 does not fit the RAM code region: flash)                  # (8 voices: the RAM they free takes the voices' render)
    HOT["build_units_c_quad_voice.o"] = ["quad_render", "qd_quad_voice_render", "?quad_lfo_incb", "?quad_lfo_incb_p",
                                         "?quad_lfo_incv", "?quad_lfo_inc"]   # (?: its helpers, where the core has them)


def place_hot(obj, names):
    b = bytearray(obj.read_bytes())
    if b[:4] != b"\x7fELF" or b[4] != 1 or b[5] != 1:
        raise SystemExit(f"fizz: {obj.name} is not a 32-bit little-endian ELF")
    shoff, = struct.unpack_from("<I", b, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", b, 0x2E)
    stroff, = struct.unpack_from("<I", b, shoff + shstrndx * shentsize + 0x10)
    secs = []
    for i in range(shnum):
        nm, = struct.unpack_from("<I", b, shoff + i * shentsize)
        end = b.index(b"\0", stroff + nm)
        secs.append((stroff + nm, bytes(b[stroff + nm:end])))
    for n in names:
        at = [o for o, s in secs if s == b".text." + n.lstrip("?").encode()]
        if not at and n.startswith("?"):
            continue
        if len(at) != 1:
            raise SystemExit(f"fizz: {obj.name} has no section .text.{n} of its own (inlined?): not placed in RAM")
        b[at[0]:at[0] + 5] = b".fzrt"
    obj.write_bytes(bytes(b))


# ---- fm1-base's files with FIZZ's changes (P2.7), made at build time from the submodule's own (which stays as it is):
# each a copy with the listed replacements, in build/overlay/, found before the original by the unity build. The
# changes are MPL-2.0 like the files (fm1-base, felucca-bsp); a replacement that no longer matches fails the build
OVERLAY = {
    # the steady mid level by column: column c lit on the scan frame where (phase + c) mod N is 0, not every column on
    # the same frame (all the mid LEDs lit at once: a ~114 Hz pulse of the whole panel, seen and heard)
    "bsp/boards/fm1/fm1_input.h": [
        ("static uint8_t fm1__mid_n = FM1_LED_MID_N, fm1__mid_ph;   /* the mid level: 1 frame lit in fm1__mid_n; this frame's */\n",
         "static uint8_t fm1__mid_n = FM1_LED_MID_N, fm1__mid_ph;   /* the mid level: 1 frame lit in fm1__mid_n; this frame's */\n"
         "/* (FIZZ, P2.7: the columns take turns on the mid level, (fm1__mid_ph + c) mod fm1__mid_n == 0; no division) */\n"
         "static inline uint32_t fm1__mid_turn(uint32_t c)\n{\n    uint32_t x = fm1__mid_ph + c;\n"
         "    while (x >= fm1__mid_n)\n        x -= fm1__mid_n;\n    return x == 0u;\n}\n"),
        ("(!fm1__mid_ph ? fm1_led_mid[c] : 0u))", "(fm1__mid_turn(c) ? fm1_led_mid[c] : 0u))"),
    ],
    # (P2.12) the panel's scanline read back (ST7789 RDSCL, 0x45) on the one data line: an upstream candidate for
    # felucca-bsp. The SPI1 bits from JieLi's AC79 SDK (Apache-2.0; its SPI examples): bit 12 receive, bit 14 the
    # pending flag's clear, bit 15 pending; bits 2, 10, 11 clear: one data line (the controller's DO, PC10)
    "bsp/boards/fm1/fm1_lcd_hw.h": [
        ("    FM1_LCD_SPI_CNT = n;\n}\n",
         "    FM1_LCD_SPI_CNT = n;\n}\n"
         "\n/* (FIZZ, P2.12) RDSCL: the command, then 24 clocks read on PC10 at ~6 MHz (the panel's read clock: a cycle\n"
         " * >= 150 ns): one dummy clock, the 16-bit scanline (STS[9:0] in its low bits), 7 clocks to spare. raw gets the\n"
         " * 24 bits as read. Main loop only, no LCD transfer running (lcd_sync first): the line is an output again, the\n"
         " * clock and the controller as they were, CS high. 0 read; -1 the controller never finished a byte */\n"
         "static int fm1_lcd_read_scanline(uint32_t *raw)\n{\n"
         "    const uint32_t con = FM1_LCD_SPI_CON, baud = FM1_LCD_SPI_BAUD;\n"
         "    uint32_t i, n, v = 0;\n    int rc = 0;\n"
         "    FM1_LCD_SPI_BAUD = 9u;\n"
         "    fm1_lcd_send_cmd(0x45u);\n    fm1_lcd_wait();\n"
         "    FM1_LCD_PC_OUT |= FM1_LCD_DC;\n"
         "    FM1_LCD_PC_DIR |= FM1_LCD_DO;                   /* (the data line in) */\n"
         "    FM1_LCD_SPI_CON = (con & ~((1u << 2) | (1u << 10) | (1u << 11))) | (1u << 12) | (1u << 14);\n"
         "    for (i = 0; i < 3u; i++) {\n"
         "        FM1_LCD_SPI_CON |= 1u << 14;\n        FM1_LCD_SPI_BUF = 0xFFu;\n"
         "        for (n = 0; n < 100000u && !(FM1_LCD_SPI_CON & 0x8000u); n++)\n            ;\n"
         "        if (n == 100000u)\n            rc = -1;\n"
         "        v = v << 8 | (FM1_LCD_SPI_BUF & 0xFFu);\n    }\n"
         "    FM1_LCD_SPI_CON = con | (1u << 14);\n"
         "    FM1_LCD_PC_DIR &= ~FM1_LCD_DO;                  /* (an output again) */\n"
         "    FM1_LCD_PC_OUT |= FM1_LCD_CS;\n"
         "    FM1_LCD_SPI_BAUD = baud;\n"
         "    *raw = v;\n    return rc;\n}\n"),
    ],
}


def overlay():
    out = fb.OUT / "overlay"
    out.mkdir(parents=True, exist_ok=True)
    for rel, reps in OVERLAY.items():
        src = (BASE / rel).read_text()
        for a, b in reps:
            if src.count(a) != 1:
                raise SystemExit(f"fizz: fm1-base's {rel} changed: build.py's OVERLAY no longer matches {a[:60]!r}")
            src = src.replace(a, b)
        (out / Path(rel).name).write_text(f"/* firmware/build.py: fm1-base's {rel} with FIZZ's changes (OVERLAY) */\n" + src)
    return out


def tc(tool, *args):
    args = list(args)
    if tool == "cc" and "-c" in args and any(isinstance(a, Path) and a.suffix == ".c" for a in args):
        args = ["-idirafter", jieli("pi32v2/include"), *args]
        args = [*DEFS, *args]
        if any(isinstance(a, Path) and a.name == "unity.c" for a in args):
            args = [f"-I{overlay().resolve().relative_to(REPO)}", *args]
        if any(isinstance(a, Path) and "o2" in a.parts and a.suffix == ".o" for a in args):
            # (P2.6: FIZZ's units call between the flash and the RAM code region, 4 MiB apart: long calls, as
            # fm1-base's Bluetooth build makes them)
            args = ["-mllvm", "-pi32v2-large-program=true", *args]
            out = args[args.index("-o") + 1]
            if out.name == "build_units_c_quad_voice.o" and VARIANT in ("o2", "o2ram"):
                args = [a if a != "-Os" else "-O2" for a in args]
            if out.name == "build_units_c_quad_voice.o" and VARIANT == "crflags":
                args = [a for a in args if a not in ("-mcpu=r3", "-mfprev1", "-ffp-contract=off", "-std=gnu11")]
            if out.name in HOT:
                r = _tc(tool, *args)
                place_hot(out, HOT[out.name])
                return r
    if tool == "pi32v2/bin/ld":
        for i, a in enumerate(args):
            if isinstance(a, Path) and a.name in ("app.ld", "app_wifi.ld"):
                args[i] = APP / "fizz.ld"
                if not SINGLE:                   # (P2.12: Standard keeps its comb lines in the RAM code region: the
                    ld = (APP / "fizz.ld").read_text()   # frame's rows and buffers marked for it go to the pool)
                    if ld.count(" *(.fz_rtdata*)") != 1 or ld.count("*/o2/fz_port.o(.bss* .sbss* COMMON)") != 1:
                        raise SystemExit("fizz: fizz.ld's .fz_rt / .pool lines changed: see build.py")
                    ld = ld.replace(" *(.fz_rtdata*)", "").replace("*/o2/fz_port.o(.bss* .sbss* COMMON)",
                                                                   "*/o2/fz_port.o(.bss* .sbss* COMMON) *(.fz_rtdata*)")
                    args[i] = fb.OUT / "fizz-std.ld"
                    args[i].write_text(ld)
                if SINGLE:                       # the voices' comb lines leave the RAM code region for the pool
                    ld = (APP / "fizz.ld").read_text()
                    comb = " */o2/build_units_c_quad_voice.o(.bss* .sbss* COMMON)"
                    if ld.count(comb) != 1 or ld.count("*/o2/fz_port.o(.bss* .sbss* COMMON)") != 1:
                        raise SystemExit("fizz: fizz.ld's .fz_rt / .pool lines changed: see build.py")
                    ld = ld.replace(comb, "").replace("*/o2/fz_port.o(.bss* .sbss* COMMON)",
                                                      "*/o2/fz_port.o(.bss* .sbss* COMMON)" + comb)
                    args[i] = fb.OUT / "fizz-1core.ld"
                    args[i].write_text(ld)
        o = args.index("-o")
        args[o:o] = ["--start-group", jieli("pi32v2/lib/r3/libm.a"), jieli("pi32v2/lib/r3/libc.a"),
                     jieli("pi32v2/lib/r3/libcompiler_rt.a"), "--end-group", "-Map", fb.OUT / "fizz.map"]
    return _tc(tool, *args)


fb.tc = tc

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
        if cmd and cmd[0] in ("emcc", os.environ.get("CC", "cc")):   # (the host's and the web's compilers)
            cmd = [cmd[0], *DEFS, *cmd[1:]]
        return subprocess.run(cmd, *a, **k)


fb.subprocess = _Subprocess()

# ---- the single-core package: its own file names (the name in fm1.toml, -1core), FZ_SINGLE_CORE=1 everywhere
if SINGLE:
    _read_conf = fb.read_conf
    fb.read_conf = lambda path: {**_read_conf(path), "name": _read_conf(path).get("name", "fizz") + "-1core" +
                                 (f"-{NAMED}" if NAMED else "")}

if __name__ == "__main__":
    sys.argv = [str(BASE / "tools" / "build.py"), str(APP), *[a for a in sys.argv[1:] if a != "--single-core"]]
    sys.exit(fb.main())
