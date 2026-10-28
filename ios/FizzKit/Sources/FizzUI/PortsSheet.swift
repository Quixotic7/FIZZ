// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PORTS: what the last scan saw (every MIDI port, the ones probed, what answered) and the LOG, for when the FM-1 is
// not found. Opened by a long press on the connect screen or the unit pill; its text can be shared.

import SwiftUI
import FizzKit

/// The last scan: every port CoreMIDI listed, the pairs probed, and the ids that answered the handshake.
public struct PortReport: Sendable, Equatable {
    public struct Answer: Sendable, Equatable {
        public let port: String
        public let id: String
    }
    public var at: Date?
    public var sources: [String] = []
    public var destinations: [String] = []
    public var probed: [String] = []
    public var answers: [Answer] = []

    public init(at: Date? = nil) { self.at = at }
}

/// What firmware an update-handshake id belongs to, when it's one we know.
public enum FirmwareName {
    public static func of(_ id: String) -> String? {
        if id.hasPrefix("FM-1_46") { return "FIZZ" }
        if id.hasPrefix("FM-1_52") { return "REMOTEFM1" }
        if id.hasPrefix("FM1_OFFICIAL_OPEN") || id.hasPrefix("FM1_COMMUNITY") { return "M-VAVE OPEN" }
        if id.range(of: #"^FM-1_0\d\d$"#, options: .regularExpression) != nil { return "M-VAVE" }
        if id.lowercased().hasPrefix("ota-") { return "UPDATE MODE" }
        return nil
    }
}

struct PortsSheet: View {
    let model: UnitModel
    @Environment(\.dismiss) private var dismiss
    @State private var showLog = false

    var body: some View {
        let r = model.ports
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                HStack {
                    Text("PORTS").fz(24, 800, em: 0.08).foregroundStyle(FZ.text.color)
                    Spacer()
                    Button { model.rescan() } label: {
                        Text("LOOK AGAIN").fz(13, 800, em: 0.08).foregroundStyle(.black)
                            .padding(.horizontal, 16).frame(height: 36)
                            .background(Capsule().fill(FZ.yellow.color))
                    }.buttonStyle(.plain)
                    Button { showLog = true } label: {
                        Text("LOG").fz(13, 800, em: 0.08).foregroundStyle(FZ.text.color)
                            .padding(.horizontal, 14).frame(height: 36)
                            .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
                    }.buttonStyle(.plain)
                    ShareLink(item: text) {
                        IconView(Icons.share, color: FZ.text.color, lineWidth: 2.2).frame(width: 20, height: 20)
                            .frame(width: 40, height: 40)
                            .overlay(Circle().strokeBorder(FZ.line.color, lineWidth: 2))
                    }.buttonStyle(.plain)
                }
                group("IN", r.sources, color: FZ.cyan)
                group("OUT", r.destinations, color: FZ.cyan)
                group("PROBED", r.probed, color: FZ.yellow)
                VStack(alignment: .leading, spacing: 8) {
                    label("ANSWERED")
                    if r.answers.isEmpty { none }
                    ForEach(Array(r.answers.enumerated()), id: \.offset) { _, a in
                        HStack(spacing: 10) {
                            Text(a.port).fz(15, 700).foregroundStyle(FZ.text.color)
                            Spacer(minLength: 8)
                            Text(a.id).fz(15, 800).foregroundStyle(a.id.hasPrefix("FM-1_46") ? FZ.green.color : FZ.red.color)
                            if let f = FirmwareName.of(a.id) {
                                Chip(text: f, color: a.id.hasPrefix("FM-1_46") ? FZ.green : FZ.red)
                            }
                        }
                    }
                }
                VStack(alignment: .leading, spacing: 6) {
                    label("LOG")
                    ForEach(Array(model.log.suffix(60).enumerated()), id: \.offset) { _, l in
                        Text(l).font(.system(size: 12, design: .monospaced)).foregroundStyle(FZ.body.color)
                            .textSelection(.enabled)
                    }
                }
            }
            .padding(20)
        }
        .background(Color.black)
        .preferredColorScheme(.dark)
        .sheet(isPresented: $showLog) { LogSheet(model: model) }
    }

    private func group(_ title: String, _ names: [String], color: RGB) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            label(title)
            if names.isEmpty { none }
            ForEach(Array(names.enumerated()), id: \.offset) { _, n in
                HStack(spacing: 10) {
                    Circle().fill(color.color).frame(width: 8, height: 8)
                    Text(n).fz(15, 700).foregroundStyle(FZ.text.color)
                }
            }
        }
    }

    private func label(_ s: String) -> some View { Text(s).fz(13, 800, em: 0.12).foregroundStyle(FZ.muted.color) }
    private var none: some View { Text("—").fz(15, 700).foregroundStyle(FZ.muted.color) }

    /// The sheet as plain text, to send with a bug report.
    private var text: String {
        let r = model.ports
        var s = "FIZZ for iOS: ports\n"
        if let at = r.at { s += "scan: \(at.formatted(date: .abbreviated, time: .standard))\n" }
        s += "in: \(r.sources.joined(separator: ", "))\nout: \(r.destinations.joined(separator: ", "))\n"
        s += "probed: \(r.probed.joined(separator: ", "))\n"
        s += "answered: \(r.answers.map { "\($0.port) = \($0.id)" }.joined(separator: ", "))\n\nlog:\n"
        s += model.log.suffix(200).joined(separator: "\n")
        return s
    }
}
