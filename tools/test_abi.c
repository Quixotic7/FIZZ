/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.3i: FIZZ (P1.3k: was QUADRILLE) driven through the core ABI, as FM1VST's host drives it: the built fizz.fm1core
 * loaded with dlopen, fm1core_get, the panel written between entry points (buttons by their labels' matrix ids, the
 * knobs as detents on enc[enc_id[role]] in enc_dir[role]), tick each millisecond, frame every 15, audio in 128-frame
 * blocks, MIDI real-time bytes through midi_in. The tempo is read from ui_info ("..., 120 BPM, ...").
 *
 *   F3 latched (OCT- and OCT+ down together, then up) + the SELECT role (P1.3x; was ALGORITHM): the tempo moves
 *   (ALGORITHM is the swing now); after a MIDI START and STOP the tempo turns again; while a MIDI clock runs the turn is
 *   the clock's (EXT CLOCK), and when the clock stops arriving the tempo is the knob's again.
 *
 *     test_abi path/to/fizz.fm1core      exit 1 on a failed check
 * It needs FM1VST's core-api (FM1CORE_API, tools/Makefile) and the built module: make -C tools abi. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fm1core.h"

static const fm1core_t *C;
static fm1panel_t *P;
static uint32_t now;
static uint64_t frames_done;
static int fails, checks;
#define CHECK(c, ...)                                     \
    do {                                                  \
        checks++;                                         \
        if (!(c)) {                                       \
            fails++;                                      \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

static void run(uint32_t n)
{
    while (n--) {
        now++;
        C->tick(now);
        if (now % 15u == 0u)
            C->frame();
        while (frames_done + 128u <= (uint64_t)now * FM1P_FS / 1000u) {
            int16_t blk[256];
            C->audio(blk, 128);
            frames_done += 128u;
        }
    }
}
static void bdown(int label) { P->buttons |= 1u << P->btn_id[label]; }
static void bup(int label) { P->buttons &= ~(1u << P->btn_id[label]); }
static void btap(int label)
{
    bdown(label);
    run(30);
    bup(label);
    run(30);
}
static void turn(int role, int d)
{
    P->enc[P->enc_id[role]] += d * P->enc_dir[role];
    run(10);
}
static int bpm(void)
{
    char info[512];
    const char *b;
    C->ui_info(info, sizeof info);
    b = strstr(info, " BPM");
    while (b && b > info && b[-1] >= '0' && b[-1] <= '9')
        b--;
    return b ? atoi(b) : -1;
}
static void midi_rt(int status) { C->midi_in(15u | (uint32_t)status << 8); }
static void f3_open(void)
{
    bdown(FM1P_B_OCTDN);
    run(20);
    bdown(FM1P_B_OCTUP);
    run(20);
    bup(FM1P_B_OCTUP);
    bup(FM1P_B_OCTDN);
    run(30);
}

int main(int argc, char **argv)
{
    void *h;
    const fm1core_t *(*get)(uint32_t);
    int b0, k;
    if (argc < 2) {
        fprintf(stderr, "usage: test_abi fizz.fm1core\n");
        return 2;
    }
    h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        printf("test_abi: %s\n", dlerror());
        return 1;
    }
    get = (const fm1core_t *(*)(uint32_t))dlsym(h, FM1CORE_SYMBOL);
    C = get ? get(FM1CORE_ABI) : NULL;
    if (!C) {
        printf("test_abi: no fm1core_get for ABI %u\n", FM1CORE_ABI);
        return 1;
    }
    P = C->panel;
    P->ready = 2;                                       /* simulated time */
    C->options(NULL, 1, 0, 1);
    C->init(0);
    C->tick(0);
    btap(FM1P_B_HOME);                                  /* wake: the step view */
    printf("test_abi: %s %s, ALGORITHM is encoder %d (dir %+d)\n", C->name, C->version, P->enc_id[FM1P_E_ALGO],
           P->enc_dir[FM1P_E_ALGO]);

    f3_open();
    b0 = bpm();
    turn(FM1P_E_SELECT, 5);
    CHECK(bpm() == b0 + 5, "F3 latched, the SELECT role +5 (P1.3x): the tempo %d -> %d", b0, bpm());
    btap(FM1P_B_OCTDN);                                 /* (F3 closed) */

    midi_rt(0xFA);                                      /* a MIDI START with no clock after it (a DAW's transport) */
    run(50);
    midi_rt(0xFC);
    run(50);
    f3_open();
    b0 = bpm();
    turn(FM1P_E_SELECT, 3);
    CHECK(bpm() == b0 + 3, "after a MIDI START and STOP: SELECT still moves the tempo (%d -> %d)", b0, bpm());
    btap(FM1P_B_OCTDN);

    midi_rt(0xFA);                                      /* a START left running, no clock: ext must not stick */
    run(800);
    f3_open();
    b0 = bpm();
    turn(FM1P_E_SELECT, 2);
    CHECK(bpm() == b0 + 2, "a START with no clock for 800 ms: not external any more, the tempo moves (%d -> %d)", b0,
          bpm());
    btap(FM1P_B_OCTDN);

    midi_rt(0xFA);                                      /* a real clock at 100 BPM (24 PPQN: 25 ms a pulse) */
    for (k = 0; k < 96; k++) {
        midi_rt(0xF8);
        run(25);
    }
    f3_open();
    b0 = bpm();
    turn(FM1P_E_SELECT, 4);
    CHECK(b0 >= 98 && b0 <= 102 && bpm() == b0, "a running clock (100 BPM): the clock's tempo (%d), a turn does not "
          "move it (%d)", b0, bpm());
    run(700);                                           /* the clock stops arriving: ext ends by itself */
    b0 = bpm();
    turn(FM1P_E_SELECT, 4);
    CHECK(bpm() == b0 + 4, "no clock for 700 ms: the knob's tempo again (%d -> %d)", b0, bpm());
    btap(FM1P_B_OCTDN);

    C->shutdown();

    {   /* P1.5a, the restart: a flash FILE (as FM1VST's <core-id>/flash.bin); the tempo (the pattern's) and the selected
         * track changed, written by themselves; shutdown; a new power-on from the file has them */
        const char *path = "build/test_abi_flash.bin";
        char info[512];
        int b1, t1;
        remove(path);
        C->options(path, 0, 0, 1);
        C->init(0);
        run(10);
        btap(FM1P_B_HOME);
        f3_open();
        turn(FM1P_E_SELECT, 7);
        b1 = bpm();
        btap(FM1P_B_OCTDN);
        bdown(FM1P_B_FX);                           /* FX held + ALGORITHM +3: track 4 */
        run(20);
        turn(FM1P_E_ALGO, 3);
        bup(FM1P_B_FX);
        run(2500);                                      /* (written by itself: 1.5 s after the last edit) */
        C->ui_info(info, sizeof info);
        t1 = strstr(info, "track ") ? atoi(strstr(info, "track ") + 6) : -1;
        C->shutdown();
        C->options(path, 0, 0, 1);
        C->init(0);
        run(100);
        C->ui_info(info, sizeof info);
        CHECK(bpm() == b1 && t1 == 4 && strstr(info, "track 4 "), "a restart from the flash file: the tempo %d (was %d) "
              "and track 4 (%s)", bpm(), b1, info);
        C->shutdown();
    }
    printf("test_abi: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
