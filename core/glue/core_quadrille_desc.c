/* SPDX-License-Identifier: GPL-3.0-only */
/* QUADRILLE's descriptor (core-api/fm1core.h, ABI 6): the one exported symbol of quadrille.fm1core. Its own unit
 * (-Wall -Wextra); the entry points are core_quadrille.c's. The button names are QUADRILLE's roles (PLAN.md §8), in
 * the panel's order: FX = TRACKS, SEL = PATTERN, ENV = PAGE, LFO = MUTE, EDIT = SOUND, GLO = FUNC, HOME, SAVE,
 * ARP = KEYS, SEQ = MIX, PLAY, REC, OCT-, OCT+. No parameter map yet: every knob is relative. */
#include <stddef.h>

#include "fm1core.h"
#include "version.h"

extern fm1panel_t fm1core_qd_panel;
void fm1core_qd_options(const char *flash_path, int no_flash, int save_on_exit, int headless);
void fm1core_qd_init(int demo);
void fm1core_qd_tick(uint32_t ms);
void fm1core_qd_frame(void);
void fm1core_qd_idle(void);
void fm1core_qd_audio(int16_t *stereo, uint32_t frames);
int fm1core_qd_midi_in(uint32_t pkt);
int fm1core_qd_midi_out_take(uint32_t *pkt);
void fm1core_qd_dump(void);
void fm1core_qd_stats(uint32_t *shed, uint32_t *cpu_pct);
void fm1core_qd_ui_info(char *buf, uint32_t n);
void fm1core_qd_shutdown(void);
int fm1core_qd_halted(void);
int fm1core_qd_flash_stage(const uint8_t *bytes, uint32_t n);
uint8_t *fm1core_qd_flash(void);
uint32_t fm1core_qd_flash_dirty(void);
int fm1core_qd_flash_sync(void);
const int8_t *fm1core_qd_keymap_get(void);

static fm1core_t CORE = {
    .abi_version = FM1CORE_ABI,
    .id = FM1CORE_ID,
    .name = "QUADRILLE",
    .version = FM1CORE_VERSION,
    .source_url = FM1CORE_SOURCE_URL("FMToneSeq (local)"),
    .family = FM1CORE_FAMILY,
    .license = FM1CORE_LICENSE,
    .gated = FM1CORE_GATED_OR_NULL,
    .credits = FM1CORE_CREDITS_OR_NULL,
    .flash_size = 0x100000u,
    .button_names = {"TRACKS", "PATTERN", "PAGE", "MUTE", "SOUND", "FUNC", "HOME", "SAVE", "KEYS", "MIX", "PLAY", "REC",
                     "OCT-", "OCT+"},
    .params = NULL,
    .nparams = 0,
    .panel = &fm1core_qd_panel,
    .keymap = NULL,
    .options = fm1core_qd_options,
    .boot_options = NULL,
    .init = fm1core_qd_init,
    .tick = fm1core_qd_tick,
    .frame = fm1core_qd_frame,
    .idle = fm1core_qd_idle,
    .audio = fm1core_qd_audio,
    .midi_in = fm1core_qd_midi_in,
    .midi_out_take = fm1core_qd_midi_out_take,
    .dump = fm1core_qd_dump,
    .stats = fm1core_qd_stats,
    .ui_info = fm1core_qd_ui_info,
    .shutdown = fm1core_qd_shutdown,
    .halted = fm1core_qd_halted,
    .flash_stage = fm1core_qd_flash_stage,
    .flash = fm1core_qd_flash,
    .flash_dirty = fm1core_qd_flash_dirty,
    .flash_sync = fm1core_qd_flash_sync,
    .param_epoch = NULL,
    .knob_target = NULL,
};

FM1CORE_EXPORT const fm1core_t *fm1core_get(uint32_t abi_version)
{
    if (abi_version != FM1CORE_ABI)
        return NULL;
    if (!CORE.keymap)
        CORE.keymap = fm1core_qd_keymap_get();
    return &CORE;
}
