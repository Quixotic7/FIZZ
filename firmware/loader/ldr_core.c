/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FIZZ's copy (firmware/loader/, 2026-11-02) of fm1-base's bsp/loader/ldr_core.c, changed only so that a package
 * also writes FIZZ's data blobs: after the app area, step 3 goes on through the package's data extents (LDR_XTAB,
 * which firmware/build.py generates per package from the blobs' sizes: whole sectors, each blob and its trailer),
 * sector by sector exactly as for the app area (the CRC16s kept in 2b, equal sectors skipped, verify, two tries).
 * The extents' limits (LDR_X_OK) are checked when this compiles and again before anything is erased. An extent
 * sector that holds another firmware's update record is written first (ldr_next). Everything else is Felucca's. */
/* Felucca update loader, the portable part (step 2 of the M-UPGRADE update
 * protocol).
 *
 * The host serves the package ("logical image") with cmd 0x30 reads (ota.c).
 * The loader:
 *   1. reads the UFW header and finds flash.bin;
 *   2. checks that the package's app area decrypts with THIS chip's key
 *      (taken from isd_config.ini in the device's own flash head), so a
 *      package for another key is refused before anything is erased;
 *   2b. reads the whole flash.bin once and checks it against the CRC16 of its
 *      UFW entry (the vendor tools' check), keeping each app sector's CRC16:
 *      a damaged or truncated package is refused before anything is erased;
 *   3. writes ONLY the app area [0x4000, 0x93000), then (FIZZ) the data extents, sector by sector, skipping
 *      sectors that are already equal, and verifies each one; a sector that
 *      comes in different from the checked pass is read again, never written;
 *      the flash head (SPL, isd_config) is never written;
 *   4. asks 0xF0000000 ("success"), then invalidates the update record (RAM
 *      and flash: 4K boundary - 256 between 0x93000 and 0xFC000, outside
 *      Felucca's store, where only user data lives) so the SPL boots the new
 *      app, and resets.
 * Power loss during 3: the record is still there, the SPL runs the loader
 * again on the next power-on and the host can resume.
 *
 * Hooks from the platform (loader.c, ldr_test.c):
 *   ldr_fread(off, p, n)  ldr_erase(off)  ldr_prog(off, p, n)   flash, 0 = ok
 *   ldr_record_clear()    forget the RAM update record
 *   ldr_progress(done, total)  both passes on one scale: the check (2b, flash.bin) then the write (3, the app area)
 * plus the ota.c hooks (frames, time, idle). */
#define LDR_APP_LO 0x4000u
#define LDR_APP_HI 0x93000u
#define LDR_REC_LO 0x93000u                     /* update records live above the app ... */
#define LDR_REC_HI 0xFC000u                     /* ... and below Felucca's globals (Felucca's records: 0xE4F00, */
#define LDR_DATA_LO 0x97000u                    /* the stock firmware's: 0xE8F00), never in Felucca's store, whose */
#define LDR_DATA_HI 0xE0000u                    /* bytes (user samples) the host writes */
#define LDR_NSEC ((LDR_X_HI - LDR_APP_LO) >> 12)   /* (FIZZ: sc[] covers the extents too) */
#define LDR_FL_MAX 0x100000u                    /* flash.bin: at most the 1 MiB part */

/* FIZZ: the data extents, LDR_XTAB(X): X(lo, hi) each, in order (firmware/build.py's ldr_extents.h, with
 * FZ_LDR_BLE: the Bluetooth package). An extent is whole sectors inside [0x97000, 0xFC000) and never touches the
 * staged loader and its record [0xE0000, 0xE5000), the package's store (USB [0x97000, 0xE0000), Bluetooth
 * [0xAF000, 0xE0000)) or, for Bluetooth, OBJ_BLE [0xE5000, 0xE7000) (the radio's bonds) */
#define LDR_X_LO 0x97000u
#define LDR_X_HI 0xFC000u
#define LDR_OVER(lo, hi, a, b) ((lo) < (b) && (a) < (hi))
#if FZ_LDR_BLE
#define LDR_KEEP(lo, hi) (LDR_OVER(lo, hi, 0xAF000u, 0xE0000u) || LDR_OVER(lo, hi, 0xE5000u, 0xE7000u))
#else
#define LDR_KEEP(lo, hi) LDR_OVER(lo, hi, 0x97000u, 0xE0000u)
#endif
#define LDR_X_OK(lo, hi) ((lo) < (hi) && !((lo) & 0xFFFu) && !((hi) & 0xFFFu) && (lo) >= LDR_X_LO && \
                          (hi) <= LDR_X_HI && !LDR_OVER(lo, hi, 0xE0000u, 0xE5000u) && !LDR_KEEP(lo, hi))
#ifndef LDR_X_NO_ASSERT                         /* (only the host test leaves it out: a bad table, refused at run time) */
#define LDR_X_ASSERT(lo, hi) _Static_assert(LDR_X_OK(lo, hi), "a data extent outside its limits");
LDR_XTAB(LDR_X_ASSERT)
#endif
#define LDR_X_ROW(lo, hi) {lo, hi},
static const uint32_t ldr_x[][2] = {{LDR_APP_LO, LDR_APP_HI}, LDR_XTAB(LDR_X_ROW)};   /* what step 3 writes */
#define LDR_NX (sizeof ldr_x / sizeof ldr_x[0])

static int ldr_x_check(uint32_t fl_size)        /* the extents again, in order, inside flash.bin: 0 = ok */
{
    uint32_t r;
    for (r = 1; r < LDR_NX; r++)
        if (!LDR_X_OK(ldr_x[r][0], ldr_x[r][1]) || ldr_x[r][0] < ldr_x[r - 1][1] || ldr_x[r][1] > fl_size)
            return -1;
    return 0;
}

static int ldr_in(uint32_t off)                 /* off in the app area or an extent */
{
    uint32_t r;
    for (r = 0; r < LDR_NX; r++)
        if (off >= ldr_x[r][0] && off < ldr_x[r][1])
            return 1;
    return 0;
}

static uint32_t ldr_wr(void)                    /* step 3's bytes: the app area and the extents */
{
    uint32_t r, n = 0;
    for (r = 0; r < LDR_NX; r++)
        n += ldr_x[r][1] - ldr_x[r][0];
    return n;
}

static int ldr_fread(uint32_t off, void *p, uint32_t n);
static int ldr_erase(uint32_t off);
static int ldr_prog(uint32_t off, const void *p, uint32_t n);
static void ldr_record_clear(void);
static void ldr_progress(uint32_t done, uint32_t total);

/* FIZZ: step 3's order. The app area; then the extent sectors that held a valid update record when the pass began
 * (another firmware's staging, e.g. the stock firmware's record at 0xE8F00: once the app is whole, its record goes
 * before anything it stages is touched, so a power cut either resumes or boots the new app); then the rest */
static uint8_t ldr_rec[LDR_NSEC];               /* by sector, as sc[]: held a record before step 3 */

static int ldr_is_rec(uint32_t s)               /* step 4's test: a record at the sector's last 256 bytes */
{
    uint8_t r[80];
    if (ldr_fread(s + 0xF00u, r, 80))
        return 0;
    return ota_rd16(r + 6) == 0x5441u && ota_rd16(r) && ota_rd16(r) == ota_crc16(r + 2, 78, 0);
}

static uint32_t ldr_after(uint32_t s, uint8_t rec)   /* the first extent sector above s with ldr_rec == rec; 0: none */
{
    uint32_t r, t;
    for (r = 1; r < LDR_NX; r++)
        for (t = ldr_x[r][0]; t < ldr_x[r][1]; t += 0x1000u)
            if (t > s && ldr_rec[(t - LDR_APP_LO) >> 12] == rec)
                return t;
    return 0u;
}

static uint32_t ldr_next(uint32_t s)            /* the sector step 3 writes after s; 0: none */
{
    uint32_t n;
    if (s + 0x1000u < LDR_APP_HI)
        return s + 0x1000u;                      /* the app area */
    if (s < LDR_APP_HI || ldr_rec[(s - LDR_APP_LO) >> 12])
        return (n = ldr_after(s, 1)) != 0u ? n : ldr_after(0u, 0);   /* the records' sectors, then the rest */
    return ldr_after(s, 0);
}

/* SFC cipher: per 32-byte block, key ^ (block offset >> 2) */
static void ldr_sfc(uint8_t *p, uint32_t n, uint32_t base_off, uint32_t key)
{
    uint32_t i, j;
    for (i = 0; i < n; i += 32u) {
        uint32_t k = key ^ ((base_off + i) >> 2);
        for (j = 0; j < 32u && i + j < n; j++) {
            p[i + j] ^= (uint8_t)k;
            k = ((k << 1) ^ (k & 0x8000u ? 0x1021u : 0u)) & 0xFFFFu;
        }
    }
}

/* chip key from the device's flash head: JLFS top entries (ENC 0xFFFF) -> isd_config.ini blob */
static int ldr_chip_key(uint32_t *key)
{
    uint8_t e[32], blob[34];
    uint32_t off, i, sum, k;
    for (off = 32; off < 0x400u; off += 32u) {
        if (ldr_fread(off, e, 32))
            return -1;
        ota_jl_enc(e, 32);
        if (ota_crc16(e + 2, 30, 0) != ota_rd16(e))
            return -2;
        if (!ota_memeq(e + 16, (const uint8_t *)"isd_config.ini", 15)) {
            if (ota_rd16(e + 14))
                return -3;                               /* last entry, not found */
            continue;
        }
        if (ldr_fread(ota_rd32(e + 4), blob, 34) || ota_crc16(blob, 32, 0) != ota_rd16(blob + 32))
            return -4;
        for (i = 0, sum = 0; i < 16u; i++)
            sum += blob[i];
        sum &= 0xFFu;
        sum = sum >= 0xE0u ? 0xAAu : sum <= 0x10u ? 0x55u : sum;
        for (i = 0, k = 0; i < 16u; i++)
            if ((uint32_t)(blob[16 + i] ^ blob[15 - i]) < sum)
                k |= 1u << i;
        *key = k;
        return 0;
    }
    return -3;
}

/* 2b. flash.bin [0, fl_size) from the host, before any erase: its CRC16 must be the UFW entry's (want); sc gets
 * the CRC16 of each app sector as it came in */
static int ldr_check(uint32_t fl_off, uint32_t fl_size, uint32_t want, uint16_t *sc, uint8_t *b)
{
    uint32_t i, n, crc = 0;
    for (i = 0; i < fl_size; i += n) {
        n = fl_size - i > 512u ? 512u : fl_size - i;
        if (ota_read(fl_off + i, b, n))
            return -7;
        crc = ota_crc16(b, n, crc);
        if (!((i + n) & 0xFFFu))
            ldr_progress(i + n, fl_size + ldr_wr());
        if (ldr_in(i)) {                                 /* (512-byte reads from 0: never across a sector) */
            uint32_t k = (i - LDR_APP_LO) >> 12;
            sc[k] = (uint16_t)ota_crc16(b, n, (i & 0xFFFu) ? sc[k] : 0u);
        }
    }
    return crc == want ? 0 : -12;
}

static int ldr_session(void)
{
    static uint8_t hdr[0x400], sec[0x1000], cur[0x1000];
    static uint16_t sc[LDR_NSEC];
    uint32_t i, k, fl_off, fl_size, ota_off, ota_len, key, s, want = ~0u, done = 0;   /* (no CRC16: no flash.bin entry) */
    int rc;
    /* 1. UFW header + entry list (ota.c; it leaves the entries decoded in hdr) */
    if ((rc = ota_ufw(hdr, &fl_off, &fl_size, &ota_off, &ota_len)) != 0)
        return rc;
    if (!fl_off || fl_size < LDR_APP_HI || fl_size > LDR_FL_MAX)
        return -3;
    if (ldr_x_check(fl_size))
        return -14;                                      /* (FIZZ) an extent outside its limits or flash.bin */
    for (i = 0; i < ota_rd16(hdr + 8); i++)
        if (ota_rd16(hdr + 0x40 + i * 0x50u) == 0)
            want = ota_rd16(hdr + 0x40 + i * 0x50u + 4u);   /* flash.bin's data CRC16 */
    /* 2. the package's app area must decrypt with this chip's key */
    if ((rc = ldr_chip_key(&key)) != 0)
        return -40 + rc;
    if (ota_read(fl_off + LDR_APP_LO, sec, 32))
        return -5;
    ldr_sfc(sec, 32, 0, key);
    if (ota_crc16(sec + 2, 30, 0) != ota_rd16(sec))
        return -6;                                       /* package for another chip key */
    /* 2b. the whole package first: nothing is erased for a damaged one */
    if ((rc = ldr_check(fl_off, fl_size, want, sc, sec)) != 0)
        return rc;
    /* 3. app area, then (FIZZ) the data extents, sector by sector: those holding an update record first (ldr_next) */
    for (k = 1; k < LDR_NX; k++)
        for (s = ldr_x[k][0]; s < ldr_x[k][1]; s += 0x1000u)
            ldr_rec[(s - LDR_APP_LO) >> 12] = (uint8_t)ldr_is_rec(s);
    for (s = LDR_APP_LO; s; s = ldr_next(s)) {
        uint32_t again;
        for (again = 0;; again++) {                      /* as checked in 2b, else read it again */
            for (k = 0; k < 0x1000u; k += 512u)
                if (ota_read(fl_off + s + k, sec + k, 512))
                    return -7;
            if (ota_crc16(sec, 0x1000u, 0) == sc[(s - LDR_APP_LO) >> 12])
                break;
            if (again == 2u)
                return -13;
        }
        if (ldr_fread(s, cur, 0x1000u))
            return -8;
        if (!ota_memeq(sec, cur, 0x1000u)) {
            uint32_t tries;
            for (tries = 0; tries < 2u; tries++) {
                if (ldr_erase(s))
                    return -9;
                for (k = 0; k < 0x1000u; k += 256u)
                    if (ldr_prog(s + k, sec + k, 256))
                        return -10;
                if (ldr_fread(s, cur, 0x1000u))
                    return -8;
                if (ota_memeq(sec, cur, 0x1000u))
                    break;
            }
            if (tries == 2u)
                return -11;                              /* verify failed twice */
        }
        done += 0x1000u;
        ldr_progress(fl_size + done, fl_size + ldr_wr());
    }
    /* 4. finish: the host confirms, the record goes, the SPL boots the app */
    for (i = 0; i < 4u; i++)
        if (!ota_read(0xF0000000u, sec, 8))
            break;
    ldr_record_clear();
    for (s = LDR_REC_HI; s > LDR_REC_LO; s -= 0x1000u) {   /* flash records: 4K boundary - 256 */
        uint8_t r[80];
        if (s - 0x1000u >= LDR_DATA_LO && s - 0x1000u < LDR_DATA_HI)
            continue;                                    /* Felucca's store: user data that may look like one */
        if (ldr_fread(s - 0x100u, r, 80))
            break;
        if (ota_rd16(r + 6) == 0x5441u && ota_rd16(r) && ota_rd16(r) == ota_crc16(r + 2, 78, 0))
            ldr_erase(s - 0x1000u);
    }
    return 0;
}
