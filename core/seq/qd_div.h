/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P2.4: a 64-bit value divided by a 32-bit one, exactly, with 32-bit divisions only. The FM-1's core divides 32 bits
 * in hardware; C's 64-bit '/' and '%' become a library routine (__divdi3 / __aeabi_ldivmod: a bit at a time) that the
 * audio path cannot afford. Long division in base 2^16 (a divisor under 2^16: four divisions) or 2^8 (under 2^24:
 * eight), else bit by bit. The quotient and remainder are C's: truncated toward zero, the remainder with the
 * dividend's sign. Header only (static inline). */
#pragma once
#include <stdint.h>

/* (the long parts: one copy a unit that uses them, not one a call) */
#if defined(__GNUC__)
#define QD_DIV_FN static __attribute__((unused, noinline))
#else
#define QD_DIV_FN static
#endif
/* x / d and x % d (d >= 1) */
QD_DIV_FN uint64_t qd_udivmod64_long(uint64_t x, uint32_t d, uint32_t *rem)
{
    uint64_t q = 0;
    uint32_t r = 0;
    int i;
    if (d < 0x10000u) {
        for (i = 48; i >= 0; i -= 16) {                 /* (r < d < 2^16: r << 16 | a digit fits 32 bits) */
            const uint32_t n = (r << 16) | (uint32_t)((x >> i) & 0xFFFFu);
            q = (q << 16) | (n / d);
            r = n % d;
        }
    } else if (d < 0x1000000u) {
        for (i = 56; i >= 0; i -= 8) {
            const uint32_t n = (r << 8) | (uint32_t)((x >> i) & 0xFFu);
            q = (q << 8) | (n / d);
            r = n % d;
        }
    } else {
        uint64_t rr = 0;
        for (i = 63; i >= 0; i--) {
            rr = (rr << 1) | ((x >> i) & 1u);
            q <<= 1;
            if (rr >= d) {
                rr -= d;
                q |= 1u;
            }
        }
        r = (uint32_t)rr;
    }
    if (rem)
        *rem = r;
    return q;
}


/* x / d and x % d (d >= 1): a 32-bit x at once, else the long division */
QD_DIV_FN uint64_t qd_udivmod64(uint64_t x, uint32_t d, uint32_t *rem)
{
    if (!(x >> 32)) {
        const uint32_t lo = (uint32_t)x;
        if (rem)
            *rem = lo % d;
        return lo / d;
    }
    return qd_udivmod64_long(x, d, rem);
}
static inline uint64_t qd_udiv64(uint64_t x, uint32_t d) { return qd_udivmod64(x, d, 0); }

/* signed x / d and x % d as C has them (truncated toward zero), d > 0 */
QD_DIV_FN int64_t qd_divmod64(int64_t x, int32_t d, int32_t *rem)
{
    uint32_t r;
    if (x >= 0) {
        const int64_t q = (int64_t)qd_udivmod64((uint64_t)x, (uint32_t)d, &r);
        if (rem)
            *rem = (int32_t)r;
        return q;
    } else {
        const int64_t q = -(int64_t)qd_udivmod64((uint64_t)0 - (uint64_t)x, (uint32_t)d, &r);
        if (rem)
            *rem = -(int32_t)r;
        return q;
    }
}
static inline int64_t qd_div64(int64_t x, int32_t d) { return qd_divmod64(x, d, 0); }
static inline int32_t qd_mod64(int64_t x, int32_t d)
{
    int32_t r;
    qd_divmod64(x, d, &r);
    return r;
}

/* num / den as the nearest float (round half to even: what (float)((double)num / den) gives but for the rare double
 * rounding), den > 0 and < 2^32; with 32-bit divisions and shifts only (P2.4: no double on the unit) */
QD_DIV_FN float qd_ratio_f(int64_t num, int64_t den)
{
    const int neg = num < 0;
    const uint64_t n = neg ? (uint64_t)0 - (uint64_t)num : (uint64_t)num, d = (uint64_t)den;
    uint32_t rem;
    uint64_t m, r;
    int e = 0, drop = 0, sticky;
    union {
        uint32_t u;
        float f;
    } p;
    float f;
    if (!n || !d)
        return 0.f;
    m = qd_udivmod64(n, (uint32_t)d, &rem);             /* the whole part, then the fraction's bits until 26 */
    r = rem;
    while (m < ((uint64_t)1 << 26)) {
        r <<= 1, m <<= 1, e++;
        if (r >= d)
            r -= d, m |= 1u;
    }
    sticky = r != 0;
    while ((m >> drop) >= ((uint64_t)1 << 24))
        drop++;
    if (drop) {                                         /* to 24 bits, rounded half to even (the bits below: sticky) */
        const uint64_t low = m & (((uint64_t)1 << drop) - 1u), half = (uint64_t)1 << (drop - 1);
        m >>= drop;
        e -= drop;
        if (low > half || (low == half && (sticky || (m & 1u))))
            m++;
    }
    f = (float)(uint32_t)m;                             /* (exact: m <= 2^24) */
    while (e > 100)                                     /* (x 2^-e: an exact power of two, in steps the exponent holds) */
        f *= 7.88860905e-31f, e -= 100;                 /* (2^-100) */
    while (e < -100)
        f *= 1.2676506e30f, e += 100;                   /* (2^100) */
    p.u = (uint32_t)(127 - e) << 23;
    f *= p.f;
    return neg ? -f : f;
}
