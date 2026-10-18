/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 ChoralRoot FM-1 contributors (a fork of Felucca) */
/* QUADRILLE: copied from ChoralRoot v0.25.1 (15ca8d0), firmware/src/master.c, GPL-3.0-only: the pot's law alone
 * (master_of_pot, verbatim). Left out: master_poll and master_boot, which read the unit's ADC (QUADRILLE gets the pot as
 * the panel's 0..1023 and calls fx_master_now with this law, qd_fx.c). Built only through qd_fx.c. */
static inline uint32_t master_of_pot(uint32_t k10) { return (k10 * k10) >> 8; }   /* 0 .. ~4096 */
