// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FIZZ's learned MIDI map, kept (mock-up 55): one file in Application Support, `midi-map.json`, in RemoteFM1's stored
// form and ids (MidiMap.json: {"midiMap": [{src, target, mode?}], "knobSpeed": 0.5 | 1 | 2}), so a map exported from
// either app reads the same in the other. Saved on every change; import replaces the map with a file's (RemoteFM1's
// export, or FIZZ's), export writes it out for the share sheet. No App Group: FIZZ has no extension to share it with.
// Written for FIZZ (RemoteFM1's own store is not used).

import Foundation
import Observation

@MainActor @Observable
public final class MidiMapStore {
    public private(set) var map: MidiMap
    /// the file (nil: kept in memory only, the snapshots)
    @ObservationIgnored public let url: URL?
    /// the map changed (the router gives it to its mapper)
    @ObservationIgnored public var onChange: ((MidiMap) -> Void)?

    /// The app's file: Application Support/midi-map.json (in the app's container). On the Mac (the tests, the
    /// snapshots) none: kept in memory.
    public static var defaultURL: URL? {
        #if os(iOS)
        guard let dir = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first else { return nil }
        return dir.appendingPathComponent("midi-map.json")
        #else
        return nil
        #endif
    }

    public init(url: URL? = MidiMapStore.defaultURL) {
        self.url = url
        map = url.flatMap { try? Data(contentsOf: $0) }.map { MidiMap(json: $0) } ?? MidiMap()
    }

    /// In memory, with this map (the demo and the snapshots).
    public init(map: MidiMap) {
        url = nil
        self.map = map
    }

    public var mappings: [MidiMapping] { map.mappings }
    public var speed: Double { map.speed }
    public func sourcesOf(_ target: String) -> [MidiMapping] { map.sourcesOf(target) }

    /// The file read again.
    public func load() {
        let m = url.flatMap { try? Data(contentsOf: $0) }.map { MidiMap(json: $0) } ?? MidiMap()
        if m != map { map = m; onChange?(map) }
    }

    @discardableResult public func bind(_ src: String, to target: String, mode: MidiMode? = nil) -> MidiMapping? {
        var m = map
        let mp = m.bind(src, to: target, mode: mode)
        if mp != nil { set(m) }
        return mp
    }

    public func setMode(_ src: String, _ mode: MidiMode) { change { $0.setMode(src, mode) } }
    public func remove(_ src: String) { change { $0.remove(src) } }
    @discardableResult public func forget(_ target: String) -> Int {
        var m = map
        let n = m.forget(target)
        if n > 0 { set(m) }
        return n
    }
    public func clear() { change { $0.clear() } }
    public func setSpeed(_ v: Double) { change { $0.setSpeed(v) } }

    // MARK: import and export

    /// The map as a file (RemoteFM1's stored form).
    public var exported: Data { map.json }

    public enum ImportError: Error, Equatable { case notAMap }

    /// A map file read in, in place of this one. A file with no "midiMap" list is not a map (nothing changes); one
    /// whose records are all unknown reads as empty, as RemoteFM1 reads it.
    public func importMap(_ data: Data) throws {
        guard let o = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any], o["midiMap"] is [Any] else {
            throw ImportError.notAMap
        }
        set(MidiMap(json: data))
    }

    // MARK: -

    private func change(_ f: (inout MidiMap) -> Void) {
        var m = map
        f(&m)
        set(m)
    }

    private func set(_ m: MidiMap) {
        guard m != map else { return }
        map = m
        save()
        onChange?(map)
    }

    private func save() {
        guard let url else { return }
        try? FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? map.json.write(to: url, options: .atomic)
    }
}
