/* SPDX-License-Identifier: MPL-2.0 */
/* FIZZ's second core, the manager (P2.6; firmware/DUALCORE.md): one job a control block (the voices 8..15 of FIZZ's
 * split render, once the core has it), its hand-over and its wait, the fallback, the escalation, the "core 1 off"
 * record that survives a reset, the counters. Plain C: the unity build (app.c) and the host test include it.
 *
 * The core 1 itself is a back end behind fz_c1_backend_t: today the SIMULATED one (core 0 runs the job, through the
 * same mailbox, check word and sequence a real core 1 will read: the protocol and the bit-exactness are proven before
 * core 1 exists); tomorrow felucca-bsp's chip/fm1_cpu1.* (issue #5, the owner's decision of 2026-10-23: wait for it),
 * handed to fz_dual_boot as its back end. FZ_SINGLE_CORE 1 (the single-core package): core 1 is never started, whatever
 * else, and every job runs on core 0 at once.
 *
 * The escalation, the record's decision and the power-on gating are from ChoralRoot's cr_cpu1.h, Michael P Jones,
 * relicensed MPL-2.0 by its author for FIZZ, 2026-10-23 (its design after Melodee PR 21, Kerem Kilic, credited there).
 * Nothing here is from felucca-bsp's, fm1-x0x's, Melodee's or Jangada's fm1_cpu1.* / fm1_multicore.h. */
#pragma once
#include <stdint.h>

#ifndef FZ_SINGLE_CORE
#define FZ_SINGLE_CORE 0
#endif

enum { FZ_C1_OFF, FZ_C1_OK, FZ_C1_FAILED, FZ_C1_GAVE_UP };          /* core 1 this session */
enum { FZ_C1_BAR_NONE, FZ_C1_BAR_CRASH, FZ_C1_BAR_HANG, FZ_C1_BAR_GAVE_UP };   /* why the record keeps it off */

typedef void (*fz_job_fn)(uint32_t arg);

/* a core 1: start (0 answered), hand a job over (1 handed), wait for it (0 done, -1 stalled, -2 refused / faulted),
 * hold it for good; its counters (requests it read wrong and refused, faults about it) */
typedef struct {
    const char *name;
    int (*start)(void);
    int (*run)(fz_job_fn fn, uint32_t arg);
    int (*wait)(void);
    void (*hold)(void);
    uint32_t (*rejected)(void);
    uint32_t (*faults)(void);
} fz_c1_backend_t;
extern const fz_c1_backend_t fz_c1_simulated;   /* core 0 runs the job, through the mailbox (fz_dual.c) */

/* the record (.noinit on the unit): kept across a reset, not a power cycle */
typedef struct {
    uint32_t magic, ran, barred;
} fz_c1_bar_t;
#define FZ_C1_BAR_MAGIC 0x5A314241u              /* "AB1Z" */

typedef struct {
    uint8_t state;                               /* FZ_C1_* */
    uint8_t barred;                              /* FZ_C1_BAR_*: why this boot kept it off, or gave it up */
    uint8_t misreads;                            /* blocks that found new misreads (the first is forgiven) */
    uint32_t posts, waits, inline_jobs;          /* jobs handed over, waited for, run by core 0 instead */
    uint32_t stalls, refusals, wait_max_ticks;
    uint32_t start_ticks;
} fz_dual_stats_t;
extern fz_dual_stats_t fz_dual;

/* once a boot, after the record is readable and before the sound: safe (FIZZ's safe mode), counted (the boot guard
 * counted the boot before: a crash's or the watchdog's reset within its window), wdt (this reset was the watchdog's),
 * want (the setting: 1 dual) */
void fz_dual_boot(fz_c1_bar_t *rec, const fz_c1_backend_t *be, int safe, int counted, int wdt, int want);
/* a crash beside a running core 1 (APP_FAULT): the record says so for the next boot */
void fz_dual_crashed(fz_c1_bar_t *rec);
/* each control block: hand the job over (1) or not (0: the caller runs it itself, now or after its own part) */
int fz_dual_post(fz_job_fn fn, uint32_t arg);
/* after core 0's own part: 0 the job is done; else core 1 was given up this block and the caller runs the job itself */
int fz_dual_wait(void);
/* the join, the usual way: posted = fz_dual_post(fn, arg); core 0's own part; fz_dual_join(posted, fn, arg): the job
 * is done after it, by core 1 or by core 0 (its output is its own buffer, so the order of the two parts is free) */
void fz_dual_join(int posted, fz_job_fn fn, uint32_t arg);
int fz_dual_active(void);                        /* core 1 takes jobs (FZ_C1_OK) */
const char *fz_dual_state_name(void);
/* core 1's side of the mailbox: one request if a new one is there (a FIZZ core-1 idle loop; the simulated core) */
void fz_c1_serve(void);

/* the simulated back end's test hooks (the host test, the simulator): run the job at the hand-over (0, as a core 1
 * that is quick) or at the wait (1); the next waits stall; misread the next requests */
extern uint8_t fz_c1_sim_late, fz_c1_sim_hang, fz_c1_sim_misread, fz_c1_sim_nostart;
