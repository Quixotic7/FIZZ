/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The host around FIZZ's core on iOS and macOS (fizzhost.h): the clock, the input queue, the published output. Written
 * for FIZZ for iOS against the core ABI (fm1core.h); the clock follows the ABI's contract (tick, frame / idle, audio
 * blocks at 44.1 kHz), as FM1VST's engine/device.cpp runs it. */
#include <mach/mach_time.h>
#include <os/lock.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "fm1core.h"
#include "fizzhost.h"

static const fm1core_t *C;

/* ---------------------------------------------------------------- the clock token --- */
static atomic_flag tok = ATOMIC_FLAG_INIT;
static int tok_try(void) { return !atomic_flag_test_and_set_explicit(&tok, memory_order_acquire); }
static void tok_free(void) { atomic_flag_clear_explicit(&tok, memory_order_release); }
void fzh_lock(void)
{
    while (!tok_try())
        sched_yield();
}
void fzh_unlock(void) { tok_free(); }

/* ---------------------------------------------------------------- the descriptor --- */
const void *fizzcore_get(void) { return fm1core_get(FM1CORE_ABI); }
int fzh_open(void)
{
    if (!C)
        C = fm1core_get(FM1CORE_ABI);
    return C != NULL && C->panel != NULL;
}
const char *fzh_str(uint32_t k)
{
    if (!fzh_open())
        return NULL;
    switch (k) {
    case 0: return C->id;
    case 1: return C->name;
    case 2: return C->version;
    case 3: return C->source_url;
    case 4: return C->family;
    case 5: return C->license;
    case 6: return fm1core_credits(C);
    case 7: return C->gated;
    default: return k >= 100u && k < 100u + FM1P_NB ? C->button_names[k - 100u] : NULL;
    }
}
uint32_t fzh_flash_size(void) { return fzh_open() ? C->flash_size : 0u; }

/* ---------------------------------------------------------------- state (the clock's) --- */
static int up;
static uint32_t dev_ms, last_frame;
static uint64_t frames_done;
#define RING 8192u                                /* device frames */
static int16_t ring[RING * 2];
static uint32_t ring_w, ring_r;

/* a 4-point Lagrange interpolator, stereo: the device's 44.1 kHz to the host's rate */
static struct {
    double rate, step, frac;
    int primed;
    float h[2][4];
} rs;
static void rs_reset(double rate)
{
    memset(&rs, 0, sizeof rs);
    rs.rate = rate;
    rs.step = (double)FM1P_FS / rate;
}
static int rs_need(void) { return rs.primed < 3 ? 3 - rs.primed : rs.frac >= 1.0 ? 1 : 0; }
static void rs_push(float l, float r)
{
    for (int c = 0; c < 2; c++) {
        float *h = rs.h[c];
        h[0] = h[1], h[1] = h[2], h[2] = h[3], h[3] = c ? r : l;
    }
    if (rs.primed < 3)
        rs.primed++;
    else
        rs.frac -= 1.0;
}
static float rs_at(const float *h, float t)
{
    const float c0 = -t * (t - 1.0f) * (t - 2.0f) * (1.0f / 6.0f), c1 = (t + 1.0f) * (t - 1.0f) * (t - 2.0f) * 0.5f;
    const float c2 = -(t + 1.0f) * t * (t - 2.0f) * 0.5f, c3 = (t + 1.0f) * t * (t - 1.0f) * (1.0f / 6.0f);
    return c0 * h[0] + c1 * h[1] + c2 * h[2] + c3 * h[3];
}

/* ---------------------------------------------------------------- the input queue --- */
enum { EV_BTN, EV_KEY, EV_ENC, EV_MASTER, EV_MIDI, EV_CLEAR };
typedef struct {
    uint8_t t, a, down, vel;
    int32_t v;
} ev_t;
#define QN 16384u
static ev_t q[QN];
static uint32_t q_r, q_w;
static os_unfair_lock q_lock = OS_UNFAIR_LOCK_INIT;

static int push(ev_t e)
{
    int ok = 0;
    os_unfair_lock_lock(&q_lock);
    if (q_w - q_r < QN) {
        q[q_w++ % QN] = e;
        ok = 1;
    }
    os_unfair_lock_unlock(&q_lock);
    return ok;
}
void fzh_button(uint32_t label, int down)
{
    if (label < FM1P_NB)
        push((ev_t){.t = EV_BTN, .a = (uint8_t)label, .down = (uint8_t)(down != 0)});
}
void fzh_key(uint32_t key, int down, uint32_t velocity)
{
    if (key < FM1P_NKEY)
        push((ev_t){.t = EV_KEY, .a = (uint8_t)key, .down = (uint8_t)(down != 0),
                    .vel = (uint8_t)(velocity > 127u ? 127u : velocity)});
}
void fzh_enc(uint32_t role, int32_t detents)
{
    if (role < FM1P_NE - 1u && detents)
        push((ev_t){.t = EV_ENC, .a = (uint8_t)role, .v = detents});
}
void fzh_master(int32_t value)
{
    push((ev_t){.t = EV_MASTER, .v = value < 0 ? 0 : value > 1023 ? 1023 : value});
}
int fzh_midi_in(uint32_t pkt) { return push((ev_t){.t = EV_MIDI, .v = (int32_t)pkt}); }
void fzh_input_clear(void)
{
    os_unfair_lock_lock(&q_lock);
    q_r = q_w;
    os_unfair_lock_unlock(&q_lock);
    push((ev_t){.t = EV_CLEAR});
}

/* one event onto the panel (Device::buttons / keys / enc / master); 0: MIDI the core has no room for yet */
static int apply(const ev_t *e)
{
    fm1panel_t *p = C->panel;
    uint32_t bit;
    switch (e->t) {
    case EV_BTN:
        bit = 1u << p->btn_id[e->a];
        if (e->down) {
            if (!(p->buttons & bit))
                p->buttons_tap |= bit;
            p->buttons |= bit;
        } else {
            p->buttons &= ~bit;
        }
        return 1;
    case EV_KEY:
        bit = 1u << e->a;
        if (e->down) {
            p->key_vel[e->a] = e->vel;
            if (!(p->keys & bit))
                p->keys_tap |= bit;
            p->keys |= bit;
        } else {
            p->keys &= ~bit;
        }
        return 1;
    case EV_ENC:
        p->enc[p->enc_id[e->a]] += e->v * p->enc_dir[e->a];
        return 1;
    case EV_MASTER:
        p->master = e->v;
        return 1;
    case EV_MIDI:
        return C->midi_in ? C->midi_in((uint32_t)e->v) : 1;
    case EV_CLEAR:
        p->keys = p->buttons = 0;
        return 1;
    }
    return 1;
}
/* the clock's side: never waits for the lock (a producer holding it: next ms) */
static void drain_in(void)
{
    if (!os_unfair_lock_trylock(&q_lock))
        return;
    while (q_r != q_w && apply(&q[q_r % QN]))
        q_r++;
    os_unfair_lock_unlock(&q_lock);
}

/* ---------------------------------------------------------------- MIDI out (clock -> one reader) --- */
#define ON 8192u
static uint32_t outq[ON];
static _Atomic uint32_t out_w, out_r;
static void drain_out(void)
{
    uint32_t w = atomic_load_explicit(&out_w, memory_order_relaxed);
    const uint32_t r = atomic_load_explicit(&out_r, memory_order_acquire);
    uint32_t pkt;
    while (w - r < ON && C->midi_out_take && C->midi_out_take(&pkt))
        outq[w++ % ON] = pkt;
    atomic_store_explicit(&out_w, w, memory_order_release);
}
uint32_t fzh_midi_out(uint32_t *pkts, uint32_t max)
{
    uint32_t r = atomic_load_explicit(&out_r, memory_order_relaxed), n = 0;
    const uint32_t w = atomic_load_explicit(&out_w, memory_order_acquire);
    while (r != w && n < max)
        pkts[n++] = outq[r++ % ON];
    atomic_store_explicit(&out_r, r, memory_order_release);
    return n;
}

/* ---------------------------------------------------------------- the LCD and the LEDs (seqlocks) --- */
static uint16_t lcd_pub[FM1P_LCD_W * FM1P_LCD_H];
static _Atomic uint32_t lcd_sq, lcd_count;
static uint32_t lcd_writes_pub, lcd_pub_ms;
static int lcd_any;
static void publish_lcd(uint32_t ms, int force)
{
    const fm1panel_t *p = C->panel;
    uint32_t s;
    if (!force && lcd_any && (p->lcd_writes == lcd_writes_pub || ms - lcd_pub_ms < 16u))
        return;
    s = atomic_load_explicit(&lcd_sq, memory_order_relaxed);
    atomic_store_explicit(&lcd_sq, s + 1u, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);
    memcpy(lcd_pub, p->lcd, sizeof lcd_pub);
    atomic_store_explicit(&lcd_sq, s + 2u, memory_order_release);
    atomic_fetch_add_explicit(&lcd_count, 1u, memory_order_release);
    lcd_writes_pub = p->lcd_writes, lcd_pub_ms = ms, lcd_any = 1;
}
uint32_t fzh_lcd_seq(void) { return atomic_load_explicit(&lcd_count, memory_order_acquire); }
int fzh_lcd_rgba(uint8_t *rgba, uint32_t *seen)
{
    static uint16_t px[FM1P_LCD_W * FM1P_LCD_H];   /* (one reader) */
    const uint32_t c = atomic_load_explicit(&lcd_count, memory_order_acquire);
    int tries;
    if (c == *seen)
        return 0;
    for (tries = 0; tries < 64; tries++) {
        const uint32_t s1 = atomic_load_explicit(&lcd_sq, memory_order_acquire);
        if (s1 & 1u) {
            sched_yield();
            continue;
        }
        memcpy(px, lcd_pub, sizeof px);
        atomic_thread_fence(memory_order_acquire);
        if (atomic_load_explicit(&lcd_sq, memory_order_relaxed) == s1)
            break;
    }
    for (uint32_t i = 0; i < FM1P_LCD_W * FM1P_LCD_H; i++) {
        const uint16_t v = (uint16_t)((px[i] >> 8) | (px[i] << 8));   /* (big-endian as sent) */
        const uint32_t r = v >> 11, g = (v >> 5) & 63u, b = v & 31u;
        rgba[4 * i] = (uint8_t)((r << 3) | (r >> 2));
        rgba[4 * i + 1] = (uint8_t)((g << 2) | (g >> 4));
        rgba[4 * i + 2] = (uint8_t)((b << 3) | (b >> 2));
        rgba[4 * i + 3] = 255;
    }
    *seen = c;
    return 1;
}

static uint8_t led_pub[FM1P_NCOL * FM1P_NLINE + 1];
static _Atomic uint32_t led_sq, led_count;
static void publish_leds(void)
{
    const fm1panel_t *p = C->panel;
    uint32_t s;
    if (!memcmp(led_pub, &p->led[0][0], FM1P_NCOL * FM1P_NLINE) && led_pub[FM1P_NCOL * FM1P_NLINE] == p->led_play_green &&
        atomic_load_explicit(&led_count, memory_order_relaxed))
        return;
    s = atomic_load_explicit(&led_sq, memory_order_relaxed);
    atomic_store_explicit(&led_sq, s + 1u, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);
    memcpy(led_pub, &p->led[0][0], FM1P_NCOL * FM1P_NLINE);
    led_pub[FM1P_NCOL * FM1P_NLINE] = p->led_play_green;
    atomic_store_explicit(&led_sq, s + 2u, memory_order_release);
    atomic_fetch_add_explicit(&led_count, 1u, memory_order_release);
}
uint32_t fzh_leds(uint8_t *led44, uint8_t *play_green)
{
    uint8_t b[sizeof led_pub];
    for (int tries = 0; tries < 64; tries++) {
        const uint32_t s1 = atomic_load_explicit(&led_sq, memory_order_acquire);
        if (s1 & 1u)
            continue;
        memcpy(b, led_pub, sizeof b);
        atomic_thread_fence(memory_order_acquire);
        if (atomic_load_explicit(&led_sq, memory_order_relaxed) == s1)
            break;
    }
    if (led44)
        memcpy(led44, b, FM1P_NCOL * FM1P_NLINE);
    if (play_green)
        *play_green = b[FM1P_NCOL * FM1P_NLINE];
    return atomic_load_explicit(&led_count, memory_order_acquire);
}

/* ---------------------------------------------------------------- the flash --- */
static _Atomic uint32_t dirty_pub;
static _Atomic int fl_req, fl_ready;
static uint8_t *fl_snap;
static uint32_t fl_snap_dirty;
uint32_t fzh_flash_dirty(void) { return atomic_load_explicit(&dirty_pub, memory_order_relaxed); }
void fzh_flash_request(void) { atomic_store_explicit(&fl_req, 1, memory_order_release); }
int fzh_flash_take(uint8_t *dst, uint32_t *dirty)
{
    if (!atomic_load_explicit(&fl_ready, memory_order_acquire))
        return 0;
    memcpy(dst, fl_snap, C->flash_size);
    if (dirty)
        *dirty = fl_snap_dirty;
    atomic_store_explicit(&fl_ready, 0, memory_order_release);
    return 1;
}
static void serve_flash(void)
{
    if (!fl_snap || !atomic_load_explicit(&fl_req, memory_order_acquire) ||
        atomic_load_explicit(&fl_ready, memory_order_acquire))
        return;
    if (C->flash_sync)
        C->flash_sync();
    memcpy(fl_snap, C->flash(), C->flash_size);
    fl_snap_dirty = C->flash_dirty();
    atomic_store_explicit(&dirty_pub, fl_snap_dirty, memory_order_relaxed);
    atomic_store_explicit(&fl_req, 0, memory_order_relaxed);
    atomic_store_explicit(&fl_ready, 1, memory_order_release);
}
uint32_t fzh_flash_copy(uint8_t *dst)
{
    if (up && C->flash_sync)
        C->flash_sync();
    memcpy(dst, C->flash(), C->flash_size);
    return C->flash_dirty();
}

/* ---------------------------------------------------------------- the cost --- */
static mach_timebase_info_data_t tb;
static _Atomic uint64_t st_blocks, st_audio_ns, st_audio_max, st_ms, st_clock_ns, st_clock_max;
static uint64_t ns(uint64_t t) { return t * tb.numer / tb.denom; }
static void add_max(_Atomic uint64_t *sum, _Atomic uint64_t *mx, uint64_t v)
{
    atomic_fetch_add_explicit(sum, v, memory_order_relaxed);
    if (v > atomic_load_explicit(mx, memory_order_relaxed))
        atomic_store_explicit(mx, v, memory_order_relaxed);
}
void fzh_stats(fzh_stats_t *o)
{
    o->blocks = atomic_load(&st_blocks), o->audio_ns = atomic_load(&st_audio_ns);
    o->audio_max_ns = atomic_load(&st_audio_max), o->ms_run = atomic_load(&st_ms);
    o->clock_ns = atomic_load(&st_clock_ns), o->clock_max_ns = atomic_load(&st_clock_max);
}
void fzh_stats_reset(void)
{
    atomic_store(&st_blocks, 0), atomic_store(&st_audio_ns, 0), atomic_store(&st_audio_max, 0);
    atomic_store(&st_ms, 0), atomic_store(&st_clock_ns, 0), atomic_store(&st_clock_max, 0);
}

/* ---------------------------------------------------------------- one device millisecond --- */
static void run_ms(void)
{
    const uint64_t t0 = mach_absolute_time();
    uint32_t ms;
    int frame;
    drain_in();
    ms = dev_ms++;
    C->tick(ms);
    frame = ms == 0 || ms - last_frame >= 15u;
    if (frame) {
        last_frame = ms;
        C->frame();
    } else {
        C->idle();
    }
    while (frames_done + FM1P_BLOCK <= (uint64_t)ms * FM1P_FS / 1000u) {
        int16_t blk[FM1P_BLOCK * 2];
        const uint64_t a0 = mach_absolute_time();
        C->audio(blk, FM1P_BLOCK);
        add_max(&st_audio_ns, &st_audio_max, ns(mach_absolute_time() - a0));
        atomic_fetch_add_explicit(&st_blocks, 1u, memory_order_relaxed);
        if (ring_w - ring_r > RING - FM1P_BLOCK)
            ring_r = ring_w - (RING - FM1P_BLOCK);   /* (nobody takes the audio: the oldest goes) */
        for (uint32_t k = 0; k < FM1P_BLOCK; k++) {
            const uint32_t i = ring_w++ % RING;
            ring[2 * i] = blk[2 * k], ring[2 * i + 1] = blk[2 * k + 1];
        }
        frames_done += FM1P_BLOCK;
    }
    drain_out();
    publish_lcd(ms, 0);
    publish_leds();
    atomic_store_explicit(&dirty_pub, C->flash_dirty(), memory_order_relaxed);
    serve_flash();
    atomic_fetch_add_explicit(&st_ms, 1u, memory_order_relaxed);
    add_max(&st_clock_ns, &st_clock_max, ns(mach_absolute_time() - t0));
}

static void pop(float *l, float *r)
{
    uint32_t i;
    while (ring_r == ring_w)
        run_ms();
    i = ring_r++ % RING;
    *l = (float)ring[2 * i] * (1.0f / 32768.0f);
    *r = (float)ring[2 * i + 1] * (1.0f / 32768.0f);
}

uint32_t fzh_render(float *left, float *right, uint32_t n, double rate)
{
    if (!tok_try()) {
        memset(left, 0, n * sizeof *left), memset(right, 0, n * sizeof *right);
        return 0;
    }
    if (!up) {
        tok_free();
        memset(left, 0, n * sizeof *left), memset(right, 0, n * sizeof *right);
        return 0;
    }
    if (rate == (double)FM1P_FS) {
        rs.rate = 0;
        for (uint32_t k = 0; k < n; k++)
            pop(&left[k], &right[k]);
    } else {
        if (rs.rate != rate)
            rs_reset(rate);
        for (uint32_t k = 0; k < n; k++) {
            while (rs_need()) {
                float l, r;
                pop(&l, &r);
                rs_push(l, r);
            }
            left[k] = rs_at(rs.h[0], (float)rs.frac);
            right[k] = rs_at(rs.h[1], (float)rs.frac);
            rs.frac += rs.step;
        }
    }
    tok_free();
    return n;
}

uint32_t fzh_run(uint32_t n)
{
    uint32_t k;
    if (!tok_try())
        return 0;
    if (!up) {
        tok_free();
        return 0;
    }
    for (k = 0; k < n; k++) {
        run_ms();
        ring_r = ring_w;
    }
    tok_free();
    return n;
}

/* ---------------------------------------------------------------- power --- */
void fzh_boot(const uint8_t *bytes, uint32_t n)
{
    fm1panel_t *p;
    if (!fzh_open())
        return;
    if (!tb.denom)
        mach_timebase_info(&tb);
    if (up) {
        if (C->shutdown)
            C->shutdown();
        up = 0;
    }
    if (!fl_snap)
        fl_snap = malloc(C->flash_size);
    p = C->panel;
    p->keys = p->buttons = p->keys_tap = p->buttons_tap = 0;
    memset((void *)p->enc, 0, sizeof p->enc);
    p->master = 0;
    C->flash_stage(bytes, bytes ? n : 0u);
    C->options(FM1CORE_FLASH_STAGED, 0, 0, 1);
    if (C->boot_options)
        C->boot_options(-1, NULL, -1);
    p->ready = 2;
    C->init(0);
    up = 1;
    dev_ms = last_frame = 0;
    frames_done = 0;
    ring_w = ring_r = 0;
    rs.rate = 0;
    os_unfair_lock_lock(&q_lock);
    q_r = q_w;
    os_unfair_lock_unlock(&q_lock);
    atomic_store(&out_r, atomic_load(&out_w));
    atomic_store(&fl_req, 0), atomic_store(&fl_ready, 0);
    atomic_store(&dirty_pub, C->flash_dirty());
    publish_lcd(0, 1);
    publish_leds();
}

void fzh_shutdown(void)
{
    if (up && C->shutdown)
        C->shutdown();
    up = 0;
}
int fzh_up(void) { return up; }
uint32_t fzh_ms(void) { return dev_ms; }
void fzh_ui_info(char *buf, uint32_t n)
{
    if (!buf || !n)
        return;
    buf[0] = 0;
    if (up && C->ui_info)
        C->ui_info(buf, n);
}
void fzh_panel_table(uint8_t *btn_id, uint8_t *enc_id, int8_t *enc_dir, int8_t *keymap)
{
    if (!fzh_open())
        return;
    if (btn_id)
        memcpy(btn_id, C->panel->btn_id, FM1P_NB);
    if (enc_id)
        memcpy(enc_id, C->panel->enc_id, FM1P_NE - 1);
    if (enc_dir)
        memcpy(enc_dir, C->panel->enc_dir, FM1P_NE - 1);
    if (keymap)
        memcpy(keymap, C->keymap, 6 * FM1P_NCOL);
}
