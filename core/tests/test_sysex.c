/* SPDX-License-Identifier: GPL-3.0-only */
/* P1.5r: FIZZ's SysEx (core/app/fizz_sx.h, docs/SYSEX.md): the 7-bit packing; the identity; a sound stored into the
 * LIBRARY and the POOL from a known blob and back byte for byte (its DRIVE), a slot given, the first free, the library
 * full, the pool's slot 1 refused; a bad checksum, a bad length, a bad patch, a bad version, an unknown command; the
 * takeover; undo (one level a message, a batch one); onto a track with its mix.
 *
 *     test_sysex                 the tests (exit 1 on a failed check)
 *     test_sysex --feed F.syx    each frame of F into a fresh FIZZ: its reply (hex) and the sound stored ("name",
 *                                its 74 values), for tools/test_sysex_dnsnd.mjs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "qd_gfx.h"
#include "sounds.h"

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
static uint8_t R[64];
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
                const qd_sound_t *snd = R[7] ? &A.proj.sound[slot - 1] : &A.per.lib[slot - 1];
                printf("name \"%s\"\nvalues", snd->name);
                for (k = 0; k < QD_QUAD_NP; k++)
                    printf(" %d", snd->patch[k]);
                printf("\n");
            }
        }
    }
}

int main(int argc, char **argv)
{
    static uint8_t f[256], in[256], pk[300], un[300], blob[QD_QUAD_BLOB], blob2[QD_QUAD_BLOB];
    static int16_t p0[QD_QUAD_NP], p1[QD_QUAD_NP];
    uint32_t n, i;
    int ok, k, s0;
    if (!qd_init()) {
        printf("test_sysex: qd_init failed\n");
        return 1;
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
    qd_quad_pack(p0, blob);
    memcpy(blob2, blob, sizeof blob2);
    blob2[87] = 0;                                      /* (as ChoralRoot's blob: DRIVE in its own byte) */
    s0 = qd_lib_free_slot(&A);
    n = store_msg(f, 0, 0, "MY EP", blob2, 33, 0, 64, 0, 0, 0, 0);
    send(f, n);
    k = rslot();
    CHECK(status() == QD_SX_OK && !R[7] && k == s0 + 1 && !strcmp(A.per.lib[k - 1].name, "MY EP") &&
              !memcmp(A.per.lib[k - 1].patch, p0, sizeof p0),
          "STORE, the LIBRARY, the first free (%d): OK, the slot back, the name, the patch byte for byte (DRIVE 33)", k);
    CHECK(A.per.lib[k - 1].preset >= QD_PRESET_MINE0 && (A.per.lib[k - 1].flags & QD_SNDF_IMPORTED) &&
              A.per.lib[k - 1].tags == QD_PKB(QD_PK_KEYS), "P1.5t: one of MY presets, imported, its own id, its kind (KEYS)");
    CHECK(A.ui.tk == QD_TK_IMPORT && !strcmp(A.ui.imp_name, "MY EP") && A.ui.imp_slot == k && !A.ui.imp_pool,
          "the panel: the import's takeover (its name, its slot)");
    qd_quad_unpack(blob, p1);
    CHECK(!memcmp(p0, p1, sizeof p0), "(the blob as FIZZ packs it unpacks the same)");
    n = store_msg(f, 0, 7, "SEVEN", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_OK && rslot() == 7 && !strcmp(A.per.lib[6].name, "SEVEN") && !A.per.lib[6].patch[QD_QP_DRIVE],
          "STORE into LIBRARY 7: there (DRIVE 0)");
    n = store_msg(f, QD_SXF_POOL, 0, "POOLED", blob2, 5, 0, 64, 0, 0, 0, 0);
    s0 = qd_pool_free_slot(&A.proj);
    send(f, n);
    CHECK(status() == QD_SX_OK && R[7] == 1 && rslot() == s0 && !strcmp(A.proj.sound[s0 - 1].name, "POOLED") &&
              A.proj.sound[s0 - 1].patch[QD_QP_DRIVE] == 5,
          "STORE, the POOL, the first free (%d)", s0);
    CHECK(A.proj.sound[s0 - 1].tags == QD_PKB(QD_PK_KEYS) && !A.proj.sound[s0 - 1].preset, ".. its kind read (P1.5t)");
    n = store_msg(f, QD_SXF_POOL, 1, "ONE", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_OK && !strcmp(A.proj.sound[0].name, "ONE"), "the POOL's slot 1: as any other (P1.5t)");
    n = store_msg(f, 0, 200, "TWO HUNDRED", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_OK && rslot() == 200 && !strcmp(A.per.lib[199].name, "TWO HUNDRED"),
          "LIBRARY 200: MY presets have 256 places (P1.5t)");
    n = store_msg(f, 0, 257, "X", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_BAD_SLOT, "LIBRARY 257: BAD SLOT");
    for (k = 0; k < QD_NLIB; k++)                       /* the library full */
        if (!A.per.lib[k].name[0])
            strcpy(A.per.lib[k].name, "FULL");
    n = store_msg(f, 0, 0, "NO ROOM", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_FULL && rslot() == 0, "the LIBRARY full: FULL");
    n = store_msg(f, 0, 9, "OVER", blob2, 0, 0, 64, 0, 0, 0, 0);
    send(f, n);
    CHECK(status() == QD_SX_OK && !strcmp(A.per.lib[8].name, "OVER"), ".. a slot given: overwritten");

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
    n = frame(f, 0x22, 0, 0);
    send(f, n);
    CHECK(reply_ok() && R[5] == QD_SX_R_ERROR && R[6] == QD_SX_BAD_COMMAND && R[7] == 0x22, "an unknown command: 7F 07 22");
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
    CHECK(!A.per.lib[k - 1].name[0], "undo: the import gone");
    qd_undo_step(&A, 1);
    CHECK(!strcmp(A.per.lib[k - 1].name, "ONE"), "redo: back");
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
        ok &= !(A.per.lib[k].name[0] == 'B' && A.per.lib[k].name[2] == 0);
        s0 += !strcmp(A.per.lib[k].name, "ONE");
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
              !strcmp(A.proj.pat[A.seq.cur].snd[2].name, "LOADED"),
          "LOAD onto track 3: its sound (the pool's), pan -24, delay 70, reverb 90, chorus untouched (not in the mask)");
    qd_shutdown();
    printf("test_sysex: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
