# QUADRILLE's tools. `make -C tools` builds render_frames; `make -C tools frames` also runs it into core/tests/out/.
# render_frames compiles the core's own sources (app, ui, gfx) as C99 with warnings on, and plutovg (vendored,
# C11, its warnings off) directly: no FM1VST needed. Objects go to tools/build/ (gitignored).
ROOT := ..
CC ?= cc
OPT ?= -O2 -g
PVG := $(ROOT)/third_party/plutovg
B := build
CORE_SRC := $(ROOT)/core/app/app.c $(ROOT)/core/ui/ui_kit.c $(ROOT)/core/ui/ui_idle.c \
            $(ROOT)/core/gfx/qd_gfx_plutovg.c render_frames.c
PVG_SRC := $(wildcard $(PVG)/source/*.c)
INC := -I$(ROOT)/core/app -I$(ROOT)/core/ui -I$(ROOT)/core/gfx -I$(PVG)/include
DEFS := -DPLUTOVG_BUILD -DPLUTOVG_BUILD_STATIC -DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD
HDRS := $(wildcard $(ROOT)/core/*/*.h) $(PVG)/include/plutovg.h
CORE_OBJ := $(addprefix $(B)/,$(notdir $(CORE_SRC:.c=.o)))
PVG_OBJ := $(addprefix $(B)/pvg-,$(notdir $(PVG_SRC:.c=.o)))
OUT := $(ROOT)/core/tests/out

vpath %.c $(ROOT)/core/app $(ROOT)/core/ui $(ROOT)/core/gfx .

all: render_frames

render_frames: $(CORE_OBJ) $(PVG_OBJ)
	$(CC) -o $@ $^ -lm

$(B)/pvg-%.o: $(PVG)/source/%.c $(PVG)/include/plutovg.h | $(B)
	$(CC) $(OPT) -std=c11 -w $(DEFS) -I$(PVG)/include -I$(PVG)/source -c -o $@ $<

$(B)/%.o: %.c $(HDRS) Makefile | $(B)
	$(CC) $(OPT) -std=c99 -Wall -Wextra -Wpedantic $(DEFS) $(INC) -c -o $@ $<

$(B):
	mkdir -p $(B)

frames: render_frames
	mkdir -p $(OUT)
	./render_frames $(OUT)

.PHONY: all frames
