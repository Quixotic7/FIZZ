/* SPDX-License-Identifier: MPL-2.0 */
/* FIZZ's second core, the manager (fz_dual.h; firmware/DUALCORE.md). Included by app.c (the unity build: the unit,
 * the simulator, the browser emulator) and by tests/dual_test.c (the host test). Wants plat_ticks24 (the wait's time).
 *
 * The escalation (fz_dual_assess), the record's decision (fz_dual_bar_decide) and the power-on gating
 * (fz_dual_boot): from ChoralRoot's cr_cpu1.h, Michael P Jones, relicensed MPL-2.0 by its author for FIZZ, 2026-10-23
 * (cr_c1_assess, cr_c1_give_up, cr_c1_bar_decide, cr_c1_bar_boot, cr_c1_power_on; their design after Melodee PR 21,
 * Kerem Kilic). The mailbox, the check word and the simulated core are FIZZ's. */
#include "fz_dual.h"

uint32_t plat_ticks24(void);

fz_dual_stats_t fz_dual;
static const fz_c1_backend_t *fz_c1;           /* the core 1 that boot started; 0 = none */
static fz_c1_bar_t *fz_c1_rec;                 /* the record, for the give-up */
static uint32_t fz_c1_seen;                    /* its misread count at the last look */

/* ------------------------------------------------ the escalation (cr_c1_assess, cr_c1_give_up) --- */
/* what core 1's counters say: 0 = keep it, -1 = give it up. New misreads in a first block: counted, kept (one block's
 * burst is one event); new misreads in a later block, or any fault or stall: give up */
static int fz_dual_assess(uint32_t rejected, uint32_t faults, int stalled)
{
    if (faults || stalled)
        return -1;
    if (rejected == fz_c1_seen)
        return 0;
    fz_c1_seen = rejected;
    return ++fz_dual.misreads > 1u ? -1 : 0;
}
static void fz_dual_give_up(void)
{
    if (fz_c1 && fz_c1->hold)
        fz_c1->hold();
    fz_dual.state = FZ_C1_GAVE_UP;
    fz_dual.barred = FZ_C1_BAR_GAVE_UP;
    if (fz_c1_rec)
        fz_c1_rec->barred = FZ_C1_BAR_GAVE_UP;  /* (a reset now: still off next boot) */
}

/* ------------------------------------------------ the record (cr_c1_bar_decide) --- */
/* the next boot's reading of the record and of its own reset: not the record (a power-on's garbage) -> none; a reason
 * left (a crash beside core 1, a give-up) -> that; core 1 ran and the watchdog reset the unit -> a hang; any other
 * reset (a reboot, an update) -> none */
static uint32_t fz_dual_bar_decide(uint32_t magic, uint32_t ran, uint32_t barred, int wdt)
{
    if (magic != FZ_C1_BAR_MAGIC)
        return FZ_C1_BAR_NONE;
    if (barred >= FZ_C1_BAR_CRASH && barred <= FZ_C1_BAR_GAVE_UP)
        return barred;
    if (ran && wdt)
        return FZ_C1_BAR_HANG;
    return FZ_C1_BAR_NONE;
}

/* ------------------------------------------------ power-on (cr_c1_bar_boot + cr_c1_power_on + cr_c1_boot) --- */
void fz_dual_boot(fz_c1_bar_t *rec, const fz_c1_backend_t *be, int safe, int counted, int wdt, int want)
{
    uint32_t d = fz_dual_bar_decide(rec->magic, rec->ran, rec->barred, wdt), t0;
    rec->ran = rec->barred = 0;
    rec->magic = FZ_C1_BAR_MAGIC;
    fz_c1_rec = rec;
    fz_c1 = 0;
    fz_c1_seen = 0;
    fz_dual.state = FZ_C1_OFF;
    fz_dual.barred = FZ_C1_BAR_NONE;
    fz_dual.misreads = 0;
#if FZ_SINGLE_CORE                             /* the single-core package: never, whatever the record says */
    (void)d; (void)be; (void)safe; (void)counted; (void)want; (void)t0;
    return;
#else
    if (!want || safe || !be)                  /* the setting off; FIZZ's safe mode; no core 1 in this build */
        return;
    if (d != FZ_C1_BAR_NONE) {                 /* a crash or a hang beside core 1 last time, or a give-up */
        fz_dual.barred = (uint8_t)d;
        return;
    }
    if (counted) {                             /* the boot before this one failed (crash or watchdog within the
                                                * guard's window, a hang in the start itself included): one core this
                                                * boot, writing nothing; the next power-on tries again */
        fz_dual.state = FZ_C1_FAILED;
        return;
    }
    t0 = plat_ticks24();
    if (be->start()) {                         /* not answering (20 ms, the back end's): held for the session */
        if (be->hold)
            be->hold();
        fz_dual.state = FZ_C1_FAILED;
    } else {
        fz_c1 = be;
        rec->ran = 1;
        fz_dual.state = FZ_C1_OK;
    }
    fz_dual.start_ticks = plat_ticks24() - t0;
#endif
}

void fz_dual_crashed(fz_c1_bar_t *rec)
{
    if (rec && rec->magic == FZ_C1_BAR_MAGIC && fz_dual.state == FZ_C1_OK) {
        rec->barred = FZ_C1_BAR_CRASH;
        if (fz_c1 && fz_c1->hold)
            fz_c1->hold();
    }
}

/* ------------------------------------------------ each block (cr_c1_check, cr_c1_wait) --- */
static uint8_t fz_dual_posted;

int fz_dual_post(fz_job_fn fn, uint32_t arg)
{
    fz_dual_posted = 0;
    if (fz_dual.state != FZ_C1_OK)
        return 0;
    if (fz_dual_assess(fz_c1->rejected(), fz_c1->faults(), 0) < 0) {
        fz_dual_give_up();
        return 0;
    }
    if (!fz_c1->run(fn, arg))
        return 0;
    fz_dual.posts++;
    fz_dual_posted = 1;
    return 1;
}

int fz_dual_wait(void)
{
    uint32_t t0, d;
    int rc;
    if (!fz_dual_posted)
        return -1;
    fz_dual_posted = 0;
    t0 = plat_ticks24();
    rc = fz_c1->wait();
    d = plat_ticks24() - t0;
    fz_dual.waits++;
    if (d > fz_dual.wait_max_ticks)
        fz_dual.wait_max_ticks = d;
    if (!rc)
        return 0;
    if (rc == -1)
        fz_dual.stalls++;
    else
        fz_dual.refusals++;
    if (fz_dual_assess(fz_c1->rejected(), fz_c1->faults(), rc == -1) < 0)
        fz_dual_give_up();                     /* a stall, a fault, a second misread: core 0 from now on */
    return rc;                                 /* (a first misread: core 1 kept; this block's job is core 0's) */
}

void fz_dual_join(int posted, fz_job_fn fn, uint32_t arg)
{
    if (posted && !fz_dual_wait())
        return;
    fn(arg);
    fz_dual.inline_jobs++;
}

int fz_dual_active(void) { return fz_dual.state == FZ_C1_OK; }

const char *fz_dual_state_name(void)
{
    static const char *const s[] = {"off", "on", "failed", "gave up"};
    return FZ_SINGLE_CORE ? "single" : s[fz_dual.state & 3u];
}

/* ------------------------------------------------ the mailbox and the simulated core 1 --- */
/* core 0 writes its line (the request: a sequence number, the job, its argument, a check word over the three), core
 * 1 its own (the answer: the sequence it finished, its status, its counters); 64-byte lines, never shared, so a real
 * core 1 and core 0 never write the same cache line. A request read with a wrong check word is refused and counted,
 * never run */
#define FZ_MB_KEY 0x46495A5Au                   /* "ZZIF" */
typedef struct {
    volatile uint32_t seq, fn, arg, check;      /* core 0's */
    fz_job_fn job;                              /* (the job itself; fn is its address's low word, for the check) */
    uint32_t pad0[16 - 4 - sizeof(fz_job_fn) / 4];
    volatile uint32_t done, status, rejected, faults;   /* core 1's */
    uint32_t pad1[12];
} fz_mbox_t;
static fz_mbox_t fz_mbox __attribute__((aligned(64)));

static uint32_t fz_mb_check(uint32_t seq, uint32_t fn, uint32_t arg)
{
    uint32_t h = seq * 0x9E3779B1u ^ fn ^ (arg << 7 | arg >> 25) ^ FZ_MB_KEY;
    return h ^ h >> 15;
}
/* core 1's side: one request if there is a new one (a real core 1's idle loop calls this; the simulated one, core 0) */
void fz_c1_serve(void)
{
    uint32_t seq = fz_mbox.seq;
    if (seq == fz_mbox.done)
        return;
    if (fz_mbox.check != fz_mb_check(seq, fz_mbox.fn, fz_mbox.arg) ||
        (uint32_t)(uintptr_t)fz_mbox.job != fz_mbox.fn) {
        fz_mbox.rejected++;
        fz_mbox.status = 2;
    } else {
        fz_mbox.job(fz_mbox.arg);
        fz_mbox.status = 0;
    }
    fz_mbox.done = seq;
}

uint8_t fz_c1_sim_late, fz_c1_sim_hang, fz_c1_sim_misread, fz_c1_sim_nostart;

static int fz_sim_start(void)
{
    fz_mbox.seq = fz_mbox.done = 0;
    fz_mbox.rejected = fz_mbox.faults = 0;
    return fz_c1_sim_nostart ? -1 : 0;
}
static int fz_sim_run(fz_job_fn fn, uint32_t arg)
{
    uint32_t seq = fz_mbox.seq + 1u;
    fz_mbox.job = fn;
    fz_mbox.fn = (uint32_t)(uintptr_t)fn;
    fz_mbox.arg = arg;
    fz_mbox.check = fz_mb_check(seq, fz_mbox.fn, arg) ^ (fz_c1_sim_misread ? 1u : 0u);
    if (fz_c1_sim_misread)
        fz_c1_sim_misread--;
    fz_mbox.seq = seq;                          /* (last: the request is whole when core 1 sees a new number) */
    if (!fz_c1_sim_late && !fz_c1_sim_hang)
        fz_c1_serve();                          /* a quick core 1: done before core 0's own part */
    return 1;
}
static int fz_sim_wait(void)
{
    if (fz_c1_sim_hang)
        return -1;                              /* (a real one: 8 ms of polling first) */
    fz_c1_serve();
    if (fz_mbox.done != fz_mbox.seq)
        return -1;
    return fz_mbox.status ? -2 : 0;
}
static void fz_sim_hold(void) {}
static uint32_t fz_sim_rejected(void) { return fz_mbox.rejected; }
static uint32_t fz_sim_faults(void) { return fz_mbox.faults; }

const fz_c1_backend_t fz_c1_simulated = {"simulated", fz_sim_start, fz_sim_run, fz_sim_wait, fz_sim_hold,
                                         fz_sim_rejected, fz_sim_faults};
