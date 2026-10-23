/* SPDX-License-Identifier: GPL-3.0-only */
/* The floating page store (store.h). */
#include <string.h>

#include "store.h"

/* a page header: the key, the record's sequence, the page's index, the record's pages, its length, CRC-16 of the
 * header's first 14 bytes and the page's payload (a page cut short in its program fails it) */
typedef struct {
    uint32_t key, rseq;
    uint16_t idx, np, len, crc;
} qds_hdr_t;

static uint16_t crc16(uint16_t c, const uint8_t *p, uint32_t n)   /* CRC-16/CCITT-FALSE */
{
    uint32_t i;
    int b;
    for (i = 0; i < n; i++) {
        c ^= (uint16_t)(p[i] << 8);
        for (b = 0; b < 8; b++)
            c = (uint16_t)(c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1);
    }
    return c;
}

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }
static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static uint32_t get32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static uint32_t page_off(const qd_store_t *s, int pg) { return s->base + (uint32_t)pg * QDS_PAGE; }
static int sec_live_log(const qd_store_t *s, int sec) { return s->sseq[sec] && s->sseq[sec] != 0xFFFFFFFFu; }

/* the page after pg in the log (a sector's last: the first page of the sector opened after it), -1 none */
static int next_page(const qd_store_t *s, int pg)
{
    const int sec = pg / (int)QDS_PPS, p = pg % (int)QDS_PPS;
    int k, best = -1;
    if (p < (int)QDS_PPS - 1)
        return pg + 1;
    for (k = 0; k < s->nsec; k++)
        if (sec_live_log(s, k) && s->sseq[k] > s->sseq[sec] && (best < 0 || s->sseq[k] < s->sseq[best]))
            best = k;
    return best < 0 ? -1 : best * (int)QDS_PPS + 1;
}

static int read_hdr(qd_store_t *s, int pg, qds_hdr_t *h, uint8_t *payload)
{
    uint8_t b[QDS_PAGE];
    if (s->fl.read(s->fl.ctx, page_off(s, pg), b, QDS_PAGE))
        return 0;
    h->key = get32(b);
    h->rseq = get32(b + 4);
    h->idx = get16(b + 8);
    h->np = get16(b + 10);
    h->len = get16(b + 12);
    h->crc = get16(b + 14);
    if (payload)
        memcpy(payload, b + QDS_HDR, QDS_PAYLOAD);
    if (h->key == 0xFFFFFFFFu && h->rseq == 0xFFFFFFFFu)
        return -1;                                      /* blank */
    return crc16(crc16(0xFFFFu, b, 14), b + QDS_HDR, QDS_PAYLOAD) == h->crc ? 1 : 0;
}

static qds_ent_t *find(qd_store_t *s, uint32_t key)
{
    int i;
    for (i = 0; i < s->nkey; i++)
        if (s->ent[i].key == key)
            return &s->ent[i];
    return 0;
}

static void index_put(qd_store_t *s, uint32_t key, uint32_t rseq, int first, int np, int len)
{
    qds_ent_t *e = find(s, key);
    if (e && e->rseq > rseq)
        return;
    if (!e) {
        if (s->nkey >= s->maxkey)
            return;
        e = &s->ent[s->nkey++];
        e->key = key;
    }
    e->rseq = rseq;
    e->page = (uint16_t)first;
    e->npages = (uint16_t)np;
    e->len = (uint16_t)len;
}

static void count_live(qd_store_t *s)
{
    int i, k;
    memset(s->live, 0, sizeof s->live);
    for (i = 0; i < s->nkey; i++) {
        int pg = s->ent[i].page;
        for (k = 0; k < s->ent[i].npages && pg >= 0; k++) {
            s->live[pg / (int)QDS_PPS]++;
            pg = next_page(s, pg);
        }
    }
}

static int do_erase(qd_store_t *s, int sec, int quiet)
{
    if (s->fl.erase(s->fl.ctx, s->base + (uint32_t)sec * QDS_SECTOR))
        return 0;
    s->c.erases++;
    if (!quiet)
        s->c.erases_noisy++;
    s->sseq[sec] = 0;
    s->live[sec] = 0;
    if (s->head >= 0 && s->head / (int)QDS_PPS == sec)
        s->head = -1;
    return 1;
}

int qd_store_blank(const qd_store_t *s)
{
    int k, n = 0;
    for (k = 0; k < s->nsec; k++)
        n += !s->sseq[k];
    return n;
}

int qd_store_mount(qd_store_t *s, const qd_flash_t *fl, uint32_t base, int nsec, qds_ent_t *ent, int maxkey)
{
    int order[QDS_MAXSEC], n = 0, i, k, newest = -1;
    uint32_t maxr = 0, maxs = 0;
    memset(s, 0, sizeof *s);
    s->ent = ent;
    s->maxkey = ent ? maxkey : 0;
    s->fl = *fl;
    s->base = base;
    s->nsec = nsec;
    s->head = -1;
    s->reserve = (int)QDS_RESERVE;
    if (nsec < s->reserve + 2 || nsec > QDS_MAXSEC || base % QDS_SECTOR)
        return 0;
    for (k = 0; k < nsec; k++) {                        /* the sectors: a log one, blank, or to erase again */
        uint8_t b[QDS_PAGE];
        uint32_t j;
        int blank = 1;
        fl->read(fl->ctx, base + (uint32_t)k * QDS_SECTOR, b, QDS_PAGE);
        if (get32(b) == QDS_MAGIC && get32(b + 4) && get32(b + 4) != 0xFFFFFFFFu &&
            get16(b + 8) == (uint16_t)~get16(b + 10)) {
            s->sseq[k] = get32(b + 4);
            order[n++] = k;
            maxs = s->sseq[k] > maxs ? s->sseq[k] : maxs;
            continue;
        }
        for (j = 0; j < QDS_SECTOR && blank; j += QDS_PAGE) {
            uint32_t q;
            fl->read(fl->ctx, base + (uint32_t)k * QDS_SECTOR + j, b, QDS_PAGE);
            for (q = 0; q < QDS_PAGE && blank; q++)
                blank = b[q] == 0xFFu;
        }
        s->sseq[k] = blank ? 0 : 0xFFFFFFFFu;
    }
    for (i = 0; i < n; i++)                             /* the log's order */
        for (k = i + 1; k < n; k++)
            if (s->sseq[order[k]] < s->sseq[order[i]]) {
                const int t = order[i];
                order[i] = order[k], order[k] = t;
            }
    {   /* the records, in the log's order */
        uint32_t okey = 0, orseq = 0;
        int open = 0, onext = 0, ofirst = -1, onp = 0, olen = 0;
        for (i = 0; i < n; i++) {
            const int sec = order[i];
            int p;
            for (p = 1; p < (int)QDS_PPS; p++) {
                qds_hdr_t h;
                const int pg = sec * (int)QDS_PPS + p, r = read_hdr(s, pg, &h, 0);
                if (r < 0) {                            /* blank: the end of this sector's pages */
                    if (i == n - 1 && s->head < 0)
                        s->head = pg;
                    open = 0;
                    break;
                }
                if (r > 0 && h.rseq > maxr)
                    maxr = h.rseq;
                if (r == 0 || !h.np) {
                    open = 0;
                    continue;
                }
                if (h.idx == 0) {
                    open = 1, okey = h.key, orseq = h.rseq, onext = 1, ofirst = pg, onp = h.np, olen = h.len;
                } else if (!open || h.key != okey || h.rseq != orseq || h.idx != onext) {
                    open = 0;
                    continue;
                } else {
                    onext++;
                }
                if (open && onext == onp) {             /* the commit page */
                    index_put(s, okey, orseq, ofirst, onp, olen);
                    open = 0;
                }
            }
        }
        if (n)
            newest = order[n - 1];
        (void)newest;
    }
    s->next_rseq = maxr + 1;
    s->next_sseq = maxs + 1;
    for (k = 0; k < nsec; k++)                          /* an erase cut short: erased again (power-on: quiet) */
        if (s->sseq[k] == 0xFFFFFFFFu)
            do_erase(s, k, 1);
    count_live(s);
    return 1;
}

int qd_store_get(qd_store_t *s, uint32_t key, void *buf, uint32_t max)
{
    const qds_ent_t *e = find(s, key);
    uint8_t pay[QDS_PAYLOAD];
    uint32_t got = 0;
    int pg, k;
    if (!e)
        return -1;
    if (e->len == QDS_DELETED)
        return QDS_DELETED;
    pg = e->page;
    for (k = 0; k < e->npages && pg >= 0 && buf; k++) {
        qds_hdr_t h;
        uint32_t take = e->len - got > QDS_PAYLOAD ? QDS_PAYLOAD : e->len - got;
        read_hdr(s, pg, &h, pay);
        if (got + take > max)
            take = max > got ? max - got : 0;
        memcpy((uint8_t *)buf + got, pay, take);
        got += take;
        pg = next_page(s, pg);
    }
    return e->len;
}

int qd_store_read(qd_store_t *s, uint32_t key, uint32_t off, void *buf, uint32_t n)
{
    const qds_ent_t *e = find(s, key);
    uint8_t pay[QDS_PAYLOAD];
    uint32_t got = 0, at = 0;
    int pg, k;
    if (!e || e->len == QDS_DELETED || off >= e->len)
        return 0;
    if (n > e->len - off)
        n = e->len - off;
    pg = e->page;
    for (k = 0; k < e->npages && pg >= 0 && got < n; k++, at += QDS_PAYLOAD) {
        if (at + QDS_PAYLOAD > off) {                   /* (a page holding some of it) */
            qds_hdr_t h;
            const uint32_t from = off + got - at, take = QDS_PAYLOAD - from < n - got ? QDS_PAYLOAD - from : n - got;
            read_hdr(s, pg, &h, pay);
            memcpy((uint8_t *)buf + got, pay + from, take);
            got += take;
        }
        pg = next_page(s, pg);
    }
    return (int)got;
}

static uint32_t pages_of(uint32_t len) { return len == QDS_DELETED || !len ? 1u : (len + QDS_PAYLOAD - 1u) / QDS_PAYLOAD; }

void qd_store_use(const qd_store_t *s, uint32_t *live_pages, uint32_t *total_pages)
{
    uint32_t l = 0;
    int i;
    for (i = 0; i < s->nkey; i++)
        l += s->ent[i].npages;
    if (live_pages)
        *live_pages = l;
    if (total_pages)
        *total_pages = (uint32_t)s->nsec * (QDS_PPS - 1u);
}

int qd_store_busy(const qd_store_t *s) { return s->busy; }
void qd_store_set_reserve(qd_store_t *s, int sectors) { s->reserve = sectors < 1 ? 1 : sectors > s->nsec - 2 ? s->nsec - 2 : sectors; }

static int put_job(qd_store_t *s, uint32_t key, uint32_t len)
{
    uint32_t live, total;
    const qds_ent_t *e = find(s, key);
    if (s->busy || (len > QDS_MAXREC && len != QDS_DELETED) || key == 0xFFFFFFFFu)
        return 0;
    if (len == QDS_DELETED && (!e || e->len == QDS_DELETED))
        return 1;                                       /* (nothing to delete) */
    qd_store_use(s, &live, &total);
    if (live - (e ? e->npages : 0u) + pages_of(len) > (uint32_t)(s->nsec - s->reserve) * (QDS_PPS - 1u))
        return 0;                                       /* it could never fit */
    s->busy = 1;
    s->job_key = key;
    s->job_len = (uint16_t)len;
    s->job_np = (uint16_t)pages_of(len);
    s->job_done = 0;
    s->job_first = -1;
    s->job_rseq = s->next_rseq++;
    s->job_data = 0, s->job_src = 0, s->job_ok = 0, s->job_ctx = 0;
    return 1;
}

int qd_store_put(qd_store_t *s, uint32_t key, const void *data, uint32_t len)
{
    if (!put_job(s, key, len))
        return 0;
    if (len != QDS_DELETED && len) {
        if (len <= QDS_JOBBUF) {
            memcpy(s->job_buf, data, len);
            s->job_data = s->job_buf;
        } else {
            s->job_data = (const uint8_t *)data;        /* (the caller keeps it while busy) */
        }
    }
    return 1;
}

int qd_store_put_src(qd_store_t *s, uint32_t key, uint32_t len, qds_src_fn src, qds_ok_fn ok, void *ctx)
{
    if (!src || len == QDS_DELETED || !put_job(s, key, len))
        return 0;
    s->job_src = src, s->job_ok = ok, s->job_ctx = ctx;
    return 1;
}

int qd_store_del(qd_store_t *s, uint32_t key) { return qd_store_put(s, key, 0, QDS_DELETED); }

/* a blank sector opened at the log's end: its header (the magic, its sequence, a check) */
static int open_sector(qd_store_t *s)
{
    uint8_t b[16];
    int k;
    for (k = 0; k < s->nsec; k++)
        if (!s->sseq[k])
            break;
    if (k == s->nsec)
        return 0;
    memset(b, 0xFF, sizeof b);
    put32(b, QDS_MAGIC);
    put32(b + 4, s->next_sseq);
    put16(b + 8, 0x5A5Au);
    put16(b + 10, (uint16_t)~0x5A5Au);
    if (s->fl.prog(s->fl.ctx, s->base + (uint32_t)k * QDS_SECTOR, b, sizeof b))
        return 0;
    s->c.progs++;
    s->sseq[k] = s->next_sseq++;
    s->head = k * (int)QDS_PPS + 1;
    return 1;
}

/* one page of a record at the head, its payload from data (the record's bytes), else src (P2.1: its owner's), else
 * pay (a payload as it is: GC's copy): 1 written, 0 no room (no blank sector) or the source failed */
static int write_page(qd_store_t *s, uint32_t key, uint32_t rseq, int idx, int np, uint32_t len, const uint8_t *data,
                      qds_src_fn src, void *ctx, const uint8_t *pay, int *at)
{
    uint8_t b[QDS_PAGE];
    uint32_t off = (uint32_t)idx * QDS_PAYLOAD, take = 0;
    if (s->head < 0 && !open_sector(s))
        return 0;
    memset(b, 0xFF, sizeof b);
    memset(b + QDS_HDR, 0, QDS_PAYLOAD);
    if (len != QDS_DELETED && off < len) {
        take = len - off > QDS_PAYLOAD ? QDS_PAYLOAD : len - off;
        if (data)
            memcpy(b + QDS_HDR, data + off, take);
        else if (src) {
            if (src(ctx, off, b + QDS_HDR, take))
                return 0;
        } else if (pay)
            memcpy(b + QDS_HDR, pay, take);
    }
    put32(b, key);
    put32(b + 4, rseq);
    put16(b + 8, (uint16_t)idx);
    put16(b + 10, (uint16_t)np);
    put16(b + 12, (uint16_t)len);
    put16(b + 14, crc16(crc16(0xFFFFu, b, 14), b + QDS_HDR, QDS_PAYLOAD));
    if (s->fl.prog(s->fl.ctx, page_off(s, s->head), b, QDS_PAGE))
        return 0;
    s->c.progs++;
    *at = s->head;
    s->head = s->head % (int)QDS_PPS == (int)QDS_PPS - 1 ? -1 : s->head + 1;
    return 1;
}

/* the log sector GC takes: the one whose records (every record with a page in it, written again whole) cost the fewest
 * pages, and fewer than a sector's (else GC gains nothing); not the head's; -1 none */
static int gc_victim(qd_store_t *s)
{
    static uint32_t cost[QDS_MAXSEC];
    const int hs = s->head >= 0 ? s->head / (int)QDS_PPS : -1;
    int i, k, best = -1;
    memset(cost, 0, sizeof cost);
    for (i = 0; i < s->nkey; i++) {                     /* each record's pages against each sector it touches */
        int pg = s->ent[i].page, last = -1;
        for (k = 0; k < s->ent[i].npages && pg >= 0; k++) {
            const int sec = pg / (int)QDS_PPS;
            if (sec != last)
                cost[sec] += s->ent[i].npages;
            last = sec;
            pg = next_page(s, pg);
        }
    }
    for (k = 0; k < s->nsec; k++)
        if (sec_live_log(s, k) && k != hs && cost[k] < QDS_PPS - 1u &&
            (best < 0 || cost[k] < cost[best] || (cost[k] == cost[best] && s->sseq[k] < s->sseq[best])))
            best = k;
    return best;
}

/* GC one sector (quiet, no record half written): its records written again whole, page by page (P2.1: no copy of a
 * record in RAM), then it is erased */
static int gc_one(qd_store_t *s)
{
    const int v = gc_victim(s);
    int i, moved = 1;
    if (v < 0)
        return 0;
    while (moved) {                                     /* (each record written again changes the index) */
        moved = 0;
        for (i = 0; i < s->nkey; i++) {
            qds_ent_t *e = &s->ent[i];
            int pg = e->page, k, in = 0;
            for (k = 0; k < e->npages && pg >= 0; k++) {
                in |= pg / (int)QDS_PPS == v;
                pg = next_page(s, pg);
            }
            if (!in)
                continue;
            {
                const uint32_t key = e->key, len = e->len, np = e->npages, rseq = s->next_rseq++;
                int first = -1, at, src = e->page;
                for (k = 0; k < (int)np; k++) {
                    uint8_t pay[QDS_PAYLOAD];
                    qds_hdr_t h;
                    memset(pay, 0, sizeof pay);
                    if (len != QDS_DELETED && src >= 0)
                        read_hdr(s, src, &h, pay);
                    if (!write_page(s, key, rseq, k, (int)np, len, 0, 0, 0, pay, &at))
                        return 0;
                    if (!k)
                        first = at;
                    src = src >= 0 ? next_page(s, src) : -1;
                }
                index_put(s, key, rseq, first, (int)np, (int)len);
                s->c.commits++;
                moved = 1;
                break;
            }
        }
    }
    if (!do_erase(s, v, 1))
        return 0;
    s->c.gcs++;
    count_live(s);
    return 1;
}

int qd_store_step(qd_store_t *s, int playing, int quiet, int max_pages)
{
    int n = playing ? 1 : max_pages < 1 ? 1 : max_pages;
    if (quiet && !s->busy) {                            /* the blank sectors ahead (§7 rules 1, 3): GC while it gains */
        int guard = s->nsec;
        while (qd_store_blank(s) < s->reserve && guard-- > 0) {
            const int before = qd_store_blank(s);
            if (!gc_one(s) || qd_store_blank(s) <= before)
                break;
        }
    }
    while (s->busy && n-- > 0) {
        int at;
        if (s->head < 0 && !qd_store_blank(s)) {        /* no blank sector to go on in */
            s->c.waits++;
            if (!quiet)
                return 1;                               /* (§7 rule 4: it waits for a quiet moment) */
            if (s->job_done) {                          /* a record half written: begun again after the GC */
                s->job_done = 0;
                s->job_first = -1;
                s->job_rseq = s->next_rseq++;
            }
            if (!gc_one(s)) {                           /* no room, even collected: the record is dropped (its
                                                         * owner sees it was not committed and puts it again later) */
                s->busy = 0;
                s->c.drops++;
                return 0;
            }
            continue;
        }
        if (s->job_done + 1 == s->job_np && s->job_ok && !s->job_ok(s->job_ctx)) {   /* P2.1: the source changed while
                                                         * it was written: not committed (its owner puts it again) */
            s->busy = 0;
            s->c.drops++;
            return 0;
        }
        if (!write_page(s, s->job_key, s->job_rseq, s->job_done, s->job_np, s->job_len, s->job_data, s->job_src,
                        s->job_ctx, 0, &at))
            return 1;
        if (playing)
            s->c.pages_playing++;
        if (!s->job_done)
            s->job_first = at;
        if (++s->job_done == s->job_np) {
            index_put(s, s->job_key, s->job_rseq, s->job_first, s->job_np, s->job_len);
            count_live(s);
            s->c.commits++;
            if (playing)
                s->c.saves_playing++;
            s->busy = 0;
        }
    }
    return s->busy;
}

int qd_store_flush(qd_store_t *s)
{
    int guard = 100000;
    while (s->busy && guard-- > 0) {
        const uint32_t before = s->c.progs + s->c.erases;
        qd_store_step(s, 0, 1, 64);
        if (s->c.progs + s->c.erases == before)
            break;                                      /* (no progress: the flash refuses, or no room at all) */
    }
    return !s->busy;
}

void qd_store_each(qd_store_t *s, uint32_t mask, uint32_t match, void (*fn)(void *ctx, uint32_t key, int len), void *ctx)
{
    int i;
    for (i = 0; i < s->nkey; i++)
        if ((s->ent[i].key & mask) == match && s->ent[i].len != QDS_DELETED)
            fn(ctx, s->ent[i].key, s->ent[i].len);
}
