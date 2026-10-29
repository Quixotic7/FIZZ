# Third-party code in QUADRILLE

## plutovg (`third_party/plutovg/`)

- What: a small anti-aliased 2D vector graphics library in C (paths, strokes, dashes, clipping, text through
  stb_truetype). Phase 1's implementation of the drawing seam, `core/gfx/qd_gfx.h` (`core/gfx/qd_gfx_plutovg.c`).
- Upstream: https://github.com/sammycage/plutovg
- Version: tag **v1.3.3**, commit **bbd91f0d06a71491691b36330f29dffa4af87ccf** (2026-05-18, "Release v1.3.3"),
  the latest release tag on 2026-10-18.
- Copied: `include/`, `source/`, `LICENSE`, `README.md` from a shallow clone of the tag. What qd_gfx needs beyond
  plutovg (font weights, letter spacing, the ellipse's arc, RGB565 out) is built in qd_gfx_plutovg.c on top of it.
  Not copied: the build files, the examples.
- **Changed by FIZZ** (P2.1 .. P2.1c, the device's strips and fixed arena; each change marked "FIZZ" in the file, and
  most built only with `QD_ARENA_REDIRECT`, the device's sizes). These are modified versions, not plutovg's or
  FreeType's originals (FTL §2):
  - `plutovg-rasterize.c`: exact culling of paths outside the strip being drawn (fills by contour, strokes by runs
    of segments, dashes per dash), the path's memory given back once its outline is made, spans converted to short
    fields, the stroker's left border used as the outline (no copy);
  - `plutovg-canvas.c`: `plutovg_canvas_trim` (working arrays above a size freed after a paint),
    `plutovg_canvas_fill_consume` / `stroke_consume`;
  - `plutovg-font.c`: a glyph cache capped (`PLUTOVG_GLYPH_CACHE_MAX`), a smaller initial table,
    `plutovg_font_face_cache_release` (the cache freed on request);
  - `plutovg-private.h`: spans with 16-bit fields under the arena;
  - `plutovg-utils.h`: arrays grow by 1.5x under the arena;
  - FreeType files: `plutovg-ft-types.h` (a 32-bit long under the arena, `PVG_FT_LONG`), `plutovg-ft-raster.c` (its
    positions and divisions on that type), `plutovg-ft-stroker.c` (`PVG_FT_Stroker_TakeLeft`, the left border's
    arrays handed over).
  - `plutovg-ft-raster.c` in WebAssembly only (`__wasm__`, for the .rfm1 that RemoteFM1 builds, which has no
    setjmp / longjmp): the render pool's overflow is a flag instead of a longjmp. The cells after it go to a spare,
    the outline is decomposed to its end, and the band is split as before, so the same rows are drawn. The device's
    and the native builds are unchanged.
- Built with `PLUTOVG_BUILD PLUTOVG_BUILD_STATIC PLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD` (the last leaves out the font
  cache's file and directory loading: the core never reads a file).
- Licences:
  - plutovg: **MIT** (`plutovg/LICENSE`, Copyright (c) 2020-2026 Samuel Ugochukwu).
  - `source/plutovg-ft-*.{c,h}`: from FreeType, under the **FreeType License** (`plutovg/source/FTL.TXT`). Its credit,
    carried in the core's descriptor credits: "Portions of this software are copyright (c) 2006 The FreeType Project
    (www.freetype.org). All rights reserved."
  - `source/plutovg-stb-*.h`: Sean Barrett's stb_truetype, stb_image, stb_image_write, public domain or MIT (their
    choice of either).

To update: shallow-clone the new tag into a temp folder, copy the same files over, rebuild, record the tag and
commit here.

## Inter Tight (`assets/fonts/`)

- What: the type of every screen (PLAN.md §9a), as ChoralRoot's firmware uses it.
- Source: the variable font `InterTight[wght].ttf` in ChoralRoot (`../ChoralRootFM1/assets/fonts/`), from
  https://github.com/rsms/inter-tight.
- Here: static instances at weights 200, 300, 400, 500, 600, 700, subset to ASCII, Latin-1 and · × → – — … −
  (`InterTight-<weight>.ttf`), made by `tools/make_fonts.py` (fontTools' instancer and subsetter), and the same bytes
  as C arrays in `core/gfx/fonts_gen.h`. Generated files, committed.
- Licence: **SIL Open Font License 1.1** (`assets/fonts/OFL.txt`), Copyright 2022 The Inter Project Authors. The
  core carries OFL.txt in its descriptor credits.

## ChoralRoot FM-1 v0.25.1 (`core/voice/quad/`, `core/voice/fx/`, `core/tests/quad_goldens/`)

L1 (2026-10-23): the owner relicensed his ChoralRoot code in FIZZ to MPL-2.0, and the Felucca-derived parts (dsp.c,
cr_core.h, cr_tables.h, fx.c's, master.c, the parts listed in research/PROVENANCE.md) were rewritten clean-room
(core/voice/quad/fz_*.h, tools/gen_fz_tables.py). The notes below describe the copy as it was made in P1.1; the
files they name as copied from Felucca's side are gone.

- What: FM TONE (ChoralRoot's QUAD engine), its tables, the DSP helpers it uses, and ChoralRoot's effects and output
  chain (the stereo chorus, the half-rate delay and reverb, the master filter, Options > Output Clean with its
  look-ahead limiter), so QUADRILLE plays what the unit will play (PLAN.md §13).
- Upstream: `../ChoralRootFM1`, tag **v0.25.1**, commit **15ca8d0** (read with `git show` / `git archive`; nothing there
  is changed).
- Copied: `firmware/src/eng_quad.c`, `quad_tables.h`, `quad_times.h`, `dsp.c`, the parts of `core.h` they read
  (`cr_core.h`), the tables of the generated `felucca_tables.h` they read (`cr_tables.h`, made by `tools/cr_tables.py`),
  `cr_fx.c`, `cr_ram.h`, the needed parts of `fx.c` and `master.c`; `tests/quad_goldens/*.json` (tests/quad_ref.py's
  renders, for `core/tests/test_quad.c`). Every copied file starts with a note: where it came from and what changed.
  The DSP is unchanged; `eng_quad.c`'s changes (a patch per voice, per-track LFO state, the ties to ChoralRoot's tracks
  cut, one out-of-bounds read guarded without changing a result) are listed in its header and marked "QUADRILLE".
  `quad_store.c` (the user-slot flash store) and `perform.c` (built out of ChoralRoot) are not needed.
- Checked against the tag: `make -C tools test` builds ChoralRoot's own engine and effects from the tag
  (`git archive` into `tools/build/cr-v0.25.1`) and compares them with the copies sample for sample.
- Licence: **GPL-3.0-only**, Copyright (C) 2026 ChoralRoot FM-1 contributors, a fork of Felucca (Copyright (C) 2026
  Leo Kuroshita (@kurogedelic), Hügelton Instruments). ChoralRoot credits SLOOP 2.6 (isod89/sloop-fm1, GPL-3.0-only)
  for the designs of the limiter's look-ahead and the stereo chorus's two taps (designs, no code; its LICENSING.md).

To update: `git -C ../ChoralRootFM1 show '<tag>:<path>'` each file, carry the QUADRILLE changes over, run
`make -C tools test` with `CR_TAG=<tag>`, record the tag and commit here and in the files' notes.
