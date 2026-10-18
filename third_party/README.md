# Third-party code in QUADRILLE

## plutovg (`third_party/plutovg/`)

- What: a small anti-aliased 2D vector graphics library in C (paths, strokes, dashes, clipping, text through
  stb_truetype). Phase 1's implementation of the drawing seam, `core/gfx/qd_gfx.h` (`core/gfx/qd_gfx_plutovg.c`).
- Upstream: https://github.com/sammycage/plutovg
- Version: tag **v1.3.3**, commit **bbd91f0d06a71491691b36330f29dffa4af87ccf** (2026-05-18, "Release v1.3.3"),
  the latest release tag on 2026-10-18.
- Copied: `include/`, `source/`, `LICENSE`, `README.md` from a shallow clone of the tag, **unmodified**. What qd_gfx
  needs beyond plutovg (font weights, letter spacing, the ellipse's arc, RGB565 out) is built in qd_gfx_plutovg.c on
  top of it. Not copied: the build files, the examples.
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
