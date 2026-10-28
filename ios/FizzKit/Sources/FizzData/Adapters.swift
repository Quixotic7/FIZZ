// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzKit's results as FizzData takes them: a backup read's stats as the manifest's transfer, and a store layout's
// sectors (fizzstore.js LAYOUTS) for the progress screen's grid.

import Foundation
import FizzKit

extension FizzTransferStats {
    /// FizzLink.backup's stats, field for field.
    public init(_ s: FizzBackupStats) {
        self.init(bytes: s.bytes, ms: s.ms, kbps: s.kbps, frames: s.frames, resent: s.resent, depth: s.depth,
                  gapMs: s.gapMs, perFrame: s.perFrame, rttAvg: s.rtt.avg, rttMedian: s.rtt.median,
                  rttP95: s.rtt.p95, rttMax: s.rtt.max)
    }
}

/// A store layout: the region's size, MY PRESETS' sectors (first), then the projects'.
public struct FizzLayout: Sendable, Equatable {
    public static let sector = 4096
    public var name: String
    public var size: Int
    public var libSectors: Int
    public var prjSectors: Int
    public var sectors: Int { libSectors + prjSectors }

    public init(name: String, size: Int, libSectors: Int, prjSectors: Int) {
        self.name = name; self.size = size; self.libSectors = libSectors; self.prjSectors = prjSectors
    }

    /// The unit's (persist.h QD_DEVICE), as fizzstore.js has it.
    public static let device = FizzLayout(name: "device", size: 73 * sector, libSectors: 22, prjSectors: 51)
}

extension FizzDataEngine {
    /// fizzstore.js layoutOf(size) with its sectors; nil for a size FIZZ never had.
    public func layout(size: Int) async throws -> FizzLayout? {
        let v = try await call("layoutOf", [.int(size)])
        guard let name = v["name"]?.string, let lib = v["lib"]?["nsec"]?.int, let prj = v["prj"]?["nsec"]?.int else { return nil }
        return FizzLayout(name: name, size: v["size"]?.int ?? size, libSectors: lib, prjSectors: prj)
    }
}

/// One engine for the app (JavaScriptCore: one VM, its calls in turn).
public enum FizzDataShared {
    private static let made: Result<FizzDataEngine, Error> = Result { try FizzDataEngine() }
    public static func engine() throws -> FizzDataEngine { try made.get() }
}
