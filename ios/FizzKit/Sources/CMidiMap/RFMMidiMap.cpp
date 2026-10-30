// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 RemoteFM1 contributors
// Derived from RemoteFM1IOS af91bc5 RFMKit/Sources/RFMCore/RFMMidiMap.cpp, adapted: RemoteMidi.c's FM-1 rule
// (rfme_fm1_candidate, RemoteFM1's GPL engine) left out; MidiMapper unchanged.
// The learned MIDI map (RFMMidiMap.h): js/midi-map.js MidiMap.feed, pickedUp and relDelta as the website has them.
#include "RFMMidiMap.h"

#include <cmath>

namespace rfm {

MidiMapper::MidiMapper() = default;

int32_t MidiMapper::relDelta(uint8_t mode, int32_t v)
{
    if (mode == RFME_MAP_REL64)
        return v - 64;
    if (mode == RFME_MAP_REL2C)
        return v < 64 ? v : v - 128;
    if (mode == RFME_MAP_RELSIGN)
        return v & 64 ? -(v & 63) : v & 63;
    return 0;
}

void MidiMapper::adopt(const MapTable &t, uint32_t *released)
{
    speed_ = t.speed == 0.5 || t.speed == 2 ? t.speed : 1;
    for (uint32_t i = 0; i < kSources; i++)
        s_[i].nt = 0xFF;
    const uint32_t n = t.n < kMapMax ? t.n : kMapMax;
    for (uint32_t i = 0; i < n; i++) {
        const rfme_map_entry_t &e = t.e[i];
        if (e.src > RFME_SRC_NOTE || e.channel > 15 || e.number > 127 || e.target > RFME_T_BUTTON + RFME_B_OCTUP)
            continue;
        if (e.src == RFME_SRC_NOTE && e.target < RFME_T_BUTTON)
            continue;                                   // (a note on a knob or MASTER: never mapped)
        Src &s = s_[index(e.src, e.channel, e.number)];
        s.nt = e.target;                                // (a source twice: the later wins)
        s.nmode = e.target < RFME_T_MASTER && e.mode <= RFME_MAP_RELSIGN ? e.mode : 0;
    }
    n_ = 0;
    for (uint32_t i = 0; i < kSources; i++) {
        Src &s = s_[i];
        s.down = false;
        if (s.nt == 0xFF) {
            s.tgt = 0xFF;
        } else if (s.tgt == s.nt) {
            if (s.mode != s.nmode) {                    // (midi-map.js setMode: the fraction goes)
                s.mode = s.nmode;
                s.acc = 0;
            }
        } else {                                        // (bind: a fresh state, seeded by learn)
            if (!s.seeded)
                s.last = -1;
            s.acc = 0;
            s.picked = s.hinted = false;
            s.tgt = s.nt;
            s.mode = s.nmode;
        }
        s.seeded = false;
        if (s.tgt != 0xFF)
            list_[n_++] = (uint16_t)i;
    }
    if (released)
        *released = btnOut_;
    btnOut_ = 0;
}

void MidiMapper::seed(uint8_t ch, uint8_t num, uint8_t value)
{
    Src &s = s_[index(RFME_SRC_CC, ch & 15, num & 127)];
    s.last = value & 127;
    s.seeded = true;
}

void MidiMapper::release()
{
    for (uint32_t i = 0; i < n_; i++)
        s_[list_[i]].down = false;
    btnOut_ = 0;
}

bool MidiMapper::pickedUp(Src &s, int32_t v, double at, double tol)
{
    const int32_t last = s.last;
    s.last = (int16_t)v;
    if (!s.picked)
        s.picked = std::fabs(v - at) <= tol || (last >= 0 && (last - at) * (v - at) <= 0);
    return s.picked;
}

bool MidiMapper::feed(const uint8_t *d, uint32_t n, int32_t master, Act &out)
{
    out = Act{};
    if (n < 3)
        return false;
    const uint8_t st = d[0] & 0xF0, ch = d[0] & 15, num = d[1] & 127, value = d[2] & 127;
    uint8_t src;
    bool on = false;
    if (st == 0xB0) {
        src = RFME_SRC_CC;
    } else if (st == 0x90 || st == 0x80) {
        src = RFME_SRC_NOTE;
        on = st == 0x90 && value > 0;                   // (a note-on at velocity 0 is a note-off)
    } else {
        return false;
    }
    Src &s = s_[index(src, ch, num)];
    if (s.tgt == 0xFF)
        return false;
    if (s.tgt >= RFME_T_BUTTON) {                       // a button: held while any of its sources is down
        s.down = src == RFME_SRC_NOTE ? on : value >= 64;
        bool any = false;
        for (uint32_t i = 0; i < n_ && !any; i++)
            any = s_[list_[i]].tgt == s.tgt && s_[list_[i]].down;
        const int label = s.tgt - RFME_T_BUTTON;
        if (any != (((btnOut_ >> label) & 1u) != 0)) {
            btnOut_ = any ? btnOut_ | 1u << label : btnOut_ & ~(1u << label);
            out.kind = Act::Button;
            out.index = (int16_t)label;
            out.value = any ? 1 : 0;
        }
        return true;
    }
    if (src != RFME_SRC_CC)
        return true;
    if (s.tgt == RFME_T_MASTER) {                       // 0..127 -> 0..1023, with pickup
        const int32_t want = (int32_t)std::lround(value * 1023.0 / 127.0);
        if (master < 0) {                               // (MASTER unknown: set at once)
            s.last = value;
            out.kind = Act::Master;
            out.value = want;
            return true;
        }
        if (master != masterSet_) {                     // moved by something else: every MASTER pot picks up again
            masterSet_ = master;
            for (uint32_t i = 0; i < n_; i++) {
                Src &x = s_[list_[i]];
                if (x.tgt == RFME_T_MASTER)
                    x.picked = x.hinted = false;
            }
        }
        if (!pickedUp(s, value, master * 127.0 / 1023.0, 2)) {
            out.kind = Act::Pickup;
            out.value = want;
            out.first = !s.hinted;
            s.hinted = true;
            return true;
        }
        masterSet_ = want;
        out.kind = Act::Master;
        out.value = want;
        return true;
    }
    int32_t delta;                                      // an encoder
    if (s.mode == RFME_MAP_ABS) {
        delta = s.last < 0 ? 0 : value - s.last;        // (its first value only sets the start)
        s.last = value;
    } else {
        delta = relDelta(s.mode, value);
    }
    s.acc += delta * speed_;
    const double steps = std::trunc(s.acc + (s.acc > 0 ? 1e-9 : -1e-9));
    s.acc -= steps;
    if (steps != 0) {
        out.kind = Act::Turn;
        out.index = s.tgt;
        out.value = (int32_t)steps;
    }
    return true;
}

} // namespace rfm
