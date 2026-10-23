#include "plutovg-private.h"
#include "plutovg-utils.h"

#include "plutovg-ft-raster.h"
#include "plutovg-ft-stroker.h"
#include "plutovg-ft-math.h"                 /* (FIZZ: the stroker's angles, cull_runs) */

#include <limits.h>
#include <stdlib.h>

void plutovg_span_buffer_init(plutovg_span_buffer_t* span_buffer)
{
    plutovg_array_init(span_buffer->spans);
    plutovg_span_buffer_reset(span_buffer);
}

void plutovg_span_buffer_init_rect(plutovg_span_buffer_t* span_buffer, int x, int y, int width, int height)
{
    plutovg_array_clear(span_buffer->spans);
    plutovg_array_ensure(span_buffer->spans, height);
    plutovg_span_t* spans = span_buffer->spans.data;
    for(int i = 0; i < height; i++) {
        spans[i].x = x;
        spans[i].y = y + i;
        spans[i].len = width;
        spans[i].coverage = 255;
    }

    span_buffer->x = x;
    span_buffer->y = y;
    span_buffer->w = width;
    span_buffer->h = height;
    span_buffer->spans.size = height;
}

void plutovg_span_buffer_reset(plutovg_span_buffer_t* span_buffer)
{
    plutovg_array_clear(span_buffer->spans);
    span_buffer->x = 0;
    span_buffer->y = 0;
    span_buffer->w = -1;
    span_buffer->h = -1;
}

void plutovg_span_buffer_destroy(plutovg_span_buffer_t* span_buffer)
{
    plutovg_array_destroy(span_buffer->spans);
}

void plutovg_span_buffer_copy(plutovg_span_buffer_t* span_buffer, const plutovg_span_buffer_t* source)
{
    plutovg_array_clear(span_buffer->spans);
    plutovg_array_append(span_buffer->spans, source->spans);
    span_buffer->x = source->x;
    span_buffer->y = source->y;
    span_buffer->w = source->w;
    span_buffer->h = source->h;
}

bool plutovg_span_buffer_contains(const plutovg_span_buffer_t* span_buffer, float x, float y)
{
    const int ix = (int)floorf(x);
    const int iy = (int)floorf(y);

    for(int i = 0; i < span_buffer->spans.size; i++) {
        plutovg_span_t* span = &span_buffer->spans.data[i];
        if(span->y != iy)
            continue;
        if(ix >= span->x && ix < (span->x + span->len)) {
            return true;
        }
    }

    return false;
}

static void plutovg_span_buffer_update_extents(plutovg_span_buffer_t* span_buffer)
{
    if(span_buffer->w != -1 && span_buffer->h != -1)
        return;
    if(span_buffer->spans.size == 0) {
        span_buffer->x = 0;
        span_buffer->y = 0;
        span_buffer->w = 0;
        span_buffer->h = 0;
        return;
    }

    plutovg_span_t* spans = span_buffer->spans.data;
    int x1 = INT_MAX;
    int y1 = spans[0].y;
    int x2 = 0;
    int y2 = spans[span_buffer->spans.size - 1].y;
    for(int i = 0; i < span_buffer->spans.size; i++) {
        if(spans[i].x < x1) x1 = spans[i].x;
        if(spans[i].x + spans[i].len > x2) x2 = spans[i].x + spans[i].len;
    }

    span_buffer->x = x1;
    span_buffer->y = y1;
    span_buffer->w = x2 - x1;
    span_buffer->h = y2 - y1 + 1;
}

void plutovg_span_buffer_extents(plutovg_span_buffer_t* span_buffer, plutovg_rect_t* extents)
{
    plutovg_span_buffer_update_extents(span_buffer);
    extents->x = span_buffer->x;
    extents->y = span_buffer->y;
    extents->w = span_buffer->w;
    extents->h = span_buffer->h;
}

void plutovg_span_buffer_intersect(plutovg_span_buffer_t* span_buffer, const plutovg_span_buffer_t* a, const plutovg_span_buffer_t* b)
{
    plutovg_span_buffer_reset(span_buffer);
    plutovg_array_ensure(span_buffer->spans, plutovg_max(a->spans.size, b->spans.size));

    plutovg_span_t* a_spans = a->spans.data;
    plutovg_span_t* a_end = a_spans + a->spans.size;

    plutovg_span_t* b_spans = b->spans.data;
    plutovg_span_t* b_end = b_spans + b->spans.size;
    while(a_spans < a_end && b_spans < b_end) {
        if(b_spans->y > a_spans->y) {
            ++a_spans;
            continue;
        }

        if(a_spans->y != b_spans->y) {
            ++b_spans;
            continue;
        }

        int ax1 = a_spans->x;
        int ax2 = ax1 + a_spans->len;
        int bx1 = b_spans->x;
        int bx2 = bx1 + b_spans->len;
        if(bx1 < ax1 && bx2 < ax1) {
            ++b_spans;
            continue;
        }

        if(ax1 < bx1 && ax2 < bx1) {
            ++a_spans;
            continue;
        }

        int x = plutovg_max(ax1, bx1);
        int len = plutovg_min(ax2, bx2) - x;
        if(len) {
            plutovg_array_ensure(span_buffer->spans, 1);
            plutovg_span_t* span = span_buffer->spans.data + span_buffer->spans.size;
            span->x = x;
            span->len = len;
            span->y = a_spans->y;
            span->coverage = (a_spans->coverage * b_spans->coverage) / 255;
            span_buffer->spans.size += 1;
        }

        if(ax2 < bx2) {
            ++a_spans;
        } else {
            ++b_spans;
        }
    }
}

#define ALIGN_SIZE(size) (((size) + 7ul) & ~7ul)
static PVG_FT_Outline* ft_outline_create(int points, int contours)
{
    size_t points_size = ALIGN_SIZE((points + contours) * sizeof(PVG_FT_Vector));
    size_t tags_size = ALIGN_SIZE((points + contours) * sizeof(char));
    size_t contours_size = ALIGN_SIZE(contours * sizeof(int));
    size_t contours_flag_size = ALIGN_SIZE(contours * sizeof(char));
    PVG_FT_Outline* outline = malloc(points_size + tags_size + contours_size + contours_flag_size + sizeof(PVG_FT_Outline));

    PVG_FT_Byte* outline_data = (PVG_FT_Byte*)(outline + 1);
    outline->points = (PVG_FT_Vector*)(outline_data);
    outline->tags = (char*)(outline_data + points_size);
    outline->contours = (int*)(outline_data + points_size + tags_size);
    outline->contours_flag = (char*)(outline_data + points_size + tags_size + contours_size);
    outline->n_points = 0;
    outline->n_contours = 0;
    outline->flags = 0x0;
    return outline;
}

static void ft_outline_destroy(PVG_FT_Outline* outline)
{
    free(outline);
}

#define FT_COORD(x) (PVG_FT_Pos)(roundf(x * 64))
static void ft_outline_move_to(PVG_FT_Outline* ft, float x, float y)
{
    ft->points[ft->n_points].x = FT_COORD(x);
    ft->points[ft->n_points].y = FT_COORD(y);
    ft->tags[ft->n_points] = PVG_FT_CURVE_TAG_ON;
    if(ft->n_points) {
        ft->contours[ft->n_contours] = ft->n_points - 1;
        ft->n_contours++;
    }

    ft->contours_flag[ft->n_contours] = 1;
    ft->n_points++;
}

static void ft_outline_line_to(PVG_FT_Outline* ft, float x, float y)
{
    ft->points[ft->n_points].x = FT_COORD(x);
    ft->points[ft->n_points].y = FT_COORD(y);
    ft->tags[ft->n_points] = PVG_FT_CURVE_TAG_ON;
    ft->n_points++;
}

static void ft_outline_cubic_to(PVG_FT_Outline* ft, float x1, float y1, float x2, float y2, float x3, float y3)
{
    ft->points[ft->n_points].x = FT_COORD(x1);
    ft->points[ft->n_points].y = FT_COORD(y1);
    ft->tags[ft->n_points] = PVG_FT_CURVE_TAG_CUBIC;
    ft->n_points++;

    ft->points[ft->n_points].x = FT_COORD(x2);
    ft->points[ft->n_points].y = FT_COORD(y2);
    ft->tags[ft->n_points] = PVG_FT_CURVE_TAG_CUBIC;
    ft->n_points++;

    ft->points[ft->n_points].x = FT_COORD(x3);
    ft->points[ft->n_points].y = FT_COORD(y3);
    ft->tags[ft->n_points] = PVG_FT_CURVE_TAG_ON;
    ft->n_points++;
}

static void ft_outline_close(PVG_FT_Outline* ft)
{
    ft->contours_flag[ft->n_contours] = 0;
    int index = ft->n_contours ? ft->contours[ft->n_contours - 1] + 1 : 0;
    if(index == ft->n_points)
        return;
    ft->points[ft->n_points].x = ft->points[index].x;
    ft->points[ft->n_points].y = ft->points[index].y;
    ft->tags[ft->n_points] = PVG_FT_CURVE_TAG_ON;
    ft->n_points++;
}

static void ft_outline_end(PVG_FT_Outline* ft)
{
    if(ft->n_points) {
        ft->contours[ft->n_contours] = ft->n_points - 1;
        ft->n_contours++;
    }
}

static PVG_FT_Outline* ft_outline_convert_stroke(const plutovg_path_t* path, const plutovg_matrix_t* matrix, const plutovg_stroke_data_t* stroke_data);

static PVG_FT_Outline* ft_outline_convert(const plutovg_path_t* path, const plutovg_matrix_t* matrix, const plutovg_stroke_data_t* stroke_data)
{
    if(stroke_data) {
        return ft_outline_convert_stroke(path, matrix, stroke_data);
    }

    plutovg_path_iterator_t it;
    plutovg_path_iterator_init(&it, path);

    plutovg_point_t points[3];
    PVG_FT_Outline* outline = ft_outline_create(path->num_points, path->num_contours);
    while(plutovg_path_iterator_has_next(&it)) {
        switch(plutovg_path_iterator_next(&it, points)) {
        case PLUTOVG_PATH_COMMAND_MOVE_TO:
            plutovg_matrix_map_points(matrix, points, points, 1);
            ft_outline_move_to(outline, points[0].x, points[0].y);
            break;
        case PLUTOVG_PATH_COMMAND_LINE_TO:
            plutovg_matrix_map_points(matrix, points, points, 1);
            ft_outline_line_to(outline, points[0].x, points[0].y);
            break;
        case PLUTOVG_PATH_COMMAND_CUBIC_TO:
            plutovg_matrix_map_points(matrix, points, points, 3);
            ft_outline_cubic_to(outline, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y);
            break;
        case PLUTOVG_PATH_COMMAND_CLOSE:
            ft_outline_close(outline);
            break;
        }
    }

    ft_outline_end(outline);
    return outline;
}

static PVG_FT_Outline* ft_outline_convert_dash(const plutovg_path_t* path, const plutovg_matrix_t* matrix, const plutovg_stroke_dash_t* stroke_dash)
{
    if(stroke_dash->array.size == 0)
        return ft_outline_convert(path, matrix, NULL);
    plutovg_path_t* dashed = plutovg_path_clone_dashed(path, stroke_dash->offset, stroke_dash->array.data, stroke_dash->array.size);
    PVG_FT_Outline* outline = ft_outline_convert(dashed, matrix, NULL);
    plutovg_path_destroy(dashed);
    return outline;
}

/* FIZZ (P2.1, a patch of this vendored copy): with a row window (plutovg_raster_rows, a strip), only what can reach the
 * window's rows becomes the outline. A fill: the contours whose rows are outside it are left out (a closed outline away
 * from a row adds nothing to its coverage). A stroke (P2.1b): every segment whose rows, widened by the most the pen, its
 * caps and joins can add, are outside it is left out: in an open contour, the kept segments in a row are a run, an open
 * contour of their own, with one left-out segment more at either end (a closed contour stays whole, or out: the
 * stroker's joins at a closed contour's start are its own). So a cut is a segment away from the widened rows: the
 * joins next to the run are made as before (an inner corner may reach a long way along its segments), and the cap
 * that replaces the join at the cut, and that join's own reach along the left-out segment, stay outside the strip. Each row's cells come only from the edges in that row: the strip's pixels are the whole
 * render's, exactly, and the outline holds only what is near the strip (a long path, the ape's lines, a dashed line) */
int plutovg_raster_rows_get(int* y0, int* y1);
typedef struct {
    PVG_FT_Outline* o;                  /* (0: counting) */
    int np, nc;
} cull_out_t;
static void co_move(cull_out_t* c, plutovg_point_t p)
{
    if(c->o) ft_outline_move_to(c->o, p.x, p.y);
    c->np++, c->nc++;
}
static void co_seg(cull_out_t* c, plutovg_path_command_t cmd, const plutovg_point_t* q)
{
    if(cmd == PLUTOVG_PATH_COMMAND_CUBIC_TO) {
        if(c->o) ft_outline_cubic_to(c->o, q[0].x, q[0].y, q[1].x, q[1].y, q[2].x, q[2].y);
        c->np += 3;
    } else {
        if(c->o) ft_outline_line_to(c->o, q[0].x, q[0].y);
        c->np++;
    }
}
static void co_close(cull_out_t* c)
{
    if(c->o) ft_outline_close(c->o);
    c->np++;
}
static int cull_in(plutovg_point_t a, const plutovg_point_t* q, int n, float lo, float hi)
{
    float mn = a.y, mx = a.y;
    for(int i = 0; i < n; i++) {
        if(q[i].y < mn) mn = q[i].y;
        if(q[i].y > mx) mx = q[i].y;
    }
    return mx >= lo && mn <= hi;
}
static int seg_n(plutovg_path_command_t cmd) { return cmd == PLUTOVG_PATH_COMMAND_CUBIC_TO ? 3 : 1; }
static int seg_len(plutovg_point_t a, const plutovg_point_t* q, int n)   /* (a length at the stroker's 1/64 px) */
{
    for(int i = 0; i < n; i++)
        if(FT_COORD(q[i].x) != FT_COORD(a.x) || FT_COORD(q[i].y) != FT_COORD(a.y))
            return 1;
    return 0;
}

/* an open contour of a stroke, cut (cull_walk): its segments near the strip, as runs, each grown at either end until
 * the stroker's border edges there cannot differ from the whole contour's inside the strip. The stroker draws a border
 * line straight on through a vertex where the line does not turn (it moves the line's end point), so an edge can run
 * from far outside the strip into it: a run starts one segment before the first vertex where its line turns (a corner:
 * a turn of the stroker's own angle, or a curve next to it) at or before the strip, and ends one segment after the
 * last such vertex after it. Every edge that the cut changes then lies on left-out segments, outside the strip (their
 * rows widened by the pen's reach) */
#define CF_IN 1
#define CF_ZERO 2
#define CF_CORNER 4                     /* (a corner at the segment's start: the stroker breaks its edges there) */
#define CF_EMIT 8
static int cull_runs(cull_out_t* c, const plutovg_matrix_t* m, plutovg_path_iterator_t body,
                     plutovg_path_iterator_t look, plutovg_point_t start, float lo, float hi)
{
    plutovg_path_iterator_t w = body;
    plutovg_point_t q[3], cur = start;
    int n = 0, i, j, have = 0;
    PVG_FT_Angle prev = 0;
    int prev_cubic = 0;
    while(w.index < look.index) {
        plutovg_path_iterator_next(&w, q);
        n++;
    }
    uint8_t* f = malloc((size_t)(n ? n : 1));
    if(!f)
        return 0;
    for(w = body, i = 0; i < n; i++) {                  /* the flags */
        plutovg_path_command_t k = plutovg_path_iterator_next(&w, q);
        plutovg_matrix_map_points(m, q, q, seg_n(k));
        f[i] = cull_in(cur, q, seg_n(k), lo, hi) ? CF_IN : 0;
        if(!seg_len(cur, q, seg_n(k))) {
            f[i] |= CF_ZERO;
        } else {
            const int cubic = k == PLUTOVG_PATH_COMMAND_CUBIC_TO;
            PVG_FT_Angle ang = 0;
            if(!cubic)
                ang = PVG_FT_Atan2(FT_COORD(q[0].x) - FT_COORD(cur.x), FT_COORD(q[0].y) - FT_COORD(cur.y));
            if(!have || cubic || prev_cubic || PVG_FT_Angle_Diff(prev, ang) != 0)
                f[i] |= CF_CORNER;                      /* (the contour's start counts as one) */
            have = 1, prev = ang, prev_cubic = cubic;
        }
        cur = q[seg_n(k) - 1];
    }
    for(i = 0; i < n; i++) {                            /* each kept segment's run grown */
        if(!(f[i] & CF_IN) || (f[i] & CF_ZERO))
            continue;
        f[i] |= CF_EMIT;
        for(j = i;;) {                                  /* back: to one segment before a corner */
            const int corner = (f[j] & CF_CORNER) != 0;
            int p = j - 1;
            while(p >= 0 && (f[p] & CF_ZERO))
                f[p--] |= CF_EMIT;
            if(p < 0)
                break;
            f[p] |= CF_EMIT;
            if(corner)
                break;
            j = p;
        }
        for(j = i;;) {                                  /* on: to one segment after a corner */
            int q2 = j + 1;
            while(q2 < n && (f[q2] & CF_ZERO))
                f[q2++] |= CF_EMIT;
            if(q2 >= n)
                break;
            f[q2] |= CF_EMIT;
            if(f[q2] & CF_CORNER)
                break;
            j = q2;
        }
    }
    {
        int run = 0;
        cur = start;
        for(w = body, i = 0; i < n; i++) {
            plutovg_path_command_t k = plutovg_path_iterator_next(&w, q);
            plutovg_matrix_map_points(m, q, q, seg_n(k));
            if(f[i] & CF_EMIT) {
                if(!run) co_move(c, cur);
                co_seg(c, k, q);
                run = 1;
            } else {
                run = 0;
            }
            cur = q[seg_n(k) - 1];
        }
    }
    free(f);
    return 1;
}

/* one pass over the path: counting (c->o 0) or writing; 0: a shape this does not take (a segment after a CLOSE with no
 * MOVE_TO): the whole path then */
static int cull_walk(const plutovg_path_t* path, const plutovg_matrix_t* m, float lo, float hi, int stroke, cull_out_t* c)
{
    plutovg_path_iterator_t it;
    plutovg_point_t q[3], cur = {0, 0}, start = {0, 0};
    plutovg_path_iterator_init(&it, path);
    while(plutovg_path_iterator_has_next(&it)) {
        plutovg_path_command_t cmd = plutovg_path_iterator_next(&it, q);
        if(cmd != PLUTOVG_PATH_COMMAND_MOVE_TO)
            return 0;
        plutovg_matrix_map_points(m, q, q, 1);
        cur = start = q[0];
        /* the contour looked at first: its rows, a cut, its first run (segments), a CLOSE and whether it is kept */
        plutovg_path_iterator_t body = it, look = it;
        float mn = start.y, mx = start.y;
        int cut = 0, closed = 0;
        plutovg_point_t lc = start;
        while(plutovg_path_iterator_has_next(&look)) {
            plutovg_path_iterator_t at = look;
            plutovg_path_command_t k = plutovg_path_iterator_next(&look, q);
            if(k == PLUTOVG_PATH_COMMAND_MOVE_TO) {
                look = at;
                break;
            }
            if(closed)
                return 0;
            if(k == PLUTOVG_PATH_COMMAND_CLOSE) {
                closed = 1;
                continue;
            }
            plutovg_matrix_map_points(m, q, q, seg_n(k));
            for(int i = 0; i < seg_n(k); i++) {
                if(q[i].y < mn) mn = q[i].y;
                if(q[i].y > mx) mx = q[i].y;
            }
            if(!cull_in(lc, q, seg_n(k), lo, hi))
                cut = 1;
            lc = q[seg_n(k) - 1];
        }
        it = look;
        if(!stroke || !cut || closed) {                 /* (a fill's contour, a stroke's closed one or one all kept:
                                                         * whole, or out) */
            if(mx < lo || mn > hi)
                continue;
            plutovg_path_iterator_t w = body;
            co_move(c, start);
            while(w.index < look.index) {
                plutovg_path_command_t k = plutovg_path_iterator_next(&w, q);
                if(k == PLUTOVG_PATH_COMMAND_CLOSE) {
                    co_close(c);
                    continue;
                }
                plutovg_matrix_map_points(m, q, q, seg_n(k));
                co_seg(c, k, q);
            }
            continue;
        }
        {   /* an open contour of a stroke, cut: its runs (cull_runs) */
            if(!cull_runs(c, m, body, look, start, lo, hi))
                return 0;
        }
    }
    return 1;
}
/* P2.1b: a dashed stroke's dashes made one at a time (no dashed copy of the path) and each kept whole when it can reach
 * the window: the dashes are open contours of their own */
#define DC_MAX 64
typedef struct {
    cull_out_t* c;
    const plutovg_matrix_t* m;
    float lo, hi;
    plutovg_point_t p[DC_MAX];
    int n, bad;
} dash_cull_t;
static void dc_flush(dash_cull_t* d)
{
    float mn, mx;
    if(d->n == 0)
        return;
    mn = mx = d->p[0].y;
    for(int i = 1; i < d->n; i++) {
        if(d->p[i].y < mn) mn = d->p[i].y;
        if(d->p[i].y > mx) mx = d->p[i].y;
    }
    if(mx >= d->lo && mn <= d->hi) {
        co_move(d->c, d->p[0]);
        for(int i = 1; i < d->n; i++)
            co_seg(d->c, PLUTOVG_PATH_COMMAND_LINE_TO, &d->p[i]);
    }
    d->n = 0;
}
static void dc_func(void* closure, plutovg_path_command_t command, const plutovg_point_t* points, int npoints)
{
    dash_cull_t* d = (dash_cull_t*)closure;
    plutovg_point_t q;
    if(command != PLUTOVG_PATH_COMMAND_MOVE_TO && command != PLUTOVG_PATH_COMMAND_LINE_TO) {
        d->bad = 1;                                     /* (a curve or a close: undashed; the whole path then) */
        return;
    }
    if(command == PLUTOVG_PATH_COMMAND_MOVE_TO)
        dc_flush(d);
    if(d->n == 0 && command != PLUTOVG_PATH_COMMAND_MOVE_TO) {
        d->bad = 1;
        return;
    }
    if(d->n >= DC_MAX) {
        d->bad = 1;
        return;
    }
    plutovg_matrix_map_points(d->m, points, &q, 1);
    d->p[d->n++] = q;
    (void)npoints;
}
static PVG_FT_Outline* ft_outline_convert_dash_cull(const plutovg_path_t* path, const plutovg_matrix_t* matrix,
                                                    const plutovg_stroke_dash_t* dash, double reach)
{
    int y0, y1;
    cull_out_t c = {0, 0, 0};
    dash_cull_t d;
    if(!plutovg_raster_rows_get(&y0, &y1))
        return NULL;
    d.c = &c, d.m = matrix, d.n = 0, d.bad = 0;
    d.lo = (float)(y0 - reach - 2.0), d.hi = (float)(y1 + reach + 2.0);
    plutovg_path_traverse_dashed(path, dash->offset, dash->array.data, dash->array.size, dc_func, &d);
    dc_flush(&d);
    if(d.bad)
        return NULL;
    c.o = ft_outline_create(c.np, c.nc);
    c.np = c.nc = 0;
    d.n = 0;
    plutovg_path_traverse_dashed(path, dash->offset, dash->array.data, dash->array.size, dc_func, &d);
    dc_flush(&d);
    ft_outline_end(c.o);
    return c.o;
}

/* the path's outline near the window (none: the whole) */
static PVG_FT_Outline* ft_outline_convert_cull(const plutovg_path_t* path, const plutovg_matrix_t* matrix, double reach,
                                               int stroke)
{
    int y0, y1;
    cull_out_t c = {0, 0, 0};
    if(!plutovg_raster_rows_get(&y0, &y1))
        return NULL;
    const float lo = (float)(y0 - reach - 2.0), hi = (float)(y1 + reach + 2.0);
    if(!cull_walk(path, matrix, lo, hi, stroke, &c))
        return NULL;
    c.o = ft_outline_create(c.np, c.nc);
    c.np = c.nc = 0;
    cull_walk(path, matrix, lo, hi, stroke, &c);
    ft_outline_end(c.o);
    return c.o;
}

static PVG_FT_Outline* ft_outline_convert_stroke(const plutovg_path_t* path, const plutovg_matrix_t* matrix, const plutovg_stroke_data_t* stroke_data)
{
    double scale_x = sqrt(matrix->a * matrix->a + matrix->b * matrix->b);
    double scale_y = sqrt(matrix->c * matrix->c + matrix->d * matrix->d);

    double scale = hypot(scale_x, scale_y) / PLUTOVG_SQRT2;
    double width = stroke_data->style.width * scale;

    PVG_FT_Fixed ftWidth = (PVG_FT_Fixed)(width * 0.5 * (1 << 6));
    PVG_FT_Fixed ftMiterLimit = (PVG_FT_Fixed)(stroke_data->style.miter_limit * (1 << 16));

    PVG_FT_Stroker_LineCap ftCap;
    switch(stroke_data->style.cap) {
    case PLUTOVG_LINE_CAP_SQUARE:
        ftCap = PVG_FT_STROKER_LINECAP_SQUARE;
        break;
    case PLUTOVG_LINE_CAP_ROUND:
        ftCap = PVG_FT_STROKER_LINECAP_ROUND;
        break;
    default:
        ftCap = PVG_FT_STROKER_LINECAP_BUTT;
        break;
    }

    PVG_FT_Stroker_LineJoin ftJoin;
    switch(stroke_data->style.join) {
    case PLUTOVG_LINE_JOIN_BEVEL:
        ftJoin = PVG_FT_STROKER_LINEJOIN_BEVEL;
        break;
    case PLUTOVG_LINE_JOIN_ROUND:
        ftJoin = PVG_FT_STROKER_LINEJOIN_ROUND;
        break;
    default:
        ftJoin = PVG_FT_STROKER_LINEJOIN_MITER_FIXED;
        break;
    }

    PVG_FT_Stroker stroker;
    PVG_FT_Stroker_New(&stroker);
    PVG_FT_Stroker_Set(stroker, ftWidth, ftCap, ftJoin, ftMiterLimit);

    PVG_FT_Outline* outline;
    {   /* FIZZ: only what reaches the strip (above); the pen's reach: half its width, times the miter limit for mitred
         * joins (square caps: x sqrt 2) */
        const double ml = stroke_data->style.join == PLUTOVG_LINE_JOIN_MITER && stroke_data->style.miter_limit > 1.5f
                              ? stroke_data->style.miter_limit : 1.5;
        plutovg_path_t* dashed = NULL;
        const plutovg_path_t* src = path;
        outline = stroke_data->dash.array.size ? ft_outline_convert_dash_cull(path, matrix, &stroke_data->dash,
                                                                              width * 0.5 * ml) : NULL;
        if(!outline && stroke_data->dash.array.size) {
            dashed = plutovg_path_clone_dashed(path, stroke_data->dash.offset, stroke_data->dash.array.data,
                                               stroke_data->dash.array.size);
            src = dashed;
        }
        if(!outline)
            outline = ft_outline_convert_cull(src, matrix, width * 0.5 * ml, 1);
        if(!outline)
            outline = ft_outline_convert(src, matrix, NULL);
        if(dashed)
            plutovg_path_destroy(dashed);
    }
    PVG_FT_Stroker_ParseOutline(stroker, outline);

    PVG_FT_UInt points;
    PVG_FT_UInt contours;
    PVG_FT_Stroker_GetCounts(stroker, &points, &contours);

    PVG_FT_Outline* stroke_outline = ft_outline_create(points, contours);
    PVG_FT_Stroker_Export(stroker, stroke_outline);

    PVG_FT_Stroker_Done(stroker);
    ft_outline_destroy(outline);
    return stroke_outline;
}

static void spans_generation_callback(int count, const PVG_FT_Span* spans, void* user)
{
    plutovg_span_buffer_t* span_buffer = (plutovg_span_buffer_t*)(user);
#ifdef QD_ARENA_REDIRECT
    plutovg_array_ensure(span_buffer->spans, count);    /* (FIZZ: the device's 8-byte spans) */
    for(int i = 0; i < count; i++) {
        plutovg_span_t* d = span_buffer->spans.data + span_buffer->spans.size + i;
        d->x = (short)spans[i].x, d->len = (short)spans[i].len, d->y = (short)spans[i].y;
        d->coverage = spans[i].coverage;
    }
    span_buffer->spans.size += count;
#else
    plutovg_array_append_data(span_buffer->spans, spans, count);
#endif
}

/* FIZZ (P2.1b, a patch of this vendored copy): set by plutovg_canvas_fill_consume / stroke_consume: the path is not
 * wanted after this paint, so its memory goes back as soon as its outline is made, before the spans (the device's
 * fixed arena holds one of the two at a time) */
int plutovg_rasterize_consume;
void plutovg_rasterize(plutovg_span_buffer_t* span_buffer, const plutovg_path_t* path, const plutovg_matrix_t* matrix, const plutovg_rect_t* clip_rect, const plutovg_stroke_data_t* stroke_data, plutovg_fill_rule_t winding)
{
    PVG_FT_Outline* outline = stroke_data ? NULL : ft_outline_convert_cull(path, matrix, 0.0, 0);   /* (FIZZ) */
    if(!outline)
        outline = ft_outline_convert(path, matrix, stroke_data);
    if(plutovg_rasterize_consume) {                     /* (FIZZ P2.1b: the path, an outline now, given back first) */
        plutovg_path_t* p = (plutovg_path_t*)path;
        free(p->elements.data);
        p->elements.data = NULL;
        p->elements.size = p->elements.capacity = 0;
        p->num_points = p->num_contours = p->num_curves = 0;
    }
    if(stroke_data) {
        outline->flags = PVG_FT_OUTLINE_NONE;
    } else {
        switch(winding) {
        case PLUTOVG_FILL_RULE_EVEN_ODD:
            outline->flags = PVG_FT_OUTLINE_EVEN_ODD_FILL;
            break;
        default:
            outline->flags = PVG_FT_OUTLINE_NONE;
            break;
        }
    }

    PVG_FT_Raster_Params params;
    params.flags = PVG_FT_RASTER_FLAG_DIRECT | PVG_FT_RASTER_FLAG_AA;
    params.gray_spans = spans_generation_callback;
    params.user = span_buffer;
    params.source = outline;
    if(clip_rect) {
        params.flags |= PVG_FT_RASTER_FLAG_CLIP;
        params.clip_box.xMin = (PVG_FT_Pos)clip_rect->x;
        params.clip_box.yMin = (PVG_FT_Pos)clip_rect->y;
        params.clip_box.xMax = (PVG_FT_Pos)(clip_rect->x + clip_rect->w);
        params.clip_box.yMax = (PVG_FT_Pos)(clip_rect->y + clip_rect->h);
    }

    plutovg_span_buffer_reset(span_buffer);
    PVG_FT_Raster_Render(&params);
    ft_outline_destroy(outline);
}
