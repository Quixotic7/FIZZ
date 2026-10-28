// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzJSValue <-> JavaScriptCore values with the C API: a byte array crosses as one memcpy into (or out of) a
// Uint8Array's own memory (JSObjectMakeTypedArrayWithBytesNoCopy; the ArrayBuffer's bytes pointer and the view's
// offset), never element by element (a store image is 299,008 bytes). Values made here are protected from the
// collector until their parent holds them. Callers hold the context (FizzDataEngine, an actor).

@preconcurrency import JavaScriptCore
import Foundation

enum JSBridge {
    // ---- strings
    static func string(_ s: String) -> JSStringRef { JSStringCreateWithCFString(s as CFString) }
    static func swiftString(_ s: JSStringRef) -> String { JSStringCopyCFString(kCFAllocatorDefault, s) as String }
    static func toString(_ v: JSValueRef, _ ctx: JSContextRef) -> String {
        guard let s = JSValueToStringCopy(ctx, v, nil) else { return "" }
        defer { JSStringRelease(s) }
        return swiftString(s)
    }

    static func property(_ o: JSObjectRef, _ name: String, _ ctx: JSContextRef) -> JSValueRef? {
        let n = string(name)
        defer { JSStringRelease(n) }
        return JSObjectGetProperty(ctx, o, n, nil)
    }

    // ---- bytes
    static func makeBytes(_ b: [UInt8], _ ctx: JSContextRef) throws -> JSValueRef {
        var exc: JSValueRef?
        let o: JSObjectRef?
        if b.isEmpty {
            o = JSObjectMakeTypedArray(ctx, kJSTypedArrayTypeUint8Array, 0, &exc)
        } else {
            let p = malloc(b.count)!
            b.withUnsafeBytes { p.copyMemory(from: $0.baseAddress!, byteCount: b.count) }
            o = JSObjectMakeTypedArrayWithBytesNoCopy(ctx, kJSTypedArrayTypeUint8Array, p, b.count, { bytes, _ in free(bytes) }, nil, &exc)
            if o == nil { free(p) }
        }
        if let exc { throw FizzDataError.from(exc, ctx) }
        guard let o else { throw FizzDataError.bridge("could not make a Uint8Array of \(b.count) bytes") }
        return o
    }

    /// A Uint8Array's (Uint8ClampedArray's, Int8Array's, ArrayBuffer's) bytes, or nil (not one).
    static func bytes(_ v: JSValueRef, _ ctx: JSContextRef) -> [UInt8]? {
        let t = JSValueGetTypedArrayType(ctx, v, nil)
        guard t == kJSTypedArrayTypeUint8Array || t == kJSTypedArrayTypeUint8ClampedArray || t == kJSTypedArrayTypeInt8Array
                || t == kJSTypedArrayTypeArrayBuffer, let o = JSValueToObject(ctx, v, nil) else { return nil }
        let buffer: JSObjectRef?, offset: Int, count: Int
        if t == kJSTypedArrayTypeArrayBuffer {
            buffer = o; offset = 0; count = JSObjectGetArrayBufferByteLength(ctx, o, nil)
        } else {
            count = JSObjectGetTypedArrayByteLength(ctx, o, nil)
            offset = JSObjectGetTypedArrayByteOffset(ctx, o, nil)
            buffer = count > 0 ? JSObjectGetTypedArrayBuffer(ctx, o, nil) : nil
        }
        guard count > 0, let buffer, let base = JSObjectGetArrayBufferBytesPtr(ctx, buffer, nil) else { return [] }
        return [UInt8](UnsafeRawBufferPointer(start: base + offset, count: count))
    }

    // ---- Swift -> JS
    static func make(_ v: FizzJSValue, _ ctx: JSContextRef) throws -> JSValueRef {
        switch v {
        case .undefined: return JSValueMakeUndefined(ctx)
        case .null: return JSValueMakeNull(ctx)
        case .bool(let b): return JSValueMakeBoolean(ctx, b)
        case .number(let n): return JSValueMakeNumber(ctx, n)
        case .string(let s):
            let js = string(s)
            defer { JSStringRelease(js) }
            return JSValueMakeString(ctx, js)
        case .bytes(let b): return try makeBytes(b, ctx)
        case .date(let d):
            var exc: JSValueRef?
            var ms: JSValueRef? = JSValueMakeNumber(ctx, (d.timeIntervalSince1970 * 1000).rounded())
            let o = JSObjectMakeDate(ctx, 1, &ms, &exc)
            if let exc { throw FizzDataError.from(exc, ctx) }
            return o!
        case .array(let a):
            let items = try a.map { x -> JSValueRef? in let r = try make(x, ctx); JSValueProtect(ctx, r); return r }
            defer { for r in items { JSValueUnprotect(ctx, r) } }
            var exc: JSValueRef?
            let o = JSObjectMakeArray(ctx, items.count, items.isEmpty ? nil : items, &exc)
            if let exc { throw FizzDataError.from(exc, ctx) }
            return o!
        case .object(let members):
            let o = JSObjectMake(ctx, nil, nil)!
            JSValueProtect(ctx, o)
            defer { JSValueUnprotect(ctx, o) }
            for m in members {
                let val = try make(m.value, ctx)
                let name = string(m.key)
                var exc: JSValueRef?
                JSObjectSetProperty(ctx, o, name, val, JSPropertyAttributes(kJSPropertyAttributeNone), &exc)
                JSStringRelease(name)
                if let exc { throw FizzDataError.from(exc, ctx) }
            }
            return o
        }
    }

    // ---- JS -> Swift
    static func read(_ v: JSValueRef, _ ctx: JSContextRef, depth: Int = 0) -> FizzJSValue {
        guard depth < 64 else { return .undefined }               // (a cycle, or something too deep to be data)
        switch JSValueGetType(ctx, v) {
        case kJSTypeUndefined: return .undefined
        case kJSTypeNull: return .null
        case kJSTypeBoolean: return .bool(JSValueToBoolean(ctx, v))
        case kJSTypeNumber: return .number(JSValueToNumber(ctx, v, nil))
        case kJSTypeString: return .string(toString(v, ctx))
        case kJSTypeObject: break
        default: return .string(toString(v, ctx))                 // (a symbol, a bigint)
        }
        if let b = bytes(v, ctx) { return .bytes(b) }
        guard let o = JSValueToObject(ctx, v, nil) else { return .undefined }
        if JSValueIsDate(ctx, v) { return .date(Date(timeIntervalSince1970: JSValueToNumber(ctx, v, nil) / 1000)) }
        if JSObjectIsFunction(ctx, o) { return .undefined }
        if JSValueIsArray(ctx, v) || JSValueGetTypedArrayType(ctx, v, nil) != kJSTypedArrayTypeNone {
            let n = Int(JSValueToNumber(ctx, property(o, "length", ctx) ?? JSValueMakeNumber(ctx, 0), nil))
            return .array((0..<max(0, n)).map { i in
                JSObjectGetPropertyAtIndex(ctx, o, UInt32(i), nil).map { read($0, ctx, depth: depth + 1) } ?? .undefined
            })
        }
        let names = JSObjectCopyPropertyNames(ctx, o)!
        defer { JSPropertyNameArrayRelease(names) }
        var out: [FizzJSMember] = []
        for i in 0..<JSPropertyNameArrayGetCount(names) {
            let name = JSPropertyNameArrayGetNameAtIndex(names, i)!
            let value = JSObjectGetProperty(ctx, o, name, nil).map { read($0, ctx, depth: depth + 1) } ?? .undefined
            out.append(FizzJSMember(key: swiftString(name), value: value))
        }
        return .object(out)
    }
}
