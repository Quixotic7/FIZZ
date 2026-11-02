/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FIZZ's update loader (firmware/loader/ldr_core.c) on the host, after fm1-base's tests/ldr_test.c: a simulated
 * host serves a real built package over the SysEx protocol (ota.c's 0x30 reads), the "device" is a simulated 1 MiB
 * flash with the package's head, another app, and a pattern everywhere else (the store, OBJ_BLE, the staged loader
 * and its record, the globals). Checks: the app area and the data extents end up equal to the package's flash.bin,
 * every other byte is as before (but the update record's sector, which step 4 clears), a second run writes nothing,
 * a power cut in the data pass then a rerun completes, a damaged package is refused before any erase.
 *   ldr_test PACKAGE.fwsc            (built with -include the package's build/<name>-ldr_extents.h)
 *   ldr_test --bad PACKAGE.fwsc      (built with -DLDR_X_NO_ASSERT and a bad table: refused, nothing erased)
 * firmware/app/tests/ldr_test.sh builds and runs them all. */
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FELUCCA_ID "ota-FM-1_900"
#define FELUCCA_OTA_DRYRUN 0

static uint8_t nor[0x100000], *logical;
static size_t logical_len;
static uint8_t rx[1024];
static uint32_t rx_len, rx_full, now_ms, requests, erases, progs, bad_range, record_cleared, f0_asked;
static int cut_after = -1;                          /* a power cut at the Nth program call in the data pass */
static jmp_buf cut;

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8;
        while (nb >= 7) { out[o++] = acc & 0x7F; acc >>= 7; nb -= 7; }
    }
    if (nb) out[o++] = acc & 0x7F;
    return o;
}
static uint32_t unpack7h(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 7;
        if (nb >= 8) { out[o++] = (uint8_t)acc; acc >>= 8; nb -= 8; }
    }
    return o;
}

static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], m[600];
    uint32_t addr, len, i, s = 0;
    unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) return 0;
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    requests++;
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0; m[6] = 0;
    memcpy(m + 7, &addr, 4);
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr >= 0xE0000000u) {
        f0_asked += addr == 0xF0000000u;
        memset(m + 14, 0, len);
        memcpy(m + 14, "success", 8);
    } else {
        if (addr + len > logical_len) { printf("read past the package %#x\n", addr); exit(1); }
        memcpy(m + 14, logical + addr, len);
    }
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    rx_len = pack7(m, 15 + len, rx);
    rx_full = 1;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { if (!rx_full) return 0; *p = rx; *n = rx_len; return 1; }
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms++; }
static int ota_erase(uint32_t off) { (void)off; return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }
#include "../../fm1-base/bsp/chip/ota.c"

static int written(uint32_t off, uint32_t n);
static int ldr_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static int ldr_erase(uint32_t off)
{
    if (off & 0xFFFu) bad_range++;
    if (off < 0x4000u) { printf("ERASE IN THE HEAD %#x\n", off); exit(1); }
    if (!written(off, 0x1000u) && off != 0xE4000u) bad_range++;   /* (0xE4000: the record's sector, step 4) */
    erases++;
    memset(nor + off, 0xFF, 0x1000);
    return 0;
}
static int ldr_prog(uint32_t off, const void *p, uint32_t n)
{
    const uint8_t *s = p;
    uint32_t i;
    if (!written(off, n) || ((off & 0xFFu) + n) > 256u) bad_range++;
    if (cut_after >= 0 && off >= 0x97000u && cut_after-- == 0) {
        for (i = 0; i < n / 2; i++) nor[off + i] &= s[i];   /* half a page, then the power goes */
        longjmp(cut, 1);
    }
    progs++;
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static void ldr_record_clear(void) { record_cleared++; }
static void ldr_progress(uint32_t done, uint32_t total) { if (done > total) { printf("progress %u of %u\n", done, total); exit(1); } }
#include "../../loader/ldr_core.c"

static int written(uint32_t off, uint32_t n)        /* inside the app area or one extent (ldr_core.c's table) */
{
    uint32_t r;
    for (r = 0; r < LDR_NX; r++)
        if (off >= ldr_x[r][0] && off + n <= ldr_x[r][1])
            return 1;
    return 0;
}

static uint8_t *load_logical(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *raw = malloc(0x200000), *lg;
    size_t n, i;
    if (!f) { perror(path); exit(2); }
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    lg = malloc(n);
    for (i = 0; i < 20; i++) memcpy(lg + i * 47, raw + i * 48, 47);
    memcpy(lg + 20 * 47, raw + 20 * 48, n - 20 * 48);
    *len = n - 20;
    free(raw);
    return lg;
}

static uint32_t flash_off(const uint8_t *lg, uint32_t *size)   /* flash.bin in the logical image */
{
    uint8_t h[0x400];
    uint32_t i, n;
    memcpy(h, lg, sizeof h);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 0) { *size = ota_rd32(e + 12); return ota_rd32(e + 8); }
    }
    return 0;
}

static uint32_t seed;
static void pattern(uint32_t lo, uint32_t hi)
{
    for (; lo < hi; lo++) {
        seed = seed * 1103515245u + 12345u;
        nor[lo] = (uint8_t)(seed >> 16);
    }
}
static void put_record(void)                         /* a valid update record at 0xE4F00 (Felucca's staging) */
{
    uint8_t r[112] = {0};
    r[2] = 0x0D; r[3] = 0x5A; r[4] = 0x01; r[5] = 0x5A; r[6] = 0x41; r[7] = 0x54;
    ota_wr16(r, ota_crc16(r + 2, 78, 0));
    memcpy(nor + 0xE4F00, r, sizeof r);
}

static int check(const char *what, int ok) { printf("  %-62s %s\n", what, ok ? "ok" : "FAIL"); return ok ? 0 : 1; }

static uint8_t before[0x100000];
static uint32_t fo, fsize;

static int kept(void)                                /* every byte outside the app area and the extents as before */
{
    uint32_t a;
    for (a = 0; a < sizeof nor; a += 0x1000u)
        if (!written(a, 0x1000u) && memcmp(nor + a, before + a, 0x1000)) {
            printf("    sector %#x changed\n", a);
            return 0;
        }
    return 1;
}
static int installed(void)                           /* the app area and the extents are the package's */
{
    uint32_t r;
    for (r = 0; r < LDR_NX; r++)
        if (memcmp(nor + ldr_x[r][0], logical + fo + ldr_x[r][0], ldr_x[r][1] - ldr_x[r][0])) {
            printf("    %#x..%#x differs\n", ldr_x[r][0], ldr_x[r][1]);
            return 0;
        }
    return 1;
}

static void device(int record)                       /* the package's head, another app, a pattern elsewhere */
{
    seed = 0x46495A5Au;
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, logical + fo, 0x4000);
    pattern(0x4000u, sizeof nor);
    if (record)
        put_record();
    memcpy(before, nor, sizeof nor);
}

int main(int argc, char **argv)
{
    int bad = 0, rc, badtab = argc > 2 && !strcmp(argv[1], "--bad");
    uint32_t r, wr = 0;
    if (argc < 2) { printf("usage: ldr_test [--bad] PACKAGE.fwsc\n"); return 2; }
    logical = load_logical(argv[argc - 1], &logical_len);
    fo = flash_off(logical, &fsize);
    printf("%s: flash.bin %u B, %s package, extents:", argv[argc - 1], fsize, FZ_LDR_BLE ? "Bluetooth" : "USB");
    for (r = 1; r < LDR_NX; r++)
        printf(" %#x..%#x", ldr_x[r][0], ldr_x[r][1]);
    printf("\n");
    if (badtab) {
        device(1);
        rc = ldr_session();
        bad += check("an extent table outside the limits refused, nothing erased", rc == -14 && erases == 0 && progs == 0);
        printf("%s\n", bad ? "LOADER TEST FAILED" : "loader test passed");
        return bad != 0;
    }
    for (r = 1; r < LDR_NX; r++)
        wr += ldr_x[r][1] - ldr_x[r][0];
    bad += check("flash.bin reaches 0xFC000; the package has data extents", fsize == 0xFC000u && LDR_NX > 1);

    /* 1. a whole install over another app, with the staging's record */
    device(1);
    rc = ldr_session();
    printf("  rc %d, %u requests, %u sector erases, %u page writes\n", rc, requests, erases, progs);
    bad += check("install completes", rc == 0);
    bad += check("the app area and the data extents == the package's flash.bin", installed());
    bad += check("the update record cleared (RAM + flash)", record_cleared && nor[0xE4F06] == 0xFF);
    memset(before + 0xE4000, 0xFF, 0x1000);          /* (step 4 erased the record's sector: Felucca's own) */
    bad += check("everything else as before (store, OBJ_BLE, 0xE0000.., >= 0xFC000, head)", kept());
    bad += check("finish asked (0xF0000000)", f0_asked >= 1);
    bad += check("no erase or write outside the app area and the extents", bad_range == 0);
    bad += check("every sector of the app area and the extents written", erases == ((0x93000u - 0x4000u + wr) >> 12) + 1u);

    /* 2. the same package again (no record now): nothing written, 0xE0000..0xE4FFF untouched too */
    memcpy(before, nor, sizeof nor);
    pattern(0xE0000u, 0xE5000u);                     /* (no record in it) */
    memcpy(before + 0xE0000, nor + 0xE0000, 0x5000);
    erases = progs = 0;
    rc = ldr_session();
    bad += check("a second run writes nothing (equal sectors skipped)", rc == 0 && erases == 0 && progs == 0);
    bad += check("  and changes nothing", !memcmp(nor, before, sizeof nor));

    /* 3. a power cut in the data pass, then the SPL runs the loader again (the record is still there) */
    for (r = 0; r < 3; r++) {
        int at = r == 0 ? 0 : r == 1 ? (int)(wr / 256u / 2u) : (int)(wr / 256u - 1u);   /* first, middle, last page */
        device(1);
        cut_after = at;
        erases = progs = 0;
        if (!setjmp(cut)) {
            ldr_session();
            bad += check("power cut happened", 0);
            continue;
        }
        cut_after = -1;
        bad += check(r == 0 ? "power cut at the data pass's first page: the record kept" :
                     r == 1 ? "power cut in the middle of the data pass: the record kept" :
                              "power cut at the data pass's last page: the record kept", nor[0xE4F06] == 0x41);
        bad_range = 0;
        rc = ldr_session();
        bad += check("  the rerun completes, the package installed", rc == 0 && installed());
        memset(before + 0xE4000, 0xFF, 0x1000);
        bad += check("  everything else as before", kept() && bad_range == 0 && nor[0xE4F06] == 0xFF);
    }

    /* 4. a damaged package (a byte in a blob): refused before any erase */
    {
        uint32_t at = fo + ldr_x[LDR_NX - 1][0] + 0x123u;
        logical[at] ^= 0x5A;
        device(1);
        erases = progs = 0;
        rc = ldr_session();
        bad += check("a damaged blob in the package refused, nothing erased", rc == -12 && erases == 0 && progs == 0);
        logical[at] ^= 0x5A;
    }
    /* 5. a package for another chip key: refused before any erase (Felucca's check, kept) */
    {
        uint8_t save = logical[fo + 0x4000 + 5];
        logical[fo + 0x4000 + 5] ^= 0x5A;
        device(1);
        erases = progs = 0;
        rc = ldr_session();
        bad += check("foreign key / damaged app head refused, nothing erased", rc == -6 && erases == 0);
        logical[fo + 0x4000 + 5] = save;
    }
    printf("%s\n", bad ? "LOADER TEST FAILED" : "loader test passed");
    return bad != 0;
}
