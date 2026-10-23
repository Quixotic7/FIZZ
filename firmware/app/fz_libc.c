/* SPDX-License-Identifier: MPL-2.0 */
/* What FIZZ's core needs of a C library that neither felucca-bsp's libc.c (memset, memcpy, memcmp: in fm1-base's
 * unity build) nor JieLi's newlib (libc.a, linked after FIZZ's units by firmware/build.py) gives the unit as it should:
 *   snprintf / vsnprintf  newlib's libc.a for pi32v2 has none (JieLi's SDK prints through its own);
 *   memmove               newlib's shares its object with memcpy, which the unity build already defines;
 *   atof                  newlib's goes through strtod's big-number code and its allocator (no heap here).
 * The formats are the ones FIZZ uses (core/: %d %u %x %c %s %%, flags - and 0, a width, a precision for s and f, *,
 * the l length; %f for one label). The unit only: the host simulator uses its own C library. */
#ifndef PLAT_HOST
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *dp = d;
    const unsigned char *sp = s;
    if (dp == sp || !n)
        return d;
    if (dp < sp || dp >= sp + n)
        while (n--)
            *dp++ = *sp++;
    else
        for (dp += n, sp += n; n--;)
            *--dp = *--sp;
    return d;
}

typedef struct {
    char *b;
    size_t n, at;                        /* the buffer's size, the characters made (written while they fit) */
} out_t;

static void put(out_t *o, char c)
{
    if (o->at + 1u < o->n)
        o->b[o->at] = c;
    o->at++;
}

static void put_field(out_t *o, const char *s, size_t len, int width, int left, char pad, const char *sign)
{
    size_t sl = 0;
    int fill;
    while (sign && sign[sl])
        sl++;
    fill = width - (int)(len + sl);
    if (!left && pad == ' ')
        for (; fill > 0; fill--)
            put(o, ' ');
    while (sl--)
        put(o, *sign++);
    if (!left && pad == '0')
        for (; fill > 0; fill--)
            put(o, '0');
    while (len--)
        put(o, *s++);
    if (left)
        for (; fill > 0; fill--)
            put(o, ' ');
}

static size_t utoa_base(char *t, unsigned long v, unsigned base)   /* digits reversed into t; their count */
{
    size_t k = 0;
    do {
        const unsigned d = (unsigned)(v % base);
        t[k++] = (char)(d < 10u ? '0' + d : 'a' + d - 10u);
        v /= base;
    } while (v);
    return k;
}

int vsnprintf(char *buf, size_t size, const char *f, va_list ap)
{
    out_t o = {buf, size, 0};
    for (; *f; f++) {
        int left = 0, width = 0, prec = -1, lng = 0;
        char pad = ' ', t[24], r[24];
        size_t k, i;
        if (*f != '%') {
            put(&o, *f);
            continue;
        }
        for (f++;; f++) {
            if (*f == '-')
                left = 1;
            else if (*f == '0')
                pad = '0';
            else if (*f != '+' && *f != ' ' && *f != '#')
                break;
        }
        if (*f == '*') {
            width = va_arg(ap, int);
            if (width < 0)
                left = 1, width = -width;
            f++;
        } else
            while (*f >= '0' && *f <= '9')
                width = width * 10 + (*f++ - '0');
        if (*f == '.') {
            f++;
            prec = 0;
            if (*f == '*') {
                prec = va_arg(ap, int);
                f++;
            } else
                while (*f >= '0' && *f <= '9')
                    prec = prec * 10 + (*f++ - '0');
        }
        while (*f == 'l' || *f == 'h' || *f == 'z')
            lng |= *f++ != 'h';
        if (left)
            pad = ' ';
        switch (*f) {
        case 'd':
        case 'i': {
            const long v = lng ? va_arg(ap, long) : va_arg(ap, int);
            const unsigned long u = v < 0 ? 0ul - (unsigned long)v : (unsigned long)v;
            k = utoa_base(t, u, 10u);
            for (i = 0; i < k; i++)
                r[i] = t[k - 1u - i];
            put_field(&o, r, k, width, left, pad, v < 0 ? "-" : "");
            break;
        }
        case 'u':
        case 'x':
        case 'X': {
            const unsigned long u = lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
            k = utoa_base(t, u, *f == 'u' ? 10u : 16u);
            for (i = 0; i < k; i++)
                r[i] = (char)(*f == 'X' && t[k - 1u - i] >= 'a' ? t[k - 1u - i] - 32 : t[k - 1u - i]);
            put_field(&o, r, k, width, left, pad, "");
            break;
        }
        case 'c':
            r[0] = (char)va_arg(ap, int);
            put_field(&o, r, 1, width, left, ' ', "");
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            size_t n = 0;
            if (!s)
                s = "(null)";
            while (s[n] && (prec < 0 || n < (size_t)prec))
                n++;
            put_field(&o, s, n, width, left, ' ', "");
            break;
        }
        case 'f':
        case 'F': {
            double v = va_arg(ap, double);
            unsigned long ip, fp, scale = 1;
            int p = prec < 0 ? 6 : prec > 6 ? 6 : prec, neg = v < 0.0;
            if (neg)
                v = -v;
            for (i = 0; i < (size_t)p; i++)
                scale *= 10u;
            v = v * (double)scale + 0.5;
            if (v > 4.0e18)              /* (FIZZ's values are small: a big one is shown as its top) */
                v = 4.0e18;
            ip = (unsigned long)(v / (double)scale);
            fp = (unsigned long)(v - (double)ip * (double)scale);
            k = utoa_base(t, ip, 10u);
            for (i = 0; i < k; i++)
                r[i] = t[k - 1u - i];
            if (p) {
                r[k++] = '.';
                for (i = (size_t)p; i--;) {
                    r[k + i] = (char)('0' + fp % 10u);
                    fp /= 10u;
                }
                k += (size_t)p;
            }
            put_field(&o, r, k, width, left, pad, neg ? "-" : "");
            break;
        }
        case '%':
            put(&o, '%');
            break;
        case 0:
            f--;
            break;
        default:                         /* (not one of FIZZ's: shown as written) */
            put(&o, '%');
            put(&o, *f);
            break;
        }
    }
    if (size)
        buf[o.at < size ? o.at : size - 1u] = 0;
    return (int)o.at;
}

int snprintf(char *buf, size_t size, const char *f, ...)
{
    va_list ap;
    int n;
    va_start(ap, f);
    n = vsnprintf(buf, size, f, ap);
    va_end(ap);
    return n;
}

/* there is no heap: core/gfx/qd_arena.c hands to free() and realloc() only what is not the arena's, which on the unit
 * is nothing (its malloc fallback is the host's alone); a pointer that came here would be a bug: kept quiet, refused */
void free(void *p) { (void)p; }
void *realloc(void *p, size_t n) { (void)p, (void)n; return 0; }

/* plutovg's assert(): a failed one stops the unit where it failed (fm1-base's crash screen and record: its PC) */
void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    (void)file, (void)line, (void)func, (void)expr;
    __builtin_trap();
}

double atof(const char *s)
{
    double v = 0.0, scale = 1.0;
    int neg = 0;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-' || *s == '+')
        neg = *s++ == '-';
    for (; *s >= '0' && *s <= '9'; s++)
        v = v * 10.0 + (double)(*s - '0');
    if (*s == '.')
        for (s++; *s >= '0' && *s <= '9'; s++) {
            scale *= 10.0;
            v += (double)(*s - '0') / scale;
        }
    return neg ? -v : v;
}
#endif
