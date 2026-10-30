// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 RemoteFM1 contributors
// Derived from RemoteFM1IOS af91bc5 RFMKit/Sources/RFMCore/RFMMidiMap.h, adapted: the map's numbers come from FIZZ's
// FizzMidiMap.h (not RemoteFM1's engine header); the mapper is reached through fzmm_* (FizzMidiMap.cpp).
// The learned MIDI map on the core thread (M8): the website's js/midi-map.js MidiMap.feed, in C++. A message from a
// mapped source becomes at most one action (an encoder turned, a button pressed or let go, MASTER set, or MASTER
// waiting for its pot); the engine applies it as a panel event. Fixed-size tables: nothing here allocates or locks.
#pragma once
#include <climits>
#include <cstdint>

#include "FizzMidiMap.h"

namespace rfm {

constexpr uint32_t kMapMax = 512;

// the map as rfme_set_midi_map gives it (the control thread -> the core thread, in a Triple)
struct MapTable {
    uint32_t n = 0;
    double speed = 1;
    rfme_map_entry_t e[kMapMax];
};

class MidiMapper {
public:
    struct Act {
        enum Kind : uint8_t { None, Turn, Button, Master, Pickup };
        Kind kind = None;
        int16_t index = 0;          // Turn: the knob role; Button: the label
        int32_t value = 0;          // Turn: detents; Button: 1 down / 0 up; Master and Pickup: the pot, 0..1023
        bool first = false;         // Pickup: the first of this wait
    };

    MidiMapper();
    // a new map. A source that keeps its target keeps its state (a new mode: its fraction goes); a new or moved one
    // starts again (an absolute pot from the value learn last saw). Nothing is held any more: *released gets the
    // labels the old map held (bit RFME_B_*), for the caller to let go
    void adopt(const MapTable &t, uint32_t *released);
    // one message (1..3 bytes); false: no mapping takes it (it flows on). true: consumed, out the action (maybe None).
    // master: MASTER now, 0..1023 (-1 unknown: a MASTER pot sets it at once)
    bool feed(const uint8_t *d, uint32_t n, int32_t master, Act &out);
    // learn saw this CC's value (the pot turns from there once it is mapped)
    void seed(uint8_t ch, uint8_t num, uint8_t value);
    // nothing held (a boot): the labels the map held, now let go by the caller's reset
    void release();

    static int32_t relDelta(uint8_t mode, int32_t v);

private:
    struct Src {
        int16_t last = -1;          // -1: none yet
        double acc = 0;
        uint8_t tgt = 0xFF, mode = 0, nt = 0xFF, nmode = 0;
        bool down = false, picked = false, hinted = false, seeded = false;
    };
    static constexpr uint32_t kSources = 2 * 16 * 128;   // (cc | note) x channel x number
    static uint32_t index(uint8_t src, uint8_t ch, uint8_t num) { return (uint32_t)src * 2048u + ch * 128u + num; }
    bool pickedUp(Src &s, int32_t v, double at, double tol);

    Src s_[kSources];
    uint16_t list_[kSources];       // the mapped sources
    uint32_t n_ = 0;
    double speed_ = 1;
    uint32_t btnOut_ = 0;           // the labels the map holds down
    int32_t masterSet_ = INT32_MIN; // the MASTER value a pot set last (INT32_MIN: none)
};

} // namespace rfm
