# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
# The fast drawing (core/gfx/qd_gfx_fast.c): a fragment of tools/Makefile, used with it
#   make -C tools -f Makefile -f gfx_fast.mk TARGET          (or: include gfx_fast.mk at the end of tools/Makefile)
#
#   fast-font        the text's atlases and outlines, core/gfx/qd_fast_font.h (tools/gen_fast_font.c, from fonts_gen.h)
#   test-gfx-fast    core/tests/test_gfx_fast.c (ASan + UBSan) at the device's sizes
#   gfx-compare      render_frames at the device's sizes with each drawing, every frame compared
#                    (tools/cmp_gfx_fast.py: OUT/gfx/gfx-compare.txt, OUT/gfx/gfx-compare-sheet.png)
#   gfx-bench        the frame's cost by screen, each drawing (tools/bench_gfx.c), and the screens' own logic alone
#                    (tools/qd_gfx_null.c)
# Everything here is built at the device's sizes (QD_DEVICE) into build/gfx/ (gitignored with build/).
GB := build/gfx
GPROF := -DQD_DEVICE
GDEFS := $(DEFS) $(GPROF)
GFX_PVG_OBJ := $(addprefix $(GB)/pvg-,$(notdir $(PVG_SRC:.c=.o)))
# the core's sources without its drawing, the render scripts (render_frames' own and its parts)
GFX_CORE := $(filter-out %/qd_gfx_plutovg.c %/qd_gfx_fast.c,$(CORE_SRC))   # (either GFX)
GFX_CORE_OBJ := $(addprefix $(GB)/,$(notdir $(GFX_CORE:.c=.o)))
GFX_RENDER_OBJ := $(addprefix $(GB)/,$(filter-out render_audio.o,$(patsubst %.c,%.o,$(wildcard render_*.c))))
FAST_FONT := $(ROOT)/core/gfx/qd_fast_font.h
$(GB)/qd_gfx_fast.o: GDEFS += -DQD_GFX_FAST                # (its arena: the unit's size, core/qd_config.h)

$(GB)/pvg-%.o: $(PVG)/source/%.c $(PVG_DEPS)
	@mkdir -p $(GB)
	$(CC) $(OPT) -std=c11 -w $(DEFS) $(GPROF) -DQD_ARENA_REDIRECT -include $(ROOT)/core/gfx/qd_arena.h \
	    -I$(PVG)/include -I$(PVG)/source -c -o $@ $<
$(GB)/%.o: %.c $(HDRS) $(FAST_FONT)
	@mkdir -p $(GB)
	$(CC) $(OPT) -std=c99 -Wall -Wextra -Wpedantic $(GDEFS) $(INC) -c -o $@ $<

# ---- the font: built with the host's plutovg (its rasteriser makes the atlases)
$(B)/gen_fast_font: gen_fast_font.c $(ROOT)/core/gfx/fonts_gen.h $(PVG_OBJ) | $(B)
	$(CC) -O1 -std=c99 -w $(DEFS) -I$(ROOT)/core/gfx -I$(PVG)/include -I$(PVG)/source -o $@ gen_fast_font.c $(PVG_OBJ) -lm
fast-font: $(B)/gen_fast_font
	$(B)/gen_fast_font $(FAST_FONT)

# ---- render_frames with either drawing (the PNGs are written by plutovg in both)
$(GB)/render_frames_pvg: $(GFX_RENDER_OBJ) $(GFX_CORE_OBJ) $(GB)/qd_gfx_plutovg.o $(GFX_PVG_OBJ)
	$(CC) -o $@ $^ -lm
$(GB)/render_frames_fast: $(GFX_RENDER_OBJ) $(GFX_CORE_OBJ) $(GB)/qd_gfx_fast.o $(GFX_PVG_OBJ)
	$(CC) -o $@ $^ -lm

GOUT := $(OUT)/gfx
gfx-compare: $(GB)/render_frames_pvg $(GB)/render_frames_fast
	mkdir -p $(GOUT)/pvg $(GOUT)/fast
	$(GB)/render_frames_pvg $(GOUT)/pvg > $(GOUT)/pvg.log
	$(GB)/render_frames_fast $(GOUT)/fast > $(GOUT)/fast.log
	$(PY) cmp_gfx_fast.py $(GOUT)/pvg $(GOUT)/fast $(GOUT)

# ---- the bench: the same scenes with plutovg, the fast drawing, and no drawing at all
BENCH_OBJ := $(GB)/bench_gfx.o $(GFX_CORE_OBJ)
$(GB)/bench_pvg: $(BENCH_OBJ) $(GB)/qd_gfx_plutovg.o $(GFX_PVG_OBJ)
	$(CC) -o $@ $^ -lm
$(GB)/bench_fast: $(BENCH_OBJ) $(GB)/qd_gfx_fast.o
	$(CC) -o $@ $^ -lm
$(GB)/bench_null: $(BENCH_OBJ) $(GB)/qd_gfx_null.o
	$(CC) -o $@ $^ -lm
gfx-bench: $(GB)/bench_pvg $(GB)/bench_fast $(GB)/bench_null
	$(GB)/bench_null null
	$(GB)/bench_pvg plutovg
	$(GB)/bench_fast fast

# ---- the test (ASan + UBSan, the device's sizes)
$(GB)/test_gfx_fast: $(T)/test_gfx_fast.c $(ROOT)/core/gfx/qd_gfx_fast.c $(FAST_FONT) $(HDRS)
	@mkdir -p $(GB)
	$(CC) $(SAN) -std=c99 -Wall -Wextra -Wpedantic $(GPROF) $(INC) -o $@ $(T)/test_gfx_fast.c $(ROOT)/core/gfx/qd_gfx_fast.c -lm
test-gfx-fast: $(GB)/test_gfx_fast
	$(GB)/test_gfx_fast

.PHONY: fast-font gfx-compare gfx-bench test-gfx-fast
