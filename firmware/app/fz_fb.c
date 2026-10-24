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
#define FZ_FB_MARGIN 24u                 /* (P2.16d: lines kept in hand at the start, was 8: an audio interrupt's ~1 ms
                                          * of spread inside the send) */
#define FZ_FB_ROW_US 96u                 /* a row out: 480 B at ~44 MHz and its conversion (the unit: 22.6 ms for 240) */

/* (its counters, fz_fb_stats_t fz_fb: app.c, before its stats lines) */
#define fz_fb_hash fz_row_hash           /* each row's indices as last sent (0: unknown): the strips' table, not used
                                          * by this build */
#ifdef PLAT_HOST
static uint16_t fz_fb_buf[2][FZ_FB_CHUNK * 240u];
#else
static uint16_t fz_fb_buf[2][FZ_FB_CHUNK * 240u] __attribute__((section(".fz_rtdata"), aligned(4)));   /* (the RAM code
                                          * region's spare: RAM, the pool and NOINIT are full) */
#endif

static uint32_t fz_fb_row_hash(const uint8_t *p)
{
    const uint32_t *w = (const uint32_t *)(const void *)p;   /* (rows are 4-byte aligned: 240 bytes) */
    uint32_t h = 2166136261u, i;
    for (i = 0; i < 60u; i++) {          /* (P2.16c: FNV-1a over words with a shift down each word, as the FIZZ agent
                                          * found: without it a word's top byte only reached the hash's top 8 bits, and
                                          * a row whose changes were all in top bytes (x % 4 == 3) matched 1 in 256) */
        h = (h ^ w[i]) * 16777619u;
        h ^= h >> 15;
    }
    return h | 1u;
}

static int fz_fb_line(uint32_t *line)    /* the scan's line now; -1 none */
{
#if defined(PLAT_WEB)
    (void)line;                          /* (the browser's emulator: its clock moves only between its calls, and its
                                          * canvas does not tear: no scan to wait for) */
    return -1;
#elif defined(PLAT_HOST)
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

/* (P2.17) DIRTY RECTANGLES. Each row is hashed in FZ_FB_SEGS segments of FZ_FB_SEG_W pixels (32 bits each: a changed
 * segment missed 1 in 2^32), so a change is known to the segment: a tall narrow change (ROLL's playhead, a meter's
 * bar) goes as a narrow window, not as whole rows, and fits one refresh. The rectangles: consecutive changed rows
 * whose changed segments are within the group's first row's (a superset sent: those pixels are the frame's anyway)
 * make a group; each run of adjacent segments in the group's set is a rectangle. Top to bottom, a group's
 * rectangles left to right. The dual build (the room: 3,840 B, half in the pool, half in NOINIT); the single-core
 * build one segment (whole rows, the row hash as before) */
#ifndef FZ_FB_SEGS
#if FM1B_CPU2 && !FZ_SINGLE_CORE
#define FZ_FB_SEGS 4u
#else
#define FZ_FB_SEGS 1u
#endif
#endif
#define FZ_FB_SEG_W (240u / FZ_FB_SEGS)
#define FZ_FB_PX_NS 380u                 /* (the send's model: a pixel converted and out, ~0.38 us at 44 MHz ..) */
#define FZ_FB_BLIT_US 14u                /* (.. and a window set and a transfer started) */
#define FZ_FB_A_MARGIN 8u                /* (lines: the scan's lead kept over a writer faster than it) */
#if FZ_FB_SEGS > 1
#ifdef PLAT_HOST
static uint32_t fz_fb_sh_a[120][FZ_FB_SEGS], fz_fb_sh_b[120][FZ_FB_SEGS];
#else
static uint32_t fz_fb_sh_a[120][FZ_FB_SEGS] __attribute__((section(".pool"), aligned(4)));
static uint32_t fz_fb_sh_b[120][FZ_FB_SEGS] __attribute__((section(".fz_nidata"), aligned(4)));   /* (NOINIT: FIZZ's) */
#endif
#define FZ_SEGH(y) ((y) < 120u ? fz_fb_sh_a[y] : fz_fb_sh_b[(y) - 120u])
#else
#define FZ_SEGH(y) (&fz_fb_hash[y])
#endif
static void fz_fb_forget(void)           /* (every segment unknown: sent at the next frame) */
{
    uint32_t y, s;
    for (y = 0; y < 240u; y++)
        for (s = 0; s < FZ_FB_SEGS; s++)
            FZ_SEGH(y)[s] = 0;
}
static uint32_t fz_fb_seg_hash(const uint8_t *p, uint32_t words)
{
    const uint32_t *w = (const uint32_t *)(const void *)p;
    uint32_t h = 2166136261u, i;
    for (i = 0; i < words; i++) {
        h = (h ^ w[i]) * 16777619u;
        h ^= h >> 15;
    }
    return h | 1u;
}

/* a rectangle (x0 .. x0 + w - 1, rows y0 .. y1 - 1) from the frame, converted and sent: as many rows a transfer as a
 * buffer holds */
static void fz_fb_out(uint8_t *const *rows, const uint16_t *pal, uint32_t x0, uint32_t w, uint32_t y0, uint32_t y1)
{
    static uint32_t k;
    const uint32_t per = FZ_FB_CHUNK * 240u / w;
    while (y0 < y1) {
        const uint32_t n = y1 - y0 < per ? y1 - y0 : per;
        uint16_t *b = fz_fb_buf[k];
        uint32_t r, x;
        for (r = 0; r < n; r++) {        /* (the other buffer may still be going out: this one is free) */
            const uint8_t *s = rows[y0 + r] + x0;
            uint16_t *d = b + w * r;
            for (x = 0; x < w; x++)
                d[x] = pal[s[x]];
        }
        lcd_blit(x0, y0, w, n, b);       /* (waits for the transfer before it: lcd.c) */
        k ^= 1u;
        y0 += n;
        if (plat_ms() - fz_led_t >= FZ_LED_MS)
            fz_lights();                 /* (the LEDs keep their pace through a long pass) */
    }
}

/* the rectangles in order: fn(x0, w, y0, y1) for each; the group rule above. ch[y]: row y's changed segments */
typedef struct {
    int32_t smin_us, smax_us;            /* (the scan's start window, in us of lines: L x line) */
    uint32_t t_us, rects, px, rows;
} fz_fb_plan_t;
static void fz_fb_rect_time(fz_fb_plan_t *p, uint32_t x0, uint32_t w, uint32_t y0, uint32_t y1)
{
    /* (the model) row y written from t_start(y) to t_end(y); the scan (from line s, 50 us a line) must have passed y
     * before it starts (A: s L + t_start > y L) and not come round to it again before it ends (B: s L + t_end < (y +
     * LINES) L). Linear in y within a rectangle: its first and last rows bound it */
    const uint32_t h = y1 - y0, per = FZ_FB_CHUNK * 240u / w;
    const uint32_t row = w * FZ_FB_PX_NS / 1000u + (FZ_FB_BLIT_US + per - 1u) / per, ts = p->t_us;
    const uint32_t te = ts + h * row;
    int32_t a0 = (int32_t)(y0 * FZ_FB_LINE_US) - (int32_t)ts, a1 = (int32_t)((y1 - 1u) * FZ_FB_LINE_US) - (int32_t)(te - row);
    int32_t b0 = (int32_t)((y0 + FZ_FB_LINES) * FZ_FB_LINE_US) - (int32_t)(ts + row);
    int32_t b1 = (int32_t)((y1 - 1u + FZ_FB_LINES) * FZ_FB_LINE_US) - (int32_t)te;
    (void)x0;
    if (a0 > p->smin_us)
        p->smin_us = a0;
    if (a1 > p->smin_us)
        p->smin_us = a1;
    if (b0 < p->smax_us)
        p->smax_us = b0;
    if (b1 < p->smax_us)
        p->smax_us = b1;
    p->t_us = te;
    p->rects++, p->px += w * h, p->rows += h;
}
static void fz_fb_rects(const uint8_t *ch, uint32_t first, uint32_t last, fz_fb_plan_t *p, uint8_t *const *rows,
                        const uint16_t *pal)
{
    uint32_t y = first;
    while (y <= last) {
        uint32_t g, e, s;
        if (!ch[y]) {
            y++;
            continue;
        }
        g = ch[y];
        e = y + 1u;
        while (e <= last && ch[e] && !(ch[e] & ~g))
            e++;
        for (s = 0; s < FZ_FB_SEGS;) {   /* (each run of adjacent segments: a rectangle) */
            uint32_t s1 = s;
            if (!(g >> s & 1u)) {
                s++;
                continue;
            }
            while (s1 < FZ_FB_SEGS && (g >> s1 & 1u))
                s1++;
            if (p)
                fz_fb_rect_time(p, s * FZ_FB_SEG_W, (s1 - s) * FZ_FB_SEG_W, y, e);
            else
                fz_fb_out(rows, pal, s * FZ_FB_SEG_W, (s1 - s) * FZ_FB_SEG_W, y, e);
            s = s1;
        }
        y = e;
    }
}

/* a frame: the rows in bands (bit k: rows k x band ..) hashed by segment, the changed ones sent as rectangles in one
 * pass; all: every row (a new palette, the screen cleared). Returns the rows sent */
static uint32_t fz_fb_send(uint8_t *const *rows, const uint16_t *pal, uint32_t bands, uint32_t band, int all)
{
    uint8_t ch[240];
    uint32_t y, first = 240u, last = 0, n = 0, t0, line, wait0, isr0, s;
    fz_fb_plan_t pl;
    for (y = 0; y < 240u; y++) {
        ch[y] = 0;
        if (all || (bands >> (y / band) & 1u)) {
            uint32_t *hs = FZ_SEGH(y), m = 0;
            for (s = 0; s < FZ_FB_SEGS; s++) {
                const uint32_t h = fz_fb_seg_hash(rows[y] + s * FZ_FB_SEG_W, FZ_FB_SEG_W / 4u);
                if (all || h != hs[s]) {
                    hs[s] = h;
                    m |= 1u << s;
                }
            }
            if (m) {
                ch[y] = (uint8_t)m;
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
    /* (P2.13, P2.17) the start: the window of scan lines s the whole pass allows (the model above, every rectangle's
     * first and last rows), less FZ_FB_MARGIN at its end for an audio interrupt inside the send; none (taller than a
     * refresh carries): started as the scan passes the first row, and counted split */
    pl.smin_us = 0, pl.smax_us = (int32_t)(2u * FZ_FB_LINES * FZ_FB_LINE_US), pl.t_us = 0, pl.rects = pl.px = pl.rows = 0;
    fz_fb_rects(ch, first, last, &pl, rows, pal);
    fz_fb.rects += pl.rects, fz_fb.px += pl.px, fz_fb.rect_rows += pl.rows;
    {
        uint32_t lo = (uint32_t)(pl.smin_us + (int32_t)FZ_FB_LINE_US - 1) / FZ_FB_LINE_US + FZ_FB_A_MARGIN, hi;
        const int32_t hm = pl.smax_us / (int32_t)FZ_FB_LINE_US - (int32_t)FZ_FB_MARGIN;
        int split = 0;
        if (pl.smin_us < 0)
            lo = FZ_FB_A_MARGIN;
        if (hm < (int32_t)lo) {
            split = 1;                   /* (too tall to be whole in one refresh: counted, sent all the same) */
            lo = first, hi = first;
        } else {
            hi = (uint32_t)hm;
        }
        if (hi >= FZ_FB_LINES)
            hi = FZ_FB_LINES - 1u;       /* (a window past the refresh's end: its part in this one) */
        wait0 = plat_ticks24();
        if (!fz_fb_line(&line)) {
            uint32_t polls = 0;
            while (!(line >= lo && line <= hi)) {
                if ((plat_ticks24() - wait0) / PLAT_TICKS_PER_US > 18000u || ++polls > 10000000u) {   /* (P2.16a: the polls cheap now, the time the bound) */
                    line = 0xFFFFu;
                    break;
                }
                fz_fb_idle();            /* (the input, the LEDs: a wait is not lost) */
                if (fz_fb_line(&line)) {
                    line = 0xFFFFu;
                    break;
                }
            }
        } else {
            line = 0xFFFFu;
        }
        y = (plat_ticks24() - wait0) / PLAT_TICKS_PER_US;
        fz_fb.wait_sum_us += y;
        if (y > fz_fb.wait_max_us)
            fz_fb.wait_max_us = y;
        if (line == 0xFFFFu)
            fz_fb.unsynced++;
        else
            fz_fb.synced++;
        if (line != 0xFFFFu && split)
            fz_fb.split++;
        isr0 = (uint32_t)fz_prof.isr_in_frame;
        t0 = plat_ticks24();
        fz_fb_rects(ch, first, last, 0, rows, pal);
        lcd_sync();
        t0 = (plat_ticks24() - t0) / PLAT_TICKS_PER_US;
        fz_fb.span_sum_us += t0;
        if (t0 > fz_fb.span_max_us)
            fz_fb.span_max_us = t0;
        /* (estimated torn: unsynced; split; or late: the pass's overrun on the model (t0 - its estimate), taken as if
         * at the start, past the window's end B for the line it started at) */
        {
            const int32_t over = (int32_t)(line * FZ_FB_LINE_US) + ((int32_t)t0 - (int32_t)pl.t_us) - pl.smax_us;
            if (line == 0xFFFFu || split || over >= 0) {
                fz_fb.torn++;
                if (line == 0xFFFFu)
                    fz_fb.torn_unsynced++;
                else if (split)
                    fz_fb.torn_split++;
                else {
                    fz_fb.torn_late++;
                    fz_fb.torn_late_isr += (uint32_t)fz_prof.isr_in_frame != isr0;
                    if ((uint32_t)over > fz_fb.late_max_us)
                        fz_fb.late_max_us = (uint32_t)over;
                }
            }
        }
    }
    return n;
}
