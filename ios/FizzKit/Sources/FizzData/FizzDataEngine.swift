// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The web Manager's data logic run in JavaScriptCore (ios/PLAN.md §3 B): FizzData.js (ios/jsbundle/build.mjs, the
// site modules unchanged) loaded once into one JSVirtualMachine + JSContext, owned by this actor. What the host gives
// the bundle: __fizzInflateRaw (raw DEFLATE, Apple's Compression) and console (to the logger); the JS polyfills
// (TextEncoder / TextDecoder, atob / btoa) are in the bundle itself. No timers: the paths bridged need none, and every
// promise they make settles before the call that made it returns (JavaScriptCore runs its microtasks when the
// outermost call into it ends); one that does not is an error (FizzDataError.unsettled), never a hang.
//
//   let fd = try FizzDataEngine()
//   let file = try await fd.makeFizzbak(FizzbakInput(kind: .full, image: image, presets: dumps, ...))
//   let b = try await fd.readBackup(bytes, fileName: "FIZZ backup 2026-10-27 1432.fizzbak")

@preconcurrency import JavaScriptCore
import Foundation
import os
import FizzKit

public actor FizzDataEngine {
    private let vm: JSVirtualMachine
    private let context: JSContext
    private let ctx: JSGlobalContextRef
    private let api: JSObjectRef
    private let apiValue: JSValue                  // (keeps FizzData alive)
    /// FIZZ's tags, bit k (fizz_sysex.js FIZZ_TAGS).
    public nonisolated let tagNames: [String]

    private static let logger = Logger(subsystem: "com.quixotic7.fizz", category: "FizzData")

    /// Loads FizzData.js (the package's resource) into a new JavaScriptCore VM. log: the bundle's console output.
    public init(log: (@Sendable (String) -> Void)? = nil) throws {
        guard let url = Bundle.module.url(forResource: "FizzData", withExtension: "js"),
              let script = try? String(contentsOf: url, encoding: .utf8) else { throw FizzDataError.bridge("FizzData.js is not in the bundle") }
        try self.init(script: script, log: log)
    }

    init(script: String, log: (@Sendable (String) -> Void)?) throws {
        let vm = JSVirtualMachine()!
        let context = JSContext(virtualMachine: vm)!
        context.name = "FizzData"
        let ctx = context.jsGlobalContextRef!
        let global = JSContextGetGlobalObject(ctx)!

        // console -> the logger
        let say: @convention(block) () -> Void = {
            let line = (JSContext.currentArguments() as? [JSValue] ?? []).map { $0.toString() ?? "" }.joined(separator: " ")
            FizzDataEngine.logger.debug("\(line, privacy: .public)")
            log?(line)
        }
        let console = JSValue(newObjectIn: context)!
        for k in ["log", "info", "warn", "error", "debug"] { console.setObject(say, forKeyedSubscript: k as NSString) }
        context.setObject(console, forKeyedSubscript: "console" as NSString)

        // __fizzInflateRaw(Uint8Array) -> Uint8Array
        let inflate: @convention(block) (JSValue) -> JSValue? = { arg in
            let c = JSContext.current()!, r = c.jsGlobalContextRef!
            do {
                guard let src = JSBridge.bytes(arg.jsValueRef, r) else { throw RawInflate.Failure(description: "__fizzInflateRaw: a Uint8Array") }
                return JSValue(jsValueRef: try JSBridge.makeBytes(try RawInflate.inflate(src), r), in: c)
            } catch {
                c.exception = JSValue(newErrorFromMessage: "\(error)", in: c)
                return nil
            }
        }
        context.setObject(inflate, forKeyedSubscript: "__fizzInflateRaw" as NSString)

        let src = JSBridge.string(script), name = JSBridge.string("FizzData.js")
        defer { JSStringRelease(src); JSStringRelease(name) }
        var exc: JSValueRef?
        JSEvaluateScript(ctx, src, nil, name, 1, &exc)
        if let exc { throw FizzDataError.from(exc, ctx) }
        guard let fd = JSBridge.property(global, "FizzData", ctx), JSValueIsObject(ctx, fd), let api = JSValueToObject(ctx, fd, nil) else {
            throw FizzDataError.bridge("FizzData.js did not define globalThis.FizzData")
        }
        let tags = JSBridge.property(api, "FIZZ_TAGS", ctx).map { JSBridge.read($0, ctx) }
        self.vm = vm
        self.context = context
        self.ctx = ctx
        self.api = api
        self.apiValue = JSValue(jsValueRef: fd, in: context)
        self.tagNames = tags?.array?.compactMap(\.string) ?? []
    }

    // MARK: - calls

    /// FizzData[name](...args), its promise awaited.
    public func call(_ name: String, _ args: [FizzJSValue] = []) async throws -> FizzJSValue {
        let r = try await settle(try invoke(api, name, args), name)
        return JSBridge.read(r.value, ctx)
    }

    /// A function of FizzData (or, with global, of globalThis) called; its raw result (sync).
    private func invoke(_ on: JSObjectRef, _ name: String, _ args: [FizzJSValue]) throws -> JSValueRef {
        guard let f = JSBridge.property(on, name, ctx), JSValueIsObject(ctx, f), let fn = JSValueToObject(ctx, f, nil), JSObjectIsFunction(ctx, fn) else {
            throw FizzDataError.bridge("FizzData has no function \(name)")
        }
        let refs = try args.map { a -> JSValueRef? in let r = try JSBridge.make(a, ctx); JSValueProtect(ctx, r); return r }
        defer { for r in refs { JSValueUnprotect(ctx, r) } }
        var exc: JSValueRef?
        let r = JSObjectCallAsFunction(ctx, fn, on, refs.count, refs.isEmpty ? nil : refs, &exc)
        if let exc { throw FizzDataError.from(exc, ctx) }
        return r ?? JSValueMakeUndefined(ctx)
    }

    private func callSync(_ name: String, _ args: [FizzJSValue] = []) throws -> FizzJSValue {
        let r = try invoke(api, name, args)
        if isPromise(r) { throw FizzDataError.bridge("\(name) is async") }
        return JSBridge.read(r, ctx)
    }

    private func isPromise(_ v: JSValueRef) -> Bool {
        guard JSValueIsObject(ctx, v), let p = JSBridge.property(JSContextGetGlobalObject(ctx), "Promise", ctx),
              let ctor = JSValueToObject(ctx, p, nil) else { return false }
        return JSValueIsInstanceOfConstructor(ctx, v, ctor, nil)
    }

    /// A JS value kept alive (JSValue protects it) across the continuation.
    private final class Held: @unchecked Sendable { let value: JSValueRef; let keep: JSValue; init(_ v: JSValue) { keep = v; value = v.jsValueRef } }
    private final class Once: @unchecked Sendable {
        private var done = false
        func finish(_ c: CheckedContinuation<Held, Error>, _ r: Result<Held, Error>) {
            guard !done else { return }
            done = true
            c.resume(with: r)
        }
    }

    /// The value, or what its promise settles to: then(..) with native blocks, into a continuation. The promise's
    /// jobs run as the then(..) call returns; a promise still pending then never will (no timers here).
    private func settle(_ v: JSValueRef, _ what: String) async throws -> Held {
        let context = self.context
        let value = JSValue(jsValueRef: v, in: context)!
        guard isPromise(v) else { return Held(value) }
        let once = Once()
        return try await withCheckedThrowingContinuation { (c: CheckedContinuation<Held, Error>) in
            let ok: @convention(block) (JSValue) -> Void = { r in once.finish(c, .success(Held(r))) }
            let no: @convention(block) (JSValue) -> Void = { e in once.finish(c, .failure(FizzDataError.from(e.jsValueRef, context.jsGlobalContextRef))) }
            value.invokeMethod("then", withArguments: [JSValue(object: ok, in: context)!, JSValue(object: no, in: context)!])
            once.finish(c, .failure(FizzDataError.unsettled(what)))
        }
    }

    // MARK: - the backup file

    /// fizzbak.js makeFizzbak with manager.js backup()'s arguments (entry.js makeUnitFizzbak): the .fizzbak's bytes.
    public func makeFizzbak(_ input: FizzbakInput) throws -> [UInt8] {
        var m: [FizzJSMember] = [
            .init(key: "kind", value: .string(input.kind.rawValue)),
            .init(key: "image", value: input.image.map { .bytes($0) } ?? .null),
            .init(key: "region", value: input.region.map { [.int($0.lo), .int($0.size)] } ?? .null),
            .init(key: "crc", value: input.crc.map { .number(Double($0)) } ?? .null),
            .init(key: "frames", value: input.presets.map { .array($0.map { .bytes($0) }) } ?? .null),
            .init(key: "fizz", value: .string(input.fizz)),
            .init(key: "unit", value: ["identity": .string(input.unitIdentity), "port": .string(input.unitPort)]),
            .init(key: "webTags", value: .sorted(input.webTags.mapValues { .array($0.map { .string($0) }) })),
            .init(key: "date", value: .date(input.date)),
        ]
        if let s = input.stats { m.append(.init(key: "stats", value: s.js)) }
        guard let b = try callSync("makeUnitFizzbak", [.object(m)]).bytes else { throw FizzDataError.bridge("makeUnitFizzbak: not bytes") }
        return b
    }

    /// fizzbak.js readBackup: a .fizzbak, a .fizzlib or a raw store image (.bin), as the BACKUP screen shows it.
    public func readBackup(_ bytes: [UInt8], fileName: String) async throws -> FizzBackupSummary {
        let v = try await call("readBackup", [.bytes(bytes), .string(fileName)])
        let m = v["manifest"] ?? [:], store = m["store"]?.nonNull
        let summary = v["summary"]?.nonNull.map { FizzRegionSummary(js: $0, tagNames: tagNames) }
        let image = v["image"]?.bytes
        var crc = store?["crc32"]?.string
        if crc == nil, let image { crc = try hex8(crc32(image)) }
        let region = store?["region"]?.array.flatMap { a in a.count == 2 ? a[0].int.flatMap { lo in a[1].int.map { FizzStoreRegion(lo: lo, size: $0) } } : nil }
        let dateText = m["date"]?.string
        var tags: [String: [String]] = [:]
        for t in v["web"]?["tags"]?.members ?? [] { tags[t.key] = t.value.array?.compactMap(\.string) ?? [] }
        return FizzBackupSummary(
            kind: v["kind"]?.string == "presets" ? .presets : .full,
            source: FizzBackupSummary.Source(rawValue: v["source"]?.string ?? "") ?? .fizzbak,
            dateText: dateText, date: dateText.flatMap(Self.parseDate), fizz: m["fizz"]?.string ?? "",
            unitIdentity: m["unit"]?["identity"]?.string, unitPort: m["unit"]?["port"]?.string,
            layout: summary?.layout ?? store?["layout"]?.string, tracks: summary?.tracks ?? store?["tracks"]?.int,
            format: summary?.format ?? store?["format"]?.int, magic: summary?.magic ?? store?["magic"]?.string,
            projects: summary?.projects, presetCount: v["presets"]?.array?.count ?? 0, storeSize: image?.count, storeCRC: crc,
            region: region, used: summary?.used, presets: (v["presets"]?.array ?? []).map { FizzSound(js: $0) }, webTags: tags,
            skipped: (v["skipped"]?.array ?? []).map { FizzSkippedFile(file: $0["file"]?.string ?? "", error: $0["error"]?.string ?? "") },
            image: image, summary: summary, transfer: m["transfer"]?.nonNull, manifest: m)
    }

    /// "FIZZ backup 2026-10-27 1432.fizzbak" (local time), "FIZZ presets ..".
    public func backupFileName(date: Date, kind: FizzBackupKind) throws -> String {
        try callSync("backupFileName", [.date(date), .string(kind.rawValue)]).string ?? ""
    }

    public func crc32(_ bytes: [UInt8]) throws -> UInt32 {
        UInt32(try callSync("crc32", [.bytes(bytes)]).number ?? 0)
    }
    public func hex8(_ n: UInt32) throws -> String { try callSync("hex8", [.number(Double(n))]).string ?? "" }

    /// The web tags' key of a sound (its name and FM TONE values).
    public func soundKey(_ sound: FizzSound) throws -> String { try callSync("soundKey", [sound.js]).string ?? "" }

    // MARK: - the store image

    /// fizzstore.js readRegion (throws: not a FIZZ store image).
    public func readRegion(_ image: [UInt8]) throws -> FizzRegionSummary {
        FizzRegionSummary(js: try callSync("readRegion", [.bytes(image)]), tagNames: tagNames)
    }

    /// The layout ("device", "host") of a store of this size, or nil.
    public func layoutOf(size: Int) throws -> String? { try callSync("layoutOf", [.int(size)])["name"]?.string }

    /// The demo unit's store: MY PRESETS from sounds (version-6 blobs; slot = their order), projects as their header
    /// records (entry.js demoStore).
    public func demoStore(presets: [FizzSound], projects: [FizzProjectName] = [], layout: String = "device") throws -> [UInt8] {
        let arg: FizzJSValue = ["presets": .array(presets.map(\.js)),
                                "projects": .array(projects.map { ["slot": .int($0.slot), "name": .string($0.name)] }),
                                "layout": .string(layout)]
        guard let b = try callSync("demoStore", [arg]).bytes else { throw FizzDataError.bridge("demoStore: not bytes") }
        return b
    }

    /// A DUMP reply (a STORE / STORE6 frame) as js/library.js's sound.
    public func soundFromStoreFrame(_ frame: [UInt8]) throws -> FizzSound {
        FizzSound(js: try callSync("soundFromStoreFrame", [.bytes(frame)]))
    }

    // MARK: - for the tests

    /// A script evaluated in the engine's context; its result.
    func evaluate(_ script: String) throws -> FizzJSValue {
        let s = JSBridge.string(script)
        defer { JSStringRelease(s) }
        var exc: JSValueRef?
        let r = JSEvaluateScript(ctx, s, nil, nil, 1, &exc)
        if let exc { throw FizzDataError.from(exc, ctx) }
        return r.map { JSBridge.read($0, ctx) } ?? .undefined
    }

    /// JSON.stringify(FizzData.toPlain(FizzData[name](...args))), its promise awaited, or "throws: message" (the
    /// fixtures' form, gen_fixtures.mjs).
    func plainJSON(_ name: String, _ args: [FizzJSValue]) async throws -> String {
        let r: Held
        do { r = try await settle(try invoke(api, name, args), name) } catch let e as FizzDataError { return "throws: \(e.message)" }
        guard let tp = JSBridge.property(api, "toPlain", ctx), let fn = JSValueToObject(ctx, tp, nil) else { throw FizzDataError.bridge("toPlain") }
        var exc: JSValueRef?
        var arg: JSValueRef? = r.value
        let p = JSObjectCallAsFunction(ctx, fn, api, 1, &arg, &exc)
        if let exc { throw FizzDataError.from(exc, ctx) }
        guard let p, let json = JSValueCreateJSONString(ctx, p, 0, &exc) else { throw FizzDataError.bridge("JSON.stringify") }
        defer { JSStringRelease(json) }
        return JSBridge.swiftString(json)
    }

    /// Bytes into a Uint8Array, through a JS function (begin, end: a subarray of it, or all) and back out.
    func roundTrip(_ bytes: [UInt8], begin: Int = 0, end: Int? = nil) throws -> [UInt8] {
        let global = JSContextGetGlobalObject(ctx)!
        if JSBridge.property(global, "__fizzEcho", ctx).map({ JSValueIsUndefined(ctx, $0) }) ?? true {
            _ = try evaluate("globalThis.__fizzEcho = (b, i, j) => b.subarray(i, j);")
        }
        let r = try invoke(global, "__fizzEcho", [.bytes(bytes), .int(begin), end.map { .int($0) } ?? .undefined])
        guard let out = JSBridge.bytes(r, ctx) else { throw FizzDataError.bridge("__fizzEcho: not bytes") }
        return out
    }

    static func parseDate(_ s: String) -> Date? {
        let f = ISO8601DateFormatter()
        f.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        if let d = f.date(from: s) { return d }
        f.formatOptions = [.withInternetDateTime]
        return f.date(from: s)
    }
}
