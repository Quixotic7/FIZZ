// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzKit against the JS: every case of vectors.json (Tests/gen_vectors.mjs), byte for byte and field by field.

import Foundation
import Testing
@testable import FizzKit

private let V = Vectors.all

private func source(_ v: JV?) -> FizzSource { FizzSource(name: v?.string ?? "mine")! }
private func track(_ v: JV?) -> FizzTrack {
    if let t = v?.int { return .index(t) }
    return .selected
}
private func tags(_ v: JV?) -> FizzTags? {
    switch v {
    case .num(let n)?: return .mask(Int(n))
    case .arr(let a)?: return .names(a.map { $0.string! })
    default: return nil
    }
}

private func runBuilder(_ fn: String, _ a: JV) throws -> [UInt8] {
    switch fn {
    case "identity": return FizzSysEx.identity()
    case "storeSound":
        let x = a["extras"].flatMap { e -> FizzTrackExtrasIn? in
            guard !e.isNull else { return nil }
            return FizzTrackExtrasIn(id: e["id"]!.int!, sound: e["sound"]!.int!, edited: e["edited"]!.bool!)
        }
        return try FizzSysEx.storeSound(
            name: a["name"]?.string ?? "", blob: V["blobs"]![a["blob"]!.string!]!.bytes, drive: a["drive"]?.int ?? 0,
            pool: a["pool"]?.bool ?? false, slot: a["slot"]?.int ?? 0, batch: a["batch"]?.bool ?? false,
            track: a["track"]?.int, pan: a["pan"]?.int, chorus: a["chorus"]?.int, delay: a["delay"]?.int,
            reverb: a["reverb"]?.int, tags: tags(a["tags"]), audition: a["audition"]?.bool ?? false,
            own: a["own"]?.bool ?? false, extras: x)
    case "paramRequest":
        return FizzSysEx.paramRequest(track: track(a["track"]), id: a["id"]!.int!, value: a["value"]!.int!,
                                      ack: a["ack"]!.bool!)
    case "browseRequest": return FizzSysEx.browseRequest(track: track(a["track"]), id: a["id"]!.int!)
    case "labelsRequest": return FizzSysEx.labelsRequest(subscribe: a["subscribe"]!.bool!)
    case "listRequest": return FizzSysEx.listRequest(factory: a["factory"]!.bool!, pool: a["pool"]!.bool!)
    case "dumpRequest": return try FizzSysEx.dumpRequest(source: source(a["source"]), slot: a["slot"]!.int!)
    case "statusRequest": return FizzSysEx.statusRequest()
    case "flushRequest": return FizzSysEx.flushRequest()
    case "projectsRequest": return FizzSysEx.projectsRequest()
    case "tagsRequest":
        return try FizzSysEx.tagsRequest(source: source(a["source"]), slot: a["slot"]!.int!, tags: tags(a["tags"]))
    case "renameRequest":
        return try FizzSysEx.renameRequest(source: source(a["source"]), slot: a["slot"]!.int!, name: a["name"]!.string!)
    case "deleteRequest":
        return try FizzSysEx.deleteRequest(source: source(a["source"]), slot: a["slot"]!.int!, confirm: a["confirm"]!.bool!)
    case "swapRequest":
        return try FizzSysEx.swapRequest(source: source(a["source"]), a: a["a"]!.int!, b: a["b"]!.int!)
    case "usersRequest": return try FizzSysEx.usersRequest(source: source(a["source"]), slot: a["slot"]!.int!)
    case "poolRefRequest": return FizzSysEx.poolRefRequest(slot: a["slot"]!.int!, id: a["id"]!.int!)
    case "projectRequest":
        let op: FizzProjectOp = ["load": .load, "new": .new, "copy": .copy, "delete": .delete][a["op"]!.string!]!
        return FizzSysEx.projectRequest(op: op, p: a["p"]!.int!, q: a["q"]!.int!)
    default:
        Issue.record("no builder \(fn)")
        return []
    }
}

@Suite("JS vectors")
struct VectorTests {
    @Test func builders() throws {
        let cases = V["builders"]!.array
        #expect(cases.count > 300)
        for c in cases {
            let fn = c["fn"]!.string!, args = c["args"]!
            if c["throws"]?.bool == true {
                #expect(throws: (any Error).self, "\(fn) \(args)") { try runBuilder(fn, args) }
            } else {
                let out = try runBuilder(fn, args)
                #expect(out == c["out"]!.bytes, "\(fn) \(args)")
            }
        }
    }

    @Test func parsers() throws {
        let cases = V["parsers"]!.array
        #expect(cases.count > 40)
        for c in cases {
            let f = c["in"]!.bytes, label = c["label"]!.string!
            let got: [(String, JV)] = [
                ("parseReply", try JV(encoding: FizzSysEx.parseReply(f))),
                ("parseLabels", try JV(encoding: FizzSysEx.parseLabels(f))),
                ("parseList", try JV(encoding: FizzSysEx.parseList(f))),
                ("parseDump", try JV(encoding: FizzSysEx.parseDump(f))),
                ("parseAck", try JV(encoding: FizzSysEx.parseAck(f))),
                ("parseStatus", try JV(encoding: FizzSysEx.parseStatus(f))),
                ("parseUsers", try JV(encoding: FizzSysEx.parseUsers(f))),
                ("parseProjects", try JV(encoding: FizzSysEx.parseProjects(f))),
            ]
            for (fn, v) in got {
                #expect(v == c[fn]!, "\(label): \(fn)\n swift \(v)\n js    \(c[fn]!)")
            }
        }
    }

    @Test func reframes() {
        for c in V["reframes"]!.array {
            let f = c["in"]!.bytes, label = c["label"]!.string!
            #expect(FizzSysEx.trackFrame(f) == c["trackFrame"]!.bytes, "\(label)")
            #expect(FizzSysEx.restoreFrame(f) == c["restore"]!.bytes, "\(label)")
            #expect(FizzSysEx.restoreFrame(f, slot: 0) == c["restore0"]!.bytes, "\(label)")
            #expect(FizzSysEx.restoreFrame(f, slot: 200) == c["restore200"]!.bytes, "\(label)")
        }
    }

    @Test func usbPackets() {
        for c in V["usb"]!.array { #expect(FizzSysEx.usbPackets(c["in"]!.bytes) == c["out"]!.u32s) }
    }

    @Test func tagMasks() {
        for c in V["tagMasks"]!.array { #expect(FizzSysEx.fizzTagMask(tags(c["tags"])) == c["mask"]!.int!, "\(c)") }
    }

    @Test func packing() {
        let cases = V["pack"]!.array
        #expect(cases.count == 41)
        for c in cases {
            let b = c["in"]!.bytes
            #expect(FizzSysEx.pack7(b) == c["grouped"]!.bytes)
            #expect(FizzSysEx.unpack7(c["grouped"]!.bytes) == c["groupedBack"]!.bytes)
            #expect(FizzSysEx.unpack7(FizzSysEx.pack7(b)) == b)
            #expect(FM1Bulk.pack7(b) == c["stream"]!.bytes)
            #expect(FM1Bulk.unpack7(c["stream"]!.bytes) == c["streamBack"]!.bytes)
        }
        for c in V["unpackOdd"]!.array {
            #expect(FizzSysEx.unpack7(c["in"]!.bytes) == c["grouped"]!.bytes)
            #expect(FM1Bulk.unpack7(c["in"]!.bytes) == c["stream"]!.bytes)
        }
    }

    @Test func crc32() {
        for c in V["crc32"]!.array {
            let c0 = c["c0"].map { UInt32($0.int!) } ?? 0
            #expect(FM1Bulk.crc32(c["in"]!.bytes, c0) == UInt32(c["out"]!.int!))
        }
        #expect(FM1Bulk.crc32(Array("123456789".utf8)) == 0xcbf4_3926)
    }

    @Test func identities() {
        #expect(FM1Bulk.handshakeQuery == V["handshakeQuery"]!.bytes)
        #expect(FM1Bulk.upgrade == V["upgrade"]!.bytes)
        for c in V["identity"]!.array {
            let label = c["label"]!.string!, want = c["out"]!
            let id = FM1Bulk.parseIdentity(c["in"]!.bytes)
            guard let id else { #expect(want.isNull, "\(label)"); continue }
            #expect(id.text == want["text"]!.string!, "\(label)")
            #expect(id.model == want["model"]!.string!, "\(label)")
            #expect(id.version == want["version"]!.int!, "\(label)")
            #expect(id.open == want["open"]!.bool!, "\(label)")
            #expect(id.isOTA == want["isOTA"]!.bool!, "\(label)")
            #expect(id.isFizz == want["isFizz"]!.bool!, "\(label)")
        }
    }

    private func request(_ r: FM1ReadRequest?) -> JV {
        guard let r else { return .null }
        return .obj(["fl": .num(Double(r.fl)), "addr": .num(Double(r.addr)), "len": .num(Double(r.len))])
    }

    @Test func updateFraming() {
        for c in V["updateFrames"]!.array {
            #expect(FM1Bulk.updateFrame(cmd: UInt8(c["cmd"]!.int!), payload: c["payload"]!.bytes) == c["out"]!.bytes)
        }
        for c in V["responses"]!.array {
            let out = FM1Bulk.response(addr: UInt32(c["addr"]!.int!), data: c["data"]!.bytes, fl: UInt8(c["fl"]!.int!))
            #expect(out == c["out"]!.bytes)
            #expect(request(FM1Bulk.parseRequest(out)) == c["parsed"]!)
        }
        for c in V["requests"]!.array {
            #expect(request(FM1Bulk.parseRequest(c["in"]!.bytes)) == c["out"]!, "\(c["out"]!)")
        }
    }

    @Test func bulkChannel() {
        for c in V["bulk"]!["out"]!.array {
            #expect(FM1Bulk.bulkFrame(tag: c["tag"]!.string!, words: c["words"]!.u32s) == c["out"]!.bytes)
        }
        for c in V["bulk"]!["in"]!.array {
            let r = FM1Bulk.parseBulk(c["in"]!.bytes), want = c["out"]!
            if let r {
                #expect(r.tag == want["tag"]?.string)
                #expect(r.data == want["data"]?.bytes)
            } else {
                #expect(want.isNull)
            }
        }
    }
}
