// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// KEPT and the unit: → KEPT copies a unit preset onto the phone (its web tags with it); SEND n TO <unit> stores kept
// presets on the unit (actions.send: MY PRESETS, the pool, or the first onto a track), their web tags joining the
// unit's sounds'. The unit's name as KEPT's buttons say it: FM-1, or THIS iPHONE / THIS iPAD for the emulated FIZZ.

import Foundation
import FizzData
import FizzKit

extension UnitModel {
    /// The connected unit is the emulated FIZZ, not an FM-1 (the unit says so; its port name for the demo and tests).
    public var unitIsEmulated: Bool {
        guard let u = unit else { return false }
        return u.isEmulated || u.portName.range(of: "this iP(hone|ad)", options: [.regularExpression, .caseInsensitive]) != nil
    }

    /// No FM-1 to talk to: the emulated FIZZ or nothing (KEPT opens first then).
    public var noFM1: Bool { unit == nil || unitIsEmulated }

    /// "FM-1", "THIS iPHONE", "THIS iPAD".
    public var unitName: String {
        guard unitIsEmulated, let p = unit?.portName else { return "FM-1" }
        return p.range(of: "ipad", options: .caseInsensitive) != nil ? "THIS iPAD" : "THIS iPHONE"
    }

    /// → KEPT: a unit preset (MY PRESETS, FACTORY; a pool place's preset) onto the phone, with its web tags; the same
    /// sound kept already: that one.
    public func keep(_ r: FizzListEntry) async -> KeepOutcome {
        guard let s = await actions.exportSounds([r]).first else { return .failed }
        let key = try? await FizzDataShared.engine().soundKey(s)
        return await keep(sound: s, webTags: webTags.of(key))
    }

    public func keep(sound s: FizzSound, webTags tags: [String] = []) async -> KeepOutcome {
        if let have = kept.same(as: s) { return .already(have) }
        guard let k = try? await kept.add(s, webTags: tags) else { return .failed }
        say("kept \(s.name) (\(k.id))")
        return .kept(k)
    }

    /// SEND n TO <unit>: the kept presets stored on the unit in order (FULL stops the rest); one outcome each
    /// (.placed: MY PRESETS' slot, or the pool's place). Their web tags join the unit's sounds'.
    public func send(kept ks: [KeptPreset], to dest: SendDestination) async -> [UnitOutcome] {
        var files: [FizzKeptSound] = []
        for k in ks { files.append(await kept.full(k) ?? FizzKeptSound(sound: k.sound, webTags: k.webTags)) }
        let out = await actions.send(files.map(\.sound), to: dest)
        guard let e = try? FizzDataShared.engine() else { return out }
        for (f, o) in zip(files, out) where o.isOK && !f.webTags.isEmpty {
            guard let key = try? await e.soundKey(f.sound) else { continue }
            for t in f.webTags { webTags.add(t, to: key) }
        }
        return out
    }
}

/// A kept SEND's outcome in a few words (the screens' note).
func keptSendNote(_ out: [UnitOutcome], to dest: SendDestination, unit: String) -> ScreenNote {
    let placed = out.filter(\.isOK).count
    let bad = out.first { !$0.isOK }.flatMap(ScreenNote.of)
    if placed == 0 { return bad ?? ScreenNote(text: "NOT NOW", color: FZ.muted, kind: .refused) }
    if let bad { return ScreenNote(text: "\(placed) SENT · \(bad.text)", color: bad.color, kind: .refused) }
    let place: String
    switch dest {
    case .mine: place = "MY PRESETS"
    case .pool: place = "POOL"
    case .track(let t): place = "TRACK \(t + 1)"
    }
    if out.count == 1, case .placed(let k) = out[0] {
        if case .track = dest { return .done("\(unit) · \(place)", FZ.violet) }
        return .done("\(unit) · \(place) \(k)", FZ.violet)
    }
    return .done("\(placed) → \(unit) · \(place)", FZ.violet)
}

/// KEPT's glyph (the phone).
enum KeptArt {
    static let phone = VectorArt(24, 24, [.rect(6.5, 2.5, 11, 19, 2.8), .path("M10.5 18.3h3")])
    static let check = VectorArt(24, 24, [.path("M5 12.5l5 5L19.5 7")])
    static let copy = VectorArt(24, 24, [.rect(8.5, 8.5, 12, 12, 2.5), .path("M15.5 5.5V3H3v12.5h2.5")])
}
