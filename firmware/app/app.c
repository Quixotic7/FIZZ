/* SPDX-License-Identifier: MPL-2.0 */
/* FIZZ on fm1-base: fm1-base's side of the app (compiled inside its unity build, after the platform: gfx.c, lcd.c and
 * the plat_* calls are in scope). FIZZ's core is behind fizz_port.h (fz_port.c).
 *
 * ONE CONTEXT. FIZZ's core is written for one caller (FM1VST runs tick, input, frame and audio in turn on one thread),
 * so all of it runs on fm1-base's main loop: the panel's input, MIDI, the clock (plat_ms), the sound and the frames.
 * The audio interrupt (app_render) only plays what the main loop rendered ahead into a ring (FZ_RING_TARGET frames,
 * ~14.5 ms: the latency this adds; 256 more after each underrun, up to 40.6 ms). A frame is FIZZ's 20-row strips
 * (qd_app_frame_strips), each sent to the LCD from where FIZZ drew it (no second screen buffer): only its rows whose
 * pixels changed (a hash a row), and while the DMA sends them the ring is topped up, the panel is sampled (its presses
 * queued, given to FIZZ after the frame, in order: a tap shorter than a frame is not lost) and MIDI in is taken. The sequencer runs from the sound, so it keeps
 * time while a frame draws; what the screen shows may move between two strips of one frame (the playhead).
 *
 * The rest: the LEDs (FIZZ's 0..255 a light onto fm1-base's off / glow / mid / lit), MASTER (FIZZ's own volume:
 * app_render ignores the platform's gain), USB SysEx both ways (APP_SYSEX: FIZZ's 7D 46 5A, and 7D 46 50 for this
 * port's counters), the data blob's check and its writer (APP_USB_CMD, firmware/tools/fizz_xdata.py). */
#include "fizz_port.h"

#define FZ_BARRIER() __asm__ volatile("" ::: "memory")
#define FZ_FRAME_MS 33u                  /* a frame at most every 33 ms (30 a second: PLAN.md §13, the owner's) */
#define FZ_LED_MS 10u                    /* the LEDs looked at every 10 ms */
#define FZ_VEL 100                       /* the keys' velocity (no APP_TOUCH yet: FM1VST's default) */

/* ---------------------------------------------------------------- the sound's ring --- */
#define FZ_RING 2048u                    /* frames (a power of two, a multiple of FIZZ's 32-frame block): 8 KB */
#define FZ_RING_TARGET 640u              /* the main loop keeps this many ahead (14.5 ms: the latency) .. */
#define FZ_RING_MAX 1792u                /* .. and more after an underrun, 256 at a time, up to this (40.6 ms) */
#define FZ_BLOCK 32u                     /* FIZZ's control block (core/voice/mix.h QD_MIX_CTL) */
#ifndef PLAT_HOST
__attribute__((section(".fz_ringbuf")))  /* (fizz.ld: in the RAM code region's spare, cleared by app_init) */
#endif
static int16_t fz_ring[2u * FZ_RING];
static uint32_t fz_target = FZ_RING_TARGET, fz_seen_under;
static volatile uint32_t fz_ring_w, fz_ring_r; /* written frames, played frames (the interrupt moves fz_ring_r) */
static volatile uint8_t fz_ring_on;         /* 0: silence (no FIZZ, a panic, an update) */
static volatile uint32_t fz_underruns;      /* halves the ring ran dry in */

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)   /* the audio interrupt */
{
    uint32_t i, r = fz_ring_r, w = fz_ring_w;
    int dry = 0;
    (void)master_q12;                    /* (MASTER is FIZZ's own volume: fz_master) */
    for (i = 0; i < n; i++) {
        if (fz_ring_on && r != w) {
            const uint32_t k = 2u * (r % FZ_RING);
            lr[2u * i] = (int32_t)fz_ring[k] * 256;
            lr[2u * i + 1u] = (int32_t)fz_ring[k + 1u] * 256;
            r++;
        } else {
            lr[2u * i] = lr[2u * i + 1u] = 0;
            dry = 1;
        }
    }
    fz_ring_r = r;
    if (dry && fz_ring_on && w)          /* (w 0: nothing made yet, the boot) */
        fz_underruns++;
}

/* the port's counters (stats: 7D 46 50 over USB, expect fz_* in the host simulator) */
static struct {
    uint32_t frames, frame_us, frame_max_us, strip_max_us, audio_blocks, audio_us, audio_max_us, ring_min, sent_rows;
} fz_st = {0, 0, 0, 0, 0, 0, 0, FZ_RING, 0};

#ifdef PLAT_HOST
/* fzsync (the simulator's comparisons with FIZZ's render_frames): the sound made as render_frames makes it, 128-frame
 * blocks as they fall due by the clock, none ahead (the ring then runs dry: the comparison's screens, not its sound) */
static uint8_t fz_host_sync;
static uint64_t fz_host_sync_done;
#endif
static void fz_fill(void)                /* the ring topped up to fz_target */
{
#ifdef PLAT_HOST
    if (fz_host_sync) {
        while (fz_host_sync == 2 && fz_host_sync_done + 128u <= (uint64_t)plat_ms() * 44100u / 1000u) {
            uint32_t k;
            for (k = 0; k < 128u; k += FZ_BLOCK) {
                const uint32_t w = fz_ring_w;
                fz_audio(&fz_ring[2u * (w % FZ_RING)], FZ_BLOCK);
                fz_ring_w = w + FZ_BLOCK;
            }
            fz_host_sync_done += 128u;
        }
        return;
    }
#endif
    if (fz_underruns != fz_seen_under) { /* (the ring ran dry: a little more ahead from now on) */
        fz_seen_under = fz_underruns;
        if (fz_target < FZ_RING_MAX)
            fz_target += 256u;
    }
    while (fz_ring_on) {
        uint32_t w = fz_ring_w, have = w - fz_ring_r, t0;
        if (w && have < fz_st.ring_min)
            fz_st.ring_min = have;
        if (have + FZ_BLOCK > fz_target)
            break;
        t0 = plat_ticks24();
        fz_audio(&fz_ring[2u * (w % FZ_RING)], FZ_BLOCK);
        t0 = (plat_ticks24() - t0) / PLAT_TICKS_PER_US;
        fz_st.audio_blocks++;
        fz_st.audio_us += t0;
        if (t0 > fz_st.audio_max_us)
            fz_st.audio_max_us = t0;
        FZ_BARRIER();                    /* (the frames before the index the interrupt reads) */
        fz_ring_w = w + FZ_BLOCK;
    }
}

/* ---------------------------------------------------------------- the panel --- */
static uint8_t fz_ok;                    /* FIZZ is up (its data blob there) */
static uint32_t fz_seen_btn, fz_seen_keys;     /* the panel as last sampled */
#define FZ_EVQ 64u
static uint16_t fz_evq[FZ_EVQ];                /* presses waiting for FIZZ: bit 15 down, bit 8 a key, the low byte its index */
static uint32_t fz_ev_w, fz_ev_r;
static int fz_master_seen = -100;

static void fz_ev_put(uint32_t e)
{
    if (fz_ev_w - fz_ev_r < FZ_EVQ)
        fz_evq[fz_ev_w++ % FZ_EVQ] = (uint16_t)e;
}

static void fz_sample(void)                 /* the panel's changes since the last look, queued (FM1VST's order: the buttons
                                          * pressed before the keys, the buttons let go after them) */
{
    const uint32_t b = plat_buttons() & ((1u << NB) - 1u), k = plat_keys(), db = b ^ fz_seen_btn, dk = k ^ fz_seen_keys;
    uint32_t i;
    if (!db && !dk)
        return;
    for (i = 0; i < NB; i++)
        if ((db >> i & 1u) && (b >> i & 1u))
            fz_ev_put(0x8000u | i);
    for (i = 0; i < NKEYS; i++)
        if (dk >> i & 1u)
            fz_ev_put((k >> i & 1u ? 0x8000u : 0u) | 0x100u | i);
    for (i = 0; i < NB; i++)
        if ((db >> i & 1u) && !(b >> i & 1u))
            fz_ev_put(i);
    fz_seen_btn = b;
    fz_seen_keys = k;
}

static void fz_dispatch(void)               /* the queued presses into FIZZ, then the knobs and MASTER */
{
    int r, pot;
    while (fz_ev_r != fz_ev_w) {
        const uint32_t e = fz_evq[fz_ev_r++ % FZ_EVQ], i = e & 0xFFu, down = e >> 15;
        if (e & 0x100u)
            fz_key((int)i, (int)down, down ? FZ_VEL : 0);
        else
            fz_button((int)i, (int)down);
    }
    for (r = 0; r < NE; r++) {
        const int32_t d = plat_enc(r);
        if (d)
            fz_knob(r, (int)d);
    }
    pot = (int)(plat_master() * 1023u / 4096u);
    if (pot - fz_master_seen >= 2 || fz_master_seen - pot >= 2 || (pot != fz_master_seen && (pot == 0 || pot == 1023))) {
        fz_master_seen = pot;               /* (a step of 2: the pot's last bit wanders) */
        fz_master(pot);
    }
}

static void fz_midi_in(void)                /* USB and the TRS jack (fm1-base's queue): channel messages, the clock */
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        const uint32_t cin = pkt & 15u, s = pkt >> 8 & 0xFFu;
        if (cin >= 8u && cin <= 14u)
            fz_midi((int)s, (int)(pkt >> 16 & 0x7Fu), (int)(pkt >> 24 & 0x7Fu));
        else if (cin == 15u && s >= 0xF8u)
            fz_midi((int)s, 0, 0);
    }
}

/* ---------------------------------------------------------------- SysEx --- */
static uint8_t fz_sx_buf[1024];             /* a reply (fizz_sx.h QD_SX_FRAME_MAX) */
#ifdef PLAT_HOST
static uint32_t fz_host_sx_out;             /* replies made (the simulator has no USB) */
#endif
static void fz_sysex_out(void)
{
    int k;
    while ((k = fz_sysex_peek()) > 0 && k <= (int)sizeof fz_sx_buf) {
        k = fz_sysex_take(fz_sx_buf, sizeof fz_sx_buf);
        if (k <= 0)
            break;
#ifdef PLAT_HOST
        fz_host_sx_out++;
#endif
        plat_sysex_send(fz_sx_buf, (uint32_t)k);
    }
}

static char *fz_put_u(char *q, const char *label, uint32_t v)
{
    char t[12];
    int n = 0;
    while (*label)
        *q++ = *label++;
    do
        t[n++] = (char)('0' + v % 10u);
    while ((v /= 10u) && n < 11);
    while (n)
        *q++ = t[--n];
    *q++ = ' ';
    return q;
}

static void fz_stats_reply(void)            /* 7D 46 50: this port's counters, one line of ASCII (fizz_xdata.py stats) */
{
    uint8_t f[340], *q = f;
    char line[320], *c = line;           /* (14 labels, 136 characters, and 14 numbers of up to 10 digits) */
    const uint32_t fr = fz_st.frames ? fz_st.frames : 1u, ab = fz_st.audio_blocks ? fz_st.audio_blocks : 1u;
    c = fz_put_u(c, "up=", fz_ok);
    c = fz_put_u(c, "xdata=", (uint32_t)(fz_xdata_state() + 1));   /* (0 another's, 1 none, 2 this build's) */
    c = fz_put_u(c, "underruns=", fz_underruns);
    c = fz_put_u(c, "ring_min=", fz_st.ring_min);
    c = fz_put_u(c, "ring_target=", fz_target);
    c = fz_put_u(c, "frames=", fz_st.frames);
    c = fz_put_u(c, "frame_avg_us=", fz_st.frame_us / fr);
    c = fz_put_u(c, "frame_max_us=", fz_st.frame_max_us);
    c = fz_put_u(c, "strip_max_us=", fz_st.strip_max_us);
    c = fz_put_u(c, "rows_sent=", fz_st.sent_rows);
    c = fz_put_u(c, "blk32_avg_us=", fz_st.audio_us / ab);
    c = fz_put_u(c, "blk32_max_us=", fz_st.audio_max_us);
    c = fz_put_u(c, "cpu_isr=", plat_cpu_pct());
    c = fz_put_u(c, "xruns=", plat_xruns());
    *q++ = 0xF0, *q++ = 0x7D, *q++ = 0x46, *q++ = 0x50, *q++ = 0x01;
    for (*c = 0, c = line; *c && q < f + sizeof f - 1u; c++)
        *q++ = (uint8_t)(*c & 0x7F);
    *q++ = 0xF7;
    plat_sysex_send(f, (uint32_t)(q - f));
}

int app_sysex(const uint8_t *p, uint32_t n)
{
    if (n >= 3u && p[0] == 0x7Du && p[1] == 0x46u && p[2] == 0x50u) {   /* "FP": the port's counters */
        if (n >= 4u && p[3] == 0x7Fu)    /* (7F: and start them again) */
            fz_st.frames = fz_st.frame_us = fz_st.frame_max_us = fz_st.strip_max_us = fz_st.audio_blocks = fz_st.audio_us =
                fz_st.audio_max_us = fz_st.sent_rows = fz_underruns = 0, fz_st.ring_min = FZ_RING;
        else
            fz_stats_reply();
        return 1;
    }
    if (n < 3u || p[0] != 0x7Du || p[1] != 0x46u || p[2] != 0x5Au)
        return 0;                        /* (not FIZZ's: the platform drops it) */
    if (fz_ok) {
        fz_sysex(p, n);
        fz_sysex_out();
    }
    return 1;
}

/* ---------------------------------------------------------------- the LEDs --- */
static void fz_lights(void)
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
static uint32_t fz_row_hash[240];           /* each row's pixels as last sent (0: unknown, sent again) */

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
    static uint32_t t_strip;
    int r, first = -1, last = -1;
    uint32_t t;
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
        fz_st.sent_rows += (uint32_t)(last - first + 1);
    }
    fz_sample();                            /* while it goes out: the panel, MIDI, the sound */
    fz_midi_in();
    fz_fill();
    lcd_sync();                          /* (FIZZ draws its next strip where this one is) */
    t = plat_ticks24();
    if (y0 && (t - t_strip) / PLAT_TICKS_PER_US > fz_st.strip_max_us)
        fz_st.strip_max_us = (t - t_strip) / PLAT_TICKS_PER_US;
    t_strip = t;
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
        fz_row_hash[i] = 0;                 /* (the whole screen sent again) */
    fz_ring_on = 1;
    fz_fill();                           /* (the sound ahead before the main loop's first look) */
}

void app_panic(void)
{
    fz_ring_on = 0;
    fz_ring_r = fz_ring_w;
}

void app_say(const char *a, const char *b) { (void)a, (void)b; }

static void fz_step(int frame)
{
    static uint32_t led_t, frame_t;
    const uint32_t now = plat_ms();
    if (!fz_ok)
        return;
    fz_sample();
    fz_dispatch();
    fz_midi_in();
    fz_tick(now);
    fz_sysex_out();
#ifdef PLAT_HOST
    if (fz_host_sync) {                     /* (render_frames' order and cadence: a frame every 15 ms, then the sound) */
        if (now - frame_t >= 15u) {
            frame_t = now;
            fz_frame(fz_strip_out, 0);
            fz_st.frames++;
        }
        fz_host_sync = 2;                   /* (the sound now, not from the frame's strips) */
        fz_fill();
        fz_host_sync = 1;
        fz_lights();
        return;
    }
#endif
    fz_fill();
    if (now - led_t >= FZ_LED_MS) {
        led_t = now;
        fz_lights();
    }
    if (frame && (now - frame_t >= FZ_FRAME_MS || !fz_st.frames)) {
        uint32_t t0 = plat_ticks24();
        frame_t = now;
        fz_frame(fz_strip_out, 0);
        t0 = (plat_ticks24() - t0) / PLAT_TICKS_PER_US;
        fz_st.frames++;
        fz_st.frame_us += t0;
        if (t0 > fz_st.frame_max_us)
            fz_st.frame_max_us = t0;
        fz_dispatch();                      /* (what was pressed while it drew) */
        fz_fill();
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
        fz_usb_reply("FZER", 1);            /* (another build's blob: this unit's is what the stamp says) */
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
    if (!rc)
        fz_ok = (uint8_t)fz_init(APP_NAME " " APP_VERSION);
    app_ui_init();                       /* (FIZZ's screen, or the message again) */
}

/* ---------------------------------------------------------------- the host simulator --- */
#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "fz_underruns"))
        *got = (int)fz_underruns;
    else if (!strcmp(what, "fz_frames"))
        *got = (int)fz_st.frames;
    else if (!strcmp(what, "fz_ring_min"))
        *got = (int)fz_st.ring_min;
    else if (!strcmp(what, "fz_rows_sent"))
        *got = (int)fz_st.sent_rows;
    else if (!strcmp(what, "fz_sx_out"))
        *got = (int)fz_host_sx_out;
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
    if (!strcmp(cmd, "fzsync")) {      /* fzsync: the sound made as render_frames makes it (above) */
        fz_host_sync = 1;
        fz_host_sync_done = (uint64_t)plat_ms() * 44100u / 1000u / 128u * 128u;
        return 1;
    }
    if (!strcmp(cmd, "fzview")) {      /* fzview N: the sequencer's view (ctl.h QD_VIEW_*: 0 PIANO ROLL, 1 STEPS, 2 EDIT) */
        fz_host_view(atoi(a));
        return 1;
    }
    if (!strcmp(cmd, "fzdraw")) {
        if (fz_ok) {
            fz_frame(fz_strip_out, 0);
            fz_st.frames++;
        }
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
