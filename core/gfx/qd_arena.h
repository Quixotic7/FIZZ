/* SPDX-License-Identifier: GPL-3.0-only */
/* P2.1: the drawing's fixed arena (research/PHASE2-BUDGET.md §5 item 8). The device has no heap: at its sizes
 * (QD_DEVICE) plutovg's allocations (its canvases, paths, span buffers, the stroker's borders, the glyph cache) come
 * out of one static block of QD_GFX_ARENA bytes (small blocks from its bottom, large from its top). This header is force-included
 * into plutovg's sources (and qd_gfx_plutovg.c) by the device's builds (-include core/gfx/qd_arena.h), so its malloc /
 * calloc / realloc / free are these; it also caps plutovg's glyph cache there (QD_GLYPH_CACHE, a patch of the vendored
 * plutovg-font.c). The host keeps the C library's and the whole cache. */
#pragma once
#ifdef QD_ARENA_REDIRECT
#include "../qd_config.h"
#if defined(QD_GLYPH_CACHE) && !defined(PLUTOVG_GLYPH_CACHE_MAX)
#define PLUTOVG_GLYPH_CACHE_MAX QD_GLYPH_CACHE
#endif
#endif
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

void *qd_arena_malloc(size_t n);
void *qd_arena_calloc(size_t a, size_t b);
void *qd_arena_realloc(void *p, size_t n);
void qd_arena_free(void *p);
size_t qd_arena_peak(void);             /* the most it has held at once (bytes, with the blocks' headers) */
size_t qd_arena_live(void);
size_t qd_arena_size(void);
unsigned qd_arena_fails(void);          /* requests it could not meet (the drawing skips what needed them) */
void qd_arena_use(int on);              /* (the host's: the core drawing now; else the C library's, a tool's own) */

#ifdef QD_ARENA_SITES
void qd_arena_site(const char *file, int line);   /* (the host's debugging: the next allocation's call site) */
void qd_arena_sites(void);                        /* .. the live bytes by site, printed */
#endif
#if defined(QD_ARENA_REDIRECT) && defined(QD_ARENA_SITES)
#define malloc(n) (qd_arena_site(__FILE__, __LINE__), qd_arena_malloc(n))
#define calloc(a, b) (qd_arena_site(__FILE__, __LINE__), qd_arena_calloc((a), (b)))
#define realloc(p, n) (qd_arena_site(__FILE__, __LINE__), qd_arena_realloc((p), (n)))
#define free(p) qd_arena_free(p)
#elif defined(QD_ARENA_REDIRECT)
#define malloc(n) qd_arena_malloc(n)
#define calloc(a, b) qd_arena_calloc((a), (b))
#define realloc(p, n) qd_arena_realloc((p), (n))
#define free(p) qd_arena_free(p)
#endif

#ifdef __cplusplus
}
#endif
