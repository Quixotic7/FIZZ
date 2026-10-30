// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// POOL's SELECT (the owner, 2026-10-29: "no way to then select multiple items in the pool"): a long-press on a sound
// starts it with that place ticked; then a tap ticks or unticks a sound (an empty place takes no tick). The bulk bar
// takes the bottom panel's place (PoolSelectViews.swift): the count, CLEAR n (asked once, saying how many steps use
// them: they go back to their track's sound, as pool.js's CLEAR), MOVE (then a place tapped: the sounds side by side
// from there, pool.js's SWAP for each, every pattern's locks following) and → KEPT n. DONE, a project loaded, or
// another tab ends it.

import Foundation

/// The places ticked (PoolView's state; a value, so the tests drive it as the view does).
public struct PoolPicks: Equatable, Sendable {
    /// SELECT is on (a long-press started it; DONE ends it)
    public private(set) var on = false
    public private(set) var places: Set<Int> = []
    /// MOVE: the next place tapped is where the block goes
    public var moving = false

    public init() {}

    /// A long-press on a place holding a sound: SELECT on with it ticked (on already: it toggles, as a tap).
    /// An empty place: nothing.
    public mutating func hold(_ k: Int, full: Bool) {
        guard full else { return }
        if on { tap(k, full: full); return }
        on = true
        moving = false
        places = [k]
    }

    /// A tap while SELECT is on: a sound ticked or unticked; an empty place: nothing.
    public mutating func tap(_ k: Int, full: Bool) {
        guard on, full else { return }
        if places.contains(k) { places.remove(k) } else { places.insert(k) }
    }

    /// DONE (a project loaded, another tab): off, nothing ticked.
    public mutating func clear() { self = PoolPicks() }

    /// The places that hold a sound no more untick (a CLEAR, the lists read again).
    public mutating func keep(_ full: Set<Int>) { places.formIntersection(full) }

    /// After a MOVE: the block's new places ticked.
    public mutating func moved(to r: ClosedRange<Int>) {
        places = Set(r)
        moving = false
    }

    /// (tests, fizzshots) these places ticked
    public mutating func set(_ ks: Set<Int>) {
        on = true
        places = ks
    }

    public var sorted: [Int] { places.sorted() }
    public var count: Int { places.count }
}
