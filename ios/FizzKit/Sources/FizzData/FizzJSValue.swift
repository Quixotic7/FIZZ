// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A JavaScript value as Swift holds it, both ways across the bridge: objects keep their keys' order (the JS writes
// JSON in insertion order, and a .fizzbak's bytes depend on it), byte arrays are Uint8Arrays, dates JS Dates.

import Foundation

public indirect enum FizzJSValue: Sendable, Equatable {
    case undefined
    case null
    case bool(Bool)
    case number(Double)
    case string(String)
    /// A Uint8Array (out of JS: any Uint8Array, Uint8ClampedArray, Int8Array or ArrayBuffer).
    case bytes([UInt8])
    case date(Date)
    case array([FizzJSValue])
    /// An object, its own enumerable properties in their order.
    case object([FizzJSMember])

    public static func int(_ v: Int) -> FizzJSValue { .number(Double(v)) }
    public static func obj(_ members: KeyValuePairs<String, FizzJSValue>) -> FizzJSValue {
        .object(members.map { FizzJSMember(key: $0.key, value: $0.value) })
    }
    /// A string-keyed dictionary as an object, its keys sorted (a stable order).
    public static func sorted(_ d: [String: FizzJSValue]) -> FizzJSValue {
        .object(d.keys.sorted().map { FizzJSMember(key: $0, value: d[$0]!) })
    }

    public subscript(key: String) -> FizzJSValue? {
        if case .object(let m) = self { return m.first { $0.key == key }?.value }
        return nil
    }
    public subscript(index: Int) -> FizzJSValue? {
        if case .array(let a) = self, a.indices.contains(index) { return a[index] }
        return nil
    }
    public var string: String? { if case .string(let s) = self { return s }; return nil }
    public var number: Double? { if case .number(let n) = self { return n }; return nil }
    public var int: Int? { if case .number(let n) = self, n.isFinite, n == n.rounded() { return Int(n) }; return nil }
    public var bool: Bool? { if case .bool(let b) = self { return b }; return nil }
    public var bytes: [UInt8]? { if case .bytes(let b) = self { return b }; return nil }
    public var array: [FizzJSValue]? { if case .array(let a) = self { return a }; return nil }
    public var members: [FizzJSMember]? { if case .object(let m) = self { return m }; return nil }
    public var isNullish: Bool { self == .null || self == .undefined }
    /// null and undefined as nil.
    public var nonNull: FizzJSValue? { isNullish ? Optional.none : self }
}

public struct FizzJSMember: Sendable, Equatable {
    public var key: String
    public var value: FizzJSValue
    public init(key: String, value: FizzJSValue) { self.key = key; self.value = value }
}

extension FizzJSValue: ExpressibleByStringLiteral, ExpressibleByIntegerLiteral, ExpressibleByFloatLiteral,
    ExpressibleByBooleanLiteral, ExpressibleByArrayLiteral, ExpressibleByDictionaryLiteral {
    public init(stringLiteral v: String) { self = .string(v) }
    public init(integerLiteral v: Int) { self = .number(Double(v)) }
    public init(floatLiteral v: Double) { self = .number(v) }
    public init(booleanLiteral v: Bool) { self = .bool(v) }
    public init(arrayLiteral v: FizzJSValue...) { self = .array(v) }
    /// (the literal's order is the object's)
    public init(dictionaryLiteral v: (String, FizzJSValue)...) { self = .object(v.map { FizzJSMember(key: $0.0, value: $0.1) }) }
}
