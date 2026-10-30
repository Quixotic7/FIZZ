// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A pattern dragged (mock-ups 43, 48; the web Manager's projtab.js "drag a pattern": lift / land / dropApply, and its
// TO sheet). A long press lifts a pattern (iPad: a drag lifts it at once); dropped on a pattern it is copied there
// (one there: asked first, COPY OVER … REPLACE), on another project's chip its first free pattern; ⌥ held (or MOVE in
// the TO sheet): moved, its own place cleared. A chip held under it a moment springs that project open (the phone: a
// card of its 64 patterns, the iPad: beside). Let go over nothing (a long press): the TO… sheet. Each drop is one change
// waiting in the yellow bar like a paste: projects.js pastePatterns run by entry.js pjDrop, so a copy into another project
// carries its pool sounds into that pool, reusing the ones it has; a pool with no room refuses it ("poolfull").

import CoreGraphics
import Foundation
import FizzData
import GameController
import SwiftUI

/// The modifier keys held on a hardware keyboard (iPad: SHIFT-tap a range, ⌘-tap one more, ⌥ while dragging: MOVE).
/// (SwiftUI's onModifierKeysChanged is macOS-only; GameController's keyboard is read instead.)
@MainActor enum HeldKeys {
    static var now: EventModifiers {
        guard let k = GCKeyboard.coalesced?.keyboardInput else { return [] }
        let on = { (c: GCKeyCode) in k.button(forKeyCode: c)?.isPressed == true }
        var m: EventModifiers = []
        if on(.leftShift) || on(.rightShift) { m.insert(.shift) }
        if on(.leftGUI) || on(.rightGUI) { m.insert(.command) }
        if on(.leftAlt) || on(.rightAlt) { m.insert(.option) }
        return m
    }
}

/// Where a pattern lands: slot q, its pattern j (nil: its first free one).
public struct ProjectsDropAt: Sendable, Equatable, Hashable {
    public var q: Int
    public var j: Int?
    public init(q: Int, j: Int?) { self.q = q; self.j = j }
}

/// A pattern lifted (projtab.js D): where from, where the finger is, what it is over.
public struct ProjectsLift: Sendable, Equatable {
    public var p: Int, k: Int
    /// the finger, in the PROJECTS screen's space (ProjectsDropZones.space)
    public var at: CGPoint
    /// the tile's size and where the finger took it (from its top left)
    public var size: CGSize, grab: CGPoint
    /// ⌥ held: moved
    public var move = false
    /// lifted by a long press (let go over nothing: the TO… sheet)
    public var touch = true
    public var over: ProjectsDropAt?
    /// over a chip (not the card or a tile)
    public var overChip = false
    /// the project sprung open (the phone's card)
    public var spring: Int?
}

/// The TO… sheet: COPY or MOVE slot p's pattern k to project q.
public struct ProjectsTo: Sendable, Equatable {
    public var p: Int, k: Int
    public var q: Int
    public var move = false
}

/// How a project held under a drag opens: the phone's card, beside (iPad landscape), or selected (iPad portrait).
enum ProjectsSpring { case card, beside, open }

/// The drop targets' places on screen, reported by the views (not observed: a drag moves too often).
@MainActor final class ProjectsDropZones {
    nonisolated static let space = "projects-drag"
    struct Grid {
        var q: Int; var rect: CGRect; var hit: (CGPoint) -> Int?
        /// a pattern's tile in the grid (nil: not known)
        var tile: ((Int) -> CGRect)? = nil
    }
    /// the grids by their place ("main", "beside"), the slot chips / pills by slot, the sprung card
    var grids: [String: Grid] = [:]
    var chips: [Int: CGRect] = [:]
    var card: Grid?
    var spring: ProjectsSpring = .card

    /// What is under the finger: the card (a dot of it, or its first free), a chip, a tile (not the lifted one).
    func target(_ at: CGPoint, lift: ProjectsLift, there: (Int) -> Bool) -> (ProjectsDropAt, chip: Bool)? {
        if let c = card, lift.spring == c.q, c.rect.insetBy(dx: -6, dy: -6).contains(at) {
            return (ProjectsDropAt(q: c.q, j: c.hit(CGPoint(x: at.x - c.rect.minX, y: at.y - c.rect.minY))), false)
        }
        for (q, r) in chips where r.insetBy(dx: -2, dy: -4).contains(at) && there(q) { return (ProjectsDropAt(q: q, j: nil), true) }
        for g in grids.values where g.rect.contains(at) {
            guard let k = g.hit(CGPoint(x: at.x - g.rect.minX, y: at.y - g.rect.minY)) else { return nil }
            if g.q == lift.p && k == lift.k { return nil }
            return (ProjectsDropAt(q: g.q, j: k), false)
        }
        return nil
    }
}

extension ProjectsModel {
    // MARK: what can go where

    /// A pattern there to carry (projtab.js: a record: not empty, not one of the demos' own), nothing else going on.
    public func liftable(_ p: Int, _ k: Int) -> Bool {
        guard writing == nil, arm == nil, !reading, let t = view?.project(p)?.pats[safe: k], let t else { return false }
        return !t.demo
    }

    /// Slot q's first free pattern.
    public func firstFree(_ q: Int) -> Int? { view?.project(q)?.pats.firstIndex { $0 == nil } }

    func name(_ q: Int) -> String {
        let s = slot(q)
        return s.name.isEmpty ? "PROJECT \(q + 1)" : s.name
    }

    /// iPad: the project beside the selected one (the one chosen, else the next one there).
    public var besideSlot: Int? {
        let n = nproj
        if let b = beside, b != sel, b < n, slot(b).there { return b }
        return (1..<max(1, n)).map { (sel + $0) % n }.first { slot($0).there }
    }

    /// iPad's pills: a tap opens that project; the one open before goes beside (the one beside tapped: they swap).
    public func tapPill(_ p: Int) async {
        if arm != nil { await tapSlot(p); return }
        if p == sel { return }
        let was = sel
        select(p)
        beside = was
    }

    /// iPad: the project beside made the selected one (a tile of it tapped), the selected one going beside.
    public func swapBeside() {
        guard let b = besideSlot else { return }
        let was = sel
        select(b)
        beside = was
    }

    // MARK: the drag

    /// A pattern lifted under the finger (at: the finger in the screen's space; grab: where on the tile).
    func lift(p: Int, k: Int, at: CGPoint, size: CGSize, grab: CGPoint, touch: Bool, move: Bool = false) {
        guard liftable(p, k), lift == nil else { return }
        lift = ProjectsLift(p: p, k: k, at: at, size: size, grab: grab, move: move, touch: touch)
        plan = nil
        Task { await fetchPlan(p: p, k: k) }
        // ⌥ pressed or let go while it is held (a keyboard there): COPY <-> MOVE (on a change only)
        if GCKeyboard.coalesced != nil {
            springKeys?.cancel()
            springKeys = Task { [weak self] in
                var alt = HeldKeys.now.contains(.option)
                while !Task.isCancelled, let self, self.lift != nil {
                    let a = HeldKeys.now.contains(.option)
                    if a != alt { alt = a; self.setDragMove(a) }
                    try? await Task.sleep(for: .milliseconds(60))
                }
            }
        }
    }

    func fetchPlan(p: Int, k: Int) async {
        guard let h = session, let pl = try? await FizzDataShared.engine().projectsDropPlan(h, p: p, k: k) else { return }
        if lift.map({ $0.p == p && $0.k == k }) ?? false || to.map({ $0.p == p && $0.k == k }) ?? false { plan = pl }
    }

    /// The finger moved: what it is over; a chip held a moment springs its project open.
    func drag(to at: CGPoint) {
        guard var l = lift else { return }
        l.at = at
        let t = zones.target(at, lift: l) { [weak self] in self?.slot($0).there ?? false }
        let over = t?.0, chip = t?.chip ?? false
        if over != l.over || chip != l.overChip {
            l.over = over
            l.overChip = chip
            springTask?.cancel()
            springTask = nil
            if let o = over, chip, !showing(o.q) {
                let q = o.q
                springTask = Task { [weak self] in
                    try? await Task.sleep(for: .milliseconds(700))
                    guard !Task.isCancelled, let self, let l = self.lift, l.over?.q == q, l.overChip else { return }
                    self.spring(q)
                }
            }
            // (the card closes once the finger is on neither it nor its chip)
            if l.spring != nil, over?.q != l.spring { l.spring = nil }
        }
        lift = l
    }

    /// Slot q on screen already as a drop target (its grid, its card).
    private func showing(_ q: Int) -> Bool {
        switch zones.spring {
        case .card: return lift?.spring == q
        case .beside: return q == sel || (!padKept && q == besideSlot)
        case .open: return q == sel
        }
    }

    private func spring(_ q: Int) {
        switch zones.spring {
        case .card: lift?.spring = q
        case .beside: padKept = false; beside = q
        case .open: select(q)
        }
    }

    /// ⌥ pressed or let go while dragging.
    public func setDragMove(_ move: Bool) {
        if lift != nil, lift?.move != move { lift?.move = move }
    }

    /// Let go: dropped where it is over (land), else (a long press) the TO… sheet.
    func release() async {
        guard let l = lift else { return }
        endLift()
        if let o = l.over { await land(p: l.p, k: l.k, q: o.q, j: o.j, move: l.move) } else if l.touch { await openTo(p: l.p, k: l.k) }
    }

    /// A drag at a moment (fizzshots, the simulator's scenes: no finger): slot p's pattern k lifted over slot q's chip
    /// (sprung open) or its pattern j, where the screen last reported them.
    public func pose(p: Int, k: Int, q: Int, j: Int? = nil, spring: Bool = false, move: Bool = false) async {
        endLift()
        guard liftable(p, k) else { return }
        let grid = zones.grids.values.first { $0.q == p }, src = grid?.tile?(k)
        let size = src?.size ?? CGSize(width: 40, height: 40)
        var at = CGPoint(x: 200, y: 300)
        if j == nil, let c = zones.chips[q] { at = CGPoint(x: c.midX, y: c.midY) }
        if let j, let g = zones.grids.values.first(where: { $0.q == q }), let r = g.tile?(j) {
            at = CGPoint(x: g.rect.minX + r.midX, y: g.rect.minY + r.midY)
        }
        lift = ProjectsLift(p: p, k: k, at: at, size: size, grab: CGPoint(x: size.width / 2, y: size.height / 2), move: move,
                            touch: true, over: ProjectsDropAt(q: q, j: j), overChip: j == nil, spring: spring ? q : nil)
        await fetchPlan(p: p, k: k)
    }

    public func endLift() {
        springTask?.cancel()
        springTask = nil
        springKeys?.cancel()
        springKeys = nil
        lift = nil
    }

    // MARK: landing (projtab.js land)

    /// Slot p's pattern k to slot q's pattern j (nil: its first free one); move: its own place cleared. One there:
    /// asked first. A full pool, a store with no room: refused, said.
    public func land(p: Int, k: Int, q: Int, j: Int?, move: Bool) async {
        guard let v = view, writing == nil else { return }
        guard let pr = v.project(q), pr.there else { say("SLOT \(q + 1) EMPTY", FZ.orange); return }
        guard let j = j ?? firstFree(q) else { say("NO FREE PATTERN", FZ.orange); return }
        if p == q && k == j { return }
        if move, v.project(p)?.demo == true, k < 3 { say("THE DEMOS' OWN", FZ.champagne); return }
        guard liftable(p, k) else { say("NOTHING TO COPY", FZ.orange); return }
        let src = p == q ? FizzClip.patName(k) : "\(name(p)) \(FizzClip.patName(k))", dst = "\(name(q)) \(FizzClip.patName(j))"
        let go: @MainActor () async -> Void = { [weak self] in
            guard let self, let h = self.session else { return }
            do {
                switch try await FizzDataShared.engine().projectsDrop(h, p: p, k: k, q: q, j: j, move: move) {
                case .success(let nv):
                    self.view = nv
                    if q == self.sel { self.pats = [j]; self.anchor = j } else if move, p == self.sel { self.pats.remove(k) }
                    self.say("\(FizzClip.patName(k)) → \(p == q ? "" : "\(self.name(q)) ")\(FizzClip.patName(j))", move ? FZ.orange : FZ.cyan)
                case .failure(let r): self.refuse(r)
                }
            } catch { self.say("NOT DONE", FZ.red) }
        }
        if pr.pats[safe: j] ?? nil != nil {
            ask = ProjectsAsk(title: move ? "MOVE OVER" : "COPY OVER", what: "\(src) → \(dst)", go: "REPLACE", tint: FZ.yellow, run: go)
        } else {
            await go()
        }
    }

    // MARK: the TO… sheet

    /// TO… for slot p's pattern k: COPY, into its own project first (projtab.js sheetOpen).
    public func openTo(p: Int, k: Int) async {
        guard liftable(p, k) else { return }
        to = ProjectsTo(p: p, k: k, q: p)
        plan = nil
        await fetchPlan(p: p, k: k)
    }

    /// TO… from the selection's buttons (one pattern selected).
    public var canTo: Bool { pats.count == 1 && pats.first.map { liftable(sel, $0) } == true }

    public func openToSelection() async {
        guard canTo, let k = pats.first else { return }
        await openTo(p: sel, k: k)
    }

    public func setToMove(_ move: Bool) { to?.move = move }

    /// A project picked in the sheet (a full pool's refuses, as its drop would).
    public func setToProject(_ q: Int) {
        guard to != nil, slot(q).there else { return }
        if plan?.target(q)?.full == true { say("POOL FULL", FZ.orange); return }
        to?.q = q
    }

    /// A pattern picked (nil: FIRST FREE): the sheet closes and it lands (asked first over one there).
    public func toPick(_ j: Int?) async {
        guard let t = to else { return }
        to = nil
        await land(p: t.p, k: t.k, q: t.q, j: j, move: t.move)
    }

    public func closeTo() { to = nil }
}

fileprivate extension Array {
    subscript(safe i: Int) -> Element? { indices.contains(i) ? self[i] : nil }
}
