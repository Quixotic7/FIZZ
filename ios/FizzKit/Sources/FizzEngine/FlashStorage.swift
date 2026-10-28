// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Where the emulated FIZZ keeps its flash between launches: one file, the core's whole 1 MiB image.

import Foundation

public struct FlashStorage: Sendable {
    public let url: URL

    public init(url: URL) { self.url = url }

    /// Documents/Emulator/fizz-flash.bin
    public static var documents: FlashStorage {
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first
            ?? URL(fileURLWithPath: NSTemporaryDirectory())
        return FlashStorage(url: docs.appendingPathComponent("Emulator/fizz-flash.bin"))
    }

    /// The saved image, nil when there is none (or it is not a whole image).
    public func load(size: Int) -> [UInt8]? {
        guard let d = try? Data(contentsOf: url), d.count == size else { return nil }
        return [UInt8](d)
    }

    public func save(_ bytes: [UInt8]) throws {
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(bytes).write(to: url, options: .atomic)
    }

    public func remove() { try? FileManager.default.removeItem(at: url) }
}
