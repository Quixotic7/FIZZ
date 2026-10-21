/* SPDX-License-Identifier: GPL-3.0-only */
/* FIZZ (P1.3k: QUADRILLE renamed; the files keep their names) as an FM1VST core (core-api/fm1core.h, ABI 6): the glue
 * between the ABI's panel and the app (core/app). The only unit, with core_quadrille_desc.c, that knows the ABI. Built
 * by FM1VST's core-glue/fizz/core.cmake (fm1_add_core) from this tree.
 *
 * In: at the top of tick, frame and idle the panel's keys and buttons become press / release events (a tap: a press
 * now, its release at the next call), the encoders' detents knob events by role (fm1p_take_role), the MASTER pot (0..1023;
 * 0 at init: the app's default until it moves), all into the app; midi_in's USB-MIDI packets (cable 0: CIN | status << 8
 * | d1 << 16 | d2 << 24) as channel messages into the app at once, and the real-time bytes (CIN 15: the clock,
 * START, CONTINUE, STOP: the sequencer follows them).
 * Time: tick(ms) is the app's clock (simulated: nothing here reads the host's clock).
 * Out: frame draws the screen (every frame, 15 ms) and copies it to the panel's LCD when it changed; init, frame and
 * idle write the LEDs: the app's levels per key and button placed through the FM-1's keymap, PLAY's green LED at its
 * own position. audio: the app's sound (P1.1: FM TONE through the voice pool and ChoralRoot's effects; P1.2: the
 * sequencer runs from this sample clock, a block of 32 frames at a time; P1.3: the controller, core/app/ctl.h). The flash
 * (P1.5a): the common NOR image (core-glue/common nor.h, the host keeps it: FM1VST's <core-id>/flash.bin) is the app's
 * flash (core/app/persist.h: the library and the projects, written by themselves); flash_sync and shutdown write what
 * is still waiting. */
#include <stdio.h>
#include <string.h>

#include "fm1core.h"
#include "version.h"
#include "panel.h"
#include "nor.h"

#include "app.h"
#include "qd_gfx.h"

fm1panel_t fm1core_qd_panel;
static const int8_t fm1core_qd_keymap[6][FM1P_NCOL] = FM1P_KEYMAP_INIT;
static qd_app_t fm1core_qd_app;
static fm1p_edges_t fm1core_qd_kedges, fm1core_qd_bedges;
static uint16_t fm1core_qd_lcd[FM1P_LCD_W * FM1P_LCD_H];
static int fm1core_qd_up;
static int32_t fm1core_qd_master_seen;

/* ---------------------------------------------------------------- in / out --- */
static void fm1core_qd_in(void)
{
    fm1panel_t *p = &fm1core_qd_panel;
    uint32_t down, up, held, taps;
    int i;
    fm1p_edges(&fm1core_qd_kedges, p->keys, fm1p_take_key_taps(p), &down, &up);
    for (i = 0; i < FM1P_NKEY; i++) {
        if ((down >> i) & 1u)
            qd_app_key(&fm1core_qd_app, i, 1, fm1p_vel(p, (uint32_t)i, 100));
        else if ((up >> i) & 1u)
            qd_app_key(&fm1core_qd_app, i, 0, 0);
    }
    /* the matrix buttons as labels (FM1P_B_* order is QUADRILLE's QD_B_* order) */
    held = fm1p_labels(p, p->buttons);
    taps = fm1p_labels(p, fm1p_take_button_taps(p));
    fm1p_edges(&fm1core_qd_bedges, held, taps, &down, &up);
    for (i = 0; i < FM1P_NB; i++) {
        if ((down >> i) & 1u)
            qd_app_button(&fm1core_qd_app, i, 1);
        else if ((up >> i) & 1u)
            qd_app_button(&fm1core_qd_app, i, 0);
    }
    /* the knobs by role (FM1P_E_SELECT .. FM1P_E_K4 is QD_K_SELECT .. QD_K_4) */
    for (i = 0; i < FM1P_NE - 1; i++) {
        const int32_t d = fm1p_take_role(p, i);
        if (d)
            qd_app_knob(&fm1core_qd_app, i, (int)d);
    }
    /* MASTER: the pot as it moves */
    if (p->master != fm1core_qd_master_seen) {
        fm1core_qd_master_seen = p->master;
        qd_app_master(&fm1core_qd_app, (int)fm1core_qd_master_seen);
    }
}

static void fm1core_qd_set_led(int id, uint8_t level)
{
    int c, l;
    if (fm1p_led_pos(&fm1core_qd_keymap[0][0], id, &c, &l))
        fm1core_qd_panel.led[c][l] = level;
}

static void fm1core_qd_leds_out(void)
{
    fm1panel_t *p = &fm1core_qd_panel;
    qd_leds_t l;
    int i;
    qd_app_leds(&fm1core_qd_app, &l);
    for (i = 0; i < FM1P_NKEY; i++)
        fm1core_qd_set_led(FM1P_NBTN + i, l.key[i]);          /* key id 14 + n: note key n */
    for (i = 0; i < FM1P_NB; i++)
        fm1core_qd_set_led(p->btn_id[i], l.button[i]);        /* the label's matrix button */
    p->led[FM1P_PLAY_GREEN_POS >> 3][(FM1P_PLAY_GREEN_POS & 7u) - 1u] = l.play_green;
    p->led_play_green = l.play_green;
}

static void fm1core_qd_lcd_out(void)
{
    qd_app_frame(&fm1core_qd_app, fm1core_qd_lcd);
    if (memcmp(fm1core_qd_panel.lcd, fm1core_qd_lcd, sizeof fm1core_qd_lcd) != 0) {
        memcpy(fm1core_qd_panel.lcd, fm1core_qd_lcd, sizeof fm1core_qd_lcd);
        fm1core_qd_panel.lcd_writes++;
    }
}

/* ---------------------------------------------------------------- the flash (P1.5a) --- */
static int fm1core_qd_fread(void *c, uint32_t off, void *dst, uint32_t n) { (void)c; return fm1nor_read(off, dst, n); }
static int fm1core_qd_ferase(void *c, uint32_t off) { (void)c; return fm1nor_erase(off); }
static int fm1core_qd_fprog(void *c, uint32_t off, const void *src, uint32_t n) { (void)c; return fm1nor_prog(off, src, n); }
static const qd_flash_t fm1core_qd_fl = {fm1core_qd_fread, fm1core_qd_ferase, fm1core_qd_fprog, 0};

/* ---------------------------------------------------------------- the entry points --- */
void fm1core_qd_options(const char *flash_path, int no_flash, int save_on_exit, int headless)
{
    (void)save_on_exit;
    fm1nor_options(flash_path, no_flash, headless, NULL);   /* (no default file: a host that names none gets RAM) */
}

void fm1core_qd_init(int demo)
{
    (void)demo;
    fm1nor_open();
    fm1p_panel_default(&fm1core_qd_panel);                    /* the FM-1's factory wiring */
    memset(&fm1core_qd_kedges, 0, sizeof fm1core_qd_kedges);
    memset(&fm1core_qd_bedges, 0, sizeof fm1core_qd_bedges);
    qd_init();
    qd_app_init(&fm1core_qd_app);
    qd_app_tick(&fm1core_qd_app, 0);
    qd_persist_attach(&fm1core_qd_app, &fm1core_qd_fl);   /* P1.5a: the library, the project worked on */
    fm1core_qd_master_seen = fm1core_qd_panel.master;   /* (0: the host has not said; the app's default stands) */
    if (fm1core_qd_master_seen)
        qd_app_master(&fm1core_qd_app, (int)fm1core_qd_master_seen);
    fm1core_qd_up = 1;
    fm1core_qd_in();
    fm1core_qd_lcd_out();
    fm1core_qd_leds_out();
}

void fm1core_qd_tick(uint32_t ms)
{
    if (!fm1core_qd_up)
        return;
    fm1core_qd_in();
    qd_app_tick(&fm1core_qd_app, ms);
}

void fm1core_qd_frame(void)
{
    if (!fm1core_qd_up)
        return;
    fm1core_qd_in();
    fm1core_qd_lcd_out();
    fm1core_qd_leds_out();
}

void fm1core_qd_idle(void)
{
    if (!fm1core_qd_up)
        return;
    fm1core_qd_in();
    fm1core_qd_leds_out();
}

void fm1core_qd_audio(int16_t *stereo, uint32_t frames)
{
    if (!fm1core_qd_up || fm1nor_stall_take()) {
        memset(stereo, 0, (size_t)frames * 2u * sizeof *stereo);
        return;
    }
    qd_app_audio(&fm1core_qd_app, stereo, frames);
}

int fm1core_qd_midi_in(uint32_t pkt)
{
    const uint32_t cin = pkt & 15u;
    if (fm1core_qd_up && cin >= 8u && cin <= 14u)             /* a channel message (note off .. pitch bend) */
        qd_app_midi(&fm1core_qd_app, (int)((pkt >> 8) & 0xFFu), (int)((pkt >> 16) & 0x7Fu), (int)((pkt >> 24) & 0x7Fu));
    else if (fm1core_qd_up && cin == 15u && ((pkt >> 8) & 0xFFu) >= 0xF8u)   /* real time: the clock, START .. */
        qd_app_midi(&fm1core_qd_app, (int)((pkt >> 8) & 0xFFu), 0, 0);
    return 1;
}
int fm1core_qd_midi_out_take(uint32_t *pkt)
{
    (void)pkt;
    return 0;
}
void fm1core_qd_dump(void) {}
void fm1core_qd_stats(uint32_t *shed, uint32_t *cpu_pct)
{
    if (shed)
        *shed = 0;
    if (cpu_pct)
        *cpu_pct = 0;
}
void fm1core_qd_ui_info(char *buf, uint32_t n)
{
    char pat[4];
    if (buf && n)
        snprintf(buf, n, "%s: %s %s, %d BPM, track %d %s (%s), oct %d, voices %d/%d, keys %07x buttons %04x",
                 fm1core_qd_app.ui.mode == QD_MODE_KEYS     ? "keys"
                 : fm1core_qd_app.ui.mode == QD_MODE_TRACKS ? "tracks"
                 : fm1core_qd_app.ui.mode == QD_MODE_STEP   ? (fm1core_qd_app.ui.nheld ? "step held" : "steps")
                                                            : "idle",
                 qd_pat_name(fm1core_qd_app.seq.cur, pat), fm1core_qd_app.playing ? "playing" : "stopped",
                 fm1core_qd_app.bpm, fm1core_qd_app.sel + 1, fm1core_qd_app.model.t[fm1core_qd_app.sel].name,
                 qd_track_sound_name(&fm1core_qd_app.model.t[fm1core_qd_app.sel]), fm1core_qd_app.oct,
                 qd_pool_in_use(&fm1core_qd_app.pool), fm1core_qd_app.pool.nvoice, (unsigned)fm1core_qd_app.keys_held,
                 (unsigned)fm1core_qd_app.buttons_held);
}
void fm1core_qd_shutdown(void)
{
    if (fm1core_qd_up)
        qd_persist_flush(&fm1core_qd_app);                    /* P1.5a: what still waits, written */
    fm1nor_close();
    qd_shutdown();                                            /* (a later frame draws again: qd_begin re-inits) */
}
int fm1core_qd_halted(void) { return 0; }                     /* nothing reboots or exits */
int fm1core_qd_flash_stage(const uint8_t *bytes, uint32_t n) { return fm1nor_stage(bytes, n); }
uint8_t *fm1core_qd_flash(void) { return fm1nor_flash(); }
uint32_t fm1core_qd_flash_dirty(void) { return fm1nor_dirty(); }
int fm1core_qd_flash_sync(void) { return fm1core_qd_up ? qd_persist_flush(&fm1core_qd_app) : 1; }   /* P1.5a */
const int8_t *fm1core_qd_keymap_get(void) { return &fm1core_qd_keymap[0][0]; }
