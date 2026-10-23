/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P2.1: the tests' and tools' look at a pool sound or one of MY presets (persist.h qd_lib_read, sounds.h
 * qd_sound_read: at the device's sizes the sounds are in the store). Each call returns a copy from a ring of eight, so
 * one expression may look at several; to change one: read it, change the copy, write it (qd_lib_write,
 * qd_sound_write). */
#pragma once
#include "app.h"

static inline const qd_sound_t *LIB(const qd_app_t *a, int i)
{
    static qd_sound_t ring[8];
    static unsigned n;
    qd_sound_t *s = &ring[n++ & 7u];
    qd_lib_read(a, i, s);
    return s;
}
static inline const qd_sound_t *POOL(const qd_project_t *pr, int k)
{
    static qd_sound_t ring[8];
    static unsigned n;
    qd_sound_t *s = &ring[n++ & 7u];
    qd_sound_read(pr, k, s);
    return s;
}
