// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A raw DEFLATE stream (a ZIP member's method 8) inflated with Apple's Compression framework, whose COMPRESSION_ZLIB is
// raw DEFLATE (RFC 1951, no zlib header): the bundle's dnInflateRaw calls it as __fizzInflateRaw (build.mjs).

import Compression
import Foundation

enum RawInflate {
    struct Failure: Error, CustomStringConvertible { let description: String }

    static func inflate(_ src: [UInt8], limit: Int = 64 << 20) throws -> [UInt8] {
        let s = UnsafeMutablePointer<compression_stream>.allocate(capacity: 1)
        defer { s.deallocate() }
        guard compression_stream_init(s, COMPRESSION_STREAM_DECODE, COMPRESSION_ZLIB) == COMPRESSION_STATUS_OK else {
            throw Failure(description: "inflate: could not start")
        }
        defer { compression_stream_destroy(s) }
        let chunk = 64 * 1024
        let dst = UnsafeMutablePointer<UInt8>.allocate(capacity: chunk)
        defer { dst.deallocate() }
        var out: [UInt8] = []
        out.reserveCapacity(max(chunk, src.count * 4))
        return try src.withUnsafeBufferPointer { sp -> [UInt8] in
            s.pointee.src_ptr = sp.baseAddress ?? UnsafePointer(dst)
            s.pointee.src_size = sp.count
            while true {
                s.pointee.dst_ptr = dst
                s.pointee.dst_size = chunk
                let before = s.pointee.src_size
                let st = compression_stream_process(s, Int32(COMPRESSION_STREAM_FINALIZE.rawValue))
                let made = chunk - s.pointee.dst_size
                out.append(contentsOf: UnsafeBufferPointer(start: dst, count: made))
                if out.count > limit { throw Failure(description: "inflate: more than \(limit) bytes") }
                switch st {
                case COMPRESSION_STATUS_END: return out
                case COMPRESSION_STATUS_OK:
                    if made == 0 && s.pointee.src_size == before { throw Failure(description: "inflate: the stream is cut short") }
                default: throw Failure(description: "inflate: not a raw DEFLATE stream")
                }
            }
        }
    }
}
