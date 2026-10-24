/* SPDX-License-Identifier: MPL-2.0 */
/* FIZZ's full-frame send (P2.12; the owner's choice: the 256-colour frame, PLAN.md): the core draws a frame of 240 x 240
 * palette indices (two row bands, a base pointer a row); this sends the rows that changed, converted to RGB565 through
 * the core's 256-entry palette a few rows at a time into two small buffers (one converted while the other goes out),
 * top to bottom in one pass, started just after the panel's scan has passed the first changed row (RDSCL: app.c
 * fz_scanline). A pass that fits one refresh is then tear-free: the scan shows the old rows on its way down and finds
 * them all new on its next. Included by app.c (the unity build: lcd_blit, plat_*, fz_scanline in scope).
 *
 * The panel (the unit, 9e65e0ba): 334 lines a refresh (320 and the porches), 60 a second: ~50 us a line; this module's
 * rows are lines 0..239. A pass takes ~88 us a row at 44 MHz (RGB565), so ~190 rows fit one refresh; a pass that does
 * not is split where it would meet the scan and its rest sent after the scan has passed it again. No scanline (a read
 * failed, or nonsense): sent at once, unsynced, and counted. */
#ifndef FZ_FB_CHUNK
#define FZ_FB_CHUNK 2u                   /* rows a transfer (a buffer: 960 B; the RAM's to spare) */
#endif
#define FZ_FB_LINES 334u                 /* the panel's lines a refresh (the unit's RDSCL: 0..333) */
#define FZ_FB_LINE_US 50u                /* ~16.67 ms / 334 */
#define FZ_FB_ROW_US 88u                 /* a row's transfer: 480 B at ~44 MHz */

/* (its counters, fz_fb_stats_t fz_fb: app.c, before its stats lines) */
#define fz_fb_hash fz_row_hash           /* each row's indices as last sent (0: unknown): the strips' table, not used
                                          * by this build */
#ifdef PLAT_HOST
static uint16_t fz_fb_buf[2][FZ_FB_CHUNK * 240u];
#else
static uint16_t fz_fb_buf[2][FZ_FB_CHUNK * 240u] __attribute__((section(".noinit"), aligned(4)));   /* (NOINIT: RAM and
                                          * the pool are the frame's; not cleared, written before each transfer) */
#endif

static uint32_t fz_fb_row_hash(const uint8_t *p)
{
    const uint32_t *w = (const uint32_t *)(const void *)p;   /* (rows are 4-byte aligned: 240 bytes) */
    uint32_t h = 2166136261u, i;
    for (i = 0; i < 60u; i++)
        h = (h ^ w[i]) * 16777619u;
    return h | 1u;
}

static int fz_fb_line(uint32_t *line)    /* the scan's line now; -1 none */
{
#ifdef PLAT_HOST
    *line = (plat_ticks24() / PLAT_TICKS_PER_US / FZ_FB_LINE_US) % FZ_FB_LINES;   /* (the host: a panel of its own) */
    return 0;
#else
    uint32_t alt;
    if (fz_scanline(line, &alt) || *line >= FZ_FB_LINES) {
        fz_fb.scan_fail++;
        return -1;
    }
    return 0;
#endif
}

/* rows [y0, y1) from the frame, converted and sent */
static void fz_fb_out(uint8_t *const *rows, const uint16_t *pal, uint32_t y0, uint32_t y1)
{
    static uint32_t k;
    while (y0 < y1) {
        const uint32_t n = y1 - y0 < FZ_FB_CHUNK ? y1 - y0 : FZ_FB_CHUNK;
        uint16_t *b = fz_fb_buf[k];
        uint32_t r, x;
        for (r = 0; r < n; r++) {        /* (the other buffer may still be going out: this one is free) */
            const uint8_t *s = rows[y0 + r];
            uint16_t *d = b + 240u * r;
            for (x = 0; x < 240u; x++)
                d[x] = pal[s[x]];
        }
        lcd_blit(0, y0, 240, n, b);      /* (waits for the transfer before it: lcd.c) */
        k ^= 1u;
        y0 += n;
        if (plat_ms() - fz_led_t >= FZ_LED_MS)
            fz_lights();                 /* (the LEDs keep their pace through a long pass) */
    }
}

/* a frame: the rows in bands (bit k: rows k x band ..) hashed, the changed ones sent in one pass; all: every row (a new
 * palette, the screen cleared). Returns the rows sent */
static uint32_t fz_fb_send(uint8_t *const *rows, const uint16_t *pal, uint32_t bands, uint32_t band, int all)
{
    uint8_t ch[240];
    uint32_t y, first = 240u, last = 0, n = 0, t0, line, wait0;
    for (y = 0; y < 240u; y++) {
        ch[y] = 0;
        if (all || (bands >> (y / band) & 1u)) {
            const uint32_t h = fz_fb_row_hash(rows[y]);
            if (all || h != fz_fb_hash[y]) {
                fz_fb_hash[y] = h;
                ch[y] = 1;
                n++;
                first = y < first ? y : first;
                last = y;
            }
        }
    }
    fz_fb.frames++;
    if (!n)
        return 0;
    fz_fb.rows += n;
    fz_fb.passes++;
    /* the start: the scan just past the first changed row (its old row shown this refresh, the new one the next) */
    wait0 = plat_ticks24();
    if (!fz_fb_line(&line)) {
        while (!(line >= first && line < first + 24u)) {   /* (up to a refresh, ~16.7 ms) */
            if ((plat_ticks24() - wait0) / PLAT_TICKS_PER_US > 18000u || fz_fb_line(&line)) {
                line = 0xFFFFu;
                break;
            }
        }
    } else {
        line = 0xFFFFu;
    }
    y = (plat_ticks24() - wait0) / PLAT_TICKS_PER_US;
    if (y > fz_fb.wait_max_us)
        fz_fb.wait_max_us = y;
    if (line == 0xFFFFu)
        fz_fb.unsynced++;
    else
        fz_fb.synced++;
    t0 = plat_ticks24();
    for (y = first; y <= last;) {        /* the runs of changed rows, top to bottom */
        uint32_t e = y;
        if (!ch[y]) {
            y++;
            continue;
        }
        while (e <= last && ch[e])
            e++;
        if (line != 0xFFFFu) {           /* (would this run meet the scan coming round again? then after it has passed) */
            const uint32_t el = (plat_ticks24() - t0) / PLAT_TICKS_PER_US / FZ_FB_LINE_US;   /* (lines gone by) */
            const uint32_t at = (line + el) % FZ_FB_LINES;   /* (where the scan is now, about) */
            const uint32_t need = (e - y) * FZ_FB_ROW_US / FZ_FB_LINE_US + 1u;   /* (lines the run takes) */
            if (at < y && at + need >= y) {   /* (it would reach the run's rows before they are all written) */
                uint32_t l2 = 0;
                fz_fb.split++;
                while (!fz_fb_line(&l2) && !(l2 >= y && l2 < y + 24u) &&
                       (plat_ticks24() - t0) / PLAT_TICKS_PER_US < 40000u)
                    ;
            }
        }
        fz_fb_out(rows, pal, y, e);
        y = e;
    }
    lcd_sync();
    t0 = (plat_ticks24() - t0) / PLAT_TICKS_PER_US;
    fz_fb.span_sum_us += t0;
    if (t0 > fz_fb.span_max_us)
        fz_fb.span_max_us = t0;
    if (line == 0xFFFFu || t0 > (FZ_FB_LINES - 24u) * FZ_FB_LINE_US)
        fz_fb.torn++;                    /* (estimated: unsynced, or longer than a refresh less the start's slack) */
    return n;
}
