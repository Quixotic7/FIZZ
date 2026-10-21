# FIZZ's tools (P1.3k: QUADRILLE renamed). Objects go to tools/build/ (gitignored); outputs to core/tests/out/
# (gitignored).
#   make -C tools              render_frames, render_audio
#   make -C tools frames       render_frames into core/tests/out/ (PNGs of the screens, the LEDs)
#   make -C tools audio        render_audio into core/tests/out/ (presets.wav, tracks.wav, pool-stress.wav, the CPU, the
#                              demo patterns: demo-A01..A03.wav and demo.txt)
#   make -C tools abi          (P1.3i) the built quadrille.fm1core driven through the core ABI (FM1VST_DIR, FM1CORE)
#   make -C tools test         the voice layer's and the sequencer's tests (sanitized) and the bit-exact tests against
#                              ChoralRoot's tag; test_seq writes its timelines to core/tests/out/ and compares them
#                              with core/tests/seq_goldens/ (build/test_seq ../core/tests/seq_goldens ../core/tests/out
#                              --bless rewrites the goldens: only when a change of the timelines is meant)
# render_frames and render_audio compile the core's own sources (app, ui, gfx, voice, seq) as C99 with warnings on,
# and plutovg (vendored, C11, its warnings off) directly: no FM1VST needed.
#
# The tests: test_quad (ChoralRoot's cr_quad_test checks on the copied engine: -fsanitize=signed-integer-overflow, as
# ChoralRoot builds it, and ASan), test_pool, test_seq and test_app (ASan + UBSan; ChoralRoot's DSP shifts negative values left,
# as its firmware does on purpose: -fno-sanitize=shift-base). test_quad_bitexact and test_fx_bitexact compare the copies
# with ChoralRoot's own code at CR_TAG, taken from the ChoralRoot checkout CR_REPO with `git archive` (nothing there
# is checked out or changed) into tools/build/cr-<tag>, its generated headers made by the tag's own tools.
ROOT := ..
CC ?= cc
OPT ?= -O2 -g
PVG := $(ROOT)/third_party/plutovg
B := build
CR_REPO ?= $(abspath $(ROOT)/../ChoralRootFM1)
CR_TAG ?= v0.25.1
CRB := $(B)/cr-$(CR_TAG)
PY ?= python3

VOICE_SRC := $(ROOT)/core/voice/quad/quad_voice.c $(ROOT)/core/voice/pool.c $(ROOT)/core/voice/mix.c \
             $(ROOT)/core/voice/fx/qd_fx.c
APP_SRC := $(ROOT)/core/app/app.c $(ROOT)/core/app/ctl.c $(ROOT)/core/app/ctl_step.c $(ROOT)/core/app/ctl_rec.c \
           $(ROOT)/core/app/ctl_leds.c $(ROOT)/core/app/ctl_latch.c $(ROOT)/core/app/ctl_preview.c \
           $(ROOT)/core/app/view.c
UI_SRC := $(ROOT)/core/ui/ui_kit.c $(ROOT)/core/ui/ui_idle.c $(ROOT)/core/ui/ui_keys.c $(ROOT)/core/ui/ui_picker.c \
          $(ROOT)/core/ui/ui_tracks.c $(ROOT)/core/ui/ui_lane.c $(ROOT)/core/ui/ui_held.c $(ROOT)/core/ui/ui_turn.c \
          $(ROOT)/core/ui/ui_layer.c $(ROOT)/core/ui/ui_octave.c
CORE_SRC := $(APP_SRC) $(UI_SRC) $(ROOT)/core/gfx/qd_gfx_plutovg.c \
            $(ROOT)/core/seq/model.c $(ROOT)/core/seq/pattern.c $(ROOT)/core/seq/seq.c $(ROOT)/core/seq/player.c \
            $(ROOT)/core/seq/demo.c $(ROOT)/core/seq/chord.c $(ROOT)/core/seq/scale.c $(ROOT)/core/seq/sounds.c \
            $(ROOT)/core/seq/clip.c $(VOICE_SRC)
PVG_SRC := $(wildcard $(PVG)/source/*.c)
INC := -I$(ROOT)/core/app -I$(ROOT)/core/ui -I$(ROOT)/core/gfx -I$(ROOT)/core/seq -I$(ROOT)/core/voice \
       -I$(ROOT)/core/voice/quad -I$(ROOT)/core/voice/fx -I$(ROOT)/core/tests -I$(PVG)/include
DEFS := -DPLUTOVG_BUILD -DPLUTOVG_BUILD_STATIC -DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD
HDRS := $(wildcard $(ROOT)/core/*/*.h) $(wildcard $(ROOT)/core/voice/*/*.h) $(wildcard $(ROOT)/core/voice/*/*.c) \
        $(wildcard $(ROOT)/core/store/*) $(ROOT)/core/app/ctl_snd.c $(ROOT)/core/app/ctl_mix.c $(ROOT)/core/app/ctl_lib.c $(ROOT)/core/app/persist.c $(ROOT)/core/ui/ui_lib.c $(ROOT)/core/ui/ui_snd.c \
        $(PVG)/include/plutovg.h
CORE_OBJ := $(addprefix $(B)/,$(notdir $(CORE_SRC:.c=.o)))
PVG_OBJ := $(addprefix $(B)/pvg-,$(notdir $(PVG_SRC:.c=.o)))
OUT := $(ROOT)/core/tests/out
WARN := -std=c99 -Wall -Wextra -Wpedantic
SAN := -O1 -g -fsanitize=address,undefined -fno-sanitize=shift-base -fno-sanitize-recover=all
SAN_ENGINE := -O2 -g -fsanitize=address,signed-integer-overflow -fno-sanitize-recover=all
T := $(ROOT)/core/tests

vpath %.c $(ROOT)/core/app $(ROOT)/core/ui $(ROOT)/core/gfx $(ROOT)/core/seq $(ROOT)/core/voice \
          $(ROOT)/core/voice/quad $(ROOT)/core/voice/fx .

all: render_frames render_audio

render_frames: $(B)/render_frames.o $(B)/render_p13b.o $(B)/render_p13c.o $(B)/render_p13d.o $(B)/render_p13e.o $(B)/render_p13f.o $(B)/render_p13q.o $(B)/render_p13s.o $(B)/render_p13u.o $(B)/render_p13w.o $(B)/render_p13x.o $(B)/render_p13y.o $(B)/render_p14a.o $(B)/render_p14b.o $(B)/render_p15a.o $(B)/render_p15b.o $(B)/render_p15d.o $(B)/render_p15e.o $(B)/render_p15f.o $(B)/render_p15g.o $(B)/render_p15h.o $(B)/render_p15i.o $(B)/render_p15j.o $(B)/render_p13p.o $(CORE_OBJ) $(PVG_OBJ)
	$(CC) -o $@ $^ -lm

render_audio: $(B)/render_audio.o $(CORE_OBJ) $(PVG_OBJ)
	$(CC) -o $@ $^ -lm

$(B)/pvg-%.o: $(PVG)/source/%.c $(PVG)/include/plutovg.h | $(B)
	$(CC) $(OPT) -std=c11 -w $(DEFS) -I$(PVG)/include -I$(PVG)/source -c -o $@ $<

$(B)/%.o: %.c $(HDRS) Makefile | $(B)
	$(CC) $(OPT) $(WARN) $(DEFS) $(INC) -c -o $@ $<

$(B):
	mkdir -p $(B)

FFMPEG ?= $(firstword $(wildcard /opt/homebrew/bin/ffmpeg /usr/local/bin/ffmpeg) ffmpeg)
frames: render_frames
	mkdir -p $(OUT)
	./render_frames $(OUT)
	@# P1.3p: the logo screen's whole loop as a GIF, 2x (pixels doubled), 15 fps, 32 colours (skipped without ffmpeg)
	-$(FFMPEG) -loglevel error -y -framerate 15 -i $(OUT)/fizz-logo/f-%03d.png \
	    -vf "scale=480:480:flags=neighbor,split[a][b];[a]palettegen=max_colors=32[p];[b][p]paletteuse=dither=none" \
	    $(OUT)/fizz-logo.gif && echo "$(OUT)/fizz-logo.gif" 

audio: render_audio
	mkdir -p $(OUT)
	./render_audio $(OUT)

# ---- the tests
$(B)/test_quad: $(T)/test_quad.c $(HDRS) | $(B)
	$(CC) $(SAN_ENGINE) $(WARN) $(INC) -o $@ $< -lm
$(B)/test_pool: $(T)/test_pool.c $(ROOT)/core/voice/pool.c $(ROOT)/core/voice/quad/quad_voice.c $(HDRS) | $(B)
	$(CC) $(SAN) $(WARN) $(INC) -o $@ $(T)/test_pool.c $(ROOT)/core/voice/pool.c $(ROOT)/core/voice/quad/quad_voice.c -lm
SAN_OBJ := $(addprefix $(B)/san/,$(notdir $(CORE_SRC:.c=.o))) $(addprefix $(B)/san/pvg-,$(notdir $(PVG_SRC:.c=.o)))
$(B)/san/pvg-%.o: $(PVG)/source/%.c $(PVG)/include/plutovg.h
	@mkdir -p $(B)/san
	$(CC) $(SAN) -std=c11 -w $(DEFS) -I$(PVG)/include -I$(PVG)/source -c -o $@ $<
$(B)/san/%.o: %.c $(HDRS) Makefile
	@mkdir -p $(B)/san
	$(CC) $(SAN) $(WARN) $(DEFS) $(INC) -c -o $@ $<
SEQ_SRC := $(ROOT)/core/seq/pattern.c $(ROOT)/core/seq/seq.c $(ROOT)/core/seq/player.c $(ROOT)/core/seq/model.c \
           $(ROOT)/core/seq/chord.c $(ROOT)/core/seq/scale.c $(ROOT)/core/seq/sounds.c $(ROOT)/core/seq/clip.c $(ROOT)/core/seq/demo.c \
           $(ROOT)/core/voice/pool.c $(ROOT)/core/voice/mix.c $(ROOT)/core/voice/quad/quad_voice.c
$(B)/test_seq: $(T)/test_seq.c $(SEQ_SRC) $(HDRS)
	$(CC) $(SAN) $(WARN) $(INC) -o $@ $(T)/test_seq.c $(SEQ_SRC) -lm
$(B)/test_app: $(T)/test_app.c $(SAN_OBJ) $(HDRS)
	$(CC) $(SAN) $(WARN) $(DEFS) $(INC) -o $@ $(T)/test_app.c $(SAN_OBJ) -lm
$(B)/test_ctl: $(T)/test_ctl.c $(SAN_OBJ) $(HDRS)
	$(CC) $(SAN) $(WARN) $(DEFS) $(INC) -o $@ $(T)/test_ctl.c $(SAN_OBJ) -lm

# the ChoralRoot tag, its generated headers (the tag's own generators)
$(CRB)/.ready: | $(B)
	@test -d $(CR_REPO)/.git || (echo "no ChoralRoot checkout at CR_REPO=$(CR_REPO)"; exit 1)
	mkdir -p $(CRB)
	git -C $(CR_REPO) archive $(CR_TAG) firmware tests tools assets/cz1-factory assets/prophet5-factory | tar -x -C $(CRB)
	mkdir -p $(CRB)/build/gen
	cd $(CRB) && $(PY) tools/gen_tables.py build/gen/felucca_tables.h && \
	    $(PY) tools/gen_samples.py build/gen/felucca_samples.h && \
	    $(PY) tools/gen_fm6_patches.py build/gen/felucca_fm6.h && \
	    $(PY) tools/gen_cz1_factory.py build/gen/melodee_cz1.h && \
	    $(PY) tools/gen_prophet_factory.py build/gen/melodee_prophet_factory.h
	touch $@
CR_INC := -I$(CRB)/firmware/src -I$(CRB)/build/gen -I$(CRB)/tests -I$(T)
$(B)/cr_quad_ref.o: $(T)/ref/cr_quad_ref.c $(T)/ref/bitexact.h $(CRB)/.ready
	$(CC) -std=c99 -O2 -w $(CR_INC) -c -o $@ $<
$(B)/cr_fx_ref.o: $(T)/ref/cr_fx_ref.c $(T)/ref/bitexact.h $(CRB)/.ready
	$(CC) -std=gnu11 -O2 -w $(CR_INC) -c -o $@ $<
$(B)/test_quad_bitexact: $(T)/test_quad_bitexact.c $(ROOT)/core/voice/quad/quad_voice.c $(B)/cr_quad_ref.o $(HDRS)
	$(CC) -O2 $(WARN) $(INC) -o $@ $(T)/test_quad_bitexact.c $(ROOT)/core/voice/quad/quad_voice.c $(B)/cr_quad_ref.o
$(B)/test_fx_bitexact: $(T)/test_fx_bitexact.c $(ROOT)/core/voice/quad/quad_voice.c $(B)/cr_fx_ref.o $(HDRS)
	$(CC) -O2 $(WARN) $(INC) -o $@ $(T)/test_fx_bitexact.c $(ROOT)/core/voice/quad/quad_voice.c $(B)/cr_fx_ref.o -lm

$(B)/test_persist: $(T)/test_persist.c $(SAN_OBJ) $(HDRS)
	$(CC) $(SAN) $(WARN) $(DEFS) $(INC) -o $@ $(T)/test_persist.c $(SAN_OBJ) -lm
$(B)/test_store: $(T)/test_store.c $(ROOT)/core/store/store.c $(ROOT)/core/store/store.h | $(B)
	$(CC) $(SAN) $(WARN) -o $@ $(T)/test_store.c $(ROOT)/core/store/store.c

TESTS := $(B)/test_quad $(B)/test_pool $(B)/test_seq $(B)/test_app $(B)/test_ctl $(B)/test_quad_bitexact $(B)/test_fx_bitexact \
         $(B)/test_store $(B)/test_persist
test: $(TESTS)
	$(B)/test_quad $(T)/quad_goldens
	$(B)/test_pool
	mkdir -p $(OUT)
	$(B)/test_seq $(T)/seq_goldens $(OUT)
	$(B)/test_app
	$(B)/test_ctl
	$(B)/test_store
	$(B)/test_persist
	$(B)/test_quad_bitexact
	$(B)/test_fx_bitexact
	@echo "make test: all passed"

# P1.3i: FIZZ (P1.3k: was QUADRILLE) through the core ABI, as FM1VST's host drives it (dlopen of the built module):
# FM1VST's core-api and the module from its build (both outside this tree: FM1VST_DIR, FM1CORE)
FM1VST_DIR ?= $(abspath $(ROOT)/../FM1VST/.claude/worktrees/quadrille)
FM1CORE ?= $(FM1VST_DIR)/build-quadrille/cores/fizz.fm1core
$(B)/test_abi: test_abi.c | $(B)
	$(CC) -std=c99 -O1 -g -Wall -Wextra -I$(FM1VST_DIR)/core-api -o $@ $< $(if $(filter Linux,$(shell uname)),-ldl,)
abi: $(B)/test_abi
	$(B)/test_abi $(FM1CORE)

# P1.3j: FIZZ through FM1VST's processor and the Remote FM-1 path, with libremotefm1's fake unit
# (tools/test_remote.cpp, built with FM1VST's remote_test flags and libraries from its build tree)
FM1VST_BUILD ?= $(FM1VST_DIR)/build-quadrille
remote:
	$(PY) build_remote.py $(FM1VST_BUILD) $(abspath test_remote.cpp) $(abspath $(B))/test_remote
	$(B)/test_remote $(abspath $(B))/remote_home

# P1.3j: a passive MIDI logger (macOS) for what the unit really sends (tools/midimon.c)
$(B)/midimon: midimon.c | $(B)
	$(CC) -O1 -Wall -o $@ $< -framework CoreMIDI -framework CoreFoundation
midimon: $(B)/midimon

.PHONY: all frames audio test abi midimon remote
