/* SPDX-License-Identifier: MPL-2.0 */
/* FIZZ's side of the seam (fizz_port.h): the core (core/app) driven as FM1VST's glue drives it (core/glue/
 * core_quadrille.c), from fm1-base's main loop. A unit of its own (fm1.toml o2), built with FIZZ's device profile
 * (fizz_prefix.h: QD_DEVICE; on the unit QD_TARGET_FM1). Its .bss (the one qd_app_t) goes to fm1-base's pool
 * (fizz.ld). */
#include <string.h>

#include "../../core/app/app.h"
#include "../../core/gfx/qd_gfx.h"
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
    (void)c;
    fz_watchdog();                       /* (a store's long flow is a loop of these: the watchdog fed at each) */
    return plat_data_erase(PLAT_DATA_LO + (off & ~0xFFFu));
}
static int fz_fprog(void *c, uint32_t off, const void *src, uint32_t n)
{
    (void)c;
    fz_watchdog();
    return plat_data_write(PLAT_DATA_LO + off, src, n);
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
        if (p && i < 4096u && !plat_data_erase(PLAT_DATA_LO + k * 4096u))
            n++;
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

void fz_key(int key, int down, int vel) { qd_app_key(&fz_app, key, down, vel); }
void fz_button(int button, int down) { qd_app_button(&fz_app, button, down); }
void fz_knob(int role, int detents) { qd_app_knob(&fz_app, role, detents); }
void fz_master(int pot) { qd_app_master(&fz_app, pot); }
void fz_midi_play(int status, int d1, int d2) { qd_app_midi_play(&fz_app, status, d1, d2); }
void fz_midi_ui(int status, int d1, int d2) { qd_app_midi_ui(&fz_app, status, d1, d2); }
int fz_frame_changed(void) { return qd_app_frame_changed(&fz_app); }
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
/* (the interrupt's paging: a pattern the sequencer wanted and the main loop's prefetch had not brought, loaded inside
 * the interrupt (a store's read, and a write if one left RAM changed): counted, it should stay 0) */
static volatile uint32_t fz_isr_loads;
void fz_audio(int16_t *stereo, uint32_t frames)
{
    const uint32_t l = fz_app.proj.pat_loads;
    qd_app_audio(&fz_app, stereo, frames);
    if (fz_app.proj.pat_loads != l)
        fz_isr_loads += fz_app.proj.pat_loads - l;
}
uint32_t fz_audio_loads(void) { return fz_isr_loads; }

/* P2.5: FIZZ's state for the stats (7D 46 50, line 4): what may make the sound or the store busy */
static char *st_u(char *q, const char *label, uint32_t v)
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
    while (*name)
        *q++ = *name++;
    q = st_u(q, "_sec=", (uint32_t)st->nsec);
    q = st_u(q, "used=", (uint32_t)used);
    q = st_u(q, "blank=", (uint32_t)blank);
    q = st_u(q, "to_erase=", (uint32_t)erase);
    q = st_u(q, "keys=", (uint32_t)st->nkey);
    q = st_u(q, "busy=", (uint32_t)st->busy);
    return q;
}
void fz_state_line(char *q)
{
    const qd_app_t *a = &fz_app;
    uint32_t live = 0, total = 0;
    q = st_u(q, "voices=", (uint32_t)qd_pool_in_use(&a->pool));
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
    }
    *q = 0;
}
/* P2.4: a frame's time by its screen (fz_prof.h; the counters are app.c's, in RAM: this unit's .bss is the pool's) */
void fz_frame(fz_strip_fn out, void *ctx)
{
    const int sc = (fz_app.ui.mode & 3) * 4 + (fz_app.ui.view & 3);
    const uint32_t t0 = plat_ticks24();
    uint32_t t;
    qd_app_frame_strips(&fz_app, 0, out, ctx);
    t = plat_ticks24() - t0;
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
    else if (!strcmp(what, "fz_pattern"))
        *got = a->seq.cur;
    else if (!strcmp(what, "fz_sel"))
        *got = a->sel;
    else if (!strcmp(what, "fz_seqtick"))              /* the transport's position (seq.h, ticks) */
        *got = (int)qd_seq_tick(&a->seq);
    else if (!strcmp(what, "fz_store_on"))             /* the flash attached and mounted */
        *got = a->per.on;
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
