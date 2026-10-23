#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
# not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Write core/voice/quad/fz_tables.h: the lookup tables of FIZZ's voice DSP.

Standard library only. Every table is computed in Python doubles, checked against known digests, and the pitch
lookup scheme (one octave of increments plus a right shift) is checked against the direct formula for all 2048
pitches before anything is written. Any mismatch stops the script with a non-zero exit status and no output file.

    python3 tools/gen_fz_tables.py [output path]
"""

import hashlib
import math
import os
import sys

FS = 44100
CTL = 32

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_OUT = os.path.join(HERE, "..", "core", "voice", "quad", "fz_tables.h")

# Expected digests: first 16 hex characters of sha256 over the comma-joined decimal values.
DIGESTS = {
    "SINE": "df65cad28317001d",
    "LFO_INC": "172b7c9ab0f79921",
    "SVF_G": "0f3f634405f511cc",
    "LEVEL_Q12": "22813877c0508647",
    "TANH_Q15": "d7820ad657098430",
}

QE_GLRATE_EXPECT = [
    743, 691, 642, 597, 555, 517, 480, 447, 415, 386, 359, 334, 311, 289, 269, 250, 232, 216, 201, 187, 174, 162,
    150, 140, 130, 121, 112, 104, 97, 90, 84, 78, 72, 67, 63, 58, 54, 50, 47, 43, 40, 37, 35, 32, 30, 28, 26, 24,
    22, 21, 19, 18, 17, 15, 14, 13, 12, 11, 11, 10, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
]

# Pitch scheme: p in 1/16 semitones. One octave = 192 steps. The stored octave is the top one, p = 1856..2047;
# a pitch p sits at slot (p + 64) % 192 of octave (p + 64) // 192 (0..10), and octave 10 is the stored one.
PITCH_STEPS = 192
PITCH_BIAS = 64
PITCH_TOP_OCT = 10
PITCH_TOP_BASE = PITCH_TOP_OCT * PITCH_STEPS - PITCH_BIAS  # 1856


def fail(msg):
    sys.stderr.write("gen_fz_tables: FAILED: " + msg + "\n")
    sys.exit(1)


def digest(values):
    return hashlib.sha256(",".join(str(v) for v in values).encode("ascii")).hexdigest()[:16]


def table_sine():
    return [round(32767 * math.sin(2 * math.pi * i / 1024)) for i in range(1025)]


def table_lfo_inc():
    out = []
    for i in range(128):
        h = 0.05 * 800 ** (i / 127)
        out.append(int(h * CTL / FS * 2 ** 32))
    return out


def table_svf_g():
    out = []
    for i in range(128):
        fc = 30 * (16000 / 30) ** (i / 127)
        out.append(int(4096 * math.tan(math.pi * fc / FS)))
    return out


def table_level_q12():
    out = [0]
    for i in range(1, 128):
        out.append(round(4096 * 10 ** (((i - 112) / 2) / 20)))
    return out


def table_tanh_q15():
    return [int(32767 * math.tanh(i / 128)) for i in range(257)]


def table_glrate():
    out = []
    for i in range(128):
        ms = 10000 ** (i / 127)
        big = max(1, int(2 ** 24 * 32 / (ms * 44.1)))
        out.append(max(1, big >> 14))
    return out


def pitch_inc_exact(p):
    return int(440 * 2 ** ((p / 16 - 69) / 12) / FS * 2 ** 32)


def table_pitch_oct():
    return [pitch_inc_exact(PITCH_TOP_BASE + j) for j in range(PITCH_STEPS)]


def pitch_inc_scheme(oct_table, p):
    # mirrors the C pitch_inc() written into the header
    if p > 2047:
        p = 2047
    q = p + PITCH_BIAS
    return oct_table[q % PITCH_STEPS] >> (PITCH_TOP_OCT - q // PITCH_STEPS)


def check_range(name, values, lo, hi):
    for i, v in enumerate(values):
        if v < lo or v > hi:
            fail("%s[%d] = %d is outside %d..%d" % (name, i, v, lo, hi))


def emit_table(ctype, name, values, per_line=12):
    lines = ["static const %s %s[%d] = {" % (ctype, name, len(values))]
    for k in range(0, len(values), per_line):
        lines.append("    " + ", ".join(str(v) for v in values[k:k + per_line]) + ",")
    lines.append("};")
    return "\n".join(lines)


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_OUT

    tables = {
        "SINE": table_sine(),
        "LFO_INC": table_lfo_inc(),
        "SVF_G": table_svf_g(),
        "LEVEL_Q12": table_level_q12(),
        "TANH_Q15": table_tanh_q15(),
    }
    for name, vals in tables.items():
        got = digest(vals)
        print("%-10s n=%4d sha256[:16]=%s %s" % (name, len(vals), got, "ok" if got == DIGESTS[name] else "MISMATCH"))
        if got != DIGESTS[name]:
            fail("%s digest %s, expected %s" % (name, got, DIGESTS[name]))

    glrate = table_glrate()
    if glrate[0] != 743:
        fail("QE_GLRATE[0] = %d, expected 743" % glrate[0])
    if glrate != QE_GLRATE_EXPECT:
        bad = [i for i in range(128) if glrate[i] != QE_GLRATE_EXPECT[i]]
        fail("QE_GLRATE differs from QE_GLRATE_EXPECT at %s" % bad[:10])
    print("QE_GLRATE  n= 128 [0]=%d matches QE_GLRATE_EXPECT ok" % glrate[0])

    check_range("SINE", tables["SINE"], -32768, 32767)
    check_range("LFO_INC", tables["LFO_INC"], 0, 0xFFFFFFFF)
    check_range("SVF_G", tables["SVF_G"], 0, 0xFFFF)
    check_range("LEVEL_Q12", tables["LEVEL_Q12"], 0, 0xFFFF)
    check_range("TANH_Q15", tables["TANH_Q15"], -32768, 32767)
    check_range("QE_GLRATE", glrate, 0, 0xFFFF)

    pitch = table_pitch_oct()
    check_range("PITCH_OCT", pitch, 0, 0x7FFFFFFF)
    for p in range(2048):
        want = pitch_inc_exact(p)
        if want > 0x7FFFFFFF:
            fail("inc(%d) = %d exceeds 0x7FFFFFFF" % (p, want))
        got = pitch_inc_scheme(pitch, p)
        if got != want:
            fail("pitch_inc scheme gives %d for p=%d, the formula %d" % (got, p, want))
    if pitch_inc_scheme(pitch, 5000) != pitch_inc_exact(2047):
        fail("pitch_inc clamp")
    print("pitch_inc  one octave (%d x uint32 = %d bytes) + shift: exact for all 2048 pitches ok"
          % (PITCH_STEPS, PITCH_STEPS * 4))

    parts = [
        "/* SPDX-License-Identifier: MPL-2.0",
        " * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was",
        " * not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */",
        "",
        "/* generated by tools/gen_fz_tables.py: do not edit */",
        "",
        "#ifndef FZ_TABLES_H",
        "#define FZ_TABLES_H",
        "",
        "#include <stdint.h>",
        "",
        "#ifndef FS",
        "#define FS 44100",
        "#endif",
        "#ifndef CTL",
        "#define CTL 32",
        "#endif",
        "",
        "/* one sine cycle in 1024 steps plus a guard point, Q15 */",
        emit_table("int16_t", "SINE", tables["SINE"]),
        "",
        "/* LFO phase increment per control block (CTL samples), 0.05 Hz .. 40 Hz exponential */",
        emit_table("uint32_t", "LFO_INC", tables["LFO_INC"], 8),
        "",
        "/* state-variable filter g = tan(pi fc / FS) in Q12, fc 30 Hz .. 16 kHz exponential */",
        emit_table("uint16_t", "SVF_G", tables["SVF_G"]),
        "",
        "/* level in Q12: 0 is silence, 112 is unity, 0.5 dB per step */",
        emit_table("uint16_t", "LEVEL_Q12", tables["LEVEL_Q12"]),
        "",
        "/* tanh(i / 128) in Q15, i = 0..256 */",
        emit_table("int16_t", "TANH_Q15", tables["TANH_Q15"]),
        "",
        "/* glide rate per control block, 1 ms .. 10 s exponential */",
        emit_table("uint16_t", "QE_GLRATE", glrate),
        "",
        "/* phase increments of the top octave, p = %d..2047 (1/16 semitones) */" % PITCH_TOP_BASE,
        emit_table("uint32_t", "FZ_PITCH_OCT", pitch, 8),
        "",
        "/* phase increment (Q32 of FS) of pitch p in 1/16 semitones, MIDI note n at 16 n; p above 2047 counts as 2047.",
        " * Equal to int(440 * 2^((p/16 - 69)/12) / FS * 2^32) for every p (checked by the generator). */",
        "static inline uint32_t pitch_inc(uint32_t p)",
        "{",
        "    uint32_t q;",
        "    if (p > 2047u)",
        "        p = 2047u;",
        "    q = p + %du;" % PITCH_BIAS,
        "    return FZ_PITCH_OCT[q %% %du] >> (%du - q / %du);" % (PITCH_STEPS, PITCH_TOP_OCT, PITCH_STEPS),
        "}",
        "",
        "#endif /* FZ_TABLES_H */",
        "",
    ]
    text = "\n".join(parts)
    with open(out_path, "w", newline="\n") as f:
        f.write(text)
    print("wrote %s" % os.path.normpath(out_path))


if __name__ == "__main__":
    main()
