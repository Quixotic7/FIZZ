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
                                            Without it: the dual build (8 voices, the second core)
  ... --define NAME[=VALUE]                 a core option for every unit (repeatable), named in stats line 8
  ... --tag NAME                            the package's name with -NAME (fizz-NAME.fwsc)
  ... --ble                                 the Bluetooth package (2026-10-27): fm1-base's Bluetooth MIDI peripheral
                                            (ble_trim, ble_no_pairing; heap and stacks from fm1.toml's fizz_ble_heap_kb
                                            and fizz_ble_stack_kb, or FZ_BLE_HEAP_KB / FZ_BLE_STACK_KB), USB audio in
                                            kept; the core's QD_PKG_BLE=1 (4 projects, 128 MY presets, the reverb's
                                            lines packed; core/qd_config.h); a second data blob at 0x97000 (code and
                                            constants out of the app slot) and its stores' own layout (persist.h). Its
                                            files: fizz-ble.fwsc, fizz-ble-xdata.bin, fizz-ble-xdata2.bin,
                                            fizz-ble-size.txt, fizz-ble.map (firmware/BUDGET.md "The Bluetooth package")
  FZ_MEASURE=1 ... (with --ble)            a measure, not a package: every region 1 MiB and the overlaps let pass,
                                            so the size report shows each region's overflow

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
# the Bluetooth package (--ble): fm1-base's Bluetooth store object (OBJ_BLE, app.h) takes XDATA1's first two sectors
# (0xE5000..0xE6FFF: inside fm1-base's FL_STORE_OK, which 0xFA000 is not), the first blob the rest; the second blob is
# the data region's first 24 sectors (0x97000..0xAEFFF), the stores after it (persist.h)
BLE_XDATA_LO = 0xE7000
BLE_X2_LO, BLE_X2_HI = 0x97000, 0xAF000
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
BLE_PKG = "--ble" in sys.argv[1:]               # the Bluetooth package (2026-10-27; firmware/BUDGET.md)
BLE_FB_LO = int(os.environ.get("FZ_BLE_FB_LO", "6"))   # (the Bluetooth package: the 8-bit frame's rows in RAM)
if BLE_PKG and SINGLE:
    raise SystemExit("--ble: the Bluetooth package is the dual-core one (no single-core Bluetooth package)")
# P2.15i: --define NAME=VALUE (repeatable): a core option for every unit (e.g. the pacing flags, core/qd_config.h), named
# in stats line 8 (defines=); --tag NAME: the package's name gets -NAME (fizz-NAME.fwsc)
EXTRA, TAG, _rest = [], "", []
_a = sys.argv[1:]
_i = 0
while _i < len(_a):
    if _a[_i] == "--define" and _i + 1 < len(_a):
        if not re.fullmatch(r"[A-Z_][A-Z0-9_]*(=[A-Za-z0-9_.+-]*)?", _a[_i + 1]):
            raise SystemExit(f"--define {_a[_i + 1]}: NAME or NAME=VALUE")
        EXTRA.append(_a[_i + 1])
        _i += 2
    elif _a[_i] == "--ble":
        _i += 1
    elif _a[_i] == "--tag" and _i + 1 < len(_a):
        TAG = _a[_i + 1]
        _i += 2
    else:
        _rest.append(_a[_i])
        _i += 1
sys.argv = [sys.argv[0], *_rest]
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
# P2.15g (the unit): the effects' seven hot functions in RAM (the drawing's set in their place gained the drawing ~3 %
# and cost the interrupt more than the pipeline hides: cpu_isr 45-51 -> 61). FZ_HOT_DRAW=1 builds the drawing's set
# instead (P2.15f)
HOT = {"build_units_c_qd_fx.o": ["xf_reverb", "xf_delay", "out_master", "xq_block", "xc_block",
                                 "fz_real_qd_fx_block", "qd_fx_block"]}
if os.environ.get("FZ_HOT_DRAW") == "1":
    HOT = {"build_units_c_qd_gfx_fast.o": ["qd_dl_play", "text_draw", "fill_path", "paint_setup", "acc_edge",
                                           "qd_text_mask_rows", "quad_dev", "rect_dev"]}
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
DEFS += [f"-D{d}" for d in EXTRA]
if BLE_PKG:
    DEFS.append("-DQD_PKG_BLE=1")                # (the core's Bluetooth profile: core/qd_config.h)
    DEFS = [d if not d.startswith("-DFZ_FB_LO=") else f"-DFZ_FB_LO={BLE_FB_LO}u" for d in DEFS]
if EXTRA:
    DEFS.append('-DFZ_DEFINES="' + ",".join(EXTRA) + '"')
if VARIANT == "o2ram":                           # (the render's 22.6 KB at -O2 fits only with the effects out, and
    HOT.pop("build_units_c_qd_fx.o", None)       # without the drawing's: the single-core build's region is the voices')
    HOT.pop("build_units_c_qd_gfx_fast.o", None)
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
# the Bluetooth package: its radio needs ~4.7 KB of the RAM code region (the radio's trim, 3.6 KB, must run from RAM;
# the packed reverb is ~0.6 KB longer), so the least hot of FIZZ's RAM code stays in the flash there: the master
# filter (xq_block: runs only while the filter is on), the chorus's taps (xc_block: only while the chorus holds
# something), the delay (xf_delay: only while it holds something), a player event (qd_player_event: once a trig, not
# every block) and the effects' block (fz_real_qd_fx_block: every block). ~4.9 KB: the flash cache's misses on them
# (P2.15g: the effects' seven out of RAM cost the interrupt 10..16 points) when they run (firmware/BUDGET.md)
BLE_COLD = {"build_units_c_qd_fx.o": ["xq_block", "xc_block", "xf_delay", "fz_real_qd_fx_block"],
            "build_units_c_player.o": ["?qd_player_event"]}
if BLE_PKG:
    for _o, _ns in BLE_COLD.items():
        if _o in HOT:
            HOT[_o] = [n for n in HOT[_o] if n not in _ns]
if SINGLE and VARIANT not in ("xip", "o2"):      # (o2: the render at -O2 does not fit the RAM code region: flash)                  # (8 voices: the RAM they free takes the voices' render)
    HOT["build_units_c_quad_voice.o"] = ["quad_render", "qd_quad_voice_render", "?quad_lfo_incb", "?quad_lfo_incb_p",
                                         "?quad_lfo_incv", "?quad_lfo_inc"]   # (?: its helpers, where the core has them)


# ---- the Bluetooth package's second blob (--ble): what leaves the app slot for the data region's first 24 sectors
# (0x97000..0xAEFFF), linked at its XIP address (fizz-ble.ld's .fz_x2) and cut out into fizz-ble-xdata2.bin. Input
# sections by unit (FIZZ's units have no -fdata-sections: a unit's code and constants go whole, by name pattern). Only
# what FIZZ runs or reads on the main loop once its blobs are checked (fz_init): the screens' code and constants, never
# the sound's (the audio interrupt, core 1), the port's (fz_port.c: the blobs' own check) or the platform's (its boot,
# crash and update screens draw without FIZZ's data). Code runs from there as from the app slot: the data region is
# read through the same XIP window (main_fm1.c's plain window from 0x93000 up: fl_plain_window_init), the flash is off
# only with the interrupts off, and FIZZ's units make long calls. And the first blob's part that its two sectors fewer
# no longer hold
# ui_layer.o (the screens' drawing, 99 KB) but two of its functions, which stay in the app slot (BLE_SLOT: their
# sections renamed .fzsl.*, as HOT's .fzrt.*), and one face of the first blob's (the 18-px light one, 6 KB: the first
# blob two sectors shorter). Balance (2026-10-27): the slot ~4.4 KB free, the first blob ~1.5 KB, the second ~2.3 KB
BLE_X2 = ["*/o2/build_units_c_ui_layer.o(.text* .rodata*)", "*(.qd_xdata.qf_bits_18_300 .qd_xdata.qf_g_18_300)"]
BLE_SLOT = {"build_units_c_ui_layer.o": ["ui_geek_draw", "ui_menu_draw"]}
# .. and its RAM: the radio's heap and statics land in RAM (its heap must stay low: fm1-base misc.c, the radio's DMA
# does not reach the pool's top), so FIZZ units' zero-initialised data move from RAM to the pool, whole (no
# -fdata-sections): app.o's 8 KB; and the 8-bit frame's rows but two (BLE_FB_LO: 86 rows, 20,640 B)
BLE_TO_POOL = ["build_units_c_app.o"]
BLE_TO_NI = ["build_units_c_ctl_latch.o"]       # (NOINIT's room the 8-bit frame's hashes left: FZ_FB_SEGS 1)


def ble_ld(ld):
    """fizz.ld (as the dual build has it) -> the Bluetooth package's link script: XDATA1 two sectors shorter (OBJ_BLE
    at 0xE5000), the second blob's region and its section (.fz_x2: first in the script, so its inputs are matched
    before .text's and .qd_xdata's wildcards)"""
    old = "    XDATA  (r)   : ORIGIN = 0x020E1000, LENGTH = 0x17000 - 12\n"
    if ld.count(old) != 1 or ld.count("SECTIONS\n{\n") != 1:
        raise SystemExit("fizz: fizz.ld's XDATA line or SECTIONS changed: see build.py ble_ld")
    ld = ld.replace(old, f"    XDATA  (r)   : ORIGIN = {XIP(BLE_XDATA_LO):#010x}, LENGTH = {XDATA_HI - BLE_XDATA_LO:#x} - 12\n"
                         f"    XDATA2 (rx)  : ORIGIN = {XIP(BLE_X2_LO):#010x}, LENGTH = {BLE_X2_HI - BLE_X2_LO:#x} - 12\n")
    for u in BLE_TO_POOL:
        piece = f" */o2/{u}(.bss* .sbss* COMMON)"
        ld = ld.replace(piece, "")
        if ld.count("*/o2/fz_port.o(.bss* .sbss* COMMON)") != 1:
            raise SystemExit("fizz: fizz.ld's .pool line changed: see build.py ble_ld")
        ld = ld.replace("*/o2/fz_port.o(.bss* .sbss* COMMON)", "*/o2/fz_port.o(.bss* .sbss* COMMON)" + piece)
    for u in BLE_TO_NI:
        ni = "*/o2/build_units_c_ui_keys.o(.bss* .sbss* COMMON)"
        if ld.count(ni) != 1:
            raise SystemExit("fizz: fizz.ld's .fz_ni line changed: see build.py ble_ld")
        ld = ld.replace(ni, ni + f" */o2/{u}(.bss* .sbss* COMMON)")
    # the CPU's PC limit (felucca-bsp fm1_guard.h: an instruction fetched outside [0x02000120, _etext] or the RAM code
    # faults, DBG_MSG bit 12): the second blob's code is above the app slot, so the window ends at its end (_etext; the
    # slot's own end is _etext_slot). Without it the first call into it faulted (the unit, 2026-10-27: EDIT's sound
    # editor, ui_snd_draw, "crash vec 1 ... d 00001000")
    et = ". = ALIGN(4); _etext = .; } > XIP"
    if ld.count(et) != 1:
        raise SystemExit("fizz: fizz.ld's _etext line changed: see build.py ble_ld")
    ld = ld.replace(et, ". = ALIGN(4); _etext_slot = .; } > XIP")
    ld = ld.replace("    _rt_load = LOADADDR(.ram_text);\n", "    _rt_load = LOADADDR(.ram_text);\n    _etext = _fz_x2_end;\n")
    t = "*(.*_const)\n"                         # (.text's last inputs: the kept functions after them)
    if ld.count(t) != 1:
        raise SystemExit("fizz: fizz.ld's .text changed: see build.py ble_ld")
    ld = ld.replace(t, "*(.*_const) *(.fzsl.*)\n")
    ld = ld.replace("SECTIONS\n{\n", "SECTIONS\n{\n    .fz_x2 : { _fz_x2_start = .; " + " ".join(BLE_X2) +
                    " . = ALIGN(4); _fz_x2_end = .; } > XDATA2\n")
    if os.environ.get("FZ_MEASURE") == "1":       # (a measure only: every region 1 MiB, the overlaps let pass; the
        ld = re.sub(r"LENGTH = [^\n]*\n", "LENGTH = 0x100000\n", ld)   # report shows each one's use; not a package)
    return ld


def place_hot(obj, names, to=b".fzrt"):
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
            raise SystemExit(f"fizz: {obj.name} has no section .text.{n} of its own (inlined?): not placed "
                             f"({to.decode()})")
        b[at[0]:at[0] + 5] = to
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
    # (2026-10-27) TWO received SysEx frames, in order: fm1-base kept one, and a frame arriving while it was pending
    # was dropped (two in one USB transfer: a host's back-to-back requests, ~6 % lost at 8 in flight on the owner's
    # FM-1). The interrupt fills slot sx_fw, the main loop takes slot sx_fr (one writer each); the frames in the pool
    # (FIZZ's spare RAM; main RAM has ~1 KB). The update loader includes its own copy (../chip/usb.c): unchanged
    "bsp/chip/usb.c": [
        ("static uint8_t sx_frame[640];\nstatic uint32_t sx_pos;\nstatic volatile uint32_t sx_frame_len;\n"
         "static volatile uint8_t sx_ready, sx_collect, sx_busy;\n",
         "/* (FIZZ: two frames, in order; firmware/build.py's OVERLAY) */\n#define SX_SLOTS 2u\n"
         "static uint8_t sx_frames[SX_SLOTS][640] __attribute__((section(\".pool\"), aligned(4)));\n"
         "static uint32_t sx_pos;\nstatic volatile uint32_t sx_lens[SX_SLOTS], sx_fw, sx_fr;   /* the interrupt's, the main loop's */\n"
         "static volatile uint8_t sx_collect, sx_busy;\n"),
        ("        sx_collect = !sx_ready;\n", "        sx_collect = sx_fw - sx_fr < SX_SLOTS;\n"),
        ("        if (sx_pos < sizeof sx_frame)\n            sx_frame[sx_pos++] = b;\n",
         "        if (sx_pos < sizeof sx_frames[0])\n            sx_frames[sx_fw % SX_SLOTS][sx_pos++] = b;\n"),
        ("            sx_frame_len = sx_pos;\n            RING_PUBLISH();\n            sx_ready = 1;",
         "            sx_lens[sx_fw % SX_SLOTS] = sx_pos;\n            RING_PUBLISH();\n            sx_fw++;"),
        ("    if (!sx_ready)\n        return 0;\n    RING_PUBLISH();                                     /* read the frame only after the flag */\n"
         "    *p = sx_frame;\n    *n = sx_frame_len;\n",
         "    if (sx_fw == sx_fr)\n        return 0;\n    RING_PUBLISH();                                     /* read the frame only after the flag */\n"
         "    *p = sx_frames[sx_fr % SX_SLOTS];\n    *n = sx_lens[sx_fr % SX_SLOTS];\n"),
        ("    RING_PUBLISH();                                     /* done with the frame before the ISR may refill it */\n    sx_ready = 0;\n",
         "    RING_PUBLISH();                                     /* done with the frame before the ISR may refill it */\n    sx_fr++;\n"),
    ],
}


# (the Bluetooth package only) fm1-base's canvas for its own screens (boot, crash, update, calibration, the splash;
# FIZZ's fz_message) shares FIZZ's 8-bit frame's memory instead of 7,680 B of its own in the pool: app.h gives
# APP_CANVAS_AT (the frame's rows in the pool, app.c fz_fb_hi). Safe because every such screen clears the panel and
# FIZZ draws its whole frame again after it (app_ui_init: fz_fb_forget, fz_redraw; a crash's screen: FIZZ is gone),
# and none is drawn while FIZZ draws (one main loop)
BLE_OVERLAY = {
    "device/gfx.c": [
        ("static uint16_t cv_px[CV_MAX] __attribute__((section(\".pool\")));\n",
         "#ifdef APP_CANVAS_AT                /* (FIZZ, the Bluetooth package: the canvas in the app's memory; firmware/\n"
         "                                     * build.py's BLE_OVERLAY) */\n#define cv_px APP_CANVAS_AT\n#else\n"
         "static uint16_t cv_px[CV_MAX] __attribute__((section(\".pool\")));\n#endif\n"),
    ],
    # (gfx.c is unity.c's neighbour, found there before any -I: the build compiles a copy of unity.c from the overlay's
    # directory, where its #include "gfx.c" finds the copy above; every other include as before)
    "device/unity.c": [],
    # SysEx over Bluetooth (FIZZ's own, the Manager's): firmware/patches/fm1-base-ble-sysex.patch (RemoteFM1's, the
    # owner's) applied first to these three (BLE_PATCHED); radio.c's queues (app.h BLE_IN_Q, BLE_OUT_Q) in the pool,
    # not RAM (software rings: the radio's buffers, which its DMA reads, are its heap's, in RAM); le_midi.c (a radio
    # unit) and the simulator's plat_host.c compiled from these copies
    "device/radio.c": [
        ("static uint32_t ble_in_q[BLE_IN_Q];\n",
         "static uint32_t ble_in_q[BLE_IN_Q] __attribute__((section(\".pool\")));   /* (FIZZ: the pool) */\n"),
        ("} ble_out[BLINKS];\n", "} ble_out[BLINKS] __attribute__((section(\".pool\")));   /* (FIZZ: the pool) */\n"),
    ],
    # (2026-10-27, a Mac's link lost at its connection updates) the FM-1's own request no longer moves a link already in
    # BLE MIDI's 7.5..15 ms (CoreMIDI sets 7.5 ms itself) to 15 ms: one control procedure less; its first range now
    # holds 7.5 ms (Apple: max >= min + 15 ms), the old ones after it
    "ble/midi/le_midi.c": [
        ("    {9, 21, 0, 400},                /* 11.25..26.25 ms (1.25 ms units; Apple: max >= min + 15 ms), 4 s timeout */\n",
         "    {6, 18, 0, 400},                /* (FIZZ) 7.5..22.5 ms: BLE MIDI's 7.5..15 ms in it */\n"
         "    {9, 21, 0, 400},                /* 11.25..26.25 ms (1.25 ms units; Apple: max >= min + 15 ms), 4 s timeout */\n"),
        ("static void check_connetion_updata_deal(void)\n{\n",
         "static uint16_t conn_iv;                         /* (FIZZ) the link's interval now (1.25 ms units; 0 unknown) */\n"
         "static void check_connetion_updata_deal(void)\n{\n"
         "    if (conn_iv >= 6u && conn_iv <= 12u) {      /* (FIZZ: already BLE MIDI's 7.5..15 ms: no request) */\n"
         "        connection_update_cnt = CONN_PARAM_TABLE_CNT;\n        return;\n    }\n"),
        ("    printf(\"conn: interval %d latency %d\\n\", conn_interval, conn_latency);",
         "    conn_iv = (uint16_t)conn_interval;           /* (FIZZ) */\n"
         "    printf(\"conn: interval %d latency %d\\n\", conn_interval, conn_latency);"),
        ("        printf(\"computer: disconnected %x\\n\", packet[5]);\n",
         "        printf(\"computer: disconnected %x\\n\", packet[5]);\n        conn_iv = 0;                             /* (FIZZ) */\n"),
    ],
    "host/plat_host.c": [],
}
BLE_PATCH = HERE / "patches" / "fm1-base-ble-sysex.patch"
BLE_PATCHED = ("device/radio.c", "ble/midi/le_midi.c", "host/plat_host.c")
_overlay_lock = __import__("threading").Lock()


def ble_patched():
    """BLE_PATCHED's files with BLE_PATCH applied (patch -p1, in OUT/ble-sysex/): {rel: text}"""
    import shutil
    tree = fb.OUT / "ble-sysex"
    for rel in BLE_PATCHED:
        (tree / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(BASE / rel, tree / rel)
    r = subprocess.run(["patch", "-p1", "-s", "-N", "-r", "-", "-d", str(tree), "-i", str(BLE_PATCH)],
                       capture_output=True, text=True)
    if r.returncode:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit(f"fizz: {BLE_PATCH.name} no longer applies to fm1-base")
    return {rel: (tree / rel).read_text() for rel in BLE_PATCHED}


def overlay():
    with _overlay_lock:                          # (the units compile in parallel: one writer)
        return _overlay()


def _overlay():
    out = fb.OUT / "overlay"
    out.mkdir(parents=True, exist_ok=True)
    files = {**OVERLAY, **(BLE_OVERLAY if BLE_PKG else {})}
    patched = ble_patched() if BLE_PKG else {}
    for f in out.glob("*"):                      # (another package's overlay left from its build: not this one's)
        if f.is_file() and f.name not in {Path(r).name for r in files}:
            f.unlink()
    for rel, reps in files.items():
        src = patched.get(rel) or (BASE / rel).read_text()
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
            ov = overlay()
            args = [f"-I{ov.resolve().relative_to(REPO)}", *args]
            if BLE_PKG:                          # (BLE_OVERLAY: the copy of unity.c)
                args = [ov / "unity.c" if isinstance(a, Path) and a.name == "unity.c" else a for a in args]
        if BLE_PKG and any(isinstance(a, Path) and a.name == "le_midi.c" for a in args):
            ov = overlay()                       # (BLE_OVERLAY: the patched copy of the radio's MIDI unit)
            args = [ov / "le_midi.c" if isinstance(a, Path) and a.name == "le_midi.c" else a for a in args]
        if any(isinstance(a, Path) and "o2" in a.parts and a.suffix == ".o" for a in args):
            # (P2.6: FIZZ's units call between the flash and the RAM code region, 4 MiB apart: long calls, as
            # fm1-base's Bluetooth build makes them)
            args = ["-mllvm", "-pi32v2-large-program=true", *args]
            out = args[args.index("-o") + 1]
            if out.name == "build_units_c_quad_voice.o" and VARIANT in ("o2", "o2ram"):
                args = [a if a != "-Os" else "-O2" for a in args]
            if out.name == "build_units_c_quad_voice.o" and VARIANT == "crflags":
                args = [a for a in args if a not in ("-mcpu=r3", "-mfprev1", "-ffp-contract=off", "-std=gnu11")]
            if out.name in HOT or (BLE_PKG and out.name in BLE_SLOT):
                r = _tc(tool, *args)
                if out.name in HOT:
                    place_hot(out, HOT[out.name])
                if BLE_PKG and out.name in BLE_SLOT:
                    place_hot(out, BLE_SLOT[out.name], b".fzsl")
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
                    args[i] = fb.OUT / ("fizz-ble.ld" if BLE_PKG else "fizz-std.ld")
                    args[i].write_text(ble_ld(ld) if BLE_PKG else ld)
                if SINGLE:                       # the voices' comb lines leave the RAM code region for the pool;
                    ld = (APP / "fizz.ld").read_text()   # 2026-10-24: the frame's rows and buffers marked for it too
                    comb = " */o2/build_units_c_quad_voice.o(.bss* .sbss* COMMON)"   # (as Standard: six tracks gave
                    if (ld.count(comb) != 1 or ld.count(" *(.fz_rtdata*)") != 1 or   # the pool 14 KB; the render
                            ld.count("*/o2/fz_port.o(.bss* .sbss* COMMON)") != 1):   # at -O2 outgrew the region)
                        raise SystemExit("fizz: fizz.ld's .fz_rt / .pool lines changed: see build.py")
                    ld = ld.replace(comb, "").replace(" *(.fz_rtdata*)", "").replace(
                        "*/o2/fz_port.o(.bss* .sbss* COMMON)", "*/o2/fz_port.o(.bss* .sbss* COMMON) *(.fz_rtdata*)" + comb)
                    args[i] = fb.OUT / "fizz-1core.ld"
                    args[i].write_text(ld)
        o = args.index("-o")
        if os.environ.get("FZ_MEASURE") == "1":
            args[o:o] = ["--no-check-sections"]
            o += 1
        args[o:o] = ["--start-group", jieli("pi32v2/lib/r3/libm.a"), jieli("pi32v2/lib/r3/libc.a"),
                     jieli("pi32v2/lib/r3/libcompiler_rt.a"), "--end-group", "-Map", fb.OUT / f"{OUTP}.map"]
    return _tc(tool, *args)


fb.tc = tc
OUTP = "fizz-ble" if BLE_PKG else "fizz"       # the map's, the blobs' and the size report's names

# ---- after the link: the blob, the stamp, the report
_build_app = fb.build_app


def section_table(elf):
    out = {}
    for ln in fb.tc("common/bin/objdump", "-h", elf).splitlines():
        p = ln.split()
        if len(p) >= 4 and p[1].startswith("."):
            out[p[1]] = (int(p[2], 16), int(p[3], 16))      # size, VMA
    return out


def cut_blob(elf, secs, sec, lo, hi, path, what):
    """a blob's section cut out of the ELF into path, checked: at its flash address lo, it and its 12-byte trailer
    inside [lo, hi), no "AT" at 0xF06 of a sector (plat.h); (bytes, CRC-32)"""
    path.unlink(missing_ok=True)
    size, vma = secs.get(sec, (0, XIP(lo)))
    if not size:
        return b"", 0
    if vma != XIP(lo):
        raise SystemExit(f"fizz: {sec} at {vma:#x}, not {XIP(lo):#x} (the link script)")
    fb.tc("common/bin/objcopy", "-O", "binary", "-j", sec, elf, path)
    blob = path.read_bytes()
    if len(blob) != size:
        raise SystemExit(f"fizz: {what} is {len(blob)} B, its section {size}")
    if lo + ((size + 3) & ~3) + 12 > hi and os.environ.get("FZ_MEASURE") != "1":
        raise SystemExit(f"fizz: {what} and its trailer do not fit {lo:#x}..{hi:#x}")
    crc = zlib.crc32(blob) & 0xFFFFFFFF
    whole = blob + b"\xff" * (-len(blob) % 4) + struct.pack("<III", 0x54585A46, len(blob), crc)   # + its trailer
    for at in range(0, len(whole), 0x1000):              # bsp/loader/ldr_core.c: a record has "AT" at 0xF06
        if whole[at + 0xF06:at + 0xF08] == b"AT":
            raise SystemExit(f"fizz: {what} has \"AT\" at {lo + at + 0xF06:#x} (plat.h): move it")
    return blob, crc


def pc_window_check(syms):
    """every function inside the CPU's PC limit (felucca-bsp fm1_guard.h: [0x02000120, _etext] and the RAM code
    _rt_start.._rt_end): one outside faults at its first instruction (the Bluetooth package's second blob, before
    ble_ld moved _etext: 2026-10-27)"""
    sym, bad = {}, []
    for ln in syms.splitlines():
        p = ln.split()
        if len(p) >= 2 and re.fullmatch(r"[0-9a-f]{8}", p[0]):
            sym[p[-1]] = int(p[0], 16)
    lo, hi, rs, re_ = 0x02000120, sym["_etext"], sym["_rt_start"], sym["_rt_end"]
    for ln in syms.splitlines():
        p = ln.split()
        if len(p) >= 6 and re.fullmatch(r"[0-9a-f]{8}", p[0]) and " F " in ln:
            a = int(p[0], 16)
            if int(p[-2], 16) and not (lo <= a <= hi or rs <= a < re_):
                bad.append(f"{p[-1]} {a:#x}")
    if bad:
        raise SystemExit(f"fizz: functions outside the CPU's PC limit [{lo:#x}, _etext {hi:#x}] and the RAM code: "
                         + ", ".join(bad[:6]))


def stamp(img, tag, blob, crc, need=True):
    """a blob's length and CRC-32 into its stamp in the image (fz_port.c fz_stamp, fz_stamp2)"""
    at = img.find(tag)
    if at < 0 and not need:
        return img
    if at < 0 or img.find(tag, at + 1) >= 0:
        raise SystemExit(f"fizz: the stamp {tag[:4]!r} (fz_port.c) is not in the image once")
    return img[:at + 8] + struct.pack("<II", len(blob), crc) + img[at + 16:]


def build_app():
    img, syms, dis, rt = _build_app()
    elf = fb.OUT / (fb.NAME + ".elf")
    secs = section_table(elf)
    lo = BLE_XDATA_LO if BLE_PKG else XDATA_LO
    blob, crc = cut_blob(elf, secs, ".qd_xdata", lo, XDATA_HI, fb.OUT / f"{OUTP}-xdata.bin", "the blob")
    img = stamp(img, struct.pack("<II", 0x44585A46, 0x504D5453), blob, crc)   # "FZXDSTMP", len, crc
    blob2, crc2 = b"", 0
    if BLE_PKG:
        blob2, crc2 = cut_blob(elf, secs, ".fz_x2", BLE_X2_LO, BLE_X2_HI, fb.OUT / f"{OUTP}-xdata2.bin",
                               "the second blob")
        if not blob2:
            raise SystemExit("fizz: the Bluetooth package has no second blob (.fz_x2)")
        img = stamp(img, struct.pack("<II", 0x32585A46, 0x504D5453), blob2, crc2)   # "FZX2STMP", len, crc
    elif struct.pack("<II", 0x32585A46, 0x504D5453) in img:
        raise SystemExit("fizz: a second blob's stamp in a package without one")
    pc_window_check(syms)
    (fb.OUT / (fb.NAME + ".bin")).write_bytes(img)
    report(img, syms, secs, blob, crc, blob2, crc2)
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


def report(img, syms, secs, blob, crc, blob2=b"", crc2=0):
    sym = {}
    for ln in syms.splitlines():
        p = ln.split()
        if len(p) >= 2 and re.fullmatch(r"[0-9a-f]{8}", p[0]):
            sym[p[-1]] = int(p[0], 16)
    pieces = map_pieces(fb.OUT / f"{OUTP}.map")
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
    xlo = BLE_XDATA_LO if BLE_PKG else XDATA_LO

    def row(name, sec):
        v = by.get(sec, {})
        return f"  {name:34s} {v.get('platform', 0):8d} {v.get('fizz', 0):8d} {v.get('libc', 0):8d}"
    lines = [f"FIZZ on fm1-base: {fb.NAME}.bin {len(img)} B of the app slot's {fb.APP_SLOT} ({fb.APP_SLOT - len(img)} free)",
             f"RAM (.data + .bss) {ram} B of 98304 ({98304 - ram} free)",
             f"pool {pool} B of 344064 ({344064 - pool} free; fm1-base insists on 8192)",
             f"RAM code region: code {sym['_rt_end'] - sym['_rt_start']} B, FIZZ's data {sym['_fz_rt_end'] - sym['_fz_rt_start']} B, "
             f"{free_rt} of 32768 free; NOINIT: FIZZ's data {sym['_fz_ni_end'] - sym['_fz_ni_start']} B, {free_ni} of 15696 free",
             f"data blob (.qd_xdata) {len(blob)} B at flash {xlo:#x} (PLAT_XDATA1, {XDATA_HI - xlo} B), CRC-32 {crc:08x}",
             *([f"second data blob (.fz_x2) {len(blob2)} B at flash {BLE_X2_LO:#x} (the data region's first "
                f"{(BLE_X2_HI - BLE_X2_LO) // 4096} sectors, {BLE_X2_HI - BLE_X2_LO - 12} B with its trailer: "
                f"{BLE_X2_HI - BLE_X2_LO - 12 - len(blob2)} free), CRC-32 {crc2:08x}"] if BLE_PKG else []),
             f"BUDGET-LINE-FM1BASE flash={len(img)}/{fb.APP_SLOT} fizz_ram={fizz_data} platform_ram={plat_data} "
             f"ram={fizz_data + plat_data}/{budget} xdata={len(blob)}/{XDATA_HI - xlo - 12}" +
             (f" xdata2={len(blob2)}/{BLE_X2_HI - BLE_X2_LO - 12}" if BLE_PKG else ""),
             "",
             f"  {'by output section (bytes)':34s} {'platform':>8s} {'FIZZ':>8s} {'libc':>8s}",
             row("code and constants (.text)", ".text"), *([row("second blob (.fz_x2)", ".fz_x2")] if BLE_PKG else []),
             row("RAM code (.ram_text)", ".ram_text"),
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
    (fb.OUT / f"{OUTP}-size.txt").write_text("\n".join(lines) + "\n")
    print("\n".join("  size  " + ln for ln in lines[:7 if BLE_PKG else 6]))


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
            if BLE_PKG and any(str(c).endswith("host/plat_host.c") for c in cmd):   # (BLE_OVERLAY: the simulator's
                ov = overlay()                   # patched copy, its own directory on the include path)
                cmd = [str(ov / "plat_host.c") if str(c).endswith("host/plat_host.c") else c for c in cmd]
                cmd.insert(1, f"-I{BASE / 'host'}")
        return subprocess.run(cmd, *a, **k)


fb.subprocess = _Subprocess()

# ---- the single-core package: its own file names (the name in fm1.toml, -1core), FZ_SINGLE_CORE=1 everywhere
if (SINGLE or TAG) and not BLE_PKG:
    _read_conf = fb.read_conf
    fb.read_conf = lambda path: {**_read_conf(path), "name": _read_conf(path).get("name", "fizz") +
                                 ("-1core" if SINGLE else "") + (f"-{NAMED}" if SINGLE and NAMED else "") +
                                 (f"-{TAG}" if TAG else "")}

# ---- the Bluetooth package: fm1-base's Bluetooth MIDI peripheral (trimmed, no pairing), its heap and stacks from
# fm1.toml's fizz_ble_heap_kb / fizz_ble_stack_kb (the environment's FZ_BLE_HEAP_KB / FZ_BLE_STACK_KB over them: the
# unit's log says what they reach, radio.c's "heap" and "stacks"), named fizz-ble
if BLE_PKG:
    _read_conf_ble = fb.read_conf

    def _ble_conf(path):
        c = _read_conf_ble(path)
        heap = int(os.environ.get("FZ_BLE_HEAP_KB", c.get("fizz_ble_heap_kb", 12)))
        stack = int(os.environ.get("FZ_BLE_STACK_KB", c.get("fizz_ble_stack_kb", 6)))
        return {**c, "name": c.get("name", "fizz") + "-ble" + (f"-{TAG}" if TAG else ""), "ble": True,
                "ble_trim": True, "ble_no_pairing": True, "ble_heap_kb": heap, "ble_stack_kb": stack,
                "usb_audio_in": True}
    fb.read_conf = _ble_conf

if __name__ == "__main__":
    sys.argv = [str(BASE / "tools" / "build.py"), str(APP), *[a for a in sys.argv[1:] if a != "--single-core"]]
    sys.exit(fb.main())
