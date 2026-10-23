/* SPDX-License-Identifier: MPL-2.0 */
/* The second core's manager on the host (fz_dual.c), with stand-in voices until FIZZ's core has its split render:
 * the power-on rules and the record, the escalation, and the samples of a block rendered as "voices 0..7 on core 0,
 * 8..15 as core 1's job" equal to one core's, whichever runs first and whatever fails on the way.
 *   cc -O2 -Wall -I.. dual_test.c -o dual_test && ./dual_test
 *   cc -O2 -Wall -I.. -DFZ_SINGLE_CORE=1 dual_test.c -o dual_test1 && ./dual_test1 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../fz_dual.c"

uint32_t plat_ticks24(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)t.tv_sec * 24000000u + (uint32_t)t.tv_nsec / 1000u * 24u;
}

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

/* ---- stand-in voices: 16 of them, each with its own state, rendered into a bus of 32 frames */
#define NV 16
#define NF 32
typedef struct { float l[NF], r[NF]; } bus_t;
typedef struct { float ph, inc, amp, fb; } voice_t;
static voice_t vo[NV];
static bus_t bus_a, bus_b;

static void voices_init(void)
{
    int v;
    for (v = 0; v < NV; v++) {
        vo[v].ph = 0.01f * (float)v;
        vo[v].inc = 0.003f + 0.0007f * (float)v;
        vo[v].amp = 1.0f / (float)(v + 2);
        vo[v].fb = 0.0f;
    }
}
static void voices_render(int v0, int v1, bus_t *b)
{
    int v, i;
    memset(b, 0, sizeof *b);
    for (v = v0; v < v1; v++)
        for (i = 0; i < NF; i++) {
            float x = vo[v].ph - (float)(int)vo[v].ph, s = (x * 2.0f - 1.0f) * (1.0f - x * x) + vo[v].fb * 0.3f;
            vo[v].fb = s;
            vo[v].ph += vo[v].inc;
            b->l[i] += s * vo[v].amp;
            b->r[i] += s * vo[v].amp * (float)(v & 3) * 0.25f;
        }
}
static void job_b(uint32_t arg) { (void)arg; voices_render(8, 16, &bus_b); }
/* a block: the shared part (stand-in: each voice's amp glides), B handed over, A here, the join, A + B in that order */
static void block(int16_t *out, int dual)
{
    int v, i, posted;
    for (v = 0; v < NV; v++)
        vo[v].amp *= 0.999f;
    posted = dual ? fz_dual_post(job_b, 0) : 0;
    voices_render(0, 8, &bus_a);
    if (dual)
        fz_dual_join(posted, job_b, 0);
    else
        job_b(0);
    for (i = 0; i < NF; i++) {
        float l = bus_a.l[i] + bus_b.l[i], r = bus_a.r[i] + bus_b.r[i];
        out[2 * i] = (int16_t)(l * 8000.0f);
        out[2 * i + 1] = (int16_t)(r * 8000.0f);
    }
}
#define NB 400
static int16_t ref[NB][2 * NF], got[NB][2 * NF];
static void render(int16_t (*o)[2 * NF], int dual, int fault_at, int what)
{
    int b;
    voices_init();
    for (b = 0; b < NB; b++) {
        if (b == fault_at) {
            if (what == 1) fz_c1_sim_hang = 1;
            if (what == 2) fz_c1_sim_misread = 1;
            if (what == 3) fz_c1_sim_misread = 2;
            if (what == 4) fz_c1_sim_late = 1;
        }
        if (what == 3 && b == fault_at + 50)
            fz_c1_sim_misread = 1;              /* (a second misread, a later block) */
        block(o[b], dual);
    }
}
static void sim_reset(void) { fz_c1_sim_late = fz_c1_sim_hang = fz_c1_sim_misread = fz_c1_sim_nostart = 0; }
static fz_c1_bar_t rec;
static void boot(int safe, int counted, int wdt, int want)
{
    memset(&fz_dual, 0, sizeof fz_dual);
    fz_dual_boot(&rec, &fz_c1_simulated, safe, counted, wdt, want);
}

int main(void)
{
    memset(&rec, 0xA5, sizeof rec);             /* a power-on's garbage */
    render(ref, 0, -1, 0);

    /* ---- power-on */
    sim_reset();
    boot(0, 0, 0, 1);
#if FZ_SINGLE_CORE
    CHECK(fz_dual.state == FZ_C1_OFF);
    CHECK(!strcmp(fz_dual_state_name(), "single"));
    render(got, 1, -1, 0);
    CHECK(!memcmp(ref, got, sizeof ref));
    CHECK(fz_dual.posts == 0 && fz_dual.inline_jobs == NB);
    rec.barred = FZ_C1_BAR_CRASH;               /* (a Standard package's record: cleared, never read) */
    boot(0, 0, 0, 1);
    CHECK(fz_dual.state == FZ_C1_OFF && fz_dual.barred == FZ_C1_BAR_NONE && rec.barred == 0);
    printf("dual_test (single-core): %s\n", fails ? "FAILED" : "ok");
    return fails != 0;
#else
    CHECK(fz_dual.state == FZ_C1_OK && rec.magic == FZ_C1_BAR_MAGIC && rec.ran == 1 && rec.barred == 0);
    boot(0, 0, 0, 0);
    CHECK(fz_dual.state == FZ_C1_OFF);          /* the setting off */
    boot(1, 0, 0, 1);
    CHECK(fz_dual.state == FZ_C1_OFF);          /* safe mode */
    boot(0, 1, 0, 1);
    CHECK(fz_dual.state == FZ_C1_FAILED && rec.barred == 0);   /* the boot before failed: one core, nothing written */
    fz_c1_sim_nostart = 1;
    boot(0, 0, 0, 1);
    CHECK(fz_dual.state == FZ_C1_FAILED && rec.ran == 0);
    sim_reset();
    boot(0, 0, 0, 1);
    boot(0, 0, 1, 1);                           /* the watchdog reset the unit while core 1 ran: a hang */
    CHECK(fz_dual.state == FZ_C1_OFF && fz_dual.barred == FZ_C1_BAR_HANG && rec.ran == 0);
    boot(0, 0, 1, 1);                           /* (that boot ran on one core: the next watchdog reset is not core 1's) */
    CHECK(fz_dual.state == FZ_C1_OK);
    fz_dual_crashed(&rec);                      /* a crash beside core 1 */
    CHECK(rec.barred == FZ_C1_BAR_CRASH);
    boot(0, 0, 0, 1);
    CHECK(fz_dual.state == FZ_C1_OFF && fz_dual.barred == FZ_C1_BAR_CRASH);
    boot(0, 0, 0, 1);                           /* a soft reset after that: tried again */
    CHECK(fz_dual.state == FZ_C1_OK);

    /* ---- the samples: core 1's job first (quick), last (late), and every failure: one core's, bit for bit */
    render(got, 1, -1, 0);
    CHECK(!memcmp(ref, got, sizeof ref));
    CHECK(fz_dual.posts == NB && fz_dual.waits == NB && fz_dual.inline_jobs == 0);
    boot(0, 0, 0, 1);
    render(got, 1, 0, 4);
    CHECK(!memcmp(ref, got, sizeof ref));
    sim_reset();

    boot(0, 0, 0, 1);                           /* a stall: given up, core 0 renders 8..15 from that block on */
    render(got, 1, 123, 1);
    CHECK(!memcmp(ref, got, sizeof ref));
    CHECK(fz_dual.state == FZ_C1_GAVE_UP && fz_dual.stalls == 1 && fz_dual.posts == 124 &&
          fz_dual.inline_jobs == NB - 123 && rec.barred == FZ_C1_BAR_GAVE_UP);
    sim_reset();
    boot(0, 0, 0, 1);
    CHECK(fz_dual.state == FZ_C1_OFF && fz_dual.barred == FZ_C1_BAR_GAVE_UP);   /* and the next boot keeps it off */

    boot(0, 0, 0, 1);                           /* one misread: refused, counted, that block core 0's, core 1 kept */
    render(got, 1, 200, 2);
    CHECK(!memcmp(ref, got, sizeof ref));
    CHECK(fz_dual.state == FZ_C1_OK && fz_dual.misreads == 1 && fz_dual.refusals == 1 && fz_dual.inline_jobs == 1);
    sim_reset();

    boot(0, 0, 0, 1);                           /* a misread in a later block: given up */
    render(got, 1, 100, 3);
    CHECK(!memcmp(ref, got, sizeof ref));
    CHECK(fz_dual.state == FZ_C1_GAVE_UP && fz_dual.misreads == 2 && rec.barred == FZ_C1_BAR_GAVE_UP);
    sim_reset();

    printf("dual_test: %s (posts %u, waits %u, wait max %u ticks)\n", fails ? "FAILED" : "ok", fz_dual.posts,
           fz_dual.waits, fz_dual.wait_max_ticks);
    return fails != 0;
#endif
}
