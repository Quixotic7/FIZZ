/* SPDX-License-Identifier: MPL-2.0 */
/* FIZZ on fm1-base: fm1-base's side of the app (compiled inside its unity build, after the platform: gfx.c, lcd.c and
 * the plat_* calls are in scope). FIZZ's core is behind fizz_port.h (fz_port.c).
 *
 * TWO CONTEXTS (P2.4). The sound is made in the audio interrupt (app_render, every 256 frames: 5.8 ms), as ChoralRoot
 * makes it: FIZZ's sequencer and voices run from it, and MIDI's real-time messages (the clock, START, CONTINUE, STOP)
 * go to the sequencer there. A slow frame can never starve it. Everything else that changes FIZZ runs on the main loop:
 * the panel, MIDI's other messages (queued by the interrupt with their time), FIZZ's clock (plat_ms), SysEx; each call
 * with the interrupts off around it (plat_lock), the clock a few ms at a time, so the interrupt never meets an edit
 * half made and waits at most one call. Those calls are made between frames and between the strips of a frame (a press
 * acts within a strip's time, and FIZZ's clock is given up to each press's own time first, so a hold is a hold). The
 * drawing, which only reads, runs with the interrupts on: what it shows may move between two strips (the playhead).
 * fm1-base's own MASTER: the render's gain (APP_GAIN of the pot), as the template has it; FIZZ's own volume stays at
 * its default.
 *
 * A frame is FIZZ's 20-row strips (qd_app_frame_strips), each sent to the LCD from where FIZZ drew it (no second
 * screen buffer), only its rows whose pixels changed (a hash a row). Frames: 30 a second for 1.5 s after a touch, 15
 * while playing, 10 on the idle screen, 4 otherwise (the screens' own motion then slower; the core says nothing of
 * what changed). The rest: the LEDs (FIZZ's 0..255 a light onto fm1-base's off / glow / mid / lit), USB SysEx both
 * ways (APP_SYSEX: FIZZ's 7D 46 5A, and 7D 46 50 for this port's counters and profile), the data blob's check and its
 * writer (APP_USB_CMD, firmware/tools/fizz_xdata.py). */
#include "fizz_port.h"
#include "fz_prof.h"

#define FZ_BARRIER() __asm__ volatile("" ::: "memory")
#define FZ_LED_MS 10u                    /* the LEDs looked at every 10 ms */
#define FZ_VEL 100                       /* the keys' velocity (no APP_TOUCH yet: FM1VST's default) */
#define FZ_TICK_CHUNK 4u                 /* FIZZ's clock: at most this many ms a lock */
#define FZ_TOUCHED_MS 1500u              /* a frame every FZ_FRAME_TOUCHED for this long after a press or a turn */
#define FZ_FRAME_TOUCHED 33u
#define FZ_FRAME_PLAYING 66u
#define FZ_FRAME_IDLE 100u               /* the idle screen's animation */
#define FZ_FRAME_QUIET 250u

/* ---------------------------------------------------------------- the locks --- */
static uint32_t fz_lock_t0;
static uint32_t fz_lock(void)
{
    const uint32_t f = plat_lock();
    fz_lock_t0 = plat_ticks24();
    return f;
}
static void fz_unlock(uint32_t f, uint32_t kind)
{
    const uint32_t d = plat_ticks24() - fz_lock_t0;
    if (d > fz_prof.lock_max)
        fz_prof.lock_max = d, fz_prof.lock_kind = kind;
    plat_unlock(f);
}

/* ---------------------------------------------------------------- the audio interrupt --- */
static int16_t fz_abuf[2u * APP_BLOCK];
static volatile uint8_t fz_audio_on;     /* 0: silence (no FIZZ, a panic, an update) */
static volatile uint8_t fz_in_frame;     /* (the profile: the interrupt's time inside frames) */
#define FZ_MQ 128u                       /* MIDI's channel messages for the main loop, with their time */
static uint32_t fz_mq_pkt[FZ_MQ], fz_mq_ms[FZ_MQ];
static volatile uint32_t fz_mq_w, fz_mq_r, fz_mq_drops;
#ifdef PLAT_HOST
static uint8_t fz_host_sync;             /* (fzsync: below) */
#endif

static void fz_midi_isr(void)            /* USB and the TRS jack (fm1-base's queue) */
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        const uint32_t cin = pkt & 15u, s = pkt >> 8 & 0xFFu, w = fz_mq_w;
        if (cin == 15u && s >= 0xF8u) {  /* the clock, START, CONTINUE, STOP: the sequencer's, here */
            if (s == 0xF8u || s == 0xFAu || s == 0xFBu || s == 0xFCu)
                fz_midi((int)s, 0, 0);
        } else if (cin >= 8u && cin <= 14u) {
            if (w - fz_mq_r < FZ_MQ) {
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
    fz_audio(fz_abuf, n);
    for (i = 0; i < 2u * n; i++)         /* 16 bits to 24, times MASTER's gain (Q12: 4096 full) */
        lr[i] = ((int32_t)fz_abuf[i] * (int32_t)master_q12) >> 4;
    d = plat_ticks24() - t0;
    fz_prof.isr += d;
    fz_prof.n_isr++;
    if (d > fz_prof.isr_max)
        fz_prof.isr_max = d;
    if (fz_in_frame)
        fz_prof.isr_in_frame += d;
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
            fz_midi((int)(p >> 8 & 0xFFu), (int)(p >> 16 & 0x7Fu), (int)(p >> 24 & 0x7Fu));
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
    uint8_t f[512], *q = f;
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
    char line[480], *c = line;
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
    c = fz_put_u(c, "frames=", fz_prof.n_frame);
    c = fz_put_u(c, "frame_avg_us=", fz_us(fz_prof.frame / fr));
    c = fz_put_u(c, "frame_max_us=", fz_us(fz_prof.frame_max));
    *c = 0;
    fz_stats_send(1, line);
    /* 2: a frame's time by phase (us a frame, averages), and the calls a frame */
    parts = fz_prof.raster + fz_prof.blend + fz_prof.glyph + fz_prof.convert + fz_prof.lcd + fz_prof.input +
            fz_prof.isr_in_frame;
    c = line;
    c = fz_put_u(c, "raster_us=", fz_us(fz_prof.raster / fr));
    c = fz_put_u(c, "blend_us=", fz_us(fz_prof.blend / fr));
    c = fz_put_u(c, "glyph_us=", fz_us(fz_prof.glyph / fr));
    c = fz_put_u(c, "rgb565_us=", fz_us(fz_prof.convert / fr));
    c = fz_put_u(c, "lcd_us=", fz_us(fz_prof.lcd / fr));
    c = fz_put_u(c, "input_us=", fz_us(fz_prof.input / fr));
    c = fz_put_u(c, "isr_in_frame_us=", fz_us(fz_prof.isr_in_frame / fr));
    c = fz_put_u(c, "rest_us=", fz_us((fz_prof.frame > parts ? fz_prof.frame - parts : 0u) / fr));   /* (the core's
                                                   * own: its screens' logic, the paths built, plutovg's state) */
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
}

int app_sysex(const uint8_t *p, uint32_t n)
{
    if (n >= 3u && p[0] == 0x7Du && p[1] == 0x46u && p[2] == 0x50u) {   /* "FP": the port's counters */
        if (n >= 4u && p[3] == 0x7Fu) {  /* (7F: and start them again) */
            fz_prof_reset();
            fz_mq_drops = 0;
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
static void fz_lights(void)              /* (FIZZ's state read, the interrupts on: a light a moment early or late) */
{
    uint8_t k[NKEYS], b[NB], g;
    uint32_t i, lit_b = 0, lit_k = 0, mid_b = 0, mid_k = 0, dim_b = 0, dim_k = 0;
    fz_leds(k, b, &g);
    for (i = 0; i < NB; i++) {           /* >= 128 lit, 25..127 the middle level, 1..24 a glow */
        lit_b |= (uint32_t)(b[i] >= 128u) << i;
        mid_b |= (uint32_t)(b[i] >= 25u && b[i] < 128u) << i;
        dim_b |= (uint32_t)(b[i] && b[i] < 25u) << i;
    }
    for (i = 0; i < NKEYS; i++) {
        lit_k |= (uint32_t)(k[i] >= 128u) << i;
        mid_k |= (uint32_t)(k[i] >= 25u && k[i] < 128u) << i;
        dim_k |= (uint32_t)(k[i] && k[i] < 25u) << i;
    }
    if (g >= 128u)                       /* PLAY's green LED (bit NB) */
        lit_b |= LED_PLAY_GREEN;
    else if (g >= 25u)
        mid_b |= LED_PLAY_GREEN;
    else if (g)
        dim_b |= LED_PLAY_GREEN;
    plat_lights(lit_b, lit_k, dim_b, dim_k, 1);
    plat_lights_mid(mid_b, mid_k);
}

/* ---------------------------------------------------------------- the screen --- */
static uint32_t fz_row_hash[240];        /* each row's pixels as last sent (0: unknown, sent again) */
static uint32_t fz_rows_sent;

static uint32_t fz_hash_row(const uint16_t *p)
{
    const uint32_t *w = (const uint32_t *)(const void *)p;
    uint32_t h = 2166136261u, i;
    for (i = 0; i < 120u; i++)
        h = (h ^ w[i]) * 16777619u;
    return h | 1u;
}

static void fz_strip_out(void *ctx, int y0, int rows, const uint16_t *px)   /* one of FIZZ's strips, drawn */
{
    int r, first = -1, last = -1;
    uint32_t t = plat_ticks24();
    (void)ctx;
    for (r = 0; r < rows; r++) {
        const uint32_t h = fz_hash_row(px + 240 * r);
        if (h != fz_row_hash[y0 + r]) {
            fz_row_hash[y0 + r] = h;
            if (first < 0)
                first = r;
            last = r;
        }
    }
    if (first >= 0) {                    /* the changed rows, from where FIZZ drew them (in RAM: the DMA's source) */
        lcd_blit(0, (uint32_t)(y0 + first), 240, (uint32_t)(last - first + 1), px + 240 * first);
        fz_rows_sent += (uint32_t)(last - first + 1);
    }
    fz_prof.lcd += plat_ticks24() - t;
#ifdef PLAT_HOST
    if (!fz_host_sync)                   /* (render_frames gives nothing between strips) */
#endif
        fz_input();                      /* while it goes out: the panel, MIDI, the clock */
    t = plat_ticks24();
    lcd_sync();                          /* (FIZZ draws its next strip where this one is) */
    fz_prof.lcd += plat_ticks24() - t;
}

static void fz_draw(void)
{
    const uint32_t t0 = plat_ticks24();
    uint32_t d;
    fz_in_frame = 1;
    fz_frame(fz_strip_out, 0);
    fz_in_frame = 0;
    d = plat_ticks24() - t0;
    fz_prof.frame += d;
    fz_prof.n_frame++;
    if (d > fz_prof.frame_max)
        fz_prof.frame_max = d;
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
}

void app_ui_init(void)
{
    uint32_t i;
    if (!fz_ok) {
        fz_message(fz_xdata_state() < 0 ? "FIZZ: ITS DATA IS ANOTHER BUILD'S" : "FIZZ NEEDS ITS DATA",
                   "RUN FIZZ_XDATA.PY OVER USB");
        return;
    }
    lcd_fill(0, 0, 240, 240, C_BLACK);
    for (i = 0; i < 240u; i++)
        fz_row_hash[i] = 0;              /* (the whole screen sent again) */
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
    static uint32_t led_t, frame_t;
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
    if (now - led_t >= FZ_LED_MS) {
        led_t = now;
        fz_lights();
    }
    fz_ui_state(&mode, &playing);
    period = now - fz_touch_ms < FZ_TOUCHED_MS ? FZ_FRAME_TOUCHED
             : mode == 0                       ? FZ_FRAME_IDLE
             : playing                         ? FZ_FRAME_PLAYING
                                               : FZ_FRAME_QUIET;
    if (now - frame_t >= period || !fz_prof.n_frame) {
        frame_t = now;
        fz_draw();
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
    else if (!strcmp(what, "fz_rows_sent"))
        *got = (int)fz_rows_sent;
    else if (!strcmp(what, "fz_sx_out"))
        *got = (int)fz_host_sx_out;
    else if (!strcmp(what, "fz_midi_drops"))
        *got = (int)fz_mq_drops;
    else
        return fz_expect(what, got);
    return 1;
}

/* sysex F0 .. F7 (hex bytes): a SysEx frame in over USB, as the platform gives it (the bytes between F0 and F7).
 * fzdraw: a frame drawn now, whatever the time since the last (a screen at an exact ms, as FIZZ's render_frames
 * draws its "frame at T"). fzsync, fzview N: as render_frames runs the app (tests/compare.txt) */
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
    if (!strcmp(cmd, "fzview")) {        /* fzview N: the sequencer's view (ctl.h QD_VIEW_*: 0 PIANO ROLL, 1 STEPS, 2 EDIT) */
        fz_host_view(atoi(a));
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
