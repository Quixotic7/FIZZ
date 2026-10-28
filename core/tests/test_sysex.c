/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.5r: FIZZ's SysEx (core/app/fizz_sx.h, docs/SYSEX.md): the 7-bit packing; the identity; a sound stored into the
 * LIBRARY and the POOL from a known blob and back byte for byte (its DRIVE), a slot given, the first free, the library
 * full, the pool's slot 1 refused; a bad checksum, a bad length, a bad patch, a bad version, an unknown command; the
 * takeover; undo (one level a message, a batch one); onto a track with its mix.
 *
 *     test_sysex                 the tests (exit 1 on a failed check)
 *     test_sysex --feed F.syx    each frame of F into a fresh FIZZ: its reply (hex) and the sound stored ("name",
 *                                its 74 values), for tools/test_sysex_dnsnd.mjs
 *     test_sysex --session F.syx P1.6l: every frame of F into one FIZZ (awake), every reply after each (hex) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "qd_gfx.h"
#include "sounds.h"
#include "snd_peek.h"                          /* (P2.1: LIB, POOL) */

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

static qd_app_t A;
static uint32_t now;
static void run(uint32_t n)
{
    while (n--)
        qd_app_tick(&A, ++now);
}
static void fresh(void)
{
    qd_app_init(&A);
    now = 0;
    qd_app_tick(&A, 0);
    qd_app_sysex_version(&A, "FIZZ 0.6.4-p1.5r");
    run(10);
}

/* a frame between F0 and F7: the header, cmd and body, its checksum */
static uint32_t frame(uint8_t *f, int cmd, const uint8_t *body, uint32_t n)
{
    uint32_t i, o = 0, sum = (uint32_t)cmd;
    f[o++] = 0x7D, f[o++] = 0x46, f[o++] = 0x5A, f[o++] = 0x01, f[o++] = (uint8_t)cmd;
    for (i = 0; i < n; i++)
        f[o++] = body[i], sum += body[i];
    f[o++] = (uint8_t)((0x80u - (sum & 0x7Fu)) & 0x7Fu);
    return o;
}
static uint32_t store_msg(uint8_t *f, int flags, int slot, const char *name, const uint8_t *blob, int drive, int track,
                          int pan, int ch, int dl, int rv, int mask)
{
    uint8_t b[QD_SX_STORE_BODY];
    uint32_t o = 0, i;
    b[o++] = (uint8_t)flags, b[o++] = (uint8_t)(slot >> 7), b[o++] = (uint8_t)(slot & 0x7F);
    for (i = 0; i < QD_SX_NAME; i++)
        b[o++] = (uint8_t)(i < strlen(name) ? name[i] : ' ');
    b[o++] = (uint8_t)drive, b[o++] = (uint8_t)track, b[o++] = (uint8_t)pan, b[o++] = (uint8_t)ch, b[o++] = (uint8_t)dl,
    b[o++] = (uint8_t)rv, b[o++] = (uint8_t)mask;
    o += qd_sx_pack7(blob, QD_SX_BLOB, b + o);
    return frame(f, QD_SX_STORE, b, o);
}
/* P1.5x: the same with the tags byte after the body */
static uint32_t store_msg_tags(uint8_t *f, int flags, int slot, const char *name, const uint8_t *blob, int track, int pan,
                               int ch, int dl, int rv, int mask, int tags)
{
    uint8_t b[QD_SX_STORE_BODY + 1];
    uint32_t o = 0, i;
    b[o++] = (uint8_t)flags, b[o++] = (uint8_t)(slot >> 7), b[o++] = (uint8_t)(slot & 0x7F);
    for (i = 0; i < QD_SX_NAME; i++)
        b[o++] = (uint8_t)(i < strlen(name) ? name[i] : ' ');
    b[o++] = 0, b[o++] = (uint8_t)track, b[o++] = (uint8_t)pan, b[o++] = (uint8_t)ch, b[o++] = (uint8_t)dl,
    b[o++] = (uint8_t)rv, b[o++] = (uint8_t)mask;
    o += qd_sx_pack7(blob, QD_SX_BLOB, b + o);
    b[o++] = (uint8_t)tags;
    return frame(f, QD_SX_STORE, b, o);
}
static uint8_t R[QD_SX_FRAME_MAX];
static int rn;
static int send(const uint8_t *f, uint32_t n)           /* -> the reply's length (R) */
{
    qd_app_sysex(&A, f, n);
    rn = qd_app_sysex_take(&A, R, sizeof R);
    return rn;
}
static int reply_ok(void)                               /* F0 7D 46 5A 01 .. chk F7, the sum 0 */
{
    int i, sum = 0;
    if (rn < 8 || R[0] != 0xF0 || R[1] != 0x7D || R[2] != 0x46 || R[3] != 0x5A || R[4] != 0x01 || R[rn - 1] != 0xF7)
        return 0;
    for (i = 5; i < rn - 1; i++) {
        if (R[i] & 0x80)
            return 0;
        sum += R[i];
    }
    return (sum & 0x7F) == 0;
}
static int status(void) { return reply_ok() && R[5] == QD_SX_R_STORE ? R[6] : -1; }
static int rslot(void) { return R[8] << 7 | R[9]; }

/* P1.6l / P1.6m: --session: every frame of the file into one FIZZ (awake, in the step view), each reply after it
 * (all of them: a long one, several) as "reply .." lines, a few ms of time between frames */
static void session(const char *path)
{
    static uint8_t buf[1 << 20];
    FILE *fp = fopen(path, "rb");
    size_t n, i, s = 0;
    if (!fp) {
        printf("cannot read %s\n", path);
        exit(1);
    }
    n = fread(buf, 1, sizeof buf, fp);
    fclose(fp);
    fresh();
    qd_app_button(&A, QD_B_HOME, 1);
    run(20);
    qd_app_button(&A, QD_B_HOME, 0);
    run(20);
    for (i = 0; i < n; i++) {
        if (buf[i] == 0xF0)
            s = i + 1;
        else if (buf[i] == 0xF7) {
            int k, g;
            qd_app_sysex(&A, buf + s, (uint32_t)(i - s));
            for (g = 0; g < 4000; g++) {                /* (replies paced over time: take them as they come) */
                while ((rn = qd_app_sysex_take(&A, R, sizeof R)) > 0) {
                    printf("reply");
                    for (k = 0; k < rn; k++)
                        printf(" %02X", R[k]);
                    printf("\n");
                }
                run(1);
                if (!qd_app_sysex_peek(&A) && qd_app_sysex_busy(&A) == 0)
                    break;
            }
        }
    }
}

static void feed(const char *path)
{
    static uint8_t buf[1 << 16];
    FILE *fp = fopen(path, "rb");
    size_t n, i, s = 0;
    if (!fp) {
        printf("cannot read %s\n", path);
        exit(1);
    }
    n = fread(buf, 1, sizeof buf, fp);
    fclose(fp);
    for (i = 0; i < n; i++) {
        if (buf[i] == 0xF0)
            s = i + 1;
        else if (buf[i] == 0xF7) {
            int k;
            fresh();
            send(buf + s, (uint32_t)(i - s));
            printf("reply");
            for (k = 0; k < rn; k++)
                printf(" %02X", R[k]);
            printf("\n");
            if (status() == QD_SX_OK) {
                const int slot = rslot();
                const qd_sound_t *snd = R[7] ? POOL(&A.proj, slot) : LIB(&A, slot - 1);
                printf("name \"%s\"\nvalues", snd->name);
                for (k = 0; k < QD_QUAD_NP; k++)
                    printf(" %d", snd->patch[k]);
                printf("\nmix %d %d %d %d\ntags %d\n", snd->pan, snd->chorus, snd->delay, snd->reverb, snd->tags);   /* (P1.5x) */
            }
        }
    }
}

/* ---------------------------------------------------------------- the web manager, Phase 1 (docs/WEB-MANAGER-PLAN.md
 * §5): STATUS, FLUSH, TAGS, RENAME, DELETE, SWAP, POOL REF, USERS, PROJECTS, PROJECT on a RAM NOR (the store attached,
 * as on the unit): every status (BUSY while playing or a job running, NOT FOUND, IN USE, BAD SLOT, BAD LENGTH, BAD
 * TRACK), the jobs stepped to their end, undo, and a power cut in the middle of a pool swap */
#define FSIZE 0x100000u
static uint8_t FL[FSIZE], FL2[FSIZE];
static int f_read(void *c, uint32_t off, void *d, uint32_t n) { (void)c; memcpy(d, FL + off, n); return off + n <= FSIZE ? 0 : -8; }
static int f_erase(void *c, uint32_t off) { (void)c; memset(FL + off / 4096u * 4096u, 0xFF, 4096u); return 0; }
static int f_prog(void *c, uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    (void)c;
    if (off / 256u != (off + n - 1u) / 256u)
        return -8;
    for (i = 0; i < n; i++)
        FL[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
static const qd_flash_t FLASH = {f_read, f_erase, f_prog, 0};
static void boot_flash(void)                            /* a power-on on the flash as it is */
{
    qd_app_init(&A);
    now = 0;
    qd_app_tick(&A, 0);
    qd_persist_attach(&A, &FLASH);
    qd_app_sysex_version(&A, "FIZZ test");
    run(10);
}
static uint32_t cmd_io;                                 /* patterns read in or written inside the handler (all cmd()s) */
static int cmd(int c, const uint8_t *body, uint32_t n)  /* a command, its reply now (0: none yet) */
{
    uint8_t f[64];
    const uint32_t l0 = A.proj.pat_loads, s0 = A.proj.pat_saves, w0 = A.per.written;
    const int r = send(f, frame(f, c, body, n));
    cmd_io += (A.proj.pat_loads - l0) + (A.proj.pat_saves - s0) + (A.per.written - w0);
    return r;
}
static int wait_reply(uint32_t ms)                      /* the job's reply (time going on, the store's steps) */
{
    rn = 0;
    while (ms-- && !(rn = qd_app_sysex_take(&A, R, sizeof R)))
        run(1);
    return rn;
}
static int ack(int c) { return reply_ok() && R[5] == QD_SX_R_ACK && R[6] == c ? R[7] : -1; }
static int ack_v1(void) { return R[8] << 7 | R[9]; }
static int ack_v2(void) { return R[10] << 7 | R[11]; }
static int mine_store(const char *name, int tags)       /* a MY preset (STORE, the first free): its place (1-based) */
{
    uint8_t f[200], blob[QD_QUAD_BLOB5];
    int16_t p[QD_QUAD_NP];
    qd_quad_preset_patch(2, p);
    qd_quad_pack5(p, blob);
    send(f, store_msg_tags(f, 0, 0, name, blob, 0, 64, 0, 0, 0, 0, tags));
    return status() == QD_SX_OK ? rslot() : -1;
}
static void lock_steps(int p, int k, int n)             /* n steps of pattern p (track 1) lock pool number k */
{
    qd_pattern_t *P = qd_proj_pat(&A.proj, p);
    int s;
    for (s = 0; s < n; s++) {
        qd_step_on(P, 1, s * 3);
        P->step[1][s * 3].sound = (uint8_t)k;
        P->step[1][s * 3].ovr |= QD_OV(QD_PM_SOUND);
    }
    P->used = 1;
    A.edit_gen++;
}
static int locks_of(int p, int k)                       /* the steps of pattern p that lock k */
{
    const qd_pattern_t *P = qd_proj_cpat(&A.proj, p);
    int t, s, n = 0;
    for (t = 0; t < QD_PAT_NTRACK; t++)
        for (s = 0; s < QD_NSTEP; s++)
            n += P->step[t][s].sound == k;
    return n;
}

static void phase1_tests(void)
{
    uint8_t b[32];
    int k, m1, m2, m3, id1, id2, ok, g, x = 0, y = 0;
    memset(FL, 0xFF, sizeof FL);
    boot_flash();
    run(3000);
    /* STATUS, PROJECTS */
    cmd(QD_SX_STATUS, 0, 0);
    CHECK(reply_ok() && R[5] == QD_SX_R_STATUS && R[6] == 1 && R[7] == QD_NTRACK && !(R[8] & QD_SXT_NOSTORE) &&
              !(R[8] & QD_SXT_PLAYING) && R[9] == 0 && R[11] == 1 && rn == 7 + 24 + 4 * QD_PKG_BLE,
          "STATUS: 42, protocol 1, 6 tracks, the store attached, stopped, project 1 worked on, one project (%d bytes)", rn);
#if QD_PKG_BLE
    CHECK(R[29] == QD_SXK_BLE && R[30] == QD_NPROJ && (R[31] << 7 | R[32]) == QD_NLIB,
          "STATUS: the Bluetooth package's byte, its 4 projects, its 128 MY PRESETS' places (%02X %02X %02X %02X)", R[29],
          R[30], R[31], R[32]);
#endif
    {
        const int lt = R[21] << 7 | R[22], pt = R[25] << 7 | R[26];
        CHECK(lt > 0 && pt > lt && (R[16] << 14 | R[17] << 7 | R[18]) == (int)A.per.written,
              "STATUS: the stores' pages (%d, %d), the records committed (%u)", lt, pt, A.per.written);
    }
    cmd(QD_SX_PROJECTS, 0, 0);
    CHECK(reply_ok() && R[5] == QD_SX_R_PROJECTS && R[6] == 0 && R[7] == QD_NPROJ && R[8] == (1 | 2 | 4) &&
              !memcmp(R + 9, A.per.pname[0], strlen(A.per.pname[0])) && R[8 + 15] == 0 && rn == 7 + 3 + QD_NPROJ * 15,
          "PROJECTS: 63, the one worked on (there, worked on, the demos), its name; project 2 not there");
    /* TAGS */
    m1 = mine_store("ALPHA", QD_PKB(QD_PK_BASS));
    m2 = mine_store("BETA", QD_PKB(QD_PK_PAD));
    m3 = mine_store("GAMMA", QD_PKB(QD_PK_LEAD));
    id1 = qd_lib_preset(&A, m1 - 1), id2 = qd_lib_preset(&A, m2 - 1);
    run(600);
    b[0] = QD_SXS_MINE, b[1] = 0, b[2] = (uint8_t)m1, b[3] = QD_PKB(QD_PK_KEYS) | QD_PKB(QD_PK_FX);
    cmd(QD_SX_TAGS, b, 4);
    CHECK(ack(QD_SX_TAGS) == QD_SX_OK && qd_lib_tags(&A, m1 - 1) == (QD_PKB(QD_PK_KEYS) | QD_PKB(QD_PK_FX)),
          "TAGS: MY preset %d's tags set (KEYS + FX)", m1);
    run(600);
    qd_undo_step(&A, -1);
    CHECK(qd_lib_tags(&A, m1 - 1) == QD_PKB(QD_PK_BASS), "TAGS: SHIFT + F1 undoes it (BASS again)");
    b[0] = QD_SXS_FACTORY, b[2] = 3, b[3] = QD_PKB(QD_PK_DRUMS);
    cmd(QD_SX_TAGS, b, 4);
    CHECK(ack(QD_SX_TAGS) == QD_SX_OK && qd_factory_tags(&A, 2) == QD_PKB(QD_PK_DRUMS), "TAGS: a factory preset's (the owner's)");
    b[0] = QD_SXS_MINE, b[2] = 200;
    cmd(QD_SX_TAGS, b, 4);
    CHECK(ack(QD_SX_TAGS) == QD_SX_NOT_FOUND, "TAGS: an empty place: NOT FOUND");
    b[0] = 7, b[2] = 1;
    cmd(QD_SX_TAGS, b, 4);
    CHECK(ack(QD_SX_TAGS) == QD_SX_BAD_SLOT, "TAGS: no such source: BAD SLOT");
    cmd(QD_SX_TAGS, b, 2);
    CHECK(ack(QD_SX_TAGS) == QD_SX_BAD_LENGTH, "TAGS: too short: BAD LENGTH");
    /* POOL REF */
    b[0] = 0, b[1] = 0, b[2] = (uint8_t)(id1 >> 14), b[3] = (uint8_t)((id1 >> 7) & 0x7F), b[4] = (uint8_t)(id1 & 0x7F);
    k = qd_pool_free_slot(&A.proj);
    cmd(QD_SX_POOLREF, b, 5);
    CHECK(ack(QD_SX_POOLREF) == QD_SX_OK && ack_v1() == k && qd_sound_preset(&A.proj, k) == id1 &&
              !strcmp(qd_sound_name(&A.proj, k), "ALPHA"),
          "POOL REF: the first free pool slot (%d) references ALPHA (no copy: its id, its name)", k);
    b[2] = 0x7F, b[3] = 0x7F, b[4] = 0x70;
    cmd(QD_SX_POOLREF, b, 5);
    CHECK(ack(QD_SX_POOLREF) == QD_SX_NOT_FOUND, "POOL REF: an id no preset has: NOT FOUND");
    b[0] = 1, b[1] = 10, b[2] = 0, b[3] = 0, b[4] = 5;
    cmd(QD_SX_POOLREF, b, 5);
    CHECK(ack(QD_SX_POOLREF) == QD_SX_BAD_SLOT, "POOL REF: slot 138: BAD SLOT");
    /* RENAME */
    b[0] = QD_SXS_MINE, b[1] = 0, b[2] = (uint8_t)m1;
    memcpy(b + 3, "ALPHA PRIME ", 12);
    cmd(QD_SX_RENAME, b, 15);
    CHECK(ack(QD_SX_RENAME) == QD_SX_OK && !strcmp(qd_lib_name(&A, m1 - 1), "ALPHA PRIME") &&
              qd_lib_preset(&A, m1 - 1) == id1 && !strcmp(qd_sound_name(&A.proj, k), "ALPHA PRIME"),
          "RENAME: MY preset (its id kept), and the pool slot referencing it follows");
    b[0] = QD_SXS_FACTORY, b[1] = 0, b[2] = 1;
    cmd(QD_SX_RENAME, b, 15);
    CHECK(ack(QD_SX_RENAME) == QD_SX_BAD_SLOT, "RENAME: a factory preset: BAD SLOT (read only)");
    b[0] = QD_SXS_PROJECT, b[2] = 1;
    memcpy(b + 3, "NOIR        ", 12);
    cmd(QD_SX_RENAME, b, 15);
    CHECK(ack(QD_SX_RENAME) == QD_SX_OK && !strcmp(A.per.pname[0], "NOIR"), "RENAME: the project worked on");
    b[2] = QD_NPROJ > 4 ? 5 : QD_NPROJ;                 /* (the Bluetooth package's 4 projects: its last) */
    cmd(QD_SX_RENAME, b, 15);
    CHECK(ack(QD_SX_RENAME) == QD_SX_NOT_FOUND, "RENAME: a project that is not there: NOT FOUND");
    b[2] = QD_NPROJ + 1;
    cmd(QD_SX_RENAME, b, 15);
    CHECK(ack(QD_SX_RENAME) == QD_SX_BAD_SLOT, "RENAME: project %d, past the last: BAD SLOT", QD_NPROJ + 1);
    cmd(QD_SX_RENAME, b, 6);
    CHECK(ack(QD_SX_RENAME) == QD_SX_BAD_LENGTH, "RENAME: no name: BAD LENGTH");
    /* SWAP, MY PRESETS */
    b[0] = QD_SXS_MINE, b[1] = 0, b[2] = (uint8_t)m1, b[3] = 0, b[4] = (uint8_t)m2;
    cmd(QD_SX_SWAP, b, 5);
    CHECK(ack(QD_SX_SWAP) == QD_SX_OK && qd_lib_preset(&A, m1 - 1) == id2 && qd_lib_preset(&A, m2 - 1) == id1 &&
              !strcmp(qd_lib_name(&A, m2 - 1), "ALPHA PRIME") && !strcmp(qd_sound_name(&A.proj, k), "ALPHA PRIME"),
          "SWAP MY PRESETS: the places swapped, the ids with them (the pool's reference unchanged)");
    b[2] = (uint8_t)m3, b[4] = 100;
    cmd(QD_SX_SWAP, b, 5);
    CHECK(ack(QD_SX_SWAP) == QD_SX_OK && !qd_lib_used(&A, m3 - 1) && !strcmp(qd_lib_name(&A, 99), "GAMMA"),
          "SWAP with an empty place: a move (GAMMA to 100)");
    b[2] = 101, b[4] = 102;
    cmd(QD_SX_SWAP, b, 5);
    CHECK(ack(QD_SX_SWAP) == QD_SX_NOT_FOUND, "SWAP of two empty places: NOT FOUND");
    b[2] = 3, b[4] = 3;
    cmd(QD_SX_SWAP, b, 5);
    CHECK(ack(QD_SX_SWAP) == QD_SX_BAD_SLOT, "SWAP of a place with itself: BAD SLOT");
    /* SWAP, the pool: every pattern's locks follow (the patterns out of RAM through their records) */
    x = k, y = qd_pool_free_slot(&A.proj);
    {
        int before[QD_NPAT][2], p;
        b[0] = 0, b[1] = 0, b[2] = (uint8_t)(id2 >> 14), b[3] = (uint8_t)((id2 >> 7) & 0x7F), b[4] = (uint8_t)(id2 & 0x7F);
        cmd(QD_SX_POOLREF, b, 5);
        CHECK(ack(QD_SX_POOLREF) == QD_SX_OK && ack_v1() == y, "(BETA in the pool at %d)", y);
        for (p = 0; p < QD_NPAT; p += 7)
            lock_steps(p, p % 2 ? x : y, 1 + p % 3);
        qd_persist_flush(&A);
        for (p = 0; p < QD_NPAT; p++)
            before[p][0] = locks_of(p, x), before[p][1] = locks_of(p, y);
        qd_app_play(&A);
        b[0] = QD_SXS_POOL, b[1] = 0, b[2] = (uint8_t)x, b[3] = 0, b[4] = (uint8_t)y;
        cmd(QD_SX_SWAP, b, 5);
        CHECK(ack(QD_SX_SWAP) == QD_SX_BUSY, "SWAP the pool while playing: BUSY");
        qd_app_stop(&A);
        run(50);
        cmd(QD_SX_SWAP, b, 5);
        CHECK(rn == 0 && A.sx.wk.cmd == QD_SX_SWAP, "SWAP the pool: a job (no reply yet)");
        cmd(QD_SX_STATUS, 0, 0);
        CHECK(reply_ok() && (R[8] & QD_SXT_JOB) && R[27] == QD_SX_SWAP, "STATUS: the job shows");
        {
            const uint8_t d[3] = {QD_SXS_MINE, 0, 1};
            uint8_t dd[4] = {QD_SXS_MINE, 0, 1, 0};
            (void)d;
            cmd(QD_SX_DELETE, dd, 4);
            CHECK(ack(QD_SX_DELETE) == QD_SX_BUSY, "another job while one runs: BUSY");
        }
        run(20);
        qd_app_play(&A);                                /* (PLAY meanwhile: the job waits) */
        g = A.sx.wk.step;
        run(300);
        CHECK(A.sx.wk.cmd == QD_SX_SWAP && A.sx.wk.step == g, "the job waits while playing (at step %d)", g);
        qd_app_stop(&A);
        memcpy(FL2, FL, sizeof FL);                     /* (the flash as it is mid-swap: a power cut, below) */
        CHECK(wait_reply(5000) && ack(QD_SX_SWAP) == QD_SX_OK, "SWAP the pool: OK when done");
        for (ok = 1, p = 0; p < QD_NPAT; p++)
            ok &= locks_of(p, x) == before[p][1] && locks_of(p, y) == before[p][0];
        CHECK(ok && qd_sound_preset(&A.proj, x) == id2 && qd_sound_preset(&A.proj, y) == id1,
              "SWAP the pool: the slots swapped and every pattern's locks of %d and %d swapped too", x, y);
        CHECK(qd_undo_count(&A, -1) == 0, "SWAP the pool: the undo levels forgotten (their bytes would not match)");
        qd_persist_flush(&A);
        boot_flash();                                   /* (after a restart: the same) */
        for (ok = 1, p = 0; p < QD_NPAT; p++)
            ok &= locks_of(p, x) == before[p][1] && locks_of(p, y) == before[p][0];
        CHECK(ok && qd_sound_preset(&A.proj, x) == id2, "SWAP the pool: kept after a restart");
        {                                               /* a power cut in the middle of it: the store mounts, FIZZ runs */
            static uint8_t keep[FSIZE];
            memcpy(keep, FL, sizeof FL);
            memcpy(FL, FL2, sizeof FL);
            boot_flash();
            run(200);
            cmd(QD_SX_STATUS, 0, 0);
            CHECK(reply_ok() && !(R[8] & QD_SXT_NOSTORE) && qd_sound_used(&A.proj, x) && qd_sound_used(&A.proj, y) &&
                      A.per.proj == 0, "a power cut mid-swap: the store mounts, the pool's two sounds there, STATUS answers");
            memcpy(FL, keep, sizeof FL);
            boot_flash();
        }
        /* USERS, DELETE (the pool) */
        b[0] = QD_SXS_POOL, b[1] = 0, b[2] = (uint8_t)x;
        cmd(QD_SX_USERS, b, 3);
        CHECK(wait_reply(500) && reply_ok() && R[5] == QD_SX_R_USERS && R[9] == 1 && R[10] == 0 &&
                  (R[11] << 7 | R[12]) == qd_pool_users_all(&A, x, 0),
              "USERS of pool %d: this project, its steps (%d)", x, R[11] << 7 | R[12]);
        b[3] = 0;
        cmd(QD_SX_DELETE, b, 4);
        CHECK(wait_reply(500) && ack(QD_SX_DELETE) == QD_SX_IN_USE && ack_v2() > 0 && qd_sound_used(&A.proj, x),
              "DELETE a pool sound steps use, no CONFIRM: IN USE (%d steps), nothing deleted", ack_v2());
        qd_app_play(&A);
        b[3] = QD_SXD_CONFIRM;
        cmd(QD_SX_DELETE, b, 4);
        CHECK(ack(QD_SX_DELETE) == QD_SX_BUSY, "DELETE while playing: BUSY");
        qd_app_stop(&A);
        run(50);
        cmd(QD_SX_DELETE, b, 4);
        CHECK(wait_reply(500) && ack(QD_SX_DELETE) == QD_SX_OK && !qd_sound_used(&A.proj, x),
              "DELETE with CONFIRM: the pool sound gone");
        run(3000);
        for (ok = 1, p = 0; p < QD_NPAT; p++)
            ok &= locks_of(p, x) == 0;
        CHECK(ok, "DELETE: every step locking it back to the track's sound (the panel's fallback)");
        qd_undo_step(&A, -1);
        CHECK(qd_sound_used(&A.proj, x) && qd_sound_preset(&A.proj, x) == id2 && locks_of(14, x) == before[14][1],
              "DELETE: SHIFT + F1 undoes it (the slot and its locks back)");
    }
    /* PROJECT: new, copy, load, delete; RENAME of another project; USERS across projects; DELETE of MY presets */
    {
        int p2;
        b[0] = QD_SXP_NEW, b[1] = 2, b[2] = 0;
        qd_app_play(&A);
        cmd(QD_SX_PROJECT, b, 3);
        CHECK(ack(QD_SX_PROJECT) == QD_SX_BUSY, "PROJECT while playing: BUSY");
        qd_app_stop(&A);
        cmd(QD_SX_PROJECT, b, 3);
        CHECK(wait_reply(2000) && ack(QD_SX_PROJECT) == QD_SX_OK && A.per.proj == 1, "PROJECT NEW 2: worked on now");
        b[0] = 0, b[1] = 0, b[2] = (uint8_t)(id1 >> 14), b[3] = (uint8_t)((id1 >> 7) & 0x7F), b[4] = (uint8_t)(id1 & 0x7F);
        cmd(QD_SX_POOLREF, b, 5);
        p2 = ack_v1();
        lock_steps(0, p2, 4);
        run(2000);
        b[0] = QD_SXP_LOAD, b[1] = 1;
        cmd(QD_SX_PROJECT, b, 3);
        CHECK(wait_reply(2000) && ack(QD_SX_PROJECT) == QD_SX_OK && A.per.proj == 0 && A.per.pexists[1],
              "PROJECT LOAD 1: back, project 2 kept");
        b[0] = QD_SXS_PROJECT, b[1] = 0, b[2] = 2;
        memcpy(b + 3, "SUNDAY      ", 12);
        cmd(QD_SX_RENAME, b, 15);
        CHECK(rn == 0 && wait_reply(500) && ack(QD_SX_RENAME) == QD_SX_OK && !strcmp(A.per.pname[1], "SUNDAY"),
              "RENAME another project: a job (its header written), OK");
        b[0] = QD_SXP_COPY, b[1] = 2, b[2] = 4;
        cmd(QD_SX_PROJECT, b, 3);
        CHECK(wait_reply(2000) && ack(QD_SX_PROJECT) == QD_SX_OK && A.per.pexists[3] && !strcmp(A.per.pname[3], "SUNDAY"),
              "PROJECT COPY 2 onto 4");
        b[2] = 1;
        cmd(QD_SX_PROJECT, b, 3);
        CHECK(ack(QD_SX_PROJECT) == QD_SX_BAD_SLOT, "PROJECT COPY onto the one worked on: BAD SLOT");
        b[0] = 9;
        cmd(QD_SX_PROJECT, b, 3);
        CHECK(ack(QD_SX_PROJECT) == QD_SX_BAD_SLOT, "PROJECT, an op it does not know: BAD SLOT");
        b[0] = QD_SXP_DELETE, b[1] = 4;
        cmd(QD_SX_PROJECT, b, 3);
        CHECK(wait_reply(2000) && ack(QD_SX_PROJECT) == QD_SX_OK && !A.per.pexists[3], "PROJECT DELETE 4");
        cmd(QD_SX_PROJECT, b, 3);
        CHECK(ack(QD_SX_PROJECT) == QD_SX_NOT_FOUND, "PROJECT DELETE of one not there: NOT FOUND");
        boot_flash();
        CHECK(!strcmp(A.per.pname[1], "SUNDAY") && !strcmp(A.per.pname[0], "NOIR"), "the projects' names after a restart");
        /* USERS of MY preset ALPHA PRIME (now at m2): this project's pool and project 2's */
        b[0] = QD_SXS_MINE, b[1] = 0, b[2] = (uint8_t)m2;
        cmd(QD_SX_USERS, b, 3);
        CHECK(wait_reply(1000) && reply_ok() && R[5] == QD_SX_R_USERS && R[9] == 2 && R[13] == 1 && (R[14] << 7 | R[15]) == 4,
              "USERS of a MY preset: two projects, project 2's 4 steps");
        b[3] = 0;
        cmd(QD_SX_DELETE, b, 4);
        CHECK(wait_reply(1000) && ack(QD_SX_DELETE) == QD_SX_IN_USE && ack_v1() == 2, "DELETE it, no CONFIRM: IN USE, 2 projects");
        b[3] = QD_SXD_CONFIRM;
        cmd(QD_SX_DELETE, b, 4);
        wait_reply(1000);
        CHECK(rn && ack(QD_SX_DELETE) == QD_SX_OK && !qd_lib_used(&A, m2 - 1) && !qd_sound_used(&A.proj, y),
              "DELETE with CONFIRM: MY preset gone, this pool's reference too");
        b[2] = (uint8_t)m1;                             /* (BETA: not used by any project now) */
        b[3] = 0;
        for (k = 1; k <= QD_NSOUND; k++)
            if (qd_sound_used(&A.proj, k) && qd_sound_preset(&A.proj, k) == id2) {
                const uint8_t c[5] = {0, (uint8_t)k, 0, 0, 0};
                cmd(QD_SX_POOLREF, c, 5);
                wait_reply(500);
            }
        cmd(QD_SX_DELETE, b, 4);
        CHECK(wait_reply(1000) && ack(QD_SX_DELETE) == QD_SX_OK && !qd_lib_used(&A, m1 - 1),
              "DELETE an unused MY preset, no CONFIRM: deleted");
    }
    /* FLUSH */
    m1 = mine_store("DELTA", 0);
    cmd(QD_SX_STATUS, 0, 0);
    ok = reply_ok() && (R[8] & QD_SXT_DIRTY);
    qd_app_play(&A);
    cmd(QD_SX_FLUSH, 0, 0);
    CHECK(ack(QD_SX_FLUSH) == QD_SX_BUSY, "FLUSH while playing: BUSY");
    qd_app_stop(&A);
    cmd(QD_SX_FLUSH, 0, 0);
    CHECK(ok && wait_reply(500) && R[5] == QD_SX_R_STATUS && !(R[8] & QD_SXT_DIRTY),
          "FLUSH: a STATUS when everything is written (dirty before, not after)");
    /* STORE onto track 7: BAD TRACK, nothing stored */
    {
        uint8_t f[200], blob[QD_QUAD_BLOB5];
        int16_t p[QD_QUAD_NP];
        const int fr = qd_lib_free_slot(&A);
        qd_quad_preset_patch(2, p);
        qd_quad_pack5(p, blob);
        send(f, store_msg(f, QD_SXF_LOAD, 0, "TRACK7", blob, 0, 6, 64, 0, 0, 0, 0));
        CHECK(status() == QD_SX_BAD_TRACK && qd_lib_free_slot(&A) == fr, "STORE with LOAD onto track 7: BAD TRACK, nothing stored");
    }
    CHECK(cmd_io == 0, "no handler read a pattern in, wrote one or committed a record (%u): the jobs did", cmd_io);
    /* without a store (safe mode): STATUS says so; PROJECT NOT FOUND */
    fresh();
    cmd(QD_SX_STATUS, 0, 0);
    CHECK(reply_ok() && (R[8] & QD_SXT_NOSTORE), "STATUS with no store attached (safe mode): flagged");
    b[0] = QD_SXP_NEW, b[1] = 2;
    cmd(QD_SX_PROJECT, b, 3);
    CHECK(ack(QD_SX_PROJECT) == QD_SX_NOT_FOUND, "PROJECT with no store: NOT FOUND");
}

/* ---------------------------------------------------------------- Manager P2-B (docs/MANAGER-P2-PLAN.md §1.3, the
 * owner's decisions): PARAM (a track's sound edited live), DUMP of a track (with its extras), STORE6 AUDITION (onto a
 * track only, its extras put it back exactly: a page's CANCEL), OWN, a STORE over a used MY PRESETS place as a job
 * (BUSY while playing, paused by PLAY; the other projects' records that loaded it keep the sound they had) */
static uint32_t store6_msg(uint8_t *f, int flags, int slot, const char *name, const int16_t *patch, int track, int pan,
                           int ch, int dl, int rv, int mask, int tags, const uint8_t *xtra)
{
    uint8_t b[QD_SX_STORE6_BODY + 1 + QD_SX_XTRA], blob[QD_QUAD_BLOB];
    uint32_t o = 0, i;
    qd_quad_pack(patch, blob);
    b[o++] = (uint8_t)flags, b[o++] = (uint8_t)(slot >> 7), b[o++] = (uint8_t)(slot & 0x7F);
    for (i = 0; i < QD_SX_NAME; i++)
        b[o++] = (uint8_t)(i < strlen(name) ? name[i] : ' ');
    b[o++] = (uint8_t)(patch[QD_QP_DRIVE] & 0x7F), b[o++] = (uint8_t)track, b[o++] = (uint8_t)pan, b[o++] = (uint8_t)ch;
    b[o++] = (uint8_t)dl, b[o++] = (uint8_t)rv, b[o++] = (uint8_t)mask;
    o += qd_sx_pack7(blob, QD_SX_BLOB6, b + o);
    b[o++] = (uint8_t)tags;
    if (xtra)
        for (i = 0; i < QD_SX_XTRA; i++)
            b[o++] = xtra[i];
    return frame(f, QD_SX_STORE6, b, o);
}
static int param(int t, int id, int v, int flags)       /* PARAM: its reply's length (0: none) */
{
    const uint32_t x = (uint32_t)(v + QD_SXP_OFS);
    const uint8_t b[6] = {(uint8_t)t, (uint8_t)id, (uint8_t)((x >> 14) & 0x7F), (uint8_t)((x >> 7) & 0x7F), (uint8_t)(x & 0x7F),
                          (uint8_t)flags};
    return cmd(QD_SX_PARAM, b, 6);
}
static int dump_track(int slot)                         /* DUMP of a track: its reply's length */
{
    const uint8_t b[3] = {QD_SXS_TRACK, (uint8_t)(slot >> 7), (uint8_t)(slot & 0x7F)};
    return cmd(QD_SX_DUMP, b, 3);
}
static int same_track(const qd_track_t *t, const qd_track_t *u)
{
    return !memcmp(t->patch, u->patch, sizeof t->patch) && t->sound == u->sound && t->edited == u->edited &&
           t->preset == u->preset && !strcmp(t->sound_name, u->sound_name) && t->mix.pan == u->mix.pan &&
           t->mix.chorus == u->mix.chorus && t->mix.delay == u->mix.delay && t->mix.reverb == u->mix.reverb;
}
static void p2b_tests(void)
{
    static uint8_t f[320], back[320];
    static int16_t p0[QD_QUAD_NP], p1[QD_QUAD_NP], p2[QD_QUAD_NP];
    qd_track_t before;
    qd_sound_t ls;
    uint8_t xt[QD_SX_XTRA];
    int m, id, n, nlib, u0, k, ok, bn, pat1, pat2;
    uint32_t io0;
    memset(FL, 0xFF, sizeof FL);
    boot_flash();
    run(3000);
    cmd_io = 0;
    /* PARAM */
    u0 = qd_undo_count(&A, -1);
    n = param(2, QD_QP_FREQ, 5000, 0);
    CHECK(n == 0 && A.model.t[2].patch[QD_QP_FREQ] == 5000 && A.model.t[2].edited &&
              qd_proj_pat(&A.proj, A.seq.cur)->snd[2].patch[QD_QP_FREQ] == 5000,
          "PARAM: track 3's FREQ 50.00 (no reply): the track's sound and the pattern's copy, marked edited");
    param(2, QD_QP_FREQ, 99999, QD_SXP_ACK);
    CHECK(ack(QD_SX_PARAM) == QD_SX_OK && ack_v1() == 3 && ack_v2() == QD_QP_FREQ && A.model.t[2].patch[QD_QP_FREQ] == 12700,
          "PARAM with ACK: OK (track 3, FREQ); a value past the range clamped as a knob's (127.00)");
    param(0, QD_QP_HARM, -2601, 0);
    param(0, QD_QP_BITS, 40, 0);
    CHECK(A.model.t[0].patch[QD_QP_HARM] == -2600 && A.model.t[0].patch[QD_QP_BITS] == 40,
          "PARAM: a negative value (HARM -26.01 -> -26.00), a value 74.. (BITS 40)");
    param(6, QD_QP_FREQ, 100, 0);
    CHECK(ack(QD_SX_PARAM) == QD_SX_BAD_TRACK, "PARAM onto track 7: BAD TRACK (answered without ACK)");
    param(1, QD_QUAD_NP, 0, 0);
    CHECK(ack(QD_SX_PARAM) == QD_SX_NOT_FOUND, "PARAM of value %d (past the sound's): NOT FOUND", QD_QUAD_NP);
    param(1, QD_QP_UNI, 4, 0);
    CHECK(A.model.t[1].patch[QD_QP_UNI] == 4, "PARAM: the VOICE page's UNISON (4)");
    A.sel = 4;
    param(QD_SXP_SELECTED, QD_QP_RESO, 3000, QD_SXP_ACK);
    CHECK(ack(QD_SX_PARAM) == QD_SX_OK && ack_v1() == 5 && A.model.t[4].patch[QD_QP_RESO] == 3000, "PARAM onto track 127: the selected one (5)");
    qd_app_play(&A);
    run(200);
    n = param(1, QD_QP_FREQ, 4000, 0);
    CHECK(n == 0 && A.model.t[1].patch[QD_QP_FREQ] == 4000, "PARAM while playing: taken at once (a knob's edit)");
    qd_app_stop(&A);
    run(800);
    CHECK(qd_undo_count(&A, -1) == u0, "PARAM: no undo level (%d -> %d)", u0, qd_undo_count(&A, -1));
    /* DUMP of a track */
    A.sel = 2;
    A.model.t[2].mix.pan = -10, A.model.t[2].mix.reverb = 33;
    dump_track(3);
    bn = rn;
    memcpy(back, R, (size_t)rn);
    {
        const int six = R[5] == QD_SX_STORE6, n7 = six ? QD_SX_BLOB6_7 : QD_SX_BLOB7;
        const uint8_t *body = R + 6, *x = body + 22 + n7 + 1;
        uint8_t blob[QD_QUAD_BLOB];
        qd_sx_unpack7(body + 22, (uint32_t)n7, blob);
        blob[87] = body[15];
        ok = qd_quad_unpack(blob, six ? QD_QUAD_BLOB : QD_QUAD_BLOB5, p0) && !memcmp(p0, A.model.t[2].patch, sizeof p0);
        CHECK(reply_ok() && (R[6] & QD_SXF_DUMP) && (R[6] & QD_SXF_LOAD) && body[16] == 2 && ok && body[17] == 64 - 10 &&
                  body[20] == 33 && !memcmp(body + 3, A.model.t[2].sound_name, strlen(A.model.t[2].sound_name)) &&
                  rn == 5 + 1 + (six ? QD_SX_STORE6_BODY : QD_SX_STORE_BODY) + 1 + QD_SX_XTRA + 2,
              "DUMP of track 3: a STORE frame with DUMP and LOAD, track 3, its name, patch, pan and sends, then its extras");
        CHECK((x[0] << 14 | x[1] << 7 | x[2]) == A.model.t[2].preset && (x[3] << 7 | x[4]) == A.model.t[2].sound &&
                  (x[5] & QD_SXX_EDITED) && (x[5] & QD_SXX_SELECTED) && x[7] == 2 &&
                  x[6] == (qd_seq_track_pat(&A.seq, 2)->trk[2].chan & 0x0F),
              "DUMP of track 3: its preset id, pool sound, edited, selected, its MIDI channel, the selected track");
    }
    dump_track(0);
    CHECK(reply_ok() && R[6 + 16] == 2, "DUMP of track 0: the selected track (3)");
    dump_track(7);
    CHECK(reply_ok() && R[5] == QD_SX_R_ERROR && R[6] == QD_SX_BAD_SLOT, "DUMP of track 7: BAD SLOT");
    /* AUDITION: onto track 3 only (nothing stored, no undo), then the DUMP sent back: the track exactly as it was */
    before = A.model.t[2];
    for (nlib = 0, k = 0; k < QD_NLIB; k++)
        nlib += qd_lib_used(&A, k);
    u0 = qd_undo_count(&A, -1);
    qd_quad_preset_patch(5, p1);
    memset(xt, 0, sizeof xt);
    xt[2] = 6;                                          /* (preset id 6: factory 6) */
    send(f, store6_msg(f, QD_SXF_LOAD | QD_SXF_AUDITION, 0, "AUDITION", p1, 2, 64 + 20, 1, 2, 3, 0x0F, 0, xt));
    for (n = 0, k = 0; k < QD_NLIB; k++)
        n += qd_lib_used(&A, k);
    run(800);
    CHECK(status() == QD_SX_OK && rslot() == 0 && n == nlib && !memcmp(A.model.t[2].patch, p1, sizeof p1) &&
              !strcmp(A.model.t[2].sound_name, "AUDITION") && A.model.t[2].preset == 6 && !A.model.t[2].edited &&
              A.model.t[2].mix.pan == 20 && A.model.t[2].mix.reverb == 3 && qd_undo_count(&A, -1) == u0,
          "AUDITION: the sound on track 3 (its preset id, its mix): nothing stored, no undo level");
    back[6] &= (uint8_t)~QD_SXF_DUMP;
    back[6] |= QD_SXF_AUDITION;
    {
        int j, sum = 0;
        for (j = 5; j < bn - 2; j++)
            sum += back[j];
        back[bn - 2] = (uint8_t)((0x80 - (sum & 0x7F)) & 0x7F);
    }
    send(back + 1, (uint32_t)bn - 2u);
    CHECK(status() == QD_SX_OK && same_track(&A.model.t[2], &before),
          "AUDITION of the track's own DUMP (its extras): the track exactly as it was (CANCEL)");
    send(f, store6_msg(f, QD_SXF_AUDITION, 0, "NO LOAD", p1, 2, 64, 0, 0, 0, 0, 0, xt));
    CHECK(status() == QD_SX_BAD_LENGTH, "AUDITION without LOAD with the extras: BAD LENGTH (the extras are the track's)");
    /* OWN */
    send(f, store6_msg(f, QD_SXF_OWN, 0, "MINE OWN", p1, 0, 64, 0, 0, 0, 0x0F, QD_PKB(QD_PK_PAD), 0));
    m = rslot();
    CHECK(status() == QD_SX_OK && !(qd_lib_flags(&A, m - 1) & QD_SNDF_IMPORTED) && qd_lib_tags(&A, m - 1) == QD_PKB(QD_PK_PAD),
          "STORE6 OWN: the owner's preset (not imported), its tags as given");
    send(f, store6_msg(f, 0, 0, "IMPORTED", p1, 0, 64, 0, 0, 0, 0x0F, 0, 0));
    CHECK(status() == QD_SX_OK && (qd_lib_flags(&A, rslot() - 1) & QD_SNDF_IMPORTED), "STORE6 without OWN: imported, as before");
    /* a STORE over a used place: a job. Tracks of project 1 and of project 2 load preset m (AUDITION with its id, as
     * a page's LIBRARY load), their records written; the preset saved over; each kept the sound it had */
    id = qd_lib_preset(&A, m - 1);
    qd_lib_read(&A, m - 1, &ls);
    memcpy(p0, ls.patch, sizeof p0);
    memset(xt, 0, sizeof xt);
    xt[0] = (uint8_t)((id >> 14) & 0x7F), xt[1] = (uint8_t)((id >> 7) & 0x7F), xt[2] = (uint8_t)(id & 0x7F);
    send(f, store6_msg(f, QD_SXF_LOAD | QD_SXF_AUDITION, 0, "MINE OWN", p0, 0, 64, 0, 0, 0, 0x0F, 0, xt));
    pat1 = A.seq.cur;
    CHECK(status() == QD_SX_OK && A.model.t[0].preset == id && !A.model.t[0].edited, "(track 1 of project 1 loads MY preset %d)", m);
    run(2500);
    {
        const uint8_t pb[3] = {QD_SXP_NEW, 2, 0};
        cmd(QD_SX_PROJECT, pb, 3);
        CHECK(wait_reply(3000) && ack(QD_SX_PROJECT) == QD_SX_OK && A.per.proj == 1, "(project 2 made)");
    }
    send(f, store6_msg(f, QD_SXF_LOAD | QD_SXF_AUDITION, 0, "MINE OWN", p0, 1, 64, 0, 0, 0, 0x0F, 0, xt));
    pat2 = A.seq.cur;
    qd_step_on(qd_proj_pat(&A.proj, pat2), 1, 0);      /* (a pattern with a trig: written) */
    qd_proj_pat(&A.proj, pat2)->used = 1;
    A.edit_gen++;
    run(2500);
    {
        const uint8_t pb[3] = {QD_SXP_LOAD, 1, 0};
        cmd(QD_SX_PROJECT, pb, 3);
        CHECK(wait_reply(3000) && ack(QD_SX_PROJECT) == QD_SX_OK && A.per.proj == 0 && qd_proj_pat(&A.proj, pat1)->snd[0].preset == id,
              "(project 1 again; its pattern %d's track 1 still MY preset %d)", pat1 + 1, m);
    }
    memcpy(p2, p0, sizeof p2);
    p2[QD_QP_FREQ] = (int16_t)(p0[QD_QP_FREQ] > 6000 ? 1000 : 9000), p2[QD_QP_FDBK] = 99;
    qd_app_play(&A);
    send(f, store6_msg(f, QD_SXF_OWN, m, "SAVED OVER", p2, 0, 64, 0, 0, 0, 0x0F, 0, 0));
    CHECK(status() == QD_SX_BUSY && !strcmp(qd_lib_name(&A, m - 1), "MINE OWN"), "STORE over a used MY place while playing: BUSY, nothing changed");
    qd_app_stop(&A);
    run(50);
    io0 = cmd_io;
    send(f, store6_msg(f, QD_SXF_OWN, m, "SAVED OVER", p2, 0, 64, 0, 0, 0, 0x0F, 0, 0));
    CHECK(rn == 0 && A.sx.wk.cmd == QD_SX_STORE6 && !strcmp(qd_lib_name(&A, m - 1), "MINE OWN"),
          "STORE over a used MY place: a job (no reply yet, the preset not changed yet)");
    run(16);
    qd_app_play(&A);                                    /* (PLAY meanwhile: the job waits) */
    k = A.sx.wk.step;
    run(300);
    CHECK(A.sx.wk.cmd == QD_SX_STORE6 && A.sx.wk.step == k, "the STORE job waits while playing (at step %d)", k);
    qd_app_stop(&A);
    CHECK(wait_reply(3000) && status() == QD_SX_OK && rslot() == m, "the STORE job: its reply when done (MY PRESETS %d)", m);
    qd_lib_read(&A, m - 1, &ls);
    CHECK(!strcmp(ls.name, "SAVED OVER") && !memcmp(ls.patch, p2, sizeof p2) && qd_lib_preset(&A, m - 1) == id &&
              !(ls.flags & QD_SNDF_IMPORTED),
          "the STORE job: MY preset %d is the new sound, its id kept", m);
    run(2500);
    qd_persist_flush(&A);
    boot_flash();                                       /* (after a restart: the projects' records as written) */
    CHECK(A.per.proj == 0 && !memcmp(qd_proj_pat(&A.proj, pat1)->snd[0].patch, p0, sizeof p0),
          "project 1's track 1 (it loaded the preset) kept the sound it had, from its record");
    {
        const uint8_t pb[3] = {QD_SXP_LOAD, 2, 0};
        cmd(QD_SX_PROJECT, pb, 3);
        CHECK(wait_reply(3000) && ack(QD_SX_PROJECT) == QD_SX_OK && !memcmp(qd_proj_pat(&A.proj, pat2)->snd[1].patch, p0, sizeof p0),
              "project 2's track 2 (another project's record) kept the sound it had too");
    }
    CHECK(cmd_io == io0, "the STORE over a used place read and wrote nothing inside the handler (the job did)");
    /* STATUS: the selected track, REC */
    A.sel = 3;
    cmd(QD_SX_STATUS, 0, 0);
    CHECK(reply_ok() && R[28] == 3 && !(R[8] & QD_SXT_REC), "STATUS: the selected track (4) after the job's byte; not recording");
}

/* ---------------------------------------------------------------- Manager fixes: BROWSE (LIBRARY's LOAD ON FM-1): the
 * unit's sound browser opened on a track at a preset, as PRESETS turned to it: the track previews it (the pattern's
 * copy untouched), STATUS and the track's DUMP say so; OCT- on the unit cancels (the track exactly as it was), OCT+
 * loads it (the pattern's track sound); another track given: selected; refused: no such preset, track 7, a name being
 * typed, a short body */
/* FIZZ MIDI (e): a MIDI sound (quad_voice.h QD_MSND_*) through SysEx: STORE6 into MY PRESETS (the same sound back),
 * AUDITION onto a track (a MIDI track), the track's DUMP (its blob the same sound); PARAM onto it: its slots' values only */
static void msnd_tests(void)
{
    static uint8_t f[320];
    static int16_t mp[QD_QUAD_NP], p0[QD_QUAD_NP];
    qd_sound_t ls;
    int m, k;
    memset(FL, 0xFF, sizeof FL);
    boot_flash();
    run(3000);
    qd_msnd_init(mp);
    for (k = 0; k < QD_MSND_N; k += 3)                  /* (some slots touched, CC numbers moved) */
        mp[QD_MSND_ID[k]] = (int16_t)(k * 4 + 1), mp[QD_MSND_CC(k)] = (int16_t)((70 + k) | QD_MSND_TOUCHED);
    send(f, store6_msg(f, QD_SXF_OWN, 0, "SYNTH CCS", mp, 0, 64, 0, 0, 0, 0x0F, 0, 0));
    m = rslot();
    qd_lib_read(&A, m - 1, &ls);
    CHECK(status() == QD_SX_OK && qd_msnd_is(ls.patch) && !memcmp(ls.patch, mp, sizeof mp),
          "STORE6 of a MIDI sound: MY PRESETS %d holds it exactly", m);
    {   /* (the out queue emptied; track 4's MIDI OUT on channel 4) */
        uint32_t pk;
        while (qd_app_midi_out_take(&A, &pk, 64u))
            ;
        ((qd_pattern_t *)qd_seq_track_pat(&A.seq, 3))->trk[3].mout = 4;
    }
    send(f, store6_msg(f, QD_SXF_LOAD | QD_SXF_AUDITION, 0, "SYNTH CCS", mp, 3, 64, 0, 0, 0, 0x0F, 0, 0));
    CHECK(status() == QD_SX_OK && !memcmp(A.model.t[3].patch, mp, sizeof mp), "AUDITION onto track 4: a MIDI track");
    {   /* its touched slots out on channel 4 (the Manager's edit reaching the synth); then one slot changed: that one */
        uint32_t pk;
        int n = 0, good = 0;
        while (qd_app_midi_out_take(&A, &pk, 64u)) {
            const int st = (int)(pk >> 8 & 0xFFu), c = (int)(pk >> 16 & 0x7Fu), v = (int)(pk >> 24 & 0x7Fu);
            n++;
            good += st == 0xB3 && c >= 70 && (c - 70) % 3 == 0 && v == (c - 70) * 4 + 1;
        }
        CHECK(n == 10 && good == 10, "AUDITION of a MIDI sound: its 10 touched slots out as CCs on the track's MIDI OUT (%d, %d)", n, good);
        mp[QD_MSND_ID[3]] = 77;
        mp[QD_MSND_ID[1]] = 5;                          /* (untouched: nothing) */
        send(f, store6_msg(f, QD_SXF_LOAD | QD_SXF_AUDITION, 0, "SYNTH CCS", mp, 3, 64, 0, 0, 0, 0x0F, 0, 0));
        n = good = 0;
        while (qd_app_midi_out_take(&A, &pk, 64u)) {
            n++;
            good += (pk >> 8 & 0xFFu) == 0xB3u && (pk >> 16 & 0x7Fu) == 73u && (pk >> 24 & 0x7Fu) == 77u;
        }
        CHECK(n == 1 && good == 1, "AUDITION again with one touched slot changed: that slot's CC only (%d)", n);
    }
    dump_track(4);
    {
        const uint8_t *body = R + 6;
        uint8_t blob[QD_QUAD_BLOB];
        qd_sx_unpack7(body + 22, QD_SX_BLOB6_7, blob);
        blob[87] = body[15];
        CHECK(reply_ok() && R[5] == QD_SX_STORE6 && qd_quad_unpack(blob, QD_QUAD_BLOB, p0) && !memcmp(p0, mp, sizeof mp) &&
                  !memcmp(blob + QD_QUAD_BLOB - 4, "MIDI", 4),
              "its DUMP: a STORE6 whose blob is the MIDI sound (\"MIDI\" in its last four bytes)");
    }
    param(3, QD_QP_FREQ, 100, QD_SXP_ACK);
    CHECK(ack(QD_SX_PARAM) == QD_SX_NOT_FOUND && !memcmp(A.model.t[3].patch, mp, sizeof mp),
          "PARAM of FREQ onto the MIDI track: NOT FOUND, the sound kept");
    param(3, QD_MSND_ID[1], 99, QD_SXP_ACK);
    CHECK(ack(QD_SX_PARAM) == QD_SX_OK && A.model.t[3].patch[QD_MSND_ID[1]] == 99, "PARAM of slot 2's value: 99");
    run(800);
}

static int browse(int t, int id)
{
    const uint8_t b[4] = {(uint8_t)t, (uint8_t)((id >> 14) & 0x7F), (uint8_t)((id >> 7) & 0x7F), (uint8_t)(id & 0x7F)};
    return cmd(QD_SX_BROWSE, b, 4);
}
static void tap(int button)
{
    qd_app_button(&A, button, 1);
    run(20);
    qd_app_button(&A, button, 0);
    run(20);
}
static int xflags(void)                                 /* a track DUMP reply's extras' flags */
{
    const int n7 = R[5] == QD_SX_STORE6 ? QD_SX_BLOB6_7 : QD_SX_BLOB7;
    return R[6 + 22 + n7 + 1 + 5];
}
static void browse_tests(void)
{
    qd_track_t before;
    qd_sound_t fs;
    int m, id, pre;
    memset(FL, 0xFF, sizeof FL);
    boot_flash();
    run(3000);
    cmd_io = 0;
    A.sel = 0;
    before = A.model.t[3];
    pre = qd_proj_pat(&A.proj, A.seq.cur)->snd[3].preset;
    qd_factory_get(&A, 6, &fs);                         /* (FACTORY 7: preset id 7) */
    browse(3, 7);
    CHECK(ack(QD_SX_BROWSE) == QD_SX_OK && ack_v1() == 4 && ack_v2() >= 1 && A.sel == 3 && A.ui.latch == QD_LATCH_BROWSE &&
              A.ui.br.t == 3 && A.model.t[3].preset == 7 && !strcmp(A.model.t[3].sound_name, fs.name) &&
              !memcmp(A.model.t[3].patch, fs.patch, sizeof fs.patch) && qd_proj_pat(&A.proj, A.seq.cur)->snd[3].preset == pre,
          "BROWSE track 4, preset 7: the browser open on track 4 (selected) at FACTORY 7 %s, the track previews it, the "
          "pattern's copy untouched", fs.name);
    cmd(QD_SX_STATUS, 0, 0);
    CHECK(reply_ok() && R[5] == QD_SX_R_STATUS && (R[8] & QD_SXT_BROWSE), "BROWSE: STATUS's flag 40 (the browser open)");
    dump_track(4);
    CHECK(reply_ok() && (xflags() & QD_SXX_BROWSE), "BROWSE: track 4's DUMP: its extras' flag 10 (previewing)");
    dump_track(1);
    CHECK(reply_ok() && !(xflags() & QD_SXX_BROWSE), "BROWSE: track 1's DUMP: not previewing");
    tap(QD_B_OCTDN);
    CHECK(A.ui.latch != QD_LATCH_BROWSE && same_track(&A.model.t[3], &before), "BROWSE then OCT- on the unit: cancelled, track 4 exactly as it was");
    cmd(QD_SX_STATUS, 0, 0);
    CHECK(reply_ok() && !(R[8] & QD_SXT_BROWSE), "BROWSE cancelled: STATUS's flag 40 off");
    /* a MY preset, onto the selected track (127), then OCT+ */
    m = mine_store("BROWSED", QD_PKB(QD_PK_LEAD));
    id = qd_lib_preset(&A, m - 1);
    run(100);
    browse(QD_SXP_SELECTED, id);
    CHECK(ack(QD_SX_BROWSE) == QD_SX_OK && ack_v1() == 4 && A.ui.latch == QD_LATCH_BROWSE && A.model.t[3].preset == id &&
              !strcmp(A.model.t[3].sound_name, "BROWSED"),
          "BROWSE the selected track (4), MY PRESETS %d (id %d): previewed", m, id);
    browse(1, 7);
    CHECK(ack(QD_SX_BROWSE) == QD_SX_OK && A.sel == 1 && A.ui.br.t == 1 && A.model.t[1].preset == 7 &&
              same_track(&A.model.t[3], &before),
          "BROWSE another track (2) while browsing track 4: track 4 put back, track 2 selected and previewing");
    browse(1, id);
    tap(QD_B_OCTUP);
    CHECK(A.ui.latch != QD_LATCH_BROWSE && A.model.t[1].preset == id && qd_proj_pat(&A.proj, A.seq.cur)->snd[1].preset == id,
          "BROWSE then OCT+ on the unit: loaded, track 2 plays MY PRESETS %d (the pattern's track sound)", m);
    /* refused */
    browse(0, 9999);
    CHECK(ack(QD_SX_BROWSE) == QD_SX_NOT_FOUND && A.ui.latch != QD_LATCH_BROWSE && A.sel == 1, "BROWSE of no such preset: NOT FOUND, nothing opened");
    browse(6, 7);
    CHECK(ack(QD_SX_BROWSE) == QD_SX_BAD_TRACK, "BROWSE onto track 7: BAD TRACK");
    {
        const uint8_t b[2] = {0, 0};
        cmd(QD_SX_BROWSE, b, 2);
        CHECK(ack(QD_SX_BROWSE) == QD_SX_BAD_LENGTH, "BROWSE with a short body: BAD LENGTH");
    }
    qd_ctl_set_mode(&A, QD_MODE_STEP);
    qd_latch_open(&A, QD_LATCH_SAVE);                   /* (SAVE PRESET: a name being typed) */
    browse(0, 7);
    CHECK(ack(QD_SX_BROWSE) == QD_SX_BUSY && A.ui.latch == QD_LATCH_SAVE, "BROWSE while a name is typed on the unit: BUSY, the typing kept");
    qd_latch_close(&A);
    CHECK(cmd_io == 0, "BROWSE: nothing read in or written inside the handler (%u)", cmd_io);
}

int main(int argc, char **argv)
{
    static uint8_t f[256], in[256], pk[300], un[300], blob[QD_QUAD_BLOB5], blob2[QD_QUAD_BLOB5];
    static int16_t p0[QD_QUAD_NP], p1[QD_QUAD_NP];
    uint32_t n, i;
    int ok, k, s0;
    if (!qd_init()) {
        printf("test_sysex: qd_init failed\n");
        return 1;
    }
    if (argc == 3 && !strcmp(argv[1], "--session")) {   /* (P1.6l) */
        session(argv[2]);
        qd_shutdown();
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "--feed")) {
        feed(argv[2]);
        qd_shutdown();
        return 0;
    }
    for (i = 0; i < 256; i++)                           /* the packing: every byte value, every tail length */
        in[i] = (uint8_t)(i * 37u + 11u);
    for (ok = 1, n = 0; n <= 30; n++) {
        const uint32_t m = qd_sx_pack7(in, n, pk);
        ok &= m == n + (n + 6) / 7 && qd_sx_unpack7(pk, m, un) == n && !memcmp(in, un, n);
        for (i = 0; i < m; i++)
            ok &= pk[i] < 0x80;
    }
    CHECK(ok && qd_sx_pack7(in, QD_SX_BLOB, pk) == QD_SX_BLOB7, "pack7: 7-bit, back byte for byte; 96 -> 110");

    fresh();                                            /* the identity */
    n = frame(f, QD_SX_IDENTITY, 0, 0);
    send(f, n);
    CHECK(reply_ok() && R[5] == QD_SX_R_IDENTITY && R[6] == 1 && rn == 7 + 16 + 2 && !memcmp(R + 7, "FIZZ 0.6.4-p1.5r", 16),
          "IDENTITY: 41, protocol 1, \"FIZZ 0.6.4-p1.5r\" (%d bytes)", rn);

    fresh();                                            /* a round trip: EP with DRIVE 33 into the first free */
    qd_quad_preset_patch(1, p0);
    p0[QD_QP_DRIVE] = 33;
    qd_quad_pack5(p0, blob);
    memcpy(blob2, blob, sizeof blob2);
    blob2[87] = 0;                                      /* (as ChoralRoot's blob: DRIVE in its own byte) */
    s0 = qd_lib_free_slot(&A);
    n = store_msg(f, 0, 0, "MY EP", blob2, 33, 0, 64, 0, 0, 0, 0);
    send(f, n);
    k = rslot();
    CHECK(status() == QD_SX_OK && !R[7] && k == s0 + 1 && !strcmp(LIB(&A, k - 1)->name, "MY EP") &&
              !memcmp(LIB(&A, k - 1)->patch, p0, sizeof p0),
          "STORE, the LIBRARY, the first free (%d): OK, the slot back, the name, the patch byte for byte (DRIVE 33)", k);
    CHECK(LIB(&A, k - 1)->preset >= QD_PRESET_MINE0 && (LIB(&A, k - 1)->flags & QD_SNDF_IMPORTED) &&
              LIB(&A, k - 1)->tags == QD_PKB(QD_PK_KEYS), "P1.5t: one of MY presets, imported, its own id, its kind (KEYS)");
    CHECK(A.ui.tk == QD_TK_IMPORT && !strcmp(A.ui.imp_name, "MY EP") && A.ui.imp_slot == k && !A.ui.imp_pool,
          "the panel: the import's takeover (its name, its slot)");
    qd_quad_unpack(blob, QD_QUAD_BLOB5, p1);
    CHECK(!memcmp(p0, p1, sizeof p0), "(the blob as FIZZ packs it unpacks the same)");
    n = store_msg(f, 0, 7, "SEVEN", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_OK && rslot() == 7 && !strcmp(LIB(&A, 6)->name, "SEVEN") && !LIB(&A, 6)->patch[QD_QP_DRIVE],
          "STORE into LIBRARY 7: there (DRIVE 0)");
    n = store_msg(f, QD_SXF_POOL, 0, "POOLED", blob2, 5, 0, 64, 0, 0, 0, 0);
    s0 = qd_pool_free_slot(&A.proj);
    send(f, n);
    CHECK(status() == QD_SX_OK && R[7] == 1 && rslot() == s0 && !strcmp(POOL(&A.proj, s0)->name, "POOLED") &&
              POOL(&A.proj, s0)->patch[QD_QP_DRIVE] == 5,
          "STORE, the POOL, the first free (%d)", s0);
    {   /* P1.5w: the pool holds references: a MY preset (imported) made, referenced there */
        const int m = qd_lib_find(&A, POOL(&A.proj, s0)->preset);
        CHECK(POOL(&A.proj, s0)->tags == QD_PKB(QD_PK_KEYS) && m >= 0 && !strcmp(LIB(&A, m)->name, "POOLED") &&
                  (LIB(&A, m)->flags & QD_SNDF_IMPORTED) && LIB(&A, m)->patch[QD_QP_DRIVE] == 5,
              ".. P1.5w: one of MY presets (imported, its tag read), referenced at that number");
    }
    n = store_msg(f, QD_SXF_POOL, 1, "ONE", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_OK && !strcmp(POOL(&A.proj, 1)->name, "ONE"), "the POOL's slot 1: as any other (P1.5t)");
    n = store_msg(f, 0, QD_NLIB > 128 ? 200 : QD_NLIB, "TWO HUNDRED", blob2, 0, 0, 64, 0, 0, 0, 0);   /* (the Bluetooth
                                                         * package's 128 places: its last) */
    send(f, n);
    CHECK(status() == QD_SX_OK && rslot() == (QD_NLIB > 128 ? 200 : QD_NLIB) &&
              !strcmp(LIB(&A, (QD_NLIB > 128 ? 200 : QD_NLIB) - 1)->name, "TWO HUNDRED"),
          "LIBRARY %d: MY presets have %d places (P1.5t)", QD_NLIB > 128 ? 200 : QD_NLIB, QD_NLIB);
    n = store_msg(f, 0, QD_NLIB + 1, "PAST", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_BAD_SLOT, "LIBRARY %d, past the last: BAD SLOT", QD_NLIB + 1);
    {   /* P1.5x: the preset keeps the frame's pan and sends (LOAD or not); the tags byte; LOAD applies them */
        int m;
        n = store_msg(f, 0, 0, "WITH MIX", blob2, 0, 0, 64 - 20, 11, 22, 33, QD_SXM_PAN | QD_SXM_CHORUS | QD_SXM_DELAY |
                      QD_SXM_REVERB);
        send(f, n);
        m = rslot() - 1;
        CHECK(status() == QD_SX_OK && LIB(&A, m)->pan == -20 && LIB(&A, m)->chorus == 11 && LIB(&A, m)->delay == 22 &&
                  LIB(&A, m)->reverb == 33 && A.model.t[0].mix.reverb != 33,
              "P1.5x: STORE without LOAD: the preset keeps its pan and sends (the track untouched)");
        n = store_msg(f, 0, 0, "NO MIX", blob2, 0, 0, 64, 9, 9, 9, 0);
        send(f, n);
        m = rslot() - 1;
        CHECK(!LIB(&A, m)->pan && !LIB(&A, m)->chorus && !LIB(&A, m)->delay && !LIB(&A, m)->reverb,
              ".. mixmask 0: no sends, pan centre");
        n = store_msg_tags(f, QD_SXF_LOAD, 0, "TAGGED", blob2, 3, 64 + 10, 40, 50, 60, 0x0F,
                           QD_PKB(QD_PK_PAD) | QD_PKB(QD_PK_FX));
        send(f, n);
        m = rslot() - 1;
        CHECK(status() == QD_SX_OK && LIB(&A, m)->tags == (QD_PKB(QD_PK_PAD) | QD_PKB(QD_PK_FX)) &&
                  LIB(&A, m)->pan == 10 && LIB(&A, m)->reverb == 60 && A.model.t[3].mix.pan == 10 &&
                  A.model.t[3].mix.chorus == 40 && A.model.t[3].mix.delay == 50 && A.model.t[3].mix.reverb == 60,
              "the tags byte (133 bytes): PAD, FX; LOAD: track 4 takes its pan and sends");
        n = store_msg_tags(f, 0, 0, "TAGS ZERO", blob2, 0, 64, 0, 0, 0, 0, 0);
        send(f, n);
        m = rslot() - 1;
        CHECK(status() == QD_SX_OK && LIB(&A, m)->tags == QD_PKB(QD_PK_KEYS), ".. a tags byte 0: the classifier's (KEYS)");
    }
    n = store_msg(f, 0, 257, "X", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_BAD_SLOT, "LIBRARY 257: BAD SLOT");
    for (k = 0; k < QD_NLIB; k++)                       /* the library full */
        if (!LIB(&A, k)->name[0]) {
            qd_sound_t s = *LIB(&A, k);
            strcpy(s.name, "FULL");
            qd_lib_write(&A, k, &s);
        }
    n = store_msg(f, 0, 0, "NO ROOM", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_FULL && rslot() == 0, "the LIBRARY full: FULL");
    n = store_msg(f, QD_SXF_POOL, 0, "NO ROOM", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_FULL, "P1.5w: the LIBRARY full: a POOL store FULL too (it needs a MY preset)");
    n = store_msg(f, 0, 9, "OVER", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_OK && !strcmp(LIB(&A, 8)->name, "OVER"), ".. a slot given: overwritten");

    fresh();                                            /* the errors: nothing stored */
    s0 = qd_lib_free_slot(&A);
    n = store_msg(f, 0, 0, "BAD SUM", blob2, 0, 0, 64, 0, 0, 0, 0);
    f[n - 1] ^= 1;
    send(f, n);
    CHECK(status() == QD_SX_BAD_CHECKSUM && qd_lib_free_slot(&A) == s0, "a bad checksum: BAD CHECKSUM, nothing stored");
    n = store_msg(f, 0, 0, "SHORT", blob2, 0, 0, 64, 0, 0, 0, 0);
    f[n - 3] = (uint8_t)((f[n - 3] + f[n - 2]) & 0x7F);  /* (one byte less, the sum kept) */
    f[n - 2] = f[n - 1];
    send(f, n - 1);
    CHECK(status() == QD_SX_BAD_LENGTH && qd_lib_free_slot(&A) == s0, "one byte short: BAD LENGTH");
    blob2[0] = 'X';
    n = store_msg(f, 0, 0, "NOT FM", blob2, 0, 0, 64, 0, 0, 0, 0);
    blob2[0] = blob[0];
    send(f, n);
    CHECK(status() == QD_SX_BAD_PATCH && qd_lib_free_slot(&A) == s0, "not an FM TONE blob: BAD PATCH");
    n = frame(f, 0x2F, 0, 0);
    send(f, n);
    CHECK(reply_ok() && R[5] == QD_SX_R_ERROR && R[6] == QD_SX_BAD_COMMAND && R[7] == 0x2F, "an unknown command: 7F 07 2F");
    n = frame(f, QD_SX_IDENTITY, 0, 0);
    f[3] = 2;
    send(f, n);
    CHECK(reply_ok() && R[5] == QD_SX_R_ERROR && R[6] == QD_SX_BAD_VERSION && R[7] == 2, "protocol 2: 7F 06 02");
    f[0] = 0x7E;
    CHECK(send(f, n) == 0, "another's SysEx: no reply");

    fresh();                                            /* undo: a level a message; a batch one */
    run(600);
    n = store_msg(f, 0, 0, "ONE", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    run(600);
    k = rslot();
    qd_undo_step(&A, -1);
    CHECK(!LIB(&A, k - 1)->name[0], "undo: the import gone");
    qd_undo_step(&A, 1);
    CHECK(!strcmp(LIB(&A, k - 1)->name, "ONE"), "redo: back");
    s0 = qd_undo_count(&A, -1);
    for (i = 0; i < 4; i++) {
        char nm[8];
        snprintf(nm, sizeof nm, "B%u", (unsigned)i);
        n = store_msg(f, i ? QD_SXF_BATCH : 0, 0, nm, blob2, 0, 0, 64, 0, 0, 0, 0);
        send(f, n);
        run(20);
    }
    run(600);
    ok = qd_undo_count(&A, -1) == s0 + 1;
    qd_undo_step(&A, -1);
    for (s0 = 0, k = 0; k < QD_NLIB; k++) {
        ok &= !(LIB(&A, k)->name[0] == 'B' && LIB(&A, k)->name[2] == 0);
        s0 += !strcmp(LIB(&A, k)->name, "ONE");
    }
    CHECK(ok && s0 == 1, "a batch of four (BATCH on the last three): one undo level, all four gone, ONE kept");

    fresh();                                            /* onto a track, its mix */
    n = store_msg(f, QD_SXF_POOL | QD_SXF_LOAD, 0, "LOADED", blob, 20, 2, 40, 10, 70, 90, QD_SXM_PAN | QD_SXM_DELAY |
                  QD_SXM_REVERB);
    k = A.model.t[2].mix.chorus;
    send(f, n);
    run(5);
    CHECK(status() == QD_SX_OK && A.model.t[2].sound == rslot() && !strcmp(A.model.t[2].sound_name, "LOADED") &&
              A.model.t[2].patch[QD_QP_DRIVE] == 20 && A.model.t[2].mix.pan == -24 && A.model.t[2].mix.delay == 70 &&
              A.model.t[2].mix.reverb == 90 && A.model.t[2].mix.chorus == k &&
              !strcmp(qd_proj_pat(&A.proj, A.seq.cur)->snd[2].name, "LOADED"),
          "LOAD onto track 3: its sound (the pool's), pan -24, delay 70, reverb 90, chorus untouched (not in the mask)");
    {   /* P1.6l: LABELS: asked, its reply (the count, 49 labels); subscribed: a reply by itself when they change, none
         * when they do not; unsubscribed: none */
        static uint8_t LR[QD_SX_FRAME_MAX];
        const uint8_t sub = QD_SXL_SUB, unsub = 0;
        char l[QD_LBL_N][QD_LBL_LEN];
        int ln, c0, ok2 = 1, j, sum2 = 0;
        fresh();
        qd_app_button(&A, QD_B_HOME, 1);                /* (awake: the step view) */
        run(20);
        qd_app_button(&A, QD_B_HOME, 0);
        run(20);
        n = frame(f, QD_SX_LABELS, &sub, 1);
        qd_app_sysex(&A, f, n);
        ln = qd_app_sysex_take(&A, LR, sizeof LR);
        for (j = 5; j < ln - 1; j++)
            sum2 += LR[j];
        ok2 = ln > 60 && LR[5] == QD_SX_R_LABELS && LR[8] == QD_LBL_N && !(sum2 & 0x7F) && LR[ln - 1] == 0xF7;
        {   /* the labels back */
            int o2 = 9;
            for (k = 0; k < QD_LBL_N && ok2; k++) {
                const int len = LR[o2++];
                ok2 &= len <= 10 && o2 + len < ln;
                memcpy(l[k], LR + o2, (size_t)len);
                l[k][len] = 0;
                o2 += len;
            }
        }
        c0 = LR[6] << 7 | LR[7];
        CHECK(ok2 && !strcmp(l[QD_LBL_W0], "STEP 1") && !strcmp(l[QD_LBL_B0 + 1], "VEL") && !strcmp(l[QD_LBL_BTN0 + QD_B_PLAY], "PLAY"),
              "LABELS: 60, the count %d, 49 labels (W1 %s, B2 %s, PLAY %s), %d bytes", c0, l[QD_LBL_W0], l[QD_LBL_B0 + 1],
              l[QD_LBL_BTN0 + QD_B_PLAY], ln);
        run(200);
        CHECK(!qd_app_sysex_peek(&A), "subscribed, nothing changed: no reply");
        qd_app_button(&A, QD_B_FUNC, 1);                /* SHIFT down: they change */
        run(60);
        ln = qd_app_sysex_take(&A, LR, sizeof LR);
        CHECK(ln > 60 && LR[5] == QD_SX_R_LABELS && (LR[6] << 7 | LR[7]) == c0 + 1 && !memcmp(LR + 9, "\x05TRACK", 6),
              "SHIFT down: a LABELS reply by itself within 50 ms, the count %d, SELECT TRACK", LR[6] << 7 | LR[7]);
        run(100);
        CHECK(!qd_app_sysex_peek(&A), "SHIFT held: no more");
        n = frame(f, QD_SX_LABELS, &unsub, 1);           /* unsubscribed: asked once, then none */
        qd_app_sysex(&A, f, n);
        ln = qd_app_sysex_take(&A, LR, sizeof LR);
        qd_app_button(&A, QD_B_FUNC, 0);
        run(200);
        CHECK(ln > 60 && !qd_app_sysex_peek(&A), "unsubscribed: its reply, then none when they change");
    }
    {   /* P1.6m: LIST and DUMP: the list after a few stores; each dumped and stored back into an empty library (the patch,
         * the name, the tags, the sends the same); a dump echoed: ignored; the factory's; a full 256 export round trip */
        static uint8_t D[300][QD_SX_FRAME_MAX];
        static int dn[300];
        static qd_sound_t want[QD_NLIB];
        static uint8_t LR[QD_SX_FRAME_MAX];
        int nd = 0, j, total = -1, got = 0, ok3 = 1, last = 0, g;
        uint32_t seed = 4242u;
        fresh();
        for (k = 0; k < 3; k++) {                       /* three of MY presets: 2, 5, 9; random patches, their mix */
            int16_t pp[QD_QUAD_NP];
            char nm[16];
            for (j = 0; j < QD_QUAD_NP; j++) {
                int16_t lo, hi;
                qd_quad_value_range(j, &lo, &hi, 0);
                seed = seed * 1664525u + 1013904223u;
                pp[j] = (int16_t)(lo + (int)((seed >> 8) % (uint32_t)(hi - lo + 1)));
            }
            snprintf(nm, sizeof nm, "LIST %d", k + 1);
            qd_lib_store(&A, k == 0 ? 1 : k == 1 ? 4 : 8, nm, pp);
            {
                const int at = k == 0 ? 1 : k == 1 ? 4 : 8;
                qd_sound_t s = *LIB(&A, at);
                s.pan = (int8_t)(-20 + k * 10);
                s.reverb = (uint8_t)(30 + k);
                s.tags = (uint8_t)(1 << k);
                qd_lib_write(&A, at, &s);
            }
        }
        {
            const uint8_t lf = 0;
            n = frame(f, QD_SX_LIST, &lf, 1);
        }
        qd_app_sysex(&A, f, n);
        for (g = 0; g < 50 && !last; g++) {
            int ln;
            while (!last && (ln = qd_app_sysex_take(&A, LR, sizeof LR)) > 0) {
                int e, o2 = 11;
                ok3 &= LR[5] == QD_SX_R_LIST;
                total = LR[8] << 7 | LR[9];
                last = LR[7];
                for (e = 0; e < LR[10]; e++, o2 += QD_SX_ENTRY) {
                    const int slot = LR[o2 + 1] << 7 | LR[o2 + 2];
                    char nm[13];
                    memcpy(nm, LR + o2 + 6, 12);
                    nm[12] = 0;
                    ok3 &= LR[o2] == QD_SXS_MINE && !strncmp(nm, LIB(&A, slot - 1)->name, strlen(LIB(&A, slot - 1)->name)) &&
                           LR[o2 + 18] == LIB(&A, slot - 1)->tags;
                    got++;
                }
            }
            run(1);
        }
        CHECK(ok3 && last && total == 3 && got == 3, "LIST: three of MY presets (slots, names, tags), one part, last (%d %d)",
              total, got);
        {
            const uint8_t lf = QD_SXL_FACTORY | QD_SXL_POOL;
            int pools = 0;
            for (j = 0; j < QD_NSOUND; j++)
                pools += POOL(&A.proj, j + 1)->name[0] != 0;
            n = frame(f, QD_SX_LIST, &lf, 1);
            qd_app_sysex(&A, f, n);
            for (got = 0, last = 0, g = 0; g < 100 && !last; g++) {
                int ln;
                while (!last && (ln = qd_app_sysex_take(&A, LR, sizeof LR)) > 0)
                    got += LR[10], total = LR[8] << 7 | LR[9], last = LR[7];
                run(1);
            }
            CHECK(last && total == 3 + QD_NFACTORY + pools && got == total, "LIST with the factory and the pool: %d "
                  "(3 + %d + %d)", got, QD_NFACTORY, pools);
        }
        for (k = 0; k < 3; k++) {                       /* DUMP each */
            const int slot = k == 0 ? 2 : k == 1 ? 5 : 9;
            const uint8_t db[3] = {QD_SXS_MINE, 0, (uint8_t)slot};
            n = frame(f, QD_SX_DUMP, db, 3);
            qd_app_sysex(&A, f, n);
            dn[nd] = qd_app_sysex_take(&A, D[nd], sizeof D[nd]);
            want[k] = (*LIB(&A, slot - 1));
            nd++;
        }
        CHECK(dn[0] > 100 && (D[0][5] == QD_SX_STORE || D[0][5] == QD_SX_STORE6) && (D[0][6] & QD_SXF_DUMP) &&
                  (D[0][7] << 7 | D[0][8]) == 2, "DUMP: a STORE / STORE6 frame, the DUMP flag, slot 2 (%d bytes)", dn[0]);
        qd_app_sysex(&A, D[0] + 1, (uint32_t)dn[0] - 2u);   /* echoed back as it is: ignored */
        CHECK(!qd_app_sysex_peek(&A), "the dump echoed as it is: not stored, no reply");
        {
            const uint8_t db[3] = {QD_SXS_MINE, 0, 3};
            n = frame(f, QD_SX_DUMP, db, 3);
            send(f, n);
            CHECK(reply_ok() && R[5] == QD_SX_R_ERROR && R[6] == QD_SX_BAD_SLOT, "DUMP of an empty place: BAD SLOT");
        }
        fresh();                                        /* stored back into an empty library */
        for (ok3 = 1, k = 0; k < 3; k++) {
            const int slot = D[k][7] << 7 | D[k][8];
            int sum3 = 0;
            D[k][6] &= (uint8_t)~QD_SXF_DUMP;
            for (j = 5; j < dn[k] - 2; j++)
                sum3 += D[k][j];
            D[k][dn[k] - 2] = (uint8_t)((0x80 - (sum3 & 0x7F)) & 0x7F);
            send(D[k] + 1, (uint32_t)dn[k] - 2u);
            ok3 &= status() == QD_SX_OK && !strcmp(LIB(&A, slot - 1)->name, want[k].name) &&
                   !memcmp(LIB(&A, slot - 1)->patch, want[k].patch, sizeof want[k].patch) &&
                   LIB(&A, slot - 1)->tags == want[k].tags && LIB(&A, slot - 1)->pan == want[k].pan &&
                   LIB(&A, slot - 1)->reverb == want[k].reverb && LIB(&A, slot - 1)->chorus == want[k].chorus;
        }
        CHECK(ok3, "stored back (the flag cleared): the same places, names, patches (110 values), tags, pan, sends");
        {   /* a factory preset: back as a new MY preset */
            const uint8_t db[3] = {QD_SXS_FACTORY, 0, 7};
            qd_sound_t fs;
            int m;
            n = frame(f, QD_SX_DUMP, db, 3);
            qd_app_sysex(&A, f, n);
            dn[0] = qd_app_sysex_take(&A, D[0], sizeof D[0]);
            qd_factory_get(&A, 6, &fs);
            D[0][6] &= (uint8_t)~QD_SXF_DUMP;
            {
                int sum3 = 0;
                for (j = 5; j < dn[0] - 2; j++)
                    sum3 += D[0][j];
                D[0][dn[0] - 2] = (uint8_t)((0x80 - (sum3 & 0x7F)) & 0x7F);
            }
            m = qd_lib_free_slot(&A);
            send(D[0] + 1, (uint32_t)dn[0] - 2u);
            CHECK(status() == QD_SX_OK && (D[0][7] << 7 | D[0][8]) == 0 && !strcmp(LIB(&A, m)->name, fs.name) &&
                      !memcmp(LIB(&A, m)->patch, fs.patch, sizeof fs.patch) && LIB(&A, m)->reverb == fs.reverb,
                  "a factory preset's dump (%s): stored back as a new MY preset, the same sound and sends", fs.name);
        }
        fresh();                                        /* the full library: 256, DUMP them all, paced */
        for (k = 0; k < QD_NLIB; k++) {
            int16_t pp[QD_QUAD_NP];
            char nm[16];
            for (j = 0; j < QD_QUAD_NP; j++) {
                int16_t lo, hi;
                qd_quad_value_range(j, &lo, &hi, 0);
                seed = seed * 1664525u + 1013904223u;
                pp[j] = (int16_t)(lo + (int)((seed >> 8) % (uint32_t)(hi - lo + 1)));
            }
            if (k % 3 == 0)                             /* (a third with every value 74.. at its default: STORE) */
                for (j = 74; j < QD_QUAD_NP; j++)
                    qd_quad_value_range(j, 0, 0, &pp[j]);
            snprintf(nm, sizeof nm, "ALL %03d", k + 1);
            qd_lib_store(&A, k, nm, pp);
            {
                qd_sound_t s = *LIB(&A, k);
                s.pan = (int8_t)((int)(seed % 128u) - 64);
                s.chorus = (uint8_t)(k % 128), s.delay = (uint8_t)((k * 7) % 128);
                s.reverb = (uint8_t)((k * 13) % 128), s.tags = (uint8_t)(1 + k % 127);
                qd_lib_write(&A, k, &s);
            }
            want[k] = (*LIB(&A, k));
        }
        {
            const uint8_t db[3] = {QD_SXS_MINE, 0, 0};
            int five = 0, six = 0;
            n = frame(f, QD_SX_DUMP, db, 3);
            qd_app_sysex(&A, f, n);
            for (nd = 0, g = 0; g < 2000 && (qd_app_sysex_busy(&A) || qd_app_sysex_peek(&A)); g++) {
                while (nd < 300 && (dn[nd] = qd_app_sysex_take(&A, D[nd], sizeof D[nd])) > 0) {
                    five += D[nd][5] == QD_SX_STORE, six += D[nd][5] == QD_SX_STORE6;
                    nd++;
                }
                run(1);
            }
            CHECK(nd == QD_NLIB && five > 50 * QD_NLIB / 256 && six > 100 * QD_NLIB / 256, "DUMP all of MY PRESETS: %d "
                  "frames, paced (%d STORE, %d STORE6) in %d ms", nd, five, six, g);
        }
        fresh();
        for (ok3 = 1, k = 0; k < nd; k++) {
            const int slot = D[k][7] << 7 | D[k][8];
            int sum3 = 0;
            D[k][6] &= (uint8_t)~QD_SXF_DUMP;
            for (j = 5; j < dn[k] - 2; j++)
                sum3 += D[k][j];
            D[k][dn[k] - 2] = (uint8_t)((0x80 - (sum3 & 0x7F)) & 0x7F);
            send(D[k] + 1, (uint32_t)dn[k] - 2u);
            ok3 &= status() == QD_SX_OK && slot == k + 1 && !strcmp(LIB(&A, k)->name, want[k].name) &&
                   !memcmp(LIB(&A, k)->patch, want[k].patch, sizeof want[k].patch) && LIB(&A, k)->tags == want[k].tags &&
                   LIB(&A, k)->pan == want[k].pan && LIB(&A, k)->chorus == want[k].chorus &&
                   LIB(&A, k)->delay == want[k].delay && LIB(&A, k)->reverb == want[k].reverb;
        }
        CHECK(ok3, "the 256 stored back into an empty library: every one the same (name, patch, tags, pan, sends)");
    }
    phase1_tests();
    p2b_tests();
    msnd_tests();
    browse_tests();
    qd_shutdown();
    printf("test_sysex: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
