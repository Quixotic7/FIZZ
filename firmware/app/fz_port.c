/* SPDX-License-Identifier: MPL-2.0 */
/* FIZZ's side of the seam (fizz_port.h): the core (core/app) driven as FM1VST's glue drives it (core/glue/
 * core_quadrille.c), from fm1-base's main loop. A unit of its own (fm1.toml o2), built with FIZZ's device profile
 * (fizz_prefix.h: QD_DEVICE; on the unit QD_TARGET_FM1). Its .bss (the one qd_app_t) goes to fm1-base's pool
 * (fizz.ld). */
#include <string.h>

#include "../../core/app/app.h"
#include "../../core/gfx/qd_gfx.h"
#include "../../core/gfx/qd_arena.h"
#include "../../core/voice/quad/quad_voice.h"
#include "plat.h"
#include "fizz_port.h"
#include "fz_prof.h"

/* the store on the unit's flash (persist.h, P2.5): the data region (0x97000..0xDFFFF, 73 sectors) from its first
 * byte; the stores' writes are the port's own steps (qd_persist_own_step: fz_persist_step, outside the locks) */
#ifndef FZ_FLASH
#define FZ_FLASH 1
#endif

static qd_app_t fz_app;
static int fz_up;

/* ---------------------------------------------------------------- the flash (persist.h, store.h) --- */
#if FZ_FLASH
#if QD_FLASH_PRJ + QD_FLASH_PRJ_SEC * 4096u > PLAT_DATA_HI - PLAT_DATA_LO || QD_FLASH_LIB_SEC * 4096u > QD_FLASH_PRJ
#error "fz_port.c: FIZZ's stores must fit the data region (persist.h QD_FLASH_* at the device's sizes)"
#endif
static int fz_fread(void *c, uint32_t off, void *dst, uint32_t n)
{
    const uint8_t *p = plat_data(PLAT_DATA_LO + off);
    (void)c;
    if (!p || !PLAT_DATA_OK(PLAT_DATA_LO + off, n))
        return -1;
    memcpy(dst, p, n);
    return 0;
}
static int fz_ferase(void *c, uint32_t off)
{
    int r;
    (void)c;
    fz_watchdog();                       /* (a store's long flow is a loop of these: the watchdog fed at each) */
    fz_flash_quiet();                    /* (P2.7: the LED lines dark while the flash keeps the interrupts off) */
    r = plat_data_erase(PLAT_DATA_LO + (off & ~0xFFFu));
    fz_flash_done();
    return r;
}
static int fz_fprog(void *c, uint32_t off, const void *src, uint32_t n)
{
    (void)c;
    fz_watchdog();
    while (n) {                          /* (P2.7: a flash page at a time, each with the LED lines dark: fm1-base's
                                          * write turns the interrupts on between its pages, and the scan lights
                                          * them again) */
        const uint32_t k = n < 256u - (off & 255u) ? n : 256u - (off & 255u);
        int r;
        fz_flash_quiet();
        r = plat_data_write(PLAT_DATA_LO + off, src, k);
        fz_flash_done();
        if (r)
            return r;
        off += k, src = (const uint8_t *)src + k, n -= k;
    }
    return 0;
}
static const qd_flash_t fz_fl = {fz_fread, fz_ferase, fz_fprog, 0};
#endif

/* ---------------------------------------------------------------- the data blob (qd_config.h QD_XDATA) --- */
/* The link puts the .qd_xdata.* sections at the XIP address of the data flash's second window (fizz.ld; plat.h
 * PLAT_XDATA1); firmware/build.py writes the blob's length and CRC-32 into this stamp, and firmware/tools/
 * fizz_xdata.py writes the blob there, then a trailer: FZXT, its length, its CRC-32. The unit draws only when the
 * flash holds this build's. */
static const volatile uint32_t fz_stamp[4] = {0x44585A46u, 0x504D5453u, 0u, 0u};   /* "FZXDSTMP", len, crc */
#define FZ_TRAILER 0x54585A46u                       /* "FZXT" */
#ifdef QD_TARGET_FM1
extern const uint8_t _fz_xdata_start[], _fz_xdata_end[];
#endif

uint32_t fz_xdata_len(void) { return fz_stamp[2]; }
uint32_t fz_xdata_crc(void) { return fz_stamp[3]; }
uint32_t fz_xdata_flash_off(void)
{
#ifdef QD_TARGET_FM1
    return PLAT_XDATA1_LO;                             /* (fizz.ld: the blob from the window's first byte) */
#else
    return 0;
#endif
}

static uint32_t fz_crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

int fz_xdata_state(void)
{
#ifdef QD_TARGET_FM1
    const uint32_t len = (uint32_t)(_fz_xdata_end - _fz_xdata_start);
    const uint8_t *t = plat_data(fz_xdata_flash_off() + ((len + 3u) & ~3u));
    uint32_t tr[3];
    if (!len)
        return 1;                                      /* (nothing in .qd_xdata: nothing to check) */
    if (!t || !plat_data(fz_xdata_flash_off()) || fz_stamp[2] != len)
        return -1;
    memcpy(tr, t, sizeof tr);
    if (tr[0] != FZ_TRAILER)
        return 0;
    if (tr[1] != len || tr[2] != fz_stamp[3] || fz_crc32(_fz_xdata_start, len) != fz_stamp[3])
        return -1;
#endif
    return 1;
}

/* ---------------------------------------------------------------- the calls --- */
int fz_init(const char *version)
{
    if (fz_xdata_state() != 1 || !qd_init())
        return 0;
    qd_app_init(&fz_app);
    qd_app_sysex_version(&fz_app, version);
    qd_app_tick(&fz_app, 0);
    fz_up = 1;
    return 1;
}

/* P2.5: the store, attached after fz_init (app.c: unless safe mode). A region with no sector of FIZZ's stores in it
 * and something else written (ChoralRoot's, RemoteFM1's, garbage) is erased first, whole: FIZZ owns it (the owner,
 * 2026-10-23). Returns the sectors erased (-1: no store in this build) */
#define FZ_STORE_SECTORS ((PLAT_DATA_HI - PLAT_DATA_LO) / 4096u)
int fz_store_foreign(void)               /* sectors neither blank nor FIZZ's, when none is FIZZ's (else 0) */
{
    uint32_t k, i, fizz = 0, other = 0;
    for (k = 0; k < FZ_STORE_SECTORS; k++) {
        const uint8_t *p = plat_data(PLAT_DATA_LO + k * 4096u);
        if (!p)
            return 0;
        if ((p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24) == QDS_MAGIC) {
            fizz++;
            continue;
        }
        for (i = 0; i < 4096u && p[i] == 0xFFu; i++)
            ;
        other += i < 4096u;
    }
    return fizz ? 0 : (int)other;
}
int fz_store_wipe(void)                  /* every non-blank sector of the region erased; the sectors erased */
{
    uint32_t k, i;
    int n = 0;
    for (k = 0; k < FZ_STORE_SECTORS; k++) {
        const uint8_t *p = plat_data(PLAT_DATA_LO + k * 4096u);
        for (i = 0; p && i < 4096u && p[i] == 0xFFu; i++)
            ;
        if (p && i < 4096u) {
            int r;
            fz_flash_quiet();
            r = plat_data_erase(PLAT_DATA_LO + k * 4096u);
            fz_flash_done();
            n += !r;
        }
    }
    return n;
}
void fz_store_attach(void)
{
#if FZ_FLASH
    if (!fz_up)
        return;
    qd_persist_attach(&fz_app, &fz_fl);
    qd_persist_own_step(&fz_app, 1);
#endif
}

/* (P2.9) what an input's gesture opens (its undo snapshot: patterns read, the undo log written, nothing the audio side
 * touches), done before the audio lock: the locked call then finds it open (qd_app_input_prepare, core 7984295) */
void fz_input_prepare(void) { qd_app_input_prepare(&fz_app); }
/* (2026-10-24: the browser's freezes) the longest input call (ms) and the most patterns one read in or wrote; the most
 * patterns a frame read in, the longest frame (ms): stats line state's in_ms_max .. fr_ms_max */
#define fz_in_ms_max fz_in_stat[0]          /* (app.c's: this unit's .bss is the pool's, or the RAM code region's) */
#define fz_in_loads_max fz_in_stat[1]
#define fz_in_saves_max fz_in_stat[2]
#define fz_fr_loads_max fz_in_stat[3]
#define fz_fr_ms_max fz_in_stat[4]
static void fz_in_done(uint32_t t0, uint32_t l0, uint32_t s0)
{
    const uint32_t t = plat_ms() - t0, l = fz_app.proj.pat_loads - l0, w = fz_app.proj.pat_saves - s0;
    if (t > fz_in_ms_max)
        fz_in_ms_max = (uint16_t)(t > 0xFFFFu ? 0xFFFFu : t);
    if (l > fz_in_loads_max)
        fz_in_loads_max = (uint16_t)(l > 0xFFFFu ? 0xFFFFu : l);
    if (w > fz_in_saves_max)
        fz_in_saves_max = (uint16_t)(w > 0xFFFFu ? 0xFFFFu : w);
}
#define FZ_IN(call) do { const uint32_t t0_ = plat_ms(), l0_ = fz_app.proj.pat_loads, s0_ = fz_app.proj.pat_saves; \
                         call; fz_in_done(t0_, l0_, s0_); } while (0)
void fz_key(int key, int down, int vel) { FZ_IN(qd_app_key(&fz_app, key, down, vel)); }
void fz_button(int button, int down) { FZ_IN(qd_app_button(&fz_app, button, down)); }
void fz_knob(int role, int detents) { FZ_IN(qd_app_knob(&fz_app, role, detents)); }
void fz_master(int pot) { qd_app_master(&fz_app, pot); }
void fz_midi_play(int status, int d1, int d2) { qd_app_midi_play(&fz_app, status, d1, d2); }
void fz_midi_ui(int status, int d1, int d2) { qd_app_midi_ui(&fz_app, status, d1, d2); }
/* (P2.7) the frame latched now (one instant: the strips drawn next are this one), the strips that would change */
static uint32_t fz_mask;
int fz_frame_changed(void)
{
    const int r = qd_app_frame_strips_changed(&fz_app, &fz_mask);
    const uint32_t b = qd_dl_kept() ? qd_dl_bytes() : 0u;
    if (b > fz_dl_max)
        fz_dl_max = b;
    fz_dl_over12k += b > 12288u - 64u;
    return r;
}
void fz_persist_step(void)
{
    if (fz_up)
        qd_persist_step(&fz_app);
}
void fz_show(int mode, int view) { qd_app_show(&fz_app, mode, view); }
void fz_seed(uint32_t seed) { qd_app_seed(&fz_app, seed); }
void fz_sysex(const uint8_t *f, uint32_t n) { qd_app_sysex(&fz_app, f, n); }
int fz_sysex_peek(void) { return qd_app_sysex_peek(&fz_app); }
int fz_sysex_take(uint8_t *out, uint32_t max) { return qd_app_sysex_take(&fz_app, out, max); }
void fz_tick(uint32_t ms) { qd_app_tick(&fz_app, ms); }
/* (P2.10, core 7fe3ea0) the clock in its two halves: what the audio side shares, under the lock, coalesced over the ms
 * since; then without the lock, the persist looks, the store's next job, SysEx's labels, a ms at a time */
void fz_tick_locked(uint32_t ms) { qd_app_tick_locked(&fz_app, ms); }
void fz_tick_free(void) { qd_app_tick_free(&fz_app); }
/* (the interrupt's paging: a pattern the sequencer wanted and the main loop's prefetch had not brought, loaded inside
 * the interrupt (a store's read, and a write if one left RAM changed): counted, it should stay 0) */
static volatile uint32_t fz_isr_loads;
/* (P2.8) the sound a control block at a time in the core's three parts (qd_app_audio's own qd_app_block, the same
 * calls and halves: the same samples), each timed: the interrupt's "rest" broken down. A call that is not whole blocks,
 * or meets the core's own frames made ahead, goes through qd_app_audio */
/* (P2.14) the second core (fm1-base's cpu2: plat_cpu2_*): each control block, voices 8..15 (the pool's second half) as
 * its job into bus B while core 0 renders 0..7 into bus A, then the join; a job it cannot take, or one dropped when it
 * was given up (cpu2_fallback), core 0 renders itself. The job writes only its voices' own state and its bus
 * (qd_app_block_voices) and its time; it runs from the flash, as fm1-base allows (core 0 never turns the flash off with
 * a job outstanding: the render waits for it) */
fz_c1_stats_t fz_c1s;
#if FM1B_CPU2
static struct {
    qd_bus_t *bus;
    int v0, v1;
    uint32_t t;                          /* (core 1's: the job's time) */
} fz_c1j;
static void fz_c1_job(uint32_t arg)
{
    const uint32_t t0 = plat_ticks24();
    (void)arg;
    qd_app_block_voices(&fz_app, fz_c1j.v0, fz_c1j.v1, fz_c1j.bus);
    fz_c1j.t = plat_ticks24() - t0;
}
#endif
#if FM1B_CPU2
/* (P2.15d, core 6c729a3) THE PIPELINE: each block, core 1 renders its share of block n's voices while core 0 renders
 * the rest and then the previous block's effects and output (qd_app_block_out of n - 1): the effects no longer wait
 * for the voices. One block (32 frames, 0.73 ms) more latency; the samples are qd_app_block_end's, one block later.
 * Two blocks in turn (fz_blk: the pool's, with this unit's .bss). With core 1 off or given up, the same order on core 0
 * alone. The split: as P2.14b, core 1 the larger half of the sounding voices */
static qd_blk_t fz_blk[2];
static uint8_t fz_blk_cur, fz_blk_have;          /* (the block to fill; a previous one to put out) */
static void fz_audio_piped(qd_app_t *a, int16_t *stereo, uint32_t nblk)
{
    uint32_t b, t0, t1, t2, t3, sb = 0, sv = 0, se = 0;
    for (b = 0; b < nblk; b++) {
        qd_blk_t *k = &fz_blk[fz_blk_cur], *prev = &fz_blk[fz_blk_cur ^ 1u];
        int16_t *out = stereo + 2u * QD_MIX_CTL * b;
        const int nv = a->pool.nvoice;
        int posted = 0, i, act = 0, n1 = 0, sp = nv;
        uint32_t to = 0;
        t0 = plat_ticks24();
        qd_app_block_begin_k(a, k);
        t1 = plat_ticks24();
        for (i = 0; i < nv; i++)
            act += a->pool.v[i].q.active != 0;
        /* (P2.16b) core 1 one sounding voice more than half (act / 2 + 1): core 0 also runs the previous block's out
         * (~1.6 voices' time on the unit: 793 us a half against ~470 a voice), so 4/4 of 8 left core 1 idle ~600 us and
         * core 0 the longer; 5/3 is the even pair. Odd counts unchanged (7: 4/3) */
        for (i = nv - 1; i >= 0 && n1 < (act ? (act / 2 + 1 < act ? act / 2 + 1 : act) : 0); i--) {
            n1 += a->pool.v[i].q.active != 0;
            sp = i;
        }
        if (plat_cpu2_on()) {
            fz_c1j.bus = &k->y, fz_c1j.v0 = sp, fz_c1j.v1 = nv;
            posted = plat_cpu2_run(fz_c1_job, 0);
        }
        qd_app_block_voices(a, 0, sp, &k->x);
        fz_c1s.c0_ticks += plat_ticks24() - t1;
        {   /* the previous block's effects and output, while core 1 renders */
            const uint32_t tq = plat_ticks24();
            if (fz_blk_have)
                qd_app_block_out(a, prev, out);
            else
                memset(out, 0, sizeof(int16_t) * 2u * QD_MIX_CTL);   /* (the very first: nothing yet) */
            to = plat_ticks24() - tq;
            se += to;
        }
        if (posted) {
            const uint32_t tw = plat_ticks24();
            const int rc = plat_cpu2_wait();
            const uint32_t w = plat_ticks24() - tw;
            if (w > fz_c1s.wait_max)
                fz_c1s.wait_max = w;
            fz_c1s.wait_ticks += w;
            fz_c1s.overlap_ticks += to;  /* (the effects' time core 0 spent while core 1 rendered: saved) */
            fz_c1s.c1_voices += (uint32_t)n1, fz_c1s.c0_voices += (uint32_t)(act - n1);
            if (rc) {
                qd_app_block_voices(a, sp, nv, &k->y);
                fz_c1s.fallbacks++;
            } else {
                fz_c1s.jobs++;
                fz_c1s.c1_ticks += fz_c1j.t;
            }
        } else {
            qd_app_block_voices(a, sp, nv, &k->y);
            if (plat_cpu2_on())
                fz_c1s.inline_jobs++;
        }
        t2 = plat_ticks24();
        qd_app_block_publish(a, k);
        t3 = plat_ticks24();
        sb += t1 - t0, sv += (t2 - t1) - to, se += t3 - t2;
        fz_blk_cur ^= 1u, fz_blk_have = 1;
    }
    fz_prof.a_begin += sb, fz_prof.a_vblk += sv, fz_prof.a_end += se;
    if (sb > fz_prof.a_begin_max)
        fz_prof.a_begin_max = sb;
    if (sv > fz_prof.a_vblk_max)
        fz_prof.a_vblk_max = sv;
    if (se > fz_prof.a_end_max)
        fz_prof.a_end_max = se;
    fz_prof.a_split_n++;
}
#endif
void fz_audio(int16_t *stereo, uint32_t frames)
{
    const uint32_t l = fz_app.proj.pat_loads;
#if FM1B_CPU2
    if (frames % QD_MIX_CTL == 0u && fz_app.ififo >= fz_app.nfifo) {
        fz_app.proj.nopage++;
        fz_audio_piped(&fz_app, stereo, frames / QD_MIX_CTL);
        fz_app.proj.nopage--;
    } else {
        qd_app_audio(&fz_app, stereo, frames);
    }
    if (fz_app.proj.pat_loads != l)
        fz_isr_loads += fz_app.proj.pat_loads - l;
    return;
#endif
    if (frames % QD_MIX_CTL == 0u && fz_app.ififo >= fz_app.nfifo) {
        qd_app_t *a = &fz_app;
        uint32_t b, t0, t1, t2, t3, sb = 0, sv = 0, se = 0;
        a->proj.nopage++;
        for (b = 0; b < frames / QD_MIX_CTL; b++) {
            qd_bus_t A, B;
            const int mid = a->pool.nvoice / 2;
            t0 = plat_ticks24();
            qd_app_block_begin(a);
            t1 = plat_ticks24();
#if FM1B_CPU2
            if (plat_cpu2_on()) {
                /* (P2.14b, the owner: the voices on core 1 first, the rest carried by core 0) the sounding voices split
                 * so core 1 has the larger half: it renders while core 0 renders the rest, so the interrupt waits for
                 * the longer of the two. Any split gives the same samples (integer sums) */
                const int nv = a->pool.nvoice;
                int posted, i, act = 0, k = 0, sp = nv;
                for (i = 0; i < nv; i++)
                    act += a->pool.v[i].q.active != 0;
                for (i = nv - 1; i >= 0 && k < (act + 1) / 2; i--) {
                    k += a->pool.v[i].q.active != 0;
                    sp = i;
                }
                fz_c1s.c1_voices += (uint32_t)k, fz_c1s.c0_voices += (uint32_t)(act - k);
                fz_c1j.bus = &B, fz_c1j.v0 = sp, fz_c1j.v1 = nv;
                posted = plat_cpu2_run(fz_c1_job, 0);
                qd_app_block_voices(a, 0, sp, &A);
                fz_c1s.c0_ticks += plat_ticks24() - t1;
                if (posted) {
                    const uint32_t tw = plat_ticks24();
                    const int rc = plat_cpu2_wait();
                    const uint32_t w = plat_ticks24() - tw;
                    if (w > fz_c1s.wait_max)
                        fz_c1s.wait_max = w;
                    fz_c1s.wait_ticks += w;
                    if (rc) {            /* (given up meanwhile: the job dropped, core 0's now) */
                        qd_app_block_voices(a, sp, nv, &B);
                        fz_c1s.fallbacks++;
                    } else {
                        fz_c1s.jobs++;
                        fz_c1s.c1_ticks += fz_c1j.t;
                    }
                } else {
                    qd_app_block_voices(a, sp, nv, &B);
                    fz_c1s.inline_jobs++;
                }
            } else
#endif
            {
                qd_app_block_voices(a, 0, mid, &A);
                qd_app_block_voices(a, mid, a->pool.nvoice, &B);
            }
            t2 = plat_ticks24();
            qd_app_block_end(a, &A, &B, stereo + 2u * QD_MIX_CTL * b);
            t3 = plat_ticks24();
            sb += t1 - t0, sv += t2 - t1, se += t3 - t2;
        }
        a->proj.nopage--;
        fz_prof.a_begin += sb, fz_prof.a_vblk += sv, fz_prof.a_end += se;
        if (sb > fz_prof.a_begin_max)    /* (P2.13: the longest half of each part: where a long interrupt went) */
            fz_prof.a_begin_max = sb;
        if (sv > fz_prof.a_vblk_max)
            fz_prof.a_vblk_max = sv;
        if (se > fz_prof.a_end_max)
            fz_prof.a_end_max = se;
        fz_prof.a_split_n++;
    } else {
        qd_app_audio(&fz_app, stereo, frames);
    }
    if (fz_app.proj.pat_loads != l)
        fz_isr_loads += fz_app.proj.pat_loads - l;
}
uint32_t fz_audio_loads(void) { return fz_isr_loads; }

/* P2.6: the audio interrupt over its time: one voice let go, as ChoralRoot's shed (its fade steal: a block's ramp to 0).
 * The one whose key is up first (a release tail), else the oldest; never one already fading. 1: one was */
int fz_shed_one(void)
{
    qd_pool_t *p = &fz_app.pool;
    int i, best = -1, best_up = 0;
    for (i = 0; i < p->nvoice; i++) {
        const qd_quad_voice_t *q = &p->v[i].q;
        const int up = !q->gate;
        if (!q->active || q->kill)
            continue;
        if (best < 0 || up > best_up || (up == best_up && p->v[i].age < p->v[best].age))
            best = i, best_up = up;
    }
    if (best < 0)
        return 0;
    p->v[best].pend = 0;
    qd_quad_voice_kill(&p->v[best].q);
    return 1;
}

/* P2.5: FIZZ's state for the stats (7D 46 50, line 4): what may make the sound or the store busy */
/* (P2.15c) bounded: a field that would not fit before st_lim is left out (st_cut), never written past it; st_need
 * counts what the line would take with every number at its widest (10 digits) */
static char *st_lim;
static uint32_t st_need, st_cut;
static char *st_u(char *q, const char *label, uint32_t v)
{
    char t[12];
    int n = 0;
    const uint32_t l = (uint32_t)strlen(label);
    st_need += l + 11u;
    if (q + l + 11u > st_lim) {
        st_cut = 1;
        return q;
    }
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
static char *st_store(char *q, const char *name, const qd_store_t *st)
{
    int k, blank = 0, erase = 0, used = 0;
    for (k = 0; k < st->nsec && k < QDS_MAXSEC; k++) {
        if (st->sseq[k] == 0)
            blank++;
        else if (st->sseq[k] == 0xFFFFFFFFu)
            erase++;
        else
            used++;
    }
    {   /* (its name as the next field's prefix, bounded as the fields are) */
        const uint32_t l = (uint32_t)strlen(name);
        st_need += l;
        if (q + l + 11u <= st_lim)
            while (*name)
                *q++ = *name++;
    }
    q = st_u(q, "_sec=", (uint32_t)st->nsec);
    q = st_u(q, "used=", (uint32_t)used);
    q = st_u(q, "blank=", (uint32_t)blank);
    q = st_u(q, "to_erase=", (uint32_t)erase);
    q = st_u(q, "keys=", (uint32_t)st->nkey);
    q = st_u(q, "busy=", (uint32_t)st->busy);
    return q;
}
uint32_t fz_state_line(char *q, char *lim)
{
    const qd_app_t *a = &fz_app;
    uint32_t live = 0, total = 0;
    st_lim = lim, st_need = 0, st_cut = 0;
    q = st_u(q, "voices=", (uint32_t)qd_pool_in_use(&a->pool));
    q = st_u(q, "of=", (uint32_t)a->pool.nvoice);         /* (the build's: 8 single-core, 16 Standard) */
    q = st_u(q, "playing=", (uint32_t)a->playing);
    q = st_u(q, "seq_playing=", (uint32_t)a->seq.playing);
    q = st_u(q, "pat_miss=", a->proj.pat_miss);
    q = st_u(q, "snd_miss=", a->proj.snd_miss);
    q = st_u(q, "sw_late=", a->seq.sw_late);
#if QD_SND_RAM
    q = st_u(q, "snd_uncached=", a->per.sc.uncached);
#endif
#if QD_PAT_RAM < QD_NPAT
    q = st_u(q, "pat_loads=", a->proj.pat_loads);
    q = st_u(q, "pat_saves=", a->proj.pat_saves);
#endif
    q = st_u(q, "in_ms_max=", fz_in_ms_max);           /* (2026-10-24: the longest input, the patterns it read in, wrote) */
    q = st_u(q, "in_loads_max=", fz_in_loads_max);
    q = st_u(q, "in_saves_max=", fz_in_saves_max);
    q = st_u(q, "fr_loads_max=", fz_fr_loads_max);     /* (the most patterns a frame read in; the longest frame) */
    q = st_u(q, "fr_ms_max=", fz_fr_ms_max);
    q = st_u(q, "store_on=", (uint32_t)a->per.on);
    if (a->per.on) {
        qd_persist_use(a, &live, &total);
        q = st_u(q, "pages_live=", live);
        q = st_u(q, "pages=", total);
        q = st_u(q, "written=", a->per.written);
        q = st_u(q, "dirty=", (uint32_t)qd_persist_dirty(a));
        q = st_u(q, "job=", (uint32_t)(a->per.job + 1));
        q = st_store(q, "lib", &a->per.lib_st);
        q = st_store(q, "prj", &a->per.prj_st);
    } else {                             /* (the widest line has them: counted all the same) */
        st_need += 5u * 21u + 2u * (3u + 6u * 21u);
    }
    *q = 0;
    return st_need + (st_cut ? 0x80000000u : 0u);
}
/* P2.4: a frame's time by its screen (fz_prof.h; the counters are app.c's, in RAM: this unit's .bss is the pool's) */
void fz_frame(fz_strip_fn out, void *ctx, int all_strips)
{
    const int sc = fz_screen_index();
    const uint32_t t0 = plat_ticks24(), m0 = plat_ms(), l0 = fz_app.proj.pat_loads;
    uint32_t t;
    {   /* (P2.7: the strips that change, from the frame latched by fz_frame_changed; all of them when forced) */
        const uint32_t n = (240u + QD_STRIP_H - 1u) / QD_STRIP_H, all = (1u << n) - 1u, m = all_strips ? all : fz_mask;
        uint32_t k;
        for (k = 0; k < n; k++)
            fz_strips_drawn += m >> k & 1u, fz_strips_kept += !(m >> k & 1u);
        qd_app_frame_strips_mask(&fz_app, 0, out, ctx, m);
    }
    t = plat_ticks24() - t0;
    if (fz_app.proj.pat_loads - l0 > fz_fr_loads_max)
        fz_fr_loads_max = (uint16_t)(fz_app.proj.pat_loads - l0 > 0xFFFFu ? 0xFFFFu : fz_app.proj.pat_loads - l0);
    if (plat_ms() - m0 > fz_fr_ms_max)
        fz_fr_ms_max = (uint16_t)(plat_ms() - m0 > 0xFFFFu ? 0xFFFFu : plat_ms() - m0);
    fz_screens[sc].n++;
    fz_screens[sc].sum += t;
    if (t > fz_screens[sc].max)
        fz_screens[sc].max = t;
}

void fz_ui_state(int *mode, int *playing)
{
    *mode = fz_app.ui.mode;
    *playing = fz_app.playing;
}
int fz_flush(void) { return fz_up ? qd_persist_flush(&fz_app) : 1; }

void fz_leds(uint8_t *key, uint8_t *button, uint8_t *play_green)
{
    qd_leds_t l;
    qd_app_leds(&fz_app, &l);
    memcpy(key, l.key, QD_NKEY);
    memcpy(button, l.button, QD_NBTN);
    *play_green = l.play_green;
}

int fz_expect(const char *what, int *got)
{
    const qd_app_t *a = &fz_app;
    if (!strcmp(what, "fz_up"))
        *got = fz_up;
    else if (!strcmp(what, "fz_mode"))                 /* 0 idle, 1 the step view, 2 keys, 3 tracks (ctl.h QD_MODE_*) */
        *got = a->ui.mode;
    else if (!strcmp(what, "fz_view"))                 /* 0 PIANO ROLL, 1 STEPS, 2 EDIT (ctl.h QD_VIEW_*) */
        *got = a->ui.view;
    else if (!strcmp(what, "fz_playing"))
        *got = a->playing;
    else if (!strcmp(what, "fz_bpm"))
        *got = a->bpm;
    else if (!strcmp(what, "fz_voices"))               /* voices sounding now */
        *got = qd_pool_in_use(&a->pool);
    else if (!strcmp(what, "fz_nvoice"))               /* the pool's voices (QD_VOICES_DEV) */
        *got = (uint32_t)a->pool.nvoice;
    else if (!strcmp(what, "fz_pattern"))
        *got = a->seq.cur;
    else if (!strcmp(what, "fz_sel"))
        *got = a->sel;
    else if (!strcmp(what, "fz_seqtick"))              /* the transport's position (seq.h, ticks) */
        *got = (int)qd_seq_tick(&a->seq);
    else if (!strcmp(what, "fz_store_on"))             /* the flash attached and mounted */
        *got = a->per.on;
    else if (!strcmp(what, "fz_prj_wiped"))            /* 2026-10-24: the projects' store erased for its format */
        *got = (int)a->per.prj_wiped;
    else if (!strcmp(what, "fz_lib_keys"))             /* the library's records (MY PRESETS, the factory tags) */
        *got = a->per.lib_st.nkey;
    else if (!strcmp(what, "fz_prj_keys"))             /* the projects' records */
        *got = a->per.prj_st.nkey;
    else if (!strcmp(what, "fz_store_written"))        /* records the store committed */
        *got = (int)a->per.written;
    else if (!strcmp(what, "fz_store_dirty"))          /* something not written yet */
        *got = qd_persist_dirty(a);
    else if (!strcmp(what, "fz_sx_frames"))            /* SysEx frames of FIZZ's taken */
        *got = (int)a->sx.frames;
    else
        return 0;
    return 1;
}

/* ---------------------------------------------------------------- the screen's stash (P2.7) --- */
void *fz_stash_borrow(uint32_t want, uint32_t keep, uint32_t *got)
{
    const uint32_t size = (uint32_t)qd_arena_size(), live = (uint32_t)qd_arena_live(), fails = qd_arena_fails();
    uint32_t n = size > live + keep + 64u ? size - live - keep - 64u : 0u;   /* (64: the blocks' heads, rounding) */
    void *p;
    int was;
    *got = 0;
    n = n < want ? n : want;
    if (!n)
        return 0;
    was = qd_arena_use(1);                       /* (the host at the device's sizes: the arena, as the unit) */
    p = qd_arena_malloc(n);
    qd_arena_use(was);
    if (p && qd_arena_fails() != fails) {        /* (the host's: a refusal answered by the C library) */
        free(p);
        p = 0;
    }
    if (p)
        *got = n;
    return p;
}
void fz_stash_return(void *p)
{
    if (p)
        qd_arena_free(p);
}
void fz_stash_on_release(void (*release)(void)) { qd_arena_set_release(release); }
int fz_test_lend(uint32_t n)
{
    void *p = qd_tmp_borrow(n);
    qd_tmp_return(p);
    return p != 0;
}

/* (P2.7) the display list (core qd_gfx_rec.c): kept, dropped (an arena borrow in a frame), its counters */
uint32_t fz_dl_max, fz_dl_over12k, fz_strips_drawn, fz_strips_kept;
int fz_dl_is_kept(void) { return qd_dl_kept(); }
void fz_dl_drop(void) { qd_dl_drop(); }
void fz_dl_counts(uint32_t *recorded, uint32_t *outgrown, uint32_t *max_bytes) { qd_dl_stats(recorded, outgrown, max_bytes); }
uint32_t fz_arena_free(void) { return (uint32_t)(qd_arena_size() - qd_arena_live()); }

/* (P2.7) the core's own timing (qd_quad_bench: one preset's voice alone, n control blocks), at boot, nothing else
 * running; and its statics' address (the pool or RAM: where the voices' data live) */
int fz_bench_shared_changed;             /* (the bench left something shared changed: a comb line held) */
int32_t fz_bench(int k, int note, int nblocks)
{
    const uint32_t h = qd_quad_shared_sig();
    const int32_t r = qd_quad_bench(k, note, nblocks);
    fz_bench_shared_changed = qd_quad_shared_sig() != h;
    return r;
}
/* (P2.10) quad_render alone, n blocks after the voice is struck (ChoralRoot's track_render_x span): ticks */
uint32_t fz_bench_render(int k, int note, int nblocks)
{
    uint32_t t0, i;
    int32_t sum = 0;
    qd_quad_bench_start(k, note);
    t0 = plat_ticks24();
    for (i = 0; i < (uint32_t)nblocks; i++)
        sum += qd_quad_bench_block(1);
    t0 = plat_ticks24() - t0;
    fz_bench_render_sum = sum;
    return t0;
}
int32_t fz_bench_render_sum;
uintptr_t fz_app_addr(void) { return (uintptr_t)&fz_app; }

#ifdef QD_GFX_FB
/* (P2.12) the 8-bit frame (core P2.6e): the port's rows attached, the core's palette, a frame drawn into them */
void fz_fb_attach(uint8_t *const rows[240]) { qd_app_fb_attach(&fz_app, rows); }
const uint16_t *fz_fb_palette(uint32_t *gen) { return qd_app_palette(&fz_app, gen); }
int fz_frame_fb(uint32_t *bands) { return qd_app_frame_fb(&fz_app, bands); }
int fz_frame_fb_begin(uint32_t *bands) { return qd_app_frame_fb_begin(&fz_app, bands); }   /* (P2.15: in steps) */
int fz_frame_fb_step(int n) { return qd_app_frame_fb_step(&fz_app, n); }
typedef char fz_fb_band_ok[QD_FB_BAND == 16 ? 1 : -1];   /* (app.c sends bands of 16 rows) */
#endif

/* (P2.12) the Geek Out pages' platform figures (core 74eb1df: qd_app_stats), filled by app.c; the store's use */
typedef char fz_st_bits_ok[QD_ST_LOAD0 == 1 && QD_ST_LOAD1 == 2 && QD_ST_FPS == 4 && QD_ST_ISR == 8 &&
                            QD_ST_LOCK == 16 && QD_ST_RAM == 32 && QD_ST_STORE == 64 && QD_ST_BOOT == 128 ? 1 : -1];
uint32_t fz_stats_fill(const fz_plat_stats_t *f)
{
    qd_app_stats_t *s = qd_app_stats(&fz_app);
    s->have = f->have;
    s->load_pm[0] = f->load_pm[0], s->load_pm[1] = f->load_pm[1];
    s->fps10 = f->fps10;
    s->frame_us = f->frame_us;
    s->isr_us = f->isr_us, s->isr_budget_us = f->isr_budget_us;
    s->late = f->late;
    s->lock_us = f->lock_us;
    s->ram_used = f->ram_used, s->ram_size = f->ram_size;
    s->store_used = f->store_used, s->store_size = f->store_size;
    s->boot = f->boot;
    return s->reset;
}
static void fz_st_count(const qd_store_t *st, uint32_t *used, uint32_t *n)
{
    int k;
    for (k = 0; k < st->nsec && k < QDS_MAXSEC; k++)
        *used += st->sseq[k] != 0 && st->sseq[k] != 0xFFFFFFFFu;
    *n += (uint32_t)st->nsec;
}
void fz_store_use(uint32_t *used, uint32_t *size)
{
    uint32_t u = 0, n = 0;
    if (fz_up) {
        fz_st_count(&fz_app.per.lib_st, &u, &n);
        fz_st_count(&fz_app.per.prj_st, &u, &n);
    }
    *used = u * 4096u, *size = n * 4096u;
}
int fz_screen_index(void)               /* (fz_screens' slot: mode x FZ_NVIEW + view; P2.15h: the Geek Out pages were
                                         * folded onto the first four views by "& 3") */
{
    return (fz_app.ui.mode & 3) * FZ_NVIEW + (fz_app.ui.view < FZ_NVIEW ? fz_app.ui.view : FZ_NVIEW - 1);
}
typedef char fz_nview_ok[QD_NVIEW <= FZ_NVIEW ? 1 : -1];
/* (P2.13) the store's region, for its backup and restore over USB (app.c): its flash offset and size */
void fz_store_region(uint32_t *off, uint32_t *size) { *off = PLAT_DATA_LO, *size = FZ_STORE_SECTORS * 4096u; }
