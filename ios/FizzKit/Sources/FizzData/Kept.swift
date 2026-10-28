// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// KEPT, the phone's own presets (entry.js "KEPT"): each a .fizzsnd as the web Manager writes it (js/library.js), its
// web tags beside FIZZ's ("webTags", only when it has some); several shared as a .fizzlib (makeFizzlib's).

import Foundation

/// A kept preset's file read: its sound and its web tags.
public struct FizzKeptSound: Sendable, Equatable {
    public var sound: FizzSound
    public var webTags: [String]
    public init(sound: FizzSound, webTags: [String] = []) { self.sound = sound; self.webTags = webTags }
}

extension FizzDataEngine {
    /// The .fizzsnd of a kept preset (source: merged into the sound's own).
    public func keptFile(_ s: FizzSound, webTags: [String] = [], source: FizzJSValue = [:]) async throws -> [UInt8] {
        try await call("keptFile", [s.js, .array(webTags.map { .string($0) }), source]).bytes ?? []
    }

    /// A .fizzsnd read (parseFizzsnd: throws when it is not one), with its web tags.
    public func keptRead(_ bytes: [UInt8], fileName: String = "") async throws -> FizzKeptSound {
        let v = try await call("keptRead", [.bytes(bytes), .string(fileName)])
        return FizzKeptSound(sound: FizzSound(js: v["sound"] ?? [:]), webTags: v["webTags"]?.array?.compactMap(\.string) ?? [])
    }

    /// Several as one .fizzlib (each member a kept file).
    public func keptLib(_ entries: [FizzKeptSound], name: String? = nil, fizz: String = "", date: Date = Date()) async throws -> [UInt8] {
        var o: [FizzJSMember] = [.init(key: "fizz", value: .string(fizz)), .init(key: "date", value: .date(date))]
        if let name { o.append(.init(key: "name", value: .string(name))) }
        let list: [FizzJSValue] = entries.map { ["sound": $0.sound.js, "webTags": .array($0.webTags.map { .string($0) })] }
        return try await call("keptLib", [.array(list), .object(o)]).bytes ?? []
    }

    /// A preset's file name (js/library.js soundFileName: "PLUCK 2.fizzsnd", the characters a file cannot have as _).
    public func keptFileName(_ name: String) async throws -> String {
        try await call("keptFileName", [.string(name)]).string ?? "PRESET.fizzsnd"
    }
}
