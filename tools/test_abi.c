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
/* P1.6l: a SysEx frame (F0 .. F7) into midi_in as USB-MIDI packets; the replies out of midi_out_take, their bytes */
static void sx_in(const uint8_t *f, int n)
{
    int i;
    for (i = 0; i < n; i += 3) {
        const int k = n - i < 3 ? n - i : 3, end = i + k >= n;
        const uint32_t cin = end ? 4u + (uint32_t)k : 4u;
        C->midi_in(cin | (uint32_t)f[i] << 8 | (k > 1 ? (uint32_t)f[i + 1] << 16 : 0u) | (k > 2 ? (uint32_t)f[i + 2] << 24 : 0u));
    }
}
static int sx_out(uint8_t *r, int max)
{
    uint32_t pkt;
    int n = 0;
    while (C->midi_out_take(&pkt) && n < max - 3) {
        const uint32_t cin = pkt & 15u, nb = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : 1u;
        uint32_t j;
        for (j = 0; j < nb; j++)
            r[n++] = (uint8_t)(pkt >> (8u * (j + 1u)));
    }
    return n;
}
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

    {   /* P1.5r: SysEx through midi_in / midi_out_take: the identity (F0 7D 46 5A 01 01 7F F7 as USB-MIDI packets),
         * its reply "FIZZ <version>"; the panel's names */
        static const uint32_t ID[3] = {4u | 0xF0u << 8 | 0x7Du << 16 | 0x46u << 24,
                                       4u | 0x5Au << 8 | 0x01u << 16 | 0x01u << 24, 6u | 0x7Fu << 8 | 0xF7u << 16};
        uint8_t r[64];
        char want[64];
        uint32_t pkt;
        int n = 0, i, ok = 1;
        for (i = 0; i < 3; i++)
            ok &= C->midi_in(ID[i]) == 1;
        run(5);
        while (C->midi_out_take(&pkt) && n < 60) {
            const uint32_t cin = pkt & 15u, nb = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : 1u;
            uint32_t j;
            for (j = 0; j < nb; j++)
                r[n++] = (uint8_t)(pkt >> (8u * (j + 1u)));
        }
        snprintf(want, sizeof want, "FIZZ %s", C->version);
        CHECK(ok && n == 9 + (int)strlen(want) && r[0] == 0xF0 && r[5] == 0x41 && r[6] == 1 &&
                  !memcmp(r + 7, want, strlen(want)) && r[n - 1] == 0xF7,
              "SysEx IDENTITY through midi_in: the reply \"%s\" through midi_out_take (%d bytes)", want, n);
        CHECK(!strcmp(C->button_names[0], "FX") && !strcmp(C->button_names[3], "LATCH") &&
                  !strcmp(C->button_names[7], "SOUNDS") && !strcmp(C->button_names[12], "F1"),
              "the panel's names are today's (FX .. LATCH .. SOUNDS .. F1)");
    }

    {   /* P1.5s: EDIT follows the music when FIZZ's own PLAY (the panel's button) runs it: VEL, nothing held, the latch
         * off; ui_info's "edit follows N" is the step EDIT shows playing (0: the track's default) */
        char info[512];
        const char *e;
        int seen = 0, steps = 0, last = -1, i;
        btap(FM1P_B_HOME);
        while (strstr((C->ui_info(info, sizeof info), info), "view EDIT") == NULL && seen++ < 4) {
            bdown(FM1P_B_GLO);                          /* SHIFT + HOME: the next view */
            run(30);
            btap(FM1P_B_HOME);
            bup(FM1P_B_GLO);
            run(30);
        }
        P->keys |= 1u << 3;                             /* B2 (G#3): VEL */
        run(30);
        P->keys &= ~(1u << 3);
        run(30);
        C->ui_info(info, sizeof info);
        e = strstr(info, "edit follows ");
        CHECK(strstr(info, "view EDIT") && e && atoi(e + 13) == 0, "EDIT, VEL, stopped: the default (%s)", info);
        btap(FM1P_B_PLAY);                              /* the panel's PLAY */
        for (i = 0; i < 2000; i += 10) {
            run(10);
            C->ui_info(info, sizeof info);
            e = strstr(info, "edit follows ");
            if (e && atoi(e + 13) > 0 && atoi(e + 13) != last)
                last = atoi(e + 13), steps++;
        }
        CHECK(strstr(info, "playing") && steps >= 4, "PLAY on the panel: EDIT follows the steps playing (%d steps seen; "
              "%s)", steps, info);
        btap(FM1P_B_PLAY);
        run(50);
        C->ui_info(info, sizeof info);
        e = strstr(info, "edit follows ");
        CHECK(e && atoi(e + 13) == 0, "stopped: the default again");
    }

    {   /* P1.5z (the owner: "Using the Shift for numbers and stuff is not actually working"): the typewriter through the
         * panel as RemoteFM1 and the Standalone deliver it: SAVE PRESET, SHIFT + F1 clears; GLO held, then B1 and W1
         * (held keys, and taps); GLO and the key down in the same tick; the key before GLO lets go */
        char info[512];
        const char *q;
        btap(FM1P_B_HOME);
        btap(FM1P_B_SAVE);                              /* SAVE PRESET */
        run(100);
        bdown(FM1P_B_GLO);                              /* SHIFT + F1: cleared */
        run(30);
        btap(FM1P_B_OCTDN);
        bup(FM1P_B_GLO);
        run(30);
        bdown(FM1P_B_GLO);                              /* GLO held (1.2 s: well past its map's time), then B1 and W1 */
        run(1200);
        P->keys |= 1u << 1;
        run(30);
        P->keys &= ~(1u << 1);
        run(30);
        P->keys |= 1u << 0;
        run(30);
        P->keys &= ~(1u << 0);
        run(30);
        P->keys_tap |= 1u << 1;                         /* .. a tap (press and release between two ticks) */
        run(30);
        bup(FM1P_B_GLO);
        run(30);
        P->keys |= 1u << 3;                             /* B2 alone: B */
        run(30);
        P->keys &= ~(1u << 3);
        run(30);
        P->buttons |= 1u << P->btn_id[FM1P_B_GLO];      /* GLO and B1 down in the same tick */
        P->keys |= 1u << 1;
        run(30);
        P->keys &= ~(1u << 1);
        run(10);
        bup(FM1P_B_GLO);
        run(30);
        C->ui_info(info, sizeof info);
        q = strstr(info, "name \"");
        CHECK(q && !strncmp(q + 6, "0.0B0\"", 6), "the typewriter through the panel: GLO + B1 '0', GLO + W1 '.', a tap "
              "'0', B2 'B', GLO and B1 in one tick '0' (%s)", q ? q : info);
        {   /* P1.5z: PRESETS right, every detent a new name (now and then a made-up word: no space); KNOB 1 the letter */
            char prev[16] = "", cur[16];
            int k, words = 0, fresh = 1;
            for (k = 0; k < 24; k++) {
                const char *e;
                turn(FM1P_E_PRESETS, 1);
                run(30);
                C->ui_info(info, sizeof info);
                q = strstr(info, "name \"");
                e = q ? strchr(q + 6, '"') : 0;
                snprintf(cur, sizeof cur, "%.*s", e ? (int)(e - q - 6) : 0, q ? q + 6 : "");
                fresh &= cur[0] && strcmp(cur, prev) != 0;
                words += cur[0] && !strchr(cur, ' ');
                snprintf(prev, sizeof prev, "%s", cur);
            }
            CHECK(fresh && words >= 1, "PRESETS through the panel: 24 detents, 24 new names, %d made-up words (the last "
                  "%s)", words, cur);
            bdown(FM1P_B_GLO);                          /* KNOB 1: SHIFT + F1 clears, B1 types A, KNOB 1 +2: C */
            run(30);
            btap(FM1P_B_OCTDN);
            bup(FM1P_B_GLO);
            run(30);
            P->keys |= 1u << 1;
            run(30);
            P->keys &= ~(1u << 1);
            run(30);
            turn(FM1P_E_ALGO, -1);
            turn(FM1P_E_K1, 2);
            run(30);
            C->ui_info(info, sizeof info);
            q = strstr(info, "name \"");
            CHECK(q && !strncmp(q + 6, "C\"", 2), "KNOB 1 through the panel: A turned to C (%s)", q ? q : info);
        }
        btap(FM1P_B_HOME);
        run(100);
    }

    {   /* P1.6l: LABELS through midi_in, subscribed: the reply, then GLO held: a reply by itself (SELECT TRACK) */
        static const uint8_t LREQ[9] = {0xF0, 0x7D, 0x46, 0x5A, 0x01, 0x20, 0x01, 0x5F, 0xF7};
        static uint8_t r[4096];
        int n, n2;
        btap(FM1P_B_HOME);
        run(100);
        sx_out(r, sizeof r);                            /* (whatever waited) */
        sx_in(LREQ, 9);
        run(5);
        n = sx_out(r, sizeof r);
        CHECK(n > 60 && r[0] == 0xF0 && r[5] == 0x60 && r[8] == 49 && r[n - 1] == 0xF7, "LABELS through midi_in: the "
              "reply out of midi_out_take (%d bytes)", n);
        run(200);
        n = sx_out(r, sizeof r);
        bdown(FM1P_B_GLO);
        run(80);
        n2 = sx_out(r, sizeof r);
        CHECK(!n && n2 > 60 && r[5] == 0x60 && !memcmp(r + 9, "\x05TRACK", 6), "subscribed: nothing while nothing "
              "changes; GLO held: LABELS by itself, SELECT TRACK (%d bytes)", n2);
        bup(FM1P_B_GLO);
        run(80);
        sx_out(r, sizeof r);
        {
            static const uint8_t UNSUB[9] = {0xF0, 0x7D, 0x46, 0x5A, 0x01, 0x20, 0x00, 0x60, 0xF7};
            sx_in(UNSUB, 9);
            run(5);
            sx_out(r, sizeof r);
        }
    }

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
