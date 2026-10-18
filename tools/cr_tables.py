#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The ChoralRoot tables QUADRILLE's voice layer needs, copied out of ChoralRoot's generated felucca_tables.h.

ChoralRoot v0.25.1 (15ca8d0) generates build/gen/felucca_tables.h with tools/gen_tables.py (FS 44100, CTL 32). FM TONE
(eng_quad.c, dsp.c) reads SINE, PITCH_OCT / pitch_inc, SVF_G and TANH_Q15; the chorus (cr_fx.c xc_block) reads
LFO_INC; the mix (fx.c mix_part_loop) reads LEVEL_Q12. This script copies those blocks verbatim (each with its own
comment line) into core/voice/quad/cr_tables.h, so QUADRILLE builds without ChoralRoot's generators.

    python3 tools/cr_tables.py CR/build/gen/felucca_tables.h core/voice/quad/cr_tables.h

To make the input: `git -C ../ChoralRootFM1 archive v0.25.1 tools/gen_tables.py | tar -x -C TMP` and
`python3 TMP/tools/gen_tables.py TMP/felucca_tables.h`. tools/Makefile's bit-exact test regenerates it from the tag
and compares the engine's output, so a table copied wrong shows there.
"""
import re
import sys

NAMES = ["SINE", "PITCH_OCT", "pitch_inc", "LFO_INC", "SVF_G", "LEVEL_Q12", "TANH_Q15"]


def blocks(lines):
    """name -> its lines (the comment line before it, if any, and the definition through its closing brace)"""
    out = {}
    i = 0
    while i < len(lines):
        m = re.match(r"static (?:inline |const )*\w+ (\w+)[\[(]", lines[i])
        if not m:
            i += 1
            continue
        name = m.group(1)
        start = i - 1 if i and lines[i - 1].startswith("/*") else i
        j = i
        while not (lines[j].startswith("};") or lines[j].startswith("}")):
            j += 1
        out[name] = lines[start:j + 1]
        i = j + 1
    return out


def main(src, dst):
    lines = open(src).read().split("\n")
    b = blocks(lines)
    missing = [n for n in NAMES if n not in b]
    if missing:
        raise SystemExit(f"cr_tables: not in {src}: {', '.join(missing)}")
    out = ["/* SPDX-License-Identifier: GPL-3.0-only",
           " * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments",
           " * Copyright (C) 2026 ChoralRoot FM-1 contributors (a fork of Felucca) */",
           "/* Copied from ChoralRoot v0.25.1 (15ca8d0): build/gen/felucca_tables.h, as tools/gen_tables.py generates it",
           " * (GPL-3.0-only). QUADRILLE: only the tables its voice layer reads, each block verbatim (made by",
           " * tools/cr_tables.py: do not edit); FS and CTL as the generated header defines them. */",
           "#pragma once",
           "#include <stdint.h>",
           "#define FS 44100",
           "#define CTL 32",
           ""]
    for n in NAMES:
        out += b[n]
        out.append("")
    open(dst, "w").write("\n".join(out))
    print(f"cr_tables: {', '.join(NAMES)} -> {dst}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
