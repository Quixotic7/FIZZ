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
# P2.0: the profile (core/qd_config.h): empty = the host's sizes; make PROFILE=-DQD_DEVICE B=build/dev builds the same
# tools at the FM-1's (make test runs both: test-host, then test-dev)
PROFILE ?=
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
HDRS := $(ROOT)/core/qd_config.h $(ROOT)/core/gfx/qd_arena.c $(wildcard $(ROOT)/core/*/*.h) $(wildcard $(ROOT)/core/voice/*/*.h) $(wildcard $(ROOT)/core/voice/*/*.c) \
        $(wildcard $(ROOT)/core/store/*) $(ROOT)/core/seq/names.c $(ROOT)/core/seq/mfx.c $(ROOT)/core/app/view_labels.c $(ROOT)/core/app/ctl_snd.c $(ROOT)/core/app/ctl_mix.c $(ROOT)/core/app/ctl_lib.c $(ROOT)/core/app/ctl_shift.c $(ROOT)/core/app/ctl_presets.c $(ROOT)/core/app/ctl_type.c $(ROOT)/core/app/undo.c $(ROOT)/core/app/fizz_sx.c $(ROOT)/core/app/persist.c $(ROOT)/core/ui/ui_lib.c $(ROOT)/core/ui/ui_presets.c $(ROOT)/core/ui/ui_type.c $(ROOT)/core/ui/ui_snd.c $(ROOT)/core/ui/ui_mfx.c $(ROOT)/core/app/ctl_mfx.c $(ROOT)/core/ui/ui_ape.c \
        $(PVG)/include/plutovg.h
CORE_OBJ := $(addprefix $(B)/,$(notdir $(CORE_SRC:.c=.o)))
PVG_OBJ := $(addprefix $(B)/pvg-,$(notdir $(PVG_SRC:.c=.o)))
OUT := $(ROOT)/core/tests/out
WARN := -std=c99 -Wall -Wextra -Wpedantic $(PROFILE)
SAN := -O1 -g -fsanitize=address,undefined -fno-sanitize=shift-base -fno-sanitize-recover=all
SAN_ENGINE := -O2 -g -fsanitize=address,signed-integer-overflow -fno-sanitize-recover=all
T := $(ROOT)/core/tests

vpath %.c $(ROOT)/core/app $(ROOT)/core/ui $(ROOT)/core/gfx $(ROOT)/core/seq $(ROOT)/core/voice \
          $(ROOT)/core/voice/quad $(ROOT)/core/voice/fx .

all: render_frames render_audio

render_frames: $(B)/render_frames.o $(B)/render_p13b.o $(B)/render_p13c.o $(B)/render_p13d.o $(B)/render_p13e.o $(B)/render_p13f.o $(B)/render_p13q.o $(B)/render_p13s.o $(B)/render_p13u.o $(B)/render_p13w.o $(B)/render_p13x.o $(B)/render_p13y.o $(B)/render_p14a.o $(B)/render_p14b.o $(B)/render_p15a.o $(B)/render_p15b.o $(B)/render_p15d.o $(B)/render_p15e.o $(B)/render_p15f.o $(B)/render_p15g.o $(B)/render_p15h.o $(B)/render_p15i.o $(B)/render_p15j.o $(B)/render_p15k.o $(B)/render_p15l.o $(B)/render_p15n.o $(B)/render_p15o.o $(B)/render_p15c.o $(B)/render_p15p.o $(B)/render_p15q.o $(B)/render_p15r.o $(B)/render_p15s.o $(B)/render_p15t.o $(B)/render_p15u.o $(B)/render_p15v.o $(B)/render_p15w.o $(B)/render_p15x.o $(B)/render_e1.o $(B)/render_p15y.o $(B)/render_p15z.o $(B)/render_e2.o $(B)/render_e3.o $(B)/render_e4.o $(B)/render_p16c.o $(B)/render_p16d.o $(B)/render_tapape.o $(B)/render_p16e.o $(B)/render_p16g.o $(B)/render_p16h.o $(B)/render_p13p.o $(CORE_OBJ) $(PVG_OBJ)
	$(CC) -o $@ $^ -lm

# P2.0: the same at the device's sizes (make PROFILE=-DQD_DEVICE B=build/dev build/dev/render_frames)
$(B)/render_frames: $(B)/render_frames.o $(B)/render_p13b.o $(B)/render_p13c.o $(B)/render_p13d.o $(B)/render_p13e.o $(B)/render_p13f.o $(B)/render_p13q.o $(B)/render_p13s.o $(B)/render_p13u.o $(B)/render_p13w.o $(B)/render_p13x.o $(B)/render_p13y.o $(B)/render_p14a.o $(B)/render_p14b.o $(B)/render_p15a.o $(B)/render_p15b.o $(B)/render_p15d.o $(B)/render_p15e.o $(B)/render_p15f.o $(B)/render_p15g.o $(B)/render_p15h.o $(B)/render_p15i.o $(B)/render_p15j.o $(B)/render_p15k.o $(B)/render_p15l.o $(B)/render_p15n.o $(B)/render_p15o.o $(B)/render_p15c.o $(B)/render_p15p.o $(B)/render_p15q.o $(B)/render_p15r.o $(B)/render_p15s.o $(B)/render_p15t.o $(B)/render_p15u.o $(B)/render_p15v.o $(B)/render_p15w.o $(B)/render_p15x.o $(B)/render_e1.o $(B)/render_p15y.o $(B)/render_p15z.o $(B)/render_e2.o $(B)/render_e3.o $(B)/render_e4.o $(B)/render_p16c.o $(B)/render_p16d.o $(B)/render_tapape.o $(B)/render_p16e.o $(B)/render_p16g.o $(B)/render_p16h.o $(B)/render_p13p.o $(CORE_OBJ) $(PVG_OBJ)
	$(CC) -o $@ $^ -lm

render_audio: $(B)/render_audio.o $(CORE_OBJ) $(PVG_OBJ)
	$(CC) -o $@ $^ -lm

# P2.1: a flash image as the core reads it, a line a project / pattern / pool sound / MY preset (tools/flash_dump.c)
$(B)/flash_dump: $(B)/flash_dump.o $(CORE_OBJ) $(PVG_OBJ)
	$(CC) -o $@ $^ -lm

# P2.1: at the device's sizes plutovg's malloc / free are the drawing's fixed arena (core/gfx/qd_arena.h)
PVG_ARENA := $(if $(findstring QD_DEVICE,$(PROFILE)),$(PROFILE) -DQD_ARENA_REDIRECT -include $(ROOT)/core/gfx/qd_arena.h,)
PVG_DEPS := $(PVG)/include/plutovg.h $(ROOT)/core/gfx/qd_arena.h $(ROOT)/core/qd_config.h
$(B)/pvg-%.o: $(PVG)/source/%.c $(PVG_DEPS) | $(B)
	$(CC) $(OPT) -std=c11 -w $(DEFS) $(PVG_ARENA) -I$(PVG)/include -I$(PVG)/source -c -o $@ $<

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

# P2.1: the strip test over the render sheets: every frame drawn whole and again in 40-row strips must be the same,
# pixel for pixel, at both sizes (the device's are drawn in strips anyway; the host's whole); the device's arena peak
strips: render_frames
	$(MAKE) PROFILE=-DQD_DEVICE B=build/dev build/dev/render_frames
	mkdir -p $(OUT)/strips-host $(OUT)/strips-dev
	./render_frames $(OUT)/strips-host --strips > $(OUT)/strips-host.log || { tail -3 $(OUT)/strips-host.log; exit 1; }
	build/dev/render_frames $(OUT)/strips-dev --strips > $(OUT)/strips-dev.log || { tail -3 $(OUT)/strips-dev.log; exit 1; }
	@grep -hE "strips:|arena: peak" $(OUT)/strips-host.log $(OUT)/strips-dev.log

audio: render_audio
	mkdir -p $(OUT)
	./render_audio $(OUT)

# ---- the tests
$(B)/test_quad: $(T)/test_quad.c $(HDRS) | $(B)
	$(CC) $(SAN_ENGINE) $(WARN) $(INC) -o $@ $< -lm
$(B)/test_pool: $(T)/test_pool.c $(ROOT)/core/voice/pool.c $(ROOT)/core/voice/quad/quad_voice.c $(HDRS) | $(B)
	$(CC) $(SAN) $(WARN) $(INC) -o $@ $(T)/test_pool.c $(ROOT)/core/voice/pool.c $(ROOT)/core/voice/quad/quad_voice.c -lm
SAN_OBJ := $(addprefix $(B)/san/,$(notdir $(CORE_SRC:.c=.o))) $(addprefix $(B)/san/pvg-,$(notdir $(PVG_SRC:.c=.o)))
$(B)/san/pvg-%.o: $(PVG)/source/%.c $(PVG_DEPS)
	@mkdir -p $(B)/san
	$(CC) $(SAN) -std=c11 -w $(DEFS) $(PVG_ARENA) -I$(PVG)/include -I$(PVG)/source -c -o $@ $<
$(B)/san/%.o: %.c $(HDRS) Makefile
	@mkdir -p $(B)/san
	$(CC) $(SAN) $(WARN) $(DEFS) $(INC) -c -o $@ $<
SEQ_SRC := $(ROOT)/core/seq/pattern.c $(ROOT)/core/seq/seq.c $(ROOT)/core/seq/player.c $(ROOT)/core/seq/model.c \
           $(ROOT)/core/seq/chord.c $(ROOT)/core/seq/scale.c $(ROOT)/core/seq/sounds.c $(ROOT)/core/seq/clip.c $(ROOT)/core/seq/demo.c \
           $(ROOT)/core/voice/pool.c $(ROOT)/core/voice/mix.c $(ROOT)/core/voice/quad/quad_voice.c \
           $(if $(findstring QD_DEVICE,$(PROFILE)),$(ROOT)/core/gfx/qd_arena.c)
$(B)/test_seq: $(T)/test_seq.c $(SEQ_SRC) $(HDRS)
	$(CC) $(SAN) $(WARN) $(INC) -o $@ $(T)/test_seq.c $(SEQ_SRC) -lm
$(B)/test_mfx: $(T)/test_mfx.c $(SEQ_SRC) $(HDRS)
	$(CC) $(SAN) $(WARN) $(INC) -o $@ $(T)/test_mfx.c $(SEQ_SRC) -lm
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

$(B)/test_sysex: $(T)/test_sysex.c $(SAN_OBJ) $(HDRS)
	$(CC) $(SAN) $(WARN) $(DEFS) $(INC) -o $@ $(T)/test_sysex.c $(SAN_OBJ) -lm
$(B)/test_persist: $(T)/test_persist.c $(SAN_OBJ) $(HDRS)
	$(CC) $(SAN) $(WARN) $(DEFS) $(INC) -o $@ $(T)/test_persist.c $(SAN_OBJ) -lm
$(B)/test_store: $(T)/test_store.c $(ROOT)/core/store/store.c $(ROOT)/core/store/store.h | $(B)
	$(CC) $(SAN) $(WARN) -o $@ $(T)/test_store.c $(ROOT)/core/store/store.c
$(B)/test_arena: $(T)/test_arena.c $(ROOT)/core/gfx/qd_arena.c $(ROOT)/core/gfx/qd_arena.h | $(B)
	$(CC) $(SAN) $(WARN) -o $@ $(T)/test_arena.c

DNSND_DIR ?= $(abspath $(ROOT)/../DigitonePresets)
TESTS := $(B)/test_quad $(B)/test_pool $(B)/test_seq $(B)/test_app $(B)/test_ctl $(B)/test_quad_bitexact $(B)/test_fx_bitexact \
         $(B)/test_store $(B)/test_persist $(B)/test_sysex $(B)/test_mfx
test: test-host test-dev budget-ratchet
	@echo "make test: all passed (host and device sizes)"

# P2.0: the same tests at the device's sizes (core/qd_config.h QD_DEVICE), built into build/dev; the timelines into
# core/tests/out/dev (the goldens are the same: the sizes change nothing a pattern sounds like). The bit-exact tests
# compare the voice and the effects with ChoralRoot's code: no size of theirs differs, they run once (test-host)
DEV_TESTS := $(B)/test_quad $(B)/test_pool $(B)/test_seq $(B)/test_mfx $(B)/test_app $(B)/test_ctl $(B)/test_store \
             $(B)/test_persist $(B)/test_sysex $(B)/test_arena
test-dev:
	$(MAKE) PROFILE=-DQD_DEVICE B=build/dev OUT=$(abspath $(OUT))/dev test-dev-run
test-dev-run: $(DEV_TESTS)
	@echo "---- the device's sizes (QD_DEVICE)"
	$(B)/test_quad $(T)/quad_goldens
	$(B)/test_pool
	mkdir -p $(OUT)
	$(B)/test_seq $(T)/seq_goldens $(OUT)
	$(B)/test_mfx
	$(B)/test_app
	$(B)/test_ctl
	$(B)/test_store
	$(B)/test_persist
	$(B)/test_sysex
	$(B)/test_arena
	@echo "make test-dev: all passed (device sizes)"

test-host: $(TESTS)
	$(B)/test_quad $(T)/quad_goldens
	$(B)/test_pool
	mkdir -p $(OUT)
	$(B)/test_seq $(T)/seq_goldens $(OUT)
	$(B)/test_mfx
	$(B)/test_app
	$(B)/test_ctl
	$(B)/test_store
	$(B)/test_persist
	$(B)/test_sysex
	@if [ -d "$(DNSND_DIR)" ]; then node test_sysex_dnsnd.mjs "$(DNSND_DIR)" $(B)/test_sysex; else echo "test_sysex_dnsnd: no $(DNSND_DIR) (skipped)"; fi
	node test_fizz_sysex.mjs $(B)/test_sysex
	$(B)/test_quad_bitexact
	$(B)/test_fx_bitexact
	@echo "make test-host: all passed"

# P1.3i: FIZZ (P1.3k: was QUADRILLE) through the core ABI, as FM1VST's host drives it (dlopen of the built module):
# FM1VST's core-api and the module from its build (both outside this tree: FM1VST_DIR, FM1CORE)
FM1VST_DIR ?= $(abspath $(ROOT)/../FM1VST/.claude/worktrees/quadrille)
FM1CORE ?= $(FM1VST_DIR)/build-quadrille/cores/fizz.fm1core
$(B)/test_abi: test_abi.c | $(B)
	$(CC) -std=c99 -O1 -g -Wall -Wextra -I$(FM1VST_DIR)/core-api -o $@ $< -lm $(if $(filter Linux,$(shell uname)),-ldl,)
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

# P2.0: the device budget (tools/budget/budget.py): RAM (32-bit static + the host bench's heap peak by call site),
# flash (JieLi's pi32v2 clang in Docker when it is up, else the arm-none-eabi link: labelled), stack; budget-gate fails
# when the device profile is over its budget
budget:
	$(PY) budget/budget.py
budget-gate:
	$(PY) budget/budget.py --gate
# P2.1: nothing of the device profile may grow past tools/budget/ratchet.txt (skipped without the arm toolchain)
budget-ratchet: | $(B)
	@$(PY) budget/budget.py --ratchet > $(B)/budget-ratchet.log; s=$$?; \
	    grep -E "^(ratchet|RATCHET|BUDGET-LINE|budget: no)" $(B)/budget-ratchet.log; exit $$s
print-%:
	@echo $($*)

.PHONY: all frames strips audio test test-host test-dev test-dev-run abi midimon remote budget budget-gate budget-ratchet
