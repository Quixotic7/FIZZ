#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""FIZZ's core as fm1-base units (fm1.toml gen, run by tools/build.py before each build): one small C file a source of
FIZZ's tools/Makefile lists (CORE_SRC, PVG_SRC), in firmware/app/build/units/, each including its source, so the
firmware builds exactly what FIZZ's own tools build and follows them when they change. plutovg's units take the
drawing's fixed arena first (core/gfx/qd_arena.h with QD_ARENA_REDIRECT: its malloc and free), as tools/Makefile's
device profile builds them. Functions of the drawing are timed for the unit's profile (PROF, fz_prof.h).

  gen_units.py GEN_DIR [--gfx plutovg|fast]   (GEN_DIR: fm1-base's generated headers, APP/build/gen; the units go
                                              beside it)

--gfx (fm1.toml's gen line; default plutovg): the unit's drawing, as FIZZ's tools/Makefile GFX= chooses it. fast:
core/gfx/qd_gfx_fast.c and no plutovg (the Makefile's DEV_PVG_SRC), QD_GFX_FAST defined for every unit of FIZZ's
(GEN_DIR/fz_gfx.h, which fizz_prefix.h includes, so fz_port.c sees the same core).
"""
import subprocess
import sys
from pathlib import Path

APP = Path(__file__).resolve().parents[1]
REPO = APP.parents[1]


def make_list(name, gfx):
    r = subprocess.run(["make", "-s", "-C", str(REPO / "tools"), f"GFX={gfx}", f"print-{name}"], capture_output=True,
                       text=True)
    if r.returncode:
        sys.stderr.write(r.stderr)
        raise SystemExit("gen_units: make print-" + name + " failed")
    return [(REPO / "tools" / s).resolve() for s in r.stdout.split()]


# P2.4: the drawing's profile (fz_prof.h): functions renamed in their own unit and wrapped there, timed (a call from
# inside the unit goes to the renamed one: only the screens' calls are counted, once)
SHAPES = [("qd_line", "float x0, float y0, float x1, float y1, qd_rgb col, float lw", "x0, y0, x1, y1, col, lw"),
          ("qd_polyline", "const float *xy, int npts, int closed, qd_rgb stroke, float lw, qd_rgb fill",
           "xy, npts, closed, stroke, lw, fill"),
          ("qd_circle", "float cx, float cy, float r, qd_rgb stroke, float lw, qd_rgb fill", "cx, cy, r, stroke, lw, fill"),
          ("qd_arc", "float cx, float cy, float r, float a0, float a1, qd_rgb stroke, float lw",
           "cx, cy, r, a0, a1, stroke, lw"),
          ("qd_ellipse", "float cx, float cy, float rx, float ry, qd_rgb stroke, float lw, qd_rgb fill",
           "cx, cy, rx, ry, stroke, lw, fill"),
          ("qd_ellipse_arc", "float cx, float cy, float rx, float ry, float a0, float a1, qd_rgb stroke, float lw",
           "cx, cy, rx, ry, a0, a1, stroke, lw"),
          ("qd_rrect", "float x, float y, float w, float h, float r, qd_rgb stroke, float lw, qd_rgb fill",
           "x, y, w, h, r, stroke, lw, fill"),
          ("qd_fill_rect", "float x, float y, float w, float h, qd_rgb col", "x, y, w, h, col"),
          ("qd_path_fill", "qd_rgb col", "col"), ("qd_path_stroke", "qd_rgb col, float lw", "col, lw"),
          ("qd_path_stroke_keep", "qd_rgb col, float lw", "col, lw"),
          ("qd_path_paint", "qd_rgb stroke, float lw, qd_rgb fill", "stroke, lw, fill")]
PROF = {
    "plutovg-rasterize": [("plutovg_rasterize", "void", "plutovg_span_buffer_t* a, const plutovg_path_t* b, "
                           "const plutovg_matrix_t* c, const plutovg_rect_t* d, const plutovg_stroke_data_t* e, "
                           "plutovg_fill_rule_t f", "a, b, c, d, e, f", "raster")],
    "plutovg-blend": [("plutovg_blend", "void", "plutovg_canvas_t* a, const plutovg_span_buffer_t* b", "a, b", "blend")],
    "plutovg-font": [("plutovg_font_face_get_glyph_path", "float", "plutovg_font_face_t* a, float b, float c, float d, "
                      "plutovg_codepoint_t e, plutovg_path_t* f", "a, b, c, d, e, f", "glyph")],
    "player": [("qd_player_block", "void", "qd_player_t *pl", "pl", "a_seq")],
    # (P2.6: the voices' render is qd_mix_voices since the core's split, 40f2a68; qd_mix_block calls it unwrapped)
    "mix": [("qd_mix_voices", "int", "qd_mix_t *m, qd_pool_t *pool, const qd_mix_track_t *tracks, int ntracks, "
             "int v0, int v1, int32_t *l, int32_t *r, int32_t *sc, int32_t *sd, int32_t *sr",
             "m, pool, tracks, ntracks, v0, v1, l, r, sc, sd, sr", "a_voices")],
    "qd_fx": [("qd_fx_block", "void", "const int32_t *sc, const int32_t *sd, const int32_t *sr, int32_t *l, int32_t *r, "
               "int32_t *out", "sc, sd, sr, l, r, out", "a_fx")],
    "qd_gfx_plutovg": [("qd_end_strip_rows", "const uint16_t *", "uint16_t *a", "a", "convert")],
    # (P2.7: since the core's display list, df6e60f, the drawing calls are qd_gfx_rec.c's in the same unit, in front of
    # the backend's qdb_*: a wrapper by name can no longer sit between them; text and shapes are not timed)
    "qd_gfx_fast": [("qd_end_strip_rows", "const uint16_t *", "uint16_t *a", "a", "convert")],
}


def prof_wrap(stem):
    """the lines before and after the source's include: the functions renamed, then their timed stand-ins"""
    if stem not in PROF:
        return [], []
    pre, post = [], ["#undef " + n for n, *_ in PROF[stem]] + ['#include "fz_prof.h"', "uint32_t plat_ticks24(void);"]
    for name, ret, params, args, field in PROF[stem]:
        real = "fz_real_" + name
        keep = "" if ret == "void" else f"{ret} fz_ret = "
        back = "" if ret == "void" else "    return fz_ret;\n"
        pre.append(f"#define {name} {real}")
        post += [f"{ret} {real}({params});", f"{ret} {name}({params});",
                 f"{ret} {name}({params})\n{{\n    const uint32_t t = plat_ticks24();\n    {keep}{real}({args});\n"
                 f"    fz_prof.{field} += plat_ticks24() - t;\n    fz_prof.n_{field}++;\n{back}}}"]
    return pre, post


def main():
    gen = Path(sys.argv[1]).resolve()
    gfx = sys.argv[sys.argv.index("--gfx") + 1] if "--gfx" in sys.argv else "plutovg"
    if gfx not in ("plutovg", "fast", "fb"):
        raise SystemExit(f"gen_units: --gfx {gfx}: plutovg, fast or fb")
    out = gen.parent / "units"
    out.mkdir(parents=True, exist_ok=True)
    gen.mkdir(parents=True, exist_ok=True)
    head = "/* generated by firmware/app/tools/gen_units.py: the unit's drawing */\n" + \
        ("#define QD_GFX_FAST 1\n" if gfx in ("fast", "fb") else "") + \
        ("#define QD_GFX_FB 1\n" if gfx == "fb" else "")   # (P2.12: the 8-bit frame: the fast drawing into it)
    if not (gen / "fz_gfx.h").exists() or (gen / "fz_gfx.h").read_text() != head:
        (gen / "fz_gfx.h").write_text(head)
    want = {}
    for kind in ("CORE_SRC", "PVG_SRC" if gfx == "plutovg" else "DEV_PVG_SRC"):
        for src in make_list(kind, gfx):
            rel = Path("../" * len(out.relative_to(REPO).parts)) / src.relative_to(REPO)
            stem = ("pvg_" if kind != "CORE_SRC" else "c_") + src.stem.replace("-", "_")
            lines = ["/* generated by firmware/app/tools/gen_units.py: do not edit */"]
            if kind != "CORE_SRC":
                lines += ["#define QD_ARENA_REDIRECT 1",
                          f'#include "{Path("../" * len(out.relative_to(REPO).parts)) / "core/gfx/qd_arena.h"}"']
            pre, post = prof_wrap(src.stem)
            lines += pre
            lines.append(f'#include "{rel.as_posix()}"')
            lines += post
            want[stem + ".c"] = "\n".join(lines) + "\n"
    for f in out.glob("*.c"):
        if f.name not in want:
            f.unlink()
    for name, text in want.items():
        f = out / name
        if not f.exists() or f.read_text() != text:
            f.write_text(text)
    print(f"units: {len(want)} in {out} (the drawing: {gfx})")


if __name__ == "__main__":
    main()
