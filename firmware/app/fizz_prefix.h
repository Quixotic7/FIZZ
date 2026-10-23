/* SPDX-License-Identifier: MPL-2.0 */
/* Included first in every unit of FIZZ's (fm1.toml o2_include): the core's device profile (core/qd_config.h) in the
 * firmware, the host simulator and the browser emulator alike, and on the unit its data blob's sections (QD_XDATA:
 * the fonts and the TAP ape's lines in the data flash). plutovg as FIZZ's tools build it. */
#pragma once
#include "fz_gfx.h"                          /* (tools/gen_units.py --gfx: QD_GFX_FAST or not) */
#define QD_DEVICE 1
#ifndef PLAT_HOST
#define QD_TARGET_FM1 1
#ifndef __STDC_NO_ATOMICS__
#define __STDC_NO_ATOMICS__ 1                /* (JieLi's clang 4: plutovg's reference counts without <stdatomic.h>) */
#endif
#endif
#define PLUTOVG_BUILD 1
#define PLUTOVG_BUILD_STATIC 1
#define PLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD 1
