// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzMidiMap.h's handle on rfm::MidiMapper (RFMMidiMap.cpp), for Swift. Written for FIZZ.
#include "FizzMidiMap.h"

#include <new>

#include "RFMMidiMap.h"

struct fzmm {
    rfm::MidiMapper mapper;
    rfm::MapTable table;
};

extern "C" fzmm_t *fzmm_new(void) { return new (std::nothrow) fzmm(); }

extern "C" void fzmm_free(fzmm_t *m) { delete m; }

extern "C" uint32_t fzmm_adopt(fzmm_t *m, const rfme_map_entry_t *map, uint32_t n, double speed)
{
    if (!m)
        return 0;
    m->table.n = n < rfm::kMapMax ? n : rfm::kMapMax;
    for (uint32_t i = 0; i < m->table.n; i++)
        m->table.e[i] = map[i];
    m->table.speed = speed;
    uint32_t released = 0;
    m->mapper.adopt(m->table, &released);
    return released;
}

extern "C" int fzmm_feed(fzmm_t *m, const uint8_t *d, uint32_t n, int32_t master, fzmm_act_t *out)
{
    if (!m || !d)
        return 0;
    rfm::MidiMapper::Act a;
    const bool took = m->mapper.feed(d, n, master, a);
    if (out) {
        out->kind = a.kind == rfm::MidiMapper::Act::Turn ? FZMM_TURN
                  : a.kind == rfm::MidiMapper::Act::Button ? FZMM_BUTTON
                  : a.kind == rfm::MidiMapper::Act::Master ? FZMM_MASTER
                  : a.kind == rfm::MidiMapper::Act::Pickup ? FZMM_PICKUP : FZMM_NONE;
        out->first = a.first ? 1 : 0;
        out->index = a.index;
        out->value = a.value;
    }
    return took ? 1 : 0;
}

extern "C" void fzmm_seed(fzmm_t *m, uint8_t ch, uint8_t num, uint8_t value)
{
    if (m)
        m->mapper.seed(ch, num, value);
}

extern "C" void fzmm_release(fzmm_t *m)
{
    if (m)
        m->mapper.release();
}
