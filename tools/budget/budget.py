#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""FIZZ's device budget (P2.0; research/PHASE2-BUDGET.md): the core at the device's sizes (core/qd_config.h, QD_DEVICE)
measured three ways, against the FM-1's resources.

  make -C tools budget        the report
  make -C tools budget-gate   the report, and a failure when the device profile is over its budget

  RAM    static: the core linked for a 32-bit target (arm-none-eabi, Cortex-M4 with a single-precision FPU, -Os,
         --gc-sections from tools/budget/dev_main.c: what the platform calls), .data + .bss by symbol;
         heap: the host bench (tools/budget/bench.c) built at the device's sizes, every allocation counted by its call
         site, through the screens that cost the most (the peak over the run);
         the LCD buffer the core draws into (the platform's DMA, counted against FIZZ).
  flash  JieLi's own pi32v2 clang (Docker, debian:bookworm-slim, ~/.jieli/toolchain; Melodee's FPU flags) when Docker
         is up: each object's function sections that the 32-bit link keeps, plus the kept constants; else the
         arm-none-eabi link's .text + .rodata, labelled as the approximation it is. libc / libm / libgcc from the link.
  stack  -fcallgraph-info=su: the deepest static chain from qd_app_frame (the main stack, with the input calls) and
         from qd_app_audio (the audio ISR's stack). Calls through function pointers are not followed (the controller's
         rows, the latches): the figures are lower bounds; the largest frames are listed too.

The budget's numbers are named below (BUDGET_*), each with where it comes from (D: docs and linker scripts, E: an
estimate, M: measured), as in the audit."""
import concurrent.futures as cf
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOLS = HERE.parent
ROOT = TOOLS.parent
OUT = TOOLS / "build" / "budget"
ARM = Path(os.environ.get("ARM_GCC_DIR", str(Path.home() / "arm-gnu-toolchain-14.2" / "bin")))
JIELI = Path(os.environ.get("JIELI_TOOLCHAIN", str(Path.home() / ".jieli" / "toolchain")))

# ---- the FM-1 (research/PHASE2-BUDGET.md §1)
FM1_RAM = 98304 + 344064 + 15664      # D: RAM (.data + .bss) + POOL + NOINIT (ChoralRoot's app.ld): 458 032 B
PLATFORM_RAM = 25000                  # E: the platform's own (HAL, ISR state, USB MIDI + UAC, cpu1, the flash driver)
HEADROOM_RAM = 8000                   # E: kept free (ChoralRoot's build demands >= 8 KiB spare in POOL)
BUDGET_RAM = FM1_RAM - PLATFORM_RAM - HEADROOM_RAM   # ~425 KB for FIZZ (the audit's "about 425 KiB")
APP_SLOT = 573440                     # D: the app slot, XIP 0x02001000 (app.ld)
PLATFORM_FLASH = 55000                # E: HAL, ISR, cpu1, USB MIDI + UAC, the LCD driver, OTA / loader, the stub's glue
MARGIN_FLASH = 8192                   # E: kept free in the slot
MAIN_STACK = 24320                    # D: the user stack (crt0.S)
ISR_STACK = 7936                      # D: the system stack (the audio ISR)
MARGIN_STACK = 2048                   # E: kept free on each

CC_FLAGS = ["-Os", "-ffunction-sections", "-fdata-sections", "-DQD_DEVICE", "-DQD_TARGET_FM1", "-DPLUTOVG_BUILD", "-DPLUTOVG_BUILD_STATIC",
            "-DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD"]
ARM_FLAGS = ["-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=hard", "-mfpu=fpv4-sp-d16"]
PI_FLAGS = ["-target", "pi32v2", "-Os", "-ffunction-sections", "-fno-builtin", "-w", "-mcpu=r3", "-mfprev1",
            "-ffp-contract=off", "-D__STDC_NO_ATOMICS__", "-DQD_DEVICE", "-DQD_TARGET_FM1", "-DPLUTOVG_BUILD", "-DPLUTOVG_BUILD_STATIC",
            "-DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD"]


# P2.1: at the device's sizes plutovg's malloc / free are the drawing's fixed arena (core/gfx/qd_arena.h; tools/Makefile
# PVG_ARENA): a static block, counted with the statics; the bench reports how much of it the screens used
PVG_ARENA = ["-DQD_ARENA_REDIRECT", "-include", str(ROOT / "core/gfx/qd_arena.h")]


def sh(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.stderr.write(" ".join(map(str, cmd)) + "\n" + r.stdout + r.stderr)
        raise SystemExit("budget: a command failed")
    return r.stdout


def make_var(name):
    return sh(["make", "-s", "-C", str(TOOLS), f"print-{name}"]).split()


def sources():
    core = [(TOOLS / s).resolve() for s in make_var("CORE_SRC")]
    pvg = [(TOOLS / s).resolve() for s in make_var("PVG_SRC")]
    inc = [s[2:] for s in make_var("INC")]
    inc = [str((TOOLS / i).resolve()) for i in inc]
    return core, pvg, inc


def compile_all(jobs):
    with cf.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        list(ex.map(lambda c: sh(c), jobs))


# ---------------------------------------------------------------- the 32-bit link
def arm_build(core, pvg, inc):
    d = OUT / "arm"
    d.mkdir(parents=True, exist_ok=True)
    gcc = str(ARM / "arm-none-eabi-gcc")
    incf = [f"-I{i}" for i in inc] + [f"-I{ROOT / 'third_party/plutovg/source'}"]
    jobs, objs = [], []
    for s in core + [HERE / "dev_main.c"]:
        o = d / (s.stem + ".o")
        objs.append(o)
        jobs.append([gcc, *ARM_FLAGS, *CC_FLAGS, "-std=c99", "-fstack-usage", "-fcallgraph-info=su", *incf, "-c", "-o",
                     str(o), str(s)])
    for s in pvg:
        o = d / ("pvg-" + s.stem + ".o")
        objs.append(o)
        jobs.append([gcc, *ARM_FLAGS, *CC_FLAGS, "-std=c11", "-w", "-Wno-incompatible-pointer-types", "-fstack-usage", "-fcallgraph-info=su", *incf,
                     *PVG_ARENA, "-c", "-o", str(o), str(s)])
    compile_all(jobs)
    elf = d / "fizz-dev.elf"
    sh([gcc, *ARM_FLAGS, "--specs=nano.specs", "--specs=nosys.specs", "-Wl,--gc-sections", f"-Wl,-Map={d / 'fizz-dev.map'}",
        "-o", str(elf), *map(str, objs), "-lm"])
    return d, elf


def parse_map(path):
    """input sections kept in the link: (section, size, file)"""
    out, pend = [], None
    in_disc = False
    for line in open(path, errors="replace"):
        if line.startswith("Discarded input sections"):
            in_disc = True
            continue
        if line.startswith("Memory Configuration") or line.startswith("Linker script and memory map"):
            in_disc = False
            continue
        if in_disc:
            continue
        m = re.match(r"^ (\.\S+|COMMON)\s+0x[0-9a-f]+\s+0x([0-9a-f]+)\s+(\S+)", line)
        if m:
            out.append((m.group(1), int(m.group(2), 16), m.group(3)))
            pend = None
            continue
        m = re.match(r"^ (\.\S+|COMMON)\s*$", line)
        if m:
            pend = m.group(1)
            continue
        if pend:
            m = re.match(r"^\s+0x[0-9a-f]+\s+0x([0-9a-f]+)\s+(\S+)", line)
            if m:
                out.append((pend, int(m.group(1), 16), m.group(2)))
            pend = None
    return out


def kind_of(sec):
    for k in (".text", ".rodata", ".data", ".bss"):
        if sec.startswith(k):
            return k
    return ".bss" if sec == "COMMON" else None


def owner_of(f):
    if "/arm/pvg-" in f:
        return "plutovg"
    if "/arm/" in f:
        return "core"
    return "libc / libm / libgcc"


def module_of(f):
    n = Path(f).stem
    groups = [("voice", ["quad_voice", "pool", "mix", "qd_fx"]), ("seq", ["model", "pattern", "seq", "player", "demo",
              "chord", "scale", "sounds", "clip"]), ("app", ["app", "ctl", "ctl_step", "ctl_rec", "ctl_leds",
              "ctl_latch", "ctl_preview", "view"]), ("ui", ["ui_kit", "ui_idle", "ui_keys", "ui_picker", "ui_tracks",
              "ui_lane", "ui_held", "ui_turn", "ui_layer", "ui_octave"]), ("gfx + fonts", ["qd_gfx_plutovg"]),
              ("the app (one qd_app_t)", ["dev_main"])]
    for g, names in groups:
        if n in names:
            return g
    return owner_of(f)


def fn_base(sec):
    """.text.quad_render.constprop.0 -> quad_render (the two compilers name their clones apart)"""
    return sec[len(".text."):].split(".")[0]


def arm_sizes(d, elf):
    secs = parse_map(d / "fizz-dev.map")
    by = {}
    for sec, size, f in secs:
        k = kind_of(sec)
        if not k:
            continue
        o = owner_of(f)
        by.setdefault(o, {}).setdefault(k, 0)
        by[o][k] += size
    mods = {}
    for sec, size, f in secs:
        k = kind_of(sec)
        if k and owner_of(f) in ("core", "plutovg"):
            mods.setdefault(module_of(f), {}).setdefault(k, 0)
            mods[module_of(f)][k] += size
    kept_fn = set()
    for sec, size, f in secs:
        if sec.startswith(".text.") and owner_of(f) in ("core", "plutovg"):
            kept_fn.add((Path(f).name, fn_base(sec)))
    syms = []
    for line in sh([str(ARM / "arm-none-eabi-nm"), "-S", "--size-sort", "-t", "d", str(elf)]).splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in "bBdD":
            syms.append((int(p[1]), p[3]))
    syms.sort(reverse=True)
    return by, mods, kept_fn, syms


# ---------------------------------------------------------------- the stack (the call graph's deepest chains)
def stack_chains(d, kept_fn):
    nodes, edges = {}, {}
    for ci in d.glob("*.ci"):
        fname = ci.stem
        txt = ci.read_text(errors="replace")
        for m in re.finditer(r'node: \{ title: "([^"]+)" label: "([^"]*)"', txt):
            title, label = m.group(1), m.group(2)
            sm = re.search(r"\\n(\d+) bytes", label)
            if sm:
                nodes[(fname, title)] = int(sm.group(1))
        for m in re.finditer(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"', txt):
            edges.setdefault((fname, m.group(1)), []).append(m.group(2))
    by_name = {}
    for (f, t), sz in nodes.items():
        by_name.setdefault(t, []).append((f, t))
    memo = {}

    def resolve(f, name):
        if (f, name) in nodes:
            return (f, name)
        c = by_name.get(name)
        return c[0] if c else None

    def depth(n, path):
        if n in memo:
            return memo[n]
        if n in path:
            return (0, ["(recursion: " + n[1] + ")"])
        path = path | {n}
        best = (0, [])
        for t in edges.get(n, []):
            r = resolve(n[0], t)
            if r:
                d2 = depth(r, path)
                if d2[0] > best[0]:
                    best = d2
        res = (nodes.get(n, 0) + best[0], [f"{n[1].split(':')[-1]} {nodes.get(n, 0)}"] + best[1])
        memo[n] = res
        return res

    out = {}
    for root in ("qd_app_frame", "qd_app_audio", "qd_app_tick", "qd_app_key", "qd_app_button", "qd_app_knob",
                 "qd_app_sysex"):
        r = resolve("app", root)
        if r:
            out[root] = depth(r, frozenset())
    frames = sorted(((sz, f"{t.split(':')[-1]} ({f})") for (f, t), sz in nodes.items()
                     if (f + ".o", t.split(":")[-1].split(".")[0]) in kept_fn), reverse=True)[:12]
    return out, frames


# ---------------------------------------------------------------- the host bench: the heap
def host_bench(core, pvg, inc):
    d = OUT / "host"
    d.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get("CC", "cc")
    hw = str(HERE / "heapwrap.h")
    incf = [f"-I{i}" for i in inc]
    jobs, objs = [], []
    for s in core:
        o = d / (s.stem + ".o")
        objs.append(o)
        jobs.append([cc, "-O2", "-g", "-std=c99", "-w", "-DQD_DEVICE", "-DPLUTOVG_BUILD", "-DPLUTOVG_BUILD_STATIC",
                     "-DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD", "-include", hw, *incf, "-c", "-o", str(o), str(s)])
    for s in pvg:
        o = d / ("pvg-" + s.stem + ".o")
        objs.append(o)
        jobs.append([cc, "-O2", "-g", "-std=c11", "-w", "-DQD_DEVICE", "-DPLUTOVG_BUILD", "-DPLUTOVG_BUILD_STATIC",
                     "-DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD", *PVG_ARENA, *incf,
                     f"-I{ROOT / 'third_party/plutovg/source'}", "-c", "-o", str(o), str(s)])
    for s in (HERE / "bench.c", HERE / "heapwrap.c"):
        o = d / (s.stem + ".o")
        objs.append(o)
        jobs.append([cc, "-O2", "-g", "-std=c99", "-w", "-DQD_DEVICE", *incf, "-c", "-o", str(o), str(s)])
    compile_all(jobs)
    exe = d / "bench"
    sh([cc, "-o", str(exe), *map(str, objs), "-lm"])
    res = {"sites": []}
    for line in sh([str(exe)]).splitlines():
        p = line.split()
        if not p or p[0] != "BUDGET":
            continue
        if p[1] == "heap_site":
            res["sites"].append((p[2], int(p[3]), int(p[4])))
        else:
            res[p[1]] = p[2:]
    return res


# ---------------------------------------------------------------- pi32v2 (Docker)
def docker_up():
    if not shutil.which("docker"):
        return False
    return subprocess.run(["docker", "info"], capture_output=True).returncode == 0


def pi_build(core, pvg, inc):
    d = OUT / "pi"
    d.mkdir(parents=True, exist_ok=True)
    root = ROOT.resolve()

    def cpath(p):
        return "/fizz/" + str(Path(p).resolve().relative_to(root))
    lines = ["set -e"]
    incf = " ".join(f"-I{cpath(i)}" for i in inc) + f" -I/fizz/third_party/plutovg/source -idirafter /opt/jieli/pi32v2/include"
    flags = " ".join(PI_FLAGS)
    for s in core:
        lines.append(f"/opt/jieli/pi32v2/bin/clang {flags} -std=gnu99 {incf} -c -o /out/{s.stem}.o {cpath(s)} &")
    for s in pvg:
        lines.append(f"/opt/jieli/pi32v2/bin/clang {flags} -std=gnu11 {incf} -c -o /out/pvg-{s.stem}.o {cpath(s)} &")
    lines.append("wait")
    (d / "build.sh").write_text("\n".join(lines) + "\n")
    sh(["docker", "run", "--rm", "--platform", "linux/amd64", "-v", f"{root}:/fizz:ro", "-v", f"{JIELI}:/opt/jieli:ro",
        "-v", f"{d}:/out", "debian:bookworm-slim", "sh", "/out/build.sh"])
    return d


def pi_sizes(d, kept_fn, arm_mods_ro):
    readelf = str(ARM / "arm-none-eabi-readelf")
    text_kept = text_all = 0
    per = {}
    for o in sorted(d.glob("*.o")):
        out = sh([readelf, "-S", "-W", str(o)])
        for m in re.finditer(r"\]\s+(\.text\S*)\s+\S+\s+[0-9a-f]+\s+[0-9a-f]+\s+([0-9a-f]+)", out):
            sec, size = m.group(1), int(m.group(2), 16)
            text_all += size
            # the arm map names the object fizz-dev's way (pvg-x.o, x.o): the same stems
            if (o.name, fn_base(sec)) in kept_fn:
                text_kept += size
                per[module_of("/arm/" + o.name)] = per.get(module_of("/arm/" + o.name), 0) + size
    return text_kept, text_all, per


# ---------------------------------------------------------------- the report
def kb(n):
    return f"{n / 1024:7.1f} KiB"


def main():
    gate = "--gate" in sys.argv
    if not (ARM / "arm-none-eabi-gcc").exists():
        print(f"budget: no arm-none-eabi-gcc in {ARM} (ARM_GCC_DIR): skipped")
        return 0
    core, pvg, inc = sources()
    print(f"FIZZ's device budget (core/qd_config.h QD_DEVICE), {ROOT.name} {sh(['git', '-C', str(ROOT), 'rev-parse', '--short', 'HEAD']).strip()}")
    d, elf = arm_build(core, pvg, inc)
    by, mods, kept_fn, syms = arm_sizes(d, elf)
    hb = host_bench(core, pvg, inc)
    stacks, frames = stack_chains(d, kept_fn)

    lcd = next((sz for sz, n in syms if n == "budget_lcd"), 0)
    core_ram = sum(by.get(o, {}).get(k, 0) for o in ("core", "plutovg") for k in (".data", ".bss")) - lcd
    lib_ram = sum(by.get("libc / libm / libgcc", {}).get(k, 0) for k in (".data", ".bss"))
    heap = int(hb["heap_peak"][0])
    ram = core_ram + heap + lcd
    print("\nRAM (32-bit static, M: the arm-none-eabi link; heap, M: the host bench at the device's sizes)")
    for m in sorted(mods):
        r = mods[m].get(".data", 0) + mods[m].get(".bss", 0)
        print(f"  static {m:<22} {r:>9} B {kb(r)}")
    print(f"  static, the core + plutovg   {core_ram:>9} B {kb(core_ram)}")
    print(f"  heap peak (bench)            {heap:>9} B {kb(heap)}   (live at the end {int(hb['heap_live'][0])}; plutovg's"
          f" in the arena)")
    ap, asz, af = (int(x) for x in hb["arena"])
    print(f"  the drawing's arena          {asz:>9} B {kb(asz)}   (a static above: the bench's screens used {ap} at most,"
          f" {af} requests refused)")
    print(f"  the LCD frame                {lcd:>9} B {kb(lcd)}   (what the core draws into: 240 x 240 RGB565 today)")
    print(f"  libc / libm's own            {lib_ram:>9} B   (the platform's: not counted)")
    print(f"  FIZZ's RAM                   {ram:>9} B {kb(ram)}   budget {BUDGET_RAM} B {kb(BUDGET_RAM)}"
          f" = {FM1_RAM} (the FM-1) - {PLATFORM_RAM} (platform, E) - {HEADROOM_RAM} (headroom)")
    print("  the largest statics:")
    for sz, n in syms[:16]:
        print(f"    {sz:>9} {n}")
    print("  the heap's largest sites (each one's own peak):")
    for site, peak, blk in hb["sites"][:10]:
        print(f"    {peak:>9} {site} (largest block {blk})")
    print(f"  the sequencer's queues over the demos: events {hb['seq_ev_peak'][0]} of {hb['seq_ev_peak'][1]}, trig records "
          f"{hb['seq_rec_peak'][0]} of {hb['seq_rec_peak'][1]}, dropped {' / '.join(hb['seq_dropped'])}")

    lib_flash = sum(by.get("libc / libm / libgcc", {}).get(k, 0) for k in (".text", ".rodata", ".data"))
    arm_flash = sum(by.get(o, {}).get(k, 0) for o in ("core", "plutovg") for k in (".text", ".rodata", ".data"))
    arm_ro = sum(by.get(o, {}).get(k, 0) for o in ("core", "plutovg") for k in (".rodata", ".data"))
    flash, how = arm_flash, "arm-none-eabi link (Thumb-2: an approximation of pi32v2)"
    print("\nflash")
    for m in sorted(mods):
        t, r = mods[m].get(".text", 0), mods[m].get(".rodata", 0) + mods[m].get(".data", 0)
        print(f"  arm {m:<22} code {t:>8}  constants {r:>8}")
    if docker_up():
        pd = pi_build(core, pvg, inc)
        kept, total, per = pi_sizes(pd, kept_fn, arm_ro)
        flash = kept + arm_ro
        how = "pi32v2 clang 4.0.1 + FPU flags: the code the link keeps, plus the kept constants"
        for m in sorted(per):
            print(f"  pi32v2 {m:<19} code {per[m]:>8}")
        print(f"  pi32v2 code kept {kept} B of {total} B compiled; constants {arm_ro} B")
    else:
        print("  (Docker is not up: the pi32v2 figure skipped; the arm-none-eabi approximation used)")
    budget_flash = APP_SLOT - PLATFORM_FLASH - lib_flash - MARGIN_FLASH
    print(f"  FIZZ's flash                 {flash:>9} B {kb(flash)}   ({how})")
    print(f"  arm-none-eabi                {arm_flash:>9} B {kb(arm_flash)}")
    print(f"  libc / libm / libgcc (arm)   {lib_flash:>9} B {kb(lib_flash)}")
    print(f"  budget                       {budget_flash:>9} B {kb(budget_flash)} = {APP_SLOT} (the app slot) - "
          f"{PLATFORM_FLASH} (platform, E) - {lib_flash} (libc, M) - {MARGIN_FLASH} (margin)")

    print("\nstack (static chains: calls through pointers not followed, a lower bound)")
    for root, (n, chain) in stacks.items():
        print(f"  {root:<14} {n:>6} B: {' > '.join(chain[:9])}{' ..' if len(chain) > 9 else ''}")
    ui = max(stacks.get(r, (0, []))[0] for r in ("qd_app_frame", "qd_app_key", "qd_app_button", "qd_app_knob",
                                                 "qd_app_tick", "qd_app_sysex"))
    au = stacks.get("qd_app_audio", (0, []))[0]
    print(f"  the main loop's deepest {ui} B of {MAIN_STACK} (margin {MARGIN_STACK}); the audio's {au} B of {ISR_STACK}")
    print("  the largest frames:")
    for sz, n in frames[:8]:
        print(f"    {sz:>6} {n}")

    over = []
    if ram > BUDGET_RAM:
        over.append(f"RAM {ram} > {BUDGET_RAM} (over by {ram - BUDGET_RAM} B)")
    if flash > budget_flash:
        over.append(f"flash {flash} > {budget_flash} (over by {flash - budget_flash} B)")
    if ui + MARGIN_STACK > MAIN_STACK:
        over.append(f"main stack {ui} + {MARGIN_STACK} > {MAIN_STACK}")
    if au + MARGIN_STACK > ISR_STACK:
        over.append(f"audio stack {au} + {MARGIN_STACK} > {ISR_STACK}")
    print("\nBUDGET-LINE ram=%d/%d flash=%d/%d stack_main=%d/%d stack_audio=%d/%d" % (ram, BUDGET_RAM, flash,
          budget_flash, ui, MAIN_STACK, au, ISR_STACK))
    if over:
        print("OVER BUDGET: " + "; ".join(over))
        return 1 if gate else 0
    print("within budget")
    return 0


if __name__ == "__main__":
    sys.exit(main())
