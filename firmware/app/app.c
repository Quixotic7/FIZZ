/* SPDX-License-Identifier: MPL-2.0 */
/* FIZZ on fm1-base: fm1-base's side of the app (compiled inside its unity build, after the platform: gfx.c, lcd.c and
 * the plat_* calls are in scope). FIZZ's core is behind fizz_port.h (fz_port.c).
 *
 * TWO CONTEXTS (P2.4). The sound is made in the audio interrupt (app_render, every 256 frames: 5.8 ms), as ChoralRoot
 * makes it: FIZZ's sequencer and voices run from it, and MIDI's sound goes there (P2.5, qd_app_midi_play: the clock and
 * the transport, the notes to the player, the controllers). A slow frame can never starve it. Everything else that
 * changes FIZZ runs on the main loop: the panel, MIDI's UI part (qd_app_midi_ui, queued by the interrupt with its
 * time), FIZZ's clock (plat_ms), SysEx; each call
 * with the audio interrupt masked around it (P2.7: that interrupt alone, fz_lock; the key and LED scan and USB run
 * on), the clock a few ms at a time, so the interrupt never meets an edit
 * half made and waits at most one call. Those calls are made between frames and between the strips of a frame (a press
 * acts within a strip's time, and FIZZ's clock is given up to each press's own time first, so a hold is a hold). The
 * drawing, which only reads, runs with the interrupts on: what it shows may move between two strips (the playhead).
 * fm1-base's own MASTER: the render's gain (APP_GAIN of the pot), as the template has it; FIZZ's own volume stays at
 * its default. The store (P2.5): FIZZ's projects and MY PRESETS in the data region, its writes a step every 15 ms on the
 * main loop between frames, outside the locks (the flash driver masks the interrupts a page at a time; it erases only
 * when FIZZ is quiet: stopped, silent, untouched 10 s).
 *
 * A frame is FIZZ's 40-row strips (qd_app_frame_strips); only the rows whose pixels changed (a hash a row) go to the
 * LCD, gathered in a stash borrowed from the drawing's arena and sent in one burst at the frame's end (P2.7). A frame is looked at 30 a second for 1.5 s after a
 * touch, 20 while playing, 15 on the idle screen, 10 otherwise, and drawn only when its picture would change
 * (qd_app_frame_changed: the screen's logic once, nothing drawn). The rest: the LEDs (FIZZ's 0..255 a light onto fm1-base's off / glow / mid / lit), USB SysEx both
 * ways (APP_SYSEX: FIZZ's 7D 46 5A, and 7D 46 50 for this port's counters and profile), the data blob's check and its
 * writer (APP_USB_CMD, firmware/tools/fizz_xdata.py). */
#include "fizz_port.h"
#include "fz_prof.h"
#include "fz_dual.c"                     /* the second core's manager (firmware/DUALCORE.md): until FIZZ's core has its
                                          * split render and felucca-bsp its core-1 start (issue #5), it boots the
                                          * simulated core and no block hands it anything */

/* the core 1 to start: felucca-bsp's chip/fm1_cpu1.* behind an fz_c1_backend_t once issue #5 lands (FZ_C1_BACKEND
 * set by build.py then); until then core 0 itself, through the mailbox. FZ_SINGLE_CORE 1 (build.py --single-core): none */
#ifndef FZ_C1_BACKEND
#define FZ_C1_BACKEND (&fz_c1_simulated)
#endif
#ifdef PLAT_HOST
static fz_c1_bar_t fz_c1_bar;
#else
static fz_c1_bar_t fz_c1_bar __attribute__((section(".noinit")));   /* "core 1 off": kept across a reset */
void app_fault(const fm1_crash_t *c)     /* (APP_FAULT) a crash: with a real core 1 running, the next boot keeps it off */
{
    (void)c;
    if (FZ_C1_BACKEND != &fz_c1_simulated)
        fz_dual_crashed(&fz_c1_bar);
}
#endif

static void fz_message(const char *a, const char *b);
#ifndef PLAT_HOST
static int remote_frame(const uint8_t *p, uint32_t n);   /* (main_fm1.c's, later in the unity build) */
#endif
void fz_watchdog(void)                   /* (fizz_port.h: the store's flash flows feed it at each operation) */
{
#ifndef PLAT_HOST
    fm1_wdt_feed();
#endif
}
#ifndef PLAT_HOST
extern uint32_t _fz_rt_start[], _fz_rt_end[], _fz_ni_start[], _fz_ni_end[];   /* (fizz.ld: cleared by app_init) */
#endif

#define FZ_BARRIER() __asm__ volatile("" ::: "memory")
#define FZ_LED_MS 10u                    /* the LEDs looked at every 10 ms */
#define FZ_VEL 100                       /* the keys' velocity (no APP_TOUCH yet: FM1VST's default) */
#define FZ_TICK_CHUNK 4u                 /* FIZZ's clock: at most this many ms a lock */
#define FZ_TOUCHED_MS 1500u              /* a frame every FZ_FRAME_TOUCHED for this long after a press or a turn */
#define FZ_FRAME_TOUCHED 33u             /* how often a frame is looked at (drawn if it changed) */
#define FZ_FRAME_PLAYING 50u
#define FZ_FRAME_IDLE 66u                /* the idle screen's animation */
#define FZ_FRAME_QUIET 100u
#define FZ_STORE_MS 15u                  /* the store's step (qd_persist_step) */

/* the profile (fz_prof.h): here, in RAM with the platform's (FIZZ's units' .bss is the pool's, which is full) */
volatile fz_prof_t fz_prof;
fz_screen_t fz_screens[FZ_NSCREEN];
void fz_prof_reset(void)
{
    uint32_t i;
    volatile uint8_t *p = (volatile uint8_t *)&fz_prof;
    for (i = 0; i < sizeof fz_prof; i++)
        p[i] = 0;
    for (i = 0; i < FZ_NSCREEN; i++)
        fz_screens[i].n = fz_screens[i].max = 0, fz_screens[i].sum = 0;
}

/* ---------------------------------------------------------------- the locks --- */
/* (P2.7) FIZZ's state is shared with the audio interrupt only: FIZZ has no APP_TICK, nothing on TIMER5 (the scan,
 * USB) reads or writes it, and the MIDI queue it takes in the audio interrupt is fm1-base's own. So its locks mask
 * ALNK0 alone, not every interrupt: the 10 kHz scan (the LEDs, the keys) and USB keep their rhythm through every lock.
 * The host's (the simulator: one thread) are plat_lock's */
#ifdef PLAT_HOST
#define fz_audio_off() plat_lock()
#define fz_audio_on_again(f) plat_unlock(f)
#else
static uint32_t fz_mask_depth;
static uint32_t fz_audio_off(void)
{
    if (!fz_mask_depth++) {              /* (the register's write inside cli .. csync sti: in effect before FIZZ's state
                                          * is touched, and no interrupt between its read and its write) */
        fm1_irq_off();
        fm1_irq_mask(FM1_IRQ_ALNK0);
        fm1_irq_on();
    }
    return 0;
}
static void fz_audio_on_again(uint32_t f)
{
    (void)f;
    if (fz_mask_depth && !--fz_mask_depth) {
        fm1_irq_off();
        fm1_irq_unmask(FM1_IRQ_ALNK0);
        fm1_irq_on();
    }
}
#endif
static uint32_t fz_lock_t0;
static uint32_t fz_lock(void)
{
    const uint32_t f = fz_audio_off();
    fz_lock_t0 = plat_ticks24();
    return f;
}
static void fz_unlock(uint32_t f, uint32_t kind)
{
    const uint32_t d = plat_ticks24() - fz_lock_t0;
    if (d > fz_prof.lock_max)
        fz_prof.lock_max = d, fz_prof.lock_kind = kind;
    fz_audio_on_again(f);
}

/* ---------------------------------------------------------------- the audio interrupt --- */
static int16_t fz_abuf[2u * APP_BLOCK];
static volatile uint8_t fz_audio_on;     /* 0: silence (no FIZZ, a panic, an update) */
static volatile uint8_t fz_in_frame;     /* (the profile: the interrupt's time inside frames) */
static volatile uint8_t fz_in_render;    /* (P2.7, the screen's stash: a borrow from the audio interrupt never takes it) */
/* (P2.7) the screen's and the LEDs' counters (stats line 7) */
static uint32_t fz_rows_sent, fz_bursts, fz_burst_rows, fz_burst_max, fz_frame_bursts, fz_frame_bursts_max, fz_st_full,
    fz_st_drops, fz_st_none, fz_st_cap_min = 240u, fz_frames_sent, fz_led_looks, fz_led_writes;
#define FZ_MQ 128u                       /* MIDI's channel messages' UI part for the main loop, with their time */
static uint32_t fz_mq_pkt[FZ_MQ], fz_mq_ms[FZ_MQ];
static volatile uint32_t fz_mq_w, fz_mq_r, fz_mq_drops, fz_midi_pkts, fz_sx_frames_in, fz_sx_bytes_in;
#ifdef PLAT_HOST
static uint8_t fz_host_sync;             /* (fzsync: below) */
static uint32_t fz_host_stale_max;        /* (app_expect fz_lcd_stale) */
static uint32_t fz_host_lend;             /* (fzlend N: a borrow of N bytes between two strips, once the stash holds rows) */
#endif

static void fz_midi_isr(void)            /* USB and the TRS jack (fm1-base's queue) */
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        const uint32_t cin = pkt & 15u, s = pkt >> 8 & 0xFFu, w = fz_mq_w;
        fz_midi_pkts++;
        if (cin == 15u && s >= 0xF8u) {  /* the clock, START, CONTINUE, STOP: the sequencer's, here */
            if (s == 0xF8u || s == 0xFAu || s == 0xFBu || s == 0xFCu)
                fz_midi_play((int)s, 0, 0);
        } else if (cin >= 8u && cin <= 14u) {
            fz_midi_play((int)s, (int)(pkt >> 16 & 0x7Fu), (int)(pkt >> 24 & 0x7Fu));   /* (heard now) */
            if (w - fz_mq_r < FZ_MQ) {   /* (its UI part on the main loop) */
                fz_mq_pkt[w % FZ_MQ] = pkt;
                fz_mq_ms[w % FZ_MQ] = plat_ms();
                FZ_BARRIER();
                fz_mq_w = w + 1u;
            } else {
                fz_mq_drops++;
            }
        }
    }
}

#define FZ_RAMP_STEP 48u                 /* the start's ramp a half: 4096 in 86 halves, 0.5 s */
#define FZ_OVER_US 4930u                 /* the shed (P2.6, ChoralRoot's): a half's sound over this (85 % of 5,805 us) ..
                                          * .. lets one voice go, at most every FZ_SHED_GAP halves */
#define FZ_SHED_GAP 2u
static uint32_t fz_ramp;                 /* 0..4096: the sound's start, from silence */
static int32_t fz_gain;                  /* the last half's gain (Q12) */
static volatile uint32_t fz_over, fz_over_run, fz_sheds;   /* halves over FZ_OVER_US; halves since the last shed; voices
                                                            * let go */

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    const uint32_t t0 = plat_ticks24();
    uint32_t i, d;
    if (!fz_audio_on || n > APP_BLOCK
#ifdef PLAT_HOST
        || fz_host_sync
#endif
    ) {
        for (i = 0; i < 2u * n; i++)
            lr[i] = 0;
        return;
    }
    fz_midi_isr();
    fz_in_render = 1;
    fz_audio(fz_abuf, n);
    fz_in_render = 0;
    {   /* 16 bits to 24 with ChoralRoot's -6 dBFS ceiling (its OUT_SHIFT 7: Q15 x 128; the codec and the amp ran clean
         * there), times MASTER's gain (Q12: 4096 full) and the start's ramp, the gain glided across the half from the
         * last one's (no step when MASTER moves) */
        const int32_t g1 = (int32_t)((master_q12 > 4096u ? 4096u : master_q12) * fz_ramp >> 12), g0 = fz_gain;
        for (i = 0; i < n; i++) {
            const int32_t g = g0 + (g1 - g0) * (int32_t)i / (int32_t)n;   /* (32 bits: |g1 - g0| <= 4096, i < 256) */
            lr[2u * i] = ((int32_t)fz_abuf[2u * i] * g) >> 5;
            lr[2u * i + 1u] = ((int32_t)fz_abuf[2u * i + 1u] * g) >> 5;
        }
        fz_gain = g1;
        if (fz_ramp < 4096u)             /* (from silence to MASTER's level in ~0.5 s after the sound starts) */
            fz_ramp = fz_ramp + FZ_RAMP_STEP > 4096u ? 4096u : fz_ramp + FZ_RAMP_STEP;
    }
    d = plat_ticks24() - t0;
    fz_prof.isr += d;
    fz_prof.n_isr++;
    if (d > fz_prof.isr_max)
        fz_prof.isr_max = d;
    if (fz_in_frame)
        fz_prof.isr_in_frame += d;
    if (fz_over_run < FZ_SHED_GAP)
        fz_over_run++;
    if (d > FZ_OVER_US * PLAT_TICKS_PER_US) {   /* the shed: the sound took most of its half: one voice let go */
        fz_over++;
        if (fz_over_run >= FZ_SHED_GAP && fz_shed_one()) {
            fz_sheds++;
            fz_over_run = 0;
        }
    }
}

/* ---------------------------------------------------------------- the panel, MIDI, the clock --- */
static uint8_t fz_ok;                    /* FIZZ is up (its data blob there) */
static uint32_t fz_seen_btn, fz_seen_keys;   /* the panel as last sampled */
#define FZ_EVQ 64u
static uint16_t fz_evq[FZ_EVQ];          /* presses waiting for FIZZ: bit 15 down, bit 8 a key, the low byte its index */
static uint32_t fz_evq_ms[FZ_EVQ], fz_ev_w, fz_ev_r;
static uint32_t fz_ticked;               /* FIZZ's clock as given (ms) */
static uint32_t fz_touch_ms;             /* the last press or turn */

static void fz_ev_put(uint32_t e, uint32_t ms)
{
    if (fz_ev_w - fz_ev_r < FZ_EVQ) {
        fz_evq_ms[fz_ev_w % FZ_EVQ] = ms;
        fz_evq[fz_ev_w++ % FZ_EVQ] = (uint16_t)e;
    }
}

static void fz_sample(void)              /* the panel's changes since the last look, queued (FM1VST's order: the buttons
                                          * pressed before the keys, the buttons let go after them) */
{
    const uint32_t b = plat_buttons() & ((1u << NB) - 1u), k = plat_keys(), db = b ^ fz_seen_btn,
                   dk = k ^ fz_seen_keys, ms = plat_ms();
    uint32_t i;
    if (!db && !dk)
        return;
    for (i = 0; i < NB; i++)
        if ((db >> i & 1u) && (b >> i & 1u))
            fz_ev_put(0x8000u | i, ms);
    for (i = 0; i < NKEYS; i++)
        if (dk >> i & 1u)
            fz_ev_put((k >> i & 1u ? 0x8000u : 0u) | 0x100u | i, ms);
    for (i = 0; i < NB; i++)
        if ((db >> i & 1u) && !(b >> i & 1u))
            fz_ev_put(i, ms);
    fz_seen_btn = b;
    fz_seen_keys = k;
}

static void fz_tick_to(uint32_t ms)      /* FIZZ's clock up to ms, a few ms a lock */
{
    while ((int32_t)(ms - fz_ticked) > 0) {
        const uint32_t n = ms - fz_ticked > FZ_TICK_CHUNK ? FZ_TICK_CHUNK : ms - fz_ticked, f = fz_lock();
        fz_ticked += n;
        fz_tick(fz_ticked);
        fz_unlock(f, FZ_LOCK_TICK);
    }
}

static uint8_t fz_sx_buf[1024];          /* a reply (fizz_sx.h QD_SX_FRAME_MAX) */
#ifdef PLAT_HOST
static uint32_t fz_host_sx_out;          /* replies made (the simulator has no USB) */
#endif
static void fz_sysex_out(void)           /* FIZZ's replies, taken under the lock, sent outside it (a send may wait) */
{
    for (;;) {
        const uint32_t f = fz_lock();
        int k = fz_sysex_peek();
        k = k > 0 && k <= (int)sizeof fz_sx_buf ? fz_sysex_take(fz_sx_buf, sizeof fz_sx_buf) : 0;
        fz_unlock(f, FZ_LOCK_TAKE);
        if (k <= 0)
            break;
#ifdef PLAT_HOST
        fz_host_sx_out++;
#endif
        plat_sysex_send(fz_sx_buf, (uint32_t)k);
    }
}

static void fz_input(void)               /* everything that changes FIZZ on the main loop, in time order */
{
    const uint32_t t0 = plat_ticks24();
    int r;
    fz_sample();
    for (;;) {
        const int ev = fz_ev_r != fz_ev_w, mi = fz_mq_r != fz_mq_w;
        uint32_t f;
        if (!ev && !mi)
            break;
        FZ_BARRIER();
        if (ev && (!mi || (int32_t)(fz_evq_ms[fz_ev_r % FZ_EVQ] - fz_mq_ms[fz_mq_r % FZ_MQ]) <= 0)) {
            const uint32_t e = fz_evq[fz_ev_r % FZ_EVQ], i = e & 0xFFu, down = e >> 15;
            fz_tick_to(fz_evq_ms[fz_ev_r % FZ_EVQ]);
            f = fz_lock();
            if (e & 0x100u)
                fz_key((int)i, (int)down, down ? FZ_VEL : 0);
            else
                fz_button((int)i, (int)down);
            fz_unlock(f, FZ_LOCK_EVENT);
            fz_ev_r++;
            fz_touch_ms = plat_ms();
        } else {
            const uint32_t p = fz_mq_pkt[fz_mq_r % FZ_MQ];
            fz_tick_to(fz_mq_ms[fz_mq_r % FZ_MQ]);
            f = fz_lock();
            fz_midi_ui((int)(p >> 8 & 0xFFu), (int)(p >> 16 & 0x7Fu), (int)(p >> 24 & 0x7Fu));
            fz_unlock(f, FZ_LOCK_MIDI);
            FZ_BARRIER();
            fz_mq_r++;
        }
    }
    fz_tick_to(plat_ms());
    for (r = 0; r < NE; r++) {
        const int32_t d = plat_enc(r);
        if (d) {
            const uint32_t f = fz_lock();
            fz_knob(r, (int)d);
            fz_unlock(f, FZ_LOCK_KNOB);
            fz_touch_ms = plat_ms();
        }
    }
    fz_sysex_out();
    fz_prof.input += plat_ticks24() - t0;
}

/* ---------------------------------------------------------------- SysEx --- */
#ifndef PLAT_HOST
static uint32_t t5_gap_max, t5_late, t5_run_max;   /* (main_fm1.c's, defined after this file in the unity build) */
extern uint32_t lock_max, lock_from;     /* (plat_fm1.c's: the longest any code kept the interrupts off, and where) */
#endif
static uint8_t fz_safe;                  /* safe mode: the store not attached (REC held at power-on) */
static uint8_t fz_store_started;         /* the store looked at (fz_store_start; app_init clears it: a reboot) */
#ifdef PLAT_HOST
static uint8_t fz_host_nowipe;           /* (the simulator's fznowipe: no erase of a foreign region) */
#endif
static int fz_wiped = -1;                /* sectors the store's region had erased at the boot or by the wipe (-1 none) */
static uint64_t fz_us(uint64_t ticks) { return ticks / PLAT_TICKS_PER_US; }

static char *fz_put_u(char *q, const char *label, uint64_t v)
{
    char t[24];
    int n = 0;
    while (*label)
        *q++ = *label++;
    do
        t[n++] = (char)('0' + (int)(v % 10u));
    while ((v /= 10u) && n < 23);
    while (n)
        *q++ = t[--n];
    *q++ = ' ';
    return q;
}

static void fz_stats_send(int part, const char *line)   /* F0 7D 46 50 part ASCII F7 */
{
    uint8_t f[680], *q = f;
    *q++ = 0xF0, *q++ = 0x7D, *q++ = 0x46, *q++ = 0x50, *q++ = (uint8_t)part;
    for (; *line && q < f + sizeof f - 1u; line++)
        *q++ = (uint8_t)(*line & 0x7F);
    *q++ = 0xF7;
#ifdef PLAT_HOST
    printf("  stats %d: %.*s\n", part, (int)(q - f - 6), (const char *)f + 5);   /* (no USB: shown) */
#endif
    plat_sysex_send(f, (uint32_t)(q - f));
}

static void fz_stats_reply(void)         /* 7D 46 50: this port's counters, three lines (fizz_xdata.py stats) */
{
    char line[640], *c = line;
    const uint64_t fr = fz_prof.n_frame ? fz_prof.n_frame : 1u, ni = fz_prof.n_isr ? fz_prof.n_isr : 1u;
    uint64_t parts;
    uint32_t i;
    /* 1: the sound, the locks, the frames */
    c = fz_put_u(c, "up=", fz_ok);
    c = fz_put_u(c, "xdata=", (uint32_t)(fz_xdata_state() + 1));   /* (0 another's, 1 none, 2 this build's) */
    c = fz_put_u(c, "isr_n=", fz_prof.n_isr);
    c = fz_put_u(c, "isr_avg_us=", fz_us(fz_prof.isr / ni));   /* (a half: APP_BLOCK frames, 5805 us) */
    c = fz_put_u(c, "isr_max_us=", fz_us(fz_prof.isr_max));
    c = fz_put_u(c, "cpu_isr=", plat_cpu_pct());
    c = fz_put_u(c, "xruns=", plat_xruns());
    c = fz_put_u(c, "lock_max_us=", fz_us(fz_prof.lock_max));
    c = fz_put_u(c, "lock_kind=", fz_prof.lock_kind);   /* (1 a press, 2 the clock, 3 a knob, 4 MIDI, 5 SysEx in, 6 out) */
    c = fz_put_u(c, "midi_drops=", fz_mq_drops);
    c = fz_put_u(c, "isr_loads=", fz_audio_loads());   /* (patterns loaded inside the interrupt: should stay 0) */
    c = fz_put_u(c, "frames=", fz_prof.n_frame);
    c = fz_put_u(c, "frame_avg_us=", fz_us(fz_prof.frame / fr));
    c = fz_put_u(c, "frame_max_us=", fz_us(fz_prof.frame_max));
    c = fz_put_u(c, "checks=", fz_prof.n_check);
    c = fz_put_u(c, "skipped=", fz_prof.n_skip);
    c = fz_put_u(c, "check_avg_us=", fz_us(fz_prof.check / (fz_prof.n_check ? fz_prof.n_check : 1u)));
    c = fz_put_u(c, "store_steps=", fz_prof.n_store);
    c = fz_put_u(c, "store_avg_us=", fz_us(fz_prof.store / (fz_prof.n_store ? fz_prof.n_store : 1u)));
    c = fz_put_u(c, "store_max_us=", fz_us(fz_prof.store_max));
    *c = 0;
    fz_stats_send(1, line);
    /* 2: a frame's time by phase (us a frame, averages), and the calls a frame */
    parts = fz_prof.raster + fz_prof.blend + fz_prof.glyph + fz_prof.text + fz_prof.shapes + fz_prof.convert +
            fz_prof.lcd + fz_prof.input + fz_prof.isr_in_frame;
    c = line;
    c = fz_put_u(c, "text_us=", fz_us(fz_prof.text / fr));       /* (the fast drawing's) */
    c = fz_put_u(c, "shapes_us=", fz_us(fz_prof.shapes / fr));
    c = fz_put_u(c, "raster_us=", fz_us(fz_prof.raster / fr));   /* (plutovg's) */
    c = fz_put_u(c, "blend_us=", fz_us(fz_prof.blend / fr));
    c = fz_put_u(c, "glyph_us=", fz_us(fz_prof.glyph / fr));
    c = fz_put_u(c, "rgb565_us=", fz_us(fz_prof.convert / fr));
    c = fz_put_u(c, "lcd_us=", fz_us(fz_prof.lcd / fr));
    c = fz_put_u(c, "input_us=", fz_us(fz_prof.input / fr));
    c = fz_put_u(c, "isr_in_frame_us=", fz_us(fz_prof.isr_in_frame / fr));
    c = fz_put_u(c, "rest_us=", fz_us((fz_prof.frame > parts ? fz_prof.frame - parts : 0u) / fr));   /* (the core's
                                                   * own: its screens' logic, the paths built, plutovg's state) */
    c = fz_put_u(c, "texts=", fz_prof.n_text / fr);
    c = fz_put_u(c, "shapes=", fz_prof.n_shapes / fr);
    c = fz_put_u(c, "rasters=", fz_prof.n_raster / fr);
    c = fz_put_u(c, "blends=", fz_prof.n_blend / fr);
    c = fz_put_u(c, "glyphs=", fz_prof.n_glyph / fr);
    *c = 0;
    fz_stats_send(2, line);
    /* 3: by screen (FIZZ's mode.view): frames, average, longest (us) */
    c = line;
    for (i = 0; i < FZ_NSCREEN; i++)
        if (fz_screens[i].n && c < line + sizeof line - 48u) {
            const char sc[] = {'s', (char)('0' + i / 4u), '.', (char)('0' + i % 4u), '=', 0};
            c = fz_put_u(c, sc, fz_screens[i].n);
            c[-1] = '/';
            c = fz_put_u(c, "", fz_us(fz_screens[i].sum / fz_screens[i].n));
            c[-1] = '/';
            c = fz_put_u(c, "", fz_us(fz_screens[i].max));
        }
    *c = 0;
    fz_stats_send(3, line);
    /* 4: inside the audio interrupt by part (us a half, averages: the sequencer, the voices, the effects, the rest);
     * FIZZ's state (the voices, the misses, the store) */
    {
        const uint64_t rest = fz_prof.isr > fz_prof.a_seq + fz_prof.a_voices + fz_prof.a_fx
                                  ? fz_prof.isr - fz_prof.a_seq - fz_prof.a_voices - fz_prof.a_fx : 0u;
        c = line;
        c = fz_put_u(c, "isr_seq_us=", fz_us(fz_prof.a_seq / ni));
        c = fz_put_u(c, "isr_voices_us=", fz_us(fz_prof.a_voices / ni));
        c = fz_put_u(c, "isr_fx_us=", fz_us(fz_prof.a_fx / ni));
        c = fz_put_u(c, "isr_rest_us=", fz_us(rest / ni));
        fz_state_line(c);
        fz_stats_send(4, line);
    }
    /* 5: the platform: the guard, MIDI and SysEx in, the TIMER5 scan's delays, fm1-base's own longest lock */
    c = line;
    c = fz_put_u(c, "over_halves=", fz_over);
    c = fz_put_u(c, "sheds=", fz_sheds);
    c = fz_put_u(c, "ramp=", fz_ramp);
    c = fz_put_u(c, "gain_q12=", (uint32_t)fz_gain);
    c = fz_put_u(c, "master_q12=", APP_GAIN(plat_master()));
    c = fz_put_u(c, "midi_in_pkts=", fz_midi_pkts);
    c = fz_put_u(c, "sysex_in=", fz_sx_frames_in);
    c = fz_put_u(c, "sysex_in_bytes=", fz_sx_bytes_in);
    c = fz_put_u(c, "usb_midi_in=", plat_midi_count(PORT_USB, 0));
    c = fz_put_u(c, "trs_midi_in=", plat_midi_count(PORT_TRS, 0));
    c = fz_put_u(c, "store_safe_mode=", fz_safe);
    c = fz_put_u(c, "store_wiped=", (uint32_t)fz_wiped);
#ifndef PLAT_HOST
    c = fz_put_u(c, "t5_run_max_us=", t5_run_max);
    c = fz_put_u(c, "t5_gap_max_us=", t5_gap_max);
    c = fz_put_u(c, "t5_late=", t5_late);
    c = fz_put_u(c, "plat_lock_max_us=", lock_max);
    c = fz_put_u(c, "plat_lock_from=", lock_from);
#endif
    *c = 0;
    fz_stats_send(5, line);
    /* 6: core 1 (fz_dual.c): its state and back end, why it is off, the jobs handed over and run by core 0 instead,
     * the failures, the longest wait */
    c = line;
    {
        const char *const w[] = {"c1=", fz_dual_state_name(), " c1_backend=", FZ_C1_BACKEND->name, " "};
        unsigned i;
        for (i = 0; i < sizeof w / sizeof w[0]; i++)
            for (const char *t = w[i]; *t;)
                *c++ = *t++;
    }
    c = fz_put_u(c, "c1_barred=", fz_dual.barred);
    c = fz_put_u(c, "c1_start_us=", fz_us(fz_dual.start_ticks));
    c = fz_put_u(c, "c1_posts=", fz_dual.posts);
    c = fz_put_u(c, "c1_inline=", fz_dual.inline_jobs);
    c = fz_put_u(c, "c1_stalls=", fz_dual.stalls);
    c = fz_put_u(c, "c1_refusals=", fz_dual.refusals);
    c = fz_put_u(c, "c1_misreads=", fz_dual.misreads);
    c = fz_put_u(c, "c1_wait_max_us=", fz_us(fz_dual.wait_max_ticks));
    *c = 0;
    fz_stats_send(6, line);
    /* 7 (P2.7): the screen's bursts and the LEDs */
    c = line;
    c = fz_put_u(c, "frames_sent=", fz_frames_sent);
    c = fz_put_u(c, "bursts=", fz_bursts);
    c = fz_put_u(c, "burst_rows=", fz_burst_rows);
    c = fz_put_u(c, "burst_max_us=", fz_us(fz_burst_max));
    c = fz_put_u(c, "bursts_a_frame_max=", fz_frame_bursts_max);
    c = fz_put_u(c, "stash_full=", fz_st_full);
    c = fz_put_u(c, "stash_taken=", fz_st_drops);
    c = fz_put_u(c, "no_stash=", fz_st_none);
    c = fz_put_u(c, "stash_rows_min=", fz_st_cap_min);
    c = fz_put_u(c, "rows_sent=", fz_rows_sent);
    c = fz_put_u(c, "led_looks=", fz_led_looks);
    c = fz_put_u(c, "led_writes=", fz_led_writes);
    *c = 0;
    fz_stats_send(7, line);
}

static void fz_wipe(void)                /* 7D 46 50 "WIPE": the store's region erased, FIZZ started again on it (as at a
                                          * power-on with a blank region; nothing restarts but FIZZ) */
{
#ifndef PLAT_HOST
    uint32_t *q;
#endif
    char line[48], *c = line;
    app_panic();
    fz_message("FIZZ: WIPING ITS STORE", "DO NOT SWITCH OFF");
    fz_wiped = fz_store_wipe();
    if (fz_ok) {
#ifndef PLAT_HOST
        for (q = _fz_rt_start; q < _fz_rt_end; q++)   /* (FIZZ's data outside .bss and the pool: as app_init) */
            *q = 0;
        for (q = _fz_ni_start; q < _fz_ni_end; q++)
            *q = 0;
#endif
        fz_ok = (uint8_t)fz_init(APP_NAME " " APP_VERSION);
        fz_ticked = 0;
        if (fz_ok && !fz_safe)
            fz_store_attach();           /* (no lock: app_panic turned the sound off) */
    }
    c = fz_put_u(c, "wiped=", (uint32_t)fz_wiped);
    *c = 0;
    fz_stats_send(9, line);
    app_ui_init();
}

int app_sysex(const uint8_t *p, uint32_t n)
{
    fz_sx_frames_in++;
    fz_sx_bytes_in += n;
    if (n >= 3u && p[0] == 0x7Du && p[1] == 0x46u && p[2] == 0x50u) {   /* "FP": the port's counters */
        if (n >= 7u && p[3] == 'W' && p[4] == 'I' && p[5] == 'P' && p[6] == 'E') {   /* the store wiped, FIZZ again */
            fz_wipe();
            return 1;
        }
        if (n >= 4u && p[3] == 0x7Fu) {  /* (7F: and start them again) */
            fz_prof_reset();
            fz_mq_drops = 0;
            fz_bursts = fz_burst_rows = fz_burst_max = fz_frame_bursts_max = fz_st_full = fz_st_drops = fz_st_none = 0;
            fz_frames_sent = fz_rows_sent = fz_led_looks = fz_led_writes = 0;
            fz_st_cap_min = 240u;
#ifndef PLAT_HOST
            t5_gap_max = t5_late = t5_run_max = 0;
#endif
        } else {
            fz_stats_reply();
        }
        return 1;
    }
    if (n < 3u || p[0] != 0x7Du || p[1] != 0x46u || p[2] != 0x5Au)
        return 0;                        /* (not FIZZ's: the platform drops it) */
    if (fz_ok) {
        const uint32_t f = fz_lock();
        fz_sysex(p, n);
        fz_unlock(f, FZ_LOCK_SYSEX);
        fz_sysex_out();
    }
    return 1;
}

/* ---------------------------------------------------------------- the LEDs --- */
/* (P2.7) FIZZ's 0..255 a light onto fm1-base's levels: >= 128 lit; 64..127 (MID) the mid level, lit 1 scan frame in 8
 * (~114 Hz; the columns take turns: build.py's OVERLAY), with the glow under it, so it never falls to dark between;
 * 1..63 (FAINT, DIM) the glow. The glow at its higher strength (fm1_led_dim_level(0): the mid level 1 in 8, not 1 in 12
 * at ~76 Hz). The picture is computed off-line and written to the scan's arrays in one go (the interrupts off for those
 * 33 bytes): the scan never sees half of it, and never a dark gap. Looked at every 10 ms, also between the strips of a
 * long frame */
#define FZ_LED_LIT 128u
#define FZ_LED_MIDL 64u
static uint32_t fz_led_t;                /* (plat_ms of the last look) */
static void fz_lights(void)              /* (FIZZ's state read, the interrupts on: a light a moment early or late) */
{
    uint8_t k[NKEYS], b[NB], g;
    uint32_t i, lit_b = 0, lit_k = 0, mid_b = 0, mid_k = 0, dim_b = 0, dim_k = 0;
    fz_led_t = plat_ms();
    fz_led_looks++;
    fz_leds(k, b, &g);
    for (i = 0; i < NB; i++) {
        lit_b |= (uint32_t)(b[i] >= FZ_LED_LIT) << i;
        mid_b |= (uint32_t)(b[i] >= FZ_LED_MIDL && b[i] < FZ_LED_LIT) << i;
        dim_b |= (uint32_t)(b[i] && b[i] < FZ_LED_LIT) << i;          /* (the glow under the mid level too) */
    }
    for (i = 0; i < NKEYS; i++) {
        lit_k |= (uint32_t)(k[i] >= FZ_LED_LIT) << i;
        mid_k |= (uint32_t)(k[i] >= FZ_LED_MIDL && k[i] < FZ_LED_LIT) << i;
        dim_k |= (uint32_t)(k[i] && k[i] < FZ_LED_LIT) << i;
    }
    if (g >= FZ_LED_LIT)                 /* PLAY's green LED (bit NB) */
        lit_b |= LED_PLAY_GREEN;
    else if (g) {
        dim_b |= LED_PLAY_GREEN;
        if (g >= FZ_LED_MIDL)
            mid_b |= LED_PLAY_GREEN;
    }
#ifdef PLAT_HOST
    plat_lights(lit_b, lit_k, dim_b, dim_k, 2);
    plat_lights_mid(mid_b, mid_k);
#else
    {
        static uint32_t was[6] = {~0u};
        static uint8_t level_set;
        uint8_t nl[FM1_NCOL] = {0}, dl[FM1_NCOL] = {0}, ml[FM1_NCOL] = {0};
        uint32_t c;
        if (was[0] == lit_b && was[1] == lit_k && was[2] == mid_b && was[3] == mid_k && was[4] == dim_b &&
            was[5] == dim_k)
            return;                      /* (the same picture: nothing written) */
        was[0] = lit_b, was[1] = lit_k, was[2] = mid_b, was[3] = mid_k, was[4] = dim_b, was[5] = dim_k;
        if (!level_set) {
            fm1_led_dim_level(0);
            level_set = 1;
        }
        for (i = 0; i < NB + NKEYS + 1u; i++) {   /* (plat_lights' map: panel.btn, led_pos; the last one PLAY's green) */
            const uint32_t on = i < NB ? lit_b >> i & 1u : i < NB + NKEYS ? lit_k >> (i - NB) & 1u : lit_b >> NB & 1u;
            const uint32_t md = i < NB ? mid_b >> i & 1u : i < NB + NKEYS ? mid_k >> (i - NB) & 1u : mid_b >> NB & 1u;
            const uint32_t dm = i < NB ? dim_b >> i & 1u : i < NB + NKEYS ? dim_k >> (i - NB) & 1u : dim_b >> NB & 1u;
            uint32_t col, bit;
            if (i < NB + NKEYS) {
                const uint8_t q = led_pos[i < NB ? panel.btn[i] : 14u + (i - NB)];
                if (q == 0xFF)
                    continue;
                col = q >> 3, bit = 1u << (q & 7u);
            } else {
                col = 8, bit = 1u << 1;  /* (PLAY's green: column 8, row PA5, as plat_lights) */
            }
            if (on)
                nl[col] |= (uint8_t)bit;
            else {
                if (dm)
                    dl[col] |= (uint8_t)bit;
                if (md)
                    ml[col] |= (uint8_t)bit;
            }
        }
        fm1_irq_off();                   /* (33 bytes: the scan sees the old picture or the new, never a mix) */
        for (c = 0; c < FM1_NCOL; c++) {
            fm1_led_dim[c] = dl[c];
            fm1_led_mid[c] = ml[c];
            fm1_led[c] = nl[c];
        }
        fm1_irq_on();
        fz_led_writes++;
    }
#endif
}

/* (P2.7) the flash's work (fz_port.c, before each erase and each page written): the interrupts off and the LED lines
 * dark until fm1-base's flash call turns them on again at its end, so a page's ~1 ms (an erase's ~18 ms) shows as a
 * short dark dip, not one column lit bright for all of it while the scan waits */
void fz_flash_quiet(void)
{
#ifndef PLAT_HOST
    fm1_irq_off();
    fm1__led_lines(0);
#endif
}
void fz_flash_done(void)
{
#ifndef PLAT_HOST
    fm1_irq_on();
#endif
}

/* ---------------------------------------------------------------- the screen --- */
static uint32_t fz_row_hash[240];        /* each row's pixels as last sent (0: unknown, sent again) */
static uint8_t fz_redraw = 1;            /* the next frame drawn whatever qd_app_frame_changed says (the screen was
                                          * cleared: the boot, an update's screens) */

static uint32_t fz_hash_row(const uint16_t *p)
{
    const uint32_t *w = (const uint32_t *)(const void *)p;
    uint32_t h = 2166136261u, i;
    for (i = 0; i < 120u; i++)
        h = (h ^ w[i]) * 16777619u;
    return h | 1u;
}

/* (P2.7) the burst: a frame's changed rows are copied into a stash borrowed from FIZZ's drawing arena for the frame's
 * length, and sent together when the frame is drawn: the screen changes inside one or two of the panel's refreshes
 * (~4-5 ms for 50 rows), not strip by strip over the frame's 30-60 ms. A run of consecutive rows is one window and
 * one transfer (at most 40 rows a transfer). A frame that changes more rows than the stash holds sends it when full
 * and goes on (an overflow: two or three bursts on a screen change). The stash takes the whole free arena (56 rows):
 * an edit applied between two strips that borrows from it (clip.c's undo levels 12,400 B, a pattern 22,768 B, a copy
 * 6,144 B) takes the stash back (fz_stash_release: the arena's release hook), so the edit always gets its room; the
 * rows the stash held are marked unknown and the next frame is drawn whatever, so they go out then. Leaving room for
 * the undo levels instead (FZ_STASH_KEEP 12416) halves the stash to 30 rows and doubles the bursts (the simulator's
 * boot test: 2.9 bursts a frame against 1.6). No stash at all (the arena busy): the rows go out strip by strip */
#define FZ_STASH_KEEP 0u
#define FZ_RUN_MAX 40u                   /* rows a transfer (a 40-row strip's: the size known to work) */
static uint16_t *fz_st;                  /* the stash, during a frame; 0 none */
static uint32_t fz_st_cap, fz_st_n;      /* rows it holds, rows in it */
static uint8_t fz_st_y[240];             /* their screen rows, in order */

static void fz_stash_release(void)       /* (the arena's release hook: a borrow inside a frame found no room) */
{
    uint32_t i;
    if (!fz_st || fz_in_render)
        return;
    for (i = 0; i < fz_st_n; i++)
        fz_row_hash[fz_st_y[i]] = 0;     /* (not sent: unknown, so sent with the next frame, drawn whatever) */
    fz_redraw = 1;
    fz_stash_return(fz_st);
    fz_st = 0, fz_st_n = 0;
    fz_st_drops++;
}

static void fz_send_stash(void)               /* the stash's rows to the LCD, in runs; back when they are out */
{
    const uint32_t t0 = plat_ticks24();
    uint32_t i = 0, d;
    if (!fz_st_n)
        return;
    while (i < fz_st_n) {
        uint32_t k = 1;
        while (i + k < fz_st_n && k < FZ_RUN_MAX && fz_st_y[i + k] == fz_st_y[i] + k)
            k++;
        lcd_blit(0, fz_st_y[i], 240, k, fz_st + 240u * i);
        i += k;
    }
    lcd_sync();
    fz_rows_sent += fz_st_n;
    fz_burst_rows += fz_st_n;
    fz_bursts++;
    fz_frame_bursts++;
    fz_st_n = 0;
    d = plat_ticks24() - t0;
    if (d > fz_burst_max)
        fz_burst_max = d;
    fz_prof.lcd += d;
}

static void fz_strip_out(void *ctx, int y0, int rows, const uint16_t *px)   /* one of FIZZ's strips, drawn */
{
    int r, first = -1, last = -1;
    uint32_t t = plat_ticks24();
    (void)ctx;
    for (r = 0; r < rows; r++) {
        const uint32_t h = fz_hash_row(px + 240 * r);
        if (h == fz_row_hash[y0 + r])
            continue;
        fz_row_hash[y0 + r] = h;
        if (fz_st) {                     /* into the stash (full: what it holds sent first) */
            if (fz_st_n == fz_st_cap) {
                fz_st_full++;
                fz_send_stash();
            }
            memcpy(fz_st + 240u * fz_st_n, px + 240 * r, 480u);
            fz_st_y[fz_st_n++] = (uint8_t)(y0 + r);
        } else {
            if (first < 0)
                first = r;
            last = r;
        }
    }
    if (first >= 0) {                    /* (no stash: the changed rows from where FIZZ drew them, as before P2.7) */
        lcd_blit(0, (uint32_t)(y0 + first), 240, (uint32_t)(last - first + 1), px + 240 * first);
        fz_rows_sent += (uint32_t)(last - first + 1);
    }
    fz_prof.lcd += plat_ticks24() - t;
#ifndef PLAT_HOST
    {   /* fm1-base's remote (fm1_remote.py: 7D 4C 43) taken here too: its SysEx buffer holds one frame, and a tap's
         * release arriving while a slow frame keeps the press waiting there was dropped (the button then stuck) */
        const uint8_t *p;
        uint32_t k;
        if (ota_frame_get(&p, &k) && remote_frame(p, k))
            ota_frame_done();
    }
#endif
#ifdef PLAT_HOST
    if (!fz_host_sync)                   /* (render_frames gives nothing between strips) */
#endif
    {
#ifdef PLAT_HOST
        if (fz_host_lend && fz_st_n) {   /* (the stash given back: an edit's borrow, rows in it) */
            fz_test_lend(fz_host_lend);
            fz_host_lend = 0;
        }
#endif
        fz_input();                      /* the panel, MIDI, the clock (a borrow here may take the stash back) */
        if (plat_ms() - fz_led_t >= FZ_LED_MS)
            fz_lights();                 /* (a long frame: the LEDs keep their pace) */
    }
    t = plat_ticks24();
    lcd_sync();                          /* (FIZZ draws its next strip where this one is) */
    fz_prof.lcd += plat_ticks24() - t;
}

static void fz_draw(void)
{
    const uint32_t t0 = plat_ticks24();
    uint32_t d;
    fz_st = (uint16_t *)fz_stash_borrow(240u * 480u, FZ_STASH_KEEP, &fz_st_cap);
    fz_st_cap /= 480u;
    if (fz_st && !fz_st_cap) {
        fz_stash_return(fz_st);
        fz_st = 0;
    }
    if (!fz_st)
        fz_st_none++;
    else if (fz_st_cap < fz_st_cap_min)
        fz_st_cap_min = fz_st_cap;
    fz_st_n = 0;
    fz_frame_bursts = 0;
    fz_in_frame = 1;
    fz_frame(fz_strip_out, 0);
    fz_in_frame = 0;
    if (fz_st) {
        fz_send_stash();                      /* the burst */
        fz_stash_return(fz_st);
        fz_st = 0;
    }
#ifdef PLAT_HOST
    {   /* (the host's check, each frame: no row on the panel differs from what its hash says was sent) */
        uint32_t y, n = 0;
        for (y = 0; y < 240u; y++)
            n += fz_row_hash[y] && fz_hash_row(fb + 240u * y) != fz_row_hash[y];
        if (n > fz_host_stale_max)
            fz_host_stale_max = n;
    }
#endif
    if (fz_frame_bursts > fz_frame_bursts_max)
        fz_frame_bursts_max = fz_frame_bursts;
    fz_frames_sent++;
    d = plat_ticks24() - t0;
    fz_prof.frame += d;
    fz_prof.n_frame++;
    if (d > fz_prof.frame_max)
        fz_prof.frame_max = d;
}

static void fz_hold_ms(uint32_t ms)      /* a message left on the screen a moment (the main loop's own wait) */
{
#ifdef PLAT_HOST
    (void)ms;                            /* (the simulator's clock moves only between its script's steps) */
#else
    const uint32_t t0 = plat_ms();
    while (plat_ms() - t0 < ms)
        plat_yield();
#endif
}

static void fz_message(const char *a, const char *b)   /* fm1-base's face on black: FIZZ cannot draw */
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    cv_begin(240, FONT_S.h, C_BLACK);
    cv_text((240 - text_w(&FONT_S, a)) / 2, 0, &FONT_S, a, C_WHITE);
    cv_blit(0, 100);
    cv_begin(240, FONT_S.h, C_BLACK);
    cv_text((240 - text_w(&FONT_S, b)) / 2, 0, &FONT_S, b, C_HI);
    cv_blit(0, 124);
    lcd_sync();
}

/* ---------------------------------------------------------------- the contract --- */
#ifndef PLAT_HOST
extern uint32_t _fz_rt_start[], _fz_rt_end[], _fz_ni_start[], _fz_ni_end[];
#endif
void app_init(void)
{
#ifndef PLAT_HOST
    uint32_t *p;
    for (p = _fz_rt_start; p < _fz_rt_end; p++)   /* FIZZ's data in the RAM code region and in NOINIT (fizz.ld): */
        *p = 0;                                   /* crt0 clears only .bss and the pool */
    for (p = _fz_ni_start; p < _fz_ni_end; p++)
        *p = 0;
#endif
    fz_ok = (uint8_t)fz_init(APP_NAME " " APP_VERSION);
    fz_ticked = 0;
    fz_store_started = 0, fz_safe = 0, fz_wiped = -1;
    fz_stash_on_release(fz_stash_release);   /* (P2.7: a borrow inside a frame takes the screen's stash back) */
    /* core 1, before the sound: not after a failed boot (fm1-base's boot guard counted the one before: a crash or the
     * watchdog within its 30 s), and the record read with this reset's cause (the watchdog's: a hang beside core 1) */
#ifdef PLAT_HOST
    fz_dual_boot(&fz_c1_bar, FZ_C1_BACKEND, 0, 0, 0, 1);
#else
    fz_dual_boot(&fz_c1_bar, FZ_C1_BACKEND, 0, bootguard.failed != 0, (fm1_boot.p3_rst & 4u) != 0, 1);
#endif
}
static void fz_store_start(void)         /* the store, once, at the first screen (the panel is read by then) */
{
    if (fz_store_started || !fz_ok)
        return;
    fz_store_started = 1;
    if (plat_buttons() >> B_REC & 1u) {  /* REC held at power-on: safe mode, no store (nothing read or written) */
        fz_safe = 1;
        fz_message("FIZZ: SAFE MODE", "THE STORE IS NOT USED");
        fz_hold_ms(1500u);
        return;
    }
#ifdef PLAT_HOST
    if (!fz_host_nowipe && fz_store_foreign() > 0) {   /* (fznowipe: the core's store meets it as it is) */
#else
    if (fz_store_foreign() > 0) {        /* another firmware's data in the region (none of FIZZ's): erased first */
#endif
        fz_message("FIZZ: CLEARING ITS STORE", "ONCE, A FEW SECONDS");
        fz_wiped = fz_store_wipe();
    }
    fz_store_attach();                   /* (no lock: the sound is not on yet, the interrupt leaves FIZZ alone; the
                                          * mount reads the region, ~0.1 s, the panel's scan going on) */
}

void app_ui_init(void)
{
    uint32_t i;
    fz_store_start();
    if (!fz_ok) {
        fz_message(fz_xdata_state() < 0 ? "FIZZ: ITS DATA IS ANOTHER BUILD'S" : "FIZZ NEEDS ITS DATA",
                   "RUN FIZZ_XDATA.PY OVER USB");
        return;
    }
    lcd_fill(0, 0, 240, 240, C_BLACK);
    for (i = 0; i < 240u; i++)
        fz_row_hash[i] = 0;              /* (the whole screen sent again) */
    fz_redraw = 1;
    if (!fz_audio_on)
        fz_ramp = 0, fz_gain = 0;        /* (the sound starts from silence) */
    fz_audio_on = 1;
}

void app_panic(void) { fz_audio_on = 0; }

void app_say(const char *a, const char *b) { (void)a, (void)b; }

#ifdef PLAT_HOST
/* fzsync (the simulator's comparisons with FIZZ's render_frames): the interrupt silent, the sound made by the main
 * loop as render_frames makes it, 128-frame blocks as they fall due by the clock, after the frame */
static uint64_t fz_host_sync_done;
static void fz_host_sync_audio(void)
{
    static int16_t blk[256];
    while (fz_host_sync_done + 128u <= (uint64_t)plat_ms() * 44100u / 1000u) {
        fz_audio(blk, 128);
        fz_host_sync_done += 128u;
    }
}
#endif

static void fz_step(int frame)
{
    static uint32_t frame_t, store_t;
    const uint32_t now = plat_ms();
    uint32_t period;
    int mode, playing;
    (void)frame;                         /* (fm1-base's ~60 a second: the frame rate is decided here) */
    if (!fz_ok)
        return;
    fz_input();
#ifdef PLAT_HOST
    if (fz_host_sync) {                  /* (render_frames' order and cadence: a frame every 15 ms, then the sound) */
        if (now - frame_t >= 15u) {
            frame_t = now;
            fz_draw();
        }
        fz_host_sync_audio();
        fz_lights();
        return;
    }
#endif
    if (now - fz_led_t >= FZ_LED_MS)
        fz_lights();
    fz_ui_state(&mode, &playing);
    period = now - fz_touch_ms < FZ_TOUCHED_MS ? FZ_FRAME_TOUCHED
             : mode == 0                       ? FZ_FRAME_IDLE
             : playing                         ? FZ_FRAME_PLAYING
                                               : FZ_FRAME_QUIET;
    if (now - frame_t >= period || fz_redraw) {
        const uint64_t tx = fz_prof.text, sh = fz_prof.shapes;   /* (the check's drawing calls are not the frame's) */
        const uint32_t ntx = fz_prof.n_text, nsh = fz_prof.n_shapes;
        uint32_t t = plat_ticks24();
        const int changed = fz_frame_changed() || fz_redraw;
        fz_prof.check += plat_ticks24() - t;
        fz_prof.text = tx, fz_prof.shapes = sh, fz_prof.n_text = ntx, fz_prof.n_shapes = nsh;
        fz_prof.n_check++;
        frame_t = now;
        if (changed) {
            fz_redraw = 0;
            fz_draw();
        } else {
            fz_prof.n_skip++;
        }
    }
    if (now - store_t >= FZ_STORE_MS) {  /* the store's step: between frames, outside the locks */
        uint32_t t = plat_ticks24(), d;
        store_t = now;
        fz_persist_step();
        d = plat_ticks24() - t;
        fz_prof.store += d;
        fz_prof.n_store++;
        if (d > fz_prof.store_max)
            fz_prof.store_max = d;
    }
}

void app_frame(void) { fz_step(1); }
void app_input_only(void) { fz_step(0); }

/* ---------------------------------------------------------------- the data blob over USB --- */
/* APP_USB_CMD: firmware/tools/fizz_xdata.py sends 46 "FZXD" len crc off (u32 each, little-endian: this build's blob,
 * fz_port.c's stamp); the unit checks they are its own, erases the sectors, reads the blob from the host in 512-byte
 * parts (the update's read requests: plat_host_read, addresses from 0), writes it and the trailer, checks it and
 * answers 46 "FZOK" or "FZER" + a code. FIZZ then starts (it could not before). Already there: FZOK at once. */
static uint32_t fz_rd32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static void fz_usb_reply(const char *tag, uint32_t code)   /* F0 pack7(00 59 46 len 0 0 body ~sum) F7, as ota.c's */
{
    uint8_t m[16], w[24];
    uint32_t i, s = 0, n = 0, acc = 0, nb = 0, k;
    m[n++] = 0, m[n++] = 0x59, m[n++] = 0x46, m[n++] = 8, m[n++] = 0, m[n++] = 0;
    for (i = 0; i < 4u; i++)
        m[n++] = (uint8_t)tag[i];
    for (i = 0; i < 4u; i++)
        m[n++] = (uint8_t)(code >> (8u * i));
    for (i = 6; i < n; i++)
        s += m[i];
    m[n++] = (uint8_t)~s;
    k = 0;
    w[k++] = 0xF0;
    for (i = 0; i < n; i++) {            /* (7 bits a byte, the low bits first: ota.c ota_pack7) */
        acc |= (uint32_t)m[i] << nb;
        nb += 8u;
        while (nb >= 7u) {
            w[k++] = (uint8_t)(acc & 0x7Fu);
            acc >>= 7;
            nb -= 7u;
        }
    }
    if (nb)
        w[k++] = (uint8_t)(acc & 0x7Fu);
    w[k++] = 0xF7;
    plat_sysex_send(w, k);
}

static uint32_t fz_xdata_write(uint32_t off, uint32_t len, uint32_t crc)   /* 0 done, else FZER's code */
{
    static uint8_t buf[512];
    uint32_t a, tr[3];
    const uint32_t end = off + ((len + 3u) & ~3u) + 12u;
    for (a = off & ~0xFFFu; a < end; a += 0x1000u)
        if (plat_data_erase(a))
            return 3;
    for (a = 0; a < len; a += sizeof buf) {
        const uint32_t k = len - a < sizeof buf ? len - a : (uint32_t)sizeof buf;
        if (plat_host_read(a, buf, k) || plat_data_write(off + a, buf, k))
            return 4;
    }
    tr[0] = 0x54585A46u, tr[1] = len, tr[2] = crc;   /* "FZXT" */
    if (plat_data_write(off + ((len + 3u) & ~3u), tr, sizeof tr))
        return 5;
    return fz_xdata_state() == 1 ? 0u : 6u;
}

void app_usb_cmd(const uint8_t *d, uint32_t n)
{
    uint32_t len, crc, off, rc;
    if (n < 7u + 16u || d[2] != 0x46u || fz_rd32(d + 6) != 0x44585A46u)   /* 46 "FZXD" */
        return;
    len = fz_rd32(d + 10), crc = fz_rd32(d + 14), off = fz_rd32(d + 18);
    if (len != fz_xdata_len() || crc != fz_xdata_crc() || off != fz_xdata_flash_off() || !len) {
        fz_usb_reply("FZER", 1);         /* (another build's blob: this unit's is what the stamp says) */
        return;
    }
    if (fz_xdata_state() == 1) {
        fz_usb_reply("FZOK", 0);
        return;
    }
    if (fz_ok) {                         /* (cannot be: FIZZ runs only on a good blob) */
        fz_usb_reply("FZER", 2);
        return;
    }
    app_panic();
    fz_message("FIZZ: WRITING ITS DATA", "DO NOT UNPLUG");
    rc = fz_xdata_write(off, len, crc);
    fz_usb_reply(rc ? "FZER" : "FZOK", rc ? rc : 1u);
    if (!rc) {
        fz_ok = (uint8_t)fz_init(APP_NAME " " APP_VERSION);
        fz_ticked = 0;
    }
    app_ui_init();                       /* (FIZZ's screen, or the message again) */
}

/* ---------------------------------------------------------------- the host simulator --- */
#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "fz_frames"))
        *got = (int)fz_prof.n_frame;
    else if (!strcmp(what, "fz_isr"))    /* audio interrupts that made FIZZ's sound */
        *got = (int)fz_prof.n_isr;
    else if (!strcmp(what, "fz_lcd_stale"))   /* the most rows a frame's end left on the panel other than what their
                                                * hash says was sent (rows a stash given back dropped, not resent) */
        *got = (int)fz_host_stale_max;
    else if (!strcmp(what, "fz_bursts"))
        *got = (int)fz_bursts;
    else if (!strcmp(what, "fz_bursts_frame_max"))
        *got = (int)fz_frame_bursts_max;
    else if (!strcmp(what, "fz_stash_full"))
        *got = (int)fz_st_full;
    else if (!strcmp(what, "fz_stash_taken"))
        *got = (int)fz_st_drops;
    else if (!strcmp(what, "fz_no_stash"))
        *got = (int)fz_st_none;
    else if (!strcmp(what, "fz_stash_rows_min"))
        *got = (int)fz_st_cap_min;
    else if (!strcmp(what, "fz_rows_sent"))
        *got = (int)fz_rows_sent;
    else if (!strcmp(what, "fz_sx_out"))
        *got = (int)fz_host_sx_out;
    else if (!strcmp(what, "fz_isr_loads"))  /* patterns loaded inside the audio interrupt (the prefetch's misses) */
        *got = (int)fz_audio_loads();
    else if (!strcmp(what, "fz_store_wiped"))   /* sectors erased at the boot or by the wipe (-1: none) */
        *got = fz_wiped;
    else if (!strcmp(what, "fz_safe"))
        *got = fz_safe;
    else if (!strcmp(what, "fz_midi_drops"))
        *got = (int)fz_mq_drops;
    else
        return fz_expect(what, got);
    return 1;
}

/* sysex F0 .. F7 (hex bytes): a SysEx frame in over USB, as the platform gives it (the bytes between F0 and F7).
 * fzdraw: a frame drawn now, whatever the time since the last (a screen at an exact ms, as FIZZ's render_frames
 * draws its "frame at T"). fzsync, fzshow MODE VIEW, fzseed N: as render_frames runs the app (tests/compare.txt) */
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    uint8_t f[512];
    uint32_t n = 0;
    const char *p;
    (void)a, (void)b, (void)c, (void)d;
    if (!strcmp(cmd, "fzsync")) {
        fz_host_sync = 1;
        fz_host_sync_done = (uint64_t)plat_ms() * 44100u / 1000u / 128u * 128u;
        return 1;
    }
    if (!strcmp(cmd, "fzlend")) {        /* fzlend N: in the next frame, between two strips, N bytes borrowed from the
                                          * drawing's arena and given back (as an edit's undo levels: the stash yields) */
        fz_host_lend = (uint32_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "fzshow")) {        /* fzshow MODE VIEW: qd_app_show (ctl.h QD_MODE_*, QD_VIEW_*) */
        const uint32_t f = fz_lock();
        fz_show(atoi(a), atoi(b));
        fz_unlock(f, FZ_LOCK_EVENT);
        return 1;
    }
    if (!strcmp(cmd, "fzdata")) {        /* fzdata FILE | random | zero: the store's region (0x97000..) written from FILE,
                                          * or noise, or zeros, as another firmware may leave it (fm1-base's "data" puts
                                          * a file 16 KiB too low: its offset counts from PLAT_DATA_LO in an area that
                                          * starts at PLAT_XDATA0_LO) */
        static uint8_t buf[4096];
        const int gen = !strcmp(a, "random") ? 1 : !strcmp(a, "zero") ? 2 : 0;
        FILE *df = gen ? 0 : fopen(a, "rb");
        uint32_t off = PLAT_DATA_LO, k, x = 12345u;
        while ((df || gen) && off < PLAT_DATA_HI &&
               (k = gen ? (uint32_t)sizeof buf : (uint32_t)fread(buf, 1, sizeof buf, df)) > 0) {
            for (uint32_t i = 0; gen && i < k; i++)
                buf[i] = gen == 2 ? 0 : (uint8_t)((x = x * 1664525u + 1013904223u) >> 24);
            plat_data_erase(off);
            plat_data_write(off, buf, k);
            off += 0x1000u;
        }
        printf("  fzdata: %s, %u sectors\n", a, (unsigned)((off - PLAT_DATA_LO) / 0x1000u));
        if (df)
            fclose(df);
        return 1;
    }
    if (!strcmp(cmd, "fznowipe")) {      /* fznowipe: a foreign region left as it is at the next boot (the core's own
                                          * handling, measured) */
        fz_host_nowipe = 1;
        return 1;
    }
    if (!strcmp(cmd, "fzstats")) {       /* fzstats: the stats lines (7D 46 50), printed */
        fz_stats_reply();
        return 1;
    }
    if (!strcmp(cmd, "fzseed")) {        /* fzseed N: qd_app_seed (the rolls of each PLAY) */
        fz_seed((uint32_t)strtoul(a, 0, 0));
        return 1;
    }
    if (!strcmp(cmd, "fzdraw")) {
        if (fz_ok)
            fz_draw();
        return 1;
    }
    if (strcmp(cmd, "sysex"))
        return 0;
    for (p = plat_host_line + 5; *p && n < sizeof f;) {
        unsigned v;
        int used;
        if (sscanf(p, " %x%n", &v, &used) != 1)
            break;
        f[n++] = (uint8_t)v;
        p += used;
    }
    if (n >= 2u && f[0] == 0xF0 && f[n - 1u] == 0xF7)
        app_sysex(f + 1, n - 2u);
    return 1;
}
#endif
