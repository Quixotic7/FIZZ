// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// JSON values compared as the JS wrote them (booleans kept apart from numbers), and vectors.json loaded.

import Foundation
@testable import FizzKit

enum JV: Equatable, CustomStringConvertible {
    case null, bool(Bool), num(Double), str(String), arr([JV]), obj([String: JV])

    init(any: Any) {
        switch any {
        case is NSNull: self = .null
        case let n as NSNumber:
            self = CFGetTypeID(n) == CFBooleanGetTypeID() ? .bool(n.boolValue) : .num(n.doubleValue)
        case let s as String: self = .str(s)
        case let a as [Any]: self = .arr(a.map(JV.init(any:)))
        case let d as [String: Any]: self = .obj(d.mapValues(JV.init(any:)))
        default: self = .null
        }
    }

    /// A Swift value as JSON (nil: null).
    init<T: Encodable>(encoding v: T?) throws {
        guard let v else { self = .null; return }
        let data = try JSONEncoder().encode(v)
        self = JV(any: try JSONSerialization.jsonObject(with: data, options: .fragmentsAllowed))
    }

    subscript(_ k: String) -> JV? { if case .obj(let d) = self { d[k] } else { nil } }
    subscript(_ i: Int) -> JV { if case .arr(let a) = self { a[i] } else { .null } }

    var int: Int? { if case .num(let n) = self { Int(n) } else { nil } }
    var bool: Bool? { if case .bool(let b) = self { b } else { nil } }
    var string: String? { if case .str(let s) = self { s } else { nil } }
    var array: [JV] { if case .arr(let a) = self { a } else { [] } }
    var isNull: Bool { self == .null }
    var bytes: [UInt8] { array.map { UInt8($0.int!) } }
    var u32s: [UInt32] { array.map { UInt32($0.int!) } }

    var description: String {
        switch self {
        case .null: "null"
        case .bool(let b): "\(b)"
        case .num(let n): n == n.rounded() ? "\(Int(n))" : "\(n)"
        case .str(let s): "\"\(s)\""
        case .arr(let a): "[" + a.map(\.description).joined(separator: ",") + "]"
        case .obj(let d): "{" + d.keys.sorted().map { "\($0):\(d[$0]!)" }.joined(separator: ",") + "}"
        }
    }
}

enum Vectors {
    static let all: JV = {
        let url = Bundle.module.url(forResource: "vectors", withExtension: "json", subdirectory: "Resources")!
        let data = try! Data(contentsOf: url)
        return JV(any: try! JSONSerialization.jsonObject(with: data))
    }()
}
