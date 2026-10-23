/* SPDX-License-Identifier: GPL-3.0-only */
/* The floating page store (P1.5a; PLAN.md §7 "Flash for patterns and sounds", "Saving with no flash writes that could
 * cause drop-outs while playing"): records (a key, up to QDS_MAXREC bytes) kept in NOR flash as runs of 256-byte pages,
 * the commit page last; a newer version of a key makes the older one stale; deleted keys leave a tombstone. Pure C,
 * no OS: the flash is three hooks (read, erase a 4 KiB sector, program within one page), so the same code runs on the
 * host over FM1VST's flash image and in Phase 2 over the device's NOR.
 *
 * THE LAYOUT. A region of whole sectors (4 KiB). A sector's page 0 is its header: QDS_MAGIC and the sector's sequence
 * number (the order the store opened them for writing: a log); pages 1..15 hold records. A record is a run of pages in
 * that order (it may cross into the next sector the log opened); each page: a 16-byte header (the key, the record's
 * sequence number, the page's index and the record's page count, the record's length, a CRC-16 of this page's
 * payload) and 240 bytes of payload. The last page commits it: its header's crc covers the WHOLE record (CRC-16 over
 * every payload byte); a record without its commit page is ignored.
 *
 * MOUNT (power-on): sectors with a header are the log, in their sequence order; a sector neither a log sector nor
 * blank (an erase cut short) is erased again (power-on is before the audio). Every committed record found goes into
 * the index; for a key, the highest record sequence wins (a tombstone, length QDS_DELETED, deletes). The write head is
 * the first blank page after the newest sector's last written page.
 *
 * WRITING. qd_store_put queues a record (copied: the caller's buffer is free at once, up to QDS_JOBBUF bytes; longer,
 * the caller keeps it until the store is not busy); P2.1: qd_store_put_src queues one whose bytes a source gives,
 * page by page as they are written (no copy of the record anywhere: the device has no room for one), and checks
 * before the commit page that the source has not changed under it (else the record is dropped: its owner writes it
 * again). qd_store_step writes its pages a few at a time. The rules of §7 are the store's own (the caller says, each step, whether anything sounds):
 *   - no erase unless the caller says it is quiet (the transport stopped and no voice sounding); the counter
 *     erases_noisy counts any that were not (it stays 0: the tests check);
 *   - while playing, at most one page a step (a UI frame): pages_playing, saves_playing count them;
 *   - the log needs blank sectors to grow into; QDS_RESERVE are kept blank when the store can (an erase only when
 *     quiet). Garbage collection (quiet only): the oldest log sector's live records are written again at the head,
 *     then it is erased. Out of blank sectors while not quiet, a record waits (waits counts it).
 * Not thread-safe (one caller). */
#pragma once
#include <stdint.h>

#include "../qd_config.h"                 /* (P2.0: QDS_MAXKEY) */

#ifdef __cplusplus
extern "C" {
#endif

#define QDS_SECTOR 4096u
#define QDS_PAGE 256u
#define QDS_PPS (QDS_SECTOR / QDS_PAGE)   /* pages a sector (page 0 the header) */
#define QDS_HDR 16u
#define QDS_PAYLOAD (QDS_PAGE - QDS_HDR)   /* 240 */
#define QDS_MAXREC (QDS_PAYLOAD * 120u)    /* a record's largest (28.1 KiB: a full pattern, 1024 steps and 1024 locks,
                                            * with its sounds) */
#define QDS_MAXSEC 256                     /* sectors a store */
#define QDS_RESERVE 6u                     /* blank sectors kept ahead by default (qd_store_set_reserve: the largest
                                            * record's sectors and one more) */
#define QDS_MAGIC 0x5346325au              /* "Z2FS" */
#define QDS_DELETED 0xFFFEu                /* a tombstone's length */

typedef struct {                           /* the flash: offsets from the start of the device's flash */
    int (*read)(void *ctx, uint32_t off, void *dst, uint32_t n);
    int (*erase)(void *ctx, uint32_t off);   /* the 4 KiB sector holding off */
    int (*prog)(void *ctx, uint32_t off, const void *src, uint32_t n);   /* within one page: clears bits */
    void *ctx;
} qd_flash_t;

typedef struct {
    uint32_t key, rseq;
    uint16_t page;                         /* its first page: sector index * QDS_PPS + page in the sector */
    uint16_t npages;
    uint16_t len;                          /* QDS_DELETED: a tombstone */
} qds_ent_t;

typedef struct {
    uint32_t erases, progs;                /* the flash's operations */
    uint32_t erases_noisy;                 /* an erase while something sounds: always 0 (§7 rule 1) */
    uint32_t pages_playing, saves_playing; /* pages programmed and records committed while playing (§7 rule 2) */
    uint32_t gcs;                          /* sectors collected */
    uint32_t waits;                        /* steps a record waited for a blank sector (not quiet) */
    uint32_t commits;                      /* records committed */
    uint32_t drops;                        /* records dropped: no room even after GC (the store full) */
} qds_count_t;

/* P2.1: a record's bytes from its owner: bytes [off, off + n) of it into dst (n <= QDS_PAYLOAD); 0 ok. ok (may be
 * null): before the commit page, 1 if the record is still what it was when it was queued */
typedef int (*qds_src_fn)(void *ctx, uint32_t off, uint8_t *dst, uint32_t n);
typedef int (*qds_ok_fn)(void *ctx);

typedef struct {
    qd_flash_t fl;
    uint32_t base;                         /* the region's first byte (sector aligned) */
    int nsec;                              /* its sectors (reserve + 2 .. QDS_MAXSEC) */
    int reserve;                           /* blank sectors kept ahead */
    uint32_t sseq[QDS_MAXSEC];             /* each sector's sequence number, 0: blank, 0xFFFFFFFF: to erase */
    uint16_t live[QDS_MAXSEC];             /* pages of index records in it */
    int nkey;
    qds_ent_t *ent;                        /* P2.1: the index, the owner's (sized to what it keeps) .. */
    int maxkey;                            /* .. its places */
    int head;                              /* the next page to write (sector * QDS_PPS + page), -1: none open */
    uint32_t next_sseq, next_rseq;
    /* the record being written */
    int busy;
    uint32_t job_key, job_rseq;
    uint16_t job_len, job_np, job_done;
    int job_first;
    uint8_t job_buf[QDS_JOBBUF];           /* (a short record's copy) */
    const uint8_t *job_data;               /* .. or the caller's bytes, or (P2.1) its source */
    qds_src_fn job_src;
    qds_ok_fn job_ok;
    void *job_ctx;
    qds_count_t c;
} qd_store_t;

/* mount the region base .. base + nsec sectors (an erased flash: an empty store) with an index of maxkey places (the
 * caller's ent: P2.1). 1 ok, 0 the region is bad */
int qd_store_mount(qd_store_t *s, const qd_flash_t *fl, uint32_t base, int nsec, qds_ent_t *ent, int maxkey);
/* a record's length (QDS_DELETED or -1: none) and, with buf, its bytes (up to max) */
int qd_store_get(qd_store_t *s, uint32_t key, void *buf, uint32_t max);
/* P2.1: bytes [off, off + n) of a record (a long one read in parts, never whole); the bytes read */
int qd_store_read(qd_store_t *s, uint32_t key, uint32_t off, void *buf, uint32_t n);
/* queue a record (len 0 .. QDS_MAXREC; QDS_DELETED with data NULL: delete the key). 0: busy (one at a time), the
 * record is too big, or the store could never hold it */
int qd_store_put(qd_store_t *s, uint32_t key, const void *data, uint32_t len);
int qd_store_put_src(qd_store_t *s, uint32_t key, uint32_t len, qds_src_fn src, qds_ok_fn ok, void *ctx);
int qd_store_del(qd_store_t *s, uint32_t key);
int qd_store_busy(const qd_store_t *s);
/* write: playing (one page at most), else up to max_pages; quiet: erases allowed (the transport stopped and nothing
 * sounding; garbage collection and the blank sectors ahead too). Returns 1 while a record is still being written */
int qd_store_step(qd_store_t *s, int playing, int quiet, int max_pages);
/* everything queued written now (quiet: a shutdown, a load); 1 done */
int qd_store_flush(qd_store_t *s);
/* the keys with prefix (key & mask) == match: called with each live one (not tombstones) */
void qd_store_each(qd_store_t *s, uint32_t mask, uint32_t match, void (*fn)(void *ctx, uint32_t key, int len), void *ctx);
/* the store's use: pages of live records, of the region's record pages (the flash meter) */
void qd_store_use(const qd_store_t *s, uint32_t *live_pages, uint32_t *total_pages);
/* the blank sectors now */
int qd_store_blank(const qd_store_t *s);
void qd_store_set_reserve(qd_store_t *s, int sectors);

#ifdef __cplusplus
}
#endif
