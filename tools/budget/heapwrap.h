/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P2.0 (tools/budget, from the Phase 2 audit): force-included into the core and plutovg for the host bench: every
 * malloc / calloc / realloc / free counted by its call site */
#pragma once
#include <stdlib.h>
#include <string.h>
void *hw_malloc(size_t n, const char *f, int l);
void *hw_calloc(size_t a, size_t b, const char *f, int l);
void *hw_realloc(void *p, size_t n, const char *f, int l);
void hw_free(void *p);
#define malloc(n) hw_malloc((n), __FILE__, __LINE__)
#define calloc(a, b) hw_calloc((a), (b), __FILE__, __LINE__)
#define realloc(p, n) hw_realloc((p), (n), __FILE__, __LINE__)
#define free(p) hw_free(p)
